// The Wi-Fi network between two WebAssembly Galaxias under Node.js (net.c): the
// page's access point, scan, connection, wrong key, addresses, TCP and UDP between
// the boards, mDNS, a board in soft-AP mode, errors. Prints each check; exit 1 if
// one fails.
//   node test/network.mjs [galaxia.mjs]
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const mjs = process.argv[2] || [path.join(here, "../build/galaxia.mjs"), path.join(here, "../../build/wasm/galaxia.mjs")]
    .find((p) => fs.existsSync(p));
const createGalaxia = (await import(mjs)).default;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const aps = [{ ssid: "Classe", password: "galaxia2026", authmode: 3, channel: 6 }];
const boards = [];

// The air: a frame sent by one board reaches all the others
async function board(name, mac) {
    const g = {
        name, out: "",
        stdin: [], gpio: {}, gpioChanged: [], touch: {}, touchChanged: [], light: 128, temperature: 30,
        mac: Uint8Array.from(mac), radioIn: [], lanIn: [], aps,
        led() {}, sound() {}, screen() {}, restart() {},
        radioSend: (p) => boards.filter((b) => b !== g).forEach((b) => { b.radioIn.push(p); wake(b); }),
        lanSend: (f) => boards.filter((b) => b !== g).forEach((b) => { b.lanIn.push(f.slice()); wake(b); }),
        stdout: (b) => { g.out += Buffer.from(b).toString("utf8"); },
    };
    boards.push(g);
    const M = await createGalaxia({ galaxia: g });
    M.galaxia = g;
    M.ccall("galaxia_main", null, [], [], { async: true });
    while (!g.out.includes(">>> ")) await sleep(20);
    return g;
}

function wake(g) {
    const w = g.wake;
    if (w) {
        g.wake = null;
        clearTimeout(g.wakeTimer);
        w();
    }
}

// Runs Python on a board (a line, or a block in paste mode), returns what it printed
// between two markers
async function py(g, code, ms = 15000) {
    const tag = `@${Math.random().toString(36).slice(2, 8)}@`;
    const start = g.out.length;
    const body = `print('${tag}S')\n${code}\nprint('${tag}E')`;
    const src = code.includes("\n") ? `\x05${body}\n\x04` : `${body.replace(/\n/g, "; ")}\r`;
    for (const b of Buffer.from(src)) g.stdin.push(b);
    wake(g);
    const t0 = Date.now();
    for (;;) {
        const lines = g.out.slice(start).split(/\r?\n/);
        const s = lines.indexOf(`${tag}S`), e = lines.indexOf(`${tag}E`);
        if (s >= 0 && e > s) {
            return lines.slice(s + 1, e).join("\n").trim();
        }
        const err = /Traceback[\s\S]*?\n(\w+Error.*)/.exec(g.out.slice(start));
        if (err) return "ERROR " + err[1];
        if (Date.now() - t0 > ms) return "TIMEOUT " + g.out.slice(start).slice(-300);
        await sleep(20);
    }
}

let failures = 0;
function check(name, ok, detail) {
    fs.writeSync(1, `${ok ? "ok  " : "FAIL"} ${name}: ${detail}\n`);
    if (!ok) failures++;
}

const a = await board("A", [2, 0xaa, 0, 0, 0, 1]);
const b = await board("B", [2, 0xbb, 0, 0, 0, 2]);
await py(a, "import network, socket, time");
await py(b, "import network, socket, time");
await py(a, "w = network.WLAN(network.STA_IF)");
await py(b, "w = network.WLAN(network.STA_IF)");

// --- scan: the page's access point
let v = await py(a, "print(w.scan())");
check("scan sees the classroom AP", v.includes("b'Classe'") && v.includes(", 6, ") && v.includes(", 3, False)"), v);

// --- wrong key: one attempt, the handshake times out
await py(a, "w.config(reconnects=0); w.connect('Classe', 'mauvaiscle')");
v = await py(a, "print(w.status()); time.sleep(6); print(w.status(), w.isconnected())");
check("wrong key: connecting, then 4-way handshake timeout (15)", v === "1001\n15 False", JSON.stringify(v));

// --- unknown SSID
await py(a, "w.connect('Inconnu', 'galaxia2026')");
v = await py(a, "time.sleep(2.5); print(w.status())");
check("unknown SSID: STAT_NO_AP_FOUND", v === String(201), v);

// --- connection, addresses (mDNS name set before the first connection)
await py(a, "network.hostname('carte-a'); w.config(reconnects=-1); w.connect('Classe', 'galaxia2026')");
await py(b, "w.connect('Classe', 'galaxia2026')");
v = await py(a, "time.sleep(2.5)\nwhile not w.isconnected(): time.sleep(0.1)\nprint(w.status(), w.ifconfig(), w.config('channel'))");
check("A connected, address on the page's network", /^1010 \('192\.168\.1\.\d+', '255\.255\.255\.0', '192\.168\.1\.1', '192\.168\.1\.1'\) 6$/.test(v), v);
const ipA = v.match(/'(192\.168\.1\.\d+)'/)[1];
v = await py(b, "while not w.isconnected(): time.sleep(0.1)\nprint(w.ifconfig()[0])");
check("B connected, another address", /^192\.168\.1\.\d+$/.test(v) && v !== ipA, `${v} (A: ${ipA})`);
const ipB = v;

