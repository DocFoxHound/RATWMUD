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

    def test_the_live_games_fields_default_to_live_and_a_place_is_one_field(self):
        # (Doc 66, Progress, "The live world's truth".) Dev moved Wren's home and wrote her new words; live's game moved her.
        home = lambda area, x, y: {'home_area': area, 'home_x': x, 'home_y': y}
        item = self.plan(npc() | home('a', 1, 1), npc(greeting='Hi.') | home('b', 1, 2), npc() | home('c', 1, 3))
        self.assertEqual(WS.default_choice(item), {'home_area': 'live', 'home_y': 'live'}, 'x is the same on both')
        self.assertEqual({f: WS.field_side(item, f) for f in ('home_area', 'home_x', 'home_y')},
                         dict.fromkeys(('home_area', 'home_x', 'home_y'), 'conflict'), 'a place is one field')
        row = WS.resolve([item], {})['live.npcs/wren']
        self.assertEqual((row['home_area'], row['home_y'], row['greeting']), ('c', 3, 'Hi.'), "live's place, dev's words")
        row = WS.resolve([item], {'live.npcs/wren': {'home_area': 'dev'}})['live.npcs/wren']
        self.assertEqual((row['home_area'], row['home_y']), ('b', 2), 'the place goes whole')
        self.assertIsNone(WS.default_choice(self.plan(npc(), npc(name='A'), npc(name='B'))), 'an authored field: the user chooses')
        born = self.plan(None, npc(name='A') | {'origin': 'runtime'}, npc(name='B') | {'origin': 'runtime'})
        self.assertEqual(WS.default_choice(born), {'name': 'live'})
        self.assertEqual(WS.owner(born, 'name'), 'game', 'a row the game made is the game\'s whole')
        self.assertEqual(WS.default_choice(self.plan(npc(), None, npc(purse=4))), 'live', 'dev removed it, the game changed it')
        self.assertIsNone(WS.default_choice(self.plan(npc(), None, npc(greeting='Kept.'))))
        # A new map without live's place: a decision, never the default.
        gone = WS.replacing([self.plan(None, None, npc() | home('cottage', 1, 1))], areas={'town'})[0]
        self.assertEqual((gone.side, gone.gone, WS.default_choice(gone)), ('conflict', ('home',), None))
        with self.assertRaises(WS.Unresolved):
            WS.resolve([gone], {}, {'town'})
        self.assertIsNone(WS.resolve([gone], {'live.npcs/wren': 'unplace'}, {'town'})['live.npcs/wren']['home_area'])
        moved = WS.resolve([gone], {'live.npcs/wren': {'home': {'cell': 'town', 'x': 2, 'y': 3}}}, {'town'})['live.npcs/wren']
        self.assertEqual((moved['home_area'], moved['home_x'], moved['home_y']), ('town', 2, 3))
        for bad in ('live', 'dev', {'home': {'cell': 'cottage', 'x': 1, 'y': 1}}):
            with self.assertRaisesRegex(WS.PublishError, 'cannot be used'):
                WS.resolve([gone], {'live.npcs/wren': bad}, {'town'})

    def test_a_place_chosen_by_any_of_its_columns_goes_whole_and_never_half(self):
        home = lambda area, x, y: {'home_area': area, 'home_x': x, 'home_y': y}
        # Both moved Wren's home; her x is the same on both sides, so the screens show it changed on both too.
        item = self.plan(npc() | home('a', 1, 1), npc() | home('b', 2, 2), npc() | home('c', 2, 3))
        everything = {'home_area': 'dev', 'home_x': 'dev', 'home_y': 'dev'}
        row = WS.resolve([item], {'live.npcs/wren': everything})['live.npcs/wren']
        self.assertEqual((row['home_area'], row['home_x'], row['home_y']), ('b', 2, 2), 'a column the two agree on is the place too')
        row = WS.resolve([item], {'live.npcs/wren': {'home_x': 'dev'}})['live.npcs/wren']
        self.assertEqual((row['home_area'], row['home_y']), ('b', 2), 'any column picks the place')
        with self.assertRaisesRegex(WS.PublishError, 'cannot be used') as split:
            WS.resolve([item], {'live.npcs/wren': {'home_area': 'dev', 'home_x': 'live', 'home_y': 'live'}})
        self.assertIn('one field', split.exception.errors[0])
        # A side that isn't one, on any column (one of a place the two sides agree on too), is refused by name.
        for bad in ({'home_x': {'cell': 'b', 'x': 1, 'y': 1}}, {'home_area': ['dev']}, {'work_x': 'nope'}, {'name': None}):
            with self.assertRaisesRegex(WS.PublishError, 'cannot be used') as refused:
                WS.resolve([item], {'live.npcs/wren': bad})
            self.assertIn(f'(not {next(iter(bad))})', refused.exception.errors[0])

    def test_unplacing_keeps_each_sides_own_changes_and_asks_for_what_both_changed(self):
        home = lambda area: {'home_area': area, 'home_x': 1, 'home_y': 1}
        text = lambda description, greeting: {'description': description, 'greeting': greeting}
        # A new map without live's cottage; dev rewrote Wren's description, both changed her greeting.
        item = WS.replacing([self.plan(npc() | home('cottage') | text('Old.', 'Hello.'), npc() | home('cottage') | text('New.', 'Dev hi.'),
                                       npc() | home('cottage') | text('Old.', 'Live hi.'))], areas={'town'})[0]
        self.assertEqual(item.gone, ('home',))
        with self.assertRaisesRegex(WS.PublishError, 'cannot be used') as asked:
            WS.resolve([item], {'live.npcs/wren': 'unplace'}, {'town'})
        self.assertIn('greeting', asked.exception.errors[0])
        row = WS.resolve([item], {'live.npcs/wren': {'home': 'unset', 'greeting': 'live'}}, {'town'})['live.npcs/wren']
        self.assertEqual((row['home_area'], row['description'], row['greeting']), (None, 'New.', 'Live hi.'))
        # Without one both changed, unplace is the row by default: dev's new description kept.
        item = WS.replacing([self.plan(npc() | home('cottage') | text('Old.', 'Hello.'), npc() | home('cottage') | text('New.', 'Hello.'),
                                       npc(purse=4) | home('cottage') | text('Old.', 'Hello.'))], areas={'town'})[0]
        row = WS.resolve([item], {'live.npcs/wren': 'unplace'}, {'town'})['live.npcs/wren']
        self.assertEqual((row['home_area'], row['description'], row['purse']), (None, 'New.', 4))
        # Dev moved her into the new map, live never moved her, both changed her greeting: her place is dev's, not gone.
        item = WS.replacing([self.plan(npc() | home('cottage') | text('Old.', 'Hello.'), npc() | home('town') | text('Old.', 'Dev hi.'),
                                       npc() | home('cottage') | text('Old.', 'Live hi.'))], areas={'town'})[0]
        self.assertEqual(item.gone, ())

    def test_rows_hanging_on_an_npc_go_as_the_npcs_conflict_ends(self):
        # Dev removed Linden (her voice and her marriage went with her); live's game moved her: kept by default, with them.
        linden = {'world_id': 'w', 'id': 'linden', 'name': 'Linden', 'home_area': 'a', 'home_x': 1, 'home_y': 1}
        voice = {'world_id': 'w', 'npc_id': 'linden', 'accent': 'broad'}
        wed = {'world_id': 'w', 'npc_id': 'sorrel', 'other_id': 'linden', 'kind': 'spouse'}
        sorrel = {'world_id': 'w', 'id': 'sorrel', 'name': 'Sorrel'}
        mine = {'world_id': 'w', 'npc_id': 'sorrel', 'accent': 'light'}
        rows = lambda *npcs, voices=(), family=(): snap(live__npcs={n['id']: n for n in npcs}, live__npc_voice={v['npc_id']: v for v in voices},
                                                       live__npc_relations={'sorrel/linden': r for r in family})
        base = rows(linden, sorrel, voices=(voice, mine), family=(wed,))
        dev = rows(sorrel, voices=(), family=())                                      # (Sorrel's own voice removed in dev too.)
        live = rows({**linden, 'home_area': 'b'}, sorrel, voices=(voice, mine), family=(wed,))
        items = WS.three_way(base, dev, live)
        sides = {i.id: i.side for i in items}
        self.assertEqual(sides, {'live.npcs/linden': 'conflict', 'live.npc_voice/linden': 'dev', 'live.npc_voice/sorrel': 'dev',
                                 'live.npc_relations/sorrel%2Flinden': 'dev'})
        self.assertEqual(WS.follows(items), {'live.npc_voice/linden': 'live.npcs/linden',
                                             'live.npc_relations/sorrel%2Flinden': 'live.npcs/linden'})
        kept = WS.resolve(items, {})
        self.assertEqual(WS.with_their_npcs(items, kept, 'live'), ({'live.npc_voice/linden', 'live.npc_relations/sorrel%2Flinden'}, []),
                         "Linden kept on live keeps hers; Sorrel's own voice, removed in dev, goes")
        gone = WS.resolve(items, {'live.npcs/linden': 'dev'})
        self.assertEqual(WS.with_their_npcs(items, gone, 'live'), (set(), []), 'she goes, they go')
        # A pull, onto dev, where dev removed her: back in dev by default, and hers back with her (from live, the base's).
        self.assertEqual(WS.with_their_npcs(items, kept, 'dev'),
                         (set(), [(WS.SPECS['live.npc_voice'], ('linden',), voice), (WS.SPECS['live.npc_relations'], ('sorrel/linden',), wed)]))
        self.assertEqual(WS.with_their_npcs(items, gone, 'dev'), (set(), []), "dev's removal chosen: she stays gone, and they")
        # An import onto live where live removed her and dev's is chosen: the same, from dev's.
        flipped = WS.three_way(base, live, dev)
        self.assertEqual(WS.with_their_npcs(flipped, WS.resolve(flipped, {'live.npcs/linden': 'dev'}), 'live')[1],
                         [(WS.SPECS['live.npc_voice'], ('linden',), voice), (WS.SPECS['live.npc_relations'], ('sorrel/linden',), wed)])
        # A member of a faction the write removes as well goes with the faction.
        watch = {'world_id': 'w', 'id': 'watch', 'name': 'The Watch'}
        sworn = {'world_id': 'w', 'faction_id': 'watch', 'npc_id': 'linden'}
        rows = lambda *npcs, factions=(), members=(): snap(live__npcs={n['id']: n for n in npcs}, live__factions={f['id']: f for f in factions},
                                                           live__faction_members={'watch/linden': m for m in members})
        for dev_factions, back in (((watch,), [(WS.SPECS['live.faction_members'], ('watch/linden',), sworn)]), ((), [])):
            items = WS.three_way(rows(linden, factions=(watch,), members=(sworn,)), rows(factions=dev_factions),
                                 rows({**linden, 'home_area': 'b'}, factions=(watch,), members=(sworn,)))
            self.assertEqual(WS.with_their_npcs(items, WS.resolve(items, {}), 'dev')[1], back)

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
        # (Their places on the old map: a decision each, never let go unasked; doc 66, Progress, "The live world's truth".)
        homeless = {c['id']: [g['place'] for g in c['gone']] for c in plan['conflicts'] if c['gone']}
        self.assertEqual(homeless, {'live.npcs/born_x': ['home', 'work', 'evening'], 'live.npcs/rook': ['evening']})
        with self.assertRaises(WS.Unresolved) as unresolved:
            WS.import_(self.dev, {}, 'password', 'Ada', replace_map=True)
        self.assertEqual(sorted(c['id'] for c in unresolved.exception.conflicts), ['live.npcs/born_x', 'live.npcs/rook'])
        result = WS.import_(self.dev, {'live.npcs/born_x': 'unplace', 'live.npcs/rook': 'unplace'}, 'password', 'Ada',
                            'The new map', expect=plan['digest'], replace_map=True)
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


