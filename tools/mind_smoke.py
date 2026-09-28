#!/usr/bin/env python3
"""The NPC Mind with the real game, offline: the Mind's fixture provider (no model, no cost), the game server with a
scripted browser-client player, and an isolated synthetic save. Three runs of the dialogue scenarios:

  1. A player tells Rowan about a promise. The reply shown in the game is the one the Mind made (by hash); the save
     then holds Rowan's bond with the player, moved by the reply's trust nudge, and the promise and a private note in
     the conversation's memory.
  2. After a restart the bond is still there.
  3. With the save aged past an hour of quiet, the conversation closes at start-up and the Mind's summary replaces
     the extractive one.

  python3 tools/mind_smoke.py
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import threading
import time

import npc_mind as mind
from game_run import ROOT, GameServer, run_scenario


def run(save: Path, folder: Path, scenario: str, endpoint: str) -> dict:
    folder.mkdir()
    with GameServer(save=save, flags=["--dev-tools", "--dialogue", endpoint], log=folder / "server.log") as server:
        return run_scenario(server.port, scenario, "ash", capture=folder, log=folder / "client.log")


def saved(save: Path) -> dict:
    return json.loads(save.read_text())


def age(save: Path) -> None:
    """Every open conversation, as if an hour had passed since its last word."""
    state = saved(save)
    for memory in state["activeMemory"]:
        memory["lastActivity"] = time.time() - 3601
    save.write_text(json.dumps(state))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.parse_args()
    root = ROOT
    output = root / "artifacts/mind-smoke" / str(time.time_ns())
    output.mkdir(parents=True)
    save = root / "Saved/Tests" / output.name / "world.json"
    save.parent.mkdir(parents=True)
    audit: list[dict] = []
    server = mind.Server(0, mind.Mind(mind.FixtureProvider(), budget=mind.Budget(100, 100), audit=audit.append))
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    endpoint = f"http://127.0.0.1:{server.server_address[1]}/dialogue"
    try:
        result = run(save, output / "first", "dialogue-live", endpoint)
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

        run(save, output / "restart", "dialogue-recall", endpoint)
        again = {(b["holder"], b["other"]) for b in saved(save).get("bonds", [])}
        assert set(bonds) <= again, "Bonds survive a restart"
        print("PASS: after a restart the bonds are still there.", flush=True)

        age(save)
        run(save, output / "aged", "dialogue-recall", endpoint)
        summaries = [s for s in saved(save).get("summaries", []) if s["npc"] == "npc_keeper"]
        assert any(s["text"].startswith("Summary. A conversation of") for s in summaries), \
            f"The Mind's summary replaced the extractive one: {[s['text'][:40] for s in summaries]}"
        assert any(a["event"] == "summary" and a["outcome"] == "success" for a in audit), "The Mind summarised"
        print("PASS: a closed conversation was summarised by the Mind.", flush=True)
        (output / "report.json").write_text(json.dumps({"passed": True, "audit": audit}, indent=2) + "\n")
        return 0
    except (AssertionError, OSError, KeyError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"FAIL: {error}", flush=True)
        (output / "report.json").write_text(json.dumps({"passed": False, "audit": audit}, indent=2) + "\n")
        return 1
    finally:
        server.shutdown()
        worker.join(timeout=5)
        print(f"Evidence: {output}", flush=True)


if __name__ == "__main__":
    raise SystemExit(main())
