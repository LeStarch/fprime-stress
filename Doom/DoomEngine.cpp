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
#include "Doom/DoomEngine.hpp"

#include "Doom/FppConstantsAc.hpp"

#include <Fw/Types/Assert.hpp>
#include <Fw/Time/TimeInterval.hpp>
#include <Os/File.hpp>
#include <Os/Task.hpp>

#include <cstring>

extern "C" {
#include "Doom/doomgeneric/doomgeneric.h"
// d_loop.h exports singletics: one game tic per TryRunTics() call.
#include "Doom/doomgeneric/d_loop.h"
// i_video.h declares the engine's active palette (struct color colors[256]).
#include "Doom/doomgeneric/i_video.h"
}  // extern "C"

namespace Doom {

DoomEngine* DoomEngine::s_instance = nullptr;

// Definition of the dispatch table declared in DoomEngine.hpp. See the
// header comment there for why this lives at class scope.
const DoomEngine::FrameOutWriter DoomEngine::kFrameOutWriters[Doom::CHUNKS_PER_FRAME] = {
    &DoomEngine::tlmWrite_FrameOut00, &DoomEngine::tlmWrite_FrameOut01,
    &DoomEngine::tlmWrite_FrameOut02, &DoomEngine::tlmWrite_FrameOut03,
    &DoomEngine::tlmWrite_FrameOut04, &DoomEngine::tlmWrite_FrameOut05,
    &DoomEngine::tlmWrite_FrameOut06, &DoomEngine::tlmWrite_FrameOut07,
    &DoomEngine::tlmWrite_FrameOut08, &DoomEngine::tlmWrite_FrameOut09,
    &DoomEngine::tlmWrite_FrameOut10, &DoomEngine::tlmWrite_FrameOut11,
    &DoomEngine::tlmWrite_FrameOut12, &DoomEngine::tlmWrite_FrameOut13,
    &DoomEngine::tlmWrite_FrameOut14, &DoomEngine::tlmWrite_FrameOut15,
    &DoomEngine::tlmWrite_FrameOut16, &DoomEngine::tlmWrite_FrameOut17,
    &DoomEngine::tlmWrite_FrameOut18, &DoomEngine::tlmWrite_FrameOut19,
    &DoomEngine::tlmWrite_FrameOut20, &DoomEngine::tlmWrite_FrameOut21,
    &DoomEngine::tlmWrite_FrameOut22, &DoomEngine::tlmWrite_FrameOut23,
    &DoomEngine::tlmWrite_FrameOut24, &DoomEngine::tlmWrite_FrameOut25,
    &DoomEngine::tlmWrite_FrameOut26, &DoomEngine::tlmWrite_FrameOut27,
    &DoomEngine::tlmWrite_FrameOut28, &DoomEngine::tlmWrite_FrameOut29,
    &DoomEngine::tlmWrite_FrameOut30, &DoomEngine::tlmWrite_FrameOut31,
    &DoomEngine::tlmWrite_FrameOut32, &DoomEngine::tlmWrite_FrameOut33,
    &DoomEngine::tlmWrite_FrameOut34, &DoomEngine::tlmWrite_FrameOut35,
    &DoomEngine::tlmWrite_FrameOut36, &DoomEngine::tlmWrite_FrameOut37,
    &DoomEngine::tlmWrite_FrameOut38, &DoomEngine::tlmWrite_FrameOut39,
    &DoomEngine::tlmWrite_FrameOut40, &DoomEngine::tlmWrite_FrameOut41,
    &DoomEngine::tlmWrite_FrameOut42, &DoomEngine::tlmWrite_FrameOut43,
    &DoomEngine::tlmWrite_FrameOut44, &DoomEngine::tlmWrite_FrameOut45,
    &DoomEngine::tlmWrite_FrameOut46, &DoomEngine::tlmWrite_FrameOut47,
    &DoomEngine::tlmWrite_FrameOut48, &DoomEngine::tlmWrite_FrameOut49,
    &DoomEngine::tlmWrite_FrameOut50, &DoomEngine::tlmWrite_FrameOut51,
    &DoomEngine::tlmWrite_FrameOut52, &DoomEngine::tlmWrite_FrameOut53,
    &DoomEngine::tlmWrite_FrameOut54, &DoomEngine::tlmWrite_FrameOut55,
    &DoomEngine::tlmWrite_FrameOut56, &DoomEngine::tlmWrite_FrameOut57,
    &DoomEngine::tlmWrite_FrameOut58, &DoomEngine::tlmWrite_FrameOut59,
    &DoomEngine::tlmWrite_FrameOut60, &DoomEngine::tlmWrite_FrameOut61,
    &DoomEngine::tlmWrite_FrameOut62, &DoomEngine::tlmWrite_FrameOut63,
    &DoomEngine::tlmWrite_FrameOut64, &DoomEngine::tlmWrite_FrameOut65,
    &DoomEngine::tlmWrite_FrameOut66, &DoomEngine::tlmWrite_FrameOut67,
    &DoomEngine::tlmWrite_FrameOut68, &DoomEngine::tlmWrite_FrameOut69,
    &DoomEngine::tlmWrite_FrameOut70, &DoomEngine::tlmWrite_FrameOut71,
    &DoomEngine::tlmWrite_FrameOut72, &DoomEngine::tlmWrite_FrameOut73,
    &DoomEngine::tlmWrite_FrameOut74, &DoomEngine::tlmWrite_FrameOut75,
    &DoomEngine::tlmWrite_FrameOut76, &DoomEngine::tlmWrite_FrameOut77,
    &DoomEngine::tlmWrite_FrameOut78, &DoomEngine::tlmWrite_FrameOut79
};

// ----------------------------------------------------------------------
// Construction / destruction
// ----------------------------------------------------------------------

DoomEngine::DoomEngine(const char* compName)
    : DoomEngineComponentBase(compName),
      m_paletteGeneration(0),
      m_framesProduced(0),
      m_engineRunning(false),
      m_tickInProgress(false),
      m_engineCreated(false),
      m_engineStartValid(false),
      m_keyQueueHead(0),
      m_keyQueueTail(0),
      m_keyQueueCount(0),
      m_overflowReported(false),
      m_keysDropped(0),
      m_inputEventsThisWindow(0),
      m_inputBytesThisWindow(0),
      m_schedTicks(0),
      m_framesThisWindow(0),
      m_frameBytesThisWindow(0),
      m_virtualSleepMs(0),
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
    if (!m_engineRunning.load()) {
        // Engine not started - publish OFF state heartbeat.
        this->tlmWrite_State(EngineState::OFF);
        return;
    }
    if (m_meltCount > 0U) {
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

    // Refresh derived telemetry. FrameOut / PaletteOut are emitted
    // either by the melt playback above or from DG_DrawFrame inside
    // Tick. Re-check the running flag so a Stop that landed mid-tick
    // is not followed by a stale RUNNING heartbeat.
    if (m_engineRunning.load()) {
        this->tlmWrite_State(EngineState::RUNNING);
    }
    this->tlmWrite_FrameCount(m_framesProduced);

    m_keyMutex.lock();
    const U8 depth = static_cast<U8>(m_keyQueueCount);
    const U32 dropped = m_keysDropped;
    m_keyMutex.unLock();
    this->tlmWrite_KeyQueueDepth(depth);
    this->tlmWrite_KeyEventsDropped(dropped);
    this->tlmWrite_FramesDropped(m_framesDropped);

    // Emit derived rate telemetry once per RATE_WINDOW_TICKS. With the
    // deployment's 35 Hz rate group (matching DOOM's native cadence)
    // this is once per second.
    m_schedTicks++;
    constexpr U32 RATE_WINDOW_TICKS = 35U;
    if (m_schedTicks >= RATE_WINDOW_TICKS) {
        const F32 windowSec = static_cast<F32>(m_schedTicks) / 35.0f;
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
        m_framesThisWindow = 0U;
        m_frameBytesThisWindow = 0U;
    }
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
    for (U32 spin = 0U; m_tickInProgress.load(); spin++) {
        if (spin >= RENDEZVOUS_MAX_SPINS) {
            this->log_WARNING_LO_StartBusy();
            return false;
        }
        const Os::Task::Status delayStatus = Os::Task::delay(Fw::TimeInterval(0, RENDEZVOUS_DELAY_USEC));
        if (delayStatus != Os::Task::Status::OP_OK) {
            this->log_WARNING_LO_StartBusy();
            return false;
        }
    }
    if (!m_engineCreated && (m_wadPath[0] != '\0')) {
        // Pre-validate the WAD: the vendored engine calls exit() via
        // I_Error on a missing WAD, which would take down the whole
        // flight process. Reject the Start instead.
        Os::File wad;
        if (wad.open(m_wadPath, Os::File::OPEN_READ) != Os::File::OP_OK) {
            this->log_WARNING_HI_WadUnavailable(Fw::LogStringArg(m_wadPath));
            this->tlmWrite_State(EngineState::FAILED);
            return false;
        }
        wad.close();
    }
    this->tlmWrite_State(EngineState::STARTING);

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
    this->tlmWrite_State(EngineState::RUNNING);
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
    this->tlmWrite_State(EngineState::OFF);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void DoomEngine::KeyTap_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Doom::DoomKey key) {
    const bool ok = this->enqueueKeyTap(static_cast<U8>(key.e));
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void DoomEngine::KeyDown_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Doom::DoomKey key) {
    const bool ok = this->enqueueKey(true, static_cast<U8>(key.e));
    this->cmdResponse_out(opCode, cmdSeq, ok ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void DoomEngine::KeyUp_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Doom::DoomKey key) {
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
            m_meltFrameNumbers[slot] = m_framesProduced;
            m_meltCount++;
        } else {
            // Buffer full: the frame is neither emitted nor buffered.
            m_framesDropped++;
        }
        return;
    }

