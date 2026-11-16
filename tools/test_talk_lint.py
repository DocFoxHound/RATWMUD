"""The truth lint's own tests (tools/talk_lint.py, tools/talk_truth.py; Docs/Design/84 §7.8)."""
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import talk_lint  # noqa: E402
import talk_truth  # noqa: E402

TALK = Path(__file__).resolve().parent.parent / 'Data' / 'Talk'

LIBRARY = """
scene t_fog_bare
when topic=lore here.region=ridgemere
a : Fog bell rang all night.
b : I counted the strokes.

scene t_fog_backed
when topic=lore here.region=ridgemere here.outdoors=fog
a : Fog bell rang all night.
b : I counted the strokes.

scene t_nets
when topic=work
a : My nets are tangled past saving.
b[fisher] : My nets too.

rule t_rain_react
when concept=react.rain
say It's raining. Of course it is.

rule t_id
when concept=talk.day
say Word is camp_w_ridgemere_6_8 is back.
"""


class Lint(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        for name in ('facts.json', 'truth.json', 'groups.talk', 'settings.json'):
            shutil.copy(TALK / name, self.root / name)
        (self.root / 'dialogue').mkdir()
        (self.root / 't.talk').write_text(LIBRARY)

    def tearDown(self):
        shutil.rmtree(self.root)

    def test_findings(self):
        found = {(f['rule'], f['entry'], f['text']) for f in talk_lint.findings(self.root)}
        self.assertIn(('fog_now', 't_fog_bare', 'Fog bell rang all night.'), found)
        self.assertNotIn(('fog_now', 't_fog_backed', 'Fog bell rang all night.'), found)
        self.assertIn(('fisher_self', 't_nets', 'My nets are tangled past saving.'), found)      # (a: anyone)
        self.assertNotIn(('fisher_self', 't_nets', 'My nets too.'), found)                       # (b[fisher])
        self.assertNotIn(('rain_now', 't_rain_react', "It's raining. Of course it is."), found)  # (react.rain holds rain)
        self.assertIn(('ids', 't_id', 'Word is camp_w_ridgemere_6_8 is back.'), found)

    def test_runtime(self):
        rules, _ = talk_truth.load(TALK)
        fog = next(r for r in rules if r.id == 'fog_now')
        self.assertIs(fog.judge({'here.weather': 'indoors', 'here.outdoors': 'clear'}), False)
        self.assertIs(fog.judge({'here.weather': 'indoors', 'here.outdoors': 'fog'}), True)
        self.assertIsNone(fog.judge({}))
        fish = next(r for r in rules if r.id == 'fishing_town')
        self.assertIs(fish.judge({'self.job': 'merchant', 'here.fishers': '1'}), False)
        self.assertIs(fish.judge({'self.job': 'merchant', 'here.fishers': '7'}), True)


if __name__ == '__main__':
    unittest.main()
