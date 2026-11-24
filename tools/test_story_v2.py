"""The trigger language, version 2 (Docs/Design/85-story-triggers-and-campaign-control.md, 3.2, 3.4, 3.5 and Phase 5):
declared variables, the new conditions, flow inside a trigger's actions, the checks that refuse what is malformed (with
the Problems list pointing at a nested action), rehearsals with pinned rolls, waits and expected variables, and a
version 1 program read as before and made version 2 doing the same. The runner's side (a version 2 story published,
waiting, finishing, and keeping a world variable) is in tools/test_story_v2_runner.py; both engines agreeing is
tools/test_story_native.py."""

import copy
import json
import re
import unittest
from unittest.mock import patch

import story_library
import story_program as rules
import story_quests as quests
import story_runner as runner
import story_triggers as triggers
import story_v2_samples as samples
import studio


def errors(p):
    return [x for x in rules.problems(p) if x['level'] == 'error']


class Validation(unittest.TestCase):
    def setUp(self):
        self.p = samples.all_samples()[0]

    def trigger(self, phase=0, index=0):
        return self.p['phases'][phase]['triggers'][index]

    def refused(self, words):
        with self.assertRaisesRegex(ValueError, words):
            rules.validate(self.p)
        found = errors(self.p)
        self.assertTrue(found, 'problems() sees what validate() refuses')
        return found

    def test_the_samples_are_sound(self):
        for p in samples.all_samples():
            rules.validate(p)
            self.assertEqual(errors(p), [])

    def test_version_1_refuses_what_is_new(self):
        p = copy.deepcopy(story_library.draft(story_library.entries()[0]['id']))
        p['phases'][0].setdefault('triggers', []).append({'id': 'x', 'on': 'arrival', 'actions': [{'do': 'stop'}]})
        with self.assertRaisesRegex(ValueError, 'needs a version 2 story'):
            rules.validate(p)
        p['phases'][0]['triggers'][-1] = {'id': 'x', 'on': 'arrival', 'enabled': False, 'actions': [{'do': 'emit', 'signal': 's'}]}
        with self.assertRaisesRegex(ValueError, 'Only a version 2 story starts a trigger turned off'):
            rules.validate(p)
        p['phases'][0]['triggers'][-1] = {'id': 'x', 'on': 'arrival', 'if': {'test': 'chance', 'percent': 5},
                                          'actions': [{'do': 'emit', 'signal': 's'}]}
        with self.assertRaisesRegex(ValueError, 'needs a version 2 story'):
            rules.validate(p)
        p['phases'][0]['triggers'].pop()
        p['variables'] = []
        with self.assertRaisesRegex(ValueError, 'Declared variables need a version 2 story'):
            rules.validate(p)

    def test_undeclared_and_mistyped_variables(self):
        self.trigger()['actions'][0]['var'] = 'haul'
        found = self.refused('Declare the variable "haul" first')
        self.assertEqual(found[0]['path'], 'phases[0].triggers[0].actions[0]')
        self.p = samples.all_samples()[0]
        self.trigger()['actions'][1] = {'do': 'add', 'var': 'gate-open', 'value': 1}
        self.refused('Arithmetic is on a number variable')
        self.p = samples.all_samples()[0]
        self.trigger()['actions'][0] = {'do': 'set', 'var': 'motto', 'value': 3}
        self.refused('"motto" is a text: set it to a text')
        self.p = samples.all_samples()[0]
        self.trigger()['actions'][0] = {'do': 'add', 'scope': 'story', 'key': 'ropes', 'value': 1}
        self.refused('Set and Add name a declared variable')
        self.p = samples.all_samples()[0]
        self.trigger()['if'] = {'test': 'counter', 'key': 'ropes', 'scope': 'story', 'op': '>', 'value': 1}
        self.refused('compares declared variables')

    def test_declarations(self):
        for bad, words in [({'id': 'ropes', 'type': 'number', 'scope': 'story'}, 'unique name'),
                           ({'id': 'n', 'type': 'number', 'scope': 'town'}, 'a scope'),
                           ({'id': 'n', 'type': 'colour', 'scope': 'story'}, 'a type'),
                           ({'id': 'n', 'type': 'number', 'scope': 'story', 'initial': 'x'}, 'is not a number'),
                           ({'id': 'n', 'type': 'wolves', 'scope': 'story', 'initial': ['a', 'a']}, 'is not a wolves'),
                           ({'id': 'n', 'type': 'timer', 'scope': 'story', 'initial': 3}, 'a timer starts unset'),
                           ({'id': 'n', 'type': 'text', 'scope': 'story', 'show': 'Shown'}, 'may be shown'),
                           ({'id': 'n', 'type': 'region', 'scope': 'story', 'initial': 'nowhere'}, 'is not a region')]:
            with self.subTest(bad=bad):
                self.p = samples.all_samples()[0]
                self.p['variables'].append(bad)
                found = self.refused(words)
                self.assertTrue(any(f['at'].get('kind') == 'variable' for f in found), found)

    def test_flow_rules(self):
        cases = [
            # (where, the action, the words)
            (lambda p: p['phases'][1]['triggers'][0]['actions'][0]['actions'].append({'do': 'wait', 'days': 1}), 'A wait cannot be inside For each'),
            (lambda p: p['phases'][1]['triggers'][0]['actions'][0]['actions'].append(
                {'do': 'each', 'over': 'participants', 'actions': [{'do': 'stop'}]}), 'cannot be inside another'),
            (lambda p: p['phases'][0]['triggers'][0]['actions'][3]['then'].insert(0, {'do': 'enter', 'phase': 'gate'}), 'must be the last action'),
            (lambda p: p['phases'][0]['triggers'][0]['actions'].append({'do': 'effect', 'effect': {'kind': 'deed.award', 'player': '$each', 'detail': 'x'}}),
             r'\$each names a wolf'),
            (lambda p: p['phases'][0]['triggers'][2]['actions'].append({'do': 'run', 'trigger': 'talk'}), 'run one another in a cycle'),
            (lambda p: p['phases'][0]['triggers'][2]['actions'].append({'do': 'run', 'trigger': 'praise'}), 'Run another trigger'),
            (lambda p: p['phases'][0]['triggers'][2]['actions'].append({'do': 'enable', 'trigger': 'close'}), 'Name a trigger this one can see'),
            (lambda p: p['phases'][0]['triggers'][2]['actions'].append({'do': 'enable', 'trigger': 'nope', 'phase': 'gate'}), 'has no trigger nope'),
            (lambda p: p['phases'][1]['triggers'][1]['actions'].insert(0, {'do': 'finish', 'outcome': 'success', 'text': 'x'}), 'is the last action'),
            (lambda p: p['phases'][0]['triggers'][0].update(**{'if': {'test': 'phase is', 'phase': 'nowhere'}}), 'Unknown phase'),
            (lambda p: p['phases'][0]['triggers'][0]['actions'].append({'do': 'wait', 'days': 1, 'clock': 'real'}) or
             p['phases'][0]['triggers'][0]['actions'].append({'do': 'wait', 'days': 1 / 86400, 'clock': 'real'}), '5 seconds'),
            (lambda p: p['phases'][0]['triggers'][0]['actions'].append({'do': 'random', 'choices': [{'weight': 1, 'actions': [{'do': 'stop'}]}]}),
             'between 2 and 8'),
            (lambda p: p['phases'][0]['triggers'][0]['actions'].extend([{'do': 'note', 'text': 'n'}] * 13), 'at most 24 actions'),
            (lambda p: p['phases'][0]['triggers'][0]['actions'].append(
                {'do': 'if', 'if': {'test': 'chance', 'percent': 50}, 'then': [{'do': 'if', 'if': {'test': 'chance', 'percent': 50}, 'then': [
                    {'do': 'if', 'if': {'test': 'chance', 'percent': 50}, 'then': [{'do': 'if', 'if': {'test': 'chance', 'percent': 50},
                                                                                    'then': [{'do': 'stop'}]}]}]}]}), 'nest 3 deep'),
            (lambda p: p['phases'][0]['triggers'][0]['actions'].append(
                {'do': 'if', 'if': {'test': 'compare', 'left': {'var': 'ropes'}, 'op': '=', 'right': 'five'}, 'then': [{'do': 'stop'}]}),
             'Compare two values of one kind'),
            (lambda p: p['phases'][0]['triggers'][0]['actions'].append(
                {'do': 'if', 'if': {'test': 'compare', 'left': {'var': 'motto'}, 'op': '>', 'right': 'a'}, 'then': [{'do': 'stop'}]}),
             'switches and texts take = and !='),
        ]
        for change, words in cases:
            with self.subTest(words=words):
                self.p = samples.all_samples()[0]
                change(self.p)
                self.refused(words)

    def test_problems_point_at_a_nested_action(self):
        then = self.trigger()['actions'][3]['then']
        then[1] = {'do': 'remember', 'var': 'finder', 'from': 'colour'}
        self.trigger()['actions'][3]['elif'][0]['then'][1]['effect']['kind'] = 'deed.awardx'
        found = errors(self.p)
        self.assertEqual([f['path'] for f in found], ['phases[0].triggers[0].actions[3].then[1]',
                                                       'phases[0].triggers[0].actions[3].elif[0].then[1]'])
        self.assertEqual(found[0]['at'], {'phase': 0, 'phaseId': 'gather', 'kind': 'trigger', 'index': 0, 'id': 'rope',
                                          'part': 'actions[3].then[1]'})

    def test_warnings_for_a_variable_nothing_uses(self):
        self.p['variables'].append({'id': 'unused', 'type': 'switch', 'scope': 'story'})
        warned = [x for x in rules.problems(self.p) if x['level'] == 'warning' and 'unused' in x['text']]
        self.assertEqual(warned[0]['path'], f'variables[{len(self.p["variables"]) - 1}]')

    def test_effects_inside_flow_are_seen_by_the_permission_checks(self):
        kinds = studio.story_effect_kinds(self.p)
        self.assertEqual(kinds, {'deed.award'})
        effects = [e['kind'] for ph in rules.phases_of(self.p) for e in rules.possible_effects(ph)]
        self.assertEqual(effects, ['deed.award', 'deed.award'])
        self.assertEqual({e['to'] for ph in rules.phases_of(self.p) for e in triggers.routes(ph)}, {'gate'})


