"""Moving a running campaign onto a newly approved version (Docs/Design/85-story-triggers-and-campaign-control.md, Phase
5b). The migration on its own (story_program.migration and version_diff): the 8 examples, part-way through each of their
rehearsals, edited three ways (words only, made version 2, a trigger taken away); a version 1 run moved onto version 2
(counters by `upgrade`'s names, `<name>.player` for one named in both scopes) and carrying on as the old one would have; a
phase taken away (the admin names where to land); a variable retyped; a wait part-way through a list (kept while what it
still has to run is unchanged); a timer whose listener went; missions reset or kept; triggers newly off. Then the runner,
in a throwaway migrated database: approving as a new version of a run, the move as a command between two world events,
the game told the new words for the visit (story.phase with `revise`), nothing read while it waits, the move refused by the
game and rolled back, moving twice, a command made against the old version refused, entry effects not sent again, the
timeline's and the audit's words; and the route. The game is stood in for by answering dm.actions rows by hand. The
database tests are skipped without the local PostgreSQL."""

import copy
import json
import secrets
import unittest

import story_program as rules
import story_runner as runner
import test_story_console as console
import test_story_steering
from test_story_console import WOLVES
from test_world_db import database_available

ASH, BIRCH, ELM = sorted(WOLVES)
EXAMPLES = runner.catalogue()['examples']

WAITER = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-waiter', 'title': 'The bell watch',
    'audience': {'kind': 'players', 'players': ['ash', 'birch']}, 'start': 'watch',
    'variables': [{'id': 'bells', 'type': 'number', 'scope': 'story'}, {'id': 'mood', 'type': 'number', 'scope': 'story'},
                  {'id': 'rung', 'type': 'number', 'scope': 'player'}],
    'phases': [
        {'id': 'watch', 'journal': 'Ring the bell.', 'effects': [], 'exits': [], 'triggers': [
            {'id': 'ring', 'on': 'arrival', 'audience': 'participant', 'actions': [
                {'do': 'add', 'var': 'bells', 'value': 1},
                {'do': 'add', 'var': 'rung', 'value': 1},
                {'do': 'wait', 'days': 1},
                {'do': 'add', 'var': 'bells', 'value': 10},
                {'do': 'if', 'if': {'test': 'compare', 'left': {'var': 'bells'}, 'op': '>=', 'right': 11},
                 'then': [{'do': 'enter', 'phase': 'done'}]}]},
            {'id': 'chat', 'on': 'conversation', 'actions': [{'do': 'add', 'var': 'mood', 'value': 1}]}]},
        {'id': 'done', 'journal': 'Done.', 'effects': [], 'exits': []}],
    'tests': [{'name': 'rung', 'cohort': ['ash', 'birch'], 'expect': ['watch', 'done'],
               'events': [{'id': 'e1', 'kind': 'arrival', 'day': 1, 'actor': 'ash'}, {'id': 'e2', 'kind': 'clock', 'day': 3}]}],
}


def edited(p, change):
    q = copy.deepcopy(p)
    change(q)
    return q


def rehearsed(p):
    """A program whose rehearsals expect the effects it now gives (an edit that adds one)."""
    for case, report in zip(p['tests'], rules.review(p)['tests']):
        if 'expectEffects' in case:
            case['expectEffects'] = report['effects']
    return p


def renamed(p, phase, to):
    """A program with one phase renamed everywhere (its id, what enters it, its rehearsals)."""
    return json.loads(json.dumps(p).replace(json.dumps(phase), json.dumps(to)))


def states_of(p):
    """Every state a program's rehearsals pass through (a copy at each fact), with the facts still to come."""
    out = []
    for case in p['tests']:
        seen = []

        def step(ev, state, path, trace, intended, seen=seen):
            seen.append(copy.deepcopy(state))
        rules._play_case(p, case, step)
        for i, state in enumerate(seen):
            out.append((case, i, state))
    return out


