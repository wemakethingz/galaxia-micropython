// A virtual MQTT broker "on the Internet", for the boards of the page (net.c).
//
// The page's access point is a router whose Internet only holds this broker: its DNS
// resolves the broker names set in the page (broker.hivemq.com...) to BROKER_IP, a
// public address, so a board reaches it as a real one, through its gateway. Boards
// on another board's soft-AP do not reach it (no Internet there), as on a real one.
//
// The broker speaks the frames of net.c (the same layout: frame_hdr_t, f_ip_t,
// f_tcp_t, packed, little endian) on the boards' BroadcastChannel, keeps a TCP
// connection per client (window and send buffer of lwIP on the other side, FIN, RST)
// and implements MQTT 3.1.1: CONNECT (3.1 and 3.1.1, any credentials, last will),
// PUBLISH QoS 0, 1, 2, retained messages, SUBSCRIBE/UNSUBSCRIBE with + and #, PING,
// keep-alive, DISCONNECT. Sessions are clean (no stored subscriptions).

export const BROKER_IP = [203, 0, 113, 10];
export const MQTT_PORT = 1883;
export const DEFAULT_BROKER_NAMES = ["broker.hivemq.com", "test.mosquitto.org", "broker.emqx.io", "mqtt.eclipseprojects.io"];

const F_IP = 11;
const IPPROTO_TCP = 6;
const SYN = 1, SYNACK = 2, FIN = 4, RST = 8, DATA = 16, WIN = 32;
const TCP_WND = 5760, TCP_MSS = 1440;
const HDR = 19, IP_LEN = 9, TCP_LEN = 7;
const ROUTER_MAC = Uint8Array.from([0x02, 0x47, 0x00, 0x00, 0x00, 0x01]);

// The BSSID net.c gives a page access point: 02:47 and the FNV-1a hash of its SSID
export function pageApBssid(ssid) {
    let h = 2166136261;
    for (const b of new TextEncoder().encode(ssid)) {
        h = Math.imul(h ^ b, 16777619) >>> 0;
    }
    return Uint8Array.from([0x02, 0x47, h & 255, (h >>> 8) & 255, (h >>> 16) & 255, h >>> 24]);
}

const same = (a, b) => a.length === b.length && a.every((v, i) => v === b[i]);
const utf8 = new TextEncoder(), utf8d = new TextDecoder();

function mqttString(s) {
    const b = utf8.encode(s);
    return [b.length >> 8, b.length & 255, ...b];
}

function remainingLength(n) {
    const out = [];
    do {
        let d = n % 128;
        n = Math.floor(n / 128);
        if (n > 0) d |= 128;
        out.push(d);
    } while (n > 0);
    return out;
}

// MQTT topic filter matching (+ one level, # the rest; $ topics only explicitly)
export function topicMatches(filter, topic) {
    const f = filter.split("/"), t = topic.split("/");
    if (topic.startsWith("$") && (f[0] === "+" || f[0] === "#")) return false;
    for (let i = 0; i < f.length; i++) {
        if (f[i] === "#") return true;
        if (i >= t.length) return false;
        if (f[i] !== "+" && f[i] !== t[i]) return false;
    }
    return f.length === t.length;
}

