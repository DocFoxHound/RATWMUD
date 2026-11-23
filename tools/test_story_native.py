"""The authoring fallback and compiled interpreter must agree on every shipped rehearsal: the 108 programs as they are
(version 1), the same 108 made version 2 (story_program.upgrade), and the version 2 samples that use every new condition
and action (tools/story_v2_samples.py), with their rolls hashed under several seeds; and the quest samples of
tools/story_v2_samples.py (Phase 6); and the night watch (Phase 8: every condition over the world's facts, the new events),
whose facts are also made up at random by tools/test_story_facts.py."""

import copy
import json
import unittest
from unittest.mock import patch

import story_library
import story_native
import story_program
import story_runner
import story_triggers
import story_v2_samples


def shipped():
    examples = list(story_runner.catalogue()['examples'].values())
    examples.extend(story_library.draft(entry['id']) for entry in story_library.entries())
    return examples


@unittest.skipUnless(story_native.interpreter(), 'Build ratw_server or set RATW_STORY_LIBRARY first.')
class NativeAuthoringParity(unittest.TestCase):
    def same(self, program, passed=True):
        actual = story_program.review(program)
        with patch('story_native.interpreter', return_value=None):
            expected = story_program.review(program)
        if passed is not None:
            self.assertEqual(actual['passed'], passed)
        self.assertEqual(actual, expected)

    def test_all_shipped_paths_match_including_traces_and_recipients(self):
        programs = shipped()
        self.assertEqual(len(programs), 108)
        for program in programs:
            with self.subTest(program=program['id']):
                self.same(program)

    def test_the_shipped_programs_as_version_2_match_too(self):
        for program in shipped():
            with self.subTest(program=program['id']):
                upgraded = story_program.upgrade(program)
                self.assertEqual(upgraded['version'], 2)
                self.same(upgraded)

    def test_every_version_2_construct_matches(self):
        for program in story_v2_samples.all_samples():
            with self.subTest(program=program['id']):
                self.same(program)
                # The same cases under other seeds and with no pinned rolls: whatever path they take, the twins agree.
                for seed in ('a', 'b', 'c', 'winter', 'x' * 80):
                    other = copy.deepcopy(program)
                    for case in other['tests']:
                        case.pop('rolls', None)
                        case['seed'] = seed
                        for key in ('expect', 'expectVariables', 'expectOutcome'):
                            case.pop(key, None)
                        case['expect'] = []
                    self.same(other, passed=None)

    def test_quests_match(self):
        """Doc 85, Phase 6: a story's quests run the same whichever twin fires its triggers (the quest effects are carried
        by both as any effect, and kept by the story after the firing)."""
        for program in (story_v2_samples.QUEST_SMALL, story_v2_samples.QUEST_CAMPAIGN):
            with self.subTest(program=program['id']):
                self.same(program)

    def test_the_talk_effects_match(self):
        """Doc 86, Phase 1b: what residents remember of the acting wolf and of the looped one, a rumour, a figure and the
        holds: carried by both twins as any effect, $actor and $each filled alike."""
        self.same(story_v2_samples.TALK)
        p = copy.deepcopy(story_v2_samples.TALK)
        p['phases'][1]['triggers'] = [{'id': 'all', 'on': 'conversation', 'audience': 'world', 'actions': [
            {'do': 'each', 'over': 'participants', 'actions': [
                {'do': 'effect', 'effect': {'kind': 'talk.remember', 'scope': 'pair', 'npc': 'rm111', 'player': '$each', 'key': 'nh_heard',
                                            'value': 'yes', 'days': 5}},
                {'do': 'effect', 'effect': {'kind': 'npc.spare', 'npc': 'rm113'}}]}]}]
        p['tests'][0]['events'].append({'id': 'e9', 'kind': 'conversation', 'day': 2, 'actor': story_v2_samples.ELM, 'target': 'rm111'})
        p['tests'][0].pop('expectVariables')
        self.same(p)

    def test_world_facts_match(self):
        """Doc 85, Phase 8: conditions over the world's facts as of the event, and over quests, the same in both twins (the
        native one sent only the wolves its conditions read)."""
        self.same(story_v2_samples.FACTS_WATCH)
        other = copy.deepcopy(story_v2_samples.FACTS_WATCH)
        for case in other['tests']:
            case.pop('expectVariables', None)
            case['cohort'] = [story_v2_samples.ASH]   # (Birch out of the story: counts and loops change, the twins still agree.)
        other['audience']['players'] = [story_v2_samples.ASH]
        self.same(other, passed=None)