class Rehearsal(unittest.TestCase):
    def test_every_sample_case_passes_with_what_it_pins(self):
        for p in samples.all_samples():
            report = rules.review(p)
            self.assertTrue(report['passed'], [(t['name'], t['path']) for t in report['tests']])
            self.assertEqual(report['uncovered'], [])

    def test_expected_variables_and_outcome_fail_when_wrong(self):
        p = samples.all_samples()[0]
        case = p['tests'][0]
        case['expectVariables']['player']['ash']['hauled'] = 7
        self.assertFalse(rules.review(p)['passed'])
        p = samples.all_samples()[0]
        p['tests'][0]['expectOutcome'] = 'failure'
        self.assertFalse(rules.review(p)['passed'])
        p = samples.all_samples()[0]
        p['tests'][0]['expectVariables'] = {'story': {'nonesuch': 1}}
        with self.assertRaisesRegex(ValueError, 'No story variable "nonesuch"'):
            rules.review(p)

    def test_the_outcome_and_each_wolfs_own_ending(self):
        case = rules.review(samples.all_samples()[0])['tests'][0]
        self.assertEqual(case['outcome'], {'outcome': 'success', 'text': 'The rope ledger is closed.', 'day': 4.5})
        self.assertEqual(case['outcomes'], {'birch': {'outcome': 'success', 'text': 'Saw the gate shut behind the last rope.', 'day': 4.5}})
        notes = [n for f in case['triggers'] for n in f.get('notes', [])]
        self.assertEqual(notes, ['More rope wanted.', 'Two have signed.', 'The gate opens.'])
        self.assertEqual([f.get('via') for f in case['triggers'] if f['trigger'] == 'praise'], ['run'], 'run while it was off: skipped')

    def test_rolls_are_pinned_or_seeded(self):
        p = samples.all_samples()[1]
        pinned = rules.review(p)['tests'][0]
        self.assertEqual(pinned['counters']['story']['chosen'], 'alder')
        p['tests'][0]['rolls'] = [10, 90, 99]
        again = rules.review(p)['tests'][0]
        self.assertEqual((again['counters']['story']['luck'], again['counters']['story']['chosen']), ('low', 'cress'))
        seeded = [rules.review({**p, 'tests': [{**p['tests'][1], 'seed': s, 'expect': ['watch', 'dawn']}]})['tests'][0]['counters']
                  for s in ('night', 'night', 'day')]
        self.assertEqual(seeded[0], seeded[1], 'a seed always gives the same rolls')
        self.assertEqual(triggers.fnv('a'), 0xaf63dc4c8601ec8c, 'FNV-1a, 64 bits (the native twin has the same)')

    def test_waits_resume_in_time_and_end_with_the_visit(self):
        p = samples.all_samples()[1]
        trace = rules.review(p)['tests'][0]['triggers']
        resumed = [(f['trigger'], f['day']) for f in trace if f.get('via') == 'wait']
        self.assertEqual(resumed, [('campaign-bell', 2.1), ('call', 2.1)])
        # A phase trigger's wait ends with its visit; a campaign trigger's goes on.
        state = rules.begin(p, samples.W, 0)
        phase = rules.phase_of(p, state)
        state['waiting'] = [{'trigger': 'call', 'scope': 'visit', 'phase': 'watch', 'at': 9, 'clock': 'game', 'actor': 'ash', 'event': {}, 'next': [5]},
                            {'trigger': 'campaign-bell', 'scope': 'campaign', 'phase': 'watch', 'at': 9, 'clock': 'game', 'actor': 'ash',
                             'event': {}, 'next': [2]}]
        rules.leave_visit(phase, state, 'dawn', 1, 'ash')
        self.assertEqual([w['trigger'] for w in state['waiting']], ['campaign-bell'])

    def test_initially_off_triggers_and_counts(self):
        p = samples.all_samples()[0]
        state = rules.begin(p, samples.W, 0, 'seed')
        self.assertEqual(state['disabled'], ['gather/praise'])
        self.assertEqual((state['seed'], state['began']), ('seed', 0))
        self.assertNotIn('seed', rules.begin(story_library.draft(story_library.entries()[0]['id']), samples.W, 0))

    def test_step_through_shows_the_variables(self):
        p = samples.all_samples()[0]
        steps = runner.rehearse_steps(p, 0)['steps']
        self.assertEqual(steps[-1]['counters']['world'], {'best': 5})
        self.assertTrue(any(i.get('startsOff') for i in steps[0]['listening']))


class Upgrade(unittest.TestCase):
    def test_version_1_programs_rehearse_the_same_as_version_2(self):
        programs = list(runner.catalogue()['examples'].values())
        programs.extend(story_library.draft(e['id']) for e in story_library.entries())
        with_counters = 0
        for p in programs:
            with self.subTest(program=p['id']):
                q = rules.upgrade(p)
                self.assertEqual(q['version'], 2)
                with_counters += bool(q.get('variables'))
                a, b = rules.review(p), rules.review(q)
                for r in (a, b):
                    r.pop('digest')
                    for t in r['tests']:
                        t.pop('outcome', None), t.pop('outcomes', None)
                self.assertEqual(a, b)
                self.assertEqual(rules.upgrade(q), q, 'a version 2 program comes back as it is')
        self.assertGreaterEqual(with_counters, 3)

    def test_counters_become_declared_numbers(self):
        p = runner.catalogue()['examples']['siege-at-greyfen']
        if p['version'] == 2:
            self.skipTest('the example is version 2 already')
        q = rules.upgrade(p)
        self.assertEqual(q['variables'], [{'id': 'defeats', 'type': 'number', 'scope': 'player'},
                                          {'id': 'line-held', 'type': 'number', 'scope': 'story'},
                                          {'id': 'guard-posts', 'type': 'number', 'scope': 'story'}])
        self.assertEqual(json.dumps(q).count('"counter"'), 0)

    def test_a_counter_in_both_scopes(self):
        p = copy.deepcopy(samples.all_samples()[0])
        v1 = story_library.draft(story_library.entries()[0]['id'])
        v1['phases'][0].setdefault('triggers', []).append({'id': 'both', 'on': 'arrival', 'audience': 'participant', 'actions': [
            {'do': 'add', 'scope': 'story', 'key': 'n', 'value': 1}, {'do': 'add', 'scope': 'player', 'key': 'n', 'value': 1}]})
        q = rules.upgrade(v1)
        self.assertEqual([(d['id'], d['scope']) for d in q['variables']], [('n', 'story'), ('n.player', 'player')])
        del p


