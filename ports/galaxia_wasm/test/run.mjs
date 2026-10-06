// Runs a REPL session on the WebAssembly Galaxia under Node.js and prints its output.
//   node wasm/test/run.mjs GALAXIA.mjs SESSION_FILE END_REGEX [TIMEOUT_S]
// SESSION_FILE: the bytes typed on the REPL ("\n" are sent as "\r"; use Ctrl-E ... Ctrl-D
// paste mode for blocks). Exits 0 once END_REGEX matches the output, 1 on timeout.
import fs from "node:fs";

const [mjs, sessionFile, endRe, timeoutS] = process.argv.slice(2);
const createGalaxia = (await import(mjs)).default;
const session = fs.readFileSync(sessionFile, "utf8").replace(/\n/g, "\r");
const end = new RegExp(endRe, "m");
let out = "";
const galaxia = {
    stdin: [...Buffer.from(session, "utf8")],
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
    temperature: 22,
    stdout: (b) => {
        const s = Buffer.from(b).toString("utf8");
        out += s;
        fs.writeSync(1, s);
        if (end.test(out)) {
            process.exit(0);
        }
    },
    screen: () => {},
    restart: () => {},
};
setTimeout(() => {
    fs.writeSync(2, "\n[timeout]\n");
    process.exit(1);
}, (Number(timeoutS) || 30) * 1000);
const M = await createGalaxia({ galaxia });
M.galaxia = galaxia;
M.ccall("galaxia_main", null, [], [], { async: true });
