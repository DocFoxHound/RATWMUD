#!/usr/bin/env python3
"""Seeds the talk library's live tables in DEV (Docs/Design/62-talk.md, phase 6): written once, then edited in Atlas
and by the Dungeon Master. Rows already there are kept (an edit is never overwritten) unless --replace names them.

  live.dialects       the regional grammars, from Data/Talk/grammar/*.grammar (plain stays a file)
  live.npc_voice      each resident's origin and accent (as the game works them out: talk_check --residents --json),
                      Ridgemere quarter (worldgen's artifacts/realm/buildings.json), traits (from the personality's two
                      adjectives: TRAITS below, decision 4) and quirk (decision 5)
  live.npc_relations  families, by rule from each household (decision 8): the eldest man and woman of age within
                      fifteen years of each other are spouses; those at least sixteen years younger than both are their
                      children; one at least eighteen years older than the husband (the household's surname is his) is
                      his parent. With no couple, the eldest is the parent of those at least sixteen years younger.
                      Communal homes are left out.

    python3 tools/talk_seed.py [--dry-run] [--replace dialects,voices,relations]
"""
from __future__ import annotations

import argparse
import collections
import json
import re
import subprocess
import sys
from pathlib import Path

import world_db

ROOT = Path(__file__).resolve().parent.parent
GRAMMARS = ROOT / 'Data' / 'Talk' / 'grammar'
BUILDINGS = ROOT / 'artifacts' / 'realm' / 'buildings.json'

# Decision 4: the personality's adjectives as traits (doc 62, "Character, as settled": a draft for the user to correct).
# Axes not named keep their middle value (temper even, talk plain, warmth reserved, humour none, honesty plain, piety
# ordinary, drive steady); where two adjectives set the same axis, the first wins.
TRAITS = {
    'playful': {'humour': 'merry', 'talk': 'chatty'}, 'suspicious': {'honesty': 'guarded', 'warmth': 'cold'},
    'tired': {'talk': 'quiet'}, 'devout': {'piety': 'devout'}, 'dreamy': {'talk': 'quiet', 'drive': 'content'},
    'wry': {'humour': 'dry'}, 'loyal': {'warmth': 'warm'}, 'anxious': {'temper': 'nervy'},
    'unhurried': {'temper': 'calm', 'drive': 'content'}, 'ambitious': {'drive': 'ambitious'},
    'blunt': {'honesty': 'frank'}, 'guarded': {'honesty': 'guarded', 'talk': 'quiet'}, 'loud': {'talk': 'chatty'},
    'proud': {'temper': 'short', 'warmth': 'reserved'}, 'patient': {'temper': 'calm'}, 'easily offended': {'temper': 'short'},
    'curious': {'talk': 'chatty'}, 'shrewd': {'honesty': 'sly'}, 'warm': {'warmth': 'warm'}, 'generous': {'warmth': 'warm'},
    'restless': {'drive': 'restless'}, 'quietly funny': {'humour': 'dry', 'talk': 'quiet'},
    'meticulous': {'temper': 'even', 'talk': 'plain'}, 'honest to a fault': {'honesty': 'frank'},
    'gentle': {'temper': 'calm', 'warmth': 'warm'}, 'stubborn': {'temper': 'short'},
}
PERSONALITY = re.compile(r'^([A-Za-z -]+?) and ([A-Za-z -]+?); (.*)$')
COMMUNAL = ('barracks', 'bunkhouse', 'quarters', 'watch_house', 'lodging')   # (Society::communalHome)


def traits_of(personality: str) -> tuple[dict, str]:
    """(traits, quirk) from a personality in worldgen's pattern; ({}, '') for one outside it (the model reads those)."""
    m = PERSONALITY.match(personality.strip())
    if not m:
        return {}, ''
    traits = {}
    for adjective in (m.group(1).lower(), m.group(2).lower()):
        for axis, value in TRAITS.get(adjective, {}).items():
            traits.setdefault(axis, value)
    quirk = re.sub(r'^(he|she|they) ', '', m.group(3).strip().rstrip('.').lower())
    slug = re.sub(r'[^a-z]+', '_', quirk).strip('_')
    while len(slug) > 60:
        slug = slug.rsplit('_', 1)[0]
    return traits, slug


def quarter_of(district: str) -> str:
    """Worldgen's district as a Ridgemere quarter: heights, commons, sump, office, works, wharf, docks, harbour_hill,
    suburbs, estate."""
    if district.startswith('estate_'):
        return 'estate'
    if district.startswith('suburb'):
        return 'suburbs'
    for q in ('heights', 'commons', 'sump', 'office', 'works', 'wharf', 'docks', 'harbour_hill'):
        if district == q or district.startswith(q + '_'):
            return q
    return ''


def quarters() -> dict[str, str]:
    """Room (a home's cell) -> its quarter."""
    if not BUILDINGS.exists():
        return {}
    out = {}
    for b in json.loads(BUILDINGS.read_text()):
        q = quarter_of(b.get('district') or '')
        for room in b.get('rooms', []):
            if q:
                out[room['id']] = q
    return out


