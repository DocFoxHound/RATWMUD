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
        self.assertEqual(set(examples), {'wagon', 'wardens', 'hidden-pup', 'road-war', 'river-relief'})
        for p in examples.values():
            self.assertTrue(rules.review(p)['passed'])
        p = examples['wardens']
        case = p['tests'][0]
        case['events'].insert(2, copy.deepcopy(case['events'][1]))
        self.assertTrue(rules.review(p)['passed'])
        case['events'][2]['actor'] = 'other'
        with self.assertRaisesRegex(ValueError, 'different contents'):
            rules.review(p)

    def test_event_library_is_playable_and_searchable(self):
        catalog = story_author.request({'op': 'catalog'}, {})
        events = catalog['eventTypes']
        self.assertEqual(len(events), 100)
        self.assertEqual(len({e['id'] for e in events}), len(events))
        self.assertEqual(len({e['title'] for e in events}), len(events))
        self.assertEqual({e['scale'] for e in events}, {'small', 'local', 'regional', 'realm'})
        self.assertEqual({e['mode'] for e in events}, {'individual', 'group', 'either'})
        self.assertEqual(len({e['category'] for e in events}), 10)
        items = {row['id'] for row in json.loads((runner.DATA / 'Items/items.json').read_text())['items']}
        for event in events:
            with self.subTest(event=event['id']):
                for objective in event['objectives']:
                    if objective['on'] in (
                        'gift',
                        'forage',
                        'stall sale',
                        'loan returned',
                    ) and 'item' in objective.get('match', {}):
                        self.assertIn(objective['match']['item'], items)
                draft = story_author.request({'op': 'draft', 'eventType': event['id']}, {})['program']
                report = rules.review(draft)
                self.assertTrue(report['passed'])
                self.assertEqual(report['uncovered'], [])
                if event['mode'] != 'individual':
                    self.assertEqual(len(report['tests']), 2)
                    self.assertEqual(
                        report['tests'][1]['path'][-1], 'work' if event['mode'] == 'group' else 'resolved'
                    )

    def test_returning_to_a_phase_reopens_its_missions(self):
        program = runner.catalogue()['examples']['river-relief']
        edge = program['phases'][1]['exits'][0]
        edge['to'] = 'survey'
        # Keep the ordinary route to accounts reachable as an alternative branch.
        program['phases'][1]['exits'].append({'on': 'clock', 'afterDays': 7, 'to': 'accounts'})
        rules.validate(program)
        case = program['tests'][0]
        state = rules.begin(program, case['cohort'], 0)
        for ev in case['events']:
            rules.advance(program, state, ev, 0)
            if state['visits'] == 3:
                break
        self.assertEqual(state['phase'], 'survey')
        self.assertEqual(rules.mission_credits(state['previousMissions'])['ferry-line'], ['player-ash', 'player-elm'])
        missions = rules.mission_snapshot(program['phases'][0], state)
        self.assertTrue(all(not m['ready'] and not m['tracks'] for m in missions))
        self.assertEqual([m['id'] for m in missions if m['optional']], ['manifest'])

    def test_wrong_notice_or_loan_cannot_satisfy_a_linked_objective(self):
        for name in ('the-shared-tool-chest', 'the-missing-return-wagon'):
            program = story_author.request({'op': 'draft', 'eventType': name}, {})['program']
            case = program['tests'][0]
            first = next(e for e in case['events'] if e['kind'] in ('notice posted', 'lent'))
            closing = next(e for e in case['events'] if e['kind'] in ('notice answered', 'loan returned'))
            self.assertEqual(first['detail'], closing['detail'])
            closing['detail'] = 'unrelated-record'
            result = rules.review(program)['tests'][0]
            self.assertFalse(result['passed'])
            self.assertEqual(result['path'][-1], 'work')
            self.assertEqual(result['missionCredits'][name], [])

    def test_campaign_tracks_each_visit_and_optional_missions(self):
        campaign = runner.catalogue()['examples']['river-relief']
        test = campaign['tests'][0]
        test['events'].insert(0, {'id': 'outsider', 'kind': 'letter sent', 'actor': 'outsider', 'day': 0})
        test['events'].insert(1, copy.deepcopy(test['events'][1]))
        visits = rules.review(campaign)['tests'][0]['missionVisits']
        self.assertEqual([v['phase'] for v in visits], ['survey', 'rigging', 'accounts'])
        self.assertEqual(
            visits[0]['credits'],
            {'passengers': ['player-ash', 'player-birch'], 'dispatch': ['player-elm'], 'manifest': ['player-ash']},
        )
        self.assertEqual(
            visits[1]['credits'],
            {'ferry-line': ['player-ash', 'player-elm'], 'provisions': ['player-birch'], 'first-aid': []},
        )
        self.assertEqual(visits[2]['credits'], test['expectMissions'])
        campaign['phases'][0]['missions'][0]['objectives'][0]['measure'] = 'coins'
        with self.assertRaisesRegex(ValueError, 'does not record coins'):
            rules.validate(campaign)

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

    def test_campaign_journal_receipts_and_archives(self):
        self.launch('river-relief')
        self.answer()
        ash, birch, elm = self.cohort
        self.fact('conversation', ash, 'rm112')  # Out of order.
        self.fact('conversation', ash, 'rm111')
        self.fact('letter sent', elm)
        self.fact('letter sent', 'outsider')
        self.tick()
        partial = self.status()
        self.assertEqual(partial['actions'][-1]['kind'], 'story.missions')
        missions = {m['id']: m for m in partial['history'][0]['missions']}
        self.assertEqual(missions['dispatch']['tracks']['group'], [0, 1])
        self.assertEqual(missions['passengers']['tracks'], {ash: [1, 0]})
        self.tick()
        self.assertEqual(self.status()['actions'], partial['actions'])
        self.answer('refused')
        self.tick()
        self.assertEqual(self.status()['status'], 'paused')
        pending = partial['actions'][-1]['id']
        retry = runner.direct(self.conn, self.world, 'retry', {'run': self.run, 'operator': 'human'})['receipt']
        self.assertNotEqual(pending, retry)
        self.assertEqual(self.status()['history'][0]['receipts'][-2:], [pending, retry])
        self.answer()
        self.fact('conversation', ash, 'rm112')
        self.fact('letter sent', elm)
        self.fact('conversation', birch, 'rm111')
        self.fact('conversation', birch, 'rm112')
        self.tick()
        detail = self.status()
        self.assertEqual(detail['state']['phase'], 'rigging')
        archived = detail['history'][1]['missions']
        self.assertEqual(
            rules.mission_credits(archived), {'passengers': [ash, birch], 'dispatch': [elm], 'manifest': []}
        )
        self.assertTrue(all(not m['tracks'] for m in detail['history'][0]['missions']))
        payload = self.conn.execute(
            'SELECT payload FROM dm.actions WHERE id=%s', (detail['actions'][-1]['id'],)
        ).fetchone()[0]
        self.assertEqual(payload['previousMissions'], archived)
        self.answer()
        self.fact('gift', ash, 'rm111', item='rope~fine', quantity=4)
        self.fact('stall sale', elm, quantity=4)
        self.fact('gift again', ash, 'rm111', item='canvas', quantity=2)
        self.tick()
        self.assertEqual(self.status()['state']['phase'], 'rigging')
        self.answer()
        self.fact('gift', birch, 'rm111', item='canvas', quantity=1)
        self.tick()
        self.assertEqual(self.status()['state']['phase'], 'accounts')
        self.answer()
        self.fact('archive work', ash, item='copy')
        self.fact('loan returned', birch)
        self.fact('notice posted', ash)
        self.tick()
        self.answer()
        self.tick()
        final = self.status()
        self.assertEqual(final['status'], 'complete')
        self.assertEqual([row['phase'] for row in final['history']], ['reopened', 'accounts', 'rigging', 'survey'])
        self.assertEqual(rules.mission_credits(final['history'][2]['missions'])['ferry-line'], [ash, birch])

    def test_unreachable_group_publication(self):
        campaign = self.examples['river-relief']
        campaign['phases'][1]['missions'][0]['contributors'] = 4
        campaign['phases'][0]['missions'][0]['percent'] = 50
        # Rehearse a successful four-member crew, then try to launch with the actual three-member audience.
        for case in campaign['tests']:
            case['cohort'].append('player-larch')
        case = campaign['tests'][0]
        case['expectMissionVisits'][1]['credits']['ferry-line'] = sorted(case['cohort'])
        index = next(i for i, e in enumerate(case['events']) if e.get('item') == 'canvas')
        for actor in ('player-birch', 'player-larch'):
            case['events'].insert(
                index,
                {
                    'id': 'canvas-' + actor,
                    'kind': 'gift',
                    'actor': actor,
                    'target': 'rm111',
                    'item': 'canvas',
                    'quantity': 1,
                    'day': 0,
                },
            )
        ticket = runner.approve(self.conn, self.world, 'human', campaign, rules.digest(campaign))['ticket']
        with self.assertRaisesRegex(ValueError, 'audience'):
            runner.publish(self.conn, self.world, ticket)

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
