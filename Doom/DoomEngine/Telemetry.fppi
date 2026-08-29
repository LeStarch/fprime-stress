# ----------------------------------------------------------------------
# Doom component telemetry channels
#
# Frame pixel data is no longer emitted here: the engine sends each
# full frame out the frameOut port; FrameTlmProcessor owns the
# per-row frame telemetry. Housekeeping channels keep their ids.
# ----------------------------------------------------------------------

@ Monotonically increasing count of frames the engine has produced
@ and sent out the frameOut port.
telemetry FrameCount: FwSizeType id 80

@ Current high-level engine state.
telemetry State: Doom.EngineState id 82

@ Number of unprocessed events currently queued for DG_GetKey.
telemetry KeyQueueDepth: U8 id 83

@ Number of key events that have been dropped because the engine
@ queue overflowed. Should stay at zero in well-behaved scenarios.
telemetry KeyEventsDropped: U32 id 84

@ Number of engine frames discarded because the screen-wipe melt
@ playback buffer was full. Should stay at zero in well-behaved
@ scenarios.
telemetry FramesDropped: U32 id 85

@ Engine output frame rate in frames per second, measured over a
@ rolling one-second window updated on schedIn.
telemetry FrameRateHz: F32 id 86

@ Engine output data rate in bytes per second sent out the frameOut
@ and paletteOut ports, measured over the same window.
telemetry FrameDataRateBps: U32 id 87

@ Inbound input data rate in bytes per second received through
@ key-event commands and parallel input ports. Each key event is
@ counted as one pressed flag (1 byte) + one code byte = 2 bytes.
telemetry InputDataRateBps: U32 id 88

@ Inbound input command rate in events per second received through
@ key-event commands and parallel input ports.
telemetry InputCommandRateHz: F32 id 89

