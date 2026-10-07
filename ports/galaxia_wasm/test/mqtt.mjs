// MQTT between two WebAssembly Galaxias under Node.js through the virtual broker
// (web/broker.js) behind the page's access point, with the firmware's umqtt.simple:
// DNS of the broker names, publish/subscribe, wildcards, QoS 1, retained message,
// last will, keep-alive, no broker from a soft-AP. Prints each check; exit 1 if
// one fails.
//   node test/mqtt.mjs [galaxia.mjs]
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { Broker, BROKER_IP, DEFAULT_BROKER_NAMES } from "../web/broker.js";

const here = path.dirname(fileURLToPath(import.meta.url));
const mjs = process.argv[2] || [path.join(here, "../build/galaxia.mjs"), path.join(here, "../../build/wasm/galaxia.mjs")]
    .find((p) => fs.existsSync(p));
const createGalaxia = (await import(mjs)).default;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

const aps = [{ ssid: "Classe", password: "galaxia2026", authmode: 3, channel: 6 }];
const hosts = Object.fromEntries(DEFAULT_BROKER_NAMES.map((n) => [n, BROKER_IP]));
const boards = [];
const events = [];
// The broker hears all the frames and answers on the same air
const broker = new Broker({
    send: (f) => boards.forEach((b) => { b.lanIn.push(f.slice()); wake(b); }),
    aps: () => aps,
    log: (e) => events.push(e),
});
broker.start();

// The air: a frame sent by one board reaches all the others
async function board(name, mac) {
    const g = {
        name, out: "",
        stdin: [], gpio: {}, gpioChanged: [], touch: {}, touchChanged: [], light: 128, temperature: 30,
        mac: Uint8Array.from(mac), radioIn: [], lanIn: [], aps, hosts,
        led() {}, sound() {}, screen() {}, restart() {},
        radioSend: (p) => boards.filter((b) => b !== g).forEach((b) => { b.radioIn.push(p); wake(b); }),
        lanSend: (f) => { boards.filter((b) => b !== g).forEach((b) => { b.lanIn.push(f.slice()); wake(b); }); broker.receive(f.slice()); },
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
for (const g of [a, b]) {
    await py(g, "import network, socket, time\nfrom umqtt.simple import MQTTClient\nw = network.WLAN(network.STA_IF)\nw.connect('Classe', 'galaxia2026')\nwhile not w.isconnected():\n    time.sleep(0.1)");
}

let v = await py(a, "print(socket.getaddrinfo('broker.hivemq.com', 1883)[0][-1])");
check("DNS: broker.hivemq.com is the virtual broker", v === "('203.0.113.10', 1883)", v);

// A subscribes (with a wildcard), B publishes
v = await py(a, `msgs = []
def cb(t, m):
    msgs.append((t, m))
ca = MQTTClient('carte-a', 'broker.hivemq.com', keepalive=30)
ca.set_callback(cb)
print(ca.connect())
ca.subscribe('classe/+/temp')
print('ok')`);
check("A connects (no session present) and subscribes", v === "0\nok", v);
v = await py(b, `cb_ = MQTTClient('carte-b', 'test.mosquitto.org')
_ = cb_.connect()
cb_.publish('classe/b/temp', '21.5')
cb_.publish('classe/b/hum', '40')
cb_.publish('classe/b/temp', '22', qos=1)
print('sent')`);
check("B publishes (QoS 0 and 1)", v === "sent", v);
v = await py(a, "t = time.ticks_ms()\nwhile len(msgs) < 2 and time.ticks_diff(time.ticks_ms(), t) < 3000:\n    _ = ca.check_msg()\n    time.sleep_ms(20)\nprint(msgs)");
check("A receives what matches classe/+/temp", v === "[(b'classe/b/temp', b'21.5'), (b'classe/b/temp', b'22')]", v);

// Retained message: a new subscriber gets it at once
await py(b, "cb_.publish('classe/etat', 'ouvert', retain=True)");
v = await py(a, "msgs.clear()\nca.subscribe('classe/etat')\n_ = ca.wait_msg()\nprint(msgs)");
check("retained message delivered on subscribe", v === "[(b'classe/etat', b'ouvert')]", v);

// Ping
v = await py(a, "ca.ping()\n_ = ca.wait_msg()\nprint('pong')");
check("PINGREQ / PINGRESP", v === "pong", v);

// Last will: B goes away without DISCONNECT
v = await py(b, `cb_.disconnect()
cw = MQTTClient('carte-b2', 'broker.emqx.io')
cw.set_last_will('classe/b/etat', 'perdue')
_ = cw.connect()
print('ok')`);
await py(a, "msgs.clear()\nca.subscribe('classe/b/etat')");
await py(b, "cw.sock.close()");
v = await py(a, "_ = ca.wait_msg()\nprint(msgs)");
check("last will published when B is lost", v === "[(b'classe/b/etat', b'perdue')]", v);
check("broker log", events.some((e) => e.type === "disconnect" && e.clientId === "carte-b2" && !e.clean)
    && events.some((e) => e.type === "connect" && e.clientId === "carte-a"), JSON.stringify(events.slice(-3)));

// From the page
await sleep(100);
broker.publish("classe/b/temp", "19");
v = await py(a, "msgs.clear()\n_ = ca.wait_msg()\nprint(msgs)");
check("message published from the page", v === "[(b'classe/b/temp', b'19')]", v);

// Another port of the broker's host: refused
v = await py(b, "s = socket.socket()\ntry:\n    s.connect(('203.0.113.10', 8883))\nexcept OSError as e:\n    print(e.errno)\ns.close()");
check("port 8883 (TLS) refused: ECONNRESET", v === "104", v);

// On a soft-AP: no Internet, no broker
await py(a, "ca.disconnect()\nap = network.WLAN(network.AP_IF)\nap.config(essid='Galaxia-A', authmode=network.AUTH_WPA2_PSK, password='12345678')\nap.active(True)");
await py(b, "w.disconnect()\nw.connect('Galaxia-A', '12345678')\nwhile not w.isconnected():\n    time.sleep(0.1)");
v = await py(b, "try:\n    socket.getaddrinfo('broker.hivemq.com', 1883)\nexcept OSError as e:\n    print(e.errno)", 20000);
check("from a soft-AP the broker name does not resolve (-202)", v === "-202", v);

broker.stop();
console.log(failures ? `${failures} failure(s)` : "all checks passed");
process.exit(failures ? 1 : 0);
