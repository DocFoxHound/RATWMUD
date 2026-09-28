#!/usr/bin/env python3
"""Isolated Storykeeper tests: never starts the game or reads a game save."""
import copy
import http.client
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch
import uuid

import dm_service as dm


def fixture(now):
    def cell(identifier, chapter='', claims=()):
        return {'id': identifier, 'name': identifier.title(), 'x': 0 if identifier == 'source' else 4,
                'y': 0, 'z': 0, 'width': 4, 'height': 4, 'outdoors': True, 'terrain': ['....'] * 4,
                'territory': {'region': 'valley', 'claims': list(claims), 'chapter': chapter}}

    def actor(identifier, npc, role='', *, online=True, active=False):
        return {'id': identifier, 'name': identifier.title(), 'npc': npc, 'online': online, 'active': active,
                'cell': 'source', 'x': .5, 'y': .5, 'age': 18, 'activity': 'resting', 'role': role,
                'cash': 10 if npc else 20, 'stock': {'herbs': 2, 'meal': 0}, 'homeCell': 'source',
                'homeX': .5, 'homeY': .5, 'relocating': False, 'relocationTarget': '', 'lastActiveAtUnix': 0}

    actors = [actor('npc_scout', True, 'resident'), actor('npc_cook', True, 'cook'),
              actor('player-ash', False), actor('player-birch', False, online=False)]
    return {'version': 1, 'worldId': 'world-123', 'sequence': 1, 'generatedAtUnix': now,
            'calendarDays': .5, 'capabilities': sorted(dm.NATIVE),
            'cells': [cell('source', claims=['faction_one']), cell('destination')],
            'factions': [{'id': 'faction_one', 'name': 'Claimants', 'color': '#abcdef'}], 'chapters': [],
            'characters': actors,
            'accounts': [{'id': 'treasury', 'cash': 1000, 'stock': {'herbs': 100, 'meal': 50}}] +
                        [{key: actor[key] for key in ('id', 'cash', 'stock')} for actor in actors],
            'economy': {'minted': 1060, 'sunk': 0, 'ledger': []}}


# The Storykeeper keeps its state in PostgreSQL: every test gets an empty dm schema in a scratch database.
SCRATCH = None


def setUpModule():
    global SCRATCH
    import secrets
    import world_db
    from test_world_db import database_available, superuser
    if not database_available():
        raise unittest.SkipTest('local PostgreSQL not running (python3 tools/world_db.py up)')
    SCRATCH = f'ratw_test_dm_{secrets.token_hex(4)}'
    with superuser() as su:
        su.execute(f'CREATE DATABASE {SCRATCH} OWNER ratw_owner')
        su.execute(f'GRANT CONNECT ON DATABASE {SCRATCH} TO ratw_game')
    with world_db.connect('dev', 'owner', dbname=SCRATCH) as owner:
        world_db.migrate(owner)
    dm.DEFAULT_CONNECT = lambda: world_db.connect('dev', 'game', dbname=SCRATCH, options='-c search_path=dm')


def tearDownModule():
    from test_world_db import superuser
    if SCRATCH:
        with superuser() as su:
            su.execute(f'DROP DATABASE {SCRATCH} WITH (FORCE)')


def empty_state():
    import world_db
    with world_db.connect('dev', 'owner', dbname=SCRATCH) as owner:
        owner.execute('TRUNCATE dm.meta, dm.documents, dm.commands, dm.audit, dm.activity, dm.routes, dm.arrivals')


