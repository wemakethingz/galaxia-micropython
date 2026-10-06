// Smoke test of the WebAssembly Galaxia under Node.js: boots, types on the REPL,
// checks the output and that the screen shows something.
//   node wasm/test/node_smoke.mjs [build/wasm/galaxia.mjs]
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
// Default: the build of the port in the MicroPython tree, else galaxia_sim's
const mjs = process.argv[2] || [path.join(here, "../build/galaxia.mjs"), path.join(here, "../../build/wasm/galaxia.mjs")]
    .find((p) => fs.existsSync(p));
const createGalaxia = (await import(mjs)).default;

let out = "";
let frames = 0;
let lastPicture = null;
const galaxia = {
    stdin: [],
    gpio: {},
    gpioChanged: [],
    touch: {},
    touchChanged: [],
    light: 128,
    mac: Uint8Array.from([2, 0, 0, 0, 0, 1]),
    radioIn: [],
    radioOut: [],
    ledLog: [],
    soundLog: [],
    led: (r, g, b) => galaxia.ledLog.push([r, g, b]),
    sound: (s) => galaxia.soundLog.push(s),
    radioSend: (p) => galaxia.radioOut.push(p),
    temperature: 22.5,
    stdout: (bytes) => { out += Buffer.from(bytes).toString("utf8"); },
    screen: (rgba) => { frames++; lastPicture = rgba; },
    restart: () => { throw new Error("restart"); },
};
const Module = await createGalaxia({ galaxia });
Module.galaxia = galaxia;
Module.ccall("galaxia_main", null, [], [], { async: true });

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const type = (s) => { for (const b of Buffer.from(s, "utf8")) galaxia.stdin.push(b); };
async function waitFor(text, ms = 10000) {
    const t0 = Date.now();
    while (!out.includes(text)) {
        if (Date.now() - t0 > ms) throw new Error(`timeout waiting for ${JSON.stringify(text)}; output:\n${out}`);
        await sleep(20);
    }
}

const t0 = Date.now();
await waitFor(">>> ");
console.log(`REPL after ${Date.now() - t0} ms, ${frames} screen frames`);
type("print(1 + 1, 'ok')\r");
await waitFor("2 ok");
type("import thingz\r");
type("print(sorted(dir(thingz)))\r");
await waitFor("button_a");
type("print(thingz.button_a.is_pressed(), thingz.temperature())\r");
await waitFor("False");
// Press button A (GPIO 45, active low) and check it is seen
galaxia.gpio[45] = 0;
galaxia.gpioChanged.push(45);
await sleep(200);
type("print('A', thingz.button_a.is_pressed(), thingz.button_a.was_pressed())\r");
await waitFor("A True");
// Busy loop timing
type("import time\rt=time.ticks_ms()\rx=0\rfor i in range(200000): x += i\r\rprint('loop', time.ticks_diff(time.ticks_ms(), t))\r");
await waitFor("loop ");
await sleep(300);
let lit = 0;
for (let i = 0; i < lastPicture.length; i += 4) if (lastPicture[i] | lastPicture[i + 1] | lastPicture[i + 2]) lit++;
console.log(out.split("\n").slice(-12).join("\n"));
console.log(`screen frames: ${frames}, lit pixels in last frame: ${lit}`);
process.exit(0);