def play_on(p, state, events):
    """A rehearsal's remaining facts, from a state, as _play_case plays them: the path of phases and the state at the end."""
    state = copy.deepcopy(state)
    path, last, elapsed = [], 0, 0
    for ev in events:
        last, elapsed = ev['day'], ev.get('elapsedDays', elapsed)
        for fact in ([ev] if ev['kind'] in ('clock', 'dm signal') else [{'kind': 'clock'}, ev]):
            while True:
                moved = rules.advance(p, state, fact, last, [], elapsed)
                state.pop('triggerEffects', None)
                if moved:
                    path.append(state['phase'])
                if not moved or fact['kind'] != 'clock':
                    break
    return path, state


def lines(report, group, what=None):
    return [x for x in report[group] if what is None or x['what'] == what]


class Migration(unittest.TestCase):
    def test_the_eight_examples_part_way_through_edited_three_ways(self):
        for name, p in EXAMPLES.items():
            for case, i, state in states_of(p):
                if state.get('outcome') or not (rules.phase_of(p, state)['exits'] or rules.phase_of(p, state).get('triggers')):
                    continue
                with self.subTest(example=name, case=case['name'], step=i):
                    # 1. Words only: everything is kept, the state is the same.
                    words = edited(p, lambda q: q.update(title=q['title'] + ' (revised)'))
                    moved, report = rules.migration(p, words, state, run='world-test')
                    self.assertIsNone(report['blocked'])
                    self.assertEqual(moved, state)
                    for group in ('reset', 'defaulted', 'dropped'):
                        self.assertEqual(report[group], [], group)
                    self.assertEqual(report['diff']['header'], ['title'])
                    # 2. Made version 2 (version 1 only): every counter kept, by upgrade's names, and the rest of the rehearsal
                    # goes as it would have.
                    if p['version'] == 1:
                        two = rules.upgrade(p)
                        moved, report = rules.migration(p, two, state, run='world-test')
                        self.assertIsNone(report['blocked'])
                        self.assertEqual(lines(report, 'dropped', 'variable') + lines(report, 'defaulted'), [])
                        self.assertEqual(moved['seed'], 'world-test')
                        rest = case['events'][i:]
                        path1, end1 = play_on(p, state, rest)
                        path2, end2 = play_on(two, moved, rest)
                        self.assertEqual(path1, path2, 'the same phases follow')
                        self.assertEqual(end1['phase'], end2['phase'])
                        ours = end2.get('counters', {})
                        self.assertEqual(end1.get('counters', {}).get('story', {}), ours.get('story', {}))
                    # 3. A trigger of the phase it is in taken away: its marks and waits go, nothing else changes.
                    phase = next(ph for ph in p['phases'] if ph['id'] == state['phase'])
                    if phase.get('triggers'):
                        gone = phase['triggers'][0]['id']
                        less = edited(p, lambda q: next(ph for ph in q['phases'] if ph['id'] == state['phase']).update(
                            triggers=[t for t in phase['triggers'] if t['id'] != gone]))
                        try:
                            rules.validate(less)
                        except ValueError:
                            continue   # (Another trigger runs or turns it on: taking it away is not a program.)
                        moved, report = rules.migration(p, less, state, run='world-test')
                        self.assertIn(state['phase'] + '/' + gone, report['diff']['triggers']['removed'])
                        self.assertEqual(report['reset'] + report['defaulted'], [])
                        self.assertNotIn(gone, moved.get('fired', {}))
                        self.assertFalse(any(w['trigger'] == gone for w in moved.get('waiting', [])))

    def test_a_version_1_run_onto_version_2_renames_a_counter_named_in_both_scopes(self):
        p = edited(EXAMPLES['winter-ferries'], lambda q: q['triggers'][1]['actions'].append(
            {'do': 'add', 'key': 'stores', 'scope': 'player', 'value': 'quantity'}))
        state = rules.begin(p, [ASH, BIRCH], 10)
        state['counters'] = {'story': {'stores': 5, 'watches': 2}, 'player': {ASH: {'stores': 3}}}
        two = rules.upgrade(p)
        self.assertIn({'id': 'stores.player', 'type': 'number', 'scope': 'player'}, two['variables'])
        moved, report = rules.migration(p, two, state, run='world-r', began=7)
        self.assertEqual(moved['counters'], {'story': {'stores': 5, 'watches': 2}, 'player': {ASH: {'stores.player': 3}}})
        renamed = [x for x in report['kept'] if x.get('renamed')]
        self.assertEqual([(x['key'], x['renamed'], x['wolf']) for x in renamed], [('stores.player', 'stores', ASH)])
        self.assertEqual((moved['seed'], moved['began']), ('world-r', 7))
        self.assertEqual(report['diff']['variables']['renamed'], [{'id': 'stores', 'scope': 'player', 'to': 'stores.player'}])
        self.assertEqual(report['diff']['variables']['added'] + report['diff']['variables']['rescoped'], [])
        self.assertIn('Version 1 did not count firings: "fired" conditions count from now.', report['warnings'])

    def test_a_phase_taken_away_needs_a_phase_to_land_in(self):
        p = EXAMPLES['winter-ferries']
        state = rules.begin(p, [ASH, BIRCH], 10)
        state.update(phase='ferry-watch', visits=2, disabled=['ferry-watch/begin-deep-freeze'],
                     fired={}, campaignFired={'open-winter-ledger': ['*']},
                     timers={'first-freeze': {'signal': 'first-freeze', 'at': 38, 'clock': 'game', 'actor': ASH, 'interval': 0, 'scope': 'campaign'}})

        new = renamed(p, 'ferry-watch', 'the-watch')
        moved, report = rules.migration(p, new, state)
        self.assertIsNone(moved)
        self.assertIn('name the phase to land in', report['blocked'])
        self.assertTrue(report['phase']['gone'])
        self.assertIn('the-watch', report['phase']['choices'])
        self.assertIn('ferry-watch', report['diff']['phases']['removed'])
        moved, report = rules.migration(p, new, state, land='the-watch')
        self.assertIsNone(report['blocked'])
        self.assertEqual((moved['phase'], moved['visits']), ('the-watch', 2), 'the same visit, as the new phase')
        self.assertEqual(moved['disabled'], ['the-watch/begin-deep-freeze'], 'the visit\'s own triggers follow it')
        self.assertIn('first-freeze', moved['timers'], 'the-watch still listens for first-freeze')
        self.assertIn("is in the new version", rules.migration(p, p, state, land='enrol')[1]['blocked'],
                      'a phase that is still there is not jumped from')
        self.assertIn('ends the story', rules.migration(p, new, state, land='ferries-reopen')[1]['blocked'])
        self.assertIn('another story', rules.migration(p, edited(p, lambda q: q.update(id='other')), state)[1]['blocked'])

    def test_a_variable_retyped_or_rescoped_starts_at_its_first_value(self):
        new = edited(WAITER, lambda q: q.update(variables=[
            {'id': 'bells', 'type': 'switch', 'scope': 'story'}, {'id': 'mood', 'type': 'number', 'scope': 'player'},
            {'id': 'rung', 'type': 'number', 'scope': 'player'}, {'id': 'fresh', 'type': 'text', 'scope': 'story', 'initial': 'hi'}]))
        new['phases'][0]['triggers'][0]['actions'][0] = {'do': 'turn on', 'var': 'bells'}
        new['phases'][0]['triggers'][0]['actions'][3] = {'do': 'turn on', 'var': 'bells'}
        new['phases'][0]['triggers'][0]['actions'][4]['if'] = {'test': 'switch', 'var': 'bells', 'is': True}
        new['phases'][0]['triggers'][1]['actions'] = [{'do': 'add', 'var': 'mood', 'value': 1}]
        rules.validate(new)
        state = rules.begin(WAITER, ['ash', 'birch'], 1, 'world-w')
        state['counters'] = {'story': {'bells': 4, 'mood': 2}, 'player': {'ash': {'rung': 1}}}
        moved, report = rules.migration(WAITER, new, state)
        self.assertEqual(moved['counters'], {'player': {'ash': {'rung': 1}}})
        why = {x['key']: x['why'] for x in report['defaulted']}
        self.assertIn('it was a number, it is a switch now', why['bells'])
        self.assertIn("it is now player's, not story's", why['mood'])
        self.assertEqual([x['key'] for x in report['added'] if x['what'] == 'variable'], ['fresh'])
        diff = report['diff']['variables']
        self.assertEqual(diff['retyped'], [{'id': 'bells', 'scope': 'story', 'from': 'number', 'to': 'switch'}])
        self.assertEqual(diff['rescoped'], [{'id': 'mood', 'from': 'story', 'to': 'player'}])

    def waiting_state(self):
        state = rules.begin(WAITER, ['ash', 'birch'], 1, 'world-w')
        rules.advance(WAITER, state, {'kind': 'arrival', 'actor': 'ash', 'id': 1}, 1, [])
        self.assertEqual(len(state['waiting']), 1)
        self.assertEqual(state['waiting'][0]['next'], [3])
        return state

    def test_a_wait_part_way_through_a_list_is_kept_only_while_what_it_still_runs_is_unchanged(self):
        state = self.waiting_state()
        # Another trigger changed: the wait is kept, and it resumes in the new version.
        other = edited(WAITER, lambda q: q['phases'][0]['triggers'][1]['actions'].append({'do': 'note', 'text': 'Chatter.'}))
        moved, report = rules.migration(WAITER, other, state)
        self.assertEqual(len(moved['waiting']), 1)
        self.assertEqual(lines(report, 'kept', 'wait')[0]['key'], 'watch/ring')
        self.assertTrue(rules.advance(other, moved, {'kind': 'clock'}, 3, []), 'it resumes and finishes its list')
        self.assertEqual((moved['phase'], moved['counters']['story']['bells']), ('done', 11))
        # What it still has to run changed: the wait is dropped (the rest of its actions will not run).
        later = edited(WAITER, lambda q: q['phases'][0]['triggers'][0]['actions'][3].update(value=20))
        moved, report = rules.migration(WAITER, later, state)
        self.assertNotIn('waiting', moved)
        self.assertIn('what it still had to run has changed', lines(report, 'dropped', 'wait')[0]['why'])
        # An action before it changed: the list it points into is not the same list.
        earlier = edited(WAITER, lambda q: q['phases'][0]['triggers'][0]['actions'][0].update(value=2))
        self.assertNotIn('waiting', rules.migration(WAITER, earlier, state)[0])
        # Its trigger gone; a version 1 program: dropped too.
        gone = edited(WAITER, lambda q: q['phases'][0]['triggers'][0].update(id='ring-again'))
        self.assertIn('no such trigger', lines(rules.migration(WAITER, gone, state)[1], 'dropped', 'wait')[0]['why'])

    def test_a_timer_whose_listener_went_is_dropped(self):
        p = EXAMPLES['winter-ferries']
        state = rules.begin(p, [ASH], 10)
        state.update(phase='ferry-watch', visits=2, timers={
            'watch-round': {'signal': 'watch-round', 'at': 17, 'clock': 'game', 'actor': ASH, 'interval': 7, 'scope': 'campaign'},
            'first-freeze': {'signal': 'first-freeze', 'at': 38, 'clock': 'game', 'actor': ASH, 'interval': 0, 'scope': 'campaign'}})
        deaf = edited(p, lambda q: q.update(triggers=[t for t in q['triggers'] if t['id'] != 'log-watch-round']))
        moved, report = rules.migration(p, deaf, state)
        self.assertEqual(sorted(moved['timers']), ['first-freeze'])
        self.assertIn('nothing in the new version listens for its signal', lines(report, 'dropped', 'timer')[0]['why'])
        self.assertIn('log-watch-round', report['diff']['triggers']['removed'])

    def test_missions_kept_when_their_objectives_are_the_same_and_reset_when_not(self):
        p = EXAMPLES['river-relief']
        state = rules.begin(p, [ASH, BIRCH], 10)
        rules.advance(p, state, {'kind': 'conversation', 'actor': ASH, 'target': 'rm111', 'id': 1}, 10, [])
        ids = sorted(state['missions'])
        self.assertTrue(ids)
        first = ids[0]

        def change(q):
            m = next(m for m in q['phases'][0]['missions'] if m['id'] == first)
            m['title'] = m['title'] + ' again'
        moved, report = rules.migration(p, edited(p, change), state)
        self.assertEqual(moved['missions'], state['missions'])
        self.assertIn('progress kept', lines(report, 'kept', 'mission')[0]['why'])

        def objectives(q):
            m = next(m for m in q['phases'][0]['missions'] if m['id'] == first)
            m['objectives'][0]['label'] = 'Something else entirely.'
        moved, report = rules.migration(p, edited(p, objectives), state)
        self.assertNotIn(first, moved.get('missions', {}))
        self.assertEqual([x['key'] for x in report['reset']], [first])

    def test_a_trigger_newly_off_is_turned_off_and_one_newly_on_stays_as_the_run_has_it(self):
        state = rules.begin(WAITER, ['ash'], 1, 'world-w')
        off = edited(WAITER, lambda q: q['phases'][0]['triggers'][1].update(enabled=False))
        moved, report = rules.migration(WAITER, off, state)
        self.assertEqual(moved['disabled'], ['watch/chat'])
        self.assertEqual(report['diff']['triggers']['newlyOff'], ['watch/chat'])
        moved, report = rules.migration(off, WAITER, moved)
        self.assertEqual(moved['disabled'], ['watch/chat'])
        self.assertIn('it stays off in this run', report['warnings'][0])

    def test_the_diff_names_what_changed(self):
        p = EXAMPLES['winter-ferries']

        def change(q):
            q['triggers'][1]['note'] = 'Ivo keeps the ledger.'
            q['triggers'][0]['actions'][0]['days'] = 6
            q['phases'].append({'id': 'thaw', 'journal': 'The thaw.', 'effects': [], 'exits': []})
            q['phases'][1]['triggers'].append({'id': 'early-thaw', 'on': 'story signal', 'signal': 'thaw-now',
                                               'actions': [{'do': 'enter', 'phase': 'thaw'}]})
            q['regions'] = [{'id': 'pier', 'cell': 'c_0_0', 'x1': 1, 'y1': 1, 'x2': 2, 'y2': 2}]
        d = rules.version_diff(p, edited(p, change))
        changed = {c['id']: c for c in d['triggers']['changed']}
        self.assertTrue(changed['offer-a-coil']['wordsOnly'])
        self.assertEqual(changed['open-winter-ledger']['what'], ['actions'])
        self.assertEqual(d['phases']['added'], ['thaw'])
        self.assertEqual(d['timers']['changed'], ['watch-round'])
        self.assertEqual(d['regions']['added'], ['pier'])
        self.assertFalse(d['same'])
        self.assertTrue(rules.version_diff(p, p)['same'])


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class Moving(unittest.TestCase):
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    setUp = console.ConsoleReads.setUp
    fact, launch, answer, tick, row = (console.ConsoleReads.fact, console.ConsoleReads.launch, console.ConsoleReads.answer,
                                       console.ConsoleReads.tick, console.ConsoleReads.row)

    def one(self):
        return runner.status(self.conn, self.world, self.run_id)

    def answer_all(self, status='applied', result='ok'):
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute("UPDATE dm.actions SET status=%s,result=%s,done_at=now() WHERE status='queued'", (status, result))
        self.conn.execute('SET LOCAL ROLE ratw_dm')

    def settle(self, rounds=6):
        for _ in range(rounds):
            self.answer_all()
            self.tick()

    def send(self, kind, payload, digest=None, visit=None):
        one = self.one()
        return runner.command(self.conn, self.world, 'martin', {
            'run': self.run_id, 'kind': kind, 'payload': payload, 'reason': 'a fix on day forty', 'key': secrets.token_hex(8),
            'visit': visit or one['visit'], 'digest': digest or one['digest']})

    def taken(self, sent):
        return self.conn.execute('SELECT status,result FROM dm.story_commands WHERE id=%s', (sent['id'],)).fetchone()

    def ferries_in_watch(self):
        self.launch('winter-ferries')
        self.answer()
        self.fact('arrival', ASH)
        self.tick()
        self.settle()
        self.fact('gift', BIRCH, 'rm111', item='rope', quantity=3)
        self.tick()
        self.assertEqual((self.one()['phase'], self.one()['state']['counters']['story']['stores']), ('ferry-watch', 3))

    def approve_for_run(self, program):
        return runner.approve(self.conn, self.world, 'ann', program, rules.digest(program), self.run_id)

    def move(self, program, **payload):
        approved = self.approve_for_run(program)
        sent = self.send('migrate', {'version': approved['version'], **payload})
        self.tick()
        return approved, sent, self.taken(sent)

    def revise_orders(self):
        return [r for r in self.conn.execute("SELECT id,payload,status FROM dm.actions WHERE kind='story.phase' ORDER BY id").fetchall()
                if r[1].get('revise')]

    def test_a_v1_run_moved_onto_a_v2_version_keeps_its_phase_values_and_timers(self):
        self.ferries_in_watch()
        old = self.one()
        two = rules.upgrade(EXAMPLES['winter-ferries'])
        two['phases'][1]['journal'] = 'Keep the ferries supplied through the freeze.'
        approved = self.approve_for_run(two)
        self.assertEqual(approved['report']['phase'], {'from': 'ferry-watch', 'to': 'ferry-watch', 'choices': [p['id'] for p in two['phases']], 'gone': False})
        listed = next(v for v in self.one()['versions'] if v['id'] == approved['version'])
        self.assertEqual((listed['status'], listed['report']['blocked']), ('approved', None), 'the console reads it with a fresh report')
        # The dry run says what it will do and leaves nothing behind.
        dry = runner.explain(self.conn, self.world, self.run_id, {'command': {'kind': 'migrate', 'payload': {'version': approved['version']}}}, 'martin')
        self.assertEqual(dry['outcome'], 'done', dry)
        self.assertIn('kept', dry['migration'])
        self.assertEqual(self.one()['digest'], old['digest'])
        self.assertEqual(self.revise_orders(), [], 'a dry run sends nothing')
        sent = self.send('migrate', {'version': approved['version']})
        behind = self.send('set', {'key': 'stores', 'scope': 'story', 'value': 1})
        self.tick()
        status, result = self.taken(sent)
        self.assertEqual(status, 'done', result)
        self.assertEqual(self.taken(behind)[0], 'refused')
        self.assertIn('another version', self.taken(behind)[1]['text'])
        now = self.one()
        self.assertEqual((now['digest'], now['program']['version'], now['approvedBy']), (rules.digest(two), 2, 'ann'))
        self.assertEqual((now['phase'], now['visit']), ('ferry-watch', old['visit']), 'the same visit')
        self.assertEqual(now['state']['counters']['story']['stores'], 3)
        self.assertEqual(sorted(now['state']['timers']), sorted(old['state']['timers']))
        self.assertTrue(now['state']['entrySent'])
        orders = self.revise_orders()
        self.assertEqual(len(orders), 1)
        self.assertEqual((orders[0][1]['visit'], orders[0][1]['journal']), (old['visit'], 'Keep the ferries supplied through the freeze.'))
        # Until the game has it, the run reads nothing.
        self.fact('gift', BIRCH, 'rm111', item='rope', quantity=2)
        self.tick()
        self.assertEqual(self.one()['state']['counters']['story']['stores'], 3)
        self.assertEqual(self.row()['waiting'][0]['on'], 'game')
        refused = self.send('set', {'key': 'stores', 'scope': 'story', 'value': 9})
        self.tick()
        self.assertEqual(self.taken(refused)[0], 'refused', 'nothing else is steered while it moves')
        self.answer_all()
        self.tick()
        self.assertEqual(self.one()['state']['counters']['story']['stores'], 5, 'the new version reads what happened meanwhile')
        self.assertNotIn('moveReceipt', self.one()['state'])
        versions = self.one()['versions']
        self.assertEqual([v['status'] for v in versions], ['current', 'past'])
        self.assertEqual((versions[0]['from'], versions[0]['movedBy']), (old['digest'], 'martin'))
        # The timeline and the audit say so.
        items = runner.timeline(self.conn, self.world, self.run_id)['items']
        moved = next(i for i in items if i['type'] == 'version')
        self.assertEqual((moved['digest'], moved['from'], moved['status'], moved['phase']), (rules.digest(two), old['digest'], 'current', 'ferry-watch'))
        audit = self.conn.execute("SELECT detail FROM dm.audit WHERE action='story.migrate'").fetchall()
        self.assertTrue(any('Moving onto version' in d[0] for d in audit), audit)
        # A command made against the old version is refused: asked now, or queued before the move was taken.
        self.assertRaisesRegex(ValueError, 'another version', self.send, 'set', {'key': 'stores', 'scope': 'story', 'value': 1},
                               digest=old['digest'])
        # And it carries on as version 2: a gift counts.
        self.fact('gift', BIRCH, 'rm111', item='rope', quantity=1)
        self.tick()
        self.assertEqual(self.one()['state']['counters']['story']['stores'], 6)

    def test_refused_by_the_game_it_goes_back_to_the_version_it_was_on(self):
        self.ferries_in_watch()
        old = self.one()
        broken = edited(EXAMPLES['winter-ferries'], lambda q: q['triggers'][1].update(note='a fix'))
        self.move(broken)
        self.assertEqual(self.one()['digest'], rules.digest(broken))
        self.steer_hold = self.send('hold', {})
        self.tick()
        self.answer_all('refused', 'Story region does not fit its live cell: pier')
        self.tick()
        back = self.one()
        self.assertEqual((back['digest'], back['status']), (old['digest'], 'held'), 'back on its version, still held')
        self.assertEqual(back['state']['counters'], old['state']['counters'])
        self.assertNotIn('moveReceipt', back['state'])
        self.assertNotIn('entrySent', back['state'])
        self.assertEqual([v['status'] for v in back['versions']], ['refused', 'current'])
        self.assertIn('does not fit', back['versions'][0]['problem'])
        item = next(i for i in runner.timeline(self.conn, self.world, self.run_id)['items'] if i['type'] == 'version')
        self.assertEqual(item['status'], 'refused')
        self.assertTrue(self.conn.execute("SELECT count(*) FROM dm.audit WHERE action='story.migrate' AND detail LIKE '%%went back%%'").fetchone()[0])

    def test_moving_twice_and_a_phase_taken_away(self):
        self.ferries_in_watch()
        first = edited(EXAMPLES['winter-ferries'], lambda q: q['triggers'][1].update(note='first fix'))
        _, _, (status, result) = self.move(first)
        self.assertEqual(status, 'done', result)
        self.answer_all()
        self.tick()
        second = renamed(first, 'ferry-watch', 'the-watch')
        approved = self.approve_for_run(second)
        self.assertIn('name the phase to land in', approved['report']['blocked'])
        refused = self.send('migrate', {'version': approved['version']})
        self.tick()
        self.assertEqual(self.taken(refused)[0], 'refused')
        sent = self.send('migrate', {'version': approved['version'], 'phase': 'the-watch'})
        self.tick()
        status, result = self.taken(sent)
        self.assertEqual(status, 'done', result)
        self.assertEqual(self.revise_orders()[-1][1]['phase'], 'the-watch')
        self.answer_all()
        self.tick()
        now = self.one()
        self.assertEqual((now['phase'], now['digest']), ('the-watch', rules.digest(second)))
        self.assertEqual([v['status'] for v in now['versions']], ['current', 'past', 'past'])
        self.assertEqual(len([i for i in runner.timeline(self.conn, self.world, self.run_id)['items'] if i['type'] == 'version']), 2)

    def test_entry_effects_are_not_sent_again_and_an_older_approval_is_withdrawn(self):
        self.ferries_in_watch()
        louder = rehearsed(edited(EXAMPLES['winter-ferries'], lambda q: q['phases'][1]['effects'].append(
            {'kind': 'band.override', 'npc': 'rm111', 'band': 'comfortable', 'days': 3})))
        quieter = edited(louder, lambda q: q.update(title=q['title'] + '.'))
        older = self.approve_for_run(louder)
        _, _, (status, result) = self.move(quieter)
        self.assertEqual(status, 'done', result)
        self.settle()
        self.assertEqual(self.conn.execute("SELECT count(*) FROM dm.actions WHERE kind='band.override'").fetchone()[0], 0,
                         'the visit\'s new entry effect is not sent by the move')
        statuses = {v['id']: v['status'] for v in self.one()['versions']}
        self.assertEqual(statuses[older['version']], 'withdrawn')
        late = self.send('migrate', {'version': older['version']})
        self.tick()
        self.assertEqual(self.taken(late)[0], 'refused')

    def test_a_v2_run_waiting_part_way_through_a_list_resumes_in_the_new_version(self):
        program = edited(WAITER, lambda q: q.update(audience={'kind': 'players', 'players': [ASH, BIRCH]},
                                                    tests=[{**q['tests'][0], 'cohort': [ASH, BIRCH],
                                                            'events': [{**q['tests'][0]['events'][0], 'actor': ASH}, q['tests'][0]['events'][1]]}]))
        ticket = runner.approve(self.conn, self.world, 'martin', program, rules.digest(program))['ticket']
        self.run_id = runner.publish(self.conn, self.world, ticket)['run']
        self.answer()
        self.fact('arrival', ASH, day=10.5)
        self.tick()
        self.assertEqual(len(self.one()['state']['waiting']), 1)
        other = edited(program, lambda q: q['phases'][0]['triggers'][1]['actions'].append({'do': 'note', 'text': 'Chatter.'}))
        _, _, (status, result) = self.move(other)
        self.assertEqual(status, 'done', result)
        self.answer_all()
        self.tick()
        self.assertEqual(len(self.one()['state']['waiting']), 1, 'kept')
        self.conn.execute('RESET ROLE')
        self.conn.execute("UPDATE game.checkpoints SET payload='{\"calendarDays\":12}' WHERE world_id=%s", (self.world,))
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        self.tick()
        self.settle()
        self.assertEqual(self.one()['phase'], 'done', 'it resumed and finished its list in the new version')



