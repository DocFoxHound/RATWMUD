"""Offline authoring and, with RATW_STORY_TEST_DSN, transactional delivery in an isolated migrated test database."""

import copy
import json
import os
import unittest

import story_author
import story_program as rules
import story_runner as runner


class AuthorTests(unittest.TestCase):
    def test_examples_and_gate(self):
        examples = runner.catalogue()['examples']
        self.assertEqual(set(examples), {'wagon', 'wardens', 'hidden-pup', 'road-war'})
        for p in examples.values():
            self.assertTrue(rules.review(p)['passed'])
        p = examples['wardens']
        case = p['tests'][0]
        case['events'].insert(2, copy.deepcopy(case['events'][1]))
        self.assertTrue(rules.review(p)['passed'])
        case['events'][2]['actor'] = 'other'
        with self.assertRaisesRegex(ValueError, 'different contents'):
            rules.review(p)

    def test_author_capability_boundary(self):
        for op in ('approve', 'publish', 'signal-ticket', 'signal', 'stop', 'execute'):
            with self.assertRaises(ValueError):
                story_author.request({'op': op}, {})
        info = story_author.request({'op': 'draft', 'example': 'wagon'}, {})
        self.assertTrue(story_author.request({'op': 'rehearse', **info}, {})['passed'])

    def test_reward_requires_player(self):
        p = runner.catalogue()['examples']['wagon']
        p['phases'][1]['exits'][0].pop('participant')
        with self.assertRaisesRegex(ValueError, 'participating player'):
            rules.review(p)


