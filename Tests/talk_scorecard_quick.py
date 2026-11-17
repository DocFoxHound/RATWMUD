#!/usr/bin/env python3
"""talk_scorecard_quick (Docs/Design/84 §15.8, a ctest): the conversation simulator over the synthetic talk town
(Tests/fixtures/talk_town, tools/talk_town.py), 2,000 frozen exchanges with the shuffled control, scored by
tools/talk_score.py against the committed baseline (Tests/fixtures/talk_scorecard_quick.json): a hard gate the baseline
passed must pass, an open one may not get worse, and no soft metric may regress by more than 10%. No database; under 60 s.

  python3 Tests/talk_scorecard_quick.py BUILD_DIR            the check (exit 1 on a broken gate)
  python3 Tests/talk_scorecard_quick.py BUILD_DIR --bless    writes the baseline (on purpose only: say why in the commit)
"""
from __future__ import annotations

import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = ROOT / "Tests" / "fixtures" / "talk_scorecard_quick.json"


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    build = Path(sys.argv[1])
    bless = "--bless" in sys.argv[2:]
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="ratw-talk-quick-") as tmp:
        out = Path(tmp) / "town"
        sim = subprocess.run([str(build / "talk_sim"), "--world", str(ROOT / "Tests" / "fixtures" / "talk_town"),
                              "--profile", "town", "--warm", "2", "--exchanges", "2000", "--ablate", "shuffle",
                              "--run", "quick", "--out", str(out),
                              "--mantles-from", str(ROOT / "Tests" / "fixtures" / "mantles")], capture_output=True, text=True)
        if sim.returncode != 0:
            print(sim.stderr, file=sys.stderr)
            return 1
        score = [sys.executable, str(ROOT / "tools" / "talk_score.py"), str(out)]
        score += ["--bless", str(BASELINE)] if bless else ["--baseline", str(BASELINE), "--check"]
        scored = subprocess.run(score, capture_output=True, text=True)
        sys.stderr.write(scored.stderr)
    took = time.monotonic() - started
    print(f"talk_scorecard_quick: {took:.1f} s", file=sys.stderr)
    if took > 60:
        print("talk_scorecard_quick: over its 60 s budget", file=sys.stderr)
        return 1
    return scored.returncode


if __name__ == "__main__":
    raise SystemExit(main())
