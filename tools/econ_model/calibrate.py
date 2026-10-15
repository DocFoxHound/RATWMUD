"""Production times from target prices (doc 63): what each craft and producer's work should take so that, priced from
cost, every good lands on the worth ladder (Data/Economy/ladder.json; each item's own place, `worth`, in items.json).
apply_ladder.py writes the result into the game's catalogue (doc 64, Phase 2).

A good's built price is the work at its trade's wage, plus its inputs, plus carting, plus the tools worn, times the
seller's margin. Raw goods come first, then everything made from them, so each step sees its inputs' new costs. The
work a step needs is what's left of its target once its inputs are paid for; where the inputs alone cost more than the
target, the step is listed as a conflict (the target, the recipe or an input's target has to give).

    python3 tools/econ_model/calibrate.py [--show all|changed|conflicts] [--write proposed_rates.json]

Reads Data/Items (crafts.json: what NPCs make and gather; recipes.json: skills, and goods only players make). Writes
nothing to the game's data (apply_ladder.py does): --write puts a listing next to this script.
"""
import argparse
import json
import pathlib

from ladder import Ladder

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
DAY = 8                     # Paid hours in a labourer's day.
GAME_DAY_SECONDS = 86400    # Craft and producer `seconds` are game seconds.
PAID_SECONDS = DAY * 3600   # A paid day's work in game seconds.


def load():
    items = {i["id"]: i for i in json.load(open(ROOT / "Data/Items/items.json"))["items"]}
    crafts = json.load(open(ROOT / "Data/Items/crafts.json"))
    recipes = json.load(open(ROOT / "Data/Items/recipes.json"))["recipes"]
    return items, crafts, recipes, Ladder(items=items)


def resolve(name, items, target):
    """An @tag input stands for the cheapest item with that tag."""
    if not name.startswith("@"):
        return name
    tag = name[1:]
    tagged = [i for i, it in items.items() if tag in it.get("tags", []) or it.get("category") == tag]
    return min(tagged, key=lambda i: target(i)) if tagged else None


def ways(items, crafts, recipes):
    """Every way of making each good: dict(kind, id, seconds, inputs, outputs, skill, wear)."""
    skill = {r["id"]: r.get("skill", 0) for r in recipes}
    wear = {}
    for t in crafts["tools"]:
        wear.setdefault(t["producer"], {})[t["item"]] = 1.0 / t["everyDays"]
    out = []
    for p in crafts["producers"]:
        per_day = PAID_SECONDS / p["seconds"]
        out.append(dict(kind="producer", id=p["id"], seconds=p["seconds"], inputs={}, outputs=p["out"], skill=0,
                        wear={i: n / per_day for i, n in wear.get(p["id"], {}).items()}, seasons=p.get("seasons")))
    npc = set()
    for c in crafts["crafts"]:
        npc.update(c["out"])
        out.append(dict(kind="craft", id=c["id"], seconds=c["seconds"], inputs=c["in"], outputs=c["out"],
                        skill=skill.get(c["id"], 0), wear={}))
    for r in recipes:
        if not set(r["out"]) & npc:                          # Only players make it: their recipe is the way.
            out.append(dict(kind="recipe", id=r["id"], seconds=r["seconds"], inputs=r["in"], outputs=r["out"],
                            skill=r.get("skill", 0), wear={}))
    return out


def order(ws):
    """Raw goods first, then each good once all its inputs are placed (cycles broken where they close)."""
    makes = {}
    for w in ws:
        for o in w["outputs"]:
            makes.setdefault(o, []).append(w)
    placed, seq = set(), []
    pending = list(ws)
    for _ in range(len(ws) + 5):
        rest = []
        for w in pending:
            if all(i in placed or i.startswith("@") or i not in makes for i in w["inputs"]):
                seq.append(w)
                placed.update(w["outputs"])
            else:
                rest.append(w)
        if not rest or len(rest) == len(pending):
            seq.extend(rest)
            break
        pending = rest
    return seq


