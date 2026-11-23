"""Chapters, campaigns and the world's variables (Docs/Design/85-story-triggers-and-campaign-control.md, 3.5 "story.start",
3.2 and Phase 10): story.start checked and rehearsed; a story that starts another refused when approved alone; a campaign
approved together (each chapter rehearsed and checked; the chapters it starts must be its own); in a throwaway, migrated
database: the first chapter published, driven to its end, and the next starting once the game has the first one's end
(the same wolves, its values carried, a third story told "story started" and "story ended" through its inbox), a chapter
started "now" beside the one that started it, one already running not started twice, the campaign ended with its last
chapter; the world's variables declared, set and removed from the console (audited), read by a chapter, a story declaring
one as another type refused; the hosted permissions (a builder for the economy, the owner or "may steer campaigns" for
the world's variables); and the watch lists and checks of the events Phase 8 left. Skipped without the local PostgreSQL."""

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
import story_runner as runner
import test_story_console as console
import test_story_steering as steering
import world_db as W
from test_story_console import WOLVES
from test_world_db import database_available

ASH, BIRCH, ELM = sorted(WOLVES)
fx = lambda kind, **k: {'do': 'effect', 'effect': {'kind': kind, **k}}


def muster(when='ended', audience='same'):
    """Chapter one: two words with Ivo, then the crossing starts (when this one has ended, by default) and it finishes."""
    start = {'story': 'the-crossing', 'audience': audience, 'carry': True, 'when': when}
    if audience == 'list':
        start['list'] = 'mustered'
    return {
        'format': 'ratw-story-program', 'version': 2, 'id': 'the-muster', 'title': 'The muster at the ford',
        'audience': {'kind': 'players', 'players': sorted(WOLVES)}, 'start': 'gather',
        'variables': [{'id': 'ropes', 'type': 'number', 'scope': 'story', 'show': 'Ropes'},
                      {'id': 'helped', 'type': 'switch', 'scope': 'player'},
                      {'id': 'mustered', 'type': 'wolves', 'scope': 'story'},
                      {'id': 'only-here', 'type': 'text', 'scope': 'story', 'initial': 'kept here'}],
        'phases': [{'id': 'gather', 'journal': 'Bring Ivo two coils of rope.', 'effects': [], 'exits': [], 'triggers': [
            {'id': 'coil', 'on': 'conversation', 'audience': 'participant', 'match': {'target': 'rm111'}, 'actions': [
                {'do': 'add', 'var': 'ropes', 'value': 1}, {'do': 'turn on', 'var': 'helped'},
                {'do': 'list add', 'var': 'mustered', 'value': {'field': 'actor'}},
                {'do': 'if', 'if': {'test': 'compare', 'left': {'var': 'ropes'}, 'op': '>=', 'right': 2},
                 'then': [fx('story.start', **start), {'do': 'finish', 'outcome': 'success', 'text': 'The muster is done.'}]}]}]}],
        'tests': [{'name': 'Two coils', 'cohort': sorted(WOLVES), 'events': [
            {'id': 'a', 'kind': 'conversation', 'day': 1, 'actor': ASH, 'target': 'rm111'},
            {'id': 'b', 'kind': 'conversation', 'day': 1.1, 'actor': BIRCH, 'target': 'rm111'}],
            'expect': ['gather'], 'expectOutcome': 'success', 'expectChapters': ['the-crossing']}],
    }


CROSSING = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'the-crossing', 'title': 'The crossing',
    'audience': {'kind': 'players', 'players': [ELM]}, 'start': 'cross',
    'variables': [{'id': 'ropes', 'type': 'number', 'scope': 'story'}, {'id': 'helped', 'type': 'switch', 'scope': 'player'},
                  {'id': 'only-here', 'type': 'number', 'scope': 'story'}, {'id': 'best', 'type': 'number', 'scope': 'world'}],
    'phases': [{'id': 'cross', 'journal': 'Cross with the ropes Ivo was given.', 'effects': [], 'exits': [], 'triggers': [
        {'id': 'over', 'on': 'conversation', 'match': {'target': 'rm112'}, 'actions': [
            {'do': 'set', 'var': 'best', 'value': {'plus': [{'var': 'best'}, {'var': 'ropes'}]}},
            {'do': 'finish', 'outcome': 'success', 'text': 'Across.'}]}]}],
    'tests': [{'name': 'Across', 'cohort': [ELM], 'events': [{'id': 'a', 'kind': 'conversation', 'day': 1, 'actor': ELM, 'target': 'rm112'}],
               'expect': ['cross'], 'expectOutcome': 'success'}],
}

WATCH = {
    'format': 'ratw-story-program', 'version': 2, 'id': 'the-watch', 'title': 'The watch on the ford',
    'audience': {'kind': 'players', 'players': [ELM]}, 'start': 'watch',
    'variables': [{'id': 'heard', 'type': 'texts', 'scope': 'story'}],
    'phases': [{'id': 'watch', 'journal': 'Keep watch.', 'effects': [], 'exits': [], 'triggers': [
        {'id': 'began', 'on': 'story started', 'actions': [{'do': 'list add', 'var': 'heard', 'value': {'field': 'target'}}]},
        {'id': 'ended', 'on': 'story ended', 'match': {'target': 'the-muster'},
         'actions': [{'do': 'note', 'text': 'The muster is over.'}, {'do': 'list add', 'var': 'heard', 'value': {'field': 'detail'}}]}]}],
    'tests': [{'name': 'Heard', 'cohort': [ELM], 'events': [
        {'id': 'a', 'kind': 'story ended', 'day': 1, 'target': 'the-muster', 'detail': 'success'}], 'expect': ['watch']}],
}


