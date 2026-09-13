# ----------------------------------------------------------------------
# Doom component events
# ----------------------------------------------------------------------

@ Engine started: the rate-group thread consumed a latched Start and
@ is now driving doomgeneric_Tick (State RUNNING). A Start after a
@ Stop resumes the existing engine.
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

@ A Start request was rejected; reason names the failed precondition
@ (ALREADY_RUNNING, START_PENDING, NOT_INITIALIZED, FAULTED).
event StartRejected(
                     reason: Doom.RequestStatus @< Which precondition failed
                   ) \
  severity warning high \
  id 3 \
  format "Start rejected - {}"

@ No WAD path was configured, or the configured WAD could not be
@ opened at initialization. The engine is not created and Start is
@ rejected with StartRejected(NOT_INITIALIZED).
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

@ A Reset request was rejected; reason names the failed precondition
@ (NOT_INITIALIZED, FAULTED). There is no game state to reset.
event ResetRejected(
                     reason: Doom.RequestStatus @< Which precondition failed
                   ) \
  severity warning low \
  id 7 \
  format "Reset rejected - {}"

@ The configured WAD opened but failed structural validation (IWAD
@ magic, directory bounds, required lumps). The engine is not created
@ and Start is rejected with StartRejected(NOT_INITIALIZED).
event WadInvalid(
                  wadPath: string size 256 @< The WAD path that failed validation
                  reason: Doom.WadStatus @< Which check failed
                ) \
  severity warning high \
  id 10 \
  format "Engine init rejected - WAD file {} invalid: {}"

@ The engine raised an internal fatal error (I_Error) or requested to
@ quit (I_Quit). The engine is stopped and permanently unavailable;
@ State is FAILED (no in-process recovery: the engine's globals are
@ left as-is). Replaces the upstream handler's process exit().
event EngineFault(
                   message: string size 128 @< The engine's error message
                 ) \
  severity warning high \
  id 9 \
  format "DOOM engine fault: {}"