@unittest.skipUnless(os.environ.get('RATW_STORY_TEST_DSN'), 'Set a disposable migrated PostgreSQL test DSN.')
class RuntimeTests(unittest.TestCase):
    def setUp(self):
        from uuid import uuid4

        import psycopg

        self.conn = psycopg.connect(os.environ['RATW_STORY_TEST_DSN'])
        self.addCleanup(self.conn.close)  # Every test rolls its uncommitted world back.
        self.world = 'fixture-' + uuid4().hex[:12]
        self.conn.execute('INSERT INTO world.worlds(id,name) VALUES (%s,%s)', (self.world, 'Test world'))
        self.conn.execute(
            'INSERT INTO game.checkpoints(world_id,schema_version,revision,payload) VALUES (%s,1,1,%s)',
            (self.world, '{"calendarDays":10}'),
        )
        for npc in ('rm111', 'rm112', 'rm113', 'rm119'):
            self.conn.execute(
                """INSERT INTO live.npcs(world_id,id,name,role,age,voice,appearance,hours_start,hours_end,
                home_x,home_y,work_x,work_y,evening_x,evening_y) VALUES (%s,%s,%s,'civilian',4,1,'{}',8,12,0,0,0,0,0,0)""",
                (self.world, npc, npc),
            )
        for faction in ('city_watch', 'road_company'):
            self.conn.execute(
                'INSERT INTO live.factions(world_id,id,name) VALUES (%s,%s,%s)', (self.world, faction, faction)
            )
        self.cohort = ['player-ash', 'player-birch', 'player-elm']
        for wolf in self.cohort:
            self.conn.execute('SET LOCAL ROLE ratw_game')
            self.conn.execute(
                "INSERT INTO game.characters(world_id,key,position,data) VALUES (%s,%s,1,'{}')", (self.world, wolf)
            )
            self.fact('arrival', actor=wolf)
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        self.examples = runner.catalogue()['examples']

    def fact(self, kind, actor='', target='', day=10, item='', quantity=0):
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute(
            'SELECT game.record_events(%s,%s::jsonb)',
            (
                self.world,
                json.dumps(
                    [{'kind': kind, 'actor': actor, 'target': target, 'day': day, 'item': item, 'quantity': quantity}]
                ),
            ),
        )
        self.conn.execute('SET LOCAL ROLE ratw_dm')

    def launch(self, id):
        p = self.examples[id]
        self.ticket = runner.approve(self.conn, self.world, 'human', p, rules.digest(p))['ticket']
        self.run = runner.publish(self.conn, self.world, self.ticket)['run']
        self.assertEqual(runner.publish(self.conn, self.world, self.ticket)['run'], self.run)
        return self.run

    def status(self):
        return runner.status(self.conn, self.world, self.run)

    def answer(self, status='applied'):
        ids = self.status()['actions']
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute('UPDATE dm.actions SET status=%s WHERE id=%s', (status, ids[-1]['id']))
        self.conn.execute('SET LOCAL ROLE ratw_dm')

    def tick(self):
        runner.tick(self.conn, self.world)

    def test_distinct_cohort_and_durable_cursor(self):
        self.launch('wardens')
        self.answer()
        self.fact('conversation', self.cohort[1], 'rm112')
        self.fact('conversation', self.cohort[0], 'rm113')
        for actor in ['outsider', self.cohort[0], self.cohort[0]]:
            self.fact('conversation', actor, 'rm112')
        self.tick()
        self.assertEqual(self.status()['state']['votes'], {'0': [self.cohort[0]]})
        # Previous votes and cursors survive a new invocation, before the next member joins.
        self.tick()
        self.fact('conversation', self.cohort[1], 'rm113')
        self.fact('conversation', self.cohort[1], 'rm112')
        self.tick()
        self.assertEqual(self.status()['state']['phase'], 'welcome')
        self.answer()
        self.tick()
        self.assertEqual(self.status()['actions'][-1]['kind'], 'festival.call')
        self.answer('refused')
        self.tick()
        self.assertEqual(self.status()['status'], 'paused')
        self.assertIn('refused', self.status()['problem'])
        first = self.status()['actions'][-1]['id']
        retry = runner.direct(self.conn, self.world, 'retry', {'run': self.run, 'operator': 'human'})
        self.assertNotEqual(retry['receipt'], first)
        self.assertEqual(self.status()['actions'][-1]['kind'], 'festival.call')
        self.assertIn(first, self.status()['history'][0]['receipts'])
        runner.direct(self.conn, self.world, 'stop', {'run': self.run, 'operator': 'human'})
        self.assertEqual(self.status()['actions'][-1]['kind'], 'story.stop')

    def test_direction_bound_to_one_run_visit_and_factions(self):
        self.launch('road-war')
        self.answer()
        self.fact('conversation', self.cohort[0], 'rm112')
        self.tick()
        self.assertEqual(self.status()['state']['phase'], 'blockade')
        for kind in ['story.phase', 'bandits.call', 'times.nudge', 'factions.sync']:
            self.assertEqual(self.status()['actions'][-1]['kind'], kind)
            self.answer()
            self.tick()
        row = self.conn.execute(
            'SELECT stance FROM live.faction_relations WHERE world_id=%s', (self.world,)
        ).fetchone()
        self.assertEqual(row[0], 'war')
        data = {'run': self.run, 'signal': 'accord signed', 'operator': 'director'}
        ticket = runner.direct(self.conn, self.world, 'signal-ticket', data)['ticket']
        stale = runner.direct(self.conn, self.world, 'signal-ticket', data)['ticket']
        with self.assertRaises(ValueError):
            runner.direct(self.conn, 'another', 'signal', {'ticket': ticket})
        self.assertEqual(runner.direct(self.conn, self.world, 'signal', {'ticket': ticket})['phase'], 'truce')
        self.assertEqual(
            runner.direct(self.conn, self.world, 'signal', {'ticket': ticket})['status'], 'already delivered'
        )
        with self.assertRaises(ValueError):
            runner.direct(self.conn, self.world, 'signal', {'ticket': stale})
        self.answer()
        self.tick()
        self.answer()
        self.tick()
        self.assertEqual(self.status()['actions'][-1]['kind'], 'story.finish')
        self.answer()
        self.tick()
        self.assertEqual(self.status()['status'], 'complete')

    def test_voice_is_opt_in_and_phase_bound(self):
        self.launch('hidden-pup')
        data = {'run': self.run, 'npc': 'rm119', 'operator': 'director'}
        with self.assertRaises(ValueError):
            runner.direct(self.conn, self.world, 'voice-ticket', data)
        self.answer()
        self.fact('conversation', self.cohort[0], 'rm113')
        self.tick()
        self.answer()
        token = runner.direct(self.conn, self.world, 'voice-ticket', data)['ticket']
        line = {'ticket': token, 'text': 'I remember a tiny pawprint on the step.'}
        with self.assertRaises(ValueError):
            runner.direct(self.conn, self.world, 'signal', line)
        first = runner.direct(self.conn, self.world, 'voice', line)
        self.assertEqual(first['receipt'], runner.direct(self.conn, self.world, 'voice', line)['receipt'])
        action = self.conn.execute(
            'SELECT kind,requested_by,payload FROM dm.actions WHERE id=%s', (first['receipt'],)
        ).fetchone()
        self.assertEqual(action[:2], ('story.voice', 'director'))
        self.assertEqual(action[2]['npc'], 'rm119')

    def test_context_digest_and_expiry(self):
        self.assertEqual(runner.atlas(self.conn, self.world, 'rm11')['matches'][0]['id'], 'rm111')
        self.assertEqual(runner.context(self.conn, self.world, 'rm111')['resident']['id'], 'rm111')
        p = self.examples['wagon']
        wrong = rules.digest(p)
        p['title'] += '!'
        with self.assertRaisesRegex(ValueError, 'exact version'):
            runner.approve(self.conn, self.world, 'human', p, wrong)
        grant = runner.approve(self.conn, self.world, 'human', p, rules.digest(p))
        self.conn.execute(
            "UPDATE dm.story_releases SET expires_at=now()-interval '1 day' WHERE id=%s", (grant['run'],)
        )
        with self.assertRaisesRegex(ValueError, 'expired'):
            runner.publish(self.conn, self.world, grant['ticket'])
        self.assertFalse(
            self.conn.execute("SELECT has_table_privilege('ratw_game','dm.story_releases','INSERT')").fetchone()[0]
        )


if __name__ == '__main__':
    unittest.main()
