"""What every good costs to make (doc 63): labour, materials, tools worn, from the live catalogue.

A good's cost is the work that went into it at a wage, plus the cost of what it was made from, plus the share of the
tools its maker wore out. Goods made together (a sheep farm's mutton, wool and sheepskin) share the batch's cost in
proportion to their catalogue prices. Tools are made of goods that need tools, so the costs are found by repeating
until they stop changing.

    python3 tools/econ_model/costs.py [--wage 12] [--markup 0.2] [--all]

--wage: a day's wage (8 paid hours, doc 42), --markup: each seller's margin over cost, added at every step.
"""
import argparse
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
DAY_SECONDS = 8 * 3600        # Paid work a day: PaidSpells game hours.


def load():
    items = {i["id"]: i for i in json.load(open(ROOT / "Data/Items/items.json"))["items"]}
    crafts = json.load(open(ROOT / "Data/Items/crafts.json"))
    return items, crafts


def ways(crafts):
    """Every way of making goods: (id, seconds, inputs, outputs, tools per batch)."""
    out = []
    tools = {}
    for t in crafts["tools"]:
        tools.setdefault(t["producer"], {})[t["item"]] = 1.0 / t["everyDays"]   # Tools worn a day.
    for p in crafts["producers"]:
        per_day = DAY_SECONDS / p["seconds"]
        wear = {item: n / per_day for item, n in tools.get(p["id"], {}).items()}
        out.append((p["id"], p["seconds"], {}, p["out"], wear))
    for c in crafts["crafts"]:
        out.append((c["id"], c["seconds"], c["in"], c["out"], {}))
    return out


def costs(items, crafts, wage, markup):
    """Unit cost of each good (what its maker paid for it all), and its selling price (cost x (1 + markup))."""
    per_second = wage / DAY_SECONDS
    makes = {}
    for w in ways(crafts):
        for good in w[3]:
            makes.setdefault(good, []).append(w)
    price = {g: items[g]["price"] if g in items else 1 for g in makes}       # Start from the catalogue.
    cost = dict(price)
    for _ in range(200):
        changed = 0.0
        for good, options in makes.items():
            best = None
            for (_id, seconds, ins, outs, wear) in options:
                if any(i not in price for i in list(ins) + list(wear)):
                    continue                                    # An input nobody makes: not a way for now.
                batch = seconds * per_second
                batch += sum(n * price[i] for i, n in ins.items())
                batch += sum(n * price[i] for i, n in wear.items())
                weight = {o: n * max(items.get(o, {}).get("price", 1), 0.1) for o, n in outs.items()}
                share = weight[good] / sum(weight.values())
                unit = batch * share / outs[good]
                best = unit if best is None else min(best, unit)
            if best is None:
                continue
            changed = max(changed, abs(best - cost[good]))
            cost[good] = best
            price[good] = best * (1 + markup)
        if changed < 1e-6:
            break
    return cost, price


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wage", type=float, default=12)
    ap.add_argument("--markup", type=float, default=0.2)
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args()
    items, crafts = load()
    cost, price = costs(items, crafts, a.wage, a.markup)
    rows = sorted(cost, key=lambda g: price[g] / max(items.get(g, {}).get("price", 1), 0.01))
    print(f"wage {a.wage}p a day, {a.markup:.0%} margin at every step; {len(rows)} goods made in the game\n")
    print(f"{'good':22} {'catalogue':>9} {'built':>8} {'ratio':>6}")
    show = rows if a.all else rows[:15] + ["..."] + rows[-15:]
    for g in show:
        if g == "...":
            print("  ...")
            continue
        cat = items.get(g, {}).get("price", 0)
        print(f"{g:22} {cat:9.2f} {price[g]:8.2f} {price[g] / max(cat, 0.01):6.2f}")


if __name__ == "__main__":
    main()
