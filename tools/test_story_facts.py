"""The world's facts and the new events (Docs/Design/85-story-triggers-and-campaign-control.md, 3.3, 3.4 and Phase 8).

Offline: the checks of the 23 new conditions (version 2 only; whose, regions, quests, values); the rehearsal of
story_v2_samples.FACTS_WATCH (facts stated by a case and changed by its facts, read as of each event; a wait resuming with
the facts of its moment; no facts: unknown, which never fires); the watch list (story_program.watch_of); and the two twins
agreeing on every condition over hundreds of made-up worlds (the native library and story_triggers.fact_test, with facts
missing, mistyped and partial). In a throwaway database (skipped without the local PostgreSQL): migration 0076's
game.record_story_facts (a row per write, the same facts moving the newest on, as of the newest event); the runner reading
each event with the facts written with it, never newer (a page at a time); the watch list going to the game with the phase;
the console's Explain and Why not with the facts read; facts kept consistent when events are written between saves (a write
of events and facts the runner is reading through)."""

import copy
import json
import random
import unittest
from unittest.mock import patch

import story_native
import story_program as rules
import story_runner as runner
import story_triggers as triggers
import story_v2_samples as samples
import test_story_console as console
from test_world_db import database_available

ASH, BIRCH = samples.ASH, samples.BIRCH


def program(conditions, events, facts=None, regions=None, quests=None):
    """A version 2 story whose triggers each note, on a conversation, whether their one condition held."""
    triggers_ = [{'id': f'c{i}', 'on': 'conversation', 'actions': [
        {'do': 'if', 'if': c, 'then': [{'do': 'list add', 'var': 'held', 'value': f'c{i}'}]}]} for i, c in enumerate(conditions)]
    p = {'format': 'ratw-story-program', 'version': 2, 'id': 'facts-probe', 'title': 'Facts probe',
         'audience': {'kind': 'players', 'players': [ASH, BIRCH]}, 'start': 'probe',
         'variables': [{'id': 'held', 'type': 'texts', 'scope': 'story'}],
         'regions': regions or [{'id': 'gate', 'cell': 'grey', 'x1': 2, 'y1': 2, 'x2': 6, 'y2': 6}],
         'phases': [{'id': 'probe', 'journal': 'Probing.', 'effects': [], 'exits': [], 'triggers': triggers_}],
         'tests': [{'name': 'probe', 'cohort': [ASH, BIRCH], 'events': events, 'expect': ['probe'], **({'facts': facts} if facts else {})}]}
    if quests:
        p['quests'] = quests
    return p


