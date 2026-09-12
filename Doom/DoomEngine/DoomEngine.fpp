module Doom {

  # ----------------------------------------------------------------------
  # Component
  # ----------------------------------------------------------------------

  @ Single component wrapping the open-source DOOM engine. Each full
  @ frame is pushed out the frameOut RawFrame port from engine-owned
  @ storage (no allocation); ground inputs arrive as
  @ KeyTap/KeyDown/KeyUp/RawKey commands and are converted into queue
  @ entries the engine consumes through its DG_GetKey hook.
  @
  @ Engine pacing is driven entirely from the schedIn port: each call
  @ runs exactly one doomgeneric_Tick (one DOOM frame of game logic)
  @ or replays one buffered screen-wipe melt frame on the rate-group
  @ thread. doomgeneric_Create is invoked synchronously by the first
  @ Start after rendezvousing with any in-flight tick; a Start after a
  @ Stop resumes the existing engine.
  @
  @ Declared passive: schedIn is sync (runs on the rate-group thread)
  @ and all command handlers are sync (run on the cmdDispatch thread),
  @ so the component owns no thread of its own. Cross-thread state is
  @ limited to the mutex-guarded key queue and the std::atomic
  @ members (start/stop, tick-in-progress, last-published-state,
  @ reset-requested).
  passive component DoomEngine {

    # ------------------------------------------------------------------
    # Scheduled ports
    # ------------------------------------------------------------------

    @ Periodic scheduled call driving one doomgeneric_Tick per pulse
    @ (or one buffered melt-frame replay while a screen wipe drains).
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
    # Frame output
    # ------------------------------------------------------------------

    @ Full-resolution frame hand-off, invoked synchronously once per
    @ emitted frame from the rate-group thread. The buffer wraps the
    @ engine's own frame storage and is valid only during the call.
    output port frameOut: Doom.RawFrame

    @ Active palette, sent before each frameOut invocation.
    output port paletteOut: Doom.PaletteSend

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
