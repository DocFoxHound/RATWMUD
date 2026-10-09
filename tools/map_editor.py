#!/usr/bin/env python3
"""Local-only Atlas editor host and validated, non-destructive world exporter.

`serve` edits the one world and the character roster live in the DEV
PostgreSQL database (tools/live_edit.py), shared with everyone else editing
it. Exports and playtests never touch the game's save database.
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
import shlex
import subprocess
import sys
import threading
import time
import zipfile

import http_body
import roster as roster_lib
import terrain_catalog
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

ROOT = Path(__file__).resolve().parent.parent
# Terrain codes, solid tiles and default heights all come from the one catalog (Data/Terrain/terrain.json).
GLYPHS = terrain_catalog.GLYPHS
SOLID = terrain_catalog.SOLID
GLYPH_HEIGHTS = terrain_catalog.DEFAULT_HEIGHTS
WEATHERS = ('clear', 'overcast', 'rain', 'storm', 'fog', 'snow', 'sandstorm')
MAX_PEOPLE = 16384            # Authored residents; the game's society holds as many (MaxResidents, RatwSociety.h).
MAX_BODY = 256 * 1024 * 1024   # Local-only and token-guarded; the 6.6M-tile western world is ~60 MB of JSON.
MAX_TILES = 262144             # What the game loads today: cells plus interiors.
LEGACY_CANVAS = 4096           # Atlas v1/v2 kept one shared canvas of at most this many tiles a side.
WORLD_REACH = 10 ** 9          # Atlas v3 world cells sit anywhere within this many tiles of 0,0.
BUCKET = 256                   # Cells are filed on this grid (their largest size) to find neighbours quickly.
ID = re.compile(r'[a-z][a-z0-9_-]{0,47}\Z')
ROLES = ('merchant', 'guard', 'civilian')
SPECIES = ('timber', 'maned', 'arctic', 'red', 'ethiopian')
PATTERNS = ('solid', 'saddle', 'mantle', 'piebald')
PERSON_KEYS = {'id', 'name', 'role', 'description', 'greeting', 'workLabel', 'age', 'appearance', 'voice',
               'hours', 'route', 'paid', 'purse', 'herbs', 'meals', 'home', 'work', 'evening', 'personality', 'backstory'}
SLOT_KEYS = {'id', 'name', 'profession', 'workLabel', 'hours', 'route', 'paid', 'purse', 'herbs', 'meals',
             'home', 'work', 'evening'}
APPEARANCE_KEYS = {'species', 'sex', 'stature', 'pattern', 'baseColor', 'gradientColor', 'markingColor'}
ECONOMY_KEYS = {'treasury': 1000000, 'storeHerbs': 10000, 'storeMeals': 10000, 'dailyHerbs': 1000, 'dailyMeals': 1000}
DEFAULT_ECONOMY = {'treasury': 1000, 'storeHerbs': 100, 'storeMeals': 50, 'dailyHerbs': 10, 'dailyMeals': 12}


class ValidationError(ValueError):
    def __init__(self, errors: list[str]):
        self.errors = errors
        super().__init__('; '.join(errors))


class HostError(RuntimeError):
    """A request that failed for a reason the author can act on; `status` is the HTTP reply."""
    status = 422


def number(value, low, high, *, integer=False):
    return (type(value) in (int, float) and low <= value <= high
            and math.isfinite(value) and (not integer or value == int(value)))


def text(value, limit, multiline=False):
    return (isinstance(value, str) and len(value) <= limit
            and all((ord(c) >= 32 and not 0xD800 <= ord(c) <= 0xDFFF) or (multiline and c == '\n') for c in value)
            and '\x7f' not in value)


def to_v3(p: dict) -> dict:
    """Atlas v1/v2 kept one shared canvas that world cells were windows into; v3 gives each cell its own ground, in
    world tiles. Converts in place (the canvas and cells must already be checked) and returns p."""
    if p.get('version') == 3:
        return p
    ox, oy = (int(p.get('origin', {}).get(k, 0)) for k in ('x', 'y'))
    rows, heights = p['terrain'], p.get('heights', {})
    for c in p['cells']:
        x, y, w, h = (int(c[k]) for k in ('x', 'y', 'width', 'height'))
        c['terrain'] = [row[x:x + w] for row in rows[y:y + h]]
        c['heights'] = {}
        for key, value in heights.items():
            hx, hy = (int(v) for v in key.split(','))
            if x <= hx < x + w and y <= hy < y + h:
                c['heights'][f'{hx - x},{hy - y}'] = value
        c['x'], c['y'] = x + ox, y + oy
    for key in ('terrain', 'heights', 'width', 'height', 'origin'):
        p.pop(key, None)
    p['version'] = 3
    return p


def check_project(value: object, for_game: bool = True) -> tuple[dict, list[str]]:
    """Validate shape first, then topology. Never mutate the caller's project. Returns the project as atlas v3.

    for_game also applies what the game can load today (256 cells and interiors, 262,144 of their tiles); the
    editor and the database hold worlds of any size.
    """
    errors: list[str] = []
    warnings: list[str] = []

    def require(condition, message):
        if not condition:
            errors.append(message)

    if not isinstance(value, dict):
        raise ValidationError(['Project must be an object.'])
    p = copy.deepcopy(value)
    require(p.get('format') == 'ratw-atlas' and type(p.get('version')) is int and p['version'] in (1, 2, 3),
            'Unsupported atlas format/version.')
    require(text(p.get('name'), 120) and bool(p.get('name', '').strip()), 'Project name must be nonempty text, at most 120 characters.')
    if 'id' in p:
        require(isinstance(p['id'], str) and ID.fullmatch(p['id']), 'World ID must be lowercase letters, digits, _ or -.')
    legacy = p.get('version') in (1, 2)
    if legacy:
        for key in ('width', 'height'):
            require(number(p.get(key), 4, LEGACY_CANVAS, integer=True), f'World {key} must be 4–{LEGACY_CANVAS} whole tiles.')
        if 'origin' in p:
            o = p['origin']
            require(isinstance(o, dict) and set(o) == {'x', 'y'} and all(number(o.get(k), -1e6, 1e6, integer=True) for k in ('x', 'y')),
                    'World origin must be {x, y} whole tiles within ±1000000.')
    else:
        require(not {'terrain', 'heights', 'width', 'height', 'origin'} & set(p),
                'An atlas v3 world has no shared canvas: every cell holds its own terrain and heights.')
    for key in ('cells', 'rooms', 'links'):
        require(isinstance(p.get(key), list), f'{key} must be an array.')
    if errors:
        raise ValidationError(errors)
    if for_game:
        require(len(p['cells']) + len(p['rooms']) <= 256, '▶ Play and file exports load at most 256 cells and interiors; Push to live and DEV servers (tools/live.sh) stream any number.')
    require(len(p['cells']) + len(p['rooms']) <= 65536, 'At most 65536 cells and interiors are supported.')
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
                        and number(val, -16, 16) and val * 2 == int(val * 2),
                        f'{label}: invalid height override {key!r}.')

    if legacy:
        # The shared canvas, and each cell as a window onto it; then every cell takes its own ground.
        width, height = int(p['width']), int(p['height'])
        grid(p, width, height, 'World')
        for c in p['cells']:
            if not isinstance(c, dict):
                continue
            require('terrain' not in c and 'heights' not in c, f'{c.get("id")}: world-cell terrain belongs to the shared canvas.')
            require(all(number(c.get(k), 4, 256, integer=True) for k in ('width', 'height'))
                    and number(c.get('x'), 0, width - int(c.get('width', 0) or 0), integer=True)
                    and number(c.get('y'), 0, height - int(c.get('height', 0) or 0), integer=True),
                    f'{c.get("id")}: rectangle falls outside world canvas.')
        if errors:
            raise ValidationError(errors[:100])
        to_v3(p)
    ids = set()
    total_tiles = 0   # Exported tiles: cells and interiors, not the empty canvas around them.
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
        require(c.get('weather') in WEATHERS, f'{cid}: unknown weather.')
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
        if 'letting' in c and c['letting'] is not None:
            # A place to let (Docs/Design/32, 5.2): its landlord is checked against the people below.
            letting = c['letting']
            ok = (isinstance(letting, dict) and set(letting) == {'kind', 'landlord', 'rent', 'level'}
                  and letting.get('kind') in ('hall', 'warehouse') and isinstance(letting.get('landlord'), str)
                  and type(letting.get('rent')) is int and 1 <= letting['rent'] <= 100000
                  and type(letting.get('level')) is int and 2 <= letting['level'] <= 5)
            require(ok, f'{cid}: a place to let needs kind (hall or warehouse), landlord, rent (1-100000) and level (2-5).')
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
        total_tiles += int(c['width']) * int(c['height'])
        grid(c, int(c['width']), int(c['height']), str(cid))
        if detached:
            require(all(number(c.get(k, 0), -1e6, 1e6) for k in ('worldX', 'worldY')),
                    f'{cid}: invalid overview position.')
        else:
            placed = all(number(c.get(k), -WORLD_REACH, WORLD_REACH, integer=True) for k in ('x', 'y'))
            require(placed, f'{cid}: a world cell sits on whole world tiles within ±{WORLD_REACH}.')
            if placed:
                c['x'], c['y'] = int(c['x']), int(c['y'])
    if for_game:
        require(total_tiles <= MAX_TILES, f'Cells plus interiors cover {total_tiles} tiles; ▶ Play and file exports load at most '
                f'{MAX_TILES}. Push to live and DEV servers (tools/live.sh) stream any number.')
    if errors:
        raise ValidationError(errors[:100])

    for a, b in overlapping_cells(p['cells']):
        errors.append(f'World cells {a["id"]} and {b["id"]} overlap.')
    if errors:
        raise ValidationError(errors[:100])
    require(bool(p['cells']), 'Add at least one world cell before exporting.')
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
    check_content(p, by_id, rows_by_id, errors, warnings)
    if errors:
        raise ValidationError(errors[:100])
    if any('\n' in c.get('description', '') for c in by_id.values()):
        warnings.append('Scene-description line breaks are folded to spaces in game headers; atlas JSON retains them.')
    return p, warnings


def check_content(p, by_id, rows_by_id, errors, warnings):
    """People, patrol routes, economy and the herb patch (atlas v2)."""
    def require(condition, message):
        if not condition:
            errors.append(message)

    def place(value, label):
        ok = (isinstance(value, dict) and set(value) == {'cell', 'x', 'y'} and value.get('cell') in by_id
              and number(value.get('x'), 0, by_id[value['cell']]['width'] - 1, integer=True)
              and number(value.get('y'), 0, by_id[value['cell']]['height'] - 1, integer=True))
        require(ok, f'{label}: choose a tile inside an existing cell.')
        if ok:
            value['x'], value['y'] = int(value['x']), int(value['y'])
            require(rows_by_id[value['cell']][value['y']][value['x']] not in SOLID, f'{label}: tile is solid.')
        return ok

    economy = p.get('economy', DEFAULT_ECONOMY)
    require(isinstance(economy, dict) and set(economy) == set(ECONOMY_KEYS),
            'economy requires treasury, storeHerbs, storeMeals, dailyHerbs and dailyMeals.')
    if isinstance(economy, dict):
        for key, limit in ECONOMY_KEYS.items():
            require(number(economy.get(key), 0, limit, integer=True), f'economy.{key} must be a whole number from 0 to {limit}.')
    herbs = p.get('herbPatch')
    if herbs is not None:
        place(herbs, 'Herb patch')
    routes = p.get('routes', [])
    route_ids = set()
    require(isinstance(routes, list) and len(routes) <= 128, 'routes must be an array of at most 128 patrol routes.')
    for route in routes if isinstance(routes, list) else []:
        if not isinstance(route, dict) or set(route) != {'id', 'name', 'posts'}:
            errors.append('Each patrol route requires only id, name and posts.')
            continue
        rid = route.get('id')
        require(isinstance(rid, str) and ID.fullmatch(rid) and rid not in route_ids, f'Invalid or duplicate route ID: {rid!r}.')
        route_ids.add(rid)
        name = route.get('name')
        require(isinstance(name, str) and text(name, 120) and bool(name.strip()), f'Route {rid}: invalid name.')
        posts = route.get('posts')
        require(isinstance(posts, list) and 1 <= len(posts) <= 64, f'Route {rid}: needs 1 to 64 posts.')
        for index, post in enumerate(posts if isinstance(posts, list) else []):
            place(post, f'Route {rid} post {index + 1}')
    people = p.get('people', [])
    require(isinstance(people, list) and len(people) <= MAX_PEOPLE, f'people must be an array of at most {MAX_PEOPLE} residents.')
    person_ids = set()
    for person in people if isinstance(people, list) else []:
        if not isinstance(person, dict) or set(person) - {'joinable', 'protected'} != PERSON_KEYS:
            errors.append('Each person requires exactly: ' + ', '.join(sorted(PERSON_KEYS)) + ' (and may have joinable, protected).')
            continue
        require(type(person.get('joinable', False)) is bool, f'Person {person["id"]}: joinable must be true or false.')
        require(type(person.get('protected', False)) is bool, f'Person {person["id"]}: protected must be true or false.')
        pid = person['id']
        label = f'Person {pid}'
        require(isinstance(pid, str) and ID.fullmatch(pid) and pid not in person_ids and pid != 'treasury'
                and not pid.startswith(('wolf-', 'player-')), f'Invalid or duplicate person ID: {pid!r}.')
        person_ids.add(pid)
        require(isinstance(person['name'], str) and text(person['name'], 120) and bool(person['name'].strip()), f'{label}: invalid name.')
        require(person['role'] in ROLES, f'{label}: role must be merchant, guard or civilian.')
        require(isinstance(person['workLabel'], str) and text(person['workLabel'], 40) and bool(person['workLabel'].strip()),
                f'{label}: activity label must be 1-40 characters.')
        require(text(person['description'], 4096) if isinstance(person['description'], str) else False, f'{label}: invalid description.')
        require(text(person['greeting'], 1024) if isinstance(person['greeting'], str) else False, f'{label}: invalid greeting.')
        require(isinstance(person['personality'], str) and text(person['personality'], 2000, True), f'{label}: invalid personality.')
        require(isinstance(person['backstory'], str) and text(person['backstory'], 6000, True), f'{label}: invalid backstory.')
        require(number(person['age'], 0, 200, integer=True), f'{label}: age must be 0-200.')
        require(number(person['voice'], 0, 31, integer=True), f'{label}: voice color must be 0-31.')
        require(type(person['paid']) is bool, f'{label}: paid must be true or false.')
        for key, limit in (('purse', 100000), ('herbs', 10000), ('meals', 10000)):
            require(number(person[key], 0, limit, integer=True), f'{label}: {key} must be 0-{limit}.')
        look = person['appearance']
        require(isinstance(look, dict) and set(look) == APPEARANCE_KEYS and look.get('species') in SPECIES
                and look.get('sex') in ('female', 'male') and look.get('stature') in ('short', 'average', 'tall')
                and look.get('pattern') in PATTERNS
                and all(number(look.get(k), 0, 7, integer=True) for k in ('baseColor', 'gradientColor', 'markingColor')),
                f'{label}: invalid appearance.')
        hours = person['hours']
        require(isinstance(hours, dict) and set(hours) == {'start', 'end'} and number(hours.get('start'), 0, 23.75)
                and number(hours.get('end'), 0, 23.75) and hours.get('start') != hours.get('end'),
                f'{label}: hours need different start and end times from 0 to 23.75.')
        route = person['route']
        require(route == '' or (person['role'] != 'merchant' and route in route_ids),
                f'{label}: only guards (on patrol) and civilians (travelling) walk routes, and the route must exist.')
        for key in ('home', 'work', 'evening'):
            place(person[key], f'{label} {key}')
        if person['role'] == 'merchant' and person['work'].get('cell') in by_id:
            w = person['work']
            rows = rows_by_id[w['cell']]
            c = by_id[w['cell']]
            require(any(0 <= w['x'] + dx < c['width'] and 0 <= w['y'] + dy < c['height']
                        and rows[w['y'] + dy][w['x'] + dx] not in SOLID
                        for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0), (1, 1), (-1, 1), (1, -1), (-1, -1))),
                    f'{label}: customers need an open tile beside the counter.')
    for area in p.get('cells', []) + p.get('rooms', []):
        letting = area.get('letting') if isinstance(area, dict) else None
        if isinstance(letting, dict) and isinstance(letting.get('landlord'), str):
            require(letting['landlord'] == 'treasury' or letting['landlord'] in person_ids,
                    f'{area.get("id")}: the place to let names an unknown landlord.')
    slots = p.get('slots', [])
    require(isinstance(slots, list) and len(slots) <= 256, 'slots must be an array of at most 256 profession slots.')
    slot_ids = set()
    for slot in slots if isinstance(slots, list) else []:
        if not isinstance(slot, dict) or set(slot) != SLOT_KEYS:
            errors.append('Each profession slot requires exactly: ' + ', '.join(sorted(SLOT_KEYS)) + '.')
            continue
        sid = slot['id']
        label = f'Slot {slot.get("name", sid)}'
        require(isinstance(sid, str) and ID.fullmatch(sid) and sid not in slot_ids, f'Invalid or duplicate slot ID: {sid!r}.')
        slot_ids.add(sid)
        require(isinstance(slot['name'], str) and text(slot['name'], 120) and bool(slot['name'].strip()), f'{label}: invalid name.')
        require(isinstance(slot['profession'], str) and ID.fullmatch(slot['profession']), f'{label}: choose a profession.')
        require(isinstance(slot['workLabel'], str) and text(slot['workLabel'], 40), f'{label}: activity label is at most 40 characters.')
        require(type(slot['paid']) is bool, f'{label}: paid must be true or false.')
        for key, limit in (('purse', 100000), ('herbs', 10000), ('meals', 10000)):
            require(number(slot[key], 0, limit, integer=True), f'{label}: {key} must be 0-{limit}.')
        hours = slot['hours']
        require(isinstance(hours, dict) and set(hours) == {'start', 'end'} and number(hours.get('start'), 0, 23.75)
                and number(hours.get('end'), 0, 23.75) and hours.get('start') != hours.get('end'), f'{label}: invalid hours.')
        require(slot['route'] == '' or slot['route'] in route_ids, f'{label}: the patrol route must exist.')
        for key in ('home', 'work', 'evening'):
            place(slot[key], f'{label} {key}')
    if people and not any(isinstance(x, dict) and x.get('role') == 'merchant' for x in people):
        warnings.append('No merchant: residents will have nowhere to buy food.')
    if herbs is None and people:
        warnings.append('No herb patch: players cannot gather herbs in this world.')


def buckets(cells):
    """World cells filed under every BUCKET-sized square they cover."""
    out = {}
    for c in cells:
        for by in range(c['y'] // BUCKET, (c['y'] + c['height'] - 1) // BUCKET + 1):
            for bx in range(c['x'] // BUCKET, (c['x'] + c['width'] - 1) // BUCKET + 1):
                out.setdefault((bx, by), []).append(c)
    return out


def overlapping_cells(cells):
    """Pairs of world cells that overlap (overlapping cells always share a bucket)."""
    found = set()
    for group in buckets(cells).values():
        for i, a in enumerate(group):
            for b in group[i + 1:]:
                if (a['x'] < b['x'] + b['width'] and b['x'] < a['x'] + a['width']
                        and a['y'] < b['y'] + b['height'] and b['y'] < a['y'] + a['height']):
                    found.add((a['id'], b['id']) if a['id'] < b['id'] else (b['id'], a['id']))
    by_id = {c['id']: c for c in cells}
    return [(by_id[a], by_id[b]) for a, b in sorted(found)]


def cell_rows(p, c):
    return list(c['terrain'])


def tile_height(p, c, x, y):
    glyph = c['terrain'][y][x]
    return c.get('heights', {}).get(f'{x},{y}', GLYPH_HEIGHTS.get(glyph, 0))


def arrival_tile(p, c, end, used, kind):
    rows = cell_rows(p, c)
    x, y = int(end['x']), int(end['y'])
    source_height = tile_height(p, c, x, y)
    if kind == 'stairs':
        # Stairs stamp the glyph but preserve any explicit height override.
        source_height = c.get('heights', {}).get(f'{x},{y}', .5)
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


def world_x(p, c):
    """Where a cell or interior sits on the world map: world cells in world tiles, interiors at their overview spot."""
    return c['worldX'] if 'worldX' in c else c['x']


def world_y(p, c):
    return c['worldY'] if 'worldY' in c else c['y']


def world_id(p):
    """Stable key for roster assignments; older atlases without an id use their name."""
    if p.get('id'):
        return p['id']
    slug = re.sub(r'[^a-z0-9]+', '_', str(p.get('name', 'world')).lower()).strip('_')[:40] or 'world'
    return slug if slug[0].isalpha() else 'w_' + slug


def resolve_slots(p, roster_data, glyph_at=None):
    """Fills profession slots from the roster. Returns (updated roster, plan, residents, warnings).

    `glyph_at(cell, x, y)` (None where unknown) stands in for the project's ground, for a plan made from a slot plan
    request (slot_plan), which carries only the tiles beside each slot's work spot; no residents are made then."""
    if not p.get('slots'):
        return roster_data, [], [], []
    updated, plan, warnings = roster_lib.assign({**p, 'id': world_id(p)}, roster_data)
    professions = {x['id']: x for x in updated['professions']}
    errors = []
    for slot in p['slots']:
        prof = professions.get(slot['profession'])
        if prof and slot['route'] and prof['behavior'] != 'guard':
            errors.append(f'Slot {slot["name"]}: only guard professions walk patrol routes.')
        if prof and prof['behavior'] == 'merchant':
            w = slot['work']
            if glyph_at is None:
                rows = cell_rows(p, next(c for c in p['cells'] + p['rooms'] if c['id'] == w['cell']))
                glyph = lambda x, y: rows[y][x] if 0 <= y < len(rows) and 0 <= x < len(rows[0]) else None
            else:
                glyph = lambda x, y: glyph_at(w['cell'], x, y)
            if not any(glyph(w['x'] + dx, w['y'] + dy) not in (None, *SOLID)
                       for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0), (1, 1), (-1, 1), (1, -1), (-1, -1))):
                errors.append(f'Slot {slot["name"]}: customers need an open tile beside the counter.')
    named = {x['id'] for x in p.get('people', [])}
    clash = [e['character'] for e in plan if e['character'] in named]
    if clash:
        errors.append('Roster characters share an ID with named NPCs: ' + ', '.join(clash))
    if errors:
        raise ValidationError(errors)
    return updated, plan, roster_lib.residents_for_slots(p, updated, plan) if glyph_at is None else [], warnings


