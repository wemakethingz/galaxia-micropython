// machine on the WebAssembly Galaxia under Node.js (ports/esp32's machine module over
// machine_hw.c, idf_shim.c, peripherals.c): CPU, Pin (the board's wiring, pulls,
// interrupts), the LED through Pin, PWM, Timer, RTC, I2C, SPI, UART, ADC, DAC,
// TouchPad, NeoPixel, and the resets (machine.reset, watchdog, deep sleep) with
// what the RTC keeps. Prints each check; exit 1 if one fails.
//   node test/machine.mjs [galaxia.mjs]
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const mjs = process.argv[2] || [path.join(here, "../build/galaxia.mjs"), path.join(here, "../../build/wasm/galaxia.mjs")]
    .find((p) => fs.existsSync(p));
const createGalaxia = (await import(mjs)).default;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

let failures = 0;
function check(name, ok, detail) {
    fs.writeSync(1, `${ok ? "ok  " : "FAIL"} ${name}: ${detail}\n`);
    if (!ok) failures++;
}

// One run of the board; a reset starts the next one with what the chip keeps
let g, M, flash = null;
const resets = [];
async function boot(bootInfo = null) {
    g = {
        out: "", stdin: [], gpio: {}, gpioChanged: [], touch: {}, touchChanged: [], light: 128, temperature: 30,
        mac: Uint8Array.from([2, 0x12, 0x34, 0x56, 0x78, 0x9a]), radioIn: [], lanIn: [], aps: [],
        ledLog: [], bits: [], boot: bootInfo,
        led: (r, gr, b) => g.ledLog.push([r, gr, b]),
        sound() {}, screen() {}, radioSend() {}, lanSend() {},
        bitstream: (pin, bytes) => g.bits.push([pin, Array.from(bytes)]),
        stdout: (b) => { g.out += Buffer.from(b).toString("utf8"); },
        reset: (reason, wake, sleepMs) => {
            const boot = { reason, wake, rtcUs: M._galaxia_rtc_now_us() };
            const ptr = M._galaxia_rtc_mem();
            boot.rtcMem = M.HEAPU8.slice(ptr, ptr + M._galaxia_rtc_mem_size());
            const f = M._galaxia_flash_image();
            flash = M.HEAPU8.slice(f, f + M._galaxia_flash_size());
            resets.push({ boot, sleepMs });
        },
    };
    M = await createGalaxia({ galaxia: g });
    M.galaxia = g;
    if (flash) M.HEAPU8.set(flash, M._galaxia_flash_image());
    M.ccall("galaxia_main", null, [], [], { async: true });
    while (!g.out.includes(">>> ")) await sleep(20);
}

function wake() {
    const w = g.wake;
    if (w) {
        g.wake = null;
        clearTimeout(g.wakeTimer);
        w();
    }
}

async function py(code, ms = 15000) {
    const tag = `@${Math.random().toString(36).slice(2, 8)}@`;
    const start = g.out.length;
    for (const b of Buffer.from(`\x05print('${tag}S')\n${code}\nprint('${tag}E')\n\x04`)) g.stdin.push(b);
    wake();
    const t0 = Date.now();
    for (;;) {
        const lines = g.out.slice(start).split(/\r?\n/);
        const s = lines.indexOf(`${tag}S`), e = lines.indexOf(`${tag}E`);
        if (s >= 0 && e > s) return lines.slice(s + 1, e).join("\n").trim();
        const err = /Traceback[\s\S]*?\n(\w+Error.*)/.exec(g.out.slice(start));
        if (err) return "ERROR " + err[1];
        if (Date.now() - t0 > ms) return "TIMEOUT " + g.out.slice(start).slice(-200);
        await sleep(20);
    }
}

// Waits for the board's reset number n + 1 (n: the count before the action), then
// boots the next run
async function nextRun(n, ms = 10000) {
    const t0 = Date.now();
    while (resets.length === n && Date.now() - t0 < ms) await sleep(20);
    if (resets.length === n) return null;
    const r = resets.at(-1);
    if (r.sleepMs > 0) await sleep(r.sleepMs);
    await boot(r.boot);
    return r;
}

await boot();
let v;

