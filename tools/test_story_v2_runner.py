"""A version 2 story run by the world's story runner (Docs/Design/85-story-triggers-and-campaign-control.md, Phase 5), in a
throwaway, migrated database: declared variables in the run's state and the world's (`dm.story_world_vars`, migration
0077), a wait that resumes when the world's clock reaches it, notes in the history, a trigger that starts off and is
turned on from the console, typed values set from the console, the dry run and "why not" over the new conditions, and the
story finishing itself (the game told with `story.finish`, then complete). The game is stood in for by answering
dm.actions rows by hand. Skipped without the local PostgreSQL."""

import json
import unittest

import story_program as rules
import story_runner as runner
import test_story_console as console
import test_story_steering as steering
from test_story_console import WOLVES
from test_world_db import database_available

ASH, BIRCH, ELM = sorted(WOLVES)

PROGRAM = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'v2-runner', 'title': 'Two words with Ivo',
    'audience': {'kind': 'players', 'players': sorted(WOLVES)}, 'start': 'open',
    'variables': [
        {'id': 'tally', 'type': 'number', 'scope': 'story', 'show': 'Words had'},
        {'id': 'best', 'type': 'number', 'scope': 'world'},
        {'id': 'seen', 'type': 'wolves', 'scope': 'story'},
        {'id': 'mood', 'type': 'text', 'scope': 'story', 'initial': 'calm'},
        {'id': 'asked', 'type': 'switch', 'scope': 'player'},
    ],
    'phases': [{'id': 'open', 'journal': 'Talk with Ivo twice.', 'effects': [], 'exits': [], 'triggers': [
        {'id': 'count', 'on': 'conversation', 'audience': 'participant', 'match': {'target': 'rm111'}, 'actions': [
            {'do': 'add', 'var': 'tally', 'value': 1},
            {'do': 'turn on', 'var': 'asked'},
            {'do': 'list add', 'var': 'seen', 'value': {'field': 'actor'}},
            {'do': 'set', 'var': 'best', 'value': {'max': [{'var': 'best'}, {'var': 'tally'}]}},
            {'do': 'if', 'if': {'test': 'compare', 'left': {'var': 'tally'}, 'op': '>=', 'right': 2},
             'then': [{'do': 'note', 'text': 'Two words had.'}, {'do': 'wait', 'days': 0.5},
                      {'do': 'effect', 'effect': {'kind': 'deed.award', 'player': '$actor', 'detail': 'Heard Ivo out.'}},
                      {'do': 'finish', 'outcome': 'success', 'text': 'Ivo was heard.'}]},
        ]},
        {'id': 'late', 'on': 'arrival', 'enabled': False, 'if': {'test': 'switch', 'var': 'asked', 'is': True},
         'actions': [{'do': 'note', 'text': 'A late arrival.'}, {'do': 'set', 'var': 'mood', 'value': 'busy'}]},
    ]}],
    'tests': [{'name': 'Two words', 'cohort': sorted(WOLVES),
               'events': [{'id': 'a', 'kind': 'conversation', 'day': 1, 'actor': ASH, 'target': 'rm111'},
                          {'id': 'b', 'kind': 'conversation', 'day': 1.1, 'actor': BIRCH, 'target': 'rm111'},
                          {'id': 'c', 'kind': 'arrival', 'day': 2, 'actor': ELM}],
               'expect': ['open'], 'expectOutcome': 'success',
               'expectVariables': {'story': {'tally': 2, 'seen': [ASH, BIRCH]}, 'world': {'best': 2}}}],
}


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class VersionTwoRuns(unittest.TestCase):
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    fact, launch, answer, tick, rewrite, row = (console.ConsoleReads.fact, console.ConsoleReads.launch, console.ConsoleReads.answer,
                                                console.ConsoleReads.tick, console.ConsoleReads.rewrite, console.ConsoleReads.row)
    one, send, taken, steer, answer_all, settle = (steering.Steering.one, steering.Steering.send, steering.Steering.taken,
                                                   steering.Steering.steer, steering.Steering.answer_all, steering.Steering.settle)

    def setUp(self):
        console.ConsoleReads.setUp(self)
        self.examples = {**self.examples, 'v2-runner': json.loads(json.dumps(PROGRAM))}

    def day(self, n):
        self.rewrite(f"UPDATE game.checkpoints SET payload='{{\"calendarDays\":{n}}}' WHERE world_id='{self.world}'")

    def state(self):
        return self.one()['state']

    def world_vars(self):
        return dict(self.conn.execute('SELECT name,value FROM dm.story_world_vars WHERE world_id=%s', (self.world,)).fetchall())

    def test_the_rehearsal_passes(self):
        self.assertTrue(rules.review(PROGRAM)['passed'])

    def test_a_version_2_story_waits_notes_keeps_a_world_value_and_finishes(self):
        self.launch('v2-runner')
        self.assertEqual(self.state()['disabled'], ['open/late'], 'it starts with "late" off')
        self.assertEqual(self.state()['seed'], self.run_id, "the run's id seeds its rolls")
        self.answer()
        self.fact('conversation', ASH, 'rm111')
        self.tick()
        self.assertEqual(self.state()['counters']['story'], {'tally': 1, 'seen': [ASH]})
        self.assertEqual(self.world_vars(), {'best': 1}, 'the world keeps its value')
        self.fact('conversation', BIRCH, 'rm111')
        self.tick()
        state = self.state()
        self.assertEqual([w['trigger'] for w in state['waiting']], ['count'])
        self.assertEqual(state['waiting'][0]['at'], 10.5)
        history = self.conn.execute('SELECT entry FROM dm.story_trigger_history WHERE run=%s ORDER BY id', (self.run_id,)).fetchall()
        self.assertEqual(history[-1][0]['notes'], ['Two words had.'])
        changes = {(c['key'], json.dumps(c['after'])) for c in history[-1][0]['changes']}
        self.assertIn(('tally', '2'), changes)
        self.assertIn(('best', '2'), changes)
        self.assertEqual(self.world_vars(), {'best': 2})
        timeline = runner.timeline(self.conn, self.world, self.run_id)
        self.assertEqual([i['notes'] for i in timeline['items'] if i.get('notes')], [['Two words had.']])
        # Half a day later the rest of its actions run: the deed, then the story ends itself.
        self.tick()
        self.assertIn('waiting', self.state(), 'not due yet')
        self.day(10.6)
        self.tick()
        state = self.state()
        self.assertNotIn('waiting', state)
        resumed = self.conn.execute('SELECT entry FROM dm.story_trigger_history WHERE run=%s ORDER BY id DESC LIMIT 1', (self.run_id,)).fetchone()[0]
        self.assertEqual((resumed['trigger'], resumed['via']), ('count', 'wait'))
        self.assertEqual(state['outcome']['outcome'], 'success')
        self.settle()
        deed = self.conn.execute("SELECT id,target_id,payload FROM dm.actions WHERE kind='deed.award'").fetchall()
        self.assertEqual([d[1:] for d in deed], [(BIRCH, {'detail': 'Heard Ivo out.'})], 'for the wolf who waited, with the event it waited with')
        self.assertEqual(self.one()['status'], 'complete')
        finish = self.conn.execute("SELECT id,payload FROM dm.actions WHERE kind='story.finish' AND target_id=%s", (self.run_id,)).fetchall()
        self.assertEqual([f[1] for f in finish], [{'outcome': 'success', 'text': 'Ivo was heard.'}])
        self.assertLess(deed[0][0], finish[0][0], 'the deed goes before the story is told it is over')
        # Another run of a story declaring "best" reads the world's value.
        other = json.loads(json.dumps(PROGRAM))
        state = rules.begin(other, sorted(WOLVES), 0)
        runner.world_load(self.conn, self.world, other, state)
        self.assertEqual(state['counters']['world'], {'best': 2})
        other['variables'][1]['type'] = 'text'
        state = rules.begin(other, sorted(WOLVES), 0)
        runner.world_load(self.conn, self.world, other, state)
        self.assertNotIn('counters', state, 'a value kept under another type is not read')

    def test_the_console_turns_on_a_trigger_that_starts_off_and_sets_typed_values(self):
        self.launch('v2-runner')
        self.answer()
        controls = self.one()['controls'] if 'controls' in self.one() else runner.controls(self.examples['v2-runner'], self.state())
        late = next(t for t in controls['triggers'] if t['id'] == 'late')
        self.assertTrue(late['off'])
        variables = {v['key']: v for v in controls['counters']}
        self.assertEqual(variables['mood'], {'key': 'mood', 'scope': 'story', 'type': 'text', 'initial': 'calm'})
        self.assertEqual(variables['tally']['show'], 'Words had')
        with self.assertRaisesRegex(ValueError, r'Choose a value of its type \(text\)'):
            self.send('set', {'key': 'mood', 'scope': 'story', 'value': 3})
        status, result = self.steer('set', {'key': 'mood', 'scope': 'story', 'value': 'wary'})
        self.assertEqual((status, result['before'], result['after']['value']), ('done', {'value': 'calm'}, 'wary'))
        status, result = self.steer('set', {'key': 'asked', 'scope': 'player', 'player': ELM, 'value': True})
        self.assertEqual(status, 'done')
        status, result = self.steer('set', {'key': 'best', 'scope': 'world', 'value': 7})
        self.assertEqual(self.world_vars(), {'best': 7}, 'set from the console, kept for the world')
        # Why not: "late" is off; then turned on, its condition is judged for the wolf.
        why = runner.explain(self.conn, self.world, self.run_id, {'trigger': 'late'})
        self.assertTrue(why)
        self.assertEqual(self.steer('enable', {'trigger': 'late'})[0], 'done')
        self.assertNotIn('disabled', self.state())
        judged = next(i for i in runner.explain(self.conn, self.world, self.run_id)['listeners'] if i.get('id') == 'late')
        self.assertTrue(judged['startsOff'])
        self.assertEqual(judged['forWolves'], [ELM], 'only the wolf whose own switch is on')
        self.fact('arrival', ELM)
        self.tick()
        self.assertEqual(self.state()['counters']['story']['mood'], 'busy')

    def test_a_dry_run_of_fire_on_a_version_2_trigger(self):
        self.launch('v2-runner')
        self.answer()
        out = runner.explain(self.conn, self.world, self.run_id, {'command': {'kind': 'fire', 'payload': {'trigger': 'count', 'actor': ASH,
                                                                                                         'check': False}}}, 'martin')
        self.assertEqual(out['outcome'], 'done', out)
        self.assertEqual(self.state().get('counters', {}).get('story', {}).get('tally'), None, 'a dry run changes nothing')
        status, result = self.steer('fire', {'trigger': 'count', 'actor': ASH, 'check': False})
        self.assertEqual(status, 'done', result)
        self.assertEqual(self.state()['counters']['story']['tally'], 1)
        self.assertEqual(self.state()['counts'], {'visit': {'open/count': 1}, 'run': {'open/count': 1}})


