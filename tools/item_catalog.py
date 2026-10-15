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
_CRAFTS_FILE = json.loads((DIR / 'crafts.json').read_text(encoding='utf-8'))
assert _CRAFTS_FILE.get('format') == 'ratw-crafts' and _CRAFTS_FILE.get('version') == 1, 'crafts.json: not a version 1 ratw-crafts file'
CRAFTS = _CRAFTS_FILE['crafts']           # What shopkeepers make now (doc 35, Phase 5, first part).
SUPPLIES = _CRAFTS_FILE['supplies']       # Business -> the ingredients its shops sell.
PRODUCERS = _CRAFTS_FILE.get('producers', [])   # Residents who bring goods in from the land.
BUYS = {b: v for b, v in _CRAFTS_FILE.get('buys', {}).items() if b != 'about'}   # What shops buy from players.
# The worth ladder (Docs/Design/64-economy-migration.md, Phase 2): its rungs, in a labourer's day-wages (LD).
LADDER = json.loads((ROOT / 'Data/Economy/ladder.json').read_text(encoding='utf-8'))
RUNGS = {r['id']: r for r in LADDER['rungs']}


def worth(ref):
    """What an item is worth in a labourer's day-wages (its rung and where in it); a group, its cheapest member."""
    if ref in GROUPS:
        return min(worth(i['id']) for i in members(ref))
    w = ITEMS[ref].get('worth')
    if not w or w.get('rung') not in RUNGS:
        return ITEMS[ref]['price'] / LADDER['catalogueDay']
    r = RUNGS[w['rung']]
    return r['from'] * (r['to'] / r['from']) ** w.get('at', 0.5)


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
    """A recipe's inputs and goods by worth (LD): what's left between them is the work's (doc 64, Phase 2; it was by
    catalogue price)."""
    cost = sum(worth(k) * n for k, n in r['in'].items())
    value = sum(worth(k) * n for k, n in r['out'].items()) + sum(worth(k) * n for k, n in r.get('by', {}).items())
    return cost, value, value - cost


NOTES = []                                  # What the checks noticed that isn't unsound.


def check():
    problems = []
    NOTES.clear()
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
    losing = []
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
            losing.append((r, cost, value, gain))
    # What the residents make and bring in counts as made too (crafts.json: the starter crafts and the producers).
    for c in CRAFTS:
        for k in c['out']:
            made[k].append(f'craft {c["id"]}')
        used.update(c['in'])
    for pr in PRODUCERS:
        for k in list(pr.get('out', {})) + list(pr.get('offSeason', {})):
            made[k].append(f'producer {pr["id"]}')
    # A recipe whose goods are worth less than its inputs: unsound if it's the only way to make them; a note if another
    # way makes them cheaper (worth is set by the way the residents make them: a dearer route is just a poorer one).
    for r, cost, value, gain in losing:
        other = any(m != r['id'] for k in r['out'] for m in made.get(k, []))
        line = f'recipe {r["id"]}: loses {-gain:.3g} LD (inputs {cost:.3g} LD, goods {value:.3g} LD)'
        (NOTES if other else problems).append(line + (' (a dearer route: its goods are made cheaper another way)' if other else ''))
    for i in ITEMS.values():
        if i.get('slot') and i['slot'] not in SLOTS:
            problems.append(f'{i["id"]}: unknown slot {i["slot"]}')
        if i.get('slot') == 'jewelry' and (not i.get('spots') or any(p not in SPOTS for p in i['spots'])):
            problems.append(f'{i["id"]}: jewellery needs spots from {sorted(SPOTS)}')
        if i['price'] < 0 or i['weight'] < 0:
            problems.append(f'{i["id"]}: negative price or weight')
        w = i.get('worth')
        if not w or w.get('rung') not in RUNGS or not 0 <= w.get('at', -1) <= 1:
            problems.append(f'{i["id"]}: worth must name a rung ({", ".join(RUNGS)}) and an at from 0 to 1')
        if not isinstance(i.get('playerSeconds'), int) or i['playerSeconds'] < 0:
            problems.append(f'{i["id"]}: playerSeconds must be a whole number of seconds, 0 or more')
        if not (made.get(i['id']) or i.get('source') or i.get('issued')):
            problems.append(f'{i["id"]}: nothing makes it and it has no source')
        if i['category'] in ('material', 'component') and i['id'] not in used and not any(
                i['id'] in b['sells'] for b in BUSINESSES.values()) and not any(i['id'] in v for v in BUYS.values()):
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
    problems += check_crafts()
    problems += check_wild()
    for s in STATIONS.values():
        if len(s['glyph']) != 1 or s['glyph'] == 'W':
            problems.append(f'station {s["id"]}: glyph must be one character and never W')
    return problems, made