class Judging(unittest.TestCase):
    def test_judge_agrees_with_the_engine_on_version_2_conditions(self):
        p = samples.all_samples()[0]
        state = rules.begin(p, samples.W, 0)
        state['counters'] = {'story': {'ropes': 4, 'gate-open': True}, 'player': {'ash': {'hauled': 2}}}
        phase = rules.phase_of(p, state)
        event = {'kind': 'gift', 'actor': 'ash', 'quantity': 1}
        for c in [{'test': 'compare', 'left': {'var': 'ropes'}, 'op': '>=', 'right': 4},
                  {'test': 'compare', 'left': {'plus': [{'var': 'ropes'}, {'field': 'quantity'}]}, 'op': '=', 'right': 5},
                  {'test': 'switch', 'var': 'gate-open', 'is': False},
                  {'test': 'compare', 'left': {'var': 'hauled'}, 'op': '=', 'right': 2},
                  {'test': 'days since', 'from': 'phase', 'op': '>=', 'days': 1},
                  {'test': 'visit is', 'op': '=', 'value': 1}]:
            with self.subTest(c=c):
                judged = triggers.judge(c, state, event, lambda: set(), None, None, phase, 3)
                self.assertEqual(judged['verdict'], triggers.satisfied(c, state, event, lambda: set(), None, phase, 3))
        stand_in = triggers.judge({'test': 'compare', 'left': {'var': 'hauled'}, 'op': '=', 'right': 2}, state, {'kind': 'gift'},
                                  lambda: set(), None, set(), phase, 3)
        self.assertEqual((stand_in['verdict'], stand_in['needs']), (None, 'wolf'))
        chance = triggers.judge({'test': 'chance', 'percent': 30}, state, event, lambda: set(), None, None, phase, 3)
        self.assertEqual((chance['verdict'], chance['needs']), (None, 'roll'))
        self.assertNotIn('draws', state, 'judging never rolls')


# ----- Doc 86, Phase 1a: C2 (the audience counted, times, div) and R1 (who took part), in the Python twin; both twins agreeing
# is tools/test_story_native.py's.
def counting(cohort, seen=None, did=(), counters=None, online=None, active_days=None, **state):
    """A version 2 story of these wolves, a firing context on game day 20, and the state it reads."""
    p = {'format': 'ratw-story-program', 'version': 2, 'id': 'count', 'title': 'Counting', 'start': 'a',
         'audience': {'kind': 'open'}, 'variables': [{'id': 'did', 'type': 'wolves', 'scope': 'story'},
                                                       {'id': 'n', 'type': 'number', 'scope': 'player'}],
         'phases': [{'id': 'a', 'journal': 'Count.', 'effects': [], 'exits': []}], 'tests': []}
    if active_days:
        p['activeDays'] = active_days
    s = rules.begin(p, cohort, 0)
    s['seen'] = dict(seen if seen is not None else {w: 20 for w in cohort})
    s['counters'] = {'story': {'did': list(did)}, **(counters or {})}
    if online is not None:
        s['online'] = online
    s.update(state)
    phase = rules.phase_of(p, s)
    return triggers.Ctx(phase, s, {'kind': 'x'}, '', 20, 0, lambda: set(), None)


WOLVES12 = [f'w{i:02d}' for i in range(12)]


def part(percent, floor=None, cap=None, of='active', src=None):
    c = {'test': 'participation', **(src or {'list': 'did'}), 'of': of, 'percent': percent}
    if floor is not None:
        c['floor'] = floor
    if cap is not None:
        c['cap'] = cap
    return c


class Audience(unittest.TestCase):
    def test_each_form_and_arithmetic(self):
        x = counting(WOLVES12, seen={w: 20 if i < 8 else 1 for i, w in enumerate(WOLVES12)}, online=WOLVES12[:3])
        self.assertEqual([triggers.value_of({'audience': k}, x) for k in ('all', 'active', 'online')], [12, 8, 3])
        self.assertEqual(triggers.value_of({'times': [{'audience': 'active'}, 2]}, x), 16)
        self.assertEqual(triggers.value_of({'div': [{'audience': 'all'}, 5]}, x), 2)
        self.assertEqual(triggers.value_of({'div': [-7, 2]}, x), -3, 'floored toward zero')
        self.assertIs(triggers.value_of({'div': [{'audience': 'all'}, 0]}, x), triggers.MISSING, 'by 0: no value')
        self.assertIs(counting(WOLVES12).audience()['online'], triggers.MISSING, 'online not known')

    def test_active_days_3_against_18(self):
        seen = {w: 20 - i * 2 for i, w in enumerate(WOLVES12)}   # (Seen 0, 2, 4 ... 22 game days ago.)
        self.assertEqual(counting(WOLVES12, seen=seen).audience()['active'], 10, '18 days: seen 0-18 days ago')
        self.assertEqual(counting(WOLVES12, seen=seen, active_days=3).audience()['active'], 2, '3 days: seen 0 or 2 days ago')

    def test_a_case_overrides_the_counts(self):
        x = counting(WOLVES12, rehearsalAudience={'all': 400, 'active': 120, 'online': 31})
        self.assertEqual([triggers.value_of({'audience': k}, x) for k in ('all', 'active', 'online')], [400, 120, 31])
        p = samples.all_samples()[2]
        self.assertTrue(rules.review(p)['passed'])
        case = next(c for c in p['tests'] if 'activeWolves' in c)
        self.assertEqual(case['audience'], {'active': 3})

    def test_the_checks(self):
        p = samples.all_samples()[2]
        rope = p['phases'][0]['triggers'][0]['actions']
        for bad, words in [({'audience': 'quiet'}, 'all, active or online'),
                           ({'times': [{'audience': 'all'}, 'x']}, 'Arithmetic is on numbers'),
                           ({'percent': {'list': 'hauls'}, 'of': 'active'}, 'list of wolves'),
                           ({'percent': {'list': 'did', 'where': {'test': 'switch', 'var': 'cheered', 'is': True}}}, 'one of the two'),
                           ({'max': [1, {'plus': [1, {'among': {'list': 'did'}}]}]}, 'one level deep')]:
            q = copy.deepcopy(p)
            q['phases'][0]['triggers'][0]['actions'][3]['value'] = bad
            with self.subTest(bad=bad), self.assertRaisesRegex(ValueError, words):
                rules.validate(q)
        self.assertTrue(rope)
        q = copy.deepcopy(p)
        q['activeDays'] = 121
        self.assertRaisesRegex(ValueError, '1–120 game days', rules.validate, q)


class Participation(unittest.TestCase):
    def test_twelve_active_five_in_the_list(self):
        x = counting(WOLVES12, did=WOLVES12[:5])
        self.assertEqual(triggers.value_of({'percent': {'list': 'did'}, 'of': 'active'}, x), 41)
        self.assertEqual(triggers.value_of({'among': {'list': 'did'}, 'of': 'active'}, x), 5)

    def test_the_floor_and_the_cap(self):
        self.assertFalse(triggers.test_v2(part(40, floor=6), counting(WOLVES12, did=WOLVES12[:5])), '40% with floor 6: need 6')
        self.assertTrue(triggers.test_v2(part(40, floor=4), counting(WOLVES12, did=WOLVES12[:5])), 'floor 4: need 5')
        two = counting(WOLVES12, seen={WOLVES12[0]: 20, WOLVES12[1]: 20}, did=WOLVES12[:2])
        self.assertTrue(triggers.test_v2(part(35, floor=3), two), '2 active, floor 3: need 2 (never more than are active)')
        many = [f'm{i:04d}' for i in range(2000)]
        self.assertTrue(triggers.test_v2(part(35, floor=3, cap=3), counting(many, did=many[:3])), 'a cap of 3 is always enough')
        self.assertEqual([triggers.share_need(d, 35, 5, 150) for d in (1, 4, 10, 118, 1000)], [1, 4, 5, 42, 150])

    def test_no_active_wolves_and_online_unknown(self):
        x = counting(WOLVES12, seen={}, did=WOLVES12[:5])
        self.assertEqual(triggers.value_of({'percent': {'list': 'did'}, 'of': 'active'}, x), 0)
        self.assertFalse(triggers.test_v2(part(10), x))
        self.assertIsNone(triggers.test_v2(part(10, of='online'), counting(WOLVES12, did=WOLVES12)), 'online not known: unknown')
        self.assertIs(triggers.value_of({'among': {'list': 'did'}, 'of': 'online'}, counting(WOLVES12)), triggers.MISSING)
        self.assertTrue(triggers.test_v2(part(50, of='online'), counting(WOLVES12, did=WOLVES12[:2], online=WOLVES12[:3])))

    def test_a_test_for_each_wolf(self):
        counters = {'player': {w: {'n': i} for i, w in enumerate(WOLVES12)}}
        where = {'where': {'test': 'compare', 'left': {'var': 'n', 'of': '$each'}, 'op': '>=', 'right': 9}}
        x = counting(WOLVES12, counters=counters)
        self.assertEqual(triggers.value_of({'among': where, 'of': 'all'}, x), 3)
        self.assertEqual(triggers.value_of({'percent': where, 'of': 'all'}, x), 25)
        self.assertTrue(triggers.test_v2(part(25, src=where), x))
        self.assertIsNone(x.each, 'the loop value is put back')

    def test_a_wolf_who_went_quiet_leaves_both_counts(self):
        seen = {w: 20 for w in WOLVES12}
        x = counting(WOLVES12, seen=seen, did=WOLVES12[:6])
        self.assertEqual(triggers.value_of({'percent': {'list': 'did'}, 'of': 'active'}, x), 50)
        seen.update({w: 1 for w in WOLVES12[:4]})   # (Four who did it went quiet.)
        x = counting(WOLVES12, seen=seen, did=WOLVES12[:6])
        self.assertEqual((triggers.value_of({'among': {'list': 'did'}, 'of': 'active'}, x), x.audience()['active']), (2, 8))
        self.assertEqual(triggers.value_of({'percent': {'list': 'did'}, 'of': 'active'}, x), 25)

    def test_judge_shows_the_counts(self):
        x = counting(WOLVES12, did=WOLVES12[:5])
        judged = triggers.judge(part(40, floor=4), x.state, {'kind': 'x'}, lambda: set(), None, None, x.phase, 20)
        self.assertEqual((judged['verdict'], judged['fact']), (True, {'among': 5, 'of': 12, 'need': 5}))