def chapters(*programs):
    return [{'program': p, 'digest': rules.digest(p)} for p in programs]


class Chapters(unittest.TestCase):
    def test_story_start_is_checked_and_rehearsed(self):
        report = rules.review(muster())
        self.assertTrue(report['passed'], report)
        self.assertEqual(report['tests'][0]['chapters'], [{'story': 'the-crossing', 'audience': 'same', 'when': 'ended'}])
        bad = muster()
        bad['tests'][0]['expectChapters'] = ['another']
        self.assertFalse(rules.review(bad)['passed'])
        self.assertTrue(rules.review(muster(audience='list'))['passed'])
        p = muster(audience='list')
        p['phases'][0]['triggers'][0]['actions'][3]['then'][0]['effect']['list'] = 'ropes'
        self.assertRaisesRegex(ValueError, 'declared list of wolves', rules.validate, p)
        p = muster()
        p['phases'][0]['triggers'][0]['actions'][3]['then'][0]['effect']['list'] = 'mustered'
        self.assertRaisesRegex(ValueError, 'names the list, and only then', rules.validate, p)
        p = muster()
        p['version'] = 1
        self.assertRaises(ValueError, rules.validate, p)
        self.assertEqual(runner.chapter_starts(muster()), ['the-crossing'])

    def test_a_chapter_keeps_its_values_for_the_next(self):
        state = rules.begin(muster(), sorted(WOLVES), 0)
        state['counters'] = {'story': {'ropes': 2, 'only-here': 'x', 'mustered': [ASH]}, 'player': {ASH: {'helped': True}}}
        carried = runner._carried(muster(), CROSSING, state)
        self.assertEqual(carried, {'story': {'ropes': 2}, 'player': {ASH: {'helped': True}}},
                         'the same name, scope and type only (only-here is a number there)')
        nxt = rules.begin(CROSSING, [ASH, BIRCH], 0)
        runner._carry_into(CROSSING, nxt, carried)
        self.assertEqual(nxt['counters'], {'story': {'ropes': 2}, 'player': {ASH: {'helped': True}}})

    def test_the_new_events_are_watched_and_need_their_names(self):
        p = json.loads(json.dumps(WATCH))
        p['phases'][0]['triggers'] = [
            {'id': 'near', 'on': 'near resident', 'match': {'target': 'rm111'}, 'actions': [{'do': 'note', 'text': 'near'}]},
            {'id': 'purse', 'on': 'purse crossed', 'match': {'target': 'town:greyfen', 'detail': '500'}, 'actions': [{'do': 'note', 'text': 'rich'}]},
            {'id': 'stand', 'on': 'standing crossed', 'match': {'target': 'watch', 'detail': '-20'}, 'actions': [{'do': 'note', 'text': 'low'}]},
            {'id': 'stance', 'on': 'relation changed', 'actions': [{'do': 'note', 'text': 'stance'}]},
            {'id': 'mantle', 'on': 'mantle bound', 'actions': [{'do': 'note', 'text': 'mantle'}]},
            {'id': 'skill', 'on': 'skill practised', 'actions': [{'do': 'note', 'text': 'skill'}]},
            {'id': 'heal', 'on': 'injury healed', 'actions': [{'do': 'note', 'text': 'healed'}]}]
        p['tests'] = [{'name': 'Nothing', 'cohort': [ELM], 'events': [], 'expect': ['watch']}]
        rules.validate(p)
        watch = rules.watch_of(p)
        self.assertEqual(watch['near'], ['rm111'])
        self.assertEqual(watch['purses'], [['town:greyfen', 500]])
        self.assertEqual(watch['standing'], [['watch', -20]])
        self.assertEqual(watch['events'], ['injury healed', 'mantle bound', 'near resident', 'purse crossed', 'relation changed',
                                           'skill practised', 'standing crossed'])
        for trigger, why in ((0, 'needs its what it is about'), (1, 'number'), (2, 'number')):
            q = json.loads(json.dumps(p))
            q['phases'][0]['triggers'][trigger]['match'].pop('detail' if trigger else 'target')
            self.assertRaisesRegex(ValueError, why, rules.validate, q)
        q = json.loads(json.dumps(p))
        q['phases'][0]['triggers'][1]['match']['detail'] = 'lots'
        self.assertRaisesRegex(ValueError, 'whole number', rules.validate, q)
        self.assertEqual(rules.watch_of(WATCH), {}, '"story started" and "story ended" are the runner\'s: nothing for the game')


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class Campaigns(unittest.TestCase):
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    fact, answer, tick, rewrite = console.ConsoleReads.fact, console.ConsoleReads.answer, console.ConsoleReads.tick, console.ConsoleReads.rewrite
    answer_all, settle = steering.Steering.answer_all, steering.Steering.settle

    def setUp(self):
        console.ConsoleReads.setUp(self)

    def release(self, run):
        return self.conn.execute('SELECT status,state,campaign,chapter,program->>\'id\' FROM dm.story_releases WHERE id=%s', (run,)).fetchone()

    def runs_of(self, story):
        return [r[0] for r in self.conn.execute("SELECT id FROM dm.story_releases WHERE program->>'id'=%s AND status<>'approved' ORDER BY approved_at",
                                                (story,))]

    def approve(self, *programs, cid='ford'):
        return runner.approve_campaign(self.conn, self.world, 'martin', {'campaign': {'id': cid, 'title': 'The ford', 'chapters': chapters(*programs)}})

    def lone(self, p):
        ticket = runner.approve(self.conn, self.world, 'martin', p, rules.digest(p))['ticket']
        return runner.publish(self.conn, self.world, ticket)['run']

    def test_a_story_that_starts_another_is_approved_with_its_campaign(self):
        self.assertRaisesRegex(ValueError, 'approve it as a chapter of a campaign', runner.approve, self.conn, self.world, 'martin',
                               muster(), rules.digest(muster()))
        self.assertRaisesRegex(ValueError, 'not a chapter of this campaign', self.approve, muster())
        loop = json.loads(json.dumps(muster()))
        loop['phases'][0]['triggers'][0]['actions'][3]['then'][0]['effect']['story'] = 'the-muster'
        loop['tests'][0]['expectChapters'] = ['the-muster']
        self.assertRaisesRegex(ValueError, 'cannot start itself', self.approve, loop)
        bad = chapters(muster(), CROSSING)
        bad[1]['digest'] = 'nope'
        self.assertRaisesRegex(ValueError, 'Chapter 2: rehearse', runner.approve_campaign, self.conn, self.world, 'martin',
                               {'campaign': {'id': 'ford', 'title': 'The ford', 'chapters': bad}})
        first = self.approve(muster(), CROSSING)
        out = self.approve(muster(), CROSSING)
        self.assertEqual([c['story'] for c in out['chapters']], ['the-muster', 'the-crossing'])
        self.assertTrue(out['ticket'])
        self.assertRaisesRegex(ValueError, 'Unknown ticket', runner.publish, self.conn, self.world, first['ticket'])   # (approved again: the old ticket gives way)
        listed = runner.campaigns(self.conn, self.world)
        self.assertEqual([(c['id'], c['status'], [ch['story'] for ch in c['chapters']]) for c in listed],
                         [('ford', 'approved', ['the-muster', 'the-crossing'])])
        self.assertEqual(listed[0]['chapters'][0]['starts'], ['the-crossing'])

    def test_the_next_chapter_starts_when_the_first_has_ended_with_its_wolves_and_values(self):
        watch = self.lone(WATCH)
        self.settle()
        out = self.approve(muster(), CROSSING)
        first = runner.publish(self.conn, self.world, out['ticket'])['run']
        self.assertEqual(self.release(first)[2:4], ('ford', 0))
        self.settle()
        self.fact('conversation', ASH, 'rm111')
        self.tick()
        self.fact('conversation', BIRCH, 'rm111')
        self.tick()
        state = self.release(first)[1]
        self.assertEqual([s['story'] for s in state['chapterStarts']], ['the-crossing'], 'it waits for this one to end')
        self.assertEqual(self.runs_of('the-crossing'), [])
        self.settle()
        status, state = self.release(first)[:2]
        self.assertEqual(status, 'complete')
        [second] = self.runs_of('the-crossing')
        st, s2, cid, n, _ = self.release(second)
        self.assertEqual((st, cid, n), ('running', 'ford', 1))
        self.assertEqual(s2['cohort'], sorted(WOLVES), "the same wolves as the muster's, not its own audience")
        self.assertEqual(s2['counters']['story'], {'ropes': 2}, 'its ropes carried')
        self.assertEqual(s2['counters']['player'], {ASH: {'helped': True}, BIRCH: {'helped': True}})
        self.assertEqual(s2['chapterOf'], {'campaign': 'ford', 'from': first, 'story': 'the-muster', 'chapter': 1})
        self.assertEqual(state['chaptersStarted'], [{'story': 'the-crossing', 'run': second}])
        history = [e[0] for e in self.conn.execute('SELECT entry FROM dm.story_trigger_history WHERE run=%s ORDER BY id', (first,))]
        self.assertTrue(any(n.startswith('Started the chapter "The crossing"') for h in history for n in h.get('notes', [])), history)
        cause = self.conn.execute('SELECT cause FROM dm.story_transitions WHERE run=%s', (second,)).fetchone()[0]
        self.assertEqual(cause, f'started by {first}')
        # The watch heard both, through its inbox: the muster's end, and the crossing begun.
        self.settle()
        heard = self.release(watch)[1]['counters']['story']['heard']
        self.assertEqual(sorted(heard), sorted(['the-muster', 'the-crossing', 'success']))
        events = self.conn.execute("SELECT requested_by,status FROM dm.story_commands WHERE run=%s AND kind='story-event'", (watch,)).fetchall()
        self.assertEqual({e for e in events}, {('story-runner', 'done')})
        self.assertFalse(self.conn.execute("SELECT 1 FROM dm.audit WHERE action='story.story-event'").fetchone(), 'never audited')
        one = runner.status(self.conn, self.world, second)
        self.assertEqual(one['campaign']['id'], 'ford')
        self.assertEqual(one['campaign']['chapter'], 1)
        self.assertEqual([len(ch['runs']) for ch in one['campaign']['chapters']], [1, 1])
        rows = {r['run']: r for r in runner.status(self.conn, self.world)['runs']}
        self.assertEqual((rows[second]['campaign'], rows[second]['chapter']), ('ford', 1))
        # The last chapter done: the campaign has ended.
        self.fact('conversation', ELM, 'rm112')
        self.settle()
        self.assertEqual(self.release(second)[0], 'complete')
        self.assertEqual(runner.campaigns(self.conn, self.world)[0]['status'], 'ended')
        best = self.conn.execute("SELECT value FROM dm.story_world_vars WHERE world_id=%s AND name='best'", (self.world,)).fetchone()[0]
        self.assertEqual(best, 2, 'the carried ropes, kept for the world')
        # (Doc 86, C11: the campaign's holds on residents let go, once.)
        self.settle()
        ends = self.conn.execute("SELECT count(*) FROM dm.actions WHERE kind='story.campaign_end' AND target_id='ford'").fetchone()[0]
        self.assertEqual(ends, 1, 'story.campaign_end is sent once')

    def test_a_new_version_of_a_running_chapter_keeps_its_holds_for_the_campaign(self):
        # (Doc 86, B3: a chapter that spares a resident for the campaign may be moved to a new version while it runs.)
        crossing = copy.deepcopy(CROSSING)
        crossing['phases'][0]['effects'] = [{'kind': 'npc.spare', 'npc': 'rm111', 'lasts': 'campaign'}]
        out = self.approve(muster(), crossing)
        first = runner.publish(self.conn, self.world, out['ticket'])['run']
        self.settle()
        self.fact('conversation', ASH, 'rm111')
        self.tick()
        self.fact('conversation', BIRCH, 'rm111')
        self.tick()
        self.settle()
        [second] = self.runs_of('the-crossing')
        newer = copy.deepcopy(crossing)
        newer['phases'][0]['journal'] = 'Cross quickly with the ropes Ivo was given.'
        moved = runner.approve(self.conn, self.world, 'martin', newer, rules.digest(newer), for_run=second)
        self.assertEqual((moved['run'], moved['warnings']), (second, []))
        self.assertRaisesRegex(ValueError, 'chapter of a campaign', runner.approve, self.conn, self.world, 'martin', newer,
                               rules.digest(newer))   # (Alone, it is no chapter.)

    def test_an_act_stopped_early_lets_go_of_its_holds(self):
        # (Doc 86, C11: the console's "Let go of the campaign's holds".)
        self.approve(muster(), CROSSING)
        out = runner.release_campaign(self.conn, self.world, 'martin', 'ford')
        kind, target = self.conn.execute('SELECT kind,target_id FROM dm.actions WHERE id=%s', (out['receipt'],)).fetchone()
        self.assertEqual((kind, target), ('story.campaign_end', 'ford'))
        self.assertRaisesRegex(ValueError, 'No such campaign', runner.release_campaign, self.conn, self.world, 'martin', 'nowhere')

    def test_a_chapter_started_now_runs_beside_the_one_that_started_it_and_never_twice(self):
        out = self.approve(muster(when='now', audience='list'), CROSSING)
        first = runner.publish(self.conn, self.world, out['ticket'])['run']
        self.settle()
        self.fact('conversation', ASH, 'rm111')
        self.fact('conversation', ASH, 'rm111')
        self.tick()
        [second] = self.runs_of('the-crossing')
        self.assertEqual(self.release(second)[1]['cohort'], [ASH], 'the wolves in the list')
        self.assertIn(self.release(first)[0], ('running', 'complete'))
        # The muster again (a fresh run of chapter one, approved again), starting the crossing while it runs: not twice.
        self.settle()
        out = self.approve(muster(when='now', audience='list'), CROSSING)
        self.assertNotIn('ticket', out, 'chapter one has started: no new ticket')
        state = rules.begin(muster(), sorted(WOLVES), 10, first)
        self.assertIsNone(runner.start_chapter(self.conn, self.world, first, muster(), state,
                                               {'story': 'the-crossing', 'audience': 'same', 'when': 'now'}))
        self.assertIn('already running', state['chaptersStarted'][0]['refused'])

    def test_a_trigger_fired_by_hand_keeps_its_story_effects(self):
        # (The console's Fire…: story.start is the story's own, never an order to the game; the chapter starts at the end.)
        out = self.approve(muster(), CROSSING)
        first = runner.publish(self.conn, self.world, out['ticket'])['run']
        self.settle()
        self.fact('conversation', ASH, 'rm111')
        self.tick()
        state = self.release(first)[1]
        sent = runner.command(self.conn, self.world, 'martin', {'run': first, 'kind': 'fire', 'payload': {'trigger': 'coil', 'actor': BIRCH},
                                                                'reason': 'Birch brought the second coil', 'key': secrets.token_hex(8),
                                                                'visit': state['visits'], 'digest': rules.digest(muster())})
        self.tick()
        status, result = self.conn.execute('SELECT status,result FROM dm.story_commands WHERE id=%s', (sent['id'],)).fetchone()
        self.assertEqual(status, 'done', result)
        self.assertEqual([s['story'] for s in self.release(first)[1].get('chapterStarts', [])], ['the-crossing'])
        self.settle()
        self.assertEqual(self.release(first)[0], 'complete')
        self.assertEqual(len(self.runs_of('the-crossing')), 1)
        kinds = [k for (k,) in self.conn.execute("SELECT payload->'items' FROM dm.actions WHERE kind='story.effects'")]
        self.assertFalse(any('story.start' in json.dumps(k) for k in kinds), 'never sent to the game')

    def test_a_stopped_chapter_starts_nothing(self):
        out = self.approve(muster(), CROSSING)
        first = runner.publish(self.conn, self.world, out['ticket'])['run']
        self.settle()
        self.fact('conversation', ASH, 'rm111')
        self.tick()
        state = self.release(first)[1]
        state['chapterStarts'] = [{'story': 'the-crossing', 'audience': 'same', 'when': 'ended', 'carry': True}]
        self.conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), first))
        sent = runner.command(self.conn, self.world, 'martin', {'run': first, 'kind': 'stop', 'payload': {}, 'reason': 'not today then',
                                                                'key': secrets.token_hex(8), 'visit': state['visits'],
                                                                'digest': rules.digest(muster())})
        self.assertTrue(sent)
        self.settle()
        self.assertEqual(self.release(first)[0], 'stopped')
        self.assertEqual(self.runs_of('the-crossing'), [])

    def test_the_worlds_variables_declared_set_and_removed_with_their_audit(self):
        do = lambda **d: runner.world_var(self.conn, self.world, 'martin', {'reason': 'the DM is testing this', **d})
        out = do(do='declare', name='best', type='number', value=5, note='the most ropes across')
        self.assertEqual(out['after'], {'type': 'number', 'value': 5})
        [v] = [v for v in runner.world_vars(self.conn, self.world)['variables'] if v['name'] == 'best']
        self.assertEqual((v['value'], v['declared'], v['note'], v['declaredBy']), (5, True, 'the most ropes across', 'martin'))
        self.assertRaisesRegex(ValueError, 'Not a value for number', do, do='set', name='best', value='lots')
        out = do(do='set', name='best', value=7)
        self.assertEqual((out['before'], out['after']), ({'value': 5}, {'value': 7}))
        self.assertRaisesRegex(ValueError, 'not declared', do, do='set', name='worst', value=1)
        # A story declaring it reads the world's value; one declaring it as another type is refused at approval.
        state = rules.begin(CROSSING, [ELM], 0)
        runner.world_load(self.conn, self.world, CROSSING, state)
        self.assertEqual(state['counters']['world'], {'best': 7})
        other = json.loads(json.dumps(CROSSING))
        other['variables'][3]['type'] = 'text'
        other['phases'][0]['triggers'][0]['actions'][0] = {'do': 'set', 'var': 'best', 'value': 'many'}
        self.assertRaisesRegex(ValueError, 'The world declares best as number', runner.approve, self.conn, self.world, 'martin',
                               other, rules.digest(other))
        # Declared again as another type while a story reads it as a number: refused; removed while one runs: refused.
        out = self.approve(muster(), CROSSING)
        self.assertEqual(out['chapters'][1]['story'], 'the-crossing')
        self.assertRaisesRegex(ValueError, 'declares best as number', do, do='declare', name='best', type='text')
        audit = runner.world_var_audit(self.conn, self.world)
        self.assertEqual([a['detail'].split(' Reason')[0] for a in audit][:2], ['best: 5 → 7.', 'Declared best (number), first value 5.'])
        self.assertTrue(all(a['who'] == 'martin' for a in audit))
        self.assertEqual(do(do='remove', name='best')['after'], None)


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class HostedCampaigns(unittest.TestCase):
    """The route on a hosted world: a campaign whose chapters change the economy needs a builder; the world's variables
    need the owner or "may steer campaigns"."""

    @classmethod
    def setUpClass(cls):
        console.ConsoleReads.setUpClass.__func__(cls)
        dm = D.DungeonMaster(connect=lambda target: W.connect('dev', 'dm', dbname=cls.name), worlds=lambda: ['prod', 'dev'])
        hosted = D.make_handler(dm, worlds=lambda: ['dev'], default=lambda: 'dev', hosted=True)
        sessions = {'dm': {'username': 'wren#b2', 'role': 'dm'}, 'builder': {'username': 'tam#c4', 'role': 'builder'},
                    'steerer': {'username': 'ash#d1', 'role': 'dm', 'maySteerCampaigns': True},
                    'owner': {'username': 'owner#a1', 'role': 'owner'}}

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
        request = urllib.request.Request(f'http://127.0.0.1:{self.server.server_port}/api/story-program', json.dumps({'target': 'dev', **body}).encode(),
                                         method='POST', headers={'Content-Type': 'application/json', 'X-Session': session})
        try:
            with urllib.request.urlopen(request, timeout=10) as reply:
                return reply.status, json.loads(reply.read())
        except urllib.error.HTTPError as error:
            return error.code, json.loads(error.read())

    def test_a_campaign_that_changes_the_economy_needs_a_builder(self):
        p = json.loads(json.dumps(CROSSING))
        p['phases'][0]['triggers'][0]['actions'].insert(0, fx('prices.reset', market='all', words='the ford is open'))
        body = {'op': 'campaign.approve', 'campaign': {'id': 'econ', 'title': 'Prices', 'chapters': chapters(muster(), p)}}
        status, data = self.call(body, 'dm')
        self.assertEqual(status, 403, data)
        self.assertIn('the economy', data['error'])
        status, data = self.call(body, 'builder')
        self.assertEqual(status, 200, data)
        self.assertEqual([c['story'] for c in data['chapters']], ['the-muster', 'the-crossing'])
        status, data = self.call({'op': 'campaigns'}, 'dm')
        self.assertEqual((status, [c['id'] for c in data['campaigns']]), (200, ['econ']))

    def test_the_worlds_variables_need_the_owner_or_may_steer(self):
        body = {'op': 'world-var', 'do': 'declare', 'name': 'bells', 'type': 'switch', 'value': False, 'reason': 'for the night story'}
        status, data = self.call(body, 'dm')
        self.assertEqual(status, 403, data)
        self.assertIn('may steer campaigns', data['error'])
        for who in ('steerer', 'owner'):
            status, data = self.call({**body, 'value': who == 'owner'}, who)
            self.assertEqual(status, 200, data)
        status, data = self.call({'op': 'world-vars'}, 'dm')
        self.assertEqual(status, 200, data)
        [v] = [v for v in data['variables'] if v['name'] == 'bells']
        self.assertEqual(v['type'], 'switch')
        self.assertEqual([a['who'] for a in data['audit']][:2], ['owner#a1', 'ash#d1'])


