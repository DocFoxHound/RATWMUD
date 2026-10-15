"""A small closed economy to try doc 63's rules on before the game (Python; three years in under a minute).

Fields are sown in spring and harvested at the end of summer: a lot of grain for the work, stored and sold through the
year. Mines are slow, heavy work. Wheat is ground into flour, carted to the city and baked with firewood into bread;
ore is smelted into iron and bronze, iron worked into steel, tools, finery's fittings and building; bronze into swords,
steel into rapiers by a master. Every step pays its hands and its owner.

Prices are built from cost (hours at the maker's wage, inputs delivered, tools worn) plus a margin that follows the
shelf. Production times are set at the founding so those prices sit on the ladder (TARGETS, in a labourer's
day-wages): bread cheap for every wolf, a bronze sword a few days' pay, a rapier a month's.

Every holder follows one rule: it spends its needs and a share of what it holds over a target set by its own income.
Money is minted once, at the founding, and the sum is checked every day.

    python3 tools/econ_model/model.py [--start normal|one_house|empty_treasury|equal] [--days 1092] [--seed 1]
"""
import argparse
import collections
import math
import random

MONTH, YEAR, HOURS = 28, 364, 8
SPRING, HARVEST = (0, 90), (196, 252)        # Days of the year: sowing, harvest.
WINTER = (273, 363)

# Goods: what's in one (inputs), the skill its trade needs, where it's made, its weight (carting), how many days of
# sales its maker keeps on the shelf, the share that spoils a day, tool wear per hour of work, and how many makers.
# TARGETS: what one should cost, in a labourer's day-wages. Hours are calibrated from them at the founding.
GOODS = {
    "wheat":      dict(kind="farm", inputs={}, skill=0, place="country", weight=5, keep=0, tool=0.002, firms=10),
    "wood":       dict(kind="gather", inputs={}, skill=0, place="country", weight=3, keep=6, tool=0.004, firms=4),
    "wool":       dict(kind="gather", inputs={}, skill=0, place="country", weight=2, keep=8, tool=0.002, firms=2),
    "iron_ore":   dict(kind="mine", inputs={}, skill=0, place="country", weight=8, keep=8, tool=0.02, firms=2),
    "copper_ore": dict(kind="mine", inputs={}, skill=0, place="country", weight=8, keep=8, tool=0.02, firms=1),
    "hide":       dict(kind="hunt", inputs={}, skill=0, place="country", weight=3, keep=0, firms=0),   # Hunters'.
    "flour":      dict(kind="make", inputs={"wheat": 1}, skill=0, place="city", weight=5, keep=6, firms=2),
    "bread":      dict(kind="make", inputs={"flour": 0.5, "wood": 0.1}, skill=0, place="city", weight=1, keep=2,
                       perish=0.15, firms=8),
    "iron":       dict(kind="make", inputs={"iron_ore": 2, "wood": 1}, skill=10, place="country", weight=4, keep=8,
                       firms=2),
    "bronze":     dict(kind="make", inputs={"copper_ore": 2, "wood": 1}, skill=20, place="country", weight=4, keep=8,
                       firms=1),
    "steel":      dict(kind="make", inputs={"iron": 1, "wood": 2}, skill=50, place="city", weight=4, keep=10, firms=1),
    "tools":      dict(kind="make", inputs={"iron": 1, "wood": 0.5}, skill=10, place="city", weight=3, keep=10,
                       firms=2),
    "cloth":      dict(kind="make", inputs={"wool": 2}, skill=10, place="city", weight=2, keep=8, firms=3),
    "leather":    dict(kind="make", inputs={"hide": 1}, skill=10, place="city", weight=2, keep=8, firms=2),
    "finery":     dict(kind="make", inputs={"cloth": 1, "iron": 0.2, "leather": 0.2}, skill=30, place="city", weight=1, keep=8,
                       firms=3),
    "works":      dict(kind="make", inputs={"wood": 3, "iron": 0.3}, skill=0, place="city", weight=0, keep=5,
                       firms=3),
    "sword":      dict(kind="make", inputs={"bronze": 1, "wood": 0.5, "leather": 0.25}, skill=30, place="city", weight=2, keep=10,
                       firms=2),
    "rapier":     dict(kind="make", inputs={"steel": 1, "wood": 1, "leather": 0.25}, skill=60, place="city", weight=2, keep=20,
                       firms=1),
}
TARGETS = {"wheat": 0.025, "wood": 0.04, "wool": 0.08, "iron_ore": 0.15, "copper_ore": 0.2, "flour": 0.05,
           "bread": 0.06, "iron": 0.75, "bronze": 1.0, "steel": 1.6, "tools": 1.5, "cloth": 0.5, "finery": 2.5,
           "works": 2.0, "sword": 3.0, "rapier": 25.0, "hide": 0.12, "leather": 0.4}
try:                                             # The game's own placings, where this model's goods share its ids.
    from ladder import Ladder
    _ladder = Ladder()
    TARGETS.update({g: _ladder.worth(g) for g in TARGETS if g in _ladder.placed})
    SKILL_WAGE = _ladder["skillWage"]
except (ImportError, OSError):
    pass
WHOLE = {"sword", "rapier", "finery"}            # Bought one at a time, never a share of one.
FARM = dict(yield_per_acre=20, sow_share=1 / 3)  # A field's work: a third at sowing, two-thirds at harvest.
# Game in the woods (doc 41's finite, regenerating sources): K beasts at most, breeding back at r a day (fastest at
# half K). A wolf with no work that day may hunt: it brings in a beast with chance `catch` x game / K, which gives
# `meat` loaves' worth of food to its household's larder and a hide it sells to a tannery.
GAME = dict(K=3000, r=0.02, catch=0.5, meat=6, on=True)
ARTISAN_FLOOR = 0.6                              # An artisan paid by the hours worked gets at least this of a day's
                                                 # wage: lower, and a shop keeps hands it has no work for (they cost it
                                                 # nothing) while they live on a fraction of a wage.
SHEEP_HIDES = 0.15                               # Hides a sheep run sells to the tanneries, a measure of wool.
# Lives (doc 63): wolves are children to 16, work to a retiring age of 58-66, and die by age: a yearly chance of
# a x e^(b x years), and more in early childhood. Each death brings a birth some months later into a young family,
# so the realm holds its numbers. A household grown past CROWDED sends a young adult out to found its own.
LIFE = dict(adult=16, retire=(58, 66), a=0.0004, b=0.085, infant=0.03, birth_after=(30, 240), crowded=6)
# Skill: a wolf learns fast beside a better hand in its trade (an apprentice beside a master), slowly alone.
# A wolf learns up to `below` short of the best hand beside it; alone, it gains a little a year. A skilled trade
# keeps one apprentice to each hand at its level (a family's child first, then the young without a post).
SKILL = dict(learn=8, alone=0.3, below=5, top=85, apprentice_pay=0.5, young=24, head=4)
GRAIN_SPOILS = 0.0004                            # A day, in store: about a seventh of a year's grain.
MARGIN = 0.15                                    # The calibration's margin; sellers' own margins float.
SKILL_WAGE = 150                                 # A trade needing skill s pays (1 + s / 150) a labourer's wage: a modest
                                                 # premium the market can wear down; the hours carry a good's worth.
CARRY_PER_WEIGHT = 0.004                         # Labourer's hours to cart a unit of weight between town and country.
BREAD_A_DAY, WOOD_A_DAY = 2, 1
# Owned goods wear with use and go back to their makers to be mended (doc 63). A full mend (of a piece worn to
# nothing) takes `hours` of its making's work and `mat` of its materials; a part-worn piece, that share of it. An owner
# sees to a piece when it's worn to MEND_AT: an heirloom is always mended; otherwise the owner may reach for something
# better it can easily afford (`better`), or simply want a new one (`fashion`: clothes and finery are the most
# disposable), and else mends it if mending costs under MEND_IF of a new one. Mending never shortens a piece's life
# (doc 35): a mended piece is whole again.
WEAR = {"sword": 360, "rapier": 540, "cloth": 150, "finery": 240}   # Days of a household's use, whole to worn out.
WATCH_WEAR = 120                                                    # A guard's sword, in daily use.
REPAIR = {
    "sword":  dict(hours=0.3, mat=0.1, heirloom=0.2, fashion=0.02, better="rapier"),
    "rapier": dict(hours=0.3, mat=0.1, heirloom=0.4, fashion=0.02),
    "cloth":  dict(hours=0.5, mat=0.3, heirloom=0.0, fashion=0.15),
    "finery": dict(hours=0.6, mat=0.4, heirloom=0.05, fashion=0.35),
}
MEND_AT, MEND_IF = 0.35, 0.6

RULE = dict(household=dict(a2=(0.3, 0.7), k=1.0), house=dict(a2=0.3, k=4.0), church=dict(a2=0.4, k=2.0),
            treasury=dict(a2=0.4, k=2.0))
FIRM = dict(buffer=1.0, markup=(-0.3, 0.6), staple_markup=1.0, step=0.04, wage_up=0.03, wage_up_short=0.10,
            wage_down=0.02, calm_weeks=6)
STAPLES = {"wheat", "flour", "bread", "wood"}    # On the Staple rung: in a shortage their margin may reach +100%.
STAPLE_SLACK = 1.2                               # Staple trades staff for a fifth over what they sell.
POACH = dict(on=True, gap=0.10, chance=0.04)     # A post paying 10% over a wolf's own: 4% a day of them move to it.
TAX, TITHE, RENT = 0.08, 0.06, 0.15


def season_day(day):
    return day % YEAR


def in_window(day, window):
    d = season_day(day)
    return window[0] <= d <= window[1]


def skill_factor(skill):
    return 1 + skill / SKILL_WAGE


def calibrate():
    """Hours of work for one of each good (for a field: per acre, sowing and harvest), from the ladder: a good's built
    price is (its hours at its trade's wage + its inputs delivered + its tools worn) x (1 + MARGIN), in labourer's
    hours; the hours are what's left of the target once inputs are paid for."""
    cost = {}
    for good, g in GOODS.items():                  # GOODS is in order: inputs before what's made of them.
        want = TARGETS[good] * HOURS / (1 + MARGIN)
        ins = sum(q * (cost[i] + CARRY_PER_WEIGHT * GOODS[i]["weight"]) for i, q in g["inputs"].items())
        work = max(want - ins, 0.15 * want)
        g["hours"] = work / skill_factor(g["skill"])
        cost[good] = (ins + work) * (1 + MARGIN)
    # Tools are made after the mines that wear them: a second pass takes the wear out of the raw goods' hours.
    for good, g in GOODS.items():
        if g.get("tool"):
            want = TARGETS[good] * HOURS / (1 + MARGIN)
            ins = sum(q * cost[i] for i, q in g["inputs"].items())
            per_hour = skill_factor(g["skill"]) + g["tool"] * cost["tools"]
            g["hours"] = max(want - ins, 0.15 * want) / per_hour
    for good, g in GOODS.items():
        if g["kind"] == "farm":
            per_acre = g["hours"] * FARM["yield_per_acre"]
            g["sow_hours"], g["harvest_hours"] = per_acre * FARM["sow_share"], per_acre * (1 - FARM["sow_share"])
    return cost


