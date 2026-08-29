// ======================================================================
// \title  DoomEngine.cpp
// \brief  F Prime DOOM engine wrapper - implementation.
//
// The engine is driven from a Svc.Sched input port. Each schedIn call
// invokes doomgeneric_Tick(), or replays one buffered melt frame while
// a screen wipe drains; upstream doomgeneric calls back into the
// extern "C" DG_* hooks at the bottom of this file, which delegate to
// the singleton DoomEngine instance.
//
// Cross-thread communication is limited to the key queue (guarded by
// m_keyMutex) and the start/stop handoff: forceStart rendezvouses with
// any in-flight schedIn tick (m_tickInProgress), publishes all engine
// state, then stores m_engineRunning; schedIn_handler loads it
// (seq_cst, ordered against m_tickInProgress) before touching any
// engine state.
// ======================================================================
#include "Doom/DoomEngine/DoomEngine.hpp"

#include "Doom/FppConstantsAc.hpp"

#include <Fw/Buffer/Buffer.hpp>
#include <Fw/Types/Assert.hpp>
#include <Fw/Types/String.hpp>
#include <Fw/Time/TimeInterval.hpp>
#include <Os/File.hpp>
#include <Os/Task.hpp>

#include <cstring>

extern "C" {
#include "Doom/DoomEngine/doomgeneric/doomgeneric.h"
// d_loop.h exports singletics: one game tic per TryRunTics() call.
#include "Doom/DoomEngine/doomgeneric/d_loop.h"
// d_main.h exports D_StartTitle: return to the boot title sequence.
#include "Doom/DoomEngine/doomgeneric/d_main.h"
// i_video.h declares the engine's active palette (struct color colors[256]).
#include "Doom/DoomEngine/doomgeneric/i_video.h"
}  // extern "C"

namespace Doom {

DoomEngine* DoomEngine::s_instance = nullptr;

// ----------------------------------------------------------------------
// Construction / destruction
// ----------------------------------------------------------------------

DoomEngine::DoomEngine(const char* compName)
    : DoomEngineComponentBase(compName),
      m_paletteGeneration(0),
      m_framesProduced(0),
      m_lastState(EngineState::OFF),
      m_engineRunning(false),
      m_tickInProgress(false),
      m_engineCreated(false),
      m_resetRequested(false),
      m_engineStartValid(false),
      m_keyQueueHead(0),
      m_keyQueueTail(0),
      m_keyQueueCount(0),
      m_overflowReported(false),
      m_keysDropped(0),
      m_inputEventsThisWindow(0),
      m_inputBytesThisWindow(0),
      m_schedTicks(0),
      m_windowElapsedUsec(0),
      m_framesThisWindow(0),
      m_frameBytesThisWindow(0),
      m_virtualSleepMs(0),
      m_useTickTime(false),
      m_tickElapsedUsec(0),
      m_realElapsedUsec(0),
      m_drawsThisTick(0),
      m_meltHead(0),
      m_meltCount(0),
      m_framesDropped(0) {
    FW_ASSERT(s_instance == nullptr);
    s_instance = this;

    (void)::memset(m_pendingPalette, 0, sizeof(m_pendingPalette));
    (void)::memset(m_keyQueue, 0, sizeof(m_keyQueue));
    (void)::memset(m_wadPath, 0, sizeof(m_wadPath));
    (void)::memset(m_argvStorage, 0, sizeof(m_argvStorage));
    (void)::memset(m_argvPointers, 0, sizeof(m_argvPointers));
    (void)::memset(m_meltFrames, 0, sizeof(m_meltFrames));
    (void)::memset(m_meltFrameNumbers, 0, sizeof(m_meltFrameNumbers));
    (void)::memset(m_frameBuffer, 0, sizeof(m_frameBuffer));
}

DoomEngine::~DoomEngine() {
    s_instance = nullptr;
}

DoomEngine* DoomEngine::getInstance() {
    return s_instance;
}

void DoomEngine::setWadPath(const char* wadPath) {
    FW_ASSERT(wadPath != nullptr);
    const FwSizeType len = static_cast<FwSizeType>(::strlen(wadPath));
    // Reject rather than silently truncate: a clipped path would fail
    // WAD load far from the actual cause.
    FW_ASSERT(len < WAD_PATH_MAX, static_cast<FwAssertArgType>(len));
    (void)::memcpy(m_wadPath, wadPath, len);
    m_wadPath[len] = '\0';
}

// ----------------------------------------------------------------------
// schedIn: one Tick per rate-group invocation
// ----------------------------------------------------------------------

namespace {
// Scope guard: clears the tick-in-progress flag on every exit path.
class TickGuard final {
  public:
    explicit TickGuard(std::atomic<bool>& flag) : m_flag(flag) { m_flag.store(true); }
    ~TickGuard() { m_flag.store(false); }
    TickGuard(const TickGuard&) = delete;
    TickGuard& operator=(const TickGuard&) = delete;

