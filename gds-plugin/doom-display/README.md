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

Run `../install.sh` (one level up from this directory). The script:

1. Copies this directory into the active fprime-gds package's
   `flask/static/addons/doom-display/`.
2. Appends `import "./doom-display/addon.js";` to that package's
   `enabled.js` (idempotent).

The **Dashboard** tab itself is enabled by the project-local
`fprime-gds.yml` in the deployment root, which uses its `flask`
section to override `JS_CONFIGURATION_FILE` to point at
`../config.js` (next to this addon). That `config.js` sets
`config.enableDashboards = true` without mutating the installed
fprime-gds site-packages tree.

After the script finishes, run `fprime-gds` from the deployment root,
open the **Dashboard** tab in your browser, click **Upload Dashboard
File**, and select `../dashboard.xml` (next to the install script).
The **DOOM** panel will appear.

## Files

- `addon.js`  - Vue component (`<doom-display>`) implementation.
- The matching dashboard XML lives one directory up at
  `gds-plugin/dashboard.xml` so it sits next to `install.sh`.
