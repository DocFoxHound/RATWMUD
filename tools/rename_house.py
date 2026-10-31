#!/usr/bin/env python3
"""House Mossgrave becomes House Timberland in Ridgemere 2.0's text, and the four Houses' trades follow the spec
(Docs/Design/67, section 7: the renames).

  python3 tools/rename_house.py --dry-run [--world ridgemere2] [--report FILE]
                                            every row and field it would change, and nothing written
  python3 tools/rename_house.py --apply --world ridgemere2 [--editor NAME]
                                            the same changes in one transaction; open Atlas sessions reload

The Houses' trades now (doc 67):

    Timberland   the city's forges (and the timber and charcoal that feed them; the sawmill, Q4)
    Grayrock     the wharf, its ships and warehouses
    Fell         fine wines, liquors and fruit goods
    Greatly      construction: brick, tile, lime, plaster, paint and stone; they built the walls of Ridgemere

Only text changes, never an id: `house_mossgrave`, `rm_halvard_mossgrave`, `rm_mossgrave_court` and the rest keep their
ids (foreign keys don't cascade on update, and the sync base keys on them), so they still read "mossgrave" in Atlas
and the DM. One NPC move is part of the trades: "keeping the wharf's books" goes from Julian Greatly to Tobias
Grayrock, at the (now Grayrock) bonded warehouse, and Julian keeps the Greatly Yard's books instead. Matching is by
whole word and case-sensitive ("Fell" is never touched on its own: Fellside, Oakfell and a resident called Fell stay).

What it covers: the faction's name; NPCs' names, work labels, descriptions, greetings, personalities and backstories;
voices' personas, grammars and lines (never their quarter: that is the city's write); interiors' names and
descriptions (the Houses' seats, works and stores, the Greatly Yard re-described as the builders' yard, the
smithies named as Timberland's, the Southgate smiths become Eastgate's); link, route, NPC-area, profession-slot,
figure and dialect text; and the project's word rule (no "dog" or "dogs" in world text: wolves, pups).

Every change is idempotent: a second run changes nothing. Every work label stays within 40 characters (Atlas's
limit), or nothing is written. DEV and PROD, and every live world, are refused: they are other worlds, built by the
frozen generator, and renaming them is the user's call.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import world_db  # noqa: E402

# The old map's worlds keep the old name (doc 67, section 7): PROD, and the old DEV, now the archive dev-old (DEV is
# Ridgemere 2.0 since 2026-10-31: doc 66). Refused by database, whatever key names it.
REFUSED_KEYS = {'dev-old', 'prod'}
REFUSED_DATABASES = {'ratw_dev', 'ratw_prod'}
LABEL_MOST = 40   # tools/map_editor.py refuses longer work labels.

OLD, NEW = 'Mossgrave', 'Timberland'


def _w(phrase: str) -> str:
    """A phrase as a whole-word, case-sensitive pattern."""
    return r'(?<![\w])' + re.escape(phrase) + r'(?![\w])'


# --------------------------------------------------------------------------- The changes, as data

# Before the names: labels the new names would make too long (and that name a raw-resource producer, doc 67 section 4).
PRE = [
    (r'(?<![\w])burns charcoal at the Charcoal Store(?![\w])', 'working the Timberland Charcoal Store'),
]

# The Houses' businesses and the smiths, renamed everywhere their names appear (interiors and their rooms, links,
# labels, greetings, descriptions). (pattern, replacement), applied in order; a leading "The"/"the" keeps its case.
NAMES = [
    (r'\b([Tt])he Grayrock Ironworks\b', r'\1he Timberland Ironworks'),
    (r'\b([Tt])he Grayrock Foundry\b', r'\1he Timberland Foundry'),
    (r'\b([Tt])he Grayrock Iron Store\b', r'\1he Timberland Iron Store'),
    (r'\b([Tt])he Fell Sawmill\b', r'\1he Timberland Sawmill'),
    (r'\b([Tt])he Greatly Bonded Warehouse\b', r'\1he Grayrock Bonded Warehouse'),
    (r'\b([Tt])he Fell Timber Store\b', r'\1he Grayrock Long Store'),
    (r"\b([Tt])he Harbourmaster's Office\b", r'\1he Grayrock Wharf Office'),
    (r'\b([Tt])he Charcoal Store\b', r'\1he Timberland Charcoal Store'),
    (r'\b([Tt])he Lime Kilns\b', r'\1he Greatly Lime Kilns'),
    (r'\b([Tt])he Tileworks\b', r'\1he Greatly Tileworks'),
    (r'\b[Tt]he Anchor Forge\b', "Timberland's Anchor Forge"),
    (r"(?<!Timberland's )\bNailers' Forge\b", "Timberland's Nailers' Forge"),
    (r'\bSouthgate Ironmongers\b', 'Eastgate Ironmongers'),
    (r'\bSouthgate Farriers\b', 'Eastgate Farriers'),
]

# Phrases of the old trades, wherever they are (the generator's templated backstories, voices, descriptions).
PHRASES = [
    # Grayrock: stone and iron -> the wharf.
    (_w('lost a brother to the quarry and does not forgive House Grayrock for it'),
     'lost a brother on the wharf and does not forgive House Grayrock for it'),
    (_w('buying Grayrock iron'), 'buying Timberland iron'),
    (_w("Grayrock's forge-hall"), "Timberland's forge-hall"),
    (_w('Grayrock limestone'), 'Greatly limestone'),
    (_w('lime from Grayrock.'), 'lime from the Greatly kilns.'),
    # Greatly: ships -> construction (the ships are Grayrock's).
    (_w('Greatly ships'), 'Grayrock ships'),
    (_w('Greatly ship'), 'Grayrock ship'),
    (_w('Greatly berth'), 'Grayrock berth'),
    (_w('every Greatly cargo and loan'), 'every Greatly contract and loan'),
    (_w('the Greatly ships take the rest down the coast'), 'the Greatly carts take the rest out to every building site in the city'),
    (_w('kiln tile is cheap'), 'Greatly tile is cheap'),
    # Fell: timber -> wines, liquors and fruit goods (the timber is Timberland's).
    (_w('a cousin in the Fell logging camps'), 'a cousin in the Fell vineyards'),
    (_w('writes from the Fell camps'), 'writes from the Fell vineyards'),
    (_w('selling Fell cedar'), 'selling Fell wine'),
    (_w('Fell charcoal for the forges and the glassworks.'), 'Timberland charcoal for its forges and the glassworks.'),
    (_w('from Fell charcoal and wharf sand.'), "from Timberland charcoal and wharf sand, for House Greatly's builders."),
    # Timberland (was Mossgrave: wool) -> the forges.
    (_w(f'grew up on a {OLD} sheep run in the Folds and still misses the quiet'),
     f'grew up at a {NEW} charcoal camp in the hills and still misses the quiet'),
    (_w(f'trading {OLD} grey'), f'trading {NEW} iron'),
    (_w(f'This is {OLD} land.'), f'This is {NEW} ground.'),
    # The smiths are Timberland's.
    (_w('Eastgate Ironmongers, smithy.'), "Eastgate Ironmongers, smithy, one of Timberland's."),
    (_w('Eastgate Farriers, smithy.'), "Eastgate Farriers, smithy, one of Timberland's."),
    (_w('The Rivet and Rain, armorer.'), "The Rivet and Rain, armorer, Timberland's."),
    # The project's word rule: wolves, never dogs.
    (_w('the smell of wet dogs'), 'the smell of wet wolves'),
    (_w('a dog asleep in the door'), 'a pup asleep in the door'),
]

# Last: the House's own name, whole word ("Mossgrave", "Mossgrave's", "Mossgraves"; never an id like
# rm_halvard_mossgrave, where the name is part of a longer word).
WORDS = [
    (_w(OLD + 's'), NEW + 's'),
    (_w(OLD), NEW),
    (r'\bdogs\b', 'wolves'),     # Any the phrases above didn't name (reported).
    (r'\bdog\b', 'wolf'),
]

# Texts written whole: the Houses' seats and the buildings whose trade changes.
INTERIOR_DESCRIPTIONS = {
    'rm_mossgrave_court': (
        f"The great hall of {NEW} Court: the House of {NEW}'s high seat under its banners. The {NEW}s own the forges of "
        "Ridgemere, from the Ironworks and the Foundry in the Cinders to the smithies of the city, and keep their "
        "accounts in the Iron Office off this hall."),
    'rm_fellhall': (
        "The great hall of Fellhall: the House of Fell's high seat under its banners. Fell makes the finest wines, "
        "liquors and fruit goods in Ridgemere: its press house, distillery and preserving house stand behind the hall, "
        "and its cellars run under it."),
    'rm_grayrock_hall': (
        "The great hall of Grayrock Hall: the House of Grayrock's high seat under its banners. The oldest of the Houses: "
        "it owns the wharf of Ridgemere, its piers, its warehouses and the ships that use them, and nothing leaves the "
        "city by sea without a Grayrock tally."),
    'rm_greatly_house': (
        "The great hall of Greatly House: the House of Greatly's high seat under its banners. The richest of the Houses "
        "and the only one inside the walls: whatever is built in Ridgemere, a wall, a roof, a coat of plaster or paint, "
        "is built by Greatly hands from Greatly brick, lime and stone. Greatly masons raised the walls of the city, "
        "they say; Greatly money lends the city the rest."),
    'rm_the_greatly_yard': (
        "House Greatly's builders' yard: stacked stone, tile, lime and timber under lean-to roofs, scaffold poles in "
        "racks, and the carts that carry it all out to every wall and roof in Ridgemere."),
    'rm_the_fell_sawmill': (
        f"The {NEW} Sawmill: saw benches in rows between stacks of sawn cedar; the wheel outside drives them. House "
        f"{NEW}'s great saw, cutting the timber for the city's beams and the charcoal of its forges."),
    'rm_the_grayrock_iron_store': (
        f"{NEW} goods waiting for a ship: bar iron, nails and castings, stacked, tallied and guarded."),
    'rm_the_greatly_bonded_warehouse': (
        "Grayrock goods waiting for a ship, and goods off the ships waiting for the Customs House: stacked, tallied and "
        "guarded."),
    'rm_the_fell_timber_store': (
        "Grayrock's long store on the wharf: cargo waiting for a ship, stacked, tallied and guarded."),
    'rm_the_harbourmaster_s_office': (
        "Grayrock's office on the wharf: tide tables, berth rolls, the warehouses' ledgers and a window over the whole "
        "Sound."),
    'rm_greywool': 'Greywool, weaver. Heavy wool for heavy wet weather.',
}

# One NPC's own text: {id: {field: [(old, new), ...]}} (a phrase of the field, or the whole field for a label).
NPC_EDITS = {
    'rm_halvard_mossgrave': {
        'description': [('whose sheep runs and warrens fill the Folds south of the city', 'whose forges keep the city in iron')],
        'backstory': [('knows every shepherd in the Folds by name', 'knows every smith in the Cinders by name')],
    },
    'rm_wenna_mossgrave': {
        'description': [("who runs the House's looms", "who runs the House's forges")],
        'backstory': [('to sell Ridgemere grey in every port', f'to sell {NEW} iron in every port')],
        'work_label': [('running the wool shed', "running the House's iron trade")],
    },
    'rm_osric_mossgrave': {'work_label': [('riding out to the Folds', 'riding out on House business')]},
    'rm_magda_calloway': {'description': [('a wool merchant.', 'an iron merchant.')]},
    'rm_wenna_dorrington': {'description': [('a wool merchant.', 'an iron merchant.')]},
    'rm_aldric_grayrock_': {
        'description': [('always on the road: to the mines above Frosthollow, to the quarries, to anyone who will buy iron',
                         'always at the wharf or off down the coast: on the piers, in the warehouses, aboard anything with a hold')],
    },
    'rm_tobias_grayrock_': {
        'description': [('the Grayrock heir, soot to the elbows', 'the Grayrock heir, tar to the elbows')],
        'work_label': [('overseeing the ironworks', "keeping the wharf's books")],
    },
    'rm_julian_greatly': {
        'description': [("who runs the House's yards and the ships that carry its brick and lime",
                         "who runs the House's yards and the crews that raise its walls")],
        'work_label': [("keeping the wharf's books", "keeping the Greatly Yard's books")],
    },
    'rm_corwin_fell_': {
        'description': [('loud and broad as a cedar', 'loud and broad as a wine tun')],
        'work_label': [('driving the logging crews', 'keeping the Fell cellars')],
    },
    'rm_isolde_fell_': {'backstory': [('since her husband drowned under a log raft', 'since her husband drowned bringing a wine barge down the coast')]},
}

# Family of a House working another House's business now: (the work place it leaves, the one it takes, the spot, or
# None for the first free floor tile). "Keeping the wharf's books" goes from Greatly to Grayrock (Tobias Grayrock leaves
# the Timberland Ironworks for Julian's desk at the Grayrock Bonded Warehouse; Julian goes to the Greatly Yard), and the
# Fell heir leaves the Timberland Sawmill for his own House's hall.
WORK_MOVES = {
    'rm_tobias_grayrock_': ('rm_the_grayrock_ironworks', 'rm_the_greatly_bonded_warehouse', (8, 8)),
    'rm_julian_greatly': ('rm_the_greatly_bonded_warehouse', 'rm_the_greatly_yard', (3, 2)),
    'rm_corwin_fell_': ('rm_the_fell_sawmill', 'rm_fellhall', None),
}

# The free-text columns, table by table (all keyed by world_id and the columns named in KEYS).
TEXT = {
    'live.factions': ['name', 'description'],
    'live.npcs': ['name', 'work_label', 'description', 'greeting', 'personality', 'backstory'],
    'live.npc_voice': ['persona', 'grammar', 'rules'],
    'world.interiors': ['name', 'description'],
    'world.links': ['name'],
    'live.patrol_routes': ['name'],
    'live.npc_areas': ['name'],
    'live.profession_slots': ['name', 'work_label'],
    'live.figures': ['name', 'title', 'about'],
    'live.dialects': ['grammar'],
}
KEYS = {'live.npc_voice': 'npc_id', 'live.dialects': 'name'}
LABELS = {('live.npcs', 'work_label'), ('live.profession_slots', 'work_label')}
FACTION_NAMES = {'house_mossgrave': f'House {NEW}'}

# Kept on purpose (the report says so): unplaced stock that names an old trade and isn't built in Ridgemere 2.0.
LEFT = [
    'rm_the_quarry_office (Grayrock\'s quarries), rm_the_fell_timber_office (Fell\'s cedar), the logging camps '
    '(fellside_*, oakfell_*): unplaced stock of the old map, not built in Ridgemere 2.0 (doc 67, section 3, "Not built"); '
    'their text is kept.',
    f'rm_the_mossgrave_wool_shed, _wool_store, _fleece_barn, _cottages, _gatehouse, _stables, _summerhouse: renamed '
    f'"{NEW}" by the whole-word rule; the wool shed\'s text still speaks of wool (unplaced stock, doc 67 section 7).',
    'The seat\'s "Forge Office": no such room or text exists in this world; the seat\'s description names the Iron '
    'Office, and the city build (lane A) makes the room.',
    f'live.npc_voice for the four {NEW}s (rm_halvard_, rm_signe_, rm_wenna_, rm_osric_mossgrave): no text names the '
    'House (persona, grammar and lines are empty); unchanged. Their quarter is the city write\'s.',
]


# --------------------------------------------------------------------------- Rewriting

COMPILED = [(re.compile(p), r) for p, r in PRE + NAMES + PHRASES + WORDS]


def rewrite(text):
    """The text with every rule applied, in order. None stays None."""
    if not text:
        return text
    for pattern, replacement in COMPILED:
        text = pattern.sub(replacement, text)
    return text


def stable(text) -> bool:
    return rewrite(text) == text


class Refused(Exception):
    pass


def refuse_world(key: str) -> dict:
    entry = world_db.world(key)
    if entry['key'] in REFUSED_KEYS or entry['database'] in REFUSED_DATABASES:
        raise Refused(f'{key} ({entry["database"]}) is refused: the old map\'s worlds (PROD and the archive dev-old) keep '
                      f'the name {OLD} (doc 67, section 7).')
    if entry['kind'] != 'dev':
        raise Refused(f'{key} is a {entry["kind"]} world; this renames a dev world only.')
    return entry


def columns(conn, table):
    schema, name = table.split('.')
    return {r[0] for r in conn.execute('SELECT column_name FROM information_schema.columns WHERE table_schema = %s '
                                       'AND table_name = %s', (schema, name))}


def plan(conn) -> dict:
    """{'world', 'revision', 'changes': [(table, key, field, old, new)], 'moves': [...], 'notes': [...]}."""
    world_id, revision = conn.execute('SELECT id, revision FROM world.worlds').fetchone()
    changes, notes, moves = [], [], []

    def change(table, key, field, old, new):
        if new != old:
            if not stable(new):
                raise Refused(f'{table} {key} {field}: the rewrite is not stable ({new!r}); fix the rules.')
            changes.append((table, key, field, old, new))

    for table, fields in TEXT.items():
        have = columns(conn, table)
        if not have:
            notes.append(f'{table}: not in this world; skipped.')
            continue
        key = KEYS.get(table, 'id')
        fields = [f for f in fields if f in have]
        rows = conn.execute(f'SELECT {key}, {", ".join(fields)} FROM {table} WHERE world_id = %s ORDER BY {key}',
                            (world_id,)).fetchall()
        for row in rows:
            ident, values = row[0], dict(zip(fields, row[1:]))
            for field, old in values.items():
                new = old
                if table == 'live.factions' and field == 'name' and ident in FACTION_NAMES:
                    new = FACTION_NAMES[ident]
                elif table == 'world.interiors' and field == 'description' and ident in INTERIOR_DESCRIPTIONS:
                    new = INTERIOR_DESCRIPTIONS[ident]
                elif table == 'live.npcs' and ident in NPC_EDITS and field in NPC_EDITS[ident]:
                    for before, after in NPC_EDITS[ident][field]:
                        if new and before in new:
                            new = new.replace(before, after)
                        elif not (new and after in rewrite(new)):
                            notes.append(f'{ident} {field}: "{before}" not found (changed since?); left as it is.')
                    new = rewrite(new)
                else:
                    new = rewrite(old)
                if (table, field) in LABELS and new and len(new) > LABEL_MOST:
                    raise Refused(f'{table} {ident}: the work label "{new}" is {len(new)} characters (at most {LABEL_MOST}).')
                change(table, ident, field, old, new)

    spots = {}
    for npc, (leaves, area, spot) in WORK_MOVES.items():
        row = conn.execute('SELECT work_area, work_x, work_y FROM live.npcs WHERE world_id = %s AND id = %s',
                           (world_id, npc)).fetchone()
        if not row:
            notes.append(f'{npc}: not in this world; its move is skipped.')
            continue
        if row[0] == area:
            continue                                    # (Moved already.)
        if row[0] != leaves:
            notes.append(f'{npc}: works at {row[0]}, not {leaves} (moved since?); its move is skipped.')
            continue
        interior = conn.execute('SELECT width, glyphs FROM world.interiors WHERE world_id = %s AND id = %s',
                                (world_id, area)).fetchone()
        if not interior:
            notes.append(f'{npc}: {area} is not in this world; its move is skipped.')
            continue
        width, glyphs = interior
        if area not in spots:                           # The work spots there of those who stay.
            spots[area] = {(x, y) for x, y in conn.execute(
                'SELECT work_x, work_y FROM live.npcs WHERE world_id = %s AND work_area = %s AND NOT (id = ANY(%s))',
                (world_id, area, list(WORK_MOVES)))}

        def free(x, y):
            return (0 <= x < width and 0 <= y * width + x < len(glyphs) and glyphs[y * width + x] in FLOORS
                    and (x, y) not in spots[area])
        if spot is None:
            spot = next(((i % width, i // width) for i in range(len(glyphs)) if free(i % width, i // width)), None)
        if spot is None or not free(*spot):
            raise Refused(f'{npc}: no free work spot at {area} {spot}; choose another.')
        spots[area].add(spot)
        moves.append((npc, tuple(row), (area, *spot)))
    return {'world': world_id, 'revision': revision, 'changes': changes, 'moves': moves, 'notes': notes}


def _floors():
    try:
        import terrain_catalog
        return frozenset(c for c, t in terrain_catalog.BY_CODE.items()
                         if not t['solid'] and t['kind'] in ('floor', 'grass') and c != '+')   # (Not a doorway.)
    except Exception:            # (The catalogue is in the tree; this is only for a stripped copy of the tool.)
        return frozenset('.f>,_')


FLOORS = _floors()


def apply(conn, p: dict, editor: str) -> int:
    """Writes the plan in the caller's transaction; the number of fields and moves written."""
    world_id = p['world']
    for table, ident, field, old, new in p['changes']:
        key = KEYS.get(table, 'id')
        done = conn.execute(f'UPDATE {table} SET {field} = %s WHERE world_id = %s AND {key} = %s AND {field} IS NOT DISTINCT FROM %s',
                            (new, world_id, ident, old))
        if done.rowcount != 1:
            raise Refused(f'{table} {ident} {field} changed while this ran; nothing written.')
    for npc, before, (area, x, y) in p['moves']:
        done = conn.execute('UPDATE live.npcs SET work_area = %s, work_x = %s, work_y = %s WHERE world_id = %s AND id = %s '
                            'AND work_area = %s AND work_x = %s AND work_y = %s', (area, x, y, world_id, npc, *before))
        if done.rowcount != 1:
            raise Refused(f'{npc} moved while this ran; nothing written.')
    count = len(p['changes']) + len(p['moves'])
    if count:
        import world_sync
        world_sync.tell_editors(conn, world_id, editor, f'House {NEW} renamed, and the Houses\' trades (doc 67)')
    return count