class Ledger:
    def __init__(self):
        self.cash, self.kind = {}, {}
        self.flows = collections.defaultdict(float)
        self.out_today = collections.defaultdict(float)   # What each purse paid out today.

    def open(self, who, kind, cash=0.0):
        self.cash[who], self.kind[who] = cash, kind

    def pay(self, a, b, amount, what):
        amount = min(amount, self.cash[a])
        if amount <= 0:
            return 0.0
        self.cash[a] -= amount
        self.cash[b] += amount
        self.flows[what] += amount
        self.out_today[a] += amount
        return amount

    def total(self):
        return sum(self.cash.values())


class Piece:
    """An owned thing: how whole it is (1 new, 0 worn out but kept and mendable), whether it's an heirloom, and how
    many it stands for (a household's clothes: one set a head)."""
    def __init__(self, good, cond=1.0, heirloom=False, count=1):
        self.good, self.cond, self.heirloom, self.count = good, cond, heirloom, count
        self.asked = -99                     # The day a maker last turned it away.


class Worker:
    def __init__(self, id, household, skill, wage):
        self.id, self.household, self.skill = id, household, skill
        self.person = None
        self.apprentice = False
        self.employer, self.wage, self.reservation = None, 0.0, wage
        self.paid = collections.deque([wage] * 7, 7)    # What reached it, a day, this last week.


class Person:
    """One wolf: its age in days, its household, and its Worker while it works."""
    def __init__(self, age, household, retire_at):
        self.age, self.household, self.retire_at = age, household, retire_at
        self.worker = None
        self.retired = False


class Household:
    def __init__(self, id, a2, place):
        self.id, self.a2, self.place = id, a2, place
        self.members = []                    # Persons.
        self.kin = set()                     # Households of its family: its heirs.
        self.workers = []
        self.income = collections.deque([0.0] * MONTH, MONTH)
        self.week_income = 0.0
        self.clothes = None                  # A Piece: the household's clothes, one set a head.
        self.larder = 0.0                    # Bread given it, meat hunted: eaten before any bread is bought.
        self.hides = 0                       # Hides waiting to be sold.
        self.hungry = 0
        self.help_budget = 0.0
        self.want_fund = 0.0
        self.owned = []                      # Pieces: swords, rapiers, finery.

    @property
    def persons(self):
        return len(self.members)

    def month_income(self):
        return sum(self.income)


class Employer:
    def __init__(self, id, wage, skill=0):
        self.id, self.wage, self.skill = id, wage, skill
        self.workers, self.vacancies = [], 0
        self.open_days, self.calm, self.offset, self.short_pay = 0, 0, 0, False


class Firm(Employer):
    def __init__(self, id, good, owner, wage, offset):
        g = GOODS[good]
        super().__init__(id, wage * skill_factor(g["skill"]), g["skill"])
        self.good, self.owner, self.offset, self.place = good, owner, offset, g["place"]
        self.keep = g["keep"]
        self.stock, self.tools, self.tool_cost = 0.0, 0.0, 0.0
        self.inputs = collections.defaultdict(float)
        self.input_cost = collections.defaultdict(float)
        self.markup, self.unit_cost = MARGIN, 1.0
        self.sales = collections.deque([0.0] * MONTH, MONTH)
        self.costs = collections.deque([0.0] * MONTH, MONTH)
        self.today_sold = self.today_unmet = self.week_takings = self.hauled = 0.0
        self.short_inputs = self.out_hit = False
        self.landlord = None
        self.out_days = 0                        # Days this month a buyer found it with nothing to sell.
        self.booked = 0.0                        # Hours of mending taken in today, worked tomorrow.
        self.mending = collections.deque([0.0] * MONTH, MONTH)   # Hours of mending done a day.
        self.mend_turned = 0.0                                   # Hours of mending turned away this week.
        self.mend_mats = collections.defaultdict(float)          # Materials mending wanted lately (laid in too).
        self.made = collections.deque([0.0] * 14, 14)            # What it made a day, these two weeks.
        # A farm's year: acres to sow, acres sown, acres left to harvest, and its year's sales (what it plans by).
        self.acres = self.to_harvest = self.sown = 0.0
        self.year_sales = 0.0
        self.dry_days = 0

    @property
    def price(self):
        return self.unit_cost * (1 + self.markup)

    def daily_sales(self):
        return sum(self.sales) / MONTH