# ----- Doc 86, Phase 1a: a chapter paced by readiness (R2), run by the story runner on a throwaway database (the fixture,
# ready_program and ReadyFixture, is test_story_console's, shared with the steering, explain and campaign tests).
from test_story_console import MUSTER, ReadyFixture, ready_program  # noqa: E402


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class Readiness(ReadyFixture, unittest.TestCase):
    """R2 in the runner (doc 86, Phase 1a), on a chapter with participation 35% floor 3 and minDays 2."""
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    setUp = VersionTwoRuns.setUp
    fact, launch, answer, tick, rewrite, row = (console.ConsoleReads.fact, console.ConsoleReads.launch, console.ConsoleReads.answer,
                                                console.ConsoleReads.tick, console.ConsoleReads.rewrite, console.ConsoleReads.row)
    one, send, taken, steer, answer_all, settle = (steering.Steering.one, steering.Steering.send, steering.Steering.taken,
                                                   steering.Steering.steer, steering.Steering.answer_all, steering.Steering.settle)
    def test_the_fixture_rehearses(self):
        for p in (ready_program(), ready_program(auto=True), ready_program(auto=True, then={'on': 'day begins'}), ready_program(after=2)):
            self.assertTrue(rules.review(p)['passed'], p['phases'][0]['ready'])

    def test_seen_at_launch_then_ready_latched_and_waiting_for_an_admin(self):
        self.ready_launch(ready_program(slow=4))
        self.assertEqual(sorted(self.state()['seen']), MUSTER[:10], 'the wolves seen before it began are active from the start')
        self.gifts(MUSTER[:4], 1)
        self.tick()
        r = self.ready()
        self.assertFalse(r['met'])
        part = r['parts'][0]
        self.assertEqual((part['value'], part['count'], part['need'], part['share'], part['met']), (4, 10, 4, 40, True))
        self.assertEqual((r['parts'][1]['value'], r['parts'][1]['met']), (0, False), '4 of 10 on day 1: not met (no days yet)')
        self.day(3.1)
        self.tick()
        r = self.ready()
        self.assertTrue(r['met'])
        self.assertEqual((r['metAt'], self.one()['phase']), (3.1, 'work'), 'auto is off: it is not moved')
        self.assertIn('ready', self.alert_kinds())
        alert = next(a for a in self.row()['alerts'] if a['kind'] == 'ready')
        self.assertEqual((alert['needsYou'], alert['to'], alert['met'], alert['parts']), (True, 'done', 2, 2))
        self.assertNotIn('stuck', self.alert_kinds(), 'a phase whose way on is Readiness is never "stuck"')
        # Six more wolves come: the share falls below the bar, and it stays ready (latched).
        for w in MUSTER[10:]:
            self.fact('arrival', w, day=3.2)
        self.tick()
        r = self.ready()
        self.assertEqual((r['parts'][0]['count'], r['parts'][0]['need'], r['parts'][0]['met'], r['met']), (16, 6, False, True))
        self.assertEqual(self.row()['ready']['to'], 'done')
        self.assertEqual([e['kind'] for e in self.state()['readyLog']], ['ready'])
        timeline = runner.timeline(self.conn, self.world, self.run_id)
        self.assertEqual([(i['readyKind'], i['key']) for i in timeline['items'] if i['type'] == 'ready'], [('ready', 'r:1')],
                         'keyed by its own number, which stays as the log is trimmed')
        older = runner.timeline(self.conn, self.world, self.run_id, before={'ready': 1})
        self.assertEqual([i for i in older['items'] if i['type'] == 'ready'], [], 'an older page leaves out what a newer one showed')
        waiting = next(w for w in self.row()['waiting'] if w['on'] == 'ready')
        self.assertEqual((waiting['to'], waiting['auto'], waiting['met']), ('done', False, True))

    def test_auto_moves_in_the_same_tick(self):
        self.ready_launch(ready_program(auto=True))
        self.gifts(MUSTER[:4], 1)
        self.day(3.1)
        self.tick()
        self.assertEqual(self.one()['phase'], 'done')
        self.assertEqual(self.one()['history'][0]['cause'], 'ready')

    def test_then_waits_for_the_next_day(self):
        self.ready_launch(ready_program(auto=True, then={'on': 'day begins'}))
        self.assertIn('day begins', rules.watch_of(self.examples['v2-ready'])['events'], 'the game is asked for its moment')
        self.gifts(MUSTER[:4], 1)
        self.day(3.1)
        self.tick()
        self.assertEqual((self.one()['phase'], self.ready()['waitingFor']), ('work', {'on': 'day begins'}))
        self.assertNotIn('ready', self.alert_kinds(), 'it goes by itself at its moment')
        self.fact('day begins', '', day=3.5, detail='Restday')
        self.tick()
        self.assertEqual(self.one()['phase'], 'done')

    def test_held_days_are_not_counted(self):
        self.ready_launch(ready_program())
        self.gifts(MUSTER[:4], 1)
        self.tick()
        self.day(1.5)
        self.assertEqual(self.steer('hold')[0], 'done')
        self.day(4.5)
        status, result = self.steer('resume')
        self.assertEqual(status, 'done', result)
        self.assertEqual(self.state()['heldDays'], {'run': 3.0, 'visit': 3.0})
        self.day(4.6)
        self.fact('arrival', MUSTER[0], day=4.6)
        self.tick()
        days = self.ready()['parts'][1]
        self.assertEqual((round(days['value'], 2), days['held'], days['met']), (0.6, 3.0, False), 'three held days do not count')
        self.day(6.1)
        self.fact('arrival', MUSTER[0], day=6.1)
        self.tick()
        self.assertTrue(self.ready()['met'])

    def test_slow_after_its_days_and_auto_after_with_no_admin(self):
        self.ready_launch(ready_program(slow=4, after=2))
        self.gifts(MUSTER[:2], 1)
        self.day(5.5)
        self.fact('arrival', MUSTER[0], day=5.5)
        self.tick()
        self.assertIn('slow', self.alert_kinds())
        slow = next(a for a in self.row()['alerts'] if a['kind'] == 'slow')
        self.assertEqual((slow['needsYou'], slow['part']['label']), (False, 'Wolves who brought rope'))
        self.assertIn('2 of 10 active wolves', slow['words'])
        self.gifts(MUSTER[2:4], 5.6)
        self.tick()
        self.assertTrue(self.ready()['met'])
        self.assertEqual(self.one()['phase'], 'work')
        self.day(7.7)
        self.fact('arrival', MUSTER[0], day=7.7)
        self.tick()
        self.assertEqual(self.one()['phase'], 'done', 'ready and not advanced for two days: it went by itself')
        self.assertEqual(self.one()['history'][0]['cause'], 'ready (no admin)')

    def test_moved_onto_a_new_version_it_keeps_when_it_was_ready(self):
        p = ready_program()
        self.ready_launch(p)
        self.gifts(MUSTER[:4], 1)
        self.day(3.1)
        self.tick()
        self.assertEqual(self.ready()['metAt'], 3.1)
        same = json.loads(json.dumps(p))
        same['phases'][0]['journal'] = 'Bring rope to the muster, all of you.'
        approved = runner.approve(self.conn, self.world, 'ann', same, rules.digest(same), self.run_id)
        status, result = self.steer('migrate', {'version': approved['version']})
        self.assertEqual(status, 'done', result)
        # (Until the game answers the move, the kept record is what the console shows: its parts, met, and no crash.)
        kept = self.row()['ready']
        self.assertTrue(kept['met'])
        self.assertEqual([x['kind'] for x in kept['parts']], ['participation', 'minDays'])
        self.assertEqual(rules.ready_snapshot(self.state())['parts'], kept['parts'])
        self.settle()
        self.assertEqual(self.ready()['metAt'], 3.1, 'the same Readiness: still ready since day 3.1')
        changed = ready_program(minimum=5)
        changed['phases'][0]['journal'] = same['phases'][0]['journal']
        approved = runner.approve(self.conn, self.world, 'ann', changed, rules.digest(changed), self.run_id)
        status, result = self.steer('migrate', {'version': approved['version']})
        self.assertEqual(status, 'done', result)
        self.settle()
        self.assertFalse(self.ready()['met'], 'a changed Readiness is measured afresh')

    def test_moved_onto_a_version_with_readiness_its_wolves_seen_lately_are_active(self):
        # (Doc 86, C2 and §7.2: Act I published before Readiness moves onto the ready version; it is seeded as a launch is.)
        plain = ready_program()
        del plain['phases'][0]['ready']
        plain['phases'][0]['exits'] = [{'on': 'dm signal', 'to': 'done', 'match': {'detail': 'Move the chapter on'}}]
        plain['tests'] = [{'name': 'Moved on by the direction', 'cohort': MUSTER, 'expect': ['work', 'done'],
                           'events': [{'id': 's', 'kind': 'dm signal', 'day': 1, 'detail': 'Move the chapter on'}]}]
        self.assertFalse(rules.counts_audience(plain))
        self.ready_launch(plain)
        self.assertNotIn('seen', self.state())
        self.gifts(MUSTER[:4], 1)
        self.tick()
        ready = ready_program()
        approved = runner.approve(self.conn, self.world, 'ann', ready, rules.digest(ready), self.run_id)
        status, result = self.steer('migrate', {'version': approved['version']})
        self.assertEqual(status, 'done', result)
        self.assertEqual(sorted(self.state()['seen']), MUSTER[:10], 'the wolves seen before the move are active at once')
        self.settle()
        self.tick()
        part = self.ready()['parts'][0]
        self.assertEqual((part['value'], part['count'], part['met']), (4, 10, True))

    def test_a_wolf_who_joins_an_open_story_by_arriving_is_seen(self):
        # (Doc 86, C2: "a wolf who joins later is seen from their first event", the arrival that brought them in.)
        p = ready_program()
        p['audience'] = {'kind': 'everyone', 'days': 30, 'cap': 5000}
        p['tests'][0]['cohort'] = []
        self.ready_launch(p, active=0)
        late = 'player-late'
        self.rewrite(f"""INSERT INTO game.characters(world_id,key,position,data) VALUES ('{self.world}','{late}',1,'{{"name":"Late"}}'::jsonb)""")
        self.fact('arrival', late, day=1.2)
        self.tick()
        self.assertIn(late, self.state()['cohort'])
        self.assertEqual(self.state()['seen'].get(late), 1.2, 'seen from the arrival that brought them into the story')
        self.assertEqual(self.ready()['parts'][0]['count'], len(self.state()['seen']))