// --- TCP: server on A, client on B
await py(a, "srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); srv.bind(('0.0.0.0', 8080)); srv.listen(1)");
await py(b, `c = socket.socket(); c.connect(('${ipA}', 8080)); c.send(b'bonjour A')`);
v = await py(a, "cl, addr = srv.accept(); print(addr[0], cl.recv(100))");
check("TCP: A accepts B and receives", v === `${ipB} b'bonjour A'`, v);
// More than the window: A's send() waits for B to read
const sending = py(a, "print(cl.send(b'salut B' * 1000))");
await sleep(300);
v = await py(b, "n = 0\nwhile n < 7000:\n    n += len(c.recv(2000))\nprint(n)");
check("TCP: 7000 bytes back to B (window 5760)", v === "7000", v);
v = await sending;
check("TCP: A's send() returns once B has read", v === "7000", v);
await py(a, "cl.close()");
v = await py(b, "print(c.recv(10), c.recv(10))");
check("TCP: peer closed, recv() gives b''", v === "b'' b''", v);
await py(b, "c.close()");

// A closes with data still unread by B: B reads it all, then b'' (its window
// updates after A's close must not be answered with RST)
await py(b, `c = socket.socket(); c.connect(('${ipA}', 8080))`);
await py(a, "cl, addr = srv.accept(); _ = cl.send(b'x' * 3000); cl.close()");
v = await py(b, "time.sleep(0.3)\nn = 0\nwhile True:\n    d = c.recv(500)\n    if not d:\n        break\n    n += len(d)\nprint(n)\nc.close()");
check("TCP: data then FIN, read after the peer closed", v === "3000", v);

// --- TCP errors
v = await py(b, `s = socket.socket()\ntry:\n    s.connect(('${ipA}', 9999))\nexcept OSError as e:\n    print(e.errno)\ns.close()`);
check("TCP: closed port, ECONNRESET (104)", v === "104", v);
v = await py(a, "srv.close(); s2 = socket.socket()\ntry:\n    s2.bind(('0.0.0.0', 8080))\nexcept OSError as e:\n    print(e.errno)\ns2.close()");
check("TCP: port in TIME_WAIT without SO_REUSEADDR, EADDRINUSE (112)", v === "112", v);
v = await py(b, "s = socket.socket(); s.settimeout(0.3)\ntry:\n    s.recv(10)\nexcept OSError as e:\n    print(e.errno)\ns.close()");
check("TCP: recv on an unconnected socket, ENOTCONN (128)", v === "128", v);

// --- UDP broadcast
await py(a, "u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); u.bind(('0.0.0.0', 5000)); u.settimeout(3)");
await py(b, "ub = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); ub.sendto(b'hello', ('255.255.255.255', 5000))");
v = await py(a, "d, f = u.recvfrom(100); print(d, f[0])");
check("UDP: broadcast from B received by A", v === `b'hello' ${ipB}`, v);
v = await py(a, "u.settimeout(0.5)\ntry:\n    u.recvfrom(10)\nexcept OSError as e:\n    print(e.errno)");
check("UDP: timeout, ETIMEDOUT (116)", v === "116", v);
await py(a, "u.close()");
await py(b, "ub.close()");

// --- mDNS
v = await py(b, "print(socket.getaddrinfo('carte-a.local', 80)[0][-1])");
check("mDNS: carte-a.local is A", v === `('${ipA}', 80)`, v);
v = await py(b, "try:\n    socket.getaddrinfo('example.com', 80)\nexcept OSError as e:\n    print(e.errno)", 20000);
check("DNS: no Internet behind the AP, -202 (EAI_FAIL)", v === "-202", v);

// --- soft-AP on A, B connects to it
await py(a, "ap = network.WLAN(network.AP_IF); ap.config(essid='Galaxia-A', authmode=network.AUTH_WPA2_PSK, password='12345678'); ap.active(True)");
v = await py(a, "print(ap.active(), ap.ifconfig()[0], ap.config('essid'))");
check("soft-AP on A", v === "True 192.168.4.1 Galaxia-A", v);
v = await py(b, "nets = [n[0] for n in w.scan()]; print(b'Galaxia-A' in nets, b'Classe' in nets)");
check("B's scan sees A's soft-AP", v === "True True", v);
await py(b, "w.disconnect(); w.connect('Galaxia-A', '12345678')");
v = await py(b, "time.sleep(2.5)\nwhile not w.isconnected(): time.sleep(0.1)\nprint(w.ifconfig()[0], w.ifconfig()[2])");
check("B on A's soft-AP: DHCP from A", v === "192.168.4.2 192.168.4.1", v);
v = await py(a, "print(len(ap.status('stations')), ap.isconnected())");
check("A's soft-AP lists B", v === "1 True", v);
await py(a, "srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); srv.bind(('0.0.0.0', 8081)); srv.listen(1)");
await py(b, "c = socket.socket(); c.connect(('192.168.4.1', 8081)); c.send(b'via AP')");
v = await py(a, "cl, addr = srv.accept(); print(addr[0], cl.recv(100)); cl.close(); srv.close()");
check("TCP through A's soft-AP", v === "192.168.4.2 b'via AP'", v);
await py(b, "c.close()");
await py(a, "ap.active(False)");
v = await py(b, "time.sleep(0.5); print(w.isconnected())");
check("A's soft-AP stops: B is disconnected", v === "False", v);

console.log(failures ? `${failures} failure(s)` : "all checks passed");
process.exit(failures ? 1 : 0);
