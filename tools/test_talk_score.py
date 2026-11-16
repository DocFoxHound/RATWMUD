"""The scorecard's gate check (tools/talk_score.py check(); Docs/Design/84 §15.3, §15.8): a real regression is caught, and
noise (the same events over a little more or less talk, a slower machine) is not; and the metrics, the card and the
listening page that the slice-end review found wanting (C3, T3, D1, the separation's direction, a failed run, the ids)."""
import copy
import json
import math
import os
import random
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import talk_score  # noqa: E402


def card(c6=None, c3=None, b5=60.0, r5=None, lines=(30000, 6000), convs=(12000, 2400), hours=(100.0, 100.0)):
    """A two-profile card as talk_score.scorecard() writes it, from each profile's events: C6 repeats, C3 good endings,
    R5 collisions (both profiles' events given as a pair)."""
    profiles = ("L1", "L2")
    details = {p: {"_counts": {"lines": lines[i], "conversations": convs[i], "hours": hours[i]}} for i, p in enumerate(profiles)}
    weight = sum(lines)

    def row(mid, per_profile):
        value = sum(v * lines[i] for i, v in enumerate(per_profile)) / weight
        name, better, gate, _test, target, _phase = talk_score.METRICS[mid]
        return {"name": name, "better": better, "gate": gate, "target": target, "value": value,
                "passes": talk_score.passes(mid, value), "by_profile": {p: round(per_profile[i], 3) for i, p in enumerate(profiles)}}

    metrics = {}
    if c6 is not None:
        metrics["C6"] = row("C6", [c6[i] * 1000 / convs[i] for i in range(2)])
    if c3 is not None:
        metrics["C3"] = row("C3", [c3[i] * 100 / convs[i] for i in range(2)])
    if r5 is not None:
        metrics["R5"] = row("R5", [r5[i] * 100 / hours[i] for i in range(2)])
    metrics["B5"] = row("B5", [b5, b5])
    metrics["B5"]["value"] = b5
    return {"metrics": metrics, "details": details}


