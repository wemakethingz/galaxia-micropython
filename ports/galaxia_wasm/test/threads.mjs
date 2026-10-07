// _thread on the WebAssembly Galaxia under Node.js (mpthreadport.c): threads that
// sleep side by side, a lock under contention with the GC, an exception in a thread,
// the firmware's supervisor module, Ctrl-C and soft reset with a thread running, the
// Worker's event loop not starved. Prints each check; exit 1 if one fails.
//   node test/threads.mjs [galaxia.mjs]
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const mjs = process.argv[2] || [path.join(here, "../build/galaxia.mjs"), path.join(here, "../../build/wasm/galaxia.mjs")]
    .find((p) => fs.existsSync(p));
const createGalaxia = (await import(mjs)).default;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const g = {
    out: "", stdin: [], gpio: {}, gpioChanged: [], touch: {}, touchChanged: [], light: 128, temperature: 30,
    mac: Uint8Array.from([2, 1, 2, 3, 4, 5]), radioIn: [], lanIn: [], aps: [],
    led() {}, sound() {}, screen() {}, restart() {}, radioSend() {}, lanSend() {},
    stdout: (b) => { g.out += Buffer.from(b).toString("utf8"); },
};
const M = await createGalaxia({ galaxia: g });
M.galaxia = g;
M.ccall("galaxia_main", null, [], [], { async: true });
while (!g.out.includes(">>> ")) await sleep(20);

let failures = 0;
function check(name, ok, detail) {
    fs.writeSync(1, `${ok ? "ok  " : "FAIL"} ${name}: ${detail}\n`);
    if (!ok) failures++;
}
const type = (s) => { for (const b of Buffer.from(s)) g.stdin.push(b); };
// Runs a block in paste mode, returns what it printed between two markers
async function py(code, ms = 15000) {
    const tag = `@${Math.random().toString(36).slice(2, 8)}@`;
    const start = g.out.length;
    type(`\x05print('${tag}S')\n${code}\nprint('${tag}E')\n\x04`);
    const t0 = Date.now();
    for (;;) {
        const lines = g.out.slice(start).split(/\r?\n/);
        const s = lines.indexOf(`${tag}S`), e = lines.indexOf(`${tag}E`);
        if (s >= 0 && e > s) return lines.slice(s + 1, e).join("\n").trim();
        if (Date.now() - t0 > ms) return "TIMEOUT " + g.out.slice(start).slice(-300);
        await sleep(20);
    }
}

let v = await py(`import _thread, time, gc
n = [0, 0]
def worker(i):
    for j in range(20):
        n[i] += 1
        time.sleep_ms(10)
_ = _thread.start_new_thread(worker, (0,))
_ = _thread.start_new_thread(worker, (1,))
t = time.ticks_ms()
while n[0] < 20 or n[1] < 20:
    time.sleep_ms(5)
print(n, time.ticks_diff(time.ticks_ms(), t) < 400)`);
check("two threads sleep side by side (20 x 10 ms each)", v === "[20, 20] True", v);

v = await py(`lock = _thread.allocate_lock()
c = [0]
def busy():
    for j in range(2000):
        with lock:
            c[0] += 1
        l = [bytearray(100) for _ in range(3)]
for _ in range(3):
    _ = _thread.start_new_thread(busy, ())
time.sleep(2)
gc.collect()
print(c[0], gc.mem_free() > 30000)`);
check("lock under contention, GC with the threads' stacks", v === "6000 True", v);

v = await py(`def boom():
    raise ValueError('dans le thread')
_ = _thread.start_new_thread(boom, ())
time.sleep_ms(100)
print('main alive')`);
check("exception in a thread printed, main goes on", v.includes("ValueError: dans le thread") && v.endsWith("main alive"), JSON.stringify(v));

v = await py(`import supervisor
cnt = 0
def cb():
    global cnt
    cnt += 1
supervisor.set_background_user_callback(cb)
time.sleep(1)
print(15 <= cnt <= 21)`);
check("supervisor: background callback every 50 ms", v === "True", v);

// Main busy, a thread in the background: the event loop still turns (the test's
// timers run), Ctrl-C stops the main thread only, soft reset stops the thread
v = await py(`bg_n = [0]
def bg():
    while True:
        bg_n[0] += 1
        time.sleep_ms(20)
_ = _thread.start_new_thread(bg, ())
print('started')`);
type("while True: pass\r\r");
let ticked = false;
setTimeout(() => { ticked = true; }, 300);
await sleep(600);
check("busy main + sleeping thread: the event loop is not starved", ticked, String(ticked));
type("\x03");
await sleep(300);
check("Ctrl-C interrupts the main thread", /KeyboardInterrupt/.test(g.out.slice(-300)), g.out.slice(-80).replace(/\r?\n/g, " | "));
v = await py("a = bg_n[0]\ntime.sleep(0.5)\nprint(bg_n[0] > a)");
check("the background thread still runs", v === "True", v);
type("\x04");
await sleep(1500);
v = await py("print('bg_n' in globals())");
check("soft reset: back to a clean board", v === "False", v);

console.log(failures ? `${failures} failure(s)` : "all checks passed");
process.exit(failures ? 1 : 0);
