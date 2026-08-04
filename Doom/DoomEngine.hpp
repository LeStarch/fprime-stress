// ======================================================================
// \title  DoomEngine.hpp
// \brief  F Prime component that wraps the open-source DOOM engine.
//
// The engine is driven entirely from the rate-group thread that calls
// the schedIn port: one Tick of doomgeneric is executed per call. The
// only other thread that ever touches component state is the command-
// dispatch thread, which writes into the mutex-guarded key queue and
// performs the start/stop handoff: forceStart first waits for any
// in-flight schedIn tick to finish (m_tickInProgress rendezvous),
// publishes engine state, then release-stores the atomic
// m_engineRunning flag, which schedIn_handler acquire-loads before
// touching any engine state.
//
// No worker thread is spawned and the tick path never sleeps - the
// rate group is the sole pacing mechanism (forceStart's bounded
// rendezvous wait runs on the caller's thread, never the rate-group
// thread). This makes execution deterministic and avoids any need for
// OS-specific scheduling primitives beyond the OSAL mutex.
// ======================================================================
#ifndef Doom_DoomEngine_HPP
#define Doom_DoomEngine_HPP

#include "Doom/DoomEngineComponentAc.hpp"
#include "Doom/FppConstantsAc.hpp"
#include <Os/Mutex.hpp>
#include <Os/RawTime.hpp>

#include <atomic>

namespace Doom {

class DoomEngine final : public DoomEngineComponentBase {
    //! Unit-test seam: lets the Tester drive the melt-playback branch
    //! of schedIn_handler without starting the real engine.
    friend class DoomEngineTester;

  public:
    //! Maximum number of pending key events queued for the DOOM engine.
    static constexpr FwSizeType KEY_QUEUE_CAPACITY = 64;

    //! Width of the DOOM frame in pixels.
    static constexpr U16 FRAME_WIDTH = 640;

    //! Height of the DOOM frame in scanlines.
    static constexpr U16 FRAME_HEIGHT = 400;

    //! Total bytes in one palette-indexed DOOM frame.
    static constexpr U32 FRAME_BYTES = static_cast<U32>(FRAME_WIDTH) * static_cast<U32>(FRAME_HEIGHT);

    //! Number of rows of a frame packed into one FrameChunk sample.
    static constexpr U16 ROWS_PER_CHUNK = 5;

    //! Maximum length of the IWAD path that may be supplied to the engine.
    static constexpr FwSizeType WAD_PATH_MAX = 256;

    //! Capacity (in frames) of the screen-wipe melt playback buffer.
    //! The melt animation spans roughly 40-70 engine draws; 80 gives
    //! margin above the observed worst case while bounding the static
    //! footprint to 80 x 256 KB = ~20.5 MB. Overflowing frames are
    //! dropped and counted in FramesDropped (the wipe then cuts to the
    //! live frame early).
    static constexpr FwSizeType MELT_QUEUE_CAPACITY = 80;

  public:
    explicit DoomEngine(const char* compName);
    ~DoomEngine() override;

    //! Set the path to the IWAD that should be passed to
    //! doomgeneric_Create when the engine starts. Must be called before
    //! the Start command is dispatched.
    void setWadPath(const char* wadPath);

    //! Accessor used by the extern "C" DG_* platform glue to reach back
    //! into the active component instance. There is exactly one Doom
    //! component instance per deployment by design.
    static DoomEngine* getInstance();

    // ------------------------------------------------------------------
    // Platform-glue callbacks invoked from extern "C" DG_* functions.
    // All run on the rate-group thread inside doomgeneric_Tick.
    // ------------------------------------------------------------------

    //! Called from DG_Init exactly once.
    void platformInit();

    //! Called from DG_DrawFrame at the end of each rendered DOOM frame.
    //! Emits FrameChunk telemetry and (if changed) the active palette.
    void platformDrawFrame();

    //! Called from DG_SleepMs. Advances virtual time rather than
    //! blocking: the rate group provides real pacing, and in-engine
    //! sleep/poll loops (e.g. the screen-wipe melt) complete without
    //! stalling the rate-group thread.
    void platformSleepMs(U32 ms);

    //! Called from DG_GetTicksMs to feed the DOOM timer subsystem.
    U32 platformGetTicksMs();

