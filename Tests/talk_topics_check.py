#!/usr/bin/env python3
"""talk_topics (Docs/Design/84 §7.3, Phase 5, a ctest): topics.json's numbers are today's. The topic director's scores
moved out of World::ambientTopic and sceneTopics into Data/Talk/topics.json; before they moved, talk_sim --dump-topics
recorded every topic the director weighed in 400 exchanges of the synthetic talk town over the library at a46cbdac (a
hash of each look's topics: kind, score to the last bit, source, subject, claim, words, facts), and that record is
Tests/fixtures/talk_topics/town-400.topics. Two directors must weigh them all the same:

  - a library with no topics.json (a46cbdac's): today's book, built in (topics::today());
  - the same with --topics Tests/fixtures/talk_topics/topics-today.json: today's numbers read from a file, as the live
    Data/Talk/topics.json is read (which is tuned since, so it isn't the one compared).

Skipped (77) without the git history --library-from needs.

  python3 Tests/talk_topics_check.py BUILD_DIR
"""
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOWN = ROOT / "Tests" / "fixtures" / "talk_town"
GOLDEN = ROOT / "Tests" / "fixtures" / "talk_topics" / "town-400.topics"
TODAY = ROOT / "Tests" / "fixtures" / "talk_topics" / "topics-today.json"


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    sim = Path(sys.argv[1]) / "talk_sim"
    failures = []
    with tempfile.TemporaryDirectory(prefix="ratw-talk-topics-") as tmp:
        for name, extra in (("built-in", []), ("topics-today.json", ["--topics", str(TODAY)])):
            out = Path(tmp) / name.replace(".", "_")
            dump = out.with_suffix(".topics")
            p = subprocess.run([str(sim), "--world", str(TOWN), "--profile", "town", "--warm", "2", "--exchanges", "400",
                                "--library-from", "a46cbdac", "--run", "topics", "--out", str(out),
                                "--dump-topics", str(dump), *extra], capture_output=True, text=True)
            if p.returncode == 77:
                print("talk_topics: skipped (" + p.stderr.strip() + ")")
                return 77
            if p.returncode != 0:
                failures.append(f"{name}: talk_sim failed ({p.returncode}): {p.stderr.strip()[-400:]}")
                continue
            got = dump.read_text().splitlines()
            want = GOLDEN.read_text().splitlines()
            if got != want:
                at = next((i for i, (g, w) in enumerate(zip(got, want)) if g != w), min(len(got), len(want)))
                failures.append(f"{name}: the director weighed differently from look {at + 1} of {len(want)} "
                                f"(got {got[at] if at < len(got) else 'nothing'}, want {want[at] if at < len(want) else 'nothing'})")
            else:
                print(f"talk_topics: {name}: {len(want)} looks, {sum(int(l.split()[1]) for l in want)} topics weighed, identical")
    for f in failures:
        print("FAIL " + f)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
