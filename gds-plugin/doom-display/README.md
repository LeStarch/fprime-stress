# doom-display: F Prime GDS JS Addon

A JavaScript-only `fprime-gds` addon that renders the DOOM frame
buffer (emitted as `DoomEngine.FrameOut` telemetry) onto an HTML
canvas and forwards keyboard input as F Prime commands.

## What it does

- Subscribes to the `DoomEngine.FrameOut` telemetry channel and
  reassembles the 20 incoming `FrameChunk` rows into a 320x200
  palette-indexed frame buffer.
- Subscribes to `DoomEngine.PaletteOut` and uses the latest 256-entry
  RGB palette to colourise the frame.
- Renders the frame onto an HTML5 canvas at 2x integer scale.
- Captures keyboard events on the canvas (WASD, arrows, Space/Ctrl,
  number keys 1-7, Shift, Escape, Tab) and translates them into
  `DoomEngine.KeyDown` / `DoomEngine.KeyUp` commands using the
  `DoomKey` enum.
- Provides Start / Stop buttons mapped to `DoomEngine.Start` /
  `DoomEngine.Stop`.

## Installation

The fprime-gds JS addon system loads addons from
`fprime_gds/flask/static/addons/` and enables them via
`fprime_gds/flask/static/addons/enabled.js`. To install this addon:

1. Copy this directory to your fprime-gds installation:

   ```bash
   FPRIME_GDS=$(python3 -c "import os, fprime_gds; print(os.path.dirname(fprime_gds.__file__))")
   cp -r doom-display "$FPRIME_GDS/flask/static/addons/"
   ```

2. Append the addon to `enabled.js`:

   ```bash
   echo 'import "./doom-display/addon.js";' >> "$FPRIME_GDS/flask/static/addons/enabled.js"
   ```

3. Restart `fprime-gds` and the **DOOM** panel will appear when you
   load the included dashboard:

   ```bash
   fprime-gds -d $(realpath dashboard.xml) ...
   ```

## Files

- `addon.js`      - Vue component (`<doom-display>`) implementation.
- `dashboard.xml` - Minimal fprime-gds dashboard that embeds the panel.
