#!/usr/bin/env python3
"""Local-only Atlas editor host and validated, non-destructive world exporter.

No dependencies, external services, or access to the game save database.
"""
from __future__ import annotations

import argparse
import copy
import io
import json
import math
from pathlib import Path
import re
import secrets
import sys
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parent.parent
GLYPHS = set('.#,\"T=~:^+')
SOLID = set('#T=')
MAX_BODY = 8 * 1024 * 1024
MAX_TILES = 262144
ID = re.compile(r'[a-z][a-z0-9_-]{0,47}\Z')


class ValidationError(ValueError):
    def __init__(self, errors: list[str]):
        self.errors = errors
        super().__init__('; '.join(errors))


def number(value, low, high, *, integer=False):
    return (type(value) in (int, float) and low <= value <= high
            and math.isfinite(value) and (not integer or value == int(value)))


def text(value, limit, multiline=False):
    return (isinstance(value, str) and len(value) <= limit
            and all((ord(c) >= 32 and not 0xD800 <= ord(c) <= 0xDFFF) or (multiline and c == '\n') for c in value)
            and '\x7f' not in value)


def check_project(value: object) -> tuple[dict, list[str]]:
    """Validate shape first, then topology. Never mutate the caller's project."""
    errors: list[str] = []
    warnings: list[str] = []

    def require(condition, message):
        if not condition:
            errors.append(message)

    if not isinstance(value, dict):
        raise ValidationError(['Project must be an object.'])
    p = copy.deepcopy(value)
    require(p.get('format') == 'ratw-atlas' and type(p.get('version')) is int and p['version'] == 1,
            'Unsupported atlas format/version.')
    require(text(p.get('name'), 120) and bool(p.get('name', '').strip()), 'Project name must be nonempty text, at most 120 characters.')
    for key in ('width', 'height'):
        require(number(p.get(key), 4, 256, integer=True), f'World {key} must be 4–256 whole tiles.')
    for key in ('cells', 'rooms', 'links'):
        require(isinstance(p.get(key), list), f'{key} must be an array.')
    if errors:
        raise ValidationError(errors)
    width, height = int(p['width']), int(p['height'])
    p['width'], p['height'] = width, height
    require(len(p['cells']) + len(p['rooms']) <= 256, 'At most 256 cells/interiors are supported.')
    require(len(p['links']) <= 2048, 'At most 2048 explicit links are supported.')

    catalogs = {}
    for key, limit in [('factions', 64), ('chapters', 128)]:
        entries = p.get(key, [])
        require(isinstance(entries, list) and len(entries) <= limit, f'{key} must be an array with at most {limit} entries.')
        catalogs[key] = set()
        if not isinstance(entries, list):
            continue
        for entry in entries:
            if not isinstance(entry, dict):
                errors.append(f'Every {key} entry must be an object.')
                continue
            required = {'id', 'name', 'color'} if key == 'factions' else {'id', 'name'}
            require(set(entry) == required, f'{key} entries require only {", ".join(sorted(required))}.')
            cid = entry.get('id')
            valid_id = isinstance(cid, str) and ID.fullmatch(cid)
            require(valid_id and cid not in catalogs[key], f'Invalid or duplicate {key} ID: {cid!r}.')
            if valid_id:
                catalogs[key].add(cid)
            require(text(entry.get('name'), 120) and bool(entry.get('name', '').strip()), f'{key}: invalid or empty name.')
            if key == 'factions':
                require(isinstance(entry.get('color'), str) and re.fullmatch(r'#[0-9a-fA-F]{6}', entry['color']),
                        'Faction color must be a six-digit #RRGGBB color.')

    def grid(container, w, h, label):
        rows = container.get('terrain')
        require(isinstance(rows, list) and len(rows) == h
                and all(isinstance(row, str) and len(row) == w and set(row) <= GLYPHS for row in rows),
                f'{label}: terrain must be {w}×{h}, using supported ASCII glyphs.')
        heights = container.get('heights', {})
        require(isinstance(heights, dict) and len(heights) <= w * h, f'{label}: invalid height overrides.')
        if isinstance(heights, dict):
            for key, val in heights.items():
                match = re.fullmatch(r'(0|[1-9][0-9]*),(0|[1-9][0-9]*)', key) if isinstance(key, str) else None
                require(bool(match) and int(match[1]) < w and int(match[2]) < h
                        and number(val, -16, 16) and val * 4 == int(val * 4),
                        f'{label}: invalid height override {key!r}.')

    grid(p, width, height, 'World')
    ids = set()
    total_tiles = width * height
    for c, detached in [(c, False) for c in p['cells']] + [(c, True) for c in p['rooms']]:
        if not isinstance(c, dict):
            errors.append('Every cell/interior must be an object.')
            continue
        cid = c.get('id')
        valid_id = isinstance(cid, str) and ID.fullmatch(cid)
        require(valid_id and cid not in ids, f'Invalid or duplicate cell ID: {cid!r}.')
        if valid_id:
            ids.add(cid)
        require(text(c.get('name'), 120) and bool(c.get('name', '').strip()), f'{cid}: invalid or empty name.')
        require(text(c.get('description', ''), 4096, True), f'{cid}: invalid description.')
        require(type(c.get('outdoors')) is bool, f'{cid}: outdoors must be boolean.')
        require(c.get('weather') in ('clear', 'rain', 'fog', 'snow'), f'{cid}: unknown weather.')
        if 'territory' in c:
            territory = c['territory']
            require(isinstance(territory, dict), f'{cid}: territory must be an object.')
            if isinstance(territory, dict):
                require(set(territory) == {'region', 'claims', 'chapter'}, f'{cid}: territory requires only region, claims and chapter.')
                require(isinstance(territory.get('region'), str) and ID.fullmatch(territory['region']), f'{cid}: invalid territory region ID.')
                chapter = territory.get('chapter')
                require(isinstance(chapter, str) and (chapter == '' or ID.fullmatch(chapter) and chapter in catalogs['chapters']),
                        f'{cid}: territory references an invalid or unknown chapter.')
                claims = territory.get('claims')
                require(isinstance(claims, list) and len(claims) <= 64, f'{cid}: territory claims must contain at most 64 faction IDs.')
                if isinstance(claims, list):
                    claim_ids = set()
                    for claim in claims:
                        valid_claim = isinstance(claim, str) and ID.fullmatch(claim)
                        require(valid_claim and claim in catalogs['factions'], f'{cid}: territory references an invalid or unknown faction.')
                        if valid_claim:
                            require(claim not in claim_ids, f'{cid}: duplicate territory claim.')
                            claim_ids.add(claim)
                    if len(claim_ids) == len(claims):
                        territory['claims'] = sorted(claims)
        if 'lighting' in c:
            light = c['lighting']
            require(isinstance(light, dict), f'{cid}: lighting must be an object.')
            if isinstance(light, dict):
                require(set(light) == {'artificial', 'daylightAccess', 'tone'},
                        f'{cid}: lighting requires only artificial, daylightAccess and tone.')
                require(number(light.get('artificial'), 0, 1) and number(light.get('daylightAccess'), 0, 1),
                        f'{cid}: lighting levels must be numbers from 0 to 1.')
                require(light.get('tone') in ('warm', 'neutral', 'cool'), f'{cid}: invalid lighting tone.')
        require(number(c.get('z', 0), -1e6, 1e6), f'{cid}: invalid Z position.')
        dims_ok = all(number(c.get(k), 4, 256, integer=True) for k in ('width', 'height'))
        require(dims_ok, f'{cid}: dimensions must be 4–256 whole tiles.')
        if not dims_ok:
            continue
        c['width'], c['height'] = int(c['width']), int(c['height'])
        if detached:
            total_tiles += int(c['width']) * int(c['height'])
            grid(c, int(c['width']), int(c['height']), str(cid))
            require(all(number(c.get(k, 0), -1e6, 1e6) for k in ('worldX', 'worldY')),
                    f'{cid}: invalid overview position.')
        else:
            require('terrain' not in c and 'heights' not in c, f'{cid}: world-cell terrain belongs to the shared canvas.')
            require(number(c.get('x'), 0, width - c['width'], integer=True)
                    and number(c.get('y'), 0, height - c['height'], integer=True),
                    f'{cid}: rectangle falls outside world canvas.')
            if number(c.get('x'), 0, width, integer=True) and number(c.get('y'), 0, height, integer=True):
                c['x'], c['y'] = int(c['x']), int(c['y'])
    require(total_tiles <= MAX_TILES, f'World plus interiors exceeds {MAX_TILES} authored tiles.')
    if errors:
        raise ValidationError(errors[:100])

    coverage = bytearray(width * height)
    for c in p['cells']:
        for y in range(int(c['y']), int(c['y'] + c['height'])):
            for x in range(int(c['x']), int(c['x'] + c['width'])):
                at = y * width + x
                if coverage[at]:
                    errors.append(f'Overlapping world cells at {x},{y}.')
                    break
                coverage[at] = 1
            if errors:
                break
        if errors:
            break
    require(bool(p['cells']) and all(coverage), 'Cut the full world canvas into nonoverlapping cells before exporting.')
    by_id = {c['id']: c for c in p['cells'] + p['rooms']}
    rows_by_id = {cid: cell_rows(p, c) for cid, c in by_id.items()}
    used = set()
    link_ids = set()
    valid_links = []
    for link in p['links']:
        if not isinstance(link, dict):
            errors.append('Link must be an object.')
            continue
        lid = link.get('id')
        valid_id = isinstance(lid, str) and ID.fullmatch(lid)
        require(valid_id and lid not in link_ids, f'Invalid or duplicate link ID: {lid!r}.')
        if valid_id:
            link_ids.add(lid)
        require(text(link.get('name'), 120) and bool(link.get('name', '').strip()), f'{lid}: invalid or empty link name.')
        require(link.get('kind') in ('door', 'passage', 'stairs'), f'{lid}: unsupported link kind.')
        require(type(link.get('open')) is bool, f'{lid}: open must be boolean.')
        require(link.get('kind') == 'door' or link.get('open') is True, f'{lid}: stairs and passages must stay open.')
        ends_ok = True
        ends = []
        for side in ('a', 'b'):
            end = link.get(side)
            if not isinstance(end, dict) or not isinstance(end.get('cell'), str) or end.get('cell') not in by_id:
                errors.append(f'{lid}: {side} references a missing cell.')
                ends_ok = False
                continue
            c = by_id[end['cell']]
            if not number(end.get('x'), 0, c['width'] - 1, integer=True) or not number(end.get('y'), 0, c['height'] - 1, integer=True):
                errors.append(f'{lid}: {side} is outside its cell.')
                ends_ok = False
                continue
            x, y = int(end['x']), int(end['y'])
            end['x'], end['y'] = x, y
            at = (end['cell'], x, y)
            require(at not in used, f'{lid}: another link already occupies {at}.')
            used.add(at)
            require(rows_by_id[end['cell']][y][x] not in SOLID, f'{lid}: {side} is on solid terrain.')
            ends.append(end)
        if ends_ok:
            require(ends[0]['cell'] != ends[1]['cell'], f'{lid}: links must connect two different cells.')
            valid_links.append(link)
    if errors:
        raise ValidationError(errors[:100])
    for link in valid_links:
        for side in ('a', 'b'):
            end = link[side]
            if not arrival_tile(p, by_id[end['cell']], end, used, link['kind']):
                errors.append(f'{link["id"]}: {side} has no clear, height-compatible neighboring arrival tile.')
    spawn = p.get('spawn')
    if not isinstance(spawn, dict) or not isinstance(spawn.get('cell'), str) or spawn.get('cell') not in by_id:
        errors.append('Choose a player spawn in an existing cell.')
    else:
        c = by_id[spawn['cell']]
        if not number(spawn.get('x'), 0, c['width'] - 1, integer=True) or not number(spawn.get('y'), 0, c['height'] - 1, integer=True):
            errors.append('Spawn is outside its cell.')
        elif (rows_by_id[spawn['cell']][int(spawn['y'])][int(spawn['x'])] in SOLID
              or (spawn['cell'], int(spawn['x']), int(spawn['y'])) in used):
            errors.append('Spawn must be on walkable terrain, away from a link endpoint.')
        else:
            spawn['x'], spawn['y'] = int(spawn['x']), int(spawn['y'])
    if errors:
        raise ValidationError(errors[:100])
    if any('\n' in c.get('description', '') for c in by_id.values()):
        warnings.append('Scene-description line breaks are folded to spaces in game headers; atlas JSON retains them.')
    return p, warnings