# ------------------------------------------------------------------- doc 86, Phase 2: values in effects, fills, the purse, the moon
ASH, BIRCH = samples.ASH, samples.BIRCH


def _values_program(actions, variables=(), match=None, audience='participant', extra=None):
    p = {'format': 'ratw-story-program', 'version': 2, 'id': 'p2', 'title': 'Values', 'start': 'a',
         'audience': {'kind': 'players', 'players': [ASH, BIRCH]},
         'variables': [{'id': 'p_voice', 'type': 'number', 'scope': 'player'}, {'id': 'guide', 'type': 'resident', 'scope': 'story'},
                       {'id': 'finder', 'type': 'wolf', 'scope': 'story'}, {'id': 'n', 'type': 'number', 'scope': 'story'},
                       {'id': 'word', 'type': 'text', 'scope': 'story'}, *variables],
         'phases': [{'id': 'a', 'journal': 'A.', 'effects': [], 'exits': [], 'triggers': [
             {'id': 't', 'on': 'conversation', 'audience': audience, **({'match': match} if match else {}), 'actions': actions}]}],
         'tests': [{'name': 'x', 'cohort': [ASH, BIRCH], 'events': [], 'expect': ['a']}]}
    if extra:
        p.update(extra)
    return p


def _fire(p, state_counters=None, event=None, native=False):
    rules.validate(p)
    state = rules.begin(p, [ASH, BIRCH], 0)
    if state_counters:
        state['counters'] = copy.deepcopy(state_counters)
    phase = rules.phase_of(p, state)
    trace = []
    ev = {'kind': 'conversation', 'id': 1, 'actor': ASH, 'target': 'rm111', **(event or {})}
    if native:
        triggers.fire(phase, state, ev, 1, 0, lambda: set(), rules.matches, lambda n, e: None, trace)
    else:
        with patch('story_native.interpreter', return_value=None):
            triggers.fire(phase, state, ev, 1, 0, lambda: set(), rules.matches, lambda n, e: None, trace)
    return state.get('triggerEffects', []), trace


