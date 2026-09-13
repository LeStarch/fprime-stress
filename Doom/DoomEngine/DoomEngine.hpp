// ======================================================================
// \title  DoomEngine.hpp
// \brief  F Prime component that wraps the open-source DOOM engine.
//
// The engine is driven entirely from the rate-group thread that calls
// the schedIn port: each call runs one doomgeneric Tick, or replays
// one buffered melt frame if a screen wipe is pending. Start and Reset
// only validate and latch a request (m_startRequested /
// m_resetRequested); the rate-group thread consumes the latch at the
// top of its next tick and performs the transition itself, so engine
// state is mutated by exactly one thread. Stop clears the atomic
// m_engineRunning flag, which the tick loads before touching the engine.
//
// No worker thread is spawned and no path ever sleeps - the rate
// group is the sole pacing mechanism. Cross-thread state is limited
// to the OSAL mutexes and the std::atomic members.
// ======================================================================
#ifndef Doom_DoomEngine_HPP
#define Doom_DoomEngine_HPP

#include <Os/File.hpp>
#include <Os/Mutex.hpp>
#include <Os/RawTime.hpp>
#include "Doom/DoomConfig/FppConstantsAc.hpp"
#include "Doom/DoomEngine/DoomEngineComponentAc.hpp"
#include "Doom/InitStatusEnumAc.hpp"
#include "Doom/KeyQueueStatusEnumAc.hpp"
#include "Doom/RequestStatusEnumAc.hpp"
#include "Doom/WadStatusEnumAc.hpp"

#include <atomic>
#include <csetjmp>

namespace Doom {

class DoomEngine final : public DoomEngineComponentBase {
    //! Unit-test seam: lets the Tester drive the melt-playback branch
    //! of schedIn_handler without starting the real engine.
    friend class DoomEngineTester;

  public:
    //! Maximum number of pending key events queued for the DOOM engine.
    static constexpr FwSizeType KEY_QUEUE_CAPACITY = 64;

    //! Width of the DOOM frame in pixels (DoomConfig.fpp; checked
    //! against DOOMGENERIC_RESX at compile time).
    static constexpr U16 FRAME_WIDTH = static_cast<U16>(Doom::FRAME_WIDTH);

    //! Height of the DOOM frame in scanlines (DoomConfig.fpp; checked
    //! against DOOMGENERIC_RESY at compile time).
    static constexpr U16 FRAME_HEIGHT = static_cast<U16>(Doom::FRAME_HEIGHT);

    //! Total bytes in one palette-indexed DOOM frame.
    static constexpr U32 FRAME_BYTES = static_cast<U32>(FRAME_WIDTH) * static_cast<U32>(FRAME_HEIGHT);

    //! Maximum length of the IWAD path that may be supplied to the engine.
    static constexpr FwSizeType WAD_PATH_MAX = 256;

    //! Maximum length of an engine fault message (matches EngineFault).
    static constexpr FwSizeType FAULT_MESSAGE_MAX = 128;

    //! Capacity (in frames) of the screen-wipe melt playback buffer,
    //! configured in DoomConfig.fpp. Each slot costs FRAME_BYTES of
    //! static footprint; overflowing frames are dropped and counted in
    //! FramesDropped (the wipe then cuts to the live frame early).
    static constexpr FwSizeType MELT_QUEUE_CAPACITY = Doom::MELT_QUEUE_CAPACITY;

    //! Number of RGB entries in the DOOM palette.
    static constexpr FwSizeType PALETTE_ENTRIES = Doom::PALETTE_BYTES / 3;

  public:
    explicit DoomEngine(const char* compName);
    ~DoomEngine() override;

    //! Set the path to the IWAD passed to doomgeneric_Create. Must be
    //! called before initEngine. A path that does not fit WAD_PATH_MAX
    //! is rejected (WadPathRejected event, path left unset) rather than
    //! truncated.
    InitStatus setWadPath(const char* wadPath);

    //! Initialization-time engine bring-up: opens the WAD and runs
    //! doomgeneric_Create (the engine's one-shot init, including all
    //! of its heap allocation). Call once from topology setup, before
    //! the rate groups start; never from the rate-group thread. Any
    //! result other than OK (WadUnavailable / WadInvalid / EngineFault
    //! event, State FAILED) means Start will be rejected.
    InitStatus initEngine();

