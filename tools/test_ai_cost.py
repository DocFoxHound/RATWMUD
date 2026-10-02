#!/usr/bin/env python3
"""The cost report (ai_cost.py), from made-up ledgers and prices."""
import json
import tempfile
import time
import unittest
from pathlib import Path

import ai_cost as A


class CostTests(unittest.TestCase):
    def setUp(self):
        self.dir = Path(tempfile.mkdtemp())
        now = time.time()
        voices = [{"t": now, "kind": "dialogue", "route": r, "npc": "wren"} for r in ("game", "game", "model", "written")]
        voices += [{"t": now, "kind": "exchange", "route": "library", "npc": "wren"}]
        voices += [{"t": now - 30 * 86400, "kind": "dialogue", "route": "model", "npc": "old"}]
        calls = [{"t": now, "event": "dialogue", "model": "big", "outcome": "success", "prompt_tokens": 500,
                  "cached_tokens": 400, "completion_tokens": 40},
                 {"t": now, "event": "summary", "model": "small", "outcome": "success", "prompt_tokens": 300,
                  "completion_tokens": 60},
                 {"t": now, "event": "dialogue", "model": "big", "outcome": "provider_http"}]
        (self.dir / "v.jsonl").write_text("\n".join(json.dumps(v) for v in voices) + "\nnot json\n")
        (self.dir / "c.jsonl").write_text("\n".join(json.dumps(c) for c in calls) + "\n")
        (self.dir / "config.json").write_text(json.dumps({"api_key": "sk-never-read", "prices": {
            "big": {"input": 2.0, "cached_input": 0.5, "output": 8.0}}}))

    def test_a_day_of_voices(self):
        since = time.time() - 86400
        result = A.report(list(A.read_lines(self.dir / "v.jsonl", since)), list(A.read_lines(self.dir / "c.jsonl", since)),
                          A.read_prices(self.dir / "config.json"))
        (day, d), = result.items()
        self.assertEqual((5, 4, 0.8), (d["linesSaid"], d["withoutModel"], d["withoutModelShare"]),
                         "The game, the library and written lines cost nothing; the old line is out of range")
        big = next(r for r in d["calls"] if r["model"] == "big")
        self.assertEqual((2, 1), (big["calls"], big["failed"]))
        self.assertAlmostEqual((100 * 2.0 + 400 * 0.5 + 40 * 8.0) / 1e6, big["cost"])
        self.assertEqual(["small"], d["unpriced"], "A model without prices is named, not guessed")
        self.assertAlmostEqual(400 / 800, d["cachedShare"])

    def test_the_key_is_never_read(self):
        self.assertNotIn("sk-never-read", json.dumps(A.read_prices(self.dir / "config.json")))


if __name__ == "__main__":
    unittest.main()
