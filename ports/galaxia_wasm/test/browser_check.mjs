// End-to-end check of the WebAssembly Galaxia page in headless Chrome (DevTools
// protocol, no dependency): boot, the example program (tilt, LED, touch), the radio
// the Wi-Fi (TCP) and MQTT between two tabs, files and EEPROM kept across a reload, a firmware crash and reboot.
//   node wasm/test/browser_check.mjs URL OUT_DIR
import { spawn } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";

const url = process.argv[2] || "http://127.0.0.1:8790/";
const outDir = process.argv[3] || ".";
const port = 9333;
const profile = fs.mkdtempSync(path.join(os.tmpdir(), "galaxia-chrome-"));
const chrome = spawn("google-chrome-stable", [
    "--headless=new", `--remote-debugging-port=${port}`, `--user-data-dir=${profile}`,
    "--no-first-run", "--window-size=1200,1000", "--autoplay-policy=no-user-gesture-required", "about:blank",
], { stdio: "ignore" });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let failures = 0;
function check(name, ok, detail = "") {
    console.log(`${ok ? "ok  " : "FAIL"} ${name}${detail ? ": " + detail : ""}`);
    if (!ok) failures++;
}

async function devtools(pathname, method = "GET") {
    for (let i = 0; i < 50; i++) {
        try {
            return await (await fetch(`http://127.0.0.1:${port}${pathname}`, { method })).json();
        } catch {
            await sleep(200);
        }
    }
    throw new Error("Chrome did not start");
}