def ready_chapters():
    """Doc 86, Phase 1a: chapter one paced by readiness (Advance by an admin), whose closing phase starts chapter two with the
    same wolves; chapter two moves on by itself at the next day once ready."""
    one = console.ready_program(story_id='nh-ch1')
    one['phases'][1] = {'id': 'done', 'journal': 'The muster is done: on to the ford.', 'effects': [], 'exits': [], 'triggers': [
        {'id': 'next', 'on': 'clock', 'afterDays': 0.01, 'once': 'visit', 'actions': [
            fx('story.start', story='nh-ch2', audience='same', carry=True, when='ended'),
            {'do': 'finish', 'outcome': 'success', 'text': 'The muster is over.'}]}]}
    one['tests'][0]['events'].append({'id': 'late', 'kind': 'clock', 'day': 3})
    one['tests'][0].update(expectOutcome='success', expectChapters=['nh-ch2'])
    two = console.ready_program(auto=True, then={'on': 'day begins'}, story_id='nh-ch2')
    return one, two


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class ReadyChapters(console.ReadyFixture, unittest.TestCase):
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    setUp = console.ConsoleReads.setUp
    fact, launch, answer, tick, rewrite, row = (console.ConsoleReads.fact, console.ConsoleReads.launch, console.ConsoleReads.answer,
                                                console.ConsoleReads.tick, console.ConsoleReads.rewrite, console.ConsoleReads.row)
    one, send, taken, steer, answer_all, settle = (steering.Steering.one, steering.Steering.send, steering.Steering.taken,
                                                   steering.Steering.steer, steering.Steering.answer_all, steering.Steering.settle)

    def test_chapter_one_advanced_then_chapter_two_opens_with_its_wolves_active(self):
        one, two = ready_chapters()
        self.add_wolves()
        self.day(1)
        for w in console.MUSTER[:10]:
            self.fact('arrival', w, day=1)
        out = runner.approve_campaign(self.conn, self.world, 'martin', {'campaign': {'id': 'nh', 'title': 'The Ninth Howl test',
                                                                                     'chapters': chapters(one, two)}})
        self.run_id = runner.publish(self.conn, self.world, out['ticket'])['run']
        self.answer()
        self.gifts(console.MUSTER[:4], 1)
        self.day(3.1)
        self.tick()
        self.assertTrue(self.ready()['met'])
        status, result = self.steer('advance', {'when': 'now'}, reason='chapter one has done enough')
        self.assertEqual(status, 'done', result)
        self.settle()
        self.day(3.2)
        self.settle()
        first = self.run_id
        self.assertEqual(self.conn.execute('SELECT status FROM dm.story_releases WHERE id=%s', (first,)).fetchone()[0], 'complete')
        second = self.conn.execute("SELECT id,state FROM dm.story_releases WHERE program->>'id'='nh-ch2'").fetchone()
        self.assertIsNotNone(second, 'chapter two started')
        self.run_id = second[0]
        self.assertEqual(sorted(second[1]['seen']), console.MUSTER[:10], 'its wolves are active from the start (seen, seeded)')
        self.settle()
        self.gifts(console.MUSTER[:4], 3.3)
        self.day(5.4)
        self.fact('arrival', console.MUSTER[0], day=5.4)
        self.settle()
        self.assertEqual(self.ready()['waitingFor'], {'on': 'day begins'})
        self.fact('day begins', '', day=5.5, detail='Marketday')
        self.settle()
        self.assertEqual(self.one()['phase'], 'done', 'chapter two moved on by itself at the next day')