def cell_rows(p, c):
    if 'terrain' in c:
        return list(c['terrain'])
    x, y, w, h = (int(c[k]) for k in ('x', 'y', 'width', 'height'))
    return [row[x:x + w] for row in p['terrain'][y:y + h]]


def tile_height(p, c, x, y):
    if 'terrain' in c:
        source, px, py = c, x, y
    else:
        source, px, py = p, x + int(c['x']), y + int(c['y'])
    glyph = source['terrain'][py][px]
    return source.get('heights', {}).get(f'{px},{py}', {':': .25, '^': .5}.get(glyph, 0))


def arrival_tile(p, c, end, used, kind):
    rows = cell_rows(p, c)
    x, y = int(end['x']), int(end['y'])
    source_height = tile_height(p, c, x, y)
    if kind == 'stairs':
        # Stairs stamp the glyph but preserve any explicit height override.
        source = c if 'terrain' in c else p
        sx, sy = (x, y) if 'terrain' in c else (x + int(c['x']), y + int(c['y']))
        source_height = source.get('heights', {}).get(f'{sx},{sy}', .5)
    options = [(x, y - 1), (x + 1, y), (x, y + 1), (x - 1, y)]
    options.sort(key=lambda at: (at[0] - (c['width'] - 1) / 2) ** 2 + (at[1] - (c['height'] - 1) / 2) ** 2)
    for ax, ay in options:
        if (0 <= ax < c['width'] and 0 <= ay < c['height'] and rows[ay][ax] not in SOLID
                and (c['id'], ax, ay) not in used and abs(tile_height(p, c, ax, ay) - source_height) <= .55):
            return ax + .5, ay + .5
    return None


