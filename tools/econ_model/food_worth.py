"""Places the foods on the worth ladder by how much they feed (Docs/Design/64-economy-migration.md, "The realm's
diet"; the user, 2026-10-15).

A food's worth is its nourishment times bread's worth a point of nourishment, times a premium for its kind:

    staple     1.0   bread, porridge, rabbit, offal: what the poor live on
    fresh      1.2-1.3   fish, meat, mutton, eggs, milk: the land's food as it comes
    preserved  1.6   smoked, salted, dried, cheese, butter, sausage: work and keeping
    cooked     1.6   a meal, a pie, a stew, a roast: a cook's work
    fine       3.0   spiced roasts, pâtés, honey cakes: for the comfortable

Drinks (ale, cider, mead, wine, tea) keep their places: they're for pleasure, not hunger. A food's place is its own
`worth` in Data/Items/items.json (rung and at), rewritten here; then tools/econ_model/apply_ladder.py recalibrates every
craft's and producer's hours, and raises any recipe whose materials now cost more than its goods.

    python3 tools/econ_model/food_worth.py [--dry-run]
"""
import argparse
import json
import pathlib
import subprocess
import sys

from ladder import Ladder
import apply_ladder

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ITEMS = ROOT / "Data/Items/items.json"

KINDS = {
    "staple": (1.0, ["bread", "porridge", "rabbit", "offal"]),
    "fresh": (1.2, ["fish", "raw_meat", "bone_broth", "marrow_bone", "milk_drink"]),
    "fresh+": (1.3, ["mutton", "eggs", "shellfish"]),
    "preserved": (1.6, ["smoked_rabbit", "smoked_fish", "smoked_mutton", "salt_fish", "salt_pork", "jerky", "pemmican",
                        "ship_biscuit", "cheese", "butter", "sausage", "blood_pudding"]),
    "cooked": (1.6, ["meal", "meat_pie", "mushroom_stew", "roast", "fine_bread"]),
    "fine": (3.0, ["spiced_roast", "liver_pate", "honey_cake"]),
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    items = {i["id"]: i for i in json.load(open(ITEMS))["items"]}
    ladder = Ladder(items=items)
    bread = items["bread"]
    per_point = ladder.worth("bread") / bread["food"]["nourish"]
    placed = {}
    for kind, (premium, ids) in KINDS.items():
        for id_ in ids:
            item = items.get(id_)
            if not item or not item.get("food", {}).get("nourish"):
                print(f"  (no food {id_})")
                continue
            ld = item["food"]["nourish"] * per_point * premium
            rung, at = ladder.place(ld)
            placed[id_] = {"rung": rung, "at": round(min(1.0, max(0.0, at)), 3)}
            print(f"  {id_:16} {kind:9} {ladder.worth(id_):7.3f} -> {ld:7.3f} LD  ({rung}, at {placed[id_]['at']})")

    def change(obj):
        if obj["id"] not in placed:
            return None
        out = dict(obj)
        out["worth"] = placed[obj["id"]]
        return out

    text, n = apply_ladder.rewrite(ITEMS, "items", change)
    print(f"{n} foods placed by nourishment (bread's {per_point * 1000:.1f} mLD a point)")
    if a.dry_run:
        return
    ITEMS.write_text(text, encoding="utf-8")
    # Then every craft's and producer's hours, from the new places.
    subprocess.run([sys.executable, str(HERE / "apply_ladder.py")], check=True, cwd=HERE)


if __name__ == "__main__":
    main()
