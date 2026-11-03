#!/usr/bin/env python3
"""A Data copy with country_farms.json tweaked, for a run.

    variant.py NAME TWEAKS.json

TWEAKS: {"goods": {good: factor}, "farmGoods": {farm_kind: {good: factor}}, "areas": {area: {"yieldScale": N,
"crewScale": N, "price": "year", "priceFactor": N}}, "hands": {farm_kind: N}}
"""
import json
import shutil
import sys
from pathlib import Path

sys.path.insert(0, '/home/martinb/Documents/GitHub/MUDProject/.claude/worktrees/farms/tools')
import country_industry as ci   # noqa: E402  (season_means_of, middles: Society::seasonal's blend)

S = Path(__file__).resolve().parent
W = Path('/home/martinb/Documents/GitHub/MUDProject/.claude/worktrees/farms')
name, tweaks = sys.argv[1], json.load(open(sys.argv[2]))
out = S / f'data_{name}'
if out.exists():
    shutil.rmtree(out)
shutil.copytree(W / 'Data', out)
p = out / 'Economy/country_farms.json'
d = json.load(open(p))
for a in d['areas']:
    for k, v in tweaks.get('areas', {}).get(a['id'], {}).items():
        a[k] = v
    for f in a['farms']:
        kind = f['kind']
        for g, ys in f['yield'].items():
            k = tweaks.get('goods', {}).get(g, 1) * tweaks.get('farmGoods', {}).get(kind, {}).get(g, 1)
            f['yield'][g] = [round(y * k, 2) for y in ys]
        for g, fs in tweaks.get('seasonFactors', {}).items():
            if g in f['yield']:
                means = ci.season_means_of(f['yield'][g])
                f['yield'][g] = [round(x, 2) for x in ci.middles([m * k for m, k in zip(means, fs)])]
        for g, k in tweaks.get('addGoods', {}).get(kind, {}).items():
            f['yield'][g] = k
        if kind in tweaks.get('hands', {}):
            f['crew']['hands'] = tweaks['hands'][kind]
json.dump(d, open(p, 'w'), indent=1)
print(out)
