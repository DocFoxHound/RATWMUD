"""Players' keeps in the closed economy (doc 63): small settlements a player group founds, with 1-3 workshops, a farm and
houses for NPCs, hiring from the realm's wolves; they appear from year 1 on and fade after some years.

    python3 tools/econ_model/keeps.py KEEPS [--days 7280] [--draw] [--seed 1]

KEEPS: how many keeps over the run (0-10). Each is founded on a day in years 1-15, lasts 2-8 years and then fades
(its workshops close, their hands are let go, its money goes back to its players). Each has 1-3 workshops (bread,
tools, swords, cloth, leather, finery, building works), each holding 3-6 hands, a farm, and houses for 10-30 wolves.

Population (the user's choice, 2026-10-14: draw). A keep's houses are filled by wolves from the towns and the realm's
number stays put: the towns refill by their births after deaths, and a faded keep's wolves go back to the towns. With
--grow (tried, not chosen) a keep's houses raise the realm's number instead: it swells to fill them and, after the
keep fades, takes decades to come back.

Money. A keep's players found it with what they've earned (taken from the town and the Houses, who paid them over
the years; nothing is minted) and go on earning from contracts; its workshops' profits go to its players, who put
them back when a workshop is short and keep the rest. The mint is on.
"""
import argparse
import collections

import model
import players
from model import YEAR

SLOTS = ["bread", "tools", "sword", "cloth", "leather", "finery", "works"]


class Keep:
    def __init__(self, i, rng, last=15):
        self.i = i
        self.owner = f"keep:{i}"
        self.start = rng.randint(1 * YEAR, last * YEAR)
        self.end = self.start + rng.randint(2 * YEAR, 8 * YEAR)
        self.slots = rng.sample(SLOTS, rng.randint(1, 3))
        self.hands = {g: rng.randint(3, 6) for g in self.slots}
        self.houses = rng.randint(10, 30)
        self.firms = []
        self.alive = False


class Keeps:
    def __init__(self, w, n, draw=True, seed=1, staffed=False, crowd=False, last=15):
        self.staffed = staffed
        rng = __import__("random").Random(seed + 100)
        self.keeps = [Keep(i, rng, last) for i in range(n)]
        if crowd:                       # The stress: every keep at once, years 5-15, three workshops of six, 30 houses.
            for k in self.keeps:
                k.start, k.end = 5 * YEAR + k.i * 7, 15 * YEAR
                k.slots = rng.sample(SLOTS, 3)
                k.hands = {g: 6 for g in k.slots}
                k.houses = 30
        self.draw = draw
        self.founded = self.faded = 0
        for k in self.keeps:
            w.L.open(k.owner, "player")

    def found(self, w, k):
        L, wage = w.L, w.market_wage()
        # Its players' money: what they've earned over the years, from the town's and the Houses' boards.
        fund = 120 * wage * (len(k.slots) + 1)
        payers = ["treasury"] + list(w.houses)
        for p in payers:
            L.pay(p, k.owner, fund / len(payers), "contracts, over the years")
        for g in k.slots + ["wheat"]:
            f = model.Firm(f"firm:{g}:keep{k.i}", g, k.owner, wage, w.rng.randrange(7))
            f.place = "country"
            if g != "wheat":
                f.max_hands = k.hands[g]
            built = w.built
            f.unit_cost = built[g] / (1 + model.MARGIN) * wage / model.HOURS
            for i in model.GOODS[g]["inputs"]:
                f.input_cost[i] = built[i] * wage / model.HOURS
            if g == "wheat":
                f.year_sales = k.houses * model.BREAD_A_DAY * 0.5 * YEAR      # Its own people's bread, at the least.
                f.max_hands = 99
            if g == "leather":
                f.offer = built["hide"] * wage / model.HOURS
            L.open(f.id, "firm")
            L.pay(k.owner, f.id, L.cash[k.owner] / (len(k.slots) + 1), "founding a workshop")
            f.vacancies = 2
            w.firms[g].append(f)
            w.firm_by_id[f.id] = f
            k.firms.append(f)
        if not self.draw:
            w.pop_target += k.houses
        k.alive = True
        self.founded += 1

    def fade(self, w, k):
        for f in k.firms:
            for x in list(f.workers):
                w.fire(f, x)
            w.firms[f.good].remove(f)
            w.L.pay(f.id, k.owner, w.L.cash[f.id], "a workshop closed")
            f.stock = 0.0
        # Debts owed to a closed workshop are written off; its own debts are paid from what's left.
        for key in [key for key in w.debts if key[1] in {f.id for f in k.firms} or key[0] in {f.id for f in k.firms}]:
            del w.debts[key]
            w.debt_age.pop(key, None)
        if not self.draw:
            w.pop_target -= k.houses
        k.alive = False
        self.faded += 1

    def __call__(self, w):
        for k in self.keeps:
            if w.day == k.start:
                self.found(w, k)
            elif w.day == k.end and k.alive:
                self.fade(w, k)
            if k.alive and self.staffed:
                # Fully staffed: its players want every place filled and pay a quarter over the going rate for it.
                for f in k.firms:
                    if f.good != "wheat":
                        f.wage = max(f.wage, w.market_wage() * model.skill_factor(f.skill) * 1.25)
                        f.vacancies = max(f.vacancies, f.max_hands - len(f.workers))
            if k.alive:
                # Contracts keep coming in: about a labourer's day a day, a workshop.
                payer = max(["treasury"] + list(w.houses), key=lambda p: w.L.cash[p])
                w.L.pay(payer, k.owner, w.market_wage() * len(k.slots), "contracts")
                # The players put money back into a workshop short of a week's working money.
                for f in k.firms:
                    short = w.working_money(f) / 4 - w.L.cash[f.id]
                    if short > 0:
                        w.L.pay(k.owner, f.id, short, "invested")
                # And spend a tenth of what they hold above a season's founding fund, in the realm.
                spare = w.L.cash[k.owner] * 0.002
                for good in ("bread", "finery", "works"):
                    price = w.price_of(good)
                    if price > 0 and spare > 0:
                        _, spent = w.buy(k.owner, good, spare / 3 / price, spare / 3, "any")
        r = w.log[-1]
        r["keeps"] = sum(k.alive for k in self.keeps)
        r["keep_hands"] = sum(len(f.workers) for k in self.keeps if k.alive for f in k.firms)
        r["target"] = w.pop_target
        r["money"] = w.money
        r["keep_money"] = sum(w.L.cash[k.owner] + sum(w.L.cash[f.id] for f in k.firms) for k in self.keeps)


