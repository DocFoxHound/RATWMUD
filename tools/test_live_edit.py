#!/usr/bin/env python3
"""Live editing: keyed edits, conflicts that name the other editor, whole-batch
refusal, the change feed and presence. Runs in a scratch database dropped
afterwards; skipped when the local PostgreSQL is not running."""
import secrets
import unittest

import live_edit as L
import roster as R
import world_db as W
import world_store as S
from test_world_db import database_available, superuser
from test_world_store import greyfen


def meta(cell):
    """A cell as `cell:` edits carry it: without its ground, which travels as tile edits."""
    return {k: v for k, v in cell.items() if k not in ('terrain', 'heights')}


def glyph(project, x, y):
    """The world tile x, y as the project's cells hold it ('.' where no cell is)."""
    for c in project['cells']:
        if c['x'] <= x < c['x'] + c['width'] and c['y'] <= y < c['y'] + c['height']:
            return c['terrain'][y - c['y']][x - c['x']]
    return '.'


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class LiveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.name = f'ratw_test_{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner)
        cls.conn = W.connect('dev', 'editor', dbname=cls.name)
        S.save_roster(cls.conn, R.load())
        cls.world = L.LiveWorld(lambda: W.connect('dev', 'editor', dbname=cls.name))

    @classmethod
    def tearDownClass(cls):
        cls.conn.close()
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def setUp(self):
        self.conn.execute('DELETE FROM world.worlds')
        self.conn.execute('DELETE FROM world.editors')
        S.save_world(self.conn, greyfen(), None, create=True)
        self.project, self.seq = L.load(self.conn)

    def test_a_lean_load_then_the_ground_of_what_comes_into_view(self):
        """Outlines and previews first; then any cell's ground, heights as rows, exactly as the whole load has it."""
        lean, seq = L.load(self.conn, lean=True)
        self.assertEqual(seq, self.seq)
        cell = lean['cells'][0]
        self.assertIsNone(cell['terrain'])
        self.assertEqual(len(cell['preview']), -(-cell['height'] // S.PREVIEW_STEP))
        self.assertEqual(cell['preview'][0], self.project['cells'][0]['terrain'][0][::S.PREVIEW_STEP])
        # Someone raises a tile; the ground sent afterwards has it, as of that edit.
        whole = self.project['cells'][0]
        x, y = whole['x'] + 2, whole['y'] + 1
        before = S.load_world(self.conn, 'greyfen')[0]['cells'][0]['heights'].get('2,1')
        self.edit((f'height:{x},{y}', before, 2.5))
        cells, ground_seq = L.ground(self.conn, [cell['id'], 'no_such_cell'])
        self.assertEqual(set(cells), {cell['id']})
        self.assertGreater(ground_seq, seq)
        sent = cells[cell['id']]
        self.assertEqual(sent['terrain'], whole['terrain'])
        self.assertEqual(S.decode_heights(sent['heightRows']), {**whole['heights'], '2,1': 2.5})
        with self.assertRaises(ValueError):
            L.ground(self.conn, [])
        with self.assertRaises(ValueError):
            L.ground(self.conn, [f'c{i}' for i in range(L.MAX_GROUND_CELLS + 1)])

    def test_the_host_fills_in_ground_a_lean_editor_never_fetched(self):
        lean, _ = L.load(self.conn, lean=True)
        filled = L.fill_ground(self.conn, lean)
        now = S.load_world(self.conn, 'greyfen')[0]
        self.assertEqual([c['terrain'] for c in filled['cells']], [c['terrain'] for c in now['cells']])
        self.assertEqual([c['heights'] for c in filled['cells']], [c['heights'] for c in now['cells']])
        self.assertTrue(all('preview' not in c for c in filled['cells']))
        # A cell the editor has re-cut keeps what it sent (placeholder or not): its outline no longer matches.
        lean, _ = L.load(self.conn, lean=True)
        lean['cells'][0]['width'] -= 1
        self.assertIsNone(L.fill_ground(self.conn, lean)['cells'][0]['terrain'])

    def test_heights_as_rows(self):
        heights = {'0,0': -16, '3,1': 16, '2,2': .5, '1,0': 0}
        rows = S.encode_heights(heights, 4, 3)
        self.assertEqual(rows, ['0W..', '...*', '..X.'])
        self.assertEqual(S.decode_heights(rows), heights)
        self.assertIsNone(S.encode_heights({'0,0': .25}, 1, 1), 'a height between half steps is sent as it is')
        self.assertIsNone(S.encode_heights({'0,0': 17}, 1, 1))

    def t(self, x, y):
        """A world tile as an edit value: its glyph, or None for plain ground."""
        g = glyph(self.project, x, y)
        return None if g == '.' else g

    def edit(self, *ops, who='Ada', client='a1', label='Test edit'):
        return self.world.edit({'clientId': client, 'editor': who, 'label': label,
                                'ops': [{'key': k, 'before': b, 'after': a} for k, b, a in ops]})

    def test_only_one_world(self):
        with self.assertRaises(Exception):
            self.conn.execute("INSERT INTO world.worlds (id, name) VALUES ('second', 'Second world')")

    def test_tile_edits_and_the_change_feed(self):
        seq = self.edit(('tile:3,3', self.t(3, 3), '~'), ('tile:4,3', self.t(4, 3), '~'))['seq']
        self.assertGreater(seq, self.seq)
        now = L.load(self.conn)[0]
        self.assertEqual(glyph(now, 3, 3) + glyph(now, 4, 3), '~~')
        feed = self.world.sync({'clientId': 'b1', 'editor': 'Bea', 'since': self.seq})
        self.assertEqual([(c['key'], c['after'], c['editor']) for c in feed['changes']],
                         [('tile:3,3', '~', 'Ada'), ('tile:4,3', '~', 'Ada')])
        self.assertEqual(feed['seq'], seq)
        # Painting plain ground back clears the tile.
        self.edit(('tile:3,3', '~', None))
        self.assertEqual(glyph(L.load(self.conn)[0], 3, 3), '.')

    def test_stale_edit_is_refused_and_names_who_changed_it(self):
        self.edit(('tile:5,5', self.t(5, 5), '~'), who='Ada')
        with self.assertRaises(L.Conflict) as raised:
            self.edit(('tile:6,5', self.t(6, 5), 'T'), ('tile:5,5', self.t(5, 5), 'T'), who='Bea', client='b1')
        conflict = raised.exception
        self.assertEqual([(c['key'], c['editor']) for c in conflict.conflicts], [('tile:5,5', 'Ada')])
        self.assertEqual(conflict.current['tile:5,5'], '~')
        # The whole batch was refused: the tile that did not conflict is untouched.
        self.assertEqual(glyph(L.load(self.conn)[0], 6, 5), glyph(self.project, 6, 5))

    def test_entities_round_trip(self):
        person = dict(self.project['people'][0])
        renamed = {**person, 'name': 'Renamed'}
        self.edit((f'person:{person["id"]}', person, renamed))
        self.assertEqual(L.load(self.conn)[0]['people'][0]['name'], 'Renamed')
        route = self.project['routes'][0]
        shorter = {**route, 'posts': route['posts'][:2]}
        self.edit((f'route:{route["id"]}', route, shorter))
        self.assertEqual(L.load(self.conn)[0]['routes'][0]['posts'], route['posts'][:2])
        self.edit((f'person:{person["id"]}', renamed, None))
        self.assertNotIn(person['id'], [p['id'] for p in L.load(self.conn)[0]['people']])

    def test_factions_and_their_claims(self):
        town = meta(self.project['cells'][0])
        watch = {'id': 'watch', 'name': 'Town Watch', 'color': '#aabbcc'}
        claimed = {**town, 'territory': {**town['territory'], 'claims': ['watch']}}
        # A new faction and its claim in one batch (the claim is written after the faction).
        self.edit((f'cell:{town["id"]}', town, claimed), ('faction:watch', None, watch))
        self.assertEqual(self.conn.execute("SELECT faction_id, area, tiles FROM live.faction_claims").fetchall(), [('watch', town['id'], [])])
        self.assertEqual(L.load(self.conn)[0]['cells'][0]['territory']['claims'], ['watch'])
        # Painted tiles (the Dungeon Master) survive editor changes to the place that keep the claim.
        self.conn.execute("UPDATE live.faction_claims SET tiles = '[[1, 1]]'")
        renamed = {**claimed, 'name': 'Renamed'}
        self.edit((f'cell:{town["id"]}', claimed, renamed))
        self.assertEqual(self.conn.execute("SELECT tiles FROM live.faction_claims").fetchone()[0], [[1, 1]])
        # Claims written directly are logged as place edits, so open editors see them.
        room = self.project['rooms'][0]
        seq = L.latest_seq(self.conn)
        L.change_claims(self.conn, 'dm', 'dm-master', 'Claimed', lambda c: c.execute(
            "INSERT INTO live.faction_claims (world_id, faction_id, area, tiles) VALUES ('greyfen', 'watch', %s, '[[2, 2]]')", (room['id'],)))
        feed = self.world.sync({'clientId': 'b1', 'editor': 'Bea', 'since': seq})['changes']
        self.assertEqual([(c['key'], c['after']['territory']['claims'], c['editor']) for c in feed], [(f'room:{room["id"]}', ['watch'], 'dm-master')])
        # Removing the claim in the editor removes the row; deleting the faction removes the rest.
        self.edit((f'cell:{town["id"]}', renamed, {**renamed, 'territory': {**renamed['territory'], 'claims': []}}))
        self.assertEqual(self.conn.execute("SELECT area FROM live.faction_claims").fetchall(), [(room['id'],)])
        self.edit(('faction:watch', watch, None))
        self.assertEqual(self.conn.execute("SELECT count(*) FROM live.faction_claims").fetchone()[0], 0)

    def test_new_interior_with_its_tiles(self):
        meta = {'id': 'cellar', 'name': 'Cellar', 'description': '', 'width': 4, 'height': 4, 'z': -1, 'outdoors': False,
                'weather': 'clear', 'lighting': {'artificial': 0, 'daylightAccess': 0, 'tone': 'cool'},
                'territory': {'region': 'unassigned', 'claims': [], 'chapter': ''}, 'worldX': 0, 'worldY': 0}
        tiles = [(f'rtile:cellar:{x},{y}', None, '#' if x in (0, 3) or y in (0, 3) else '.') for y in range(4) for x in range(4)]
        self.edit(('room:cellar', None, meta), *tiles)
        room = next(r for r in L.load(self.conn)[0]['rooms'] if r['id'] == 'cellar')
        self.assertEqual(room['terrain'], ['####', '#..#', '#..#', '####'])
        self.assertEqual(room['lighting']['tone'], 'cool')

    def test_new_cell_in_world_coordinates_and_no_overlap(self):
        cell = {'id': 'west', 'name': 'West fields', 'description': '', 'x': -32, 'y': 0, 'width': 32, 'height': 24, 'z': 0,
                'outdoors': True, 'weather': 'clear', 'lighting': {'artificial': 1, 'daylightAccess': 1, 'tone': 'warm'},
                'territory': {'region': 'unassigned', 'claims': [], 'chapter': ''}}
        self.edit(('cell:west', None, cell), ('tile:-10,5', None, '~'))
        project = L.load(self.conn)[0]
        west = next(c for c in project['cells'] if c['id'] == 'west')
        self.assertEqual((west['x'], west['y']), (-32, 0))            # World tiles: there is no canvas.
        self.assertEqual(west['terrain'][5][22], '~')
        self.assertEqual(self.conn.execute('SELECT min_x, width FROM world.worlds').fetchone(), (-32, 96), 'bounds follow the cells')
        far = {**cell, 'id': 'far', 'x': 700_000_000, 'y': -9_000_000}
        self.edit(('cell:far', None, far), (f'tile:{far["x"] + 1},{far["y"] + 2}', None, '#'))
        far_cell = next(c for c in L.load(self.conn)[0]['cells'] if c['id'] == 'far')
        self.assertEqual(far_cell['terrain'][2][1], '#')
        with self.assertRaises(L.Rejected) as raised:
            self.edit(('cell:clash', None, {**cell, 'id': 'clash', 'x': -8}))
        self.assertIn('overlap', str(raised.exception))

    def test_removing_a_place_someone_uses_is_refused(self):
        project = self.project
        home = project['people'][0]['home']['cell']
        room = next(r for r in project['rooms'] if r['id'] == home)
        meta = {k: v for k, v in room.items() if k not in ('terrain', 'heights')}
        with self.assertRaises(L.Rejected) as raised:
            self.edit((f'room:{home}', meta, None))
        self.assertIn('removed', str(raised.exception))

    def test_undo_is_just_the_inverse_edit(self):
        self.edit(('tile:7,7', self.t(7, 7), 'T'), who='Ada')
        self.edit(('tile:7,7', 'T', None), who='Ada', label='Undo')
        self.assertEqual(glyph(L.load(self.conn)[0], 7, 7), '.')
        # Undoing after someone else changed the same tile conflicts instead of wiping their work.
        self.edit(('tile:8,8', self.t(8, 8), 'T'), who='Ada')
        self.edit(('tile:8,8', 'T', '#'), who='Bea', client='b1')
        with self.assertRaises(L.Conflict) as raised:
            self.edit(('tile:8,8', 'T', None), who='Ada', label='Undo')
        self.assertEqual(raised.exception.conflicts[0]['editor'], 'Bea')

    def test_presence(self):
        self.world.sync({'clientId': 'a1', 'editor': 'Ada', 'color': '#e6c481', 'since': 0, 'state': {'view': 'shop'}})
        seen = self.world.sync({'clientId': 'b1', 'editor': 'Bea', 'color': '#a8ceda', 'since': self.seq, 'state': {}})
        self.assertEqual([(e['editor'], e['state']) for e in seen['editors']], [('Ada', {'view': 'shop'})])

    def test_an_editor_holding_a_replaced_world_is_told_to_reload(self):
        world = self.world.load()['project']['id']
        self.assertFalse(self.world.sync({'clientId': 'c1', 'since': 0, 'world': world}).get('reload'))
        self.assertTrue(self.world.sync({'clientId': 'c1', 'since': 0, 'world': 'a_world_since_replaced'})['reload'])

    def test_bad_values_are_refused(self):
        with self.assertRaises(L.Rejected):
            self.edit(('tile:1,1', self.t(1, 1), '?'))
        with self.assertRaises(L.Rejected):
            self.edit(('cell:town', meta(self.project['cells'][0]), {**meta(self.project['cells'][0]), 'id': 'other'}))
        with self.assertRaises(ValueError):
            self.world.sync({'since': -1})


if __name__ == '__main__':
    unittest.main()