class Check(unittest.TestCase):
    def test_same_events_over_other_line_counts_is_not_worse(self):
        # Slice 1's end: C6 3 against 3, but the profiles' line counts moved, so the weighted mean rose a little.
        base = card(c6=(3, 0), lines=(31247, 35404))
        now = card(c6=(3, 0), lines=(31840, 34780))
        self.assertGreater(now["metrics"]["C6"]["value"], base["metrics"]["C6"]["value"])
        self.assertEqual(talk_score.check(now, base), [])

    def test_more_repeats_on_an_open_gate_is_worse(self):
        # (Nine against three is a 7% chance under no change: the exact test of the two counts doesn't call it; fifteen,
        # 0.4%, it does.)
        problems = talk_score.check(card(c6=(15, 0)), card(c6=(3, 0)))
        self.assertEqual(len(problems), 1)
        self.assertIn("C6", problems[0])
        self.assertIn("open hard gate", problems[0])
        self.assertIn("15 events against 3", problems[0])

    def test_a_handful_fewer_of_a_rare_soft_event_is_noise(self):
        # C3 frozen at Slice 1's end: 18 good endings against 22 in 26,142 conversations.
        self.assertEqual(talk_score.check(card(c3=(18, 0)), card(c3=(22, 0))), [])

    def test_a_soft_metric_falling_by_far_more_than_its_noise_is_caught(self):
        problems = talk_score.check(card(c3=(200, 40)), card(c3=(400, 80)))
        self.assertEqual(len(problems), 1)
        self.assertIn("regresses more than 10%", problems[0])

    def test_collisions_within_their_noise_and_beyond_it(self):
        base = card(r5=(2100, 56))
        self.assertEqual(talk_score.check(card(r5=(2110, 56)), base), [])
        self.assertEqual(len(talk_score.check(card(r5=(2400, 56)), base)), 1)

    def test_a_timing_moves_with_the_machine(self):
        base = card(b5=67.6)
        self.assertEqual(talk_score.check(card(b5=59.2), base), [])
        self.assertEqual(len(talk_score.check(card(b5=40.0), base)), 1)

    def test_a_passed_hard_gate_still_breaks_on_one_event(self):
        base = card(c6=(0, 0))
        self.assertTrue(base["metrics"]["C6"]["passes"])
        problems = talk_score.check(card(c6=(1, 0)), base)
        self.assertEqual(len(problems), 1)
        self.assertIn("the baseline passed", problems[0])

    def test_a_card_without_counts_compares_values_as_before(self):
        base, now = card(c6=(3, 0)), card(c6=(4, 0))
        for c in (base, now):
            c.pop("details")
        self.assertEqual(len(talk_score.check(now, base)), 1)
        same = copy.deepcopy(base)
        self.assertEqual(talk_score.check(same, base), [])


    def test_noise_alone_rarely_fails_a_card(self):
        # The slice-end review: at one standard deviation, Poisson noise at Slice 1's counts failed two cards in three.
        # Both cards drawn afresh (the baseline is a run too), at today's counts: R5 frozen and fixtures, C6, C4, T2.
        rng = random.Random(5)

        def poisson(lam):
            if lam > 50:
                return max(0, int(round(rng.gauss(lam, math.sqrt(lam)))))
            k, p, limit = 0, 1.0, math.exp(-lam)
            while True:
                p *= rng.random()
                if p <= limit:
                    return k
                k += 1

        rates = {"R5": 2166, "C6": 3, "C4": 1, "T2": 16, "K3": 109}
        flagged = 0
        trials = 1500
        for _ in range(trials):
            a, b = ({m: poisson(lam) for m, lam in rates.items()} for _ in range(2))
            if talk_score.check(events_card(a), events_card(b)):
                flagged += 1
        self.assertLess(flagged / trials, .08, f"{flagged} of {trials} cards flagged on noise alone")

    def test_k3_and_r1_move_a_little_without_failing(self):
        # Slice 1's cards: K3 109 against 109 and R1 0.418125 against 0.418125, compared exactly before.
        self.assertEqual(talk_score.check(events_card({"K3": 110}), events_card({"K3": 109})), [])
        self.assertEqual(len(talk_score.check(events_card({"K3": 160}), events_card({"K3": 109}))), 1)
        r1 = lambda v: {"metrics": {"R1": row("R1", v)}, "runs": {}}
        self.assertEqual(talk_score.check(r1(0.417), r1(0.418125)), [])
        self.assertEqual(len(talk_score.check(r1(0.15), r1(0.418125))), 1)

    def test_a_profile_missing_from_the_card_is_a_problem(self):
        base, now = card(c6=(3, 0)), card(c6=(3, 0))
        base["runs"] = {"L1": "a", "L2": "b"}
        now["runs"] = {"L1": "a"}
        problems = talk_score.check(now, base)
        self.assertTrue(any("L2" in p and "hasn't" in p for p in problems), problems)

    def test_refreeze_takes_only_the_rows_named(self):
        base = card(c6=(3, 0), c3=(22, 0))
        now = card(c6=(0, 0), c3=(40, 0))
        out = talk_score.refreeze(base, now, ["C6"], "closed")
        self.assertEqual(out["metrics"]["C6"], now["metrics"]["C6"])
        self.assertEqual(out["metrics"]["C3"], base["metrics"]["C3"])
        self.assertEqual(out["refrozen"][0]["metrics"], ["C6"])
        self.assertEqual(len(talk_score.check(card(c6=(1, 0)), out)), 1)      # (Locked at 0 now: one repeat breaks it.)


def row(mid, value, events=None):
    name, better, gate, _test, target, _phase = talk_score.METRICS[mid]
    r = {"name": name, "better": better, "gate": gate, "target": target, "value": value,
         "passes": talk_score.passes(mid, value), "by_profile": {"L1": value}}
    if events is not None:
        r["events"] = events
    return r


