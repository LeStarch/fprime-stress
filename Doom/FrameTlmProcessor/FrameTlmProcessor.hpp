// ======================================================================
// \title  FrameTlmProcessor.hpp
// \brief  Emits downsampled frames as per-row telemetry.
// ======================================================================
#ifndef Doom_FrameTlmProcessor_HPP
#define Doom_FrameTlmProcessor_HPP

#include "Doom/DoomConfig/FppConstantsAc.hpp"
#include "Doom/FrameTlmProcessor/FrameTlmProcessorComponentAc.hpp"

namespace Doom {

class FrameTlmProcessor final : public FrameTlmProcessorComponentBase {
  public:
    //! Maximum number of row channels (FrameRow000..FrameRow399).
    static constexpr U16 MAX_ROWS = Doom::FRAME_HEIGHT;

    //! Exact pixels per row channel (configured downsampled width).
    static constexpr U16 ROW_WIDTH = Doom::DOWNSAMPLED_WIDTH;

    explicit FrameTlmProcessor(const char* compName);
    ~FrameTlmProcessor() override;

  private:
    //! Emit each scanline of the incoming frame on its own FrameRow
    //! channel, up to the frame's height.
    void frameIn_handler(FwIndexType portNum, U32 frameNumber, U16 width, U16 height, Fw::Buffer& pixels) override;

    //! Re-emit the palette as PaletteOut telemetry.
    void paletteIn_handler(FwIndexType portNum, const Doom::Palette& palette) override;

    // Each row index has its own channel id so TlmChan's slot store
    // keeps every row of a frame alive within one tick. The table maps
    // row index -> tlmWrite_FrameRowNNN; it lives here because those
    // base-class methods are protected.
    using RowWriter = void (FrameTlmProcessorComponentBase::*)(const Doom::FrameRow&, Fw::Time) const;
    static const RowWriter kRowWriters[MAX_ROWS];

    //! Staging row reused for every emission (no per-call allocation).
    Doom::FrameRow m_row;
};

}  // namespace Doom

#endif
