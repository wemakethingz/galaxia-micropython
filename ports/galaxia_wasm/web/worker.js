// Web Worker running one WebAssembly Galaxia.
//
// Messages from the page:
//   {type: "start", state}                  boot; state = {flash, log, eeprom, mac} kept
//                                           across reboots (FAT image, SPIFFS log, settings
//                                           EEPROM, MAC address)
//   {type: "in", data: Uint8Array}          bytes typed on the REPL
//   {type: "gpio", pin, level, t}           buttons A (45) / B (43) / SYSTEME (0): 0 =
//                                           pressed; t: when (the page's performance.now())
//   {type: "touch", pad, touched, t}        touch pads N 8, S 4, E 5, W 9
//   {type: "accel", x, y, z}                acceleration, milli-g, board axes
//   {type: "compass", heading}             the heading thingz.compass.heading() reports
//   {type: "magnetic", x, y, z}             raw magnetic field, milligauss, magnetometer axes
//   {type: "temperature", value}            chip temperature, degrees C
//   {type: "light", value}                  light level on the LED, 0-255
//   {type: "aps", aps, hosts}               the page's Wi-Fi access points:
//                                           [{ssid, password, authmode, channel}], and
//                                           the names their router's DNS knows
//   {type: "upload", files, main}           a program: files [{path, data: Uint8Array}]
//                                           written straight into the flash, main the
//                                           file to execute; the program running stops
//                                           and the board soft-resets to run it
//   {type: "key", id, op, path, data}       the USB key as the computer sees it
//                                           (usb_key.c), op: list, read, write, mkdir,
//                                           remove, space; answered by {type: "key",
//                                           id, ...result} or {type: "key", id, error}
//                                           (-FRESULT, or "off")
//   {type: "get-state"}                     answered by {type: "state", state}
// Messages to the page:
//   {type: "ready"}                         booted (main.py or the REPL comes next)
//   {type: "out", data: Uint8Array}         REPL output
//   {type: "screen", data: RGBA, width, height}
//   {type: "led", r, g, b}
//   {type: "sound", sound: {loop, rate, samples} | null}
//   {type: "uploaded", ok}                  the uploaded files are in the flash
//   {type: "state", state}
//   {type: "crash", reason, state}          the firmware stopped (the board reboots)
//   {type: "reset", state, sleepMs}         the board resets (machine.reset, watchdog), or
//                                           sleeps deeply for sleepMs (-1: until reset)
//   {type: "error", message}                could not start
// The radio (ESP-NOW broadcast) goes between the boards of the same browser through
// a BroadcastChannel.
import createGalaxia from "./galaxia.mjs";

let Module = null;
let ended = false;
const radio = new BroadcastChannel("galaxia-radio");
// Wi-Fi frames (net.c): the boards of the same browser are on the same air
const lan = new BroadcastChannel("galaxia-lan");

const galaxia = {
    stdin: [],
    gpio: {},
    gpioChanged: [],
    touch: {},
    touchChanged: [],
    inputs: [],                 // buttons and pads, applied in time (library.js)
    inputApplied: null,
    light: 128,
    temperature: 30,
    mac: new Uint8Array(6),
    accel: [0, 0, -1000],       // lying flat, screen up (board axes, milli-g)
    heading: 0,                 // compass heading, degrees (or a raw field: magnetic)
    magnetic: null,
    radioIn: [],
    wake: null,
    wakeTimer: null,
    ready: () => (flushOut(), postMessage({ type: "ready" })),
    stdout: (bytes) => stdout(bytes),
    screen: (rgba, width, height) => {
        const data = new Uint8ClampedArray(rgba.buffer);
        postMessage({ type: "screen", data, width, height }, [data.buffer]);
    },
    led: (r, g, b) => postMessage({ type: "led", r, g, b }),
    sound: (sound) => postMessage({ type: "sound", sound }, sound ? [sound.samples.buffer] : []),
    radioSend: (packet) => radio.postMessage(packet),
    lanIn: [],
    lanSend: (frame) => lan.postMessage(frame),
    aps: [],                    // the page's access points (the classroom's Wi-Fi)
    hosts: {},                  // their router's DNS: name -> [a, b, c, d]
    reset: (reason, wake, sleepMs) => reset(reason, wake, sleepMs),
    upload: null,               // a program the page sent (library.js)
    uploaded: (ok) => (flushOut(), postMessage({ type: "uploaded", ok })),
    boot: null,                 // what the previous run left (bootInfo)
};

// REPL output, as the USB sends it: in packets. The firmware writes a few bytes at a
// time (a print is its text, then "\r\n"); a message each, a program printing in a
// loop sends the page more than it can take, and the page falls seconds behind. The
// pieces go together, every OUT_EVERY_MS or OUT_MAX bytes.
const OUT_EVERY_MS = 16;
const OUT_MAX = 64 * 1024;
let out = [], outBytes = 0, outTimer = null;

