// A _thread sleeping in a loop while the main thread computes, with an image on the
// raw screen: the screen's refresh timer (which reads the image file) must not run on
// the thread's stack, where it ended up calling garbage ("'str' object isn't
// callable") when the system menu's task ran in the middle of it. The esp_timer
// callbacks run from the main thread (idf_shim.c, run_timers). Exit 1 on failure.
//   node test/thread_timers.mjs [galaxia.mjs]
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const mjs = process.argv[2] || path.join(here, "../build/galaxia.mjs");
const createGalaxia = (await import(mjs)).default;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// A 20 x 20 BMP, 8 bits indexed (as the sprites library's icons): a white disc
function bmp() {
    const w = 20, h = 20, palette = 256 * 4, offset = 54 + palette;
    const b = Buffer.alloc(offset + w * h);
    b.write("BM", 0);
    b.writeUInt32LE(b.length, 2);
    b.writeUInt32LE(offset, 10);
    b.writeUInt32LE(40, 14);
    b.writeInt32LE(w, 18);
    b.writeInt32LE(h, 22);
    b.writeUInt16LE(1, 26);
    b.writeUInt16LE(8, 28);
    b.writeUInt32LE(w * h, 34);
    b.writeUInt32LE(256, 46);
    b.writeUInt32LE(0xffffff, 54 + 4);              // colour 1: white
    for (let y = 0; y < h; y++) {
        for (let x = 0; x < w; x++) {
            b[offset + y * w + x] = (x - 9.5) ** 2 + (y - 9.5) ** 2 < 80 ? 1 : 0;
        }
    }
    return b;
}

const code = `import thingz, time, _thread
thingz.display.raw.show()
n = [0]
def w():
    while True:
        time.sleep_ms(50)
        n[0] += 1
_thread.start_new_thread(w, ())
img = thingz.display.raw.Img(50, 90, '/circle.bmp', 0xffffff)
img.show(True)
t0 = time.ticks_ms()
while time.ticks_diff(time.ticks_ms(), t0) < 6000:
    pass
print('DONE', n[0])
`;

const g = {
    out: "", stdin: [], gpio: {}, gpioChanged: [], touch: {}, touchChanged: [], light: 128, temperature: 30,
    mac: Uint8Array.from([2, 0, 0, 0, 0, 3]), radioIn: [], lanIn: [], aps: [], frames: 0,
    led() {}, sound() {}, radioSend() {}, lanSend() {}, reset() {}, uploaded() {},
    screen: () => { g.frames++; },
    stdout: (b) => { g.out += Buffer.from(b).toString("utf8"); },
};
const M = await createGalaxia({ galaxia: g });
M.galaxia = g;
M.ccall("galaxia_main", null, [], [], { async: true });
const wake = () => { const w = g.wake; if (w) { g.wake = null; clearTimeout(g.wakeTimer); w(); } };
while (!g.out.includes(">>> ")) await sleep(50);
g.out = "";
g.upload = { files: [{ path: "/circle.bmp", data: bmp() }, { path: "code.py", data: Buffer.from(code) }], main: "code.py", requested: true };
wake();
const t0 = Date.now();
while (!/DONE \d+/.test(g.out) && Date.now() - t0 < 30000) await sleep(100);
g.frames = 0;
await sleep(500);
const done = /DONE (\d+)/.exec(g.out);
const failed = /Traceback|isn't callable|Unhandled exception/.test(g.out);
let failures = 0;
function check(name, ok, detail) {
    fs.writeSync(1, `${ok ? "ok  " : "FAIL"} ${name}: ${detail}\n`);
    if (!ok) failures++;
}
check("the thread ran", done && Number(done[1]) > 50, done ? `${done[1]} sleeps` : JSON.stringify(g.out.slice(-200)));
check("no error", !failed, failed ? JSON.stringify(g.out.slice(g.out.search(/Traceback|Unhandled|isn't/)).slice(0, 200)) : "none");
fs.writeSync(1, failures ? `${failures} check(s) failed\n` : "all checks passed\n");
process.exit(failures ? 1 : 0);