# ----- Doc 86, J2 (Phase 8): realm variables, each wolf's own, kept in dm.story_realm_vars (migration 0081) and shared by
# every story of the world that declares the name.
def realm_program(pid, kind='number', step=1):
    act = {'do': 'add', 'var': 'p_voice', 'value': step} if kind == 'number' else {'do': 'turn on', 'var': 'p_voice'}
    return {
        'format': 'ratw-story-program', 'version': 2, 'id': pid, 'title': 'Voices of ' + pid,
        'audience': {'kind': 'players', 'players': sorted(WOLVES)}, 'start': 'open',
        'variables': [{'id': 'p_voice', 'type': kind, 'scope': 'realm', 'show': 'Your voice'}],
        'phases': [{'id': 'open', 'journal': 'Lend your voice.', 'effects': [], 'exits': [], 'triggers': [
            {'id': 'voice', 'on': 'conversation', 'audience': 'participant', 'actions': [act]}]}],
        'tests': [{'name': 'A voice', 'cohort': sorted(WOLVES),
                   'events': [{'id': 'a', 'kind': 'conversation', 'day': 1, 'actor': ASH, 'target': 'rm111'}], 'expect': ['open'],
                   'expectVariables': {'realm': {ASH: {'p_voice': step if kind == 'number' else True}}}}],
    }