def quote(value):
    # std::quoted string escaping; UTF-8 remains literal, no JSON escape syntax.
    return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"') + '"'


def export_files(value):
    p, warnings = check_project(value)
    by_id = {c['id']: c for c in p['cells'] + p['rooms']}
    used = {(e['cell'], int(e['x']), int(e['y'])) for link in p['links'] for e in (link['a'], link['b'])}
    fixtures = []
    stamps = {}
    for link in p['links']:
        prefix = 'stairs_' if link['kind'] == 'stairs' else 'link_'
        ids = (prefix + link['id'] + '_a', prefix + link['id'] + '_b')
        for i, side in enumerate(('a', 'b')):
            a, b = link[side], link['b' if side == 'a' else 'a']
            arrival = arrival_tile(p, by_id[b['cell']], b, used, link['kind'])
            fixtures.append((ids[i], link['name'], a['cell'], a['x'] + .5, a['y'] + .5,
                             b['cell'], *arrival, ids[1 - i], link['open'] or link['kind'] != 'door',
                             False, False, link['kind'] == 'passage', '-'))
            if link['kind'] != 'passage':
                stamps[(a['cell'], a['x'], a['y'])] = '^' if link['kind'] == 'stairs' else '+'
    # Ownership raster makes seam generation linear in authored terrain size.
    owner = [[None] * p['width'] for _ in range(p['height'])]
    for c in p['cells']:
        for y in range(c['y'], c['y'] + c['height']):
            owner[y][c['x']:c['x'] + c['width']] = [c['id']] * c['width']
    seam = 0
    for y in range(p['height']):
        for x in range(p['width']):
            for dx, dy, edge, back in ((1, 0, 'E', 'W'), (0, 1, 'S', 'N')):
                nx, ny = x + dx, y + dy
                if nx >= p['width'] or ny >= p['height'] or owner[y][x] == owner[ny][nx]:
                    continue
                ca, cb = by_id[owner[y][x]], by_id[owner[ny][nx]]
                ax, ay, bx, by = x - ca['x'], y - ca['y'], nx - cb['x'], ny - cb['y']
                if (ca.get('z', 0) != cb.get('z', 0) or p['terrain'][y][x] in SOLID
                        or p['terrain'][ny][nx] in SOLID or (ca['id'], ax, ay) in used or (cb['id'], bx, by) in used
                        or abs(tile_height(p, ca, ax, ay) - tile_height(p, cb, bx, by)) > .55):
                    continue
                for a, b, px, py, tx, ty, e, suffix, other in (
                        (ca, cb, ax, ay, bx, by, edge, 'a', 'b'),
                        (cb, ca, bx, by, ax, ay, back, 'b', 'a')):
                    fixtures.append((f'seam_{seam}_{suffix}', 'Open boundary', a['id'], px + .5, py + .5,
                                     b['id'], tx + .5, ty + .5, f'seam_{seam}_{other}', True, False, True, True, e))
                seam += 1
    if len(fixtures) > 65536:
        raise ValidationError(['Export exceeds 65536 fixtures; use larger cells or fewer links.'])
    files = {}
    manifest = ['RATW_WORLD 1']
    for faction in p.get('factions', []):
        manifest.append(f'faction {quote(faction["id"])} {quote(faction["name"])} {quote(faction["color"])}')
    for chapter in p.get('chapters', []):
        manifest.append(f'chapter {quote(chapter["id"])} {quote(chapter["name"])}')
    for c in by_id.values():
        rows = [list(row) for row in cell_rows(p, c)]
        overrides = {}
        for y, row in enumerate(rows):
            for x, glyph in enumerate(row):
                height = tile_height(p, c, x, y)
                stamp = stamps.get((c['id'], x, y))
                if stamp:
                    row[x] = stamp
                    if stamp == '^':
                        source = c if 'terrain' in c else p
                        sx, sy = (x, y) if 'terrain' in c else (x + c['x'], y + c['y'])
                        height = source.get('heights', {}).get(f'{sx},{sy}', .5)
                base = {':': .25, '^': .5}.get(row[x], 0)
                if height != base:
                    overrides[(x, y)] = height
        headers = [f'id: {c["id"]}', f'name: {c["name"]}',
                   'description: ' + c.get('description', '').replace('\n', ' '),
                   f'world: {c.get("x", c.get("worldX", 0))} {c.get("y", c.get("worldY", 0))} {c.get("z", 0)}',
                   f'outdoors: {str(c["outdoors"]).lower()}', f'weather: {c["weather"]}',
                   'wind: 0 0.5 1' if c['outdoors'] else 'wind: 0 0 0']
        light = c.get('lighting', {'artificial': 1, 'daylightAccess': 1, 'tone': 'warm'})
        headers.append(f'lighting: {light["artificial"]:.17g} {light["daylightAccess"]:.17g} {light["tone"]}')
        headers += [f'height: {x} {y} {h:g}' for (x, y), h in sorted(overrides.items())]
        files[f'cells/{c["id"]}.cell'] = '\n'.join(headers + ['grid:'] + [''.join(row) for row in rows]) + '\n'
        manifest.append(f'cell {quote(c["id"])} {quote("cells/" + c["id"] + ".cell")}')
        territory = c.get('territory', {'region': 'unassigned', 'claims': [], 'chapter': ''})
        if territory != {'region': 'unassigned', 'claims': [], 'chapter': ''}:
            claims = ''.join(' ' + quote(claim) for claim in territory['claims'])
            manifest.append(f'territory {quote(c["id"])} {quote(territory["region"])} '
                            f'{quote(territory["chapter"] or "-")} {len(territory["claims"])}{claims}')
    spawn = p['spawn']
    manifest.append(f'spawn {quote(spawn["cell"])} {spawn["x"] + .5} {spawn["y"] + .5}')
    for f in fixtures:
        a, name, cell, x, y, target, ax, ay, pair, opened, locked, boundary, passage, edge = f
        manifest.append(f'door {quote(a)} {quote(name)} {quote(cell)} {x:g} {y:g} {quote(target)} '
                        f'{ax:g} {ay:g} {quote(pair)} {int(opened)} {int(locked)} {int(boundary)} {int(passage)} {quote(edge)}')
    files['world.ratw'] = '\n'.join(manifest) + '\n'
    files['atlas.json'] = json.dumps(p, ensure_ascii=False, indent=2) + '\n'
    files['README.txt'] = ('RATW authored world export v1\n\n'
        'This is offline content, not a game save. Extract into a NEW directory.\n'
        'Launch from the RATWMUD repository:\n'
        'bash tools/play.sh -RatwWorld=/absolute/path/to/world.ratw\n'
        'A separate custom-world save is used by default. Do not reuse a save after changing cell topology.\n'
        'No NPCs are authored in this editor slice. The built-in demonstration is unchanged.\n'
        'atlas.json reopens the editable continuous canvas; .cell files are separate runtime cells.\n'
        f'Cells: {len(by_id)}; reciprocal boundary pairs: {seam}; explicit links: {len(p["links"])}.\n'
        + '\n'.join(warnings) + '\n')
    return files


