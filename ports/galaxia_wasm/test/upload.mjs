// Upload of a program by the page (worker.js {type: "upload"}): the files go straight
// into the flash at the soft reset the upload asks for, as
// microcontroller.set_file_to_execute() does: from the REPL, from a program sleeping,
// from a program that never sleeps. Prints each check; exit 1 if one fails.
//   node test/upload.mjs [galaxia.mjs]
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

const uploads = [];
const g = {
    out: "", stdin: [], gpio: {}, gpioChanged: [], touch: {}, touchChanged: [], light: 128, temperature: 30,
    mac: Uint8Array.from([2, 0, 0, 0, 0, 7]), radioIn: [], lanIn: [], aps: [],
    led() {}, sound() {}, screen() {}, radioSend() {}, lanSend() {}, reset() {},
    stdout: (b) => { g.out += Buffer.from(b).toString("utf8"); },
    uploaded: (ok) => uploads.push(ok),
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
async function until(text, ms = 15000) {
    const t0 = Date.now();
    while (!g.out.includes(text)) {
        if (Date.now() - t0 > ms) return false;
        await sleep(20);
    }
    return true;
}
function type(s) {
    for (const b of Buffer.from(s.replace(/\n/g, "\r"), "utf8")) g.stdin.push(b);
    wake();
}
function upload(main, files) {
    g.out = "";
    g.upload = {
        files: files.map(([p, text]) => ({ path: p, data: new TextEncoder().encode(text) })),
        main, requested: true,
    };
    wake();
}

await until(">>> ");

// From the REPL: a library in /lib and the program using it
upload("code.py", [
    ["code.py", "import salut\nprint('A' + salut.mot())\n"],
    ["/lib/salut/__init__.py", "def mot():\n    return 'BC'\n"],
]);
check("from the REPL", await until("ABC"), JSON.stringify(g.out.slice(-60)));
check("uploaded reported", uploads.length === 1 && uploads[0] === true, JSON.stringify(uploads));
await until(">>> ");
type("import os; print(os.listdir('/lib/salut'), open('code.py').read().count('salut'))\n");
check("files in the flash", await until("['__init__.py'] 2"), JSON.stringify(g.out.slice(-80)));

// While a program sleeps
upload("code.py", [["code.py", "import time\nwhile True:\n    print('tic')\n    time.sleep(0.2)\n"]]);
await until("tic");
upload("code.py", [["code.py", "print('D' + 'EF')\n"]]);
check("from a sleeping program", await until("DEF"), JSON.stringify(g.out.slice(-80)));

// While a program never sleeps
upload("code.py", [["code.py", "n = 0\nprint('busy')\nwhile True:\n    n += 1\n"]]);
await until("busy");
await sleep(300);
upload("main.py", [["main.py", "print('G' + 'HI')\n"]]);
check("from a busy program", await until("GHI"), JSON.stringify(g.out.slice(-80)));

// The file to execute stays across soft resets (NVS)
g.out = "";
type("\x04");
check("main kept", await until("GHI"), JSON.stringify(g.out.slice(-80)));

fs.writeSync(1, failures ? `${failures} check(s) failed\n` : "all checks passed\n");
process.exit(failures ? 1 : 0);
