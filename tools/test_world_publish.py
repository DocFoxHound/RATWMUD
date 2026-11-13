#!/usr/bin/env python3
"""Publish (tools/world_publish.py; Docs/Design/78-hosts-run-their-own-worlds.md, 10.3, 12.3-12.5; Phase M8): the count
of unpublished map changes, the memory check, the save's checks against a map, the one child that validates and builds
(and leaves nothing when it is refused, runs out of memory or is stopped), Discard, the previous map published again,
the entrypoint's fallback, the build-format number, and the job with its limits."""
import json
import os
import re
import secrets
import tempfile
import time
import unittest
from pathlib import Path

import live_edit
import roster as R
import world_build as B
import world_db as W
import world_publish as P
import world_store as S
from test_world_db import database_available, superuser
from test_world_store import greyfen

DB_TAG = os.environ.get('RATW_TEST_DB_TAG', '')
ROOT = Path(__file__).resolve().parent.parent
WHO = {'username': 'pell#3f2a1c', 'account': 'pell', 'role': 'owner'}


def files_of(grid, doors=()):
    manifest = ['RATW_WORLD 3', 'area "town"'] + list(doors)
    return {'world.ratw': '\n'.join(manifest) + '\n', 'cells/town.cell': 'id: town\ngrid:\n' + '\n'.join(grid) + '\n'}