class Phase2Values(unittest.TestCase):
    def test_coins_from_a_value_clamped_and_floored(self):
        grant = lambda coins: {'do': 'effect', 'effect': {'kind': 'character.grant', 'player': '$actor', 'purse': 'town:greyfen',
                                                           'reason': 'r', 'coins': coins}}
        p = _values_program([grant({'min': [20, {'var': 'p_voice', 'of': '$actor'}]})])
        effects, _ = _fire(p, {'player': {ASH: {'p_voice': 7}}})
        self.assertEqual(effects[0]['coins'], 7)
        effects, _ = _fire(p, {'player': {ASH: {'p_voice': 90}}})
        self.assertEqual(effects[0]['coins'], 20)
        p = _values_program([grant({'times': [{'var': 'p_voice', 'of': '$actor'}, 1000]})])
        self.assertEqual(_fire(p, {'player': {ASH: {'p_voice': 9}}})[0][0]['coins'], 2500, 'kept to the parameter\'s most')
        p = _values_program([grant({'div': [{'var': 'n'}, 3]})])
        self.assertEqual(_fire(p, {'story': {'n': 8}})[0][0]['coins'], 2, 'floored')
        self.assertEqual(_fire(p, {'story': {'n': 0}})[0][0]['coins'], 1, 'kept to the least')

    def test_a_missing_value_skips_with_a_note(self):
        p = _values_program([{'do': 'effect', 'effect': {'kind': 'npc.walk', 'npc': {'var': 'guide'}, 'cell': 'grey', 'x': 3, 'y': 4,
                                                          'pace': 'walk', 'cue': 'c'}}])
        effects, trace = _fire(p)
        self.assertEqual(effects, [])
        self.assertIn('skipped: npc has no value', trace[0]['notes'])
        effects, _ = _fire(p, {'story': {'guide': 'rm112'}})
        self.assertEqual(effects[0]['npc'], 'rm112', 'a resident from a variable')
        p = _values_program([{'do': 'effect', 'effect': {'kind': 'deed.award', 'player': {'field': 'target'}, 'detail': 'x'}}])
        self.assertEqual(_fire(p, event={'target': ASH})[0][0]['player'], ASH, 'a wolf from the event')
        p = _values_program([{'do': 'effect', 'effect': {'kind': 'story.ping', 'cell': 'grey', 'x': {'var': 'n'}, 'y': 4, 'player': '$actor',
                                                          'label': 'here', 'minutes': {'var': 'n'}}}])
        effects, _ = _fire(p, {'story': {'n': 0}})
        self.assertEqual((effects[0]['x'], effects[0]['minutes']), (0, 1), 'x may be 0; minutes is kept to its least')

    def test_the_kinds_are_checked(self):
        bad = _values_program([{'do': 'effect', 'effect': {'kind': 'npc.walk', 'npc': {'var': 'finder'}, 'cell': 'grey', 'x': 3, 'y': 4,
                                                            'pace': 'walk', 'cue': 'c'}}])
        with self.assertRaisesRegex(ValueError, '"finder" is a wolf: the resident needs a resident'):
            rules.validate(bad)
        self.assertTrue(errors(bad))
        bad = _values_program([{'do': 'effect', 'effect': {'kind': 'item.give', 'player': '$actor', 'item': 'bread', 'quantity': {'var': 'word'}}}])
        with self.assertRaisesRegex(ValueError, 'is a number'):
            rules.validate(bad)
        bad = _values_program([{'do': 'effect', 'effect': {'kind': 'item.give', 'player': '$actor', 'item': 'bread', 'quantity': 2}}])
        bad['phases'][0]['effects'] = [{'kind': 'item.give', 'player': 'player-ash', 'item': 'bread', 'quantity': {'var': 'n'}}]
        with self.assertRaisesRegex(ValueError, 'invalid item.give.quantity'):
            rules.validate(bad)   # (What a phase sends as it opens is literal.)
        bad = _values_program([{'do': 'effect', 'effect': {'kind': 'npc.line', 'npc': {'var': 'guide'}, 'text': 'x'}}])
        with self.assertRaises(ValueError):
            rules.validate(bad)

    def test_fills_in_words(self):
        post = lambda text: [{'do': 'remember', 'var': 'finder', 'from': 'actor'}, {'do': 'set', 'var': 'n', 'value': 34},
                             {'do': 'effect', 'effect': {'kind': 'board.post', 'town': 'greyfen', 'days': 2, 'text': text}}]
        p = _values_program(post('{finder} counted {n}; {{braces}} stay, {p_voice.of} is theirs.'))
        effects, _ = _fire(p, {'player': {ASH: {'p_voice': 3}}})
        self.assertEqual(effects[0]['text'], '{@w:' + ASH + '} counted 34; {braces} stay, 3 is theirs.')
        with self.assertRaisesRegex(ValueError, 'Declare the variable "nobody" first'):
            rules.validate(_values_program(post('{nobody} came.')))
        with self.assertRaisesRegex(ValueError, 'each wolf\'s own: write {p_voice.of}'):
            rules.validate(_values_program(post('{p_voice} came.')))
        with self.assertRaisesRegex(ValueError, 'only a variable of each wolf'):
            rules.validate(_values_program(post('{n.of} came.')))
        with self.assertRaisesRegex(ValueError, 'A lone'):
            rules.validate(_values_program(post('a { lone brace')))
        with self.assertRaisesRegex(ValueError, 'too long once filled'):
            rules.validate(_values_program(post('{word} ' + 'x' * 100)))   # (A text fills at its widest, 200: 301 of 300.)
        rules.validate(_values_program(post('{word} ' + 'x' * 99)))
        p = _values_program(post('ok'))
        p['phases'][0]['effects'] = [{'kind': 'board.post', 'town': 'greyfen', 'days': 2, 'text': 'Opened by {finder}.'}]
        with self.assertRaisesRegex(ValueError, 'fills words in a trigger'):
            rules.validate(p)
        p = _values_program(post('{word} said nothing.'))
        effects, trace = _fire(p)
        self.assertEqual(effects[0]['text'], ' said nothing.', 'an empty text fills as nothing')

    def test_a_purse_read_now(self):
        grant = {'do': 'effect', 'effect': {'kind': 'character.grant', 'player': '$actor', 'purse': 'town:greyfen', 'reason': 'r',
                                            'coins': {'min': [12, {'div': [{'purse': 'town:greyfen'}, 20]}]}}}
        p = _values_program([grant])
        effects, _ = _fire(p, event={'facts': {'purses': {'town:greyfen': 212}}})
        self.assertEqual(effects[0]['coins'], 10, 'min(12, 212 / 20)')
        effects, trace = _fire(p, event={'facts': {'purses': {}}})
        self.assertEqual(effects, [])
        self.assertIn('skipped: coins has no value', trace[0]['notes'])
        self.assertEqual(rules.watch_of(p)['purseFacts'], ['town:greyfen'])
        self.assertIn('purses', rules.watch_of(p)['facts'])
        bad = _values_program([{'do': 'set', 'var': 'n', 'value': {'purse': 'nowhere'}}])
        with self.assertRaisesRegex(ValueError, 'Name a purse'):
            rules.validate(bad)

    def test_a_mirror_reads_the_world_number_a_wolf_names(self):
        variables = [{'id': 'p_town', 'type': 'text', 'scope': 'player'}, {'id': 'v_grey', 'type': 'number', 'scope': 'world'},
                     {'id': 'v_cold', 'type': 'number', 'scope': 'world'},
                     {'id': 'p_howl', 'type': 'number', 'scope': 'player', 'show': 'Your town\'s howl',
                      'mirror': {'key': 'p_town', 'names': {'greyfen': 'v_grey', 'coldwater': 'v_cold'}}}]
        p = _values_program([{'do': 'set', 'var': 'n', 'value': {'var': 'p_howl', 'of': '$actor'}}], variables)
        counters = {'player': {ASH: {'p_town': 'greyfen'}, BIRCH: {'p_town': 'coldwater'}}, 'world': {'v_grey': 5, 'v_cold': 9}}
        state = rules.begin(p, [ASH, BIRCH], 0)
        state['counters'] = counters
        self.assertEqual(quests.trackers(p, state, ASH), [{'label': 'Your town\'s howl', 'value': 5}])
        self.assertEqual(quests.trackers(p, state, BIRCH), [{'label': 'Your town\'s howl', 'value': 9}])
        state['counters'] = {'world': {'v_grey': 5}}
        self.assertEqual(quests.trackers(p, state, ASH)[0]['value'], 0, 'no town yet: 0')
        self.assertEqual(_fire(p, counters)[1][0]['trigger'], 't')
        state = {**rules.begin(p, [ASH, BIRCH], 0), 'counters': counters}
        x = triggers.Ctx(rules.phase_of(p, state), state, {}, BIRCH, 1, 0, lambda: set(), None)
        self.assertEqual(triggers.value_of({'var': 'p_howl', 'of': '$actor'}, x), 9, 'triggers read it as any variable')
        with self.assertRaisesRegex(ValueError, 'mirrors a world variable: it is read, never set'):
            rules.validate(_values_program([{'do': 'set', 'var': 'p_howl', 'value': 3}], variables))
        bad = copy.deepcopy(variables)
        bad[-1]['mirror']['names']['greyfen'] = 'n'
        with self.assertRaisesRegex(ValueError, 'mirrors declared world numbers'):
            rules.validate(_values_program([{'do': 'stop'}], bad))
        told = copy.deepcopy(variables)
        told[-1]['talk'] = True
        with self.assertRaisesRegex(ValueError, 'a mirror is not told to residents'):
            rules.validate(_values_program([{'do': 'stop'}], told))
        # (Nor does the console's Set change one: it holds nothing of its own.)
        with self.assertRaisesRegex(ValueError, 'mirrors a world number: set that number instead'):
            runner._check_payload(p, 'set', {'key': 'p_howl', 'scope': 'player', 'player': ASH, 'value': 5})
        runner._check_payload(p, 'set', {'key': 'v_grey', 'scope': 'world', 'value': 5})

    def test_a_tracker_of_another(self):
        variables = [{'id': 'bar', 'type': 'number', 'scope': 'story', 'show': 'This chapter', 'showOf': 'goal'},
                     {'id': 'goal', 'type': 'number', 'scope': 'story'}]
        p = _values_program([{'do': 'stop'}], variables)
        rules.validate(p)
        state = rules.begin(p, [ASH], 0)
        state['counters'] = {'story': {'bar': 34, 'goal': 80}}
        self.assertEqual(quests.trackers(p, state, ASH), [{'label': 'This chapter', 'value': 34, 'goal': 80}])
        bad = copy.deepcopy(variables)
        bad[1] = {'id': 'goal', 'type': 'number', 'scope': 'player'}
        with self.assertRaisesRegex(ValueError, 'cannot read'):
            rules.validate(_values_program([{'do': 'stop'}], bad))

    def test_filters_that_begin_with_or_are_one_of(self):
        p = _values_program([{'do': 'add', 'var': 'n', 'value': 1}], match={'target': {'in': ['rm111', 'rm112']}})
        self.assertEqual(_fire(p, event={'target': 'rm112'})[1][0]['trigger'], 't')
        self.assertEqual(_fire(p, event={'target': 'rm113'})[1], [])
        deed = copy.deepcopy(p)
        deed['phases'][0]['triggers'][0].update(on='deed', match={'detail': {'starts': 'stood watch at the ford'}})
        rules.validate(deed)
        ev = lambda detail: {'kind': 'deed', 'id': 1, 'actor': ASH, 'detail': detail}
        self.assertTrue(rules.matches(deed['phases'][0]['triggers'][0], ev('stood watch at the ford (d12)')))
        self.assertFalse(rules.matches(deed['phases'][0]['triggers'][0], ev('Stood watch at the ford (d12)')), 'case-sensitive')
        gift = {'on': 'gift', 'match': {'item': {'in': ['rope', 'axle']}}}
        self.assertTrue(rules.matches(gift, {'kind': 'gift', 'item': 'rope~fine'}), 'the gift item rule inside "in"')
        self.assertFalse(rules.matches({'on': 'gift', 'match': {'item': {'in': ['rope~fine']}}}, {'kind': 'gift', 'item': 'rope'}))
        cond = _values_program([{'do': 'add', 'var': 'n', 'value': 1}])
        cond['phases'][0]['triggers'][0]['if'] = {'test': 'field', 'field': 'target', 'op': 'starts', 'value': 'rm11'}
        self.assertTrue(_fire(cond)[1])
        cond['phases'][0]['triggers'][0]['if'] = {'test': 'field', 'field': 'target', 'op': 'in', 'value': ['rm2', 'rm3']}
        self.assertFalse(_fire(cond)[1])
        for bad, words in [({'target': {'starts': ''}}, 'Invalid trigger filters'), ({'target': {'in': []}}, 'Invalid trigger filters'),
                           ({'target': {'in': ['x'] * 33}}, 'Invalid trigger filters'), ({'target': {'begins': 'x'}}, 'Invalid trigger filters')]:
            with self.assertRaisesRegex(ValueError, words):
                rules.validate(_values_program([{'do': 'stop'}], match=bad))
        near = _values_program([{'do': 'stop'}])
        near['phases'][0]['triggers'][0].update(on='near resident', match={'target': {'in': ['rm111']}})
        with self.assertRaisesRegex(ValueError, 'as exact words'):
            rules.validate(near)
        cond['phases'][0]['triggers'][0]['if'] = {'test': 'field', 'field': 'coins', 'op': 'in', 'value': ['3']}
        with self.assertRaisesRegex(ValueError, 'text fields'):
            rules.validate(cond)

    def test_an_exit_on_a_watched_event_names_exact_words(self):
        # (Review of Phase 2: an exit's "one of" passed approval, and the watch then failed on every tick.)
        for on, match in [('near resident', {'target': {'in': ['rm111', 'rm112']}}),
                          ('purse crossed', {'target': {'starts': 'town:'}, 'detail': '100'}),
                          ('standing crossed', {'target': 'wardens', 'detail': {'in': ['5', '10']}})]:
            p = _values_program([{'do': 'stop'}])
            p['phases'].append({'id': 'b', 'journal': 'B.', 'effects': [], 'exits': []})
            p['phases'][0]['exits'] = [{'on': on, 'to': 'b', 'match': match}]
            with self.subTest(on), self.assertRaisesRegex(ValueError, 'as exact words'):
                rules.validate(p)
            self.assertNotIn('near', rules.watch_of(p), 'the watch passes over a filter it cannot look for')
        p['phases'][0]['exits'] = [{'on': 'purse crossed', 'to': 'b', 'match': {'target': 'town:greyfen', 'detail': 'lots'}}]
        with self.assertRaisesRegex(ValueError, 'names a whole number to cross'):
            rules.validate(p)
        p['phases'][0]['exits'] = [{'on': 'near resident', 'to': 'b', 'match': {'target': 'rm111'}}]
        rules.validate(p)
        self.assertEqual(rules.watch_of(p)['near'], ['rm111'])

    def test_the_moon(self):
        p = _values_program([{'do': 'add', 'var': 'n', 'value': 1}])
        p['phases'][0]['triggers'][0]['if'] = {'test': 'calendar is', 'what': 'moon', 'value': 'full'}
        self.assertTrue(_fire(p, event={'facts': {'world': {'moon': 'full'}}})[1])
        self.assertFalse(_fire(p, event={'facts': {'world': {'moon': 'new'}}})[1])
        self.assertFalse(_fire(p)[1], 'no facts: unknown, never fires')
        p['phases'][0]['triggers'][0]['if']['value'] = 'blue'
        with self.assertRaisesRegex(ValueError, 'or the moon'):
            rules.validate(p)
        moon = _values_program([{'do': 'stop'}])
        moon['phases'][0]['triggers'][0].update(on='moon', match={'detail': 'new'})
        rules.validate(moon)
        self.assertIn('moon', rules.watch_of(moon)['events'])
        for detail in ('ful', {'in': ['full', 'gibbous']}, {'starts': 'wa'}):
            moon['phases'][0]['triggers'][0]['match'] = {'detail': detail}
            with self.subTest(detail), self.assertRaisesRegex(ValueError, 'the moon is new, waxing, full or waning'):
                rules.validate(moon)
        moon['phases'][0]['triggers'][0]['match'] = {'detail': {'in': ['full', 'new']}}
        rules.validate(moon)
        with self.assertRaisesRegex(ValueError, 'the moon is new, waxing, full or waning'):
            rules.check_world_facts({'world': {'moon': 'blue'}})

    def test_a_camp_named_with_the_story_prefix(self):
        spawn = lambda camp: [{'do': 'effect', 'effect': {'kind': 'bandits.call', 'player': '$actor', 'count': 2, 'camp': camp}}]
        with self.assertRaisesRegex(ValueError, 'declares its prefix'):
            rules.validate(_values_program(spawn('sq_alder')))
        with self.assertRaisesRegex(ValueError, 'begins with the story\'s prefix'):
            rules.validate(_values_program(spawn('alder_mere'), extra={'prefix': 'sq'}))
        with self.assertRaisesRegex(ValueError, 'prefix is 2 to 8 small letters'):
            rules.validate(_values_program(spawn('sq_alder'), extra={'prefix': 'SQ'}))
        with self.assertRaisesRegex(ValueError, 'invalid bandits.call.camp'):
            rules.validate(_values_program(spawn('sq_Alder Mere'), extra={'prefix': 'sq'}))
        p = _values_program(spawn('sq_alder'), extra={'prefix': 'sq'})
        rules.validate(p)
        self.assertEqual(rules.camps_of(p), {'sq_alder'})
        found = errors(_values_program(spawn('alder_mere'), extra={'prefix': 'sq'}))
        self.assertTrue(any('prefix' in f['text'] for f in found))



