"""Players in the closed economy (doc 63): hoarders, market crashes, and the mint that answers hoarding only when it
harms. Each scenario runs the model with players doing one thing, and reports how the realm fares and recovers.

    python3 tools/econ_model/players.py SCENARIO [--days N] [--mint] [--start-day D]

Scenarios:
  none         no players (the baseline)
  hoard        20 players earn from contracts (paid by the town and the Houses) and keep 95% of it
  hoard-spend  as hoard, then from day D + 5 years they spend their hoards over a year
  dump-bread   a player sells a month of the realm's bread at half its price, over 30 days
  dump-swords  a player sells 200 swords at a third of their price
  corner-grain a player buys 60% of the grain in store, holds it 120 days, then sells it back
  corner-grain-hard  the same with 90%, bought just before the harvest (start it on a year's day 170)
  raid-hands   a player hires 40 wolves away from their trades for 60 days at twice the going rate

The mint (--mint) is a thermostat on the money in use: all money, less hoarded money. Hoarded money is players'
dormant money: what a player's purse (or a keep's) holds beyond two months of its own spending, as a four-week
average. NPCs' purses aren't counted: their rules keep their money moving. Harm is wages falling more than 3% in 8 weeks, more than 10% of wolves idle,
or more than 1% hungry. When hoarding pulls the money in use more than 5% under what the realm was founded with, and
there is harm, the mint strikes the difference, up to 1% of that a week, into the town treasury, which pays it out as
works and relief. When the money in use is more than 5% over it (hoards spent again) and prices aren't falling, it
melts the difference, up to 2% a week, the same share from every purse: the town, the church, the Houses, every
household, business and player (but not a household with under a week's food money). It never melts more than it
struck.
"""
import argparse
import collections
import math

import model
from model import YEAR, MONTH, HOURS


class Stall:
    """A player's goods for sale: no makers, no hands; a price it sets."""
    def __init__(self, id, good, stock, price):
        self.id, self.good, self.stock, self._price = id, good, stock, price
        self.place = "any"
        self.today_sold = self.today_unmet = self.week_takings = 0.0
        self.out_hit = False

    @property
    def price(self):
        return self._price


class Mint:
    def __init__(self, w, on=True):
        self.on = on
        self.out_avg = collections.defaultdict(float)      # Each purse's spending a day (a month's average).
        self.wages = collections.deque(maxlen=8)           # The going wage, week by week.
        self.struck = 0.0                                  # Struck and not yet melted.
        self.target = w.money                              # The money in use the realm was founded with.
        self.normal = []                                   # Dormant shares measured before players (year 2).
        self.last_hoarded = 0.0
        self.recent = collections.deque(maxlen=4)
        self.log = []

    def dormant(self, w):
        """Players' dormant money: what a player's purse (or a keep's) holds beyond two months of its own spending.
        NPCs' purses aren't counted: their rules keep their money moving, and the realm balances them on its own."""
        total = 0.0
        for who, cash in w.L.cash.items():
            if w.L.kind[who] == "player":
                total += max(0.0, cash - 60 * self.out_avg[who])
        return total

    def melt(self, w, amount):
        """Melt `amount` from every purse alike, the same share of what each holds (a recoinage's fee): the town, the
        church, the Houses, every household, business and player; but not a household with under a week's food money,
        so the melt never leaves anyone hungry."""
        L = w.L
        spared = {h.id for h in w.households if L.cash[h.id] < 7 * w.need_cost(h)}
        purses = [who for who, cash in L.cash.items() if cash > 0 and who not in spared]
        held = sum(L.cash[who] for who in purses)
        if held <= 0:
            return 0.0
        share = min(1.0, amount / held)
        melted = 0.0
        for who in purses:
            take = L.cash[who] * share
            L.cash[who] -= take
            melted += take
        L.flows["the mint melts"] += melted
        return melted

    def __call__(self, w):
        for who in w.L.cash:
            self.out_avg[who] += (w.L.out_today.get(who, 0.0) - self.out_avg[who]) / MONTH
        if w.day % 7:
            return
        self.wages.append(w.market_wage())
        dormant = self.dormant(w)
        normal = 0.0                                         # Players' purses: none of it is the realm's own.
        self.recent.append(dormant - normal * w.money)
        hoarded = sum(self.recent) / len(self.recent)        # Dormant money above the realm's normal (4 weeks).
        trend = self.wages[-1] / self.wages[0] - 1 if len(self.wages) == 8 else 0.0
        r = w.log[-1]
        idle = r["idle"] / max(1, len(w.workers))
        hungry = r["hungry"] / max(1, r["people"])
        harm = trend < -0.03 or idle > 0.10 or hungry > 0.01
        struck = melted = 0.0
        # A thermostat on the money in use (all money, less what's hoarded above the realm's normal): it should stay
        # where the realm was founded.
        in_use = w.money - max(0.0, hoarded)
        gap = self.target - in_use
        if self.on and w.day >= 2 * YEAR and gap > 0.05 * self.target and harm:
            struck = min(gap, 0.01 * self.target)                  # Too little in use, and it's doing harm: strike.
            w.L.cash["treasury"] += struck
            w.L.flows["the mint strikes"] += struck
            w.money += struck
            self.struck += struck
        elif self.on and self.struck > 0 and gap < -0.05 * self.target and trend >= 0:
            # Too much in use (hoards spent again) and prices not falling: melt what was struck.
            melted = self.melt(w, min(self.struck, -gap, 0.02 * self.target))
            w.money -= melted
            self.struck -= melted
        self.last_hoarded = hoarded
        self.log.append(dict(day=w.day, dormant=dormant, hoarded=hoarded, normal=normal, trend=trend, idle=idle, hungry=hungry, harm=harm,
                             struck=struck, melted=melted, outstanding=self.struck, money=w.money))


