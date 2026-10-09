// The USB key as the computer sees it (usb_key.c): the FAT partition the firmware has
// mounted, read and written block by block with a second FAT driver. What one side
// writes, the other reads; a write by the computer restarts the program, as
// msc_disk.c does. Prints each check; exit 1 if one fails.
//   node test/usb_key.mjs [galaxia.mjs]
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

const g = {
    out: "", stdin: [], gpio: {}, gpioChanged: [], touch: {}, touchChanged: [], light: 128, temperature: 30,
    mac: Uint8Array.from([2, 0, 0, 0, 0, 6]), radioIn: [], lanIn: [], aps: [],
    led() {}, sound() {}, screen() {}, radioSend() {}, lanSend() {}, reset() {},
    stdout: (b) => { g.out += Buffer.from(b).toString("utf8"); },
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
async function until(text, ms = 10000) {
    const t0 = Date.now();
    while (!g.out.includes(text)) {
        if (Date.now() - t0 > ms) return false;
        await sleep(20);
    }
    return true;
}
async function py(line, end = ">>> ") {
    g.out = "";
    for (const b of Buffer.from(line + "\r", "utf8")) g.stdin.push(b);
    wake();
    await until(end);
    return g.out;
}
// The computer's side (as worker.js)
const buf = (n, fn) => { const p = M._malloc(Math.max(1, n)); try { return fn(p); } finally { M._free(p); } };
const key = {
    list: (p) => buf(1 << 16, (b) => {
        const n = M.ccall("galaxia_key_list", "number", ["string", "number", "number"], [p, b, 1 << 16]);
        return n < 0 ? n : M.UTF8ToString(b, n).split("\n").filter(Boolean).map((l) => l.split("\t")[0] + ":" + l.split("\t")[2]).sort();
    }),
    read: (p) => {
        const n = M.ccall("galaxia_key_size", "number", ["string"], [p]);
        return n < 0 ? n : buf(n, (b) => Buffer.from(M.HEAPU8.slice(b, b + M.ccall("galaxia_key_read", "number", ["string", "number", "number"], [p, b, n]))).toString());
    },
    write: (p, text) => { const d = Buffer.from(text); return buf(d.length, (b) => { M.HEAPU8.set(d, b); return M.ccall("galaxia_key_write", "number", ["string", "number", "number"], [p, b, d.length]); }); },
    mkdir: (p) => M.ccall("galaxia_key_mkdir", "number", ["string"], [p]),
    remove: (p) => M.ccall("galaxia_key_remove", "number", ["string"], [p]),
    space: () => buf(16, (b) => { M.ccall("galaxia_key_space", "number", ["number"], [b]); return [M.HEAPF64[b >> 3], M.HEAPF64[(b >> 3) + 1]]; }),
};

await until(">>> ");
const out = await py("import os; print(os.statvfs('/')[0], os.statvfs('/')[2])");
check("4096-byte blocks, as the board's partition", /^4096 /m.test(out), JSON.stringify(out.split("\r\n")[1]));

await py("f = open('du_firmware.txt', 'w'); f.write('bonjour'); f.close()");
check("the computer sees the firmware's files", JSON.stringify(key.list("/")).includes("f:du_firmware.txt"), JSON.stringify(key.list("/")));
check("and reads them", key.read("/du_firmware.txt") === "bonjour", JSON.stringify(key.read("/du_firmware.txt")));

// The computer writes while a program runs: the program restarts (msc_disk.c)
await py("f = open('code.py', 'w'); f.write('import time\\nprint(\\'tourne\\')\\nwhile True: time.sleep(0.1)\\n'); f.close()");
g.out = "";
g.stdin.push(4);
wake();
check("a program runs", await until("tourne"), JSON.stringify(g.out.slice(-60)));
g.out = "";
check("a folder", key.mkdir("/images") === 0, "mkdir");
check("a file in it", key.write("/images/gagne.bmp", "BMDATA") > 0, "write");
check("the program restarted", await until("Redémarrage du programme"), JSON.stringify(g.out.slice(-80)));
await until("tourne");
g.out = "";
g.stdin.push(3);        // the program runs again: Ctrl-C for the REPL
wake();
await until(">>> ");
const seen = await py("import os; print(os.listdir('/images'), open('/images/gagne.bmp').read())");
check("the firmware reads the computer's file", seen.includes("['gagne.bmp'] BMDATA"), JSON.stringify(seen.split("\r\n")[1]));

check("a full folder cannot go", key.remove("/images") < 0, "remove /images");
check("delete the file", key.remove("/images/gagne.bmp") === 0, "remove");
check("then the folder", key.remove("/images") === 0, "remove /images");
// each write of the FAT restarted the program (as on the board): let them pass
let gone = "";
for (let i = 0; i < 5 && !/^False/m.test(gone); i++) {
    await sleep(1000);
    g.stdin.push(3);
    wake();
    await sleep(300);
    gone = await py("import os; print('images' in os.listdir('/'))");
}
check("gone for the firmware too", /^False/m.test(gone), JSON.stringify(gone.slice(-40)));
const [free, total] = key.space();
check("free space", total > 1.9e6 && free > 0 && free < total, `${free} of ${total} bytes`);

fs.writeSync(1, failures ? `${failures} check(s) failed\n` : "all checks passed\n");
process.exit(failures ? 1 : 0);