def bring_program():
    """A story of Ash alone that takes Birch in when its trigger "bring" is fired by hand."""
    p = realm_program('realm-bring')
    p['audience'] = {'kind': 'players', 'players': [ASH]}
    p['phases'][0]['triggers'].append({'id': 'bring', 'on': 'conversation', 'audience': 'participant', 'match': {'target': 'rm113'},
                                       'actions': [{'do': 'effect', 'effect': {'kind': 'participant.add', 'player': BIRCH}}]})
    p['tests'][0]['cohort'] = [ASH]
    return p


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class RealmVariables(unittest.TestCase):
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    fact, launch, answer, tick = console.ConsoleReads.fact, console.ConsoleReads.launch, console.ConsoleReads.answer, console.ConsoleReads.tick
    one, send, taken, steer, answer_all = (steering.Steering.one, steering.Steering.send, steering.Steering.taken,
                                           steering.Steering.steer, steering.Steering.answer_all)

    def setUp(self):
        console.ConsoleReads.setUp(self)
        self.examples = {**self.examples, **{p['id']: p for p in (realm_program('realm-a'), realm_program('realm-b', step=2),
                                                                   realm_program('realm-c'), bring_program())}}

    def kept(self, wolf=ASH):
        row = self.conn.execute("SELECT value FROM dm.story_realm_vars WHERE world_id=%s AND name='p_voice' AND wolf=%s",
                                (self.world, wolf)).fetchone()
        return row and row[0]

    def realm_of(self, run):
        state = self.conn.execute('SELECT state FROM dm.story_releases WHERE id=%s', (run,)).fetchone()[0]
        return state.get('counters', {}).get('realm', {})

    def test_the_rehearsal_reads_and_writes_a_wolfs_realm_value(self):
        self.assertTrue(rules.review(realm_program('realm-a'))['passed'])

    def test_two_runs_add_to_one_value_and_a_third_reads_it(self):
        a = self.launch('realm-a')
        self.answer()
        b = self.launch('realm-b')
        self.answer()
        self.fact('conversation', ASH, 'rm111')
        self.tick()
        self.assertEqual(self.kept(), 3, 'one adds 1, the other 2: both kept, neither lost')
        self.fact('conversation', ASH, 'rm111')
        self.tick()
        self.assertEqual(self.kept(), 6, "each run reads the other's write before it adds again")
        # (The runs take their turns one after the other, in either order: the second read the first's write.)
        self.assertEqual(sorted(self.realm_of(r)[ASH]['p_voice'] for r in (a, b)) in ([4, 6], [5, 6]), True)
        self.assertEqual(self.kept(BIRCH), None, 'nothing kept for a wolf who never spoke')
        # (Set from the console, for one wolf, with a reason and audited: a run reads it at its next turn.)
        out = runner.realm_var(self.conn, self.world, 'martin', {'name': 'p_voice', 'wolf': ASH, 'value': 10, 'reason': 'a test of the console'})
        self.assertEqual((out['before'], out['after']), ({'value': 6}, {'value': 10}))
        self.assertRaisesRegex(ValueError, 'Not a value for number', runner.realm_var, self.conn, self.world, 'martin',
                               {'name': 'p_voice', 'wolf': ASH, 'value': 'loud', 'reason': 'a test of the console'})
        self.assertRaisesRegex(ValueError, 'no wolf keeps it', runner.realm_var, self.conn, self.world, 'martin',
                               {'name': 'p_other', 'wolf': ASH, 'value': 1, 'reason': 'a test of the console'})
        listing = runner.realm_vars(self.conn, self.world, name='p_voice')
        self.assertEqual([(v['wolf'], v['value']) for v in listing['values']], [(ASH, 10)])
        self.assertEqual({s['story'] for s in listing['names'][0]['stories']}, {'realm-a', 'realm-b'})
        self.assertEqual(runner.realm_var_audit(self.conn, self.world)[0]['detail'].split(' Reason')[0], f'p_voice for {ASH}: 6 → 10.')
        self.fact('conversation', ASH, 'rm111')
        self.tick()
        self.assertEqual(self.kept(), 13)
        self.assertIn(sorted(self.realm_of(r)[ASH]['p_voice'] for r in (a, b)), ([11, 13], [12, 13]),
                      "the first run to take its turn read the console's 10, then added its own")
        # A third story launched now has each wolf's value from the start.
        c = self.launch('realm-c')
        self.assertEqual(self.realm_of(c), {ASH: {'p_voice': 13}})

    def keep(self, wolf, value, hours=0):
        self.conn.execute(
            "INSERT INTO dm.story_realm_vars(world_id,name,wolf,type,value,updated_at) VALUES (%s,'p_voice',%s,'number',%s::jsonb,"
            "now() - make_interval(hours => %s))", (self.world, wolf, json.dumps(value), hours))

    def test_a_wolf_a_command_takes_in_has_their_kept_value_read_then(self):
        # (After review: a wolf joining from the console, here by a trigger fired by hand that takes them in, was never
        # read, as only a tick's turn read a joining wolf; the run's cursor then never reached their older row, and its
        # next write lost what they kept.)
        self.keep(ASH, 5)
        self.keep(BIRCH, 10, hours=1)
        self.launch('realm-bring')
        self.answer()
        self.assertEqual(self.realm_of(self.run_id), {ASH: {'p_voice': 5}})
        status, result = self.steer('fire', {'trigger': 'bring', 'actor': ASH, 'check': False})
        self.assertEqual(status, 'done', result)
        self.assertEqual(self.realm_of(self.run_id), {ASH: {'p_voice': 5}, BIRCH: {'p_voice': 10}}, 'read as they join')
        self.answer_all()
        self.fact('conversation', BIRCH, 'rm111')
        self.tick()
        self.assertEqual(self.kept(BIRCH), 11, 'what Birch kept, added to; never started again from nothing')
        self.assertEqual(self.kept(ASH), 5)

    def test_a_wolf_who_joined_unread_is_read_whole_at_the_next_turn(self):
        # (The safety net: a cohort wolf with nothing loaded is read whatever the cursor says.)
        self.keep(ASH, 5)
        self.keep(BIRCH, 10, hours=1)
        self.launch('realm-bring')
        self.answer()
        state = self.conn.execute('SELECT state FROM dm.story_releases WHERE id=%s', (self.run_id,)).fetchone()[0]
        state['cohort'] = sorted(state['cohort'] + [BIRCH])
        runner.world_load(self.conn, self.world, bring_program(), state)
        self.assertEqual(state['counters']['realm'], {ASH: {'p_voice': 5}, BIRCH: {'p_voice': 10}})

    def test_a_type_clash_is_refused(self):
        self.launch('realm-a')
        self.answer()
        self.fact('conversation', ASH, 'rm111')
        self.tick()
        other = realm_program('realm-switch', kind='switch')
        self.assertRaisesRegex(ValueError, r'The wolves keep p_voice as a number \(realm\)', runner.approve, self.conn, self.world, 'martin',
                               other, rules.digest(other))
        self.conn.execute("DELETE FROM dm.story_realm_vars WHERE world_id=%s", (self.world,))
        self.assertRaisesRegex(ValueError, r'"Voices of realm-a" keeps p_voice as a number', runner.approve, self.conn, self.world, 'martin',
                               other, rules.digest(other))
        # (A value kept under another type is not read: the story starts from its first value.)
        self.conn.execute("""INSERT INTO dm.story_realm_vars(world_id,name,wolf,type,value) VALUES (%s,'p_loud',%s,'text','"yes"')""",
                          (self.world, ASH))
        p = realm_program('realm-loud')
        p['variables'][0]['id'] = 'p_loud'
        p['phases'][0]['triggers'][0]['actions'][0]['var'] = 'p_loud'
        state = rules.begin(p, sorted(WOLVES), 0)
        runner.world_load(self.conn, self.world, p, state)
        self.assertEqual(state.get('counters', {}).get('realm', {}), {})


