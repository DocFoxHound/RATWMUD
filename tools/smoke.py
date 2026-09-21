#!/usr/bin/env python3
"""Run real Unreal processes in an isolated save and collect their evidence."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sqlite3
import socket
import errno
import time


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("network", "gallery", "walkthrough", "persistence"))
    parser.add_argument("--headless", action="store_true", help="Skip GPU screenshots for transport testing")
    parser.add_argument("--packaged", action="store_true", help="Run archived Linux binaries without the editor")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    engine = Path(os.environ.get("RATW_UNREAL_ROOT", "/home/martinb/Applications/UnrealEngine/5.8.2"))
    editor = engine / "Engine/Binaries/Linux/UnrealEditor"
    output = root / ("artifacts/packaged-evidence" if args.packaged else "artifacts/screenshots")
    logs = root / "artifacts/logs"
    output.mkdir(parents=True, exist_ok=True)
    logs.mkdir(parents=True, exist_ok=True)
    run_id = str(time.time_ns())
    save = root / "Saved/Tests" / run_id / "world.sqlite"
    save.parent.mkdir(parents=True, exist_ok=True)
    processes: list[subprocess.Popen] = []
    handles = []
    common = ([str(root / "artifacts/package/Linux/RATWMUD/Binaries/Linux/RATWMUD")]
              if args.packaged else [str(editor), str(root / "RATWMUD.uproject")])
    flags = ["-NoSplash", "-NoSound", "-Unattended", "-noscreenmessages", "-NoSteam", "-NoVSync", "-ForceRes", "-ForceLogFlush", "-RatwDevTools"]

    def start(label: str, arguments: list[str]) -> subprocess.Popen:
        handle = (logs / f"{'packaged-' if args.packaged else ''}{label}.log").open("w")
        handles.append(handle)
        child = subprocess.Popen(common + arguments + flags, cwd=root, stdout=handle, stderr=subprocess.STDOUT)
        processes.append(child)
        print(f"Started {label}: PID {child.pid}", flush=True)
        return child

    try:
        if args.mode == "network":
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as reservation:
                reservation.bind(("127.0.0.1", 0))
                port = reservation.getsockname()[1]
            hosting = ["/Engine/Maps/Entry?listen", "-RatwHeadlessHost"] if args.packaged else ["/Engine/Maps/Entry", "-server"]
            server = start("server", hosting + ["-nullrhi", f"-port={port}", "-MULTIHOME=127.0.0.1", f"-RatwSave={save}"])
            deadline = time.monotonic() + 90
            while time.monotonic() < deadline:
                if server.poll() is not None:
                    raise RuntimeError("Server exited before listening; inspect artifacts/logs/server.log")
                log = (logs / ("packaged-server.log" if args.packaged else "server.log")).read_text(errors="replace")
                if f"listening on port {port}" in log:
                    break
                # Packaged stdout can buffer a quiet host's final startup line.
                # The port was free before spawning our isolated server.
                with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
                    try:
                        probe.bind(("127.0.0.1", port))
                    except OSError as error:
                        if error.errno == errno.EADDRINUSE:
                            break
                        raise
                time.sleep(0.25)
            else:
                raise RuntimeError("Timed out waiting for server to listen")
            clients = []
            for identity, name in (("ash", "Ash"), ("bracken", "Bracken")):
                graphics = ["-nullrhi"] if args.headless else ["-windowed", "-ResX=1600", "-ResY=1000", "-RenderOffscreen"]
                clients.append(start(identity, [f"127.0.0.1:{port}", "-game", f"-RatwIdentity={identity}", f"-RatwName={name}", "-RatwScenario=network", f"-RatwCaptureDir={output}"] + graphics))
            codes = [client.wait(timeout=150) for client in clients]
            results = [json.loads((output / f"network-{identity}.json").read_text()) for identity in ("ash", "bracken")]
            if any(codes) or not all(item["passed"] for item in results):
                raise RuntimeError(f"Network scenario failed: exits={codes}, details={[item['detail'] for item in results]}")
            print("PASS: two independent clients moved and received each other's IC and local OOC events.", flush=True)
        elif args.mode == "persistence":
            for scenario in ("persist-write", "persist-read", "persist-aged"):
                if scenario == "persist-aged":
                    # Only this run's disposable SQLite fixture is aged. The
                    # core tests independently verify exact3599/3600 boundaries.
                    with sqlite3.connect(save) as database:
                        payload = json.loads(database.execute("SELECT payload FROM world_state WHERE id=1").fetchone()[0])
                        assert payload["activeMemory"], "No NPC interaction was stored"
                        for memory in payload["activeMemory"]:
                            memory["lastActivity"] = time.time() - 3601
                        database.execute("UPDATE world_state SET payload=? WHERE id=1", (json.dumps(payload),))
                child = start(scenario, ["/Engine/Maps/Entry", "-game", "-nullrhi", "-RatwIdentity=ash", "-RatwName=Ash", f"-RatwScenario={scenario}", f"-RatwSave={save}", f"-RatwCaptureDir={output}"])
                code = child.wait(timeout=150)
                result = json.loads((output / f"{scenario}-ash.json").read_text())
                if code or not result["passed"]:
                    raise RuntimeError(f"{scenario} failed: {result['detail']}")
                print(f"PASS: {scenario}: {result['detail']}", flush=True)
            print("PASS: separate process restarts retained character, map memory and conversation; one hour inactive produced a permanent summary.", flush=True)
        elif args.mode == "walkthrough":
            child = start("walkthrough", ["/Engine/Maps/Entry", "-game", "-windowed", "-ResX=1600", "-ResY=1000", "-RenderOffscreen", "-RatwIdentity=ash", "-RatwName=Ash", "-RatwScenario=walkthrough", f"-RatwSave={save}", f"-RatwCaptureDir={output}"])
            code = child.wait(timeout=240)
            result = json.loads((output / "walkthrough-ash.json").read_text())
            images = [output / name for name in ("06-visible-vertical-world.png", "07-quiet-loft.png", "08-rain-in-juniper-yard.png")]
            if code or not result["passed"] or not all(path.exists() and path.stat().st_size > 1024 for path in images):
                raise RuntimeError(f"Walkthrough failed: {result['detail']}")
            print(f"PASS: {result['detail']}", flush=True)
        else:
            child = start("gallery", ["/Engine/Maps/Entry", "-game", "-windowed", "-ResX=1600", "-ResY=1000", "-RenderOffscreen", "-RatwIdentity=ash", "-RatwName=Ash", "-RatwScenario=gallery", f"-RatwSave={save}", f"-RatwCaptureDir={output}"])
            code = child.wait(timeout=600)
            result = json.loads((output / "gallery-ash.json").read_text())
            images = [output / name for name in ("01-tavern-local.png", "02-character-sheet.png", "03-inventory.png", "04-world-map.png", "05-settings.png", "09-text-first-layout.png")]
            if code or not result["passed"] or not all(path.exists() and path.stat().st_size > 1024 for path in images):
                raise RuntimeError("Gallery failed or one or more screenshots are missing")
            print("PASS: six actual Unreal viewport screenshots captured.", flush=True)
        return 0
    finally:
        for child in reversed(processes):
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait(timeout=10)
        for handle in handles:
            handle.close()


if __name__ == "__main__":
    raise SystemExit(main())
