"""The trigger catalogue (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 4; Data/Storylines/trigger-catalogue.json):
every event, condition, action and effect described, with typed fields and a sentence, and the validator's tables read
from it saying what they said before (the effect checks compared with the hand-written ones they replaced, on every
parameter of every effect, with values in and out of bounds)."""

import itertools
import json
import random
import re
import unittest

import story_program as rules
import story_runner as runner
import story_triggers as triggers

CAT = rules.CATALOGUE
SLOT = re.compile(r'\{([^}]+)\}')


def legacy_validate_effect(effect, pid, regions=None, scenes=None):
    """validate_effect as it was written by hand before the catalogue (story_program.py at d3c7c1bf)."""
    require, integer, word, identifier = rules.require, rules.integer, rules.word, rules.identifier
    kind = effect.get('kind') if isinstance(effect, dict) else None
    require(isinstance(kind, str) and kind in rules.EFFECTS, f'{pid}: unknown effect.')
    rules.shape(effect, ('kind', *rules.EFFECTS[kind]))
    for k in rules.EFFECTS[kind]:
        v = effect[k]
        if k in ('x', 'y', 'inDays'):
            ok = integer(v, 0, 30 if k == 'inDays' else 4095)
        elif k == 'stance':
            ok = v in ('allied', 'friendly', 'neutral', 'tense', 'hostile', 'war')
        elif k == 'disposition':
            ok = integer(v, -100, 100)
        elif k == 'reason':
            ok = word(v, 300)
        elif k == 'count':
            ok = integer(v, 1, 6)
        elif k == 'skill':
            ok = v in ('craft', 'commerce', 'labour', 'service', 'travel', 'watch')
        elif k == 'region':
            ok = identifier(v) and v in (regions or {})
        elif k == 'pace':
            ok = v in ('walk', 'run')
        elif k == 'posture':
            ok = v in ('standing', 'sitting', 'lying')
        elif k == 'buff':
            ok = v == 'fed'
        elif k == 'quantity':
            ok = integer(v, 1, 100)
        elif k == 'hours':
            ok = integer(v, 1, 168)
        elif k == 'points' and kind == 'story.area.damage':
            ok = integer(v, 1, 99)
        elif k == 'points':
            ok = integer(v, -100, 100) and v != 0
        elif k == 'days':
            ok = integer(v, 1, 112 if kind == 'band.override' else 366)
        elif k == 'word' and kind == 'harvest.override':
            ok = v in ('poor', 'fair', 'good', 'clear')
        elif k == 'band':
            ok = v in ('destitute', 'poor', 'comfortable', 'rich', 'clear')
        elif k == 'add' and kind == 'character.injury':
            ok = v in rules.INJURY_TYPES
        elif k == 'severity':
            ok = integer(v, 1, 3)
        elif k in ('text', 'name', 'title', 'detail', 'words'):
            ok = word(v, 200 if k in ('text', 'detail') else 80)
        else:
            ok = identifier(v) or (k == 'player' and v == '$actor')
        require(ok, f'{pid}: invalid {kind}.{k}.')


# Values each parameter is tried with: in bounds, at and past the bounds, of the wrong type.
PROBES = [0, 1, -1, 2, 3, 6, 7, 30, 31, 99, 100, 101, -100, -101, 112, 113, 168, 169, 366, 367, 4095, 4096, 1.5, True, None,
          '', ' ', 'x' * 79, 'x' * 80, 'x' * 81, 'x' * 200, 'x' * 201, 'x' * 300, 'x' * 301, 'rm111', '$actor', 'world', 'bad id!',
          'walk', 'run', 'sitting', 'fed', 'poor', 'clear', 'rich', 'war', 'craft', 'torn_flank', 'gate', 'line\x01', 'two\nlines']


def verdict(check, effect):
    try:
        check(effect, 'p', {'gate': {}}, None)
        return 'ok'
    except ValueError as error:
        return str(error)


