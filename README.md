# fprime-stress

An F Prime library that wraps the open-source DOOM engine
([doomgeneric](https://github.com/ozkl/doomgeneric)) as a single
F Prime active component, together with an `F Prime subtopology` that
lets any deployment absorb the whole DOOM subsystem in one declaration.

This library is the source-of-truth for the Doom component and its
subtopology. A companion deployment lives at
[`JPL-Devin/fprime-stress-reference`](https://github.com/JPL-Devin/fprime-stress-reference)
and stitches it together with the standard F Prime services
(CdhCore + ComCcsds + FileHandling + CmdSequencer).

## Contents

```
Doom/                           - DoomEngine component
  Doom.fpp / Doom.cpp / Doom.hpp
  Commands.fppi
  Telemetry.fppi
  Events.fppi
  doomgeneric/                  - vendored upstream DOOM source (GPLv2)
DoomSubtopology/                - reusable subtopology wrapper
  DoomSubtopology.fpp
  DoomSubtopologyConfig/
  SubtopologyTopologyDefs.hpp
  PingEntries.hpp
gds-plugin/                     - JS-only fprime-gds addon
  doom-display/                 - Vue component, dashboard
  install.sh                    - one-shot installer
THIRDPARTY/                     - upstream license attribution
LICENSE                         - root license (GPLv2; see below)
```

## Architecture

* The component is **driven from the rate group**. The deployment
  wires a `Svc.Sched` member output of a rate group to
  `DoomSubtopology.Subtopology.schedIn`; each pulse runs one
  `doomgeneric_Tick()` (one DOOM frame of game logic) on the
  rate-group thread. Pace the rate group at ~30 Hz to match DOOM's
  native 35 fps cadence.
* `doomgeneric_Create()` runs **synchronously** in the `Start`
  command handler. When it returns, the engine is initialised and the
  rate group can start driving it.
* The frame buffer is published as `FrameOut` telemetry of type
  `Doom.FrameChunk` (rows packed into ~3 KB chunks). The palette is
  published as `PaletteOut` whenever it changes.
* Inputs arrive as `KeyTap` / `KeyDown` / `KeyUp` / `RawKey`
  commands and are translated into queue entries that DOOM consumes
  through `DG_GetKey`. The key queue is the only piece of
  cross-thread state and is guarded by `Os::Mutex`.
* All status output uses **FPP events** (`EngineStarted`,
  `EngineStopped`, `KeyQueueOverflow`, `AlreadyRunning`) - no
  direct `Fw::Logger` calls.

## Memory

The component reserves all of its working buffers (frame buffer,
palette, key queue, argv storage) as fixed-size members inside the
DoomEngine instance. Nothing in the F Prime glue calls `malloc` after
initialisation. doomgeneric's own `Z_Init` zone arena is the only
remaining heap call and it is performed once on the first `Start`
command from inside upstream source we deliberately do not modify.

## Licensing

Upstream doomgeneric is vendored verbatim under `Doom/doomgeneric/`
and is GPLv2 (see `Doom/doomgeneric/COPYING`). Because the final
deployment binary links this code, the whole binary is GPLv2. The new
F Prime glue (the DoomEngine component, the DoomSubtopology, and the
GDS plugin) is GPLv2 to match the linked work. The F Prime framework
itself is unchanged Apache-2.0. See `LICENSE` and
`THIRDPARTY/doomgeneric.md` for the full discussion.