MAX_SLOTS = 20000


def slot_plan(request, roster_data):
    """Who would fill each profession slot, from only what that depends on (Atlas sends it when any of it changes):
    {id, name, slots, people: [named NPC IDs], ground: {"cell|x|y": glyph} beside each slot's work spot}.
    Returns (plan, warnings); raises ValueError for a malformed request and ValidationError for problems."""
    if not isinstance(request, dict):
        raise ValueError('A slot plan request is an object.')
    slots, people, ground = request.get('slots'), request.get('people'), request.get('ground', {})
    place = lambda v: isinstance(v, dict) and isinstance(v.get('cell'), str) and all(isinstance(v.get(k), int) for k in ('x', 'y'))
    if (not isinstance(slots, list) or len(slots) > MAX_SLOTS or not isinstance(people, list) or not isinstance(ground, dict)
            or not all(isinstance(s_, dict) and all(isinstance(s_.get(k), str) for k in ('id', 'name', 'profession', 'route'))
                       and place(s_.get('work')) for s_ in slots)
            or not all(isinstance(i, str) for i in people)
            or not all(isinstance(k, str) and isinstance(v, str) and len(v) == 1 for k, v in ground.items())):
        raise ValueError('Malformed slot plan request.')
    p = {'id': str(request.get('id') or ''), 'name': str(request.get('name') or 'world'), 'slots': slots,
         'people': [{'id': i} for i in people], 'cells': [], 'rooms': []}
    _, plan, _, warnings = resolve_slots(p, roster_data, lambda cell, x, y: ground.get(f'{cell}|{x}|{y}'))
    return plan, warnings


