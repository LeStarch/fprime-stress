// ======================================================================
// \title  FrameTlmProcessorTester.hpp
// \brief  Unit-test harness for the FrameTlmProcessor component.
// ======================================================================
#ifndef Doom_FrameTlmProcessorTester_HPP
#define Doom_FrameTlmProcessorTester_HPP

#include "Doom/FppConstantsAc.hpp"
#include "Doom/FrameTlmProcessor.hpp"
#include "Doom/FrameTlmProcessorGTestBase.hpp"

namespace Doom {

class FrameTlmProcessorTester final : public FrameTlmProcessorGTestBase {
  public:
    static constexpr FwIndexType TEST_INSTANCE_ID = 0;
    static constexpr FwSizeType MAX_HISTORY_SIZE = 16;
    static constexpr U32 FRAME_BYTES = static_cast<U32>(Doom::FRAME_WIDTH) * static_cast<U32>(Doom::FRAME_HEIGHT);

    FrameTlmProcessorTester();
    ~FrameTlmProcessorTester();

    // ------------------------------------------------------------------
    // Tests
    // ------------------------------------------------------------------

    void testEmitsOneChannelPerRow();
    void testEmitsFullResolutionRows();
    void testReEmitsPalette();
    void testRejectsOversizedDimensions();
    void testRejectsShortBuffer();

  private:
    // Wiring and init provided by auto-generated helpers.
    void connectPorts();
    void initComponents();

    //! Invoke frameIn with a position-dependent pattern.
    void sendFrame(U32 frameNumber, U16 width, U16 height, U32 bufferSize);

    FrameTlmProcessor component;

    //! Backing storage handed to the component through frameIn.
    U8 m_pixels[FRAME_BYTES];
};

}  // namespace Doom

#endif
