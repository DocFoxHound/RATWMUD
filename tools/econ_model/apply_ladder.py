"""Writes the worth ladder into the game's catalogue (Docs/Design/64-economy-migration.md, Phase 2).

Into Data/Items/items.json, for every item: `worth` (its rung and where in it, 0..1) and `playerSeconds` (how long a
player takes to make it: 0 for every item for now, the user's choice, to be set per item in Atlas and the Dungeon
Master). An item keeps the place it has; one with none takes the ladder's anchor for it, or where its catalogue price
puts it.

Into Data/Items/crafts.json, for every craft and producer: `hours`, the NPC work a batch (or a producer's spell)
takes so that, priced from cost, its goods sell at their worth (calibrate.py). It sits beside `seconds`, which still
paces making until doc 64's Phase 3 makes goods from hours.

A recipe whose inputs alone cost more than its goods are worth is a conflict: those goods' worth is raised to what
they cost to make (the work its usual share), and the ladder settled again, until there are none.

    python3 tools/econ_model/apply_ladder.py [--dry-run]

Re-runnable: it changes only the fields above, one item or craft to a line, the rest of each line as it was.
"""
import argparse
import json
import pathlib

import calibrate
from ladder import Ladder

ROOT = pathlib.Path(__file__).resolve().parents[2]
ITEMS = ROOT / "Data/Items/items.json"
CRAFTS = ROOT / "Data/Items/crafts.json"


def settle(items, crafts, recipes, ladder, passes=12):
    """Places every item, raising conflicts' goods to their built cost until none are left. Returns the rows of the
    last calibration and the items raised (id -> (old LD, new LD))."""
    raised = {}
    for i in items.values():
        if "worth" not in i:
            rung, at = ladder.place(ladder.worth(i["id"]))
            i["worth"] = {"rung": rung, "at": round(at, 3)}
    rows = []
    for _ in range(passes):
        rows, cost, _now, target = calibrate.calibrate(items, crafts, recipes, ladder)
        conflicts = [r for r in rows if r["conflict"]]
        if not conflicts:
            break
        for r in conflicts:
            for o in r["outputs"]:
                if o not in items:
                    continue
                old = ladder.worth(o)
                new = cost[o] / calibrate.DAY
                if new > old * 1.001:
                    rung, at = ladder.place(new)
                    items[o]["worth"] = {"rung": rung, "at": round(min(1.0, at), 3)}
                    raised[o] = (raised.get(o, (old, new))[0], new)
    return rows, raised


def rewrite(path, key, change):
    """Rewrites a catalogue file one object a line: `change(obj)` returns the object to write, or None to keep it."""
    lines = path.read_text(encoding="utf-8").split("\n")
    out, inside, changed = [], False, 0
    for line in lines:
        stripped = line.strip()
        if stripped.startswith(f'"{key}": ['):
            inside = True
            out.append(line)
            continue
        if inside and stripped.startswith("]"):
            inside = False
        if inside and stripped.startswith("{"):
            comma = stripped.endswith(",")
            obj = json.loads(stripped.rstrip(","))
            new = change(obj)
            if new is not None and new != obj:
                indent = line[: len(line) - len(line.lstrip())]
                # (A line written with \u escapes keeps them.)
                line = indent + json.dumps(new, ensure_ascii="\\u" in stripped) + ("," if comma else "")
                changed += 1
        out.append(line)
    return "\n".join(out), changed


def after(obj, anchor, extra):
    """`obj` with `extra`'s keys placed after `anchor` (or at the end), replacing any already there."""
    out = {}
    for k, v in obj.items():
        if k in extra:
            continue
        out[k] = v
        if k == anchor:
            out.update(extra)
    if anchor not in obj:
        out.update(extra)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    items, crafts, recipes, _ = calibrate.load()
    ladder = Ladder(items=items)
    rows, raised = settle(items, crafts, recipes, ladder)
    left = [r["id"] for r in rows if r["conflict"]]
    hours = {(r["kind"], r["id"]): round(r["proposed"] / 3600, 3) for r in rows if r["kind"] in ("craft", "producer")}

    def item_change(obj):
        w = items[obj["id"]]["worth"]
        return after(obj, "price", {"worth": {"rung": w["rung"], "at": w["at"]}, "playerSeconds": obj.get("playerSeconds", 0)})

    def craft_change(kind):
        def change(obj):
            h = hours.get((kind, obj["id"]))
            return after(obj, "seconds", {"hours": h}) if h is not None else None
        return change

    items_text, n_items = rewrite(ITEMS, "items", item_change)
    crafts_text, n_crafts = rewrite(CRAFTS, "crafts", craft_change("craft"))
    CRAFTS_tmp = crafts_text
    # (The producers are in the same file: a second pass over the text.)
    tmp = CRAFTS.with_suffix(".tmp")
    tmp.write_text(CRAFTS_tmp, encoding="utf-8")
    crafts_text, n_producers = rewrite(tmp, "producers", craft_change("producer"))
    tmp.unlink()
    print(f"{n_items} items given their worth and player time; {n_crafts} crafts and {n_producers} producers their hours")
    print(f"{len(raised)} items raised to what they cost to make; conflicts left: {left or 'none'}")
    for o, (old, new) in sorted(raised.items(), key=lambda x: -x[1][1] / max(x[1][0], 1e-9))[:25]:
        rung, at = ladder.place(new)
        print(f"  {o:22} {old:8.3f} -> {new:8.3f} LD  ({ladder.rungs[rung]['name']}, at {at:.2f})")
    if not a.dry_run:
        ITEMS.write_text(items_text, encoding="utf-8")
        CRAFTS.write_text(crafts_text, encoding="utf-8")
        print("written:", ITEMS.relative_to(ROOT), CRAFTS.relative_to(ROOT))


if __name__ == "__main__":
    main()