class Checks(unittest.TestCase):
    def bad(self, condition, words, version=2):
        p = program([condition], [])
        p['version'] = version
        if version == 1:
            p.pop('variables')
            p['phases'][0]['triggers'][0]['actions'] = [{'do': 'emit', 'signal': 'x'}]
            p['phases'][0]['triggers'][0]['if'] = condition
        with self.assertRaisesRegex(ValueError, words):
            rules.validate(p)

    def test_every_new_condition_is_checked(self):
        self.bad({'test': 'carries', 'item': 'rope', 'op': '>=', 'value': 1}, 'version 2', version=1)
        self.bad({'test': 'wolf in region', 'region': 'nowhere'}, 'Unknown story region')
        self.bad({'test': 'carries', 'item': 'rope', 'op': '~', 'value': 1}, 'comparison')
        self.bad({'test': 'carries', 'item': 'rope', 'op': '>=', 'value': -1}, 'whole number')
        self.bad({'test': 'standing', 'faction': 'watch', 'op': '>=', 'value': 101}, 'whole number')
        self.bad({'test': 'wolf here', 'who': '$each'}, 'For each')
        self.bad({'test': 'wolf here', 'who': 'not an id!'}, 'Name the wolf')
        self.bad({'test': 'wolf is', 'state': 'asleep', 'is': True}, 'Downed, hurt')
        self.bad({'test': 'wolf indoors', 'is': 'yes'}, 'Yes or no')
        self.bad({'test': 'resident is', 'npc': 'rm111', 'state': 'merry'}, 'Alive, dead')
        self.bad({'test': 'resident has', 'npc': 'rm111', 'what': 'hat', 'value': 'red'}, 'job, mood')
        # (Doc 86, J10: a tier or a playable family; Death Walkers are residents only.)
        self.bad({'test': 'wolf has', 'what': 'tier', 'value': 'mighty'}, 'normal, gifted or quickened')
        self.bad({'test': 'wolf has', 'what': 'family', 'value': 'death_walker'}, 'A family is one of')
        self.bad({'test': 'wolf has', 'what': 'colour', 'value': 'grey'}, "tier or its family")
        families = json.loads((rules.CATALOGUE_FILE.parents[1] / 'Gifts/families.json').read_text())['families']
        self.assertEqual(triggers.GIFT_FAMILIES, tuple(f['id'] for f in families if not f.get('npcOnly')), 'the playable families')
        self.bad({'test': 'hour between', 'from': 0, 'to': 24}, 'Hours from 0')
        self.bad({'test': 'calendar is', 'what': 'weekday', 'value': 'winter'}, 'weekday, a season')
        self.bad({'test': 'weather is', 'weather': 'hail'}, 'Clear, overcast')
        self.bad({'test': 'relation is', 'faction': 'a', 'other': 'b', 'op': '>', 'stance': 'allied'}, 'comparison')
        self.bad({'test': 'relation is', 'faction': 'a', 'other': 'b', 'op': '>=', 'stance': 'cold'}, 'War, hostile')
        self.bad({'test': 'carries', 'item': 'rope', 'op': '>=', 'value': 1, 'extra': 1}, '.')
        self.bad({'test': 'quest is', 'quest': 'nothing', 'status': 'done'}, 'Unknown quest')
        p = program([{'test': 'objective is', 'quest': 'q', 'objective': 'nope', 'status': 'done'}], [],
                    quests=[{'id': 'q', 'title': 'Q', 'objectives': [{'id': 'o', 'label': 'O', 'on': 'conversation'}]}])
        with self.assertRaisesRegex(ValueError, 'no objective'):
            rules.validate(p)
        p['phases'][0]['triggers'][0]['actions'][0]['if'] = {'test': 'quest is', 'quest': 'q', 'status': 'lost'}
        with self.assertRaisesRegex(ValueError, 'A quest is none'):
            rules.validate(p)

    def test_the_catalogue_lists_them(self):
        ids = {c['id'] for c in rules.CATALOGUE['conditions']}
        self.assertLessEqual({*triggers.FACTS_V2, *triggers.QUEST_V2}, ids)
        self.assertEqual(set(rules.FACT_CONDITIONS), set(triggers.FACTS_V2))
        self.assertEqual(len(triggers.FACTS_V2) + len(triggers.QUEST_V2), 24)   # (Doc 86, J10: wolf has.)
        self.assertEqual(len(rules.WATCHED_EVENTS), 36)   # (Phase 8's 24, Phase 10's 8, doc 86's moon and month begins, howled,
        # chorus closed.)
        for e in rules.CATALOGUE['events']:
            if e['id'] in rules.WATCHED_EVENTS:
                self.assertTrue(e['help'].endswith('(the story\'s watch list).'), e['id'])

    def test_rehearsal_facts_are_checked(self):
        p = copy.deepcopy(samples.FACTS_WATCH)
        p['tests'][0]['facts'] = {'wolves': {ASH: {'carries': {'rope': 'three'}}}}
        with self.assertRaisesRegex(ValueError, 'numbers by id'):
            rules.review(p)
        p['tests'][0]['facts'] = {'moon': {}}
        with self.assertRaisesRegex(ValueError, 'Facts have'):
            rules.review(p)
        p['tests'][0]['facts'] = {'wolves': {ASH: {'indoors': 1}}}
        with self.assertRaisesRegex(ValueError, 'mistyped'):
            rules.review(p)


