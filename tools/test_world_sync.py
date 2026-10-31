#!/usr/bin/env python3
"""Pull and import with conflicts (tools/world_sync.py; doc 66, section 4): the three-way table, field-by-field
choices, pull and import end to end, maintenance, the release, players' data untouched, and the game server closing
for maintenance (exit 76) and refusing to start in it.

The comparison's own tests need nothing. The rest run against throwaway worlds made with world_db.py new (a live world
with Greyfen, and a dev world copied from it, its upstream) in a scratch registry, dropped afterwards; skipped without
the local PostgreSQL (and, for the server's, without a built build-core/ratw_server). DEV and PROD are never touched."""
import json
import os
from pathlib import Path
import secrets
import socket
import subprocess
import tempfile
import time
import unittest

import live_edit as L
import roster as R
import world_build
import world_db as W
import world_store as S
import world_sync as WS
from test_world_db import database_available, superuser
from test_world_store import greyfen

ROOT = Path(__file__).resolve().parent.parent
SERVER = ROOT / 'build-core' / 'ratw_server'


def snap(**tables):
    """A Snapshot from {table: {key: row}} (keys as tuples of one id here)."""
    out = WS.Snapshot({s.table: {} for s in WS.SYNCED})
    for table, rows in tables.items():
        out[table.replace('__', '.')] = {(k,): (WS.digest_of(r), r) for k, r in rows.items()}
    return out


def npc(name='Wren', greeting='Hello.', purse=0):
    return {'world_id': 'w', 'id': 'wren', 'name': name, 'greeting': greeting, 'purse': purse}


def unmigrate(key):
    """Takes migration 0055 back off a throwaway world: as DEV and PROD are until the user migrates them."""
    sql = (W.MIGRATIONS / '0053_postless_routes.sql').read_text(encoding='utf-8')
    with W.connect(key, 'owner') as conn:
        with conn.transaction():
            conn.execute('DROP TABLE live.business_settings')
            conn.execute('ALTER TABLE live.economy DROP COLUMN internal_store, DROP COLUMN store_markup, DROP COLUMN store_resale, '
                         'DROP COLUMN resale_markup, DROP COLUMN store_rule')
            conn.execute(sql)
            conn.execute("DELETE FROM public.schema_migrations WHERE version = '0055'")


class ThreeWayTests(unittest.TestCase):
    """Section 4's table, row by row, on NPC rows."""

    def plan(self, base, dev, live):
        """The one item for key 'wren' (or None) given its base, dev and live rows (None: no such row)."""
        rows = lambda r: {'live__npcs': {} if r is None else {'wren': r}}
        items = WS.three_way(snap(**rows(base)), snap(**rows(dev)), snap(**rows(live)))
        self.assertLessEqual(len(items), 1)
        return items[0] if items else None

    def test_dev_equals_live_is_nothing_to_do(self):
        self.assertIsNone(self.plan(npc(), npc(), npc()))
        self.assertIsNone(self.plan(npc(), npc(greeting='Hi.'), npc(greeting='Hi.')), 'both changed the same way')
        self.assertIsNone(self.plan(npc(), None, None), 'removed on both')
        self.assertIsNone(self.plan(None, npc(), npc()), 'added the same on both')

    def test_only_live_changed_takes_lives(self):
        for live in (npc(purse=5), None):
            item = self.plan(npc(), npc(), live)
            self.assertEqual(item.side, 'live')
            self.assertEqual(item.live, live)
        self.assertEqual(self.plan(None, None, npc()).side, 'live', 'added in live')

    def test_only_dev_changed_keeps_devs(self):
        for dev in (npc(greeting='Hi.'), None):
            self.assertEqual(self.plan(npc(), dev, npc()).side, 'dev')
        self.assertEqual(self.plan(None, npc(), None).side, 'dev', 'added in dev')

    def test_both_changed_differently_is_a_conflict_shown_field_by_field(self):
        item = self.plan(npc(), npc(name='Wren', greeting='Hi.'), npc(purse=5))
        self.assertEqual(item.side, 'conflict')
        shown = WS.describe(item)
        self.assertEqual((shown['id'], shown['dev'], shown['live'], shown['fieldChoices']), ('live.npcs/wren', 'changed', 'changed', True))
        self.assertEqual({f['field']: (f['base'], f['dev'], f['live'], f['changed']) for f in shown['fields']},
                         {'greeting': ('Hello.', 'Hi.', 'Hello.', 'dev'), 'purse': (0, 0, 5, 'live')})
        self.assertEqual(self.plan(None, npc(purse=1), npc(purse=2)).side, 'conflict', 'added differently on both')

    def test_removed_on_one_side_and_changed_on_the_other_is_a_conflict(self):
        for dev, live in ((None, npc(purse=5)), (npc(purse=5), None)):
            item = self.plan(npc(), dev, live)
            self.assertEqual(item.side, 'conflict')
            shown = WS.describe(item)
            self.assertEqual({shown['dev'], shown['live']}, {'removed', 'changed'})
            self.assertFalse(shown['fieldChoices'], 'a removed row is kept or not, as a whole')

    def test_choices(self):
        item = self.plan(npc(), npc(name='Wrenna', greeting='Hi.'), npc(name='Wren the Elder', purse=5))
        items = [item]
        with self.assertRaises(WS.Unresolved) as missing:
            WS.resolve(items, {})
        self.assertEqual([c['id'] for c in missing.exception.conflicts], ['live.npcs/wren'])
        with self.assertRaisesRegex(WS.PublishError, 'no longer there'):
            WS.resolve(items, {'live.npcs/wren': 'dev', 'live.npcs/gone': 'live'})
        self.assertEqual(WS.resolve(items, {'live.npcs/wren': 'dev'})['live.npcs/wren'], item.dev)
        self.assertEqual(WS.resolve(items, {'live.npcs/wren': 'live'})['live.npcs/wren'], item.live)
        # Field by field: the name, changed on both, must be chosen; greeting (dev's) and purse (live's) merge.
        with self.assertRaisesRegex(WS.PublishError, 'cannot be used') as unchosen:
            WS.resolve(items, {'live.npcs/wren': {}})
        self.assertIn('name', unchosen.exception.errors[0])
        merged = WS.resolve(items, {'live.npcs/wren': {'name': 'dev'}})['live.npcs/wren']
        self.assertEqual((merged['name'], merged['greeting'], merged['purse']), ('Wrenna', 'Hi.', 5))
        merged = WS.resolve(items, {'live.npcs/wren': {'name': 'live', 'purse': 'dev'}})['live.npcs/wren']
        self.assertEqual((merged['name'], merged['greeting'], merged['purse']), ('Wren the Elder', 'Hi.', 0))
        with self.assertRaisesRegex(WS.PublishError, 'cannot be used'):
            WS.resolve(items, {'live.npcs/wren': {'name': 'both'}})
        removed = self.plan(npc(), None, npc(purse=5))
        with self.assertRaisesRegex(WS.PublishError, 'cannot be used') as whole:
            WS.resolve([removed], {'live.npcs/wren': {'purse': 'live'}})
        self.assertIn('whole row', whole.exception.errors[0])

    def test_ids_and_long_values(self):
        self.assertEqual(WS.item_id('world.terrain_chunks', (0, -1)), 'world.terrain_chunks/0/-1')
        self.assertEqual(WS.item_id('live.dialects', ('a/b%',)), 'live.dialects/a%2Fb%25')
        self.assertEqual(WS.item_id('world.worlds', ()), 'world.worlds')
        long = WS.shown('.' * 1000)
        self.assertEqual(long['$long'], 1002)
        self.assertEqual(WS.shown('short'), 'short')

    def test_rows_compare_as_stored(self):
        import datetime
        row = {'a': b'\x00\x01', 'at': datetime.datetime(2026, 10, 30, tzinfo=datetime.timezone.utc), 'h': 8.0, 'j': {'x': [1, 2.5]}}
        once = WS.normalized(row)
        self.assertEqual(WS.normalized(WS.from_json(once)), once, 'a row read back from the base is the same row')


