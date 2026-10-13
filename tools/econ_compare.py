#!/usr/bin/env python3
"""Economy runs side by side (Docs/Design/61-industry-of-the-realm.md, "Testing").

  build-econ/econ_watch EXPORT DAYS OUT --deterministic      # a run (a 28-day month takes about ten minutes)
  python3 tools/econ_compare.py OUT [OUT...] [--names A,B]   # what they came to, each in a column

For each run: the residents' health week by week (median purse, Gini, short of a week's food, broke, hungry,
starving); each town at the end (residents, median purse, short, food on its shops' shelves, its buildings'
condition); the great houses' and treasuries' money; the goods makers were shortest of in the last week; and what
moved between the towns (coins and goods, by kind: caravans, standing orders, trade).
"""
from __future__ import annotations

import argparse
import csv
import json
import sys
from collections import defaultdict
from pathlib import Path


def load(run: Path) -> dict:
    days = [json.loads(line) for line in (run / 'days.jsonl').read_text().splitlines() if line.strip()]
    materials = defaultdict(lambda: [0, 0])
    last = max((d['day'] for d in days if isinstance(d.get('day'), int)), default=0)
    with open(run / 'materials.csv') as f:
        for row in csv.DictReader(f):
            if int(row['day']) > last - 7:
                materials[row['item']][0] += int(row['maker_hours_short'])
                materials[row['item']][1] += int(row['supplier_hours_short'])
    trade = defaultdict(lambda: [0, 0])
    with open(run / 'trade.csv') as f:
        for row in csv.DictReader(f):
            a, b = row['from'], row['to']
            if '(' in a and '(' in b:
                continue                        # (The capital's purse and the road's: not a town's trade.)
            trade[(a, b, row['kind'])][0] += int(row['coins'])
            trade[(a, b, row['kind'])][1] += int(row['goods'])
    return {'days': days, 'materials': materials, 'trade': trade}


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('runs', nargs='+', type=Path)
    parser.add_argument('--names', help='Column names, comma-separated')
    parser.add_argument('--goods', type=int, default=15, help='How many short goods to list')
    args = parser.parse_args(argv)
    names = args.names.split(',') if args.names else [r.name for r in args.runs]
    runs = [load(r) for r in args.runs]
    w = 14

    def row(label, values):
        print(f'  {label:<34}' + ''.join(f'{str(v):>{w}}' for v in values))

    print('Residents, week by week')
    row('', names)
    for day in (7, 14, 21, 28, 56, 84):
        at = [next((d for d in r['days'] if d.get('day') == day), None) for r in runs]
        if not any(at):
            continue
        for key in ('median', 'gini', 'short', 'broke', 'hungry', 'starving'):
            row(f'day {day}: {key}', [('' if d is None else round(d[key], 3) if isinstance(d[key], float) else d[key]) for d in at])
    ends = [r['days'][-1] for r in runs]
    print('\nTowns at the end: residents / median purse / short / food on shelves / condition')
    towns = sorted({t for e in ends for t in e.get('towns', {})})
    row('', names)
    for t in towns:
        cells = []
        for e in ends:
            x = e.get('towns', {}).get(t)
            cells.append('' if not x else f"{x['median']}/{x['short']}/{x['food']}/{round(e.get('condition', {}).get(t, 0))}")
        row(t, cells)
    print('\nMoney held at the end (great houses, treasuries, the church)')
    holders = sorted({a for e in ends for a, _ in e.get('top_accounts', []) if a.startswith(('house:', 'stores:', 'treasury', 'town:all:church'))})
    row('', names)
    for h in holders:
        row(h, [dict(e.get('top_accounts', [])).get(h, '') for e in ends])
    print(f'\nMakers short (maker-hours, the last 7 days), the {args.goods} worst in any run')
    worst = sorted({g for r in runs for g in r['materials']}, key=lambda g: -max(r['materials'].get(g, [0])[0] for r in runs))
    row('', names)
    for g in worst[:args.goods]:
        row(g, [r['materials'].get(g, [0, 0])[0] for r in runs])
    print('\nBetween the towns (coins; goods), the whole run')
    pairs = sorted({k for r in runs for k in r['trade']}, key=lambda k: -max(r['trade'].get(k, [0])[0] for r in runs))
    row('', names)
    for k in pairs[:30]:
        row(f'{k[0][:10]}>{k[1][:10]} {k[2][:12]}', [(f"{r['trade'][k][0]};{r['trade'][k][1]}" if k in r['trade'] else '') for r in runs])
    return 0


if __name__ == '__main__':
    sys.exit(main())