class Rehearsal(unittest.TestCase):
    def test_the_night_watch_rehearses_as_pinned(self):
        report = rules.review(samples.FACTS_WATCH)
        self.assertTrue(report['passed'], [(t['name'], t['passed']) for t in report['tests']])

    def test_merge_facts_section_by_section(self):
        base = {'wolves': {'a': {'cell': 'x'}, 'b': {'cell': 'y'}}, 'world': {'hour': 3}}
        out = rules.merge_facts(base, {'wolves': {'a': None, 'c': {'cell': 'z'}}, 'world': {'hour': 4}})
        self.assertEqual(out, {'wolves': {'b': {'cell': 'y'}, 'c': {'cell': 'z'}}, 'world': {'hour': 4}})
        self.assertEqual(base['wolves']['a'], {'cell': 'x'}, 'the base is not changed')

    def test_a_wait_keeps_no_facts_and_resumes_with_those_of_its_moment(self):
        p = copy.deepcopy(samples.FACTS_WATCH)
        state = rules.begin(p, [ASH, BIRCH], 1)
        facts = {'wolves': {ASH: {'cell': 'grey', 'x': 3, 'y': 3, 'indoors': False}}}
        rules.advance(p, state, {'id': 1, 'kind': 'door opened', 'actor': ASH, 'facts': facts}, 1, [])
        self.assertEqual(len(state['waiting']), 1)
        self.assertNotIn('facts', state['waiting'][0]['event'])
        later = {'wolves': {ASH: {'cell': 'inn', 'indoors': True}}}
        rules.advance(p, state, {'id': 2, 'kind': 'clock', 'facts': later}, 1.5, [])
        self.assertEqual(state['counters']['story']['events'], ['door opened', 'indoors after'])


class Watch(unittest.TestCase):
    def test_a_story_that_asks_nothing_costs_nothing(self):
        for p in runner.catalogue()['examples'].values():
            self.assertEqual(rules.watch_of(p), {}, p['id'])

    def test_the_watch_list(self):
        w = rules.watch_of(samples.FACTS_WATCH)
        self.assertEqual(w['facts'], ['relations', 'residents', 'weather', 'wolves', 'world'])
        self.assertEqual((w['items'], w['factions'], w['towns'], w['residents'], w['cells']),
                         (['rope'], ['greyfen_watch'], ['greyfen'], ['rm111', 'rm119'], ['grey']))
        self.assertEqual(w['relations'], [['greyfen_watch', 'road_company']])
        self.assertTrue(w['weatherWhereWolves'])
        self.assertIn('door locked', w['events'], 'a quest objective listens too')
        self.assertIn('night falls', w['events'], 'and an exit')
        self.assertNotIn('conversation', w['events'], 'only the watched events')
        only = program([{'test': 'hour between', 'from': 1, 'to': 2}], [])
        self.assertEqual(rules.watch_of(only), {'facts': ['world']})