@unittest.skipUnless(story_native.interpreter(), 'Build ratw_server or set RATW_STORY_LIBRARY first.')
class ParticipationParity(unittest.TestCase):
    """Doc 86, Phase 1a: every C2 and R1 form and the condition participation, fired by both twins on the same states."""

    FORMS = [{'audience': 'all'}, {'audience': 'active'}, {'audience': 'online'},
             {'times': [{'audience': 'active'}, 3]}, {'div': [{'audience': 'all'}, 4]}, {'div': [7, {'audience': 'online'}]},
             *({share: src, 'of': of} for share in ('percent', 'among') for of in ('active', 'all', 'online')
               for src in ({'list': 'did'}, {'where': {'test': 'compare', 'left': {'var': 'n', 'of': '$each'}, 'op': '>=', 'right': 3}},
                           {'where': {'test': 'any', 'conditions': [{'test': 'in list', 'list': 'did', 'value': 'w03'},
                                                                    {'test': 'switch', 'var': 'on', 'of': '$each', 'is': True}]}}))]

    def program(self):
        variables = [{'id': 'did', 'type': 'wolves', 'scope': 'story'}, {'id': 'n', 'type': 'number', 'scope': 'player'},
                     {'id': 'on', 'type': 'switch', 'scope': 'player'}, {'id': 'out', 'type': 'numbers', 'scope': 'story'},
                     {'id': 'hits', 'type': 'number', 'scope': 'story'}]
        sets = [{'do': 'list add', 'var': 'out', 'value': {'plus': [{'times': [i, 1000]}, f]}}
                for i, f in enumerate(self.FORMS)]
        conditions = [{'test': 'participation', 'list': 'did', 'percent': 35, 'floor': 3},
                      {'test': 'participation', 'list': 'did', 'percent': 90, 'floor': 2, 'cap': 4, 'of': 'all'},
                      {'test': 'participation', 'where': {'test': 'compare', 'left': {'var': 'n', 'of': '$each'}, 'op': '>', 'right': 1},
                       'percent': 50, 'of': 'online'},
                      {'test': 'participation', 'source': 'where', 'where': {'test': 'not', 'condition': {
                          'test': 'switch', 'var': 'on', 'of': '$each', 'is': True}}, 'percent': 20, 'floor': 2}]
        ifs = [{'do': 'if', 'if': c, 'then': [{'do': 'add', 'var': 'hits', 'value': 10 ** i}]} for i, c in enumerate(conditions)]
        return {'format': 'ratw-story-program', 'version': 2, 'id': 'parity', 'title': 'Counting in both twins', 'start': 'a',
                'audience': {'kind': 'open'}, 'variables': variables,
                'phases': [{'id': 'a', 'journal': 'Count.', 'effects': [], 'exits': [], 'triggers': [
                    {'id': 'count', 'on': 'conversation', 'actions': [*sets[:12], {'do': 'run', 'trigger': 'more'}]},
                    {'id': 'more', 'on': 'story signal', 'signal': 'never', 'actions': [*sets[12:24], {'do': 'run', 'trigger': 'rest'}]},
                    {'id': 'rest', 'on': 'story signal', 'signal': 'never', 'actions': [*sets[24:], *ifs]}]}],
                'tests': [{'name': 'x', 'cohort': [], 'events': [], 'expect': ['a']}]}

    def both(self, state, facts=None):
        p = self.program()
        story_program.validate(p)
        phase = story_program.phase_of(p, state)
        event = {'kind': 'conversation', 'id': 1, 'actor': '', **({'facts': facts} if facts is not None else {})}
        out = []
        for native in (True, False):
            s = copy.deepcopy(state)
            trace = []
            if native:
                story_triggers.fire(phase, s, event, 20, 0, lambda: set(), story_program.matches, lambda n, e: None, trace)
            else:
                with patch('story_native.interpreter', return_value=None):
                    story_triggers.fire(phase, s, event, 20, 0, lambda: set(), story_program.matches, lambda n, e: None, trace)
            out.append((s, trace))
        self.assertEqual(out[0][0], out[1][0])
        self.assertEqual(out[0][1], out[1][1])
        return out[0][0]

    def states(self):
        wolves = [f'w{i:02d}' for i in range(12)]
        base = {'phase': 'a', 'entered': 0, 'visits': 1, 'votes': {}, 'actor': '', 'seed': 's', 'began': 0, 'cohort': wolves}
        counters = {'story': {'did': wolves[:5]}, 'player': {w: {'n': i % 5, 'on': i % 3 == 0} for i, w in enumerate(wolves)}}
        yield 'twelve active, five did', {**base, 'seen': {w: 20 for w in wolves}, 'counters': counters}, None
        yield 'some went quiet', {**base, 'seen': {w: 20 if i % 2 else 1 for i, w in enumerate(wolves)}, 'counters': counters}, None
        yield 'nobody active', {**base, 'seen': {}, 'counters': counters}, None
        yield 'online from the facts', {**base, 'seen': {w: 19 for w in wolves}, 'counters': counters}, {'wolves': {w: {} for w in wolves[:4]}}
        yield 'online as the run knows it', {**base, 'seen': {w: 19 for w in wolves}, 'counters': counters, 'online': wolves[3:5]}, None
        yield 'nobody online', {**base, 'seen': {w: 19 for w in wolves}, 'counters': counters, 'online': []}, None
        yield 'counts given', {**base, 'seen': {}, 'counters': counters, 'rehearsalAudience': {'all': 400, 'active': 120, 'online': 31},
                               'rehearsalActive': wolves[:7]}, None
        yield 'no story', {**base, 'cohort': [], 'seen': {}, 'counters': {'story': {'did': []}}}, None

    def test_every_form_and_the_condition_match(self):
        seen_hits, sizes = set(), set()
        for name, state, facts in self.states():
            with self.subTest(state=name):
                after = self.both(state, facts)
                seen_hits.add(after['counters']['story'].get('hits', 0))
                sizes.add(len(after['counters']['story'].get('out', [])))
        self.assertGreater(len(seen_hits), 2, 'the conditions came out differently across the states')
        self.assertGreater(len(sizes), 1, 'online not known gives no value (the form is skipped), known gives one')

    def test_times_past_two_to_the_53_agree(self):
        """A product past 2^53 is the double's in both twins (exact ints in Python would differ in the last digits), and a
        `div` or a `plus` after it comes out the same."""
        big = {'times': [{'times': [999_999_937, 999_999_929]}, 999_999_893]}
        forms = [big, {'div': [{'times': [999_999_937, 999_999_929]}, 7]}, {'plus': [{'times': [999_999_937, 999_999_929]}, 1]},
                 {'minus': [{'times': [999_999_937, 999_999_929]}, {'times': [999_999_929, 999_999_937]}]}]
        self.FORMS = forms
        original = self.program
        def program():
            p = original()
            p['phases'][0]['triggers'] = [{'id': 'count', 'on': 'conversation', 'actions': [
                {'do': 'list add', 'var': 'out', 'value': f} for f in forms]}]
            return p
        self.program = program
        base = {'phase': 'a', 'entered': 0, 'visits': 1, 'votes': {}, 'actor': '', 'seed': 's', 'began': 0, 'cohort': [],
                'seen': {}, 'counters': {'story': {'out': []}}}
        after = self.both(base)
        self.assertEqual(len(after['counters']['story']['out']), len(forms))