def events_card(counts: dict) -> dict:
    """A one-profile card from each metric's events (exposures fixed at Slice 1's: 230 h, 31,000 lines, 13,000 talks)."""
    exposure = {"R5": 230.0, "C6": 13000.0, "C4": 31000.0, "T2": 31000.0, "K3": 13000.0}
    per = {"R5": 100, "C6": 1000, "C4": 1000, "T2": 1000, "K3": None}
    metrics = {}
    for mid, n in counts.items():
        value = n if per[mid] is None else n * per[mid] / exposure[mid]
        r = row(mid, value, [n, exposure[mid]])
        r["passes"] = False                         # (Open gates, as Slice 1's are.)
        metrics[mid] = r
    return {"metrics": metrics, "runs": {"L1": "x"}}


# ------------------------------------------------------------------ The metrics, on transcripts written here

def write_run(d: Path, convs: list, residents: dict, profile="L1", run="frozen-L1"):
    """A talk_sim run folder: each conversation (its head, then its lines as (who, to, text)), and the residents."""
    d.mkdir(parents=True, exist_ok=True)
    (d / "run.json").write_text(json.dumps({"profile": profile, "run": run, "hours": 10}))
    (d / "residents.jsonl").write_text("".join(json.dumps({"id": k, **v}) + "\n" for k, v in residents.items()))
    heads, lines = [], []
    for i, (head, said) in enumerate(convs, 1):
        heads.append(json.dumps({"conv": i, "topic": "smalltalk", "reason": "done", **head}))
        for beat, (who, to, text) in enumerate(said):
            lines.append(json.dumps({"conv": i, "beat": beat, "t": i * 100 + beat, "who": who, "to": to, "text": text,
                                     "entry": f"e{i}.{beat}", "listener": "sim", "cell": "c", "accent": "none",
                                     "dialect": "ridgemere", "used": []}))
    (d / "convs.jsonl").write_text("\n".join(heads) + "\n")
    (d / "lines.jsonl").write_text("\n".join(lines) + "\n")
    return d


PEOPLE = {"jory": {"name": "Jory Rimepeak"}, "varg": {"name": "Varg Acorndell"}, "wren": {"name": "Wren Tarford"}}


