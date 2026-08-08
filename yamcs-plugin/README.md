# yamcs-plugin: DOOM display for fprime-yamcs

A [yamcs-web](https://yamcs.org/) extension that renders the DOOM frame
telemetry produced by `Doom::DoomEngine` directly inside the YAMCS web
interface, and forwards keyboard input back as F Prime commands. It is
the YAMCS counterpart of the fprime-gds addon in `../gds-plugin`.

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
Space / Ctrl. Use the Start/Stop buttons to command the engine.

## How it works

- `fprime-yamcs --yamcs-web-extension-dirs` injects
  `doom-display/doom-display.js` as a module script into the yamcs-web
  index page (via the yamcs-web `addExtension` API).
- The script resolves the `FrameNN` / `Palette` packet containers and
  the `KeyDown`/`KeyUp`/`Start`/`Stop` commands from the YAMCS MDB at
  runtime, so it works with any deployment embedding the Doom
  subtopology regardless of naming or base ids.
- Frames arrive through a WebSocket subscription to the raw
  `tm_realtime` packet stream and are decoded from the binary
  packetized-telemetry (Svc.TlmPacketizer, APID 4) wire format; a
  parameter subscription would inflate the 8.6 MB/s binary stream
  roughly tenfold as protobuf-JSON.
- Deployments downlinking via Svc.TlmChan (per-channel packets,
  APID 1) are not supported; use packetized telemetry as configured in
  fprime-stress-reference.
