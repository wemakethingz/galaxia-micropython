// Peripherals of the WebAssembly Galaxia under Node.js: accelerometer, compass, touch
// pads, LED and light level, sound, radio, log. Prints each check; exit 1 if one fails.
//   node wasm/test/peripherals.mjs [galaxia.mjs]
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
// Default: the build of the port in the MicroPython tree, else galaxia_sim's
const mjs = process.argv[2] || [path.join(here, "../build/galaxia.mjs"), path.join(here, "../../build/wasm/galaxia.mjs")]
    .find((p) => fs.existsSync(p));
const createGalaxia = (await import(mjs)).default;

let out = "";
const galaxia = {
    stdin: [], gpio: {}, gpioChanged: [], touch: {}, touchChanged: [], light: 128, temperature: 30,
    mac: Uint8Array.from([2, 0x11, 0x22, 0x33, 0x44, 0x55]),
    radioIn: [], radioOut: [], ledLog: [], soundLog: [],
    led: (r, g, b) => galaxia.ledLog.push([r, g, b]),
    sound: (s) => galaxia.soundLog.push(s),
    radioSend: (p) => galaxia.radioOut.push(p),
    stdout: (b) => { out += Buffer.from(b).toString("utf8"); },
    screen: () => {},
    restart: () => {},
};
const M = await createGalaxia({ galaxia });
M.galaxia = galaxia;
M._galaxia_set_accel(0, 0, -1000);
M.ccall("galaxia_main", null, [], [], { async: true });

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let failures = 0;
function check(name, ok, detail) {
    fs.writeSync(1, `${ok ? "ok  " : "FAIL"} ${name}: ${detail}\n`);
    if (!ok) failures++;
}
// Runs one REPL line, returns what it printed after the marker
async function py(line, ms = 8000) {
    const tag = `@${Math.random().toString(36).slice(2, 8)}@`;
    const start = out.length;
    for (const b of Buffer.from(`print('${tag}', ${line})\r`)) galaxia.stdin.push(b);
    const re = new RegExp(`^${tag} (.*)\\r?$`, "m");
    const t0 = Date.now();
    for (;;) {
        const m = re.exec(out.slice(start));
        if (m) return m[1];
        const err = /Traceback[\s\S]*?\n(\w+Error.*)/.exec(out.slice(start));
        if (err) return "ERROR " + err[1];
        if (Date.now() - t0 > ms) return "TIMEOUT";
        await sleep(10);
    }
}
async function exec(line) {
    await py(`exec(${JSON.stringify(line)})`);
}

while (!out.includes(">>> ")) await sleep(20);
await exec("from thingz import *");

// Accelerometer: board axes in milli-g (flat: z = -1000)
let v = await py("accelerometer.get_x(), accelerometer.get_y(), accelerometer.get_z()");
let a = v.split(" ").map(Number);
check("accelerometer flat", Math.abs(a[0]) < 40 && Math.abs(a[1]) < 40 && Math.abs(a[2] + 1000) < 40, v);
M._galaxia_set_accel(500, -250, -800);
await sleep(100);
v = await py("accelerometer.get_x(), accelerometer.get_y(), accelerometer.get_z()");
a = v.split(" ").map(Number);
check("accelerometer tilted", Math.abs(a[0] - 500) < 40 && Math.abs(a[1] + 250) < 40 && Math.abs(a[2] + 800) < 40, v);

// Compass: heading = atan2(x, y) of the field (tilt compensated: board flat)
M._galaxia_set_accel(0, 0, -1000);
await sleep(100);
M._galaxia_set_compass(0, 300, -400);
await sleep(600);
v = await py("compass.heading()");
check("compass heading 0", Math.abs(Number(v)) < 3 || Math.abs(Number(v) - 360) < 3, v);
M._galaxia_set_compass(90, 300, -400);
await sleep(600);
v = await py("compass.heading()");
check("compass heading 90", Math.abs(Number(v) - 90) < 3, v);