if __name__ == '__main__':
    unittest.main()


def cast_program():
    """Doc 86, C7: a story that casts a carter of Larkfield for the wolf who comes, the quest's giver whoever was cast."""
    return {
        'format': 'ratw-story-program', 'version': 2, 'id': 'cast-carter', 'title': 'The carter wanted',
        'audience': {'kind': 'players', 'players': sorted(WOLVES)}, 'start': 'open',
        'variables': [{'id': 'carter', 'type': 'resident', 'scope': 'story'}, {'id': 'tried', 'type': 'residents', 'scope': 'story'},
                      {'id': 'missed', 'type': 'number', 'scope': 'story'}],
        'phases': [{'id': 'open', 'journal': 'A carter is wanted.', 'effects': [], 'exits': [], 'triggers': [
            {'id': 'cast', 'on': 'conversation', 'audience': 'participant', 'actions': [
                {'do': 'effect', 'effect': {'kind': 'cast.pick', 'var': 'carter', 'job': 'carter', 'town': 'larkfield', 'notIn': 'tried',
                                            'near': '$actor'}}]},
            {'id': 'found', 'on': 'cast chosen', 'actions': [{'do': 'list add', 'var': 'tried', 'value': {'field': 'target'}}]},
            {'id': 'none', 'on': 'cast failed', 'actions': [{'do': 'add', 'var': 'missed', 'value': 1}]}]}],
        'quests': [{'id': 'axle', 'title': 'An axle', 'giver': {'var': 'carter'},
                    'objectives': [{'id': 'bring', 'label': 'Bring an axle.', 'on': 'gift', 'match': {'item': 'axle'}}]}],
        'tests': [{'name': 'A carter', 'cohort': sorted(WOLVES), 'cast': {'carter': ['rm111']},
                   'events': [{'id': 'a', 'kind': 'conversation', 'day': 1, 'actor': ASH, 'target': 'rm112'}], 'expect': ['open'],
                   'expectVariables': {'story': {'carter': 'rm111', 'tried': ['rm111']}}}],
    }


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class Casting(unittest.TestCase):
    """Doc 86, C7: the runner's casting from the world's residents (living, placed, of the work and the town asked, never one
    left out, the nearest the wolf first), answered in the same turn; the quest's offer at the resident cast."""
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    fact, launch, answer, tick = console.ConsoleReads.fact, console.ConsoleReads.launch, console.ConsoleReads.answer, console.ConsoleReads.tick

    def setUp(self):
        console.ConsoleReads.setUp(self)
        self.examples = {**self.examples, 'cast-carter': cast_program()}
        c = self.conn
        c.execute('RESET ROLE')
        c.execute("INSERT INTO world.areas(world_id,id,kind) VALUES (%s,'lark-yard','cell'),(%s,'lark-row','cell'),(%s,'cold-yard','cell')",
                  (self.world,) * 3)
        c.execute("""INSERT INTO world.cells(world_id,id,name,x,y,width,height,region) VALUES
                     (%s,'lark-yard','The yard',0,0,30,30,'larkfield'),(%s,'lark-row','The row',40,0,30,30,'larkfield'),
                     (%s,'cold-yard','The cold yard',90,0,30,30,'coldwater')""", (self.world,) * 3)
        people = [('cst_near', 'carter at the yard', 'lark-yard', 'lark-yard', 4, 4), ('cst_far', 'carter on the row', 'lark-row', 'lark-row', 20, 20),
                  ('cst_cold', 'carter at Coldwater', 'cold-yard', 'cold-yard', 4, 4), ('cst_baker', 'baker', 'lark-yard', 'lark-yard', 5, 5),
                  ('cst_dead', 'carter, once', 'lark-yard', 'lark-yard', 4, 5), ('cst_away', 'carter of the Country', None, None, 0, 0)]
        for npc, label, home, work, x, y in people:
            c.execute("""INSERT INTO live.npcs(world_id,id,name,role,age,voice,appearance,hours_start,hours_end,work_label,home_area,home_x,home_y,
                         work_area,work_x,work_y,evening_x,evening_y) VALUES (%s,%s,%s,'civilian',30,1,'{}',8,12,%s,%s,%s,%s,%s,%s,%s,0,0)""",
                      (self.world, npc, npc, label, home, x, y, work, x, y))
        c.execute("INSERT INTO live.npc_state(world_id,npc_id,alive,state) VALUES (%s,'cst_dead',false,'{}')", (self.world,))
        c.execute('SET LOCAL ROLE ratw_dm')

    def pick(self, **request):
        return runner.cast_pick(self.conn, self.world, {'var': 'carter', 'seed': 's', **request})

    def test_the_query(self):
        self.assertIn(self.pick(job='carter', town='larkfield'), ('cst_near', 'cst_far'), 'a living, placed carter of Larkfield')
        self.assertEqual(self.pick(job='carter', town='larkfield', at={'cell': 'lark-row', 'x': 19, 'y': 19, 'town': 'larkfield'}), 'cst_far',
                         'the nearest the wolf: at their work in the wolf\'s place')
        self.assertEqual(self.pick(job='carter', town='larkfield', at={'cell': 'lark-yard', 'x': 3, 'y': 3, 'town': 'larkfield'}), 'cst_near')
        self.assertEqual(self.pick(job='carter', town='larkfield', notIn=['cst_near']), 'cst_far', 'never one left out')
        self.assertIsNone(self.pick(job='carter', town='larkfield', notIn=['cst_near', 'cst_far']),
                          'the dead and the unplaced are never cast')
        self.assertEqual(self.pick(job='carter', town='coldwater'), 'cst_cold')
        self.assertEqual(self.pick(job='baker'), 'cst_baker')
        self.assertIsNone(self.pick(job='carter', faction='house_greatly'), 'no House claims their work')
        self.assertEqual(self.pick(job='carter', town='larkfield'), self.pick(job='carter', town='larkfield'), 'the same seed, the same')

    def test_a_run_casts_in_its_turn_and_offers_the_quest_there(self):
        run = self.launch('cast-carter')
        self.answer()
        self.fact('conversation', ASH, 'rm112')
        self.tick()
        state = self.conn.execute('SELECT state FROM dm.story_releases WHERE id=%s', (run,)).fetchone()[0]
        self.assertIn(state['counters']['story']['carter'], ('cst_near', 'cst_far'))
        self.assertEqual(state['counters']['story']['tried'], [state['counters']['story']['carter']], "'cast chosen' fired in the same turn")
        self.assertNotIn('castRequests', state)
        orders = runner.status(self.conn, self.world, run)['orders']
        quest_orders = [o for o in orders if o.get('kind') == 'story.quests']
        self.assertTrue(quest_orders, orders)
        payload = self.conn.execute('SELECT payload FROM dm.actions WHERE id=%s', (quest_orders[0]['id'],)).fetchone()[0]
        payload = json.loads(payload) if isinstance(payload, str) else payload
        self.assertEqual([o['npc'] for o in payload['offers'] if o['quest'] == 'axle'], [state['counters']['story']['carter']],
                         'the offer goes to the resident cast')
        for _ in range(2):
            self.answer()
            self.fact('conversation', ASH, 'rm112', day=11)
            self.tick()
        state = self.conn.execute('SELECT state FROM dm.story_releases WHERE id=%s', (run,)).fetchone()[0]
        self.assertEqual(sorted(state['counters']['story']['tried']), ['cst_far', 'cst_near'])
        self.assertEqual(state['counters']['story'].get('missed'), 1, 'a third time: nobody left to cast')