@unittest.skipUnless(story_native.interpreter(), 'Build ratw_server or set RATW_STORY_LIBRARY first.')
class ValuesParity(unittest.TestCase):
    """Doc 86, Phase 2: values in effect parameters (C1a), fills (C1b), the purse (M2), the mirror (N3), filters that begin
    with or are one of (C12b) and the moon (J4), fired by both twins on the same states."""

    ASH, BIRCH, ELM = 'player-ash', 'player-birch', 'player-elm'

    def program(self):
        A = self.ASH
        variables = [{'id': 'n', 'type': 'number', 'scope': 'story'}, {'id': 'word', 'type': 'text', 'scope': 'story'},
                     {'id': 'guide', 'type': 'resident', 'scope': 'story'}, {'id': 'finder', 'type': 'wolf', 'scope': 'story'},
                     {'id': 'spot', 'type': 'place', 'scope': 'story'}, {'id': 'thing', 'type': 'item', 'scope': 'story'},
                     {'id': 'side', 'type': 'faction', 'scope': 'story'}, {'id': 'lit', 'type': 'switch', 'scope': 'story'},
                     {'id': 'p_n', 'type': 'number', 'scope': 'player'}, {'id': 'p_town', 'type': 'text', 'scope': 'player'},
                     {'id': 'v_grey', 'type': 'number', 'scope': 'world'}, {'id': 'v_cold', 'type': 'number', 'scope': 'world'},
                     {'id': 'p_howl', 'type': 'number', 'scope': 'player', 'show': 'Howl',
                      'mirror': {'key': 'p_town', 'names': {'greyfen': 'v_grey', 'coldwater': 'v_cold'}}},
                     {'id': 'out', 'type': 'numbers', 'scope': 'story'}]
        grant = lambda coins: {'do': 'effect', 'effect': {'kind': 'character.grant', 'player': '$actor', 'purse': 'town:greyfen',
                                                           'reason': 'r', 'coins': coins}}
        actions = [
            grant({'min': [20, {'var': 'p_n', 'of': '$actor'}]}),
            grant({'div': [{'purse': 'town:greyfen'}, 20]}),
            grant({'times': [{'var': 'p_howl', 'of': '$actor'}, 1000]}),
            grant({'minus': [{'var': 'n'}, 50]}),
            {'do': 'effect', 'effect': {'kind': 'npc.walk', 'npc': {'var': 'guide'}, 'cell': {'var': 'spot'}, 'x': {'var': 'n'}, 'y': 4,
                                        'pace': 'walk', 'cue': 'c'}},
            {'do': 'effect', 'effect': {'kind': 'item.give', 'player': {'field': 'target'}, 'item': {'var': 'thing'}, 'quantity': {'var': 'p_n', 'of': '$actor'}}},
            {'do': 'effect', 'effect': {'kind': 'board.post', 'town': 'greyfen', 'days': 2,
                                        'text': '{finder} met {guide} at {spot} with {thing} for {side}: {n}, {lit}, {{x}}, {p_n.of} {p_howl.of}.'}},
            {'do': 'effect', 'effect': {'kind': 'story.tell', 'to': 'world', 'text': 'The howl is {p_howl.of}; the purse {n}; {word}.'}},
            {'do': 'each', 'over': 'participants', 'actions': [
                {'do': 'effect', 'effect': {'kind': 'journal.note', 'player': '$each', 'text': 'You at {p_howl.of}, {p_n.of}.'}},
                {'do': 'effect', 'effect': {'kind': 'board.post', 'town': 'greyfen', 'days': 1, 'text': 'Seen: {p_town.of}.'}},
                {'do': 'list add', 'var': 'out', 'value': {'var': 'p_howl', 'of': '$each'}}]},
            {'do': 'each', 'over': 'list', 'list': 'out', 'actions': [
                {'do': 'effect', 'effect': {'kind': 'deed.award', 'player': '$actor', 'detail': 'Counted {n}, {p_n.of}.'}}]},
        ]
        return {'format': 'ratw-story-program', 'version': 2, 'id': 'parity2', 'title': 'Values in both twins', 'start': 'a',
                'audience': {'kind': 'open'}, 'variables': variables,
                'phases': [{'id': 'a', 'journal': 'Values.', 'effects': [], 'exits': [], 'triggers': [
                    {'id': 'all', 'on': 'conversation', 'audience': 'participant', 'actions': actions},
                    {'id': 'starts', 'on': 'deed', 'match': {'detail': {'starts': 'Stood watch'}}, 'actions': [{'do': 'add', 'var': 'n', 'value': 1}]},
                    {'id': 'in', 'on': 'deed', 'match': {'cell': {'in': ['grey', 'far']}, 'item': {'in': ['rope', 'axle~fine']}},
                     'actions': [{'do': 'add', 'var': 'n', 'value': 10}]},
                    {'id': 'field', 'on': 'deed', 'if': {'test': 'any', 'conditions': [{'test': 'field', 'field': 'detail', 'op': 'starts', 'value': 'Held'},
                                                                 {'test': 'field', 'field': 'target', 'op': 'in', 'value': ['rm111', 'rm112']}]},
                     'actions': [{'do': 'add', 'var': 'n', 'value': 100}]},
                    {'id': 'moon', 'on': 'deed', 'if': {'test': 'calendar is', 'what': 'moon', 'value': 'full'},
                     'actions': [{'do': 'add', 'var': 'n', 'value': 1000}]}]}],
                'tests': [{'name': 'x', 'cohort': [], 'events': [], 'expect': ['a']}]}

    def both(self, state, event):
        p = self.program()
        story_program.validate(p)
        phase = story_program.phase_of(p, state)
        out = []
        for native in (True, False):
            s = copy.deepcopy(state)
            trace = []
            if native:
                story_triggers.fire(phase, s, event, 20, 0, lambda: set(), story_program.matches, lambda n, e: None, trace)
            else:
                with patch('story_native.interpreter', return_value=None):
                    story_triggers.fire(phase, s, event, 20, 0, lambda: set(), story_program.matches, lambda n, e: None, trace)
            out.append((s, trace))
        self.assertEqual(out[0][0], out[1][0])
        self.assertEqual(out[0][1], out[1][1])
        return out[0][0], out[0][1]

    def states(self):
        A, B, E = self.ASH, self.BIRCH, self.ELM
        base = {'phase': 'a', 'entered': 0, 'visits': 1, 'votes': {}, 'actor': '', 'seed': 's', 'began': 0, 'cohort': [A, B, E]}
        full = {'story': {'n': 7, 'word': 'oak', 'guide': 'rm111', 'finder': A, 'spot': 'grey', 'thing': 'rope', 'side': 'watch', 'lit': True},
                'player': {A: {'p_n': 4, 'p_town': 'greyfen'}, B: {'p_n': 30, 'p_town': 'coldwater'}, E: {'p_town': 'nowhere'}},
                'world': {'v_grey': 3, 'v_cold': 8}}
        yield 'every value known', {**base, 'counters': full}, {'purses': {'town:greyfen': 212}, 'world': {'moon': 'full'}}
        yield 'nothing set', {**base, 'counters': {}}, None
        yield 'no purse, no moon', {**base, 'counters': full}, {'purses': {}, 'world': {}}
        yield 'the actor not in the story', {**base, 'cohort': [B, E], 'counters': full}, {'purses': {'town:greyfen': 5}}

    def test_every_form_matches(self):
        texts, skipped = set(), 0
        for name, state, facts in self.states():
            for kind, fields in [('conversation', {'target': self.BIRCH}),
                                 ('deed', {'detail': 'Stood watch at the ford (d3)', 'cell': 'grey', 'item': 'axle~fine', 'target': 'rm112'}),
                                 ('deed', {'detail': 'Held the gate (d4)', 'cell': 'near', 'item': 'axle', 'target': 'rm9'}),
                                 ('deed', {'detail': 'stood watch', 'cell': 'far', 'item': 'rope~good'})]:
                with self.subTest(state=name, kind=kind, fields=fields):
                    event = {'kind': kind, 'id': 1, 'actor': self.ASH, **fields, **({'facts': facts} if facts is not None else {})}
                    after, trace = self.both(state, event)
                    for e in after.get('triggerEffects', []):
                        texts.update(v for v in e.values() if isinstance(v, str) and ('{@' in v or v.endswith('.')))
                    skipped += sum('skipped' in n for f in trace for n in f.get('notes', []))
        self.assertTrue(any('{@w:' + self.ASH + '}' in t and '{@r:rm111}' in t and '{@p:grey}' in t and '{@i:rope}' in t and '{@f:watch}' in t
                            and 'yes' in t and '{x}' in t for t in texts), texts)
        self.assertGreater(skipped, 3, 'missing values skip with a note in both')



