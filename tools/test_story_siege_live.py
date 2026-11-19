"""Publish the Greyfen siege to a disposable native server and run its palace scene through the game.

Combat counts and the final two conversations are controlled game-event fixtures. Every walk, arrival, speech,
posture and spawn in between is executed by the server. Uses the same local database and binary as test_dm_live.
"""

import json
import signal
import subprocess
import time
import unittest

import story_program
import story_runner
import test_dm_live as live
import world_db


@unittest.skipUnless(
    live.database_available() and live.SERVER.exists() and live.NODE, 'local PostgreSQL, server and Node required'
)
class SiegeLive(unittest.TestCase):
    def test_defense_to_palace_to_relief(self):
        fixture = live.DungeonMasterLive()
        fixture.setUpClass()
        self.addCleanup(fixture.tearDownClass)

        def start(seconds):
            server, port = fixture.start(seconds, ('--speed', '15'))

            def stop():
                if server.poll() is None:
                    server.send_signal(signal.SIGTERM)
                server.communicate(timeout=40)

            self.addCleanup(stop)
            return server, port

        server, port = start(90)
        first = subprocess.run(
            [
                live.NODE,
                '--experimental-strip-types',
                '--no-warnings',
                'tools/client/hold.ts',
                '--port',
                str(port),
                '--identity',
                'greyfen-defender',
                '--seconds',
                '2',
            ],
            cwd=live.ROOT,
            capture_output=True,
            text=True,
            check=True,
            timeout=20,
        )
        wolf = next(
            row['in']
            for line in first.stdout.splitlines()
            if line.startswith('{') and 'in' in (row := json.loads(line))
        )
        server.send_signal(signal.SIGTERM)
        server.communicate(timeout=40)

        program = story_runner.catalogue()['examples']['siege-at-greyfen']
        with world_db.connect('dev', 'dm', dbname=fixture.name) as dm:
            world = story_runner.world_id(dm)
            grant = story_runner.approve(dm, world, 'test-dm', program, story_program.digest(program))
            run = story_runner.publish(dm, world, grant['ticket'])['run']

        def watch(port):
            observer = subprocess.Popen(
                [
                    live.NODE,
                    '--experimental-strip-types',
                    '--no-warnings',
                    'tools/client/hold.ts',
                    '--port',
                    str(port),
                    '--identity',
                    'greyfen-defender',
                    '--seconds',
                    '230',
                ],
                cwd=live.ROOT,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )

            def leave():
                if observer.poll() is None:
                    observer.terminate()
                observer.wait(timeout=10)

            self.addCleanup(leave)

        server, port = start(240)
        watch(port)

        def record(*facts):
            with world_db.connect('dev', 'game', dbname=fixture.name) as game:
                day = story_runner.game_day(game, world)
                data = [{'day': day, 'actor': wolf, **f} for f in facts]
                game.execute('SELECT game.record_events(%s,%s::jsonb)', (world, json.dumps(data)))

        def tick_until(predicate, seconds):
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                with world_db.connect('dev', 'dm', dbname=fixture.name) as dm:
                    story_runner.tick_run(dm, world, run, lambda: story_runner.game_day(dm, world))
                    status = story_runner.status(dm, world, run)
                self.assertNotEqual(status['status'], 'paused', str((status['problem'], status['actions'])))
                self.assertNotEqual(
                    status['state']['phase'], 'reserves', 'An actor failed to reach the authored destination.'
                )
                if predicate(status):
                    return status
                time.sleep(0.15)
            self.fail(f'Timed out in {status["state"]["phase"]}: {status["trace"][:4]} {status["actions"][-3:]}')

        at_breach = {'kind': 'bandit falls', 'target': 'camp_test', 'cell': 'town', 'x': 29, 'y': 34}
        record(
            {**at_breach, 'x': 28},
            {**at_breach, 'cell': 'barracks'},
            {**at_breach, 'actor': 'outsider'},
            {'kind': 'bandit falls', 'cell': 'town'},
            at_breach,
            at_breach,
        )
        defense = tick_until(lambda s: s['state'].get('counters', {}).get('story', {}).get('line-held') == 2, 20)
        self.assertEqual(defense['state']['phase'], 'hold')
        record(at_breach)

        # Restart after the game accepts the cross-door courier order. Run progress and the
        # resident's pending movement must be enough to finish without resubmitting that order.
        def courier_dispatched(status):
            if status['state']['phase'] != 'council':
                return False
            with world_db.connect('dev', 'dm', dbname=fixture.name) as dm:
                return (
                    dm.execute(
                        "SELECT 1 FROM dm.actions WHERE kind='npc.walk' AND status='applied' "
                        "AND payload->>'run'=%s AND payload->>'cue'='at-court'",
                        (run,),
                    ).fetchone()
                    is not None
                )

        tick_until(courier_dispatched, 80)
        server.send_signal(signal.SIGTERM)
        server.communicate(timeout=40)
        server, port = start(240)
        watch(port)
        scene = tick_until(lambda s: s['state']['phase'] == 'relief', 150)
        self.assertEqual(
            scene['state']['counters'], {'player': {wolf: {'defeats': 3}}, 'story': {'line-held': 3, 'guard-posts': 2}}
        )

        with world_db.connect('dev', 'dm', dbname=fixture.name) as dm:
            events = dm.execute(
                'SELECT kind,actor,detail,cell,x,y FROM game.events WHERE world_id=%s AND target=%s '
                "AND kind IN ('scene arrived','scene posed','scene spoken') ORDER BY id",
                (world, run),
            ).fetchall()
        cues = [e[2] for e in events]
        self.assertEqual(
            cues[:8],
            ['at-throne', 'seated', 'at-breach', 'at-court', 'report-heard', 'on-feet', 'at-table', 'orders-given'],
        )
        self.assertCountEqual(cues[8:], ['left-post', 'right-post'])
        self.assertIn(('scene arrived', 'sloe', 'at-court', 'barracks', 21, 4), events)
        self.assertIn(('scene arrived', 'birch', 'left-post', 'town', 30, 37), events)
        self.assertIn(('scene arrived', 'tamsin', 'right-post', 'town', 33, 37), events)

        record({'kind': 'conversation', 'target': 'wren'}, {'kind': 'conversation', 'target': 'harrow'})
        result = tick_until(lambda s: s['status'] == 'complete', 25)
        self.assertEqual([v['phase'] for v in reversed(result['history'])], ['hold', 'council', 'relief', 'secured'])