class Players:
    def __init__(self, w, scenario, start):
        self.scenario, self.start = scenario, start
        self.ids = []
        if scenario in ("hoard", "hoard-spend"):
            for i in range(20):
                pid = f"player:{i}"
                w.L.open(pid, "player")
                self.ids.append(pid)
        if scenario in ("corner-grain", "corner-grain-hard", "raid-hands", "dump-bread", "dump-swords"):
            w.L.open("player:rich", "player")
            self.ids.append("player:rich")
        self.raided = []
        self.raider = model.Employer("player:rich", 0.0)        # The player's own hire (not one of the realm's).
        self.held = 0.0
        self.events = []

    def melt(self, w, amount):
        """Melt `amount` from every purse alike, the same share of what each holds (a recoinage's fee): the town, the
        church, the Houses, every household, business and player; but not a household with under a week's food money,
        so the melt never leaves anyone hungry."""
        L = w.L
        spared = {h.id for h in w.households if L.cash[h.id] < 7 * w.need_cost(h)}
        purses = [who for who, cash in L.cash.items() if cash > 0 and who not in spared]
        held = sum(L.cash[who] for who in purses)
        if held <= 0:
            return 0.0
        share = min(1.0, amount / held)
        melted = 0.0
        for who in purses:
            take = L.cash[who] * share
            L.cash[who] -= take
            melted += take
        L.flows["the mint melts"] += melted
        return melted

    def __call__(self, w):
        d = w.day - self.start
        s = self.scenario
        if d < 0:
            return
        L = w.L
        if s in ("hoard", "hoard-spend"):
            wage = w.market_wage()
            spending_out = s == "hoard-spend" and d >= 5 * YEAR
            for pid in self.ids:
                # Contracts: a player earns about two labourers' days a day from the town's and the Houses' boards.
                payers = ["treasury"] + list(w.houses)
                payer = max(payers, key=lambda p: L.cash[p])
                if not spending_out:
                    L.pay(payer, pid, 2 * wage, "contracts")
                # Spending: 5% of what it earns, on bread, finery and works; or, spending out, a year's fall of it.
                budget = 0.1 * wage if not spending_out else L.cash[pid] / max(1, 6 * YEAR - d)
                for good in ("finery", "works", "bread"):
                    if budget <= 0:
                        break
                    price = w.price_of(good)
                    if price > 0:
                        _, spent = w.buy(pid, good, budget / price, budget, "any")
                        budget -= spent
        elif s == "dump-bread" and d == 0:
            loaves = sum(h.persons for h in w.households) * model.BREAD_A_DAY * 30
            w.stalls["bread"] = [Stall("player:rich", "bread", loaves, w.price_of("bread") * 0.5)]
            self.stall_days, self.stall = 30, w.stalls["bread"][0]
        elif s == "dump-swords" and d == 0:
            w.stalls["sword"] = [Stall("player:rich", "sword", 200, w.price_of("sword") / 3)]
            self.stall = w.stalls["sword"][0]
        elif s in ("corner-grain", "corner-grain-hard"):
            farms = w.firms["wheat"]
            if d == 0:
                L.cash["player:rich"] += 0                 # Its money is a hoard it already holds: below.
            if d == 0:
                want = sum(f.stock for f in farms) * (0.9 if s == "corner-grain-hard" else 0.6)
                price = w.price_of("wheat")
                # The player's hoard: what it would have earned (taken from the Houses, who paid it over years).
                purse = want * price * 1.6
                for hid in w.houses:
                    L.pay(hid, "player:rich", purse / len(w.houses), "contracts, over the years")
                got, _ = w.buy("player:rich", "wheat", want, L.cash["player:rich"], "any")
                self.held = got
                self.events.append((w.day, f"bought {got:.0f} wheat"))
            if d == 120 and self.held > 0:
                w.stalls["wheat"] = [Stall("player:rich", "wheat", self.held, w.price_of("wheat") * 0.9)]
                self.events.append((w.day, f"sells {self.held:.0f} wheat back"))
                self.held = 0
        elif s == "raid-hands":
            if d == 0:
                purse = 40 * 2 * w.market_wage() * 60 * 1.1
                for hid in w.houses:
                    L.pay(hid, "player:rich", purse / len(w.houses), "contracts, over the years")
                hands = sorted((x for x in w.workers if x.employer is not None and isinstance(x.employer, model.Firm)),
                               key=lambda x: -x.employer.wage)[:0] or \
                    [x for x in w.workers if x.employer is not None and isinstance(x.employer, model.Firm)]
                w.rng.shuffle(hands)
                for x in hands[:40]:
                    e = x.employer
                    e.workers.remove(x)
                    e.vacancies += 1
                    x.employer = self.raider
                    self.raider.workers.append(x)
                    self.raided.append(x)
            if 0 <= d < 60:
                for x in self.raided:
                    p = L.pay("player:rich", x.household.id, 2 * w.market_wage(), "wages from a player")
                    x.paid[-1] += p
                    x.household.income[-1] += p
                    x.household.week_income += p
            if d == 60:
                for x in list(self.raider.workers):
                    x.employer = None                       # Let go: back to the labour market.
                self.raider.workers.clear()
                self.raided = []


