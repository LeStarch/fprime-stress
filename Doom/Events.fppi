# ----------------------------------------------------------------------
# Doom component events
# ----------------------------------------------------------------------

@ Engine started successfully. doomgeneric_Create has returned and the
@ rate-group thread will begin driving doomgeneric_Tick on the next
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
