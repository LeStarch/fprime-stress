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
@ opened at initialization. The engine is not created and Start is
@ rejected with EngineUnavailable.
event WadUnavailable(
                      wadPath: string size 256 @< The WAD path that failed to open
                    ) \
  severity warning high \
  id 5 \
  format "Engine init rejected - cannot open WAD file {}"

@ The game was reset back to its initial (title screen) state in
@ response to a Reset command.
event EngineReset() \
  severity activity high \
  id 6 \
  format "DOOM engine reset to initial state"

@ A Reset command was issued with no created (or a faulted) engine;
@ there is no game state to reset.
event ResetNotStarted() \
  severity warning low \
  id 7 \
  format "Reset ignored - engine not initialized"

@ Start was rejected because initEngine did not succeed at
@ initialization, or the engine has since faulted (EngineFault).
event EngineUnavailable() \
  severity warning high \
  id 8 \
  format "Start rejected - engine not initialized or faulted"

@ The engine raised an internal fatal error (I_Error) or requested to
@ quit (I_Quit). The engine is stopped and permanently unavailable;
@ State is FAILED. Replaces the upstream handler's process exit().
event EngineFault(
                   message: string size 128 @< The engine's error message
                 ) \
  severity warning high \
  id 9 \
  format "DOOM engine fault: {}"
