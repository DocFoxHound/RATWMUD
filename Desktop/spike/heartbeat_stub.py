#!/usr/bin/env python3
"""Phase 0 spike (doc 77, sections 5.1 and 7): the world agent's heartbeat against a stub central.

Standard library only. Three roles:
  heartbeat_stub.py central --port P --keys KEYS.json [--expire S]   a stub central: /worlds/heartbeat, /worlds, /keys
  heartbeat_stub.py agent --central URL --token T --dir DIR [--every S] [--once]
                                                                      a world agent: heartbeat + /keys into DIR
  heartbeat_stub.py selftest                                          both, with short times, checked

The agent is the shape doc 77 recommends: outside the C++ server, it posts the listing and the online wolves, keeps
central's public keys in a file the server would read (written atomically), and leaves central's ban list and deleted
wolves in the operator bridge's directory for the server. It logs its clock offset from central's. Spike code only:
plain http on loopback, a fixed token, nothing stored.
"""
import argparse
import hashlib
import http.server
import json
import os
import sys
import tempfile
import threading
import time
import urllib.request

# ---------------- the stub central ----------------


class Central:
    def __init__(self, keys_path, expire):
        self.keys_path = keys_path
        self.expire = expire
        # One host token, as made on central and shown once; stored hashed.
        self.tokens = {hashlib.sha256(b"spike-host-token").hexdigest(): "world-spike-1"}
        self.worlds = {}
        self.lock = threading.Lock()

    def listed(self):
        now = time.time()
        with self.lock:
            return [dict(w, id=i) for i, w in self.worlds.items() if now - w["seen"] < self.expire]


def handler_for(central):
    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *a):
            pass

        def reply(self, code, obj):
            body = json.dumps(obj).encode()
            self.send_response(code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            if self.path == "/keys":
                with open(central.keys_path, "rb") as f:
                    return self.reply(200, json.load(f))
            if self.path == "/worlds":
                return self.reply(200, {"worlds": central.listed()})
            self.reply(404, {"error": "not found"})

        def do_POST(self):
            if self.path != "/worlds/heartbeat":
                return self.reply(404, {"error": "not found"})
            auth = self.headers.get("Authorization", "")
            token = auth.removeprefix("Bearer ").encode()
            world = central.tokens.get(hashlib.sha256(token).hexdigest())
            if not world:
                return self.reply(401, {"error": "unknown host token"})
            length = int(self.headers.get("Content-Length", "0"))
            if length > 64 * 1024:
                return self.reply(413, {"error": "too big"})
            try:
                beat = json.loads(self.rfile.read(length))
            except ValueError:
                return self.reply(400, {"error": "not JSON"})
            with central.lock:
                central.worlds[world] = {
                    "name": str(beat.get("name", ""))[:48],
                    "players": len(beat.get("online", [])),
                    "limit": int(beat.get("limit", 0)),
                    "protocol": beat.get("protocol"),
                    # The address is where the heartbeat came from, never a field the host fills in (doc 77, 7).
                    "address": f"{self.client_address[0]}:{int(beat.get('port', 0))}",
                    "seen": time.time(),
                }
            self.reply(200, {"world": world, "clock": time.time(), "bans": ["acct-banned-1"], "deleted": []})

    return Handler


def serve_central(port, keys, expire):
    central = Central(keys, expire)
    server = http.server.ThreadingHTTPServer(("127.0.0.1", port), handler_for(central))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server, central


# ---------------- the world agent ----------------


def write_atomic(path, data: bytes):
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(path), prefix=".tmp-")
    with os.fdopen(fd, "wb") as f:
        f.write(data)
    os.replace(tmp, path)


def call(url, token=None, body=None, timeout=5):
    req = urllib.request.Request(url, data=json.dumps(body).encode() if body is not None else None,
                                 headers={"Content-Type": "application/json"})
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read(65536))