ARRIVE = ("UPDATE live.npcs SET home_area = %s, home_x = %s, home_y = %s WHERE world_id = 'greyfen' AND id = %s "
          "AND home_area IS NULL RETURNING id")               # (As Game::makeResidents places an arrival from the Country.)
MOVE = "UPDATE live.npcs SET home_area = %s, home_x = %s, home_y = %s WHERE world_id = 'greyfen' AND id = %s RETURNING id"
BIRTH = ("INSERT INTO live.npcs (world_id, id, position, name, role, description, greeting, personality, backstory, work_label, "
         "age, voice, appearance, paid, purse, herbs, meals, hours_start, hours_end, home_area, home_x, home_y, work_area, work_x, "
         "work_y, evening_area, evening_x, evening_y, origin) SELECT world_id, %s, 900, %s, 'civilian', 'A young wolf, born here.', "
         "'Hello.', personality, 'Born in the world, not written.', '-', 0, voice, appearance, false, 0, 0, 1, hours_start, "
         "hours_end, %s, 1, 1, %s, 1, 1, %s, 1, 1, 'runtime' FROM live.npcs WHERE id = 'wren' RETURNING id")


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class LiveTruthTests(ScratchWorlds, unittest.TestCase):
    """The live world's truth (doc 66, Progress): the game's NPCs and markets survive Atlas's saves, pulls and imports;
    conflicts default to live; a new map lists the NPCs whose places it hasn't. The game's writes are made as the game
    server makes them, as ratw_game (migration 0059 logs them)."""

    def game(self, key, query, params=()):
        with W.connect(key, 'game') as conn:
            return conn.execute(query, params).fetchall()

    def npc(self, key, npc_id, columns='home_area, home_x, home_y'):
        return self.sql(key, f"SELECT {columns} FROM live.npcs WHERE id = %s", (npc_id,))[0]

    # -- Atlas on a world the game runs ------------------------------------------
    def test_atlas_whole_world_save_keeps_a_placement_the_game_made_and_conflicts_where_both_changed(self):
        self.sql(self.dev, "UPDATE live.npcs SET home_area = NULL, home_x = 0, home_y = 0 WHERE id = 'maple'")   # (Unplaced.)
        editor = self.conn(self.dev, 'editor')
        project, revision, seen = S.load_world(editor, 'greyfen', with_seen=True)
        self.assertEqual(seen, S.game_seen(editor))
        self.assertEqual(self.game(self.dev, ARRIVE, ('longhouse', 17, 9, 'maple')), [('maple',)], 'the arrival')
        self.assertEqual(S.game_seen(editor), seen + 1, 'logged by the database')
        maple = next(p for p in project['people'] if p['id'] == 'maple')
        self.assertIsNone(maple['home'], 'Atlas read it unplaced')
        maple['greeting'] = 'Atlas wrote this.'
        outcome = {}
        revision = S.save_world(editor, project, revision, seen=seen, outcome=outcome)
        self.assertEqual(self.npc(self.dev, 'maple', 'home_area, home_x, home_y, greeting'), ('longhouse', 17, 9, 'Atlas wrote this.'),
                         "the game's placement kept, Atlas's own edit saved")
        self.assertEqual(outcome, {'kept': ['live.npcs/maple'], 'conflicts': []})
        # Both changed the home: a conflict, the game's by default; then Atlas's, chosen.
        place = lambda cell, x, y: {'cell': cell, 'x': x, 'y': y}
        for choices, game, want, chosen in (({}, place('cottage', 3, 3), ('cottage', 3, 3), 'game'),
                                            ({'live.npcs/maple': 'atlas'}, place('wren_house', 4, 4), ('barracks', 1, 1), 'atlas')):
            project, revision = S.load_world(editor, 'greyfen')
            seen = S.game_seen(editor)
            base = next(p for p in project['people'] if p['id'] == 'maple')['home']
            self.game(self.dev, MOVE, (game['cell'], game['x'], game['y'], 'maple'))
            next(p for p in project['people'] if p['id'] == 'maple')['home'] = place('barracks', 1, 1)
            outcome = {}
            S.save_world(editor, project, revision, seen=seen, choices=choices, outcome=outcome)
            self.assertEqual(self.npc(self.dev, 'maple'), want)
            self.assertEqual([(c['id'], [(f['field'], f['base'], f['atlas'], f['game'], f['chosen']) for f in c['fields']])
                              for c in outcome['conflicts']],
                             [('live.npcs/maple', [('home', base, place('barracks', 1, 1), game, chosen)])])
        # Without what the save had seen, the game's changes at the project's revision count as after it: a field
        # holding a value the game held meanwhile is the game's.
        project, revision = S.load_world(editor, 'greyfen')
        self.game(self.dev, MOVE, ('wren_house', 5, 2, 'maple'))
        next(p for p in project['people'] if p['id'] == 'maple')['description'] = 'Moved again.'
        S.save_world(editor, project, revision)
        self.assertEqual(self.npc(self.dev, 'maple', 'home_area, description'), ('wren_house', 'Moved again.'))

    def test_a_whole_world_save_without_seen_keeps_what_its_author_changed_after_the_game(self):
        # The game places Maple and Rook; a generator loads the world after (it has them placed), and re-homes Maple on
        # purpose, leaving Rook. Saved without `seen`: both of the game's changes are at the project's revision, but
        # Maple's new home is its author's (listed, the save's), Rook's the game's.
        self.game(self.dev, MOVE, ('longhouse', 3, 3, 'maple'))
        self.game(self.dev, MOVE, ('longhouse', 5, 5, 'rook'))
        editor = self.conn(self.dev, 'editor')
        project, revision = S.load_world(editor, 'greyfen')
        next(p for p in project['people'] if p['id'] == 'maple')['home'] = {'cell': 'cottage', 'x': 3, 'y': 3}
        outcome = {}
        S.save_world(editor, project, revision, outcome=outcome)
        self.assertEqual(self.npc(self.dev, 'maple'), ('cottage', 3, 3), "the author's re-homing stands")
        self.assertEqual(self.npc(self.dev, 'rook'), ('longhouse', 5, 5))
        self.assertEqual([(c['id'], [(f['field'], f['chosen']) for f in c['fields']]) for c in outcome['conflicts']],
                         [('live.npcs/maple', [('home', 'atlas')])], 'told, and the save\'s')
        # With `seen` (load_world's), nothing the game did is after it: the author's re-homing, nothing to tell.
        project, revision, seen = S.load_world(editor, 'greyfen', with_seen=True)
        next(p for p in project['people'] if p['id'] == 'rook')['home'] = {'cell': 'shop', 'x': 3, 'y': 4}
        outcome = {}
        S.save_world(editor, project, revision, seen=seen, outcome=outcome)
        self.assertEqual((self.npc(self.dev, 'rook'), outcome), (('shop', 3, 4), {'kept': [], 'conflicts': []}))
        self.assertTrue(any('maple' in line for line in S.truth_lines({'kept': ['live.npcs/maple'], 'conflicts': []})))

    def test_the_games_place_is_put_back_only_where_the_saves_map_has_it(self):
        # The game moves Maple into the cottage; a generator's save, loaded before, takes the cottage away (its residents
        # re-homed or unplaced). The game's place isn't on the saved map: the save's stands, and the caller is told.
        for table in ('world.places', 'world.resources', 'live.faction_claims', 'live.npc_areas', 'live.patrol_posts'):
            self.sql(self.dev, f"DELETE FROM {table} WHERE area = 'cottage'")
        editor = self.conn(self.dev, 'editor')
        project, revision, seen = S.load_world(editor, 'greyfen', with_seen=True)
        self.game(self.dev, MOVE, ('cottage', 4, 4, 'maple'))
        self.game(self.dev, MOVE, ('wren_house', 2, 2, 'rook'))                 # (A solid tile: not put back either.)
        project['rooms'] = [r for r in project['rooms'] if r['id'] != 'cottage']
        project['links'] = [k for k in project['links'] if 'cottage' not in (k['a']['cell'], k['b']['cell'])]
        for person in project['people']:
            for place in ('home', 'work', 'evening'):
                if (person[place] or {}).get('cell') == 'cottage':
                    person[place] = None
        outcome = {}
        S.save_world(editor, project, revision, seen=seen, outcome=outcome)
        self.assertEqual(self.npc(self.dev, 'maple'), ('longhouse', 17, 9), "the save's, the game's place gone")
        self.assertEqual(self.npc(self.dev, 'rook'), ('longhouse', 6, 9))
        self.assertEqual(self.one(self.dev, "SELECT count(*) FROM world.interiors WHERE id = 'cottage'"), 0, 'the save made')
        problems = {c['id']: [f.get('problem', '') for f in c['fields']] for c in outcome['conflicts']}
        self.assertEqual(sorted(problems), ['live.npcs/maple', 'live.npcs/rook'])
        self.assertIn('choose a tile inside an existing cell', problems['live.npcs/maple'][0])
        self.assertIn('solid', problems['live.npcs/rook'][0])

    def test_a_game_write_made_during_a_save_waits_for_it_and_is_never_lost(self):
        # Atlas's whole-world save and a keyed edit, each paused after reading Maple, while the game places her: the
        # game's write waits for the save (her row is locked as read) and is made on what was saved.
        import threading
        for name, save in (('a whole-world save', lambda editor: S.save_world(editor, self.project, self.revision, seen=self.seen)),
                           ('a keyed edit', lambda editor: L.apply_edit(editor, 'atlas-tab', 'Ada', 'Greeting', [
                               {'key': 'person:maple', 'before': self.maple, 'after': {**self.maple, 'greeting': 'Atlas, keyed.'}}]))):
            self.sql(self.dev, "UPDATE live.npcs SET home_area = NULL, home_x = 0, home_y = 0 WHERE id = 'maple'")
            editor = self.conn(self.dev, 'editor')
            self.project, self.revision, self.seen = S.load_world(editor, 'greyfen', with_seen=True)
            self.maple = L.Batch(editor, L.world_row(editor)).current('person:maple')
            next(p for p in self.project['people'] if p['id'] == 'maple')['greeting'] = 'Atlas, whole.'
            reading, going = threading.Event(), threading.Event()
            real_write, real_flush = S.write_rows, L.Batch.flush

            def paused(real):
                def run(*args, **kwargs):
                    reading.set()
                    going.wait(10)
                    return real(*args, **kwargs)
                return run
            S.write_rows, L.Batch.flush = paused(real_write), paused(real_flush)
            try:
                saving = threading.Thread(target=save, args=(editor,))
                saving.start()
                self.assertTrue(reading.wait(10), name)
                arriving = threading.Thread(target=self.game, args=(self.dev, ARRIVE, ('longhouse', 17, 9, 'maple')))
                arriving.start()
                arriving.join(0.5)
                self.assertTrue(arriving.is_alive(), f'{name}: the game waits for the save')
                going.set()
                saving.join(10)
                arriving.join(10)
            finally:
                S.write_rows, L.Batch.flush = real_write, real_flush
                going.set()
            self.assertEqual(self.npc(self.dev, 'maple', 'home_area, home_x, home_y')[0], 'longhouse', f'{name}: the arrival kept')
            self.assertIn(self.npc(self.dev, 'maple', 'greeting')[0], ('Atlas, whole.', 'Atlas, keyed.'), f'{name}: and Atlas\'s edit')

    def test_atlas_edits_after_a_game_placement_save_and_conflict_where_both_changed(self):
        editor = self.conn(self.dev, 'editor')
        project, seq, game_seq = L.load(editor, with_game=True)
        seen = next(p for p in project['people'] if p['id'] == 'maple')
        self.game(self.dev, MOVE, ('cottage', 3, 3, 'maple'))          # (The game moves Maple; Atlas still shows her old home.)
        # Atlas's own change to an authored field saves on the game's row.
        merged = {}
        L.apply_edit(editor, 'atlas-tab', 'Ada', 'Greeting', [{'key': 'person:maple', 'before': seen,
                                                               'after': {**seen, 'greeting': 'Atlas hello.'}}], merged)
        self.assertEqual(self.npc(self.dev, 'maple', 'home_area, home_x, greeting'), ('cottage', 3, 'Atlas hello.'))
        self.assertEqual(merged['person:maple']['home'], {'cell': 'cottage', 'x': 3, 'y': 3}, 'the editor is told what was written')
        # The heartbeat brings the game's change to every open Atlas.
        beat = L.sync(editor, 'other-tab', 'Bo', '#aabbcc', seq, {}, game_since=game_seq)
        game = [c for c in beat['changes'] if c['clientId'] == 'game']
        self.assertEqual([(c['key'], c['editor'], c['after']['home']) for c in game],
                         [('person:maple', 'The game', {'cell': 'cottage', 'x': 3, 'y': 3})])
        self.assertGreater(beat['gameSeq'], game_seq)
        # Both changed the home: refused, naming the game's and Atlas's values; the game's stays.
        seen = L.Batch(editor, L.world_row(editor)).current('person:maple')
        self.game(self.dev, MOVE, ('wren_house', 5, 2, 'maple'))
        with self.assertRaises(L.Conflict) as refused:
            L.apply_edit(editor, 'atlas-tab', 'Ada', 'Move Maple', [{'key': 'person:maple', 'before': seen,
                                                                   'after': {**seen, 'home': {'cell': 'barracks', 'x': 1, 'y': 1}}}])
        conflict = refused.exception.conflicts[0]
        self.assertEqual((conflict['key'], conflict['game'], conflict['editor']), ('person:maple', True, 'The game'))
        self.assertEqual(conflict['fields'], [{'field': 'home', 'base': {'cell': 'cottage', 'x': 3, 'y': 3},
                                               'mine': {'cell': 'barracks', 'x': 1, 'y': 1},
                                               'game': {'cell': 'wren_house', 'x': 5, 'y': 2}}])
        self.assertEqual(self.npc(self.dev, 'maple'), ('wren_house', 5, 2), "the game's, until Atlas's is chosen")
        # Chosen "mine" in the conflict: the edit again, on what is there now.
        now = refused.exception.current['person:maple']
        L.apply_edit(editor, 'atlas-tab', 'Ada', 'Move Maple', [{'key': 'person:maple', 'before': now,
                                                                'after': {**now, 'home': {'cell': 'barracks', 'x': 1, 'y': 1}}}])
        self.assertEqual(self.npc(self.dev, 'maple'), ('barracks', 1, 1))
        # Another editor's change is refused as ever (not the game's).
        stale = L.Batch(editor, L.world_row(editor)).current('person:maple')
        L.apply_edit(editor, 'bo-tab', 'Bo', 'Name', [{'key': 'person:maple', 'before': stale, 'after': {**stale, 'name': 'Maple Bo'}}])
        with self.assertRaises(L.Conflict) as other:
            L.apply_edit(editor, 'atlas-tab', 'Ada', 'Greeting', [{'key': 'person:maple', 'before': stale,
                                                                   'after': {**stale, 'greeting': 'Again.'}}])
        self.assertEqual(other.exception.conflicts[0]['editor'], 'Bo')

    def test_the_heartbeat_tags_the_games_changes_with_the_edits_their_values_hold(self):
        # This tab's own edit of Maple and the game's move of her both commit after the heartbeat read the edit log and
        # before it read the game's changes: whatever it reports must not be taken for older than that edit (the
        # editor drops a change older than its own), or the move is lost once gameSince passes it.
        editor, other = self.conn(self.dev, 'editor'), self.conn(self.dev, 'editor')
        _, seq, game_seq = L.load(editor, with_game=True)
        maple = L.Batch(editor, L.world_row(editor)).current('person:maple')
        own, real = {}, L.game_feed

        def meanwhile(conn, world, game_since):
            L.game_feed = real
            own['seq'] = L.apply_edit(other, 'atlas-tab', 'Ada', 'Greeting', [{'key': 'person:maple', 'before': maple,
                                                                              'after': {**maple, 'greeting': 'Mine.'}}])
            self.game(self.dev, MOVE, ('cottage', 3, 3, 'maple'))
            return real(conn, world, game_since)
        L.game_feed = meanwhile
        try:
            first = L.sync(editor, 'atlas-tab', 'Ada', '#aabbcc', seq, {}, game_since=game_seq)
        finally:
            L.game_feed = real
        second = L.sync(editor, 'atlas-tab', 'Ada', '#aabbcc', first['seq'], {}, game_since=first.get('gameSeq', game_seq))
        taken = [c for beat in (first, second) for c in beat['changes'] if c['clientId'] == 'game' and not own['seq'] > c['seq']]
        self.assertEqual([(c['key'], c['after']['home'], c['after']['greeting']) for c in taken],
                         [('person:maple', {'cell': 'cottage', 'x': 3, 'y': 3}, 'Mine.')], 'the move arrives, with the edit')

    # -- Import and pull ----------------------------------------------------------
    def test_import_keeps_prods_live_npc_and_market_state(self):
        # PROD's game: an arrival from the Country placed, a birth, its save (the households, tills, stock, prices,
        # the loop), each NPC's running state; its own economy settings.
        self.sql(self.live, "UPDATE live.npcs SET home_area = NULL, home_x = 0, home_y = 0 WHERE id = 'maple'")
        self.game(self.live, ARRIVE, ('cottage', 4, 4, 'maple'))
        self.game(self.live, BIRTH, ('born_t', 'Thistle', 'longhouse', 'longhouse', 'longhouse'))
        save = '{"society": {"tills": {"till:job:brann": 412}, "loop": {"prices": {"bread": 1.2}}, "households": 3}}'
        self.game(self.live, "INSERT INTO game.checkpoints (world_id, schema_version, revision, payload) VALUES "
                             "('greyfen', 1, 9, %s) RETURNING world_id", (save,))
        self.game(self.live, "INSERT INTO live.npc_state (world_id, npc_id, state) VALUES ('greyfen', 'brann', "
                             "'{\"cash\": 41, \"stock\": {\"meal\": 3}}') RETURNING npc_id")
        self.sql(self.live, "UPDATE live.economy SET treasury = 5500, gate_toll = 31, rules = '{\"treasuryTarget\": 5500}'")
        market = self.sql(self.live, 'SELECT payload::text FROM game.checkpoints UNION ALL SELECT state::text FROM live.npc_state '
                                     'UNION ALL SELECT row_to_json(e)::text FROM live.economy e')
        # DEV's authored changes: a building renamed, an NPC's words, its own economy settings.
        self.sql(self.dev, "UPDATE world.interiors SET name = 'The Corner Shop' WHERE id = 'shop'")
        self.sql(self.dev, "UPDATE live.npcs SET greeting = 'Dev words.' WHERE id = 'sloe'")
        self.sql(self.dev, 'UPDATE live.economy SET gate_toll = 10')
        WS.maintenance(self.live, True, 'Ada', 'Import')
        plan = WS.plan_import(self.dev)
        self.assertEqual(self.ids(plan['liveChanges']), ['live.npcs/born_t', 'live.npcs/maple'])
        self.assertEqual([c['live'] for c in plan['liveChanges']], ['added', 'changed'], 'the birth an addition live')
        self.assertEqual(plan['conflicts'], [])
        self.assertIn('gate_toll', plan['economy']['fields'])
        self.assertTrue(any('keeps its own' in w for w in plan['warnings']))
        result = WS.import_(self.dev, {}, 'password', 'Ada', 'A corner shop')
        self.assertEqual(result['summary']['live'], {'NPCs': {'added': 0, 'changed': 1, 'removed': 0}})
        self.assertEqual(self.one(self.live, "SELECT name FROM world.interiors WHERE id = 'shop'"), 'The Corner Shop')
        self.assertEqual(self.one(self.live, "SELECT greeting FROM live.npcs WHERE id = 'sloe'"), 'Dev words.')
        self.assertEqual(self.npc(self.live, 'maple'), ('cottage', 4, 4), "PROD's arrival kept")
        self.assertEqual(self.npc(self.live, 'born_t', 'name, origin'), ('Thistle', 'runtime'), "PROD's birth kept")
        self.assertEqual(self.sql(self.live, 'SELECT payload::text FROM game.checkpoints UNION ALL SELECT state::text FROM live.npc_state '
                                             'UNION ALL SELECT row_to_json(e)::text FROM live.economy e'), market,
                         "the market, the save and the economy's settings untouched")
        # DEV is brought level: it has PROD's birth and arrival now, and nothing left to compare.
        self.assertEqual(self.npc(self.dev, 'maple'), ('cottage', 4, 4))
        self.assertEqual(self.npc(self.dev, 'born_t', 'name'), ('Thistle',))
        self.assertEqual(self.one(self.dev, 'SELECT gate_toll FROM live.economy'), 10, "DEV's economy its own")
        self.assertEqual(WS.diff(self.dev)['counts'], {'live': 0, 'dev': 0, 'conflicts': 0})

    def test_conflicts_default_to_live(self):
        # DEV moves Maple's home and writes her new words; PROD's game moves her too.
        self.sql(self.dev, "UPDATE live.npcs SET home_area = 'shop', home_x = 3, home_y = 4, greeting = 'Dev words.' WHERE id = 'maple'")
        self.game(self.live, MOVE, ('cottage', 3, 3, 'maple'))
        # DEV removes Linden; PROD's game moved him meanwhile.
        self.sql(self.dev, "DELETE FROM live.npcs WHERE id = 'linden'")
        self.game(self.live, MOVE, ('cottage', 7, 7, 'linden'))
        plan = WS.diff(self.dev)
        conflicts = {c['id']: c for c in plan['conflicts']}
        self.assertEqual(sorted(conflicts), ['live.npcs/linden', 'live.npcs/maple'])
        maple = conflicts['live.npcs/maple']
        self.assertEqual(maple['default'], {'home_area': 'live', 'home_y': 'live'}, 'the fields that differ (x is the same)')
        self.assertEqual({f['field']: (f['owner'], f['changed'], f['default']) for f in maple['fields']},
                         {'home_area': ('game', 'conflict', 'live'), 'home_x': ('game', 'conflict', 'live'),
                          'home_y': ('game', 'conflict', 'live'), 'greeting': ('authored', 'dev', 'dev')})
        self.assertEqual(conflicts['live.npcs/linden']['default'], 'live', "removed in dev, moved by live's game: live's")
        self.assertEqual(plan['defaults'], {'live.npcs/maple': maple['default'], 'live.npcs/linden': 'live'})
        # An Update from live with no choices: the game's home, dev's words, Linden back.
        result = WS.pull(self.dev, {}, 'Ada', expect=plan['digest'])
        self.assertEqual(sorted(result['defaulted']), ['live.npcs/linden', 'live.npcs/maple'])
        self.assertEqual(self.npc(self.dev, 'maple', 'home_area, home_x, greeting'), ('cottage', 3, 'Dev words.'))
        self.assertEqual(self.npc(self.dev, 'linden'), ('cottage', 7, 7))
        # Import: the same both ways. By default PROD's; chosen, dev's.
        self.sql(self.dev, "UPDATE live.npcs SET home_area = 'shop', home_x = 3, home_y = 4 WHERE id = 'maple'")
        self.sql(self.dev, "UPDATE live.npcs SET home_area = 'shop', home_x = 6, home_y = 6 WHERE id = 'rook'")
        WS.maintenance(self.live, True, 'Ada')
        self.game(self.live, MOVE, ('wren_house', 1, 1, 'maple'))
        self.game(self.live, MOVE, ('wren_house', 5, 1, 'rook'))
        plan = WS.plan_import(self.dev)
        self.assertEqual(sorted(plan['defaults']), ['live.npcs/maple', 'live.npcs/rook'])
        self.assertEqual(plan['errors'], [])
        result = WS.import_(self.dev, {'live.npcs/rook': 'dev'}, 'password', 'Ada', expect=plan['digest'])
        self.assertEqual(result['defaulted'], ['live.npcs/maple'])
        self.assertEqual(self.npc(self.live, 'maple'), ('wren_house', 1, 1), "PROD's by default")
        self.assertEqual(self.npc(self.live, 'rook'), ('shop', 6, 6), "dev's, chosen")
        self.assertEqual(self.npc(self.live, 'maple', 'greeting'), ('Dev words.',), "dev's own words came across")

    HERS = ("SELECT (SELECT count(*) FROM live.npc_voice WHERE npc_id = 'linden'), (SELECT count(*) FROM live.npc_relations "
            "WHERE 'linden' IN (npc_id, other_id)), (SELECT count(*) FROM live.faction_members WHERE npc_id = 'linden')")

    def give_linden_hers(self):
        """Linden's voice, her marriage to Sorrel (both ways) and her place in the watch, in both worlds and the base."""
        for key in (self.live, self.dev):
            self.sql(key, "INSERT INTO live.npc_voice (world_id, npc_id, accent) VALUES ('greyfen', 'linden', 'broad')")
            self.sql(key, "INSERT INTO live.npc_relations (world_id, npc_id, other_id, kind) VALUES "
                          "('greyfen', 'linden', 'sorrel', 'spouse'), ('greyfen', 'sorrel', 'linden', 'spouse')")
            self.sql(key, "INSERT INTO live.factions (world_id, id, position, name, color) VALUES ('greyfen', 'watch', 9, 'The Watch', '#445566') "
                          "ON CONFLICT DO NOTHING")
            self.sql(key, "INSERT INTO live.faction_members (world_id, faction_id, npc_id) VALUES ('greyfen', 'watch', 'linden')")
        WS.init_base(self.dev, 'Ada')
        return self.HERS

    def test_an_npc_dev_removed_and_live_kept_keeps_her_voice_family_and_factions(self):
        hers = self.give_linden_hers()
        self.sql(self.dev, "DELETE FROM live.npcs WHERE id = 'linden'")              # (Her rows go with her in dev.)
        self.sql(self.dev, "UPDATE live.npcs SET greeting = 'Dev words.' WHERE id = 'sloe'")
        self.game(self.live, MOVE, ('cottage', 7, 7, 'linden'))
        WS.maintenance(self.live, True, 'Ada')
        plan = WS.plan_import(self.dev)
        self.assertEqual(plan['defaults'].get('live.npcs/linden'), 'live')
        self.assertEqual({c['id']: c['follows'] for c in plan['devChanges'] if c['follows']},
                         {i: 'live.npcs/linden' for i in ('live.npc_voice/linden', 'live.npc_relations/linden/sorrel/spouse',
                                                          'live.npc_relations/sorrel/linden/spouse', 'live.faction_members/watch/linden')})
        WS.import_(self.dev, {}, 'password', 'Ada', 'Kept Linden')
        self.assertEqual(self.npc(self.live, 'linden'), ('cottage', 7, 7))
        self.assertEqual(self.sql(self.live, hers)[0], (1, 2, 1), 'kept with her, by default')
        self.assertEqual(self.sql(self.dev, hers)[0], (1, 2, 1), 'and dev brought level')
        self.assertEqual(WS.diff(self.dev)['counts'], {'live': 0, 'dev': 0, 'conflicts': 0})
        # Removed in dev again and chosen dev's: she goes, and they with her.
        self.sql(self.dev, "DELETE FROM live.npcs WHERE id = 'linden'")
        self.game(self.live, MOVE, ('cottage', 8, 7, 'linden'))
        WS.import_(self.dev, {'live.npcs/linden': 'dev'}, 'password', 'Ada', 'Linden goes')
        self.assertEqual(self.sql(self.live, hers)[0], (0, 0, 0))

    def test_an_npc_dev_removed_and_live_kept_comes_back_to_dev_with_her_voice_family_and_factions(self):
        # The same, by an Update from live: Linden comes back to dev by default, and hers with her, so a later import
        # never takes them from PROD.
        hers = self.give_linden_hers()
        self.sql(self.dev, "DELETE FROM live.npcs WHERE id = 'linden'")
        self.game(self.live, MOVE, ('cottage', 7, 7, 'linden'))
        WS.maintenance(self.live, True, 'Ada')
        with self.assertRaisesRegex(WS.PublishError, 'Update from live brings dev level'):
            WS.import_(self.dev, {}, 'password', 'Ada', 'Nothing')     # (Dev's only change ends as live has it.)
        plan = WS.diff(self.dev)
        self.assertEqual({c['id']: c['follows'] for c in plan['devChanges'] if c['follows']},
                         {i: 'live.npcs/linden' for i in ('live.npc_voice/linden', 'live.npc_relations/linden/sorrel/spouse',
                                                          'live.npc_relations/sorrel/linden/spouse', 'live.faction_members/watch/linden')})
        result = WS.pull(self.dev, {}, 'Ada', expect=plan['digest'])
        self.assertEqual(result['applied']['live'], {'NPCs': {'added': 1, 'changed': 0, 'removed': 0},
                                                     'faction members': {'added': 1, 'changed': 0, 'removed': 0},
                                                     'NPC voices': {'added': 1, 'changed': 0, 'removed': 0},
                                                     'families': {'added': 2, 'changed': 0, 'removed': 0}})
        self.assertEqual(self.npc(self.dev, 'linden'), ('cottage', 7, 7))
        self.assertEqual(self.sql(self.dev, hers)[0], (1, 2, 1), 'back in dev with her')
        self.assertEqual(WS.diff(self.dev)['counts'], {'live': 0, 'dev': 0, 'conflicts': 0})
        self.sql(self.dev, "UPDATE live.npcs SET greeting = 'Dev words.' WHERE id = 'sloe'")
        WS.import_(self.dev, {}, 'password', 'Ada', 'Words')
        self.assertEqual(self.sql(self.live, hers)[0], (1, 2, 1), 'and never taken from PROD after')
        # Chosen dev's in the pull: she stays gone from dev, and they with her.
        self.sql(self.dev, "DELETE FROM live.npcs WHERE id = 'linden'")
        self.game(self.live, MOVE, ('cottage', 8, 7, 'linden'))
        WS.pull(self.dev, {'live.npcs/linden': 'dev'}, 'Ada')
        self.assertEqual(self.sql(self.dev, hers)[0], (0, 0, 0))

    def test_an_npc_live_removed_and_dev_chosen_comes_back_to_live_with_her_voice_family_and_factions(self):
        # The other way round: PROD removed Linden (her rows went with her there), DEV rewrote her words; DEV's is
        # chosen: she comes back to PROD with her voice, marriage and faction, and DEV keeps them.
        hers = self.give_linden_hers()
        self.sql(self.live, "DELETE FROM live.npcs WHERE id = 'linden'")
        self.sql(self.dev, "UPDATE live.npcs SET greeting = 'Dev words.' WHERE id = 'linden'")
        WS.maintenance(self.live, True, 'Ada')
        plan = WS.plan_import(self.dev)
        self.assertIsNone(plan['defaults'].get('live.npcs/linden'), 'an authored change against a removal: the user chooses')
        self.assertEqual(sorted(c['id'] for c in plan['liveChanges'] if c['follows']),
                         ['live.faction_members/watch/linden', 'live.npc_relations/linden/sorrel/spouse',
                          'live.npc_relations/sorrel/linden/spouse', 'live.npc_voice/linden'])
        WS.import_(self.dev, {'live.npcs/linden': 'dev'}, 'password', 'Ada', 'Linden back')
        self.assertEqual(self.npc(self.live, 'linden', 'greeting'), ('Dev words.',))
        self.assertEqual(self.sql(self.live, hers)[0], (1, 2, 1), 'back in PROD with her')
        self.assertEqual(self.sql(self.dev, hers)[0], (1, 2, 1), 'and kept in dev, brought level')
        self.assertEqual(WS.diff(self.dev)['counts'], {'live': 0, 'dev': 0, 'conflicts': 0})

    def test_a_new_map_with_no_base_keeps_what_prods_game_made(self):
        # The real port: DEV's new map imported into PROD, which DEV has no base of. PROD's game placed Maple and filled
        # Rook's purse; DEV rewrote Sloe's words and re-homed Rook on its new map. Who changed a field of the live game's
        # can't be known: a conflict, PROD's by default and DEV's when chosen; the authored ones are DEV's.
        for key in (self.live, self.dev):
            self.sql(key, "UPDATE live.npcs SET home_area = NULL, home_x = 0, home_y = 0 WHERE id = 'maple'")
        self.game(self.live, ARRIVE, ('longhouse', 3, 3, 'maple'))
        self.game(self.live, "UPDATE live.npcs SET purse = 77 WHERE id = 'rook' RETURNING id")
        self.sql(self.dev, "UPDATE live.npcs SET greeting = 'Dev words.' WHERE id = 'sloe'")
        self.sql(self.dev, "UPDATE live.npcs SET home_area = 'wren_house', home_x = 5, home_y = 5 WHERE id = 'rook'")
        MapTests.new_map(self, self.dev)
        self.sql(self.dev, 'DELETE FROM admin.sync_base_rows')
        self.sql(self.dev, 'DELETE FROM admin.sync_base')
        WS.maintenance(self.live, True, 'Ada', 'A new map')
        plan = WS.plan_import(self.dev)
        self.assertTrue(plan['replacesMap'])
        self.assertTrue(plan['base']['how'].startswith('none'))
        self.assertEqual(plan['liveChanges'], [])
        self.assertIn('live.npcs/sloe', self.ids(plan['devChanges']))
        home = dict.fromkeys(('home_area', 'home_x', 'home_y'), 'live')
        self.assertEqual(plan['defaults'], {'live.npcs/maple': home, 'live.npcs/rook': {**home, 'purse': 'live'}}, "the game's: live's")
        rook = next(c for c in plan['conflicts'] if c['id'] == 'live.npcs/rook')
        self.assertEqual({f['field']: (f['base'], f['dev'], f['live'], f['changed']) for f in rook['fields']},
                         {'home_area': (None, 'wren_house', 'longhouse', 'conflict'), 'home_x': (None, 5, 6, 'conflict'),
                          'home_y': (None, 5, 9, 'conflict'), 'purse': (None, 30, 77, 'conflict')}, 'no base to show')
        # Rook's new home DEV's, chosen; his purse PROD's, and Maple's arrival, by default.
        WS.import_(self.dev, {'live.npcs/rook': {'home_area': 'dev'}}, 'password', 'Ada', 'The new map', expect=plan['digest'],
                   replace_map=True)
        self.assertEqual(self.npc(self.live, 'maple'), ('longhouse', 3, 3), "PROD's arrival kept")
        self.assertEqual(self.npc(self.live, 'rook', 'home_area, home_x, home_y, purse'), ('wren_house', 5, 5, 77))
        self.assertEqual(self.npc(self.live, 'sloe', 'greeting'), ('Dev words.',), "DEV's words came across")
        self.assertEqual(self.npc(self.dev, 'rook', 'home_area, purse'), ('wren_house', 77), 'DEV brought level')
        self.assertEqual(WS.diff(self.dev)['counts'], {'live': 0, 'dev': 0, 'conflicts': 0})

    def test_rows_the_game_made_are_additions_on_the_live_side(self):
        self.game(self.live, BIRTH, ('born_b', 'Bramble', 'longhouse', 'longhouse', 'longhouse'))
        plan = WS.diff(self.dev)
        self.assertEqual([(c['id'], c['side'], c['live'], c['dev']) for c in plan['liveChanges']],
                         [('live.npcs/born_b', 'live', 'added', 'absent')])
        WS.pull(self.dev, {}, 'Ada')
        self.assertEqual(self.npc(self.dev, 'born_b', 'name, origin'), ('Bramble', 'runtime'))
        self.assertEqual(WS.diff(self.dev)['counts'], {'live': 0, 'dev': 0, 'conflicts': 0}, 'in the base now')
        # A row the game made is the game's whole: renamed in dev and moved live, live's by default.
        self.sql(self.dev, "UPDATE live.npcs SET name = 'Bramble Dev' WHERE id = 'born_b'")
        self.game(self.live, "UPDATE live.npcs SET name = 'Bramble Live' WHERE id = 'born_b' RETURNING id")
        conflict = WS.diff(self.dev)['conflicts'][0]
        self.assertEqual((conflict['id'], conflict['default']), ('live.npcs/born_b', {'name': 'live'}))
        self.assertEqual({f['owner'] for f in conflict['fields']}, {'game'})
        WS.maintenance(self.live, True, 'Ada')
        self.sql(self.dev, "UPDATE live.npcs SET greeting = 'Dev.' WHERE id = 'sloe'")
        WS.import_(self.dev, {}, 'password', 'Ada')
        self.assertEqual(self.npc(self.live, 'born_b', 'name'), ('Bramble Live',), 'never removed, never renamed by default')

    def test_a_new_map_lists_the_npcs_whose_places_it_hasnt_and_takes_each_decision(self):
        MapTests.new_map(self, self.dev)
        MapTests.take_the_cottage_away(self)
        # PROD's game: a birth in the cottage, Rook's evenings there; Maple moved there while DEV moved her to the shop.
        self.game(self.live, BIRTH, ('born_c', 'Clover', 'cottage', 'cottage', 'cottage'))
        self.game(self.live, "UPDATE live.npcs SET evening_area = 'cottage', evening_x = 1, evening_y = 1 WHERE id = 'rook' RETURNING id")
        self.game(self.live, MOVE, ('cottage', 2, 2, 'maple'))
        self.sql(self.dev, "UPDATE live.npcs SET home_area = 'shop', home_x = 3, home_y = 3 WHERE id = 'maple'")
        WS.maintenance(self.live, True, 'Ada', 'A new map')
        plan = WS.plan_import(self.dev)
        self.assertTrue(plan['replacesMap'])
        gone = {c['id']: ([(g['place'], g['area']) for g in c['gone']], c['default']) for c in plan['conflicts'] if c['gone']}
        self.assertEqual(gone, {'live.npcs/born_c': ([('home', 'cottage'), ('work', 'cottage'), ('evening', 'cottage')], None),
                                'live.npcs/rook': ([('evening', 'cottage')], None),
                                'live.npcs/maple': ([('home', 'cottage')], None)}, 'a decision each, no default')
        maple = next(c for c in plan['conflicts'] if c['id'] == 'live.npcs/maple')
        self.assertEqual(maple['gone'][0]['dev'], {'cell': 'shop', 'x': 3, 'y': 3}, "dev's place offered")
        self.assertNotIn('cottage', [a['id'] for a in plan['areas']])
        self.assertIn('longhouse', [a['id'] for a in plan['areas']], 'the new map\'s places to pick from')
        with self.assertRaises(WS.Unresolved) as unresolved:
            WS.import_(self.dev, {}, 'password', 'Ada', replace_map=True)
        self.assertEqual(sorted(c['id'] for c in unresolved.exception.conflicts),
                         ['live.npcs/born_c', 'live.npcs/maple', 'live.npcs/rook'])
        choices = {'live.npcs/born_c': {'home': {'cell': 'longhouse', 'x': 3, 'y': 3}, 'work': 'unset', 'evening': 'unset'},
                   'live.npcs/rook': 'unplace', 'live.npcs/maple': {'home': 'dev'}}
        with self.assertRaisesRegex(WS.PublishError, 'cannot be used') as refused:
            WS.import_(self.dev, {**choices, 'live.npcs/rook': 'live'}, 'password', 'Ada', replace_map=True)
        self.assertTrue(any('not on the new map' in e and 'rook' in e for e in refused.exception.errors), refused.exception.errors)
        with self.assertRaisesRegex(WS.PublishError, 'cannot be used'):
            WS.import_(self.dev, {**choices, 'live.npcs/born_c': 'dev'}, 'password', 'Ada', replace_map=True)
        result = WS.import_(self.dev, choices, 'password', 'Ada', 'The new map', expect=plan['digest'], replace_map=True)
        self.assertEqual((result['summary']['letGo']['npcs'], result['summary']['letGo']['slots']), (2, 0))
        self.assertEqual(self.npc(self.live, 'born_c', 'home_area, home_x, home_y, work_area, evening_area'),
                         ('longhouse', 3, 3, None, None), 'a new home picked; no work, no evenings')
        self.assertEqual(self.npc(self.live, 'rook', 'home_area, evening_area'), ('longhouse', None), 'his evenings unset, the rest kept')
        self.assertEqual(self.npc(self.live, 'maple'), ('shop', 3, 3), "dev's place taken")


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
        # the studio writes them, and the rules and the services) queues economy.sync, which the running server applies
        # at once.
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
            # (Doc 68, Progress, "First fixes", migration 0058: the rules' and the services' records are read live too.)
            result = economy.set_rules(self.dev, {'set': {'almsADay': 12, 'fairFood': 0}}, 'Ada')
            self.assertTrue(result['changed'], result)
            result = economy.set_services(self.dev, {'set': {'groom_and_trim': {'on': False, 'every': 9}}}, 'Ada')
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
