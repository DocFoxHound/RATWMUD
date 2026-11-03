#!/usr/bin/env python3
"""What a run with the Country's food farms did, year by year (Docs/Design/71-the-country-s-industries.md, 4, Phase B: what
the 3-year runs watch), from world_check's economy file (`--economy F --farms DAY`):

    python3 tools/farms_summary.py ECONOMY.jsonl [--base BASE.jsonl]

Per year (365 lines from the first): the treasury (its lowest at an hour's end, its nights under its cap, by season), the
wage support the short tills wanted and what of it went unpaid (the switch's: the farms' and the trades the switch takes
orders from, `switchTrades`; and the city's ordinary short tills'), the surplus paid out (against BASE's same year, a run
without the farms, where given), the toll's and the alms' trim, the price level's range; per area (while on): the days
its farms paid their crews in full from their own tills (the gate: 99%), the days its stock ran short of a shop's want
by season (the gate: no more than a day a season), its spoilage (of what it grew), its takings, wages and basket a day,
its stock at the year's end; the crews' spending a day and their short days; by good, the days it ran short by season
and the farms' sales of it a day; the money made by the farms' founding and whether the money stayed conserved every
day."""
from __future__ import annotations

import argparse
import collections
import json
from pathlib import Path

SEASONS = ['spring', 'summer', 'autumn', 'winter']


def lines(path):
    out = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line:
                out.append(json.loads(line))
    return out


def years(rows):
    """The rows in years of 365 lines from the first."""
    return [rows[i:i + 365] for i in range(0, len(rows), 365) if rows[i:i + 365]]


def areas_of(rows):
    return sorted({k[len('farm_'):-len('_on')] for r in rows for k in r if k.startswith('farm_') and k.endswith('_on')})


def table(head, body):
    out = ['| ' + ' | '.join(head) + ' |', '|' + '---|' * len(head)]
    out += ['| ' + ' | '.join(str(c) for c in row) + ' |' for row in body]
    return '\n'.join(out)


def summary(rows, base=None) -> str:
    say = []
    areas = areas_of(rows)
    base_years = years(base) if base else []
    say.append(f'{len(rows)} days; money conserved every day: {all(r.get("conserved") for r in rows)}; the farms\' founding made '
               f'{max((r.get("farms_minted", 0) for r in rows), default=0):,}p.\n')
    body = []
    for y, yr in enumerate(years(rows)):
        lows = {s: min((r['treasury_low'] for r in yr if r.get('season') == s and r.get('treasury_low', -1) >= 0), default=None)
                for s in SEASONS}
        under = {s: sum(r.get('under_cap', 0) for r in yr if r.get('season') == s) for s in SEASONS}
        surplus = sum(r.get('surplus_paid', 0) for r in yr)
        base_surplus = sum(r.get('surplus_alms', 0) + r.get('works_spent', 0) for r in base_years[y]) if y < len(base_years) else None
        levels = [r['price_level'] for r in yr if 'price_level' in r]
        body.append([y + 1, ' / '.join('-' if lows[s] is None else f'{lows[s]:,}' for s in SEASONS),
                     ' / '.join(str(under[s]) for s in SEASONS), f'{sum(r.get("support_wanted", 0) for r in yr):,}',
                     f'{sum(r.get("support_unpaid_switch", 0) for r in yr):,} + {sum(r.get("support_unpaid_ordinary", 0) for r in yr):,}',
                     f'{surplus:,}' + ('' if base_surplus is None else f' ({surplus - base_surplus:+,} on the base\'s {base_surplus:,})'),
                     f'{yr[-1].get("toll_now", 0):g}% ({yr[-1].get("toll_trim", 0):+g}), {yr[-1].get("alms_now", 0)}p ({yr[-1].get("alms_trim", 0):+d})',
                     f'{min(levels):.3f}-{max(levels):.3f}' if levels else '-'])
    say.append('### The treasury\n')
    say.append(table(['Year', 'Lowest (Sp / Su / Au / Wi)', 'Nights under its cap', 'Support wanted', 'Unpaid: the switch\'s + the city\'s ordinary',
                      'Surplus paid out', 'Toll (trim), alms (trim) at the end', 'Price level'], body))
    for y, yr in enumerate(years(rows)):
        say.append(f'\n### Year {y + 1}: the farms by area\n')
        body = []
        for a in areas:
            on = [r for r in yr if r.get(f'farm_{a}_on')]
            if not on:
                body.append([a, 'off', '', '', '', '', '', ''])
                continue
            paid = sum(1 for r in on if r.get(f'farm_{a}_wages_paid', 0) >= r.get(f'farm_{a}_wages_owed', 0))
            out = collections.Counter(r.get('season') for r in on if r.get(f'farm_{a}_out', 0) > 0)
            grown = sum(r.get(f'farm_{a}_grown', 0) for r in on)
            spoiled = sum(r.get(f'farm_{a}_spoiled', 0) for r in on)
            mean = lambda k: sum(r.get(f'farm_{a}_{k}', 0) for r in on) / len(on)
            body.append([a, len(on), f'{paid / len(on):.1%}', ' / '.join(str(out[s]) for s in SEASONS),
                         f'{spoiled / grown:.1%}' if grown else '-',
                         f'{mean("takings"):,.0f}p, {mean("wages_paid"):,.0f}p, {mean("basket"):,.0f}p',
                         f'{on[-1].get(f"farm_{a}_stock", 0):,}', f'{mean("crew_spent"):,.0f}p, {sum(r.get(f"farm_{a}_crew_short", 0) for r in on)}'])
        say.append(table(['Area', 'Days on', 'Paid its crews in full', 'Days short of stock (Sp / Su / Au / Wi)', 'Spoiled (of grown)',
                          'Takings, wages, basket a day', 'Stock at the end', 'Crews\' spending a day, short days'], body))
        # By good (`farms_out_goods`): which of an area's goods the shops wanted that the farms couldn't fill, and the farms'
        # sales of it a day (a good no farm grows sells nothing).
        out = collections.defaultdict(collections.Counter)
        sold = collections.Counter()
        for r in yr:
            for g, n in r.get('farms_out_goods', {}).items():
                if n > 0:
                    out[g][r.get('season')] += 1
            for g, n in r.get('farms_sold_goods', {}).items():
                sold[g] += n
        if out:
            say.append(f'\nDays a good ran short (Sp / Su / Au / Wi), year {y + 1}: ' + '; '.join(
                f'{g} {" / ".join(str(out[g][s]) for s in SEASONS)} (sold {sold[g] / len(yr):,.1f} a day)'
                for g in sorted(out, key=lambda g: -sum(out[g].values()))))
    return '\n'.join(say) + '\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('economy')
    ap.add_argument('--base', help="the same run without the farms (its surplus paid out, year by year)")
    a = ap.parse_args()
    print(summary(lines(a.economy), lines(a.base) if a.base else None))


if __name__ == '__main__':
    main()
