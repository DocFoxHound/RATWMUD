# 24 — The Western World

## What it is

The whole western world around Upper Accord, laid out from the author's drawing (`~/Pictures/western_world_map.png`):
Ridgemere in the north-west, Ser Ferro in the south-west, the Northern Fortress, the Ghost Town, a lake with an
island fortress, a mountain valley with a village, the ocean and its isle, the great river, the swamps, and the towns
along the roads. Ridgemere and Ser Ferro are designed cities with their people (see 25-great-cities.md); the towns
and fortresses are still placeholder walls with empty ground inside, ready to be built out later.

```
cd tools
python3 -m worldgen.western --preview DIR --out DIR   # world.png, canvas.npz, western_cells.json (~2 minutes)
python3 -m worldgen.western --import-dev              # replace the generated cells in DEV, keep everything else
python3 -m unittest test_western
```

Previews of the current generation in `artifacts/western/`: `world.png` (the whole world, one pixel per two tiles),
`roads.png` (each road's route), `biomes.png` (glyph close-ups: rain coast, snow thinning into temperate land, grain
fields, deep swamp), `ridgemere.png` and `tarn.png`.

## Scale and placement

The drawing is 2048 px square; with Upper Accord's region matched to where it sits on it, one drawing pixel is 1.25
tiles, so the world is **2560 × 2560 tiles**: world x −1792…767, y −512…2047. Upper Accord keeps its coordinates
(0…767, 0…767) and its 14 cells are left as they are, except the streams painted into Southern Peak (below). Everything else is cut into 256-tile cells named `w_<region>_<cx>_<cy>`:
**91 cells** (the 3 × 3 squares of Upper Accord are skipped; so would open sea be).

Ground within 48 tiles of Upper Accord eases to the heights of its edge, so the two join without cliffs, and the
Accord Road meets Upper Accord's main road where it leaves its region (world 0, 430).

## How it is made

