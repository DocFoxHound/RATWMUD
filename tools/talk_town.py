#!/usr/bin/env python3
"""The synthetic talk town (Docs/Design/84 §15.8): a small world export for the conversation simulator's ctest runs
(talk_scorecard_quick, the golden transcripts), read with no database.

Three places side by side, each in its own dialect region: Ridgemere's market square (plain speech), Coldwater's quay
(Coldwater's) and Saltwell's oasis inn (Saltwell's), with forty residents of every age, both sexes, a few families and
the usual trades, working in small groups where a listener at the middle can hear them, and living in small homes off
their place (a family or a lodger or two to each, so only they count as a household). They wake at home: talk_sim
--warm runs the town on an hour or two first, so they are at work when the listening starts. Written as
tools/world_build.py export writes a world (world.ratw, cells/ID.cell, seams/ID), and read by Game's worldExport.

  python3 tools/talk_town.py                 writes Tests/fixtures/talk_town/
  python3 tools/talk_town.py --out DIR       somewhere else

The output is committed; regenerate only on purpose (the quick scorecard's baseline and the goldens read it). The doc
84 mantles the town will wear come with Phase 9.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent.parent
WIDTH, HEIGHT = 24, 16

# (cell id, name, region, outdoors)
PLACES = [
    ("tt_ridgemere_square", "Ridgemere Market Square", "ridgemere", True),
    ("tt_coldwater_quay", "Coldwater Quay", "coldwater", True),
    ("tt_saltwell_inn", "The Oasis Inn", "saltwell", False),
]

# Per place: (id suffix, name, sex, age, role, work label). Names are the realm's kind; none says "dog".
PEOPLE = {
    "ridgemere": [
        ("baker", "Hollin Ashby", "female", 44, "merchant", "baking bread"),
        ("baker_son", "Tam Ashby", "male", 15, "civilian", "running errands"),
        ("smith", "Garrick Flint", "male", 52, "merchant", "smithing"),
        ("smith_wife", "Maren Flint", "female", 49, "civilian", "mending clothes"),
        ("pup", "Wren Flint", "female", 8, "civilian", "playing"),
        ("grocer", "Odile Penhallow", "female", 38, "merchant", "keeping the stall"),
        ("guard", "Brannoc Hale", "male", 31, "guard", "keeping the watch"),
        ("guard2", "Sefa Morrow", "female", 27, "guard", "keeping the watch"),
        ("carter", "Joss Wainwright", "male", 35, "civilian", "carting goods"),
        ("clerk", "Ida Quillon", "female", 29, "civilian", "copying the city rolls"),
        ("elder", "Old Petrel Vane", "male", 74, "civilian", "resting"),
        ("weaver", "Linnet Strand", "female", 33, "merchant", "weaving cloth"),
        ("porter", "Dunstan Rook", "male", 41, "civilian", "carrying loads"),
        ("healer", "Sorrel Mendham", "female", 57, "civilian", "tending the sick"),
        ("apprentice", "Kit Barrow", "male", 17, "civilian", "learning the trade"),
        ("chandler", "Aldous Wick", "male", 60, "merchant", "making candles"),
    ],
    "coldwater": [
        ("boatman", "Corwin Marsh", "male", 46, "civilian", "rowing the ferry"),
        ("boatman_wife", "Ysolde Marsh", "female", 43, "merchant", "keeping the stall"),
        ("pup", "Fen Marsh", "male", 9, "civilian", "playing"),
        ("soaper", "Brisk Tallow", "female", 36, "civilian", "boiling soap"),
        ("weaver", "Harl Loomis", "male", 39, "civilian", "working the looms"),
        ("swineherd", "Mott Pennick", "male", 28, "civilian", "minding the swine"),
        ("elder", "Grandam Oona Reed", "female", 71, "civilian", "resting"),
        ("guard", "Teague Holloway", "male", 33, "guard", "keeping the watch"),
        ("stiller", "Nessa Coombe", "female", 50, "civilian", "running the still"),
        ("youth", "Pell Coombe", "male", 14, "civilian", "running errands"),
        ("carter", "Wick Marshwade", "female", 34, "civilian", "carting goods"),
        ("keeper", "Myrtle Stillwater", "female", 47, "merchant", "keeping the stores"),
    ],
    "saltwell": [
        ("innkeeper", "Oriel Sandmere", "female", 45, "merchant", "keeping the inn"),
        ("raker", "Ridge Saltash", "male", 38, "civilian", "raking salt"),
        ("raker_wife", "Fern Saltash", "female", 35, "civilian", "raking salt"),
        ("pup", "Dune Saltash", "male", 7, "civilian", "playing"),
        ("storekeeper", "Thistle Stonepick", "female", 51, "merchant", "keeping the stores"),
        ("guard", "Hazan Drywell", "male", 30, "guard", "keeping the watch"),
        ("elder", "Old Amber Dusk", "female", 77, "civilian", "resting"),
        ("cook", "Basil Ovenden", "male", 42, "civilian", "cooking"),
        ("potter", "Clay Hearne", "male", 55, "merchant", "throwing pots"),
        ("youth", "Saffron Hearne", "female", 16, "civilian", "learning the trade"),
        ("drover", "Jet Caravel", "male", 37, "civilian", "driving the caravan"),
        ("water", "Mira Wellspring", "female", 29, "civilian", "drawing water"),
    ],
}
KIN = {
    "ridgemere": [("smith", "smith_wife", "spouse"), ("smith", "pup", "parent"), ("smith_wife", "pup", "parent"),
                  ("baker", "baker_son", "parent")],
    "coldwater": [("boatman", "boatman_wife", "spouse"), ("boatman", "pup", "parent"),
                  ("boatman_wife", "pup", "parent"), ("stiller", "youth", "parent")],
    "saltwell": [("raker", "raker_wife", "spouse"), ("raker", "pup", "parent"), ("raker_wife", "pup", "parent"),
                 ("potter", "youth", "parent")],
}


# Who lives together, by place: a family under one roof, the rest alone or two to a lodging (so only families and a few
# lodgers share a home: Society::household counts a shared home cell).
HOMES = {
    "ridgemere": [["baker", "baker_son"], ["smith", "smith_wife", "pup"], ["grocer"], ["guard", "guard2"], ["carter"],
                  ["clerk"], ["elder"], ["weaver"], ["porter"], ["healer"], ["apprentice", "chandler"]],
    "coldwater": [["boatman", "boatman_wife", "pup"], ["soaper"], ["weaver"], ["swineherd"], ["elder"], ["guard"],
                  ["stiller", "youth"], ["carter"], ["keeper"]],
    "saltwell": [["innkeeper"], ["raker", "raker_wife", "pup"], ["storekeeper"], ["guard"], ["elder"], ["cook"],
                 ["potter", "youth"], ["drover"], ["water"]],
}
HOME_W, HOME_H = 6, 5


def spots(n: int) -> list[tuple[float, float]]:
    """Small groups of about four, two tiles apart, around the middle of the place (the listener stands at its
    centre), so each resident has two or three within talking distance (3 tiles) and all are in earshot."""
    out = []
    groups = [(7.5, 5.5), (12.5, 5.5), (17.5, 5.5), (7.5, 10.5), (12.5, 10.5), (17.5, 10.5)]
    for i in range(n):
        gx, gy = groups[(i // 3) % len(groups)]
        k = i % 3
        out.append((gx + (k - 1) * 1.0, gy + (1.0 if k == 1 else 0.0)))
    return out


def resident(rid: str, name: str, sex: str, age: int, role: str, work: str, home: tuple[str, float, float],
             cell: str, x: float, y: float) -> str:
    # id name role work description greeting age | species sex stature pattern | base gradient marking speakingColour
    # paid start end | route purse herbs meals | home(cell x y) work(cell x y) evening(cell x y)
    hc, hx, hy = home
    return (f'resident "{rid}" "{name}" "{role}" "{work}" "A resident of the talk town." "Well met." {age} '
            f'"timber" "{sex}" "average" "saddle" 3 1 5 1 1 6 22 "-" 40 0 1 '
            f'"{hc}" {hx} {hy} "{cell}" {x} {y} "{cell}" {x} {y}\n')


def write(out: Path) -> None:
    if out.exists():
        shutil.rmtree(out)
    (out / "cells").mkdir(parents=True)
    (out / "seams").mkdir()
    manifest = ["RATW_WORLD 3\n"]
    seams: dict[str, str] = {cid: "" for cid, *_ in PLACES}
    for i, (cid, name, region, outdoors) in enumerate(PLACES):
        rows = ["." * WIDTH for _ in range(HEIGHT)]
        doorway = list(rows[1])                     # (Its homes' doors, '+', along the top: see below.)
        for k in range(len(HOMES[region])):
            doorway[1 + 2 * k] = "+"
        rows[1] = "".join(doorway)
        grid = "\n".join(rows)
        (out / "cells" / f"{cid}.cell").write_text(
            f"id: {cid}\nname: {name}\ndescription: The talk town's {name}.\nworld: {i * WIDTH} 0 0\n"
            f"outdoors: {'true' if outdoors else 'false'}\nweather: clear\nsize: {WIDTH} {HEIGHT}\ngrid:\n{grid}\n")
        manifest.append(f'area "{cid}"\n')
    seam = 0
    for i in range(len(PLACES) - 1):
        a_cell, b_cell = PLACES[i][0], PLACES[i + 1][0]
        for k in range(HEIGHT):
            a, b = f"seam_{seam}_a", f"seam_{seam}_b"
            seams[a_cell] += (f'door "{a}" "Open boundary" "{a_cell}" {WIDTH - .5} {k + .5} "{b_cell}" .5 {k + .5} '
                              f'"{b}" 1 0 1 1 "E"\n')
            seams[b_cell] += (f'door "{b}" "Open boundary" "{b_cell}" .5 {k + .5} "{a_cell}" {WIDTH - .5} {k + .5} '
                              f'"{a}" 1 0 1 1 "W"\n')
            seam += 1
    # Each household's home: a small room off its place, through a door on the place's top edge.
    homes: dict[str, tuple[str, float, float]] = {}
    doors = []
    for i, (cid, _, region, _) in enumerate(PLACES):
        for k, members in enumerate(HOMES[region]):
            hid = f"tt_{region[:2]}_home_{k}"
            rows = ["." * HOME_W for _ in range(HOME_H)]
            rows[4] = "...+.."
            grid = "\n".join(rows)
            (out / "cells" / f"{hid}.cell").write_text(
                f"id: {hid}\nname: A home in the talk town\ndescription: A small room.\nworld: {i * WIDTH + k * 2} "
                f"{40 + k * 8} 0\noutdoors: false\nweather: clear\nsize: {HOME_W} {HOME_H}\ngrid:\n{grid}\n")
            seams[hid] = ""
            manifest.append(f'area "{hid}"\n')
            dx = 1.5 + k * 2
            doors.append(f'door "door_{hid}_out" "Door" "{cid}" {dx} 1.5 "{hid}" 3.5 3.5 "door_{hid}_in" 1 0 0 0 "-"\n')
            doors.append(f'door "door_{hid}_in" "Door" "{hid}" 3.5 4.5 "{cid}" {dx} 2.5 "door_{hid}_out" 1 0 0 0 "-"\n')
            for n, who in enumerate(members):
                homes[f"tt_{region[:2]}_{who}"] = (hid, 1.5 + n, 1.5)
    for cid, text in seams.items():
        (out / "seams" / cid).write_text(text)
    for i, (cid, _, region, _) in enumerate(PLACES):
        exits = [PLACES[j][0] for j in (i - 1, i + 1) if 0 <= j < len(PLACES)]
        manifest.append(f'exits "{cid}" {len(exits)} ' + " ".join(f'"{e}"' for e in exits) + "\n")
        manifest.append(f'territory "{cid}" "{region}" "-" 0\n')
        for k in range(len(HOMES[region])):
            manifest.append(f'territory "tt_{region[:2]}_home_{k}" "{region}" "-" 0\n')
    manifest.extend(doors)
    manifest.append(f'spawn "{PLACES[0][0]}" 12.5 13.5\n')
    manifest.append("economy 20000 100 50\n")
    for cid, _, region, _ in PLACES:
        people = PEOPLE[region]
        for (suffix, name, sex, age, role, work), (x, y) in zip(people, spots(len(people))):
            rid = f"tt_{region[:2]}_{suffix}"
            manifest.append(resident(rid, name, sex, age, role, work, homes[rid], cid, x, y))
    for cid, _, region, _ in PLACES:
        for a, b, kind in KIN[region]:
            manifest.append(f'kin "tt_{region[:2]}_{a}" "tt_{region[:2]}_{b}" {kind}\n')
    (out / "world.ratw").write_text("".join(manifest))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--out", type=Path, default=ROOT / "Tests" / "fixtures" / "talk_town")
    args = parser.parse_args()
    write(args.out)
    print(f"talk town written to {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
