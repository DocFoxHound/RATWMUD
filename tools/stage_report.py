#!/usr/bin/env python3
"""Read a soak's --stage file (world_check --stage FILE: Docs/Design/79-a-staged-economy.md, 16.13; Phase 11).

  python3 tools/stage_report.py FILE [--brief]

Prints the run's days at a glance (not with --brief) (wall time, the slider, the bands' shares and hunger, thefts, wagons, carts, ships
and events seen, faucets), each market over the run (households, price index first and last and its range, goods at the
clamp's ends, Times), hunger over the days' hours and the planned thieves (lines that have them), and every watched size with its first, largest and last value. A size is "growing" when its last
quarter's least is above its first quarter's largest and it rose by more than a tenth and by more than two: a long run
must keep those bounded (doc 79, 15). For reading a soak, never for balancing.
"""
from __future__ import annotations

import json
import sys


def flat(sizes: dict, prefix: str = '') -> dict:
    out = {}
    for k, v in sizes.items():
        if isinstance(v, dict):
            out.update(flat(v, prefix + k + '.'))
        elif isinstance(v, (int, float)) and not isinstance(v, bool):
            out[prefix + k] = v
    return out


def growing(values: list) -> bool:
    if len(values) < 8:
        return False
    q = max(2, len(values) // 4)
    first, last = values[:q], values[-q:]
    return min(last) > max(first) and values[-1] - values[0] > max(2, 0.1 * abs(values[0]))


def read(path: str) -> list:
    with open(path, encoding='utf-8') as f:
        return [json.loads(line) for line in f if line.strip()]


def seen(line: dict, *kinds: str) -> int:
    return sum(line.get('seen', {}).get(k, 0) for k in kinds)


def report(lines: list, days: bool = True) -> list:
    out = []
    if not lines:
        return ['no lines']
    walls = [l['wall'] for l in lines]
    out.append(f"{len(lines)} days; wall a day: first {walls[0]:.2f} s, mean {sum(walls) / len(walls):.2f}, "
               f"least {min(walls):.2f}, most {max(walls):.2f}, last {walls[-1]:.2f}")
    if days:
        out.append('day  date               wall  slider  dest/poor/comf/rich (hungry)    thefts  wagons out/in/robbed  carts  ships in/out  events  faucet +/-')
        for l in lines:
            b = l['bands']
            bands = '/'.join(f"{b[k]['share']:.2f}" for k in ('destitute', 'poor', 'comfortable', 'rich'))
            hungry = '/'.join(f"{b[k]['hungry']:.2f}" for k in ('destitute', 'poor', 'comfortable', 'rich'))
            out.append(f"{l['runDay']:>3}  {l['date']:<18} {l['wall']:>5.2f}  {l['slider']:>5}  {bands} ({hungry})  "
                       f"{l['thefts']['thefts']:>3}  {seen(l, 'caravan departs'):>3}/{seen(l, 'caravan arrives'):>3}/{seen(l, 'raid'):>2}  "
                       f"{seen(l, 'cart departs'):>4}  {seen(l, 'ship docks'):>2}/{seen(l, 'ship sails'):>2}  "
                       f"{seen(l, 'event begins'):>3}  {l['faucets']['made']:.0f}/{l['faucets']['sunk']:.0f}")
    if not all(l.get('realBalanced') for l in lines):
        out.append('REAL MONEY NOT BALANCED on ' + ', '.join(str(l['runDay']) for l in lines if not l.get('realBalanced')))
    out.append('markets: households (first, last), price index first, last, least, most; goods at the clamp (most); Times seen')
    ids = [m['id'] for m in lines[0]['markets']]
    for mid in ids:
        rows = [next((m for m in l['markets'] if m['id'] == mid), None) for l in lines]
        rows = [r for r in rows if r]
        prices = [r['price'] for r in rows]
        times = sorted({r['times'] for r in rows})
        out.append(f"  {mid:<12} {rows[0]['households']:>4} {rows[-1]['households']:>4}  {prices[0]:.3f} {prices[-1]:.3f} "
                   f"{min(prices):.3f} {max(prices):.3f}  high {max(r['atHigh'] for r in rows)} low {max(r['atLow'] for r in rows)} "
                   f"of {rows[-1]['goods']}  hungry most {max(r['hungry'] for r in rows)}  thefts {sum(r['thefts'] for r in rows)}  "
                   f"{', '.join(times)}")
    # Hunger over each day (not only the line's hour) and the planned thieves, where the lines have them.
    if all('hungryMost' in l['bands'].get('destitute', {}) for l in lines):
        out.append('hungry households over the day, by band (dest/poor/comf/rich): most at any hour '
                   + '/'.join(str(max(l['bands'][k]['hungryMost'] for l in lines)) for k in ('destitute', 'poor', 'comfortable', 'rich'))
                   + '; mean of the days\' hourly means '
                   + '/'.join(f"{sum(l['bands'][k]['hungryMean'] for l in lines) / len(lines):.2f}"
                              for k in ('destitute', 'poor', 'comfortable', 'rich')))
    if all('thieves' in l for l in lines):
        keys = ('considered', 'hungerHigh', 'hungry', 'hungryShut', 'hungryNoShop', 'foodMarks', 'pocketMarks')
        out.append('thieves over the run: ' + ', '.join(f"{k} {sum(l['thieves'].get(k, 0) for l in lines):.0f}" for k in keys)
                   + f"; thefts from tills {sum(l['thefts'].get('fromTills', 0) for l in lines)}")
    out.append('sizes: first, most, last')
    series: dict = {}
    for l in lines:
        for k, v in flat(l.get('sizes', {})).items():
            series.setdefault(k, []).append(v)
    grew = []
    for k, values in series.items():
        flag = growing(values)
        if flag:
            grew.append(k)
        out.append(f"  {k:<32} {values[0]:>10} {max(values):>10} {values[-1]:>10}{'  GROWING' if flag else ''}")
    out.append('growing: ' + (', '.join(grew) if grew else 'none'))
    return out


def main(argv: list) -> int:
    if not argv or argv[0] in ('-h', '--help'):
        print(__doc__)
        return 0 if argv else 2
    lines = read(argv[0])
    print('\n'.join(report(lines, days='--brief' not in argv)))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