class RankParity(ValuesParity):
    """Doc 86, Phase 3: Rank the wolves (C3a) and a tally over every wolf (J11), fired by both twins on the same states."""

    def program(self):
        variables = [{'id': 'p_voice', 'type': 'number', 'scope': 'player'}, {'id': 'p_backs', 'type': 'text', 'scope': 'player'},
                     {'id': 'p_sworn', 'type': 'switch', 'scope': 'player'}, {'id': 'n', 'type': 'number', 'scope': 'story'},
                     {'id': 'least', 'type': 'number', 'scope': 'story'}, {'id': 'top', 'type': 'wolves', 'scope': 'story'},
                     {'id': 'board', 'type': 'wolves', 'scope': 'world'}, {'id': 'pledged', 'type': 'wolves', 'scope': 'story'},
                     {'id': 'sums', 'type': 'numbers', 'scope': 'story'}]
        tallies = [{'sum': 'p_voice'}, {'sum': 'p_voice', 'where': 'p_backs', 'is': 'lamp'}, {'where': 'p_sworn', 'is': False}, {},
                   {'sum': 'p_voice', 'over': {'list': 'pledged'}}, {'where': 'p_backs', 'is': 'stone', 'over': {'list': 'pledged'}}]
        actions = [{'do': 'list add', 'var': 'sums', 'value': {'plus': [{'tally': t}, k * 1000]}} for k, t in enumerate(tallies)]
        actions += [{'do': 'rank', 'var': 'p_voice', 'top': 2, 'into': 'top'},
                    {'do': 'rank', 'var': 'p_voice', 'top': {'var': 'n'}, 'min': {'var': 'least'}, 'over': 'list', 'list': 'pledged', 'into': 'board'},
                    {'do': 'set', 'var': 'n', 'value': {'times': [{'tally': {'sum': 'p_voice'}}, 2]}},
                    {'do': 'each', 'over': 'list', 'list': 'top', 'actions': [
                        {'do': 'effect', 'effect': {'kind': 'title.give', 'player': '$each', 'title': 'the Lamp of Greyfen'}}]}]
        return {'format': 'ratw-story-program', 'version': 2, 'id': 'parity3', 'title': 'Ranks in both twins', 'start': 'a',
                'audience': {'kind': 'open'}, 'variables': variables,
                'phases': [{'id': 'a', 'journal': 'Ranks.', 'effects': [], 'exits': [], 'triggers': [
                    {'id': 'all', 'on': 'conversation', 'audience': 'participant', 'actions': actions}]}],
                'tests': [{'name': 'x', 'cohort': [], 'events': [], 'expect': ['a']}]}

    def states(self):
        A, B, E = self.ASH, self.BIRCH, self.ELM
        base = {'phase': 'a', 'entered': 0, 'visits': 1, 'votes': {}, 'actor': '', 'seed': 's', 'began': 0, 'cohort': [A, B, E]}
        yield 'ties', {**base, 'counters': {'player': {A: {'p_voice': 5, 'p_backs': 'lamp'}, B: {'p_voice': 5, 'p_backs': 'stone', 'p_sworn': True},
                                                       E: {'p_voice': 1.5}},
                                            'story': {'n': 3, 'least': 2, 'pledged': [A, E, 'player-gone']}}}, None
        yield 'nothing set', {**base, 'counters': {}}, None
        yield 'another seed', {**base, 'seed': 'other', 'counters': {'player': {A: {'p_voice': 5}, B: {'p_voice': 5}, E: {'p_voice': 5}},
                                                                     'story': {'pledged': [B]}}}, None
        yield 'the least unknown', {**base, 'counters': {'player': {A: {'p_voice': 2}}, 'story': {'n': 9}}}, None

    def test_every_form_matches(self):
        for name, state, _ in self.states():
            with self.subTest(state=name):
                after, trace = self.both(state, {'kind': 'conversation', 'id': 1, 'actor': self.ASH, 'target': 'rm111'})
                self.assertIn('sums', after['counters']['story'])
                self.assertIn('top', after['counters']['story'])


if __name__ == '__main__':
    unittest.main()
