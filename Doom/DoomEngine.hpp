// ======================================================================
// \title  Doom.hpp
// \brief  F Prime component that wraps the open-source DOOM engine.
//
// The engine is driven entirely from the rate-group thread that calls
// the schedIn port: one Tick of doomgeneric is executed per call. The
// only other thread that ever touches component state is the command-
// dispatch thread, which only writes into the key queue. That queue is
// the single piece of shared state and is guarded by Os::Mutex.
//
// No worker thread is spawned and no internal sleep is performed - the
// rate group is the sole pacing mechanism. This makes execution
// deterministic and avoids any need for OS-specific scheduling
// primitives beyond the OSAL mutex.
// ======================================================================
#ifndef Doom_DoomEngine_HPP
#define Doom_DoomEngine_HPP

#include "Doom/DoomEngineComponentAc.hpp"
#include <Os/Mutex.hpp>
#include <Os/RawTime.hpp>

namespace Doom {

class DoomEngine final : public DoomEngineComponentBase {
  public:
    //! Maximum number of pending key events queued for the DOOM engine.
    static constexpr FwSizeType KEY_QUEUE_CAPACITY = 64;

    //! Width of the DOOM frame in pixels.
    static constexpr U16 FRAME_WIDTH = 320;

    //! Height of the DOOM frame in scanlines.
    static constexpr U16 FRAME_HEIGHT = 200;

    //! Total bytes in one palette-indexed DOOM frame.
    static constexpr U32 FRAME_BYTES = static_cast<U32>(FRAME_WIDTH) * static_cast<U32>(FRAME_HEIGHT);

    //! Number of rows of a frame packed into one FrameChunk sample.
    static constexpr U16 ROWS_PER_CHUNK = 10;

    //! Maximum length of the IWAD path that may be supplied to the engine.
    static constexpr FwSizeType WAD_PATH_MAX = 256;

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

    //! Called from DG_SleepMs. No-op: the rate group provides pacing.
    void platformSleepMs(U32 ms) const;

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
    //! a GDS to dispatch the Start command. Returns true on success.
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
    U8 m_pendingPalette[256 * 3];
    //! Counter incremented whenever the engine swaps palettes.
    U32 m_paletteGeneration;
    //! Palette generation value last sent to the ground.
    U32 m_lastEmittedPaletteGeneration;

    //! Total frames produced by the engine.
    U32 m_framesProduced;

    //! True once Start has fired and doomgeneric_Create has returned.
    bool m_engineRunning;

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

    // ------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------

    //! Configured WAD path. Empty string means "let DOOM auto-search".
    char m_wadPath[WAD_PATH_MAX];

    //! Argv storage for doomgeneric_Create. DOOM's parser keeps the
    //! pointers, so the backing strings must outlive the engine.
    char m_argvStorage[8][WAD_PATH_MAX];

    //! Singleton pointer used by extern "C" DG_* glue.
    static DoomEngine* s_instance;
};

}  // namespace Doom

#endif
