"""Doc 85, Phase 9: the new world effects (Docs/Design/85-story-triggers-and-campaign-control.md, 3.5).

The catalogue's 31 new effects with typed parameters and sentences; the checks (each one sound, each one's parts that go
together, its bounds, a tracker that is a shown variable); the game order each becomes (story_runner.world_order); a
tracker hidden and shown again by the story itself; the hosted permissions read from the catalogue (acting on wolves,
factions); and, in a throwaway database, the world's references checked at approval (doors, towns, tiles, items, strangers,
mantles, templates) and a published story sending them to the game in one batch, the console's Tell to a party and the
world. The game applying each is Tests/story_world_tests.cpp.
"""
import copy
import json
import secrets
import threading
import unittest
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer

import dungeon_master as D
import story_program as rules
import story_quests as quests
import story_runner as runner
import studio
import test_story_console as console
import world_db as W
from test_story_console import admin, WOLVES
from test_world_db import database_available

CAT = rules.CATALOGUE
NEW = ('story.tell', 'journal.note', 'story.ping', 'story.tracker', 'item.take', 'warrant.issue', 'warrant.clear', 'visitor.add',
       'visitor.leave', 'mantle.assign', 'mantle.release', 'npc.kill', 'npc.hold', 'npc.release', 'npc.follow', 'npc.unfollow',
       'door.lock', 'door.unlock', 'door.open', 'door.close', 'item.place', 'board.post', 'weather.set', 'event.add', 'event.cancel',
       'prices.reset', 'project.complete', 'project.cancel', 'deed.revoke', 'storyline.tick', 'scene.stop')
GATE = {'id': 'gate', 'cell': 'gate', 'x1': 1, 'y1': 1, 'x2': 4, 'y2': 4}

# One sound example of each (the acting wolf where a wolf is named).
GOOD = {
    'story.tell': {'to': 'party', 'player': '$actor', 'text': 'Stay together.', 'from': 'rm111'},
    'journal.note': {'text': 'The tracks lead east.'},
    'story.ping': {'cell': 'gate', 'x': 2, 'y': 3, 'label': 'The tracks', 'minutes': 30},
    'story.tracker': {'shown': False, 'var': 'ropes'},
    'item.take': {'player': '$actor', 'item': 'rope', 'quantity': 2},
    'warrant.issue': {'player': '$actor', 'town': 'greyfen', 'fine': 12, 'reason': 'breaking the curfew'},
    'warrant.clear': {'player': '$actor', 'town': 'greyfen'},
    'visitor.add': {'visitor': 'pale-rider', 'name': 'The Pale Rider', 'cell': 'gate', 'x': 2, 'y': 2, 'minutes': 30, 'like': 'rm111'},
    'visitor.leave': {'visitor': 'pale-rider'},
    'mantle.assign': {'npc': 'rm111', 'mantle': 'mt001'},
    'mantle.release': {'npc': 'rm111'},
    'npc.kill': {'npc': 'rm119'},
    'npc.hold': {'npc': 'rm112'},
    'npc.release': {'npc': 'rm112'},
    'npc.follow': {'npc': 'rm113', 'player': '$actor'},
    'npc.unfollow': {'npc': 'rm113'},
    'door.lock': {'door': 'link_inn_a'},
    'door.unlock': {'door': 'link_inn_a'},
    'door.open': {'door': 'link_inn_a'},
    'door.close': {'door': 'link_inn_a'},
    'item.place': {'item': 'rope', 'quantity': 2, 'cell': 'gate', 'x': 3, 'y': 3},
    'board.post': {'town': 'greyfen', 'text': 'Bar your doors tonight.', 'days': 3},
    'weather.set': {'place': 'gate', 'weather': 'snow', 'hours': 6},
    'event.add': {'market': 'greyfen', 'event': 'drovers_in', 'days': 1},
    'event.cancel': {'market': 'greyfen', 'event': 'drovers_in'},
    'prices.reset': {'market': 'all', 'words': 'the roads are open again'},
    'project.complete': {'town': 'greyfen', 'project': 'granary'},
    'project.cancel': {'town': 'greyfen', 'project': 'granary'},
    'deed.revoke': {'player': '$actor', 'detail': 'Held the bridge.'},
    'storyline.tick': {'player': '$actor', 'template': 'the-debt', 'step': 2, 'objective': 1},
    'scene.stop': {},
    # (Doc 86, Phase 1b: what residents remember of a story, and a rumour it starts; tools/test_story_talk.py tests them.)
    'talk.remember': {'scope': 'town', 'town': 'greyfen', 'key': 'nh_stage', 'value': 'reckoning', 'days': 30},
    'talk.forget': {'scope': 'town', 'town': 'greyfen', 'key': 'nh_stage'},
    'belief.plant': {'subject': 'rm111', 'claim': 'was seen at the mill at midnight', 'town': 'greyfen', 'count': 3, 'confidence': 60},
    # (Doc 86, Phase 3: a title, a resident's greeting for one wolf, a rumour put to rest.)
    'title.give': {'player': '$actor', 'title': 'the Lamp of Greyfen', 'known': 'town', 'town': 'greyfen', 'days': 30},
    'title.take': {'player': '$actor', 'title': 'the Lamp of Greyfen'},
    'npc.greet': {'npc': 'rm111', 'player': '$actor', 'text': 'Back again? #greet#', 'days': 7},
    'belief.forget': {'subject': 'rm111', 'claimKind': 'story.rumour', 'town': 'greyfen', 'share': 50},
    # (Doc 86, Phase 5: an animal for the next hunt in a story region.)
    'hunt.stage': {'region': 'gate', 'species': 'bear', 'count': 1, 'minutes': 120},
    # (Doc 86, Phase 7: a story's foe, sent away, and a resident made one.)
    'foe.add': {'foe': 'grey-captain', 'name': 'The Grey Captain', 'cell': 'gate', 'x': 3, 'y': 2, 'strength': 'champion', 'band': 4,
                'stance': 'waits', 'minutes': 60, 'like': 'rm111'},
    'foe.leave': {'foe': 'grey-captain'},
    'foe.outlaw': {'npc': 'rm113', 'minutes': 30, 'strength': 'tough', 'band': 2},
}
PHASE1B_ORDERS = ('talk.remember', 'talk.forget', 'belief.plant')
PHASE3_ORDERS = ('title.give', 'title.take', 'npc.greet', 'belief.forget')
PHASE5_ORDERS = ('hunt.stage',)   # (Doc 86, L1: an animal for the next hunt in a region.)
PHASE7_ORDERS = ('foe.add', 'foe.leave', 'foe.outlaw')   # (Doc 86, C10: foes.)


