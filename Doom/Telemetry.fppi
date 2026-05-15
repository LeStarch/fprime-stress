# ----------------------------------------------------------------------
# Doom component telemetry channels
# ----------------------------------------------------------------------

@ Frame chunk stream. The component emits CHUNKS_PER_FRAME of these per
@ rendered frame; the GDS reassembles them by frame counter and row.
telemetry FrameOut: Doom.FrameChunk id 0

@ Monotonically increasing count of frames the engine has produced.
telemetry FrameCount: U32 id 1

@ Current DOOM 256-entry RGB palette. Emitted on engine start and
@ whenever the engine swaps palettes.
telemetry PaletteOut: Doom.Palette id 2

@ Current high-level engine state.
telemetry State: Doom.EngineState id 3

@ Number of unprocessed events currently queued for DG_GetKey.
telemetry KeyQueueDepth: U8 id 4

@ Number of key events that have been dropped because the engine
@ queue overflowed. Should stay at zero in well-behaved scenarios.
telemetry KeyEventsDropped: U32 id 5

@ Number of telemetry frame-chunk emissions skipped because the
@ component scheduling fell behind the engine.
telemetry FramesDropped: U32 id 6

@ Engine output frame rate in frames per second, measured over a
@ rolling one-second window updated on schedIn.
telemetry FrameRateHz: F32 id 7

@ Engine downlink data rate in bytes per second emitted via
@ FrameOut + PaletteOut telemetry, measured over the same window.
telemetry FrameDataRateBps: U32 id 8

@ Inbound input data rate in bytes per second received through
@ key-event commands and parallel input ports. Each key event is
@ counted as one pressed flag (1 byte) + one code byte = 2 bytes.
telemetry InputDataRateBps: U32 id 9

@ Inbound input command rate in events per second received through
@ key-event commands and parallel input ports.
telemetry InputCommandRateHz: F32 id 10
