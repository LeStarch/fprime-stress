/**
 * doom-display: JS-only fprime-gds addon that paints the DOOM frame
 * buffer onto a Canvas and forwards keyboard input as F Prime commands.
 *
 * Subscribes to:
 *   - FrameTlmProcessor.FrameRow000..FrameRow399 (one FrameRow channel
 *     per downsampled scanline; this multiplex is what defeats
 *     TlmChan's slot-store collapse - see the library README)
 *   - FrameTlmProcessor.PaletteOut (Palette struct, 768 RGB bytes)
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
import {_dictionaries} from "../../js/datastore.js";
import {_loader} from "../../js/loader.js";
import {SaferParser} from "../../js/json.js";

// Native JSON.parse, saved before SaferParser overrode the global. The
// stock SaferParser's regex tokenizer is O(N**2) on the input and
// throws RangeError("Invalid array length") on the multi-MB channel
// responses produced by hundreds of FrameRowNNN channels at 35 Hz. The
// addon polls its own session and decodes responses with the native
// parser directly, since DOOM telemetry contains no NaN/Infinity/BigInt
// values that SaferParser was added to handle.
const nativeJsonParse = SaferParser.language_parse;

// Full resolution must match Doom/FpConstants in the library. The
// actual displayed size is dynamic: the FrameDownsampler's DOWNSAMPLE
// parameter shrinks both dimensions, and each FrameRow carries its
// width, so the display adapts per frame.
const MAX_WIDTH = 640;
const MAX_ROWS = 400;

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
                    Frame: {{ lastFrame }} ({{ frameWidth }}x{{ frameHeight }})
                    / Rows rx: {{ rowsReceived }}
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
            canvasWidth: MAX_WIDTH * 2,
            canvasHeight: MAX_ROWS * 2,
            rowsReceived: 0,
            lastFrame: 0,
            frameWidth: MAX_WIDTH,
            frameHeight: MAX_ROWS,
            lastKey: "(none)",
            // Map of channel id -> row index (FrameRow000..FrameRow399).
            frameChannelIds: null,
            paletteChannelId: null,
            startCmd: null,
            stopCmd: null,
            keyDownCmd: null,
            keyUpCmd: null,
            _pixels: null,
            _palette: null,
            _intervalId: null,
            // _heldKeys is initialised in mounted(); Vue 2 does not
            // proxy data() keys that start with _, so it cannot live
            // here as the proxy would shadow direct access through
            // `this._heldKeys`.
            _heldKeys: null,
            // Own-session polling - independent of the broken SaferParser
            // bulk pipeline. Each addon instance gets its own session key
            // and its own retrieval cursor through fprime_gds' RamHistory.
            _sessionKey: null,
            _pollInFlight: false,
        };
    },
    mounted() {
        // _pixels: worst-case MAX_WIDTH x MAX_ROWS backbuffer; _palette: 768-byte RGB.
        this._pixels = new Uint8Array(MAX_WIDTH * MAX_ROWS);
        this._palette = new Uint8Array(DEFAULT_PALETTE);
        // Direct instance property; bypasses Vue's reactive proxy.
        this._heldKeys = {};
        this.refreshDictionary();

        // Acquire a dedicated session up front so that the very first
        // poll starts from a known cursor and we don't race the loader's
        // session creation. The session endpoint is tiny so the broken
        // SaferParser is not a hazard here.
        const self = this;
        fetch("/session").then((r) => r.json()).then((data) => {
            self._sessionKey = data.session;
        }).catch((err) => {
            console.warn("doom-display: failed to acquire session:", err);
        });

        // Poll at 100 ms - small enough to keep each /channels response
        // well below the size where native JSON.parse starts to feel it,
        // fast enough that the frame counter stays close to the engine's
        // 35 Hz emission rate.
        this._intervalId = setInterval(this.poll, 100);
    },
    beforeDestroy() {
        if (this._intervalId != null) {
            clearInterval(this._intervalId);
            this._intervalId = null;
        }
    },
    methods: {
        refreshDictionary() {
            // Build an id->row-index map. Each FrameRowNNN channel
            // carries the downsampled scanline NNN.
            const ids = {};
            let resolved = 0;
            for (let i = 0; i < MAX_ROWS; i++) {
                const suffix = ".FrameRow" + String(i).padStart(3, "0");
                const cid = findChannel(suffix);
                if (cid != null) {
                    ids[cid] = i;
                    resolved++;
                }
            }
            this.frameChannelIds = resolved === MAX_ROWS ? ids : null;
            this.paletteChannelId = findChannel(".PaletteOut");
            this.startCmd   = findCommand(".Start");
            this.stopCmd    = findCommand(".Stop");
            this.keyDownCmd = findCommand(".KeyDown");
            this.keyUpCmd   = findCommand(".KeyUp");
        },
        poll() {
            // Retry dictionary resolution until the deployment dictionary
            // has loaded; channel ids are not known synchronously at mount.
            if (this.frameChannelIds == null || this.paletteChannelId == null ||
                this.keyDownCmd == null || this.startCmd == null) {
                this.refreshDictionary();
            }
            this.pullChannels();
        },
        pullChannels() {
            // Drain everything that has accumulated in the addon's private
            // session since the last poll. Native JSON.parse here is the
            // load-bearing detail - the broken SaferParser blows up on a
            // single multi-MB channel response, which is exactly what
            // hundreds of FrameRowNNN packets at 35 Hz produce.
            if (this._sessionKey == null || this._pollInFlight) { return; }
            this._pollInFlight = true;
            const url = "/channels?session=" + encodeURIComponent(this._sessionKey)
                + "&limit=8192";
            const self = this;
            fetch(url).then((r) => r.text()).then((text) => {
                const data = nativeJsonParse(text);
                const items = (data && data.history) || [];
                if (items.length > 0) {
                    self.onChannels(items);
                }
            }).catch((err) => {
                console.warn("doom-display: channel poll failed:", err);
            }).finally(() => {
                self._pollInFlight = false;
            });
        },
        onChannels(items) {
            if (this.frameChannelIds == null) {
                this.refreshDictionary();
                if (this.frameChannelIds == null) { return; }
            }
            let blitted = false;
            for (let i = 0; i < items.length; i++) {
                const item = items[i];
                if (item.val == null) { continue; }
                if (this.frameChannelIds[item.id] !== undefined) {
                    blitted = this.absorbRow(item.val) || blitted;
                } else if (item.id === this.paletteChannelId) {
                    this.absorbPalette(item.val);
                }
            }
            if (blitted) {
                this.blit();
            }
        },
        absorbRow(val) {
            // val is the deserialised FrameRow struct - {frame, row,
            // width, pixels:[bytes...]}; only the first `width` bytes
            // of pixels are valid.
            const rowStruct = val.value || val;
            const row    = Number(rowStruct.row);
            const width  = Number(rowStruct.width);
            const frame  = Number(rowStruct.frame);
            const pixels = rowStruct.pixels;
            if (!Array.isArray(pixels) && !(pixels instanceof Uint8Array)) {
                return false;
            }
            if (width < 1 || width > MAX_WIDTH || row < 0 || row >= MAX_ROWS) {
                return false;
            }
            // The downsampler shrinks both dimensions by the same factor,
            // so the frame height follows from the row width.
            const height = Math.round(MAX_ROWS * width / MAX_WIDTH);
            if (width !== this.frameWidth) {
                this.frameWidth = width;
                this.frameHeight = height;
                this._pixels.fill(0);
            }
            const offset = row * width;
            const limit = Math.min(width, pixels.length);
            for (let i = 0; i < limit; i++) {
                this._pixels[offset + i] = pixels[i] & 0xff;
            }
            this.rowsReceived++;
            // Last row of the frame triggers a blit.
            if ((row + 1) >= height) {
                this.lastFrame = frame;
                return true;
            }
            return false;
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
            // Build an ImageData at the current downsampled size, then
            // CSS-scale the canvas up to the fixed on-screen size.
            const width = this.frameWidth;
            const height = this.frameHeight;
            const img = ctx.createImageData(width, height);
            const data = img.data;
            for (let i = 0; i < width * height; i++) {
                const idx = this._pixels[i];
                const palBase = idx * 3;
                data[i * 4]     = this._palette[palBase];
                data[i * 4 + 1] = this._palette[palBase + 1];
                data[i * 4 + 2] = this._palette[palBase + 2];
                data[i * 4 + 3] = 0xff;
            }
            // putImageData at native size and let the canvas's intrinsic
            // size be CSS-scaled to a fixed on-screen size.
            ctx.canvas.width  = width;
            ctx.canvas.height = height;
            ctx.canvas.style.width  = (MAX_WIDTH * 2) + "px";
            ctx.canvas.style.height = (MAX_ROWS * 2) + "px";
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
            // The REST shape is a flat array of enum literal / scalar
            // values, not a list of `{value: ...}` wrappers; the
            // fprime-gds Flask serializer rejects the wrapper form with
            // an enum-validation error.
            const payload = {
                "key": 0xfeedcafe,
                "arguments": args || [],
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
