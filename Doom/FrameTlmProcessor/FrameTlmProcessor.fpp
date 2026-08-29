module Doom {

  @ Emits the downsampled DOOM frame as row telemetry: one FrameRow
  @ channel per scanline (FrameRow000..FrameRow399), written only up
  @ to the incoming frame's height. Each row gets its own channel id
  @ because TlmChan is a slot store: writing one id N times per tick
  @ would collapse to a single ground sample. Passive and
  @ allocation-free: rows are staged in a single member FrameRow.
  passive component FrameTlmProcessor {

    @ Incoming (possibly downsampled) frame to emit as row telemetry.
    sync input port frameIn: Doom.RawFrame

    @ Incoming palette, re-emitted as PaletteOut telemetry.
    sync input port paletteIn: Doom.PaletteSend

    @ Time get port used to tag telemetry samples and events.
    time get port timeCaller

    @ A frame arrived with a height or width exceeding the modeled
    @ maximums, or a buffer smaller than width * height; it was dropped.
    event InvalidFrame(
                        width: U16 @< Incoming frame width
                        height: U16 @< Incoming frame height
                      ) \
      severity warning low \
      format "Dropped frame: {} x {} exceeds modeled dimensions or buffer too small" \
      throttle 5

    @ Enables event handling.
    import Fw.Event

    @ Enables telemetry.
    import Fw.Channel

    include "FrameTlmProcessorTelemetry.fppi"

  }

}