def program(effects=(), entry=(), audience=None):
    """A version 2 story whose one phase sends `entry` as it opens and whose trigger sends `effects` (for the acting wolf)."""
    return {
        'format': 'ratw-story-program', 'version': 2, 'id': 'long-night', 'title': 'The long night',
        'audience': audience or {'kind': 'players', 'players': list(WOLVES)}, 'start': 'dark', 'regions': [GATE],
        'variables': [{'id': 'ropes', 'type': 'number', 'scope': 'story', 'show': 'Ropes delivered'},
                      {'id': 'quiet', 'type': 'switch', 'scope': 'story'}],
        'phases': [{'id': 'dark', 'journal': 'Something is coming down the road.', 'effects': list(entry), 'exits': [],
                    # (12 effects a trigger, in order; each trigger once in the campaign.)
                    'triggers': [{'id': f'seen-{n}', 'on': 'arrival', 'audience': 'participant', 'once': 'campaign',
                                  'actions': [{'do': 'effect', 'effect': e} for e in chunk]}
                                 for n, chunk in enumerate([list(effects)[i:i + 12] for i in range(0, len(effects), 12)] or [[]])
                                 if chunk] or [{'id': 'seen', 'on': 'arrival', 'audience': 'participant', 'actions': [{'do': 'note', 'text': 'x'}]}]}],
        'tests': [{'name': 'Ash comes in', 'cohort': list(WOLVES), 'events': [{'id': 'e1', 'kind': 'arrival', 'actor': 'player-ash', 'day': 1}],
                   'expect': ['dark']}],
    }


def effect(kind, **more):
    return {'kind': kind, **GOOD[kind], **more}