// --- CPU, identity, reset cause, clock
v = await py("import machine, time\nprint(machine.freq(), machine.unique_id(), machine.reset_cause() == machine.PWRON_RESET)");
check("freq 160 MHz, unique_id = MAC, power-on reset", v === "160000000 b'\\x02\\x124Vx\\x9a' True", v);
v = await py("machine.freq(80000000)\nprint(machine.freq())\nmachine.freq(160000000)");
check("freq(80 MHz)", v === "80000000", v);
v = await py("print(time.localtime()[:3], time.time() < 60)");
check("RTC clock from 0 at power-on (epoch 2000)", v === "(2000, 1, 1) True", v);
v = await py("from machine import RTC\nr = RTC()\nr.datetime((2026, 10, 7, 2, 14, 30, 0, 0))\nprint(time.localtime()[:5])");
check("RTC().datetime() sets the clock", v === "(2026, 10, 7, 14, 30)", v);

// --- Pin: the board's wiring, pulls, input-only pad, interrupts
v = await py("from machine import Pin\nprint(Pin(45).value(), Pin(43).value())");
check("buttons A/B released: 1 (board pull-ups)", v === "1 1", v);
g.gpio[45] = 0;
g.gpioChanged.push(45);
await sleep(50);
v = await py("print(Pin(45).value())");
check("button A pressed: 0", v === "0", v);
g.gpio[45] = 1;
g.gpioChanged.push(45);
v = await py("print(Pin(4, Pin.IN, Pin.PULL_UP).value(), Pin(5, Pin.IN, Pin.PULL_DOWN).value(), Pin(6, Pin.IN).value())");
check("pulls: up 1, down 0, floating 0", v === "1 0 0", v);
v = await py("p = Pin(7, Pin.OUT)\np.on()\na = p.value()\np.off()\nprint(a, p.value())");
check("output read back", v === "1 0", v);
v = await py("try:\n    Pin(46, Pin.OUT)\nexcept ValueError as e:\n    print(e)");
check("GPIO46 is input only", v === "pin can only be input", v);
v = await py("presses = []\n_ = Pin(43).irq(lambda p: presses.append(p.value()), Pin.IRQ_FALLING)\nprint('armed')");
g.gpio[43] = 0;
g.gpioChanged.push(43);
await sleep(100);
g.gpio[43] = 1;
g.gpioChanged.push(43);
await sleep(100);
v = await py("print(presses)");
check("IRQ_FALLING on button B", v === "[0]", v);
v = await py("hits = []\np8 = Pin(8, Pin.OUT)\n_ = Pin(8).irq(lambda p: hits.append(1), Pin.IRQ_RISING)\np8.on()\ntime.sleep_ms(10)\nprint(hits)");
check("an output change raises the pad's interrupt", v === "[1]", v);

// --- the LED through Pin (common 10 sinks): red at full
await py("led_r = Pin(41, Pin.OUT)\nled_r.on()");
await sleep(50);
check("Pin(41).on(): the LED's red at full", g.ledLog.at(-1)?.[0] === 255, JSON.stringify(g.ledLog.at(-1)));
await py("led_r.off()");

// --- PWM
v = await py("from machine import PWM\np = PWM(Pin(5), freq=1000, duty_u16=16384)\nprint(p.freq(), p.duty_u16())");
check("PWM 1 kHz, duty_u16", v === "1000 16384", v);
// 30 MHz: 1 bit of resolution, computed on the APB clock but set on the XTAL one (40 MHz)
v = await py("try:\n    PWM(Pin(6), freq=30000000)\nexcept ValueError as e:\n    print(e)");
check("PWM: unreachable frequency (the board's LEDC limits)", v === "unreachable frequency 30000000", v);
await py("p.deinit()");

// --- Timer (hardware timers 0-3)
v = await py("from machine import Timer\nn = [0]\nt = Timer(0)\nt.init(period=50, mode=Timer.PERIODIC, callback=lambda t: n.__setitem__(0, n[0] + 1) if False else n.append(1))\ntime.sleep(1)\nt.deinit()\nprint(18 <= len(n) - 1 <= 21)");
check("Timer(0) periodic 50 ms: ~20 in 1 s", v === "True", v);
v = await py("once = []\nt1 = Timer(3, period=100, mode=Timer.ONE_SHOT, callback=lambda t: once.append(1))\ntime.sleep_ms(400)\nprint(once)");
check("Timer(3) one shot", v === "[1]", v);

