// Phase 0 spike (doc 77): a stand-in world for the desktop client's transport tests. Not the game server.
//
//   node spike/ws_server.mjs [--lan 192.168.122.1]
//
// Listens on ws://127.0.0.1:47811, wss://127.0.0.1:47812 (spike/out/server.pem, made by spike/make_certs.sh) and,
// with --lan, ws://<that address>:47811 as well (a plain ws:// world that is not loopback, for the mixed-content
// question). Writes nothing anywhere; stops after --for seconds (default 1800).
//
// What it does with what it gets:
//   - a binary frame: sent straight back (round trips);
//   - text {"cmd":"stream","size":450,"hz":20,"seconds":5,"snapEvery":20,"snapSize":3000}: frames at that rate;
//   - text {"cmd":"flood","size":N,"count":M}: M frames as fast as the socket takes them.
// Every frame it makes starts with a float64 (little-endian) send time in ms since the epoch, then a uint32 sequence.
import { WebSocketServer } from "ws";
import https from "node:https";
import fs from "node:fs";
import path from "node:path";
import crypto from "node:crypto";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);
const opt = (name, fallback) => {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : fallback;
};
const lan = opt("--lan", null);
const lifetime = Number(opt("--for", 1800));
const nowMs = () => performance.timeOrigin + performance.now();

const noise = crypto.randomBytes(8 << 20); // Random, like compressed game data.
function frame(size, seq) {
  const buf = Buffer.allocUnsafe(Math.max(size, 12));
  noise.copy(buf, 12, 0, buf.length - 12);
  buf.writeDoubleLE(nowMs(), 0);
  buf.writeUInt32LE(seq, 8);
  return buf;
}

function attach(wss, label) {
  wss.on("connection", (ws, req) => {
    console.log(`[${label}] connection from ${req.socket.remoteAddress} origin=${req.headers.origin ?? "-"} url=${req.url}`);
    ws.on("message", (data, isBinary) => {
      if (isBinary) {
        ws.send(data, { binary: true });
        return;
      }
      let msg;
      try {
        msg = JSON.parse(data.toString());
      } catch {
        return;
      }
      if (msg.cmd === "stream") stream(ws, msg);
      else if (msg.cmd === "flood") flood(ws, msg);
      else if (msg.cmd === "hello") ws.send(JSON.stringify({ hello: label, origin: req.headers.origin ?? null }));
    });
  });
}

function stream(ws, { size = 450, hz = 20, seconds = 5, snapEvery = 0, snapSize = 3000 }) {
  const total = Math.round(hz * seconds);
  const started = nowMs();
  let seq = 0;
  // Sends in small bursts on a 5 ms timer so high rates keep their spacing.
  const timer = setInterval(() => {
    const due = Math.min(total, Math.floor(((nowMs() - started) / 1000) * hz) + 1);
    while (seq < due && ws.readyState === ws.OPEN) {
      const snap = snapEvery > 0 && seq % snapEvery === snapEvery - 1;
      ws.send(frame(snap ? snapSize : size, seq), { binary: true });
      seq++;
    }
    if (seq >= total || ws.readyState !== ws.OPEN) {
      clearInterval(timer);
      setTimeout(() => ws.readyState === ws.OPEN && ws.send(JSON.stringify({ end: "stream", sent: seq })), 200);
    }
  }, 5);
}

function flood(ws, { size = 450, count = 1000 }) {
  let seq = 0;
  const started = nowMs();
  const pump = () => {
    while (seq < count && ws.readyState === ws.OPEN) {
      ws.send(frame(size, seq++), { binary: true });
      if (ws.bufferedAmount > 4 << 20) {
        setTimeout(pump, 1);
        return;
      }
    }
    if (ws.readyState === ws.OPEN)
      ws.send(JSON.stringify({ end: "flood", sent: seq, serverMs: nowMs() - started }));
  };
  pump();
}

const listening = [];
const plain = new WebSocketServer({ host: "127.0.0.1", port: 47811, maxPayload: 64 << 20 });
attach(plain, "ws-loopback");
listening.push("ws://127.0.0.1:47811");
if (lan) {
  const other = new WebSocketServer({ host: lan, port: 47811, maxPayload: 64 << 20 });
  attach(other, "ws-lan");
  listening.push(`ws://${lan}:47811`);
}
const certFile = path.join(here, "out", "server.pem");
if (fs.existsSync(certFile)) {
  const server = https.createServer({ cert: fs.readFileSync(certFile), key: fs.readFileSync(path.join(here, "out", "server.key")) });
  const secure = new WebSocketServer({ server, maxPayload: 64 << 20 });
  attach(secure, "wss-loopback");
  server.listen(47812, "127.0.0.1");
  listening.push("wss://127.0.0.1:47812");
}
for (const s of [plain]) s.on("error", (e) => { console.error(e.message); process.exit(1); });
console.log(`spike world listening: ${listening.join(" ")} (pid ${process.pid}, stops in ${lifetime} s)`);
setTimeout(() => process.exit(0), lifetime * 1000).unref?.();
setTimeout(() => process.exit(0), lifetime * 1000);
