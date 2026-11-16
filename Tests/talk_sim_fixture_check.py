#!/usr/bin/env python3
"""talk_sim_fixture_check (Docs/Design/84 §15.1, a ctest; the slice-end review): talk_sim refuses a fixture whose files
aren't the ones it recorded (a file missing, or one whose fnv64 differs: a partial or damaged copy) before it replays
anything, and a run with --library-from leaves no copy of the library in /tmp. No database: the synthetic talk town.

  python3 Tests/talk_sim_fixture_check.py BUILD_DIR
"""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOWN = ROOT / "Tests" / "fixtures" / "talk_town"


def fnv64(path: Path) -> str:
    h = 1469598103934665603
    for b in path.read_bytes():
        h = ((h ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return f"{h:016x}"


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    sim = Path(sys.argv[1]) / "talk_sim"
    failures = []
    with tempfile.TemporaryDirectory(prefix="ratw-talk-fixture-") as tmp:
        fx = Path(tmp) / "fx"
        fx.mkdir()
        for name, text in (("cp-0.json.gz", b"one"), ("cp-1.json.gz", b"two"), ("track.jsonl.gz", b"three")):
            (fx / name).write_bytes(text)
        manifest = {"world": "talk_town", "build": 0, "world_seconds": 10,
                    "checkpoints": [{"file": "cp-0.json.gz", "t": 0, "day": 1, "fnv64": fnv64(fx / "cp-0.json.gz")},
                                    {"file": "cp-1.json.gz", "t": 5, "day": 1.5, "fnv64": "0000000000000000"}],
                    "track": {"file": "track.jsonl.gz", "fnv64": fnv64(fx / "track.jsonl.gz")}}
        (fx / "fixture.json").write_text(json.dumps(manifest))

        def run(*extra):
            return subprocess.run([str(sim), "--world", str(TOWN), "--profile", "town", "--out", str(Path(tmp) / "out"), *extra],
                                  capture_output=True, text=True)

        p = run("--fixtures", str(fx))
        if p.returncode != 1 or "cp-1.json.gz isn't the one recorded" not in p.stderr:
            failures.append(f"a damaged checkpoint is refused (rc {p.returncode}): {p.stderr.strip()[-300:]}")
        (fx / "cp-1.json.gz").unlink()
        p = run("--fixtures", str(fx))
        if p.returncode != 1 or "cp-1.json.gz is missing" not in p.stderr:
            failures.append(f"a missing checkpoint is refused (rc {p.returncode}): {p.stderr.strip()[-300:]}")
        # --library-from: the copy of the library is gone when the run ends.
        before = set(Path(tempfile.gettempdir()).glob("ratw-talk-lib-*"))
        p = run("--warm", "1", "--exchanges", "5", "--library-from", "HEAD")
        after = set(Path(tempfile.gettempdir()).glob("ratw-talk-lib-*"))
        if p.returncode == 77:
            print("talk_sim_fixture_check: no git history here; the library check skipped", file=sys.stderr)
        elif p.returncode != 0:
            failures.append(f"a run over the library at HEAD (rc {p.returncode}): {p.stderr.strip()[-300:]}")
        elif after - before:
            failures.append(f"--library-from left {', '.join(str(d) for d in sorted(after - before))} behind")
    for f in failures:
        print("talk_sim_fixture_check: " + f, file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
