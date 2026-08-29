// ======================================================================
// \title  FrameDownsamplerTests.cpp
// \brief  GoogleTest cases for the FrameDownsampler unit tests.
//
// Kept in its own translation unit: each generated GTestBase header
// defines ASSERT_EVENTS_* macros, and components sharing an event name
// (InvalidFrame) would collide if included together.
// ======================================================================

#include "Doom/test/ut/FrameDownsamplerTester.hpp"

#include <memory>

TEST(Downsampler, PassThroughAtX1) {
    // Heap-allocated: the frame buffers are too large for the stack.
    auto tester = std::make_unique<Doom::FrameDownsamplerTester>();
    tester->testPassThroughAtX1();
}

TEST(Downsampler, DecimatesInPlaceX2) {
    auto tester = std::make_unique<Doom::FrameDownsamplerTester>();
    tester->testDecimatesInPlace(Doom::DownsampleFactor::X2);
}

TEST(Downsampler, DecimatesInPlaceX4) {
    auto tester = std::make_unique<Doom::FrameDownsamplerTester>();
    tester->testDecimatesInPlace(Doom::DownsampleFactor::X4);
}

TEST(Downsampler, DecimatesInPlaceX8) {
    auto tester = std::make_unique<Doom::FrameDownsamplerTester>();
    tester->testDecimatesInPlace(Doom::DownsampleFactor::X8);
}

TEST(Downsampler, DecimatesInPlaceX16) {
    auto tester = std::make_unique<Doom::FrameDownsamplerTester>();
    tester->testDecimatesInPlace(Doom::DownsampleFactor::X16);
}

TEST(Downsampler, ForwardsPalette) {
    auto tester = std::make_unique<Doom::FrameDownsamplerTester>();
    tester->testForwardsPalette();
}

TEST(Downsampler, RejectsIndivisibleDimensions) {
    auto tester = std::make_unique<Doom::FrameDownsamplerTester>();
    tester->testRejectsIndivisibleDimensions();
}

TEST(Downsampler, RejectsShortBuffer) {
    auto tester = std::make_unique<Doom::FrameDownsamplerTester>();
    tester->testRejectsShortBuffer();
}

TEST(Downsampler, DefaultsToX2WhenParamUnset) {
    auto tester = std::make_unique<Doom::FrameDownsamplerTester>();
    tester->testDefaultsToX2WhenParamUnset();
}