// --- I2C: the board's bus on SCL 37 / SDA 33; nothing on other pins
v = await py("from machine import I2C, SoftI2C\nprint(I2C(1, scl=Pin(37), sda=Pin(33)).scan(), I2C(1, scl=Pin(1), sda=Pin(2)).scan())");
check("I2C scan: LIS2DH12, LIS2MDL, EEPROM on 37/33; nothing elsewhere", v === "[25, 30, 80] []", v);
v = await py("print(SoftI2C(scl=Pin(1), sda=Pin(2)).scan())");
check("SoftI2C on free pins: nothing", v === "[]", v);

// --- SPI: SPI2 (id 1) is the screen's; SPI3 (id 2) is free
v = await py("from machine import SPI, SoftSPI\ntry:\n    SPI(1)\nexcept OSError as e:\n    print(e)\ns = SPI(2, baudrate=1000000)\nprint(s.read(3))");
check("SPI(1) in use by the screen, SPI(2) reads 0", v === "SPI host already in use\nb'\\x00\\x00\\x00'", v);

// --- UART
v = await py("from machine import UART\nu = UART(1, 9600, tx=Pin(1), rx=Pin(2))\nprint(u.write(b'hello'), u.read(), u.any())\nprint(u)");
check("UART: write, nothing to read", v.startsWith("5 None 0\nUART(1, baudrate=9600,"), v);

// --- ADC, DAC, TouchPad
v = await py("from machine import ADC, DAC, TouchPad\n_ = Pin(3, Pin.IN, Pin.PULL_UP)\nprint(ADC(Pin(3)).read_u16(), ADC(Pin(9), atten=ADC.ATTN_11DB).read_u16() < 1000)");
check("ADC: pulled-up pad full scale, floating near 0", v === "65535 True", v);
v = await py("d = DAC(Pin(17))\nd.write(128)\nprint(ADC(Pin(17)).read() > 3000)");
check("DAC on the speaker pin, read back by ADC2", v === "True", v);
v = await py("print(TouchPad(Pin(8)).read())");
const untouched = Number(v);
g.touch[8] = 1;
g.touchChanged.push(8);
await sleep(50);
v = await py("print(TouchPad(Pin(8)).read())");
check("TouchPad(Pin(8)): the N pad, higher when touched", Number(v) > untouched, `${untouched} -> ${v}`);
g.touch[8] = 0;
g.touchChanged.push(8);

// --- NeoPixel through the RMT bitstream
v = await py("from neopixel import NeoPixel\nnp = NeoPixel(Pin(1), 2)\nnp[0] = (255, 0, 0)\nnp[1] = (0, 0, 16)\nnp.write()\nprint('ok')");
check("NeoPixel: GRB bytes on pin 1", v === "ok" && JSON.stringify(g.bits.at(-1)) === JSON.stringify([1, [0, 255, 0, 0, 0, 16]]), JSON.stringify(g.bits.at(-1)));

// --- resets: machine.reset() keeps the RTC clock and memory
await py("r.memory(b'garde moi')\nprint(time.localtime()[:4])");
let n = resets.length;
await py("machine.reset()", 1000);
let r = await nextRun(n);
v = await py("import machine, time\nfrom machine import RTC\nprint(machine.reset_cause() == machine.HARD_RESET, RTC().memory(), time.localtime()[:4])");
check("machine.reset(): HARD_RESET, RTC memory and clock kept", v === "True b'garde moi' (2026, 10, 7, 14)", v);

// --- the watchdog
n = resets.length;
await py("wdt = machine.WDT(timeout=500)\nprint('fed once')");
r = await nextRun(n, 5000);
v = r ? await py("import machine\nprint(machine.reset_cause() == machine.WDT_RESET)") : "no reset";
check("WDT not fed: the board resets, WDT_RESET", v === "True", v);

// --- deep sleep: off for the time, then a boot from deep sleep
n = resets.length;
await py("import machine\nmachine.deepsleep(300)", 1000);
r = await nextRun(n);
v = await py("import machine\nprint(machine.reset_cause() == machine.DEEPSLEEP_RESET, machine.wake_reason() == machine.TIMER_WAKE)");
check("deepsleep(300): DEEPSLEEP_RESET, TIMER_WAKE", r && r.sleepMs === 300 && v === "True True", `${r && r.sleepMs} ${v}`);

console.log(failures ? `${failures} failure(s)` : "all checks passed");
process.exit(failures ? 1 : 0);
