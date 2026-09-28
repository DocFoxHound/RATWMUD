#!/usr/bin/env python3
"""The NPC Mind with the real game, offline: the Mind's fixture provider (no model, no cost), a real Unreal game
process and an isolated synthetic save. Three runs of the dialogue scenarios:

  1. A player tells Rowan about a promise. The reply shown in the game is the one the Mind made (by hash); the save
     then holds Rowan's bond with the player, moved by the reply's trust nudge, and the promise and a private note in
     the conversation's memory.
  2. After a restart the bond is still there.
  3. With the save aged past an hour of quiet, the conversation closes at start-up and the Mind's summary replaces
     the extractive one.

  python3 tools/mind_smoke.py
  python3 tools/mind_smoke.py --standalone build-core/ratw_server   the same against the standalone server
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import socket
import subprocess
import threading
import time

import npc_mind as mind


STANDALONE: str | None = None     # The standalone server, to run the game against instead of in one Unreal process.


def run(root: Path, binary: list[str], save: Path, folder: Path, scenario: str, endpoint: str) -> dict:
    folder.mkdir()
    command = binary + ["/Engine/Maps/Entry", "-game", "-nullrhi", "-NoSound", "-Unattended", "-NoSplash", "-NoSteam",
                        "-ForceLogFlush", "-RatwDevTools", "-RatwDevIdentity", "-RatwIdentity=ash", "-RatwName=Ash",
                        f"-RatwCaptureDir={folder}", f"-RatwScenario={scenario}"]
    authority = None
    if STANDALONE:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        server_log = (folder / "server.log").open("w")
        authority = subprocess.Popen([STANDALONE, "--save", str(save), "--port", str(port), "--dev-identity", "--dev-tools",
                                      "--dialogue", endpoint], cwd=root, stdout=server_log, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 30
        while f"listening on port {port}" not in (folder / "server.log").read_text(errors="replace"):
            if authority.poll() is not None or time.monotonic() > deadline:
                raise OSError("The standalone server did not start.")
            time.sleep(.1)
        command += [f"-RatwServer=127.0.0.1:{port}"]
    else:
        command += [f"-RatwSave={save}", f"-RatwDialogueEndpoint={endpoint}"]
    with (folder / "engine.log").open("w") as log:
        process = subprocess.Popen(command, cwd=root, stdout=log, stderr=subprocess.STDOUT)
        try:
            code = process.wait(timeout=150)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)
            if authority:
                authority.terminate()
                authority.wait(timeout=30)
    result = json.loads((folder / f"{scenario}-ash.json").read_text())
    assert code == 0 and result["passed"], f"{scenario} failed: {result.get('detail')}"
    return result


def saved(save: Path) -> dict:
    if STANDALONE:
        return json.loads(save.read_text())
    with sqlite3.connect(save) as database:
        return json.loads(database.execute("SELECT payload FROM world_state WHERE id=1").fetchone()[0])


def age(save: Path) -> None:
    """Every open conversation, as if an hour had passed since its last word."""
    state = saved(save)
    for memory in state["activeMemory"]:
        memory["lastActivity"] = time.time() - 3601
    if STANDALONE:
        save.write_text(json.dumps(state))
        return
    with sqlite3.connect(save) as database:
        database.execute("UPDATE world_state SET payload=? WHERE id=1", (json.dumps(state),))


def main() -> int:
    global STANDALONE
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--standalone", metavar="RATW_SERVER", help="Run against the standalone server (build-core/ratw_server).")
    STANDALONE = parser.parse_args().standalone
    root = Path(__file__).resolve().parent.parent
    engine = Path(os.environ.get("RATW_UNREAL_ROOT", "/home/martinb/Applications/UnrealEngine/5.8.2"))
    binary = [str(engine / "Engine/Binaries/Linux/UnrealEditor"), str(root / "RATWMUD.uproject")]
    output = root / "artifacts/mind-smoke" / str(time.time_ns())
    output.mkdir(parents=True)
    save = root / "Saved/Tests" / output.name / ("world.json" if STANDALONE else "world.sqlite")
    save.parent.mkdir(parents=True)
    audit: list[dict] = []
    server = mind.Server(0, mind.Mind(mind.FixtureProvider(), budget=mind.Budget(100, 100), audit=audit.append))
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    endpoint = f"http://127.0.0.1:{server.server_address[1]}/dialogue"
    try:
        result = run(root, binary, save, output / "first", "dialogue-live", endpoint)
        replies = [a for a in audit if a["event"] == "dialogue"]
        assert len(replies) == 1 and replies[0]["outcome"] == "success", "Expected one reply from the Mind"
        shown = result.get("dialogueEvidence", {}).get("replyText", "")
        assert shown == "I hear you, Ash.", f"The game showed the Mind's words, not its authored line: {shown!r}"
        state = saved(save)
        bonds = {(b["holder"], b["other"]): b for b in state.get("bonds", [])}
        keeper = [b for (holder, other), b in bonds.items() if holder == "npc_keeper" and other == "player-ash"]
        assert keeper and keeper[0]["trust"] >= 1, f"Rowan's bond with the player, with the promise's trust: {keeper}"
        turns = [t for m in state.get("activeMemory", []) if m["npc"] == "npc_keeper" for t in m["turns"]]
        whos = {t["who"] for t in turns}
        assert "(their promise)" in whos and "(your note)" in whos, f"The promise and the note are remembered: {whos}"
        print(f"PASS: the Mind's reply reached the game; Rowan's trust in the player is {keeper[0]['trust']:.1f}; "
              "the promise and a note are in memory.", flush=True)

        run(root, binary, save, output / "restart", "dialogue-recall", endpoint)
        again = {(b["holder"], b["other"]) for b in saved(save).get("bonds", [])}
        assert set(bonds) <= again, "Bonds survive a restart"
        print("PASS: after a restart the bonds are still there.", flush=True)

        age(save)
        run(root, binary, save, output / "aged", "dialogue-recall", endpoint)
        summaries = [s for s in saved(save).get("summaries", []) if s["npc"] == "npc_keeper"]
        assert any(s["text"].startswith("Summary. A conversation of") for s in summaries), \
            f"The Mind's summary replaced the extractive one: {[s['text'][:40] for s in summaries]}"
        assert any(a["event"] == "summary" and a["outcome"] == "success" for a in audit), "The Mind summarised"
        print("PASS: a closed conversation was summarised by the Mind.", flush=True)
        (output / "report.json").write_text(json.dumps({"passed": True, "audit": audit}, indent=2) + "\n")
        return 0
    except (AssertionError, OSError, KeyError, ValueError, sqlite3.Error, subprocess.SubprocessError) as error:
        print(f"FAIL: {error}", flush=True)
        (output / "report.json").write_text(json.dumps({"passed": False, "audit": audit}, indent=2) + "\n")
        return 1
    finally:
        server.shutdown()
        worker.join(timeout=5)
        print(f"Evidence: {output}", flush=True)


if __name__ == "__main__":
    raise SystemExit(main())
