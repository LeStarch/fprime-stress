// ======================================================================
// \title  DoomTestMain.cpp
// \brief  GoogleTest entrypoint for the DoomEngine unit tests.
// ======================================================================

#include "Doom/test/ut/DoomEngineTester.hpp"

#include <memory>

TEST(Nominal, CommandsEnqueueKeys) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testCommandsEnqueueKeys();
}

TEST(Nominal, ParallelPortsEnqueueKeys) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testParallelPortsEnqueueKeys();
}

TEST(OffNominal, KeyQueueOverflowEmitsEvent) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testOverflowEmitsEvent();
}

TEST(Nominal, StopRespondsWhenNotRunning) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testStopCommandResponds();
}

TEST(Nominal, SchedInPulseSafeWhenEngineOff) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testSchedInWhenEngineOff();
}

TEST(Nominal, VirtualSleepAdvancesTicks) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testVirtualSleepAdvancesTicks();
}

TEST(Nominal, DrawFrameEmitsFirstDrawAndBuffersMelt) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testDrawFrameEmitsFirstDrawAndBuffersMelt();
}

TEST(Nominal, SchedInPlaysBackMeltFrames) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testSchedInPlaysBackMeltFrames();
}

TEST(OffNominal, MeltOverflowCountsDroppedFrames) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testMeltOverflowCountsDroppedFrames();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