def export_zip(project):
    content = io.BytesIO()
    with zipfile.ZipFile(content, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in export_files(project).items():
            archive.writestr(name, data.encode('utf-8'))
    return content.getvalue()


def write_export(project, output: Path):
    files = export_files(project)  # Validate before making any output.
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=False)  # Never replace any existing export.
    for name, data in files.items():
        dest = output / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(data, encoding='utf-8')
    return output / 'world.ratw'


def demo_project():
    rows = []
    for y in range(48):
        row = []
        for x in range(64):
            glyph = ','
            if 21 <= y <= 24 or 28 <= x <= 31:
                glyph = '.'
            if 48 <= x <= 51:
                glyph = '~'
            if 21 <= y <= 24 and 46 <= x <= 53:
                glyph = '.'
            if 6 <= y <= 11 and 39 <= x <= 43:
                glyph = '^'
            if ((x * 17 + y * 31) % 97 == 0 and glyph == ','):
                glyph = '#'
            row.append(glyph)
        rows.append(''.join(row))
    cells = []
    for iy in range(2):
        for ix in range(2):
            cells.append(dict(id=f'field_{iy * 2 + ix + 1}', name=('Juniper Approach', 'Spring Rise', 'South Meadow', 'River Crossing')[iy * 2 + ix],
                              description='An open stretch of the Juniper country.', x=ix * 32, y=iy * 24,
                              width=32, height=24, z=0, outdoors=True, weather='clear'))
    return dict(format='ratw-atlas', version=1, name='Juniper country', width=64, height=48,
                terrain=rows, heights={}, cells=cells, rooms=[], links=[],
                spawn=dict(cell='field_1', x=16, y=22))


