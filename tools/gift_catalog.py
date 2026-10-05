#!/usr/bin/env python3
"""The Gift catalog (Docs/Design/43-gifts.md): checks Data/Gifts/families.json.

    python3 tools/gift_catalog.py        # prints each problem; exits 1 if there is any

Every family has both tiers, each with its best-for line, Tell, Cost and Limit (and a Quickened Overreach); every
ability a known kind, a mana cost and a one-line summary; ids are unique; the eight playable families are there and the
Death Walker is NPC-only. A Gifted ability never deals damage, so none is a shape or gathered spell.
"""
from __future__ import annotations

import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
PATH = ROOT / 'Data/Gifts/families.json'
KINDS = {'work', 'instant', 'gathered', 'channelled', 'reaction', 'fightlong', 'passive', 'shape', 'twoturn'}
PLAYABLE = ['fire', 'earth', 'water', 'wind', 'sound', 'blinker', 'gravity', 'seer']


def problems(doc: dict | None = None) -> list[str]:
    doc = doc if doc is not None else json.loads(PATH.read_text(encoding='utf-8'))
    found: list[str] = []
    tiers = doc.get('tiers', {})
    for tier in ('normal', 'gifted', 'quickened'):
        if not tiers.get(tier, {}).get('name') or not tiers.get(tier, {}).get('best'):
            found.append(f'tier {tier}: a name and a best-for line')
    families = doc.get('families', [])
    ids = [f.get('id') for f in families]
    if len(set(ids)) != len(ids):
        found.append('family ids repeat')
    playable = [f['id'] for f in families if not f.get('npcOnly')]
    if playable != PLAYABLE:
        found.append(f'the playable families are {PLAYABLE}, not {playable}')
    if not any(f.get('id') == 'death_walker' and f.get('npcOnly') for f in families):
        found.append('the Death Walker is in the catalog, NPC-only')
    abilities: set[str] = set()
    for f in families:
        fid = f.get('id', '?')
        for key in ('name', 'colour', 'domain'):
            if not f.get(key):
                found.append(f'{fid}: no {key}')
        for tier in ('gifted', 'quickened'):
            t = f.get(tier)
            if not isinstance(t, dict):
                found.append(f'{fid}: no {tier} tier')
                continue
            for key in ('best', 'tell', 'cost', 'limit') + (('overreach',) if tier == 'quickened' else ()):
                if not t.get(key):
                    found.append(f'{fid} {tier}: no {key}')
            listed = t.get('abilities', [])
            if not f.get('npcOnly') and not listed:
                found.append(f'{fid} {tier}: no abilities')
            if tier == 'quickened' and len(listed) > 5:
                found.append(f'{fid} quickened: at most five abilities')
            for a in listed:
                aid = a.get('id', '?')
                if aid in abilities:
                    found.append(f'{fid} {tier}: ability id {aid} repeats')
                abilities.add(aid)
                if a.get('kind') not in KINDS:
                    found.append(f'{fid} {tier} {aid}: unknown kind {a.get("kind")!r}')
                if not a.get('name') or not a.get('summary'):
                    found.append(f'{fid} {tier} {aid}: a name and a summary')
                for key in ('mana', 'perTurn', 'perTile'):
                    if key in a and (not isinstance(a[key], (int, float)) or isinstance(a[key], bool) or a[key] < 0):
                        found.append(f'{fid} {tier} {aid}: {key} must be a number, 0 or more')
                if not isinstance(a.get('mana'), (int, float)):
                    found.append(f'{fid} {tier} {aid}: no mana')
                if tier == 'gifted' and a.get('kind') in ('shape', 'gathered', 'twoturn'):
                    found.append(f'{fid} gifted {aid}: Gifted abilities deal no damage ({a.get("kind")} is a Quickened kind)')
    return found


if __name__ == '__main__':
    found = problems()
    for p in found:
        print(p)
    print('Gift catalog: ' + ('OK' if not found else f'{len(found)} problem(s)'))
    sys.exit(1 if found else 0)
