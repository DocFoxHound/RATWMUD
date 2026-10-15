"""The worth ladder (Data/Economy/ladder.json, doc 63): rungs, where each item sits, and what that is in a labourer's
day-wages (LD). An item's place is its own `worth` in Data/Items/items.json; else the ladder's `anchors` (the
placings it began from); else where its catalogue price puts it.

    worth(item)          -> LD, from its placing (rung + at) or its catalogue price
    place(ld)            -> (rung id, at) for a worth: where an editor's slider would put it
    python3 tools/econ_model/ladder.py [item ...]   prints placings (the anchors by default)
"""
import json
import math
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
LADDER = ROOT / "Data/Economy/ladder.json"


class Ladder:
    def __init__(self, path=LADDER, items=None):
        self.data = json.load(open(path))
        self.rungs = {r["id"]: r for r in self.data["rungs"]}
        self.placed = self.data["anchors"]
        self.catalogue = items if items is not None else \
            {i["id"]: i for i in json.load(open(ROOT / "Data/Items/items.json"))["items"]}

    def __getitem__(self, key):
        return self.data[key]

    def at_rung(self, rung, at):
        r = self.rungs[rung]
        return r["from"] * (r["to"] / r["from"]) ** at

    def worth(self, item):
        own = self.catalogue.get(item, {}).get("worth")
        if own:
            return self.at_rung(own["rung"], own.get("at", 0.5))
        p = self.placed.get(item)
        if p:
            return self.at_rung(p["rung"], p.get("at", 0.5))
        return self.catalogue.get(item, {}).get("price", 1) / self.data["catalogueDay"]

    def place(self, ld):
        rungs = self.data["rungs"]
        for r in rungs:
            if ld < r["to"] or r is rungs[-1]:
                ld = max(ld, r["from"])
                return r["id"], math.log(ld / r["from"]) / math.log(r["to"] / r["from"])
        raise ValueError(ld)


if __name__ == "__main__":
    lad = Ladder()
    for item in sys.argv[1:] or list(lad.placed):
        ld = lad.worth(item)
        rung, at = lad.place(ld)
        print(f"{item:14} {ld:8.3f} LD  {lad.rungs[rung]['name']:9} at {at:.2f}")