class Catalogue(unittest.TestCase):
    def test_the_new_effects_are_in_the_catalogue_with_typed_parameters_and_sentences(self):
        ids = [e['id'] for e in CAT['effects']]
        self.assertEqual(len(ids), 89)   # (Doc 86 Phase 7's three foe effects. Phase 10's story.start, the story's own: not a game order; doc 86 Phase 1b's 7, Phase 3's 4,
        self.assertNotIn('story.start', runner.WORLD_ORDERS)   # Phase 4's story.notify and quest.withdraw, the story's own too.)
        for kind in ('story.notify', 'quest.withdraw'):
            self.assertNotIn(kind, runner.WORLD_ORDERS)
            self.assertIn(kind, quests.STORY_EFFECTS)
        for kind in NEW:
            with self.subTest(kind=kind):
                self.assertIn(kind, ids)
                item = next(e for e in CAT['effects'] if e['id'] == kind)
                self.assertTrue(item['title'] and item['help'] and item['sentence'])
                self.assertIn(item['group'], {g['id'] for g in CAT['actionGroups']})
                for p in item['params']:
                    self.assertIn(p['type'], CAT['types'])
        self.assertEqual(set(NEW) - {'story.tracker'} | set(PHASE1B_ORDERS) | set(PHASE3_ORDERS) | set(PHASE5_ORDERS) | set(PHASE7_ORDERS),
                         set(runner.WORLD_ORDERS),
                         'every game order has its mapping')
        self.assertIn('story.tracker', quests.STORY_EFFECTS, 'the tracker is the story\'s own')

    def test_the_lists_the_catalogue_names_are_the_games(self):
        market = json.loads((runner.DATA / 'Economy/market.json').read_text())['events']['list']
        event = next(p for p in rules.EFFECT_PARAMS['event.add'] if p['key'] == 'event')
        self.assertEqual(event['values'], [e['id'] for e in market])
        weather = next(p for p in rules.EFFECT_PARAMS['weather.set'] if p['key'] == 'weather')
        self.assertEqual(sorted(weather['values']), sorted(['clear', 'rain', 'fog', 'snow', 'overcast', 'storm', 'sandstorm']))
        # (Nothing for the calendar: a story never moves the day or the hour.)
        self.assertFalse([e for e in CAT['effects'] if any(w in e['id'] for w in ('calendar', 'time.', 'clock', 'hour', 'day'))])

    def test_who_may_send_them_on_a_hosted_world(self):
        self.assertEqual(rules.WOLF_EFFECTS, {'character.injury', 'story.area.damage', 'item.take', 'warrant.issue'})
        # (Doc 85, Phase 10: the economy's effects need a builder or the owner too: prices and projects.)
        self.assertEqual(rules.BUILDER_EFFECTS, {'faction.relation', 'standing.adjust', 'prices.reset', 'project.complete', 'project.cancel',
                                                 'figure.set', 'figure.clear',   # (Doc 86, C9: the realm's figures.)
                                                 'foe.outlaw'})   # (Doc 86, C10: a resident made a foe.)
        self.assertEqual(studio.STORY_WOLF_KINDS, rules.WOLF_EFFECTS)
        self.assertEqual(runner.WOLF_EFFECTS, rules.WOLF_EFFECTS)
        p = program([effect('item.take')])
        self.assertIn('item.take', studio.story_effect_kinds(p) & studio.STORY_WOLF_KINDS, 'approval needs "may act on wolves"')
        p = program()
        p['quests'] = [{'id': 'q', 'title': 'Q', 'rewards': [effect('warrant.issue')]}]
        self.assertIn('warrant.issue', studio.story_effect_kinds(p), "a quest's rewards are read too")
        self.assertRaisesRegex(ValueError, "may act on players", runner._may_release, program([effect('item.take')]), 'fire', {},
                               {'relations': True, 'wolves': False})
        self.assertRaisesRegex(ValueError, 'standing with a faction', runner._may_release,
                               program([{'kind': 'standing.adjust', 'player': '$actor', 'faction': 'watch', 'points': 2, 'reason': 'well done'}]),
                               'fire', {}, {'relations': False, 'wolves': True})
        runner._may_release(program([effect('weather.set'), effect('event.add'), effect('story.tell')]), 'fire', {},
                            {'relations': False, 'wolves': False})
        for kind in ('prices.reset', 'project.complete', 'project.cancel'):
            self.assertRaisesRegex(ValueError, 'the economy', runner._may_release, program([effect(kind)]), 'fire', {},
                                   {'relations': False, 'wolves': True})