if __name__ == '__main__':
    unittest.main()


# Doc 86, Phase 4: stories that talk to each other. A quest pack's news reaches the chapter running (C12a), a world
# variable a pack changes wakes the chapter that declares it (C13a), and a side story counts only in a campaign chapter (J3).
def _story(pid, title, variables, triggers, quests=None):
    return {'format': 'ratw-story-program', 'version': 2, 'id': pid, 'title': title,
            'audience': {'kind': 'players', 'players': sorted(WOLVES)}, 'start': 'on', 'variables': variables,
            **({'quests': quests} if quests else {}),
            'phases': [{'id': 'on', 'journal': 'Under way.', 'effects': [], 'exits': [], 'triggers': triggers}],
            'tests': [{'name': 'Nothing', 'cohort': sorted(WOLVES), 'events': [], 'expect': ['on']}]}


PACK = _story('lamp-pack', 'The lamp pack', [{'id': 'nh_realm', 'type': 'number', 'scope': 'world'}], [
    {'id': 'voice', 'on': 'conversation', 'audience': 'participant', 'match': {'target': 'rm111'},
     'actions': [fx('story.notify', signal='voice', player='$actor', amount=2)]},
    {'id': 'unheard', 'on': 'conversation', 'audience': 'participant', 'match': {'target': 'rm113'},
     'actions': [fx('story.notify', signal='nobody_listens', player='$actor')]},
    {'id': 'realm', 'on': 'conversation', 'match': {'target': 'rm112'}, 'actions': [{'do': 'add', 'var': 'nh_realm', 'value': 3}]}])

