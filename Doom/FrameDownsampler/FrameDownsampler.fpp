module Doom {

  @ Synchronous, allocation-free frame downsampler. Receives a full
  @ frame over frameIn, decimates it in place inside the caller's
  @ buffer (striding by the DOWNSAMPLE factor and packing the kept
  @ bytes toward the front), and forwards the same buffer with reduced
  @ dimensions out frameOut. Passive: the whole pipeline runs on the
  @ caller's (rate-group) thread.
  passive component FrameDownsampler {

    @ Incoming full-resolution frame. The buffer is decimated in place.
    sync input port frameIn: Doom.RawFrame

    @ Outgoing downsampled frame, forwarded within the same call.
    output port frameOut: Doom.RawFrame

    @ Incoming palette, forwarded untouched.
    sync input port paletteIn: Doom.PaletteSend

    @ Outgoing palette.
    output port paletteOut: Doom.PaletteSend

    @ Downsample factor applied to both frame dimensions.
    param DOWNSAMPLE: Doom.DownsampleFactor default Doom.DownsampleFactor.X2

    @ A frame arrived whose dimensions or buffer size are inconsistent
    @ with the active downsample factor; the frame was dropped.
    event InvalidFrame(
                        width: U16 @< Incoming frame width
                        height: U16 @< Incoming frame height
                        factor: U8 @< Active downsample factor
                      ) \
      severity warning low \
      format "Dropped frame: {} x {} not divisible by factor {} or buffer too small" \
      throttle 5

    @ Time get port used to tag events.
    time get port timeCaller

    @ Enables command handling (parameter set/save).
    import Fw.Command

    @ Enables event handling.
    import Fw.Event

    @ Port to return the value of a parameter.
    param get port prmGetOut

    @ Port to set the value of a parameter.
    param set port prmSetOut

  }

}
