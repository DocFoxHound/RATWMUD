#!/usr/bin/env python3
"""tools/talk_rate.py's packet (Docs/Design/84 §15.7): blind, its key apart, nothing drawn twice.

  cd tools && python3 -m unittest test_talk_rate
"""
from __future__ import annotations

import argparse
import contextlib
import io
import json
import random
import re
import tempfile
import unittest
from pathlib import Path

import talk_rate


def write_run(d: Path, convs: int, opener: str, people=("a", "b"), shuffled_too: bool = True):
    d.mkdir(parents=True)
    (d / "run.json").write_text(json.dumps({"profile": "L1", "hours": 100}))
    (d / "residents.jsonl").write_text("".join(json.dumps({"id": p, "name": p.upper() + "ra Stone"}) + "\n" for p in people))
    lines, heads, shuffled = [], [], []
    for c in range(convs):
        heads.append({"conv": c, "topic": "talk.gossip" if c % 2 else "talk.weather", "cell": f"cell_{c}", "day": 30.5})
        for b in range(2 + c % 3):
            line = {"conv": c, "beat": b, "t": 60.0 * c + b, "who": people[b % 2], "entry": f"e{c}.{b}",
                    "text": f"{opener} {c}." if b == 0 else f"Reply {b} to {c}."}
            lines.append(line)
            shuffled.append(dict(line, text=line["text"] if b == 0 else f"Swapped {b} in {c}.", shuffled_from=c + 1))
    (d / "lines.jsonl").write_text("".join(json.dumps(l) + "\n" for l in lines))
    (d / "convs.jsonl").write_text("".join(json.dumps(h) + "\n" for h in heads))
    if shuffled_too:
        (d / "lines-shuffled.jsonl").write_text("".join(json.dumps(l) + "\n" for l in shuffled))
        (d / "convs-shuffled.jsonl").write_text("".join(json.dumps(h) + "\n" for h in heads))


class Packet(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        write_run(self.root / "new", 40, "Fresh word")
        write_run(self.root / "old", 40, "Stale word")

    def tearDown(self):
        self.tmp.cleanup()

    def make(self, each=10, key=None):
        out = self.root / "packet"
        args = argparse.Namespace(new=self.root / "new", old=self.root / "old", shuffled=None, out=out, key=key,
                                  each=each, seed=3, title="How natural is it?")
        with contextlib.redirect_stderr(io.StringIO()):
            rc = talk_rate.packet(args)
        return rc, out

    def test_blind_with_the_key_apart(self):
        key = self.root / "elsewhere" / "key.json"
        rc, out = self.make(key=key)
        self.assertEqual(rc, 0)
        self.assertFalse((out / "key.json").exists(), "--key keeps the key out of the packet's folder")
        k = json.loads(key.read_text())
        self.assertEqual(k["counts"], {"new": 10, "old": 10, "shuffled": 10})
        self.assertEqual(sorted(k["items"], key=int), [str(i) for i in range(1, 31)])
        sheet = (out / "packet.md").read_text()
        page = re.sub(r"<script>.*?</script>", "", (out / "rate.html").read_text(), flags=re.S)   # (What is shown.)
        for text in (sheet, page):
            for leak in ("new", "old", "shuffled", "baseline", "control", "cell_", "e1", "talk.gossip", "L1", "day 30"):
                self.assertIsNone(re.search(rf"\b{re.escape(leak)}\b", text, re.I), f"{leak!r} would tell the sources apart")
        self.assertEqual(sheet.count("\n## "), 30)
        self.assertIn("> **Ara Stone:**", sheet)
        self.assertEqual(len((out / "ratings.csv").read_text().splitlines()), 31)

    def test_no_conversation_or_opener_twice(self):
        rc, out = self.make(each=12)
        self.assertEqual(rc, 0)
        k = json.loads((out / "key.json").read_text())["items"]
        new = {v["conv"] for v in k.values() if v["source"] == "new"}
        shuffled = {v["conv"] for v in k.values() if v["source"] == "shuffled"}
        self.assertFalse(new & shuffled, "a shuffled talk keeps its real one's first line: never both in a packet")
        firsts = [s.split("\n")[2] for s in (out / "packet.md").read_text().split("\n## ")[1:]]
        self.assertEqual(len(firsts), len(set(firsts)), "no two items open with the same words")

    def test_too_few_is_refused(self):
        rc, _ = self.make(each=25)          # (40 talks: 25 new leave 15 for the shuffled control.)
        self.assertEqual(rc, 1)

    def test_sample_spreads_and_records(self):
        run = talk_rate.talk_score.Run(self.root / "new")
        taken, openers = set(), set()
        got = talk_rate.sample(run, 8, random.Random(1), taken, openers)
        self.assertEqual(len(got), 8)
        self.assertEqual(len(taken), 8)
        self.assertEqual(len(openers), 8)
        self.assertEqual({len(x[2]) for x in got}, {2, 3, 4}, "spread over length")
        again = talk_rate.sample(run, 40, random.Random(2), taken, openers)
        self.assertEqual(len(again), 32, "what was taken is never taken again")


if __name__ == "__main__":
    unittest.main()