class Phase5Howls(unittest.TestCase):
    """Doc 86, Phase 5: the tarn (story_v2_samples.HOWLS): a resident's howl and a staged bear as the story's effects, the
    howls of its wolves counted, the crew at the ropes, a chorus of three calling the hunt and the bear's kill ending the
    quest; both twins alike."""

    def review(self, events=None, native=True):
        p = copy.deepcopy(samples.HOWLS)
        if events is not None:
            case = p['tests'][0]
            case['events'] = events(case['events'])
            for key in ('expectQuests', 'expectVariables', 'expectEffects'):
                case.pop(key, None)
            case['expect'] = ['gather']
        if native:
            return rules.review(p)
        with patch('story_native.interpreter', return_value=None):
            return rules.review(p)

    def test_the_tarn_rehearses_clean_in_both_twins(self):
        native, python = self.review(), self.review(native=False)
        self.assertTrue(native['passed'] and not native['uncovered'], native)
        self.assertEqual(native, python)

    def test_a_chorus_of_two_calls_nothing(self):
        report = self.review(lambda ev: ev[:5])
        self.assertTrue(report['passed'], report['tests'][0])
        self.assertEqual(report['tests'][0]['path'], ['gather'])

    def test_a_bear_killed_away_from_the_tarn_is_not_old_brown(self):
        # (After review: the kill is at the killer's tile in the hunt's arena, wider than the tarn: in the tarn's cell it
        # counts wherever it fell; in another cell it is not Old Brown.)
        report = self.review(lambda ev: [*ev[:6], {**ev[6], 'x': 9}])
        self.assertTrue(report['passed'], report['tests'][0])
        self.assertEqual(report['tests'][0]['quests'], {'bear': {samples.ASH: 'done'}})
        report = self.review(lambda ev: [*ev[:6], {**ev[6], 'cell': 'elsewhere'}])
        self.assertTrue(report['passed'], report['tests'][0])
        self.assertEqual(report['tests'][0]['quests'], {'bear': {samples.ASH: 'active'}})

    def test_the_new_events_are_mission_evidence_and_watched(self):
        for kind in ('howled', 'chorus closed'):
            self.assertIn(kind, rules.MISSION_EVENTS)
            self.assertIn(kind, rules.WATCHED_EVENTS)
        self.assertIn('quantity', rules.MISSION_MEASURES)
        self.assertIn('chorus closed', rules.MISSION_MEASURES['quantity'])
        w = rules.watch_of(samples.HOWLS)
        self.assertEqual(set(w['events']) & {'howled', 'chorus closed', 'night falls', 'hunted'}, {'howled', 'chorus closed', 'night falls'})
        self.assertEqual(w['residentLists'], ['crew'])