class CatalogueShape(unittest.TestCase):
    def test_every_event_is_described(self):
        groups = {g['id'] for g in CAT['groups']}
        ids = [e['id'] for e in CAT['events']]
        self.assertEqual(len(ids), len(set(ids)))
        self.assertEqual(len(rules.EVENTS), 160)
        self.assertEqual(set(ids) - set(rules.EVENTS), {triggers.SIGNAL, rules.MISSION_GATE})
        for e in CAT['events']:
            with self.subTest(event=e['id']):
                self.assertIn(e['group'], groups)
                for key in ('title', 'sentence', 'help'):
                    self.assertTrue(isinstance(e[key], str) and e[key].strip(), key)
                self.assertIsInstance(e['fields'], dict)
                for name, field in e['fields'].items():
                    self.assertIn(name, (*rules.FIELDS, 'quantity', 'coins', *(('signal',) if e['id'] == triggers.SIGNAL else ())))
                    self.assertIn(field['type'], CAT['types'])
                    self.assertTrue(field.get('label'))
                slots = SLOT.findall(e['sentence'])
                allowed = set(rules.FIELDS) | ({'signal'} if e['id'] == triggers.SIGNAL else set())
                self.assertLessEqual(set(slots), allowed, e['sentence'])
                self.assertLessEqual(set(slots) - {'signal'}, set(e['fields']), 'a slot of a field the event does not fill')
                self.assertIsInstance(e['tile'], bool)

    def test_conditions_actions_effects(self):
        self.assertEqual([c['id'] for c in CAT['conditions']], ['all', 'any', 'not', 'field', 'counter', 'region', 'mission',
                                                                *triggers.CONDITIONS_V2])
        # (Version 2, doc 85 Phase 5: the conditions and actions marked v2; an action's `do` is its id unless it says.)
        self.assertTrue(all(c.get('v2') for c in CAT['conditions'][7:]))
        dos = [a.get('do', a['id']) for a in CAT['actions']]
        self.assertEqual(set(dos), {'set', 'add', 'emit', 'schedule', 'cancel', 'effect', 'enter', *triggers.FLOW_V2, *triggers.STATE_V2})
        self.assertEqual(len({a['id'] for a in CAT['actions']}), len(CAT['actions']))
        self.assertTrue(all(a.get('nest') for a in CAT['actions'] if a.get('do', a['id']) in ('if', 'each', 'random')))
        self.assertEqual(len(CAT['effects']), 25)
        for item in [*CAT['conditions'], *CAT['actions'], *CAT['effects']]:
            with self.subTest(item=item['id']):
                self.assertTrue(item['title'] and item['help'])
                keys = [p['key'] for p in item.get('params', [])]
                self.assertEqual(len(keys), len(set(keys)))
                slots = {k for slot in SLOT.findall(item['sentence']) for k in slot.split(',')}
                self.assertEqual(slots, set(keys), 'every parameter is a slot of the sentence, and only those')
                for param in item.get('params', []):
                    self.assertIn(param['type'], CAT['types'])
                    if param['type'] == 'int':
                        self.assertLessEqual(param['min'], param['max'])
                    if param['type'] == 'choice':
                        self.assertTrue(param['values'])
        groups = {g['id'] for g in CAT['actionGroups']}
        self.assertTrue(all(e['group'] in groups for e in [*CAT['actions'], *CAT['effects']]))

    def test_tables_read_from_it(self):
        self.assertEqual(rules.EFFECTS['npc.walk'], ('npc', 'cell', 'x', 'y', 'pace', 'cue'))
        self.assertEqual(len(rules.MISSION_EVENTS), 48)
        self.assertEqual(len(rules.MISSION_MEASURES['quantity']), 13)
        self.assertEqual(len(rules.MISSION_MEASURES['coins']), 18)
        self.assertEqual(len(rules.INJURY_TYPES), 26)
        self.assertLessEqual(set(rules.MISSION_EVENTS), set(rules.EVENTS))
        served = runner.catalogue()
        self.assertEqual(served['catalogue'], CAT)
        self.assertEqual(served['effects'], rules.EFFECTS)

    def test_effect_checks_agree_with_the_hand_written_ones(self):
        rng = random.Random(85)
        good = {'npc': 'rm111', 'cell': 'grey', 'x': 3, 'y': 4, 'pace': 'walk', 'cue': 'gate', 'text': 'Hold the line.', 'posture': 'sitting',
                'region': 'gate', 'points': 5, 'item': 'rope', 'quantity': 2, 'buff': 'fed', 'hours': 4, 'count': 2, 'player': '$actor',
                'add': 'torn_flank', 'severity': 2, 'ship': 'gull', 'area': 'orchards', 'word': 'poor', 'days': 3, 'words': 'blight',
                'band': 'poor', 'town': 'greyfen', 'name': 'Thaw fair', 'inDays': 2, 'template': 'the-debt', 'resident': 'rm111',
                'place': 'grey', 'faction': 'watch', 'other': 'road', 'stance': 'tense', 'disposition': -20, 'reason': 'the raids',
                'skill': 'craft', 'market': 'grey', 'detail': 'Held the ford.', 'project': 'well', 'title': 'A new well', 'scene': 'oath'}
        compared = 0
        for kind, keys in rules.EFFECTS.items():
            if kind == 'scene.play':
                continue   # (Needs an embedded scene: both run the same checks after the loop.)
            base = {'kind': kind, **{k: good[k] for k in keys}}
            self.assertEqual(verdict(rules.validate_effect, base), 'ok', kind)
            for key in keys:
                for value in PROBES:
                    effect = {**base, key: value}
                    self.assertEqual(verdict(rules.validate_effect, effect), verdict(legacy_validate_effect, effect), (kind, key, value))
                    compared += 1
            for pair in itertools.islice(itertools.combinations(keys, 2), 6):
                effect = {**base, **{k: rng.choice(PROBES) for k in pair}}
                self.assertEqual(verdict(rules.validate_effect, effect), verdict(legacy_validate_effect, effect), (kind, pair))
            missing = dict(base)
            missing.pop(keys[0])
            self.assertEqual(verdict(rules.validate_effect, missing), verdict(legacy_validate_effect, missing))
        self.assertGreater(compared, 3000)
        for bad in ({'kind': 'npc.fly'}, {}, None):
            self.assertEqual(verdict(rules.validate_effect, bad), verdict(legacy_validate_effect, bad))


if __name__ == '__main__':
    unittest.main()