class Metrics(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def score(self, convs, residents=PEOPLE):
        return talk_score.score_run(talk_score.Run(write_run(self.root / f"r{len(list(self.root.iterdir()))}", convs, residents)))

    def test_t3_the_listener_in_the_third_person(self):
        m = self.score([({"teller": "jory", "other": "varg"}, [("jory", "varg", "Varg Acorndell snores like a mill.")])])
        self.assertEqual(m["T3_detail"]["by_kind"], {"listener_third": 1})

    def test_t3_a_name_said_as_a_call_is_not_one(self):
        m = self.score([({"teller": "jory", "other": "varg"}, [("jory", "varg", "New pup on your round, Varg Acorndell."),
                                                                ("jory", "varg", "Varg Acorndell, a word?"),
                                                                ("jory", "varg", "Between us, Varg Acorndell: Wren was robbed.")])])
        self.assertEqual(m["T3"], 0)

    def test_t3_own_name_and_an_unbacked_eyewitness(self):
        m = self.score([
            ({"teller": "wren", "other": "varg"}, [("wren", "varg", "Wren Tarford was robbed.")]),
            ({"teller": "wren", "other": "varg", "topic": "gossip", "seen": "no"},
             [("wren", "varg", "I saw Jory Rimepeak take it with my own eyes.")]),
            ({"teller": "wren", "other": "varg", "topic": "gossip", "seen": "yes"},
             [("wren", "varg", "I saw Jory Rimepeak take it."), ("varg", "wren", "I saw it too, I swear.")]),
        ])
        self.assertEqual(m["T3_detail"]["by_kind"], {"own_name": 1, "unbacked": 2})

    def test_c3_a_talk_cut_off_is_not_a_good_ending(self):
        m = self.score([({"reason": "gone"}, [("jory", "varg", "Hm.")]),
                        ({}, [("jory", "varg", "Well, farewell.")]),
                        ({}, [("jory", "varg", "Hm.")]),
                        ({"marked": True}, [("jory", "varg", "Hold that thought.")])])
        self.assertEqual(m["C3"], 50.0)
        self.assertEqual(m["C3_detail"]["cut_off"], 25.0)

    def with_fields(self, d: Path, fields: dict) -> Path:
        """The run's lines with more of what talk_sim writes (moves, tone, fit...), by (conversation, beat)."""
        lines = [json.loads(l) for l in (d / "lines.jsonl").read_text().splitlines()]
        for l in lines:
            l.update(fields.get((l["conv"], l["beat"]), {}))
        (d / "lines.jsonl").write_text("".join(json.dumps(l) + "\n" for l in lines))
        return d

    def test_c3_a_farewell_by_its_moves_or_a_dialects_words(self):
        # Phase 3: "And you." takes leave by its move; "Root well." and "Good harvest, Varg." by the dialects' words;
        # "A good harvest this year." and an ack don't.
        d = write_run(self.root / "c3m", [({}, [("jory", "varg", "Farewell."), ("varg", "jory", "And you.")]),
                                          ({}, [("jory", "varg", "Root well.")]),
                                          ({}, [("jory", "varg", "Good harvest, Varg.")]),
                                          ({}, [("jory", "varg", "A good harvest this year.")]),
                                          ({"ended": "cap"}, [("jory", "varg", "Right.")])], PEOPLE)
        self.with_fields(d, {(1, 1): {"moves": ["farewell.return"]}, (5, 0): {"moves": ["ack"]}})
        m = talk_score.score_run(talk_score.Run(d))
        self.assertEqual(m["C3"], 60.0)
        self.assertEqual(m["C3_detail"]["cap"], 20.0)

    def test_c1_by_moves_and_a_move_of_its_own_is_no_kind(self):
        # A farewell owes farewell.return: met by it, not by another farewell; react.news is met by a variant of it.
        d = write_run(self.root / "c1", [({}, [("jory", "varg", "Farewell."), ("varg", "jory", "Farewell to you.")]),
                                         ({}, [("jory", "varg", "Farewell."), ("varg", "jory", "And you.")]),
                                         ({}, [("jory", "varg", "Wren's wed."), ("varg", "jory", "I heard.")])], PEOPLE)
        self.with_fields(d, {(1, 0): {"moves": ["farewell"], "owes": ["farewell.return"]}, (1, 1): {"moves": ["farewell"]},
                             (2, 0): {"moves": ["farewell"], "owes": ["farewell.return"]}, (2, 1): {"moves": ["farewell.return"]},
                             (3, 0): {"moves": ["tell.news"], "owes": ["react.news"]}, (3, 1): {"moves": ["react.news.known"]}})
        m = talk_score.score_run(talk_score.Run(d))
        self.assertAlmostEqual(m["C1"], 200 / 3)
        self.assertFalse(talk_score.meets("farewell.return", "farewell", talk_score.move_book()))
        self.assertTrue(talk_score.meets("react.news.known", "react.news", talk_score.move_book()))

    def test_c5_c9_and_k1_phase_5(self):
        # C5: a line on another topic than the one before it, with no pivot, change of subject or return among its moves;
        # C9: lines standing on the talk ledger (a callback, a return, "Back again?") per 10 talks; K1: the topics
        # topics.json calls everyday (smalltalk is, gossip and player aren't).
        d = write_run(self.root / "c5", [({"topic": "smalltalk"}, [("jory", "varg", "Fine day."), ("varg", "jory", "It is."),
                                                                   ("jory", "varg", "Anyway. Prices!"), ("varg", "jory", "Wagons late.")]),
                                         ({"topic": "gossip"}, [("jory", "varg", "Back again? Heard about Wren?")]),
                                         ({"topic": "player"}, [("jory", "varg", "Who's that?")])], PEOPLE)
        self.with_fields(d, {(1, 0): {"topic_now": "smalltalk"}, (1, 1): {"topic_now": "smalltalk"},
                             (1, 2): {"topic_now": "prices", "moves": ["change_subject", "tell.complaint"]},
                             (1, 3): {"topic_now": "caravan", "callback": True},
                             (2, 0): {"topic_now": "gossip", "moves": ["again.open", "tell.gossip"]}, (3, 0): {"topic_now": "player"}})
        m = talk_score.score_run(talk_score.Run(d))
        self.assertAlmostEqual(m["C5"], 1000 / 6)
        self.assertEqual(m["C5_detail"]["turns"], 2)
        self.assertAlmostEqual(m["C9"], 2 * 10 / 3)
        if talk_score.EVERYDAY is not None:
            self.assertAlmostEqual(m["K1"], 100 / 3)
        old = write_run(self.root / "old", [({"topic": "smalltalk"}, [("jory", "varg", "Fine day.")])], PEOPLE)
        m = talk_score.score_run(talk_score.Run(old))
        self.assertNotIn("C5", m)

    def test_t2_judges_a_line_at_its_own_hour(self):
        # Phase 5: a talk begun by day ("here.time": day at its head) that closes at dusk says "Good night." truly.
        d = write_run(self.root / "t2h", [({"facts": {"here.time": "day", "here.hour": "17"}},
                                           [("jory", "varg", "Fine evening."), ("varg", "jory", "Good night.")]),
                                          ({"facts": {"here.time": "day", "here.hour": "12"}}, [("jory", "varg", "Good night.")])], PEOPLE)
        self.with_fields(d, {(1, 1): {"hour": 18.2}, (2, 0): {"hour": 12.1}})
        m = talk_score.score_run(talk_score.Run(d))
        self.assertEqual(m["T2_detail"]["by_kind"], {"night_greeting": 1})

    def test_t5_told_what_they_knew_without_a_known_answer(self):
        # Phase 4: a claim passed on to one who held it before is fine answered "I'd heard"; not with "Never!"; one told it
        # fresh is judged and fine; a line with no to_knew isn't judged.
        d = write_run(self.root / "t5", [({}, [("jory", "varg", "Wren's wed."), ("varg", "jory", "I'd heard.")]),
                                         ({}, [("jory", "varg", "Wren's wed."), ("varg", "jory", "Never!")]),
                                         ({}, [("jory", "varg", "Wren's wed."), ("varg", "jory", "Never!")]),
                                         ({}, [("jory", "varg", "Wren's wed."), ("varg", "jory", "Never!")])], PEOPLE)
        self.with_fields(d, {(1, 0): {"asserts": ["marriage"], "to_knew": "yes"}, (1, 1): {"moves": ["react.news.known"]},
                             (2, 0): {"asserts": ["marriage"], "to_knew": "yes"}, (2, 1): {"moves": ["react.news.new"]},
                             (3, 0): {"asserts": ["marriage"], "to_knew": "no"}, (3, 1): {"moves": ["react.news.new"]},
                             (4, 0): {"asserts": ["marriage"]}})
        m = talk_score.score_run(talk_score.Run(d))
        self.assertAlmostEqual(m["T5"], 100 / 3)
        self.assertEqual(m["T5_detail"]["asserted"], 3)

    def test_b1_the_slowest_profile_and_its_noise(self):
        # Phase 4: B1 from talk_sim's run.json; a card's is the slowest profile's; a timing past its gate breaks it only
        # beyond its share of noise.
        d = write_run(self.root / "b1", [({}, [("jory", "varg", "Hm.")])], PEOPLE)
        meta = json.loads((d / "run.json").read_text())
        meta.update({"beat_ms": {"n": 10, "mean": .05, "p99": .2, "max": .4}, "look_ms": {"n": 5, "mean": .6, "p99": 3.5}})
        (d / "run.json").write_text(json.dumps(meta))
        m = talk_score.score_run(talk_score.Run(d))
        self.assertEqual((m["B1"], m["B1P"], m["B1L"]), (.05, .2, 3.5))
        self.assertEqual(talk_score.combine({"a": {**m, "_counts": {"lines": 1, "conversations": 1}},
                                             "b": {**m, "B1L": 4.0, "_counts": {"lines": 9, "conversations": 1}}})["B1L"], 4.0)
        base = {"metrics": {"B1L": row("B1L", 3.5)}}
        self.assertEqual(talk_score.check({"metrics": {"B1L": row("B1L", 5.2)}}, base), [])
        self.assertEqual(len(talk_score.check({"metrics": {"B1L": row("B1L", 6.0)}}, base)), 1)
        # (A hard timing gate the baseline passed breaks only beyond its noise too: B5's, say.)
        hard = {"metrics": {"B5": row("B5", 160.0)}}
        self.assertEqual(talk_score.check({"metrics": {"B5": row("B5", 140.0)}}, hard), [])
        self.assertEqual(len(talk_score.check({"metrics": {"B5": row("B5", 100.0)}}, hard)), 1)

    def test_c4_by_tone_and_a_bridge(self):
        d = write_run(self.root / "c4", [({}, [("jory", "varg", "Old Tam passed."), ("varg", "jory", "Lovely weather.")]),
                                         ({}, [("jory", "varg", "Old Tam passed."), ("varg", "jory", "Well."),
                                               ("varg", "jory", "Lovely weather.")])], PEOPLE)
        self.with_fields(d, {(1, 0): {"tone": "grave"}, (1, 1): {"tone": "light"}, (2, 0): {"tone": "grave"},
                             (2, 1): {"moves": ["assess"]}, (2, 2): {"tone": "light"}})
        m = talk_score.score_run(talk_score.Run(d))
        self.assertEqual(m["C4_detail"], {"clashes": 1, "by_tone": 1})

    def test_r7_and_x1_leave_the_closings_out_and_c2_is_the_best_fit(self):
        convs = [({}, [("jory", "varg", f"News {i}."), ("varg", "jory", f"Answer {i}."), ("jory", "varg", "Farewell."),
                       ("varg", "jory", "And you.")]) for i in range(1, 11)]
        d = write_run(self.root / "r7", convs, PEOPLE)
        fields = {}
        for i in range(1, 11):
            fields[(i, 1)] = {"entry": f"answer{i}", "fit": 3 if i > 1 else 1, "best": 3}
            fields[(i, 2)] = {"entry": "bye", "moves": ["farewell"]}
            fields[(i, 3)] = {"entry": "bye_back", "moves": ["farewell.return"]}
        m = talk_score.score_run(talk_score.Run(self.with_fields(d, fields)))
        self.assertEqual(m["R7"], 1000.0)                 # (Ten answers, ten entries; the closings apart.)
        self.assertEqual(m["R7_detail"]["closing_beats"], 20)
        self.assertEqual(m["C2"], 90.0)

    def test_d3_isnt_judged_where_one_dialect_was_heard(self):
        lines = [{"who": f"w{i % 3}", "text": "aye", "dialect": "ridgemere", "accent": "light"} for i in range(90)]
        value, detail = talk_score.guess_dialects(lines)
        self.assertIsNone(value)
        self.assertEqual(detail["judged"], 0)

    def test_r8_and_d3_a_speaker_or_a_few_lines_are_noise(self):
        # Phase 3's frozen card: habit lines 48 against 79 over 66,742 and 95,071 lines; D3 one speaker of seven.
        def r8(n, lines):
            c = {"metrics": {"R8": row("R8", n * 100 / lines)}, "details": {"L1": {"_counts": {"lines": lines, "conversations": 1},
                                                                                     "R8_detail": {"lines20": lines}}}}
            c["metrics"]["R8"]["passes"] = True
            return c
        self.assertEqual(talk_score.check(r8(79, 95071), r8(48, 66742)), [])
        self.assertEqual(len(talk_score.check(r8(300, 95071), r8(48, 66742))), 1)

        def d3(right, judged):
            c = {"metrics": {"D3": row("D3", right * 100 / judged)}, "details": {"L1": {"_counts": {"lines": 9000, "conversations": 1},
                                                                                       "D3_detail": {"judged": judged}}}}
            return c
        self.assertEqual(talk_score.check(d3(2, 7), d3(3, 7)), [])
        self.assertEqual(len(talk_score.check(d3(20, 100), d3(80, 100))), 1)

    def test_d1_gates_the_plain_speakers(self):
        d = write_run(self.root / "d1", [({}, [("jory", "varg", "Aye.")])], PEOPLE)
        lines = [json.loads(l) for l in (d / "lines.jsonl").read_text().splitlines()]
        lines[0]["used"] = ["ridgemere"]
        (d / "lines.jsonl").write_text(json.dumps(lines[0]) + "\n")
        m = talk_score.score_run(talk_score.Run(d))
        self.assertEqual(m["D1"], 100.0)
        self.assertFalse(talk_score.passes("D1", m["D1"]))
        self.assertEqual(m["_counts"]["none_lines"], 1)

    def test_t4_a_stance_that_wanders_or_answers_against_itself(self):
        d = write_run(self.root / "t4", [({}, [("jory", "varg", "The Watch? Useless."), ("varg", "jory", "No argument from me.")]),
                                         ({}, [("jory", "varg", "The Watch keep us safe."), ("varg", "jory", "Well. Each to their own.")])],
                      PEOPLE)
        lines = [json.loads(l) for l in (d / "lines.jsonl").read_text().splitlines()]
        stance = [("against", ["give.opinion"], 0), ("for", ["agree"], 0), ("for", ["give.opinion"], 0), ("against", ["disagree"], 0)]
        for l, (side, moves, drift) in zip(lines, stance):
            l.update({"issue": "watch", "stance": side, "moves": moves, "drift": drift, "day": 1})
        (d / "lines.jsonl").write_text("".join(json.dumps(l) + "\n" for l in lines))
        m = talk_score.score_run(talk_score.Run(d))
        # Jory: against, then for with no event between: drift. Varg: agreed with one on the other side; then for to against.
        self.assertEqual(m["T4_detail"]["drifted"], 2)
        self.assertEqual(m["T4_detail"]["answered_against_own"], 1)
        lines[2]["drift"] = 1                       # (An event moved Jory's: not drift.)
        lines[3]["stance"] = "for"                  # (Varg for, said a disagree to a for: against his own.)
        lines[1]["stance"] = "against"              # (Varg against, agreeing with an against: his own; then for: drift.)
        (d / "lines.jsonl").write_text("".join(json.dumps(l) + "\n" for l in lines))
        m = talk_score.score_run(talk_score.Run(d))
        self.assertEqual(m["T4_detail"]["drifted"], 1)
        self.assertEqual(m["T4_detail"]["answered_against_own"], 1)
        self.assertGreater(m["T4"], 0)
        self.assertFalse(talk_score.passes("T4", m["T4"]))

    def test_t6_they_of_one_known_wolf(self):
        m = self.score([({}, [("jory", "varg", "Wren Tarford was robbed. Have you seen their face?"),
                              ("jory", "varg", "Wren Tarford was robbed. Did you see her face?"),
                              ("jory", "varg", "They say Wren Tarford left in a hurry."),
                              ("jory", "varg", "Wren Tarford and Varg Acorndell wed, and they're happy.")])])
        self.assertEqual(m["T6_detail"]["lines"], 1)

    def test_d1_by_the_accent_spoken(self):
        d = write_run(self.root / "d1s", [({}, [("jory", "varg", "Calm water, sister."), ("jory", "varg", "Aye.")])], PEOPLE)
        lines = [json.loads(l) for l in (d / "lines.jsonl").read_text().splitlines()]
        lines[0].update({"accent": "none", "spoken_accent": "light", "used": ["coldwater"]})   # (A step broader, home speech.)
        lines[1].update({"accent": "light", "spoken_accent": "none"})
        (d / "lines.jsonl").write_text("".join(json.dumps(l) + "\n" for l in lines))
        m = talk_score.score_run(talk_score.Run(d))
        self.assertEqual(m["D1"], 0.0)              # (Said plainly: no dialect word.)
        self.assertEqual(m["D1L"], 100.0)
        self.assertEqual(m["D1A"], 100.0)
        self.assertEqual(m["D1A_detail"], {"broader": 1, "plainer": 1})

    def test_a_coherence_metric_the_shuffle_beats_does_not_separate(self):
        # Slice 1's frozen C3: 0.068% real against 0.177% shuffled, 61% apart, the wrong way.
        self.assertFalse(talk_score.real_better("C3", 0.068, 0.177))
        self.assertTrue(talk_score.real_better("C4", 0.0, 0.2))


class Card(unittest.TestCase):
    """tools/talk_card.py with a stand-in talk_sim that fails: the card fails, and nothing an earlier run left is scored."""

    def test_a_failed_run_fails_the_card_and_old_files_are_not_scored(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            build = root / "build"
            build.mkdir()
            sim = build / "talk_sim"
            sim.write_text("#!/bin/sh\necho 'talk_sim: the world did not start: no database' >&2\nexit 1\n")
            sim.chmod(0o755)
            fixtures = root / "fx"
            fixtures.mkdir()
            (fixtures / "fixture.json").write_text(json.dumps({"checkpoints": [{"day": 27.8}, {"day": 28.0}]}))
            out = root / "out"
            stale = write_run(out / "frozen-L1", [({}, [("jory", "varg", "Hm.")])], PEOPLE)
            env = dict(os.environ, RATW_DATABASE_URL="postgresql:///nowhere")
            p = subprocess.run([sys.executable, str(Path(__file__).resolve().parent / "talk_card.py"), "--fixtures", str(fixtures),
                                "--out", str(out), "--modes", "frozen", "--profiles", "L1", "--build", str(build),
                                "--baseline", str(root / "none.json")], capture_output=True, text=True, env=env)
            self.assertEqual(p.returncode, 1, p.stderr)
            self.assertIn("failed (rc 1)", p.stderr)
            self.assertFalse((stale / "run.json").exists(), "the earlier run's transcripts were left to be scored")
            self.assertFalse((out / "scorecard-frozen.json").exists())

    def test_a_missing_fixture_is_said_plainly(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = subprocess.run([sys.executable, str(Path(__file__).resolve().parent / "talk_card.py"), "--fixtures",
                                str(Path(tmp) / "nothing"), "--out", str(Path(tmp) / "out")], capture_output=True, text=True,
                               env=dict(os.environ, RATW_DATABASE_URL="x"))
            self.assertNotEqual(p.returncode, 0)
            self.assertIn("no fixture", p.stderr)
            self.assertNotIn("Traceback", p.stderr)


class Listen(unittest.TestCase):
    def test_two_modes_of_one_profile_keep_their_marks_apart(self):
        import talk_listen
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            a = write_run(root / "frozen-L1", [({}, [("jory", "varg", "Hm.")])], PEOPLE, run="frozen-L1")
            b = write_run(root / "fixtures-L1", [({}, [("jory", "varg", "Hm.")])], PEOPLE, run="fixtures-L1")
            runs = [talk_score.Run(a), talk_score.Run(b)]
            talk_listen.labels(runs)
            page = talk_listen.render_html(list(talk_listen.conversations(runs)), "t", "s")
            self.assertIn('data-id="frozen-L1-1"', page)
            self.assertIn('data-id="fixtures-L1-1"', page)


if __name__ == "__main__":
    unittest.main()
