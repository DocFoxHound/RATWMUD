"""The worth ladder (ladder.json, doc 63): rungs, where each item sits, and what that is in a labourer's day-wages.

    worth(item)          -> LD, from its placing (rung + at) or its catalogue price
    place(ld)            -> (rung id, at) for a worth: where an editor's slider would put it
    python3 tools/econ_model/ladder.py [item ...]   prints placings (all placed items by default)
"""
import json
import math
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]


class Ladder:
    def __init__(self, path=HERE / "ladder.json", items=None):
        self.data = json.load(open(path))
        self.rungs = {r["id"]: r for r in self.data["rungs"]}
        self.placed = self.data["items"]
        self.catalogue = items if items is not None else \
            {i["id"]: i for i in json.load(open(ROOT / "Data/Items/items.json"))["items"]}

    def __getitem__(self, key):
        return self.data[key]

    def at_rung(self, rung, at):
        r = self.rungs[rung]
        return r["from"] * (r["to"] / r["from"]) ** at

    def worth(self, item):
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