def calibrate(items, crafts, recipes, ladder):
    m, carry, sk = ladder["markup"], ladder["carryPerWeight"], ladder["skillWage"]

    target = ladder.worth

    cost = {}                                   # Built price in labourer's hours, under the proposed times.
    now = {}                                    # The same under today's times.
    rows = []
    ws = order(ways(items, crafts, recipes))
    for passno in range(3):                     # A few passes settle the tools (picks wear in the mines that dig iron).
        rows = []
        for w in ws:
            ins = {}
            for i, q in w["inputs"].items():
                r = resolve(i, items, target)
                if r:
                    ins[r] = ins.get(r, 0) + q
            factor = 1 + w["skill"] / sk

            def price_of(g, table):
                return table.get(g, target(g) * DAY)

            def inputs_cost(table):
                c = sum(q * (price_of(i, table) + carry * items.get(i, {}).get("weight", 1)) for i, q in ins.items())
                return c + sum(q * price_of(i, table) for i, q in w["wear"].items())

            want = sum(n * target(o) * DAY for o, n in w["outputs"].items())        # The batch at its targets.
            work_now = w["seconds"] / 3600 * factor
            built_now = (work_now + inputs_cost(now)) * (1 + m)
            work = want / (1 + m) - inputs_cost(cost)
            conflict = work < 0.15 * want / (1 + m)
            if conflict:
                # The inputs leave too little for the work (under 15% of the price): give the work the usual share
                # of a made good's price (a third), and let the price rise to cover it.
                work = max(work, 0.0) + 0.33 * want / (1 + m)
            seconds = work / factor * 3600
            for o, n in w["outputs"].items():
                share = n * target(o) / max(sum(k * target(x) for x, k in w["outputs"].items()), 1e-9)
                built = (work + inputs_cost(cost)) * (1 + m)
                if o not in cost or passno or w["kind"] != "recipe":
                    cost[o] = built * share / n
                now.setdefault(o, built_now * share / n)
            rows.append(dict(w, proposed=seconds, conflict=conflict, target=want / DAY,
                             built=(work + inputs_cost(cost)) * (1 + m) / DAY, built_now=built_now / DAY,
                             factor=factor))

    return rows, cost, now, target


def budget(items, crafts, cost, now, target):
    """Labourer's hours a head a day for food and the household's needs, today and proposed."""
    hh = crafts["households"]
    def basket(table):
        food = 2 * table.get("bread", target("bread") * DAY)
        home = 0.0
        for need in hh["needs"]:
            g = need["any"][0]
            per = 1 if need.get("perPerson") else 1 / 3.4                    # A household of 3.4 heads.
            home += table.get(g, target(g) * DAY) / need["everyDays"] * per
        return food, home
    return basket(cost), basket(now)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--show", default="key", choices=["key", "all", "changed", "conflicts"])
    ap.add_argument("--write")
    a = ap.parse_args()
    items, crafts, recipes, ladder = load()
    rows, cost, now, target = calibrate(items, crafts, recipes, ladder)
    key = {"wheat", "flour", "bread", "firewood", "timber", "iron_ore", "iron_bar", "copper_ore", "bronze_bar",
           "steel_bar", "sword", "iron_sword", "steel_sword", "rapier", "nails", "planks", "wool_scarf", "mining_pick",
           "charcoal", "mutton", "cheese", "candle_tallow"}
    print(f"{'way':22} {'kind':8} {'makes':28} {'target LD':>9} {'now LD':>8} {'built LD':>8} {'secs now':>8} {'proposed':>8}  x")
    for r in rows:
        outs = ",".join(r["outputs"])
        if a.show == "key" and not set(r["outputs"]) & key:
            continue
        if a.show == "changed" and abs(r["proposed"] / r["seconds"] - 1) < 0.25:
            continue
        if a.show == "conflicts" and not r["conflict"]:
            continue
        print(f"{r['id']:22} {r['kind']:8} {outs[:28]:28} {r['target']:9.3f} {r['built_now']:8.3f} {r['built']:8.3f} "
              f"{r['seconds']:8.0f} {r['proposed']:8.0f} {r['proposed'] / r['seconds']:5.1f}{'  CONFLICT' if r['conflict'] else ''}")
    (food, home), (food_now, home_now) = budget(items, crafts, cost, now, target)
    print(f"\nA head's day of bread and household needs: {food + home:.2f} labourer's hours proposed "
          f"({food:.2f} bread, {home:.2f} home), {food_now + home_now:.2f} today.")
    print(f"Conflicts (inputs alone over target; their price rises to 'built'): {sum(r['conflict'] for r in rows)} of {len(rows)} ways.")
    if a.write:
        out = {r["id"]: dict(kind=r["kind"], seconds=round(r["proposed"]), was=r["seconds"]) for r in rows}
        (HERE / a.write).write_text(json.dumps(out, indent=1))
        print("wrote", HERE / a.write)


if __name__ == "__main__":
    main()