class LayoutTests(unittest.TestCase):
    """A generator's write and the map identity (doc 66, Progress): a new identity only for a new layout."""

    def test_the_layout_is_cells_interiors_and_links_not_people(self):
        p = {'cells': [{'id': 'c', 'terrain': ['..'], 'name': 'C', 'weather': 'rain'}],
             'rooms': [{'id': 'r', 'terrain': ['#'], 'description': 'A room.'}],
             'links': [{'id': 'l', 'kind': 'door', 'a': {'cell': 'c'}, 'b': {'cell': 'r'}, 'name': 'Door'}],
             'people': [{'id': 'wren', 'workLabel': 'baker'}]}
        fp = WS.layout_fingerprint(p)
        same = json.loads(json.dumps(p))
        same['people'][0]['workLabel'] = 'docker'
        same['rooms'][0]['description'] = 'Another room.'
        same['cells'][0]['weather'] = 'clear'
        same['links'].reverse()
        self.assertEqual(WS.layout_fingerprint(same), fp)
        for change in (lambda q: q['cells'][0].update(terrain=['.#']), lambda q: q['rooms'].append({'id': 'r2'}),
                       lambda q: q['links'][0]['b'].update(x=3)):
            other = json.loads(json.dumps(p))
            change(other)
            self.assertNotEqual(WS.layout_fingerprint(other), fp)

    def test_auto_keeps_only_a_stamp_of_the_same_layout(self):
        self.assertEqual(WS.map_for_layout({'id': 'm', 'layout': 'a'}, 'a'), 'keep')
        self.assertEqual(WS.map_for_layout({'id': 'm', 'layout': 'a'}, 'b'), 'new')
        self.assertEqual(WS.map_for_layout({'id': 'm'}, 'a'), 'new', 'a stamp from before fingerprints')
        self.assertEqual(WS.map_for_layout(None, 'a', 'same'), 'new', 'nothing to keep')
        self.assertEqual(WS.map_for_layout({'id': 'm'}, 'a', 'same'), 'keep')
        self.assertEqual(WS.map_for_layout({'id': 'm', 'layout': 'a'}, 'a', 'new'), 'new')


