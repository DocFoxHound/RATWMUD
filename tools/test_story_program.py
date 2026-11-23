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
        self.assertEqual(
            set(examples),
            {
                'wagon',
                'wardens',
                'hidden-pup',
                'road-war',
                'river-relief',
                'reed-ford',
                'winter-ferries',
                'siege-at-greyfen',
            },
        )
        for p in examples.values():
            self.assertTrue(rules.review(p)['passed'])
        p = examples['wardens']
        case = p['tests'][0]
        case['events'].insert(2, copy.deepcopy(case['events'][1]))
        self.assertTrue(rules.review(p)['passed'])
        case['events'][2]['actor'] = 'other'
        with self.assertRaisesRegex(ValueError, 'different contents'):
            rules.review(p)

    def test_rehearsal_has_a_work_and_time_budget(self):
        from unittest.mock import patch

        p = runner.catalogue()['examples']['siege-at-greyfen']
        with self.assertRaisesRegex(ValueError, 'too large to run in one request'):
            rules.review(p, rules.RehearsalBudget(steps=10))
        with self.assertRaisesRegex(ValueError, 'too large to run in one request'):
            rules.review(p, rules.RehearsalBudget(seconds=-1, steps=10**9))
        self.assertTrue(rules.review(p)['passed'])  # The default budget runs every bundled example.
        # The live runner's dispatch is never budgeted.
        self.assertIsNone(rules._budget.get())
        with patch.object(rules, 'REHEARSAL_SECONDS', 0.01):
            held = [rules._rehearsals.acquire(timeout=1) for _ in range(rules.REHEARSALS_AT_ONCE)]
            try:
                with self.assertRaisesRegex(ValueError, 'Other rehearsals are running'):
                    rules.review(p)
            finally:
                for ok in held:
                    if ok:
                        rules._rehearsals.release()
        self.assertTrue(rules.review(p)['passed'])
        with patch.object(rules, 'REHEARSAL_STEPS', 10):
            with self.assertRaisesRegex(ValueError, 'too large'):
                story_author.request({'op': 'rehearse', 'program': p}, {}, None, False)

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
                self.assertEqual(report['uncoveredTriggers'], [])
                if event['mode'] != 'individual':
                    self.assertEqual(len(report['tests']), 2)
                    self.assertEqual(
                        report['tests'][1]['path'][-1],
                        'work' if event['mode'] == 'group' else 'resolved',
                    )

    def test_component_catalogue_round_trip(self):
        catalog = story_author.request({'op': 'catalog'}, {})
        self.assertTrue(rules.review(story_author.request({'op': 'draft', 'blank': True}, {})['program'])['passed'])
        for entry in catalog['eventTypes']:
            with self.subTest(entry=entry['id']):
                pieces = story_author.request({'op': 'components', 'eventType': entry['id']}, {})['components']
                self.assertEqual(pieces, entry['components'])
                program = story_author.request({'op': 'draft', 'eventType': entry['id']}, {})['program']
                report = rules.review(program)
                work = next(p for p in program['phases'] if p['id'] == 'work')
                for part, o in zip(
                    [c for c in pieces if c['role'] == 'objective'],
                    work['missions'][0]['objectives'],
                    strict=True,
                ):
                    self.assertEqual(part['objective'], o)
                    self.assertIn(part['trigger'], work['triggers'])
                    self.assertEqual(part['trigger']['actions'][0]['signal'], o['signal'])
                    self.assertTrue(any(t['trigger'] == part['trigger']['id'] for t in report['tests'][0]['triggers']))
                self.assertEqual(
                    [e['kind'] for e in report['tests'][0]['effects']],
                    [e['kind'] for e in entry.get('ending', [])],
                )

    def test_participants_keep_independent_deadlines_with_the_same_name(self):
        p = runner.catalogue()['blank']
        phase = p['phases'][0]
        timer = {'signal': 'last-call', 'owner': 'player'}
        phase['triggers'] = [
            {
                'id': 'enrol',
                'on': 'arrival',
                'audience': 'participant',
                'actions': [{'do': 'schedule', 'days': 3, **timer}],
            },
            {
                'id': 'withdraw',
                'on': 'departure',
                'audience': 'participant',
                'actions': [{'do': 'cancel', **timer}],
            },
            {
                'id': 'remind',
                'on': 'story signal',
                'signal': 'last-call',
                'audience': 'participant',
                'actions': [{'do': 'add', 'key': 'reminders', 'scope': 'player', 'value': 1}],
            },
        ]
        rules.validate(p)
        state = rules.begin(p, ['ash', 'fern'], 0)
        for day, actor, kind in [
            (1, 'ash', 'arrival'),
            (2, 'fern', 'arrival'),
            (3, 'ash', 'departure'),
        ]:
            rules.advance(p, state, {'kind': kind, 'actor': actor}, day)
        rules.advance(p, state, {'kind': 'clock'}, 4)
        self.assertNotIn('counters', state)
        rules.advance(p, state, {'kind': 'clock'}, 5)
        self.assertEqual(state['counters'], {'player': {'fern': {'reminders': 1}}})
        self.assertFalse(state['timers'])

    def test_campaign_timers_and_independent_real_clock(self):
        program = runner.catalogue()['examples']['winter-ferries']
        regular, outage = rules.review(program)['tests']
        self.assertEqual(regular['path'][-1], 'ferries-reopen')
        self.assertEqual(outage['path'], ['enrol', 'ferry-watch', 'deep-freeze', 'limited-crossings'])
        self.assertEqual(outage['counters'], {'story': {'watches': 1}})
        self.assertEqual([f['trigger'] for f in regular['triggers']].count('open-winter-ledger'), 1)
        # Hold the world calendar at day zero. The same release can span three actual calendar months.
        for timer in program['triggers'][0]['actions'][:3]:
            timer['clock'] = 'real'
        for case in program['tests']:
            for fact in case['events']:
                fact['elapsedDays'], fact['day'] = fact['day'], 0
        result = rules.review(program)
        self.assertTrue(result['passed'])
        self.assertEqual(result['uncoveredTriggers'], [])
        self.assertEqual(result['tests'][0]['counters'], regular['counters'])
        program['tests'][0]['events'][-1]['elapsedDays'] = 1
        with self.assertRaisesRegex(ValueError, 'elapsed real days'):
            rules.review(program)

    def test_injury_effect_requires_a_native_dm_injury(self):
        effect = {'kind': 'character.injury', 'player': 'player-ash', 'severity': 1}
        rules.validate_effect({**effect, 'add': 'cracked_rib'}, 'winter')
        with self.assertRaisesRegex(ValueError, 'character.injury.add'):
            rules.validate_effect({**effect, 'add': 'unknown_injury'}, 'winter')

    def test_signal_validation_and_dispatch_order(self):
        program = runner.catalogue()['examples']['reed-ford']
        phase = program['phases'][0]
        s = rules.begin(program, ['player-ash'], 0)
        self.assertFalse(
            rules.advance(
                program,
                s,
                {
                    'kind': 'story signal',
                    'signal': 'open-crossing',
                    'actor': 'player-ash',
                },
                0,
            )
        )
        self.assertNotIn('triggerEffects', s)
        first = phase['triggers'][0]
        first['actions'].append({'do': 'emit', 'signal': 'inspect'})
        first['if'] = {
            'test': 'all',
            'conditions': [
                {
                    'test': 'not',
                    'condition': {
                        'test': 'field',
                        'field': 'actor',
                        'op': '=',
                        'value': 'outsider',
                    },
                },
                {'test': 'field', 'field': 'quantity', 'op': '>=', 'value': 1},
            ],
        }
        self.assertTrue(rules.review(program)['passed'])
        phase['triggers'][2]['actions'].append({'do': 'emit', 'signal': 'inspect'})
        with self.assertRaisesRegex(ValueError, 'cycle'):
            rules.validate(program)
        phase['triggers'][2]['actions'].pop()
        phase['triggers'][3].pop('audience')
        with self.assertRaisesRegex(ValueError, 'participant trigger'):
            rules.validate(program)

    def test_one_fact_credits_at_most_one_objective_in_each_mission(self):
        program = story_author.request({'op': 'draft', 'eventType': 'axle-at-the-ford'}, {})['program']
        phase = program['phases'][0]
        phase['triggers'][0]['actions'] += phase['triggers'][1]['actions']
        s = rules.begin(program, ['player-ash'], 0)
        event = {
            'kind': 'conversation',
            'actor': 'player-ash',
            'target': 'rm111',
            'quantity': 1,
        }
        self.assertFalse(rules.advance(program, s, event, 0))
        self.assertEqual(s['missions']['axle-at-the-ford']['tracks']['player-ash'], [1, 0])
        self.assertTrue(rules.advance(program, s, event, 0))

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

    def test_region_edges_missing_evidence_and_condition_groups(self):
        program = runner.catalogue()['blank']
        program['regions'] = [{'id': 'breach', 'cell': 'gate', 'x1': 0, 'y1': 4, 'x2': 2, 'y2': 5}]
        inside = {'test': 'region', 'region': 'breach'}
        true = {'test': 'field', 'field': 'item', 'op': '=', 'value': 'rope'}
        conditions = {
            'inside': inside,
            'outside': {'test': 'not', 'condition': inside},
            'either': {'test': 'any', 'conditions': [inside, true]},
            'neither': {'test': 'not', 'condition': {'test': 'any', 'conditions': [inside, true]}},
            'not-both': {'test': 'not', 'condition': {'test': 'all', 'conditions': [inside, true]}},
        }
        phase = program['phases'][0]
        phase['triggers'] = [
            {
                'id': key,
                'on': 'bandit falls',
                'if': condition,
                'actions': [{'do': 'add', 'key': key, 'value': 1, 'scope': 'story'}],
            }
            for key, condition in conditions.items()
        ]
        for position, expected in [
            ({'cell': 'gate', 'x': 0, 'y': 4}, ['inside', 'either', 'not-both']),
            ({'cell': 'gate', 'x': 2, 'y': 5}, ['inside', 'either', 'not-both']),
            ({'cell': 'gate', 'x': 3, 'y': 5}, ['outside', 'neither', 'not-both']),
            ({'cell': 'other', 'x': 2, 'y': 5}, ['outside', 'neither', 'not-both']),
            ({'cell': 'gate'}, ['not-both']),
            ({'item': 'rope'}, ['either']),
        ]:
            with self.subTest(position=position):
                case = program['tests'][0]
                case.update(events=[{'id': 'a', 'kind': 'bandit falls', 'day': 0, **position}], expectTriggers=expected)
                self.assertTrue(rules.review(program)['passed'])
        phase['triggers'][0]['if'] = {'test': 'region', 'region': 'lost'}
        with self.assertRaisesRegex(ValueError, 'Unknown story region'):
            rules.validate(program)
        phase['triggers'][0]['if'] = inside
        program['regions'][0]['x1'] = 3
        with self.assertRaisesRegex(ValueError, 'tile bounds'):
            rules.validate(program)

    def test_reward_requires_player(self):
        p = runner.catalogue()['examples']['wagon']
        p['phases'][1]['exits'][0].pop('participant')
        with self.assertRaisesRegex(ValueError, 'participating player'):
            rules.review(p)