def random_facts(rng):
    def wolf():
        w = {}
        for key, make in (('cell', lambda: rng.choice(['grey', 'mill', 'inn'])), ('x', lambda: rng.randint(0, 8)), ('y', lambda: rng.randint(0, 8)),
                          ('indoors', lambda: rng.random() < .5), ('town', lambda: rng.choice(['greyfen', 'ridgemere'])),
                          ('party', lambda: rng.choice(['', 'p1', 'p2'])), ('partySize', lambda: rng.randint(1, 3)),
                          ('carries', lambda: {k: rng.randint(0, 4) for k in rng.sample(['rope', 'bread'], rng.randint(0, 2))}),
                          ('coins', lambda: rng.randint(0, 50)), ('standing', lambda: {'watch': rng.randint(-50, 50)} if rng.random() < .7 else {}),
                          ('reputation', lambda: {'greyfen': rng.randint(-3, 3)} if rng.random() < .7 else {}),
                          ('downed', lambda: rng.random() < .3), ('hurt', lambda: rng.random() < .3), ('wanted', lambda: rng.random() < .3),
                          ('held', lambda: rng.random() < .2),
                          # (Doc 86, J10: the Gift the wolf was made with.)
                          ('gift', lambda: rng.choice([{'tier': 'normal', 'family': ''}, {'tier': 'gifted', 'family': 'seer'},
                                                       {'tier': 'quickened', 'family': 'fire'}, {'tier': 'gifted'}]))):
            if rng.random() < .85:
                w[key] = make()
        return w
    f = {}
    if rng.random() < .9:
        f['wolves'] = {w: wolf() for w in (ASH, BIRCH, 'elm') if rng.random() < .75}
    if rng.random() < .85:
        f['world'] = {k: v for k, v in (('hour', rng.randint(0, 23)), ('weekday', rng.choice(triggers.CALENDAR['weekday'])),
                                         ('season', rng.choice(triggers.CALENDAR['season'])), ('period', rng.choice(triggers.CALENDAR['period'])))
                      if rng.random() < .85}
    if rng.random() < .85:
        f['residents'] = {r: {k: v for k, v in (('alive', rng.random() < .7), ('cell', rng.choice(['grey', 'mill'])), ('x', rng.randint(0, 8)),
                                                  ('y', rng.randint(0, 8)), ('indoors', rng.random() < .5),
                                                  ('mantle', rng.choice(['', 'carter'])), ('job', rng.choice(['guard', 'smith'])),
                                                  ('mood', rng.choice(['even', 'low'])), ('town', 'greyfen'),
                                                  ('post', rng.choice(['', 'reigning at the Great Palace']))) if rng.random() < .85}
                          for r in ('rm111', 'rm119') if rng.random() < .8}
    if rng.random() < .85:
        f['weather'] = {c: rng.choice(triggers.WEATHERS) for c in ('grey', 'mill', 'inn') if rng.random() < .7}
    if rng.random() < .85:
        f['relations'] = {'watch>road': rng.choice(triggers.STANCES)} if rng.random() < .8 else {}
    return f


ALL = [
    {'test': 'wolf here'}, {'test': 'wolf here', 'who': BIRCH}, {'test': 'wolf in region', 'region': 'gate'},
    {'test': 'wolf in place', 'place': 'grey'}, {'test': 'wolf in town', 'town': 'greyfen'}, {'test': 'wolf indoors', 'is': True},
    {'test': 'wolf indoors', 'is': False}, {'test': 'count in region', 'region': 'gate', 'op': '>=', 'value': 1},
    {'test': 'count in region', 'region': 'gate', 'op': '=', 'value': 0}, {'test': 'carries', 'item': 'rope', 'op': '>=', 'value': 2},
    {'test': 'carries', 'item': 'bread', 'op': '=', 'value': 0}, {'test': 'coins', 'op': '<', 'value': 20},
    {'test': 'standing', 'faction': 'watch', 'op': '>', 'value': 0}, {'test': 'reputation', 'town': 'greyfen', 'op': '>=', 'value': 1},
    *({'test': 'wolf is', 'state': s, 'is': v} for s in triggers.WOLF_STATES for v in (True, False)),
    {'test': 'party size', 'op': '>=', 'value': 2}, {'test': 'in party with', 'other': BIRCH}, {'test': 'in party with', 'who': BIRCH, 'other': '$actor'},
    *({'test': 'resident is', 'npc': 'rm111', 'state': s} for s in triggers.RESIDENT_STATES),
    {'test': 'resident in place', 'npc': 'rm111', 'place': 'grey'}, {'test': 'resident in region', 'npc': 'rm119', 'region': 'gate'},
    *({'test': 'resident has', 'npc': 'rm111', 'what': w, 'value': v} for w, v in (('mantle', 'carter'), ('job', 'guard'), ('mood', 'low'), ('town', 'greyfen'),
                                                                                   ('post', 'reigning at the Great Palace'), ('post', ''))),
    *({'test': 'wolf has', 'what': w, 'value': v} for w, v in (('tier', 'normal'), ('tier', 'quickened'), ('family', 'seer'), ('family', ''))),
    {'test': 'wolf has', 'who': BIRCH, 'what': 'tier', 'value': 'gifted'},
    {'test': 'hour between', 'from': 6, 'to': 18}, {'test': 'hour between', 'from': 22, 'to': 4},
    *({'test': 'calendar is', 'what': w, 'value': v} for w, v in (('weekday', 'Restday'), ('season', 'winter'), ('period', 'night'))),
    {'test': 'weather is', 'weather': 'rain'}, {'test': 'weather is', 'where': 'mill', 'weather': 'clear'},
    *({'test': 'relation is', 'faction': 'watch', 'other': 'road', 'op': op, 'stance': 'neutral'} for op in ('=', '!=', '>=', '<=')),
    {'test': 'not', 'condition': {'test': 'wolf here', 'who': 'elm'}},
    {'test': 'any', 'conditions': [{'test': 'wolf indoors', 'is': True}, {'test': 'hour between', 'from': 0, 'to': 5}]},
]