class Phase6Items(unittest.TestCase):
    """Doc 86, Phase 6 (J9): the watch list (story_v2_samples.LETTERS): a story's own letter given, carried and delivered,
    matched as letter^watch_list in a rehearsal in both twins; a plain letter, or another story's, is not it; and what
    approval refuses."""

    def review(self, events=None, native=True):
        p = copy.deepcopy(samples.LETTERS)
        if events is not None:
            case = p['tests'][0]
            case['events'] = events(case['events'])
            for key in ('expectQuests', 'expectVariables', 'expectEffects'):
                case.pop(key, None)
            case['expect'] = ['carry']
        if native:
            return rules.review(p)
        with patch('story_native.interpreter', return_value=None):
            return rules.review(p)

    def test_the_watch_list_rehearses_clean_in_both_twins(self):
        native, python = self.review(), self.review(native=False)
        self.assertTrue(native['passed'] and not native['uncovered'], native)
        self.assertEqual(native, python)

    def test_a_plain_letter_or_another_storys_is_not_the_list(self):
        theirs = rules.tagged_item('another-story', 'letter', 'watch_list')
        for item in ('letter', theirs, 'letter~fine'):
            with self.subTest(item=item):
                report = self.review(lambda ev: [*ev[:4], {**ev[4], 'item': item}])
                self.assertTrue(report['passed'], report['tests'][0])
                self.assertEqual(report['tests'][0]['quests'], {'deliver': {samples.ASH: 'active'}})
        # (The game's own id of this story's list is the list, as the author's item^tag is.)
        mine = rules.tagged_item('v2-watch-list', 'letter', 'watch_list')
        report = self.review(lambda ev: [*ev[:4], {**ev[4], 'item': mine}])
        self.assertEqual(report['tests'][0]['quests'], {'deliver': {samples.ASH: 'done'}})

    def test_carrying_none_is_not_carrying(self):
        report = self.review(lambda ev: [ev[0], {**ev[2], 'facts': samples.carrying(samples.ASH, 0)}])
        self.assertEqual(report['tests'][0]['counters'].get('story', {}).get('carried', False), False, report['tests'][0])

    def test_the_hashed_form_is_the_games(self):
        mine = rules.tagged_item('v2-watch-list', 'letter', 'watch_list')
        self.assertRegex(mine, r'^letter\^[0-9a-f]{8}\.watch_list$')
        self.assertEqual(mine, 'letter^' + rules.tag_hash('v2-watch-list') + '.watch_list')
        p = copy.deepcopy(samples.LETTERS)
        x = rules.expand_tags(p)
        self.assertIsNot(x, p)
        self.assertEqual(p['quests'][0]['objectives'][0]['match']['item'], 'letter^watch_list', 'the author\'s program is untouched')
        self.assertEqual(x['quests'][0]['objectives'][0]['match']['item'], mine)
        phase = rules.effective_phase(p, p['phases'][0])
        seen = next(t for t in phase['triggers'] if t['id'] == 'seen')
        self.assertEqual(seen['if']['item'], mine)
        self.assertEqual(next(t for t in phase['triggers'] if t['id'] == 'any')['match']['item'], 'letter', 'a plain filter stays plain')
        self.assertEqual(rules.quests.index(p)['deliver']['objectives'][0]['match']['item'], mine)
        self.assertIn(mine, rules.watch_of(p)['items'])
        plain = copy.deepcopy(samples.HOWLS)
        self.assertIs(rules.expand_tags(plain), plain, 'a program with no story items is read as it is')
        self.assertEqual(rules.story_items(p), {
            'letter^watch_list': {'name': 'The watch list', 'keep': False, 'item': mine},
            'writ^thanks': {'name': "The warden's writ of thanks", 'keep': True, 'item': rules.tagged_item('v2-watch-list', 'writ', 'thanks')}})

    def test_what_approval_refuses(self):
        def program(edit):
            p = copy.deepcopy(samples.LETTERS)
            edit(p)
            return p
        give = lambda p: p['phases'][0]['triggers'][0]['actions'][0]['effect']
        cases = [
            (lambda p: give(p).pop('tag'), 'is for a story\'s own item'),
            (lambda p: give(p).update(tag='Watch List'), 'invalid item.give.tag'),
            (lambda p: give(p).update(tag='x' * 33), 'invalid item.give.tag'),
            (lambda p: give(p).update(item='letter~fine'), 'invalid item.give.item'),
            (lambda p: give(p).update(name='The #list#'), 'one line of words'),
            (lambda p: give(p).update(name='x' * 61), 'invalid item.give.name'),
            (lambda p: p['quests'][0]['rewards'].append({**samples.THANKS_WRIT, 'keep': False}), 'one name, and kept or not'),
            (lambda p: p['quests'][0]['objectives'][0]['match'].update(item='letter^Watch'), 'named <item>^<mark>'),
            (lambda p: p['quests'][0]['objectives'][0]['match'].update(item=rules.tagged_item('v2-watch-list', 'letter', 'watch_list')),
             'named <item>^<mark>'),
            (lambda p: p['phases'][0]['triggers'][1]['if'].update(item='letter^'), 'Name the item'),
            # (Named whole: "begins with" never meets the game's id, the story's mark being inside it.)
            (lambda p: p['quests'][0]['objectives'][0]['match'].update(item={'starts': 'letter^watch'}), 'named whole'),
        ]
        for edit, words in cases:
            with self.subTest(words=words):
                with self.assertRaisesRegex(ValueError, re.escape(words) if '<' in words else words):
                    rules.validate(program(edit))
        rules.validate(program(lambda p: give(p).pop('name')))   # (A story's own item may go unnamed: it shows as its base.)
        rules.validate(program(lambda p: p['quests'][0]['objectives'][0]['match'].update(item={'in': ['letter^watch_list', 'writ^thanks']})))
        rules.validate(program(lambda p: p['quests'][0]['objectives'][0]['match'].update(item={'starts': 'lett'})))

    def test_one_the_story_never_gives_is_warned_of(self):
        # (The hash is this program's: only what an earlier version of the story gave could match it, so it is a warning.)
        def warnings(edit):
            p = copy.deepcopy(samples.LETTERS)
            edit(p)
            rules.validate(p)
            return [w for w in rules.problems(p) if 'Nothing in this story gives' in w['text']]
        self.assertEqual(warnings(lambda p: None), [])
        for edit, item, path in [
                (lambda p: p['quests'][0]['objectives'][0]['match'].update(item='letter^nothing'), 'letter^nothing', 'quests[0].objectives[0].match.item'),
                (lambda p: p['phases'][0]['triggers'][1]['if'].update(item='rope^watch_list'), 'rope^watch_list', 'phases[0].triggers[1].if.item'),
                (lambda p: p['quests'][0]['objectives'][0]['match'].update(item={'in': ['letter^watch_list', 'writ^nothing']}), 'writ^nothing',
                 'quests[0].objectives[0].match.item')]:
            with self.subTest(item=item):
                found = warnings(edit)
                self.assertEqual([(w['level'], w['path']) for w in found], [('warning', path)], found)
                self.assertIn(item, found[0]['text'])

    def test_the_expanded_programs_are_safe_across_threads(self):
        # (The HTTP threads, the scheduler and rehearsals read the cache at once; one may let a program go from it while
        # another is between reading the program and its phase.)
        import threading
        mine = rules.tagged_item('v2-watch-list', 'letter', 'watch_list')
        p = copy.deepcopy(samples.LETTERS)
        real = rules._expanded
        def let_go(q):   # (Another thread lets every program go the moment this one is read.)
            out = real(q)
            with rules._EXPANDED_LOCK:
                rules._EXPANDED.clear()
            return out
        with patch.object(rules, '_expanded', side_effect=let_go):
            phase = rules.expanded_phase(p, p['phases'][0])
        self.assertEqual(next(t for t in phase['triggers'] if t['id'] == 'seen')['if']['item'], mine)
        with patch.object(rules, 'EXPANDED_MOST', 2):   # (The oldest goes, one at a time.)
            a, b, c = (copy.deepcopy(samples.LETTERS) for _ in range(3))
            for q in (a, b, a, c):
                rules.expand_tags(q)
            self.assertEqual(list(rules._EXPANDED), [id(a), id(c)])
        errors, programs = [], [copy.deepcopy(samples.LETTERS) for _ in range(8)]
        def read():
            try:
                for _ in range(200):
                    for q in programs:
                        ph = rules.effective_phase(q, q['phases'][0])
                        assert next(t for t in ph['triggers'] if t['id'] == 'seen')['if']['item'] == mine
            except Exception as error:   # (Any error at all is the failure.)
                errors.append(error)
        with patch.object(rules, 'EXPANDED_MOST', 3):
            threads = [threading.Thread(target=read) for _ in range(6)]
            for t in threads:
                t.start()
            for t in threads:
                t.join()
        self.assertEqual(errors, [])

    def test_its_name_is_world_text(self):
        # (The name is shown in the pack: the story words check reads it as every text a wolf reads.)
        import story_words
        self.assertEqual(story_words.check(copy.deepcopy(samples.LETTERS)), [])
        p = copy.deepcopy(samples.LETTERS)
        p['phases'][0]['triggers'][0]['actions'][0]['effect']['name'] = 'The dog list'
        found = story_words.check(p)
        self.assertTrue(found and found[0]['blocks'] and found[0]['path'].endswith('.name'), found)


