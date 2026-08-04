# ----------------------------------------------------------------------
# Doom component commands
# ----------------------------------------------------------------------

# Commands are sync because the component is passive (no worker
# thread) and run on the command-dispatch thread. Key handlers are a
# short mutex-guarded enqueue; Start runs engine bring-up inline and
# may first wait (bounded, ~1 s) for an in-flight rate-group tick.

@ Start the wrapped DOOM engine. The first Start calls
@ doomgeneric_Create, which loads the WAD and runs DOOM's
@ initialisation through to the first idle tick; a Start after a Stop
@ resumes the existing engine. After this command completes, the
@ rate-group thread (via the schedIn port) drives doomgeneric_Tick.
sync command Start \
  opcode 0

@ Press and release a named DOOM key in a single tick. Useful for menu
@ navigation, weapon select, and one-shot inputs from command sequences.
sync command KeyTap(
                     key: Doom.DoomKey
                   ) \
  opcode 1

@ Press and hold a named DOOM key. The key remains held until a
@ matching KeyUp is issued or until the engine is stopped. Mirrors a
@ real keyboard well enough for "walk forward + fire" kinds of inputs
@ from a command sequence.
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
@ Use this from the GDS keyboard plugin to forward arbitrary keys
@ (numeric digits, characters, etc.) without growing the enum.
sync command RawKey(
                     pressed: bool
                     code: U8
                   ) \
  opcode 4

@ Stop the wrapped DOOM engine cooperatively. The rate group will
@ stop driving Tick and FrameOut telemetry will cease.
sync command Stop \
  opcode 5
