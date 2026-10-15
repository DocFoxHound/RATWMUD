"""A century of the realm (doc 63): run the model for 100 years and look for anything that drifts, breaks or runs
away. Prints a line a year, a table a decade, and the things that stand out.

    python3 tools/econ_model/century.py [--years 100] [--keeps N] [--seed 1] [--out FILE.csv]

With --keeps N, N players' keeps (fully staffed, drawing on the towns) are founded at random through the century,
each lasting 2-8 years, and the mint is on.
"""
import argparse
import csv
import statistics

import keeps as keeps_mod
import model
import players
from model import YEAR

KEYS = ["people", "homes", "masters", "apprentices", "elders", "gini", "poorer_half", "hungry", "idle", "wage",
        "bread_ld", "sword_ld", "rapier_ld", "money_household", "money_firm", "money_house", "money_church",
        "money_treasury", "money_player", "game", "keeps", "keep_hands"]
RUNGS = {"bread_ld": (0.04, 0.10), "sword_ld": (1.5, 4.5), "rapier_ld": (15, 40)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--years", type=int, default=100)
    ap.add_argument("--keeps", type=int, default=0)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out")
    a = ap.parse_args()
    w = model.World("normal", a.seed)
    if a.keeps:
        k = keeps_mod.Keeps(w, a.keeps, draw=True, seed=a.seed, staffed=True, last=max(2, a.years - 8))
        w.hooks += [k, players.Mint(w, True)]
    days = a.years * YEAR
    for _ in range(days):
        w.step()
    rows = []
    for r in w.log:
        for key in KEYS:
            r.setdefault(key, 0)
        rows.append(r)
    if a.out:
        with open(a.out, "w", newline="") as f:
            out = csv.DictWriter(f, ["day"] + KEYS, extrasaction="ignore")
            out.writeheader()
            out.writerows(rows)
    print(f"{a.years} years, seed {a.seed}, {a.keeps} keeps; money at the end {w.money:,.0f}")
    print("by decade (means; hungry: days with more than 50 hungry; off-rung: days outside the rung's band)")
    head = ["years", "people", "homes", "masters", "gini", "poor½", "wage", "bread", "sword", "rapier", "hungry d",
            "short d", "off-rung", "homes$", "firms$", "Houses$", "church$", "town$", "players$", "keeps"]
    print(" ".join(f"{h:>8}" for h in head))
    for d0 in range(0, a.years, 10):
        part = [r for r in rows if d0 * YEAR <= r["day"] < (d0 + 10) * YEAR]
        if not part:
            break
        m = lambda k: statistics.fmean(r[k] for r in part)
        hungry = sum(1 for r in part if r["hungry"] > 50)
        short = sum(1 for r in part if r["hungry"] > 300)
        off = sum(1 for r in part for k, (lo, hi) in RUNGS.items() if not lo <= r[k] <= hi)
        vals = [f"{d0}-{d0 + 10}", m("people"), m("homes"), m("masters"), m("gini"), m("poorer_half"), m("wage"),
                m("bread_ld"), m("sword_ld"), m("rapier_ld"), hungry, short, off, m("money_household"),
                m("money_firm"), m("money_house"), m("money_church"), m("money_treasury"), m("money_player"),
                max(r["keeps"] for r in part)]
        print(" ".join(f"{v:>8}" if isinstance(v, str) else f"{v:8.3f}" if abs(v) < 10 else f"{v:8.0f}" for v in vals))
    # What stands out: the worst days, and the drifts from the first decade's settled state (years 3-10).
    print("\nwhat stands out:")
    settled = [r for r in rows if 3 * YEAR <= r["day"] < 10 * YEAR]
    if not settled:
        return
    late = [r for r in rows if r["day"] >= (a.years - 10) * YEAR]
    for key in ["people", "homes", "masters", "gini", "poorer_half", "wage", "bread_ld", "sword_ld", "rapier_ld",
                "money_household", "money_firm", "money_house", "money_church", "money_treasury", "game"]:
        s0 = statistics.fmean(r[key] for r in settled)
        s1 = statistics.fmean(r[key] for r in late)
        change = (s1 / s0 - 1) if s0 else 0
        flag = "  <-- drift" if abs(change) > 0.25 else ""
        print(f"  {key:16} years 3-10 {s0:12.3f}   last decade {s1:12.3f}   {change:+7.1%}{flag}")
    worst = max(rows, key=lambda r: r["hungry"])
    print(f"  most hungry on one day: {worst['hungry']} (day {worst['day']}, year {worst['day'] / YEAR:.1f})")
    print(f"  wage range after year 3: {min(r['wage'] for r in rows if r['day'] > 3 * YEAR):.2f} - "
          f"{max(r['wage'] for r in rows if r['day'] > 3 * YEAR):.2f}")
    print(f"  successions to an untrained heir: {w.successions}")
    print(f"  masters range: {min(r['masters'] for r in rows)} - {max(r['masters'] for r in rows)}; "
          f"people {min(r['people'] for r in rows)} - {max(r['people'] for r in rows)}; "
          f"homes {min(r['homes'] for r in rows)} - {max(r['homes'] for r in rows)}")


if __name__ == "__main__":
    main()