def agent_once(central, token, directory, listing):
    online_file = os.path.join(directory, "online.json")   # Written by the server in the real thing.
    online = json.load(open(online_file)) if os.path.exists(online_file) else []
    sent = time.time()
    reply = call(f"{central}/worlds/heartbeat", token, dict(listing, online=online))
    offset = reply["clock"] - (sent + time.time()) / 2
    keys = call(f"{central}/keys")
    write_atomic(os.path.join(directory, "keys.json"), json.dumps(keys).encode())
    write_atomic(os.path.join(directory, "central-bans.json"),
                 json.dumps({"bans": reply.get("bans", []), "deleted": reply.get("deleted", [])}).encode())
    return {"world": reply["world"], "clockOffsetS": round(offset, 4), "keys": len(keys.get("keys", []))}


def run_agent(args):
    listing = {"name": "Spike world", "port": 47811, "limit": 40, "protocol": 1, "voices": False, "private": False}
    while True:
        try:
            print(json.dumps(agent_once(args.central, args.token, args.dir, listing)), flush=True)
        except Exception as e:  # A failed beat is logged and retried next time; the world keeps running.
            print(json.dumps({"error": str(e)}), flush=True)
        if args.once:
            return 0
        time.sleep(args.every)


# ---------------- the self-test ----------------


def selftest():
    with tempfile.TemporaryDirectory() as d:
        keys = os.path.join(d, "central-keys.json")
        json.dump({"keys": [{"kty": "OKP", "crv": "Ed25519", "kid": "k1", "x": "A" * 43}]}, open(keys, "w"))
        server, central = serve_central(0, keys, expire=1.5)
        url = f"http://127.0.0.1:{server.server_address[1]}"
        world_dir = os.path.join(d, "world")
        os.makedirs(world_dir)
        json.dump(["wolf-" + "a" * 32, "wolf-" + "b" * 32], open(os.path.join(world_dir, "online.json"), "w"))
        listing = {"name": "Spike world", "port": 47811, "limit": 40, "protocol": 1}
        checks = []
        try:
            call(f"{url}/worlds/heartbeat", "wrong-token", listing)
            checks.append(("wrong token refused", False))
        except urllib.error.HTTPError as e:
            checks.append(("wrong token refused", e.code == 401))
        t0 = time.perf_counter()
        result = agent_once(url, "spike-host-token", world_dir, listing)
        beat_ms = (time.perf_counter() - t0) * 1000
        listed = call(f"{url}/worlds")["worlds"]
        checks.append(("listed after a beat", len(listed) == 1 and listed[0]["players"] == 2))
        checks.append(("address from the connection", listed and listed[0]["address"] == "127.0.0.1:47811"))
        checks.append(("keys file written", json.load(open(os.path.join(world_dir, "keys.json")))["keys"][0]["kid"] == "k1"))
        checks.append(("bans left for the server", "acct-banned-1" in json.load(open(os.path.join(world_dir, "central-bans.json")))["bans"]))
        checks.append(("clock offset under 50 ms", abs(result["clockOffsetS"]) < 0.05))
        time.sleep(2.0)
        checks.append(("dropped after silence", call(f"{url}/worlds")["worlds"] == []))
        server.shutdown()
        for name, ok in checks:
            print(f"{'ok  ' if ok else 'FAIL'} {name}")
        print(f"one beat (heartbeat + /keys + two atomic files): {beat_ms:.1f} ms")
        return 0 if all(ok for _, ok in checks) else 1


def main():
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="role", required=True)
    c = sub.add_parser("central")
    c.add_argument("--port", type=int, default=47820)
    c.add_argument("--keys", required=True)
    c.add_argument("--expire", type=float, default=180)
    a = sub.add_parser("agent")
    a.add_argument("--central", required=True)
    a.add_argument("--token", required=True)
    a.add_argument("--dir", required=True)
    a.add_argument("--every", type=float, default=60)
    a.add_argument("--once", action="store_true")
    sub.add_parser("selftest")
    args = p.parse_args()
    if args.role == "central":
        serve_central(args.port, args.keys, args.expire)
        print(f"stub central on http://127.0.0.1:{args.port}", flush=True)
        threading.Event().wait()
    if args.role == "agent":
        return run_agent(args)
    return selftest()


if __name__ == "__main__":
    sys.exit(main())
