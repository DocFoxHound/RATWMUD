"""Sheep and rabbit farms, and more of the towns' people on the land (Docs/Design/42-money-in-circulation.md, "Jobs and
farms", the user, 2026-10-06: "we plainly need more land workers").

  cd tools && python3 -m worldgen.farms [--dry-run] [--preview DIR] [--out FILE]

Additive, on the world as DEV holds it now (backed up first, under artifacts/backups), after worldgen.industry:
  1. Sheep farms (a fenced pasture and a barn) and rabbit farms (a yard of hutches and a rabbitry: a long shed of hutches
     stacked three high) are built in the country around each settlement, as industry's sites are, every door and work
     spot walkable from the road.
  2. Each settlement keeps a shop open for about every RESIDENTS_PER_SHOP of its people (cities CITY_PER_SHOP): the
     first of each kind it needs to eat and mend (ESSENTIAL) always, the rest closing from the edge of town inwards, never
     the square's first. A closed shop becomes its family's house: the counters become tables, the room a home.
  3. Their keepers and help, and in the cities some of the town's labourers, go out to work the new farms. Everyone keeps
     their name, home, family and looks; only their work changes. Over 64, they retire.
Saved with DEV's revision checked, so an edit made meanwhile stops it rather than being lost.
"""
from __future__ import annotations

import argparse
import json
import math
import random
import re
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path

from .industry import HOURS, OX, OY, Plot, RETIRE, SETTLEMENTS, place_site, reword

SEED = 6066
RESIDENTS_PER_SHOP, CITY_PER_SHOP = 15, 10          # Placeholders (doc 42, open question 7).
CITIES = {'upper_accord', 'ser_ferro', 'ridgemere'}
# Kept open, the first of each (nearest the square): what a town needs to eat, sleep and mend.
ESSENTIAL = {'bakery', 'general', 'provisioner', 'fishmonger', 'butcher', 'inn', 'smithy'}
PREFIX = 'farm_'

KINDS = {
    'sheep': dict(yard=(26, 16), paint='pasture', building='barn', hands=4, label='raises sheep at {n}',
                  name='{n} Sheep Farm', building_name='{n} Barn', ground='open'),
    'rabbit': dict(yard=(24, 14), paint='hutches', building='rabbitry', hands=5, label='keeps rabbits at {n}',
                   name='{n} Rabbit Farm', building_name='{n} Rabbitry', ground='open'),
}
# Each settlement: its farms (kind, how many, at most), and the names they take (a farm's name is short: the work label
# "keeps rabbits at <name>" must fit 40 letters).
FARMS = {
    'upper_accord': ([('rabbit', 2), ('sheep', 2)], ['Coneyhill', 'Burrowdene', 'Ewecombe', 'Fleecemoor']),
    'ser_ferro': ([('rabbit', 2), ('sheep', 2)], ['Le Tane', 'Conigliera', 'Pecorara', 'Il Vello']),
    'ridgemere': ([('rabbit', 2), ('sheep', 2)], ['Harrowburrow', 'Kitwarren', 'Ramsfold', 'Woolgate']),
    'saltreach': ([('rabbit', 1), ('sheep', 1)], ['Dunewarren', 'Saltmead']),
    'lakeside': ([('rabbit', 1), ('sheep', 1)], ['Reedwarren', 'Shorelea']),
    'fenhollow': ([('rabbit', 1), ('sheep', 1)], ['Mirewarren', 'Fenlea']),
    'amberford': ([('rabbit', 1), ('sheep', 1)], ['Amberwarren', 'Steppelea']),
    'hollowmere_village': ([('rabbit', 1)], ['Hollowwarren']),
    'cinderbrook': ([('rabbit', 1), ('sheep', 1)], ['Ashwarren', 'Cinderlea']),
    'westmarch': ([('rabbit', 1), ('sheep', 1)], ['Chalkwarren', 'Downlea']),
    'accord_crossing': ([('rabbit', 1), ('sheep', 1)], ['Fordwarren', 'Crossinglea']),
}
STYLE = {'ser_ferro': 'serferro', 'ridgemere': 'ridgemere'}
LABOURERS = re.compile(r'carries loads|runs messages|carrying messages|washes linen|washing clothes|sweeps|sweeping|'
                       r'sells from a tray|idling|hauling crates')