CREW = ['rm111', 'rm112', 'rm113', 'rm114', 'rm115', 'rm116', 'rm117']


def crew_story(conditions, events, initial=CREW, scope='story', facts=None):
    """Doc 86, J6: the facts probe with a residents list, the crew at the ropes."""
    p = program(conditions, events, facts=facts)
    p['variables'].append({'id': 'crew', 'type': 'residents', 'scope': scope, 'initial': list(initial)})
    return p


def crew_at(inside, dead=(), away=()):
    """The world's facts with the crew: `inside` stand in the gate, the rest out of it; `dead` died there; `away` unknown."""
    res = {}
    for i, r in enumerate(CREW):
        if r in away:
            continue
        res[r] = {'alive': r not in dead, 'cell': 'grey', 'x': 3 if r in inside or r in dead else 7, 'y': 3, 'indoors': False}
    return {'residents': res}


class ResidentsInARegion(unittest.TestCase):
    """Doc 86, J6: count in region counts the residents of a list who stand in it now."""

    def held(self, conditions, facts, initial=CREW):
        p = crew_story(conditions, [{'id': 'e1', 'kind': 'conversation', 'day': 1, 'actor': ASH, 'target': 'rm111', 'facts': facts}],
                       initial=initial)
        report = rules.review(p)
        return sorted(report['tests'][0]['counters'].get('story', {}).get('held', []))

    def test_three_of_seven_at_the_ropes(self):
        at = {'test': 'count in region', 'region': 'gate', 'op': '>=', 'value': 3, 'who': 'residents', 'list': 'crew'}
        four = {**at, 'value': 4}
        exactly = {**at, 'op': '=', 'value': 3}
        self.assertEqual(self.held([at, four, exactly], crew_at(CREW[:3])), ['c0', 'c2'], 'three of the seven stand in the gate')
        # (A dead one standing there does not count; one the facts do not have is not counted.)
        self.assertEqual(self.held([at, exactly], crew_at(CREW[:2], dead=CREW[2:3])), [])
        self.assertEqual(self.held([at, exactly], crew_at(CREW[:3], away=CREW[3:])), ['c0', 'c1'])
        # (Every one listed unknown: the whole condition is unknown, which never fires, nor does its "not".)
        self.assertEqual(self.held([at, {'test': 'not', 'condition': at}], {'residents': {}}), [])
        self.assertEqual(self.held([{'test': 'not', 'condition': at}], crew_at(CREW[:1])), ['c0'])
        # (An empty list counts none: known, nought.)
        self.assertEqual(self.held([{**at, 'op': '=', 'value': 0}], crew_at(CREW[:3]), initial=[]), ['c0'])
        # (The wolves, as ever, when it says nothing: the story's wolves.)
        self.assertEqual(self.held([{'test': 'count in region', 'region': 'gate', 'op': '>=', 'value': 1}],
                                   {'wolves': {ASH: {'cell': 'grey', 'x': 3, 'y': 3}}, **crew_at(CREW)}), ['c0'])

    def test_the_checks(self):
        def bad(c, words, scope='story', kind='residents'):
            p = crew_story([c], [], scope=scope, initial=[] if scope != 'story' or kind != 'residents' else CREW)
            p['variables'][-1]['type'] = kind
            with self.assertRaisesRegex(ValueError, words):
                rules.validate(p)
        base = {'test': 'count in region', 'region': 'gate', 'op': '>=', 'value': 3}
        bad({**base, 'who': 'residents'}, 'names their list')
        bad({**base, 'list': 'crew'}, 'names their list')
        bad({**base, 'who': 'everyone'}, "story's wolves or the residents")
        bad({**base, 'who': 'residents', 'list': 'crew'}, 'list of residents', kind='wolves')
        bad({**base, 'who': 'residents', 'list': 'crew'}, 'kept for each wolf', scope='player')
        bad({**base, 'who': 'residents', 'list': 'nobody'}, 'Declare the variable')
        rules.validate(crew_story([{**base, 'who': 'residents', 'list': 'crew'}], []))
        rules.validate(crew_story([{**base, 'who': 'wolves'}], []))
        rules.validate(crew_story([{**base, 'who': 'residents', 'list': 'crew'}], [], scope='world', initial=[]))

    def test_the_watch_names_the_lists_and_the_runner_their_members(self):
        p = rules.validate(crew_story([{'test': 'count in region', 'region': 'gate', 'op': '>=', 'value': 3, 'who': 'residents', 'list': 'crew'},
                                       {'test': 'resident is', 'npc': 'rm900', 'state': 'alive'}], []))
        w = rules.watch_of(p)
        self.assertEqual((w['facts'], w['residents'], w['residentLists']), (['residents'], ['rm900'], ['crew']))
        state = {'cohort': [ASH], 'counters': {'story': {'crew': ['rm112', 'rm111']}}}
        now = runner.watch_now(p, state)
        self.assertEqual(now, {'facts': ['residents'], 'residents': ['rm111', 'rm112', 'rm900']}, 'the game is sent members, not lists')
        self.assertEqual(runner.watch_now(p, {'cohort': [ASH]})['residents'], sorted(['rm900', *CREW]), 'the list\'s first value')
        self.assertNotEqual(runner._watch_key(now), runner._watch_key(runner.watch_now(p, {'cohort': [ASH], 'counters': {'story': {'crew': []}}})))
        self.assertNotIn('residentLists', runner.watch_now(rules.validate(program([{'test': 'hour between', 'from': 1, 'to': 2}], [])), {}))