def run(n, days, draw=True, seed=1, staffed=False, crowd=False):
    w = model.World("normal", seed)
    mint = players.Mint(w, True)
    keeps = Keeps(w, n, draw, seed, staffed, crowd)
    w.hooks += [keeps, mint]
    for _ in range(days):
        w.step()
    return w, keeps, mint


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("keeps", type=int)
    ap.add_argument("--days", type=int, default=20 * YEAR)
    ap.add_argument("--grow", action="store_true", help="keeps' houses raise the realm's number (the default draws)")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--every", type=int, default=364)
    ap.add_argument("--staffed", action="store_true", help="keeps fill every place, paying a quarter over the rate")
    ap.add_argument("--crowd", action="store_true", help="every keep at once, years 5-15, 3 workshops of 6, 30 houses")
    a = ap.parse_args()
    w, keeps, mint = run(a.keeps, a.days, not a.grow, a.seed, a.staffed, a.crowd)
    print(f"{a.keeps} keeps ({'their houses grow the realm' if a.grow else 'drawing on the towns'}"
          f"{', fully staffed' if a.staffed else ''}): "
          + ", ".join(f"#{k.i} years {k.start / YEAR:.1f}-{k.end / YEAR:.1f} {'+'.join(k.slots)} houses {k.houses}"
                      for k in keeps.keeps))
    cols = [("day", "day"), ("keeps", "keeps"), ("keep_hands", "k.hands"), ("people", "people"), ("target", "target"),
            ("gini", "gini"), ("poorer_half", "poor½"), ("hungry", "hungry"), ("idle", "idle"), ("wage", "wage"),
            ("bread_ld", "bread"), ("sword_ld", "sword"), ("rapier_ld", "rapier"), ("keep_money", "k.money"),
            ("money", "money")]
    print(" ".join(f"{h:>8}" for _, h in cols))
    for r in w.log:
        r.setdefault("money", w.money)
        if r["day"] % a.every == 0 or r["day"] == w.log[-1]["day"]:
            print(" ".join(f"{r.get(c, 0):8.3f}" if isinstance(r.get(c, 0), float) and abs(r.get(c, 0)) < 100
                           else f"{r.get(c, 0):8.0f}" for c, _ in cols))
