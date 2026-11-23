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
    keys = tuple(k for k in rules.EFFECTS[kind] if k != 'camp')   # (Doc 86, L2: a camp's name came later, optional.)
    rules.shape(effect, ('kind', *keys))
    for k in keys:
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


# (Doc 85, Phase 6: effects with no hand-written check before the catalogue.)
PHASE6 = (*rules.quests.STORY_EFFECTS, 'character.grant', 'item.give', 'standing.adjust')
# (Doc 85, Phase 9's world effects: checked by tools/test_story_effects.py, beside the catalogue.)
PHASE9 = tuple(runner.WORLD_ORDERS) + ('story.tracker',)
# (Doc 86, Phase 1b's: residents spared, figures, talk memory and rumours (tools/test_story_talk.py); npc.protect's `lasts`.)
PHASE1B = ('npc.spare', 'npc.unspare', 'figure.set', 'figure.clear', 'npc.protect')


class CatalogueShape(unittest.TestCase):
    def test_phase6_effects(self):
        ok = [
            {'kind': 'quest.start', 'quest': 'axle', 'player': '$actor'},
            {'kind': 'objective.progress', 'quest': 'axle', 'objective': 'bring', 'player': 'ash', 'amount': 2},
            {'kind': 'npc.offer', 'npc': 'rm111', 'id': 'ask-ivo', 'words': 'Any news?', 'reply': 'Plenty.'},
            {'kind': 'npc.offer', 'npc': 'rm111', 'id': 'ask-ivo', 'words': 'Any news?', 'reply': 'Plenty.', 'player': '$actor', 'once': True},
            {'kind': 'character.grant', 'player': '$actor', 'coins': 12, 'purse': 'town:greyfen', 'reason': 'thanks'},
            {'kind': 'character.grant', 'player': '$actor', 'coins': 12, 'purse': 'town:greyfen:works', 'reason': 'thanks'},
            {'kind': 'character.grant', 'player': '$actor', 'coins': 12, 'purse': 'till:shop-3', 'reason': 'thanks'},
            {'kind': 'item.give', 'player': 'ash', 'item': 'bread', 'quantity': 20},
            {'kind': 'standing.adjust', 'player': 'ash', 'faction': 'watch', 'points': -10, 'reason': 'a brawl'},
        ]
        for e in ok:
            self.assertEqual(verdict(rules.validate_effect, e), 'ok', e)
        bad = [
            {'kind': 'objective.progress', 'quest': 'axle', 'objective': 'bring', 'player': 'ash', 'amount': 0},
            {'kind': 'npc.offer', 'npc': 'rm111', 'id': 'x', 'words': 'Any news?', 'reply': 'Plenty.', 'once': 1},
            {'kind': 'npc.offer', 'npc': 'rm111', 'id': 'x', 'words': 'Any news?'},
            {'kind': 'character.grant', 'player': '$actor', 'coins': 12, 'purse': 'treasury', 'reason': 'thanks'},
            {'kind': 'character.grant', 'player': '$actor', 'coins': 12, 'purse': 'town:greyfen:vault', 'reason': 'thanks'},
            {'kind': 'character.grant', 'player': '$actor', 'coins': 2501, 'purse': 'town:greyfen', 'reason': 'thanks'},
            {'kind': 'character.grant', 'player': '$actor', 'coins': 12, 'reason': 'thanks'},
            {'kind': 'item.give', 'player': 'ash', 'item': 'bread', 'quantity': 21},
            {'kind': 'standing.adjust', 'player': 'ash', 'faction': 'watch', 'points': 0, 'reason': 'a brawl'},
        ]
        for e in bad:
            self.assertNotEqual(verdict(rules.validate_effect, e), 'ok', e)

    def test_every_event_is_described(self):
        groups = {g['id'] for g in CAT['groups']}
        ids = [e['id'] for e in CAT['events']]
        self.assertEqual(len(ids), len(set(ids)))
        # (160, Phase 6's 11 quest and party events, Phase 8's 24, Phase 10's 10; doc 86's moon, title given; Phase 4's story notified,
        # variable changed, month begins, succession; Phase 5's howled and chorus closed.)
        self.assertEqual(len(rules.EVENTS), 213)
        self.assertEqual(set(ids) - set(rules.EVENTS), {triggers.SIGNAL, rules.MISSION_GATE, rules.READY_EXIT})   # (Doc 86: the ready exit.)
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
        self.assertEqual(len(CAT['effects']), 86)   # (25, Phase 6's 14, Phase 9's 31, Phase 10's story.start, doc 86 Phase 1b's 7, Phase 3's 4, Phase 4's 2,
        # Phase 5's npc.howl and hunt.stage.)
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
        self.assertEqual(len(rules.MISSION_EVENTS), 72)   # (52, 11 of Phase 8's new events a wolf does, doc 86's six (L3), title given,
        # Phase 5's howled and chorus closed.)
        # (Doc 86, L3: the six events with no wolf, or a wolf only sometimes, are mission evidence; M5: trade done has a cell.)
        self.assertLessEqual({'camp cleared', 'caravan arrives', 'ship lost', 'ship docks', 'project built', 'festival'}, set(rules.MISSION_EVENTS))
        trade = next(e for e in CAT['events'] if e['id'] == 'trade done')
        self.assertEqual(trade['fields']['cell']['type'], 'place')
        self.assertFalse(trade['tile'])
        self.assertEqual(len(rules.MISSION_MEASURES['quantity']), 16)   # (Phase 8: item picked up, trade done; doc 86's chorus closed.)
        self.assertEqual(len(rules.MISSION_MEASURES['coins']), 19)   # (Phase 8: trade done.)
        self.assertEqual(len(rules.INJURY_TYPES), 26)
        self.assertLessEqual(set(rules.MISSION_EVENTS), set(rules.EVENTS))
        served = runner.catalogue()
        self.assertEqual(served['catalogue'], CAT)
        self.assertEqual(served['effects'], rules.EFFECTS)

    def test_the_doubts_kinds_are_claims_kinds(self):
        # (Doc 86, J8: the kinds of claim a story may put to rest are claims.json's, and the game's list is the same.)
        claims = json.loads((runner.DATA / 'Talk/claims.json').read_text())['kinds']
        kinds = next(p for p in rules.EFFECT_PARAMS['belief.forget'] if p['key'] == 'claimKind')['values']
        self.assertLessEqual(set(kinds), set(claims))
        game = (runner.DATA.parent / 'Core/RatwGameStoryWorld.cpp').read_text()
        listed = re.search(r'doubtable\(const std::string& kind\)\n\{\n    static const std::set<std::string> kinds\{([^}]*)\}', game)
        self.assertEqual(set(re.findall(r'"([a-z.]+)"', listed.group(1))), set(kinds))

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
            if kind == 'scene.play' or kind in PHASE6 or kind in PHASE9 or kind in PHASE1B:
                continue   # (Needs an embedded scene: both run the same checks after the loop. Phase 6's are new: below.)
            keys = tuple(k for k in keys if k != 'camp')   # (Doc 86, L2: optional, and the hand-written checks had none.)
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
