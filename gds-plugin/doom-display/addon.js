/**
 * doom-display: JS-only fprime-gds addon that paints the DOOM frame
 * buffer onto a Canvas and forwards keyboard input as F Prime commands.
 *
 * Subscribes to:
 *   - DoomEngine.FrameOut    (FrameChunk struct, 20 chunks per frame)
 *   - DoomEngine.PaletteOut  (Palette struct, 768 RGB bytes)
 *
 * Sends:
 *   - DoomEngine.KeyDown / DoomEngine.KeyUp (DoomKey enum)
 *   - DoomEngine.Start / DoomEngine.Stop
 *
 * Drop this directory into fprime-gds:
 *   <site-packages>/fprime_gds/flask/static/addons/doom-display/
 * Then add `import "./doom-display/addon.js";` to the addons enabled.js
 * (or use the patch-addons.sh script in this directory).
 */
import {_datastore, _dictionaries} from "../../js/datastore.js";
import {_loader} from "../../js/loader.js";

const FRAME_WIDTH = 320;
const FRAME_HEIGHT = 200;
const CHUNKS_PER_FRAME = 20;

// Default 256-entry RGB palette used until the first PaletteOut packet
// is observed. A grayscale ramp keeps the canvas readable on startup.
const DEFAULT_PALETTE = (function() {
    const arr = new Uint8Array(256 * 3);
    for (let i = 0; i < 256; i++) {
        arr[i * 3]     = i;
        arr[i * 3 + 1] = i;
        arr[i * 3 + 2] = i;
    }
    return arr;
})();

// Map browser keys to the DoomKey enum values declared in Doom.fpp.
// These names must match the auto-generated DoomKey enum constants.
const KEY_TO_DOOMKEY = {
    "ArrowUp":     "UP",
    "ArrowDown":   "DOWN",
    "ArrowLeft":   "LEFT",
    "ArrowRight":  "RIGHT",
    "w":           "UP",
    "s":           "DOWN",
    "a":           "STRAFE_L",
    "d":           "STRAFE_R",
    "Control":     "FIRE",
    " ":           "USE",
    "Enter":       "ENTER",
    "Shift":       "SHIFT",
    "Escape":      "ESCAPE",
    "Tab":         "TAB",
    "y":           "Y",
    "n":           "N",
    "1":           "WEAPON1",
    "2":           "WEAPON2",
    "3":           "WEAPON3",
    "4":           "WEAPON4",
    "5":           "WEAPON5",
    "6":           "WEAPON6",
    "7":           "WEAPON7",
    "p":           "PAUSE",
};

function findChannel(suffix) {
    const dict = _dictionaries.channels || {};
    for (const id in dict) {
        const name = (dict[id].full_name || "").toLowerCase();
        if (name.endsWith(suffix.toLowerCase())) {
            return Number(id);
        }
    }
    return null;
}

function findCommand(suffix) {
    const dict = _dictionaries.commands || {};
    for (const name in dict) {
        if (name.toLowerCase().endsWith(suffix.toLowerCase())) {
            return name;
        }
    }
    return null;
}

