# ----------------------------------------------------------------------
# Doom component telemetry channels
# ----------------------------------------------------------------------
#
# FrameOut is split into CHUNKS_PER_FRAME (= 80) distinct telemetry
# channels (FrameOut00 .. FrameOut79) rather than one channel emitted
# 80 times per cycle. This is the same multiplexing pattern a real
# flight payload would use for streaming imagery: TlmChan is a
# SCADA-style slot store - it retains only the most recent value per
# channel id between Run ticks - so writing one channel id 80 times
# in a single rate-group tick collapses to one ground sample. Giving
# each chunk position its own id keeps all 80 chunks per tick alive
# in the slot table; tlmSendRun then ships them all on the same Run
# cycle and the ground reassembles a full 640 x 400 frame per tick.
# This also preserves the bulk-downlink stress (2,800 chunks/sec all
# the way to the framer) that the deployment is designed to exercise.

@ Frame scanlines [  0..  4] of the latest DOOM frame.
telemetry FrameOut00: Doom.FrameChunk id 0

@ Frame scanlines [  5..  9] of the latest DOOM frame.
telemetry FrameOut01: Doom.FrameChunk id 1

@ Frame scanlines [ 10.. 14] of the latest DOOM frame.
telemetry FrameOut02: Doom.FrameChunk id 2

@ Frame scanlines [ 15.. 19] of the latest DOOM frame.
telemetry FrameOut03: Doom.FrameChunk id 3

@ Frame scanlines [ 20.. 24] of the latest DOOM frame.
telemetry FrameOut04: Doom.FrameChunk id 4

@ Frame scanlines [ 25.. 29] of the latest DOOM frame.
telemetry FrameOut05: Doom.FrameChunk id 5

@ Frame scanlines [ 30.. 34] of the latest DOOM frame.
telemetry FrameOut06: Doom.FrameChunk id 6

@ Frame scanlines [ 35.. 39] of the latest DOOM frame.
telemetry FrameOut07: Doom.FrameChunk id 7

@ Frame scanlines [ 40.. 44] of the latest DOOM frame.
telemetry FrameOut08: Doom.FrameChunk id 8

@ Frame scanlines [ 45.. 49] of the latest DOOM frame.
telemetry FrameOut09: Doom.FrameChunk id 9

@ Frame scanlines [ 50.. 54] of the latest DOOM frame.
telemetry FrameOut10: Doom.FrameChunk id 10

@ Frame scanlines [ 55.. 59] of the latest DOOM frame.
telemetry FrameOut11: Doom.FrameChunk id 11

@ Frame scanlines [ 60.. 64] of the latest DOOM frame.
telemetry FrameOut12: Doom.FrameChunk id 12

@ Frame scanlines [ 65.. 69] of the latest DOOM frame.
telemetry FrameOut13: Doom.FrameChunk id 13

@ Frame scanlines [ 70.. 74] of the latest DOOM frame.
telemetry FrameOut14: Doom.FrameChunk id 14

@ Frame scanlines [ 75.. 79] of the latest DOOM frame.
telemetry FrameOut15: Doom.FrameChunk id 15

@ Frame scanlines [ 80.. 84] of the latest DOOM frame.
telemetry FrameOut16: Doom.FrameChunk id 16

@ Frame scanlines [ 85.. 89] of the latest DOOM frame.
telemetry FrameOut17: Doom.FrameChunk id 17

@ Frame scanlines [ 90.. 94] of the latest DOOM frame.
telemetry FrameOut18: Doom.FrameChunk id 18

@ Frame scanlines [ 95.. 99] of the latest DOOM frame.
telemetry FrameOut19: Doom.FrameChunk id 19

@ Frame scanlines [100..104] of the latest DOOM frame.
telemetry FrameOut20: Doom.FrameChunk id 20

@ Frame scanlines [105..109] of the latest DOOM frame.
telemetry FrameOut21: Doom.FrameChunk id 21

@ Frame scanlines [110..114] of the latest DOOM frame.
telemetry FrameOut22: Doom.FrameChunk id 22

