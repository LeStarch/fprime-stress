// ======================================================================
// \title  FrameDownsamplerTester.cpp
// \brief  Unit-test harness for the FrameDownsampler component.
// ======================================================================

#include "Doom/test/ut/FrameDownsamplerTester.hpp"

#include <cstring>

namespace Doom {

FrameDownsamplerTester::FrameDownsamplerTester()
    : FrameDownsamplerGTestBase("FrameDownsamplerTester", FrameDownsamplerTester::MAX_HISTORY_SIZE),
      component("FrameDownsampler") {
    this->initComponents();
    this->connectPorts();
    (void)::memset(m_pixels, 0, sizeof(m_pixels));
    (void)::memset(m_outPixels, 0, sizeof(m_outPixels));
}

FrameDownsamplerTester::~FrameDownsamplerTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void FrameDownsamplerTester::from_frameOut_handler(FwIndexType portNum,
                                                   U32 frameNumber,
                                                   U16 width,
                                                   U16 height,
                                                   Fw::Buffer& pixels) {
    this->pushFromPortEntry_frameOut(frameNumber, width, height, pixels);
    m_outFrameNumber = frameNumber;
    m_outWidth = width;
    m_outHeight = height;
    m_outSize = static_cast<U32>(pixels.getSize());
    m_outData = pixels.getData();
    const U32 copyBytes = FW_MIN(m_outSize, static_cast<U32>(sizeof(m_outPixels)));
    (void)::memcpy(m_outPixels, pixels.getData(), copyBytes);
    m_outCount++;
}

void FrameDownsamplerTester::sendFrame(U32 frameNumber, U16 width, U16 height, U32 bufferSize) {
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

void FrameDownsamplerTester::testPassThroughAtX1() {
    this->paramSet_DOWNSAMPLE(Doom::DownsampleFactor::X1, Fw::ParamValid::VALID);
    this->component.loadParameters();

    this->sendFrame(7U, Doom::FRAME_WIDTH, Doom::FRAME_HEIGHT, FRAME_BYTES);

    ASSERT_EQ(m_outCount, 1u);
    ASSERT_EQ(m_outFrameNumber, 7U);
    ASSERT_EQ(m_outWidth, Doom::FRAME_WIDTH);
    ASSERT_EQ(m_outHeight, Doom::FRAME_HEIGHT);
    ASSERT_EQ(m_outSize, +FRAME_BYTES);
    // Same buffer forwarded, contents untouched.
    ASSERT_EQ(m_outData, m_pixels);
    for (U32 i = 0; i < FRAME_BYTES; i++) {
        ASSERT_EQ(m_outPixels[i], static_cast<U8>(i % 251U)) << "pixel " << i;
    }
    ASSERT_EVENTS_InvalidFrame_SIZE(0);
}

void FrameDownsamplerTester::testDecimatesInPlace(Doom::DownsampleFactor::T factor) {
    this->paramSet_DOWNSAMPLE(factor, Fw::ParamValid::VALID);
    this->component.loadParameters();

    const U16 f = static_cast<U16>(factor);
    const U16 w = Doom::FRAME_WIDTH;
    const U16 h = Doom::FRAME_HEIGHT;
    this->sendFrame(3U, w, h, FRAME_BYTES);

    const U16 outW = static_cast<U16>(w / f);
    const U16 outH = static_cast<U16>(h / f);
    ASSERT_EQ(m_outCount, 1u);
    ASSERT_EQ(m_outFrameNumber, 3U);
    ASSERT_EQ(m_outWidth, outW);
    ASSERT_EQ(m_outHeight, outH);
    ASSERT_EQ(m_outSize, static_cast<U32>(outW) * static_cast<U32>(outH));
    // Same storage reused: decimation packed bytes toward the front.
    ASSERT_EQ(m_outData, m_pixels);
    for (U32 r = 0; r < outH; r++) {
        for (U32 c = 0; c < outW; c++) {
            const U32 srcIdx = (r * f) * static_cast<U32>(w) + (c * f);
            const U8 expected = static_cast<U8>(srcIdx % 251U);
            ASSERT_EQ(m_outPixels[r * outW + c], expected) << "factor " << f << " row " << r << " col " << c;
        }
    }
    ASSERT_EVENTS_InvalidFrame_SIZE(0);
}

void FrameDownsamplerTester::testForwardsPalette() {
    Doom::Palette pal;
    pal.set_generation(9U);
    this->invoke_to_paletteIn(0, pal);
    ASSERT_from_paletteOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_paletteOut->at(0).palette.get_generation(), 9U);
}

void FrameDownsamplerTester::testRejectsIndivisibleDimensions() {
    this->paramSet_DOWNSAMPLE(Doom::DownsampleFactor::X16, Fw::ParamValid::VALID);
    this->component.loadParameters();

    // 100 x 90: 90 is not divisible by 16 - drop with an event.
    this->sendFrame(1U, 96U, 90U, FRAME_BYTES);
    ASSERT_EQ(m_outCount, 0u);
    ASSERT_EVENTS_InvalidFrame_SIZE(1);
    ASSERT_EVENTS_InvalidFrame(0, 96U, 90U, 16U);
}

void FrameDownsamplerTester::testRejectsShortBuffer() {
    this->paramSet_DOWNSAMPLE(Doom::DownsampleFactor::X2, Fw::ParamValid::VALID);
    this->component.loadParameters();

    // Buffer smaller than width * height - drop with an event.
    this->sendFrame(1U, Doom::FRAME_WIDTH, Doom::FRAME_HEIGHT, FRAME_BYTES - 1U);
    ASSERT_EQ(m_outCount, 0u);
    ASSERT_EVENTS_InvalidFrame_SIZE(1);
}

void FrameDownsamplerTester::testDefaultsToX2WhenParamUnset() {
    // No paramSet: the component falls back to the model default (X2).
    this->component.loadParameters();

    this->sendFrame(2U, Doom::FRAME_WIDTH, Doom::FRAME_HEIGHT, FRAME_BYTES);
    ASSERT_EQ(m_outCount, 1u);
    ASSERT_EQ(m_outWidth, Doom::FRAME_WIDTH / 2U);
    ASSERT_EQ(m_outHeight, Doom::FRAME_HEIGHT / 2U);
}

}  // namespace Doom
