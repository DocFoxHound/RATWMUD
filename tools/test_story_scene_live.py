"""Real clients watch a standalone ceremony and play an embedded, private party scene on disposable Greyfen."""

import json
import subprocess
import threading
import time
import unittest

import npc_mind
import story_author
import story_program
import story_runner
import test_dm_live as live
import world_db


class SceneClients:
    def __init__(self, port):
        self.process = subprocess.Popen(
            [live.NODE, '--experimental-strip-types', '--no-warnings', 'tools/client/scene-test.ts', str(port)],
            cwd=live.ROOT,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
        )

    def request(self, op, identity, **data):
        self.process.stdin.write(json.dumps({'op': op, 'identity': identity, **data}) + '\n')
        self.process.stdin.flush()
        reply = json.loads(self.process.stdout.readline())
        if 'error' in reply:
            raise AssertionError(reply['error'])
        return reply['result']

    def close(self):
        self.process.communicate(timeout=10)


@unittest.skipUnless(
    live.database_available() and live.SERVER.exists() and live.NODE, 'local PostgreSQL, server and Node required'
)
class SceneLive(unittest.TestCase):
    def setUp(self):
        self.fixture = live.DungeonMasterLive()
        self.fixture.setUpClass()
        self.addCleanup(self.fixture.tearDownClass)
        self.mind = npc_mind.Server(0, npc_mind.Mind(npc_mind.FixtureProvider(), audit=lambda event: None))
        threading.Thread(target=self.mind.serve_forever, daemon=True).start()
        self.addCleanup(self.mind.server_close)
        self.addCleanup(self.mind.shutdown)
        self.server, port = self.fixture.start(
            240, ('--speed', '8', '--dialogue', f'http://127.0.0.1:{self.mind.server_port}/dialogue')
        )
        self.addCleanup(self.stop_server)
        self.clients = SceneClients(port)
        self.addCleanup(self.clients.close)
        self.players = {}
        for name in ['ash', 'fern', 'willow']:
            self.clients.request('connect', name)
            self.players[name] = self.until(lambda name=name: self.view(name).get('id'), 10, 'player signs in')
        self.run = None

    def stop_server(self):
        if self.server.poll() is None:
            self.server.terminate()
        self.server.communicate(timeout=40)

    def until(self, fn, seconds, label):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            result = fn()
            if result:
                return result
            time.sleep(0.12)
        self.fail('Timed out: ' + label)

    def view(self, name):
        return self.clients.request('read', name)

    def command(self, name, **message):
        self.clients.request('command', name, command=message)

    def move(self, name, x, y):
        action = self.fixture.queue('character.move', self.players[name], {'cell': 'town', 'x': x, 'y': y})
        self.assertEqual(self.fixture.outcome(action)[0], 'applied')

    def publish(self, program):
        # Wait for the game to checkpoint these native sign-ins before publication freezes eligibility.
        def has_characters():
            with world_db.connect('dev', 'dm', dbname=self.fixture.name) as db:
                return db.execute(
                    'SELECT count(*) FROM game.characters WHERE key=ANY(%s)', (list(self.players.values()),)
                ).fetchone()[0] == len(self.players)

        self.until(has_characters, 50, 'eligible characters in the checkpoint')
        with world_db.connect('dev', 'dm', dbname=self.fixture.name) as db:
            self.world = story_runner.world_id(db)
            approval = story_runner.approve(db, self.world, 'test-host', program, story_program.digest(program))
            self.run = story_runner.publish(db, self.world, approval['ticket'])['run']
        self.done = threading.Event()
        self.errors = []

        def run_worker():
            while not self.done.is_set():
                try:
                    with world_db.connect('dev', 'dm', dbname=self.fixture.name) as db:
                        story_runner.tick_run(db, self.world, self.run, lambda: story_runner.game_day(db, self.world))
                except Exception as exc:  # noqa: BLE001 - propagate runner failure to the test thread
                    self.errors.append(exc)
                    break
                self.done.wait(0.1)

        worker = threading.Thread(target=run_worker, daemon=True)
        worker.start()
        self.addCleanup(worker.join, 5)
        self.addCleanup(self.done.set)

    def beat(self, name, beat, seconds=55):
        def current():
            if self.errors:
                raise self.errors[0]
            s = self.view(name)['scene']
            return s if s and s['beat'] == beat and not s.get('generating') else None

        return self.until(current, seconds, name + ' sees ' + beat)

    def vote(self, name, scene, option):
        self.command(name, type='scene_vote', instance=scene['instance'], beat=scene['beat'], option=option)

    def finish(self, phase):
        def done():
            if self.errors:
                raise self.errors[0]
            with world_db.connect('dev', 'dm', dbname=self.fixture.name) as db:
                status = story_runner.status(db, self.world, self.run)
            if status['status'] == 'paused':
                self.fail(str(status['problem']))
            return status if status['status'] == 'complete' else None

        self.assertEqual(self.until(done, 20, 'campaign finishes')['state']['phase'], phase)

    def example(self, visibility):
        s = story_author.request({'op': 'scene.draft', 'example': 'greyfen-oath'}, {})['scene']
        s['visibility'] = visibility
        if visibility != 'public':
            s.pop('area')
            s['start'] = 'witness'
            s['beats'] = [b for b in s['beats'] if b['type'] not in ('move', 'pose')]
        for b in s['beats']:
            if 'seconds' in b:
                b['seconds'] = 8 if b['type'] in ('choice', 'response') else 2
        return s

    def test_public_late_arrival_and_tied_vote_take_the_authored_default(self):
        self.move('willow', 38, 21)
        s = self.example('public')
        program = story_author.request({'op': 'scene.wrap', 'scene': s}, {})['program']
        self.publish(program)
        ballot = self.beat('ash', 'oath')
        self.assertIsNone(self.view('willow')['scene'])
        self.vote('ash', ballot, 'carters')
        self.until(lambda: self.view('ash')['scene'].get('voted'), 3, 'Ash ballot acknowledged')
        self.vote('ash', ballot, 'bridge')  # Cannot change or cast a second ballot.
        self.vote('fern', ballot, 'bridge')
        self.move('willow', 29, 21)
        late = self.beat('willow', 'oath', 3)
        self.assertFalse(late['canRespond'])
        self.vote('willow', late, 'carters')
        self.assertIn('bridge', self.beat('ash', 'bridge-watch', 15)['text'].lower())
        self.finish('complete')

    def test_embedded_party_decision_uses_typed_reply_and_returns_to_the_campaign(self):
        self.command('ash', type='action', action='invite', target=self.players['fern'])
        self.until(lambda: (self.view('fern').get('party') or {}).get('invite'), 8, 'party invitation')
        self.command('fern', type='party', verb='accept')
        self.until(lambda: len((self.view('ash').get('party') or {}).get('members', [])) == 2, 8, 'party formed')
        scene = self.example('party')
        scene['lockMovement'] = True
        program = story_author.request({'op': 'draft', 'blank': True}, {})['program']
        program = story_author.request({'op': 'scene.import', 'program': program, 'scene': scene}, {})['program']
        play = {'kind': 'scene.play', 'scene': scene['id'], 'cue': 'first-watch', 'player': self.players['ash']}
        program['phases'][0].update(
            effects=[play],
            triggers=[
                {
                    'id': 'mark-road',
                    'on': 'scene choice',
                    'match': {'detail': 'first-watch.carters'},
                    'actions': [{'do': 'set', 'scope': 'story', 'key': 'road', 'value': 1}],
                },
                {
                    'id': 'open-road',
                    'on': 'scene finished',
                    'match': {'detail': 'first-watch'},
                    'if': {'test': 'counter', 'scope': 'story', 'key': 'road', 'op': '=', 'value': 1},
                    'actions': [{'do': 'enter', 'phase': 'road'}],
                },
            ],
            exits=[{'on': 'scene finished', 'match': {'detail': 'first-watch'}, 'to': 'bridge'}],
        )
        for route in ('road', 'bridge'):
            program['phases'].append(
                {'id': route, 'journal': 'Take the first watch at the ' + route + '.', 'effects': [], 'exits': []}
            )
        finished = {'kind': 'scene finished', 'id': 'bell', 'day': 0, 'detail': 'first-watch'}
        program['tests'] = [
            {
                'name': 'Carters',
                'cohort': ['player-ash'],
                'events': [{'kind': 'scene choice', 'detail': 'first-watch.carters', 'id': 'vote', 'day': 0}, finished],
                'expect': ['opening', 'road'],
            },
            {'name': 'Bridge', 'cohort': ['player-ash'], 'events': [finished], 'expect': ['opening', 'bridge']},
        ]
        self.publish(program)
        ballot = self.beat('ash', 'oath')
        self.assertTrue(ballot['lockMovement'])
        before = self.view('ash')['position']
        self.command('ash', type='path', x=30.5, y=21.5)
        time.sleep(0.35)
        after = self.view('ash')['position']
        self.assertEqual((before['x'], before['y']), (after['x'], after['y']))
        self.assertEqual(self.beat('fern', 'oath')['instance'], ballot['instance'])
        self.assertIsNone(self.view('willow')['scene'])
        self.vote('willow', ballot, 'bridge')
        self.vote('ash', ballot, 'carters')
        self.vote('fern', ballot, 'carters')
        prompt = self.beat('ash', 'name-the-watch')
        private = 'The northbound wool merchant is my sister.'
        self.command('ash', type='scene_reply', instance=prompt['instance'], beat=prompt['beat'], text=private)
        generated = self.beat('fern', 'road-watch')
        self.assertEqual(generated['text'], 'I have heard your answer.')
        self.assertIsNone(self.view('willow')['scene'])
        self.finish('road')
        with world_db.connect('dev', 'dm', dbname=self.fixture.name) as db:
            facts = db.execute(
                "SELECT detail FROM game.events WHERE world_id=%s AND target=%s AND kind='scene response'",
                (self.world, self.run),
            ).fetchall()
            self.assertEqual(facts, [('first-watch.name-the-watch.submitted',)])
            self.assertFalse(
                db.execute(
                    'SELECT 1 FROM game.events WHERE world_id=%s AND detail LIKE %s', (self.world, '%' + private + '%')
                ).fetchone()
            )

    def test_individual_response_expires_and_its_fallback_releases_the_player(self):
        scene = self.example('individual')
        for b in scene['beats']:
            if b['type'] == 'choice':
                b['mode'] = 'first'
            if b['type'] == 'response':
                b['seconds'] = 3
        scene['lockMovement'] = True
        program = story_author.request({'op': 'scene.wrap', 'scene': scene, 'player': self.players['ash']}, {})[
            'program'
        ]
        self.publish(program)
        ballot = self.beat('ash', 'oath')
        self.vote('ash', ballot, 'carters')
        self.beat('ash', 'name-the-watch')
        self.assertIsNone(self.view('fern')['scene'])
        self.assertIn('names', self.beat('ash', 'silent-watch', 10)['text'])
        self.finish('complete')
        with world_db.connect('dev', 'dm', dbname=self.fixture.name) as db:
            facts = db.execute(
                "SELECT detail FROM game.events WHERE world_id=%s AND target=%s AND kind='scene response'",
                (self.world, self.run),
            ).fetchall()
            self.assertEqual(facts, [('presentation.name-the-watch.timeout',)])
        self.assertIsNone(self.view('ash')['scene'])

    def test_unavailable_scene_provider_uses_the_authored_line(self):
        from unittest.mock import patch

        scene = self.example('individual')
        scene['start'] = 'name-the-watch'
        scene['beats'] = [b for b in scene['beats'] if b['id'] in ('name-the-watch', 'road-watch', 'silent-watch')]
        fallback = next(b['text'] for b in scene['beats'] if b['id'] == 'road-watch')
        program = story_author.request({'op': 'scene.wrap', 'scene': scene, 'player': self.players['ash']}, {})[
            'program'
        ]
        with patch.object(
            self.mind.mind.provider, 'complete', side_effect=npc_mind.BridgeError('provider_unavailable')
        ) as provider:
            self.publish(program)
            prompt = self.beat('ash', 'name-the-watch')
            self.command(
                'ash',
                type='scene_reply',
                instance=prompt['instance'],
                beat=prompt['beat'],
                text='The flour cart is overdue.',
            )
            self.assertEqual(self.beat('ash', 'road-watch')['text'], fallback)
            self.assertTrue(provider.called)
        self.finish('complete')

    def test_private_standalone_starts_for_next_returning_member(self):
        # Every connected member is in the frozen audience. Only the player who next arrives sees the scene.
        scene = self.example('individual')
        text = 'Look for the secret lantern at the river.'
        scene.update(
            title='The hidden lantern',
            start='lantern',
            beats=[{'id': 'lantern', 'type': 'line', 'speaker': 'captain', 'text': text, 'seconds': 8}],
        )
        self.publish(story_author.request({'op': 'scene.wrap', 'scene': scene}, {})['program'])

        def watch_journal(name):
            return next((entry for entry in self.view(name).get('journal', []) or [] if entry['id'] == self.run), None)

        self.until(lambda: watch_journal('willow'), 30, 'another eligible wolf has the opening journal')
        self.assertIsNone(self.view('ash')['scene'])
        self.clients.request('close', 'fern')
        # Wait for the game to admit a new connection for the same signed-in character.
        self.clients.request('connect', 'fern')
        self.until(lambda: self.view('fern').get('id') == self.players['fern'], 15, 'Fern returns')
        playing = self.beat('fern', 'lantern')
        self.assertEqual((playing['title'], playing['text']), (scene['title'], text))
        for outsider in ('ash', 'willow'):
            self.assertIsNone(self.view(outsider)['scene'])
            journal = watch_journal(outsider)
            self.assertEqual(journal['title'], 'A private scene')
            for private_text in (scene['title'], text):
                self.assertNotIn(private_text, json.dumps(journal))
        self.finish('complete')
        with world_db.connect('dev', 'dm', dbname=self.fixture.name) as db:
            row = db.execute(
                """SELECT payload->>'player',payload->>'visit' FROM dm.actions
                               WHERE kind='story.scene' AND target_id=%s""",
                (self.run,),
            ).fetchall()
            self.assertEqual(row, [(self.players['fern'], '2')])

    def test_overlapping_private_scenes_keep_the_second_ballot_open(self):
        first = self.example('individual')
        first.update(
            id='first-lantern',
            start='reading',
            beats=[
                {
                    'id': 'reading',
                    'speaker': 'captain',
                    'type': 'line',
                    'text': 'Count the lanterns before you answer.',
                    'seconds': 8,
                }
            ],
        )
        second = self.example('individual')
        second.update(
            id='second-lantern',
            start='ballot',
            beats=[
                {
                    'id': 'ballot',
                    'speaker': 'captain',
                    'type': 'choice',
                    'mode': 'first',
                    'seconds': 5,
                    'default': 'wait',
                    'text': 'Will you carry the next lantern?',
                    'options': [{'id': 'carry', 'label': 'Yes', 'next': 'accepted'}, {'id': 'wait', 'label': 'Wait'}],
                },
                {
                    'id': 'accepted',
                    'speaker': 'captain',
                    'type': 'line',
                    'text': 'The next lantern is yours.',
                    'seconds': 3,
                },
            ],
        )
        actor = self.players['ash']

        def play(scene):
            return {'kind': 'scene.play', 'scene': scene['id'], 'player': actor, 'cue': 'lantern'}

        p = story_author.request({'op': 'draft', 'blank': True}, {})['program']
        p['scenes'] = [first, second]
        p['audience'] = {'kind': 'players', 'players': [actor]}
        p['phases'][0]['effects'] = [play(first), play(second)]
        p['tests'][0]['cohort'] = [actor]
        self.publish(p)
        reading = self.beat('ash', 'reading')
        ballot = self.beat('ash', 'ballot', 18)
        self.assertNotEqual(reading['instance'], ballot['instance'])
        self.assertGreater(ballot['remainingMs'], 1500)
        self.assertIsNone(self.view('willow')['scene'])
        self.vote('ash', ballot, 'carry')
        self.assertEqual(self.beat('ash', 'accepted')['text'], 'The next lantern is yours.')
        self.finish('opening')
        with world_db.connect('dev', 'dm', dbname=self.fixture.name) as db:
            rows = db.execute(
                """SELECT kind,detail FROM game.events WHERE target=%s AND kind IN ('scene finished','scene choice')
                             ORDER BY id""",
                (self.run,),
            ).fetchall()
            self.assertEqual(
                rows, [('scene finished', 'lantern'), ('scene choice', 'lantern.carry'), ('scene finished', 'lantern')]
            )
