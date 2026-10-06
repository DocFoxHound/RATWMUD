"""Bunkhouses for the farms' hired hands, and Ser Ferro's plaster works (Docs/Design/42-money-in-circulation.md,
"Farmhands"; the user, 2026-10-06).

  cd tools && python3 -m worldgen.bunkhouses [--dry-run] [--out FILE]

Additive, on the world as DEV holds it now (backed up first, under artifacts/backups):
  1. Every farm, orchard, vineyard, pasture, dairy, piggery, sheep farm and rabbit farm without a bunkhouse gets one
     beside its building: "<site> Bunkhouse", ten beds and a hearth, walkable from the road. The game finds it by its
     name for the site's workers ("... at <site>"), lodges hands hired from elsewhere there, and the farm stocks its larder.
  2. Ser Ferro gets a plaster works outside its gates ("The Ser Ferro Plaster Works"): a keeper who runs it (the
     plasterworks business: plaster from the mines' quicklime and the farms' pelts and sheepskins) and two hands, from
     the city's labourers or those looking for work.
Saved with DEV's revision checked, so an edit made meanwhile stops it rather than being lost.
"""
from __future__ import annotations

import argparse
import json
import random
import sys
import time
from pathlib import Path

from . import farms as F
from .buildings import Building, tenement
from .cities import fp
from .industry import HOURS, KINDS, OX, OY, PREFIX, RETIRE, SETTLEMENTS, STYLE, Plot, place_lone, place_site, reword, \
    site_records

SEED = 6067
FARM_KINDS = {'farm', 'orchard', 'vineyard', 'pasture', 'dairy', 'piggery'}
BEDS = 10
PLASTER = dict(yard=None, building='plasterworks', hands=2, label='works at {building}', name='The {n} Plaster Works',
               keeper='runs {building}', building_name='The {n} Plaster Works', ground='open')
PLASTER_CELLS = ['w_ser_ferro_marches_1_8', 'w_ser_ferro_marches_1_9', 'w_ser_ferro_marches_0_8']