class ServiceFixture(unittest.TestCase):
    def setUp(self):
        empty_state()
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.now = 1790000000.
        self.service = dm.DMService(self.root / 'state', self.root / 'exchange', clock=lambda: self.now)
        self.snapshot = fixture(self.now)
        self.write_snapshot()
        self.service.tick()

    def tearDown(self):
        self.service.close()
        self.temp.cleanup()

    def write_snapshot(self, advance=0):
        self.now += advance
        self.snapshot['generatedAtUnix'] = self.now
        self.snapshot['sequence'] += 1
        dm.atomic_json(self.service.exchange_dir / 'snapshot.json', self.snapshot)

    def command(self, action, payload, identifier=None):
        return self.service.command({'id': identifier or uuid.uuid4().hex, 'action': action, 'payload': payload})['result']

    def chapter(self, identifier='chapter_one', **changes):
        data = {'id': identifier, 'name': 'Wayfarers', 'memberIds': ['player-ash'], 'cellIds': ['destination'],
                'housing': 3, 'jobCapacity': 2, 'attraction': 50, 'treatyModifier': .25}
        data.update(changes)
        return self.command('chapter.upsert', data)

    def event(self, kind='weather', payload=None, **changes):
        data = {'title': 'A deliberate event', 'kind': kind,
                'payload': payload if payload is not None else {'cell': 'source', 'preset': 'rain'}}
        data.update(changes)
        return self.command('event.create', data)

    def approve(self, event):
        return self.command('event.approve', {'eventId': event['id']})

    def receipt(self, event, ok=True, **changes):
        data = {'version': 1, 'id': event['id'], 'worldId': self.snapshot['worldId'], 'ok': ok,
                'detail': 'Authority result.', 'appliedAtUnix': self.now}
        data.update(changes)
        dm.atomic_json(self.service.inbox / (event['id'] + '.json'), data)