Vue.component("doom-display", {
    template: `
        <div class="doom-display">
            <div class="doom-controls" style="margin-bottom: 6px;">
                <button v-on:click="sendStart" :disabled="!startCmd">Start</button>
                <button v-on:click="sendStop"  :disabled="!stopCmd">Stop</button>
                <span style="margin-left: 12px;">
                    Frame: {{ lastFrame }} / Chunks rx: {{ chunksReceived }}
                    / Last key: {{ lastKey }}
                </span>
            </div>
            <canvas ref="canvas"
                    :width="canvasWidth" :height="canvasHeight"
                    tabindex="0"
                    style="background:#000; border:1px solid #444; outline:none; image-rendering: pixelated;"
                    v-on:keydown="onKeyDown"
                    v-on:keyup="onKeyUp"
                    v-on:click="focusCanvas">
            </canvas>
            <div style="font-size: 11px; color: #888; margin-top: 4px;">
                Click the canvas to focus, then use WASD / arrows / Space / Ctrl.
            </div>
        </div>
    `,
    data() {
        return {
            canvasWidth: FRAME_WIDTH * 2,
            canvasHeight: FRAME_HEIGHT * 2,
            chunksReceived: 0,
            lastFrame: 0,
            lastKey: "(none)",
            frameChannelId: null,
            paletteChannelId: null,
            startCmd: null,
            stopCmd: null,
            keyDownCmd: null,
            keyUpCmd: null,
            // Last seen FrameOut.val and PaletteOut.val. _datastore stores
            // the last value of each channel; we re-read on every tick.
            lastFrameVal: null,
            lastPaletteVal: null,
            _pixels: null,
            _palette: null,
            _intervalId: null,
            _heldKeys: {},
        };
    },
    mounted() {
        // _pixels: full 320x200 backbuffer; _palette: 768-byte RGB.
        this._pixels = new Uint8Array(FRAME_WIDTH * FRAME_HEIGHT);
        this._palette = new Uint8Array(DEFAULT_PALETTE);
        this.refreshDictionary();

        // _datastore.channels is a Vue-reactive object; rather than
        // attaching a watcher to each channel id, poll at 30 Hz which
        // matches DOOM's frame rate and is cheap enough.
        this._intervalId = setInterval(this.poll, 33);
    },
    beforeDestroy() {
        if (this._intervalId != null) {
            clearInterval(this._intervalId);
            this._intervalId = null;
        }
    },
    methods: {
        refreshDictionary() {
            this.frameChannelId   = findChannel(".FrameOut");
            this.paletteChannelId = findChannel(".PaletteOut");
            this.startCmd   = findCommand(".Start");
            this.stopCmd    = findCommand(".Stop");
            this.keyDownCmd = findCommand(".KeyDown");
            this.keyUpCmd   = findCommand(".KeyUp");
        },
        poll() {
            if (this.frameChannelId == null || this.paletteChannelId == null) {
                this.refreshDictionary();
                return;
            }
            const frameCh = _datastore.channels[this.frameChannelId];
            if (frameCh != null && frameCh.val != null && frameCh.val !== this.lastFrameVal) {
                this.lastFrameVal = frameCh.val;
                this.absorbChunk(frameCh.val);
            }
            const palCh = _datastore.channels[this.paletteChannelId];
            if (palCh != null && palCh.val != null && palCh.val !== this.lastPaletteVal) {
                this.lastPaletteVal = palCh.val;
                this.absorbPalette(palCh.val);
            }
        },
        absorbChunk(val) {
            // val is the deserialised FrameChunk struct - {width, frame,
            // row, rowCount, pixels:[bytes...]}. The exact field shape
            // depends on the GDS json serialisation; try both nested and
            // flat layouts.
            const chunk = val.value || val;
            const row      = Number(chunk.row);
            const rowCount = Number(chunk.rowCount);
            const frame    = Number(chunk.frame);
            const pixels   = chunk.pixels;
            if (!Array.isArray(pixels) && !(pixels instanceof Uint8Array)) {
                return;
            }
            const bytesThisChunk = rowCount * FRAME_WIDTH;
            const offset = row * FRAME_WIDTH;
            for (let i = 0; i < bytesThisChunk && i < pixels.length; i++) {
                this._pixels[offset + i] = pixels[i] & 0xff;
            }
            this.chunksReceived++;
            // After the final chunk of a frame (row + rowCount == FRAME_HEIGHT)
            // we have a complete frame - blit it.
            if ((row + rowCount) >= FRAME_HEIGHT) {
                this.lastFrame = frame;
                this.blit();
            }
        },
        absorbPalette(val) {
            const pal = val.value || val;
            const rgb = pal.rgb;
            if (!Array.isArray(rgb) && !(rgb instanceof Uint8Array)) {
                return;
            }
            for (let i = 0; i < this._palette.length && i < rgb.length; i++) {
                this._palette[i] = rgb[i] & 0xff;
            }
        },
        blit() {
            const canvas = this.$refs.canvas;
            if (canvas == null) {
                return;
            }
            const ctx = canvas.getContext("2d");
            if (ctx == null) {
                return;
            }
            // Build an ImageData at native 320x200, then use the canvas's
            // 2x css scaling for the visible size.
            const img = ctx.createImageData(FRAME_WIDTH, FRAME_HEIGHT);
            const data = img.data;
            for (let i = 0; i < FRAME_WIDTH * FRAME_HEIGHT; i++) {
                const idx = this._pixels[i];
                const palBase = idx * 3;
                data[i * 4]     = this._palette[palBase];
                data[i * 4 + 1] = this._palette[palBase + 1];
                data[i * 4 + 2] = this._palette[palBase + 2];
                data[i * 4 + 3] = 0xff;
            }
            // Render at native resolution into an offscreen canvas, then
            // scale up. createImageBitmap is async; for simplicity we
            // putImageData directly and let the canvas's intrinsic size
            // (320x200) be CSS-scaled to canvasWidth x canvasHeight.
            ctx.canvas.width  = FRAME_WIDTH;
            ctx.canvas.height = FRAME_HEIGHT;
            ctx.canvas.style.width  = (FRAME_WIDTH * 2) + "px";
            ctx.canvas.style.height = (FRAME_HEIGHT * 2) + "px";
            ctx.putImageData(img, 0, 0);
        },
        focusCanvas() {
            const canvas = this.$refs.canvas;
            if (canvas != null) {
                canvas.focus();
            }
        },
        translateKey(ev) {
            return KEY_TO_DOOMKEY[ev.key] || null;
        },
        sendCmd(fullName, args) {
            if (fullName == null) {
                return;
            }
            const payload = {
                "key": 0xfeedcafe,
                "arguments": (args || []).map((a) => ({"value": a})),
            };
            _loader.load("/commands/" + fullName, "PUT", payload).catch((err) => {
                console.warn("doom-display: command failed:", fullName, err);
            });
        },
        sendStart() { this.sendCmd(this.startCmd, []); },
        sendStop()  { this.sendCmd(this.stopCmd, []); },
        onKeyDown(ev) {
            const key = this.translateKey(ev);
            if (key == null) {
                return;
            }
            ev.preventDefault();
            // Suppress autorepeat - browsers fire keydown repeatedly while
            // the key is held; DOOM only wants the leading edge.
            if (this._heldKeys[key]) {
                return;
            }
            this._heldKeys[key] = true;
            this.lastKey = key + " down";
            this.sendCmd(this.keyDownCmd, [key]);
        },
        onKeyUp(ev) {
            const key = this.translateKey(ev);
            if (key == null) {
                return;
            }
            ev.preventDefault();
            this._heldKeys[key] = false;
            this.lastKey = key + " up";
            this.sendCmd(this.keyUpCmd, [key]);
        },
    },
});