// One tab driven through its DevTools socket
async function openTab(pageUrl) {
    const target = (await devtools("/json/list")).find((t) => t.type === "page" && t.url === "about:blank")
        || await devtools("/json/new?about:blank", "PUT");
    const ws = new WebSocket(target.webSocketDebuggerUrl);
    await new Promise((r) => ws.addEventListener("open", r));
    let id = 0;
    const pending = new Map();
    ws.addEventListener("message", (e) => {
        const m = JSON.parse(e.data);
        if (m.id && pending.has(m.id)) {
            pending.get(m.id)(m);
            pending.delete(m.id);
        }
    });
    // A command that gets no answer in 20 s fails instead of hanging the test
    const send = (method, params = {}) => new Promise((r) => {
        const i = ++id;
        pending.set(i, r);
        ws.send(JSON.stringify({ id: i, method, params }));
        setTimeout(() => {
            if (pending.delete(i)) {
                console.log(`(no answer to ${method} ${JSON.stringify(params).slice(0, 80)})`);
                r({});
            }
        }, 20000);
    });
    const tab = {
        send,
        evaluate: async (expr) => {
            const r = await send("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true });
            return r.result?.result?.value;
        },
        // Headless Chrome may never answer a screenshot of a background tab: bring it
        // to the front first, and do without the picture if it still does not come
        shot: async (name) => {
            await send("Page.bringToFront");
            const r = await send("Page.captureScreenshot", { format: "png" });
            if (r.result?.data) {
                fs.writeFileSync(path.join(outDir, name), Buffer.from(r.result.data, "base64"));
            }
        },
        term: () => tab.evaluate("galaxiaTerm()"),
        waitRunning: async () => {
            for (let i = 0; i < 150; i++) {
                const s = await tab.evaluate("document.getElementById('status').textContent");
                if (s && s.startsWith("en marche")) return s;
                await sleep(100);
            }
            return "not running";
        },
        waitTerm: async (re, ms = 8000) => {
            const t0 = Date.now();
            while (Date.now() - t0 < ms) {
                if (re.test(await tab.term())) return true;
                await sleep(100);
            }
            return false;
        },
        typed: (text) => tab.evaluate(`galaxiaSend(${JSON.stringify(text)})`),
        pointer: (selector, type) => tab.evaluate(`document.querySelector('${selector}').dispatchEvent(new PointerEvent('${type}', {bubbles: true, pointerId: 1}))`),
        press: async (selector, ms = 150) => {
            await tab.pointer(selector, "pointerdown");
            await sleep(ms);
            await tab.pointer(selector, "pointerup");
        },
        // Mean colour of the screen canvas in a rectangle of the 160 x 128 picture
        screenPixels: (x, y, w, h) => tab.evaluate(`(() => {
            const d = document.getElementById('screen').getContext('2d').getImageData(${x}, ${y}, ${w}, ${h}).data;
            let r = 0, g = 0, b = 0;
            for (let i = 0; i < d.length; i += 4) { r += d[i]; g += d[i + 1]; b += d[i + 2]; }
            const n = d.length / 4;
            return [Math.round(r / n), Math.round(g / n), Math.round(b / n)];
        })()`),
    };
    await send("Runtime.enable");
    await send("Page.enable");
    await send("Page.navigate", { url: pageUrl });
    return tab;
}

// --- tab 1: boot, the example program
const t1 = await openTab(url);
const t0 = Date.now();
check("boot", (await t1.waitRunning()).startsWith("en marche"), `${Date.now() - t0} ms`);
await t1.evaluate("document.getElementById('run').click()");
await sleep(3500);
let px = await t1.screenPixels(70, 50, 20, 20);
check("program: green square in the middle", px[1] > 200 && px[0] < 50, JSON.stringify(px));

// Tilt to the right: the square follows the accelerometer
await t1.evaluate("document.querySelector('[data-tilt=\"45,0\"]').click()");
await sleep(600);
px = await t1.screenPixels(115, 50, 20, 20);
check("tilt right: square moved right", px[1] > 200, JSON.stringify(px));
await t1.shot("1-tilted.png");

// Button A: LED blue (and a 440 Hz tone)
await t1.press('.hwbtn[data-pin="45"]');
await sleep(400);
const led = await t1.evaluate("document.getElementById('led').style.background");
// (set_colors(0, 0, 255) is a 50 % duty on the board, shown 186 by the page)
check("button A: LED blue", /rgb\(0, 0, 186\)/.test(led), led);

// Touch pad N: prints the compass heading; set the heading to 120 first
await t1.evaluate("(() => { const s = document.getElementById('s-head'); s.value = 120; s.dispatchEvent(new Event('input')); })()");
await sleep(1200);
await t1.press('.tpad[data-pad="8"]', 200);
check("touch N: heading printed", await t1.waitTerm(/cap : 1(19|20|21)\b/), (await t1.term()).split("\n").slice(-2).join(" | "));
await t1.shot("2-led-touch.png");

// --- radio between two tabs (two boards)
const t2 = await openTab(url);
check("second board boots", (await t2.waitRunning()).startsWith("en marche"));
await sleep(2500);
for (const t of [t1, t2]) {
    await t.typed("\x03\x03");
    await sleep(300);
    await t.typed("from thingz import *\r");
}
await sleep(500);
await t2.typed("radio.send('ping depuis 2')\r");
await sleep(500);
await t1.typed("print('RX', radio.receive())\r");
check("radio: tab 1 receives tab 2's message", await t1.waitTerm(/RX \[b'.*', 'ping depuis 2'\]/), (await t1.term()).split("\n").slice(-2).join(" | "));

// --- Wi-Fi between the two tabs: both on the page's access point, TCP from 2 to 1
for (const t of [t1, t2]) {
    await t.typed("import network, socket, time\rw = network.WLAN(network.STA_IF); w.connect('Classe', 'galaxia2026')\r");
}
await t1.typed("while not w.isconnected(): time.sleep(0.1)\r\rprint('IP', w.ifconfig()[0])\r");
check("Wi-Fi: tab 1 connected", await t1.waitTerm(/^IP 192\.168\.1\.\d+/m, 10000), (await t1.term()).split("\n").slice(-2).join(" | "));
const ip1 = /^IP (192\.168\.1\.\d+)/m.exec(await t1.term())[1];
await t1.typed("srv = socket.socket(); srv.bind(('', 8080)); srv.listen(1)\r");
await t2.typed("while not w.isconnected(): time.sleep(0.1)\r\r");
await sleep(3000);
await t2.typed(`c = socket.socket(); c.connect(('${ip1}', 8080)); c.send(b'bonjour onglet 1')\r`);
await t1.typed("cl, a = srv.accept(); print('TCP', cl.recv(100)); cl.close(); srv.close()\r");
check("Wi-Fi: TCP from tab 2 to tab 1", await t1.waitTerm(/TCP b'bonjour onglet 1'/, 10000), (await t1.term()).split("\n").slice(-2).join(" | "));

// --- MQTT through the virtual broker hosted by one of the tabs
await t1.typed("from umqtt.simple import MQTTClient\rm = MQTTClient('onglet1', 'broker.hivemq.com'); _ = m.connect(); m.set_callback(lambda t, v: print('MQTT', t, v)); m.subscribe('classe/test')\r");
await sleep(1500);
await t2.typed("from umqtt.simple import MQTTClient\rm = MQTTClient('onglet2', 'test.mosquitto.org'); _ = m.connect(); m.publish('classe/test', 'bonjour MQTT')\r");
await sleep(1500);
await t1.typed("_ = m.wait_msg()\r");
check("MQTT: tab 1 receives tab 2's message through the broker", await t1.waitTerm(/MQTT b'classe\/test' b'bonjour MQTT'/, 10000), (await t1.term()).split("\n").slice(-2).join(" | "));
const mqLog = await t1.evaluate("document.getElementById('mq-log').textContent");
check("MQTT: the page's broker log", /onglet2 : classe\/test = bonjour MQTT/.test(mqLog), mqLog.slice(-120));

// --- files kept: reload tab 1, main.py runs again
await t1.evaluate("document.getElementById('reboot').click()");
await sleep(500);
await t1.send("Page.reload");
check("reload", (await t1.waitRunning()).startsWith("en marche"));
const eeprom = await t1.evaluate(`new Promise((resolve) => {
    const req = indexedDB.open("galaxia-wasm", 1);
    req.onsuccess = () => {
        const get = req.result.transaction("board").objectStore("board").get("state");
        get.onsuccess = () => resolve(get.result && get.result.eeprom ? Array.from(get.result.eeprom.slice(0, 3)) : null);
    };
})`);
check("EEPROM saved with the files (PCB 1.0.9)", eeprom && eeprom[0] === 1 && eeprom[1] === 3, JSON.stringify(eeprom));
await sleep(3500);
px = await t1.screenPixels(0, 40, 160, 40);
check("after reload: main.py runs again (green square)", px[1] > 10, JSON.stringify(px));
await t1.shot("3-after-reload.png");

// --- machine.reset(): the board resets, the RTC memory is kept
await t1.typed("\x03\x03import machine\rmachine.RTC().memory(b'apres reset')\rmachine.reset()\r");
check("machine.reset(): the board restarts", (await t1.waitRunning()).startsWith("en marche"));
await sleep(3500);
await t1.typed("\x03\x03import machine\rprint('RC', machine.reset_cause() == machine.HARD_RESET, machine.RTC().memory())\r");
check("after machine.reset(): HARD_RESET, RTC memory kept", await t1.waitTerm(/RC True b'apres reset'/), (await t1.term()).split("\n").slice(-2).join(" | "));

// --- deep sleep: the page shows the board off, then it boots again
await t1.typed("machine.deepsleep(1500)\r");
await sleep(500);
const sleepStatus = await t1.evaluate("document.getElementById('status').textContent");
check("deepsleep: the board is off", sleepStatus === "en sommeil profond", sleepStatus);
check("deepsleep: it wakes on its timer", (await t1.waitRunning()).startsWith("en marche"));
await sleep(3500);
await t1.typed("\x03\x03import machine\rprint('WK', machine.reset_cause() == machine.DEEPSLEEP_RESET)\r");
check("after deepsleep: DEEPSLEEP_RESET", await t1.waitTerm(/WK True/), (await t1.term()).split("\n").slice(-2).join(" | "));

// --- crash: infinite recursion, the board reboots with its files
await t1.typed("\x03\x03\x05def r(n):\r    return r(n + 1)\rr(0)\r\x04");
check("crash reported", await t1.waitTerm(/plantage du firmware/, 10000));
await sleep(4000);
px = await t1.screenPixels(0, 40, 160, 40);
check("after the crash: main.py runs again", px[1] > 10, JSON.stringify(px));

console.log(failures ? `${failures} failure(s)` : "all checks passed");
chrome.kill();
process.exit(failures ? 1 : 0);