class Checks(unittest.TestCase):
    def ok(self, *effects, entry=()):
        return rules.validate(program(effects, entry))

    def bad(self, pattern, *effects, entry=()):
        self.assertRaisesRegex(ValueError, pattern, rules.validate, program(effects, entry))

    def test_each_is_sound(self):
        for kind in NEW:
            with self.subTest(kind=kind):
                self.ok(effect(kind))
                if kind != 'story.tracker':
                    self.ok(entry=[{**effect(kind), **({'player': 'player-ash'} if GOOD[kind].get('player') == '$actor' else {})}])
        self.ok({'kind': 'story.tell', 'to': 'region', 'region': 'gate', 'text': 'Look up.'})
        self.ok({'kind': 'story.tell', 'to': 'participants', 'text': 'Look up.'})
        self.ok({'kind': 'story.tell', 'to': 'world', 'text': 'The bells ring.'})
        self.ok({'kind': 'weather.set', 'town': 'greyfen', 'weather': 'fog', 'hours': 1})
        self.ok({**effect('door.lock'), 'ifRefused': 'skip'})
        self.ok({**effect('item.take'), 'ifRefused': 'when here', 'waitDays': 3})

    def test_parts_that_go_together(self):
        self.bad('names the wolf', {'kind': 'story.tell', 'to': 'party', 'text': 'x'})
        self.bad('names the wolf', {'kind': 'story.tell', 'to': 'world', 'player': '$actor', 'text': 'x'})
        self.bad('names the region', {'kind': 'story.tell', 'to': 'region', 'text': 'x'})
        self.bad('invalid story.tell.region', {'kind': 'story.tell', 'to': 'region', 'region': 'nowhere', 'text': 'x'})
        self.bad('one line', {'kind': 'story.tell', 'to': 'world', 'text': 'two\nlines'})
        self.bad('one place', {'kind': 'weather.set', 'weather': 'fog', 'hours': 1})
        self.bad('one place', {'kind': 'weather.set', 'place': 'gate', 'town': 'greyfen', 'weather': 'fog', 'hours': 1})
        self.bad("invalid visitor.add.visitor", {**effect('visitor.add'), 'visitor': 'Pale Rider'})
        self.bad("stranger's id", {**effect('visitor.add'), 'visitor': 'Pale_rider'})
        self.bad('shown', {'kind': 'story.tracker', 'shown': True, 'var': 'quiet'})
        self.bad('shown', {'kind': 'story.tracker', 'shown': True, 'var': 'nothing'})
        self.bad('goes in a trigger', entry=[effect('story.tracker')])

    def test_bounds(self):
        for kind, key, value in [('weather.set', 'hours', 73), ('weather.set', 'weather', 'hail'), ('story.ping', 'minutes', 0),
                                 ('story.ping', 'label', 'x' * 41), ('item.take', 'quantity', 21), ('warrant.issue', 'fine', 201),
                                 ('item.place', 'quantity', 6), ('board.post', 'days', 29), ('event.add', 'days', 8),
                                 ('event.add', 'event', 'a_dragon'), ('visitor.add', 'minutes', 1441), ('storyline.tick', 'step', 0),
                                 ('journal.note', 'text', 'x' * 201)]:
            with self.subTest(kind=kind, key=key):
                self.bad(f'invalid {kind}.{key}', {**effect(kind), key: value})


