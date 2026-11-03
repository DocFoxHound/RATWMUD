#!/usr/bin/env python3
"""The doc's tables for a farms run: per year against the base and the 12-year run; per area; the treasury's day."""
import collections
import json
import sys

sys.path.insert(0, '/tmp/claude-1000/-home-martinb-Documents-GitHub-MUDProject/da6a1181-05f7-491a-a334-c3e7343d5540/scratchpad/farms_sim')
import analyse as A  # noqa: E402

SEASONS = ['spring', 'summer', 'autumn', 'winter']
run_path, base_path, long_path = sys.argv[1:4]
run = A.lines(run_path)
base = A.lines(base_path, run[-1]['day'])
long = A.lines(long_path, run[-1]['day'])


def rows_of(rows):
    return {y: A.year_row(rs, y) for y, rs in A.years(rows)}


R, B, L = rows_of(run), rows_of(base), rows_of(long)
ag = {x['year']: x for x in A.against(run_path, base_path)}

print('| Year | Run | Hunger (mean, worst) | Wages paid | Treasury, 4-week means (first -> last) | Its lowest | Nights under its cap | Support unpaid (the switch\'s + the ordinary) | Surplus paid out | Gini, 4-week means | Owners\' share |')
print('|---|---|---|---|---|---|---|---|---|---|---|')
for y in sorted(R):
    for name, T in (('farms', R), ('base', B), ('12-year run', L)):
        t = T.get(y)
        if not t:
            continue
        unpaid = f'{t["unpaid_switch"]:,} + {t["unpaid_ord"]:,}' if name != '12-year run' else '-'
        under = t['under_cap'] if name != '12-year run' else f'{t["under_cap"]} (by the day\'s line)'
        print(f'| {y + 1} | {name} | {t["hungry"]:.3f}, {t["hungry_worst"]} | {t["wages"]:.2f}% | {t["t_first"]:,.0f} -> {t["t_last"]:,.0f} | '
              f'{t["t_low"]:,} | {under} | {unpaid} | {t["surplus"]:,} | {t["g_first"]:.3f} -> {t["g_last"]:.3f} | {t["owners"]:.3f} |')
print()
print('| Year | Under the base\'s path (mean, most) | Surplus not paid out (of the base\'s) | The switch\'s cost a day | Wage support more | Toll more | Alms more | Unpaid support more |')
print('|---|---|---|---|---|---|---|---|')
for y, x in sorted(ag.items()):
    print(f'| {y} | {x["under_mean"]:,.0f}p, {x["under_most"]:,}p | {x["surplus_less"]:,}p ({x["surplus_less_pct"]:.0f}%) | {x["cost_a_day"]:.0f}p | '
          f'{x["support_more"]:+.0f}p | {x["toll_more"]:+.0f}p | {x["alms_more"]:+.0f}p | {x["unpaid_extra"]:+,}p |')
print()
# Per area: by year, the days a good ran short by season, the farms' wages paid, stock at the year's end, spoiled of grown.
areas = A.areas_of(run)
goods_area = {}
d = json.load(open('/home/martinb/Documents/GitHub/MUDProject/.claude/worktrees/farms/Data/Economy/country_farms.json'))
for a in d['areas']:
    for g in a['goods']:
        goods_area[g] = a['id']
print('| Area | Year | Days short (Sp / Su / Au / Wi) and of what | Farm wages paid | Stock at the year\'s end | Spoiled (of grown) | Takings, wages, basket a day |')
print('|---|---|---|---|---|---|---|')
for a in areas:
    for y, rs in A.years(run):
        on = [r for r in rs if r.get(f'farm_{a}_on')]
        if not on:
            continue
        short = collections.defaultdict(collections.Counter)
        days = collections.Counter()
        for r in on:
            hit = False
            for g, n in r.get('farms_out_goods', {}).items():
                if n > 0 and goods_area.get(g) == a:
                    short[g][r['season']] += 1
                    hit = True
            days[r['season']] += hit
        what = '; '.join(f'{g} {" / ".join(str(short[g][s]) for s in SEASONS)}' for g in sorted(short, key=lambda g: -sum(short[g].values())))
        owed = sum(r[f'farm_{a}_wages_owed'] for r in on)
        paid = sum(r[f'farm_{a}_wages_paid'] for r in on)
        grown = sum(r[f'farm_{a}_grown'] for r in on)
        spoiled = sum(r[f'farm_{a}_spoiled'] for r in on)
        m = lambda k: sum(r[f'farm_{a}_{k}'] for r in on) / len(on)
        print(f'| {a} | {y + 1} | {" / ".join(str(days[s]) for s in SEASONS)}{": " + what if what else ""} | {100 * paid / owed:.1f}% | '
              f'{on[-1][f"farm_{a}_stock"]:,} | {100 * spoiled / grown:.1f}% | {m("takings"):,.0f}p, {m("wages_paid"):,.0f}p, {m("basket"):,.0f}p |')
print()
pf = A.per_farm(run)
full = sum(1 for by in pf.values() if all(v['full'] == v['days'] for v in by.values()))
print(f'{full} of {len(pf)} farms paid their crews in full from their own tills every day; least float {min(min(v["float_min"] for v in by.values()) for by in pf.values())}p')
# The thinnest farm-season: takings over wages.
thin = []
for fid, by in pf.items():
    for k, v in by.items():
        thin.append((v['takings'] / v['owed'] if v['owed'] else 9, fid, k))
thin.sort()
print('thinnest seasons (takings / wages):', ', '.join(f'{fid} y{k[0] + 1} {k[1]} {x:.2f}' for x, fid, k in thin[:6]))
bad = A.conserved(run)
print(f'money conserved every day counting the minted: {"yes" if not bad else bad[:5]}; minted by the farms {run[-1]["farms_minted"]:,}p; '
      f'money {run[0]["money_start"]:,}p -> {run[-1]["money"]:,}p')
# Support by trade, run against the base's 1-year (world_check4) where present.
sup = collections.defaultdict(collections.Counter)
for r in run:
    for k, v in r.get('support_by_trade', {}).items():
        sup[r['day'] // 365][k] += v
for y in sorted(sup):
    n = sum(1 for r in run if r['day'] // 365 == y)
    print(f'support by trade y{y + 1}:', ', '.join(f'{k} {v / n:.1f}' for k, v in sup[y].most_common(16)))
