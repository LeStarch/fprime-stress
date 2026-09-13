module Doom {

  # ----------------------------------------------------------------------
  # Telemetry struct types
  #
  # The wrapped DOOM engine runs at a fixed FRAME_WIDTH x FRAME_HEIGHT
  # palette-indexed resolution (constants in DoomConfig.fpp). The
  # engine hands each full frame to a downsampler over a synchronous
  # RawFrame port; the downsampler decimates it in place by the
  # compile-time DOWNSAMPLE_FACTOR and forwards it to a telemetry
  # processor that emits one FrameRow channel per downsampled scanline.
  # ----------------------------------------------------------------------

  @ One scanline of the downsampled DOOM frame. The pixel array is
  @ sized exactly to the configured downsampled width, so each row
  @ carries no unused on-wire bytes.
  struct FrameRow {
    @ Monotonically increasing frame counter set by the engine.
    frame: U32
    @ Scanline index within the downsampled frame (0..height-1).
    row: U16
    @ Width in pixels of this row (= Doom.DOWNSAMPLED_WIDTH).
    width: U16
    @ Palette-indexed pixel data for this scanline.
    pixels: [Doom.DOWNSAMPLED_WIDTH] U8
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
  # Engine state enums
  # ----------------------------------------------------------------------

  enum EngineState {
    OFF       = 0  @< Engine not running: never started, or stopped (resumable).
    STARTING  = 1  @< Start accepted; the rate group applies it on its next tick.
    RUNNING   = 2  @< Engine is ticking and producing frames.
    FAILED    = 3  @< Engine failed to start (e.g. WAD unavailable).
  } default OFF

  @ Result of the structural IWAD check run by initEngine before the
  @ engine is created. Every failure names the check that rejected it.
  enum WadStatus {
    VALID                   = 0  @< Header, directory and required lumps all check out.
    SIZE_UNKNOWN            = 1  @< The file size could not be determined.
    SHORT_HEADER            = 2  @< Fewer than 12 header bytes.
    NOT_IWAD                = 3  @< Magic is not "IWAD".
    LUMP_COUNT_OUT_OF_RANGE = 4  @< numlumps is zero or above the bound.
    DIRECTORY_OUTSIDE_FILE  = 5  @< Directory offset/extent exceeds the file.
    DIRECTORY_SEEK_FAILED   = 6  @< Seeking to the directory failed.
    SHORT_DIRECTORY         = 7  @< Directory read returned fewer bytes than declared.
    LUMP_OUTSIDE_FILE       = 8  @< A lump's filepos + size exceeds the file.
    REQUIRED_LUMP_MISSING   = 9  @< PLAYPAL, COLORMAP, PNAMES or TEXTURE1 absent.
  } default VALID

  @ Outcome of initEngine (topology-time engine creation).
  enum InitStatus {
    OK              = 0  @< Engine created; Start is now accepted.
    WAD_UNAVAILABLE = 1  @< No WAD path configured or the file could not be opened.
    WAD_INVALID     = 2  @< The WAD failed structural validation (see WadStatus).
    ENGINE_FAULT    = 3  @< doomgeneric_Create raised I_Error/I_Quit.
    WAD_PATH_TOO_LONG = 4  @< The configured WAD path does not fit WAD_PATH_MAX.
  } default OK

  @ Outcome of a Start or Reset request; the rejections name the
  @ precondition that failed.
  enum RequestStatus {
    ACCEPTED        = 0  @< Request latched for the rate-group thread.
    ALREADY_RUNNING = 1  @< Engine is already being ticked.
    START_PENDING   = 2  @< A previous Start has not yet been applied.
    NOT_INITIALIZED = 3  @< initEngine did not succeed.
    FAULTED         = 4  @< The engine has faulted; FAILED is terminal.
  } default ACCEPTED

  @ Outcome of queuing key input for the engine.
  enum KeyQueueStatus {
    QUEUED           = 0  @< All entries of the event were queued.
    QUEUE_FULL       = 1  @< No entry was queued; input dropped and counted.
    CODE_NOT_ALLOWED = 2  @< Key code is not a DoomKey enumerator; nothing queued.
  } default QUEUED

  @ Why an incoming raw frame was dropped instead of processed.
  enum FrameRejectReason {
    BAD_WIDTH    = 0  @< Width does not match / divide by the configured value.
    BAD_HEIGHT   = 1  @< Height does not match / divide by the configured value.
    NULL_BUFFER  = 2  @< Pixel buffer has no data pointer.
    SHORT_BUFFER = 3  @< Pixel buffer holds fewer than width * height bytes.
  } default BAD_WIDTH

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
    @ 'y' is deliberately absent: it confirms Quit Game / End Game, which faults the engine.
    N           = 0x6E  @< Decline a menu prompt ('n').
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

  @ Raw key event. Used by the rawKeyIn parallel input port; the code
  @ must be a DoomKey enumerator or it is rejected (KeyRejected).
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