function validFilter(filter) {
    if (!filter.length) return false;
    const f = filter.split("/");
    return f.every((l, i) => (l === "#" ? i === f.length - 1 : l === "+" || !/[+#]/.test(l)));
}

export class Broker {
    // send(frame: Uint8Array): to the boards; aps(): the page's access points;
    // log(event): {type: "connect"|"disconnect"|"publish"|"subscribe", ...}
    constructor({ send, aps, log = () => {} }) {
        this.send = send;
        this.aps = aps;
        this.log = log;
        this.conns = new Map();        // "ip:port" -> connection
        this.retained = new Map();     // topic -> {payload, qos}
        this.timer = null;
    }

    start() {
        this.timer = setInterval(() => this.tick(), 1000);
    }

    stop() {
        clearInterval(this.timer);
        for (const c of [...this.conns.values()]) this.close(c, true);
    }

    clients() {
        return [...this.conns.values()].filter((c) => c.session).map((c) => c.session.clientId);
    }

    // ---- TCP ----

    receive(frame) {
        if (frame.length < HDR + IP_LEN || frame[0] !== F_IP) return;
        const bssid = frame.subarray(13, 19);
        if (!this.aps().some((ap) => ap.ssid && same(pageApBssid(ap.ssid), bssid))) return;
        const dst = frame.subarray(HDR + 4, HDR + 8);
        if (!same(dst, BROKER_IP) || frame[HDR + 8] !== IPPROTO_TCP || frame.length < HDR + IP_LEN + TCP_LEN) return;
        const dv = new DataView(frame.buffer, frame.byteOffset);
        const t = HDR + IP_LEN;
        const sport = dv.getUint16(t, true), dport = dv.getUint16(t + 2, true);
        const flags = frame[t + 4], n = dv.getUint16(t + 5, true);
        const ip = frame.slice(HDR, HDR + 4);
        const mac = frame.slice(1, 7);
        const key = `${ip.join(".")}:${sport}`;
        let c = this.conns.get(key);
        if (flags & SYN) {
            const peer = { ip, port: sport, mac, bssid: bssid.slice(), dport };
            if (dport !== MQTT_PORT) {
                this.tcp(peer, RST, 0);     // the broker's host only listens on 1883
                return;
            }
            if (c) this.close(c, false, true);
            c = { ...peer, wnd: TCP_WND, out: [], rx: [], session: null, closing: false };
            this.conns.set(key, c);
            this.tcp(c, SYNACK, TCP_WND);
            return;
        }
        if (!c) {
            if (!(flags & RST)) this.tcp({ ip, port: sport, mac, bssid, dport }, RST, 0);
            return;
        }
        if (flags & RST) {
            this.close(c, true, true);
            return;
        }
        if (flags & WIN) {
            c.wnd = Math.min(c.wnd + n, TCP_WND);
            this.flush(c);
        }
        if (flags & DATA) {
            const data = frame.subarray(t + TCP_LEN, t + TCP_LEN + n);
            for (const b of data) c.rx.push(b);
            this.tcp(c, WIN, data.length);     // read at once: the window opens again
            this.parse(c);
        }
        if (flags & FIN) {
            // The client closed: without DISCONNECT, its last will goes out
            this.close(c, true, true);
        }
    }

    tcp(c, flags, n, data = new Uint8Array(0)) {
        const f = new Uint8Array(HDR + IP_LEN + TCP_LEN + data.length);
        const dv = new DataView(f.buffer);
        f[0] = F_IP;
        f.set(ROUTER_MAC, 1);
        f.set(c.mac, 7);
        f.set(c.bssid, 13);
        f.set(BROKER_IP, HDR);
        f.set(c.ip, HDR + 4);
        f[HDR + 8] = IPPROTO_TCP;
        const t = HDR + IP_LEN;
        dv.setUint16(t, c.dport ?? MQTT_PORT, true);
        dv.setUint16(t + 2, c.port, true);
        f[t + 4] = flags;
        dv.setUint16(t + 5, n, true);
        f.set(data, t + TCP_LEN);
        this.send(f);
    }

    write(c, bytes) {
        for (const b of bytes) c.out.push(b);
        this.flush(c);
    }

    // As much as the client's window takes, in segments of one MSS
    flush(c) {
        while (c.out.length && c.wnd > 0 && this.conns.get(`${c.ip.join(".")}:${c.port}`) === c) {
            const n = Math.min(TCP_MSS, c.wnd, c.out.length);
            const chunk = Uint8Array.from(c.out.splice(0, n));
            c.wnd -= n;
            this.tcp(c, DATA, n, chunk);
        }
        if (c.closing && !c.out.length) {
            this.tcp(c, FIN, 0);
            this.conns.delete(`${c.ip.join(".")}:${c.port}`);
        }
    }

    // Ends a connection: `abrupt` sends the last will; `gone` when the client
    // already closed or reset it (nothing to send back)
    close(c, abrupt, gone = false) {
        const s = c.session;
        if (s) {
            c.session = null;
            if (abrupt && s.will) this.route(s.will.topic, s.will.payload, s.will.qos, s.will.retain);
            this.log({ type: "disconnect", clientId: s.clientId, clean: !abrupt });
        }
        if (gone) {
            this.conns.delete(`${c.ip.join(".")}:${c.port}`);
        } else {
            c.closing = true;
            this.flush(c);
        }
    }

    tick() {
        const now = Date.now();
        for (const c of [...this.conns.values()]) {
            const s = c.session;
            // Keep-alive: one and a half times the period without a packet
            if (s && s.keepalive && now - s.lastSeen > s.keepalive * 1500) this.close(c, true);
        }
    }

    // ---- MQTT ----

    parse(c) {
        for (;;) {
            if (c.rx.length < 2) return;
            let len = 0, mult = 1, i = 1;
            for (;; i++) {
                if (i >= c.rx.length) return;
                len += (c.rx[i] & 127) * mult;
                mult *= 128;
                if (!(c.rx[i] & 128)) break;
                if (i === 4) {
                    this.close(c, true);    // malformed
                    return;
                }
            }
            if (c.rx.length < i + 1 + len) return;
            const header = c.rx[0];
            const body = Uint8Array.from(c.rx.slice(i + 1, i + 1 + len));
            c.rx.splice(0, i + 1 + len);
            if (c.session) c.session.lastSeen = Date.now();
            this.packet(c, header >> 4, header & 15, body);
            if (!this.conns.has(`${c.ip.join(".")}:${c.port}`) || c.closing) return;
        }
    }

    packet(c, type, flags, b) {
        const s = c.session;
        const str = (o) => [utf8d.decode(b.subarray(o + 2, o + 2 + ((b[o] << 8) | b[o + 1]))), o + 2 + ((b[o] << 8) | b[o + 1])];
        const bin = (o) => [b.slice(o + 2, o + 2 + ((b[o] << 8) | b[o + 1])), o + 2 + ((b[o] << 8) | b[o + 1])];
        if (!s && type !== 1) {
            this.close(c, false);       // first packet must be CONNECT
            return;
        }
        switch (type) {
            case 1: {   // CONNECT
                if (s) {
                    this.close(c, true);    // a second CONNECT is a protocol violation
                    return;
                }
                let [proto, o] = str(0);
                const level = b[o], cflags = b[o + 1], keepalive = (b[o + 2] << 8) | b[o + 3];
                o += 4;
                if (!((proto === "MQTT" && level === 4) || (proto === "MQIsdp" && level === 3))) {
                    this.write(c, [0x20, 2, 0, 1]);     // unacceptable protocol version
                    this.close(c, false);
                    return;
                }
                let clientId;
                [clientId, o] = str(o);
                const clean = !!(cflags & 2);
                if (!clientId && !clean) {
                    this.write(c, [0x20, 2, 0, 2]);     // identifier rejected
                    this.close(c, false);
                    return;
                }
                let will = null;
                if (cflags & 4) {
                    let topic, payload;
                    [topic, o] = str(o);
                    [payload, o] = bin(o);
                    will = { topic, payload, qos: (cflags >> 3) & 3, retain: !!(cflags & 32) };
                }
                // A client that reconnects with the same identifier replaces the old one
                for (const other of this.conns.values()) {
                    if (other !== c && other.session && other.session.clientId === clientId) this.close(other, true);
                }
                c.session = { clientId: clientId || `auto-${Math.random().toString(36).slice(2, 10)}`, keepalive, will,
                    subs: new Map(), lastSeen: Date.now(), nextId: 1, qos2: new Set() };
                this.write(c, [0x20, 2, 0, 0]);
                this.log({ type: "connect", clientId: c.session.clientId, ip: c.ip.join(".") });
                return;
            }
            case 3: {   // PUBLISH
                const qos = (flags >> 1) & 3, retain = !!(flags & 1);
                let [topic, o] = str(0);
                let id = 0;
                if (qos > 0) {
                    id = (b[o] << 8) | b[o + 1];
                    o += 2;
                }
                const payload = b.slice(o);
                if (qos === 3 || /[+#]/.test(topic)) {
                    this.close(c, true);
                    return;
                }
                if (qos === 1) this.write(c, [0x40, 2, id >> 8, id & 255]);
                if (qos === 2) {
                    this.write(c, [0x50, 2, id >> 8, id & 255]);
                    if (s.qos2.has(id)) return;     // duplicate: already routed
                    s.qos2.add(id);
                }
                this.route(topic, payload, qos, retain);
                this.log({ type: "publish", clientId: s.clientId, topic, payload: utf8d.decode(payload), retain });
                return;
            }
            case 6:     // PUBREL
                s.qos2.delete((b[0] << 8) | b[1]);
                this.write(c, [0x70, 2, b[0], b[1]]);
                return;
            case 5:     // PUBREC (our QoS 2 delivery)
                this.write(c, [0x62, 2, b[0], b[1]]);
                return;
            case 4: case 7:     // PUBACK, PUBCOMP
                return;
            case 8: {   // SUBSCRIBE
                const id = (b[0] << 8) | b[1];
                const codes = [], added = [];
                for (let o = 2; o < b.length;) {
                    let filter;
                    [filter, o] = str(o);
                    const qos = b[o++] & 3;
                    if (!validFilter(filter) || qos === 3) {
                        codes.push(0x80);
                        continue;
                    }
                    s.subs.set(filter, qos);
                    codes.push(qos);
                    added.push([filter, qos]);
                }
                this.write(c, [0x90, ...remainingLength(2 + codes.length), id >> 8, id & 255, ...codes]);
                for (const [filter, qos] of added) {
                    this.log({ type: "subscribe", clientId: s.clientId, topic: filter });
                    for (const [topic, m] of this.retained) {
                        if (topicMatches(filter, topic)) this.deliver(c, topic, m.payload, Math.min(qos, m.qos), true);
                    }
                }
                return;
            }
            case 10: {  // UNSUBSCRIBE
                for (let o = 2; o < b.length;) {
                    let filter;
                    [filter, o] = str(o);
                    s.subs.delete(filter);
                }
                this.write(c, [0xb0, 2, b[0], b[1]]);
                return;
            }
            case 12:    // PINGREQ
                this.write(c, [0xd0, 0]);
                return;
            case 14:    // DISCONNECT: no last will
                s.will = null;
                this.close(c, false);
                return;
            default:
                this.close(c, true);
        }
    }

    deliver(c, topic, payload, qos, retain) {
        const s = c.session;
        const id = qos ? s.nextId : 0;
        if (qos) s.nextId = s.nextId === 65535 ? 1 : s.nextId + 1;
        const body = [...mqttString(topic), ...(qos ? [id >> 8, id & 255] : []), ...payload];
        this.write(c, [0x30 | (qos << 1) | (retain ? 1 : 0), ...remainingLength(body.length), ...body]);
    }

    // A message to the subscribers (and kept if retained)
    route(topic, payload, qos, retain) {
        if (typeof payload === "string") payload = utf8.encode(payload);
        if (retain) {
            if (payload.length) this.retained.set(topic, { payload, qos });
            else this.retained.delete(topic);
        }
        for (const c of this.conns.values()) {
            const s = c.session;
            if (!s || c.closing) continue;
            let best = -1;
            for (const [filter, q] of s.subs) {
                if (topicMatches(filter, topic)) best = Math.max(best, q);
            }
            if (best >= 0) this.deliver(c, topic, payload, Math.min(best, qos), false);
        }
    }

    // A message published from the page
    publish(topic, payload, retain = false) {
        this.route(topic, payload, 0, retain);
        this.log({ type: "publish", clientId: "(page)", topic, payload, retain });
    }
}