def read_json(raw):
    def no_constant(value):
        raise ValueError(f'Nonfinite JSON number: {value}')
    def unique_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f'Duplicate JSON property: {key}')
            result[key] = value
        return result
    return json.loads(raw, parse_constant=no_constant, object_pairs_hook=unique_pairs)


def make_server(port=8765):
    token = secrets.token_urlsafe(24)
    static = {'/': ('index.html', 'text/html'), '/index.html': ('index.html', 'text/html'),
              '/app.mjs': ('app.mjs', 'text/javascript'), '/model.mjs': ('model.mjs', 'text/javascript'),
              '/style.css': ('style.css', 'text/css')}

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            # Request paths only; never log project bodies or the session token.
            pass

        def trusted(self):
            origin = f'http://127.0.0.1:{self.server.server_port}'
            local = f'http://localhost:{self.server.server_port}'
            return (self.headers.get('Host') in (origin[7:], local[7:])
                    and self.headers.get('Origin', origin) in (origin, local))

        def reply(self, status, data, mime='application/json', download=False):
            body = data if isinstance(data, bytes) else json.dumps(data, ensure_ascii=True).encode('utf-8')
            self.send_response(status)
            self.send_header('Content-Type', mime + '; charset=utf-8' if mime != 'application/zip' else mime)
            self.send_header('Content-Length', str(len(body)))
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; object-src 'none'")
            if download:
                self.send_header('Content-Disposition', 'attachment; filename="ratw-world.zip"')
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            if not self.trusted():
                return self.reply(403, {'error': 'Local editor origin required.'})
            path = urlsplit(self.path).path
            if path == '/api/session':
                return self.reply(200, {'token': token})
            if path == '/api/demo':
                return self.reply(200, demo_project())
            if path not in static:
                return self.reply(404, {'error': 'Not found.'})
            filename, mime = static[path]
            try:
                self.reply(200, (ROOT / 'Editor' / filename).read_bytes(), mime)
            except OSError:
                self.reply(404, {'error': 'Editor asset is unavailable.'})

        def do_POST(self):
            if not self.trusted() or not secrets.compare_digest(self.headers.get('X-RATW-Editor', ''), token):
                return self.reply(403, {'error': 'Local editor session required.'})
            path = urlsplit(self.path).path
            if path not in ('/api/validate', '/api/export'):
                return self.reply(404, {'error': 'Not found.'})
            if self.headers.get_content_type() != 'application/json' or self.headers.get('Transfer-Encoding'):
                return self.reply(415, {'error': 'A bounded JSON request is required.'})
            try:
                size = int(self.headers.get('Content-Length', '-1'))
            except ValueError:
                size = -1
            if not 0 < size <= MAX_BODY:
                return self.reply(413, {'error': 'Project must be at most 8 MB.'})
            self.connection.settimeout(10)
            try:
                raw = self.rfile.read(size)
                if len(raw) != size:
                    raise ValueError('Incomplete request body.')
                project = read_json(raw)
                if path == '/api/export':
                    return self.reply(200, export_zip(project), 'application/zip', True)
                _, warnings = check_project(project)
                return self.reply(200, {'valid': True, 'errors': [], 'warnings': warnings})
            except ValidationError as error:
                return self.reply(422, {'valid': False, 'errors': error.errors, 'warnings': []})
            except (ValueError, TypeError, KeyError, OverflowError, RecursionError):
                return self.reply(400, {'error': 'Malformed atlas JSON.'})
            except TimeoutError:
                return self.reply(408, {'error': 'Request body timed out.'})

    server = ThreadingHTTPServer(('127.0.0.1', port), Handler)
    server.daemon_threads = True
    return server


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', nargs='?', default='serve', choices=('serve', 'validate', 'export'))
    parser.add_argument('project', nargs='?', type=Path)
    parser.add_argument('--port', type=int, default=8765)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.command == 'serve':
        server = make_server(args.port)
        print(f'RATW Atlas editor: http://127.0.0.1:{server.server_port}\nLocal-only. Exports never modify the running game. Ctrl+C to stop.', flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            server.server_close()
        return 0
    if not args.project or args.project.stat().st_size > MAX_BODY:
        parser.error('Choose a project JSON file of at most 8 MB.')
    try:
        project = read_json(args.project.read_text(encoding='utf-8'))
        if args.command == 'validate':
            _, warnings = check_project(project)
            print('Valid atlas.' + ('\n' + '\n'.join(warnings) if warnings else ''))
        else:
            if not args.output:
                parser.error('Export requires --output pointing to a new directory.')
            print(write_export(project, args.output))
    except (ValueError, OSError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
