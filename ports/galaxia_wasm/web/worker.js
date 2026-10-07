// Web Worker running one WebAssembly Galaxia.
//
// Messages from the page:
//   {type: "start", state}                  boot; state = {flash, log, eeprom, mac} kept
//                                           across reboots (FAT image, SPIFFS log, settings
//                                           EEPROM, MAC address)
//   {type: "in", data: Uint8Array}          bytes typed on the REPL
//   {type: "gpio", pin, level}              buttons A (45) / B (43): 0 = pressed
//   {type: "touch", pad, touched}           touch pads N 8, S 4, E 5, W 9
//   {type: "accel", x, y, z}                acceleration, milli-g, board axes
//   {type: "compass", heading}             the heading thingz.compass.heading() reports
//   {type: "magnetic", x, y, z}             raw magnetic field, milligauss, magnetometer axes
//   {type: "temperature", value}            chip temperature, degrees C
//   {type: "light", value}                  light level on the LED, 0-255
//   {type: "aps", aps, hosts}               the page's Wi-Fi access points:
//                                           [{ssid, password, authmode, channel}], and
//                                           the names their router's DNS knows
//   {type: "get-state"}                     answered by {type: "state", state}
// Messages to the page:
//   {type: "ready"}                         booted (main.py or the REPL comes next)
//   {type: "out", data: Uint8Array}         REPL output
//   {type: "screen", data: RGBA, width, height}
//   {type: "led", r, g, b}
//   {type: "sound", sound: {loop, rate, samples} | null}
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
    light: 128,
    temperature: 30,
    mac: new Uint8Array(6),
    accel: [0, 0, -1000],       // lying flat, screen up (board axes, milli-g)
    heading: 0,                 // compass heading, degrees (or a raw field: magnetic)
    magnetic: null,
    radioIn: [],
    wake: null,
    wakeTimer: null,
    ready: () => postMessage({ type: "ready" }),
    stdout: (bytes) => postMessage({ type: "out", data: bytes }, [bytes.buffer]),
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
    boot: null,                 // what the previous run left (bootInfo)
};

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
    postMessage({ type: "reset", state, sleepMs }, transfers(state));
    close();
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
            galaxia.gpio[m.pin] = m.level;
            galaxia.gpioChanged.push(m.pin);
            break;
        case "touch":
            galaxia.touch[m.pad] = m.touched;
            galaxia.touchChanged.push(m.pad);
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
