// ======================================================================
// \title  DoomTestMain.cpp
// \brief  GoogleTest entrypoint for the DoomEngine unit tests.
// ======================================================================

#include "Doom/test/ut/DoomEngineTester.hpp"

TEST(Nominal, CommandsEnqueueKeys) {
    Doom::DoomEngineTester tester;
    tester.testCommandsEnqueueKeys();
}

TEST(Nominal, ParallelPortsEnqueueKeys) {
    Doom::DoomEngineTester tester;
    tester.testParallelPortsEnqueueKeys();
}

TEST(OffNominal, KeyQueueOverflowEmitsEvent) {
    Doom::DoomEngineTester tester;
    tester.testOverflowEmitsEvent();
}

TEST(Nominal, StopRespondsWhenNotRunning) {
    Doom::DoomEngineTester tester;
    tester.testStopCommandResponds();
}

TEST(Nominal, SchedInPulseSafeWhenEngineOff) {
    Doom::DoomEngineTester tester;
    tester.testSchedInWhenEngineOff();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