class ScratchWorlds:
    """A scratch registry for the class, a live world with Greyfen to copy from, and for each test a live world and a
    dev world copied from it (its upstream, so with its base)."""

    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        registry = Path(cls.folder.name) / 'worlds.json'
        registry.write_text(W.WORLDS_FILE.read_text(encoding='utf-8'), encoding='utf-8')
        cls.saved_env = dict(os.environ)
        os.environ.update({'RATW_WORLDS_FILE': str(registry), 'RATW_WORLDS_LOCAL': str(Path(cls.folder.name) / 'local.json')})
        W.ensure_roles()
        cls.tag = secrets.token_hex(3)
        cls.count = 0
        cls.seed = f's{cls.tag}_seed'
        W.new_world(cls.seed, 'Seed', 'live', log=lambda *_: None)
        with W.connect(cls.seed, 'owner') as conn:
            S.save_roster(conn, R.load())
            S.save_world(conn, greyfen(), None, create=True)

    @classmethod
    def tearDownClass(cls):
        with superuser() as su:
            su.execute(f'DROP DATABASE IF EXISTS ratw_{cls.seed} WITH (FORCE)')
        os.environ.clear()
        os.environ.update(cls.saved_env)
        cls.folder.cleanup()
        W.worlds()

    def setUp(self):
        type(self).count += 1
        self.live, self.dev = f's{self.tag}_l{self.count}', f's{self.tag}_d{self.count}'
        W.new_world(self.live, 'Live', 'live', source=self.seed, log=lambda *_: None)
        W.new_world(self.dev, 'Dev', 'dev', upstream_key=self.live, source=self.live, log=lambda *_: None)
        self.open = []

    def tearDown(self):
        for conn in self.open:
            conn.close()
        with superuser() as su:
            for key in (self.live, self.dev):
                su.execute(f'DROP DATABASE IF EXISTS ratw_{key} WITH (FORCE)')

    def conn(self, key, role='owner'):
        conn = W.connect(key, role)
        self.open.append(conn)
        return conn

    def sql(self, key, query, params=()):
        with W.connect(key, 'owner') as conn:
            cur = conn.execute(query, params)
            return cur.fetchall() if cur.description else None

    def one(self, key, query, params=()):
        return self.sql(key, query, params)[0][0]

    def game_data(self, key):
        """Every game.* table's rows, as text: players' data, the save, the event log, memories."""
        tables = [r[0] for r in self.sql(key, "SELECT table_name FROM information_schema.tables WHERE table_schema = 'game' "
                                              "AND table_type = 'BASE TABLE' ORDER BY 1")]
        return {t: self.sql(key, f'SELECT coalesce(string_agg(t::text, E\'\\n\' ORDER BY t::text), \'\') FROM game.{t} t')[0][0]
                for t in tables}

    def ids(self, items):
        return sorted(i['id'] for i in items)


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class PullTests(ScratchWorlds, unittest.TestCase):
    def test_a_copy_of_its_upstream_starts_with_its_base(self):
        status = WS.status(self.dev)
        self.assertEqual((status['base']['how'], status['base']['upstream'], status['base']['world']), ('copy', self.live, 'greyfen'))
        self.assertGreater(status['base']['rows'], 30)
        plan = WS.diff(self.dev)
        self.assertEqual(plan['counts'], {'live': 0, 'dev': 0, 'conflicts': 0})
        with self.assertRaisesRegex(WS.PublishError, 'no upstream'):
            WS.diff('dev-old')                 # (The old map's archive: a side project now.)
        with self.assertRaisesRegex(WS.PublishError, 'live world'):
            WS.diff(self.live)

    def test_pull_takes_lives_changes_and_keeps_devs(self):
        self.sql(self.live, "UPDATE live.npcs SET purse = 77 WHERE id = 'wren'")
        self.sql(self.live, "INSERT INTO live.factions (world_id, id, name) VALUES ('greyfen', 'wardens', 'Wardens')")
        self.sql(self.live, "INSERT INTO live.npc_state (world_id, npc_id, state) VALUES ('greyfen', 'wren', '{\"cash\": 3}')")
        self.sql(self.live, "UPDATE live.economy SET treasury = 999")
        self.sql(self.dev, "UPDATE live.npcs SET greeting = 'Dev hello.' WHERE id = 'sloe'")
        plan = WS.diff(self.dev)
        self.assertEqual(self.ids(plan['liveChanges']), ['live.factions/wardens', 'live.npcs/wren'])
        self.assertEqual(self.ids(plan['devChanges']), ['live.npcs/sloe'])
        self.assertEqual(plan['conflicts'], [])
        self.assertEqual(plan['summary']['live']['live'], {'NPCs': {'added': 0, 'changed': 1, 'removed': 0},
                                                          'factions': {'added': 1, 'changed': 0, 'removed': 0}})
        self.assertTrue(plan['warnings'], 'the live server may be running')
        game = self.game_data(self.dev)
        seq = self.one(self.dev, 'SELECT coalesce(max(seq), 0) FROM world.edits')
        revision = self.one(self.dev, 'SELECT revision FROM world.worlds')
        result = WS.pull(self.dev, editor='Ada')
        self.assertEqual(result['applied']['live']['NPCs']['changed'], 1)
        self.assertEqual(self.one(self.dev, "SELECT purse FROM live.npcs WHERE id = 'wren'"), 77)
        self.assertEqual(self.one(self.dev, "SELECT name FROM live.factions WHERE id = 'wardens'"), 'Wardens')
        self.assertEqual(self.one(self.dev, "SELECT greeting FROM live.npcs WHERE id = 'sloe'"), 'Dev hello.')
        self.assertNotEqual(self.one(self.live, "SELECT greeting FROM live.npcs WHERE id = 'sloe'"), 'Dev hello.', 'live untouched')
        self.assertEqual(self.sql(self.dev, 'SELECT count(*) FROM live.npc_state')[0][0], 0, 'running state is not synced')
        self.assertNotEqual(self.one(self.dev, 'SELECT treasury FROM live.economy'), 999, 'nor the economy')
        self.assertEqual(self.game_data(self.dev), game, "players' data untouched")
        # Open Atlas sessions reload; a new build is due.
        self.assertGreater(self.one(self.dev, 'SELECT revision FROM world.worlds'), revision)
        with W.connect(self.dev, 'editor') as editor:
            self.assertTrue(L.sync(editor, 'client', 'Bo', '#aabbcc', seq, {})['reload'])
        after = WS.diff(self.dev)
        self.assertEqual((after['counts'], after['base']['how']), ({'live': 0, 'dev': 1, 'conflicts': 0}, 'pull'))

    def test_business_settings_are_pulled_and_the_store_switches_are_not(self):
        # Doc 67, 6: a business's store room switch is authored and travels; the world's switches and markups
        # (live.economy) are each world's own lever.
        self.sql(self.live, "INSERT INTO live.business_settings (world_id, npc_id, internal_store) VALUES ('greyfen', 'brann', false)")
        self.sql(self.live, 'UPDATE live.economy SET store_markup = 10, store_rule = true')
        plan = WS.diff(self.dev)
        self.assertEqual(self.ids(plan['liveChanges']), ['live.business_settings/brann'])
        self.assertEqual(plan['summary']['live']['live'], {'business settings': {'added': 1, 'changed': 0, 'removed': 0}})
        WS.pull(self.dev, editor='Ada')
        self.assertEqual(self.sql(self.dev, 'SELECT npc_id, internal_store FROM live.business_settings'), [('brann', False)])
        self.assertEqual(self.sql(self.dev, 'SELECT store_markup, store_rule FROM live.economy'), [(100, False)], 'not the economy')
        with W.connect(self.dev, 'editor') as editor:
            self.assertEqual(S.load_world(editor, 'greyfen')[0]['economy']['businesses'], {'brann': {'internalStore': False}})

    def test_a_world_without_the_business_settings_counts_as_having_none(self):
        # A live world not yet migrated to 0055 (as PROD is): the table missing on one side is no rows on both, and
        # pulls and imports go on as before; the other side's rows are left as they are.
        self.sql(self.dev, "INSERT INTO live.business_settings (world_id, npc_id, internal_store) VALUES ('greyfen', 'wren', false)")
        unmigrate(self.live)
        self.sql(self.live, "UPDATE live.npcs SET purse = 66 WHERE id = 'wren'")
        plan = WS.diff(self.dev)
        self.assertEqual(self.ids(plan['liveChanges']), ['live.npcs/wren'])
        self.assertEqual(plan['devChanges'], [], 'the dev world\'s business settings are not compared')
        WS.pull(self.dev, editor='Ada')
        self.assertEqual(self.one(self.dev, "SELECT purse FROM live.npcs WHERE id = 'wren'"), 66)
        self.assertEqual(self.sql(self.dev, 'SELECT npc_id FROM live.business_settings'), [('wren',)], 'kept')
        self.sql(self.dev, "UPDATE live.npcs SET greeting = 'Hi from dev.' WHERE id = 'sloe'")
        WS.maintenance(self.live, True, 'Ada', 'Import')
        result = WS.import_(self.dev, {}, 'password', 'Ada')
        self.assertEqual(result['summary']['live'], {'NPCs': {'added': 0, 'changed': 1, 'removed': 0}})
        self.assertEqual(self.sql(self.dev, 'SELECT npc_id FROM live.business_settings'), [('wren',)], 'still kept after the import')
        self.assertEqual(WS.diff(self.dev)['counts'], {'live': 0, 'dev': 0, 'conflicts': 0})

    def test_conflicts_field_by_field(self):
        self.sql(self.dev, "UPDATE live.npcs SET name = 'Wrenna', greeting = 'Dev greeting.' WHERE id = 'wren'")
        self.sql(self.live, "UPDATE live.npcs SET name = 'Wren the Elder', purse = 5 WHERE id = 'wren'")
        plan = WS.diff(self.dev)
        self.assertEqual(self.ids(plan['conflicts']), ['live.npcs/wren'])
        fields = {f['field']: f['changed'] for f in plan['conflicts'][0]['fields']}
        self.assertEqual(fields, {'name': 'conflict', 'greeting': 'dev', 'purse': 'live'})
        with self.assertRaises(WS.Unresolved):
            WS.pull(self.dev)
        with self.assertRaisesRegex(WS.PublishError, 'no longer there'):
            WS.pull(self.dev, {'live.npcs/wren': 'dev', 'live.npcs/nobody': 'live'})
        with self.assertRaisesRegex(WS.PublishError, 'changed since'):
            WS.pull(self.dev, {'live.npcs/wren': 'dev'}, expect='0' * 40)
        WS.pull(self.dev, {'live.npcs/wren': {'name': 'live'}}, expect=plan['digest'])
        self.assertEqual(self.sql(self.dev, "SELECT name, greeting, purse FROM live.npcs WHERE id = 'wren'")[0],
                         ('Wren the Elder', 'Dev greeting.', 5))
        after = WS.diff(self.dev)
        self.assertEqual((self.ids(after['devChanges']), after['conflicts']), (['live.npcs/wren'], []),
                         "dev's greeting is still dev's change, for the import")

    def test_removed_against_changed(self):
        self.sql(self.dev, "DELETE FROM live.npcs WHERE id = 'sloe'")
        self.sql(self.live, "UPDATE live.npcs SET greeting = 'Still here.' WHERE id = 'sloe'")
        self.sql(self.live, "DELETE FROM live.npcs WHERE id = 'rook'")
        self.sql(self.dev, "UPDATE live.npcs SET greeting = 'Changed in dev.' WHERE id = 'rook'")
        plan = WS.diff(self.dev)
        conflicts = {c['id']: (c['dev'], c['live']) for c in plan['conflicts']}
        self.assertEqual(conflicts, {'live.npcs/sloe': ('removed', 'changed'), 'live.npcs/rook': ('changed', 'removed')})
        with self.assertRaisesRegex(WS.PublishError, 'cannot be used'):
            WS.pull(self.dev, {'live.npcs/sloe': {'greeting': 'live'}, 'live.npcs/rook': 'dev'})
        WS.pull(self.dev, {'live.npcs/sloe': 'live', 'live.npcs/rook': 'dev'})
        self.assertEqual(self.one(self.dev, "SELECT greeting FROM live.npcs WHERE id = 'sloe'"), 'Still here.', 'back in dev')
        self.assertEqual(self.one(self.dev, "SELECT greeting FROM live.npcs WHERE id = 'rook'"), 'Changed in dev.')
        after = WS.diff(self.dev)
        self.assertEqual([(c['id'], c['dev']) for c in after['devChanges']], [('live.npcs/rook', 'added')],
                         'kept in dev: an import brings it back to live')

    def test_a_first_base(self):
        self.sql(self.dev, 'DELETE FROM admin.sync_base_rows')
        self.sql(self.dev, 'DELETE FROM admin.sync_base')
        with self.assertRaisesRegex(WS.PublishError, 'no base'):
            WS.diff(self.dev)
        self.sql(self.dev, "UPDATE live.npcs SET greeting = 'Dev.' WHERE id = 'sloe'")
        self.sql(self.live, "UPDATE live.npcs SET purse = 3 WHERE id = 'wren'")
        WS.init_base(self.dev, 'Ada')
        plan = WS.diff(self.dev)
        self.assertEqual((self.ids(plan['devChanges']), plan['liveChanges']), (['live.npcs/sloe', 'live.npcs/wren'], []),
                         "taken from live now: every difference counts as dev's")
        WS.init_base(self.dev, 'Ada', prefer='live')
        plan = WS.diff(self.dev)
        self.assertEqual((self.ids(plan['liveChanges']), plan['devChanges']), (['live.npcs/sloe', 'live.npcs/wren'], []))
        self.assertEqual(WS.status(self.dev)['base']['how'], 'init')

    def test_routes_are_one_unit_with_their_posts(self):
        route = self.one(self.live, 'SELECT id FROM live.patrol_routes ORDER BY id LIMIT 1')
        self.sql(self.live, 'UPDATE live.patrol_posts SET x = x + 1 WHERE route_id = %s AND seq = 0', (route,))
        plan = WS.diff(self.dev)
        self.assertEqual(self.ids(plan['liveChanges']), [f'live.patrol_routes/{route}'])
        self.assertEqual([f['field'] for f in plan['liveChanges'][0]['fields']], ['posts'])
        WS.pull(self.dev)
        self.assertEqual(self.sql(self.dev, 'SELECT area, x, y FROM live.patrol_posts WHERE route_id = %s ORDER BY seq', (route,)),
                         self.sql(self.live, 'SELECT area, x, y FROM live.patrol_posts WHERE route_id = %s ORDER BY seq', (route,)))


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class ImportTests(ScratchWorlds, unittest.TestCase):
    def test_import_needs_maintenance_and_the_password_and_records_a_release(self):
        self.sql(self.dev, "UPDATE live.npcs SET greeting = 'From dev.' WHERE id = 'sloe'")
        self.sql(self.dev, "UPDATE world.interiors SET name = 'The Old Shop' WHERE id = 'shop'")
        self.sql(self.live, "INSERT INTO game.checkpoints (world_id, schema_version, revision, payload) VALUES ('greyfen', 1, 4, '{\"x\": 1}')")
        self.sql(self.live, "INSERT INTO live.npc_state (world_id, npc_id, state) VALUES ('greyfen', 'wren', '{\"cash\": 3}')")
        game = self.game_data(self.live)
        with self.assertRaises(WS.NotInMaintenance):
            WS.import_(self.dev, {}, 'password', 'Ada')
        plan = WS.plan_import(self.dev)
        self.assertTrue(any('not in maintenance' in b for b in plan['blocked']))
        self.assertEqual(self.ids(plan['devChanges']), ['live.npcs/sloe', 'world.interiors/shop'])
        listener = self.conn(self.live, 'game')
        listener.execute('LISTEN ratw_maintenance')
        listener.execute('LISTEN ratw_release')
        status = WS.maintenance(self.live, True, 'Ada', 'Adding a shop')
        self.assertEqual((status['server']['state'], status['server']['by'], status['server']['note']), ('maintenance', 'Ada', 'Adding a shop'))
        note = next(listener.notifies(timeout=2, stop_after=1))
        self.assertEqual((note.channel, json.loads(note.payload)['state']), ('ratw_maintenance', 'maintenance'))
        self.assertEqual(WS.plan_import(self.dev)['blocked'], [])
        with self.assertRaises(WS.WrongPassword):
            WS.import_(self.dev, {}, 'guess', 'Ada')
        result = WS.import_(self.dev, {}, 'password', 'Ada', 'Older shop sign')
        self.assertEqual(result['release'], 1)
        self.assertEqual(result['summary'], {'world': {'interiors': {'added': 0, 'changed': 1, 'removed': 0}},
                                             'live': {'NPCs': {'added': 0, 'changed': 1, 'removed': 0}}})
        self.assertEqual(self.one(self.live, "SELECT greeting FROM live.npcs WHERE id = 'sloe'"), 'From dev.')
        self.assertEqual(self.one(self.live, "SELECT name FROM world.interiors WHERE id = 'shop'"), 'The Old Shop')
        note = next(listener.notifies(timeout=2, stop_after=1))
        self.assertEqual((note.channel, json.loads(note.payload)['release']), ('ratw_release', 1))
        release = self.sql(self.live, "SELECT number, kind, pushed_by, note, summary - 'map', content IS NOT NULL, summary->'map'->>'id' "
                                      'FROM admin.releases')
        self.assertEqual(release, [(1, 'import', 'Ada', 'Older shop sign', result['summary'], True, 'original:greyfen')],
                         'the release records the map live is on')
        self.assertEqual(self.one(self.live, 'SELECT count(*) FROM world.builds WHERE release = 1'), 1, 'and its game build')
        self.assertEqual(self.game_data(self.live), game, "players' data untouched")
        self.assertEqual(self.one(self.live, 'SELECT count(*) FROM live.npc_state'), 1)
        after = WS.diff(self.dev)
        self.assertEqual((after['counts'], after['base']['how'], after['base']['release']),
                         ({'live': 0, 'dev': 0, 'conflicts': 0}, 'import', 1))
        # Rollback is as it was: the authored content of a release.
        self.sql(self.dev, "UPDATE world.interiors SET name = 'The New Shop' WHERE id = 'shop'")
        self.assertEqual(WS.import_(self.dev, {}, 'password', 'Ada')['release'], 2)
        self.assertEqual(WS.Sync().rollback(self.live, 1, 'password', 'Ada')['release'], 3)
        self.assertEqual(self.one(self.live, "SELECT name FROM world.interiors WHERE id = 'shop'"), 'The Old Shop')
        self.assertEqual(self.one(self.live, 'SELECT kind FROM admin.releases WHERE number = 3'), 'rollback')

    def test_business_settings_are_imported(self):
        self.sql(self.dev, "INSERT INTO live.business_settings (world_id, npc_id, internal_store) VALUES ('greyfen', 'wren', false)")
        self.sql(self.dev, 'UPDATE live.economy SET resale_markup = 50')
        WS.maintenance(self.live, True, 'Ada', 'Import')
        result = WS.import_(self.dev, {}, 'password', 'Ada')
        self.assertEqual(result['summary']['live'], {'business settings': {'added': 1, 'changed': 0, 'removed': 0}})
        self.assertEqual(self.sql(self.live, 'SELECT npc_id, internal_store FROM live.business_settings'), [('wren', False)])
        self.assertEqual(self.one(self.live, 'SELECT resale_markup FROM live.economy'), 75, 'the world\'s markups stay its own')
        manifest = self.one(self.live, "SELECT live.people_manifest('greyfen')")
        self.assertIn('nostore "wren"', manifest.split('\n'))

    def test_conflicts_are_resolved_at_import_and_dev_ends_level_with_live(self):
        self.sql(self.dev, "UPDATE live.npcs SET greeting = 'Dev greeting.' WHERE id = 'sloe'")
        self.sql(self.dev, "UPDATE live.npcs SET name = 'Wrenna' WHERE id = 'wren'")
        WS.maintenance(self.live, True, 'Ada')
        self.sql(self.live, "UPDATE live.npcs SET purse = 9 WHERE id = 'harrow'")         # (The DM, during maintenance.)
        self.sql(self.live, "UPDATE live.npcs SET name = 'Wren the Elder' WHERE id = 'wren'")
        with self.assertRaises(WS.Unresolved) as unresolved:
            WS.import_(self.dev, {}, 'password', 'Ada')
        self.assertEqual([c['id'] for c in unresolved.exception.conflicts], ['live.npcs/wren'])
        result = WS.import_(self.dev, {'live.npcs/wren': 'live'}, 'password', 'Ada')
        self.assertEqual(result['conflicts'], 1)
        self.assertEqual(self.one(self.live, "SELECT greeting FROM live.npcs WHERE id = 'sloe'"), 'Dev greeting.')
        self.assertEqual(self.one(self.live, "SELECT name FROM live.npcs WHERE id = 'wren'"), 'Wren the Elder')
        self.assertEqual(self.one(self.dev, "SELECT purse FROM live.npcs WHERE id = 'harrow'"), 9, "dev takes live's changes")
        self.assertEqual(self.one(self.dev, "SELECT name FROM live.npcs WHERE id = 'wren'"), 'Wren the Elder', 'and the choice')
        self.assertGreaterEqual(result['dev']['updated'], 2)
        self.assertEqual(WS.diff(self.dev)['counts'], {'live': 0, 'dev': 0, 'conflicts': 0})

    def test_import_waits_for_the_game_server_to_stop(self):
        WS.maintenance(self.live, True, 'Ada')
        self.sql(self.dev, "UPDATE live.npcs SET greeting = 'Dev.' WHERE id = 'sloe'")
        server = self.conn(self.live, 'game')
        server.execute('SELECT pg_advisory_lock(%s::int, %s::int)', WS.OWNER_LOCK)     # (As a running game server holds it.)
        self.assertTrue(WS.status(self.live)['serverRunning'])
        self.assertTrue(any('not stopped' in b for b in WS.plan_import(self.dev)['blocked']))
        with self.assertRaisesRegex(WS.PublishError, 'still running'):
            WS.import_(self.dev, {}, 'password', 'Ada')
        self.assertEqual(self.one(self.live, 'SELECT count(*) FROM admin.releases'), 0)
        server.execute('SELECT pg_advisory_unlock_all()')
        self.assertEqual(WS.import_(self.dev, {}, 'password', 'Ada')['release'], 1)

    def test_import_refuses_to_strand_live_npcs(self):
        # Dev removes the cottage (its two residents unplaced first); live moves someone's evenings there.
        for column in ('home_area', 'work_area', 'evening_area'):
            self.sql(self.dev, f"UPDATE live.npcs SET {column} = NULL WHERE {column} = 'cottage'")
            self.sql(self.dev, f"UPDATE live.profession_slots SET {column} = NULL WHERE {column} = 'cottage'")
        for table, column in (('world.places', 'area'), ('world.resources', 'area'), ('live.faction_claims', 'area'),
                              ('live.npc_areas', 'area'), ('live.patrol_posts', 'area')):
            self.sql(self.dev, f"DELETE FROM {table} WHERE {column} = 'cottage'")
        self.sql(self.dev, "DELETE FROM world.links WHERE a_area = 'cottage' OR b_area = 'cottage'")
        self.sql(self.dev, "DELETE FROM world.areas WHERE id = 'cottage'")
        self.sql(self.live, "UPDATE live.npcs SET evening_area = 'cottage', evening_x = 1, evening_y = 1 WHERE id = 'rook'")
        WS.maintenance(self.live, True, 'Ada')
        with self.assertRaises(WS.PublishError) as refused:
            WS.import_(self.dev, {}, 'password', 'Ada')
        self.assertTrue(any('rook' in e and 'cottage' in e for e in refused.exception.errors), refused.exception.errors)
        self.assertEqual(self.one(self.live, "SELECT count(*) FROM world.interiors WHERE id = 'cottage'"), 1, 'live unchanged')
        self.assertEqual(self.one(self.live, 'SELECT count(*) FROM admin.releases'), 0)

    def test_maintenance_off_is_announced_too(self):
        listener = self.conn(self.live, 'game')
        listener.execute('LISTEN ratw_maintenance')
        WS.maintenance(self.live, True, 'Ada')
        WS.maintenance(self.live, False, 'Ada')
        states = [json.loads(n.payload)['state'] for n in listener.notifies(timeout=2, stop_after=2)]
        self.assertEqual(states, ['maintenance', 'running'])
        self.assertEqual(WS.status(self.live)['server']['state'], 'running')
        with W.connect(self.live, 'dm') as dm:
            self.assertEqual(WS.server_state(dm)['state'], 'running', 'the DM reads it')


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class MapTests(ScratchWorlds, unittest.TestCase):
    """The same map (doc 66): a map identity per world; pull only between worlds on one map; an import of another map
    only with the replace-the-map confirmation (and live's migrations), live data never erased."""

    def new_map(self, key, name='Greyfen the Second'):
        with W.connect(key, 'owner') as conn:
            with conn.transaction():
                return WS.stamp_map(conn, name, 'Ada', 'generated')

    def take_the_cottage_away(self):
        """Dev's own map change: the cottage goes (its residents unplaced in dev first, as Atlas would)."""
        for column in ('home_area', 'work_area', 'evening_area'):
            self.sql(self.dev, f"UPDATE live.npcs SET {column} = NULL WHERE {column} = 'cottage'")
            self.sql(self.dev, f"UPDATE live.profession_slots SET {column} = NULL WHERE {column} = 'cottage'")
        for table, column in (('world.places', 'area'), ('world.resources', 'area'), ('live.faction_claims', 'area'),
                              ('live.npc_areas', 'area'), ('live.patrol_posts', 'area')):
            self.sql(self.dev, f"DELETE FROM {table} WHERE {column} = 'cottage'")
        self.sql(self.dev, "DELETE FROM world.links WHERE a_area = 'cottage' OR b_area = 'cottage'")
        self.sql(self.dev, "DELETE FROM world.areas WHERE id = 'cottage'")

    def test_a_generators_write_keeps_its_map_while_its_layout_is_the_same(self):
        project = {'cells': [{'id': 'c', 'x': 0, 'y': 0, 'width': 2, 'height': 1, 'terrain': ['..'], 'name': 'C'}],
                   'rooms': [], 'links': [], 'spawn': {'cell': 'c', 'x': 0, 'y': 0}, 'people': [{'id': 'wren'}]}

        def write(p, mode='auto'):
            with W.connect(self.dev, 'owner') as conn:
                with conn.transaction():
                    return WS.stamp_for_layout(conn, WS.layout_fingerprint(p), 'Generated', 'Ada', 'generated', mode)
        first, how = write(project)
        self.assertEqual(how, 'new')
        self.assertEqual(WS.status(self.dev)['map']['id'], first['id'])
        people_only = dict(project, people=[], cells=[dict(project['cells'][0], name='Renamed')])
        self.assertEqual(write(people_only), (first, 'kept'), 'people and names are not the layout')
        self.assertEqual(WS.status(self.dev)['map']['id'], first['id'])
        moved = dict(project, cells=[dict(project['cells'][0], terrain=['.#'])])
        self.assertEqual(write(moved, 'same')[1], 'kept', '--same-map, whatever the layout')
        self.assertEqual(write(moved)[1], 'kept', 'and the kept stamp records the layout it was told is the same map')
        second, how = write(project)
        self.assertEqual(how, 'new', 'a changed layout is a new map')
        self.assertNotEqual(second['id'], first['id'])
        self.assertEqual(write(project, 'new')[1], 'new', '--new-map, whatever the layout')

    def test_a_copy_is_on_its_upstreams_map_and_ordinary_edits_keep_it(self):
        dev, live = WS.status(self.dev)['map'], WS.status(self.live)['map']
        self.assertEqual((dev['id'], live['id'], dev['from']), ('original:greyfen', 'original:greyfen', 'original'))
        self.sql(self.dev, "UPDATE world.interiors SET name = 'The Old Shop' WHERE id = 'shop'")    # (An Atlas edit.)
        self.sql(self.live, "UPDATE live.npcs SET purse = 12 WHERE id = 'wren'")
        plan = WS.diff(self.dev)
        self.assertTrue(plan['maps']['same'])
        self.assertFalse(plan['replacesMap'])
        WS.pull(self.dev, editor='Ada')
        self.assertEqual(self.one(self.dev, "SELECT purse FROM live.npcs WHERE id = 'wren'"), 12, 'the same map pulls as ever')
        self.assertEqual(WS.status(self.dev)['map']['id'], 'original:greyfen', 'and keeps its map')

    def test_pull_is_refused_between_two_maps(self):
        stamp = self.new_map(self.dev)
        self.assertEqual(WS.status(self.dev)['map'], stamp)
        self.sql(self.live, "UPDATE live.npcs SET purse = 12 WHERE id = 'wren'")
        revision = self.one(self.dev, 'SELECT revision FROM world.worlds')
        for attempt in (lambda: WS.diff(self.dev), lambda: WS.pull(self.dev, editor='Ada')):
            with self.assertRaises(WS.DifferentMap) as refused:
                attempt()
            self.assertEqual(refused.exception.status, 409)
            message = str(refused.exception)
            self.assertIn("on a different map (Greyfen Crossing's original map)", message)
            self.assertIn('(Greyfen the Second)', message)
            self.assertIn('nothing to update from', message)
            self.assertEqual((refused.exception.maps['dev']['id'], refused.exception.maps['live']['id'], refused.exception.maps['same']),
                             (stamp['id'], 'original:greyfen', False))
        self.assertNotEqual(self.one(self.dev, "SELECT purse FROM live.npcs WHERE id = 'wren'"), 12, 'nothing pulled')
        self.assertEqual(self.one(self.dev, 'SELECT revision FROM world.worlds'), revision)

    def test_an_import_of_another_map_needs_the_confirmation_and_never_erases_live_data(self):
        stamp = self.new_map(self.dev)
        self.take_the_cottage_away()
        self.sql(self.dev, "UPDATE world.interiors SET name = 'The New Shop' WHERE id = 'shop'")
        sloe = self.sql(self.live, "SELECT name, greeting FROM live.npcs WHERE id = 'sloe'")
        self.sql(self.dev, "DELETE FROM live.npcs WHERE id = 'sloe'")                  # (Not on the new map's roster.)
        # Live, meanwhile: a newcomer, someone's evenings moved into the cottage, a players' save.
        self.sql(self.live, "INSERT INTO live.npcs (world_id, id, position, name, role, description, greeting, personality, backstory, "
                            "work_label, age, voice, appearance, paid, purse, herbs, meals, hours_start, hours_end, home_area, home_x, "
                            "home_y, work_area, work_x, work_y, evening_area, evening_x, evening_y, origin) SELECT world_id, 'born_x', "
                            "999, 'Briar', role, description, greeting, personality, backstory, work_label, 0, voice, appearance, paid, "
                            "0, 0, 1, hours_start, hours_end, 'cottage', 1, 1, 'cottage', 1, 1, 'cottage', 1, 1, 'runtime' "
                            "FROM live.npcs WHERE id = 'wren'")
        self.sql(self.live, "UPDATE live.npcs SET evening_area = 'cottage', evening_x = 1, evening_y = 1 WHERE id = 'rook'")
        self.sql(self.live, "INSERT INTO game.checkpoints (world_id, schema_version, revision, payload) VALUES ('greyfen', 1, 4, '{\"x\": 1}')")
        game = self.game_data(self.live)
        WS.maintenance(self.live, True, 'Ada', 'A new map')
        plan = WS.plan_import(self.dev)
        self.assertTrue(plan['replacesMap'])
        self.assertEqual((plan['maps']['dev']['id'], plan['maps']['live']['id']), (stamp['id'], 'original:greyfen'))
        self.assertEqual(plan['blocked'], [], 'nothing blocks it but the confirmation')
        self.assertTrue(any('replaces' in w for w in plan['warnings']))
        self.assertIn('live.npcs/sloe', self.ids(plan['liveChanges']), 'removed in dev, kept live')
        self.assertIn('world.interiors/cottage', self.ids(plan['devChanges']), "the map is dev's")
        self.assertGreaterEqual(plan['letGo']['npcs'], 2)
        self.assertIn('Briar', plan['letGo']['names'])
        with self.assertRaises(WS.DifferentMap) as refused:
            WS.import_(self.dev, {}, 'password', 'Ada')
        self.assertIn('confirm it', str(refused.exception))
        self.assertEqual(self.one(self.live, 'SELECT count(*) FROM admin.releases'), 0, 'nothing imported')
        with self.assertRaises(WS.WrongPassword):
            WS.import_(self.dev, {}, 'guess', 'Ada', replace_map=True)
        result = WS.import_(self.dev, {}, 'password', 'Ada', 'The new map', expect=plan['digest'], replace_map=True)
        self.assertTrue(result['replacedMap'])
        self.assertGreaterEqual(result['summary']['letGo']['npcs'], 2)
        self.assertEqual(self.one(self.live, "SELECT count(*) FROM world.interiors WHERE id = 'cottage'"), 0, 'the old map is gone')
        self.assertEqual(self.one(self.live, "SELECT name FROM world.interiors WHERE id = 'shop'"), 'The New Shop')
        self.assertEqual(self.sql(self.live, "SELECT name, greeting FROM live.npcs WHERE id = 'sloe'"), sloe, 'never erased')
        self.assertEqual(self.sql(self.live, "SELECT home_area, work_area, evening_area FROM live.npcs WHERE id = 'born_x'"),
                         [(None, None, None)], 'the newcomer kept, unplaced: its house is not on the new map')
        self.assertIsNone(self.one(self.live, "SELECT evening_area FROM live.npcs WHERE id = 'rook'"), "rook's evenings let go")
        self.assertIsNotNone(self.one(self.live, "SELECT home_area FROM live.npcs WHERE id = 'rook'"), 'the rest of rook kept')
        self.assertEqual(self.game_data(self.live), game, "players' data untouched")
        release = self.one(self.live, 'SELECT summary FROM admin.releases WHERE number = 1')
        self.assertEqual((release['map']['id'], release['replacedMap']['id']), (stamp['id'], 'original:greyfen'))
        # Both on one map now, level: Update from live works again, and the newcomer came to dev.
        self.assertEqual(WS.status(self.live)['map']['id'], stamp['id'])
        after = WS.diff(self.dev)
        self.assertTrue(after['maps']['same'])
        self.assertEqual(after['counts'], {'live': 0, 'dev': 0, 'conflicts': 0})
        self.assertEqual(self.one(self.dev, "SELECT name FROM live.npcs WHERE id = 'born_x'"), 'Briar')
        self.sql(self.live, "UPDATE live.npcs SET purse = 21 WHERE id = 'wren'")
        WS.pull(self.dev, editor='Ada')
        self.assertEqual(self.one(self.dev, "SELECT purse FROM live.npcs WHERE id = 'wren'"), 21)

    def test_a_new_map_needs_every_migration_its_dev_world_has(self):
        self.new_map(self.dev)
        unmigrate(self.live)                         # (As PROD is: behind DEV.)
        WS.maintenance(self.live, True, 'Ada')
        plan = WS.plan_import(self.dev)
        self.assertTrue(any('lacks migrations' in b and 'live.business_settings' in b for b in plan['blocked']), plan['blocked'])
        with self.assertRaisesRegex(WS.PublishError, 'lacks migrations') as refused:
            WS.import_(self.dev, {}, 'password', 'Ada', replace_map=True)
        self.assertTrue(any('live.business_settings' in e for e in refused.exception.errors))
        self.assertEqual(self.one(self.live, 'SELECT count(*) FROM admin.releases'), 0)

    def test_the_identity_of_a_live_world_follows_its_releases(self):
        with W.connect(self.live, 'owner') as conn:
            self.assertEqual(WS.map_identity(conn, 'live')['id'], 'original:greyfen')
            conn.execute("INSERT INTO admin.releases (world_id, number, pushed_by, note, kind, content, summary) VALUES "
                         "('greyfen', 7, 'Ada', '', 'import', '{}', '{\"map\": {\"id\": \"map-x\", \"name\": \"X\"}}')")
            self.assertEqual((WS.map_identity(conn, 'live')['id'], WS.map_identity(conn, 'live')['release']), ('map-x', 7))
            self.assertEqual(WS.map_identity(conn, 'live', upto=6)['id'], 'original:greyfen', 'as it was before release 7')
        stamp = self.new_map(self.dev)
        with W.connect(self.dev, 'owner') as conn:
            self.assertEqual(WS.map_identity(conn, 'dev'), stamp, "a dev world's own stamp")


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class LauncherTests(ScratchWorlds, unittest.TestCase):
    """tools/live.sh on status 76: it waits for the world to open, then starts the live choice as it is by then. The
    tools are linked into a folder of their own with a stand-in for server.sh (exits 76 the first time, then 0)."""

    def setUp(self):
        super().setUp()
        self.other = f'{self.live}x'
        W.new_world(self.other, 'Other', 'live', log=lambda *_: None)
        self.root = tempfile.TemporaryDirectory()
        tools = Path(self.root.name) / 'tools'
        tools.mkdir()
        for path in (ROOT / 'tools').iterdir():
            if path.name != 'server.sh' and path.is_file():
                (tools / path.name).symlink_to(path)
        self.log = Path(self.root.name) / 'runs.log'
        (tools / 'server.sh').write_text('#!/usr/bin/env bash\necho "$*" >> "$RATW_STUB_LOG"\n'
                                         '[[ "$(wc -l < "$RATW_STUB_LOG")" -eq 1 ]] && exit 76\nexit 0\n')

    def tearDown(self):
        self.root.cleanup()
        with superuser() as su:
            su.execute(f'DROP DATABASE IF EXISTS ratw_{self.other} WITH (FORCE)')
        super().tearDown()

    def runs(self):
        return self.log.read_text().splitlines() if self.log.exists() else []

    def test_the_launcher_waits_for_maintenance_and_follows_the_switch(self):
        W.set_local(live=self.live)
        WS.maintenance(self.live, True, 'Ada')
        launcher = subprocess.Popen(['bash', str(Path(self.root.name) / 'tools' / 'live.sh'), 'server'],
                                    env={**os.environ, 'RATW_STUB_LOG': str(self.log), 'RATW_MAINTENANCE_POLL': '1'},
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            deadline = time.monotonic() + 20
            while not self.runs() and time.monotonic() < deadline:
                time.sleep(.2)
            time.sleep(2)
            self.assertIsNone(launcher.poll(), 'waiting while the world is in maintenance')
            self.assertEqual(len(self.runs()), 1)
            WS.switch_live(self.other, 'Ada')                  # (The DM switches; the new world waits in maintenance.)
            time.sleep(2)
            self.assertEqual(len(self.runs()), 1, 'still waiting: the new live world is in maintenance too')
            WS.maintenance(self.other, False, 'Ada')
            out, _ = launcher.communicate(timeout=30)
        finally:
            if launcher.poll() is None:
                launcher.kill()
                launcher.communicate()
        self.assertEqual(launcher.returncode, 0, out)
        runs = self.runs()
        self.assertEqual(len(runs), 2)
        self.assertIn(f'--database {self.live}', runs[0])
        self.assertIn(f'--database {self.other}', runs[1], 'started on the live world chosen meanwhile')
        self.assertIn('closed for maintenance', out)
        self.assertIn(f'the live world is now {self.other}', out)


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


@unittest.skipUnless(database_available() and SERVER.exists(), 'local PostgreSQL not running, or ratw_server not built')
class ServerMaintenanceTests(ScratchWorlds, unittest.TestCase):
    """The game server closes for maintenance (status 76) and will not start while its world is in it."""

    def start(self, key, *extra, port=None):
        return subprocess.Popen([str(SERVER), '--database', key, '--port', str(port or free_port()), '--for', '60', *extra],
                                env={**os.environ, 'RATW_DATABASE_URL': W.libpq_conninfo(key, 'game'), 'RATW_AI': 'off'},
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    def test_a_dev_world_s_new_build_restarts_its_server_and_not_a_scratch_one(self):
        # (DEV run as the server: Atlas's edits reach it as a new build, announced like a release; doc 66, Progress.)
        with W.connect(self.dev, 'editor') as editor:
            world_build.build(editor, 'test')
        port = free_port()
        server, scratch = self.start(self.dev, port=port), self.start(self.dev, '--scratch')
        try:
            for process in (server, scratch):
                for line in process.stdout:
                    if 'listening on port' in line:
                        break
            holder = WS.status(self.dev)['serverHolder']
            self.assertEqual((holder['port'], holder['local'], holder['pid']), (port, True, server.pid), holder)
            with W.connect(self.dev, 'editor') as editor:
                build, _ = world_build.build(editor, 'test')
            out, _ = server.communicate(timeout=30)
            self.assertEqual(server.returncode, 75, out)
            self.assertIn(f'RATW_RELEASE_RESTART build={build}', out)
            time.sleep(3)
            self.assertIsNone(scratch.poll(), 'a scratch server stays on the build it loaded')
            scratch.kill()
            self.assertIn('RATW_RELEASE_IGNORED', scratch.communicate()[0])
        finally:
            for process in (server, scratch):
                if process.poll() is None:
                    process.kill()
                    process.communicate()

    def test_the_economys_rules_reach_the_running_server(self):
        # Doc 68, Progress, "Seasons and the DM's rules": a change of the economy's rules (here the seasons, written as
        # the studio writes them) queues economy.sync, which the running server applies at once.
        import studio
        with W.connect(self.dev, 'editor') as editor:
            world_build.build(editor, 'test')
        server = self.start(self.dev)
        try:
            for line in server.stdout:
                if 'listening on port' in line:
                    break
            economy = studio.Economy()
            result = economy.set_seasons(self.dev, {'set': {'grain': {'normal': 40}}}, 'Ada')
            self.assertTrue(result['changed'], result)
            action = economy.queue_sync(self.dev, 'Ada')
            for _ in range(60):
                with W.connect(self.dev, 'dm') as dm:
                    status, said = dm.execute('SELECT status, result FROM dm.actions WHERE id = %s', (action,)).fetchone()
                if status != 'queued':
                    break
                time.sleep(.5)
            self.assertEqual((status, said), ('applied', 'The economy\'s rules are read again and in force.'))
        finally:
            server.kill()
            server.communicate()

    def test_the_server_closes_for_maintenance_and_waits_for_it_to_end(self):
        with W.connect(self.dev, 'editor') as editor:
            world_build.build(editor, 'test')
        server = self.start(self.dev)
        try:
            for line in server.stdout:
                if 'listening on port' in line:
                    break
            started = time.monotonic()
            WS.maintenance(self.dev, True, 'Ada', 'Tests')
            out, _ = server.communicate(timeout=30)
            self.assertEqual(server.returncode, WS.MAINTENANCE_EXIT, out)
            self.assertLess(time.monotonic() - started, 15, 'nobody playing: no grace to wait out')
            self.assertIn('RATW_MAINTENANCE', out)
        finally:
            if server.poll() is None:
                server.kill()
                server.communicate()
        again = self.start(self.dev)
        out, _ = again.communicate(timeout=30)
        self.assertEqual(again.returncode, WS.MAINTENANCE_EXIT, out)
        self.assertIn('closed for maintenance', out)
        WS.maintenance(self.dev, False, 'Ada')
        third = self.start(self.dev)
        try:
            for line in third.stdout:
                if 'listening on port' in line:
                    break
            self.assertIsNone(third.poll(), 'open again: it runs')
        finally:
            third.kill()
            third.communicate()


if __name__ == '__main__':
    unittest.main()