@unittest.skipUnless(
    os.environ.get('RATW_STORY_TEST_DSN'),
    'Set a disposable migrated PostgreSQL test DSN.',
)
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
        # (Placed, with a home: doc 86's V1 refuses a resident with no place in the world.)
        self.conn.execute("INSERT INTO world.areas(world_id,id,kind) VALUES (%s,'fixture-homes','cell')", (self.world,))
        for npc in ('rm111', 'rm112', 'rm113', 'rm119'):
            self.conn.execute(
                """INSERT INTO live.npcs(world_id,id,name,role,age,voice,appearance,hours_start,hours_end,
                home_area,home_x,home_y,work_x,work_y,evening_x,evening_y) VALUES (%s,%s,%s,'civilian',4,1,'{}',8,12,'fixture-homes',0,0,0,0,0,0)""",
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

    def fact(self, kind, actor='', target='', day=10, item='', quantity=0, **fields):
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute(
            'SELECT game.record_events(%s,%s::jsonb)',
            (
                self.world,
                json.dumps(
                    [{'kind': kind, 'actor': actor, 'target': target, 'day': day, 'item': item, 'quantity': quantity, **fields}]
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

    def test_scene_play_embeds_approved_document_and_uses_originating_player(self):
        import story_scenes

        scene = story_scenes.blank()
        scene['cast'] = [{'id': 'speaker', 'npc': 'rm111'}]
        program = story_scenes.wrap(scene)
        self.examples['portable'] = program
        self.launch('portable')
        self.answer()
        player = self.cohort[1]
        self.fact('arrival', player)
        self.tick()
        self.assertEqual(self.status()['state']['phase'], 'playing')
        self.answer()
        self.tick()
        receipt = self.status()['actions'][-1]['id']
        kind, target, payload = self.conn.execute(
            'SELECT kind,target_id,payload FROM dm.actions WHERE id=%s', (receipt,)
        ).fetchone()
        self.assertEqual((kind, target), ('story.scene', self.run))
        self.assertEqual(payload, {'scene': scene, 'player': player, 'visit': 2, 'cue': 'presentation'})
        self.answer()
        self.fact('scene finished', player, 'another-run', quantity=2, detail='presentation')
        self.fact('scene finished', player, self.run, quantity=1, detail='presentation')
        self.tick()
        self.assertEqual(self.status()['state']['phase'], 'playing')
        self.fact('scene finished', player, self.run, quantity=2, detail='presentation')
        self.tick()
        self.assertEqual(self.status()['state']['phase'], 'complete')
        scene['cast'][0]['npc'] = 'missing-resident'
        with self.assertRaisesRegex(ValueError, 'Unknown npc'):
            runner.references(self.conn, self.world, program)

    def test_written_response_branch_ignores_other_runs_and_visits(self):
        p = self.examples['answers'] = story_author.request({'op': 'draft', 'blank': True}, {})['program']
        p['phases'][0]['triggers'] = [
            {
                'id': outcome,
                'on': 'scene response',
                'match': {'detail': 'oath.promise.' + outcome},
                'actions': [{'do': 'add', 'key': outcome, 'scope': 'story', 'value': 1}],
            }
            for outcome in ('submitted', 'timeout')
        ]
        self.launch('answers')
        self.answer()
        for run, visit, outcome in [
            ('other-run', 1, 'submitted'),
            (self.run, 2, 'timeout'),
            (self.run, 1, 'submitted'),
            (self.run, 1, 'timeout'),
        ]:
            self.fact('scene response', self.cohort[0], run, quantity=visit, detail='oath.promise.' + outcome)
        self.tick()
        self.assertEqual(self.status()['state']['counters'], {'story': {'submitted': 1, 'timeout': 1}})

    def test_scene_participant_is_verified_before_publication(self):
        import story_scenes

        p = story_scenes.wrap(story_scenes.blank(), self.cohort[0])
        p['phases'][0]['effects'][0]['player'] = self.cohort[1]
        approval = runner.approve(self.conn, self.world, 'human', p, rules.digest(p))
        with self.assertRaisesRegex(ValueError, 'opening audience'):
            runner.publish(self.conn, self.world, approval['ticket'])
        self.assertFalse(
            self.conn.execute('SELECT 1 FROM dm.actions WHERE target_id=%s', (approval['run'],)).fetchone()
        )

    def test_terminal_phase_waits_for_native_scene_end(self):
        import story_scenes

        player = self.cohort[0]
        p = story_scenes.wrap(story_scenes.blank(), player)
        p['phases'] = p['phases'][:1]
        p['phases'][0]['exits'] = []
        p['tests'] = [{'name': 'Closing scene', 'cohort': [player], 'events': [], 'expect': ['playing']}]
        self.examples['closing'] = p
        self.launch('closing')
        self.answer()
        self.tick()
        self.assertEqual(self.status()['actions'][-1]['kind'], 'story.scene')
        self.answer()
        self.tick()
        self.assertEqual(self.status()['actions'][-1]['kind'], 'story.scene')
        self.fact('scene finished', player, self.run, quantity=2, detail='presentation')
        self.tick()
        self.assertEqual(self.status()['actions'][-1]['kind'], 'story.scene')
        self.fact('scene finished', player, self.run, quantity=1, detail='presentation')
        self.tick()
        self.assertEqual(self.status()['actions'][-1]['kind'], 'story.finish')
        self.answer()
        self.tick()
        self.assertEqual(self.status()['status'], 'complete')

    def test_overlapping_presentations_with_same_cue_hold_the_terminal_visit(self):
        import story_scenes

        scene = story_scenes.blank()
        first, second, _ = self.cohort
        p = self.examples['paired'] = story_scenes.wrap(scene)

        # A single arrival matches two separate rules in order. Both scenes are staged for this visit.
        def launch(player):
            return {
                'do': 'effect',
                'effect': {'kind': 'scene.play', 'scene': scene['id'], 'player': player, 'cue': 'shared'},
            }

        p.update(
            start='standby',
            phases=[
                {
                    'id': 'standby',
                    'journal': 'Wait for a volunteer.',
                    'effects': [],
                    'exits': [],
                    'triggers': [
                        {
                            'id': 'notify-witness',
                            'on': 'arrival',
                            'audience': 'participant',
                            'actions': [launch(first)],
                        },
                        {
                            'id': 'notify-volunteer',
                            'on': 'arrival',
                            'audience': 'participant',
                            'actions': [launch('$actor'), {'do': 'enter', 'phase': 'end'}],
                        },
                    ],
                },
                {'id': 'end', 'journal': 'The invitations have been sent.', 'effects': [], 'exits': []},
            ],
            tests=[
                {
                    'name': 'One invitation for each wolf',
                    'cohort': [first, second],
                    'events': [{'kind': 'arrival', 'actor': second, 'day': 0, 'id': 'join'}],
                    'expect': ['standby', 'end'],
                }
            ],
        )
        self.launch('paired')
        self.answer()
        self.fact('arrival', second)
        self.tick()
        self.answer()
        # (Doc 85, Phase 7: both presentations go to the game together, in order, in one batch.)
        self.tick()
        last = self.status()['actions'][-1]
        self.assertEqual(last['kind'], 'story.effects')
        items = self.conn.execute('SELECT payload FROM dm.actions WHERE id=%s', (last['id'],)).fetchone()[0]['items']
        self.assertEqual([i['kind'] for i in items], ['story.scene', 'story.scene'])
        self.assertEqual([(i['payload']['cue'], i['payload']['player'], i['payload']['visit']) for i in items],
                         [('shared', first, 2), ('shared', second, 2)])
        self.answer()
        self.fact('scene finished', second, self.run, quantity=2, detail='shared')
        self.fact('scene finished', first, self.run, quantity=1, detail='shared')
        self.tick()
        self.assertEqual(self.status()['actions'][-1]['kind'], 'story.effects')
        self.fact('scene failed', first, self.run, quantity=2, detail='shared')
        self.tick()
        self.assertEqual(self.status()['actions'][-1]['kind'], 'story.finish')

    def test_only_reviewed_tiles_and_current_scene_receipts_drive_the_run(self):
        self.conn.execute('RESET ROLE')
        self.conn.execute("INSERT INTO world.areas(world_id,id,kind) VALUES (%s,'gate','cell')", (self.world,))
        self.conn.execute(
            """INSERT INTO world.cells(world_id,id,name,x,y,width,height)
                             VALUES (%s,'gate','Gate',200,100,8,6)""",
            (self.world,),
        )
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        p = runner.catalogue()['blank']
        zone = {'id': 'breach', 'cell': 'gate', 'x1': 1, 'y1': 2, 'x2': 8, 'y2': 5}
        p['regions'] = [zone]
        with self.assertRaisesRegex(ValueError, 'outside gate'):
            runner.references(self.conn, self.world, p)
        self.assertEqual(runner.atlas(self.conn, self.world, 'Gate')['matches'],
                         [{'kind':'cell','id':'gate','name':'Gate','width':8,'height':6}])
        zone['x2'] = 7
        phase = p['phases'][0]
        phase['triggers'] = [
            {
                'id': 'defend',
                'on': 'bandit falls',
                'audience': 'participant',
                'if': {'test': 'region', 'region': 'breach'},
                'actions': [
                    {'do': 'add', 'key': 'defended', 'scope': 'player', 'value': 1},
                    {
                        'do': 'effect',
                        'effect': {'kind': 'npc.speak', 'npc': 'rm111', 'text': 'Hold the gate.', 'cue': 'report'},
                    },
                ],
            },
            {
                'id': 'reward',
                'on': 'scene spoken',
                'match': {'detail': 'report', 'actor': 'rm111'},
                'actions': [
                    {
                        'do': 'effect',
                        'effect': {'kind': 'story.area.buff', 'region': 'breach', 'buff': 'fed', 'hours': 2},
                    }
                ],
            },
            {
                'id': 'stand-watch',
                'on': 'region entered',
                'match': {'target': 'breach'},
                'audience': 'participant',
                'actions': [{'do': 'add', 'key': 'visits', 'scope': 'story', 'value': 1}],
            },
        ]
        self.examples['local'] = p
        self.launch('local')
        self.answer()
        ash = self.cohort[0]
        for coordinates in [{}, {'x': 0, 'y': 2}, {'x': 1, 'y': 2}]:
            self.fact('bandit falls', ash, cell='gate', **coordinates)
        self.tick()
        self.tick()
        receipt = self.status()['actions'][-1]['id']
        self.assertEqual(
            self.conn.execute('SELECT kind,target_id,payload FROM dm.actions WHERE id=%s', (receipt,)).fetchone(),
            ('npc.speak', 'rm111', {'text': 'Hold the gate.', 'cue': 'report', 'visit': 1, 'run': self.run}),
        )
        self.assertEqual(self.status()['state']['counters']['player'], {ash: {'defended': 1}})
        trace = self.status()['trace'][0]
        self.assertEqual((trace['cell'], trace['x'], trace['y']), ('gate', 1, 2))
        self.answer()
        self.fact('scene spoken', 'rm111', 'other-run', quantity=1, detail='report')
        self.fact('scene spoken', 'rm111', self.run, quantity=3, detail='report')
        self.fact('region entered', ash, 'breach', detail='other-run')
        self.tick()
        self.assertFalse(self.status()['state'].get('triggerEffects'))
        self.fact('region entered', ash, 'breach', detail=self.run)
        self.fact('scene spoken', 'rm111', self.run, quantity=1, detail='report')
        self.tick()
        self.tick()
        receipt = self.status()['actions'][-1]['id']
        kind, target, payload = self.conn.execute(
            'SELECT kind,target_id,payload FROM dm.actions WHERE id=%s', (receipt,)
        ).fetchone()
        self.assertEqual((kind, target), ('story.area_buff', self.run), self.status())
        self.assertEqual(
            payload,
            {
                'region': 'breach',
                'cell': 'gate',
                'x1': 1,
                'x2': 7,
                'y1': 2,
                'y2': 5,
                'buff': 'fed',
                'hours': 2,
                'visit': 1,
            },
        )
        self.assertEqual(self.status()['state']['counters']['story'], {'visits': 1})

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
        # The world was down past the ten minutes: the act expired unapplied, and Retry sends it again.
        self.answer('expired')
        self.tick()
        self.assertEqual(self.status()['status'], 'paused')
        expired = self.status()['actions'][-1]['id']
        again = runner.direct(self.conn, self.world, 'retry', {'run': self.run, 'operator': 'human'})['receipt']
        self.assertNotEqual(again, expired)
        self.assertEqual(self.status()['actions'][-1]['kind'], 'festival.call')
        # Paused, then the game answered applied after all (from its journal, after a restart): Retry resumes, sending nothing.
        self.answer('expired')
        self.tick()
        self.answer('applied')
        sent = len(self.status()['actions'])
        resumed = runner.direct(self.conn, self.world, 'retry', {'run': self.run, 'operator': 'human'})
        self.assertEqual((resumed['status'], len(self.status()['actions'])), ('running', sent))
        self.assertEqual(self.status()['status'], 'running')
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

    def test_live_trigger_effect_order_and_durable_cursor(self):
        program = self.examples['reed-ford']
        # A separate repeatable world action leaves the phase open and must settle before further facts.
        effect = {'kind': 'deed.award', 'player': '$actor', 'detail': 'Ferry hand'}
        program['phases'][0]['triggers'].insert(
            0,
            {
                'id': 'lend-a-hand',
                'on': 'tended',
                'audience': 'participant',
                'once': 'player',
                'actions': [
                    {'do': 'effect', 'effect': effect},
                    {'do': 'add', 'scope': 'player', 'key': 'help', 'value': 1},
                ],
            },
        )
        self.launch('reed-ford')
        self.answer()
        ash, birch, _ = self.cohort
        self.fact('story signal', ash)  # Raw database facts cannot impersonate an internal signal.
        self.fact('tended', ash)
        self.fact('tended', ash)
        self.fact('gift', ash, 'rm111', quantity=2, item='rope')
        self.fact('gift again', birch, 'rm111', quantity=2, item='rope')
        # (Doc 85, Phase 7: the runner reads on while effects gather, and sends them together. Ash's deed for tending
        # waits, and the gifts move the story to the crossing in the same turn: the deed goes with the new visit.)
        self.tick()
        status = self.status()
        self.assertEqual(status['state']['phase'], 'crossing')
        self.assertEqual(status['state']['counters']['player'][ash], {'help': 1, 'hauled': 2})
        self.assertEqual([e['player'] for e in status['state']['arrivalEffects']], [ash, birch])
        self.answer()
        self.tick()
        batch = self.status()['actions'][-1]
        self.assertEqual(batch['kind'], 'story.effects')
        self.assertEqual([(i['kind'], i['target']) for i in batch['items']], [('deed.award', ash), ('deed.award', birch)])
        self.tick()
        self.assertEqual(self.status()['actions'][-1]['id'], batch['id'], 'one batch at a time, waited for')
        # The game refuses Ash's deed (a pause, the default): Birch's is not applied after it, and the story pauses.
        self.conn.execute('SET LOCAL ROLE ratw_game')
        self.conn.execute('UPDATE dm.actions SET status=%s,result=%s WHERE id=%s', ('applied', json.dumps(
            [{'ok': False, 'text': 'That wolf is not in the world.'}, {'ok': False, 'held': True, 'text': 'Not applied.'}]), batch['id']))
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        self.tick()
        self.assertEqual(self.status()['status'], 'paused')
        self.assertIn('deed.award for player-ash', self.status()['problem'])
        retry = runner.direct(self.conn, self.world, 'retry', {'run': self.run, 'operator': 'human'})
        self.assertNotEqual(retry['receipt'], batch['id'])
        again = self.status()['actions'][-1]
        self.assertEqual([(i['kind'], i['target']) for i in again['items']], [('deed.award', ash), ('deed.award', birch)])
        self.answer()
        self.tick()
        status = self.status()
        self.assertEqual((status['status'], status['state']['phase']), ('running', 'crossing'))
        self.fact('notice posted', ash)
        self.tick()
        self.answer()
        self.tick()
        self.assertEqual(self.status()['status'], 'complete')

    def test_timer_fires_and_persists_without_event_rows(self):
        p = self.examples['reed-ford']
        p['phases'][0]['triggers'].append(
            {
                'id': 'deadline',
                'on': 'clock',
                'afterDays': 1,
                'once': 'visit',
                'actions': [{'do': 'add', 'key': 'overdue', 'scope': 'story', 'value': 1}],
            }
        )
        self.launch('reed-ford')
        self.answer()
        for _ in range(3):
            runner.tick_run(self.conn, self.world, self.run, lambda: 12)
        self.assertEqual(self.status()['state']['counters']['story']['overdue'], 1)

    def test_named_timer_survives_idle_polls_and_cancellation(self):
        p = self.examples['reed-ford']
        p['phases'][0]['triggers'].extend(
            [
                {
                    'id': 'start-watch',
                    'on': 'letter sent',
                    'actions': [{'do': 'schedule', 'signal': 'watch', 'days': 1, 'repeat': True}],
                },
                {
                    'id': 'one-shot',
                    'on': 'letter sent',
                    'actions': [{'do': 'schedule', 'signal': 'reminder', 'days': 1.5}],
                },
                {
                    'id': 'cancel-watch',
                    'on': 'reported',
                    'actions': [{'do': 'cancel', 'signal': 'watch'}],
                },
                {
                    'id': 'watch-turn',
                    'on': 'story signal',
                    'signal': 'watch',
                    'audience': 'participant',
                    'actions': [{'do': 'add', 'key': 'rounds', 'scope': 'player', 'value': 1}],
                },
                {
                    'id': 'remind',
                    'on': 'story signal',
                    'signal': 'reminder',
                    'actions': [{'do': 'add', 'key': 'notes', 'scope': 'story', 'value': 1}],
                },
            ]
        )
        self.launch('reed-ford')
        self.answer()
        self.fact('letter sent', self.cohort[0])
        runner.tick_run(self.conn, self.world, self.run, lambda: 10)
        for day in (10.5, 11, 11, 12, 12.3):
            runner.tick_run(self.conn, self.world, self.run, lambda day=day: day)
        self.assertEqual(
            self.status()['state']['counters'],
            {'player': {self.cohort[0]: {'rounds': 2}}, 'story': {'notes': 1}},
        )
        self.assertNotIn('reminder', self.status()['state']['timers'])
        self.fact('reported', self.cohort[0], day=12.5)
        runner.tick_run(self.conn, self.world, self.run, lambda: 14)
        self.assertEqual(self.status()['state']['timers'], {})
        runner.tick_run(self.conn, self.world, self.run, lambda: 15)
        self.assertEqual(self.status()['state']['counters']['player'][self.cohort[0]]['rounds'], 2)

    def test_real_deadline_uses_recording_time_and_survives_world_clock_pause(self):
        from unittest.mock import patch

        p = self.examples['reed-ford']
        p['phases'][0]['triggers'].extend(
            [
                {
                    'id': 'start-season',
                    'on': 'letter sent',
                    'actions': [
                        {
                            'do': 'schedule',
                            'signal': 'quarter',
                            'days': 90,
                            'clock': 'real',
                        }
                    ],
                },
                {
                    'id': 'end-season',
                    'on': 'story signal',
                    'signal': 'quarter',
                    'actions': [{'do': 'set', 'key': 'ended', 'scope': 'story', 'value': 1}],
                },
            ]
        )
        self.launch('reed-ford')
        self.answer()
        epoch = self.status()['state']['publishedAt']
        self.fact('letter sent', self.cohort[0])
        runner.tick_run(self.conn, self.world, self.run, lambda: 10)
        deadline = self.status()['state']['timers']['quarter']['at']
        self.assertAlmostEqual(deadline, 90, places=3)
        with patch('story_runner.time.time', return_value=epoch + 89 * 86400):
            runner.tick_run(self.conn, self.world, self.run, lambda: 10)
        self.assertNotIn('counters', self.status()['state'])
        with patch('story_runner.time.time', return_value=epoch + 91 * 86400):
            runner.tick_run(self.conn, self.world, self.run, lambda: 10)
        self.assertEqual(self.status()['state']['counters']['story'], {'ended': 1})
        self.assertFalse(self.status()['state']['timers'])

    def test_broken_release_isolated_and_trace_cursor_is_stable(self):
        self.launch('reed-ford')
        broken = self.run
        self.conn.execute(
            "UPDATE dm.story_releases SET state=jsonb_set(state,'{phase}',%s::jsonb) WHERE id=%s",
            ('"missing"', broken),
        )
        p = self.examples['river-relief']
        p['phases'][0]['triggers'] = [
            {
                'id': 'count-news',
                'on': 'notice posted',
                'actions': [{'do': 'add', 'key': 'news', 'value': 1, 'scope': 'story'}],
            }
        ]
        self.launch('river-relief')
        self.answer()
        for _ in range(51):
            self.fact('notice posted', self.cohort[0])
        self.tick()
        self.assertEqual(
            self.conn.execute('SELECT status FROM dm.story_releases WHERE id=%s', (broken,)).fetchone()[0],
            'paused',
        )
        status = self.status()
        self.assertEqual(len(status['trace']), 50)
        self.assertEqual(status['state']['counters']['story']['news'], 51)
        earlier = runner.status(self.conn, self.world, self.run, before_trigger=status['beforeTrigger'])
        self.assertEqual(len(earlier['trace']), 1)
        self.assertLess(earlier['trace'][0]['id'], status['trace'][-1]['id'])
        self.tick()
        self.assertEqual(self.status()['trace'], status['trace'])
        self.assertEqual(earlier['trace'][0]['actor'], self.cohort[0])

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
        # (Doc 85, Phase 7: the phase, then its opening effects together, in order, in one batch.)
        self.assertEqual(self.status()['actions'][-1]['kind'], 'story.phase')
        self.answer()
        self.tick()
        batch = self.status()['actions'][-1]
        self.assertEqual((batch['kind'], [i['kind'] for i in batch['items']]),
                         ('story.effects', ['bandits.call', 'times.nudge', 'factions.sync']))
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
        # (Doc 85, Phase 7: a direction goes into the runner's inbox, never refused for world events waiting.)
        self.fact('conversation', self.cohort[1], 'rm112')
        self.assertEqual(runner.direct(self.conn, self.world, 'signal', {'ticket': ticket})['status'], 'queued')
        self.assertEqual(
            runner.direct(self.conn, self.world, 'signal', {'ticket': ticket})['status'], 'already delivered'
        )
        self.tick()
        self.assertEqual(self.status()['state']['phase'], 'truce')
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
        self.conn.execute('SET LOCAL ROLE ratw_game')
        for key, text in (('m1', 'Conversation record. Ash said: "my secret"'), ('m2', 'Summary. Ash asked about the ford.')):
            self.conn.execute(
                'INSERT INTO game.npc_memories(world_id,key,position,data) VALUES (%s,%s,1,%s::jsonb)',
                (self.world, key, json.dumps({'npc': 'rm111', 'subject': 'player-ash', 'text': text})),
            )
        self.conn.execute('SET LOCAL ROLE ratw_dm')
        memories = runner.context(self.conn, self.world, 'rm111')['memories']
        self.assertEqual([m['summary'] for m in memories], ['Summary. Ash asked about the ford.'])
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


class ReadinessShapes(unittest.TestCase):
    """Doc 86, R2: a phase's Readiness and its ready exit, as story_program checks them."""

    def setUp(self):
        import story_v2_samples
        self.p = copy.deepcopy(story_v2_samples.READY)

    def refused(self, words):
        self.assertRaisesRegex(ValueError, words, rules.validate, self.p)

    def test_the_sample_is_sound(self):
        rules.validate(self.p)
        report = rules.review(self.p)
        self.assertTrue(report['passed'])
        self.assertEqual((report['uncovered'], report['uncoveredTriggers'], report['uncoveredReady']), ([], [], []))

    def test_two_ready_exits_are_refused(self):
        self.p['phases'][0]['exits'].append({'on': 'ready', 'to': 'crossed'})
        self.refused('exactly one ready exit')

    def test_ready_without_its_exit_and_the_exit_without_ready(self):
        self.p['phases'][0]['exits'] = [{'on': 'dm signal', 'to': 'ford', 'match': {'detail': 'Cross now'}}]
        self.refused('exactly one ready exit')
        self.setUp()
        del self.p['phases'][1]['ready']
        self.refused('a ready exit needs the phase')
        self.setUp()
        self.p['phases'][0]['exits'][0]['match'] = {'detail': 'x'}
        self.refused('Unexpected fields|unexpected fields')

    def test_a_fact_or_a_wolf_value_in_a_value_part_is_refused(self):
        self.p['phases'][1]['ready']['all'][0]['value'] = {'field': 'quantity'}
        self.refused('not a wolf\'s or the event\'s')
        self.setUp()
        self.p['phases'][1]['ready']['all'][0]['value'] = {'var': 'hauls', 'of': '$actor'}
        self.refused('not a wolf\'s or the event\'s')
        self.setUp()
        self.p['phases'][0]['ready']['all'][2]['any'][1]['test'] = {'test': 'wolf indoors', 'is': True}
        self.refused('not the world\'s facts')

    def test_an_actor_reward_behind_a_ready_exit_is_refused(self):
        self.p['phases'][1]['effects'] = [{'kind': 'deed.award', 'player': '$actor', 'detail': 'Crossed first.'}]
        self.refused('requires a participating player')

    def test_the_moments_and_bounds(self):
        self.p['phases'][1]['ready']['then'] = {'on': 'moon', 'match': {'detail': 'new'}}
        rules.validate(self.p)   # (Doc 86, Phase 2: the moon is a moment now, J4.)
        self.assertEqual(rules.ready_words(self.p['phases'][1]['ready']['then']), 'the next new moon')
        self.setUp()
        self.p['phases'][1]['ready']['then'] = {'on': 'month begins'}
        self.refused('Readiness waits for one of')
        self.setUp()
        self.p['phases'][0]['ready']['auto'] = True
        self.refused('autoAfterDays is for a phase that waits')
        self.setUp()
        self.p['phases'][0]['ready']['all'] = []
        self.refused('1 to 8 parts')
        self.setUp()
        self.p['phases'][0]['ready']['all'][2]['any'] = self.p['phases'][0]['ready']['all'][2]['any'][:1]
        self.refused('2 to 4 parts')
        self.setUp()
        self.p['phases'][0]['ready']['all'][2]['any'][0] = {'any': [{'minDays': 1}, {'minDays': 2}]}
        self.refused('one deep')
        self.setUp()
        self.p['phases'][0]['ready']['all'][2]['any'][0]['missions'] = ['elsewhere']
        self.refused('missions of this phase')
        self.setUp()
        self.p['version'] = 1
        del self.p['variables']
        self.assertRaises(ValueError, rules.validate, self.p)

    def test_the_watch_list_asks_for_its_moment(self):
        self.assertIn('day begins', rules.watch_of(self.p)['events'])
        self.assertIn('wolves', rules.watch_of(self.p).get('facts', []), 'counting who is online reads the signed-in wolves')


class FilterForms(unittest.TestCase):
    """Doc 86, C12b: a filter may begin with a text or be one of several, in triggers, exits, missions and objectives; a
    deed's detail ends with "(<deed id>)", so it is matched by how it begins."""

    def test_a_deed_matched_by_how_it_begins(self):
        exit_ = {'on': 'deed', 'to': 'b', 'match': {'detail': {'starts': 'stood watch at the ford'}}}
        self.assertTrue(rules.matches(exit_, {'kind': 'deed', 'detail': 'stood watch at the ford (d41)'}))
        self.assertFalse(rules.matches(exit_, {'kind': 'deed', 'detail': 'kept the ford (d42)'}))
        self.assertTrue(rules.matches({'on': 'gift', 'match': {'item': {'starts': 'rope'}}}, {'kind': 'gift', 'item': 'rope~fine'}))
        self.assertFalse(rules.matches({'on': 'gift', 'match': {'item': {'starts': 'rope~f'}}}, {'kind': 'gift', 'item': 'rope'}))

    def test_exits_missions_and_moments_take_them(self):
        import story_v2_samples as samples
        p = copy.deepcopy(samples.all_samples()[0])
        p['phases'][0]['exits'].append({'on': 'deed', 'to': p['phases'][0]['id'], 'match': {'detail': {'starts': 'stood'}, 'cell': {'in': ['a', 'b']}}})
        rules.validate(p)
        p['phases'][0]['exits'][-1]['match'] = {'detail': {'starts': 'x', 'in': ['y']}}
        self.assertRaisesRegex(ValueError, 'invalid event filter', rules.validate, p)
        self.assertEqual(rules.match_words({'in': ['a', 'b']}), 'one of a, b')
        self.assertEqual(rules.stand_in_match({'cell': {'in': ['a', 'b']}, 'detail': {'starts': 'x'}, 'item': 'rope'}),
                         {'cell': 'a', 'detail': 'x', 'item': 'rope'})
        self.assertEqual(rules.match_values({'in': ['a', 'b']}) + rules.match_values({'starts': 'x'}) + rules.match_values('c'), ['a', 'b', 'c'])
        weather = {'format': 'ratw-story-program', 'version': 1, 'id': 'w', 'title': 'W', 'start': 'a', 'audience': {'kind': 'players', 'players': ['p']},
                   'phases': [{'id': 'a', 'journal': 'A.', 'effects': [], 'exits': [{'on': 'weather changed', 'to': 'a', 'match': {'target': {'in': ['x', 'y']}}}]}],
                   'tests': []}
        self.assertEqual(rules.watch_of(weather).get('cells'), ['x', 'y'], 'each cell of an "in" is watched for its weather')


if __name__ == '__main__':
    unittest.main()
