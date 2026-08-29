// ======================================================================
// \title  FrameTlmProcessorTests.cpp
// \brief  GoogleTest cases and entrypoint for the FrameTlmProcessor
//         unit tests.
// ======================================================================

#include "Doom/FrameTlmProcessor/test/ut/FrameTlmProcessorTester.hpp"

#include <memory>

TEST(TlmProcessor, EmitsOneChannelPerRow) {
    // Heap-allocated: the frame buffers are too large for the stack.
    auto tester = std::make_unique<Doom::FrameTlmProcessorTester>();
    tester->testEmitsOneChannelPerRow();
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