@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class MovingRoute(unittest.TestCase):
    """Approving as a new version of a run and moving it, through POST /api/story-program: a DM in the user's own studio, a
    viewer not; on a hosted world the move is a steering command (the owner, or "may steer campaigns")."""
    setUpClass = classmethod(test_story_steering.SteeringRoute.setUpClass.__func__)
    tearDownClass = classmethod(test_story_steering.SteeringRoute.tearDownClass.__func__)
    serve = classmethod(test_story_steering.SteeringRoute.serve.__func__)
    call, token, body = (test_story_steering.SteeringRoute.call, test_story_steering.SteeringRoute.token,
                         test_story_steering.SteeringRoute.body)

    def test_approve_as_a_new_version_then_move_it(self):
        run = self.runs['winter-ferries']
        new = edited(EXAMPLES['winter-ferries'], lambda q: q['triggers'][1].update(note='Ivo keeps the ledger.'))
        viewer = self.token('camp-viewer', 'a long viewer password')
        status, data = self.call({'op': 'approve', 'target': 'dev', 'program': new, 'digest': rules.digest(new), 'forRun': run}, viewer)
        self.assertEqual(status, 403, data)
        dm = self.token('camp-dm', 'a long dm password')
        status, data = self.call({'op': 'approve', 'target': 'dev', 'program': new, 'digest': rules.digest(new), 'forRun': run}, dm)
        self.assertEqual(status, 200, data)
        self.assertNotIn('ticket', data, 'nothing to publish: it is moved onto')
        self.assertEqual((data['run'], data['report']['blocked']), (run, None))
        self.assertTrue(data['report']['diff']['triggers']['changed'][0]['wordsOnly'])
        other = edited(new, lambda q: q.update(id='another-story'))
        status, data2 = self.call({'op': 'approve', 'target': 'dev', 'program': other, 'digest': rules.digest(other), 'forRun': run}, dm)
        self.assertEqual(status, 422, data2)
        status, one = self.call({'op': 'status', 'target': 'dev', 'run': run}, dm)
        self.assertEqual([v['status'] for v in one['versions']], ['approved'])
        move = self.body('winter-ferries', 'migrate', {'version': data['version']})
        status, refused = self.call(move, session='dm', port=self.hosted_port)
        self.assertEqual(status, 403, refused)
        self.assertIn('may steer campaigns', refused['error'])
        status, sent = self.call(move, session='steerer', port=self.hosted_port)
        self.assertEqual((status, sent['kind']), (200, 'migrate'), sent)


if __name__ == '__main__':
    unittest.main()
