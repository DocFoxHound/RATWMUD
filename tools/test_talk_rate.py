#!/usr/bin/env python3
"""tools/talk_rate.py (Docs/Design/84 §15.7): the packet blind, its key apart, nothing drawn twice; the score from one
rater or several, on any axes, with agreement between them.

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


class Score(unittest.TestCase):
    KEY = {str(i): {"source": s, "run": "", "conv": i} for i, s in
           enumerate(["new", "new", "old", "old", "shuffled", "shuffled"], 1)}

    def test_krippendorff_reference(self):
        # Krippendorff (2011), "Computing Krippendorff's alpha-reliability": 4 observers, 12 units, some missing.
        n = None
        rows = [[1, 2, 3, 3, 2, 1, 4, 1, 2, n, n, n], [1, 2, 3, 3, 2, 2, 4, 1, 2, 5, n, 3],
                [n, 3, 3, 3, 2, 3, 4, 2, 2, 5, 1, n], [1, 2, 3, 3, 2, 4, 4, 1, 2, 5, 1, n]]
        units = [[r[u] for r in rows if r[u] is not None] for u in range(12)]
        self.assertAlmostEqual(talk_rate.krippendorff(units, "interval"), 0.849, places=3)
        self.assertAlmostEqual(talk_rate.krippendorff(units, "ordinal"), 0.815, places=3)
        self.assertEqual(talk_rate.krippendorff([[1, 1], [2, 2], [3, 3]]), 1.0)

    def test_many_raters_and_axes(self):
        a = {"rater": "A", "ratings": [{"id": "1", "believable": 4, "coherent": 5}, {"id": "2", "believable": 2, "coherent": 1,
                                                                                    "note": "Nothing answers."},
                                       {"id": "3", "believable": 4, "coherent": 4}, {"id": "4", "believable": 5, "coherent": 5},
                                       {"id": "5", "believable": 1, "coherent": 1}, {"id": "6", "believable": 2, "coherent": 1}]}
        b = {"rater": "B", "ratings": [dict(x, note="Odd.") if x["id"] == "2" else dict(x) for x in a["ratings"]]}
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "ratings.json"
            path.write_text(json.dumps({"raters": [a, b]}))
            raters = talk_rate.read_raters([path])
        self.assertEqual([r["name"] for r in raters], ["A", "B"])
        out = talk_rate.score_ratings(self.KEY, raters, worst=1)
        h1 = out["H1"]
        self.assertEqual(out["axes"], ["believable", "coherent", "overall"])
        self.assertEqual(h1["axis"], "overall", "with two axes, H1 is judged on their mean")
        self.assertAlmostEqual(h1["mean"]["new"], 3.0)
        self.assertAlmostEqual(h1["mean"]["old"], 4.5)
        self.assertAlmostEqual(h1["new_minus_old"], -1.5)
        self.assertEqual(h1["met"], {"new": False, "new_minus_shuffled": False, "new_minus_old": False})
        self.assertEqual(h1["by_axis"]["coherent"]["new"]["dist"], {"1": 2, "5": 2})
        self.assertEqual(out["agreement"]["coherent"]["alpha_interval"], 1.0, "two raters giving the same numbers agree")
        self.assertEqual(out["agreement"]["coherent"]["exact_pct"], 100.0)
        self.assertEqual(out["H2"], "not rated")
        self.assertEqual([w["item"] for w in out["worst"]], ["2"], "the new source's lowest, only")
        self.assertEqual(out["worst"][0]["notes"], {"A": "Nothing answers.", "B": "Odd."})
        self.assertIn("**Not met**", talk_rate.report(out))

    def test_one_csv_still_scores(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "ratings.csv"
            path.write_text("item,rating,writer\n1,5,1\n2,4,0\n3,3,0\n4,3,0\n5,1,0\n6,,0\n")
            out = talk_rate.score_ratings(self.KEY, talk_rate.read_raters([path]), worst=0)
        self.assertEqual(out["axes"], ["rating"])
        self.assertAlmostEqual(out["H1"]["mean"]["new"], 4.5)
        self.assertAlmostEqual(out["H1"]["new_minus_shuffled"], 3.5, msg="an item left blank isn't rated")
        self.assertTrue(out["H1"]["met"]["new_minus_shuffled"])
        self.assertEqual(out["H2"]["new"], 50.0)
        self.assertEqual(out["agreement"], {}, "one rater: no agreement to measure")


if __name__ == "__main__":
    unittest.main()