class ServiceTests(ServiceFixture):
    def test_snapshot_is_omniscient_but_explicit_allowlist(self):
        self.snapshot.update(providerKey='NEVER_RETURN', chat=['private dialogue'])
        self.snapshot['characters'][0]['draft'] = 'NEVER_RETURN'
        self.write_snapshot()
        state = self.service.state()
        self.assertTrue(state['bridge']['live'])
        self.assertEqual(len(state['snapshot']['characters']), 4)
        self.assertNotIn('NEVER_RETURN', dm.encode(state))
        self.assertNotIn('chat', state['snapshot'])

    def test_cached_snapshot_survives_restart_but_stays_read_only(self):
        draft = self.event()
        self.now += 11
        state = self.service.state()
        self.assertFalse(state['bridge']['live'])
        self.assertIsNotNone(state['snapshot'])
        self.service.close()
        self.service = dm.DMService(self.root / 'state', self.root / 'exchange', clock=lambda: self.now)
        self.assertFalse(self.service.state()['bridge']['live'])
        self.assertEqual(self.service.snapshot['worldId'], 'world-123')
        with self.assertRaises(dm.DMError):
            self.approve(draft)
        with self.assertRaises(dm.DMError):
            self.command('campaign.upsert', {'name': 'Must remain read-only'})
        self.assertEqual(self.service._get('event', draft['id'])['status'], 'draft')

    def test_changed_world_id_never_mixes_history(self):
        self.chapter()
        draft = self.event()
        self.snapshot['worldId'] = 'another-world'
        self.write_snapshot(1)
        state = self.service.state()
        self.assertFalse(state['bridge']['live'])
        self.assertEqual(state['snapshot']['worldId'], 'world-123')
        self.assertEqual(len(state['chapters']), 1)
        with self.assertRaises(dm.DMError):
            self.approve(draft)

    def test_future_and_rollback_snapshots_are_rejected(self):
        self.snapshot['generatedAtUnix'] = self.now + 1
        dm.atomic_json(self.service.exchange_dir / 'snapshot.json', self.snapshot)
        self.assertFalse(self.service.state()['bridge']['live'])
        self.snapshot['generatedAtUnix'] = self.now
        self.snapshot['sequence'] = 0
        dm.atomic_json(self.service.exchange_dir / 'snapshot.json', self.snapshot)
        self.assertFalse(self.service.state()['bridge']['live'])

    def test_malformed_snapshot_does_not_destroy_last_good(self):
        self.snapshot['characters'][0]['cash'] = True
        self.write_snapshot(1)
        state = self.service.state()
        self.assertFalse(state['bridge']['live'])
        self.assertEqual(state['snapshot']['characters'][0]['cash'], 10)

    def test_private_directories_and_symlinks_fail_closed(self):
        unsafe = self.root / 'unsafe'
        unsafe.mkdir(mode=0o755)
        with self.assertRaises(dm.DMError):
            dm.private_dir(unsafe)
        link = self.root / 'link'
        link.symlink_to(self.service.exchange_dir, target_is_directory=True)
        with self.assertRaises(dm.DMError):
            dm.private_dir(link)
        snapshot = self.service.exchange_dir / 'snapshot.json'
        snapshot.unlink()
        (self.service.state_dir / 'secret').write_text('{}')
        snapshot.symlink_to(self.service.state_dir / 'secret')
        self.assertFalse(self.service.state()['bridge']['live'])

    def test_campaign_beat_references_prevent_cascading_deletion(self):
        campaign = self.command('campaign.upsert', {'name': "Winter's Reach", 'description': 'A local plan.'})
        beat = self.command('beat.upsert', {'campaignId': campaign['id'], 'title': 'The road grows quiet', 'kind': 'brigands'})
        with self.assertRaises(dm.DMError):
            self.command('campaign.delete', {'campaignId': campaign['id']})
        self.command('beat.delete', {'beatId': beat['id']})
        self.command('campaign.delete', {'campaignId': campaign['id']})
        self.assertFalse(self.service._all('campaign'))

    def test_parameterized_text_and_strict_schema(self):
        title = "O'Brien'); DROP TABLE documents;--"
        campaign = self.command('campaign.upsert', {'name': title})
        self.assertEqual(self.service._get('campaign', campaign['id'])['name'], title)
        with self.assertRaises(dm.DMError):
            self.command('campaign.upsert', {'name': 'x', 'executor': 'shell'})
        with self.assertRaises(dm.DMError):
            self.command('chapter.upsert', {'name': 'x', 'memberIds': [], 'cellIds': [], 'housing': True,
                                           'jobCapacity': 1, 'attraction': 10})
        with self.assertRaises(dm.DMError):
            dm.decode('{"action":"a","action":"b"}')
        with self.assertRaises(dm.DMError):
            dm.decode('{"value":NaN}')
        with self.assertRaises(dm.DMError):
            self.command('campaign.upsert', {'id': False, 'name': 'Not an ID'})
        with self.assertRaises(dm.DMError):
            self.event('notice', {'scope': 'world', 'text': 'A notice', 'target': False})

    def test_command_idempotency_is_persistent_and_content_bound(self):
        identifier = 'same-request'
        first = self.command('campaign.upsert', {'name': 'Only once'}, identifier)
        second = self.command('campaign.upsert', {'name': 'Only once'}, identifier)
        self.assertEqual(first, second)
        self.service.close()
        self.service = dm.DMService(self.root / 'state', self.root / 'exchange', clock=lambda: self.now)
        self.assertEqual(first, self.command('campaign.upsert', {'name': 'Only once'}, identifier))
        self.assertEqual(len(self.service._all('campaign')), 1)
        with self.assertRaises(dm.DMError):
            self.command('campaign.upsert', {'name': 'Another request'}, identifier)

    def test_event_requires_explicit_approval_and_result(self):
        event = self.event()
        self.assertEqual(event['status'], 'draft')
        self.assertEqual(list(self.service.outbox.iterdir()), [])
        event = self.approve(event)
        self.assertEqual(event['status'], 'queued')
        envelope = dm.decode((self.service.outbox / (event['id'] + '.json')).read_bytes())
        self.assertEqual(envelope['worldId'], 'world-123')
        self.assertEqual(envelope['expiresAtUnix'] - envelope['createdAtUnix'], 120)
        self.assertEqual(self.service._get('event', event['id'])['status'], 'queued')
        self.receipt(event)
        self.service.tick()
        self.assertEqual(self.service._get('event', event['id'])['status'], 'applied')

    def test_native_rejection_is_failed_not_applied(self):
        event = self.approve(self.event())
        self.receipt(event, False, detail='Door or reach rule changed.')
        self.service.tick()
        self.assertEqual(self.service._get('event', event['id'])['status'], 'failed')

    def test_unsupported_story_executors_remain_blocked(self):
        for kind in dm.STORY_ONLY:
            event = self.event(kind, {'description': 'A possible campaign complication.'})
            self.assertEqual(event['status'], 'blocked')
            with self.assertRaises(dm.DMError):
                self.approve(event)
        self.assertEqual(list(self.service.outbox.iterdir()), [])

    def test_cancel_before_dispatch_only(self):
        event = self.event(scheduledAt=dm.utc(self.now + 60))
        event = self.approve(event)
        self.command('event.cancel', {'eventId': event['id']})
        self.write_snapshot(70)
        self.service.tick()
        self.assertEqual(list(self.service.outbox.iterdir()), [])
        queued = self.approve(self.event())
        with self.assertRaises(dm.DMError):
            self.command('event.cancel', {'eventId': queued['id']})

    def test_fixed_utc_scheduler_and_missed_window(self):
        event = self.approve(self.event(scheduledAt=dm.utc(self.now + 20)))
        self.assertEqual(event['status'], 'scheduled')
        self.write_snapshot(19)
        self.service.tick()
        self.assertFalse(list(self.service.outbox.iterdir()))
        self.write_snapshot(1)
        self.service.tick()
        self.assertEqual(self.service._get('event', event['id'])['status'], 'queued')
        missed = self.approve(self.event(scheduledAt=dm.utc(self.now + 5)))
        self.write_snapshot(130)
        self.service.tick()
        self.assertEqual(self.service._get('event', missed['id'])['status'], 'blocked')
        self.assertFalse((self.service.outbox / (missed['id'] + '.json')).exists())
        with self.assertRaises(dm.DMError):
            self.event(scheduledAt='2026-09-22T10:00:00-06:00')

    def test_stale_scheduler_defers_without_rescheduling(self):
        event = self.approve(self.event(scheduledAt=dm.utc(self.now + 12)))
        self.now += 15
        self.service.tick()
        stored = self.service._get('event', event['id'])
        self.assertEqual(stored['status'], 'scheduled')
        self.assertEqual(stored['scheduledAt'], event['scheduledAt'])
        self.assertFalse(list(self.service.outbox.iterdir()))

    def test_transactional_outbox_retries_same_envelope_after_restart(self):
        event = self.event()
        with patch.object(dm, 'atomic_json', side_effect=OSError('simulated disk issue')):
            event = self.approve(event)
        saved_envelope = self.service._get('event', event['id'])['envelope']
        self.assertFalse(list(self.service.outbox.iterdir()))
        self.service.close()
        self.service = dm.DMService(self.root / 'state', self.root / 'exchange', clock=lambda: self.now)
        self.service.tick()
        actual = dm.decode((self.service.outbox / (event['id'] + '.json')).read_bytes())
        self.assertEqual(saved_envelope, actual)
        self.assertEqual(len(self.service._all('event')), 1)

    def test_invalid_receipt_never_confirms_an_effect(self):
        event = self.approve(self.event())
        self.receipt(event, worldId='wrong-world')
        self.service.tick()
        self.assertEqual(self.service._get('event', event['id'])['status'], 'queued')
        self.receipt(event, appliedAtUnix=self.now + 1000)
        self.service.tick()
        self.assertEqual(self.service._get('event', event['id'])['status'], 'queued')
        self.now += 121
        self.service.tick()
        self.assertEqual(self.service._get('event', event['id'])['status'], 'failed')

    def test_chapter_notice_is_one_native_request_for_real_members(self):
        chapter = self.chapter(memberIds=['player-ash', 'player-birch'])
        event = self.approve(self.event('notice', {'scope': 'chapter', 'target': chapter['id'], 'text': 'A public bell sounds.'}))
        envelope = self.service._get('event', event['id'])['envelope']
        self.assertEqual(envelope['payload'], {'scope': 'players', 'targets': ['player-ash', 'player-birch'],
                                              'text': 'A public bell sounds.'})
        self.assertEqual(len(list(self.service.outbox.iterdir())), 1)

    def test_finite_economy_transfers_revalidate_accounts_and_balances(self):
        good = {'from': 'treasury', 'to': 'player-ash', 'item': 'meal', 'quantity': 2, 'coins': 5}
        self.assertEqual(self.approve(self.event('economy_transfer', good))['status'], 'queued')
        for change in ({'coins': 1001}, {'quantity': 51}, {'from': 'invented_account'}):
            with self.assertRaises(dm.DMError):
                self.approve(self.event('economy_transfer', good | change))
        for change in ({'coins': -1}, {'quantity': .5}, {'item': 'magic_sword'}, {'coins': True}):
            with self.assertRaises(dm.DMError):
                self.event('economy_transfer', good | change)

    def test_chapter_sites_are_explicit_nonoverlapping_operator_overlays(self):
        chapter = self.chapter()
        self.assertIn('Operator-declared', chapter['siteAuthority'])
        with self.assertRaises(dm.DMError):
            self.chapter('other_chapter')
        self.snapshot['cells'][1]['territory']['chapter'] = 'authored_chapter'
        self.snapshot['chapters'] = [{'id': 'authored_chapter', 'name': 'An authored Chapter'}]
        self.write_snapshot(1)
        with self.assertRaises(dm.DMError):
            self.chapter()

    def test_activity_uses_real_observations_and_no_gap_fill(self):
        chapter = self.chapter()
        self.assertIsNone(self.command('chapter.peak', {'chapterId': chapter['id']})['suggestion'])
        self.snapshot['characters'][2]['active'] = True
        self.write_snapshot(2)
        self.service.tick()  # First active observation creates no invented prior minutes.
        self.write_snapshot(6)
        self.service.tick()
        rows = self.service.state()['activity']
        self.assertAlmostEqual(sum(row['activeMinutes'] for row in rows), .1)
        peak = self.command('chapter.peak', {'chapterId': chapter['id']})
        self.assertGreater(dm.parse_utc(peak['suggestion']['scheduledAt']), self.now)
        self.snapshot['characters'][2]['cell'] = 'destination'
        self.write_snapshot(2)
        self.service.tick()
        self.assertEqual(self.service.state()['routes'][0]['transitions'], 1)
        minutes = sum(row['activeMinutes'] for row in self.service.state()['activity'])
        self.write_snapshot(3600)
        self.service.tick()
        self.assertAlmostEqual(sum(row['activeMinutes'] for row in self.service.state()['activity']), minutes)

    def test_opinions_require_known_targets_and_bounded_scores(self):
        self.chapter()
        opinion = self.command('opinion.set', {'factionId': 'faction_one', 'targetType': 'chapter',
                              'targetId': 'chapter_one', 'score': -25, 'reason': 'A declared diplomatic choice.'})
        self.assertEqual(opinion['score'], -25)
        for change in ({'score': -101}, {'targetId': 'unknown'}, {'score': True}):
            with self.assertRaises(dm.DMError):
                self.command('opinion.set', {'factionId': 'faction_one', 'targetType': 'chapter',
                             'targetId': 'chapter_one', 'score': 0, 'reason': 'test'} | change)

    def test_migration_preview_is_deterministic_and_protects_essential_roles(self):
        self.chapter()
        first = self.command('migration.preview', {'chapterId': 'chapter_one'})
        second = self.command('migration.preview', {'chapterId': 'chapter_one'})
        self.assertEqual(first['candidates'], second['candidates'])
        self.assertEqual([candidate['npcId'] for candidate in first['candidates']], ['npc_scout'])
        self.assertEqual(first['capacity']['available'], 2)
        self.assertEqual(list(self.service.outbox.iterdir()), [])

    def test_migration_acceptance_is_not_arrival_and_resentment_once_only(self):
        self.chapter()
        preview = self.command('migration.preview', {'chapterId': 'chapter_one'})
        candidate = preview['candidates'][0]
        event = self.command('migration.approve', {'previewId': preview['id'], 'npcId': candidate['npcId']})
        self.assertEqual(event['status'], 'queued')
        self.receipt(event)
        self.service.tick()
        self.assertFalse(self.service._all('opinion'))
        self.assertEqual(self.service._get('event', event['id'])['status'], 'queued')
        npc = self.snapshot['characters'][0]
        npc.update(cell='destination', homeCell='destination', homeX=candidate['x'], homeY=candidate['y'],
                   x=candidate['x'], y=candidate['y'])
        self.write_snapshot(2)
        self.service.tick()
        self.assertEqual(self.service._get('event', event['id'])['status'], 'applied')
        self.assertEqual(self.service._all('opinion')[0]['score'], -candidate['resentment'])
        self.service.tick()
        self.assertEqual(self.service._all('opinion')[0]['score'], -candidate['resentment'])
        self.chapter('chapter_two', cellIds=['source'])
        cooldown = self.command('migration.preview', {'chapterId': 'chapter_two'})
        self.assertFalse(cooldown['candidates'])

    def test_rejected_or_expired_migration_never_creates_resentment(self):
        self.chapter()
        preview = self.command('migration.preview', {'chapterId': 'chapter_one'})
        event = self.command('migration.approve', {'previewId': preview['id'], 'npcId': 'npc_scout'})
        self.receipt(event, False)
        self.service.tick()
        self.assertFalse(self.service._all('opinion'))
        preview = self.command('migration.preview', {'chapterId': 'chapter_one'})
        self.write_snapshot(301)
        with self.assertRaises(dm.DMError):
            self.command('migration.approve', {'previewId': preview['id'], 'npcId': 'npc_scout'})
        self.assertFalse(self.service._all('opinion'))

    def test_changed_preview_terms_and_capacity_require_review(self):
        self.chapter(housing=1, jobCapacity=1)
        preview = self.command('migration.preview', {'chapterId': 'chapter_one'})
        self.chapter(housing=1, jobCapacity=1, treatyModifier=1)
        with self.assertRaises(dm.DMError):
            self.command('migration.approve', {'previewId': preview['id'], 'npcId': 'npc_scout'})
        fresh = self.command('migration.preview', {'chapterId': 'chapter_one'})
        self.command('migration.approve', {'previewId': fresh['id'], 'npcId': 'npc_scout'})
        exhausted = self.command('migration.preview', {'chapterId': 'chapter_one'})
        self.assertEqual(exhausted['capacity']['available'], 0)
        self.assertFalse(exhausted['candidates'])

    def test_unassigned_or_different_region_never_invents_a_migration(self):
        self.chapter()
        for region in ('another_region', 'unassigned'):
            self.snapshot['cells'][0]['territory']['region'] = region
            self.write_snapshot(1)
            preview = self.command('migration.preview', {'chapterId': 'chapter_one'})
            self.assertFalse(preview['candidates'])

    def test_recruited_resident_is_not_a_migration_candidate(self):
        self.chapter()
        self.snapshot['characters'][0]['recruited'] = True
        self.write_snapshot(1)
        preview = self.command('migration.preview', {'chapterId': 'chapter_one'})
        self.assertFalse(preview['candidates'])
        with self.assertRaises(dm.DMError):
            self.approve(self.event('npc_relocate', {'npc': 'npc_scout', 'cell': 'destination', 'x': 1.5, 'y': 1.5}))

    def test_arrival_waits_for_fresh_authority_after_native_acceptance(self):
        self.chapter()
        preview = self.command('migration.preview', {'chapterId': 'chapter_one'})
        candidate = preview['candidates'][0]
        event = self.command('migration.approve', {'previewId': preview['id'], 'npcId': candidate['npcId']})
        self.receipt(event)
        self.service.tick()
        npc = self.snapshot['characters'][0]
        npc.update(homeCell='destination', homeX=candidate['x'], homeY=candidate['y'])
        self.write_snapshot(2)
        self.now += 11
        self.service.tick()
        self.assertEqual(self.service._get('event', event['id'])['status'], 'queued')
        self.assertFalse(self.service._all('opinion'))
        self.now += 600
        self.service.tick()
        self.assertEqual(self.service._get('event', event['id'])['status'], 'failed')
        self.assertFalse(self.service._all('opinion'))

    def test_preview_requires_known_capacity_but_never_fabricates_factions(self):
        self.chapter()
        self.snapshot['cells'][0]['territory']['claims'] = []
        self.write_snapshot(1)
        preview = self.command('migration.preview', {'chapterId': 'chapter_one'})
        self.assertEqual(preview['candidates'][0]['sourceFactions'], [])
        event = self.command('migration.approve', {'previewId': preview['id'], 'npcId': 'npc_scout'})
        self.receipt(event)
        self.service.tick()
        npc = self.snapshot['characters'][0]
        npc.update(homeCell='destination', homeX=event['payload']['x'], homeY=event['payload']['y'])
        self.write_snapshot(2)
        self.service.tick()
        self.assertEqual(self.service._get('event', event['id'])['status'], 'applied')
        self.assertFalse(self.service._all('opinion'))

    def test_declined_migration_draft_has_no_side_effects(self):
        event = self.event('npc_relocate', {'npc': 'npc_scout', 'cell': 'destination', 'x': 1.5, 'y': 1.5})
        self.command('event.cancel', {'eventId': event['id']})
        self.service.tick()
        self.assertEqual(self.service._get('event', event['id'])['status'], 'cancelled')
        self.assertFalse(list(self.service.outbox.iterdir()))
        self.assertFalse(self.service._all('opinion'))

    def test_stale_outbox_write_is_not_retried_until_live(self):
        event = self.event()
        with patch.object(dm, 'atomic_json', side_effect=OSError('temporarily unwritable')):
            event = self.approve(event)
        self.now += 11
        self.service.tick()
        self.assertFalse(list(self.service.outbox.iterdir()))
        self.write_snapshot()
        self.service.tick()
        self.assertTrue((self.service.outbox / (event['id'] + '.json')).exists())

    def test_late_claimed_success_is_not_accepted_after_expiry(self):
        event = self.approve(self.event())
        self.now += 121
        self.receipt(event, appliedAtUnix=self.now)
        self.service.tick()
        self.assertEqual(self.service._get('event', event['id'])['status'], 'failed')

    def test_activity_splits_hour_boundary_and_survives_restart_without_double_count(self):
        self.chapter()
        self.snapshot['characters'][2]['active'] = True
        self.now = (int(self.now) // 3600 + 1) * 3600 - 2
        self.write_snapshot()
        self.service.tick()
        self.write_snapshot(4)
        self.service.tick()
        rows = self.service.state()['activity']
        self.assertEqual(len(rows), 2)
        self.assertAlmostEqual(sum(row['activeMinutes'] for row in rows), 4 / 60)
        self.service.close()
        self.service = dm.DMService(self.root / 'state', self.root / 'exchange', clock=lambda: self.now)
        self.service.tick()
        self.assertAlmostEqual(sum(row['activeMinutes'] for row in self.service.state()['activity']), 4 / 60)


class HTTPTests(ServiceFixture):
    def setUp(self):
        super().setUp()
        self.token = 'test-only-random-token'
        self.server = dm.ThreadingHTTPServer(('127.0.0.1', 0), dm.handler_class(self.service, self.token, dm.ROOT / 'DM'))
        self.port = self.server.server_address[1]
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(5)
        super().tearDown()

    def http(self, method='GET', path='/api/state', body=None, headers=None):
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=5)
        defaults = {'Host': f'127.0.0.1:{self.port}', 'Authorization': 'Bearer ' + self.token}
        defaults.update(headers or {})
        conn.request(method, path, body=body, headers=defaults)
        result = conn.getresponse()
        data = result.read()
        code = result.status
        conn.close()
        return code, data

    def test_http_auth_host_origin_and_query_guards(self):
        self.assertEqual(self.http()[0], 200)
        for headers, expected in [({'Authorization': ''}, 401), ({'Authorization': 'Bearer bad'}, 401),
                                  ({'Host': 'evil.example'}, 403), ({'Origin': 'https://evil.example'}, 403),
                                  ({'Origin': 'null'}, 403)]:
            self.assertEqual(self.http(headers=headers)[0], expected)
        self.assertEqual(self.http(path='/api/state?token=' + self.token)[0], 400)
        self.assertEqual(self.http(path='/api/unknown', headers={'Authorization': ''})[0], 401)
        self.assertEqual(self.http(path='/../Saved/DM/session.json')[0], 404)
        self.assertEqual(self.http('OPTIONS', headers={'Authorization': ''})[0], 401)
        self.assertEqual(self.http(headers={'Authorization': 'Bearer café'})[0], 401)

    def test_http_post_schema_content_type_and_body_limits(self):
        command = {'id': 'http-once', 'action': 'campaign.upsert', 'payload': {'name': 'HTTP plan'}}
        body = dm.encode(command)
        self.assertEqual(self.http('POST', '/api/command', body, {'Content-Type': 'application/json'})[0], 200)
        self.assertEqual(self.http('POST', '/api/command', body, {'Content-Type': 'text/plain'})[0], 415)
        self.assertEqual(self.http('POST', '/api/command', 'x' * (dm.MAX_BODY + 1), {'Content-Type': 'application/json'})[0], 413)
        self.assertEqual(self.http('POST', '/api/command', '{"id":null}', {'Content-Type': 'application/json'})[0], 422)
        self.assertEqual(self.http('POST', '/api/command', '{"a":NaN}', {'Content-Type': 'application/json'})[0], 422)


if __name__ == '__main__':
    unittest.main(verbosity=2)