@ Frame scanlines [115..119] of the latest DOOM frame.
telemetry FrameOut23: Doom.FrameChunk id 23

@ Frame scanlines [120..124] of the latest DOOM frame.
telemetry FrameOut24: Doom.FrameChunk id 24

@ Frame scanlines [125..129] of the latest DOOM frame.
telemetry FrameOut25: Doom.FrameChunk id 25

@ Frame scanlines [130..134] of the latest DOOM frame.
telemetry FrameOut26: Doom.FrameChunk id 26

@ Frame scanlines [135..139] of the latest DOOM frame.
telemetry FrameOut27: Doom.FrameChunk id 27

@ Frame scanlines [140..144] of the latest DOOM frame.
telemetry FrameOut28: Doom.FrameChunk id 28

@ Frame scanlines [145..149] of the latest DOOM frame.
telemetry FrameOut29: Doom.FrameChunk id 29

@ Frame scanlines [150..154] of the latest DOOM frame.
telemetry FrameOut30: Doom.FrameChunk id 30

@ Frame scanlines [155..159] of the latest DOOM frame.
telemetry FrameOut31: Doom.FrameChunk id 31

@ Frame scanlines [160..164] of the latest DOOM frame.
telemetry FrameOut32: Doom.FrameChunk id 32

@ Frame scanlines [165..169] of the latest DOOM frame.
telemetry FrameOut33: Doom.FrameChunk id 33

@ Frame scanlines [170..174] of the latest DOOM frame.
telemetry FrameOut34: Doom.FrameChunk id 34

@ Frame scanlines [175..179] of the latest DOOM frame.
telemetry FrameOut35: Doom.FrameChunk id 35

@ Frame scanlines [180..184] of the latest DOOM frame.
telemetry FrameOut36: Doom.FrameChunk id 36

@ Frame scanlines [185..189] of the latest DOOM frame.
telemetry FrameOut37: Doom.FrameChunk id 37

@ Frame scanlines [190..194] of the latest DOOM frame.
telemetry FrameOut38: Doom.FrameChunk id 38

@ Frame scanlines [195..199] of the latest DOOM frame.
telemetry FrameOut39: Doom.FrameChunk id 39

@ Frame scanlines [200..204] of the latest DOOM frame.
telemetry FrameOut40: Doom.FrameChunk id 40

@ Frame scanlines [205..209] of the latest DOOM frame.
telemetry FrameOut41: Doom.FrameChunk id 41

@ Frame scanlines [210..214] of the latest DOOM frame.
telemetry FrameOut42: Doom.FrameChunk id 42

@ Frame scanlines [215..219] of the latest DOOM frame.
telemetry FrameOut43: Doom.FrameChunk id 43

@ Frame scanlines [220..224] of the latest DOOM frame.
telemetry FrameOut44: Doom.FrameChunk id 44

@ Frame scanlines [225..229] of the latest DOOM frame.
telemetry FrameOut45: Doom.FrameChunk id 45

@ Frame scanlines [230..234] of the latest DOOM frame.
telemetry FrameOut46: Doom.FrameChunk id 46

@ Frame scanlines [235..239] of the latest DOOM frame.
telemetry FrameOut47: Doom.FrameChunk id 47

@ Frame scanlines [240..244] of the latest DOOM frame.
telemetry FrameOut48: Doom.FrameChunk id 48

@ Frame scanlines [245..249] of the latest DOOM frame.
telemetry FrameOut49: Doom.FrameChunk id 49

@ Frame scanlines [250..254] of the latest DOOM frame.
telemetry FrameOut50: Doom.FrameChunk id 50

@ Frame scanlines [255..259] of the latest DOOM frame.
telemetry FrameOut51: Doom.FrameChunk id 51

@ Frame scanlines [260..264] of the latest DOOM frame.
telemetry FrameOut52: Doom.FrameChunk id 52

@ Frame scanlines [265..269] of the latest DOOM frame.
telemetry FrameOut53: Doom.FrameChunk id 53

@ Frame scanlines [270..274] of the latest DOOM frame.
telemetry FrameOut54: Doom.FrameChunk id 54

