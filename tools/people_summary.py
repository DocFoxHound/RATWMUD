"""A long economy run's years (doc 68, Progress, "Births follow deaths; arrivals from the Country; the treasury's target").

python3 tools/people_summary.py ECONOMY.jsonl [LOOP.jsonl]

From world_check --economy's line a day: money conserved every day (the ledger's, and the money supply the start's: no
money is made since newcomers went); per year (days 5-364, then whole years) hunger, wages paid, the treasury, the
households' purses and the earners' Gini as 4-week means (the year's first and last 28 days), the owners' share, the
drift (toll, alms, the treasury's level, wages, prices, shelves); and the people: residents, households (and those of
one), pups, working age, retired, births, arrivals and deaths, the children still owed, the unplaced NPCs left, the posts
that pay held (the year's fewest), the longest a post stood empty, how the empty posts were filled, and the players
(there are none). With LOOP.jsonl (--loop-state), the toll's and the alms' moves every 28 days.
"""
import json
import sys


def mean(xs):
    xs = list(xs)
    return sum(xs) / len(xs) if xs else 0.0


def main():
    rows = [json.loads(line) for line in open(sys.argv[1])]
    loop = [json.loads(line) for line in open(sys.argv[2])] if len(sys.argv) > 2 else []
    bad = [r['day'] for r in rows if not r['conserved'] or r['money'] != r['money_start']]
    print(f'# {sys.argv[1]}: {len(rows)} days (day {rows[0]["day"]} to {rows[-1]["day"]}); money conserved every day (and the '
          f'money supply the start\'s, {rows[0]["money_start"]}p): {"yes" if not bad else "NO, days " + str(bad[:10])}; players: '
          f'{max(r.get("players", 0) for r in rows)} at most')
    births = sum(r['births'] for r in rows)
    deaths = sum(r['deaths'] for r in rows)
    print(f'# the whole run: {births} born, {deaths} died, {rows[-1].get("births_owed", 0)} children owed at the end '
          f'(births + owed = deaths: {"yes" if births + rows[-1].get("births_owed", 0) == deaths else "NO"}); '
          f'{sum(r.get("arrivals", 0) for r in rows)} arrived from the Country, {rows[0].get("unplaced", 0)} -> '
          f'{rows[-1].get("unplaced", 0)} unplaced')
    years = (rows[-1]['day'] + 1 + 364) // 365
    for y in range(years):
        lo, hi = 365 * y + (5 if y == 0 else 0), 365 * y + 364
        rs = [r for r in rows if lo <= r['day'] <= hi]
        if len(rs) < 56:
            continue
        first, last = rs[:28], rs[-28:]
        e = rs[-1]
        owed, paid = sum(r['wages_owed'] for r in rs), sum(r['wages_paid'] for r in rs)
        profit = sum(r['profit'] for r in rs)
        print(f'y{y + 1} days {rs[0]["day"]}-{e["day"]}: hungry {mean(r["hungry_mean"] for r in rs):.3f} worst {max(r["hungry_most"] for r in rs)} '
              f'days {sum(1 for r in rs if r["hungry_most"] > 0)} wages {100 * paid / owed:.2f}% tillsShort {mean(r["tills_short"] for r in rs):.1f} '
              f'treasury4 {mean(r["treasury"] for r in first):.0f}->{mean(r["treasury"] for r in last):.0f} '
              f'(low {min(r["treasury"] for r in rs)}) purses4 {mean(r["purses"] for r in first):.0f}->{mean(r["purses"] for r in last):.0f} '
              f'tills4 {mean(r["tills"] for r in last):.0f} country4 {mean(r["country"] for r in last):.0f} '
              f'gini4 {mean(r["gini_earners"] for r in first):.3f}->{mean(r["gini_earners"] for r in last):.3f} '
              f'owners {profit / (profit + paid):.3f} profit {profit / len(rs):.0f} paid {paid / len(rs):.0f} '
              f'alms {mean(r["alms"] for r in rs):.0f} support {mean(r["wage_support"] for r in rs):.0f} surplus {mean(r["surplus"] for r in rs):.0f} '
              f'toll {mean(r["toll"] for r in rs):.0f} spent {mean(r["residents_spent"] for r in rs):.0f}')
        print(f'    people: residents {e["residents"]} households {e["households"]} (alone {e.get("households_alone", "-")}) '
              f'earning {e["earning_households"]} | births {sum(r["births"] for r in rs)} arrivals {sum(r.get("arrivals", 0) for r in rs)} '
              f'deaths {sum(r["deaths"] for r in rs)} owed {e.get("births_owed", "-")} unplaced {e.get("unplaced", "-")} | pups {e["pups"]} '
              f'working-age {e["working_age"]} retired {e["retired"]} age {e["age_mean"]:.1f} | paying posts held '
              f'{min(r.get("earning_held", 0) for r in rs)}-{e.get("earning_held", 0)}/{e.get("earning_posts", 0)} (empty longest '
              f'{max(r.get("empty_longest", 0) for r in rs):.1f} d) all posts {e["posts_held"]}/{e["posts"]} largest household '
              f'{e["household_most"]} | filled so far: city {e.get("filled_city", 0)} (next class {e.get("filled_city_next", 0)}) '
              f'Country {e.get("filled_country", 0)} (next class {e.get("filled_country_next", 0)}) local {e.get("filled_local", 0)} '
              f'best-trained {e.get("filled_best", 0)}')
        print(f'    drift: toll {e["toll_now"]} alms {e["alms_now"]} level {e["treasury_level"]:.0f} | keeper {e["wage_keeper"]:.2f} '
              f'hand {e["wage_hand"]:.2f} step mean {e["wage_step_mean"]:.2f} sd {e["wage_step_sd"]:.2f} up {e["wage_up"]} down {e["wage_down"]}'
              f' top {e["wage_top"]} foot {e["wage_foot"]} | prices moved {e["price_moved"]} sd {e["price_sd"]:.3f} '
              f'range {e["price_low"]:.2f}-{e["price_high"]:.2f} level {mean(r["price_level"] for r in rs):.3f} '
              f'(min {min(r["price_level"] for r in rs):.3f} max {max(r["price_level"] for r in rs):.3f}) '
              f'mean sd {mean(r["price_sd"] for r in rs):.3f} | shelves {e["shelves"]} mean {e["shelf_mean"]:.1f}')
    if loop:
        print('## The toll and the alms every 28 days (moved from the world\'s) and the treasury\'s level')
        line = []
        for d in loop:
            l = d['loop']
            line.append(f'{d["day"]}:{l.get("toll", 0):+.1f}/{l.get("alms", 0):+d}/{l.get("level", -1):.0f}')
            if len(line) == 8:
                print('  ' + '  '.join(line))
                line = []
        if line:
            print('  ' + '  '.join(line))


main()