// Compass calibration: the firmware writes it to the EEPROM (hal/eeprom.c), the
// headings set afterwards take it into account. The board turns during the calibration.
const eepromAt = (i) => M.HEAPU8[M._galaxia_eeprom_image() + i];
check("EEPROM: PCB 1.0.9, not calibrated", eepromAt(0) === 1 && eepromAt(1) === 3 && eepromAt(3) === 0xff,
    [0, 1, 3].map(eepromAt).join(","));
for (const b of Buffer.from("compass.calibrate(2, 40); print('CAL', 'done')\r")) galaxia.stdin.push(b);
const t0 = Date.now();
while (!/^CAL done/m.test(out) && Date.now() - t0 < 10000) {
    M._galaxia_set_compass(((Date.now() - t0) / 2000) * 720, 300, -400);
    await sleep(20);
}
check("compass calibrated, stored in the EEPROM", /^CAL done/m.test(out) && eepromAt(3) !== 0xff,
    [3, 4, 5, 6].map(eepromAt).join(","));
M._galaxia_set_compass(200, 300, -400);
await sleep(300);
v = await py("compass.heading()");
check("compass heading 200 after calibration", Math.abs(Number(v) - 200) < 3, v);

// Touch pad N (GPIO 8)
galaxia.touch[8] = 1;
galaxia.touchChanged.push(8);
await sleep(100);
v = await py("touch_n.is_touched(), touch_s.is_touched()");
check("touch N touched", v === "True False", v);
galaxia.touch[8] = 0;
galaxia.touchChanged.push(8);
await sleep(100);
v = await py("touch_n.is_touched(), touch_n.was_touched()");
check("touch N released", v === "False True", v);

// LED colour
await exec("led.set_colors(255, 128, 0)");
await sleep(50);
let last = galaxia.ledLog.at(-1);
check("LED colour", last && last[0] === 255 && Math.abs(last[1] - 128) <= 1 && last[2] === 0, JSON.stringify(last));

// Light level measured through the LED
galaxia.light = 200;
v = await py("led.read_light_level()");
check("light 200", Math.abs(Number(v) - 200) <= 6, v);
galaxia.light = 20;
v = await py("led.read_light_level()");
check("light 20", Math.abs(Number(v) - 20) <= 6, v);
await sleep(50);
last = galaxia.ledLog.at(-1);
check("LED back on after the measurement", last && last[0] === 255, JSON.stringify(last));

// Sound: a 440 Hz tone, then off
await exec("sound.play(True, 440)");
await sleep(50);
let s = galaxia.soundLog.at(-1);
check("tone 440 Hz", s && s.loop && Math.abs(s.rate / s.samples.length - 440) < 1, s ? `${s.rate} / ${s.samples.length}` : "none");
await exec("sound.play(False)");
await sleep(50);
check("tone off", galaxia.soundLog.at(-1) === null, String(galaxia.soundLog.at(-1)));

// Radio: send, and receive a packet from another board on the same channel
v = await py("radio.get_channel()");
const channel = Number(v);
await exec("radio.send('bonjour')");
const sent = galaxia.radioOut.at(-1);
check("radio send", sent && Buffer.from(sent.data).toString() === "bonjour" && sent.channel === channel,
    sent ? `${Buffer.from(sent.data)} on ${sent.channel}` : "none");
galaxia.radioIn.push({ channel, mac: Uint8Array.from([2, 9, 9, 9, 9, 9]), data: Buffer.from("salut") });
await sleep(100);
v = await py("radio.receive()");
check("radio receive", v.includes("salut"), v);

// Log: SPIFFS, copied to the USB key as /data.csv at the next start
await exec("log.delete()");
await exec("log.set_columns(['t', 'v'])");
await exec("log.add([('t', 1), ('v', 2)])");
for (const b of Buffer.from("\x04")) galaxia.stdin.push(b);       // soft reset
await sleep(3000);
while (!out.slice(-200).includes(">>> ")) await sleep(20);
v = await py("open('/data.csv').read().replace('\\n', '|')");
check("log copied to /data.csv", v.includes("1;2"), v);

fs.writeSync(1, failures ? `${failures} failure(s)\n` : "all checks passed\n");
process.exit(failures ? 1 : 0);