function stdout(bytes) {
    out.push(bytes);
    outBytes += bytes.length;
    if (outBytes >= OUT_MAX) {
        flushOut();
    } else if (!outTimer) {
        outTimer = setTimeout(flushOut, OUT_EVERY_MS);
    }
}

function flushOut() {
    clearTimeout(outTimer);
    outTimer = null;
    if (!outBytes) {
        return;
    }
    const data = new Uint8Array(outBytes);
    let at = 0;
    for (const piece of out) {
        data.set(piece, at);
        at += piece.length;
    }
    out = [];
    outBytes = 0;
    postMessage({ type: "out", data }, [data.buffer]);
}

radio.onmessage = (e) => {
    galaxia.radioIn.push(e.data);
    if (galaxia.radioIn.length > 64) {
        galaxia.radioIn.shift();        // nobody reads: as packets lost in the air
    }
    wake();
};

lan.onmessage = (e) => {
    galaxia.lanIn.push(e.data);
    if (galaxia.lanIn.length > 256) {
        galaxia.lanIn.shift();          // nobody reads: lost in the air
    }
    wake();
};

// What survives a reboot: the FAT partition, the log, the EEPROM, the MAC address
function boardState() {
    const state = { mac: galaxia.mac, flash: null, log: null, eeprom: null };
    try {
        const ptr = Module._galaxia_flash_image();
        state.flash = Module.HEAPU8.slice(ptr, ptr + Module._galaxia_flash_size());
        const eeprom = Module._galaxia_eeprom_image();
        state.eeprom = Module.HEAPU8.slice(eeprom, eeprom + Module._galaxia_eeprom_size());
    } catch { /* memory unusable */ }
    try {
        state.log = Module.FS.readFile("/spiffs/data.csv");
    } catch { /* no log */ }
    return state;
}

const transfers = (state) => [state.flash, state.log, state.eeprom, state.boot && state.boot.rtcMem]
    .filter(Boolean).map((a) => a.buffer);

// esp_reset_reason_t of ESP-IDF
const ESP_RST_PANIC = 4;

// What the chip keeps across a reset that is not a power-on: the cause, the RTC
// clock (us) and the RTC memory (machine_hw.c)
function bootInfo(reason, wake = 0) {
    const boot = { reason, wake };
    try {
        boot.rtcUs = Module._galaxia_rtc_now_us();
        const ptr = Module._galaxia_rtc_mem();
        boot.rtcMem = Module.HEAPU8.slice(ptr, ptr + Module._galaxia_rtc_mem_size());
    } catch { /* memory unusable: as a power-on */ }
    return boot;
}

// The firmware aborted (as the board panics and reboots): hand the state to the page
function crashed(reason) {
    if (ended) {
        return;
    }
    ended = true;
    const state = boardState();
    state.boot = bootInfo(ESP_RST_PANIC);
    flushOut();                         // what it printed last, before it goes
    postMessage({ type: "crash", reason: String(reason), state }, transfers(state));
    close();
}

// The board resets (machine.reset(), the watchdog, a deep sleep of `sleepMs`, -1:
// until the reset button)
function reset(reason, wake, sleepMs) {
    if (ended) {
        return;
    }
    ended = true;
    const state = boardState();
    state.boot = bootInfo(reason, wake);
    flushOut();
    postMessage({ type: "reset", state, sleepMs }, transfers(state));
    close();
}

// The computer's operations on the USB key: the FAT partition the firmware has
// mounted, read and written block by block (usb_key.c)
function usbKey(m) {
    if (!Module || ended) {
        return { error: "off" };
    }
    const M = Module;
    const call = (name, types, args) => M.ccall(name, "number", types, args);
    const withBuffer = (size, fn) => {
        const p = M._malloc(Math.max(1, size));
        try {
            return fn(p);
        } finally {
            M._free(p);
        }
    };
    switch (m.op) {
        case "list":
            return withBuffer(1 << 16, (p) => {
                const n = call("galaxia_key_list", ["string", "number", "number"], [m.path, p, 1 << 16]);
                if (n < 0) return { error: n };
                const entries = M.UTF8ToString(p, n).split("\n").filter(Boolean).map((line) => {
                    const [kind, size, ...name] = line.split("\t");
                    return { name: name.join("\t"), dir: kind === "d", size: Number(size) };
                });
                return { entries };
            });
        case "read": {
            const size = call("galaxia_key_size", ["string"], [m.path]);
            if (size < 0) return { error: size };
            return withBuffer(size, (p) => {
                const n = call("galaxia_key_read", ["string", "number", "number"], [m.path, p, size]);
                return n < 0 ? { error: n } : { data: M.HEAPU8.slice(p, p + n) };
            });
        }
        case "write":
            return withBuffer(m.data.length, (p) => {
                M.HEAPU8.set(m.data, p);
                const n = call("galaxia_key_write", ["string", "number", "number"], [m.path, p, m.data.length]);
                return n < 0 ? { error: n } : {};
            });
        case "mkdir":
        case "remove": {
            const r = call(m.op === "mkdir" ? "galaxia_key_mkdir" : "galaxia_key_remove", ["string"], [m.path]);
            return r < 0 ? { error: r } : {};
        }
        case "space":
            return withBuffer(16, (p) => {
                const r = call("galaxia_key_space", ["number"], [p]);
                return r < 0 ? { error: r } : { free: M.HEAPF64[p >> 3], total: M.HEAPF64[(p >> 3) + 1] };
            });
        default:
            return { error: "op" };
    }
}

