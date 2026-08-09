# ----------------------------------------------------------------------
# Doom component events
# ----------------------------------------------------------------------

@ Engine started successfully: on the first Start doomgeneric_Create
@ has returned; on a Start after a Stop the existing engine resumes.
@ The rate-group thread begins driving doomgeneric_Tick on the next
@ schedIn pulse.
event EngineStarted() \
  severity activity high \
  id 0 \
  format "DOOM engine started"

@ Engine cooperatively stopped. The rate group will stop driving Tick.
event EngineStopped() \
  severity activity high \
  id 1 \
  format "DOOM engine stopped"

@ A key event arrived for a queue that was already full. Reported once
@ per overflow so the ground operator notices saturation.
event KeyQueueOverflow() \
  severity warning low \
  id 2 \
  format "DOOM key queue overflow - input dropped"

@ A Start command was issued while the engine was already running.
event AlreadyRunning() \
  severity warning low \
  id 3 \
  format "Start ignored - engine already running"

@ A Start command could not rendezvous with the rate-group thread:
@ a previous tick was still in flight after the bounded wait, or the
@ wait itself failed.
event StartBusy() \
  severity warning low \
  id 4 \
  format "Start rejected - could not rendezvous with the rate group"

@ No WAD path was configured, or the configured WAD could not be
@ opened. Start is rejected so these cases cannot take down the whole
@ flight process via the engine's internal error handler. The check is
@ advisory: a WAD that opens but is malformed, or is removed after the
@ check, can still reach the vendored error handler.
event WadUnavailable(
                      wadPath: string size 256 @< The WAD path that failed to open
                    ) \
  severity warning high \
  id 5 \
  format "Start rejected - cannot open WAD file {}"

@ The game was reset back to its initial (title screen) state in
@ response to a Reset command.
event EngineReset() \
  severity activity high \
  id 6 \
  format "DOOM engine reset to initial state"

@ A Reset command was issued before the engine was ever started;
@ there is no game state to reset.
event ResetNotStarted() \
  severity warning low \
  id 7 \
  format "Reset ignored - engine has never been started"