CHAPTER = _story('nh-ch1', 'The chalk night', [
    {'id': 'p_voice', 'type': 'number', 'scope': 'player'}, {'id': 'heard', 'type': 'number', 'scope': 'story'},
    {'id': 'woken', 'type': 'number', 'scope': 'story'}, {'id': 'nh_realm', 'type': 'number', 'scope': 'world'}], [
    {'id': 'news', 'on': 'story notified', 'audience': 'participant', 'match': {'detail': 'voice'},
     'actions': [{'do': 'add', 'var': 'p_voice', 'value': {'field': 'quantity'}}]},
    {'id': 'woke', 'on': 'variable changed', 'match': {'target': 'nh_realm'},
     'actions': [{'do': 'set', 'var': 'heard', 'value': {'field': 'quantity'}}, {'do': 'add', 'var': 'woken', 'value': 1}]},
    {'id': 'own', 'on': 'conversation', 'match': {'target': 'rm119'}, 'actions': [{'do': 'add', 'var': 'nh_realm', 'value': 100}]}])


@unittest.skipUnless(database_available(), 'local PostgreSQL required')
class StoriesThatTalk(unittest.TestCase):
    setUpClass = classmethod(console.ConsoleReads.setUpClass.__func__)
    tearDownClass = classmethod(console.ConsoleReads.tearDownClass.__func__)
    fact, tick = console.ConsoleReads.fact, console.ConsoleReads.tick
    answer_all, settle = steering.Steering.answer_all, steering.Steering.settle
    setUp, release, approve, lone = Campaigns.setUp, Campaigns.release, Campaigns.approve, Campaigns.lone

    def commands(self, run, detail=None):
        return self.conn.execute(
            """SELECT payload->'event',status FROM dm.story_commands WHERE run=%s AND kind='story-event'""" +
            (" AND payload->'event'->>'detail'=%s" if detail else '') + ' ORDER BY id', (run, detail) if detail else (run,)).fetchall()

    def test_the_checks(self):
        rules.validate(PACK)
        rules.validate(CHAPTER)
        bad = copy.deepcopy(CHAPTER)
        bad['phases'][0]['triggers'][1]['match']['target'] = 'heard'
        self.assertRaisesRegex(ValueError, 'names a world variable this story declares', rules.validate, bad)
        bad = copy.deepcopy(CHAPTER)
        del bad['phases'][0]['triggers'][0]['match']
        self.assertRaisesRegex(ValueError, r'needs its news \(detail\) named', rules.validate, bad)
        self.assertTrue(runner._listens(CHAPTER, 'story notified', 'detail', 'voice'))
        self.assertFalse(runner._listens(CHAPTER, 'story notified', 'detail', 'other'))
        self.assertTrue(runner._listens(CHAPTER, 'variable changed', 'target', 'nh_realm'))
        self.assertEqual(rules.watch_of(CHAPTER), {}, 'the stories\' own events are the runner\'s: nothing for the game')

    def test_a_packs_news_reaches_the_running_chapter(self):
        chapter, pack = self.lone(CHAPTER), self.lone(PACK)
        self.settle()
        self.fact('conversation', ASH, 'rm111')
        self.settle()
        told = self.commands(chapter, 'voice')
        self.assertEqual(len(told), 1)
        event, status = told[0]
        self.assertEqual((event['kind'], event['actor'], event['target'], event['item'], event['quantity'], status),
                         ('story notified', ASH, 'lamp-pack', pack, 2, 'done'))
        self.assertEqual(self.release(chapter)[1]['counters']['player'][ASH]['p_voice'], 2, "the chapter's voice rises for Ash")
        self.assertNotIn('notices', self.release(pack)[1], 'told: none left waiting')
        # News nobody listens for goes nowhere.
        self.fact('conversation', BIRCH, 'rm113')
        self.settle()
        self.assertEqual(self.commands(chapter, 'nobody_listens'), [])
        self.assertNotIn('notices', self.release(pack)[1])
        # 200 a tick at most: the rest wait in the state for the next.
        state = self.release(pack)[1]
        state['notices'] = [{'signal': 'voice', 'player': ELM, 'amount': 1}] * 250
        self.conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), pack))
        self.answer_all()
        self.tick()
        self.assertLessEqual(len(self.commands(chapter, 'voice')), 1 + 200)
        self.assertEqual(len(self.release(pack)[1].get('notices', [])), 50, '50 wait for the next tick')
        self.settle(rounds=40)   # (The chapter takes its inbox a few dozen commands a tick.)
        self.assertEqual(len(self.commands(chapter, 'voice')), 1 + 250)
        self.assertEqual({s for _, s in self.commands(chapter, 'voice')}, {'done'})
        self.assertEqual(self.release(chapter)[1]['counters']['player'][ELM]['p_voice'], 250)
        # Named: only that story's run hears it (here, a story that is not running: nobody).
        state = self.release(pack)[1]
        state['notices'] = [{'signal': 'voice', 'player': ASH, 'amount': 5, 'story': 'nh-ch2'}]
        self.conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), pack))
        self.settle()
        self.assertEqual(self.release(chapter)[1]['counters']['player'][ASH]['p_voice'], 2)

    def test_news_named_for_one_story_reaches_only_its_run(self):
        # (Doc 86, Phase 4 after review: two chapters listening; the news names one.)
        second = copy.deepcopy(CHAPTER)
        second.update(id='nh-ch2', title='The second night')
        first, other, pack = self.lone(CHAPTER), self.lone(second), self.lone(PACK)
        self.settle()
        state = self.release(pack)[1]
        state['notices'] = [{'signal': 'voice', 'player': ASH, 'amount': 5, 'story': 'nh-ch2'}]
        self.conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), pack))
        self.settle()
        self.assertEqual(self.commands(first, 'voice'), [], 'the chapter not named hears nothing')
        [(event, status)] = self.commands(other, 'voice')
        self.assertEqual((event['actor'], event['quantity'], status), (ASH, 5, 'done'))
        self.assertEqual(self.release(other)[1]['counters']['player'][ASH]['p_voice'], 5)
        # Unnamed, both hear it.
        state = self.release(pack)[1]
        state['notices'] = [{'signal': 'voice', 'player': BIRCH, 'amount': 1}]
        self.conn.execute('UPDATE dm.story_releases SET state=%s::jsonb WHERE id=%s', (json.dumps(state), pack))
        self.settle()
        self.assertEqual((len(self.commands(first, 'voice')), len(self.commands(other, 'voice'))), (1, 2))

    def test_a_world_variable_changed_wakes_the_story_that_declares_it(self):
        chapter, pack = self.lone(CHAPTER), self.lone(PACK)
        self.settle()
        self.fact('conversation', ASH, 'rm112')
        self.settle()
        story = self.release(chapter)[1]['counters']['story']
        self.assertEqual((story['heard'], story['woken']), (3, 1), 'woken once, with the new value')
        [(event, status)] = self.commands(chapter)
        self.assertEqual((event['kind'], event['target'], event['detail'], event['item'], status), ('variable changed', 'nh_realm', '3', pack, 'done'))
        # Coalesced: two changes before the chapter takes the first are one telling, with the latest value.
        runner.tell_changes(self.conn, self.world, pack, {'nh_realm': ('number', 6)})
        runner.tell_changes(self.conn, self.world, pack, {'nh_realm': ('number', 9)})
        queued = [e for e, s in self.commands(chapter) if s == 'queued']
        self.assertEqual([(e['quantity'], e['detail']) for e in queued], [(9, '9')])
        self.settle()
        self.assertEqual(self.release(chapter)[1]['counters']['story']['woken'], 2)
        # A story never hears its own changes: the chapter's own write tells the pack nothing (it does not listen) and not
        # itself.
        before = len(self.commands(chapter))
        self.fact('conversation', ASH, 'rm119')
        self.settle()
        self.assertEqual(len(self.commands(chapter)), before)
        self.assertEqual(self.commands(pack), [])
        value = self.conn.execute("SELECT value FROM dm.story_world_vars WHERE world_id=%s AND name='nh_realm'", (self.world,)).fetchone()[0]
        self.assertEqual(value, 103, 'the tellings change nothing: only a story\'s own writes do')

    def test_a_side_story_counts_only_in_a_campaign_chapter(self):
        crossing = copy.deepcopy(CROSSING)
        crossing['quests'] = [{'id': 'ferry', 'title': 'The ferryman\'s debt', 'closure': 'side',
                               'objectives': [{'id': 'o', 'label': 'Pay the ferryman.', 'on': 'gift', 'match': {'target': 'rm112'}}]}]
        rules.validate(crossing)
        self.assertRaisesRegex(ValueError, 'A side story counts only in a campaign chapter', runner.approve, self.conn, self.world,
                               'martin', crossing, rules.digest(crossing))
        out = self.approve(muster(), crossing)
        self.assertTrue(out['ticket'])
        # A story's news for a story not approved yet: a warning, not a refusal (a pack may be approved first).
        pack = copy.deepcopy(PACK)
        pack['phases'][0]['triggers'][0]['actions'][0]['effect']['story'] = 'nh-ch9'
        warnings = runner.approve(self.conn, self.world, 'martin', pack, rules.digest(pack))['warnings']
        self.assertTrue(any('No story "nh-ch9"' in w for w in warnings), warnings)
