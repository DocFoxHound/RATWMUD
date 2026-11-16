"""The scorecard's gate check (tools/talk_score.py check(); Docs/Design/84 §15.3, §15.8): a real regression is caught, and
noise (the same events over a little more or less talk, a slower machine) is not."""
import copy
import sys
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
        problems = talk_score.check(card(c6=(9, 0)), card(c6=(3, 0)))
        self.assertEqual(len(problems), 1)
        self.assertIn("C6", problems[0])
        self.assertIn("open hard gate", problems[0])
        self.assertIn("9 events against 3", problems[0])

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


if __name__ == "__main__":
    unittest.main()