    //! Terminal engine fault, entered from the engine's I_Error/I_Quit
    //! via the extern "C" glue. Records the fault (EngineFault event,
    //! State FAILED, engine stopped) and longjmps out of the
    //! doomgeneric_Create / doomgeneric_Tick call that was in progress.
    //! Asserts if no engine call is in progress. Does not return.
    //!
    //! Fault-containment invariants (see the longjmp discussion in
    //! DoomEngine.cpp): the jump unwinds only C frames of the engine
    //! plus the trampoline; no C++ object with a destructor may be
    //! live between the setjmp and this call, no lock may be held, and
    //! the engine is never re-entered after a fault (FAILED is terminal).
    [[noreturn]] void engineFault(const char* message);

    //! Accessor used by the extern "C" DG_* platform glue to reach back
    //! into the component instance. There is exactly one Doom
    //! component instance per deployment by design.
    static DoomEngine* getInstance();

    // ------------------------------------------------------------------
    // Platform-glue callbacks invoked from extern "C" DG_* functions.
    // All run on the rate-group thread inside doomgeneric_Tick.
    // ------------------------------------------------------------------

    //! Called from DG_Init exactly once.
    void platformInit();

    //! Called from DG_DrawFrame at the end of each rendered DOOM frame.
    //! The first draw of a tick is sent out the frameOut port with the
    //! active palette; subsequent draws in the same tick (a screen
    //! wipe) are buffered into the melt ring, or dropped and counted in
    //! FramesDropped when the ring is full.
    void platformDrawFrame();

    //! Called from DG_SleepMs. Advances virtual time rather than
    //! blocking: the rate group provides real pacing, and in-engine
    //! sleep/poll loops (e.g. the screen-wipe melt) complete without
    //! stalling the rate-group thread.
    void platformSleepMs(U32 ms);

    //! Called from DG_GetTicksMs to feed the DOOM timer subsystem.
    U32 platformGetTicksMs();

    //! Called from DG_GetKey to drain the next queued key event. Returns
    //! true if an event was returned, false if the queue is empty or a
    //! tic barrier was consumed. Non-blocking.
    bool platformGetKey(bool& pressed, U8& code);

    //! Called from DG_SetWindowTitle. The deployment has no real window
    //! so the title is intentionally ignored.
    void platformSetTitle(const char* title);

    //! Programmatic engine start. Identical to the Start command
    //! except no cmdResponse is emitted. Intended for the autoStart
    //! path in Main.cpp where the binary is launched headless without
    //! a GDS to dispatch the Start command. Validates (engine created,
    //! not faulted, not running), publishes STARTING and latches the
    //! request; the rate-group thread performs the start on its next
    //! schedIn tick (EngineStarted, RUNNING). Never blocks. Any result
    //! other than ACCEPTED was reported via StartRejected.
    RequestStatus forceStart();

  private:
    // ------------------------------------------------------------------
    // Handler implementations
    // ------------------------------------------------------------------

    void schedIn_handler(FwIndexType portNum, U32 context) override;