@unittest.skipUnless(story_native.interpreter(), 'Build ratw_server or set RATW_STORY_LIBRARY first.')
class Parity(unittest.TestCase):
    def test_residents_counted_in_a_region_agree(self):
        """Doc 86, J6: the twins count the crew alike over made-up worlds (residents missing, dead, partial), and as the list
        changes."""
        conditions = [{'test': 'count in region', 'region': 'gate', 'op': op, 'value': v, 'who': 'residents', 'list': 'crew'}
                      for op in ('>=', '=', '<') for v in (0, 1, 3)]
        conditions.append({'test': 'not', 'condition': conditions[0]})
        for seed in range(12):
            rng = random.Random(100 + seed)
            events = []
            for i in range(30):
                res = {r: {k: v for k, v in (('alive', rng.random() < .8), ('cell', rng.choice(['grey', 'mill'])), ('x', rng.randint(0, 8)),
                                              ('y', rng.randint(0, 8))) if rng.random() < .9} for r in CREW if rng.random() < .8}
                events.append({'id': f'e{i}', 'kind': 'conversation', 'day': 1 + i / 100, 'actor': ASH, 'target': rng.choice(CREW),
                               **({'facts': {'residents': res}} if rng.random() < .9 else {})})
            p = crew_story(conditions, events, initial=rng.sample(CREW, rng.randint(0, 7)))
            # (The list changes as it goes: a trigger takes the one talked to off it.)
            p['phases'][0]['triggers'].append({'id': 'off', 'on': 'conversation', 'actions': [
                {'do': 'list remove', 'var': 'crew', 'value': {'field': 'target'}}]})
            native = rules.review(p)
            with patch('story_native.interpreter', return_value=None):
                python = rules.review(p)
            self.assertEqual(native, python, f'seed {seed}')

    def test_every_world_condition_agrees_over_made_up_worlds(self):
        """Each of ALL's conditions in its own trigger, over 30 conversations whose facts are made up at random (facts
        missing, partial, wolves away): the twins note the same conditions held, event by event, under 12 seeds."""
        chunks = [ALL[i:i + 60] for i in range(0, len(ALL), 60)]
        for seed in range(12):
            rng = random.Random(seed)
            events = [{'id': f'e{i}', 'kind': 'conversation', 'day': 1 + i / 100, 'actor': rng.choice([ASH, BIRCH, 'elm']), 'target': 'rm111',
                       **({'facts': random_facts(rng)} if rng.random() < .9 else {})} for i in range(30)]
            for chunk in chunks:
                p = program(chunk, events)
                for e in p['tests'][0]['events']:
                    if 'facts' in e and rng.random() < .5:   # (Some state the world anew, some change only part of it.)
                        e['facts'] = {k: v for k, v in e['facts'].items() if rng.random() < .6}
                native = rules.review(p)
                with patch('story_native.interpreter', return_value=None):
                    python = rules.review(p)
                self.assertEqual(native, python, f'seed {seed}')
                self.assertTrue(native['tests'][0]['counters'].get('story', {}).get('held'), 'something held')

    def test_the_night_watch_agrees(self):
        native = rules.review(samples.FACTS_WATCH)
        with patch('story_native.interpreter', return_value=None):
            self.assertEqual(native, rules.review(samples.FACTS_WATCH))


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class Runner(unittest.TestCase):
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    fact, answer, tick = console.ConsoleReads.fact, console.ConsoleReads.answer, console.ConsoleReads.tick

    def setUp(self):
        console.ConsoleReads.setUp(self)

    def write(self, events, facts):
        """A write of events and the world's facts, as the game makes it: one transaction, the facts after the events."""
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute('SELECT game.record_events(%s,%s::jsonb)', (self.world, json.dumps(
            [{'kind': 'conversation', 'target': 'rm111', 'day': 10, **e} for e in events])))
        at = self.conn.execute('SELECT game.record_story_facts(%s,%s::jsonb)', (self.world, json.dumps(facts))).fetchone()[0]
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        return at

    def story(self):
        p = program([{'test': 'carries', 'item': 'rope', 'op': '>=', 'value': 1}, {'test': 'wolf indoors', 'is': True}], [])
        p['audience'] = {'kind': 'players', 'players': list(console.WOLVES)}
        p.pop('regions')
        for t in p['phases'][0]['triggers']:   # (The condition on the trigger itself, so Why not judges it.)
            t['if'] = t['actions'][0]['if']
            t['actions'] = t['actions'][0]['then']
        p['tests'] = [{'name': 'none', 'cohort': list(console.WOLVES), 'events': [], 'expect': ['probe']}]
        ticket = runner.approve(self.conn, self.world, 'martin', p, rules.digest(p))['ticket']
        self.run_id = runner.publish(self.conn, self.world, ticket)['run']
        self.answer()
        self.tick()
        return p

    def held(self):
        state = self.conn.execute('SELECT state FROM dm.story_releases WHERE id=%s', (self.run_id,)).fetchone()[0]
        return state.get('counters', {}).get('story', {}).get('held', [])

    def test_record_story_facts(self):
        a = self.write([{'actor': 'player-ash'}], {'wolves': {}})
        b = self.write([{'actor': 'player-ash'}], {'wolves': {}})
        rows = self.conn.execute('SELECT as_of_event FROM game.story_facts WHERE world_id=%s', (self.world,)).fetchall()
        self.assertEqual(rows, [(b,)], 'the same facts move the newest row on')
        newest = self.conn.execute('SELECT max(id) FROM game.events WHERE world_id=%s', (self.world,)).fetchone()[0]
        self.assertEqual((b, b > a), (newest, True), 'as of the newest event written with them')
        c = self.write([{'actor': 'player-ash'}], {'wolves': {'player-ash': {}}})
        self.assertEqual(len(self.conn.execute('SELECT 1 FROM game.story_facts WHERE world_id=%s', (self.world,)).fetchall()), 2)
        self.assertEqual(runner.facts_at(runner.facts_page(self.conn, self.world, a, c), b), {'wolves': {}})
        self.assertEqual(runner.facts_at(runner.facts_page(self.conn, self.world, c, c), c), {'wolves': {'player-ash': {}}})
        self.assertEqual(runner.latest_facts(self.conn, self.world), {'wolves': {'player-ash': {}}})

    def test_the_runner_reads_each_event_with_its_own_facts(self):
        p = self.story()
        phase_order = self.conn.execute("SELECT payload FROM dm.actions WHERE kind='story.phase' ORDER BY id DESC LIMIT 1").fetchone()[0]
        self.assertEqual(phase_order['watch'], {'facts': ['wolves'], 'items': ['rope']}, 'the watch list goes with the phase')
        indoors = {'wolves': {'player-ash': {'cell': 'inn', 'x': 1, 'y': 1, 'indoors': True, 'carries': {}}}}
        roped = {'wolves': {'player-ash': {'cell': 'grey', 'x': 1, 'y': 1, 'indoors': False, 'carries': {'rope': 2}}}}
        # Two writes before the runner looks: the first event must be judged with the first write's facts, never the second's.
        self.write([{'actor': 'player-ash'}], indoors)
        self.write([{'actor': 'player-ash'}, {'actor': 'player-ash'}], roped)
        self.tick()
        self.assertEqual(self.held(), ['c1', 'c0'], 'indoors, then with rope (as of each event, never newer)')
        fired = [r[0] for r in self.conn.execute("SELECT entry->>'trigger' FROM dm.story_trigger_history WHERE run=%s ORDER BY id", (self.run_id,))]
        self.assertEqual(fired, ['c1', 'c0', 'c0'], 'the first event with the first write\'s facts, the next two with the second\'s')
        # A third write, after: as before, as of it.
        self.write([{'actor': 'player-birch'}], roped)
        self.tick()
        self.assertEqual(self.held(), ['c1', 'c0'], 'Birch is not in the facts: unknown, never fires')
        self.assertEqual(self.conn.execute('SELECT count(*) FROM dm.story_trigger_history WHERE run=%s', (self.run_id,)).fetchone()[0], 3)
        self.assertTrue(runner.wants_facts(p))

    def test_explain_and_why_not_read_the_facts(self):
        self.story()
        self.write([{'actor': 'player-ash'}], {'wolves': {'player-ash': {'cell': 'inn', 'indoors': True}}})
        self.tick()
        out = runner.explain(self.conn, self.world, self.run_id)
        self.assertEqual(out['watch'], {'facts': ['wolves'], 'items': ['rope']})
        self.assertEqual(set(out['facts']['wolves']), {'player-ash'})
        why = runner.why_not(self.conn, self.world, self.run_id, {'trigger': 'c0'})
        cond = why['events'][0]['conditions']
        self.assertEqual(why['events'][0]['verdict'], 'conditions')
        self.assertEqual((cond['test'], cond['verdict'], cond['fact'], cond['expected']), ('carries', False, 0, 1))
        what = runner.explain(self.conn, self.world, self.run_id, {'fact': {'kind': 'conversation', 'actor': 'player-ash'}})
        self.assertEqual([f['trigger'] for f in what['fired']], ['c1'], 'what if: the world as it is now (indoors, no rope)')
        self.assertNotIn('facts', what['fact'])


if __name__ == '__main__':
    unittest.main()
