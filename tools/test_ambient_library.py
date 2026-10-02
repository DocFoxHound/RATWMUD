#!/usr/bin/env python3
"""The exchange library's writer (ambient_library.py), offline: what it keeps, and what it refuses."""
import json
import tempfile
import unittest
from pathlib import Path

import ambient_library as L


def line(who, text):
    return {"speaker": who, "text": text}


class LibraryTests(unittest.TestCase):
    def test_the_rules(self):
        self.assertEqual([["a", "They say {subject} {claim}."], ["b", "Is that so."]],
                         L.check("gossip", [line("a", "They say {subject} {claim}."), line("b", "Is that so.")]))
        for bad in ([line("a", "Hello.")],                                                  # one line
                    [line("a", "Hi."), line("a", "Hi again.")],                             # one speaker
                    [line("b", "Hi."), line("a", "Hi.")],                                   # b first
                    [line("a", "They say {subject} {victim}."), line("b", "Hm.")],          # a blank not on the list
                    [line("a", "I hear things."), line("b", "Oh?")],                        # gossip without its blanks
                    [line("a", "They say {subject} {claim}, and Bertram saw it."), line("b", "Hm.")],   # a made-up name
                    [line("a", "x" * 300), line("b", "Hm.")]):
            self.assertIsNone(L.check("gossip", bad), bad)
        self.assertIsNotNone(L.check("quarrel", [line("a", "I'll not stand for it, {listener}."), line("b", "Then sit.")]),
                             "\"I'll\" is no name")

    def test_written_offline_and_added_to_the_library(self):
        out = Path(tempfile.mkdtemp()) / "library.json"
        out.write_text(json.dumps({"exchanges": [{"kind": "day", "band": "any", "lines": [["a", "{day}"], ["b", "Aye."]]}]}))
        made = L.write(L.Fixture(), None, ["gossip", "day"], 2, json.loads(out.read_text())["exchanges"])
        self.assertEqual(len(L.BANDS["gossip"]) * len(L.TONES) + len(L.BANDS["day"]) * len(L.TONES), len(made),
                         "One good exchange a call kept; the one with a name of its own refused")
        self.assertTrue(all(m["lines"][0][1].startswith(("They say", "{day}")) for m in made))
        again = L.write(L.Fixture(), None, ["gossip"], 2, made)
        self.assertEqual([], again, "Nothing already in the library is added twice")


if __name__ == "__main__":
    unittest.main()
