// ======================================================================
// \title  DoomTestMain.cpp
// \brief  GoogleTest entrypoint for the DoomEngine unit tests.
// ======================================================================

#include "Doom/DoomEngine/test/ut/DoomEngineTester.hpp"

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

TEST(OffNominal, ResetRejectsBeforeStart) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testResetRejectsBeforeStart();
}

TEST(Nominal, ResetCommandSetsFlag) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testResetCommandSetsFlag();
}

TEST(Nominal, SchedInAppliesReset) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testSchedInAppliesReset();
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

TEST(OffNominal, StopCancelsPendingStart) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testStopCancelsPendingStart();
}

TEST(Nominal, StartRejectsMissingWad) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testStartRejectsMissingWad();
}

TEST(OffNominal, InitRejectsMalformedWad) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testInitRejectsMalformedWad();
}

TEST(Nominal, RawKeyRejectsUnlistedCode) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testRawKeyRejectsUnlistedCode();
}

TEST(Nominal, SetWadPathRejectsOverlongPath) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testSetWadPathRejectsOverlongPath();
}

TEST(Nominal, StartRejectsUnconfiguredWad) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testStartRejectsUnconfiguredWad();
}

TEST(Nominal, StartCommandRejectsWhenRunning) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testStartCommandRejectsWhenRunning();
}

TEST(Nominal, HeartbeatSelfHealsStaleRunning) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testHeartbeatSelfHealsStaleRunning();
}

TEST(Nominal, KeyTapAllOrNothing) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testKeyTapAllOrNothing();
}

TEST(Nominal, StopWhileRunning) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testStopWhileRunning();
}

TEST(Nominal, ForceStartResumesAfterStop) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testForceStartResumesAfterStop();
}

TEST(OffNominal, ForceStartWhenAlreadyRunning) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testForceStartWhenAlreadyRunning();
}

TEST(OffNominal, StartRejectsWithoutInit) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testStartRejectsWithoutInit();
}

TEST(OffNominal, EngineFaultStopsEngine) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testEngineFaultStopsEngine();
}

TEST(OffNominal, EngineFaultUnwindsToArmedCaller) {
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testEngineFaultUnwindsToArmedCaller();
}

TEST(OffNominal, KeyRejectedThrottleReArmsOnReset) {
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testKeyRejectedThrottleReArmsOnReset();
}

TEST(Nominal, RateTelemetryWindow) {
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testRateTelemetryWindow();
}

TEST(Nominal, ValidateWadWalksChunkedDirectory) {
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testValidateWadWalksChunkedDirectory();
}

TEST(Nominal, VariableRateContextAdvancesClock) {
    // Heap-allocated: the melt frame buffer is too large for the stack.
    auto tester = std::make_unique<Doom::DoomEngineTester>();
    tester->testVariableRateContextAdvancesClock();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