HELP = re.compile(r'helping at|helps at|serves at|minds the shop|minding the shop')


def centres():
    c = {s[0]: (s[3][0] + s[3][2] // 2 + OX, s[3][1] + s[3][3] // 2 + OY) for s in SETTLEMENTS}
    c['upper_accord'] = (384, 384)
    return c


def near_cells(project, sid, centre, n=8):
    """The outdoor cells (ground level) nearest a settlement, nearest first, not its own: where its farms may go."""
    out = []
    for c in project['cells']:
        if c.get('z', 0) != 0 or not c.get('outdoors', True) or c.get('territory', {}).get('region') == sid:
            continue
        out.append((math.hypot(c['x'] + c['width'] / 2 - centre[0], c['y'] + c['height'] / 2 - centre[1]), c['id']))
    return [cid for _, cid in sorted(out)[:n]]


def door_distance(project, room_id, centre, links, cells):
    """How far a room's street door is from the settlement's centre (on the canvas); far, for a room with none."""
    link = links.get(room_id)
    if not link or link['a']['cell'] not in cells:
        return 1e9
    c = cells[link['a']['cell']]
    return math.hypot(c['x'] + link['a']['x'] - centre[0], c['y'] + link['a']['y'] - centre[1])


def rebalance(project, report):
    """The shops each settlement closes (edge first), and who that frees: {sid: ([closed room ids], [people])}."""
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from supply_balance import BUSINESSES, first_match
    region = {a['id']: a['territory']['region'] for a in project['cells'] + project['rooms']}
    links = {l['b']['cell']: l for l in project['links'] if l['kind'] == 'door'}
    cells = {c['id']: c for c in project['cells']}
    centre = centres()
    by_town = defaultdict(list)
    for p in project['people']:
        by_town[region.get(p['home']['cell'], '?')].append(p)
    out = {}
    for sid in FARMS:
        folk = by_town.get(sid, [])
        shops = []
        for p in folk:
            if p['role'] != 'merchant':
                continue
            b = first_match(p['workLabel'], BUSINESSES)
            if b and b.get('kind') in ('works', 'yard'):
                continue                              # (Industry stays.)
            kind = b['id'] if b else p['workLabel']
            shops.append((door_distance(project, p['work']['cell'], centre[sid], links, cells), kind, p))
        shops.sort(key=lambda s: (s[0], s[2]['id']))  # Nearest the square first.
        kept, closable, seen = [], [], set()
        for d, kind, p in shops:
            if kind in ESSENTIAL and kind not in seen:
                kept.append(p)
                seen.add(kind)
            else:
                closable.append((d, kind, p))
        target = max(math.ceil(len(folk) / (CITY_PER_SHOP if sid in CITIES else RESIDENTS_PER_SHOP)), len(kept))
        closing = [p for _, _, p in sorted(closable, key=lambda s: -s[0])][:max(0, len(shops) - target)]   # Edge first.
        closed = {p['work']['cell'] for p in closing}
        help_ = [p for p in folk if p['role'] == 'civilian' and 16 <= p['age'] < RETIRE and p['work']['cell'] in closed
                 and HELP.search(p['workLabel'])]
        out[sid] = (sorted(closed), closing + help_)
        report(f'{sid}: {len(folk)} residents, {len(shops)} shops: {len(closing)} close ({len(shops) - len(closing)} stay open), '
               f'{len(help_)} help with them')
    return out, by_town


def make_home(room, family_name):
    """A closed shop made its family's house: its counters tables, its name and words a home's."""
    room['terrain'] = [row.replace('=', 'T') for row in room['terrain']]
    room['name'] = f'The {family_name} House' if family_name else f'{room["name"]} (a house now)'
    room['description'] = (f'{room["name"]}: once a shop, now a home. The counter is a long table, the shelves hold the '
                           f'household\'s things, and the street door is just a front door.')
    room.pop('letting', None)


def build(project, report=print):
    """The project with the farms built and people moved onto them (a copy)."""
    out = {**project, 'cells': [dict(c) for c in project['cells']], 'rooms': [dict(r) for r in project['rooms']],
           'links': list(project['links']), 'people': [dict(p) for p in project.get('people', [])]}
    names_built = {f'{n} Barn' for _, names in FARMS.values() for n in names} | \
                  {f'{n} Rabbitry' for _, names in FARMS.values() for n in names}
    if any(r['name'] in names_built for r in out['rooms']):
        raise ValueError('farms already built (a farm\'s barn or rabbitry is there)')
    cells = {c['id']: c for c in out['cells'] if c.get('z', 0) == 0}
    centre = centres()
    reserved = {a['id'] for a in out['cells'] + out['rooms']}
    # Who is freed, and so how many farms each settlement can work.
    freed, by_town = rebalance(out, report)
    plots, built = {}, defaultdict(list)
    for sid, (wanted, names) in FARMS.items():
        pool = [p for p in freed[sid][1] if p['age'] < RETIRE]
        labour = sorted([p for p in by_town.get(sid, []) if p['role'] == 'civilian' and 16 <= p['age'] < RETIRE
                         and LABOURERS.search(p['workLabel'])], key=lambda p: p['id'])
        workers = len(pool) + len(labour) // 2
        rng = random.Random(f'{SEED}:{sid}')
        names = list(names)
        hands = 0
        for kind, most in wanted:
            spec = KINDS[kind]
            for _ in range(most):
                if hands >= workers or not names:
                    break
                n = names.pop(0)
                site = None
                for cid in near_cells(out, sid, centre[sid]):
                    if cid not in cells:
                        continue
                    if cid not in plots:
                        plots[cid] = Plot(out, cells[cid], centre[sid], reserved, sid)
                    plot = plots[cid]
                    plot.region = sid
                    site = place_site(plot, kind, spec, n, STYLE.get(sid, 'city'), rng)
                    if site:
                        break
                if not site:
                    report(f'  {sid}: no room for a {kind} farm ({n})')
                    continue
                site['kind'], site['n'] = kind, n
                built[sid].append(site)
                hands += spec['hands']
                report(f'  {sid}: {site["name"]} in {site["cell"]}')
    for plot in plots.values():
        if plot.changed:
            plot.write_back()
            out['rooms'] += [{**r, 'worldX': r['worldX'] + plot.cell['x'], 'worldY': r['worldY'] + plot.cell['y']}
                             for r in plot.site.rooms]
            out['links'] += plot.site.links
    # The closed shops, made homes.
    rooms = {r['id']: r for r in out['rooms']}
    surname = lambda p: p['name'].split()[-1] if ' ' in p['name'] else ''
    homes = 0
    for sid, (closed, people) in freed.items():
        keepers = {p['work']['cell']: p for p in people if p['role'] == 'merchant'}
        for rid in closed:
            if rid in rooms:
                make_home(rooms[rid], surname(keepers[rid]) if rid in keepers else '')
                homes += 1
    report(f'{homes} closed shops made homes')
    # The people: keepers and help out to the farms, then labourers for the hands still wanted.
    moved = Counter()
    for sid in FARMS:
        people = freed[sid][1]
        pool = sorted([p for p in people if p['age'] < RETIRE], key=lambda p: (p['role'] != 'merchant', p['id']))
        for p in people:
            if p['age'] >= RETIRE:
                p['role'], p['workLabel'], p['paid'] = 'civilian', 'retired', False
                p['work'] = dict(p['home'])
                reword(p, 'retired', 'has retired from the shop')
                moved['retired'] += 1
        jobs = []
        for s in built.get(sid, []):
            spec = KINDS[s['kind']]
            for i in range(spec['hands']):
                label = spec['label'].format(n=s['n'])[:40]
                if s['spots']:
                    x, y = s['spots'][i % len(s['spots'])]
                    where = {'cell': s['cell'], 'x': x, 'y': y}
                else:
                    room = s['records'][0]['rooms'][0]
                    spot = room['work'][i % len(room['work'])] if room['work'] else {'x': 2, 'y': 2}
                    where = {'cell': room['id'], 'x': spot['x'], 'y': spot['y']}
                jobs.append((label, where, s))
        by_site = defaultdict(list)                  # (Round the farms, so each gets some.)
        for job in jobs:
            by_site[job[2]['name']].append(job)
        jobs = [q.pop(0) for _ in range(len(jobs)) for q in [max(by_site.values(), key=len)] if q]
        labour = sorted([p for p in by_town.get(sid, []) if p['role'] == 'civilian' and 16 <= p['age'] < RETIRE
                         and LABOURERS.search(p['workLabel']) and p not in pool], key=lambda p: p['id'])
        rest = pool + labour[:min(len(labour) // 2, max(0, len(jobs) - len(pool)))]
        filled = 0
        for label, where, s in jobs:
            if not rest:
                break
            filled += 1
            p = rest.pop(0)
            p['role'], p['workLabel'], p['work'] = 'civilian', label, where
            p['hours'] = {'start': HOURS[0], 'end': HOURS[1]}
            p['paid'] = True
            p['route'] = ''
            reword(p, label)
            moved[s['kind']] += 1
        left = [p for p in rest if p in pool]
        for p in left:
            # A shop gone with no farm place left: a living at the town's labour (doc 42, Phase 3).
            p['role'], p['workLabel'], p['paid'] = 'civilian', '-', True
            p['work'] = dict(p['home'])
            reword(p, 'looking for work', 'is looking for work')
        report(f'  {sid}: {len(jobs)} farm places, {filled} filled; {len(left)} with no farm place left look for work')
    report(f'by kind: {dict(moved)}')
    return out, built


def main(argv=None):
    import map_editor
    import world_db
    import world_store
    from .western import dev_project
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dry-run', action='store_true', help='Build and check, but do not save to DEV')
    parser.add_argument('--out', type=Path, help='Write the project here as JSON')
    parser.add_argument('--preview', type=Path, help='Write a picture of each changed cell into this folder')
    args = parser.parse_args(argv)
    project, revision = dev_project()
    merged, built = build(project)
    try:
        map_editor.check_project(merged, for_game=False)
        print('Atlas validation: OK')
    except map_editor.ValidationError as error:
        print('Atlas validation failed:', *error.errors[:30], sep='\n  ')
        return 1
    if args.out:
        args.out.write_text(json.dumps(merged), encoding='utf-8')
    if args.preview:
        args.preview.mkdir(parents=True, exist_ok=True)
        changed = {s['cell'] for sites in built.values() for s in sites}
        for c in merged['cells']:
            if c['id'] in changed:
                (args.preview / f'{c["id"]}.txt').write_text('\n'.join(c['terrain']), encoding='utf-8')
        print(f'Previews in {args.preview}')
    if args.dry_run:
        print('Dry run: DEV not changed.')
        return 0
    folder = Path(__file__).resolve().parents[2] / 'artifacts/backups'
    folder.mkdir(parents=True, exist_ok=True)
    backup = folder / f'{project["id"]}_dev_r{revision}_before_farms_{time.strftime("%Y%m%d-%H%M%S")}.atlas.json'
    backup.write_text(json.dumps(project, ensure_ascii=False), encoding='utf-8')
    print(f'DEV revision {revision} backed up to {backup}.')
    with world_db.connect('dev', 'editor') as conn:
        new = world_store.save_world(conn, merged, revision)
    print(f'Saved to DEV, revision {new}.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
