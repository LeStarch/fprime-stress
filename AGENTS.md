# fprime-stress — guide for AI agents and reviewers

This library follows the F Prime conventions in `lib/fprime/AGENTS.md`
of the consuming deployment (C++14, no exceptions/RTTI, no dynamic
allocation after init, `FW_ASSERT`, F Prime types) and the C/C++ rules
in `.github/skills/fprime-cpp-design/SKILL.md` there. The rules below
are project additions; reviewers should flag violations as must-fix.

## Project rules

### STRESS-1 — Enumerated statuses, not booleans

Any value that reports the *outcome* of an operation — a return status,
an event argument, a telemetry channel, a rejection reason — must be an
FPP `enum`, never `bool`. Each enumerator names one specific outcome or
failed precondition so a single warning event tells the operator *why*
(e.g. `StartRejected(reason: RequestStatus)` yielding `NOT_INITIALIZED`
rather than `false`).

- Define the enum in FPP (`Doom.fpp`) so it is shared by C++, events,
  telemetry, and the ground dictionary.
- Give every rejection path its own enumerator; do not fold distinct
  causes into one value or into a free-form string.
- Map enums to `Fw::CmdResponse` at the command boundary; never return
  the raw `bool` up through helpers.
- `bool` remains correct for genuine binary facts that are not
  statuses: a key being pressed, an internal latch such as
  "start requested", a per-frame flag.

Reviewer finding class: `stress-bool-status`. Developer checklist:
before adding a `bool` return or event argument, ask whether the
caller or operator would ever want to know *which* way it failed. If
yes, it is an enum.

### STRESS-2 — Validate before entering the engine

Ground-reachable inputs that reach stock `doomgeneric` (WAD path and
file, key codes, frame geometry) are validated in a dedicated helper
and rejected with a warning event carrying the failed check (per
STRESS-1) before the engine sees them: `setWadPath` / `validateWad`
(`WadPathRejected`, `WadInvalid`), `validateKeyCode` (`KeyRejected`),
the frame components' size checks (`InvalidFrame`). The engine's own
`I_Error` path is the last line of defence, never the first.

Key codes are an allow-list: only `DoomKey` enumerators reach the
engine, and the enum deliberately omits the `'y'` confirm and every
function key, so the Quit / End Game confirmation (`I_Quit` → terminal
`FAILED`) and the F2/F6/F9 save and quick-load shortcuts are unreachable
from the ground. Known residual: the in-menu Save Game entry is still
navigable with ESCAPE/DOWN/ENTER and writes `doomsav<n>.dsg` in the
working directory. Adding an enumerator is adding a ground-reachable
engine binding: check what stock `m_menu.c` / `g_game.c` do with it.

## Layout

| Path | Contents |
| --- | --- |
| `Doom/Doom.fpp` | Shared constants, structs, enums, ports. Model is source of truth. |
| `Doom/DoomEngine/` | Passive component hosting stock `doomgeneric` (git submodule; do not edit). |
| `Doom/FrameDownsampler/`, `Doom/FrameTlmProcessor/` | Frame pipeline components. |
| `Doom/DoomSubtopology/` | Subtopology wiring the pipeline. |
| `tools/fprime-get-doom/` | WAD fetch CLI. |
| `yamcs-plugin/` | YAMCS DOOM display. |

## Commands

```bash
cd Doom && fprime-util check -j"$(nproc)" --recursive
node --check yamcs-plugin/doom-display/doom-display.js
python3 -m py_compile tools/fprime-get-doom/src/fprime_get_doom/cli.py
```
