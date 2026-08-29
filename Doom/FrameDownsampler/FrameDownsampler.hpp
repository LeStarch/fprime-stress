// ======================================================================
// \title  FrameDownsampler.hpp
// \brief  In-place, allocation-free frame decimator.
// ======================================================================
#ifndef Doom_FrameDownsampler_HPP
#define Doom_FrameDownsampler_HPP

#include "Doom/FrameDownsamplerComponentAc.hpp"

namespace Doom {

class FrameDownsampler final : public FrameDownsamplerComponentBase {
  public:
    explicit FrameDownsampler(const char* compName);
    ~FrameDownsampler() override;

  private:
    //! Decimate the frame in place by the DOWNSAMPLE factor and
    //! forward the same buffer with reduced dimensions.
    void frameIn_handler(FwIndexType portNum, U32 frameNumber, U16 width, U16 height, Fw::Buffer& pixels) override;

    //! Forward the palette untouched.
    void paletteIn_handler(FwIndexType portNum, const Doom::Palette& palette) override;
};

}  // namespace Doom

#endif
