// ======================================================================
// \title  DoomTester.hpp
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
    static constexpr FwEnumStoreType TEST_INSTANCE_QUEUE_DEPTH = 16;
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
    void testSchedInWhenEngineOff();

  private:
    // Wiring and init provided by auto-generated helpers.
    void connectPorts();
    void initComponents();

    FwSizeType drainKeys(bool* pressedOut, U8* codeOut, FwSizeType maxEvents);

    DoomEngine component;
};

}  // namespace Doom

#endif
