// ======================================================================
// \title  FrameDownsampler.cpp
// \brief  In-place, allocation-free frame decimator.
//
// The incoming buffer wraps caller-owned storage valid for the
// duration of the synchronous port call. Decimation strides over the
// source pixels and packs the kept bytes toward the front of the same
// buffer: every source index >= its destination index, so a forward
// traversal never overwrites unread data. No copy, no allocation.
// ======================================================================
#include "Doom/FrameDownsampler.hpp"

namespace Doom {

FrameDownsampler::FrameDownsampler(const char* compName) : FrameDownsamplerComponentBase(compName) {}

FrameDownsampler::~FrameDownsampler() {}

void FrameDownsampler::frameIn_handler(FwIndexType portNum,
                                       U32 frameNumber,
                                       U16 width,
                                       U16 height,
                                       Fw::Buffer& pixels) {
    Fw::ParamValid valid;
    const Doom::DownsampleFactor factorParam = this->paramGet_DOWNSAMPLE(valid);
    // Fall back to the model default on an unset/invalid parameter.
    const U16 factor = ((valid == Fw::ParamValid::VALID) || (valid == Fw::ParamValid::DEFAULT))
                           ? static_cast<U16>(factorParam.e)
                           : static_cast<U16>(Doom::DownsampleFactor::X2);

    // Validate rather than assert: dimensions arrive over a port and a
    // misbehaving upstream must not take the deployment down.
    const U32 frameBytes = static_cast<U32>(width) * static_cast<U32>(height);
    if ((factor == 0U) || ((width % factor) != 0U) || ((height % factor) != 0U) || (pixels.getData() == nullptr) ||
        (pixels.getSize() < frameBytes)) {
        this->log_WARNING_LO_InvalidFrame(width, height, static_cast<U8>(factor));
        return;
    }

    const U16 outWidth = static_cast<U16>(width / factor);
    const U16 outHeight = static_cast<U16>(height / factor);

    if (factor > 1U) {
        U8* const pix = pixels.getData();
        // In-place stride-and-pack: read index always >= write index.
        for (U32 r = 0; r < outHeight; r++) {
            const U32 srcRow = r * static_cast<U32>(factor) * static_cast<U32>(width);
            const U32 dstRow = r * static_cast<U32>(outWidth);
            for (U32 c = 0; c < outWidth; c++) {
                pix[dstRow + c] = pix[srcRow + (c * static_cast<U32>(factor))];
            }
        }
        pixels.setSize(static_cast<U32>(outWidth) * static_cast<U32>(outHeight));
    }

    if (this->isConnected_frameOut_OutputPort(0)) {
        this->frameOut_out(0, frameNumber, outWidth, outHeight, pixels);
    }
}

void FrameDownsampler::paletteIn_handler(FwIndexType portNum, const Doom::Palette& palette) {
    if (this->isConnected_paletteOut_OutputPort(0)) {
        this->paletteOut_out(0, palette);
    }
}

}  // namespace Doom
