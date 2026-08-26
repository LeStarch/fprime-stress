// ======================================================================
// \title  FrameTlmProcessorTests.cpp
// \brief  GoogleTest cases for the FrameTlmProcessor unit tests.
//
// Kept in its own translation unit: each generated GTestBase header
// defines ASSERT_EVENTS_* macros, and components sharing an event name
// (InvalidFrame) would collide if included together.
// ======================================================================

#include "Doom/test/ut/FrameTlmProcessorTester.hpp"

#include <memory>

TEST(TlmProcessor, EmitsOneChannelPerRow) {
    // Heap-allocated: the frame buffers are too large for the stack.
    auto tester = std::make_unique<Doom::FrameTlmProcessorTester>();
    tester->testEmitsOneChannelPerRow();
}

TEST(TlmProcessor, EmitsFullResolutionRows) {
    auto tester = std::make_unique<Doom::FrameTlmProcessorTester>();
    tester->testEmitsFullResolutionRows();
}

TEST(TlmProcessor, ReEmitsPalette) {
    auto tester = std::make_unique<Doom::FrameTlmProcessorTester>();
    tester->testReEmitsPalette();
}

TEST(TlmProcessor, RejectsOversizedDimensions) {
    auto tester = std::make_unique<Doom::FrameTlmProcessorTester>();
    tester->testRejectsOversizedDimensions();
}

TEST(TlmProcessor, RejectsShortBuffer) {
    auto tester = std::make_unique<Doom::FrameTlmProcessorTester>();
    tester->testRejectsShortBuffer();
}