    void Start_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void Stop_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void Reset_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void KeyTap_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Doom::DoomKey& key) override;
    void KeyDown_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Doom::DoomKey& key) override;
    void KeyUp_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Doom::DoomKey& key) override;
    void RawKey_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, bool pressed, U8 code) override;

    //! Parallel-to-command input port handlers. Same enqueue path as the
    //! command handlers - intended for sensor adapters, sequencer
    //! macros, and unit-test drivers that don't want to bounce through
    //! the command dispatcher.
    void keyTapIn_handler(FwIndexType portNum, const Doom::DoomKey& key) override;
    void keyDownIn_handler(FwIndexType portNum, const Doom::DoomKey& key) override;
    void keyUpIn_handler(FwIndexType portNum, const Doom::DoomKey& key) override;
    void rawKeyIn_handler(FwIndexType portNum, bool pressed, U8 code) override;

    // ------------------------------------------------------------------
    // Internal helpers
    // ------------------------------------------------------------------

    //! Allow-list check for raw key codes: only DoomKey enumerators
    //! reach the engine. Returns QUEUED when allowed.
    static KeyQueueStatus validateKeyCode(U8 code);

    //! Validate a raw code, then enqueueKey; emits KeyRejected on failure.
    KeyQueueStatus enqueueRawKey(bool pressed, U8 code);

    //! Enqueue one (pressed, code) key event under m_keyMutex.
    KeyQueueStatus enqueueKey(bool pressed, U8 code);

    //! Enqueue down, tic barrier, up atomically: all three entries are
    //! queued or none is, so an overflow cannot leave a key stuck down.
    KeyQueueStatus enqueueKeyTap(U8 code);

    //! Shared enqueue core: queues all entries or none, updating the
    //! rate-window counters and overflow reporting under m_keyMutex.
    KeyQueueStatus enqueueKeyEvents(const U16* entries, FwSizeType count);

    //! Map a key-queue status onto the command response for the key commands.
    static Fw::CmdResponse keyResponse(KeyQueueStatus status);

    //! Record and emit the State telemetry channel.
    void publishState(EngineState state);

    //! State/telemetry half of engineFault (no longjmp).
    void recordEngineFault(const char* message);

    //! Structural check of an opened IWAD (magic, directory bounds,
    //! required lumps) so malformed files are rejected before the
    //! engine can reach an I_Error on them. Returns VALID or the
    //! check that failed.
    WadStatus validateWad(Os::File& wad);

    //! Shared Start/Reset precondition: the engine must have been
    //! created and not have faulted. Caller holds m_startMutex.
    RequestStatus engineAvailable() const;

    //! Rate-group thread: consume a latched Start - rebase the engine
    //! clock, discard melt/draw state, set m_engineRunning, publish.
    void applyStart();

    //! Pack one key event into the queue's wire format: bit 8 is the
    //! pressed flag, bits 0-7 the key code (unpacked by platformGetKey).
    static constexpr U16 packKeyEntry(bool pressed, U8 code) {
        return static_cast<U16>((pressed ? (1U << 8) : 0U) | static_cast<U16>(code));
    }

    //! Queue marker that ends the current tic's DG_GetKey drain, so the
    //! entries after it are only seen by the engine on the next tic.
    static constexpr U16 KEY_ENTRY_TIC_BARRIER = static_cast<U16>(1U << 9);

    //! Send one full frame out the frameOut port (after the palette on
    //! paletteOut). src holds FRAME_BYTES of 8-bit palette indices; it
    //! is copied into m_frameBuffer, which downstream may mutate.
    void emitFrame(const U8* src, U32 frameNumber);

    //! Capture the active DOOM palette out of the engine's color table.
    //! Bumps m_paletteGeneration if anything changed. Rate-group thread.
    void capturePaletteIfChanged();

    //! Build a NUL-terminated argv vector for doomgeneric_Create from
    //! the configured WAD path. Storage lives inside the component.
    //! Returns argc.
    int buildEngineArgv(const char** argv, int maxArgv);

  private:
    // ------------------------------------------------------------------
    // Engine-thread state (rate-group thread only).
    // ------------------------------------------------------------------

    //! Most recently captured palette (R0,G0,B0,...).
    U8 m_pendingPalette[Doom::PALETTE_BYTES];
    //! Counter incremented whenever the engine swaps palettes.
    U32 m_paletteGeneration;

    //! Total frames produced by the engine.
    FwSizeType m_framesProduced;

    //! Last state published via publishState; the not-running
    //! heartbeat re-emits it (preserving FAILED) except a stale
    //! RUNNING, which it self-heals to OFF.
    std::atomic<EngineState::T> m_lastState;

    //! True while the engine is being driven by the rate group. Set
    //! only by applyStart (rate-group thread); cleared by Stop/fault.
    std::atomic<bool> m_engineRunning;

    //! Set by forceStart after validation; consumed by the rate-group
    //! thread at the top of its next tick (applyStart). Cleared by Stop.
    std::atomic<bool> m_startRequested;

    //! True once doomgeneric_Create has run. The upstream engine's
    //! initialisation is one-shot, so Create is never invoked twice.
    bool m_engineCreated;

    //! True once the engine has faulted (I_Error/I_Quit). Terminal:
    //! engine state is unrecoverable, so Start is rejected thereafter.
    std::atomic<bool> m_engineFaulted;

    //! Fault return point around doomgeneric_Create / doomgeneric_Tick,
    //! valid while m_faultJmpArmed. Only C frames lie between the
    //! setjmp and engineFault's longjmp.
    std::jmp_buf m_faultJmp;
    bool m_faultJmpArmed;

    //! Set by Reset_cmdHandler; consumed by the rate-group thread at
    //! the top of its next running tick, which flushes input state
    //! and returns the game to the title sequence.
    std::atomic<bool> m_resetRequested;

    //! Serializes forceStart/Stop/Reset callers (autoStart thread vs
    //! ground commands) so their checks see a consistent picture.
    Os::Mutex m_startMutex;

    //! Engine start reference time for DG_GetTicksMs.
    Os::RawTime m_engineStart;
    //! True once m_engineStart has been populated.
    bool m_engineStartValid;

    // ------------------------------------------------------------------
    // Cross-thread state (key queue).
    // Written by command-dispatch thread, read by rate-group thread
    // through platformGetKey().
    // ------------------------------------------------------------------

    //! Pending key events. Each entry is (pressed << 8) | code.
    U16 m_keyQueue[KEY_QUEUE_CAPACITY];
    //! Head/tail/count for m_keyQueue.
    FwSizeType m_keyQueueHead;
    FwSizeType m_keyQueueTail;
    FwSizeType m_keyQueueCount;
    //! Mutex guarding m_keyQueue.
    Os::Mutex m_keyMutex;

    //! True if a key-queue overflow event has already been reported
    //! since the last successful enqueue. Read/written under m_keyMutex.
    bool m_overflowReported;
    //! Total key events dropped due to overflow.
    U32 m_keysDropped;

    //! Per-window counters for the input-rate telemetry. Incremented
    //! under m_keyMutex by enqueueKeyEvents() (all command and
    //! parallel-port paths funnel through it).
    U32 m_inputEventsThisWindow;
    U32 m_inputBytesThisWindow;

    // ------------------------------------------------------------------
    // Rate-window state (rate-group thread only).
    // Used to compute FrameRateHz / FrameDataRateBps and to publish the
    // input rates harvested from m_inputEventsThisWindow etc.
    // ------------------------------------------------------------------

    //! Scheduler ticks inside the current rate window.
    U32 m_schedTicks;
    //! Microseconds accumulated inside the current rate window from
    //! the schedIn context (variable-rate mode only).
    U32 m_windowElapsedUsec;
    //! Frames produced inside the current rate window.
    U32 m_framesThisWindow;
    //! Bytes of frameOut + paletteOut emitted inside the current window.
    U32 m_frameBytesThisWindow;

    // ------------------------------------------------------------------
    // Engine clock and melt-playback state (rate-group thread only;
    // reset by applyStart when a latched Start is consumed).
    // ------------------------------------------------------------------

    //! Virtual milliseconds accumulated by platformSleepMs; added to
    //! the real elapsed time reported by platformGetTicksMs.
    U32 m_virtualSleepMs;

    //! True once a nonzero schedIn context (microseconds per tick) has
    //! been seen: the engine clock then advances by tick time instead
    //! of the OS clock, tracking a variable-rate group exactly.
    bool m_useTickTime;

    //! Microseconds of tick time accumulated from the schedIn context.
    //! Seeded from m_realElapsedUsec at mode switch so the clock never
    //! steps backwards.
    U64 m_tickElapsedUsec;

    //! Real microseconds elapsed since Start, accumulated in 64 bits
    //! with m_engineStart rebased on every read so getDiffUsec's U32
    //! range (~71.6 min) is never exceeded.
    U64 m_realElapsedUsec;

    //! Frames drawn by the engine within the current schedIn tick.
    //! Used to emit telemetry for at most one frame per tick.
    U32 m_drawsThisTick;

    //! Ring buffer of screen-wipe melt frames captured during a
    //! multi-draw tick, played back one per cycle by schedIn_handler.
    U8 m_meltFrames[MELT_QUEUE_CAPACITY][FRAME_BYTES];
    //! Engine frame number for each buffered melt frame.
    U32 m_meltFrameNumbers[MELT_QUEUE_CAPACITY];
    //! Index of the oldest buffered melt frame.
    FwSizeType m_meltHead;
    //! Number of buffered melt frames.
    FwSizeType m_meltCount;

    //! Frames dropped because the melt buffer was full.
    U32 m_framesDropped;

    //! Engine-owned copy of the frame sent out frameOut. Downstream
    //! (the downsampler) mutates it in place during the port call, so
    //! the live DG_ScreenBuffer cannot be sent directly.
    U8 m_frameBuffer[FRAME_BYTES];

    // ------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------

    //! Configured WAD path. Must be set before Start: an empty path
    //! is rejected with WadUnavailable (auto-search is not permitted).
    char m_wadPath[WAD_PATH_MAX];

    //! Argv storage for doomgeneric_Create. DOOM's parser caches both
    //! the pointer array (myargv) and the strings indefinitely, so
    //! both must outlive the call.
    char m_argvStorage[8][WAD_PATH_MAX];
    const char* m_argvPointers[8];

    //! Singleton pointer used by extern "C" DG_* glue.
    static DoomEngine* s_instance;
};

}  // namespace Doom

#endif
