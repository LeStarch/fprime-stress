# yamcs-plugin: DOOM display for fprime-yamcs

A [yamcs-web](https://yamcs.org/) extension that renders the DOOM frame
telemetry produced by `Doom::DoomEngine` directly inside the YAMCS web
interface, and forwards keyboard input back as F Prime commands. It is
the project's browser display for the DOOM frame pipeline.

## Usage

Run [fprime-yamcs](https://github.com/fprime-community/fprime-yamcs)
with the extension directory:

```sh
fprime-yamcs \
    --yamcs-web-extension-dirs <path-to-fprime-stress>/yamcs-plugin/doom-display \
    --yamcs-realtime-only-channels 'DoomSubtopology.doom.FrameOut*' 'DoomSubtopology.doom.PaletteOut'
```

Open the YAMCS web UI and click the red **DOOM** button in the lower
right corner. Click the canvas to focus it, then use WASD / arrows /
Space / Ctrl. The panel offers three controls:

- **Start/Stop** — a single toggle that commands the engine and
  follows the engine's `State` telemetry channel, so the label stays
  correct even when the engine is started or stopped from elsewhere
  (autoStart, GDS, sequences) or the panel is closed and reopened.
- **Reset** — sends `Doom.Reset`, returning the game to its boot
  title screen (the engine is not torn down; input is flushed and
  the title sequence restarted; `EngineReset` / `ResetNotStarted`
  events report the outcome).
- **Record/Stop Recording** — a toggle that records every command
  sent from the panel; stopping downloads them as an F Prime textual
  `.seq` sequence file with relative time tags (verified against the
  fprime-gds `SeqFileParser`). Only commands accepted by YAMCS are
  recorded. Recording continues while the panel is closed or
  minimized and is capped at 10000 commands.

## How it works

- `fprime-yamcs --yamcs-web-extension-dirs` injects
  `doom-display/doom-display.js` as a module script into the yamcs-web
  index page (via the yamcs-web `addExtension` API).
- The script resolves the `FrameNN` / `Palette` packet containers,
  the `KeyDown`/`KeyUp`/`Start`/`Stop`/`Reset` commands, and the
  `State` telemetry parameter from the YAMCS MDB at runtime, so it
  works with any deployment embedding the Doom subtopology regardless
  of naming or base ids (if no `State` parameter exists next to the
  commands, the Start/Stop toggle falls back to click-seeded state).
- Frames arrive through a WebSocket subscription to the raw
  `tm_realtime` packet stream and are decoded from the binary
  packetized-telemetry (Svc.TlmPacketizer, APID 4) wire format; a
  parameter subscription would inflate the 8.6 MB/s binary stream
  roughly tenfold as protobuf-JSON.
- Deployments downlinking via Svc.TlmChan (per-channel packets,
  APID 1) are not supported; use packetized telemetry as configured in
  fprime-stress-reference.
