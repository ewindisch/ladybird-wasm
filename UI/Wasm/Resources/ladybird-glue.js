// ladybird-glue.js — pre-JS glue loaded before the WASM module initialises.
// Sets up the Ladybird canvas, event wiring, and the ladybirdOnPaint callback.
// This file is embedded via --pre-js and runs in the module scope.

var Ladybird = {
    canvas: null,
    ctx: null,
    initialized: false,

    init: async function(canvasId, width, height) {
        this.canvas = document.getElementById(canvasId);
        if (!this.canvas) {
            console.error('[Ladybird] canvas not found:', canvasId);
            return false;
        }
        this.canvas.width  = width;
        this.canvas.height = height;
        this.ctx = this.canvas.getContext('2d', { alpha: false, desynchronized: true });
        if (!this.ctx) {
            console.error('[Ladybird] failed to get 2D context');
            return false;
        }
        this.setupEventHandlers();
        let result;
        try {
            // Use async:true so ccall returns a Promise — required when
            // wasm_initialize is Asyncified (reachable from tor_socket_* imports
            // via ASYNCIFY static analysis).  If it runs synchronously the
            // Promise resolves immediately; if it suspends we await the resume.
            result = await Module.ccall('wasm_initialize', 'number',
                ['number', 'number'], [width, height], { async: true });
        } catch (e) {
            console.error('[Ladybird] wasm_initialize threw:', e);
            return false;
        }
        if (result !== 0) {
            console.error('[Ladybird] wasm_initialize failed:', result);
            return false;
        }
        this.initialized = true;
        return true;
    },

    setupEventHandlers: function() {
        const canvas = this.canvas;

        const coords = (e) => {
            const r = canvas.getBoundingClientRect();
            return { x: Math.floor(e.clientX - r.left), y: Math.floor(e.clientY - r.top) };
        };

        const mods = (e) => {
            let m = 0;
            if (e.shiftKey) m |= 1;
            if (e.ctrlKey)  m |= 2;
            if (e.altKey)   m |= 4;
            if (e.metaKey)  m |= 8;
            return m;
        };

        canvas.addEventListener('mousedown', (e) => {
            const p = coords(e);
            Module.ccall('wasm_mouse_down', 'void', ['number','number','number','number'],
                [p.x, p.y, e.button, mods(e)]);
            e.preventDefault();
        });

        canvas.addEventListener('mouseup', (e) => {
            const p = coords(e);
            Module.ccall('wasm_mouse_up', 'void', ['number','number','number','number'],
                [p.x, p.y, e.button, mods(e)]);
            e.preventDefault();
        });

        canvas.addEventListener('mousemove', (e) => {
            const p = coords(e);
            Module.ccall('wasm_mouse_move', 'void', ['number','number','number'],
                [p.x, p.y, mods(e)]);
        });

        // Translate a DOM KeyboardEvent.key string to the ui_name expected by
        // Ladybird's key_code_from_string (Libraries/LibWeb/UIEvents/KeyCode.h).
        //
        // Notable differences from DOM:
        //   DOM "Enter"     → Ladybird "Return"
        //   DOM "ArrowLeft" → Ladybird "Left"  (same for Right/Up/Down)
        //   DOM " "         → Ladybird "Space"
        //   DOM "Control"   → Ladybird "LeftControl" / "RightControl" (by location)
        //   DOM "Shift"     → Ladybird "LeftShift"   / "RightShift"
        //   DOM "Alt"       → Ladybird "LeftAlt"     / "RightAlt"
        //   DOM "Meta"      → Ladybird "LeftSuper"   / "RightSuper"
        //   DOM "AltGraph"  → Ladybird "AltGr"
        //   DOM lowercase   → Ladybird uppercase  (e.g. "a" → "A")
        //   Any unrecognised multi-char key is passed through; key_code_from_string
        //   returns Key_Invalid for truly unknown strings (safe no-op).
        const domToLadybirdKey = (e) => {
            const k = e.key || '';
            const loc = e.location || 0; // KeyboardEvent.DOM_KEY_LOCATION_RIGHT = 2
            switch (k) {
                case 'Enter':      return 'Return';
                case 'ArrowLeft':  return 'Left';
                case 'ArrowRight': return 'Right';
                case 'ArrowUp':    return 'Up';
                case 'ArrowDown':  return 'Down';
                case ' ':          return 'Space';
                case 'Control':    return loc === 2 ? 'RightControl' : 'LeftControl';
                case 'Shift':      return loc === 2 ? 'RightShift'   : 'LeftShift';
                case 'Alt':        return loc === 2 ? 'RightAlt'     : 'LeftAlt';
                case 'Meta':       return loc === 2 ? 'RightSuper'   : 'LeftSuper';
                case 'AltGraph':   return 'AltGr';
                default:
                    // Single printable character: Ladybird stores A-Z in uppercase.
                    if (k.length === 1) return k.toUpperCase();
                    // Multi-char (F1-F12, Tab, Backspace, Escape, etc.) — pass through.
                    return k;
            }
        };

        canvas.addEventListener('keydown', (e) => {
            const keyPtr = Module.stringToNewUTF8(domToLadybirdKey(e));
            Module.ccall('wasm_key_down', 'void', ['number','number'], [keyPtr, mods(e)]);
            Module._free(keyPtr);
            e.preventDefault();
        });

        canvas.addEventListener('keyup', (e) => {
            const keyPtr = Module.stringToNewUTF8(domToLadybirdKey(e));
            Module.ccall('wasm_key_up', 'void', ['number','number'], [keyPtr, mods(e)]);
            Module._free(keyPtr);
            e.preventDefault();
        });

        canvas.addEventListener('wheel', (e) => {
            Module.ccall('wasm_scroll', 'void', ['number','number'],
                [Math.round(e.deltaX), Math.round(e.deltaY)]);
            e.preventDefault();
        }, { passive: false });

        canvas.addEventListener('contextmenu', (e) => e.preventDefault());

        canvas.tabIndex = 0;
        canvas.focus();
    },

    // Called from WasmPageClient::page_did_paint via EM_ASM
    onPaint: function() {
        if (!this.initialized || !this.ctx) return;

        try {
            const width   = Module.ccall('wasm_get_bitmap_width',  'number', [], []);
            const height  = Module.ccall('wasm_get_bitmap_height', 'number', [], []);
            const dataPtr = Module.ccall('wasm_get_bitmap_data',   'number', [], []);

            if (!dataPtr || width === 0 || height === 0) return;

            const pixelCount = width * height * 4;
            // Re-read HEAPU8 after each WASM call in case memory grew (ALLOW_MEMORY_GROWTH).
            // Capture as a plain copy to avoid detached-ArrayBuffer errors if memory
            // grows again between the subarray() call and putImageData().
            const heapu8 = Module.HEAPU8;
            if (!heapu8 || heapu8.buffer.byteLength === 0) return;

            // Convert BGRA → RGBA using a fresh array (never backed by WASM memory)
            const rgba = new Uint8ClampedArray(pixelCount);
            for (let i = 0; i < pixelCount; i += 4) {
                rgba[i]     = heapu8[dataPtr + i + 2];   // R
                rgba[i + 1] = heapu8[dataPtr + i + 1];   // G
                rgba[i + 2] = heapu8[dataPtr + i];       // B
                rgba[i + 3] = heapu8[dataPtr + i + 3];   // A
            }

            this.ctx.putImageData(new ImageData(rgba, width, height), 0, 0);
        } catch (e) {
            // Swallow paint errors (e.g. "Cannot perform Construct on a detached
            // ArrayBuffer" when WASM memory grows mid-paint).  The next RAF tick
            // will repaint successfully once memory views are stable.
            console.warn('[Ladybird] onPaint error (will retry on next frame):', e.message);
        }
    },

    navigate: function(url) {
        if (!this.initialized) return false;
        const ptr = Module.stringToNewUTF8(url);
        // wasm_navigate returns immediately — it only queues a deferred_invoke.
        // The actual navigation (including any spin_until loops) runs asynchronously
        // from the main event loop, so no ASYNCIFY wrapping is needed here.
        Module.ccall('wasm_navigate', 'void', ['number'], [ptr]);
        Module._free(ptr);
        return true;
    },

    resize: function(width, height) {
        if (!this.initialized) return false;
        this.canvas.width  = width;
        this.canvas.height = height;
        Module.ccall('wasm_resize', 'void', ['number','number'], [width, height]);
        return true;
    }
};

// Global callback invoked from WasmPageClient::page_did_paint (via EM_ASM)
window.ladybirdOnPaint = function() { Ladybird.onPaint(); };
window.Ladybird = Ladybird;
