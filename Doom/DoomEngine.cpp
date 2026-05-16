// ======================================================================
// \title  Doom.cpp
// \brief  F Prime DOOM engine wrapper - implementation.
//
// The engine is driven from a Svc.Sched input port. The schedIn handler
// invokes doomgeneric_Tick(); upstream doomgeneric calls back into the
// extern "C" DG_* hooks at the bottom of this file, which delegate to
// the singleton DoomEngine instance.
//
// Cross-thread communication is limited to the key queue: command
// handlers run on the active component thread and only enqueue key
// events under m_keyMutex; the rate-group thread drains the queue
// inside DG_GetKey. No other state is shared across threads.
// ======================================================================
#include "Doom/DoomEngine.hpp"

#include "Doom/FppConstantsAc.hpp"

#include <Fw/Types/Assert.hpp>
#include <Fw/Time/TimeInterval.hpp>

#include <cstring>

extern "C" {
#include "Doom/doomgeneric/doomgeneric.h"
struct color {
    unsigned b : 8;
    unsigned g : 8;
    unsigned r : 8;
    unsigned a : 8;
};
extern struct color colors[256];
}  // extern "C"

namespace Doom {

DoomEngine* DoomEngine::s_instance = nullptr;

// ----------------------------------------------------------------------
// Construction / destruction
// ----------------------------------------------------------------------

DoomEngine::DoomEngine(const char* compName)
    : DoomEngineComponentBase(compName),
      m_paletteGeneration(0),
      m_lastEmittedPaletteGeneration(0),
      m_framesProduced(0),
      m_engineRunning(false),
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
      m_frameBytesThisWindow(0) {
    FW_ASSERT(s_instance == nullptr);
    s_instance = this;

    (void)::memset(m_pendingPalette, 0, sizeof(m_pendingPalette));
    (void)::memset(m_keyQueue, 0, sizeof(m_keyQueue));
    (void)::memset(m_wadPath, 0, sizeof(m_wadPath));
    (void)::memset(m_argvStorage, 0, sizeof(m_argvStorage));
    (void)::memset(m_argvPointers, 0, sizeof(m_argvPointers));
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
    const FwSizeType copy = (len < (WAD_PATH_MAX - 1U)) ? len : (WAD_PATH_MAX - 1U);
    (void)::memcpy(m_wadPath, wadPath, copy);
    m_wadPath[copy] = '\0';
}

// ----------------------------------------------------------------------
// schedIn: one Tick per rate-group invocation
// ----------------------------------------------------------------------

void DoomEngine::schedIn_handler(FwIndexType portNum, U32 context) {
    if (!m_engineRunning) {
        // Engine not started - publish IDLE state heartbeat.
        this->tlmWrite_State(EngineState::OFF);
        return;
    }
    // Drive one DOOM frame of game logic. upstream doomgeneric will
    // call back into DG_GetKey / DG_DrawFrame on this same thread.
    doomgeneric_Tick();

    // Refresh derived telemetry. FrameOut / PaletteOut are emitted from
    // DG_DrawFrame which runs inside Tick above.
    this->tlmWrite_State(EngineState::RUNNING);
    this->tlmWrite_FrameCount(m_framesProduced);

    m_keyMutex.lock();
    const U8 depth = static_cast<U8>(m_keyQueueCount);
    const U32 dropped = m_keysDropped;
    m_keyMutex.unLock();
    this->tlmWrite_KeyQueueDepth(depth);
    this->tlmWrite_KeyEventsDropped(dropped);
    this->tlmWrite_FramesDropped(0U);  // No frame drops in pull model.

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
    if (m_engineRunning) {
        this->log_WARNING_LO_AlreadyRunning();
        return false;
    }
    this->tlmWrite_State(EngineState::STARTING);

    // Initialise reference time used by DG_GetTicksMs.
    const Os::RawTime::Status rt = m_engineStart.now();
    m_engineStartValid = (rt == Os::RawTime::Status::OP_OK);

    // doomgeneric caches argv (as myargv) and walks it later from
    // M_CheckParm, so both the pointer array and the backing strings
    // must outlive the call. Both live in DoomEngine members.
    const int argc = this->buildEngineArgv(m_argvPointers,
                                           static_cast<int>(FW_NUM_ARRAY_ELEMENTS(m_argvPointers)));
    doomgeneric_Create(argc, const_cast<char**>(m_argvPointers));

    m_engineRunning = true;
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
    if (m_engineRunning) {
        m_engineRunning = false;
        this->log_ACTIVITY_HI_EngineStopped();
    }
    this->tlmWrite_State(EngineState::OFF);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void DoomEngine::KeyTap_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Doom::DoomKey key) {
    const U8 code = static_cast<U8>(key.e);
    const bool downOk = this->enqueueKey(true, code);
    const bool upOk = this->enqueueKey(false, code);
    const Fw::CmdResponse response = (downOk && upOk) ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR;
    this->cmdResponse_out(opCode, cmdSeq, response);
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
    const U8 code = static_cast<U8>(key.e);
    (void)this->enqueueKey(true, code);
    (void)this->enqueueKey(false, code);
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

bool DoomEngine::enqueueKey(bool pressed, U8 code) {
    bool ok = false;
    bool emitOverflow = false;
    m_keyMutex.lock();
    // Every received event counts toward the input rate windows,
    // regardless of whether the queue had room. 2 bytes per event
    // (pressed flag byte + key code byte) - documented in Telemetry.fppi.
    m_inputEventsThisWindow++;
    m_inputBytesThisWindow += 2U;
    if (m_keyQueueCount < KEY_QUEUE_CAPACITY) {
        const U16 entry = static_cast<U16>((pressed ? (1U << 8) : 0U) | static_cast<U16>(code));
        m_keyQueue[m_keyQueueTail] = entry;
        m_keyQueueTail = (m_keyQueueTail + 1U) % KEY_QUEUE_CAPACITY;
        m_keyQueueCount++;
        m_overflowReported = false;
        ok = true;
    } else {
        m_keysDropped++;
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
    chunk.set_frame(m_framesProduced);
    U8* const chunkPixels = chunk.get_pixels();

    for (U16 currentRow = 0; currentRow < FRAME_HEIGHT;
         currentRow = static_cast<U16>(currentRow + ROWS_PER_CHUNK)) {
        const U32 offset = static_cast<U32>(currentRow) * static_cast<U32>(FRAME_WIDTH);
        (void)::memcpy(chunkPixels, &src[offset], Doom::FRAME_CHUNK_BYTES);
        chunk.set_row(currentRow);
        this->tlmWrite_FrameOut(chunk);
        // Account for on-wire bytes of this chunk: the fixed-size pixel
        // payload plus the small struct header (frame U32 + row/rowCount/
        // width U16 = 10 B). Round to FRAME_CHUNK_BYTES + 16 to cover
        // serialization framing without overcounting.
        m_frameBytesThisWindow += static_cast<U32>(Doom::FRAME_CHUNK_BYTES) + 16U;
    }

    if (m_paletteGeneration != m_lastEmittedPaletteGeneration) {
        Doom::Palette pal;
        pal.set_generation(m_paletteGeneration);
        U8* const dest = pal.get_rgb();
        (void)::memcpy(dest, m_pendingPalette, sizeof(m_pendingPalette));
        this->tlmWrite_PaletteOut(pal);
        m_frameBytesThisWindow += static_cast<U32>(Doom::PALETTE_BYTES) + 16U;
        m_lastEmittedPaletteGeneration = m_paletteGeneration;
    }
}

void DoomEngine::capturePaletteIfChanged() {
    bool changed = false;
    for (FwSizeType i = 0; i < 256; ++i) {
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

void DoomEngine::platformSleepMs(U32 ms) const {
    // Intentional no-op: rate group provides pacing.
    (void)ms;
}

U32 DoomEngine::platformGetTicksMs() {
    if (!m_engineStartValid) {
        return 0U;
    }
    Os::RawTime current;
    const Os::RawTime::Status nowStatus = current.now();
    if (nowStatus != Os::RawTime::Status::OP_OK) {
        return 0U;
    }
    U32 deltaUsec = 0U;
    const Os::RawTime::Status diffStatus = current.getDiffUsec(m_engineStart, deltaUsec);
    if (diffStatus != Os::RawTime::Status::OP_OK) {
        return 0U;
    }
    return deltaUsec / 1000U;
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
        const FwSizeType len = static_cast<FwSizeType>(::strlen(m_wadPath));
        const FwSizeType copy = (len < (WAD_PATH_MAX - 1U)) ? len : (WAD_PATH_MAX - 1U);
        (void)::memcpy(m_argvStorage[argc], m_wadPath, copy);
        m_argvStorage[argc][copy] = '\0';
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