def _many(actions, n, counters, variables=(), native=False, seed='seed-1'):
    """Doc 86, Phase 3: a trigger fired for a story of `n` wolves (w0000...), their counters given; the state after it."""
    import time
    wolves = [f'w{i:04d}' for i in range(n)]
    p = _values_program(actions, variables=[{'id': 'top', 'type': 'wolves', 'scope': 'story'},
                                            {'id': 'board', 'type': 'wolves', 'scope': 'world'},
                                            {'id': 'pledged', 'type': 'wolves', 'scope': 'story'},
                                            {'id': 'p_backs', 'type': 'text', 'scope': 'player'},
                                            {'id': 'p_sworn', 'type': 'switch', 'scope': 'player'}, {'id': 'total', 'type': 'number', 'scope': 'story'},
                                            *variables])
    p['audience'] = {'kind': 'players', 'players': wolves[:2]}
    rules.validate(p)
    state = rules.begin(p, wolves, 0, seed=seed)
    state['counters'] = copy.deepcopy(counters)
    phase = rules.phase_of(p, state)
    trace = []
    ev = {'kind': 'conversation', 'id': 1, 'actor': wolves[0], 'target': 'rm111'}
    started = time.perf_counter()
    if native:
        triggers.fire(phase, state, ev, 1, 0, lambda: set(), rules.matches, lambda n, e: None, trace)
    else:
        with patch('story_native.interpreter', return_value=None):
            triggers.fire(phase, state, ev, 1, 0, lambda: set(), rules.matches, lambda n, e: None, trace)
    return state, trace, time.perf_counter() - started


class Phase3Ranks(unittest.TestCase):
    """Doc 86, C3a and J11: Rank the wolves, and a tally over every wolf."""

    def test_rank_highest_first_ties_by_the_seed(self):
        counters = {'player': {'w0000': {'p_voice': 5}, 'w0001': {'p_voice': 9}, 'w0002': {'p_voice': 5}, 'w0003': {'p_voice': 2},
                               'w0004': {}}}
        rank = {'do': 'rank', 'var': 'p_voice', 'top': 3, 'into': 'top'}
        state, _, _ = _many([rank], 5, counters)
        top = state['counters']['story']['top']
        self.assertEqual(top[0], 'w0001')
        self.assertEqual(set(top[1:]), {'w0000', 'w0002'}, 'the two fives, then nobody else (top 3)')
        tie = sorted(['w0000', 'w0002'], key=lambda w: triggers.fnv('seed-1|' + w))
        self.assertEqual(top[1:], tie, 'ties broken by the seed')
        again, _, _ = _many([rank], 5, counters, seed='seed-2')
        self.assertEqual(again['counters']['story']['top'][1:], sorted(['w0000', 'w0002'], key=lambda w: triggers.fnv('seed-2|' + w)))
        # (At least 5: the two below and the one with no value are left out; the list is cleared first.)
        counters['story'] = {'top': ['w0004']}
        state, _, _ = _many([{**rank, 'top': 10, 'min': 5}], 5, counters)
        self.assertEqual(len(state['counters']['story']['top']), 3)
        self.assertNotIn('w0004', state['counters']['story']['top'])

    def test_rank_over_a_list_into_a_world_list(self):
        counters = {'player': {f'w{i:04d}': {'p_voice': i} for i in range(6)}, 'story': {'pledged': ['w0001', 'w0003', 'outsider']}}
        state, _, _ = _many([{'do': 'rank', 'var': 'p_voice', 'top': {'var': 'total'}, 'over': 'list', 'list': 'pledged', 'into': 'board'}],
                            6, {**counters, 'story': {**counters['story'], 'total': 1}})
        self.assertEqual(state['counters']['world']['board'], ['w0003'], 'only the list\'s wolves; top from a value')
        state, trace, _ = _many([{'do': 'rank', 'var': 'p_voice', 'top': {'var': 'missing_var'}, 'into': 'top'}], 3, counters,
                                variables=[{'id': 'missing_var', 'type': 'number', 'scope': 'player'}])
        self.assertIn('top', state['counters'].get('story', {}))   # (A wolf's own value read for the acting wolf: 0, so 1.)

    def test_rank_checks(self):
        bad = [({'do': 'rank', 'var': 'n', 'top': 3, 'into': 'top'}, 'rank by a player number'),
               ({'do': 'rank', 'var': 'p_voice', 'top': 0, 'into': 'top'}, 'How many: 1 to 500'),
               ({'do': 'rank', 'var': 'p_voice', 'top': 3, 'into': 'p_backs'}, 'list of wolves'),
               ({'do': 'rank', 'var': 'p_voice', 'top': 3, 'over': 'list', 'into': 'top'}, 'name the list'),
               ({'do': 'rank', 'var': 'p_voice', 'top': 3, 'list': 'pledged', 'into': 'top'}, 'name the list'),
               ({'do': 'each', 'over': 'participants', 'actions': [{'do': 'rank', 'var': 'p_voice', 'top': 3, 'into': 'top'}]}, 'not allowed inside For each')]
        for action, words in bad:
            with self.subTest(words=words):
                with self.assertRaisesRegex(ValueError, words):
                    _many([action], 2, {})

    def test_tally_sums_and_counts(self):
        counters = {'player': {'w0000': {'p_voice': 4, 'p_backs': 'lamp'}, 'w0001': {'p_voice': 3, 'p_backs': 'lamp'},
                               'w0002': {'p_voice': 10, 'p_backs': 'stone'}, 'w0003': {'p_backs': 'lamp'}, 'w0004': {'p_sworn': True}}}
        cases = [({'sum': 'p_voice'}, 17), ({'sum': 'p_voice', 'where': 'p_backs', 'is': 'lamp'}, 7), ({'where': 'p_backs', 'is': 'lamp'}, 3),
                 ({}, 5), ({'where': 'p_sworn', 'is': False}, 4), ({'sum': 'p_voice', 'over': {'list': 'pledged'}}, 14)]
        for tally, want in cases:
            with self.subTest(tally=tally):
                state, _, _ = _many([{'do': 'set', 'var': 'total', 'value': {'plus': [{'tally': tally}, 0]}}], 5,
                                    {**counters, 'story': {'pledged': ['w0000', 'w0002', 'nobody']}})
                self.assertEqual(state['counters']['story']['total'], want)

    def test_tally_checks(self):
        bad = [({'tally': {'sum': 'n'}}, 'a player number'), ({'tally': {'where': 'p_backs'}}, 'name both, or neither'),
               ({'tally': {'where': 'p_backs', 'is': True}}, 'is a text'), ({'tally': {'where': 'p_voice', 'is': 3}}, 'text or switch'),
               ({'tally': {'over': {'list': 'p_backs'}}}, 'list of wolves'), ({'plus': [{'plus': [{'tally': {}}, 1]}, 1]}, 'one level deep'),
               ({'tally': {'sum': 'p_voice', 'among': 1}}, '')]
        for value, words in bad:
            with self.subTest(value=value):
                with self.assertRaisesRegex(ValueError, words):
                    _many([{'do': 'set', 'var': 'total', 'value': value}], 2, {})
        with self.assertRaisesRegex(ValueError, 'not allowed inside For each'):
            _many([{'do': 'each', 'over': 'participants', 'actions': [{'do': 'set', 'var': 'total', 'value': {'tally': {}}}]}], 2, {})

    def test_a_tally_over_1200_wolves_in_a_rehearsal_cohort(self):
        counters = {'player': {f'w{i:04d}': {'p_voice': i % 7, 'p_backs': 'lamp' if i % 3 == 0 else 'stone'} for i in range(1200)}}
        actions = [{'do': 'set', 'var': 'total', 'value': {'tally': {'sum': 'p_voice', 'where': 'p_backs', 'is': 'lamp'}}},
                   {'do': 'set', 'var': 'n', 'value': {'tally': {}}}, {'do': 'rank', 'var': 'p_voice', 'top': 500, 'into': 'top'}]
        state, _, took = _many(actions, 1200, counters)
        self.assertEqual(state['counters']['story']['total'], sum(i % 7 for i in range(0, 1200, 3)))
        self.assertEqual(state['counters']['story']['n'], 1200)
        self.assertEqual(len(state['counters']['story']['top']), 500)
        self.assertLess(took, 0.5, 'under the rehearsal budget')
        native, _, _ = _many(actions, 1200, counters, native=True)
        self.assertEqual(native['counters'], state['counters'], 'both twins agree')


if __name__ == '__main__':
    unittest.main()