def check_crafts():
    """The starter crafts: whole batches of at most two ingredients, real makers, never at a loss."""
    problems = []
    ids = [c['id'] for c in CRAFTS]
    for d in {x for x in ids if ids.count(x) > 1}:
        problems.append(f'craft {d}: duplicate id')
    for c in CRAFTS:
        if not 1 <= len(c['in']) <= 2:
            problems.append(f'craft {c["id"]}: needs one or two ingredients')
        for part in ('in', 'out'):
            for k, n in c[part].items():
                if k not in ITEMS:
                    problems.append(f'craft {c["id"]}: unknown item {k}')
                if not isinstance(n, int) or n < 1:
                    problems.append(f'craft {c["id"]}: {k} must be a whole count')
        for m in c['makers']:
            if m not in BUSINESSES:
                problems.append(f'craft {c["id"]}: unknown maker {m}')
        if not c['makers'] or c['seconds'] <= 0:
            problems.append(f'craft {c["id"]}: needs makers and seconds')
        if all(k in ITEMS for p in ('in', 'out') for k in c[p]):
            cost, value, gain = margin(c)
            if gain <= 0:
                problems.append(f'craft {c["id"]}: makes nothing ({cost:.3g} LD in, {value:.3g} LD out)')
        if not isinstance(c.get('hours'), (int, float)) or c['hours'] <= 0:
            problems.append(f'craft {c["id"]}: needs its hours (tools/econ_model/apply_ladder.py)')
    for pr in PRODUCERS:
        if not pr.get('match') or pr.get('seconds', 0) <= 0 or not pr.get('out'):
            problems.append(f'producer {pr.get("id")}: needs match words, seconds and goods')
        for k, n in pr.get('out', {}).items():
            if k not in ITEMS or not isinstance(n, int) or n < 1:
                problems.append(f'producer {pr["id"]}: {k} must be a known good in a whole count')
        if any(x not in (0, 1, 2, 3) for x in pr.get('seasons', [])):
            problems.append(f'producer {pr["id"]}: seasons are 0 to 3')
        if not isinstance(pr.get('hours'), (int, float)) or pr['hours'] <= 0:
            problems.append(f'producer {pr["id"]}: needs its hours (tools/econ_model/apply_ladder.py)')
    for b, goods in BUYS.items():
        if b not in BUSINESSES:
            problems.append(f'buys: unknown business {b}')
        for g in goods:
            if g not in ITEMS:
                problems.append(f'buys {b}: unknown item {g}')
    for b, goods in SUPPLIES.items():
        if b not in BUSINESSES:
            problems.append(f'supplies: unknown business {b}')
        for g in goods:
            if g not in ITEMS:
                problems.append(f'supplies {b}: unknown item {g}')
    return problems


def check_wild():
    """Hunting and foraging (Data/Wild, doc 41): every animal's ground and yield, every forage good, known."""
    problems = []
    wild = DIR.parent / 'Wild'
    animals = json.loads((wild / 'animals.json').read_text(encoding='utf-8'))
    forage = json.loads((wild / 'forage.json').read_text(encoding='utf-8'))
    ground = animals['ground']
    for a in animals['species']:
        for h in a['habitats']:
            if h not in ground:
                problems.append(f'animal {a["id"]}: unknown ground {h}')
        for k, n in a['yield'].items():
            if k not in ITEMS or not isinstance(n, int) or n < 1:
                problems.append(f'animal {a["id"]}: yield {k} must be a known good in a whole count')
        if a['temper'] not in ('flee', 'cornered', 'fierce') or a['health'] <= 0 or len(a['glyph']) != 1:
            problems.append(f'animal {a["id"]}: temper, health or glyph')
    for g in forage['ground']:
        for good in g['goods']:
            if good['item'] not in ITEMS:
                problems.append(f'forage {g["id"]}: unknown item {good["item"]}')
    sold = {g for v in list(BUYS.values()) + list(SUPPLIES.values()) for g in v}
    for k in sorted({k for a in animals['species'] for k in a['yield']} | {x['item'] for g in forage['ground'] for x in g['goods']}):
        if k not in sold:
            problems.append(f'{k}: hunted or foraged, but no shop buys it')
    return problems


def unsupplied():
    """Ingredients of the starter crafts that nothing in the game supplies, makes or produces (only a maker's
    starting stock)."""
    sold = {g for goods in SUPPLIES.values() for g in goods}
    sold |= {k for c in CRAFTS for k in c['out']} | {k for pr in PRODUCERS for k in pr['out']}
    return sorted({k for c in CRAFTS for k in c['in'] if k not in sold})


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
        print(f'{"recipe":28} {"cost LD":>8} {"value LD":>8} {"margin":>8} {"LD/min":>7}')
        for r in sorted(RECIPES, key=lambda r: r['id']):
            cost, value, gain = margin(r)
            print(f'{r["id"]:28} {cost:8.3f} {value:8.3f} {gain:+8.3f} {gain / (r["seconds"] / 60):7.3f}')
    cats = defaultdict(int)
    for i in ITEMS.values():
        cats[i['category']] += 1
    print(f'{len(ITEMS)} items ({", ".join(f"{k} {v}" for k, v in sorted(cats.items()))}); '
          f'{len(RECIPES)} recipes; {len(STATIONS)} stations; {len(BUSINESSES)} businesses; {len(CRAFTS)} starter crafts')
    if '--crafts' in argv:
        print(f'{"craft":20} {"makers":34} {"in":34} {"out":22} {"cost":>5} {"value":>5}')
        for c in CRAFTS:
            cost, value, _ = margin(c)
            print(f'{c["id"]:20} {",".join(c["makers"]):34} {str(c["in"]):34} {str(c["out"]):22} {cost:5g} {value:5g}')
    for k in unsupplied():
        print(f'NOTE: {k} goes into a starter craft but nothing supplies, makes or produces it yet')
    for n in NOTES:
        print('NOTE:', n)
    for p in problems:
        print('PROBLEM:', p)
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