class PureTests(unittest.TestCase):
    def test_map_keys_wait_for_publish_and_the_live_layers_do_not(self):
        for key in ('tile:3,4', 'height:3,4', 'rtile:shop:1,1', 'room:shop', 'cell:town', 'link:shop_door', 'spawn', 'name',
                    'herb', 'chapter:c1', 'slot:s1'):
            self.assertTrue(P.map_key(key), key)
        for key in ('person:mara', 'route:watch', 'faction:f', 'economy'):
            self.assertFalse(P.map_key(key), key)

    def test_memory_reads_the_cgroup_and_the_kernel(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / 'proc').mkdir()
            (root / 'sys/fs/cgroup').mkdir(parents=True)
            (root / 'proc/meminfo').write_text('MemTotal: 16000000 kB\nMemAvailable:    7340032 kB\n')
            (root / 'sys/fs/cgroup/memory.max').write_text('max\n')
            (root / 'sys/fs/cgroup/memory.current').write_text('900000000\n')
            m = P.memory(5 * P.GIB, folder)
            self.assertEqual((m['free'], m['ok'], m['limit']), (7340032 * 1024, True, None), 'no cgroup limit: MemAvailable')
            self.assertEqual(P.memory_words(m), 'This computer can publish map changes.')
            (root / 'sys/fs/cgroup/memory.max').write_text(str(4 * P.GIB) + '\n')
            m = P.memory(5 * P.GIB, folder)
            self.assertEqual((m['free'], m['ok']), (4 * P.GIB - 900000000, False), 'the cgroup has less left')
            words = P.memory_words(m)
            self.assertIn('can\'t publish yet', words)
            self.assertIn('5.0 GB', words)
            self.assertIn('3.2 GB', words)
        self.assertTrue(P.memory(5 * P.GIB, '/nonexistent')['ok'], 'nothing readable never refuses')

    def test_the_save_against_a_map(self):
        door = 'door "link_d_a" "Shop door" "town" 2.5 1.5 "shop" 1.5 1.5 "link_d_b" 0 0 0 0 "-"'
        view = P.MapView(files=files_of(['#####', '#...#', '#####'], [door]))
        self.assertTrue(view.stands('town', 1.5, 1.5))
        self.assertFalse(view.stands('town', 0.5, 0.5), 'a wall')
        self.assertFalse(view.stands('town', 9.5, 1.5), 'beyond the edge')
        self.assertFalse(view.stands('town', 2.5, 1.5, {}), 'under a closed door')
        self.assertTrue(view.stands('town', 2.5, 1.5), 'a lease or a stall isn\'t held to the doors')
        self.assertTrue(view.stands('town', 2.5, 1.5, {'link_d_a': True}), 'the save has it open')
        saved = {'wolves': [{'id': 'wolf-1', 'name': 'Ada', 'cell': 'town', 'x': 0.5, 'y': 0.5},
                            {'id': 'wolf-2', 'name': 'Bo', 'cell': 'town', 'x': 1.5, 'y': 1.5},
                            {'id': 'wolf-3', 'name': 'Cy', 'cell': 'gone', 'x': 1.5, 'y': 1.5}],
                 'leases': [{'id': 'l1', 'holder': 'wolf-2', 'cell': 'gone', 'kind': 'flat', 'x': None, 'y': None},
                            {'id': 'l2', 'holder': 'wolf-2', 'cell': 'town', 'kind': 'bed', 'x': 0.5, 'y': 1.5},
                            {'id': 'l3', 'holder': 'wolf-2', 'cell': 'town', 'kind': 'bed', 'x': 3.5, 'y': 1.5}],
                 'stalls': [{'id': 's1', 'keeper': 'wolf-2', 'cell': 'gone', 'x': 1, 'y': 1},
                            {'id': 's2', 'keeper': 'wolf-2', 'cell': 'town', 'x': 4.5, 'y': 1.5}],
                 'doors': {'link_d_a': False, 'link_unknown_a': False}}
        errors, warnings = P.saved_problems(view, saved)
        text = ' | '.join(e['text'] for e in errors)
        self.assertEqual(len(errors), 4, text)
        self.assertIn('rents a flat in gone, which this map removes', text)
        self.assertIn('rents a bed in town, and this map builds over its spot', text)
        self.assertIn('keeps a market stall in gone, which this map removes', text)
        self.assertIn('keeps a market stall in town, and this map builds over its spot', text)
        self.assertEqual(errors[1]['cell'], 'town', 'a "Show me" spot')
        self.assertEqual(len(warnings), 1)
        self.assertIn('1 wolf last stood where this map has a wall', warnings[0]['text'])
        self.assertIn('Ada', warnings[0]['text'], 'the wolf in a wall; one in a place gone wakes at the spawn by itself')
        # (R2: a whole place's lease has no bed (Core saves -1, -1): only its place is checked; a name holding U+2028
        # is one line of the manifest still, and a line that won't parse is no door; a save's odd numbers stand nowhere.)
        whole = {**saved, 'stalls': [], 'leases': [{'id': 'l4', 'holder': 'wolf-2', 'cell': 'town', 'kind': 'place', 'x': -1.0, 'y': -1.0}]}
        self.assertEqual(P.saved_problems(view, whole)[0], [])
        odd = 'door "link_o_a" "Old\u2028door" "town" 2.5 1.5 "shop" 1.5 1.5 "link_o_b" 0 0 0 0 "-"\ndoor "broken'
        self.assertEqual(list(P.MapView(files=files_of(['#####', '#...#', '#####'], [odd])).doors), ['link_o_a'])
        self.assertFalse(view.stands('town', float('nan'), 1.5))
        self.assertFalse(view.stands('town', float('inf'), 1.5))
        self.assertEqual((P._float('1.5'), P._float('abc'), P._float(None), P._float('NaN'), P._float('1e400')), (1.5, None, None, None, None))
        steps = door.replace(' 0 0 0 0 "-"', ' 1 0 0 1 "-"')
        errors, _ = P.saved_problems(P.MapView(files=files_of(['#####', '#...#', '#####'], [steps])), {**saved, 'leases': [], 'stalls': []})
        self.assertEqual(len(errors), 1)
        self.assertIn('open steps, which can\'t be closed', errors[0]['text'])

    def test_a_validators_problem_gets_its_place(self):
        """The closing run: "Spawn must be on walkable terrain" came with no "Show me". The validator's words name the
        place; located() finds it in the project."""
        project = {'spawn': {'cell': 'w_1', 'x': 104, 'y': 56}, 'cells': [{'id': 'w_1'}, {'id': 'w_2'}],
                   'rooms': [{'id': 'rm_inn'}], 'links': [{'id': 'rm_inn_door', 'a': {'cell': 'w_2', 'x': 7, 'y': 9}, 'b': {'cell': 'rm_inn', 'x': 1, 'y': 1}}],
                   'people': [{'id': 'rm_ivo', 'name': 'Ivo Tarmarsh', 'home': {'cell': 'rm_inn', 'x': 3, 'y': 1},
                               'work': {'cell': 'w_1', 'x': 92, 'y': 34}, 'evening': {'cell': 'w_2', 'x': 2, 'y': 4}}]}
        at = lambda text: {k: v for k, v in P.located(text, project).items() if k != 'text'}
        self.assertEqual(at('Spawn must be on walkable terrain, away from a link endpoint.'), {'cell': 'w_1', 'x': 104, 'y': 56})
        self.assertEqual(at('World cells w_2 and w_1 overlap.'), {'cell': 'w_2'})
        self.assertEqual(at('rm_inn_door: a has no clear, height-compatible neighboring arrival tile.'), {'cell': 'w_2', 'x': 7, 'y': 9})
        self.assertEqual(at('Person rm_ivo evening: tile is solid.'), {'cell': 'w_2', 'x': 2, 'y': 4})
        self.assertEqual(at('Person rm_ivo: band must be destitute, poor, comfortable, rich or null.'), {'cell': 'w_1', 'x': 92, 'y': 34})
        self.assertEqual(at('economy.rules must be {rule: whole number}.'), {})
        self.assertEqual(P.located('Spawn is outside its cell.', None), {'text': 'Spawn is outside its cell.'})
        self.assertEqual(P.located('Spawn is outside its cell.', {'spawn': 7, 'cells': 'x'}), {'text': 'Spawn is outside its cell.'})

    def test_the_build_format_is_the_servers(self):
        header = (ROOT / 'Core' / 'RatwGame.h').read_text(encoding='utf-8')
        core = int(re.search(r'WorldBuildFormat = (\d+);', header).group(1))
        self.assertEqual(B.BUILD_FORMAT, core, 'tools/world_build.py and Core/RatwGame.h name one build format')
        self.assertEqual(B.READS, tuple(f for f in (core, core - 1) if f >= 1), 'the current format and the one before')
        self.assertIn('format == WorldBuildFormat || format == WorldBuildFormat - 1', header)