def run(scenario, days, mint_on, start_day, seed=1):
    w = model.World("normal", seed)
    mint = Mint(w, mint_on)
    players = Players(w, scenario, start_day)
    w.hooks += [players, mint]
    for _ in range(days):
        w.step()
    return w, mint, players


def watch(w, mint, players, every, good=None):
    cols = ["day", "gini", "poorer_half", "hungry", "idle", "wage", "bread_ld", "sword_ld", "rapier_ld"]
    print(" ".join(f"{c:>9}" for c in cols) + f" {'players':>9} {'hoarded':>9} {'struck':>9} {'money':>9}"
          + (f" {good + ' LD':>9} {good + ' hands':>11}" if good else ""))
    mlog = {m["day"]: m for m in mint.log}
    for r in w.log:
        if r["day"] % every:
            continue
        m = mlog.get(r["day"]) or mlog.get(r["day"] - r["day"] % 7) or {}
        held = sum(w.L.cash[p] for p in players.ids)
        extra = ""
        if good:
            fs = w.firms[good]
            extra = f" {r.get(good + '_ld', 0):9.3f} {r.get(good + '_hands', 0):11}"
        print(" ".join(f"{r[c]:9.3f}" if isinstance(r[c], float) else f"{r[c]:9}" for c in cols)
              + f" {r.get('players', 0):9.0f} {m.get('hoarded', 0):9.0f} {m.get('outstanding', 0):9.0f} {r.get('money', w.money):9.0f}" + extra)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("scenario", choices=["none", "hoard", "hoard-spend", "dump-bread", "dump-swords", "corner-grain",
                                         "corner-grain-hard", "raid-hands"])
    ap.add_argument("--days", type=int, default=8 * YEAR)
    ap.add_argument("--mint", action="store_true")
    ap.add_argument("--start-day", type=int, default=3 * YEAR)
    ap.add_argument("--every", type=int, default=91)
    ap.add_argument("--watch", default=None, help="a good to follow: its price (LD) and hands")
    a = ap.parse_args()
    w0 = None

    def extra(w):
        r = w.log[-1]
        r["players"] = sum(w.L.cash[p] for p in w.L.cash if w.L.kind[p] == "player")
        r["money"] = w.money
        if a.watch:
            fs = w.firms[a.watch]
            r[a.watch + "_ld"] = w.price_of(a.watch) / w.market_wage()
            r[a.watch + "_hands"] = sum(len(f.workers) for f in fs)
    w = model.World("normal", 1)
    mint = Mint(w, a.mint)
    players = Players(w, a.scenario, a.start_day)
    w.hooks += [players, mint]
    for _ in range(a.days):
        w.step()
        extra(w)
    print(f"{a.scenario}, mint {'on' if a.mint else 'off'}, players from day {a.start_day}; events: {players.events}")
    watch(w, mint, players, a.every, a.watch)