@ Frame scanlines [275..279] of the latest DOOM frame.
telemetry FrameOut55: Doom.FrameChunk id 55

@ Frame scanlines [280..284] of the latest DOOM frame.
telemetry FrameOut56: Doom.FrameChunk id 56

@ Frame scanlines [285..289] of the latest DOOM frame.
telemetry FrameOut57: Doom.FrameChunk id 57

@ Frame scanlines [290..294] of the latest DOOM frame.
telemetry FrameOut58: Doom.FrameChunk id 58

@ Frame scanlines [295..299] of the latest DOOM frame.
telemetry FrameOut59: Doom.FrameChunk id 59

@ Frame scanlines [300..304] of the latest DOOM frame.
telemetry FrameOut60: Doom.FrameChunk id 60

@ Frame scanlines [305..309] of the latest DOOM frame.
telemetry FrameOut61: Doom.FrameChunk id 61

@ Frame scanlines [310..314] of the latest DOOM frame.
telemetry FrameOut62: Doom.FrameChunk id 62

@ Frame scanlines [315..319] of the latest DOOM frame.
telemetry FrameOut63: Doom.FrameChunk id 63

@ Frame scanlines [320..324] of the latest DOOM frame.
telemetry FrameOut64: Doom.FrameChunk id 64

@ Frame scanlines [325..329] of the latest DOOM frame.
telemetry FrameOut65: Doom.FrameChunk id 65

@ Frame scanlines [330..334] of the latest DOOM frame.
telemetry FrameOut66: Doom.FrameChunk id 66

@ Frame scanlines [335..339] of the latest DOOM frame.
telemetry FrameOut67: Doom.FrameChunk id 67

@ Frame scanlines [340..344] of the latest DOOM frame.
telemetry FrameOut68: Doom.FrameChunk id 68

@ Frame scanlines [345..349] of the latest DOOM frame.
telemetry FrameOut69: Doom.FrameChunk id 69

@ Frame scanlines [350..354] of the latest DOOM frame.
telemetry FrameOut70: Doom.FrameChunk id 70

@ Frame scanlines [355..359] of the latest DOOM frame.
telemetry FrameOut71: Doom.FrameChunk id 71

@ Frame scanlines [360..364] of the latest DOOM frame.
telemetry FrameOut72: Doom.FrameChunk id 72

@ Frame scanlines [365..369] of the latest DOOM frame.
telemetry FrameOut73: Doom.FrameChunk id 73

@ Frame scanlines [370..374] of the latest DOOM frame.
telemetry FrameOut74: Doom.FrameChunk id 74

@ Frame scanlines [375..379] of the latest DOOM frame.
telemetry FrameOut75: Doom.FrameChunk id 75

@ Frame scanlines [380..384] of the latest DOOM frame.
telemetry FrameOut76: Doom.FrameChunk id 76

@ Frame scanlines [385..389] of the latest DOOM frame.
telemetry FrameOut77: Doom.FrameChunk id 77

@ Frame scanlines [390..394] of the latest DOOM frame.
telemetry FrameOut78: Doom.FrameChunk id 78

@ Frame scanlines [395..399] of the latest DOOM frame.
telemetry FrameOut79: Doom.FrameChunk id 79

@ Monotonically increasing count of frames the engine has produced
@ and sent as FrameOutNN telemetry.
telemetry FrameCount: FwSizeType id 80

@ Current DOOM 256-entry RGB palette. Emitted with every frame so a
@ late-attaching ground system converges on the active palette.
telemetry PaletteOut: Doom.Palette id 81

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

@ Engine downlink data rate in bytes per second emitted via
@ FrameOutNN + PaletteOut telemetry, measured over the same window.
telemetry FrameDataRateBps: U32 id 87

@ Inbound input data rate in bytes per second received through
@ key-event commands and parallel input ports. Each key event is
@ counted as one pressed flag (1 byte) + one code byte = 2 bytes.
telemetry InputDataRateBps: U32 id 88

@ Inbound input command rate in events per second received through
@ key-event commands and parallel input ports.
telemetry InputCommandRateHz: F32 id 89

