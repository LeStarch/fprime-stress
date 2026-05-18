/**
 * doom-display: JS-only fprime-gds addon that paints the DOOM frame
 * buffer onto a Canvas and forwards keyboard input as F Prime commands.
 *
 * Subscribes to:
 *   - DoomEngine.FrameOut00..FrameOut79 (80 distinct FrameChunk channels,
 *     one per scanline group of 5 rows; this multiplex is what defeats
 *     TlmChan's slot-store collapse - see the library README)
 *   - DoomEngine.PaletteOut             (Palette struct, 768 RGB bytes)
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
// responses produced by 80 FrameOutNN telemetry channels at 35 Hz. The
// addon polls its own session and decodes responses with the native
// parser directly, since DOOM telemetry contains no NaN/Infinity/BigInt
// values that SaferParser was added to handle.
const nativeJsonParse = SaferParser.language_parse;

// Resolution must match Doom/FpConstants in the library:
// FRAME_WIDTH=640, FRAME_HEIGHT=400, ROWS_PER_CHUNK=5 => 80 chunks/frame.
const FRAME_WIDTH = 640;
const FRAME_HEIGHT = 400;
const CHUNKS_PER_FRAME = 80;

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
            // Set of 80 channel ids (FrameOut00..FrameOut79) used as a
            // lookup table for chunk dispatch. Indexed by chunk
            // position, value is the channel id.
            frameChannelIds: null,
            paletteChannelId: null,
            startCmd: null,
            stopCmd: null,
            keyDownCmd: null,
            keyUpCmd: null,
            _pixels: null,
            _palette: null,
            _intervalId: null,
            _heldKeys: {},
            // Own-session polling - independent of the broken SaferParser
            // bulk pipeline. Each addon instance gets its own session key
            // and its own retrieval cursor through fprime_gds' RamHistory.
            _sessionKey: null,
            _pollInFlight: false,
        };
    },
    mounted() {
        // _pixels: full FRAME_WIDTH x FRAME_HEIGHT backbuffer; _palette: 768-byte RGB.
        this._pixels = new Uint8Array(FRAME_WIDTH * FRAME_HEIGHT);
        this._palette = new Uint8Array(DEFAULT_PALETTE);
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
            // Build an id->chunk-index map. Each FrameOutNN channel
            // carries the chunk at scanline group NN.
            const ids = {};
            let resolved = 0;
            for (let i = 0; i < CHUNKS_PER_FRAME; i++) {
                const suffix = ".FrameOut" + (i < 10 ? "0" + i : "" + i);
                const cid = findChannel(suffix);
                if (cid != null) {
                    ids[cid] = i;
                    resolved++;
                }
            }
            this.frameChannelIds = resolved === CHUNKS_PER_FRAME ? ids : null;
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
            // single multi-MB channel response, which is exactly what 80
            // FrameOutNN packets at 35 Hz produce.
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
                    blitted = this.absorbChunk(item.val) || blitted;
                } else if (item.id === this.paletteChannelId) {
                    this.absorbPalette(item.val);
                }
            }
            if (blitted) {
                this.blit();
            }
        },
        absorbChunk(val) {
            // val is the deserialised FrameChunk struct - {frame, row,
            // rowCount, width, pixels:[bytes...]}.
            const chunk = val.value || val;
            const row      = Number(chunk.row);
            const rowCount = Number(chunk.rowCount);
            const frame    = Number(chunk.frame);
            const pixels   = chunk.pixels;
            if (!Array.isArray(pixels) && !(pixels instanceof Uint8Array)) {
                return false;
            }
            const bytesThisChunk = rowCount * FRAME_WIDTH;
            const offset = row * FRAME_WIDTH;
            const limit = Math.min(bytesThisChunk, pixels.length);
            for (let i = 0; i < limit; i++) {
                this._pixels[offset + i] = pixels[i] & 0xff;
            }
            this.chunksReceived++;
            // Last chunk of the frame triggers a blit.
            if ((row + rowCount) >= FRAME_HEIGHT) {
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
            // Build an ImageData at native FRAME_WIDTH x FRAME_HEIGHT,
            // then use the canvas's 2x css scaling for the visible size.
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
            // putImageData at native size and let the canvas's intrinsic
            // size be CSS-scaled to canvasWidth x canvasHeight (2x).
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