def build(project, report=print):
    out = {**project, 'cells': [dict(c) for c in project['cells']], 'rooms': [dict(r) for r in project['rooms']],
           'links': list(project['links']), 'people': [dict(p) for p in project.get('people', [])]}
    names = {r['name'] for r in out['rooms']}
    if 'The Ser Ferro Plaster Works' in names:
        raise ValueError('already built (the plaster works is there)')
    cells = {c['id']: c for c in out['cells'] if c.get('z', 0) == 0}
    reserved = {a['id'] for a in out['cells'] + out['rooms']}
    centre = {s[0]: (s[3][0] + s[3][2] // 2 + OX, s[3][1] + s[3][3] // 2 + OY) for s in SETTLEMENTS}
    centre['upper_accord'] = (384, 384)
    links = {l['b']['cell']: l for l in out['links'] if l['kind'] == 'door'}
    rooms_by_name = {r['name']: r['id'] for r in out['rooms']}
    # The farm sites: industry's (by kind) and the sheep and rabbit farms (by their barn or rabbitry).
    sites = []
    for (sid, kind, n), site in site_records(out).items():
        if kind in FARM_KINDS:
            sites.append((sid, n, site['rooms'][0]))
    for sid, (_, farm_names) in F.FARMS.items():
        for n in farm_names:
            for building in (f'{n} Barn', f'{n} Rabbitry'):
                if building in rooms_by_name:
                    sites.append((sid, n, rooms_by_name[building]))
    plots = {}
    rng = random.Random(f'{SEED}:bunks')
    built = skipped = 0
    for sid, n, room_id in sites:
        name = f'{n} Bunkhouse'
        if name in names:
            skipped += 1
            continue
        door = links.get(room_id)
        if not door or door['a']['cell'] not in cells:
            report(f'  {sid}: {n}: no door outside to build by')
            continue
        cid = door['a']['cell']
        if cid not in plots:
            plots[cid] = Plot(out, cells[cid], centre.get(sid, (0, 0)), reserved, sid)
        plot = plots[cid]
        plot.region = sid
        plot.toward = (door['a']['x'], door['a']['y'])
        style = STYLE.get(sid, 'city')
        p = tenement(rng, 'works' if style != 'serferro' else 'serferro', BEDS)
        b = Building('tenement', name, style, fp(p.w, p.h), [p.room('', name, 'Ten bunks along the walls, a stove and a '
                     'table, and pegs for the hired hands\' coats: where those taken on for the season sleep.')],
                     district=PREFIX)
        if place_lone(plot, b, rng):
            names.add(name)
            built += 1
        else:
            report(f'  {sid}: no room for a bunkhouse at {n}')
    report(f'{built} bunkhouses built ({skipped} sites had one)')
    # The plaster works outside Ser Ferro.
    site = None
    for cid in PLASTER_CELLS:
        if cid not in cells:
            continue
        if cid not in plots:
            plots[cid] = Plot(out, cells[cid], centre['ser_ferro'], reserved, 'ser_ferro')
        plot = plots[cid]
        plot.region = 'ser_ferro'
        site = place_site(plot, 'plasterworks', PLASTER, 'Ser Ferro', STYLE['ser_ferro'], rng)
        if site:
            break
    if not site:
        raise ValueError('no room for the plaster works outside Ser Ferro')
    report(f'  ser_ferro: {site["name"]} in {site["cell"]}')
    for plot in plots.values():
        if plot.changed:
            plot.write_back()
            out['rooms'] += [{**r, 'worldX': r['worldX'] + plot.cell['x'], 'worldY': r['worldY'] + plot.cell['y']}
                             for r in plot.site.rooms]
            out['links'] += plot.site.links
    # Its people: a keeper and two hands from Ser Ferro's labourers, or those looking for work.
    region = {a['id']: a['territory']['region'] for a in out['cells'] + out['rooms']}
    town = [p for p in out['people'] if region.get(p['home']['cell']) == 'ser_ferro']
    pool = sorted([p for p in town if p['role'] == 'civilian' and 16 <= p['age'] < RETIRE and
                   (p['workLabel'] in ('-', 'looking for work') or F.LABOURERS.search(p['workLabel']))], key=lambda p: p['id'])
    room = site['records'][0]['rooms'][0]
    spots = room['work'] or [{'x': 2, 'y': 2}]
    bname = site['records'][0]['name']
    jobs = [('merchant', PLASTER['keeper'].format(building=bname)[:40], spots[0], (6, 18))]
    jobs += [('civilian', PLASTER['label'].format(building=bname)[:40], spots[(i + 1) % len(spots)], HOURS) for i in range(2)]
    if len(pool) < len(jobs):
        raise ValueError(f'only {len(pool)} of Ser Ferro\'s labourers free for the plaster works')
    for (role, label, spot, hours), p in zip(jobs, pool):
        p['role'], p['workLabel'], p['work'] = role, label, {'cell': room['id'], 'x': spot['x'], 'y': spot['y']}
        p['hours'] = {'start': hours[0], 'end': hours[1]}
        p['paid'] = role != 'merchant'
        p['route'] = ''
        reword(p, label)
        report(f'  {p["name"]}: {label}')
    return out


def main(argv=None):
    import map_editor
    import world_db
    import world_store
    from .western import dev_project
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dry-run', action='store_true', help='Build and check, but do not save to DEV')
    parser.add_argument('--out', type=Path, help='Write the project here as JSON')
    args = parser.parse_args(argv)
    project, revision = dev_project()
    merged = build(project)
    try:
        map_editor.check_project(merged, for_game=False)
        print('Atlas validation: OK')
    except map_editor.ValidationError as error:
        print('Atlas validation failed:', *error.errors[:30], sep='\n  ')
        return 1
    if args.out:
        args.out.write_text(json.dumps(merged), encoding='utf-8')
    if args.dry_run:
        print('Dry run: DEV not changed.')
        return 0
    folder = Path(__file__).resolve().parents[2] / 'artifacts/backups'
    folder.mkdir(parents=True, exist_ok=True)
    backup = folder / f'{project["id"]}_dev_r{revision}_before_bunkhouses_{time.strftime("%Y%m%d-%H%M%S")}.atlas.json'
    backup.write_text(json.dumps(project, ensure_ascii=False), encoding='utf-8')
    print(f'DEV revision {revision} backed up to {backup}.')
    with world_db.connect('dev', 'editor') as conn:
        new = world_store.save_world(conn, merged, revision)
    print(f'Saved to DEV, revision {new}.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