def export_files(value, roster_data=None, with_roster=False, stream=False):
    """Builds the runtime files. with_roster also returns (files, updated roster, plan).

    stream builds the layout the game server streams from the database (no size limit): a "RATW_WORLD 3" manifest
    that names each cell with an "area" record, lists which cells each one's seams lead to ("exits"), and keeps
    only explicit doors; each cell's file under cells/ID.cell and its side of every seam under seams/ID.
    """
    p, warnings = check_project(value, for_game=not stream)
    if roster_data is None:
        roster_data = roster_lib.load()
    updated_roster, plan, slot_people, slot_warnings = resolve_slots(p, roster_data)
    warnings += slot_warnings
    source = p
    p = {**p, 'people': p.get('people', []) + slot_people}
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
    # Seams can only cross cell edges, so look there rather than over the whole (possibly
    # huge, mostly empty) world: a tile east of a cell's right edge is on some cell's
    # left edge, one south of its bottom edge on some cell's top edge. Sorting keeps
    # reading order, so seam IDs are stable.
    left_edge = {(c['x'], y): c for c in p['cells'] for y in range(c['y'], c['y'] + c['height'])}
    top_edge = {(x, c['y']): c for c in p['cells'] for x in range(c['x'], c['x'] + c['width'])}
    crossings = []
    for c in p['cells']:
        for y in range(c['y'], c['y'] + c['height']):
            x = c['x'] + c['width'] - 1
            if (x + 1, y) in left_edge:
                crossings.append((y, x, 0, c, left_edge[(x + 1, y)]))
        for x in range(c['x'], c['x'] + c['width']):
            y = c['y'] + c['height'] - 1
            if (x, y + 1) in top_edge:
                crossings.append((y, x, 1, c, top_edge[(x, y + 1)]))
    seam = 0
    for y, x, direction, ca, cb in sorted(crossings, key=lambda k: k[:3]):
        dx, dy, edge, back = ((1, 0, 'E', 'W'), (0, 1, 'S', 'N'))[direction]
        nx, ny = x + dx, y + dy
        ax, ay, bx, by = x - ca['x'], y - ca['y'], nx - cb['x'], ny - cb['y']
        if (ca.get('z', 0) != cb.get('z', 0) or ca['terrain'][ay][ax] in SOLID
                or cb['terrain'][by][bx] in SOLID or (ca['id'], ax, ay) in used or (cb['id'], bx, by) in used
                or abs(tile_height(p, ca, ax, ay) - tile_height(p, cb, bx, by)) > .55):
            continue
        for a, b, px, py, tx, ty, e, suffix, other in (
                (ca, cb, ax, ay, bx, by, edge, 'a', 'b'),
                (cb, ca, bx, by, ax, ay, back, 'b', 'a')):
            fixtures.append((f'seam_{seam}_{suffix}', 'Open boundary', a['id'], px + .5, py + .5,
                             b['id'], tx + .5, ty + .5, f'seam_{seam}_{other}', True, False, True, True, e))
        seam += 1
    # A streamed build's seams travel with their cells, so only doors and stairs count against the manifest's limit.
    if len([f for f in fixtures if not (stream and f[0].startswith('seam_'))]) > 65536:
        raise ValidationError(['Export exceeds 65536 fixtures; use larger cells or fewer links.'])
    files = {}
    content = bool(p['people'] or p.get('routes') or p.get('herbPatch') or 'economy' in p or p.get('slots'))
    manifest = ['RATW_WORLD 3' if stream else 'RATW_WORLD 2' if content else 'RATW_WORLD 1']
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
                        height = c.get('heights', {}).get(f'{x},{y}', .5)
                base = GLYPH_HEIGHTS.get(row[x], 0)
                if height != base:
                    overrides[(x, y)] = height
        headers = [f'id: {c["id"]}', f'name: {c["name"]}',
                   'description: ' + c.get('description', '').replace('\n', ' '),
                   f'world: {world_x(p, c)} {world_y(p, c)} {c.get("z", 0)}', f'size: {c["width"]} {c["height"]}',
                   f'outdoors: {str(c["outdoors"]).lower()}', f'weather: {c["weather"]}',
                   'wind: 0 0.5 1' if c['outdoors'] else 'wind: 0 0 0']
        light = c.get('lighting', {'artificial': 1, 'daylightAccess': 1, 'tone': 'warm'})
        headers.append(f'lighting: {light["artificial"]:.17g} {light["daylightAccess"]:.17g} {light["tone"]}')
        headers += [f'height: {x} {y} {h:g}' for (x, y), h in sorted(overrides.items())]
        files[f'cells/{c["id"]}.cell'] = '\n'.join(headers + ['grid:'] + [''.join(row) for row in rows]) + '\n'
        manifest.append(f'area {quote(c["id"])}' if stream else f'cell {quote(c["id"])} {quote("cells/" + c["id"] + ".cell")}')
        territory = c.get('territory', {'region': 'unassigned', 'claims': [], 'chapter': ''})
        if territory != {'region': 'unassigned', 'claims': [], 'chapter': ''}:
            claims = ''.join(' ' + quote(claim) for claim in territory['claims'])
            manifest.append(f'territory {quote(c["id"])} {quote(territory["region"])} '
                            f'{quote(territory["chapter"] or "-")} {len(territory["claims"])}{claims}')
        if c.get('letting'):
            let = c['letting']
            manifest.append(f'let {quote(c["id"])} {quote(let["kind"])} {quote(let["landlord"])} {let["rent"]} {let["level"]}')
    spawn = p['spawn']
    seams, exits = {}, {}
    if stream:
        # Each seam travels with the cell it stands in; the manifest keeps which cells they lead to.
        for f in fixtures:
            if f[0].startswith('seam_'):
                exits.setdefault(f[2], set()).add(f[5])
        for cid in sorted(exits):
            manifest.append(f'exits {quote(cid)} {len(exits[cid])} ' + ' '.join(quote(n) for n in sorted(exits[cid])))
    manifest.append(f'spawn {quote(spawn["cell"])} {spawn["x"] + .5} {spawn["y"] + .5}')
    for f in fixtures:
        a, name, cell, x, y, target, ax, ay, pair, opened, locked, boundary, passage, edge = f
        line = (f'door {quote(a)} {quote(name)} {quote(cell)} {x:g} {y:g} {quote(target)} '
                f'{ax:g} {ay:g} {quote(pair)} {int(opened)} {int(locked)} {int(boundary)} {int(passage)} {quote(edge)}')
        if stream and a.startswith('seam_'):
            seams.setdefault(cell, []).append(line)
        else:
            manifest.append(line)
    if content:
        e = p.get('economy', DEFAULT_ECONOMY)
        manifest.append(f'economy {e["treasury"]} {e["storeHerbs"]} {e["storeMeals"]} {e["dailyHerbs"]} {e["dailyMeals"]}')
    at = lambda spot: f'{quote(spot["cell"])} {spot["x"] + .5:g} {spot["y"] + .5:g}'
    if p.get('herbPatch'):
        manifest.append('herbs ' + at(p['herbPatch']))
    for route in p.get('routes', []):
        manifest.append(f'route {quote(route["id"])} {len(route["posts"])} ' + ' '.join(at(post) for post in route['posts']))
    for r in p.get('people', []):
        look = r['appearance']
        fold = lambda value: value.replace('\n', ' ')
        manifest.append(' '.join([
            'resident', quote(r['id']), quote(r['name']), quote(r['role']), quote(r['workLabel']),
            quote(fold(r['description'])), quote(fold(r['greeting'])), str(r['age']),
            quote(look['species']), quote(look['sex']), quote(look['stature']), quote(look['pattern']),
            str(look['baseColor']), str(look['gradientColor']), str(look['markingColor']), str(r['voice']),
            str(int(r['paid'])), f'{r["hours"]["start"]:g}', f'{r["hours"]["end"]:g}', quote(r['route'] or '-'),
            str(r['purse']), str(r['herbs']), str(r['meals']), at(r['home']), at(r['work']), at(r['evening'])]))
        if r.get('personality') or r.get('backstory'):
            manifest.append(f'story {quote(r["id"])} {quote(fold(r.get("personality", "")))} {quote(fold(r.get("backstory", "")))}')
        if r.get('joinable'):
            manifest.append(f'joinable {quote(r["id"])}')     # May travel with a player's party (doc 32, 2.3).
        if r.get('protected'):
            manifest.append(f'protected {quote(r["id"])}')    # Can't be attacked or robbed of coin by players (doc 57, 6).
    files['world.ratw'] = '\n'.join(manifest) + '\n'
    if stream:
        for c in by_id.values():
            files[f'seams/{c["id"]}'] = ''.join(line + '\n' for line in seams.get(c['id'], []))
    files['atlas.json'] = json.dumps(source, ensure_ascii=False, indent=2) + '\n'
    files['README.txt'] = ('RATW authored world export v1\n\n'
        'This is offline content, not a game save. Extract into a NEW directory.\n'
        'Launch from the RATWMUD repository:\n'
        'bash tools/play.sh --world /absolute/path/to/world.ratw\n'
        'A separate custom-world save is used by default. Do not reuse a save after changing cell topology.\n'
        f'Residents: {len(p.get("people", []))}; patrol routes: {len(p.get("routes", []))}.\n'
        'atlas.json reopens the editable world (atlas v3: every cell holds its own ground); .cell files are the runtime cells.\n'
        f'Cells: {len(by_id)}; reciprocal boundary pairs: {seam}; explicit links: {len(p["links"])}.\n'
        + '\n'.join(warnings) + '\n')
    if with_roster:
        return files, updated_roster, plan
    return files


