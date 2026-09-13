# ----------------------------------------------------------------------
# Doom component commands
# ----------------------------------------------------------------------

# Commands are sync because the component is passive (no worker
# thread) and run on the command-dispatch thread. No handler blocks
# beyond a short mutex hold: key handlers enqueue, Start/Reset latch a
# request that the rate-group thread applies on its next tick.

@ Request that the rate-group thread start driving the DOOM engine
@ created by initEngine at topology setup. Responds OK once the request
@ is accepted (State STARTING); the next schedIn tick performs the
@ start (EngineStarted, State RUNNING). A Start after a Stop resumes
@ the existing engine. Responds EXECUTION_ERROR and emits
@ StartRejected naming the failed precondition (ALREADY_RUNNING,
@ START_PENDING, NOT_INITIALIZED, FAULTED).
sync command Start \
  opcode 0

@ Press a named DOOM key on one engine tic and release it on the next,
@ so gameplay keys (FIRE, USE, WEAPONn) register. Useful for menu
@ navigation, weapon select, and one-shot inputs from command sequences.
sync command KeyTap(
                     key: Doom.DoomKey
                   ) \
  opcode 1

@ Press and hold a named DOOM key. The key remains held until a
@ matching KeyUp is issued (a Stop/Start cycle preserves engine-side
@ held-key state). Mirrors a real keyboard well enough for "walk
@ forward + fire" kinds of inputs from a command sequence.
sync command KeyDown(
                      key: Doom.DoomKey
                    ) \
  opcode 2

@ Release a previously held DOOM key. Safe to issue when the key is
@ not currently held.
sync command KeyUp(
                    key: Doom.DoomKey
                  ) \
  opcode 3

@ Press or release a raw DOOM key code, bypassing the named enum.
@ Intended for ground tooling or sequences that need arbitrary keys
@ (numeric digits, characters, etc.) without growing the enum.
sync command RawKey(
                     pressed: bool
                     code: U8
                   ) \
  opcode 4

@ Stop the wrapped DOOM engine cooperatively. The rate group will
@ stop driving Tick and frame output will cease. May block
@ behind an in-flight Start (worst case: first-Start bring-up).
sync command Stop \
  opcode 5

@ Reset the game back to its initial state (the title / attract
@ screen shown on a fresh boot). The reset is applied by the
@ rate-group thread on its next tick: the pending key queue is
@ flushed, any in-flight screen-wipe playback is discarded, and the
@ engine returns to the title sequence via its own start-title
@ machinery. The engine itself is not torn down (the upstream engine
@ has no clean shutdown path). A Reset while the engine is created
@ but stopped responds OK and latches the request; it is applied on
@ the first tick after the next Start. Responds EXECUTION_ERROR and
@ emits ResetRejected (NOT_INITIALIZED, FAULTED) if there is no
@ engine to reset.
sync command Reset \
  opcode 6
