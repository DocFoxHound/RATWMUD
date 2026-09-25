#!/usr/bin/env python3
"""Offline model/export/HTTP safety tests for the authoring tool."""
import copy
import http.client
import io
import json
from pathlib import Path
import shlex
import tempfile
import threading
import unittest
import zipfile

import map_editor as editor


def flat():
    p = editor.demo_project()
    p['terrain'] = ['.' * 64 for _ in range(48)]
    return p


def interior(p):
    p['rooms'].append(dict(id='inn', name='The bent branch', description='Quiet\ninterior.',
                           width=16, height=12, terrain=['.' * 16 for _ in range(12)],
                           heights={}, worldX=0, worldY=0, z=1, outdoors=False, weather='clear'))
    p['links'].append(dict(id='inn_entry', name='Inn door', kind='door', open=False,
                           a=dict(cell='field_1', x=10, y=10), b=dict(cell='inn', x=7, y=10)))
    return p


class ExportTests(unittest.TestCase):
    def political(self):
        p = interior(flat())
        p['factions'] = [dict(id='north', name='North Wardens', color='#6688AA'),
                         dict(id='south', name='South Wardens', color='#CC9988')]
        p['chapters'] = [dict(id='hearth', name='Hearth Chapter')]
        return p

    def test_territory_legacy_export_unchanged(self):
        p = flat()
        original = editor.export_files(p)
        p['factions'] = []
        p['chapters'] = []
        for c in p['cells']:
            c['territory'] = dict(region='unassigned', claims=[], chapter='')
        explicit = editor.export_files(p)
        for path in original:
            if path != 'atlas.json':
                self.assertEqual(original[path], explicit[path])
        self.assertNotIn('territory ', original['world.ratw'])
        self.assertNotIn('faction ', original['world.ratw'])
        self.assertNotIn('chapter ', original['world.ratw'])

    def test_territory_catalog_export_roundtrip_and_neutral_geometry(self):
        p = self.political()
        baseline = editor.export_files(interior(flat()))
        p['cells'][0]['territory'] = dict(region='north_reach', claims=['south', 'north'], chapter='hearth')
        p['rooms'][0]['territory'] = dict(region='north_reach', claims=['north'], chapter='hearth')
        p['cells'][1]['territory'] = dict(region='unclaimed_reach', claims=[], chapter='')
        before = copy.deepcopy(p)
        files = editor.export_files(p)
        self.assertEqual(p, before)
        rows = [shlex.split(line) for line in files['world.ratw'].splitlines()]
        self.assertEqual(rows[1], ['faction', 'north', 'North Wardens', '#6688AA'])
        self.assertEqual(rows[3], ['chapter', 'hearth', 'Hearth Chapter'])
        territories = [row for row in rows if row[0] == 'territory']
        self.assertEqual(territories, [
            ['territory', 'field_1', 'north_reach', 'hearth', '2', 'north', 'south'],
            ['territory', 'field_2', 'unclaimed_reach', '-', '0'],
            ['territory', 'inn', 'north_reach', 'hearth', '1', 'north']])
        for path in baseline:
            if path.startswith('cells/'):
                self.assertEqual(baseline[path], files[path])
        self.assertEqual([line for line in files['world.ratw'].splitlines() if line.startswith(('door ', 'spawn '))],
                         [line for line in baseline['world.ratw'].splitlines() if line.startswith(('door ', 'spawn '))])
        reopened = json.loads(files['atlas.json'])
        self.assertEqual(reopened['cells'][0]['territory']['claims'], ['north', 'south'])
        self.assertEqual(editor.export_files(reopened), files)

    def test_territory_catalog_malformed_inputs(self):
        faction = dict(id='north', name='North', color='#112233')
        chapter = dict(id='hearth', name='Hearth')
        for bad in [None, {}, True, 'x', [None], [True], [{}], [dict(faction, id='../bad')],
                    [dict(faction, id='a' * 49)], [dict(faction, id=False)], [dict(faction, name=' ')],
                    [dict(faction, name='\ud800')], [dict(faction, name='bad\nline')],
                    [dict(faction, color='#123')], [dict(faction, color='red')],
                    [dict(faction, color='#abcdef00')], [dict(faction, color=True)],
                    [dict(faction, color='#gggggg')], [dict(faction, extra=1)], [faction, faction]]:
            p = flat()
            p['factions'] = bad
            self.bad(p)
        for bad in [None, {}, True, [None], [{}], [dict(chapter, name='')], [dict(chapter, id=4)],
                    [dict(chapter, color='#ffffff')], [chapter, chapter]]:
            p = flat()
            p['chapters'] = bad
            self.bad(p)

    def test_territory_shape_references_and_duplicates(self):
        valid = dict(region='reach', claims=['north'], chapter='hearth')
        for bad in [None, [], True, {}, dict(valid, region=''), dict(valid, region='../reach'),
                    dict(valid, region=True), dict(valid, region='a' * 49), dict(valid, claims=None),
                    dict(valid, claims='north'), dict(valid, claims=['north', 'north']),
                    dict(valid, claims=[False]), dict(valid, claims=[{}]), dict(valid, claims=['absent']),
                    dict(valid, chapter=True), dict(valid, chapter='absent'), dict(valid, chapter=None),
                    dict(valid, extra=1), dict(region='reach', claims=[])]:
            for group in ['cells', 'rooms']:
                p = self.political()
                p[group][0]['territory'] = bad
                self.bad(p)

    def test_territory_catalog_limits_and_full_claims(self):
        p = flat()
        p['factions'] = [dict(id=f'f_{i}', name=f'Faction {i}', color='#112233') for i in range(64)]
        p['chapters'] = [dict(id=f'c_{i}', name=f'Chapter {i}') for i in range(128)]
        p['cells'][0]['territory'] = dict(region='a' * 48, claims=[f['id'] for f in p['factions']], chapter='c_127')
        files = editor.export_files(p)
        row = next(shlex.split(row) for row in files['world.ratw'].splitlines() if row.startswith('territory '))
        self.assertEqual(row[4], '64')
        self.assertEqual(len(row), 69)
        bad = copy.deepcopy(p)
        bad['factions'].append(dict(id='extra', name='Extra', color='#123456'))
        self.bad(bad, '64')
        bad = copy.deepcopy(p)
        bad['chapters'].append(dict(id='extra', name='Extra'))
        self.bad(bad, '128')
        bad = copy.deepcopy(p)
        bad['cells'][0]['territory']['claims'].append('f_0')
        self.bad(bad, '64')

    def test_territory_catalog_escaping_and_independent_namespaces(self):
        p = flat()
        name = 'The "North" \\ Hearth 🐺'
        p['factions'] = [dict(id='field_1', name=name, color='#abcdef')]
        p['chapters'] = [dict(id='field_1', name=name)]
        p['cells'][0]['territory'] = dict(region='field_1', claims=['field_1'], chapter='field_1')
        rows = [shlex.split(row) for row in editor.export_files(p)['world.ratw'].splitlines()]
        self.assertEqual(rows[1], ['faction', 'field_1', name, '#abcdef'])
        self.assertEqual(rows[2], ['chapter', 'field_1', name])
        self.assertEqual(rows[4], ['territory', 'field_1', 'field_1', 'field_1', '1', 'field_1'])

    def test_territory_unknown_catalog_deletion_rejected(self):
        for key in ['factions', 'chapters']:
            p = self.political()
            p['rooms'][0]['territory'] = dict(region='reach', claims=['north'], chapter='hearth')
            del p[key]
            self.bad(p, 'unknown')

    def test_lighting_export_legacy_and_explicit(self):
        p = interior(flat())
        files = editor.export_files(p)
        self.assertIn('lighting: 1 1 warm\n', files['cells/inn.cell'])
        p['rooms'][0]['lighting'] = dict(artificial=0, daylightAccess=0, tone='neutral')
        p['cells'][0]['lighting'] = dict(artificial=.5, daylightAccess=.25, tone='cool')
        files = editor.export_files(p)
        self.assertIn('lighting: 0 0 neutral\n', files['cells/inn.cell'])
        self.assertIn('lighting: 0.5 0.25 cool\n', files['cells/field_1.cell'])
        self.assertEqual(json.loads(files['atlas.json'])['rooms'][0]['lighting'], p['rooms'][0]['lighting'])
        self.assertEqual(editor.export_files(json.loads(files['atlas.json'])), files)

    def test_lighting_validation(self):
        valid = dict(artificial=.7, daylightAccess=.3, tone='warm')
        for bad in [None, [], True, {}, dict(valid, artificial=True), dict(valid, artificial='1'),
                    dict(valid, artificial=float('nan')), dict(valid, artificial=float('inf')),
                    dict(valid, artificial=-.1), dict(valid, daylightAccess=1.1),
                    dict(valid, tone='red'), dict(valid, unexpected=1)]:
            p = flat()
            p['cells'][0]['lighting'] = bad
            with self.assertRaises(editor.ValidationError):
                editor.export_files(p)

    def bad(self, p, contains=None):
        before = copy.deepcopy(p)
        with self.assertRaises(editor.ValidationError) as raised:
            editor.export_files(p)
        if contains:
            self.assertIn(contains, str(raised.exception))
        self.assertEqual(p, before)

    def test_demo_and_deterministic_roundtrip(self):
        p = editor.demo_project()
        before = copy.deepcopy(p)
        files = editor.export_files(p)
        self.assertEqual(files, editor.export_files(p))
        self.assertEqual(p, before)
        self.assertEqual(json.loads(files['atlas.json']), p)
        self.assertEqual(len([f for f in files if f.endswith('.cell')]), 4)
        self.assertTrue(files['world.ratw'].startswith('RATW_WORLD 1\n'))

    def test_flat_full_seams_reciprocal(self):
        files = editor.export_files(flat())
        lines = [shlex.split(row) for row in files['world.ratw'].splitlines() if row.startswith('door ')]
        # Vertical seam48 + horizontal seam64; two directional fixtures each.
        self.assertEqual(len(lines), (48 + 64) * 2)
        by_id = {row[1]: row for row in lines}
        for row in lines:
            paired = by_id[row[9]]
            self.assertEqual(paired[9], row[1])
            self.assertEqual(row[3], paired[6])
            self.assertEqual(row[6], paired[3])
            self.assertEqual(row[10:14], ['1', '0', '1', '1'])
        # Auto cuts preserve every original tile, no new border walls.
        for name in ('field_1', 'field_2', 'field_3', 'field_4'):
            self.assertEqual(files[f'cells/{name}.cell'].split('grid:\n')[1], ('.' * 32 + '\n') * 24)

    def test_interior_explicit_door_and_scene_newline(self):
        files = editor.export_files(interior(flat()))
        self.assertIn('description: Quiet interior.\n', files['cells/inn.cell'])
        self.assertIn('+', files['cells/inn.cell'].split('grid:\n')[1])
        doors = [shlex.split(row) for row in files['world.ratw'].splitlines() if row.startswith('door "link_')]
        self.assertEqual(len(doors), 2)
        self.assertEqual(doors[0][10:14], ['0', '0', '0', '0'])
        self.assertEqual(doors[0][6], 'inn')
        self.assertIn('line breaks', files['README.txt'])

    def test_open_passage_stairs_and_original_height(self):
        for kind in ('stairs', 'passage'):
            p = interior(flat())
            p['links'][0]['kind'] = kind
            p['links'][0]['open'] = True
            p['heights']['10,10'] = .25
            files = editor.export_files(p)
            row = next(shlex.split(r) for r in files['world.ratw'].splitlines()
                       if r.startswith('door "' + ('stairs_' if kind == 'stairs' else 'link_')))
            self.assertEqual(row[10], '1')
            self.assertEqual(row[13], '1' if kind == 'passage' else '0')
            self.assertIn('height: 10 10 0.25\n', files['cells/field_1.cell'])

    def test_height_blocks_only_incompatible_seam(self):
        p = flat()
        p['heights']['32,12'] = 3
        lines = [r for r in editor.export_files(p)['world.ratw'].splitlines() if r.startswith('door ')]
        self.assertEqual(len(lines), 222)

    def test_explicit_endpoint_reserves_seam(self):
        p = interior(flat())
        p['links'][0]['a'] = dict(cell='field_1', x=31, y=12)
        lines = [shlex.split(r) for r in editor.export_files(p)['world.ratw'].splitlines() if r.startswith('door ')]
        self.assertEqual(len(lines), 224)  # one seam pair removed + one door pair.

    def test_different_z_not_automatic_adjacency(self):
        p = flat()
        p['cells'][0]['z'] = 1
        seams = [r for r in editor.export_files(p)['world.ratw'].splitlines() if r.startswith('door ')]
        self.assertEqual(len(seams), 112)

    def test_missing_or_overlapping_partition(self):
        for mutate in (lambda p: p.update(cells=[]), lambda p: p['cells'].pop(),
                       lambda p: p['cells'][1].update(x=0)):
            p = flat()
            mutate(p)
            self.bad(p)

    def test_structural_rejections(self):
        mutations = [lambda p: p.update(version=2), lambda p: p.update(width=True),
                     lambda p: p.update(name='   '),
                     lambda p: p.update(width=10**500), lambda p: p.update(height=257),
                     lambda p: p.update(rooms={}), lambda p: p['terrain'].pop(),
                     lambda p: p['terrain'].__setitem__(0, 'x' * 64),
                     lambda p: p['cells'][0].update(id='../evil'),
                     lambda p: p['cells'][1].update(id='field_1'),
                     lambda p: p['cells'][0].update(terrain=['.' * 32] * 24),
                     lambda p: p['cells'][0].update(name='Name\nspawn: hacked'),
                     lambda p: p['cells'][0].update(name='\ud800'),
                     lambda p: p['cells'][0].update(name=''),
                     lambda p: p['cells'][0].update(width=3),
                     lambda p: p['cells'][0].update(outdoors=1),
                     lambda p: p['cells'][0].update(weather='hurricane')]
        for mutate in mutations:
            p = flat()
            mutate(p)
            self.bad(p)

    def test_height_validation(self):
        for key, value in [('64,0', 1), ('1,48', 1), ('01,0', 1), ('1,1', .1), ('1,1', 17),
                           ('1,1', True), ('1,1', float('inf')), ('1,1', float('nan')), ('../x', 1)]:
            p = flat()
            p['heights'][key] = value
            # NaN deliberately cannot compare equal to itself in deep-copy tests.
            with self.assertRaises(editor.ValidationError):
                editor.export_files(p)

    def test_broken_links(self):
        changes = [lambda p: p['links'][0]['a'].update(cell='missing'),
                   lambda p: p['links'][0]['a'].update(cell=[]),
                   lambda p: p['links'][0]['a'].update(x=-1),
                   lambda p: p['links'][0]['a'].update(x=.5),
                   lambda p: p['links'][0]['b'].update(cell='field_1'),
                   lambda p: p['links'].append(copy.deepcopy(p['links'][0])),
                   lambda p: p['links'][0].update(kind='magic'),
                   lambda p: p['links'][0].update(kind='stairs', open=False),
                   lambda p: p['links'][0].update(open=1),
                   lambda p: p['links'][0].update(id='../../bad')]
        for change in changes:
            p = interior(flat())
            change(p)
            self.bad(p)

    def test_solid_endpoint_no_arrival_and_steep_arrival(self):
        p = interior(flat())
        p['terrain'][10] = '.' * 10 + '#' + '.' * 53
        self.bad(p, 'solid')
        p = interior(flat())
        p['rooms'][0]['terrain'] = ['#' * 16 for _ in range(12)]
        p['rooms'][0]['terrain'][10] = '#' * 7 + '.' + '#' * 8
        self.bad(p, 'arrival')
        p = interior(flat())
        p['heights']['10,10'] = 16
        self.bad(p, 'arrival')

    def test_spawn_validation(self):
        for spawn in (None, {'cell': 'missing', 'x': 1, 'y': 1}, {'cell': [], 'x': 1, 'y': 1},
                      {'cell': 'field_1', 'x': -1, 'y': 1}, {'cell': 'field_1', 'x': 10, 'y': 10}):
            p = interior(flat())
            p['spawn'] = spawn
            self.bad(p, 'pawn')

    def test_zip_paths_and_utf8_quotes(self):
        p = interior(flat())
        p['links'][0]['name'] = 'Wolf "gate" \\ Moon — doorway'
        archive = zipfile.ZipFile(io.BytesIO(editor.export_zip(p)))
        for name in archive.namelist():
            self.assertFalse(name.startswith('/') or '..' in Path(name).parts)
        door = next(shlex.split(row) for row in archive.read('world.ratw').decode().splitlines() if row.startswith('door "link'))
        self.assertEqual(door[2], p['links'][0]['name'])

    def test_write_new_directory_only(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp) / 'new'
            manifest = editor.write_export(flat(), out)
            self.assertTrue(manifest.is_file())
            original = manifest.read_bytes()
            with self.assertRaises(FileExistsError):
                editor.write_export(flat(), out)
            self.assertEqual(manifest.read_bytes(), original)
            p = flat()
            p['spawn'] = None
            with self.assertRaises(editor.ValidationError):
                editor.write_export(p, Path(temp) / 'invalid')
            self.assertFalse((Path(temp) / 'invalid').exists())

    def test_integer_json_float_normalization(self):
        p = flat()
        p['width'] = 64.0
        p['cells'][0]['x'] = 0.0
        p['cells'][0]['width'] = 32.0
        self.assertIn('world.ratw', editor.export_files(p))

    def test_json_duplicate_and_nonfinite(self):
        for raw in ('{"a":1,"a":2}', '{"a":NaN}', '{"a":Infinity}'):
            with self.assertRaises(ValueError):
                editor.read_json(raw)


class HttpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = editor.make_server(0)
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        cls.port = cls.server.server_port

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()

    def request(self, method, path, body=None, headers=None):
        conn = http.client.HTTPConnection('127.0.0.1', self.port, timeout=5)
        conn.request(method, path, body=body, headers=headers or {})
        result = conn.getresponse()
        status, headers, raw = result.status, dict(result.getheaders()), result.read()
        conn.close()
        return status, headers, raw

    def auth(self):
        status, _, raw = self.request('GET', '/api/session')
        self.assertEqual(status, 200)
        return {'Content-Type': 'application/json', 'X-RATW-Editor': json.loads(raw)['token']}

    def test_demo_validate_and_export(self):
        status, _, raw = self.request('GET', '/api/demo')
        self.assertEqual(status, 200)
        headers = self.auth()
        status, _, result = self.request('POST', '/api/validate', raw, headers)
        self.assertEqual(status, 200)
        self.assertTrue(json.loads(result)['valid'])
        status, response, archive = self.request('POST', '/api/export', raw, headers)
        self.assertEqual(status, 200)
        self.assertEqual(response['Content-Type'], 'application/zip')
        self.assertIn('world.ratw', zipfile.ZipFile(io.BytesIO(archive)).namelist())

    def test_no_session_no_cross_origin(self):
        body = json.dumps(flat())
        for headers in ({'Content-Type': 'application/json'},
                        dict(self.auth(), Origin='https://evil.example'),
                        dict(self.auth(), Host='evil.example')):
            self.assertEqual(self.request('POST', '/api/export', body, headers)[0], 403)
        self.assertEqual(self.request('GET', '/api/session', headers={'Host': 'evil.example'})[0], 403)

    def test_invalid_body_and_size(self):
        headers = self.auth()
        self.assertEqual(self.request('POST', '/api/export', b'not json', headers)[0], 400)
        self.assertEqual(self.request('POST', '/api/export', b'{}', headers)[0], 422)
        self.assertEqual(self.request('POST', '/api/export', b'', dict(headers, **{'Content-Length': str(editor.MAX_BODY + 1)}))[0], 413)
        self.assertEqual(self.request('POST', '/api/export', b'{}', dict(headers, **{'Content-Type': 'text/plain'}))[0], 415)
        # Error replies must remain valid JSON even for malformed Unicode keys.
        status, _, raw = self.request('POST', '/api/export', b'{"\\ud800":1,"\\ud800":2}', headers)
        self.assertEqual(status, 400)
        self.assertIn('error', json.loads(raw))
        p = flat()
        p['name'] = '\ud800'
        status, _, raw = self.request('POST', '/api/export', json.dumps(p), headers)
        self.assertEqual(status, 422)
        self.assertTrue(json.loads(raw)['errors'])

    def test_static_allowlist(self):
        for path in ('/../README.md', '/tools/npc_bridge.py', '/.env', '/api/../../secret'):
            self.assertEqual(self.request('GET', path)[0], 404)
        status, headers, _ = self.request('GET', '/api/session')
        self.assertEqual(status, 200)
        self.assertEqual(headers['Cache-Control'], 'no-store')
        self.assertNotIn('Access-Control-Allow-Origin', headers)


if __name__ == '__main__':
    unittest.main(verbosity=2)
