// ======================================================================
// \title  FrameTlmProcessorTester.cpp
// \brief  Unit-test harness for the FrameTlmProcessor component.
// ======================================================================

#include "Doom/test/ut/FrameTlmProcessorTester.hpp"

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
    // A 320 x 200 (X2-downsampled) frame: rows 0..199 each get their
    // own channel; rows 200+ stay silent.
    const U16 w = 320;
    const U16 h = 200;
    this->sendFrame(5U, w, h, static_cast<U32>(w) * static_cast<U32>(h));

    ASSERT_TLM_SIZE(h);
    ASSERT_TLM_FrameRow000_SIZE(1);
    ASSERT_TLM_FrameRow199_SIZE(1);
    ASSERT_TLM_FrameRow200_SIZE(0);
    ASSERT_TLM_FrameRow399_SIZE(0);

    // First and last emitted rows carry the right metadata and payload.
    const Doom::FrameRow& first = this->tlmHistory_FrameRow000->at(0).arg;
    ASSERT_EQ(first.get_frame(), 5U);
    ASSERT_EQ(first.get_row(), 0U);
    ASSERT_EQ(first.get_width(), w);
    const Doom::FrameRow& last = this->tlmHistory_FrameRow199->at(0).arg;
    ASSERT_EQ(last.get_frame(), 5U);
    ASSERT_EQ(last.get_row(), 199U);
    ASSERT_EQ(last.get_width(), w);
    const U32 lastOffset = 199U * static_cast<U32>(w);
    for (U32 i = 0; i < w; i++) {
        ASSERT_EQ(first.get_pixels()[i], static_cast<U8>(i % 251U)) << "row 0 pixel " << i;
        ASSERT_EQ(last.get_pixels()[i], static_cast<U8>((lastOffset + i) % 251U)) << "row 199 pixel " << i;
    }
    // Bytes beyond the row width are zeroed.
    for (U32 i = w; i < Doom::FRAME_WIDTH; i++) {
        ASSERT_EQ(first.get_pixels()[i], 0U) << "trailing byte " << i;
    }
    ASSERT_EVENTS_InvalidFrame_SIZE(0);
}

void FrameTlmProcessorTester::testEmitsFullResolutionRows() {
    // A full 640 x 400 frame uses every modeled row channel.
    this->sendFrame(1U, Doom::FRAME_WIDTH, Doom::FRAME_HEIGHT, FRAME_BYTES);
    ASSERT_TLM_SIZE(Doom::FRAME_HEIGHT);
    ASSERT_TLM_FrameRow399_SIZE(1);
    const Doom::FrameRow& last = this->tlmHistory_FrameRow399->at(0).arg;
    ASSERT_EQ(last.get_row(), 399U);
    ASSERT_EQ(last.get_width(), Doom::FRAME_WIDTH);
}

void FrameTlmProcessorTester::testReEmitsPalette() {
    Doom::Palette pal;
    pal.set_generation(4U);
    this->invoke_to_paletteIn(0, pal);
    ASSERT_TLM_PaletteOut_SIZE(1);
    ASSERT_EQ(this->tlmHistory_PaletteOut->at(0).arg.get_generation(), 4U);
}

void FrameTlmProcessorTester::testRejectsOversizedDimensions() {
    // Height beyond the modeled channels - drop with an event.
    this->sendFrame(1U, 320U, 401U, FRAME_BYTES);
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_InvalidFrame_SIZE(1);
    ASSERT_EVENTS_InvalidFrame(0, 320U, 401U);

    // Width beyond the modeled row payload - drop with an event.
    this->sendFrame(1U, 641U, 200U, FRAME_BYTES);
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_InvalidFrame_SIZE(2);
}

void FrameTlmProcessorTester::testRejectsShortBuffer() {
    const U16 w = 320;
    const U16 h = 200;
    this->sendFrame(1U, w, h, (static_cast<U32>(w) * static_cast<U32>(h)) - 1U);
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_InvalidFrame_SIZE(1);
}

}  // namespace Doom