# --------------------------------------------------------------------------- The report

def _sentences(text):
    return re.split(r'(?<=[.!?])\s+|\n', text or '')


def report(key: str, p: dict) -> str:
    out = [f'rename_house.py on {key} (world {p["world"]}, revision {p["revision"]}): doc 67, section 7.',
           'House Mossgrave becomes House Timberland in this world\'s text; the Houses\' trades follow the spec.',
           'Ids are kept (house_mossgrave, rm_*mossgrave*): they still read "mossgrave" in Atlas and the DM.', '']
    by_table = {}
    for c in p['changes']:
        by_table.setdefault(c[0], []).append(c)
    total = len(p['changes']) + len(p['moves'])
    out.append(f'{total} changes: ' + ', '.join(f'{t} {len(c)}' for t, c in by_table.items()) +
               (f', work places {len(p["moves"])}' if p['moves'] else '') + '.')
    fields = {}
    for table, _, field, _, _ in p['changes']:
        fields[(table, field)] = fields.get((table, field), 0) + 1
    for (table, field), n in sorted(fields.items()):
        out.append(f'  {table}.{field}: {n}')
    for table, changes in by_table.items():
        out += ['', f'== {table}']
        for _, ident, field, old, new in changes:
            if len(old or '') <= 160 and '\n' not in (old or ''):
                out.append(f'{ident} {field}: "{old}" -> "{new}"')
            else:
                olds, news = _sentences(old), _sentences(new)
                gone = [s for s in olds if s not in news]
                came = [s for s in news if s not in olds]
                out.append(f'{ident} {field}: ' + ' / '.join(f'"{s}"' for s in gone) + ' -> ' +
                           ' / '.join(f'"{s}"' for s in came))
    if p['moves']:
        out += ['', '== live.npcs work places']
        for npc, before, after in p['moves']:
            out.append(f'{npc}: {before[0]} ({before[1]}, {before[2]}) -> {after[0]} ({after[1]}, {after[2]})')
    out += ['', '== Left as they are'] + LEFT
    if p['notes']:
        out += ['', '== Notes'] + p['notes']
    return '\n'.join(out) + '\n'


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--dry-run', action='store_true', help='list every change; write nothing')
    mode.add_argument('--apply', action='store_true', help='make the changes, in one transaction')
    parser.add_argument('--world', default='dev', help='a dev world key (default dev, Ridgemere 2.0; never dev-old or prod)')
    parser.add_argument('--report', help='also write the list to this file')
    parser.add_argument('--editor', default='rename_house', help='who the edit log names')
    args = parser.parse_args(argv)
    try:
        refuse_world(args.world)
        role = 'editor'
        with world_db.connect(args.world, role) as conn:
            if args.dry_run:
                with conn.transaction():
                    conn.execute('SET TRANSACTION READ ONLY')
                    p = plan(conn)
                text = report(args.world, p)
            else:
                with conn.transaction():
                    conn.execute('SELECT 1 FROM world.worlds FOR UPDATE')   # Atlas's edits take turns on this row.
                    p = plan(conn)
                    n = apply(conn, p, args.editor)
                text = report(args.world, p) + (f'\nWritten: {n} changes; the world\'s revision raised and open Atlas '
                                                f'sessions told to reload.\n' if n else '\nNothing to change.\n')
    except (Refused, world_db.DatabaseError) as error:
        print(f'rename_house: {error}', file=sys.stderr)
        return 1
    if args.report:
        Path(args.report).parent.mkdir(parents=True, exist_ok=True)
        Path(args.report).write_text(text)
    sys.stdout.write(text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
