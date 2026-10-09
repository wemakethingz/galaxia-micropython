// The firmware's system menu (ports/esp32/debug_mode) on the WebAssembly Galaxia: the
// SYSTEME button (GPIO0) opens and closes it, the touch pads and A move in it, a long
// press leaves it, "Redémarrer" restarts the board, and a soft reset stops its task as
// on the board (about 1 s). Inputs go through the timed queue (worker.js), as the
// page's. Prints each check; exit 1 if one fails.
//   node test/system_menu.mjs [galaxia.mjs]
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const mjs = process.argv[2] || path.join(here, "../build/galaxia.mjs");
const createGalaxia = (await import(mjs)).default;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

let failures = 0;
function check(name, ok, detail) {
    fs.writeSync(1, `${ok ? "ok  " : "FAIL"} ${name}: ${detail}\n`);
    if (!ok) failures++;
}

const resets = [];
let frame = null;
const g = {
    out: "", stdin: [], gpio: {}, gpioChanged: [], touch: {}, touchChanged: [], inputs: [], inputApplied: null,
    light: 128, temperature: 30, mac: Uint8Array.from([2, 0, 0, 0, 0, 8]), radioIn: [], lanIn: [], aps: [],
    led() {}, sound() {}, radioSend() {}, lanSend() {},
    screen: (rgba) => { frame = Buffer.from(rgba); },
    stdout: (b) => { g.out += Buffer.from(b).toString("utf8"); },
    reset: (reason) => resets.push(reason),
};
const M = await createGalaxia({ galaxia: g });
M.galaxia = g;
M.ccall("galaxia_main", null, [], [], { async: true });

function wake() {
    const w = g.wake;
    if (w) {
        g.wake = null;
        clearTimeout(g.wakeTimer);
        w();
    }
}
function input(kind, id, value) {
    g.inputs.push({ kind, id, value, t: performance.now() });
    wake();
}
// A click on a button (active low) or a touch pad, then time for the screen
async function press(pin, ms = 120) {
    input("gpio", pin, 0);
    await sleep(ms);
    input("gpio", pin, 1);
    await sleep(500);
}
async function touch(pad) {
    input("touch", pad, 1);
    await sleep(120);
    input("touch", pad, 0);
    await sleep(250);
}
async function until(test, ms = 10000) {
    const t0 = Date.now();
    while (!test()) {
        if (Date.now() - t0 > ms) return false;
        await sleep(20);
    }
    return true;
}
const SYSTEME = 0, A = 45, S = 4;

await until(() => g.out.includes(">>> "));
await sleep(600);
const repl = frame;

await press(SYSTEME);
const menu = frame;
check("SYSTEME opens the menu", !menu.equals(repl), "the screen changed");
await press(SYSTEME);
check("SYSTEME closes it", !frame.equals(menu), "the screen changed back");

await press(SYSTEME);
await press(SYSTEME, 1500);
check("long press leaves it", !frame.equals(menu), "the screen changed back");

// "Redémarrer", the last entry: the board restarts (esp_restart: ESP_RST_SW)
await press(SYSTEME);
for (let i = 0; i < 7; i++) await touch(S);
await press(A);
check("Redémarrer", await until(() => resets.length > 0, 3000), `reset reasons ${JSON.stringify(resets)}`);
check("software reset", resets[0] === 3, `reason ${resets[0]}`);

fs.writeSync(1, failures ? `${failures} check(s) failed\n` : "all checks passed\n");
process.exit(failures ? 1 : 0);