class RosterFile:
    """The shared roster as a JSON file. world_store.DbRoster has the same interface."""

    def __init__(self, path=None):
        self.path = path or roster_lib.ROSTER

    def load(self):
        return roster_lib.load(self.path)

    def save(self, roster):
        roster_lib.save(roster, self.path)
        return roster_lib.load(self.path)


def roster_store(roster=None):
    """A roster store from a store, a file path, or None for the default file."""
    return roster if hasattr(roster, 'load') else RosterFile(roster)


def export_and_assign(project, roster=None):
    """Export that makes profession assignments permanent in the shared roster (a store or a file path)."""
    store = roster_store(roster)
    current = store.load()
    files, updated, plan = export_files(project, current, with_roster=True)
    if any(entry['new'] for entry in plan) or updated != current:
        store.save(updated)
    return files, plan


def export_zip(project):
    content = io.BytesIO()
    files, _ = export_and_assign(project)
    with zipfile.ZipFile(content, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, data in files.items():
            archive.writestr(name, data.encode('utf-8'))
    return content.getvalue()


def write_export(project, output: Path, roster_path=None):
    output = output.resolve()
    if output.exists():
        raise FileExistsError(f'{output} already exists; exports always go to a new directory.')
    files, _ = export_and_assign(project, roster_path)  # Validates before making any output.
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


DIST = ROOT / 'Editor' / 'dist'
WORLDS = ROOT / 'Data' / 'Worlds'
PORTRAITS = ROOT / 'Data' / 'Portraits'
PLAYTESTS = ROOT / 'Saved' / 'Playtests'
MIME = {'.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css', '.svg': 'image/svg+xml',
        '.png': 'image/png', '.json': 'application/json', '.woff2': 'font/woff2', '.ttf': 'font/ttf',
        '.ico': 'image/x-icon'}
NOT_BUILT = (b'<!doctype html><meta charset=utf-8><title>Atlas Workshop</title><body style="font:15px system-ui;'
             b'background:#0d1413;color:#ded5c3;padding:40px"><h1>Atlas Workshop is not built yet</h1><p>Run once:</p>'
             b'<pre>npm --prefix Editor install\nnpm --prefix Editor run build</pre><p>Then reload this page.</p>')


def bundled_worlds():
    out = []
    if WORLDS.is_dir():
        for folder in sorted(p for p in WORLDS.iterdir() if p.is_dir()):
            sources = list(folder.glob('*.atlas.json'))
            if len(sources) == 1:
                try:
                    p = read_json(sources[0].read_text(encoding='utf-8'))
                    out.append({'id': folder.name, 'name': p.get('name', folder.name), 'source': sources[0],
                                'people': len(p.get('people', [])) + len(p.get('slots', [])),
                                'cells': len(p.get('cells', [])) + len(p.get('rooms', []))})
                except (ValueError, OSError):
                    continue
    return out


def bundled_folder(world_key):
    """The Data/Worlds entry for a folder name or a world ID, or None."""
    for world in bundled_worlds():
        if world['id'] == world_key:
            return world
        try:
            if world_id(read_json(world['source'].read_text(encoding='utf-8'))) == world_key:
                return world
        except (ValueError, OSError):
            continue
    return None


def write_bundled(world, project, roster):
    """Writes a bundled world's atlas and regenerates the game files it loads from. Validates first."""
    files, _ = export_and_assign(project, roster)
    folder = world['source'].parent
    world['source'].write_text(json.dumps(project, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    for old in (folder / 'cells').glob('*.cell'):
        if f'cells/{old.name}' not in files:
            old.unlink()
    for name, text_ in files.items():
        if name == 'world.ratw' or name.startswith('cells/'):
            (folder / name).parent.mkdir(parents=True, exist_ok=True)
            (folder / name).write_text(text_, encoding='utf-8')
    return world['source']


def merge_roster(incoming, on_disk):
    """Editor saves never rewrite assignments (exports own them) or erase anyone who has worked."""
    incoming = roster_lib.check_roster(incoming)
    disk = {c['id']: c for c in on_disk['characters']}
    seen = set()
    for c in incoming['characters']:
        seen.add(c['id'])
        old = disk.get(c['id'])
        c['assignment'], c['profession'] = (old['assignment'], old['profession']) if old else (None, '')
    for cid, old in disk.items():
        if cid not in seen and old['profession']:
            # Someone who has held a job keeps their record and history, marked removed.
            incoming['characters'].append({**old, 'status': 'removed' if old['status'] == 'active' else old['status']})
    return roster_lib.check_roster(incoming)


# How long a playtest server may sit with nobody playing before it stops: Atlas opens the game in the browser, so
# closing the page ends the playtest instead of leaving a server running (they used to pile up, one per launch).
PLAYTEST_IDLE = 300


def wait_until_listening(process, log: Path, seconds: float = 240.0):
    """Waits for the launched server to say it is listening, so a server that is refused or fails is reported, not
    announced as started. Still building after `seconds` (DEV's world can take a while): left to carry on."""
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        text = log.read_text(errors='replace') if log.exists() else ''
        if 'RATW server listening on port' in text:          # (Not the NPC Mind's own "listening on port".)
            return
        rejected = [line for line in text.splitlines() if 'RATW_WORLD_REJECTED' in line]
        if rejected:
            raise HostError('The game did not start: ' + rejected[-1].split('RATW_WORLD_REJECTED:', 1)[-1].strip())
        if process.poll() is not None:
            last = [line for line in text.splitlines() if line.strip()][-3:]
            raise HostError('The game did not start (exit ' + str(process.returncode) + '): ' + (' / '.join(last) or 'no output'))
        time.sleep(0.25)


def launch_game(manifest: Path, save: Path, quick: bool, log: Path):
    command = ['bash', str(ROOT / 'tools' / 'play.sh'), '--world', str(manifest), '--save', str(save),
               '--idle-exit', str(PLAYTEST_IDLE)]
    if quick:
        command += ['--identity', 'tester', '--name', 'Tester']
    with open(log, 'wb') as out:
        process = subprocess.Popen(command, cwd=ROOT, stdout=out, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                                   start_new_session=True)
    wait_until_listening(process, log)
    return command


def launch_live(quick: bool, log: Path):
    """Plays DEV's world as the DEV server would: builds it, then streams it from the database (no size limit). As a
    scratch server (Docs/Design/20-world-database.md): it reads DEV's world and save but writes nothing back, so it runs
    beside the real DEV server, and the playtest never touches the game's save database."""
    command = ['bash', str(ROOT / 'tools' / 'live.sh'), 'play', 'dev', '--scratch', '--idle-exit', str(PLAYTEST_IDLE)]
    if quick:
        command += ['--identity', 'tester', '--name', 'Tester']
    with open(log, 'wb') as out:
        process = subprocess.Popen(command, cwd=ROOT, stdout=out, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                                   start_new_session=True)
    wait_until_listening(process, log)
    return command


def lacks_ground(project) -> bool:
    """A project from an editor that loaded lean, with cells whose ground it never fetched (the host fills them in)."""
    return isinstance(project, dict) and isinstance(project.get('cells'), list) and any(
        isinstance(c, dict) and c.get('terrain') is None for c in project['cells'])


def tile_count(project) -> int:
    return sum(int(a.get('width', 0)) * int(a.get('height', 0)) for a in project.get('cells', []) + project.get('rooms', [])
               if isinstance(a, dict) and isinstance(a.get('width'), int) and isinstance(a.get('height'), int))


def make_server(port=8765, roster_path=None, launcher=launch_game, ai_config_path=None, world=None, publisher=None,
                live_launcher=launch_live):
    """`world` is the live world (live_edit.LiveWorld) and `publisher` Push to live (publish.Publisher);
    without them only the roster file and exports work."""
    token = secrets.token_urlsafe(24)
    rosters = world.roster if world else RosterFile(roster_path)

    def live():
        if world is None:
            error = HostError('Live editing needs the DEV database; start the editor with tools/editor.sh.')
            error.status = 503
            raise error
        return world

    def publishing():
        if publisher is None:
            error = HostError('Push to live needs the DEV and PROD databases; start the editor with tools/editor.sh.')
            error.status = 503
            raise error
        return publisher
    lock = threading.Lock()  # One roster read-modify-write at a time.
    ai = {'config': None, 'error': 'No AI config found (Saved/Config/RATWNPCAI.local.json or RATW_AI_CONFIG).'}
    config_path = ai_config_path or roster_lib.default_ai_config()
    if config_path:
        try:
            import npc_bridge
            ai['config'], ai['error'] = npc_bridge.load_config(Path(config_path)), ''
        except Exception as error:  # noqa: BLE001 - only a safe code leaves the bridge.
            ai['error'] = f'AI config unusable: {getattr(error, "code", "invalid")}.'

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            # Request paths only; never log project bodies or the session token.
            pass

        def trusted(self):
            port_ = self.server.server_port
            hosts = (f'127.0.0.1:{port_}', f'localhost:{port_}')
            origins = tuple(f'http://{h}' for h in hosts) + ('http://127.0.0.1:5173', 'http://localhost:5173')
            forwarded = self.headers.get('Host') in ('127.0.0.1:5173', 'localhost:5173')  # Vite dev proxy.
            return ((self.headers.get('Host') in hosts or forwarded)
                    and self.headers.get('Origin', 'http://' + hosts[0]) in origins)

        def reply(self, status, data, mime='application/json', download=False):
            body = data if isinstance(data, bytes) else json.dumps(data, ensure_ascii=True).encode('utf-8')
            self.send_response(status)
            self.send_header('Content-Type', mime + '; charset=utf-8' if mime.startswith(('text/', 'application/json')) else mime)
            body = http_body.send(self, body, mime)
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.send_header('Content-Security-Policy', "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; object-src 'none'")
            if download:
                self.send_header('Content-Disposition', 'attachment; filename="ratw-world.zip"')
            self.end_headers()
            self.wfile.write(body)

        def static(self, path):
            if path.startswith('/portraits/'):
                name = path.rsplit('/', 1)[1]
                file = PORTRAITS / name
                if re.fullmatch(r'[a-z]+\.png', name) and file.is_file():
                    return self.reply(200, file.read_bytes(), 'image/png')
                return self.reply(404, {'error': 'Not found.'})
            if not (DIST / 'index.html').is_file():
                return self.reply(200, NOT_BUILT, 'text/html') if path in ('/', '/index.html') else self.reply(404, {'error': 'Not found.'})
            relative = 'index.html' if path in ('/', '/index.html') else path.lstrip('/')
            file = (DIST / relative).resolve()
            # Only files inside the built bundle, never anything else on disk.
            if DIST.resolve() not in file.parents or not file.is_file() or file.suffix not in MIME:
                return self.reply(404, {'error': 'Not found.'})
            return self.reply(200, file.read_bytes(), MIME[file.suffix])

        def body(self):
            if self.headers.get_content_type() != 'application/json' or self.headers.get('Transfer-Encoding'):
                raise PermissionError('A bounded JSON request is required.')
            size = int(self.headers.get('Content-Length', '-1'))
            if not 0 < size <= MAX_BODY:
                raise OverflowError('Request must be at most 64 MB.')
            self.connection.settimeout(10)
            raw = self.rfile.read(size)
            if len(raw) != size:
                raise ValueError('Incomplete request body.')
            return read_json(raw)

        def do_GET(self):
            if not self.trusted():
                return self.reply(403, {'error': 'Local editor origin required.'})
            path = urlsplit(self.path).path
            query = parse_qs(urlsplit(self.path).query)
            if path == '/api/session':
                return self.reply(200, {'token': token})
            if path == '/api/demo':
                return self.reply(200, demo_project())
            try:
                if path == '/api/live/world':
                    # ?lean=1: cell outlines and previews now, each cell's ground as it comes into view.
                    return self.reply(200, live().load(lean=query.get('lean') == ['1']))
                if path == '/api/live/ground':
                    ids = [i for i in ','.join(query.get('cells', [])).split(',') if i]
                    return self.reply(200, live().ground(ids))
                if path == '/api/live/camps':
                    return self.reply(200, live().camps())
                if path == '/api/publish':
                    return self.reply(200, publishing().preview())
                if path == '/api/roster':
                    with lock:
                        return self.reply(200, rosters.load())
            except HostError as error:
                return self.reply(error.status, {'error': str(error)})
            except ValueError as error:
                return self.reply(400, {'error': str(error)})
            if path == '/api/ai':
                return self.reply(200, {'available': ai['config'] is not None, 'model': getattr(ai['config'], 'model', ''), 'error': ai['error']})
            return self.static(path)

        def do_POST(self):
            if not self.trusted() or not secrets.compare_digest(self.headers.get('X-RATW-Editor', ''), token):
                return self.reply(403, {'error': 'Local editor session required.'})
            path = urlsplit(self.path).path
            try:
                data = self.body()
                # A lean editor sends cells whose ground it never fetched without it; the database has it.
                if world is not None and path in ('/api/validate', '/api/export', '/api/roster/preview') and lacks_ground(data):
                    data = live().fill(data)
                if world is not None and path == '/api/playtest' and isinstance(data, dict) and lacks_ground(data.get('project')):
                    data['project'] = live().fill(data['project'])
                if path == '/api/validate':
                    # A world edited live in DEV is streamed, so its size is a note, not an error.
                    _, warnings = check_project(data, for_game=world is None)
                    if world is not None and tile_count(data) > MAX_TILES:
                        warnings.append(f'Cells plus interiors cover {tile_count(data)} tiles, more than a ▶ Play '
                                        f'export holds ({MAX_TILES}); ▶ Play runs it from the DEV database instead.')
                    return self.reply(200, {'valid': True, 'errors': [], 'warnings': warnings})
                if path == '/api/export':
                    with lock:
                        files, _ = export_and_assign(data, rosters)
                    content = io.BytesIO()
                    with zipfile.ZipFile(content, 'w', zipfile.ZIP_DEFLATED) as archive:
                        for name, text_ in files.items():
                            archive.writestr(name, text_.encode('utf-8'))
                    return self.reply(200, content.getvalue(), 'application/zip', True)
                if path in ('/api/live/edit', '/api/live/sync'):
                    if not isinstance(data, dict):
                        raise ValueError('Bad request.')
                    try:
                        return self.reply(200, live().edit(data) if path == '/api/live/edit' else live().sync(data))
                    except HostError as error:
                        # Conflicts and refusals carry the current values so the editor can show the true state.
                        extra = {k: getattr(error, k) for k in ('conflicts', 'current') if hasattr(error, k)}
                        return self.reply(error.status, {'error': str(error), **extra})
                if path in ('/api/publish/push', '/api/publish/rollback', '/api/publish/pull'):
                    if not isinstance(data, dict):
                        raise ValueError('Bad request.')
                    with lock:
                        run = {'push': publishing().push, 'rollback': publishing().rollback, 'pull': publishing().pull}[path.rsplit('/', 1)[1]]
                        return self.reply(200, run(data))
                if path == '/api/roster':
                    with lock:
                        merged = rosters.save(merge_roster(data, rosters.load()))
                    return self.reply(200, merged)
                if path == '/api/roster/plan':
                    with lock:
                        current = rosters.load()
                    plan, warnings = slot_plan(data, current)
                    names = {c['id']: c['name'] for c in current['characters']}
                    return self.reply(200, {'plan': [{**e, 'name': names.get(e['character'], e['character'])} for e in plan], 'warnings': warnings})
                if path == '/api/roster/preview':
                    with lock:
                        current = rosters.load()
                    p, _ = check_project(data, for_game=world is None)
                    _, plan, _, warnings = resolve_slots(p, current)
                    names = {c['id']: c['name'] for c in current['characters']}
                    return self.reply(200, {'plan': [{**e, 'name': names.get(e['character'], e['character'])} for e in plan], 'warnings': warnings})
                if path == '/api/roster/generate':
                    if not ai['config']:
                        return self.reply(503, {'error': ai['error']})
                    if not isinstance(data, dict):
                        raise ValueError('Bad request.')
                    with lock:
                        current = rosters.load()
                    made = roster_lib.generate(ai['config'], current, int(data.get('count', 1)),
                                               [f for f in data.get('favor', []) if isinstance(f, str)], str(data.get('theme', '')))
                    return self.reply(200, {'characters': made})
                if path == '/api/playtest':
                    if not isinstance(data, dict) or not isinstance(data.get('project'), dict):
                        raise ValueError('Bad request.')
                    stamp = time.strftime('%Y%m%d-%H%M%S')
                    name = world_id(data['project']) + '-' + stamp
                    folder = PLAYTESTS / name
                    if world is not None and tile_count(data['project']) > MAX_TILES:
                        # Too big for a single-file playtest: the world being edited is DEV's, so play DEV's build.
                        folder.mkdir(parents=True, exist_ok=True)
                        command = live_launcher(bool(data.get('quickStart', True)), folder / 'game.log')
                        shown = folder.relative_to(ROOT) if ROOT in folder.parents else folder
                        return self.reply(200, {'folder': str(shown), 'manifest': 'DEV database (streamed build)',
                                                'command': ' '.join(shlex.quote(c) for c in command)})
                    with lock:
                        manifest = write_export(data['project'], folder, rosters)
                    command = launcher(manifest, folder / 'save.json', bool(data.get('quickStart', True)), folder / 'game.log')
                    shown = folder.relative_to(ROOT) if ROOT in folder.parents else folder
                    return self.reply(200, {'folder': str(shown), 'manifest': str(manifest),
                                            'command': ' '.join(shlex.quote(c) for c in command)})
                return self.reply(404, {'error': 'Not found.'})
            except ValidationError as error:
                return self.reply(422, {'valid': False, 'errors': error.errors, 'warnings': []})
            except HostError as error:
                return self.reply(error.status, {'error': str(error), **({'errors': error.errors} if hasattr(error, 'errors') else {})})
            except roster_lib.RosterError as error:
                return self.reply(422, {'valid': False, 'errors': error.errors, 'warnings': []})
            except PermissionError as error:
                return self.reply(415, {'error': str(error)})
            except OverflowError as error:
                return self.reply(413, {'error': str(error)})
            except TimeoutError:
                return self.reply(408, {'error': 'Request body timed out.'})
            except (ValueError, TypeError, KeyError, RecursionError):
                return self.reply(400, {'error': 'Malformed request JSON.'})

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
        # live_edit imports this module by name; run as a script it is __main__, and a second
        # copy would have its own HostError that this server's handlers do not catch.
        sys.modules.setdefault('map_editor', sys.modules[__name__])
        import live_edit
        import publish
        server = make_server(args.port, world=live_edit.LiveWorld(), publisher=publish.Publisher())
        print(f'RATW Atlas editor: http://127.0.0.1:{server.server_port}\n'
              'Editing the world live in the DEV database. Local-only. Ctrl+C to stop.', flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            server.server_close()
        return 0
    if not args.project or args.project.stat().st_size > MAX_BODY:
        parser.error('Choose a project JSON file of at most 64 MB.')
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
