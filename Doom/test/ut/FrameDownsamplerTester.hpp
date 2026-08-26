// ======================================================================
// \title  FrameDownsamplerTester.hpp
// \brief  Unit-test harness for the FrameDownsampler component.
// ======================================================================
#ifndef Doom_FrameDownsamplerTester_HPP
#define Doom_FrameDownsamplerTester_HPP

#include "Doom/FppConstantsAc.hpp"
#include "Doom/FrameDownsampler.hpp"
#include "Doom/FrameDownsamplerGTestBase.hpp"

namespace Doom {

class FrameDownsamplerTester final : public FrameDownsamplerGTestBase {
  public:
    static constexpr FwIndexType TEST_INSTANCE_ID = 0;
    static constexpr FwSizeType MAX_HISTORY_SIZE = 16;
    static constexpr U32 FRAME_BYTES = static_cast<U32>(Doom::FRAME_WIDTH) * static_cast<U32>(Doom::FRAME_HEIGHT);

    FrameDownsamplerTester();
    ~FrameDownsamplerTester();

    // ------------------------------------------------------------------
    // Tests
    // ------------------------------------------------------------------

    void testPassThroughAtX1();
    void testDecimatesInPlace(Doom::DownsampleFactor::T factor);
    void testForwardsPalette();
    void testRejectsIndivisibleDimensions();
    void testRejectsShortBuffer();
    void testDefaultsToX2WhenParamUnset();

  private:
    // Wiring and init provided by auto-generated helpers.
    void connectPorts();
    void initComponents();

    //! Captures each frameOut invocation while the buffer is valid.
    void from_frameOut_handler(FwIndexType portNum,
                               U32 frameNumber,
                               U16 width,
                               U16 height,
                               Fw::Buffer& pixels) override;

    //! Fill m_pixels with a position-dependent pattern and invoke
    //! frameIn with a buffer wrapping it.
    void sendFrame(U32 frameNumber, U16 width, U16 height, U32 bufferSize);

    FrameDownsampler component;

    //! Backing storage handed to the component through frameIn.
    U8 m_pixels[FRAME_BYTES];

    // Snapshot of the last frameOut call.
    U32 m_outFrameNumber = 0;
    U16 m_outWidth = 0;
    U16 m_outHeight = 0;
    U32 m_outSize = 0;
    const U8* m_outData = nullptr;
    U8 m_outPixels[FRAME_BYTES];
    FwSizeType m_outCount = 0;
};

}  // namespace Doom

#endif
