module Doom {

  # ----------------------------------------------------------------------
  # Frame dimensions
  #
  # The wrapped DOOM engine runs at a fixed 640 x 400 palette-indexed
  # resolution. The engine hands each full frame to a downsampler over
  # a synchronous RawFrame port; the downsampler decimates it in place
  # and forwards it to a telemetry processor that emits one FrameRow
  # telemetry channel per scanline of the (possibly reduced) frame.
  # ----------------------------------------------------------------------

  @ Width of the full-resolution DOOM frame in pixels.
  constant FRAME_WIDTH = 640

  @ Height of the full-resolution DOOM frame in scanlines.
  constant FRAME_HEIGHT = 400

  @ Number of palette bytes (256 entries * 3 bytes per RGB triple).
  constant PALETTE_BYTES = 768

  # ----------------------------------------------------------------------
  # Telemetry struct types
  # ----------------------------------------------------------------------

  @ One scanline of the (possibly downsampled) DOOM frame. The pixel
  @ array is sized for the full-resolution width; `width` gives the
  @ number of valid leading pixels at the active downsample factor.
  struct FrameRow {
    @ Monotonically increasing frame counter set by the engine.
    frame: U32
    @ Scanline index within the downsampled frame (0..height-1).
    row: U16
    @ Width in pixels of this row. Only the first `width` bytes of
    @ `pixels` are valid; trailing bytes are zero.
    width: U16
    @ Palette-indexed pixel data for this scanline.
    pixels: [Doom.FRAME_WIDTH] U8
  }

  @ The active DOOM palette as a flat RGB byte array. Emitted with
  @ every frame so the ground converges on the active palette
  @ regardless of when it attached.
  struct Palette {
    @ Monotonically increasing palette generation counter.
    generation: U32
    @ 256 RGB triples, packed R0,G0,B0,R1,G1,B1,...
    rgb: [Doom.PALETTE_BYTES] U8
  }

  # ----------------------------------------------------------------------
  # Downsampling
  # ----------------------------------------------------------------------

  @ Downsample factor applied to each frame dimension. Restricted to
  @ powers of 2 that divide both FRAME_WIDTH (640) and FRAME_HEIGHT
  @ (400) evenly (32 would give a fractional height).
  enum DownsampleFactor : U8 {
    X1  = 1   @< 640 x 400 pass-through
    X2  = 2   @< 320 x 200
    X4  = 4   @< 160 x 100
    X8  = 8   @<  80 x  50
    X16 = 16  @<  40 x  25
  } default X2

  # ----------------------------------------------------------------------
  # Engine state enums
  # ----------------------------------------------------------------------

  enum EngineState {
    OFF       = 0  @< Engine not running: never started, or stopped (resumable).
    STARTING  = 1  @< Engine bring-up is running in the Start handler.
    RUNNING   = 2  @< Engine is ticking and producing frames.
    FAILED    = 3  @< Engine failed to start (e.g. WAD unavailable).
  } default OFF

  # ----------------------------------------------------------------------
  # Ground-facing key enumeration
  #
  # Maps semantically named DOOM inputs onto raw key codes consumed by
  # the wrapped engine. The numeric values are the doomkeys.h codes the
  # DOOM source already uses, so the mapping is a direct lookup.
  # ----------------------------------------------------------------------

  enum DoomKey : U8 {
    LEFT        = 0xAC  @< Turn left (KEY_LEFTARROW).
    RIGHT       = 0xAE  @< Turn right (KEY_RIGHTARROW).
    UP          = 0xAD  @< Move forward (KEY_UPARROW).
    DOWN        = 0xAF  @< Move backward (KEY_DOWNARROW).
    STRAFE_L    = 0xA0  @< Strafe left (KEY_STRAFE_L).
    STRAFE_R    = 0xA1  @< Strafe right (KEY_STRAFE_R).
    USE         = 0xA2  @< Use / open door / activate (KEY_USE).
    FIRE        = 0xA3  @< Fire weapon (KEY_FIRE).
    ESCAPE      = 0x1B  @< Menu escape (KEY_ESCAPE).
    ENTER       = 0x0D  @< Menu confirm (KEY_ENTER).
    TAB         = 0x09  @< Automap (KEY_TAB).
    SHIFT       = 0xB6  @< Run modifier (KEY_RSHIFT).
    Y           = 0x79  @< Confirmation 'y'.
    N           = 0x6E  @< Confirmation 'n'.
    WEAPON1     = 0x31  @< Select weapon 1 ('1').
    WEAPON2     = 0x32  @< Select weapon 2 ('2').
    WEAPON3     = 0x33  @< Select weapon 3 ('3').
    WEAPON4     = 0x34  @< Select weapon 4 ('4').
    WEAPON5     = 0x35  @< Select weapon 5 ('5').
    WEAPON6     = 0x36  @< Select weapon 6 ('6').
    WEAPON7     = 0x37  @< Select weapon 7 ('7').
    PAUSE       = 0xFF  @< Pause game (KEY_PAUSE).
  }

  # ----------------------------------------------------------------------
  # Ports
  #
  # Parallel-to-command input ports. Any component (e.g. a sensor adapter
  # such as an IMU translating tilt into strafe events, a sequencer
  # macro, or a unit test driver) can wire directly into these ports
  # and inject inputs without going through the command dispatcher. The
  # port handlers funnel into the same mutex-guarded queue used by the
  # command handlers, so the engine sees a single ordered input stream.
  # ----------------------------------------------------------------------

  @ Named-enum key event. Used by the keyTapIn / keyDownIn / keyUpIn
  @ parallel input ports.
  port KeyEvent(
                 key: Doom.DoomKey
               )

  @ Raw key event. Used by the rawKeyIn parallel input port for
  @ arbitrary key codes not covered by the named enum.
  port RawKeyEvent(
                    pressed: bool
                    code: U8
                  )

  @ Synchronous frame hand-off. `pixels` wraps caller-owned storage of
  @ width * height palette indices, valid (and mutable by the callee)
  @ only for the duration of the port call. No ownership transfer.
  port RawFrame(
                 frameNumber: U32
                 width: U16
                 height: U16
                 ref pixels: Fw.Buffer
               )

  @ Synchronous palette hand-off, sent whenever a frame is sent.
  port PaletteSend(
                    palette: Doom.Palette
                  )

}