def origins() -> dict[str, dict]:
    """Every resident's facts as the game works them out (origin and accent: World::talkOrigin)."""
    candidates = sorted(ROOT.glob('build*/talk_check'), key=lambda p: p.stat().st_mtime, reverse=True)
    if not candidates:
        sys.exit('No talk_check built: cmake --build <a build dir> --target talk_check')
    run = subprocess.run([str(candidates[0]), '--world', 'dev', '--residents', '--json'], capture_output=True, text=True,
                         timeout=600, check=True)
    out = {}
    for line in run.stdout.splitlines():
        if line.startswith('{'):
            facts = json.loads(line)
            out[facts['id']] = facts
    return out


def families(npcs: list[dict]) -> list[tuple[str, str, str]]:
    """(npc, other, kind) by the rule in this file's docstring."""
    homes = collections.defaultdict(list)
    for n in npcs:
        if n['home_area'] and not any(c in n['home_area'] for c in COMMUNAL):
            homes[(n['home_area'], n['name'].split()[-1])].append(n)
    out = []
    for members in homes.values():
        adults = sorted((n for n in members if n['age'] >= 18), key=lambda n: -n['age'])
        husband = wife = None
        for man in (n for n in adults if n['sex'] == 'male'):
            match = next((w for w in adults if w['sex'] == 'female' and abs(w['age'] - man['age']) <= 15), None)
            if match:
                husband, wife = man, match
                break
        if not husband:
            # One parent: the eldest, of those at least sixteen years younger.
            if adults:
                eldest = adults[0]
                out += [(eldest['id'], n['id'], 'parent') for n in members if n['age'] <= eldest['age'] - 16]
            continue
        out.append((husband['id'], wife['id'], 'spouse'))
        for n in members:
            if n is husband or n is wife:
                continue
            if n['age'] <= min(husband['age'], wife['age']) - 16:
                out += [(husband['id'], n['id'], 'parent'), (wife['id'], n['id'], 'parent')]
            elif n['age'] >= husband['age'] + 18:
                out.append((n['id'], husband['id'], 'parent'))
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--dry-run', action='store_true', help='count what would be written, write nothing')
    parser.add_argument('--replace', default='', help='comma-separated: dialects, voices, relations (overwrite those)')
    args = parser.parse_args()
    replace = {r for r in args.replace.split(',') if r}
    with world_db.connect('dev', 'editor') as conn:
        worlds = [r[0] for r in conn.execute('SELECT id FROM world.worlds').fetchall()]
        if len(worlds) != 1:
            sys.exit(f'Expected one world in DEV, found {worlds}')
        world = worlds[0]
        npcs = [{'id': r[0], 'name': r[1], 'age': r[2], 'sex': (r[3] or {}).get('sex', ''), 'home_area': r[4] or '',
                 'personality': r[5] or ''}
                for r in conn.execute('SELECT id, name, age, appearance, home_area, personality FROM live.npcs '
                                      'WHERE world_id = %s AND origin = %s', (world, 'authored')).fetchall()]
        # The dialects.
        dialects = [(f.stem, i, f.read_text()) for i, f in enumerate(sorted(GRAMMARS.glob('*.grammar'))) if f.stem != 'plain']
        # Each resident's voice.
        facts = origins()
        where = quarters()
        voices, outside = [], 0
        for n in npcs:
            traits, quirk = traits_of(n['personality'])
            outside += not traits
            f = facts.get(n['id'], {})
            voices.append((world, n['id'], f.get('from', ''), f.get('accent', ''), where.get(n['home_area'], ''),
                           json.dumps(traits), quirk))
        relations = families(npcs)
        print(f'{world}: {len(dialects)} dialects; {len(voices)} voices ({outside} personalities outside the pattern, for '
              f'the model; {sum(1 for v in voices if v[4])} with a Ridgemere quarter); {len(relations)} family relations '
              f'({sum(1 for r in relations if r[2] == "spouse")} couples).')
        if args.dry_run:
            return 0
        with conn.transaction():
            if 'dialects' in replace:
                conn.execute('DELETE FROM live.dialects WHERE world_id = %s', (world,))
            for name, position, text in dialects:
                conn.execute('INSERT INTO live.dialects (world_id, name, position, grammar) VALUES (%s, %s, %s, %s) '
                             'ON CONFLICT DO NOTHING', (world, name, position, text))
            if 'voices' in replace:
                conn.execute("DELETE FROM live.npc_voice WHERE world_id = %s", (world,))
            for v in voices:
                conn.execute('INSERT INTO live.npc_voice (world_id, npc_id, home_region, accent, quarter, traits, quirk, '
                             "written_by) VALUES (%s, %s, %s, %s, %s, %s::jsonb, %s, 'seed') ON CONFLICT DO NOTHING", v)
            if 'relations' in replace:
                conn.execute('DELETE FROM live.npc_relations WHERE world_id = %s', (world,))
            for npc, other, kind in relations:
                conn.execute('INSERT INTO live.npc_relations (world_id, npc_id, other_id, kind) VALUES (%s, %s, %s, %s) '
                             'ON CONFLICT DO NOTHING', (world, npc, other, kind))
        print('Written (rows already there kept).')
    return 0


if __name__ == '__main__':
    sys.exit(main())
