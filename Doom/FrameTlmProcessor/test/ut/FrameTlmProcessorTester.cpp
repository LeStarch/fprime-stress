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
    // Position-dependent pattern so any offset bug fails the checks. Oversized
    // dimensions are rejected before the pixels are read, so clamp the fill.
    const U32 requested = static_cast<U32>(width) * static_cast<U32>(height);
    const U32 bytes = (requested < FRAME_BYTES) ? requested : FRAME_BYTES;
    for (U32 i = 0; i < bytes; i++) {
        m_pixels[i] = static_cast<U8>(i % 251U);
    }
    Fw::Buffer buffer(m_pixels, bufferSize);
    this->invoke_to_frameIn(0, frameNumber, width, height, buffer);
}

void FrameTlmProcessorTester::checkRow(const Doom::FrameRow& row, U32 frameNumber, U16 rowIndex, U16 width) {
    // Pixel pattern follows sendFrame: value = (row * width + col) % 251.
    ASSERT_EQ(row.get_frame(), frameNumber);
    ASSERT_EQ(row.get_row(), rowIndex);
    ASSERT_EQ(row.get_width(), width);
    const U32 base = static_cast<U32>(rowIndex) * static_cast<U32>(width);
    for (U32 i = 0; i < width; i++) {
        ASSERT_EQ(row.get_pixels()[i], static_cast<U8>((base + i) % 251U)) << "row " << rowIndex << " pixel " << i;
    }
}

const Doom::FrameRow& FrameTlmProcessorTester::lastRow(U16 height) {
    // The final row's channel depends on the compile-time factor.
    switch (height) {
        case 400U:
            return this->tlmHistory_FrameRow399->at(0).arg;
        case 200U:
            return this->tlmHistory_FrameRow199->at(0).arg;
        case 100U:
            return this->tlmHistory_FrameRow099->at(0).arg;
        case 50U:
            return this->tlmHistory_FrameRow049->at(0).arg;
        default:
            EXPECT_EQ(height, 25U) << "unsupported DOWNSAMPLED_HEIGHT";
            return this->tlmHistory_FrameRow024->at(0).arg;
    }
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

    // First, second and last rows carry the right metadata and the
    // payload slice at the right stride.
    ASSERT_TLM_FrameRow001_SIZE(1);
    this->checkRow(this->tlmHistory_FrameRow000->at(0).arg, 5U, 0U, w);
    this->checkRow(this->tlmHistory_FrameRow001->at(0).arg, 5U, 1U, w);
    this->checkRow(this->lastRow(h), 5U, static_cast<U16>(h - 1U), w);
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
    ASSERT_EVENTS_InvalidFrame(0, w, 401U, Doom::FrameRejectReason::BAD_HEIGHT);

    // Width other than the configured row width - drop with an event.
    this->sendFrame(1U, static_cast<U16>(w + 1U), 100U, FRAME_BYTES);
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_InvalidFrame_SIZE(2);
    ASSERT_EVENTS_InvalidFrame(1, static_cast<U16>(w + 1U), 100U, Doom::FrameRejectReason::BAD_WIDTH);
}

void FrameTlmProcessorTester::testRejectsShortBuffer() {
    const U16 w = static_cast<U16>(Doom::DOWNSAMPLED_WIDTH);
    const U16 h = static_cast<U16>(Doom::DOWNSAMPLED_HEIGHT);
    this->sendFrame(1U, w, h, (static_cast<U32>(w) * static_cast<U32>(h)) - 1U);
    ASSERT_TLM_SIZE(0);
    ASSERT_EVENTS_InvalidFrame_SIZE(1);
    ASSERT_EVENTS_InvalidFrame(0, w, h, Doom::FrameRejectReason::SHORT_BUFFER);
}

}  // namespace Doom