    //! Called from DG_GetKey to drain the next queued key event. Returns
    //! true if an event was returned, false otherwise. Non-blocking.
    bool platformGetKey(bool& pressed, U8& code);

    //! Called from DG_SetWindowTitle. The deployment has no real window
    //! so the title is intentionally ignored.
    void platformSetTitle(const char* title);

    //! Programmatic engine bring-up. Identical to the Start command
    //! except no cmdResponse is emitted. Intended for the autoStart
    //! path in Main.cpp where the binary is launched headless without
    //! a GDS to dispatch the Start command. Safe to call while the
    //! rate groups are running: it rendezvouses with any in-flight
    //! schedIn tick before touching engine state. Returns true on
    //! success.
    bool forceStart();

  private:
    // ------------------------------------------------------------------
    // Handler implementations
    // ------------------------------------------------------------------

    void schedIn_handler(FwIndexType portNum, U32 context) override;

    void Start_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void Stop_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) override;
    void KeyTap_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Doom::DoomKey key) override;
    void KeyDown_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Doom::DoomKey key) override;
    void KeyUp_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, Doom::DoomKey key) override;
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

    //! Enqueue one (pressed, code) key event under m_keyMutex. Returns
    //! true on success, false if the queue was full.
    bool enqueueKey(bool pressed, U8 code);

    //! Emit one full frame as FrameOut chunk telemetry plus the active
    //! palette. src holds FRAME_BYTES of 8-bit palette indices;
    //! frameNumber is stamped into every chunk of the emission.
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
    // FrameOutNN dispatch table.
    //
    // Each FrameChunk position has its own telemetry channel id
    // (FrameOut00 .. FrameOut79). The dispatch table maps the chunk
    // index to the matching tlmWrite_FrameOutNN member-function pointer.
    // Lives inside the class because the base-class tlmWrite_FrameOutNN
    // methods are protected; only the derived class can take their
    // address.
    // ------------------------------------------------------------------

    using FrameOutWriter = void (DoomEngineComponentBase::*)(
        const Doom::FrameChunk&, Fw::Time) const;
    static const FrameOutWriter kFrameOutWriters[Doom::CHUNKS_PER_FRAME];

    // ------------------------------------------------------------------
    // Engine-thread state (rate-group thread only).
    // ------------------------------------------------------------------

    //! Most recently captured palette (R0,G0,B0,...).
    U8 m_pendingPalette[256 * 3];
    //! Counter incremented whenever the engine swaps palettes.
    U32 m_paletteGeneration;

    //! Total frames produced by the engine.
    U32 m_framesProduced;

    //! True once Start has fired and doomgeneric_Create has returned.
    //! Release-stored by the command thread, acquire-loaded by the
    //! rate-group thread; carries the start-state publication.
    std::atomic<bool> m_engineRunning;

    //! True while schedIn_handler is executing. Set before the handler
    //! reads m_engineRunning; forceStart waits for it to clear before
    //! mutating engine state (see the rendezvous in forceStart).
    std::atomic<bool> m_tickInProgress;

    //! True once doomgeneric_Create has run. The vendored engine's
    //! initialisation is one-shot, so Create is never invoked twice.
    bool m_engineCreated;

    //! Serializes concurrent forceStart callers (autoStart thread vs
    //! a ground Start command).
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
    //! under m_keyMutex by every enqueueKey() call (command and
    //! parallel-port paths both go through enqueueKey).
    U32 m_inputEventsThisWindow;
    U32 m_inputBytesThisWindow;

    // ------------------------------------------------------------------
    // Rate-window state (rate-group thread only).
    // Used to compute FrameRateHz / FrameDataRateBps and to publish the
    // input rates harvested from m_inputEventsThisWindow etc.
    // ------------------------------------------------------------------

    //! Total scheduler ticks since engine start.
    U32 m_schedTicks;
    //! Frames produced inside the current rate window.
    U32 m_framesThisWindow;
    //! Bytes of FrameOut + PaletteOut emitted inside the current window.
    U32 m_frameBytesThisWindow;

    //! Virtual milliseconds accumulated by platformSleepMs; added to
    //! the real elapsed time reported by platformGetTicksMs.
    U32 m_virtualSleepMs;

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

    // ------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------

    //! Configured WAD path. Empty string means "let DOOM auto-search".
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