1. **Biomes from the drawing.** Each pixel is matched to the drawing's colours: ocean/lake/river water, cold north,
   temperate middle, plains south, swamp. Ink, labels and markers (and the anti-aliased fringe of the ink, which
   otherwise reads as dark swamp) take the colour around them. Three regions are laid out from the plan rather than
   the colours:
   - **The rain coast** around Ridgemere (the drawing's grey highland and ~550 tiles around it, about ten cells):
     Cascadia-like cedar rainforest, fern glades, moss, wet meadows, mossy bluffs, alder bottoms and old growth;
     its cells rain. Snow stays on the high peaks only.
   - **Ser Ferro's grain country** (the plains around the city and the south edge of the downs): a patchwork of
     golden grain fields, stubble, pasture and flowers parcelled by hedgerows, golden meadows and orchard hills,
     on long rolling swells. No packed earth.
   - **The deep swamp**: the drawing paints both swamps one olive, so it is the swamp south of the great river.
2. **Gradual transitions.** Biomes do not meet at a line: each tile picks among the biomes around it, weighted by how
   much of each lies within ~250 tiles, with clumped and speckled noise, so neighbouring biomes interleave over about
   a cell's width. Snow also thins toward the edge of the cold north (tundra, then grass, show through), and the
   rain coast gives way to snow eastward and to temperate land southward. Sub-biome patches (~150 tiles) interleave
   for a few tiles at their borders.
3. **Heights.** Each drawn range is a set of peaks joined by ridges (a spanning tree over its peaks): a crest that
   dips to saddles between peaks, steep flanks and broad foothills, all laid on noise-bent coordinates so nothing is
   round, then cut by ridged noise into spurs and gullies. The drawn hill arcs are joined the same way into downs.
   Ser Ferro's country rolls in long swells; elsewhere lowland knolls come and go with noise. Forest climbs the
   mountain flanks (cedar on the rain coast); rock, scree, moss and snow cover the heights.
4. **Water.** Deep water in the middle of the ocean, lake and rivers, wadeable shallows at the edges, bog water in the
   swamps and ice on cold inland water; beaches, the rain coast's shingle and reed banks line the shores. The lake
   keeps a 36-tile shore from Upper Accord's region so a road fits down its east side.
   **The Eastern Vale's river** rises in a tarn below Southern Peak, fed by three streams from springs on the peak's
   southern flank (painted into Upper Accord's `southern_peak` cell, with its heights kept, the only change to Upper
   Accord). It leaves the tarn ~7 tiles wide and widens to the drawing's river by the confluence.
5. **Settlements**, each wholly inside one cell, on levelled ground with gates facing their roads:
   | | Cell |
   |---|---|
   | Ridgemere (city, wall follows the coast, fills the cell's land) | Ridgemere Heights, North West |
   | Ser Ferro (city, wall follows the river bank) | The Ser Ferro Marches, South |
   | Cinderbrook | The Ser Ferro Marches, West |
   | Lakeside (wall follows the lake shore) | The Mirrormere Shore, East |
   | Accord Crossing | The Accord Marches, North East |
   | Fenhollow | The Mirelands, Heart |
   | Amberford | The Amber Steppe, North |
   | Saltreach, Hollowmere, Westmarch, the Northern, Isle and Dark Fortresses, the Ghost Town | as drawn |
   Gates open through the whole wall, and roads arrive at a settlement's own level, so every one can be walked into.
6. **Roads.** 17 roads and spurs follow the drawn ones through their waypoints, but find their own way between them
   (A* over 4-tile squares): the sea and lakes cost dearly, so roads go round them and keep a few tiles back from the
   shore; rivers cost little, so they are bridged where needed; slopes and heights cost by steepness, so roads take
   passes. Each is graded (never more than half a step per tile, the land eased into it over a dozen tiles either
   side); only the Lake Causeway to the island fortress runs straight over water.
7. **Points of interest.** About 550 of them, at least ~78 tiles apart and at least two in every cell, chosen by
   biome (ruins, watchtowers, stone circles, camps, ponds, groves, great trees, shrines, farmsteads, orchards, flower
   fields, outcrops, mesas, ravines, dry creeks, wolf dens, cairns, frozen lakes, hot springs, ice ridges, sunken
   ruins, boardwalks, hummocks, drowned shrines). Their shapes are irregular, never circles.
8. **Cliffs** are marked wherever a drop cannot be walked, last, so nothing leaves an unmarked ledge. Near Upper
   Accord the ground eases to its edge heights and is smoothed so the edge's profile does not streak outward.

Checked with the game's own step rules, 97.9% of walkable land outside Upper Accord is one connected region and all
of every settlement's ground can be reached. The rest is mostly the Isle of Grey Horns (reached by sea).

Weather: rain coast cells rain, cold cells snow, deep swamp fog, swamp overcast, the rest clear. (The seasonal
climate model is still temperate for all of them; per-region climate is future work.)

Atlas zooms out to 0.2 screen pixels a tile, enough to see the whole world; zoomed out, each cell is drawn as one
image written pixel by pixel, so the whole world appears in moments.

## How it runs (DEV revision 5, build 9)

| | |
|---|---|
| World | 105 cells (91 new) + 171 interiors = 276 places, 6.59 M tiles, 71,948 doors and seams |
| Atlas JSON | ~59 MB, most of it height overrides (3.16 M) |
| DEV save / load (Python) | 24 s / 5.6 s |
| `world_build.py dev` | 34 s, 860 MB peak |
| `world_check`, every place in memory | 7.7 s, 535 MB |
| `world_check --simulate 7 9` (157 residents, walking player) | tick mean 5.7 ms, p99 9.7 ms, p99.9 22 ms (budget 50 ms); a player's view 2.1 ms |
| Game server boot | 5.4 s; streams 181 of 276 places |
| Atlas: open (host + parse + first check) | ~5.6 s + 1.8 s + 7.5 s |
| Atlas: one edit / re-check | ~100 ms / ~10 ms |

**Fixed to get there:**
- Atlas re-checked and copied the whole world on every edit: a paint stroke took 9.3 s, a re-check 2 s. A cell's rows
  and heights are now frozen when it is made, so "is this cell unchanged?" is a comparison of two references rather
  than of every height, and the legacy quarter-step scan skips unchanged cells (`Editor/src/model/model.mjs`).
  Changing a cell's ground in place now throws; replace it (as every operation already did).
- Loading a world from Postgres sorted 3.3 M height keys by re-parsing each key string: 12.5 s → 5.6 s, same output
  (`tools/world_store.py`).

**Still heavy:** heights are stored as sparse `"x,y": h` overrides, which is 40 MB of the 54 MB. The first open of the
world in Atlas (checking every cell once) is ~15 s. A compact height encoding (e.g. one character per tile per row, as
the game's snapshots already use) would cut both roughly in half; this is the next thing to do if the world grows.
