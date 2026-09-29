#!/usr/bin/env python3
"""The NPC Mind (npc_mind.py), offline: no provider is ever called. The history test uses a scratch database."""
from __future__ import annotations

import http.client
import json
import secrets
import threading
import unittest

import npc_mind as mind
from npc_bridge import BridgeError

CONTEXT = {"npc": "Wren", "player": "Ash", "heard": "Thank you for the bread.", "description": "A baker wolf.",
           "activity": "Baking.", "memory": "", "scene": "A warm bakery.", "personality": "Kind and wry.",
           "backstory": "Came up the river road.", "npcId": "wren", "subjectId": "player-ash",
           "relationship": "You know Ash and like them.", "mood": "warm"}


def reply(**changes):
    value = {"speech": "Mind the crust.", "emotion": "warm", "affinity": 1, "trust": 0, "remember": "",
             "promise_by": "none", "promise": ""}
    value.update(changes)
    return value


class Recording:
    """A provider that remembers what it was asked and answers as told."""

    def __init__(self, answer=None):
        self.calls, self.answer = [], answer or reply()

    def complete(self, system, user, name, schema, max_tokens, timeout):
        self.calls.append({"system": system, "user": json.loads(user), "name": name})
        return (self.answer(self.calls[-1]) if callable(self.answer) else self.answer), {"total_tokens": 7}


class DecodingTests(unittest.TestCase):
    def test_a_good_reply(self):
        got = mind.decode_dialogue(reply(promise_by="npc", promise="I will save you a loaf."))
        self.assertEqual({"text": "Mind the crust.", "emotion": "warm", "affinity": 1, "trust": 0, "remember": "",
                          "promise": {"by": "npc", "what": "I will save you a loaf."}}, got)

    def test_feelings_are_clamped(self):
        got = mind.decode_dialogue(reply(affinity=40, trust=-9))
        self.assertEqual((3, -3), (got["affinity"], got["trust"]))

    def test_long_speech_is_cut_at_a_sentence(self):
        got = mind.decode_dialogue(reply(speech="A short one. " * 80))
        self.assertLessEqual(len(got["text"]), mind.MAX_SPEECH)
        self.assertTrue(got["text"].endswith("."))

    def test_bad_replies_are_refused(self):
        for bad in (reply(speech=""), reply(affinity="3"), reply(affinity=1.5), reply(promise_by="someone"),
                    {**reply(), "extra": 1}, {"speech": "only"}, "not an object", reply(speech="bell\x07")):
            with self.assertRaises(BridgeError, msg=repr(bad)):
                mind.decode_dialogue(bad)

    def test_an_empty_promise_is_no_promise(self):
        self.assertIsNone(mind.decode_dialogue(reply(promise_by="player", promise="  "))["promise"])

    def test_notes_are_one_short_line(self):
        got = mind.decode_dialogue(reply(remember="Line one\nline two " + "x" * 400))
        self.assertNotIn("\n", got["remember"])
        self.assertLessEqual(len(got["remember"]), mind.MAX_NOTE)

    def test_context_is_checked(self):
        for bad in ({**CONTEXT, "heard": ""}, {**CONTEXT, "npc": " "}, {**CONTEXT, "relationship": "x" * 2000},
                    {**CONTEXT, "mood": 3}, []):
            with self.assertRaises(BridgeError):
                mind.clean_dialogue_context(bad)