    this->emitFrame(src, m_framesProduced);
}

void DoomEngine::emitFrame(const U8* src, U32 frameNumber) {
    FW_ASSERT(src != nullptr);
    this->capturePaletteIfChanged();

    // FRAME_HEIGHT is an exact multiple of ROWS_PER_CHUNK (400 / 5 = 80),
    // so every chunk is the same fixed size and no partial-chunk handling
    // is needed. Enforce that invariant at compile time so the loop body
    // can copy a fixed FRAME_CHUNK_BYTES per iteration.
    static_assert((FRAME_HEIGHT % ROWS_PER_CHUNK) == 0U,
                  "FRAME_HEIGHT must be an integer multiple of ROWS_PER_CHUNK");
    static_assert(static_cast<U32>(ROWS_PER_CHUNK) * static_cast<U32>(FRAME_WIDTH) ==
                      static_cast<U32>(Doom::FRAME_CHUNK_BYTES),
                  "ROWS_PER_CHUNK * FRAME_WIDTH must equal FRAME_CHUNK_BYTES");

    Doom::FrameChunk chunk;
    chunk.set_width(FRAME_WIDTH);
    chunk.set_rowCount(ROWS_PER_CHUNK);
    chunk.set_frame(frameNumber);
    U8* const chunkPixels = chunk.get_pixels();

    // Each iteration writes its chunk to a distinct telemetry channel id
    // (FrameOut00 .. FrameOut79) via the dispatch table. The N->channel-id
    // multiplex is what defeats TlmChan's slot-store collapse - see the
    // header comment on kFrameOutWriters above.
    for (U16 chunkIdx = 0; chunkIdx < Doom::CHUNKS_PER_FRAME; chunkIdx++) {
        const U16 currentRow = static_cast<U16>(chunkIdx * ROWS_PER_CHUNK);
        const U32 offset = static_cast<U32>(currentRow) * static_cast<U32>(FRAME_WIDTH);
        (void)::memcpy(chunkPixels, &src[offset], Doom::FRAME_CHUNK_BYTES);
        chunk.set_row(currentRow);
        (this->*kFrameOutWriters[chunkIdx])(chunk, Fw::Time());
        // Account for on-wire bytes of this chunk: the fixed-size pixel
        // payload plus the small struct header (frame U32 + row/rowCount/
        // width U16 = 10 B). Round to FRAME_CHUNK_BYTES + 16 to cover
        // serialization framing without overcounting.
        m_frameBytesThisWindow += static_cast<U32>(Doom::FRAME_CHUNK_BYTES) + 16U;
    }

    // The palette (768 B, negligible vs 256 KB frames) is emitted
    // every frame rather than on change, so the ground converges on
    // the active palette regardless of when it attached. Melt replays
    // use the live palette (palette swaps mid-wipe are not preserved).
    Doom::Palette pal;
    pal.set_generation(m_paletteGeneration);
    U8* const dest = pal.get_rgb();
    (void)::memcpy(dest, m_pendingPalette, sizeof(m_pendingPalette));
    this->tlmWrite_PaletteOut(pal);
    m_frameBytesThisWindow += static_cast<U32>(Doom::PALETTE_BYTES) + 16U;
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
    // Never block the rate-group thread. Instead treat a sleep request
    // as virtual time passing: advance the clock DG_GetTicksMs reports
    // by the requested amount. The only in-engine sleep loops (the
    // screen-wipe melt in D_Display, and TryRunTics' wait loop, which
    // singletics mode never reaches) poll I_GetTime between I_Sleep(1)
    // calls, so advancing virtual time lets them run to completion
    // immediately instead of stalling the 35 Hz cycle.
    m_virtualSleepMs += ms;
}

U32 DoomEngine::platformGetTicksMs() {
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
