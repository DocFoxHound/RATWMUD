#!/usr/bin/env python3
"""The item catalog (doc 35): items, recipes, stations and businesses in Data/Items.

  python3 tools/item_catalog.py              check the catalog; print a summary
  python3 tools/item_catalog.py --margins    also list every recipe's input cost, output value and margin
  python3 tools/item_catalog.py --item ID    show one item, how it is made and what it goes into

Exits non-zero when the catalog is unsound: duplicate ids, a recipe naming an unknown item or station, an item no
recipe, source or issue can produce, or a recipe that loses money at Common quality.

Python tools import this module (ITEMS, RECIPES, STATIONS, BUSINESSES) instead of reading the files themselves.
"""
from __future__ import annotations

import json
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIR = ROOT / 'Data/Items'
GROUPS = ('@fuel', '@dye', '@gem')
SLOTS = {'mouth', 'head', 'throat', 'body', 'harness', 'shoulders', 'sling', 'paws', 'jewelry', 'loop'}
# Where jewellery clips to the fur (doc 35, 1.3); 'any' is all of them.
SPOTS = {'ears', 'crown', 'ruff', 'chest', 'back', 'foreleg_left', 'foreleg_right', 'hindleg_left', 'hindleg_right', 'tail', 'any'}
TIERS = ('hamlet', 'town', 'city', 'industrial')


def _load(name, key):
    data = json.loads((DIR / name).read_text(encoding='utf-8'))
    assert data.get('format') == f'ratw-{key}' and data.get('version') == 1, f'{name}: not a version 1 ratw-{key} file'
    return data[key]


ITEMS = {i['id']: i for i in _load('items.json', 'items')}
RECIPES = _load('recipes.json', 'recipes')
STATIONS = {s['id']: s for s in _load('stations.json', 'stations')}
BUSINESSES = {b['id']: b for b in _load('businesses.json', 'businesses')}


def members(group):
    """The items a group token such as @fuel stands for."""
    tag = group[1:]
    return [i for i in ITEMS.values() if tag in i.get('tags', [])]


def price(ref):
    """An input's price; a group costs its cheapest member."""
    if ref in GROUPS:
        return min(i['price'] for i in members(ref))
    return ITEMS[ref]['price']


def margin(r):
    cost = sum(price(k) * n for k, n in r['in'].items())
    value = sum(price(k) * n for k, n in r['out'].items()) + sum(price(k) * n for k, n in r.get('by', {}).items())
    return cost, value, value - cost


def check():
    problems = []
    raw = json.loads((DIR / 'items.json').read_text(encoding='utf-8'))['items']
    if len(raw) != len(ITEMS):
        problems.append('duplicate item ids')
    ids = [r['id'] for r in RECIPES]
    for d in {x for x in ids if ids.count(x) > 1}:
        problems.append(f'recipe {d}: duplicate id')
    for g in GROUPS:
        if not members(g):
            problems.append(f'{g}: no items carry that tag')
    made = defaultdict(list)
    used = set()
    for r in RECIPES:
        if r['station'] not in STATIONS:
            problems.append(f'recipe {r["id"]}: unknown station {r["station"]}')
        if r.get('tool') and r['tool'] not in ITEMS:
            problems.append(f'recipe {r["id"]}: unknown tool {r["tool"]}')
        for part in ('in', 'out', 'by'):
            for k, n in r.get(part, {}).items():
                if k not in ITEMS and k not in GROUPS:
                    problems.append(f'recipe {r["id"]}: unknown item {k}')
                if n <= 0:
                    problems.append(f'recipe {r["id"]}: non-positive count for {k}')
        if any(k not in ITEMS and k not in GROUPS for p in ('in', 'out', 'by') for k in r.get(p, {})):
            continue
        for k in r['out']:
            made[k].append(r['id'])
        for k in list(r.get('by', {})):
            made[k].append(r['id'])
        used.update(r['in'])
        if r.get('tool'):
            used.add(r['tool'])
        cost, value, gain = margin(r)
        if gain < 0:
            problems.append(f'recipe {r["id"]}: loses {-gain:g}p (inputs {cost:g}p, outputs {value:g}p)')
    for i in ITEMS.values():
        if i.get('slot') and i['slot'] not in SLOTS:
            problems.append(f'{i["id"]}: unknown slot {i["slot"]}')
        if i.get('slot') == 'jewelry' and (not i.get('spots') or any(p not in SPOTS for p in i['spots'])):
            problems.append(f'{i["id"]}: jewellery needs spots from {sorted(SPOTS)}')
        if i['price'] < 0 or i['weight'] < 0:
            problems.append(f'{i["id"]}: negative price or weight')
        if not (made.get(i['id']) or i.get('source') or i.get('issued')):
            problems.append(f'{i["id"]}: nothing makes it and it has no source')
        if i['category'] in ('material', 'component') and i['id'] not in used and not any(
                i['id'] in b['sells'] for b in BUSINESSES.values()):
            # A material nobody uses or sells is a dead end in the economy.
            tagged = any(t in i.get('tags', []) for t in ('fuel', 'dye', 'gem'))
            if not tagged:
                problems.append(f'{i["id"]}: a material nothing uses or sells')
    for b in BUSINESSES.values():
        if b['tier'] not in TIERS:
            problems.append(f'business {b["id"]}: unknown tier {b["tier"]}')
        for s in b['stations']:
            if s not in STATIONS:
                problems.append(f'business {b["id"]}: unknown station {s}')
        for s in b['sells']:
            if s not in ITEMS and s not in {i['category'] for i in ITEMS.values()}:
                problems.append(f'business {b["id"]}: sells unknown {s}')
    for s in STATIONS.values():
        if len(s['glyph']) != 1 or s['glyph'] == 'W':
            problems.append(f'station {s["id"]}: glyph must be one character and never W')
    return problems, made


def show(item_id, made):
    i = ITEMS[item_id]
    print(json.dumps(i, ensure_ascii=False, indent=2))
    for r in RECIPES:
        if item_id in r['out'] or item_id in r.get('by', {}):
            cost, value, gain = margin(r)
            print(f'made by {r["id"]}: {r["trade"]} at {r["station"]} (skill {r["skill"]}, {r["seconds"]} s'
                  f'{", wait %g d" % r["wait"] if r.get("wait") else ""}) from {r["in"]}; margin {gain:+g}p')
    into = [r['id'] for r in RECIPES if item_id in r['in']]
    print('goes into:', ', '.join(into) or '-')


def main(argv):
    problems, made = check()
    if '--item' in argv:
        show(argv[argv.index('--item') + 1], made)
        return 0
    if '--margins' in argv:
        print(f'{"recipe":28} {"cost":>8} {"value":>8} {"margin":>8} {"p/min":>7}')
        for r in sorted(RECIPES, key=lambda r: r['id']):
            cost, value, gain = margin(r)
            print(f'{r["id"]:28} {cost:8.2f} {value:8.2f} {gain:+8.2f} {gain / (r["seconds"] / 60):7.2f}')
    cats = defaultdict(int)
    for i in ITEMS.values():
        cats[i['category']] += 1
    print(f'{len(ITEMS)} items ({", ".join(f"{k} {v}" for k, v in sorted(cats.items()))}); '
          f'{len(RECIPES)} recipes; {len(STATIONS)} stations; {len(BUSINESSES)} businesses')
    for p in problems:
        print('PROBLEM:', p)
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