class Orders(unittest.TestCase):
    """The game order each becomes: its own name, what it acts on as the target, the run and its visit in the payload."""

    def order(self, e, actor='player-ash'):
        # (A foe sent away is one the story brings: doc 86, C10.)
        phase = rules.phases_of(rules.validate(program([effect('foe.add'), e] if e['kind'] == 'foe.leave' else [e])))[0]
        return runner.world_order(e, actor, 'world-run', 3, phase)

    def test_each_order(self):
        for kind in runner.WORLD_ORDERS:
            with self.subTest(kind=kind):
                k, target, payload = self.order(effect(kind))
                self.assertEqual(k, kind)
                self.assertEqual((payload['run'], payload['visit']), ('world-run', 3))
        self.assertEqual(self.order(effect('item.take'))[1], 'player-ash', 'the acting wolf')
        self.assertEqual(self.order(effect('npc.follow'))[2]['player'], 'player-ash')
        self.assertEqual(self.order(effect('door.lock'))[1], 'link_inn_a')
        self.assertEqual(self.order(effect('journal.note'))[1], 'world-run')
        k, target, payload = self.order({'kind': 'story.tell', 'to': 'region', 'region': 'gate', 'text': 'Look up.'})
        self.assertEqual((target, payload['region']), ('world-run', GATE))
        self.assertEqual(self.order(effect('weather.set'))[1:], ('gate', {'weather': 'snow', 'hours': 6, 'run': 'world-run', 'visit': 3}))
        self.assertEqual(self.order({'kind': 'weather.set', 'town': 'greyfen', 'weather': 'fog', 'hours': 1})[1:],
                         ('greyfen', {'weather': 'fog', 'hours': 1, 'streets': True, 'run': 'world-run', 'visit': 3}))
        self.assertEqual(self.order(effect('event.add'))[1:], ('greyfen', {'kind': 'drovers_in', 'inDays': 1, 'run': 'world-run', 'visit': 3}))
        self.assertEqual(self.order(effect('project.complete'))[1:], ('greyfen', {'kind': 'granary', 'run': 'world-run', 'visit': 3}))
        self.assertEqual(self.order(effect('visitor.add'))[1], 'pale-rider')
        self.assertEqual(self.order(effect('storyline.tick'))[2]['template'], 'the-debt')
        # (Doc 86, L1: the staged hunt names the story's region with its bounds, for the run.)
        self.assertEqual(self.order(effect('hunt.stage'))[1:], ('world-run', {'region': GATE, 'species': 'bear', 'count': 1, 'minutes': 120,
                                                                        'run': 'world-run', 'visit': 3}))

    def test_a_resident_howls_and_a_hunt_is_staged(self):
        # (Doc 86, J5: npc.howl is the resident's own order; L1: hunt.stage's animal is one of Data/Wild/animals.json, its
        # count 1 to 3, its region the story's; undone at the run's end.)
        self.assertEqual(runner.action({'kind': 'npc.howl', 'npc': 'rm111'}, 'player-ash'), ('npc.howl', 'rm111', {}))
        rules.validate(program([{'kind': 'npc.howl', 'npc': 'rm111'}, effect('hunt.stage')]))
        for bad in ({'species': 'dragon'}, {'species': 'Bear'}, {'count': 4}, {'count': 0}, {'minutes': 1441}, {'region': 'nowhere'}):
            with self.subTest(bad=bad):
                self.assertRaisesRegex(ValueError, 'invalid hunt.stage', rules.validate, program([effect('hunt.stage', **bad)]))
        self.assertIn('hunt.stage', rules.UNDONE_AT_END)
        self.assertNotIn('hunt.stage', rules.WOLF_EFFECTS | rules.BUILDER_EFFECTS)
        self.assertNotIn('npc.howl', rules.WOLF_EFFECTS | rules.BUILDER_EFFECTS)
        self.assertEqual({s['id'] for s in runner.static_refs()['species']}, set(rules.animal_habitats()))
        # (A glyph under two grounds is the first's, as the game reads animals.json: w is water, not mud.)
        self.assertEqual(rules.animal_ground()['w'], 'water')
        self.assertEqual(rules.animal_ground()['D'], 'mud')

    def test_foes(self):
        # (Doc 86, C10: the orders, by the story's id for the foe and the resident made one; the checks; undone at the end; a
        # resident made a foe needs a builder.)
        self.assertEqual(self.order(effect('foe.add'))[:2], ('foe.add', 'grey-captain'))
        self.assertEqual(self.order(effect('foe.add'))[2], {'name': 'The Grey Captain', 'cell': 'gate', 'x': 3, 'y': 2, 'strength': 'champion',
                                                             'band': 4, 'stance': 'waits', 'minutes': 60, 'like': 'rm111', 'run': 'world-run',
                                                             'visit': 3})
        self.assertEqual(self.order(effect('foe.leave'))[:2], ('foe.leave', 'grey-captain'))
        self.assertEqual(self.order(effect('foe.outlaw'))[1:], ('rm113', {'minutes': 30, 'strength': 'tough', 'band': 2, 'run': 'world-run', 'visit': 3}))
        rules.validate(program([effect('foe.add'), effect('foe.leave'), effect('foe.outlaw', lethal=True)]))
        rules.validate(program([effect('foe.add', band={'min': [8, {'div': [{'audience': 'active'}, 2]}]})]))   # (C1a: a band from the crowd.)
        for bad in ({'foe': 'Grey Captain'}, {'foe': 'gc'}, {'foe': 'npc-rm113'}, {'strength': 'follower'}, {'band': 9}, {'strength': 'mighty'}, {'stance': 'roams'}, {'minutes': 1441},
                    {'name': ''}, {'x': 5000}):
            with self.subTest(bad=bad):
                self.assertRaisesRegex(ValueError, 'invalid foe.add', rules.validate, program([effect('foe.add', **bad)]))
        for bad in ({'band': 7}, {'minutes': 721}):
            with self.subTest(bad=bad):
                self.assertRaisesRegex(ValueError, 'invalid foe.outlaw', rules.validate, program([effect('foe.outlaw', **bad)]))
        self.assertRaisesRegex(ValueError, 'one name and one strength', rules.validate,
                               program([effect('foe.add'), effect('foe.add', name='The Captain')]))
        self.assertRaisesRegex(ValueError, 'names a foe the story brings', rules.validate, program([effect('foe.leave', foe='hakon')]))
        self.assertEqual(rules.BUILDER_EFFECTS & set(PHASE7_ORDERS), {'foe.outlaw'})
        self.assertFalse(rules.WOLF_EFFECTS & set(PHASE7_ORDERS))
        self.assertLessEqual({'foe.add', 'foe.outlaw'}, set(rules.UNDONE_AT_END))
        self.assertIn('foe', rules.CATALOGUE['types'])
        # (A foe event read for a foe the story never brings is warned of; one it brings, or a resident it makes one, is not.)
        p = program([effect('foe.add'), effect('foe.outlaw')])
        p['phases'][0]['triggers'].append({'id': 'beaten', 'on': 'foe defeated', 'match': {'target': 'grey-captain'},
                                            'actions': [{'do': 'note', 'text': 'x'}]})
        p['phases'][0]['triggers'].append({'id': 'held', 'on': 'foe defeated', 'match': {'target': 'rm113'},
                                            'actions': [{'do': 'note', 'text': 'x'}]})
        rules.validate(p)
        self.assertFalse([w for w in rules.program_warnings(p) if 'foe this story never brings' in w['text']])
        p['phases'][0]['triggers'].append({'id': 'other', 'on': 'foe fled', 'match': {'target': 'hakon'}, 'actions': [{'do': 'note', 'text': 'x'}]})
        warned = [w for w in rules.program_warnings(p) if 'foe this story never brings' in w['text']]
        self.assertEqual(len(warned), 1)
        self.assertIn('hakon', warned[0]['text'])
        self.assertTrue(warned[0]['path'].startswith('phases[0].triggers[3]'), warned[0]['path'])

    def test_a_storys_own_item_is_given_by_the_games_id(self):
        # (Doc 86, J9: item.give with a tag: the item by the id the game keeps it under, its name, kept or not, and the run
        # that takes it back; a plain item.give is the order it always was.)
        import story_v2_samples as samples
        p = copy.deepcopy(samples.LETTERS)
        rules.validate(p)
        phase = rules.effective_phase(p, p['phases'][0])
        state = {'actor': 'player-ash', 'visits': 2, 'phase': 'carry'}
        kind, target, payload = runner.order_of(None, 'w', 'world-run', 'martin', state, samples.WATCH_LIST, phase)
        self.assertEqual((kind, target), ('item.give', 'player-ash'))
        self.assertEqual(payload, {'item': rules.tagged_item('v2-watch-list', 'letter', 'watch_list'), 'quantity': 1, 'name': 'The watch list',
                                   'run': 'world-run', 'visit': 2})
        kind, target, payload = runner.order_of(None, 'w', 'world-run', 'martin', state, {**samples.THANKS_WRIT, 'reward': 'deliver'}, phase)
        self.assertEqual(payload, {'item': rules.tagged_item('v2-watch-list', 'writ', 'thanks'), 'quantity': 1,
                                   'name': "The warden's writ of thanks", 'keep': True, 'run': 'world-run', 'visit': 2})
        plain = {'kind': 'item.give', 'player': '$actor', 'item': 'rope', 'quantity': 2}
        self.assertEqual(runner.order_of(None, 'w', 'world-run', 'martin', state, plain, phase), ('item.give', 'player-ash', {'item': 'rope', 'quantity': 2}))
        self.assertEqual(rules.tag_hash('v2-watch-list'), rules.tagged_item('v2-watch-list', 'x', 'y')[2:10])
        self.assertIn('tag', rules.CATALOGUE['types'])
        params = {q['key']: q for q in rules.EFFECT_PARAMS['item.give']}
        self.assertEqual((params['tag']['type'], params['name']['type'], params['keep']['type']), ('tag', 'text', 'bool'))
        self.assertTrue(all(params[k].get('optional') for k in ('tag', 'name', 'keep')))
        self.assertNotIn('item.give', rules.UNDONE_AT_END, 'only a story\'s own item is taken back, and only unless kept: said in its help')

    def test_the_tracker_is_hidden_and_shown_again_by_the_story(self):
        p = rules.validate(program([effect('story.tracker')]))
        state = {'counters': {'story': {'ropes': 2}}}
        self.assertEqual(quests.trackers(p, state, 'player-ash'), [{'label': 'Ropes delivered', 'value': 2}])
        phase = rules.phases_of(p)[0]
        run = quests.Run(p, phase, state, 1, 0, lambda *a: None)
        run.story_effect({'kind': 'story.tracker', 'shown': False, 'var': 'ropes'}, 'player-ash')
        self.assertEqual(quests.trackers(p, state, 'player-ash'), [])
        run.story_effect({'kind': 'story.tracker', 'shown': True, 'var': 'ropes'}, 'player-ash')
        self.assertEqual(quests.trackers(p, state, 'player-ash'), [{'label': 'Ropes delivered', 'value': 2}])
        self.assertNotIn('hiddenTrackers', state)
        report = rules.review(p)
        self.assertTrue(report['passed'], report)


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class InTheWorld(unittest.TestCase):
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    fact, tick = console.ConsoleReads.fact, console.ConsoleReads.tick

    def setUp(self):
        console.ConsoleReads.setUp(self)
        self.conn.execute('RESET ROLE')
        self.conn.execute("INSERT INTO world.areas(world_id,id,kind) VALUES (%s,'gate','cell'),(%s,'inn','cell')", (self.world, self.world))
        self.conn.execute("""INSERT INTO world.cells(world_id,id,name,x,y,width,height,region)
                             VALUES (%s,'gate','Gate',200,100,8,6,'greyfen'),(%s,'inn','The inn',300,100,6,6,'greyfen')""",
                          (self.world, self.world))
        self.conn.execute("""INSERT INTO world.links(world_id,id,name,kind,open,a_area,a_x,a_y,b_area,b_x,b_y)
                             VALUES (%s,'inn','The inn door','door',false,'gate',5,1,'inn',1,1)""", (self.world,))
        self.conn.execute('SET LOCAL ROLE ratw_dm')

    def approve(self, p):
        return runner.approve(self.conn, self.world, 'martin', p, rules.digest(p))

    def every(self):
        return [effect(k) for k in runner.WORLD_ORDERS if k != 'mantle.assign']

    def test_the_worlds_references_are_checked_at_approval(self):
        self.approve(program(self.every()))
        for e, words in [({**effect('door.lock'), 'door': 'link_cellar_a'}, 'Unknown door'),
                         ({**effect('board.post'), 'town': 'atlantis'}, 'Unknown town'),
                         ({**effect('weather.set'), 'place': 'moon'}, 'Unknown place'),
                         ({**effect('story.ping'), 'x': 40}, 'outside gate'),
                         ({**effect('item.place'), 'item': 'dragon-egg'}, 'Unknown item'),
                         ({**effect('visitor.add'), 'visitor': 'rm111'}, 'nobody in the world has'),
                         (effect('mantle.assign'), 'Unknown mantle'),
                         ({**effect('storyline.tick'), 'template': 'no-such'}, 'Unknown DM quest template'),
                         ({**effect('story.tell'), 'from': 'rm999'}, 'Unknown from'),
                         ({**effect('npc.hold'), 'npc': 'rm999'}, 'Unknown npc')]:
            with self.subTest(words=words):
                self.assertRaisesRegex(ValueError, words, self.approve, program([e]))
        refs = runner.refs(self.conn, self.world)
        self.assertEqual(refs['doors'], [{'id': 'link_inn_a', 'name': 'The inn door', 'cell': 'gate'}])
        self.assertEqual(refs['mantles'], [])

    def test_a_hunt_staged_in_a_town_is_warned_of(self):
        # (Doc 86, L1, after review: no hunt begins in a town, as the game has its towns, two or more with a market.)
        staged = program([effect('hunt.stage')])
        town = lambda out: [c['text'] for c in out if 'no hunt begins in a town' in c['text']]
        self.assertEqual(town(runner.world_checks(self.conn, self.world, staged)), [], 'no towns saved yet')
        self.conn.execute('RESET ROLE')
        self.conn.execute("""UPDATE game.checkpoints SET payload='{"calendarDays":10,"society":{"report":{"markets":
                             [{"id":"greyfen"},{"id":"tarnby"}]}}}' WHERE world_id=%s""", (self.world,))
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        out = [c for c in runner.world_checks(self.conn, self.world, staged) if 'no hunt begins in a town' in c['text']]
        self.assertEqual([(c['text'], c['blocks']) for c in out],
                         [('gate is in the town of greyfen: no hunt begins in a town, so this animal is never met.', False)])

    def test_a_published_story_sends_them_to_the_game_in_one_batch(self):
        ticket = self.approve(program(self.every()))['ticket']
        run = runner.publish(self.conn, self.world, ticket)['run']
        self.answer_newest()                         # (The phase.)
        self.tick()
        self.fact('arrival', 'player-ash')
        self.tick()
        rows = self.conn.execute("SELECT kind,payload FROM dm.actions WHERE kind='story.effects' ORDER BY id").fetchall()
        self.assertEqual(len(rows), 1, 'one batch')
        items = rows[0][1]['items']
        self.assertEqual([i['kind'] for i in items], [k for k in runner.WORLD_ORDERS if k != 'mantle.assign'])
        door = next(i for i in items if i['kind'] == 'door.lock')
        self.assertEqual((door['target'], door['payload']['run'], door['ifRefused']), ('link_inn_a', run, 'pause'))
        took = next(i for i in items if i['kind'] == 'item.take')
        self.assertEqual((took['target'], took['payload']), ('player-ash', {'item': 'rope', 'quantity': 2, 'run': run, 'visit': 1}))

    def test_the_consoles_tell_reaches_a_party_and_the_world(self):
        ticket = self.approve(program())['ticket']
        run = runner.publish(self.conn, self.world, ticket)['run']
        self.answer_newest()
        self.tick()
        visit, digest = self.conn.execute("SELECT (state->>'visits')::int,digest FROM dm.story_releases WHERE id=%s", (run,)).fetchone()
        for to, extra in (('party', {'player': 'player-ash'}), ('world', {})):
            runner.command(self.conn, self.world, 'martin', {'run': run, 'kind': 'tell', 'payload': {'to': to, 'text': 'Hold fast.', **extra},
                                                               'reason': 'telling the wolves', 'key': secrets.token_hex(8), 'visit': visit,
                                                               'digest': digest})
            self.tick()
            order = self.conn.execute("SELECT payload FROM dm.actions WHERE kind='story.tell' ORDER BY id DESC LIMIT 1").fetchone()[0]
            self.assertEqual((order['to'], order.get('player')), (to, extra.get('player')))
        self.assertRaisesRegex(ValueError, 'a party, a region or the world', runner.command, self.conn, self.world, 'martin',
                               {'run': run, 'kind': 'tell', 'payload': {'to': 'moon', 'text': 'x'}, 'reason': 'telling the wolves',
                                'key': secrets.token_hex(8), 'visit': visit, 'digest': digest})

    def answer_newest(self):
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute("UPDATE dm.actions SET status='applied',result='ok',done_at=now() WHERE status='queued'")
        self.conn.execute('SET LOCAL ROLE ratw_dm')


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class HostedApproval(unittest.TestCase):
    """POST /api/story-program op approve on a hosted world: standing with a faction needs a builder or the owner."""

    @classmethod
    def setUpClass(cls):
        console.ConsoleReads.setUpClass.__func__(cls)
        dm = D.DungeonMaster(connect=lambda target: W.connect('dev', 'dm', dbname=cls.name), worlds=lambda: ['prod', 'dev'])
        hosted = D.make_handler(dm, worlds=lambda: ['dev'], default=lambda: 'dev', hosted=True)
        sessions = {'dm': {'username': 'wren#b2', 'role': 'dm'}, 'builder': {'username': 'tam#c4', 'role': 'builder'}}

        class Hosted(hosted):
            def trusted(self):
                return True

            def handle_api(self, method):
                self.editor_session = sessions[self.headers.get('X-Session')]
                return super().handle_api(method)

        cls.server = ThreadingHTTPServer(('127.0.0.1', 0), Hosted)
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        console.ConsoleReads.tearDownClass.__func__(cls)

    def call(self, body, session):
        request = urllib.request.Request(f'http://127.0.0.1:{self.server.server_port}/api/story-program', json.dumps(body).encode(),
                                         method='POST', headers={'Content-Type': 'application/json', 'X-Session': session})
        try:
            with urllib.request.urlopen(request, timeout=10) as reply:
                return reply.status, json.loads(reply.read())
        except urllib.error.HTTPError as error:
            return error.code, json.loads(error.read())

    def test_standing_needs_a_builder(self):
        p = program(entry=[{'kind': 'standing.adjust', 'player': 'player-ash', 'faction': 'watch', 'points': 2, 'reason': 'well done'}])
        body = {'op': 'approve', 'target': 'dev', 'program': p, 'digest': rules.digest(p)}
        status, data = self.call(body, 'dm')
        self.assertEqual(status, 403, data)
        self.assertIn('standing with a faction', data['error'])
        status, data = self.call(body, 'builder')
        self.assertNotEqual(status, 403, data)   # (Past the permission: this world has no such faction, so it is refused there.)
        q = program(entry=[{**effect('weather.set')}, {**effect('board.post')}, {**effect('event.add')}])
        status, data = self.call({'op': 'approve', 'target': 'dev', 'program': q, 'digest': rules.digest(q)}, 'dm')
        self.assertNotEqual(status, 403, data)   # (The weather, a notice and a market event are a Dungeon Master's, as by hand.)

    def test_the_economy_needs_a_builder(self):
        # (Doc 85, Phase 10: prices eased and projects finished or called off change the economy: a builder or the owner.)
        for kind in ('prices.reset', 'project.complete', 'project.cancel'):
            with self.subTest(kind=kind):
                p = program(entry=[effect(kind)])
                body = {'op': 'approve', 'target': 'dev', 'program': p, 'digest': rules.digest(p)}
                status, data = self.call(body, 'dm')
                self.assertEqual(status, 403, data)
                self.assertIn('the economy', data['error'])
                status, data = self.call(body, 'builder')
                self.assertNotEqual(status, 403, data)


if __name__ == '__main__':
    unittest.main()
