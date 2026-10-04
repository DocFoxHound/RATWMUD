"""Every settlement rebuilt from scratch (Docs/Design/39): shops with their keepers' flats above, the towns' basics,
the cities' squares of market stalls, and the ground blended at Upper Accord's northern edge.

  cd tools && python3 -m worldgen.rebuild [--dry-run] [--preview DIR]

In order, on the world as DEV holds it now (backed up first, under artifacts/backups):
  1. Upper Accord's city and campuses generated again (its ground comes out exactly as DEV has it: checked) and its
     residents with them;
  2. the western world generated again, which rebuilds Ridgemere, Ser Ferro, every town, the village, the fortresses
     and the Ghost Town, peoples them, and fills the three cities out (uax_, rmx_, sfx_);
  3. the ground of Upper Accord's three northern cells blended into the moors north of them (worldgen.blend).
Everyone the generators made is new; anyone else (an NPC a Dungeon Master made, say) is kept. A newcomer who would
take the ID of someone who lived here before is given another (the game keeps a resident's saved life, purse, bonds
and memories by ID, and a new person mustn't wake up in an old one's). Saved with DEV's revision checked, so an edit
made meanwhile stops it rather than being lost; then the old residents' saved state is cleared from the game's tables.
"""
from __future__ import annotations

import argparse
import json
import time
from pathlib import Path

from . import blend
from . import residents as R
from . import upper_accord as UA
from .western import GENERATED, Western, assemble, dev_project, ua_edges_from

UA_REGIONS = {'upper_accord', 'concord_hall', 'training_grounds', 'warden_order'}


def retire_ids(project, old_ids, keep_ids):
    """Gives every regenerated resident whose ID someone in the old world had a new one (a trailing underscore, as the
    generators make a taken ID unique). Returns {old: new}."""
    taken = set(old_ids) | {p['id'] for p in project['people']}
    renamed = {}
    for p in project['people']:
        if p['id'] in old_ids and p['id'] not in keep_ids:
            new = p['id']
            while new in taken:
                new += '_'
            taken.add(new)
            renamed[p['id']] = new
            p['id'] = new
    return renamed


def clear_old_state(conn, world_id, gone):
    """The saved game state of residents no longer in the world (`gone`: their IDs): their bodies, bonds, rumours they
    held or that were about them, NPC memories and their DM-set state. Players' and everyone else's stay. The game drops
    their lives and purses itself when it next loads its save. Returns rows deleted, by table."""
    gone = sorted(gone)
    out = {}
    for table, where in (('live.npc_state', 'npc_id = ANY(%s)'),
                         ('game.npcs', 'key = ANY(%s)'),
                         ('game.bonds', "(data->>'holder' = ANY(%s) OR data->>'other' = ANY(%s))"),
                         ('game.beliefs', "(data->>'holder' = ANY(%s) OR data->>'subject' = ANY(%s))"),
                         ('game.npc_memories', "data->>'npc' = ANY(%s)")):
        args = (world_id, gone) + ((gone,) if where.count('%s') == 2 else ())
        out[table] = conn.execute(f'DELETE FROM {table} WHERE world_id = %s AND {where}', args).rowcount
    return out


def rebuild(project, report=print):
    """The rebuilt world (a new project) from DEV's."""
    # Who stays: anyone not made by a generator (no generated prefix, and not one of Upper Accord's own).
    region_of = {a['id']: a['territory']['region'] for a in project['cells'] + project['rooms']}
    keep = [p for p in project.get('people', [])
            if not p['id'].startswith(GENERATED + ('uax_',)) and region_of.get(p['home']['cell']) not in UA_REGIONS]
    report(f'keeping {len(keep)} residents no generator made: {", ".join(p["name"] for p in keep) or "none"}')
    old_ids = {p['id'] for p in project.get('people', [])}
    # 1. Upper Accord: its city and campuses, and its people.
    _, site, _, region = UA.generated_project()
    merged = UA.merge(project, region, replace=True)
    P = R.populate(merged, site.manifest)
    merged = R.apply(merged, P, replace=True)
    report(f'upper accord: {len(region["rooms"])} interiors, {len(P.people)} residents')
    # 2. The western world, its cities and towns (and the cities filled out), on Upper Accord's edges as they now are.
    world = Western(ua_edges_from(merged))
    world.reserved_ids = {a['id'] for a in merged['cells'] + merged['rooms'] if not a['id'].startswith(GENERATED)}
    world.build()
    merged = assemble(merged, world, world.cells())
    merged['people'] = keep + [p for p in merged['people'] if p['id'] not in {k['id'] for k in keep}]
    # 3. Upper Accord's northern edge, blended into the moors.
    report(f'ground blended at the northern edge: {blend.blend_north(merged)} tiles')
    # 4. Nobody new takes an old resident's ID (and with it their saved life).
    renamed = retire_ids(merged, old_ids, {p['id'] for p in keep})
    report(f'{len(renamed)} newcomers given IDs no one here had before')
    merged['retired'] = sorted(old_ids - {p['id'] for p in merged['people']})
    return merged


def main(argv=None):
    import map_editor
    import world_db
    import world_store
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dry-run', action='store_true', help='Build and check, but do not save to DEV')
    parser.add_argument('--out', type=Path, help='Write the rebuilt project here as JSON')
    args = parser.parse_args(argv)
    project, revision = dev_project()
    merged = rebuild(project)
    gone = merged.pop('retired')
    roles = {}
    for p in merged['people']:
        roles[p['role']] = roles.get(p['role'], 0) + 1
    print(f'{len(merged["people"])} residents {roles}; {len(merged["rooms"])} interiors; {len(merged["links"])} links')
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
    backup = folder / f'{project["id"]}_dev_r{revision}_before_rebuild_{time.strftime("%Y%m%d-%H%M%S")}.atlas.json'
    backup.write_text(json.dumps(project, ensure_ascii=False), encoding='utf-8')
    print(f'DEV revision {revision} backed up to {backup}.')
    with world_db.connect('dev', 'editor') as conn:
        new = world_store.save_world(conn, merged, revision)
    print(f'Saved to DEV, revision {new}.')
    with world_db.connect('dev', 'owner') as conn:
        with conn.transaction():
            cleared = clear_old_state(conn, merged['id'], gone)
    print(f'The old residents\' saved state cleared: {cleared}.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