class World:
    def __init__(self, start="normal", seed=1, households=260, money=1_000_000):
        self.rng = random.Random(seed)
        self.L = Ledger()
        self.day, self.start, self.money = 0, start, money
        self.households, self.workers = [], []
        self.firms = collections.defaultdict(list)
        self.houses, self.services = {}, {}
        self.debts, self.debt_age = collections.defaultdict(float), {}
        self.slack, self.helped_today = 0.0, 0
        self.relief_left = {"church": 0.0, "treasury": 0.0}
        self.relief_week = {}
        self.given_bread = 0.0
        self.births_due, self.births, self.deaths = [], 0, 0
        self.pop_target = 0                   # Set at the founding: the realm's number (keeps' houses raise it).
        self.stalls = {}                      # Good -> players' stalls selling it (tools/econ_model/players.py).
        self.servant_posts = {}               # Household -> its servants (an Employer), once it keeps any.
        self.successions = 0                  # Masters' shops passed to an heir with no master's training.
        self.hooks = []                       # Called each day before the measures: players, the mint.
        self.game, self.hunted, self.hides_sold = float(GAME["K"]), 0, 0
        self.helped_ids = set()
        self.month_flows, self.month_out = {}, {}
        self.watch_swords = [Piece("sword", random.Random(seed + 1).uniform(0.35, 1.0)) for _ in range(14)]
        self.log = []
        self.built = calibrate()
        self.found(households)

    # ---- founding -------------------------------------------------------------------------------------------------
    def found(self, n):
        rng, L = self.rng, self.L
        wage0 = 12.0
        for name in ("Greatly", "Fell", "Grayrock"):
            L.open(f"house:{name}", "house")
            self.houses[f"house:{name}"] = dict(week=0.0, owned=[Piece("rapier", rng.random(), rng.random() < 0.5)])
        L.open("church", "church")
        L.open("treasury", "treasury")
        wid = 0
        self.next_hh = n
        for i in range(n):
            persons = rng.choice([1, 2, 2, 3, 3, 4, 4, 5, 6])
            earners = 0 if rng.random() < 0.08 else min(persons, rng.choice([1, 1, 2, 2]))
            h = Household(f"hh{i}", rng.uniform(*RULE["household"]["a2"]), "country" if rng.random() < 0.3 else "city")
            L.open(h.id, "household")
            # Its people: the earners grown, the rest children (or, in a household with no earner, the old).
            for k in range(persons):
                if k < earners:
                    years = rng.uniform(18, 56)
                elif earners == 0 or rng.random() < 0.12:
                    years = rng.uniform(60, 78)
                else:
                    years = rng.uniform(0, 15.9)
                pp = Person(int(years * YEAR), h, int(rng.uniform(*LIFE["retire"]) * YEAR))
                pp.retired = years >= 58
                h.members.append(pp)
            # Founded with what it already owns, of every age, so things wear out over the years rather than all
            # being bought in the first.
            h.clothes = Piece("cloth", rng.uniform(0.35, 1.0), count=persons)
            for good, chance in (("sword", 0.7 if earners else 0.2), ("rapier", 0.12), ("finery", 0.5)):
                if rng.random() < chance:
                    h.owned.append(Piece(good, rng.uniform(0.35, 1.0), rng.random() < REPAIR[good]["heirloom"]))
            for pp in h.members[:earners]:
                r = rng.random()                         # Few masters: about one wolf in fifty.
                skill = rng.uniform(0, 15) if r < 0.6 else rng.uniform(15, 35) if r < 0.88 else \
                    rng.uniform(35, 55) if r < 0.98 else rng.uniform(60, 75)
                w = Worker(wid, h, skill, wage0)
                wid += 1
                w.person, pp.worker = pp, w
                h.workers.append(w)
                self.workers.append(w)
            self.households.append(h)
        self.next_wid = wid
        for h in self.households:                         # Families: each household kin to one or two others.
            for other in rng.sample(self.households, 2):
                if other is not h and rng.random() < 0.7:
                    h.kin.add(other.id)
                    other.kin.add(h.id)
        owned_by_house = {"iron_ore": "house:Grayrock", "copper_ore": "house:Grayrock", "iron": "house:Grayrock",
                          "bronze": "house:Grayrock", "flour": "house:Fell", "wool": "house:Fell",
                          "works": "house:Greatly", "steel": "house:Greatly"}
        keepers = [h for h in self.households if h.workers]
        rng.shuffle(keepers)
        k = 0
        for good, g in GOODS.items():
            for j in range(g["firms"]):
                owner = owned_by_house.get(good)
                if owner is None:
                    owner = keepers[k].id
                    k += 1
                f = Firm(f"firm:{good}:{j}", good, owner, wage0, rng.randrange(7))
                L.open(f.id, "firm")
                self.firms[good].append(f)
        houses = list(self.houses)
        for i, f in enumerate(f for good in ("wheat", "wood", "wool", "iron_ore", "copper_ore")
                              for f in self.firms[good]):
            f.landlord = houses[i % len(houses)]
        self.carter = Firm("firm:carter", "wood", "house:Fell", wage0, rng.randrange(7))
        self.carter.good, self.carter.place, self.carter.keep = "carriage", "both", 0
        L.open(self.carter.id, "firm")
        for name, wage, count in (("watch", wage0, 14), ("clergy", wage0 * 1.1, 5), ("works", wage0 * 0.85, 0),
                                  ("fabric", wage0, 0)):      # The church's masons (its building fund's crew).
            e = Employer(f"svc:{name}", wage)
            e.vacancies = count
            self.services[name] = e
        for h in self.houses:
            e = Employer(f"svc:retainers:{h}", wage0)
            e.vacancies = 4
            self.services[f"retainers:{h}"] = e
            self.services[f"estate:{h}"] = Employer(f"svc:estate:{h}", wage0)   # Its estate's builders and hands.
        self.by_id = {h.id: h for h in self.households}
        self.firm_by_id = {f.id: f for f in self.all_firms()}
        self.pop_target = sum(h.persons for h in self.households)
        self.seed_firms(wage0)
        self.staff()
        self.found_money()

    def need_per_day(self):
        heads = sum(h.persons for h in self.households)
        need = collections.defaultdict(float)
        need["bread"] = heads * BREAD_A_DAY
        need["wood"] = len(self.households) * WOOD_A_DAY * 1.2
        need["cloth"] = heads / WEAR["cloth"] * 0.5
        need["finery"] = len(self.households) / 25
        need["works"] = 2
        need["sword"] = 0.05
        need["rapier"] = 0.02
        for good in reversed(list(GOODS)):
            for i, q in GOODS[good]["inputs"].items():
                need[i] += need[good] * q
        for good, g in GOODS.items():
            need["tools"] += need[good] * g["hours"] * g.get("tool", 0)
        return need

    def seed_firms(self, wage):
        need = self.need_per_day()
        for good, fs in self.firms.items():
            g = GOODS[good]
            for f in fs:
                f.unit_cost = self.built[good] / (1 + MARGIN) * wage / HOURS
                for i in g["inputs"]:
                    f.input_cost[i] = self.built[i] * wage / HOURS
                f.sales = collections.deque([need[good] / len(fs)] * MONTH, MONTH)
                f.stock = need[good] / len(fs) * max(f.keep, 1)
                for i, q in g["inputs"].items():
                    f.inputs[i] = need[good] / len(fs) * q * 3
                if g.get("tool"):
                    f.tools = 2.0
                    f.tool_cost = self.built["tools"] * wage / HOURS
                if good == "leather":
                    f.offer = self.built["hide"] * wage / HOURS
                if g["kind"] == "farm":
                    year = need[good] * YEAR / len(fs)
                    f.stock = year * (HARVEST[1] + 14) / YEAR     # Founded in spring: enough in store to the harvest.
                    f.year_sales = year
        self.carter.unit_cost = CARRY_PER_WEIGHT * wage / HOURS

    def staff(self):
        need = self.need_per_day()
        for good, fs in self.firms.items():
            g = GOODS[good]
            if g["kind"] == "farm":
                hands = 1.0 * len(fs)                     # Between sowing and harvest a farm keeps a hand.
            else:
                hands = need[good] * g["hours"] / HOURS * 1.1
            for f in fs:
                f.vacancies = max(1, round(hands / len(fs)))
        carried = sum(need[g] * q * GOODS[i]["weight"] for g in GOODS for i, q in GOODS[g]["inputs"].items()
                      if GOODS[i]["place"] != GOODS[g]["place"])
        self.carter.vacancies = max(1, round(carried * CARRY_PER_WEIGHT / HOURS * 1.2))
        free = list(self.workers)
        self.rng.shuffle(free)
        for e in sorted(self.employers(), key=lambda e: -e.skill):        # Skilled posts first.
            for w in [w for w in free if w.skill >= e.skill][: e.vacancies]:
                free.remove(w)
                self.hire(e, w)

    def employers(self):
        yield from self.all_firms()
        yield from self.services.values()

    def all_firms(self):
        for fs in self.firms.values():
            yield from fs
        yield self.carter

    def found_money(self):
        L, rng, M = self.L, self.rng, self.money
        if self.start == "one_house":
            L.cash["house:Greatly"] = M
            return
        if self.start == "equal":
            for h in self.households:
                L.cash[h.id] = M / len(self.households)
            return
        shares = dict(household=0.40, firm=0.15, house=0.30, church=0.05, treasury=0.10)
        if self.start == "empty_treasury":
            shares = dict(household=0.45, firm=0.15, house=0.35, church=0.05, treasury=0.0)
        weights = [rng.expovariate(1) for _ in self.households]
        for h, w in zip(self.households, weights):
            L.cash[h.id] = shares["household"] * M * w / sum(weights)
        fs = list(self.all_firms())
        for f in fs:
            L.cash[f.id] = shares["firm"] * M / len(fs)
        for h, s in zip(self.houses, (0.5, 0.3, 0.2)):
            L.cash[h] = shares["house"] * M * s
        L.cash["church"] = shares["church"] * M
        L.cash["treasury"] = shares["treasury"] * M

    # ---- labour ---------------------------------------------------------------------------------------------------
    def hire(self, e, w):
        if w.employer is not None:                   # The post left behind is open again at once.
            w.employer.workers.remove(w)
            w.employer.vacancies += 1
        w.employer, w.wage, w.reservation = e, e.wage, e.wage
        e.workers.append(w)
        e.vacancies = max(0, e.vacancies - 1)
        w.apprentice = False                         # A new post ends an apprenticeship (its master sets one anew).
        w.paid = collections.deque([e.wage] * 7, 7)  # A new post: judged by what it pays from now on.

    def fire(self, e, w):
        e.workers.remove(w)
        w.employer = None
        w.apprentice = False

    def labour_market(self):
        rng = self.rng
        open_ = [e for e in self.employers() if e.vacancies > 0]
        for e in self.employers():                   # Skill: learned beside a better hand, slowly alone.
            if not e.workers or e.skill <= 0:
                continue
            best = max(x.skill for x in e.workers)
            # Up to a few points short of the best hand, but always to the trade's own level where a hand there
            # teaches: an apprenticeship ends.
            reach = max(best - SKILL["below"], e.skill if best >= e.skill else 0)
            reach = min(reach, e.skill + SKILL["below"])     # No further than its own trade's level and a little.
            for w in e.workers:
                if w.skill < SKILL["top"]:
                    if reach > w.skill:
                        rate = SKILL["learn"]
                    elif w is getattr(e, "head", None) and w.skill < e.skill:
                        rate = SKILL["head"]                 # Heading the shop, working toward mastership.
                    else:
                        rate = SKILL["alone"]
                    w.skill += rate / YEAR
                if w.apprentice and w.skill >= e.skill:
                    w.apprentice = False             # Out of its apprenticeship: full pay.
        if not open_:
            return
        going = self.market_wage()
        for w in self.workers:
            # A post is judged by what it paid this last week, not what it promised: a hand getting under 60% of the
            # going rate (a master with no orders, an employer that can't pay) looks for work every day.
            if w.apprentice:
                continue                              # Bound to its master till its apprenticeship ends (indentures).
            pay = sum(w.paid) / 7
            underpaid = w.employer is not None and pay < 0.6 * going
            if w.employer is not None and pay < 0.4 * going and len(w.employer.workers) > 1 \
                    and not getattr(w.employer, "seasonal", False):
                self.fire(w.employer, w)          # Paid next to nothing for a week: it leaves, for whatever comes.
                underpaid = False
            if w.employer is None:
                w.reservation *= 0.995
            elif underpaid:
                w.reservation = max(pay, 0.5 * going)
            elif self.day % MONTH != w.id % MONTH or rng.random() > 0.15:
                continue
            fits = [e for e in open_ if e.vacancies > 0 and w.skill >= e.skill
                    and (w.employer is None or not getattr(e, "seasonal", False))]  # Harvest work is day labour.
            if not fits:
                continue
            best = max(rng.sample(fits, min(6, len(fits))), key=lambda e: e.wage)
            if (w.employer is None or underpaid) and best is not w.employer and best.wage >= w.reservation * 0.95:
                self.hire(best, w)
            elif w.employer is not None and best is not w.employer and best.wage > w.wage * 1.05:
                self.hire(best, w)
        self.masters_to_their_trade()
        self.apprenticeships()
        if POACH["on"]:
            self.poach([e for e in open_ if e.vacancies > 0 and not getattr(e, "seasonal", False)])
            self.harvest_call([e for e in open_ if e.vacancies > 0 and getattr(e, "seasonal", False)])
        for e in open_:                              # Posts still open at the day's end.
            e.open_days += e.vacancies > 0

    def apprenticeships(self):
        """A skilled trade with a place free takes on a young wolf as its apprentice: one with no post first, else one
        in plain work (an apprenticeship is the better prospect). A masters' trade (skill 50 and up) keeps an
        apprentice to each master even with no post open, so it never dies with its last master."""
        young = [w for w in self.workers if w.person and w.person.age < SKILL["young"] * YEAR and not w.apprentice
                 and (w.employer is None or (isinstance(w.employer, Firm) and w.employer.skill < 10))]
        young.sort(key=lambda w: w.employer is not None)       # The idle first.
        if not young:
            return
        for f in self.all_firms():
            if f.skill < 30:
                continue
            places = self.apprentice_places(f)
            masters_trade = f.skill >= 50
            while places > 0 and (f.vacancies > 0 or masters_trade) and young:
                if not masters_trade and young[0].employer is not None:
                    break                                       # Only a masters' trade takes the young from a post.
                w = young.pop(0)
                if w.employer is not None:
                    w.employer.vacancies += 1                  # Its old post is open for another.
                    w.employer.workers.remove(w)
                    w.employer = None
                f.vacancies += 0 if f.vacancies > 0 else 1
                self.hire(f, w)
                w.apprentice = True
                places -= 1

    def masters_to_their_trade(self):
        """A masters' trade (skill 50 and up) with a post open seeks out a master: idle, or working below its level
        (in a trade that doesn't need it). Its own trade first."""
        for f in self.all_firms():
            if f.skill < 50 or f.vacancies <= 0:
                continue
            free = [w for w in self.workers if w.skill >= f.skill and not w.apprentice and
                    (w.employer is None or w.employer.skill < f.skill)]
            if free:
                w = max(free, key=lambda w: w.skill)
                if w.employer is not None:
                    w.employer.vacancies += 1
                self.hire(f, w)
                f.head = w

    def apprentice_places(self, f):
        """One apprentice to each hand already at the trade's level; two while the trade can't fill its posts."""
        qualified = sum(1 for x in f.workers if not x.apprentice and x.skill >= f.skill)
        per = 2 if (f.vacancies > 0 and f.calm == 0) or f.skill >= 50 else 1
        apprentices = sum(x.apprentice for x in f.workers)
        cap = max(1, len(f.workers) // 3) if f.skill < 50 else qualified * per   # At most a third of its hands.
        return min(qualified * per, cap) - apprentices

    def poach(self, open_):
        """A trade short of hands draws them from other day jobs: a share of the wolves it can use, who earn
        well under its wage, hear of the post and move each day. The short trade raises its wage until enough do."""
        rng = self.rng
        for e in sorted(open_, key=lambda e: -e.wage):
            pool = [w for w in self.workers if w.employer is not None and w.employer is not e and not w.apprentice
                    and w.skill >= e.skill and w.wage < e.wage * (1 - POACH["gap"])
                    and len(w.employer.workers) > 1             # Nobody leaves a post they hold alone,
                    and w.employer.vacancies == 0]              # nor an employer already short of hands.
            for w in pool:
                if e.vacancies <= 0:
                    break
                if rng.random() < POACH["chance"]:
                    self.hire(e, w)

    def harvest_call(self, farms):
        """Sowing and harvest call on everyone: hands from trades that aren't staples (finery, cloth, building,
        the blades) go to the fields for the season, a tenth of them a day, and their own posts reopen behind them."""
        rng = self.rng
        pool = [w for w in self.workers if w.employer is not None and isinstance(w.employer, Firm) and not w.apprentice
                and w.employer.good not in STAPLES and w.employer is not self.carter
                and GOODS.get(w.employer.good, {}).get("kind") == "make" and len(w.employer.workers) > 1
                and w.employer.skill < 30]                      # Masters' trades don't go reaping.
        rng.shuffle(pool)
        for f in farms:
            for w in pool:
                if f.vacancies <= 0:
                    break
                if w.employer is not f and rng.random() < 0.1:
                    self.hire(f, w)

    def pay_wages(self, e, payer, part=1.0):
        bill = e.wage * sum(SKILL["apprentice_pay"] if w.apprentice else part for w in e.workers)
        if bill <= 0:
            return 0.0
        share = min(1.0, self.L.cash[payer] / bill)
        paid = 0.0
        for w in e.workers:
            p = self.L.pay(payer, w.household.id, e.wage * share * (SKILL["apprentice_pay"] if w.apprentice
                                                                     else part), "wages")
            w.paid[-1] += p
            w.household.income[-1] += p
            w.household.week_income += p
            paid += p
        if share < 1:
            e.short_pay = True
        return paid

    def wage_week(self, e):
        """Up when a post stood open most of the week; down slowly after calm weeks while hands stand idle; never
        under what the trade's hands can get elsewhere (the going rate for their skill): an employer that can't pay
        that lets hands go instead."""
        rng = self.rng
        floor = self.market_wage() * skill_factor(e.skill) * 0.85 if self.day > 7 else 0.0
        if e.short_pay:
            e.short_pay = False
            e.wage *= 1 - rng.uniform(0, FIRM["wage_down"] * 2)
            if len(e.workers) > 1:
                self.fire(e, rng.choice(e.workers))
        elif e.open_days >= 4:
            # The shorter of hands, the bigger the raise: up to a tenth a week when half its posts stand open; but
            # never past twice the going rate for its skill (past that a shortage shows in its prices instead).
            short = e.vacancies / max(1, e.vacancies + len(e.workers))
            step = FIRM["wage_up"] + (FIRM["wage_up_short"] - FIRM["wage_up"]) * min(1.0, short * 2)
            e.wage = min(e.wage * (1 + rng.uniform(0.3, 1.0) * step), self.market_wage() * skill_factor(e.skill) * 2)
            e.calm = 0
        else:
            e.calm += 1
            if e.calm >= FIRM["calm_weeks"] and self.slack > 0.03:
                e.wage *= 1 - rng.uniform(0, FIRM["wage_down"])
        # Nor long far over it: wolves elsewhere would do the work for less, so a wage over 30% above the going rate
        # for its skill slides back a twentieth a week whenever the employer has no post open.
        if self.day > 7 and e.vacancies == 0 and e.wage > self.market_wage() * skill_factor(e.skill) * 1.3:
            e.wage *= 0.95
        e.wage = max(e.wage, floor)
        e.open_days = 0

    def market_wage(self):
        """A labourer's wage: the middle of what the unskilled trades pay, by their hands (once a day)."""
        if getattr(self, "_mw_day", None) != self.day:
            ws = sorted(f.wage for f in self.all_firms() if f.skill <= 10 and GOODS.get(f.good, {}).get("kind") != "farm"
                        for _ in range(max(1, len(f.workers))))   # Not the harvest's pay: it follows this, not leads.
            self._mw, self._mw_day = ws[len(ws) // 2], self.day
        return self._mw

    # ---- trade ----------------------------------------------------------------------------------------------------
    def carriage(self, seller, place, good):
        if place == "any" or seller.place in (place, "both"):
            return 0.0
        return self.carter.price * GOODS[good]["weight"]

    def buy(self, buyer, good, want, budget, place, asks=None):
        """Up to `want` of `good` from the cheapest of a few sellers that have it, delivered to `place`. A firm short
        of money may take up to a week of its turnover on credit. What it couldn't get, up to `asks` (a day's real
        need), is left with a seller as unmet demand: the shortage that makes makers take on hands."""
        asks = want if asks is None else asks
        if want <= 1e-9:
            return 0.0, 0.0
        credit = 0.0
        if buyer in self.firm_by_id:
            f = self.firm_by_id[buyer]
            owed = sum(v for (d, _), v in self.debts.items() if d == buyer)
            credit = max(0.0, 7 * f.daily_sales() * f.unit_cost - owed)
            budget = max(budget, credit)
        fs = self.firms[good]
        sellers = [f for f in fs if f.stock >= 0.01] + [s for s in self.stalls.get(good, []) if s.stock >= 0.01]
        if not sellers:
            if budget > 0 and fs:
                for f in self.rng.sample(fs, min(2, len(fs))):
                    f.today_unmet += asks / min(2, len(fs))
                    f.out_hit = True
            return 0.0, 0.0
        look = self.rng.sample(sellers, min(4, len(sellers)))
        got = spent = 0.0
        for s in sorted(look, key=lambda s: s.price + self.carriage(s, place, good)):
            carry = self.carriage(s, place, good)
            unit = s.price + carry
            n = min(want - got, s.stock, (budget - spent) / unit, (self.L.cash[buyer] + credit) / unit)
            haul = 0.0
            if carry > 0 and GOODS[good]["weight"] > 0:
                wt = GOODS[good]["weight"]
                if n * wt > self.carter_room:      # Loads the carts couldn't take.
                    self.carter.today_unmet += min(n, asks) * wt - max(0.0, self.carter_room)
                    # A business fetches the rest itself with its own hands (up to a third of their day), slower
                    # and out of its own making; a household or an institution waits for the carts.
                    f = self.firm_by_id.get(buyer)
                    if f is not None:
                        free = len(f.workers) * HOURS / 3 - f.hauled
                        haul = max(0.0, min(n - max(0.0, self.carter_room) / wt, free / (wt * CARRY_PER_WEIGHT * 2)))
                n = min(n, max(0.0, self.carter_room) / wt + haul)
            if good in WHOLE:
                n = math.floor(n + 1e-9)
            if n <= 1e-6:
                continue
            p = self.L.pay(buyer, s.id, n * s.price, f"buys {good}")
            if p < n * s.price - 1e-9 and credit > 0:
                owe = min(n * s.price - p, credit)
                self.debts[(buyer, s.id)] += owe
                credit -= owe
                p += owe
            n = p / s.price
            by_cart = n - haul if carry > 0 else 0.0
            if haul > 0:
                f = self.firm_by_id[buyer]
                hours = min(haul, n) * GOODS[good]["weight"] * CARRY_PER_WEIGHT * 2   # On foot: twice the carts' time.
                f.hauled += hours
                f.booked += hours
            if carry > 0 and by_cart > 0:
                n_cart, n = n, by_cart
                c = self.L.pay(buyer, self.carter.id, n * carry, "carriage")
                if c < n * carry - 1e-9:
                    self.debts[(buyer, self.carter.id)] += n * carry - c
                    credit -= n * carry - c
                self.carter_room -= n * GOODS[good]["weight"]
                self.carter.today_sold += n * GOODS[good]["weight"]
                n = n_cart
            s.stock -= n
            s.today_sold += n
            s.week_takings += p
            got += n
            spent += n * s.price + max(0.0, by_cart) * carry
            if got >= want - 1e-6:
                break
        short = min(asks, want) - got
        if short > 1e-6 and budget - spent > 0:          # Every seller it tried hears it was short.
            for s in look:
                s.today_unmet += short / len(look)
            if got <= 1e-6:
                look[0].out_hit = True
        return got, spent

    def repay(self, f):
        for key in [k for k in self.debts if k[0] == f.id]:
            paid = self.L.pay(f.id, key[1], self.debts[key], "repaid")
            self.debts[key] -= paid
            if self.debts[key] <= 1e-9:
                del self.debts[key]
                self.debt_age.pop(key, None)
            else:
                self.debt_age[key] = self.debt_age.get(key, 0) + 1
                if self.debt_age[key] > 56:          # Written off: the seller's loss.
                    del self.debts[key]
                    del self.debt_age[key]

    # ---- making ---------------------------------------------------------------------------------------------------
    def normal_cost(self, f):
        """One at full work, rent included: the landlord's share of the price is a cost like any other."""
        g = GOODS[f.good]
        c = g["hours"] * f.wage / HOURS
        c += sum(q * f.input_cost[i] for i, q in g["inputs"].items())
        c += g["hours"] * g.get("tool", 0) * f.tool_cost
        return c / (1 - RENT) if f.landlord else c

    def lay_in(self, f, plan, tool_hours):
        g = GOODS[f.good]
        f.short_inputs = False
        for i, q in g["inputs"].items():
            want = plan * q * (7 if f.good in STAPLES else 4) - f.inputs[i] + f.mend_mats[i]
            f.mend_mats[i] *= 0.8
            got, spent = self.buy(f.id, i, want, self.L.cash[f.id] * 0.5, f.place,
                                  asks=max(0.0, plan * q - f.inputs[i]))
            if got > 0:
                have = max(0.0, f.inputs[i])
                f.input_cost[i] = (have * f.input_cost[i] + spent) / (have + got)
                f.inputs[i] = have + got
            if f.inputs[i] < plan * q:
                f.short_inputs = True
        if g.get("tool"):
            per_day = tool_hours * g["tool"]
            want = per_day * 10 - f.tools
            got, spent = self.buy(f.id, "tools", want, self.L.cash[f.id] * 0.2, f.place,
                                  asks=max(0.0, per_day - f.tools))
            if got > 0:
                f.tool_cost = (max(0.0, f.tools) * f.tool_cost + spent) / (max(0.0, f.tools) + got)
                f.tools = max(0.0, f.tools) + got

    def ahead(self, f):
        """Woodcutters lay in for winter from a month before it, and keep a winter's extra burning on hand."""
        if f.good == "wood" and (WINTER[0] - MONTH <= season_day(self.day) <= WINTER[1]):
            return 1.6
        return 1.0

    def firm_day(self, f):
        g = GOODS[f.good]
        self.repay(f)
        if g["kind"] == "farm":
            return self.farm_day(f)
        mended, f.booked = f.booked, 0.0               # Yesterday's mending comes out of today's hours.
        cap = max(0.0, len(f.workers) * HOURS - mended) / g["hours"]
        if g.get("tool") and f.tools <= 0:
            cap *= 0.5                                # No tools: half the work done.
        plan = min(cap, f.daily_sales() * (STAPLE_SLACK if f.good in STAPLES else 1.1) + 0.2)
        self.lay_in(f, plan, plan * g["hours"])
        n = cap
        for i, q in g["inputs"].items():
            n = min(n, f.inputs[i] / q)
        n = max(0.0, min(n, f.daily_sales() * self.ahead(f) * f.keep - f.stock + 0.5))
        used = 0.0
        for i, q in g["inputs"].items():
            f.inputs[i] -= n * q
            used += n * q * f.input_cost[i]
        if g.get("tool"):
            wear = n * g["hours"] * g["tool"]
            used += min(wear, f.tools) * f.tool_cost
            f.tools = max(0.0, f.tools - wear)
        f.stock += n
        f.made.append(n)
        if f.good == "wool":                              # The flocks give hides too (beasts culled, sheepskins).
            f.hides = getattr(f, "hides", 0.0) + n * SHEEP_HIDES
        share = 1.0
        if g["skill"] >= 30 and f.workers:                # An artisan works to order: paid for the hours worked.
            share = min(1.0, max(ARTISAN_FLOOR, (n * g["hours"] + mended) / (len(f.workers) * HOURS)))
        f.costs.append(self.pay_wages(f, f.id, share) + used)
        f.unit_cost = self.normal_cost(f)


    def farm_day(self, f):
        """Sow in spring as many acres as the year's sales need; harvest at summer's end what was sown; sell from the
        store all year. Hands are needed at sowing and harvest; between, a farm keeps one."""
        g = GOODS["wheat"]
        work = len(f.workers) * HOURS
        sowing, harvest = in_window(self.day, SPRING), in_window(self.day, HARVEST)
        if work and (sowing or harvest):
            self.lay_in(f, 0, work)
        if g.get("tool") and f.tools <= 0:
            work *= 0.5
        if sowing and f.sown < f.acres:
            done = min(f.acres - f.sown, work / g["sow_hours"])
            f.sown += done
            f.to_harvest += done
            f.tools = max(0.0, f.tools - done * g["sow_hours"] * g["tool"])
        elif harvest and f.to_harvest > 0:
            done = min(f.to_harvest, work / g["harvest_hours"])
            f.to_harvest -= done
            f.stock += done * FARM["yield_per_acre"]
            f.tools = max(0.0, f.tools - done * g["harvest_hours"] * g["tool"])
        d = season_day(self.day)
        if d == HARVEST[1] + 1:
            f.to_harvest = 0.0                         # What wasn't brought in rots in the field.
        if d == SPRING[0]:
            # Sow for a year of sales (sold and asked for) and a margin, less what the store will still hold when the
            # harvest comes in.
            # The harvest must last until the next one is in (a year and the harvest's length), through a year of
            # the store's spoiling, with a tenth over.
            daily = max(f.year_sales / YEAR, f.daily_sales())   # The year's rate, or the last month's if higher.
            carry = max(0.0, f.stock - daily * HARVEST[0])
            need = daily * (YEAR + HARVEST[1] - HARVEST[0]) / (1 - GRAIN_SPOILS * YEAR) * 1.1
            # A store that ran dry before the harvest last year: what was bought undercounts what was wanted. Sow
            # more, up to half again for a month or more without grain.
            need *= 1 + 0.5 * min(1.0, f.dry_days / MONTH)
            f.dry_days = 0
            f.acres = max(0.0, need - carry) / FARM["yield_per_acre"]
            f.sown = 0.0
            f.year_sales = 0.0
        f.stock *= 1 - GRAIN_SPOILS
        if f.stock < f.daily_sales():
            f.dry_days += 1
        f.costs.append(self.pay_wages(f, f.id))
        f.unit_cost = (g["hours"] * f.wage / HOURS + g["hours"] * g["tool"] * f.tool_cost) / (1 - RENT)

    def farm_hands_wanted(self, f):
        g = GOODS["wheat"]
        d = season_day(self.day)
        if SPRING[0] <= d < SPRING[1]:
            left, days = (f.acres - f.sown) * g["sow_hours"], SPRING[1] - d
        elif HARVEST[0] - 7 <= d < HARVEST[1]:
            left, days = f.to_harvest * g["harvest_hours"], HARVEST[1] - max(d, HARVEST[0])
        else:
            return 1
        return max(1, math.ceil(left / (HOURS * max(days * 0.7, 3))))

    def carter_day(self):
        c = self.carter
        c.costs.append(self.pay_wages(c, c.id))
        c.sales.append(c.today_sold + c.today_unmet)
        if c.today_unmet > 0:                        # Loads turned away today: hands wanted now, not at the week's end.
            needed = math.ceil((c.today_sold + c.today_unmet) * CARRY_PER_WEIGHT / HOURS * 1.2)
            c.vacancies = max(c.vacancies, needed - len(c.workers))
        c.unit_cost = CARRY_PER_WEIGHT * c.wage / HOURS
        c.today_sold = 0.0

    # ---- weekly and monthly reviews -------------------------------------------------------------------------------
    def firm_week(self, f):
        rng = self.rng
        sales = f.daily_sales()
        if f is self.carter:                         # Asked to carry more than its carts hold: a short shelf.
            room = len(f.workers) * HOURS / CARRY_PER_WEIGHT
            use = sales * STAPLE_SLACK / room if room else 2.0          # Carting keeps a fifth spare, like staples.
            days, low, high = (1.0 if use > 0.95 else 5.0 if use < 0.5 else 3.0), 2.0, 4.0
            hours = CARRY_PER_WEIGHT
        elif GOODS[f.good]["kind"] == "farm":
            to_go = (HARVEST[1] - season_day(self.day)) % YEAR or YEAR
            days, low, high = f.stock / max(sales, 0.01), to_go * 0.9, to_go * 1.3
            hours = 0
        else:
            days = f.stock / sales if sales > 0.01 else 99
            low, high = f.keep * 0.3, f.keep * 1.7     # Measured after the day's sales: a full shelf is keep - 1 days.
            hours = GOODS[f.good]["hours"]
        if rng.random() < 0.75:
            if days < low:
                f.markup += rng.uniform(0, FIRM["step"])
            elif days > high:
                f.markup -= rng.uniform(0, FIRM["step"])
            else:                                     # Neither short nor glutted: competition wears the margin back.
                f.markup += (MARGIN - f.markup) * rng.uniform(0, 0.25)
            top = FIRM["staple_markup"] if f.good in STAPLES else FIRM["markup"][1]
            f.markup = min(max(f.markup, FIRM["markup"][0]), top)
        if hours == 0:
            desired = self.farm_hands_wanted(f)
            f.seasonal = desired > 1
            if f.seasonal:                              # Sowing and harvest pay a quarter over the market.
                f.wage = self.market_wage() * 1.25
            else:                                       # Between, the farm's hand is paid the going rate.
                f.wage = self.market_wage()
        else:
            output = sales * (STAPLE_SLACK if f.good in STAPLES else 1.0) * self.ahead(f)   # Room for a busy day.
            if days < low and not f.short_inputs:
                output *= 1 + 0.5 * (low - days) / low
            elif days > high:
                output *= 0.8
            # Hands for the mending it did, and one more if it turned mending away this week.
            mending = sum(f.mending) / MONTH + (HOURS if f.mend_turned > HOURS else 0.0)
            f.mend_turned = 0.0
            if f.short_inputs and GOODS[f.good]["inputs"]:
                # Short of materials: only the hands its materials keep busy (what it has made lately, and what's
                # in its store for the week), not the hands its buyers' wants would call for.
                feasible = max(sum(f.made) / 14, min(f.inputs[i] / q for i, q in GOODS[f.good]["inputs"].items()) / 7)
                output = min(output, feasible * 1.1)
            desired = max(1, math.ceil((output * hours + mending) / HOURS))
            desired = min(desired, getattr(f, "max_hands", desired))        # A keep's workshop holds so many.
        self.wage_week(f)
        if f.good == "leather":                           # A tannery offers more for hides when it's short of them.
            short = f.inputs["hide"] < f.daily_sales() * 3
            f.offer *= 1.05 if short else 0.97
            f.offer = min(max(f.offer, 0.2 * f.price), 0.9 * f.price)
        # A masters' trade with no hand at its level and no one heading it appoints its best-trained heir (the user's
        # succession rule): it doesn't wait for a master who isn't there.
        if f.skill >= 50 and not any(x.skill >= f.skill and not x.apprentice for x in f.workers) \
                and getattr(f, "head", None) not in f.workers:
            self.succeed(f, None)
        # Its posts are counted against its full hands: apprentices are hands over and above, so a master's trade
        # keeps a post open for a master until it has one.
        hands = [x for x in f.workers if not x.apprentice]
        f.vacancies = max(0, min(desired, 2 * len(hands) + 2) - len(hands))   # Grows by steps, not leaps.
        excess = len(hands) - desired
        if excess > 0:
            for w in rng.sample(hands, max(1, excess // 2) if hours else excess):
                self.fire(f, w)
        if f.landlord:
            self.income(f.landlord, self.L.pay(f.id, f.landlord, f.week_takings * RENT, "rent"))
        f.week_takings = 0.0
        self.owner_tops_up(f)

    def working_money(self, f):
        """A month of running at its staffing: the hands it has and at most two posts it's offering (posts it can't
        fill aren't wages it pays: counting them had a short-handed trade sitting on a fortune)."""
        staff = (len(f.workers) + min(f.vacancies, 2)) * f.wage
        if f is self.carter:
            return MONTH * staff
        g = GOODS[f.good]
        if g["kind"] == "farm":                          # A farm carries a year's sowing and harvest wages.
            return f.acres * (g["sow_hours"] + g["harvest_hours"]) / HOURS * f.wage * 1.3 + MONTH * staff
        per_unit = sum(q * f.input_cost[i] for i, q in g["inputs"].items()) + g["hours"] * g.get("tool", 0) * f.tool_cost
        return MONTH * (staff + f.daily_sales() * per_unit)

    def firm_month(self, f):
        spare = self.L.cash[f.id] - FIRM["buffer"] * self.working_money(f)
        if spare > 0:
            paid = self.L.pay(f.id, f.owner, spare, "profit")
            self.income(f.owner, paid)
            if hasattr(f, "put_in"):                     # What came back counts against what was put in.
                f.put_in[-1] -= paid

    def owner_tops_up(self, f):
        """A week's working money from the owner for a business short of it; but an owner that has put in more than
        two months' working money over the last quarter without its coming back stops: the business shrinks to what
        it can pay for."""
        f.put_in = getattr(f, "put_in", collections.deque([0.0] * 13, 13))
        f.put_in.append(0.0)
        short = self.working_money(f) / 4 - self.L.cash[f.id]
        if short <= 0 or sum(f.put_in) > 2 * self.working_money(f) / FIRM["buffer"]:
            return
        o = f.owner
        floor = 7 * self.need_cost(self.by_id[o]) if o in self.by_id else 0.0
        f.put_in[-1] += self.L.pay(o, f.id, min(short, max(0.0, self.L.cash[o] - floor)), "invested")

    def income(self, who, amount):
        if who in self.houses:
            self.inst_income[who][-1] += amount
            self.houses[who]["week"] += amount
        elif who in self.by_id:
            h = self.by_id[who]
            h.income[-1] += amount
            h.week_income += amount

    # ---- households -----------------------------------------------------------------------------------------------
    def price_of(self, good):
        fs = self.firms[good]
        if not fs:                                      # Hides: what the tanneries offer for them.
            offers = [getattr(f, "offer", 0.0) for f in self.firms["leather"]]
            return sum(offers) / max(1, len(offers))
        return sum(f.price for f in fs) / len(fs)

    def need_cost(self, h):
        return h.persons * BREAD_A_DAY * self.price_of("bread") + WOOD_A_DAY * self.price_of("wood")

    def household_day(self, h):
        L = self.L
        need = h.persons * BREAD_A_DAY
        eaten = min(h.larder, need)                    # What's in the larder (given bread, hunted meat) first.
        h.larder = (h.larder - eaten) * 0.5            # Half of what's left keeps another day.
        got, _ = self.buy(h.id, "bread", need - eaten, L.cash[h.id], "any")
        h.hungry = max(0, round((need - eaten - got) / BREAD_A_DAY))
        wood = WOOD_A_DAY * (1.6 if in_window(self.day, WINTER) else 1.0)
        self.buy(h.id, "wood", wood, L.cash[h.id], "any")
        h.clothes.cond = max(0.0, h.clothes.cond - 1 / WEAR["cloth"])
        if h.clothes.cond < MEND_AT:                     # Clothes are a need: seen to from what the household has.
            self.see_to(h.id, h.clothes, max(0.0, L.cash[h.id] - self.need_cost(h) * 8))
        for piece in h.owned:
            piece.cond = max(0.0, piece.cond - 1 / WEAR[piece.good])
        # The one rule: a share of what's held above the target, half towards things (finery; a sword; a rapier for
        # the rich) and half on help about the home.
        target = RULE["household"]["k"] * h.month_income() + 10 * self.need_cost(h)   # A week and more of food kept.
        spare = (L.cash[h.id] - target) * h.a2 / MONTH
        if spare <= 0:
            h.want_fund = min(h.want_fund, max(0.0, L.cash[h.id] - target))
            return
        h.want_fund += spare * 0.5
        h.help_budget = h.help_budget * 0.9 + spare * 0.5
        self.wants(h)

    def wants(self, h):
        """What the want fund goes on: seeing to a worn sword, rapier or finery first; then a sword if it has none and
        works, a rapier if it's rich, finery up to a piece a head (two over, if rich)."""
        L, rng = self.L, self.rng
        rich = h.month_income() >= self.rich_income and L.cash[h.id] > 2 * self.price_of("rapier")
        for piece in h.owned:
            if piece.cond < MEND_AT and h.want_fund > 0:
                h.want_fund -= self.see_to(h.id, piece, h.want_fund, rich)
                return
        has = collections.Counter(p.good for p in h.owned)
        for good, wanted in (("rapier", rich and not has["rapier"]),
                             ("sword", h.workers and not has["sword"] and not has["rapier"]),
                             ("finery", has["finery"] < h.persons + (2 if rich else 0))):
            if wanted and h.want_fund >= self.price_of(good):
                got, spent = self.buy(h.id, good, 1, h.want_fund, "any", asks=0.05)
                h.want_fund -= spent
                if got >= 0.99:
                    h.owned.append(Piece(good, 1.0, rng.random() < REPAIR[good]["heirloom"]))
                return
        # Nothing it wants: work done on the home (a new roof, a room, whitewash), as much as the fund runs to, which
        # always finds builders; and a household with a great deal keeps servants of its own (below).
        if h.want_fund >= self.price_of("works"):
            n = h.want_fund * 0.5 / self.price_of("works")
            _, spent = self.buy(h.id, "works", n, h.want_fund * 0.5, "any", asks=min(n, 1.0))
            h.want_fund -= spent

    def mend_quote(self, f, piece):
        g, r = GOODS[piece.good], REPAIR[piece.good]
        work = r["hours"] * g["hours"] * f.wage / HOURS
        mats = r["mat"] * sum(q * f.input_cost[i] for i, q in g["inputs"].items())
        return (1 - piece.cond) * piece.count * (work + mats) * (1 + max(f.markup, 0.0))

    def mend(self, owner, piece, budget):
        """Take a worn piece to the cheapest of a few makers with hands to spare and the materials; True if mended.
        A maker too busy or short of materials hears it was asked (and takes on hands for it)."""
        g, r = GOODS[piece.good], REPAIR[piece.good]
        wear = (1 - piece.cond) * piece.count
        hours = wear * r["hours"] * g["hours"]
        fs = [f for f in self.firms[piece.good] if f.workers]
        if not fs:
            return False
        look = sorted(self.rng.sample(fs, min(3, len(fs))), key=lambda f: self.mend_quote(f, piece))
        mats = {i: wear * r["mat"] * q for i, q in g["inputs"].items()}
        busy = no_mats = 0
        for f in look:
            if f.booked + hours > len(f.workers) * HOURS * 0.5:
                busy += 1
                continue
            if any(f.inputs[i] < m for i, m in mats.items()):
                no_mats += 1
                for i, m in mats.items():             # It lays in the materials for next time.
                    f.mend_mats[i] += m
                continue
            price = self.mend_quote(f, piece)
            if price > budget or self.L.cash[owner] < price:
                return False
            paid = self.L.pay(owner, f.id, price, f"mends {piece.good}")
            f.week_takings += paid
            for i, m in mats.items():
                f.inputs[i] -= m
            f.booked += hours
            f.mending[-1] += hours
            for i, m in mats.items():
                f.mend_mats[i] = max(0.0, f.mend_mats[i] - m)
            piece.cond = 1.0
            return True
        # Turned away for want of hands: the makers hear it (once a week for a piece waiting), and take hands on.
        if busy and self.day - piece.asked >= 7:
            piece.asked = self.day
            for f in look:
                f.mend_turned += hours / len(look)
        return False

    def see_to(self, owner, piece, budget, rich=False):
        """A worn piece: mend it, or buy a new one (or something better). Returns what was spent."""
        L, rng, r = self.L, self.rng, REPAIR[piece.good]
        before = L.cash[owner]
        if not piece.heirloom:
            better = r.get("better")
            if better and rich and rng.random() < 0.3 and budget >= self.price_of(better):
                got, _ = self.buy(owner, better, 1, budget, "any", asks=0.02)
                if got >= 0.99:
                    piece.good, piece.cond, piece.heirloom = better, 1.0, rng.random() < REPAIR[better]["heirloom"]
                    return before - L.cash[owner]
            want_new = rng.random() < r["fashion"]
        else:
            want_new = False
        new = self.price_of(piece.good) * piece.count
        fs = [f for f in self.firms[piece.good] if f.workers]
        quote = min((self.mend_quote(f, piece) for f in fs), default=new)
        if not want_new and (piece.heirloom or quote < MEND_IF * new):
            if self.mend(owner, piece, budget):
                return before - L.cash[owner]
        got, _ = self.buy(owner, piece.good, piece.count, budget, "any", asks=piece.count * 0.2)
        if got >= piece.count - 0.01:
            piece.cond = 1.0
        elif got > 0:
            piece.cond = min(1.0, piece.cond + got / piece.count)
        return before - L.cash[owner]

    def leftovers(self):
        """At the day's end a baker gives what would spoil overnight to the households that need it: the hungry
        first, then those with less than a few days' food money. Goods, not money: nothing is paid."""
        hungry = sorted((h for h in self.households if h.hungry > 0), key=lambda h: -h.hungry)
        poor = [h for h in self.households if not h.hungry and self.L.cash[h.id] < self.need_cost(h) * 3]
        queue = hungry + poor
        for good, g in GOODS.items():
            if not g.get("perish"):
                continue
            for f in self.firms[good]:
                spoil = f.stock * g["perish"]
                f.stock -= spoil
                while spoil > 1e-6 and queue:
                    h = queue[0]
                    room = max(0.0, h.persons * BREAD_A_DAY - h.larder)
                    give = min(room, spoil)
                    h.larder += give
                    spoil -= give
                    self.given_bread += give
                    if h.larder >= h.persons * BREAD_A_DAY - 1e-6:
                        queue.pop(0)

    # ---- lives ------------------------------------------------------------------------------------------------------
    def demography(self):
        """A day of lives: every wolf ages; children come of age (into a family member's trade, as its apprentice,
        where there's a better hand to learn from); workers retire; some die, by age; births follow deaths."""
        rng = self.rng
        for h in list(self.households):
            for pp in list(h.members):
                pp.age += 1
                years = pp.age / YEAR
                if pp.worker is None and not pp.retired and years >= LIFE["adult"]:
                    self.come_of_age(pp)
                elif pp.worker is not None and pp.age >= pp.retire_at:
                    self.leave_work(pp)
                    pp.retired = True
                yearly = LIFE["a"] * math.exp(LIFE["b"] * years) + (LIFE["infant"] if years < 5 else 0.0)
                if rng.random() < yearly / YEAR:
                    self.die(pp)
        for due in [d for d in self.births_due if d <= self.day]:
            self.births_due.remove(due)
            self.birth()
        # A realm under its number (new homes built: a keep's houses) grows into it, a birth every few days.
        if self.pop_target and self.people() + len(self.births_due) < self.pop_target and self.rng.random() < 0.3:
            self.births_due.append(self.day + self.rng.randint(*LIFE["birth_after"]))
        if self.day % 7 == 3:
            for h in list(self.households):
                if h.persons > LIFE["crowded"]:
                    self.leave_home(h)

    def people(self):
        return sum(h.persons for h in self.households)

    def come_of_age(self, pp):
        h = pp.household
        w = Worker(self.next_wid, h, self.rng.uniform(0, 5), self.market_wage())
        self.next_wid += 1
        w.person, pp.worker = pp, w
        h.workers.append(w)
        self.workers.append(w)
        # An apprenticeship in the family's trade: taken on beside the best hand there, at half pay.
        trades = [x.employer for x in h.workers if x is not w and isinstance(x.employer, Firm) and x.employer.skill > 0
                  and self.apprentice_places(x.employer) > 0]
        if trades:
            e = max(trades, key=lambda e: max(y.skill for y in e.workers))
            e.vacancies += 1
            self.hire(e, w)
            w.apprentice = True

    def leave_work(self, pp):
        w = pp.worker
        e = w.employer
        if e is not None:
            e.workers.remove(w)
            e.vacancies += 1                          # Its post is open for the next.
            w.employer = None
        self.workers.remove(w)
        pp.household.workers.remove(w)
        pp.worker = None
        if isinstance(e, Firm) and e.skill >= 30 and not any(x.skill >= e.skill and not x.apprentice for x in e.workers):
            self.succeed(e, pp.household)

    def important(self, w):
        """A wolf in a master's place or an important position: a qualified hand in a skilled trade, a keeper working
        in its own shop, the clergy."""
        e = w.employer
        if isinstance(e, Firm):
            if e.skill >= 30 and w.skill >= e.skill and not w.apprentice:
                return True
            if e.owner == w.household.id:
                return True
        return e is not None and e.id == "svc:clergy"

    def succeed(self, f, old_household):
        """A master's trade left with no hand at its level: its best apprentice steps up; with none, the best-trained
        wolf in the realm not already in a master's place or an important position takes it. The successor inherits
        the workshop (unless a great House owns it) and works toward mastership at the bench."""
        apprentices = [x for x in f.workers if x.apprentice]
        if apprentices:
            heir = max(apprentices, key=lambda x: x.skill)
            heir.apprentice = False
        else:
            pool = [x for x in self.workers if x.employer is not f and not x.apprentice and not self.important(x)]
            if not pool:
                return
            heir = max(pool, key=lambda x: (x.skill, -x.person.age))
            if heir.employer is not None:
                heir.employer.vacancies += 1
            f.vacancies += 1
            self.hire(f, heir)
        f.head = heir
        if f.owner not in self.houses:                  # A House keeps its shop; anyone else's passes to the heir.
            f.owner = heir.household.id
        self.successions += 1

    def die(self, pp):
        h = pp.household
        if pp.worker is not None:
            self.leave_work(pp)
        h.members.remove(pp)
        self.deaths += 1
        if self.people() + len(self.births_due) < self.pop_target:     # Under the realm's number: a birth to follow.
            self.births_due.append(self.day + self.rng.randint(*LIFE["birth_after"]))
        if not h.members:
            self.die_out(h)

    def die_out(self, h):
        """The last of a household gone: its money and goods to its kin (else the town); a business it kept to its
        best hand (the apprentice, the journeyman), else to its kin."""
        kin = [self.by_id[k] for k in sorted(h.kin) if k in self.by_id and self.by_id[k] is not h]   # Sorted: repeatable.
        heir = kin[0] if kin else None
        self.L.pay(h.id, heir.id if heir else "treasury", self.L.cash[h.id], "estate")
        if heir:
            heir.owned.extend(h.owned)
            heir.kin |= h.kin - {heir.id}
        for f in self.all_firms():
            if f.owner == h.id:
                hands = [x for x in f.workers if x.household.id in self.by_id and x.household is not h]
                f.owner = max(hands, key=lambda x: x.skill).household.id if hands else \
                    (heir.id if heir else self.rng.choice(self.households).id)
        e = self.servant_posts.pop(h.id, None)           # Its servants let go.
        if e is not None:
            for x in list(e.workers):
                self.fire(e, x)
            self.services.pop(f"servants:{h.id}", None)
        self.households.remove(h)
        del self.by_id[h.id]
        for other in self.households:
            other.kin.discard(h.id)

    def birth(self):
        """A child born into a young family: one with a wolf of 18-40, the fewer children the likelier."""
        rng = self.rng
        young = [h for h in self.households if any(18 * YEAR <= m.age <= 40 * YEAR for m in h.members)]
        if not young:
            return
        weights = [1.0 / (1 + sum(1 for m in h.members if m.age < LIFE["adult"] * YEAR)) for h in young]
        h = rng.choices(young, weights)[0]
        h.members.append(Person(0, h, int(rng.uniform(*LIFE["retire"]) * YEAR)))
        self.births += 1

    def leave_home(self, h):
        """A crowded household: a young adult goes to found its own, with a share of the purse; kin, and heirs."""
        rng = self.rng
        young = [m for m in h.members if 18 * YEAR <= m.age <= 32 * YEAR]
        if not young:
            return
        pp = rng.choice(young)
        nh = Household(f"hh{self.next_hh}", rng.uniform(*RULE["household"]["a2"]), h.place)
        self.next_hh += 1
        self.L.open(nh.id, "household")
        self.L.pay(h.id, nh.id, self.L.cash[h.id] / h.persons, "leaves home")
        h.members.remove(pp)
        pp.household = nh
        nh.members.append(pp)
        if pp.worker is not None:
            h.workers.remove(pp.worker)
            pp.worker.household = nh
            nh.workers.append(pp.worker)
        nh.clothes = Piece("cloth", h.clothes.cond, count=1)
        nh.kin = {h.id} | set(sorted(h.kin)[:1])
        h.kin.add(nh.id)
        self.households.append(nh)
        self.by_id[nh.id] = nh

    def hunt(self):
        """The game breeds back; each wolf with no work today may hunt; its catch feeds its household and its hides go
        to the tanneries, at what each offers, as far as each wants them (a week of its making)."""
        G, rng, L = GAME, self.rng, self.L
        self.game += G["r"] * self.game * (1 - self.game / G["K"])
        hunters = [w for w in self.workers if w.employer is None and w.id not in self.helped_ids] if G["on"] else []
        rng.shuffle(hunters)
        self.hunted = 0
        for w in hunters:
            if self.game >= 1 and rng.random() < G["catch"] * self.game / G["K"]:
                self.game -= 1
                self.hunted += 1
                h = w.household
                h.larder += G["meat"]
                h.hides += 1
        tanners = [f for f in self.firms["leather"] if f.workers]
        for h in self.firms["wool"] + self.households:         # The flocks' hides, and the hunters'.
            if not hasattr(h, "hides"):
                continue
            while h.hides >= 1 and tanners:
                f = max(tanners, key=lambda f: f.offer)
                if f.inputs["hide"] > f.daily_sales() * 7 + 2 or L.cash[f.id] < f.offer:
                    tanners.remove(f)
                    continue
                p = L.pay(f.id, h.id, f.offer, "hides")
                if isinstance(h, Household):
                    h.income[-1] += p
                    h.week_income += p
                else:
                    h.week_takings += p
                h.hides -= 1
                have = max(0.0, f.inputs["hide"])
                f.input_cost["hide"] = (have * f.input_cost["hide"] + p) / (have + 1)
                f.inputs["hide"] = have + 1
                self.hides_sold += 1

    def daily_relief(self):
        """Relief by the day: a day's food and fuel to each household short of it, from what the church set aside at
        its week's reckoning, then from what the town did."""
        L = self.L
        short = [h for h in self.households if h.hungry > 0 or L.cash[h.id] < self.need_cost(h)]
        for h in short:
            want = self.need_cost(h) - min(h.larder, h.persons * BREAD_A_DAY) * self.price_of("bread")
            for payer in ("church", "treasury"):
                left = self.relief_left[payer]
                if want <= 0 or left <= 0:
                    continue
                paid = L.pay(payer, h.id, min(want, left), "alms" if payer == "church" else "relief")
                self.relief_left[payer] -= paid
                self.relief_week[payer] = self.relief_week.get(payer, 0.0) + paid
                want -= paid

    def servants(self):
        """A household holding more than half a year of its own income (an inheritance, a windfall) keeps servants:
        posts of its own at the going rate, as many as a tenth of its excess pays for a month. They go when it can't."""
        for h in self.households:
            e = self.servant_posts.get(h.id)
            excess = self.L.cash[h.id] - 6 * max(h.month_income(), 7 * self.need_cost(h))
            if e is None:
                if excess <= 0:
                    continue
                e = self.servant_posts[h.id] = Employer(f"svc:servants:{h.id}", self.market_wage())
                self.services[f"servants:{h.id}"] = e
            e.wage = self.market_wage() * 0.9
            can = max(0, int(max(0.0, excess) * 0.1 / (e.wage * MONTH)))
            self.staff_to(e, can * e.wage * 7, can)

    def home_help(self):
        idle = [w for w in self.workers if w.employer is None]
        self.rng.shuffle(idle)
        pay = self.market_wage() * 0.8
        self.helped_today = 0
        self.helped_ids = set()
        for h in sorted(self.households, key=lambda h: -h.help_budget):
            while idle and h.help_budget >= pay and self.L.cash[h.id] >= pay:
                w = idle.pop()
                p = self.L.pay(h.id, w.household.id, pay, "home help")
                w.household.income[-1] += p
                w.household.week_income += p
                h.help_budget -= p
                self.helped_today += 1
                self.helped_ids.add(w.id)
            if not idle:
                break

    def taxes_week(self, who, week_income):
        if week_income > 0:
            self.inst_income["treasury"][-1] += self.L.pay(who, "treasury", week_income * TAX, "tax")
            self.inst_income["church"][-1] += self.L.pay(who, "church", week_income * TITHE, "tithe")

    # ---- institutions ---------------------------------------------------------------------------------------------
    def week_budget(self, who, rule):
        month_in = sum(self.inst_income[who])
        if self.day < MONTH:
            month_in = self.L.cash[who] / (RULE[rule]["k"] + 1)
        gap = self.L.cash[who] - RULE[rule]["k"] * month_in
        return max(0.0, min(self.L.cash[who], (month_in + RULE[rule]["a2"] * gap) / 4))

    def staff_to(self, e, budget, most):
        can = max(0, min(most, int(budget / max(e.wage * 7, 1e-9))))
        e.vacancies = max(0, can - len(e.workers))
        while len(e.workers) > can:
            self.fire(e, self.rng.choice(e.workers))
        return len(e.workers) * e.wage * 7

    def house_week(self, hid):
        h = self.houses[hid]
        ret = self.services[f"retainers:{hid}"]
        ret.wage = self.market_wage()
        self.taxes_week(hid, h["week"])
        h["week"] = 0.0
        budget = self.week_budget(hid, "house")
        budget -= self.staff_to(ret, budget * 0.3, 12)
        for piece in h["owned"]:
            piece.cond = max(0.0, piece.cond - 7 / WEAR[piece.good])
            if piece.cond < MEND_AT and budget > 0:
                budget -= self.see_to(hid, piece, budget)
        if budget > 0:
            got, spent = self.buy(hid, "finery", budget * 0.3 / self.price_of("finery"), budget * 0.3, "city")
            budget -= spent
            _, spent = self.buy(hid, "works", budget * 0.4 / self.price_of("works"), budget * 0.4, "city")
            budget -= spent
        # What's left keeps its estate's crew: building, hedging, the household's own works. Always to be had.
        estate = self.services[f"estate:{hid}"]
        estate.wage = self.market_wage()
        self.staff_to(estate, max(0.0, budget), len(self.workers) // 10)

    def church_week(self):
        clergy = self.services["clergy"]
        clergy.wage = self.market_wage() * 1.1
        budget = self.week_budget("church", "church")
        # Set aside for the week's daily relief: what it took last week and a third over (what isn't given stays in
        # the purse and comes round again in next week's budget).
        self.relief_left["church"] = min(budget, max(self.relief_week.get("church", 0.0) * 1.33, budget * 0.1))
        self.relief_week["church"] = 0.0
        budget -= self.relief_left["church"]
        budget -= self.staff_to(clergy, budget * 0.5, 6)
        if budget > 0:
            _, spent = self.buy("church", "works", budget * 0.5 / self.price_of("works"), budget * 0.5, "city")
            budget -= spent
        # The building fund's own masons take what's left: the church's money always buys work.
        fabric = self.services["fabric"]
        fabric.wage = self.market_wage()
        self.staff_to(fabric, max(0.0, budget), len(self.workers) // 10)

    def treasury_week(self):
        watch, works = self.services["watch"], self.services["works"]
        watch.wage = self.market_wage()
        works.wage = self.market_wage() * (0.85 if self.slack > 0.03 else 1.05)
        budget = self.week_budget("treasury", "treasury")
        # The parish keeps some by for what the church can't cover.
        self.relief_left["treasury"] = min(budget * 0.3, self.relief_week.get("treasury", 0.0) * 1.33 + budget * 0.05)
        self.relief_week["treasury"] = 0.0
        budget -= self.relief_left["treasury"]
        budget -= self.staff_to(watch, budget * 0.5, 16)
        arms = self.watch_swords
        for piece in arms:
            piece.cond = max(0.0, piece.cond - 7 / WATCH_WEAR)
            if piece.cond < MEND_AT and budget > 0:
                budget -= self.see_to("treasury", piece, budget)
        while len(arms) < len(watch.workers) and budget > self.price_of("sword"):
            got, spent = self.buy("treasury", "sword", 1, budget, "city", asks=0.2)
            budget -= spent
            if got < 0.99:
                break
            arms.append(Piece("sword"))
        budget -= self.staff_to(works, budget * 0.8, len(self.workers) // 5)
        if budget > 0:
            self.buy("treasury", "works", budget / self.price_of("works"), budget, "city")

    # ---- the day --------------------------------------------------------------------------------------------------
    def step(self):
        L = self.L
        if self.day == 0:
            self.inst_income = {w: collections.deque([0.0] * MONTH, MONTH) for w in [*self.houses, "church", "treasury"]}
        L.out_today.clear()
        self.demography()
        for h in self.households:
            h.income.append(0.0)
            h.clothes.count = h.persons
        for d in self.inst_income.values():
            d.append(0.0)
        for f in self.all_firms():
            f.today_sold = f.today_unmet = f.hauled = 0.0
            f.out_hit = False
        for w in self.workers:
            w.paid.append(0.0)
        self.carter_room = len(self.carter.workers) * HOURS / CARRY_PER_WEIGHT
        for good in GOODS:                                         # Upstream first.
            fs = list(self.firms[good])
            self.rng.shuffle(fs)
            for f in fs:
                self.firm_day(f)
        hs = list(self.households)
        self.rng.shuffle(hs)
        incomes = sorted(h.month_income() for h in hs)
        self.rich_income = incomes[int(len(incomes) * 0.85)]      # The top sixth of earners count as rich.
        for h in hs:
            self.household_day(h)
        self.daily_relief()
        self.leftovers()
        self.home_help()
        self.hunt()
        for f in self.all_firms():
            if f is not self.carter:
                f.sales.append(f.today_sold + f.today_unmet)
                f.year_sales += f.today_sold + f.today_unmet
                f.out_days += f.out_hit
        self.carter_day()
        for name, e in self.services.items():
            payer = "treasury" if name in ("watch", "works") else "church" if name in ("clergy", "fabric") \
                else name.split(":", 1)[1]
            self.pay_wages(e, payer)
        self.labour_market()
        self.slack = (sum(1 for w in self.workers if w.employer is None) - self.helped_today) / len(self.workers)
        wd = self.day % 7
        for f in self.all_firms():
            if f.offset == wd:
                self.firm_week(f)
            if (self.day - f.offset) % MONTH == 0 and self.day > 0:
                self.firm_month(f)
        for i, hid in enumerate(self.houses):
            if wd == (2 + i) % 7:
                self.house_week(hid)
        if wd == 5:
            self.servants()
        if wd == 6:
            for h in self.households:
                self.taxes_week(h.id, h.week_income)
                h.week_income = 0.0
            self.church_week()
        if wd == 0:
            self.treasury_week()
        assert abs(L.total() - self.money) < 1e-6 * self.money, "money was made or lost"
        self.measure()
        for hook in self.hooks:
            hook(self)
        self.day += 1

    # ---- measures -------------------------------------------------------------------------------------------------
    def measure(self):
        L = self.L
        if self.day % MONTH == MONTH - 1:
            self.month_flows = dict(L.flows)
            L.flows.clear()
            self.month_out = {g: sum(f.out_days for f in fs) for g, fs in self.firms.items()}
            for f in self.all_firms():
                f.out_days = 0
        by = collections.defaultdict(float)
        for who, c in L.cash.items():
            by[L.kind[who]] += c
        purses = sorted(L.cash[h.id] for h in self.households)
        wage = self.market_wage()
        self.log.append(dict(
            day=self.day, **{f"money_{k}": v for k, v in by.items()},
            gini=gini(purses), median=purses[len(purses) // 2],
            poorer_half=sum(purses[: len(purses) // 2]) / max(sum(purses), 1e-9),
            hungry=sum(h.hungry for h in self.households),
            idle=sum(1 for w in self.workers if w.employer is None and w.id not in self.helped_ids),
            wage=wage, bread_ld=self.price_of("bread") / wage, sword_ld=self.price_of("sword") / wage,
            rapier_ld=self.price_of("rapier") / wage, wheat_ld=self.price_of("wheat") / wage,
            farm_hands=sum(len(f.workers) for f in self.firms["wheat"]),
            game=self.game, hunted=self.hunted, hides_sold=self.hides_sold,
            people=sum(h.persons for h in self.households), homes=len(self.households),
            masters=sum(1 for x in self.workers if x.skill >= 60), apprentices=sum(x.apprentice for x in self.workers),
            elders=sum(1 for h in self.households for m in h.members if m.retired),
            births=self.births, deaths=self.deaths,
        ))


def gini(xs):
    xs = sorted(max(0.0, x) for x in xs)
    n, s = len(xs), sum(xs)
    if n == 0 or s == 0:
        return 0.0
    return sum((2 * i - n + 1) * x for i, x in enumerate(xs)) / (n * s)


def run(start="normal", days=3 * YEAR, seed=1, quiet=False, every=56, money=1_000_000):
    w = World(start, seed, money=money)
    for _ in range(days):
        w.step()
    if not quiet:
        report(w, every)
    return w


def report(w, every=56):
    heads = sum(h.persons for h in w.households)
    print(f"start: {w.start}; {len(w.households)} households, {heads} people, {len(w.workers)} workers, {w.money:,}p")
    print("hours for one (calibrated): " + ", ".join(
        f"{g} {GOODS[g]['hours']:.2f}" for g in GOODS if g != "wheat") +
        f"; a field of wheat: sow {GOODS['wheat']['sow_hours']:.1f}h, harvest {GOODS['wheat']['harvest_hours']:.1f}h,"
        f" yields {FARM['yield_per_acre']}")
    cols = [("day", "day"), ("money_household", "homes"), ("money_firm", "firms"), ("money_house", "Houses"),
            ("money_church", "church"), ("money_treasury", "town"), ("gini", "gini"), ("median", "median"),
            ("poorer_half", "poor½"), ("hungry", "hungry"), ("idle", "idle"), ("farm_hands", "farm"),
            ("wage", "wage"), ("bread_ld", "bread"), ("sword_ld", "sword"), ("rapier_ld", "rapier")]
    print(" ".join(f"{h:>7}" for _, h in cols) + "   (bread, sword, rapier: in a labourer's day-wages)")
    print("lives:  day  people homes births deaths masters apprentices elders")
    for r in w.log:
        if r["day"] % every == 0 or r["day"] == w.log[-1]["day"]:
            print(f"      {r['day']:5} {r['people']:6} {r['homes']:5} {r['births']:6} {r['deaths']:6} {r['masters']:7} "
                  f"{r['apprentices']:11} {r['elders']:6}")
    last = w.log[-YEAR:] if len(w.log) >= YEAR else w.log
    print(f"last year: hunted {sum(r['hunted'] for r in last):.0f} beasts, hides sold {w.hides_sold:.0f} in all, "
          f"game in the woods {w.game:.0f} of {GAME['K']}, bread given away {w.given_bread:.0f} loaves in all")
    for r in w.log:
        if r["day"] % every == 0 or r["day"] == w.log[-1]["day"]:
            out = []
            for c, _ in cols:
                v = r.get(c, 0)
                if c in ("day", "hungry", "idle", "farm_hands") or abs(v) >= 100:
                    out.append(f"{v:7.0f}")
                elif c == "bread_ld":
                    out.append(f"{v:7.3f}")
                else:
                    out.append(f"{v:7.2f}")
            print(" ".join(out))


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--start", default="normal", choices=["normal", "one_house", "empty_treasury", "equal"])
    ap.add_argument("--days", type=int, default=3 * YEAR)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--every", type=int, default=56)
    ap.add_argument("--no-poach", action="store_true", help="hands don't move to short trades for better pay")
    ap.add_argument("--no-hunt", action="store_true", help="nobody hunts")
    ap.add_argument("--money", type=int, default=1_000_000, help="pennies minted at the founding")
    a = ap.parse_args()
    POACH["on"] = not a.no_poach
    GAME["on"] = not a.no_hunt
    run(a.start, a.days, a.seed, every=a.every, money=a.money)
