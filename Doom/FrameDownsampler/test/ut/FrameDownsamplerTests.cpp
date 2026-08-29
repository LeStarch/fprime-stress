// ======================================================================
// \title  FrameDownsamplerTests.cpp
// \brief  GoogleTest cases and entrypoint for the FrameDownsampler
//         unit tests.
// ======================================================================

#include "Doom/FrameDownsampler/test/ut/FrameDownsamplerTester.hpp"

#include <memory>

TEST(Downsampler, DecimatesInPlace) {
    // Heap-allocated: the frame buffers are too large for the stack.
    auto tester = std::make_unique<Doom::FrameDownsamplerTester>();
    tester->testDecimatesInPlace();
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

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