class Hosted:
    folder = None

    def __init__(self, host=None, server=None):
        self.host, self.up = host, server

    def host_account(self):
        return self.host

    def server(self):
        return self.up


@unittest.skipUnless(database_available(), 'local PostgreSQL not running (python3 tools/world_db.py up)')
class PublishTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.name = f'ratw_test_{DB_TAG}{secrets.token_hex(4)}'
        with superuser() as su:
            su.execute(f'CREATE DATABASE {cls.name} OWNER ratw_owner')
            su.execute(f'GRANT CONNECT ON DATABASE {cls.name} TO ratw_editor, ratw_dm, ratw_game')
        with W.connect('dev', 'owner', dbname=cls.name) as owner:
            W.migrate(owner)
        cls.conn = W.connect('dev', 'editor', dbname=cls.name)
        S.save_roster(cls.conn, R.load())

    @classmethod
    def tearDownClass(cls):
        cls.conn.close()
        with superuser() as su:
            su.execute(f'DROP DATABASE {cls.name} WITH (FORCE)')

    def setUp(self):
        self.conn.execute('DELETE FROM world.worlds')
        with self.owner() as owner:
            owner.execute('DELETE FROM dm.audit')
            owner.execute("UPDATE admin.server_state SET state = 'running', note = ''")
        S.save_world(self.conn, greyfen(), None, create=True)
        self.first, _ = B.build(self.conn, 'test')

    def owner(self):
        return W.connect('dev', 'owner', dbname=self.name)

    def editor(self):
        return W.connect('dev', 'editor', dbname=self.name)

    def dm(self):
        return W.connect('dev', 'dm', dbname=self.name)

    def edit(self, key, after):
        """One of Atlas's edits; a callable `after` is given the value now."""
        with self.conn.transaction():
            now = live_edit.Batch(self.conn, live_edit.world_row(self.conn)).current(key)
        value = after(now) if callable(after) else after
        live_edit.apply_edit(self.conn, 'test', 'pell', 'a test edit', [{'key': key, 'before': now, 'after': value}])
        return now

    def now(self, key):
        with self.conn.transaction():
            return live_edit.Batch(self.conn, live_edit.world_row(self.conn)).current(key)

    def builds(self):
        return self.conn.execute('SELECT count(*) FROM world.builds').fetchone()[0]

    def test_the_count_and_the_child(self):
        self.assertEqual(P.unpublished(self.conn)['count'], 0, 'a fresh build: nothing unpublished')
        self.edit('tile:30,22', '#')
        self.conn.execute("""INSERT INTO world.edits (world_id, batch, editor, client_id, label, key, before, after)
                             VALUES ('greyfen', 1, 'pell', 'test', 'an NPC', 'person:mara', 'null', 'null')""")
        count = P.unpublished(self.conn)
        self.assertEqual((count['count'], count['last']['by']), (1, 'pell'), 'the map edit counts; the NPC\'s is live')
        with self.owner() as owner:                             # (A wolf, away, on the tile the edit walled.)
            owner.execute("""INSERT INTO game.characters (world_id, key, position, data) VALUES ('greyfen', 'wolf-1', 0,
                             '{"id": "wolf-1", "name": "Ada", "cell": "town", "x": 30.5, "y": 22.5}')""")
        steps = []
        result = P.run_child('dev', 'Editor: pell', steps.append, dbname=self.name)
        self.assertTrue(result['ok'], result)
        self.assertEqual({'checking', 'loading', 'validating', 'building', 'storing'} - {s.get('step') for s in steps}, set())
        self.assertEqual(len(result['warnings']), 1, result)
        self.assertIn('Ada', result['warnings'][0]['text'])
        build = P.newest_build(self.conn)
        self.assertEqual((build['id'], build['format'], build['by']), (result['build'], B.BUILD_FORMAT, 'Editor: pell'))
        self.assertEqual(P.unpublished(self.conn)['count'], 0, 'published: the count starts again')
        self.assertEqual(P.newest(self.conn), {'build': build['id'], 'format': B.BUILD_FORMAT, 'readable': True, 'unpublished': 0,
                                               'reads': list(B.READS)})
        body = self.conn.execute("SELECT body FROM world.build_cells WHERE build_id = %s AND cell_id = 'town'", (build['id'],)).fetchone()[0]
        self.assertEqual(body.partition('\ngrid:\n')[2].splitlines()[22][30], '#', 'the build holds the edit')

    def test_a_refused_starved_or_stopped_child_leaves_nothing(self):
        self.edit('tile:30,22', '#')
        with self.owner() as owner:                             # (A lease in a place the map doesn't have.)
            owner.execute("""INSERT INTO game.lodgings (world_id, key, position, data) VALUES ('greyfen', 'l1', 0,
                             '{"id": "l1", "holder": "wolf-1", "cell": "old_inn", "kind": "flat"}')""")
        result = P.run_child('dev', 'Editor: pell', dbname=self.name)
        self.assertEqual((result['ok'], result['code']), (False, 3), result)
        self.assertIn('rents a flat in old_inn, which this map removes', result['errors'][0]['text'])
        self.assertEqual(self.builds(), 1, 'refused: no build')
        with self.owner() as owner:
            owner.execute('DELETE FROM game.lodgings')
        # A world the validator refuses (its starting place walled in): the Atlas validator's own words, and no build.
        spawn = greyfen()['spawn']
        self.edit(f'tile:{spawn["x"]},{spawn["y"]}', '#')
        result = P.run_child('dev', 'Editor: pell', dbname=self.name)
        self.assertEqual((result['ok'], result['code']), (False, 3), result)
        self.assertTrue(result['errors'] and all(e['text'] for e in result['errors']), result)
        self.assertEqual(self.builds(), 1)
        self.edit(f'tile:{spawn["x"]},{spawn["y"]}', None)
        # Out of memory under its RLIMIT_AS: it dies alone, and nothing is stored.
        result = P.run_child('dev', 'Editor: pell', dbname=self.name, env={**os.environ, 'RATW_PUBLISH_RLIMIT_MB': '96'})
        self.assertFalse(result['ok'], result)
        self.assertIn('othing was changed', result['errors'][0]['text'])
        self.assertEqual(self.builds(), 1, 'a starved build leaves nothing')
        # Stopped at its time limit.
        result = P.run_child('dev', 'Editor: pell', dbname=self.name, seconds=0.05)
        self.assertFalse(result['ok'])
        self.assertIn('was stopped: nothing was changed', result['errors'][0]['text'])
        self.assertEqual(self.builds(), 1, 'a killed build leaves nothing')
        self.assertEqual(P.unpublished(self.conn)['count'], 3, 'and the edits are still unpublished')

    def test_discard(self):
        was_tile = self.edit('tile:30,22', '#')
        was_cell = self.edit('cell:town', lambda c: {**c, 'name': 'Greyfen Ford'})
        self.edit('tile:30,22', '~')
        self.assertEqual(P.unpublished(self.conn)['count'], 3)
        out = P.discard(self.conn, 'pell')
        self.assertEqual(out, {'discarded': 3, 'keys': 2})
        self.assertEqual(self.now('tile:30,22'), was_tile)
        self.assertEqual(self.now('cell:town')['name'], was_cell['name'])
        self.assertEqual(P.unpublished(self.conn)['count'], 0, 'the source is the build\'s again')
        self.assertEqual(P.discard(self.conn, 'pell'), {'discarded': 0, 'keys': 0})
        # Changed some other way since: the whole discard is refused, and nothing is put back.
        self.edit('tile:31,22', '#')
        self.edit('cell:town', lambda c: {**c, 'name': 'Greyfen Ford'})
        self.conn.execute("UPDATE world.cells SET name = 'Somewhere Else' WHERE id = 'town'")
        with self.assertRaises(P.Refused) as refused:
            P.discard(self.conn, 'pell')
        self.assertIn('changed again some other way', str(refused.exception))
        self.assertEqual(self.now('tile:31,22'), '#', 'nothing was discarded')

    def test_the_previous_map_and_the_fallback(self):
        self.assertIsNone(B.previous(self.conn), 'one build: nothing to go back to')
        self.assertEqual(P.fallback(self.conn), {'failed': self.first, 'now': None, 'why': 'no previous map'})
        self.edit('tile:30,22', '#')
        second, cells = B.build(self.conn, 'test')
        self.assertEqual(B.previous(self.conn), self.first)
        with self.conn.transaction(), self.dm() as dm:
            self.assertEqual(P.back_problems(self.conn, dm, self.first), ([], []))
        # (The closing run: a resident given a home since, where the previous map has a wall or no such place, would
        # stop the game server loading the world on it: going back says who, first.)
        home = self.conn.execute('SELECT id, home_area, home_x, home_y FROM live.npcs ORDER BY id LIMIT 1').fetchone()
        self.conn.execute("UPDATE live.npcs SET home_area = 'town', home_x = 30, home_y = 22 WHERE id = %s", (home[0],))
        view = P.MapView(conn=self.conn, build_id=second)
        found = P.residents_problems(self.conn, view, 'greyfen')
        self.assertEqual(len(found), 1, found)
        self.assertIn('where the previous map has a wall', found[0]['text'])
        self.assertEqual((found[0]['cell'], found[0]['x'], found[0]['y']), ('town', 30, 22))
        with self.conn.transaction(), self.dm() as dm:
            self.assertEqual(P.back_problems(self.conn, dm, self.first), ([], []), 'the first map has ground there')
        self.conn.execute('UPDATE live.npcs SET home_area = %s, home_x = %s, home_y = %s WHERE id = %s', (*home[1:], home[0]))
        self.assertEqual(P.residents_problems(self.conn, view, 'greyfen'), [])
        out = P.fallback(self.conn)                             # (The second build "didn't start".)
        self.assertEqual(out['failed'], second)
        third = out['now']
        row = self.conn.execute('SELECT copy_of, created_by, format, edits_through FROM world.builds WHERE id = %s', (third,)).fetchone()
        self.assertEqual(row[:3], (self.first, 'fallback', B.BUILD_FORMAT))
        self.assertTrue(self.conn.execute('SELECT failed FROM world.builds WHERE id = %s', (second,)).fetchone()[0])
        self.assertEqual(self.conn.execute('SELECT count(*) FROM world.build_cells WHERE build_id = %s', (third,)).fetchone()[0], cells)
        old = self.conn.execute("SELECT body FROM world.build_cells WHERE build_id = %s AND cell_id = 'town'", (third,)).fetchone()[0]
        self.assertNotEqual(old.partition('\ngrid:\n')[2].splitlines()[22][30], '#', 'the previous map, as it was')
        self.assertEqual(P.unpublished(self.conn)['count'], 1, 'the edit is kept, unpublished')
        self.assertIsNone(B.previous(self.conn), 'going back twice never goes forward to the failed map')
        self.assertTrue(P.newest(self.conn)['readable'])
        # (R2: after two bad maps in a row the one before the newest is bad too: the fallback goes to the map the server
        # last ran well on, when the entrypoint names it and it is still there.)
        self.edit('tile:31,22', '#')
        fourth, _ = B.build(self.conn, 'test')
        fifth, _ = B.build(self.conn, 'test')
        out = P.fallback(self.conn, good=third)
        self.assertEqual((out['failed'], self.conn.execute('SELECT copy_of FROM world.builds WHERE id = %s', (out['now'],)).fetchone()[0]),
                         (fifth, self.first), 'the good map (itself a copy of the first), not the fourth')
        self.assertEqual(P.fallback(self.conn, good=out['now'])['why'], 'no previous map', 'never onto the map that just failed')
        third = out['now']
        self.conn.execute('UPDATE world.builds SET format = %s WHERE id = %s', (B.BUILD_FORMAT + 2, third))
        self.assertFalse(P.newest(self.conn)['readable'], 'a format this server doesn\'t load')

    def publisher(self, run=None, hosted=None, backup=lambda: None, free=8 * P.GIB):
        def log(who, action, target, detail):
            with self.dm() as conn:
                conn.execute('INSERT INTO dm.audit (at, action, target, detail, who) VALUES (%s, %s, %s, %s, %s)',
                             (time.time(), action, target, detail, who['username']))

        def built(world, by, on_step):
            on_step({'step': 'building', 'done': 1, 'of': 2})
            build, cells = B.build(self.conn, by)
            return {'ok': True, 'code': 0, 'build': build, 'cells': cells, 'errors': [], 'warnings': []}
        return P.Publisher('dev', self.editor, self.dm, hosted=hosted or Hosted(), run=run or built, request_backup=backup,
                           sleep=lambda s: None, log=log,
                           read_memory=lambda: {'need': 5 * P.GIB, 'free': free, 'limit': None, 'ok': free >= 5 * P.GIB})

    def finished(self, publisher):
        publisher.thread.join(30)
        return publisher.state()['job']

    def server_state(self):
        return self.conn.execute('SELECT state FROM admin.server_state').fetchone()[0]

    def test_the_job(self):
        pub = self.publisher()
        state = pub.state()
        self.assertEqual((state['unpublished']['count'], state['job'], state['serverState'], state['previous']), (0, None, 'running', None))
        self.assertEqual(state['hostOnly']['available'], False)
        self.assertIn('host token', state['hostOnly']['reason'])
        self.assertEqual(pub.summary()['unpublished'], 0)
        with self.assertRaises(P.Refused) as refused:
            pub.start(WHO, 'host')
        self.assertEqual(refused.exception.status, 409, 'open only to me needs a host the world knows')
        with self.assertRaises(P.Refused) as refused:
            self.publisher(free=3 * P.GIB).start(WHO, 'later')
        self.assertIn('needs about 5.0 GB of memory free; this world has 3.0 GB', str(refused.exception))
        with self.assertRaises(P.Refused):
            pub.start(WHO, 'whenever')
        self.edit('tile:30,22', '#')
        pub.start(WHO, 'later')
        job = self.finished(pub)
        self.assertEqual((job['step'], job['by'], job['restart']), ('done', 'pell', 'later'), job)
        self.assertEqual(self.server_state(), 'running', 'when everyone has left: the state is left alone')
        self.assertEqual(pub.state()['previous'], self.first)
        # Now, with a minute's warning: closed, then opened again (no entrypoint here to wait for).
        pub.start(WHO, 'now')
        self.assertEqual(self.finished(pub)['step'], 'done')
        self.assertEqual(self.server_state(), 'running')
        # Open only to me, then open to everyone.
        hosted = self.publisher(hosted=Hosted(host='acct-' + 'a' * 32))
        self.assertTrue(hosted.state()['hostOnly']['available'])
        hosted.start(WHO, 'host')
        self.assertEqual(self.finished(hosted)['step'], 'done')
        self.assertEqual(self.server_state(), 'host_only')
        self.assertEqual(hosted.open_all(WHO), {'ok': True})
        self.assertEqual(self.server_state(), 'running')
        with self.assertRaises(P.Refused):
            hosted.open_all(WHO)
        # No backup, no Publish; a refused build says why.
        none = self.publisher(backup=lambda: 'This world folder has no backup service.')
        none.start(WHO, 'later')
        job = self.finished(none)
        self.assertEqual((job['step'], job['errors'][0]['text']), ('refused', 'This world folder has no backup service.'))
        bad = self.publisher(run=lambda *a: {'ok': False, 'code': 3, 'errors': [{'text': 'A wall.', 'cell': 'town'}], 'warnings': []})
        bad.start(WHO, 'later')
        job = self.finished(bad)
        self.assertEqual((job['step'], job['errors'][0]['cell']), ('refused', 'town'))
        # Six an hour an account (the five above and one more); another account still may.
        pub.start(WHO, 'later')
        self.finished(pub)
        with self.assertRaises(P.Refused) as refused:
            pub.start(WHO, 'later')
        self.assertEqual(refused.exception.status, 429)
        pub.start({'username': 'wren#aa11bb', 'role': 'builder'}, 'later')
        self.assertEqual(self.finished(pub)['by'], 'wren')
        # The update rebuild's answer: only for a map this server can't load, outside the hourly count (the world is
        # down until then), and nothing discarded when it can't start.
        with self.assertRaises(P.Refused) as refused:
            pub.rebuild(WHO)
        self.assertIn('doesn\'t need rebuilding', str(refused.exception))
        self.conn.execute('UPDATE world.builds SET format = %s WHERE id = (SELECT max(id) FROM world.builds)', (B.BUILD_FORMAT + 2,))
        self.edit('tile:31,22', '#')
        short = self.publisher(free=3 * P.GIB)
        with self.assertRaises(P.Refused):
            short.rebuild(WHO, True)
        self.assertEqual(P.unpublished(self.conn)['count'], 1, 'short of memory: nothing was discarded')
        pub.rebuild(WHO, True)
        job = self.finished(pub)
        self.assertEqual((job['step'], job['rebuild']), ('done', True), job)
        self.assertNotEqual(self.now('tile:31,22'), '#', 'discarded first, then rebuilt')
        self.assertTrue(P.newest(self.conn)['readable'])

    def test_closing_and_opening_stay_the_owners(self):
        """R2: /api/dm/server/maintenance is the owner's. A builder's Publish "now" (or going back) on a world its owner
        closed leaves it closed, and on one open only to its host leaves it so; the owner's own Publish opens it."""
        builder = {'username': 'wren#aa11bb', 'role': 'builder'}
        pub = self.publisher(hosted=Hosted(host='acct-' + 'a' * 32))
        with self.owner() as owner:
            owner.execute("UPDATE admin.server_state SET state = 'maintenance', note = 'Closed by the owner'")
        self.edit('tile:30,22', '#')
        pub.start(builder, 'now')
        job = self.finished(pub)
        self.assertEqual((job['step'], self.server_state()), ('done', 'maintenance'), job)
        self.assertIn('stays closed', job['warnings'][-1]['text'])
        pub.back(builder)
        pub.thread and pub.thread.join(5)
        time.sleep(0.3)
        self.assertEqual(self.server_state(), 'maintenance', 'going back opens nothing either')
        with self.owner() as owner:
            owner.execute("UPDATE admin.server_state SET state = 'host_only'")
        pub.start(builder, 'now')
        job = self.finished(pub)
        self.assertEqual(self.server_state(), 'host_only', job)
        self.assertIn('stays open only to its host', job['warnings'][-1]['text'])
        with self.owner() as owner:
            owner.execute("UPDATE admin.server_state SET state = 'maintenance'")
        pub.start(WHO, 'now')
        self.assertEqual((self.finished(pub)['step'], self.server_state()), ('done', 'running'), 'the owner\'s own')

    def test_a_failed_audit_row_leaves_no_job(self):
        """R2: the job was set before its audit row, so a failed write left a job nobody ran and every later Publish
        refused as busy."""
        pub = self.publisher()
        pub.log = lambda *a: (_ for _ in ()).throw(RuntimeError('no audit'))
        with self.assertRaises(RuntimeError):
            pub.start(WHO, 'later')
        self.assertIsNone(pub.job)

    def test_going_back_and_live_edits_ahead_of_the_map(self):
        pub = self.publisher()
        with self.assertRaises(P.Refused):
            pub.back(WHO)
        self.edit('tile:30,22', '#')
        second, _ = B.build(self.conn, 'test')
        self.edit('tile:31,22', '#')
        out = pub.back(WHO, True)
        self.assertTrue(out['ok'])
        self.assertEqual(self.conn.execute('SELECT copy_of FROM world.builds WHERE id = %s', (out['build'],)).fetchone()[0], self.first)
        self.assertEqual((out['discarded'], P.unpublished(self.conn)['count']), (2, 0), 'both edits since the previous map, discarded')
        self.assertNotEqual(self.now('tile:30,22'), '#')
        # A live edit naming a place the running map lacks: "Publish the map first."
        self.assertIsNone(pub.ahead({'kind': 'npc.move', 'home': {'cell': 'town', 'x': 3}}))
        self.conn.execute("INSERT INTO world.areas (world_id, id, kind) VALUES ('greyfen', 'new_inn', 'interior')")
        self.conn.execute("""INSERT INTO world.edits (world_id, batch, editor, client_id, label, key, before, after)
                             VALUES ('greyfen', 2, 'pell', 'test', 'a room', 'room:new_inn', 'null', '{}')""")
        self.assertEqual(pub.ahead({'kind': 'npc.move', 'home': {'cell': 'new_inn', 'x': 3}}), 'new_inn')
        self.assertIsNone(pub.ahead({'note': 'the new inn'}))
        # The map editor's own edits of residents and routes, to the tile (the closing run: Ivo's evening in a room not
        # published, accepted, stopped the game server at its next start).
        person = lambda **places: [{'key': 'person:ivo', 'before': None, 'after': {'id': 'ivo', 'name': 'Ivo Tarmarsh', **places}}]
        self.assertIn('Ivo Tarmarsh would be in new_inn', pub.ahead_ops(person(evening={'cell': 'new_inn', 'x': 2, 'y': 2})))
        grid = self.conn.execute("SELECT body FROM world.build_cells WHERE build_id = %s AND cell_id = 'town'",
                                 (P.newest_build(self.conn)['id'],)).fetchone()[0].partition('\ngrid:\n')[2].split('\n')
        wall = next((x, y) for y, row in enumerate(grid) for x, g in enumerate(row) if g == '#')
        open_ = next((x, y) for y, row in enumerate(grid) for x, g in enumerate(row) if g == '.')
        self.assertIn('has a wall or no ground', pub.ahead_ops(person(home={'cell': 'town', 'x': wall[0], 'y': wall[1]})))
        self.assertIsNone(pub.ahead_ops(person(home={'cell': 'town', 'x': open_[0], 'y': open_[1]})))
        self.assertIn('would be in new_inn', pub.ahead_ops([{'key': 'route:beat', 'before': None, 'after': {
            'id': 'beat', 'name': 'The beat', 'posts': [{'cell': 'town', 'x': open_[0], 'y': open_[1]}, {'cell': 'new_inn', 'x': 1, 'y': 1}]}}]))
        self.assertIsNone(pub.ahead_ops([{'key': 'room:new_inn', 'before': None, 'after': {'id': 'new_inn'}},
                                         {'key': 'tile:1,1', 'before': None, 'after': '#'},
                                         {'key': 'person:ivo', 'before': {'home': {'cell': 'new_inn', 'x': 1, 'y': 1}}, 'after': None}]),
                          'map edits wait for Publish; a resident removed stands nowhere')
        self.assertIsNone(pub.ahead_ops('nonsense'))


if __name__ == '__main__':
    unittest.main()
