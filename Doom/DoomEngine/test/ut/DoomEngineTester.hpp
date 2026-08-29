// ======================================================================
// \title  DoomEngineTester.hpp
// \brief  Unit-test harness for the DoomEngine component.
//
// The tests exercise the F Prime-facing surface of the component
// (commands, parallel input ports, telemetry, events) without ever
// starting the wrapped doomgeneric engine. The Start command path is
// deliberately not invoked here because doomgeneric_Create needs a real
// WAD file on disk and spins up game-loop globals that would persist
// across test cases.
// ======================================================================
#ifndef Doom_DoomEngineTester_HPP
#define Doom_DoomEngineTester_HPP

#include "Doom/DoomEngine.hpp"
#include "Doom/DoomEngineGTestBase.hpp"

namespace Doom {

class DoomEngineTester final : public DoomEngineGTestBase {
  public:
    static constexpr FwIndexType TEST_INSTANCE_ID = 0;
    static constexpr FwSizeType MAX_HISTORY_SIZE = 256;

    DoomEngineTester();
    ~DoomEngineTester();

    // ------------------------------------------------------------------
    // Tests
    // ------------------------------------------------------------------

    void testCommandsEnqueueKeys();
    void testParallelPortsEnqueueKeys();
    void testOverflowEmitsEvent();
    void testStopCommandResponds();
    void testResetRejectsBeforeStart();
    void testResetCommandSetsFlag();
    void testSchedInAppliesReset();
    void testSchedInWhenEngineOff();
    void testVirtualSleepAdvancesTicks();
    void testVariableRateContextAdvancesClock();
    void testDrawFrameEmitsFirstDrawAndBuffersMelt();
    void testSchedInPlaysBackMeltFrames();
    void testMeltOverflowCountsDroppedFrames();
    void testForceStartBusyRendezvousTimesOut();
    void testForceStartWhenAlreadyRunning();
    void testForceStartResumesAfterStop();
    void testStopWhileRunning();
    void testKeyTapAllOrNothing();
    void testStartRejectsMissingWad();
    void testStartRejectsUnconfiguredWad();
    void testStartCommandRejectsWhenRunning();
    void testHeartbeatSelfHealsStaleRunning();

  private:
    // Wiring and init provided by auto-generated helpers.
    void connectPorts();
    void initComponents();

    //! Captures a snapshot of each frameOut invocation: the buffer
    //! contents are only valid during the synchronous call, so the
    //! default history (which stores the Fw::Buffer) is not enough.
    void from_frameOut_handler(FwIndexType portNum,
                               U32 frameNumber,
                               U16 width,
                               U16 height,
                               Fw::Buffer& pixels) override;

    FwSizeType drainKeys(bool* pressedOut, U8* codeOut, FwSizeType maxEvents);

    DoomEngine component;

    // Snapshot of frameOut calls (metadata plus a pixel copy).
    struct FrameCapture {
        U32 frameNumber;
        U16 width;
        U16 height;
        U32 size;
        U8 pixels[DoomEngine::FRAME_BYTES];
    };
    static constexpr FwSizeType MAX_FRAME_CAPTURES = 4;
    FrameCapture m_frameCaptures[MAX_FRAME_CAPTURES];
    FwSizeType m_frameCaptureCount = 0;
};

}  // namespace Doom

#endif