// An input wakes the board early from a wait (REPL prompt, sleep)
function wake() {
    const w = galaxia.wake;
    if (w) {
        galaxia.wake = null;
        clearTimeout(galaxia.wakeTimer);
        w();
    }
}

async function start(state = {}) {
    galaxia.boot = state.boot || null;
    if (state.mac) {
        galaxia.mac = state.mac;
    } else {
        crypto.getRandomValues(galaxia.mac);
        galaxia.mac[0] = (galaxia.mac[0] & 0xfe) | 0x02;   // locally administered, unicast
    }
    try {
        Module = await createGalaxia({
            galaxia,
            print: (t) => console.log("[galaxia]", t),
            printErr: (t) => console.warn("[galaxia]", t),
            onAbort: (what) => crashed(what),
        });
        Module.galaxia = galaxia;
        if (state.eeprom) {
            // before the compass field: it depends on the calibration stored there
            const ptr = Module._galaxia_eeprom_image();
            Module.HEAPU8.set(state.eeprom.subarray(0, Module._galaxia_eeprom_size()), ptr);
        }
        Module._galaxia_set_accel(...galaxia.accel);
        if (galaxia.magnetic) {
            Module._galaxia_set_magnetic(...galaxia.magnetic);
        } else {
            Module._galaxia_set_compass(galaxia.heading, 300, -400);
        }
        if (state.flash) {
            const ptr = Module._galaxia_flash_image();
            Module.HEAPU8.set(state.flash.subarray(0, Module._galaxia_flash_size()), ptr);
        }
        if (state.log) {
            Module.FS.mkdirTree("/spiffs");
            Module.FS.writeFile("/spiffs/data.csv", state.log);
        }
        await Module.ccall("galaxia_main", null, [], [], { async: true });
    } catch (err) {
        if (err instanceof RangeError || err instanceof WebAssembly.RuntimeError) {
            crashed(err.message);   // native stack overflow, trap: the board panics
        } else {
            postMessage({ type: "error", message: String(err && err.stack || err) });
        }
    }
}

onmessage = (e) => {
    const m = e.data;
    switch (m.type) {
        case "start":
            start(m.state || {});
            return;
        case "in":
            for (const b of m.data) galaxia.stdin.push(b);
            break;
        case "gpio":
            galaxia.inputs.push({ kind: "gpio", id: m.pin, value: m.level, t: m.t ?? performance.now() });
            break;
        case "touch":
            galaxia.inputs.push({ kind: "touch", id: m.pad, value: m.touched, t: m.t ?? performance.now() });
            break;
        case "accel":
            galaxia.accel = [m.x, m.y, m.z];
            Module && Module._galaxia_set_accel(m.x, m.y, m.z);
            return;
        case "compass":
            galaxia.heading = m.heading;
            galaxia.magnetic = null;
            Module && Module._galaxia_set_compass(m.heading, 300, -400);
            return;
        case "magnetic":
            galaxia.magnetic = [m.x, m.y, m.z];
            Module && Module._galaxia_set_magnetic(m.x, m.y, m.z);
            return;
        case "temperature":
            galaxia.temperature = m.value;
            return;
        case "aps":
            galaxia.aps = m.aps || [];
            galaxia.hosts = m.hosts || {};
            return;
        case "light":
            galaxia.light = m.value;
            return;
        case "upload":
            galaxia.upload = { files: m.files, main: m.main, requested: true };
            break;
        case "key": {
            const r = usbKey(m);
            postMessage({ type: "key", id: m.id, ...r }, r.data ? [r.data.buffer] : []);
            break;      // a write may have restarted the program: wake the board
        }
        case "get-state":
            if (Module) {
                const state = boardState();
                postMessage({ type: "state", state }, transfers(state));
            }
            return;
        default:
            return;
    }
    wake();
};
