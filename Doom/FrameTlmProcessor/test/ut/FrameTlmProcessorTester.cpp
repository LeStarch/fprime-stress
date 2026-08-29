// ======================================================================
// \title  FrameTlmProcessorTester.cpp
// \brief  Unit-test harness for the FrameTlmProcessor component.
// ======================================================================

#include "Doom/FrameTlmProcessor/test/ut/FrameTlmProcessorTester.hpp"

#include <cstring>

namespace Doom {

FrameTlmProcessorTester::FrameTlmProcessorTester()
    : FrameTlmProcessorGTestBase("FrameTlmProcessorTester", FrameTlmProcessorTester::MAX_HISTORY_SIZE),
      component("FrameTlmProcessor") {
    this->initComponents();
    this->connectPorts();
    (void)::memset(m_pixels, 0, sizeof(m_pixels));
}

FrameTlmProcessorTester::~FrameTlmProcessorTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void FrameTlmProcessorTester::sendFrame(U32 frameNumber, U16 width, U16 height, U32 bufferSize) {
    // Position-dependent pattern so any offset bug fails the checks.
    const U32 bytes = static_cast<U32>(width) * static_cast<U32>(height);
    for (U32 i = 0; i < bytes; i++) {
        m_pixels[i] = static_cast<U8>(i % 251U);
    }
    Fw::Buffer buffer(m_pixels, bufferSize);
    this->invoke_to_frameIn(0, frameNumber, width, height, buffer);
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void FrameTlmProcessorTester::testEmitsOneChannelPerRow() {
    // A configured downsampled frame: rows 0..h-1 each get their own
    // channel; rows h and beyond stay silent.
    const U16 w = static_cast<U16>(Doom::DOWNSAMPLED_WIDTH);
    const U16 h = static_cast<U16>(Doom::DOWNSAMPLED_HEIGHT);
    this->sendFrame(5U, w, h, static_cast<U32>(w) * static_cast<U32>(h));

    ASSERT_TLM_SIZE(h);
    ASSERT_TLM_FrameRow000_SIZE(1);
    if (h < Doom::FRAME_HEIGHT) {
        ASSERT_TLM_FrameRow399_SIZE(0);
    } else {
        ASSERT_TLM_FrameRow399_SIZE(1);
    }

    // The first emitted row carries the right metadata and payload.
    const Doom::FrameRow& first = this->tlmHistory_FrameRow000->at(0).arg;
    ASSERT_EQ(first.get_frame(), 5U);
    ASSERT_EQ(first.get_row(), 0U);
    ASSERT_EQ(first.get_width(), w);
    for (U32 i = 0; i < w; i++) {
        ASSERT_EQ(first.get_pixels()[i], static_cast<U8>(i % 251U)) << "row 0 pixel " << i;
    }
    ASSERT_EVENTS_InvalidFrame_SIZE(0);
}

void FrameTlmProcessorTester::testReEmitsPalette() {
    Doom::Palette pal;
    pal.set_generation(4U);
    this->invoke_to_paletteIn(0, pal);
    ASSERT_TLM_PaletteOut_SIZE(1);
    ASSERT_EQ(this->tlmHistory_PaletteOut->at(0).arg.get_generation(), 4U);
}

void FrameTlmProcessorTester::testRejectsOversizedDimensions() {
    const U16 w = static_cast<U16>(Doom::DOWNSAMPLED_WIDTH);

    // Height beyond the modeled channels - drop with an event.
    this->sendFrame(1U, w, 401U, FRAME_BYTES);
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_InvalidFrame_SIZE(1);
    ASSERT_EVENTS_InvalidFrame(0, w, 401U);

    // Width other than the configured row width - drop with an event.
    this->sendFrame(1U, static_cast<U16>(w + 1U), 100U, FRAME_BYTES);
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_InvalidFrame_SIZE(2);
}

void FrameTlmProcessorTester::testRejectsShortBuffer() {
    const U16 w = static_cast<U16>(Doom::DOWNSAMPLED_WIDTH);
    const U16 h = static_cast<U16>(Doom::DOWNSAMPLED_HEIGHT);
    this->sendFrame(1U, w, h, (static_cast<U32>(w) * static_cast<U32>(h)) - 1U);
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_InvalidFrame_SIZE(1);
}

}  // namespace Doom