class MindTests(unittest.TestCase):
    def test_identity_first_and_the_scene_after(self):
        provider = Recording()
        got = mind.Mind(provider, audit=lambda e: None).dialogue(CONTEXT)
        call = provider.calls[0]
        self.assertTrue(call["system"].startswith(mind.RULES))
        for lasting in ("You are Wren. A baker wolf.", "Personality: Kind and wry.", "Backstory: Came up the river road."):
            self.assertIn(lasting, call["system"])
        self.assertEqual("Thank you for the bread.", call["user"]["heard"])
        self.assertEqual("You know Ash and like them.", call["user"]["relationship"])
        for kept_out in ("backstory", "personality", "description", "npcId", "subjectId"):
            self.assertNotIn(kept_out, call["user"])
        self.assertEqual("Mind the crust.", got["text"])

    def test_history_joins_the_scene(self):
        class FakeHistory:
            def recent(self, npc_id, subject_id, npc_name, subject_name):
                return f"Day 3: {subject_name} paid you 6 pennies for 1 meal (resident food purchase)."
        provider = Recording()
        mind.Mind(provider, FakeHistory(), audit=lambda e: None).dialogue(CONTEXT)
        self.assertIn("Ash paid you 6 pennies", provider.calls[0]["user"]["history"])

    def test_a_broken_history_never_stops_a_reply(self):
        class Broken:
            def recent(self, *args):
                raise RuntimeError("database down")
        self.assertEqual("Mind the crust.", mind.Mind(Recording(), Broken(), audit=lambda e: None).dialogue(CONTEXT)["text"])

    def test_their_life_joins_the_scene(self):
        class FakeHistory:
            def recent(self, *args):
                return ""
            def life(self, npc_id):
                return "Spring 2, Year 1: You became apprentice to Rowan (Baker)." if npc_id == "wren" else ""
        provider = Recording()
        mind.Mind(provider, FakeHistory(), audit=lambda e: None).dialogue(CONTEXT)
        self.assertEqual("Spring 2, Year 1: You became apprentice to Rowan (Baker).", provider.calls[0]["user"]["life"])
        self.assertIn('"life" is what the NPC', provider.calls[0]["system"])
        mind.Mind(provider, FakeHistory(), audit=lambda e: None).dialogue(dict(CONTEXT, npcId="rowan"))
        self.assertNotIn("life", provider.calls[1]["user"], "Nothing lived, nothing sent")

    def test_a_broken_life_never_stops_a_reply(self):
        class Broken:
            def recent(self, *args):
                return ""
            def life(self, npc_id):
                raise RuntimeError("database down")
        self.assertEqual("Mind the crust.", mind.Mind(Recording(), Broken(), audit=lambda e: None).dialogue(CONTEXT)["text"])

    def test_a_life_is_read_once_a_while(self):
        reads, now = [], [0.0]

        class Conn:
            def __enter__(self):
                return self
            def __exit__(self, *a):
                return False
        import chronicle
        real = chronicle.load
        chronicle.load = lambda conn, npc, **kw: reads.append(npc) or chronicle.compile_chronicle(
            npc, [{"id": 1, "day": 1, "kind": "marriage", "actor": npc, "target": "sorrel"}], {"sorrel": "Sorrel"}, **{"second_person": True})
        try:
            history = mind.History(lambda: Conn(), clock=lambda: now[0])
            self.assertEqual("Spring 2, Year 1: You married Sorrel.", history.life("wren"))
            history.life("wren")
            now[0] += mind.History.LIFE_SECONDS + 1
            history.life("wren")
        finally:
            chronicle.load = real
        self.assertEqual(["wren", "wren"], reads)

    def test_a_story_from_a_chronicle_only(self):
        provider = Recording(lambda call: {"story": "Fennel came to Greyfen in spring.\n\nShe learned to bake."})
        got = mind.Mind(provider, audit=lambda e: None).story(
            {"name": "Fennel", "description": "A young baker.", "lines": ["Spring 1, Year 1: Fennel first appeared."] * 200})
        self.assertEqual({"story": "Fennel came to Greyfen in spring.\n\nShe learned to bake."}, got, "Paragraphs kept")
        call = provider.calls[0]
        self.assertEqual("npc_story", call["name"])
        self.assertIn("Never invent events", call["system"])
        self.assertEqual(mind.MAX_STORY_LINES, len(call["user"]["lines"]))
        for bad in ({"name": "Fennel", "lines": []}, {"lines": ["x"]}, {"name": "Fennel", "lines": "x"}, []):
            with self.assertRaises(BridgeError):
                mind.Mind(provider, audit=lambda e: None).story(bad)
        for reply_ in ({"story": ""}, {"story": 3}, {"story": "x", "extra": 1}):
            with self.assertRaises(BridgeError):
                mind.decode_story(reply_)
        self.assertLessEqual(len(mind.decode_story({"story": "x" * 10_000})["story"]), mind.MAX_STORY)
        fixture = mind.Mind(mind.FixtureProvider(), audit=lambda e: None).story({"name": "Fennel", "lines": ["Day: A.", "Day: B."]})
        self.assertEqual("The chronicle of Fennel holds 2 entries. A. B.", fixture["story"])

    def test_budgets(self):
        now = [0.0]
        budget = mind.Budget(per_hour=3, per_speaker_minute=2, clock=lambda: now[0])
        m = mind.Mind(Recording(), budget=budget, audit=lambda e: None)
        m.dialogue(CONTEXT)
        m.dialogue(CONTEXT)
        with self.assertRaises(BridgeError) as caught:
            m.dialogue(CONTEXT)
        self.assertEqual("budget_exhausted", caught.exception.code, "Two a minute from one speaker")
        m.dialogue({**CONTEXT, "subjectId": "player-birch"})
        with self.assertRaises(BridgeError):
            m.dialogue({**CONTEXT, "subjectId": "player-finch"})     # Three an hour in all.
        now[0] = 3601
        self.assertEqual("Mind the crust.", m.dialogue(CONTEXT)["text"], "An hour later there is room again")

    def test_busy_when_every_slot_is_taken(self):
        entered, release = threading.Event(), threading.Event()

        def slow(call):
            entered.set()
            release.wait(5)
            return reply()
        m = mind.Mind(Recording(slow), concurrency=1, wait=0.05, audit=lambda e: None)
        worker = threading.Thread(target=m.dialogue, args=(CONTEXT,))
        worker.start()
        entered.wait(5)
        with self.assertRaises(BridgeError) as caught:
            m.dialogue({**CONTEXT, "subjectId": "player-birch"})
        self.assertEqual("busy", caught.exception.code)
        release.set()
        worker.join(5)

    def test_the_audit_holds_no_words(self):
        entries = []
        mind.Mind(Recording(reply(speech="A secret recipe.")), audit=entries.append).dialogue(CONTEXT)
        text = json.dumps(entries)
        for private in ("A secret recipe", "Thank you for the bread", "Ash"):
            self.assertNotIn(private, text)
        self.assertEqual("success", entries[0]["outcome"])

    def test_summaries(self):
        got = mind.Mind(mind.FixtureProvider(), audit=lambda e: None).summarize(
            {"npc": "Wren", "turns": [{"who": "Ash", "text": "I promise to pay you back."}, {"who": "wren", "text": "Good."}]})
        self.assertIn("Ash said", got["summary"])
        with self.assertRaises(BridgeError):
            mind.Mind(mind.FixtureProvider(), audit=lambda e: None).summarize({"npc": "Wren", "turns": []})

    def test_the_fixture_makes_promises(self):
        got = mind.Mind(mind.FixtureProvider(), audit=lambda e: None).dialogue(
            {**CONTEXT, "heard": "I promise to bring the flour tomorrow."})
        self.assertEqual("player", got["promise"]["by"])
        self.assertEqual(1, got["trust"])


class HttpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        budget = mind.Budget(per_hour=100, per_speaker_minute=100)
        cls.server = mind.Server(0, mind.Mind(mind.FixtureProvider(), budget=budget, audit=lambda e: None))
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.port = cls.server.server_address[1]

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()

    def post(self, path, value, host=None, content_type="application/json"):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        body = json.dumps(value).encode()
        connection.request("POST", path, body=body, headers={"Host": host or f"127.0.0.1:{self.port}",
                                                             "Content-Type": content_type})
        response = connection.getresponse()
        data = json.loads(response.read())
        connection.close()
        return response.status, data

    def test_dialogue_and_summaries(self):
        status, data = self.post("/dialogue", CONTEXT)
        self.assertEqual(200, status)
        self.assertEqual({"text", "emotion", "affinity", "trust", "remember", "promise"}, set(data))
        status, data = self.post("/summarize", {"npc": "Wren", "turns": [{"who": "Ash", "text": "Hello."}]})
        self.assertEqual((200, True), (status, "summary" in data))

    def test_guards(self):
        self.assertEqual(403, self.post("/dialogue", CONTEXT, host="evil.example")[0])
        self.assertEqual(403, self.post("/elsewhere", CONTEXT)[0])
        self.assertEqual(403, self.post("/dialogue", CONTEXT, content_type="text/plain")[0])
        self.assertEqual(400, self.post("/dialogue", {"npc": "Wren"})[0])


class HistoryTests(unittest.TestCase):
    """Against a scratch database with the event log (skipped without the local PostgreSQL)."""

    def setUp(self):
        import test_world_db as T
        import world_db as W
        if not T.database_available():
            self.skipTest("local PostgreSQL not running")
        self.name = f"ratw_test_{secrets.token_hex(4)}"
        with T.superuser() as su:
            su.execute(f"CREATE DATABASE {self.name} OWNER ratw_owner")
            su.execute(f"GRANT CONNECT ON DATABASE {self.name} TO ratw_game")
        self.addCleanup(self.drop, T)
        with W.connect("dev", "owner", dbname=self.name) as owner:
            W.migrate(owner)
            owner.execute("INSERT INTO world.worlds (id, name) VALUES ('w', 'Test world')")
        self.game = lambda: W.connect("dev", "game", dbname=self.name)
        events = [{"kind": "economy", "actor": "player-ash", "target": "wren", "item": "meal", "quantity": 1,
                   "coins": 6, "day": 2.5, "detail": "resident food purchase"},
                  {"kind": "conversation", "actor": "player-ash", "target": "wren"},
                  {"kind": "economy", "actor": "player-birch", "target": "wren", "coins": 6}]
        with self.game() as game:
            game.execute("SELECT game.record_events('w', %s::jsonb)", (json.dumps(events),))

    def drop(self, T):
        with T.superuser() as su:
            su.execute(f"DROP DATABASE IF EXISTS {self.name} WITH (FORCE)")

    def test_a_pairs_dealings(self):
        text = mind.History(self.game).recent("wren", "player-ash", "Wren", "Ash")
        self.assertEqual("Day 3: Ash paid you 6 pennies for 1 meal (resident food purchase).", text,
                         "Only this pair, conversations left out (they are in memory already)")
        self.assertEqual("", mind.History(self.game).recent("wren", "", "Wren", ""))


if __name__ == "__main__":
    unittest.main()