  private:
    std::atomic<bool>& m_flag;
};
}  // namespace

void DoomEngine::schedIn_handler(FwIndexType portNum, U32 context) {
    // Publish tick-in-progress BEFORE reading m_engineRunning (both
    // seq_cst): forceStart() only mutates engine state after seeing
    // this flag clear, so either it waits for this tick or this tick
    // observes the engine stopped. See the rendezvous in forceStart().
    TickGuard guard(m_tickInProgress);
    // Variable-rate support: a nonzero context carries the rate
    // group's period in microseconds per tick. The engine clock then
    // advances by tick time, so any rate-group frequency paces DOOM
    // correctly. Context 0 keeps the legacy OS-clock behavior.
    if (context > 0U) {
        if (!m_useTickTime) {
            // Fold in real time accrued so far; the clock never steps back.
            (void)this->platformGetTicksMs();
            m_tickElapsedUsec = m_realElapsedUsec;
            m_useTickTime = true;
        }
        m_tickElapsedUsec += context;
        m_windowElapsedUsec += context;
    }
    if (!m_engineRunning.load()) {
        // Engine not running - re-publish the last state (OFF, FAILED,
        // or a transient STARTING) rather than clobbering it with OFF.
        // Self-heal a stale RUNNING left by a tick that raced a Stop,
        // re-checking the running flag so a concurrent Start that just
        // published RUNNING is not overwritten.
        const EngineState::T last = m_lastState.load();
        if (last == EngineState::RUNNING) {
            if (!m_engineRunning.load()) {
                this->publishState(EngineState::OFF);
            }
        } else {
            this->tlmWrite_State(EngineState(last));
        }
        return;
    }
    const bool resetRequested = m_resetRequested.exchange(false);
    if (resetRequested) {
        // Flush pending input and any in-flight melt playback, then
        // return the game to the title sequence - the same state a
        // freshly booted engine idles in. D_StartTitle only latches
        // engine-side flags; the title sequence itself starts on the
        // next Tick, so this tick skips straight to the telemetry
        // refresh below.
        m_keyMutex.lock();
        m_keyQueueHead = 0U;
        m_keyQueueTail = 0U;
        m_keyQueueCount = 0U;
        m_overflowReported = false;
        m_keyMutex.unLock();
        m_meltHead = 0U;
        m_meltCount = 0U;
        D_StartTitle();
        this->log_ACTIVITY_HI_EngineReset();
    } else if (m_meltCount > 0U) {
        // A screen-wipe melt was captured on an earlier tick (see
        // platformDrawFrame). Play it back one frame per cycle so the
        // animation is visible at its native 35 Hz pace, holding the
        // game paused meanwhile — the same thing the upstream engine
        // does while its blocking wipe loop runs.
        this->emitFrame(m_meltFrames[m_meltHead], m_meltFrameNumbers[m_meltHead]);
        m_meltHead = (m_meltHead + 1U) % MELT_QUEUE_CAPACITY;
        m_meltCount--;
    } else {
        // Drive one DOOM frame of game logic. upstream doomgeneric will
        // call back into DG_GetKey / DG_DrawFrame on this same thread.
        m_drawsThisTick = 0U;
        doomgeneric_Tick();
    }

    // Refresh derived telemetry. frameOut / paletteOut are emitted
    // either by the melt playback above or from DG_DrawFrame inside
    // Tick. Re-check the running flag so a Stop that landed mid-tick
    // is not followed by a stale RUNNING heartbeat.
    if (m_engineRunning.load()) {
        this->publishState(EngineState::RUNNING);
    }
    this->tlmWrite_FrameCount(m_framesProduced);

    m_keyMutex.lock();
    const U8 depth = static_cast<U8>(m_keyQueueCount);
    const U32 dropped = m_keysDropped;
    m_keyMutex.unLock();
    this->tlmWrite_KeyQueueDepth(depth);
    this->tlmWrite_KeyEventsDropped(dropped);
    this->tlmWrite_FramesDropped(m_framesDropped);

    // Emit derived rate telemetry roughly once per second: after one
    // second of accumulated tick time in variable-rate mode, or after
    // 35 ticks in the legacy fixed-35-Hz mode.
    m_schedTicks++;
    constexpr U32 RATE_WINDOW_TICKS = 35U;
    constexpr U32 RATE_WINDOW_USEC = 1000000U;
    const bool windowDone = m_useTickTime ? (m_windowElapsedUsec >= RATE_WINDOW_USEC)
                                          : (m_schedTicks >= RATE_WINDOW_TICKS);
    if (windowDone) {
        const F32 windowSec = m_useTickTime
            ? (static_cast<F32>(m_windowElapsedUsec) / 1000000.0f)
            : (static_cast<F32>(m_schedTicks) / 35.0f);
        const F32 frameRate = static_cast<F32>(m_framesThisWindow) / windowSec;
        const U32 frameDataRate = (windowSec > 0.0f)
            ? static_cast<U32>(static_cast<F32>(m_frameBytesThisWindow) / windowSec)
            : 0U;
        // Snapshot input counters under the same mutex used to update
        // them; reset them inside the lock so the next window starts
        // clean and we don't lose events that arrive between unlock
        // and reset.
        m_keyMutex.lock();
        const U32 inputEvents = m_inputEventsThisWindow;
        const U32 inputBytes = m_inputBytesThisWindow;
        m_inputEventsThisWindow = 0U;
        m_inputBytesThisWindow = 0U;
        m_keyMutex.unLock();
        const F32 inputRate = static_cast<F32>(inputEvents) / windowSec;
        const U32 inputDataRate = (windowSec > 0.0f)
            ? static_cast<U32>(static_cast<F32>(inputBytes) / windowSec)
            : 0U;

        this->tlmWrite_FrameRateHz(frameRate);
        this->tlmWrite_FrameDataRateBps(frameDataRate);
        this->tlmWrite_InputCommandRateHz(inputRate);
        this->tlmWrite_InputDataRateBps(inputDataRate);

        m_schedTicks = 0U;
        m_windowElapsedUsec = 0U;
        m_framesThisWindow = 0U;
        m_frameBytesThisWindow = 0U;
    }
}

// ----------------------------------------------------------------------
// State telemetry
// ----------------------------------------------------------------------

void DoomEngine::publishState(EngineState state) {
    m_lastState.store(state.e);
    this->tlmWrite_State(state);
}

// ----------------------------------------------------------------------
// Commands
// ----------------------------------------------------------------------

bool DoomEngine::forceStart() {
    // Serialize concurrent callers (autoStart thread vs a ground Start
    // command): only one caller may run the check-rendezvous-create
    // sequence at a time.
    Os::ScopeLock startLock(m_startMutex);
    if (m_engineRunning.load()) {
        this->log_WARNING_LO_AlreadyRunning();
        return false;
    }
    // Rendezvous with the rate-group thread: a schedIn tick that
    // loaded m_engineRunning==true before a Stop may still be
    // executing; wait for it to finish before mutating engine state
    // (bounded at ~1 s, far beyond any tick duration).
    bool rendezvousOk = true;
    for (U32 spin = 0U; m_tickInProgress.load(); spin++) {
        if (spin >= RENDEZVOUS_MAX_SPINS) {
            rendezvousOk = false;
            break;
        }
        const Os::Task::Status delayStatus =
            Os::Task::delay(Fw::TimeInterval(0, RENDEZVOUS_DELAY_USEC));
        if (delayStatus != Os::Task::Status::OP_OK) {
            rendezvousOk = false;
            break;
        }
    }
    if (!rendezvousOk) {
        // Busy rejects deliberately leave State telemetry untouched.
        this->log_WARNING_LO_StartBusy();
        return false;
    }
    if (!m_engineCreated) {
        // Pre-validate the WAD: the vendored engine calls exit() via
        // I_Error on a missing or unfindable WAD, which would take
        // down the whole flight process. Reject the Start instead.
        // Advisory only (TOCTOU): a WAD removed after this check can
        // still reach I_Error.
        Os::File wad;
        if ((m_wadPath[0] == '\0') ||
            (wad.open(m_wadPath, Os::File::OPEN_READ) != Os::File::OP_OK)) {
            const char* const shown = (m_wadPath[0] != '\0') ? m_wadPath : "(no WAD path configured)";
            this->log_WARNING_HI_WadUnavailable(Fw::String(shown));
            this->publishState(EngineState::FAILED);
            return false;
        }
        wad.close();
    }
    this->publishState(EngineState::STARTING);

    // (Re)base the reference time used by DG_GetTicksMs so time spent
    // stopped is not counted. The elapsed-time accumulators are reset
    // only on the first start: the vendored timer caches a basetime
    // derived from this clock, so it must never step backwards across
    // a Stop->Start cycle.
    const Os::RawTime::Status rt = m_engineStart.now();
    m_engineStartValid = (rt == Os::RawTime::Status::OP_OK);

    // Discard melt/draw pacing state left over from a previous run
    // (e.g. a melt playback interrupted by Stop).
    m_meltHead = 0U;
    m_meltCount = 0U;
    m_drawsThisTick = 0U;

    if (!m_engineCreated) {
        m_virtualSleepMs = 0U;
        m_realElapsedUsec = 0U;
        m_tickElapsedUsec = 0U;

        // Run the engine in singletics mode: exactly one game tic per
        // doomgeneric_Tick() call, removing every wall-clock dependency
        // from TryRunTics (no catch-up bursts, no busy-wait against the
        // 35 Hz tic boundary). Set before Create so startup tics are
        // already singletics-paced (upstream does this for -timedemo).
        singletics = 1U;

        // doomgeneric caches argv (as myargv) and walks it later from
        // M_CheckParm, so both the pointer array and the backing strings
        // must outlive the call. Both live in DoomEngine members.
        const int argc = this->buildEngineArgv(m_argvPointers,
                                               static_cast<int>(FW_NUM_ARRAY_ELEMENTS(m_argvPointers)));
        // The vendored engine's init (Z_Init, W_AddFile, ...) is
        // one-shot; guard Create so a ground Stop->Start cycle resumes
        // the existing engine instead of re-initialising it.
        // const_cast: the vendored C API takes char** but never
        // mutates the argv strings.
        doomgeneric_Create(argc, const_cast<char**>(m_argvPointers));
        m_engineCreated = true;
    }

    // seq_cst store, matching schedIn_handler's load: all handoff
    // operations share the single seq_cst total order.
    m_engineRunning.store(true);
    this->log_ACTIVITY_HI_EngineStarted();
    this->publishState(EngineState::RUNNING);
    return true;
}

void DoomEngine::Start_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    const bool ok = this->forceStart();
    this->cmdResponse_out(opCode, cmdSeq,
                          ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void DoomEngine::Stop_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Cooperative stop - the rate group will simply stop calling Tick.
    // DOOM has no clean shutdown path, so we just stop driving it.
    // Serialize against an in-flight forceStart (e.g. autoStart) so a
    // Stop is never silently overwritten by a concurrent start.
    Os::ScopeLock startLock(m_startMutex);
    if (m_engineRunning.load()) {
        m_engineRunning.store(false);
        this->log_ACTIVITY_HI_EngineStopped();
    }
    this->publishState(EngineState::OFF);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void DoomEngine::Reset_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Serialize with Start/Stop so m_engineCreated is read consistently.
    Os::ScopeLock startLock(m_startMutex);
    if (!m_engineCreated) {
        this->log_WARNING_LO_ResetNotStarted();
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    // Applied by the rate-group thread at the top of its next running
    // tick; if the engine is stopped the reset is consumed on the
    // first tick after the next Start.
    m_resetRequested.store(true);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void DoomEngine::KeyTap_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Doom::DoomKey& key) {
    const bool ok = this->enqueueKeyTap(static_cast<U8>(key.e));
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void DoomEngine::KeyDown_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Doom::DoomKey& key) {
    const bool ok = this->enqueueKey(true, static_cast<U8>(key.e));
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void DoomEngine::KeyUp_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Doom::DoomKey& key) {
    const bool ok = this->enqueueKey(false, static_cast<U8>(key.e));
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void DoomEngine::RawKey_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool pressed, U8 code) {
    const bool ok = this->enqueueKey(pressed, code);
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

// ----------------------------------------------------------------------
// Parallel input port handlers
//
// Mirror the key-input commands so other components (e.g. a sensor
// adapter, a sequencer macro) can drive inputs directly. Same enqueue
// path as the command handlers; key-queue overflow is reported via the
// KeyQueueOverflow event the same way.
// ----------------------------------------------------------------------

void DoomEngine::keyTapIn_handler(FwIndexType /*portNum*/, const Doom::DoomKey& key) {
    (void)this->enqueueKeyTap(static_cast<U8>(key.e));
}

void DoomEngine::keyDownIn_handler(FwIndexType /*portNum*/, const Doom::DoomKey& key) {
    (void)this->enqueueKey(true, static_cast<U8>(key.e));
}

void DoomEngine::keyUpIn_handler(FwIndexType /*portNum*/, const Doom::DoomKey& key) {
    (void)this->enqueueKey(false, static_cast<U8>(key.e));
}

void DoomEngine::rawKeyIn_handler(FwIndexType /*portNum*/, bool pressed, U8 code) {
    (void)this->enqueueKey(pressed, code);
}

// ----------------------------------------------------------------------
// Key queue helpers
// ----------------------------------------------------------------------

bool DoomEngine::enqueueKeyEvents(const U16* entries, FwSizeType count) {
    bool ok = false;
    bool emitOverflow = false;
    m_keyMutex.lock();
    // Every received event counts toward the input rate windows,
    // regardless of whether the queue had room. 2 bytes per event
    // (pressed flag byte + key code byte) - documented in Telemetry.fppi.
    m_inputEventsThisWindow += static_cast<U32>(count);
    m_inputBytesThisWindow += static_cast<U32>(count) * 2U;
    // All-or-nothing: either every entry fits or none is queued (a
    // partial down/up tap would leave the key stuck down).
    if ((m_keyQueueCount + count) <= KEY_QUEUE_CAPACITY) {
        for (FwSizeType i = 0; i < count; i++) {
            m_keyQueue[m_keyQueueTail] = entries[i];
            m_keyQueueTail = (m_keyQueueTail + 1U) % KEY_QUEUE_CAPACITY;
            m_keyQueueCount++;
        }
        m_overflowReported = false;
        ok = true;
    } else {
        m_keysDropped += static_cast<U32>(count);
        if (!m_overflowReported) {
            m_overflowReported = true;
            emitOverflow = true;
        }
    }
    m_keyMutex.unLock();
    if (emitOverflow) {
        this->log_WARNING_LO_KeyQueueOverflow();
    }
    return ok;
}

bool DoomEngine::enqueueKey(bool pressed, U8 code) {
    const U16 entry = packKeyEntry(pressed, code);
    return this->enqueueKeyEvents(&entry, 1);
}

bool DoomEngine::enqueueKeyTap(U8 code) {
    const U16 entries[2] = {packKeyEntry(true, code), packKeyEntry(false, code)};
    return this->enqueueKeyEvents(entries, 2);
}

bool DoomEngine::platformGetKey(bool& pressed, U8& code) {
    bool drained = false;
    m_keyMutex.lock();
    if (m_keyQueueCount > 0U) {
        const U16 entry = m_keyQueue[m_keyQueueHead];
        m_keyQueueHead = (m_keyQueueHead + 1U) % KEY_QUEUE_CAPACITY;
        m_keyQueueCount--;
        pressed = ((entry >> 8) & 0x01U) != 0U;
        code = static_cast<U8>(entry & 0xFFU);
        drained = true;
    }
    m_keyMutex.unLock();
    return drained;
}

// ----------------------------------------------------------------------
// Platform glue (called inside doomgeneric_Tick on rate-group thread)
// ----------------------------------------------------------------------

void DoomEngine::platformInit() {
    // Nothing extra to do; component construction already prepared
    // every backing buffer.
}

void DoomEngine::platformDrawFrame() {
    // Upstream allocates DG_ScreenBuffer as RESX * RESY * 4 bytes; with
    // CMAP256 only the first RESX * RESY bytes are written and they are
    // 8-bit palette indices.
    FW_ASSERT(DG_ScreenBuffer != nullptr);
    const U8* const src = reinterpret_cast<const U8*>(DG_ScreenBuffer);

    m_framesProduced++;
    m_framesThisWindow++;
    m_drawsThisTick++;

    // A melt draws its whole animation inside one Tick (see the
    // pacing model in the README): emit the first draw live, capture
    // the rest for schedIn_handler to play back one frame per cycle.
    if (m_drawsThisTick > 1U) {
        if (m_meltCount < MELT_QUEUE_CAPACITY) {
            const FwSizeType slot = (m_meltHead + m_meltCount) % MELT_QUEUE_CAPACITY;
            (void)::memcpy(m_meltFrames[slot], src, FRAME_BYTES);
            m_meltFrameNumbers[slot] = static_cast<U32>(m_framesProduced);
            m_meltCount++;
        } else {
            // Buffer full: the frame is neither emitted nor buffered.
            m_framesDropped++;
        }
        return;
    }

    this->emitFrame(src, static_cast<U32>(m_framesProduced));
}

void DoomEngine::emitFrame(const U8* src, U32 frameNumber) {
    FW_ASSERT(src != nullptr);
    this->capturePaletteIfChanged();

    // The palette goes first so the receiver can render the frame that
    // follows. Sent every frame so a late-attaching ground converges.
    if (this->isConnected_paletteOut_OutputPort(0)) {
        Doom::Palette pal;
        pal.set_generation(m_paletteGeneration);
        (void)::memcpy(pal.get_rgb(), m_pendingPalette, sizeof(m_pendingPalette));
        this->paletteOut_out(0, pal);
        m_frameBytesThisWindow += static_cast<U32>(Doom::PALETTE_BYTES) + 16U;
    }

    // Copy into engine-owned storage: the downstream downsampler packs
    // the buffer in place, and DOOM partially redraws DG_ScreenBuffer
    // between frames, so the live buffer must not be handed out.
    if (this->isConnected_frameOut_OutputPort(0)) {
        (void)::memcpy(m_frameBuffer, src, FRAME_BYTES);
        Fw::Buffer pixels(m_frameBuffer, FRAME_BYTES);
        this->frameOut_out(0, frameNumber, FRAME_WIDTH, FRAME_HEIGHT, pixels);
        m_frameBytesThisWindow += FRAME_BYTES + 16U;
    }
}

void DoomEngine::capturePaletteIfChanged() {
    bool changed = false;
    for (FwSizeType i = 0; i < PALETTE_ENTRIES; ++i) {
        const U8 r = static_cast<U8>(colors[i].r);
        const U8 g = static_cast<U8>(colors[i].g);
        const U8 b = static_cast<U8>(colors[i].b);
        const FwSizeType base = i * 3U;
        if ((m_pendingPalette[base] != r) ||
            (m_pendingPalette[base + 1U] != g) ||
            (m_pendingPalette[base + 2U] != b)) {
            m_pendingPalette[base] = r;
            m_pendingPalette[base + 1U] = g;
            m_pendingPalette[base + 2U] = b;
            changed = true;
        }
    }
    if (changed) {
        m_paletteGeneration++;
    }
}

void DoomEngine::platformSleepMs(U32 ms) {
    // Never block the rate-group thread: accumulate the request as
    // virtual time for DG_GetTicksMs (see README, "Pacing model").
    m_virtualSleepMs += ms;
}

U32 DoomEngine::platformGetTicksMs() {
    // Variable-rate mode: the clock is the tick time accumulated from
    // the schedIn context, so DOOM's timers track the actual rate.
    if (m_useTickTime) {
        return static_cast<U32>(m_tickElapsedUsec / 1000U) + m_virtualSleepMs;
    }
    // Real elapsed time accumulates in a U64 and m_engineStart is
    // rebased on every successful read, so each getDiffUsec delta stays
    // tiny and its U32 range (~71.6 min) is never hit. On a failed read
    // the clock simply does not advance; it never steps backwards.
    if (m_engineStartValid) {
        Os::RawTime current;
        if (current.now() == Os::RawTime::Status::OP_OK) {
            U32 deltaUsec = 0U;
            if (current.getDiffUsec(m_engineStart, deltaUsec) == Os::RawTime::Status::OP_OK) {
                m_realElapsedUsec += deltaUsec;
                m_engineStart = current;
            }
        }
    }
    return static_cast<U32>(m_realElapsedUsec / 1000U) + m_virtualSleepMs;
}

void DoomEngine::platformSetTitle(const char* title) {
    // DG_SetWindowTitle is purely informational for a real desktop
    // build; the flight wrap has no window. Ignored.
    (void)title;
}

// ----------------------------------------------------------------------
// argv synthesis for doomgeneric_Create
// ----------------------------------------------------------------------

int DoomEngine::buildEngineArgv(const char** argv, int maxArgv) {
    FW_ASSERT(argv != nullptr);
    int argc = 0;
    // argv[0]: process name. doomgeneric inspects argv[0] only for the
    // logging banner; the value is otherwise unused.
    (void)::memcpy(m_argvStorage[argc], "doom", 5U);
    argv[argc] = m_argvStorage[argc];
    argc++;

    if ((m_wadPath[0] != '\0') && (argc + 1 < maxArgv)) {
        const char iwadFlag[] = "-iwad";
        (void)::memcpy(m_argvStorage[argc], iwadFlag, sizeof(iwadFlag));
        argv[argc] = m_argvStorage[argc];
        argc++;
        // m_wadPath is NUL-terminated and length-checked by setWadPath.
        const FwSizeType len = static_cast<FwSizeType>(::strlen(m_wadPath));
        (void)::memcpy(m_argvStorage[argc], m_wadPath, len);
        m_argvStorage[argc][len] = '\0';
        argv[argc] = m_argvStorage[argc];
        argc++;
    }
    return argc;
}

}  // namespace Doom

// ======================================================================
// extern "C" DG_* hooks consumed by upstream doomgeneric
// ======================================================================
extern "C" {

void DG_Init(void) {
    Doom::DoomEngine* const inst = Doom::DoomEngine::getInstance();
    if (inst != nullptr) {
        inst->platformInit();
    }
}

void DG_DrawFrame(void) {
    Doom::DoomEngine* const inst = Doom::DoomEngine::getInstance();
    if (inst != nullptr) {
        inst->platformDrawFrame();
    }
}

void DG_SleepMs(uint32_t ms) {
    Doom::DoomEngine* const inst = Doom::DoomEngine::getInstance();
    if (inst != nullptr) {
        inst->platformSleepMs(static_cast<U32>(ms));
    }
}

uint32_t DG_GetTicksMs(void) {
    Doom::DoomEngine* const inst = Doom::DoomEngine::getInstance();
    if (inst != nullptr) {
        return static_cast<uint32_t>(inst->platformGetTicksMs());
    }
    return 0U;
}

int DG_GetKey(int* pressed, unsigned char* key) {
    Doom::DoomEngine* const inst = Doom::DoomEngine::getInstance();
    if ((inst == nullptr) || (pressed == nullptr) || (key == nullptr)) {
        return 0;
    }
    bool pressedFlag = false;
    U8 code = 0U;
    if (!inst->platformGetKey(pressedFlag, code)) {
        return 0;
    }
    *pressed = pressedFlag ? 1 : 0;
    *key = static_cast<unsigned char>(code);
    return 1;
}

void DG_SetWindowTitle(const char* title) {
    Doom::DoomEngine* const inst = Doom::DoomEngine::getInstance();
    if (inst != nullptr) {
        inst->platformSetTitle(title);
    }
}

}  // extern "C"
