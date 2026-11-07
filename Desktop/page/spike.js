// Phase 0 spike (doc 77): the bundled page's transport tests. Runs on load, reports each result to the shell
// (spike_report, printed on stdout) and on screen. Every world frame starts with a float64 send time (ms since the
// epoch, the server's performance.timeOrigin + now()) and a uint32 sequence.
"use strict";
const T = window.__TAURI__;
const logEl = document.getElementById("log");
const nowMs = () => performance.timeOrigin + performance.now();
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const LOOP = "ws://127.0.0.1:47811";
let LAN = "ws://192.168.122.1:47811"; // Replaced by the shell's SPIKE_LAN.
const SECURE = "wss://127.0.0.1:47812";

function report(obj) {
  const line = JSON.stringify(obj);
  logEl.textContent += line + "\n";
  if (T) T.core.invoke("spike_report", { line }).catch(() => {});
}

// ---- the canvas: something moving, and a frame counter (is the window drawing at all, and how smoothly?) ----
const canvas = document.getElementById("c");
const g = canvas.getContext("2d");
let frames = 0;
let worst = 0;
let last = performance.now();
function draw(t) {
  frames++;
  worst = Math.max(worst, t - last);
  last = t;
  g.fillStyle = "#2a251e";
  g.fillRect(0, 0, canvas.width, canvas.height);
  for (let i = 0; i < 400; i++) {
    const x = (i * 37 + t * 0.2) % canvas.width;
    const y = 110 + Math.sin(t / 300 + i) * 90;
    g.fillStyle = `hsl(${(i * 9) % 360} 60% 60%)`;
    g.fillRect(x, y, 6, 6);
  }
  requestAnimationFrame(draw);
}
requestAnimationFrame(draw);
function fpsWindow() {
  const f0 = frames;
  const t0 = performance.now();
  worst = 0;
  return () => ({ fps: +((frames - f0) / ((performance.now() - t0) / 1000)).toFixed(1), worstFrameMs: +worst.toFixed(1) });
}

// ---- transports: one shape for all of them ----
// open(url) -> { send(bytes|string), close() }; frames go to handlers.onBinary(Uint8Array), onText(string).
const transports = {
  direct: async (url, h) => {
    const ws = new WebSocket(url);
    ws.binaryType = "arraybuffer";
    await new Promise((ok, fail) => {
      ws.onopen = ok;
      ws.onerror = () => fail(new Error("WebSocket error (blocked, refused or untrusted)"));
      setTimeout(() => fail(new Error("timeout")), 4000);
    });
    ws.onmessage = (e) => (typeof e.data === "string" ? h.onText(e.data) : h.onBinary(new Uint8Array(e.data)));
    return { send: (m) => ws.send(m), close: () => ws.close() };
  },
  loopbackRelay: async (url, h) => {
    const relay = await T.core.invoke("loopback_relay");
    return transports.direct(`${relay}?to=${encodeURIComponent(url)}`, h);
  },
  plugin: async (url, h) => {
    const ws = await T.websocket.connect(url);
    ws.addListener((m) => {
      if (m.type === "Binary") h.onBinary(Uint8Array.from(m.data));
      else if (m.type === "Text") h.onText(m.data);
    });
    return {
      send: (m) => (typeof m === "string" ? ws.send(m) : ws.send(Array.from(m))),
      close: () => ws.disconnect(),
    };
  },
  ipcRelay: async (url, h) => {
    const ch = new T.core.Channel();
    ch.onmessage = (m) => {
      if (m instanceof ArrayBuffer) h.onBinary(new Uint8Array(m));
      else if (ArrayBuffer.isView(m)) h.onBinary(new Uint8Array(m.buffer, m.byteOffset, m.byteLength));
      else if (Array.isArray(m)) h.onBinary(Uint8Array.from(m));
      else if (typeof m === "string") h.onText(m);
    };
    const id = await T.core.invoke("relay_open", { url, onFrame: ch });
    const headers = { "relay-id": String(id) };
    return {
      send: (m) =>
        typeof m === "string"
          ? T.core.invoke("relay_send", { text: m }, { headers })
          : T.core.invoke("relay_send", m, { headers }),
      close: () => T.core.invoke("relay_close", { id }),
    };
  },
};

function stats(xs) {
  if (!xs.length) return null;
  const s = [...xs].sort((a, b) => a - b);
  const q = (p) => +s[Math.min(s.length - 1, Math.floor(p * s.length))].toFixed(2);
  const mean = s.reduce((a, b) => a + b, 0) / s.length;
  return { n: s.length, mean: +mean.toFixed(3), median: q(0.5), p95: q(0.95), p99: q(0.99), max: +s[s.length - 1].toFixed(2) };
}

async function session(kind, url) {
  const waiting = [];
  const texts = [];
  const h = {
    onBinary: (b) => {
      const w = waiting.shift();
      if (w) w(b);
      else if (h.sink) h.sink(b);
    },
    onText: (t) => texts.push(t) && h.textWaiter && h.textWaiter(),
    sink: null,
    textWaiter: null,
  };
  const t0 = performance.now();
  const conn = await transports[kind](url, h);
  const connectMs = +(performance.now() - t0).toFixed(1);
  const nextText = (ms) =>
    new Promise((ok, fail) => {
      if (texts.length) return ok(JSON.parse(texts.shift()));
      h.textWaiter = () => { h.textWaiter = null; ok(JSON.parse(texts.shift())); };
      setTimeout(() => fail(new Error("timeout waiting for the server")), ms);
    });
  return { conn, h, connectMs, waiting, nextText };
}

