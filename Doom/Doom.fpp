module Doom {

  # ----------------------------------------------------------------------
  # Frame dimensions
  #
  # The wrapped DOOM engine runs at a fixed 640 x 400 palette-indexed
  # resolution. Frames are streamed down as a sequence of FrameChunk
  # telemetry samples, each carrying a contiguous run of complete rows.
  # At DOOM's native 35 Hz this is 80 chunks * 3200 B * 35 Hz ~ 8.6 MB/s
  # of sustained downlink telemetry - a deliberately punishing stress
  # workload.
  # ----------------------------------------------------------------------

  @ Width of the DOOM frame in pixels.
  constant FRAME_WIDTH = 640

  @ Height of the DOOM frame in scanlines.
  constant FRAME_HEIGHT = 400

  @ Number of rows packed into a single FrameChunk telemetry sample.
  @ FRAME_WIDTH * ROWS_PER_CHUNK + FrameChunk header must fit inside
  @ FW_COM_BUFFER_MAX_SIZE (4096 in this project). 5 * 640 = 3200 B.
  constant ROWS_PER_CHUNK = 5

  @ Number of palette-indexed pixel bytes per FrameChunk.
  constant FRAME_CHUNK_BYTES = 3200

  @ Number of FrameChunk samples emitted to downlink one complete frame.
  @ FRAME_HEIGHT (400) / ROWS_PER_CHUNK (5) = 80.
  constant CHUNKS_PER_FRAME = 80

  @ Number of palette bytes (256 entries * 3 bytes per RGB triple).
  constant PALETTE_BYTES = 768

  # ----------------------------------------------------------------------
  # Telemetry struct types
  # ----------------------------------------------------------------------

  @ A contiguous block of DOOM frame scanlines. The component emits
  @ CHUNKS_PER_FRAME of these per displayed frame, each carrying
  @ ROWS_PER_CHUNK rows of FRAME_WIDTH palette indices.
  struct FrameChunk {
    @ Monotonically increasing frame counter set by the component.
    frame: U32
    @ Index of the first scanline contained in this chunk (0..FRAME_HEIGHT-1).
    row: U16
    @ Number of scanlines packed in this chunk.
    rowCount: U16
    @ Width in pixels of each row in this chunk.
    width: U16
    @ Palette-indexed pixel data, laid out row-major. Only the first
    @ rowCount * width bytes are valid; trailing bytes are zero.
    pixels: [Doom.FRAME_CHUNK_BYTES] U8
  }

  @ The active DOOM palette as a flat RGB byte array. Emitted whenever
  @ the engine changes the palette (level start, damage tint, etc.).
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
    OFF       = 0  @< Engine has not been started yet.
    STARTING  = 1  @< doomgeneric_Create is running on the worker task.
    RUNNING   = 2  @< Engine is ticking and producing frames.
    FAILED    = 3  @< Engine task exited or failed to start.
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

  # ----------------------------------------------------------------------
  # Component
  # ----------------------------------------------------------------------

  @ Single component wrapping the open-source DOOM engine. Frames are
  @ pushed down as FrameChunk telemetry; ground inputs arrive as
  @ KeyTap/KeyDown/KeyUp/RawKey commands and are converted into queue
  @ entries the engine consumes through its DG_GetKey hook.
  @
  @ Engine pacing is driven entirely from the schedIn port: each call
  @ runs exactly one doomgeneric_Tick (one DOOM frame of game logic)
  @ on the rate-group thread. doomgeneric_Create is invoked
  @ synchronously by the Start command handler.
  @
  @ Declared passive: schedIn is sync (runs on the rate-group thread)
  @ and all command handlers are sync (run on the cmdDispatch thread),
  @ so the component owns no thread of its own. Cross-thread state is
  @ limited to the key queue, which is mutex-guarded.
  passive component DoomEngine {

    # ------------------------------------------------------------------
    # Scheduled ports
    # ------------------------------------------------------------------

    @ Periodic scheduled call driving one doomgeneric_Tick per pulse.
    @ Declared sync so a tick that overruns its rate-group budget
    @ trips Svc.ActiveRateGroup's cycle-slip telemetry directly,
    @ giving hard evidence of an over-budget engine instead of
    @ silently dropping the late frame.
    sync input port schedIn: Svc.Sched

    # ------------------------------------------------------------------
    # Parallel input ports
    #
    # Mirror the key-input commands so any in-topology source (sensor
    # adapter, macro, etc.) can drive inputs without bouncing through
    # the command dispatcher.
    # ------------------------------------------------------------------

    @ Tap (press + release) a named key.
    sync input port keyTapIn: Doom.KeyEvent

    @ Press and hold a named key.
    sync input port keyDownIn: Doom.KeyEvent

    @ Release a previously held named key.
    sync input port keyUpIn: Doom.KeyEvent

    @ Raw press-or-release with an arbitrary key code.
    sync input port rawKeyIn: Doom.RawKeyEvent

    # ------------------------------------------------------------------
    # Standard component interfaces
    # ------------------------------------------------------------------

    @ Time get port used to tag telemetry samples and events.
    time get port timeCaller

    # ------------------------------------------------------------------
    # Commands
    # ------------------------------------------------------------------

    include "Commands.fppi"

    # ------------------------------------------------------------------
    # Telemetry
    # ------------------------------------------------------------------

    include "Telemetry.fppi"

    # ------------------------------------------------------------------
    # Events
    # ------------------------------------------------------------------

    include "Events.fppi"

    # ------------------------------------------------------------------
    # Standard interfaces pulled from the framework
    # ------------------------------------------------------------------

    import Fw.Event
    import Fw.Command
    import Fw.Channel

  }

}
