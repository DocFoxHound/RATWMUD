"""The trigger language, version 2 (Docs/Design/85-story-triggers-and-campaign-control.md, 3.2, 3.4, 3.5 and Phase 5):
declared variables, the new conditions, flow inside a trigger's actions, the checks that refuse what is malformed (with
the Problems list pointing at a nested action), rehearsals with pinned rolls, waits and expected variables, and a
version 1 program read as before and made version 2 doing the same. The runner's side (a version 2 story published,
waiting, finishing, and keeping a world variable) is in tools/test_story_v2_runner.py; both engines agreeing is
tools/test_story_native.py."""

import copy
import json
import unittest
from unittest.mock import patch

import story_library
import story_program as rules
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


if __name__ == '__main__':
    unittest.main()