async function rtt(s, count = 300, size = 450) {
  const times = [];
  const payload = new Uint8Array(size);
  crypto.getRandomValues(payload);
  for (let i = 0; i < count; i++) {
    const t0 = performance.now();
    const got = new Promise((ok, fail) => {
      s.waiting.push(ok);
      setTimeout(() => fail(new Error("echo timeout")), 3000);
    });
    s.conn.send(payload);
    const back = await got;
    if (back.length !== size) throw new Error(`echo size ${back.length}`);
    times.push(performance.now() - t0);
  }
  return stats(times);
}

// The server pushes frames; we measure one-way delay (same machine, same clock) and losses.
async function stream(s, params, seconds) {
  const delays = [];
  let bytes = 0;
  let got = 0;
  let lastSeq = -1;
  let outOfOrder = 0;
  s.h.sink = (b) => {
    const v = new DataView(b.buffer, b.byteOffset, b.byteLength);
    delays.push(nowMs() - v.getFloat64(0, true));
    const seq = v.getUint32(8, true);
    if (seq !== lastSeq + 1) outOfOrder++;
    lastSeq = seq;
    bytes += b.length;
    got++;
  };
  const fps = fpsWindow();
  const t0 = performance.now();
  s.conn.send(JSON.stringify({ cmd: "stream", ...params, seconds }));
  const end = await s.nextText(seconds * 1000 + 20000);
  const wall = (performance.now() - t0) / 1000;
  s.h.sink = null;
  const floor = Math.min(...delays);
  return { sent: end.sent, got, outOfOrder, delayMs: stats(delays), delayAboveMinMs: stats(delays.map((d) => d - floor)), mbPerS: +(bytes / wall / 1e6).toFixed(2), ...fps() };
}

async function flood(s, size, count, timeoutS = 60) {
  let got = 0;
  let bytes = 0;
  let lastDelay = 0;
  s.h.sink = (b) => {
    got++;
    bytes += b.length;
    lastDelay = nowMs() - new DataView(b.buffer, b.byteOffset, b.byteLength).getFloat64(0, true);
  };
  const fps = fpsWindow();
  const t0 = performance.now();
  s.conn.send(JSON.stringify({ cmd: "flood", size, count }));
  let end;
  try {
    end = await s.nextText(timeoutS * 1000);
  } catch (e) {
    s.h.sink = null;
    return { size, count, got, error: String(e.message || e), seconds: +((performance.now() - t0) / 1000).toFixed(1) };
  }
  const wall = (performance.now() - t0) / 1000;
  s.h.sink = null;
  return {
    size,
    count,
    got,
    seconds: +wall.toFixed(2),
    framesPerS: Math.round(got / wall),
    mbPerS: +(bytes / wall / 1e6).toFixed(1),
    lastFrameDelayMs: +lastDelay.toFixed(1),
    serverSendMs: +end.serverMs.toFixed(1),
    ...fps(),
  };
}

async function full(kind, url) {
  const out = { test: "full", kind, url };
  try {
    const s = await session(kind, url);
    out.connectMs = s.connectMs;
    out.rtt450 = await rtt(s, 300, 450);
    out.rtt3k = await rtt(s, 100, 3000);
    // One player's view: 20 motion frames a second plus 5 snapshots of 3 KB (doc 31: five views a second).
    out.game = await stream(s, { size: 450, hz: 25, snapEvery: 5, snapSize: 3000 }, 5);
    // A crowd: 50 players' motion (1,000 frames a second), with a 3 KB snapshot every 50.
    out.crowd = await stream(s, { size: 450, hz: 1000, snapEvery: 50, snapSize: 3000 }, 4);
    out.flood450 = await flood(s, 450, 20000);
    out.flood3k = await flood(s, 3000, 5000);
    out.flood64k = await flood(s, 64 << 10, 500); // A full first snapshot is tens of KB (doc 31).
    out.flood1m = await flood(s, 1 << 20, 64, 120);
    s.conn.close();
  } catch (e) {
    out.error = String(e && e.message ? e.message : e);
  }
  report(out);
}

async function quick(kind, url) {
  const out = { test: "quick", kind, url };
  try {
    const s = await session(kind, url);
    out.connectMs = s.connectMs;
    out.rtt450 = await rtt(s, 100, 450);
    s.conn.close();
  } catch (e) {
    out.error = String(e && e.message ? e.message : e);
  }
  report(out);
}

(async () => {
  let os = "?";
  try {
    const cfg = await T.core.invoke("spike_config");
    LAN = cfg.lan;
    os = cfg.os;
  } catch {}
  report({
    os,
    test: "env",
    origin: location.origin,
    secureContext: window.isSecureContext,
    userAgent: navigator.userAgent,
    tauri: !!T,
    devicePixelRatio: window.devicePixelRatio,
  });
  await sleep(1500);
  const idle = fpsWindow();
  await sleep(3000);
  report({ test: "canvas_idle", ...idle() });

  // (a) the page's own WebSocket: loopback, a plain ws:// world that isn't loopback, and wss://.
  await quick("direct", LOOP);
  await quick("direct", LAN);
  await quick("direct", SECURE);
  // (b) the shell carrying the socket, to plain ws:// and to wss://.
  for (const url of [LAN, SECURE]) {
    await quick("ipcRelay", url);
    await quick("loopbackRelay", url);
    await quick("plugin", url);
  }
  // The measured runs, on loopback so the network doesn't muddy them.
  const only = (new URLSearchParams(location.search).get("only") || "").split(",").filter(Boolean);
  for (const kind of ["direct", "loopbackRelay", "ipcRelay", "plugin"])
    if (!only.length || only.includes(kind)) await full(kind, LOOP);
  report({ test: "summary", done: true });
})();
