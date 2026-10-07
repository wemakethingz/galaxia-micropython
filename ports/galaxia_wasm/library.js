// JavaScript side of the galaxia_sim WebAssembly port. Runs in the Web Worker (or
// Node.js for tests). The page talks to the Worker with messages: worker.js fills
// Module.galaxia (input queue, GPIO levels) and receives output through its callbacks.

addToLibrary({
    $GALAXIA__postset: "var MP_JS_EPOCH = performance.now();",
    $GALAXIA: {},

    mp_js_ticks_ms__deps: ["$GALAXIA"],
    mp_js_ticks_ms: () => Math.floor(performance.now() - MP_JS_EPOCH),
    mp_js_ticks_us: () => Math.floor((performance.now() - MP_JS_EPOCH) * 1000) >>> 0,
    mp_js_time_ms: () => Date.now(),
    mp_js_random_u32: () => globalThis.crypto.getRandomValues(new Uint32Array(1))[0],

    // REPL output (UTF-8 bytes)
    galaxia_js_stdout: (ptr, len) => {
        Module.galaxia.stdout(HEAPU8.slice(ptr, ptr + len));
    },

    // Next byte typed on the REPL, or -1 when none is waiting
    galaxia_js_stdin_byte: () => {
        const q = Module.galaxia.stdin;
        return q.length ? q.shift() : -1;
    },

    galaxia_js_stdin_available: () => Module.galaxia.stdin.length,

    // While a program runs: take the interrupt character (Ctrl-C) out of the input
    // queue, as the board's USB receive handler does. Returns 1 if it was there.
    galaxia_js_stdin_take_interrupt: (c) => {
        const q = Module.galaxia.stdin;
        const i = q.indexOf(c);
        if (i < 0) {
            return 0;
        }
        q.splice(i, 1);
        return 1;
    },

    // Level of a GPIO input set by the page (buttons are active low: 1 = released)
    galaxia_js_gpio_get: (pin) => {
        const v = Module.galaxia.gpio[pin];
        return v === undefined ? -1 : v;     // -1: the page does not drive this pad
    },

    // Next GPIO whose level the page changed (edge for the interrupt handlers), or -1
    galaxia_js_gpio_next_changed: () => {
        const q = Module.galaxia.gpioChanged;
        return q.length ? q.shift() : -1;
    },

    // Touch pads: state set by the page (1 = touched), and the pads it changed
    galaxia_js_touch_get: (pad) => (Module.galaxia.touch[pad] ? 1 : 0),
    galaxia_js_touch_next_changed: () => {
        const q = Module.galaxia.touchChanged;
        return q.length ? q.shift() : -1;
    },

    // Light level on the LED, 0 (dark) to 255
    galaxia_js_light: () => Module.galaxia.light,

    // The RGB LED changed (0-255 each)
    galaxia_js_led: (r, g, b) => Module.galaxia.led(r, g, b),

    // Sound: a waveform looped at `rate` samples/s (a tone), queued samples (WAV), stop
    galaxia_js_sound_loop: (rate, ptr, len) => Module.galaxia.sound({ loop: true, rate, samples: HEAPU8.slice(ptr, ptr + len) }),
    galaxia_js_sound_chunk: (rate, ptr, len) => Module.galaxia.sound({ loop: false, rate, samples: HEAPU8.slice(ptr, ptr + len) }),
    galaxia_js_sound_stop: () => Module.galaxia.sound(null),

    // Radio (ESP-NOW broadcast): send a packet, and the next one received (its length,
    // or -1), with the sender's MAC address and the channel
    galaxia_js_radio_send: (channel, macPtr, dataPtr, len) => Module.galaxia.radioSend({
        channel, mac: HEAPU8.slice(macPtr, macPtr + 6), data: HEAPU8.slice(dataPtr, dataPtr + len) }),
    galaxia_js_radio_next: (dataPtr, max, macPtr, channelPtr) => {
        const p = Module.galaxia.radioIn.shift();
        if (!p) {
            return -1;
        }
        const n = Math.min(p.data.length, max);
        HEAPU8.set(p.data.subarray(0, n), dataPtr);
        HEAPU8.set(p.mac.subarray(0, 6), macPtr);
        HEAP32[channelPtr >> 2] = p.channel;
        return n;
    },

    // Wi-Fi network (net.c): send a frame to the other boards, and the next frame
    // received (its length, or -1)
    galaxia_js_lan_send: (ptr, len) => Module.galaxia.lanSend && Module.galaxia.lanSend(HEAPU8.slice(ptr, ptr + len)),
    galaxia_js_lan_next: (ptr, max) => {
        const f = Module.galaxia.lanIn && Module.galaxia.lanIn.shift();
        if (!f) {
            return -1;
        }
        const n = Math.min(f.length, max);
        HEAPU8.set(f.subarray(0, n), ptr);
        return n;
    },
    // The page's access points (the classroom's Wi-Fi): {ssid, password, authmode,
    // channel}; 0 when there is no access point `index`
    galaxia_js_page_ap__deps: ["$stringToUTF8"],
    galaxia_js_page_ap: (index, ssidPtr, passwordPtr, authmodePtr, channelPtr) => {
        const ap = (Module.galaxia.aps || [])[index];
        if (!ap) {
            return 0;
        }
        stringToUTF8(ap.ssid || "", ssidPtr, 33);
        stringToUTF8(ap.password || "", passwordPtr, 65);
        HEAP32[authmodePtr >> 2] = ap.authmode ?? (ap.password ? 3 : 0);
        HEAP32[channelPtr >> 2] = ap.channel || 6;
        return 1;
    },

    // The page router's DNS: the address of a name (the virtual broker's names),
    // 0 if it does not know it
    galaxia_js_dns__deps: ["$UTF8ToString"],
    galaxia_js_dns: (namePtr) => {
        const ip = (Module.galaxia.hosts || {})[UTF8ToString(namePtr).toLowerCase()];
        return ip ? (ip[0] | (ip[1] << 8) | (ip[2] << 16) | (ip[3] << 24)) >>> 0 : 0;
    },

    // This board's MAC address (6 bytes)
    galaxia_js_mac: (ptr) => HEAPU8.set(Module.galaxia.mac, ptr),

    // Screen: what the board shows changed (RGBA picture, width x height)
    galaxia_js_screen: (ptr, width, height) => {
        Module.galaxia.screen(HEAPU8.slice(ptr, ptr + width * height * 4), width, height);
    },

    // Sleep without burning CPU: the Worker's event loop runs meanwhile (Asyncify)
    galaxia_js_sleep__async: true,
    galaxia_js_sleep: (ms) => Asyncify.handleSleep((wakeUp) => {
        if (ms <= 0) {
            // A macrotask without the 4 ms clamp of nested setTimeout
            const ch = new MessageChannel();
            ch.port1.onmessage = () => wakeUp();
            ch.port2.postMessage(0);
        } else {
            Module.galaxia.wake = wakeUp;
            const t = setTimeout(() => {
                Module.galaxia.wake = null;
                wakeUp();
            }, ms);
            Module.galaxia.wakeTimer = t;
        }
    }),
});
