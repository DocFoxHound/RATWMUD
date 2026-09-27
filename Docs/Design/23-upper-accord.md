# 23 — Upper Accord, the Starting Region

**Status:** Terrain, city and campuses imported into DEV 2026-09-26 (replacing Greyfen there); residents added the same day (DEV revision 2).

## What it is

Upper Accord is the capital of the Warden Order and the seat of the Concord Hall. It is laid out from the author's drawing (`~/Pictures/upper_accord_map.png`):

- **The city:** a walled city fills a crater between three peaks.
- **Concord Hall** holds the northern peak's shoulder, at the top of a grand stair from the city's north corner.
- **The Warden Training Grounds** lie in a long terrace on the eastern peak's flank, beside the city's east wall.
- **The Warden Order** holds the southern peak's shoulder, at the end of a switchback path from the south gate.
- **Trails** outside the walls join Concord Hall and the Order to the Training Grounds.
- **The main road** climbs in from the west to the Main Gate.

It is built by a generator, not drawn by hand: `tools/worldgen/`. The generator is deterministic (fixed seeds), so re-running it gives the same world. After it is imported, **Atlas is the source of truth**. Polish it by hand there; don't regenerate over it.

```
cd tools
python3 -m worldgen.upper_accord --preview DIR --out DIR   # previews, atlas JSON, building manifest
python3 -m worldgen.upper_accord --import-dev              # add it to the DEV world (see "Importing")
python3 -m unittest test_worldgen                          # validity, walkability, merge, Postgres round trip
```

## Layout

The region is a 768 × 768 tile frame of 14 cells. The city and each campus have a cell of their own, so walking a campus never flips between cells mid-courtyard.

| Cell | Tiles | Contents |
| --- | --- | --- |
| Upper Accord | 256 × 256 | The walled city |
| Concord Hall | 128 × 128 | Senate, chapel, hearing chambers, Annex, quarters, refectory |
| Warden Training Grounds | 128 × 256 | Barracks, cookhouse, yards, range, rings, boulder field, obstacle course |
| Warden Order | 128 × 128 | Leader's Hall, barracks, housing, refectory, armory, crypt library |
| Ten wilderness cells | 128–256 | The three peaks, two saddles, the western approach and the slopes |

**Heights:**

| Place | Ground height |
| --- | --- |
| Map edge (lowlands) | about −3 |
| City floor | 0 |
| Training Grounds | +5 |
| Warden Order | +7 |
| Concord Hall | +9 |
| Peaks (snow, bare rock, ledges) | up to +16 |

Upper slopes break into 2½-high terraces, so cliffs appear where the ground drops more than a full step.

- **The grand stair** climbs half a step per tile from the city to Concord Hall.
- **Paths** climb evenly between their ends and blend into the land beside them.
- **Every building door** can be reached on foot from the western road. The generator checks this with the game's own step rules.

## The city

- **Wall:** an old stone curtain wall, three tiles thick, with towers at its corners.
- **Gates:** the Main Gate (west), the South Gate and the Stair Gate. All are open archways.
- **Streets:** a cobbled ring road inside the wall, and avenues from each gate to a central plaza with a fountain, a statue, market stalls, wells and trees. Side streets form a grid of blocks.
- **Buildings:** 135. Each is a walled block with a roof (slate or shingle for homes, shingle for shops, stone for civic buildings) and one door to the street.
- **Wards** decide what goes where:

| Ward | What it holds |
| --- | --- |
| Gate | Inns, a tavern, armorers, fletchers, provisioners, the Gate Watch barracks, a warehouse |
| Market (around the plaza) | General goods, bakers, butchers, tailors, a jeweler, chandlers, a fishmonger, a moneychanger, an inn |
| Crafts (south-east) | Smiths, a tanner, weavers, carpenters, a potter, a cooper, a mason, a brewer, a tinker, an infirmary |
| Hill (below the Stair Gate) | Apothecaries, herbalists, scribes, a cartographer, the House of Mending, the Hall of Records, a guardhouse |
| Residential (north-east, west, south-west, east) | 80 family houses spread across all the wards |

## The campuses

Each campus is described by its style of stone:

**Concord Hall** — carved pale marble, pale roofs, banners on the walls, colonnades, gardens with pools, braziers, and a statue-lined carpet from the Stair Gate to the Senate.
- The Grand Hall of Concord: tiered pews facing a high seat.
- The Chapel of the First Oath: a nave, pillars and an altar.
- Three hearing chambers.
- Clerks' Quarters and a refectory.
- **The Concord Annex:** a reading hall, then the upper stacks, the scroll vaults and the deep archive, three levels down.

**Warden Training Grounds** — open, flat and rocky.
- The Pup Den (straw bedding), Trainee Barracks, Trainers' Hall, the Training Cookhouse and Training Stores.
- A drill square with a running track, an archery range and three sparring rings.
- Rows of training dummies, a boulder field, and an obstacle course of rubble, water, fences and scree.

**Warden Order** — old grey stone, undecorated.
- The Leader's Hall: the high seat and council table, with the leader's quarters upstairs.
- Two barracks, eight permanent Warden houses, a refectory and an armory.
- An old graveyard.
- **The Warden Crypt Library:** a low chapel over three catacomb levels of burial niches, shelves and sarcophagi, down to the founders' vault.

## Residents

`python3 -m worldgen.residents` (from `tools/`) adds the population to the world in DEV. It reads the world as it is in DEV, so Atlas edits are kept, and it takes each building's purpose from the generator's manifest. Every place it gives a resident is checked against DEV's actual tiles. It refuses to run twice unless given `--replace`, and `--dry-run` checks everything without saving.

**157 residents: 88 civilians, 20 guards and 49 merchants.**

| Where | Who |
| --- | --- |
| **City** | A keeper for each of the 45 shops, taverns and inns, living in a family house, often with a partner, a helper or a youngster.<br>Healers, record clerks and warehouse hands.<br>Stall-holders, a water-drawer, a wall-mender and an old storyteller at the fountain.<br>The **Accord Watch**: day and night shifts on three circuits (ring road, market round, gate walk), and a guard at each gate. |
| **Concord Hall** | The Speaker of the Concord, three councillors, three arbiters, the chaplain, four Annex keepers (one on each level down), the refectory cook and messenger clerks. |
| **Training Grounds** | Four trainers (drill square, sparring rings, archery range, obstacle course), eight trainees, five pups, the cook and the quartermaster. |
| **Warden Order** | The leader, four advisors, six Wardens on the compound round and the ridge-trail patrol, four Wardens drilling, the crypt-library keeper, the refectory cook and the armorer. |

**Factions and territory:**
- **The Concord** and **The Accord Watch** claim the city.
- **The Concord** holds Concord Hall.
- **The Warden Order** holds the Training Grounds and its own compound.
- Interiors, including the floors above and below, take the claims of the cell their street door opens onto.

**Other world settings:**
- The economy is sized for a capital of this size.
- The herb patch sits beside the main road below the Main Gate.
- Houses that the generator had only numbered now carry their family's name (Ashwalker House).

**Text:** names, looks, personalities, greetings and backstories are assembled from trait, quirk and hook lists. They are a starting point to rewrite by hand in Atlas, especially for the key figures.

## Running a town this size

The first simulation of the region with its residents averaged about 45 ms per server tick (the budget is 50 ms), with stalls of several seconds. The simulation was written for Greyfen-sized cells, and several costs grew with a 256×256 cell's million-node navigation grid and its thousand-plus seam records. The fixes, all in `RatwWorld.cpp`:

- **Streaming:** it no longer walks every door of every occupied cell each tick. It works only when someone changes cells, or when the 10-second unload check is due, and each cell's neighbour list is cached.
- **Next-cell routes** (`firstSteps`): cached per starting cell, and cleared whenever doors change.
- **Path search:**
  - buffers are reused instead of allocating 13 MB per search;
  - the per-node checks are hoisted out of the inner loop;
  - it is weighted A* (×2), so paths may be slightly longer than the shortest;
  - it answers "no route" at once when the goal lies in another walkable region of the cell (a cached region map, rebuilt when the ground changes).
- **Path smoothing:** looks a bounded distance ahead (it was cubic in path length).
- **Residents' routes:**
  - they head for the nearest crossing they can actually reach;
  - they aim for the ground in front of a closed door rather than the door itself;
  - they back off for 5 seconds after a failed search;
  - at most 6 plan a route in any one half-second update, so a shift change doesn't stall the server.
- **Walking to a closed door:** each step is now judged from the step before it, where it used to be judged from the starting tile. Before, a walk down any slope failed.
- **Limits:** the resident cap is raised from 128 to 256 (editor, exporter, world loader and society restore).

**Measured with `world_check --simulate`:**

| Period | Mean tick | p99 | p99.9 | Result |
| --- | --- | --- | --- | --- |
| Morning (05:30–09:30) | 0.8 ms | 5.9 ms | 22 ms | 146 of 157 in place, the rest walking, none without a route |
| Evening rush (16:00–22:00) | 2.1 ms | 27 ms | 51 ms | 152 in place, the rest walking, none without a route |

The only tick near a quarter of a second is the server's first, when it loads the region.

**With a player in the world** (found on the first playtest: the game was sluggish, and the view showed only rain):

- **Visibility:** each player's visibility was recomputed every tick for every tile of the cell, and each tile recomputed the light and sight range from scratch. On a 256×256 cell that meant 89 ms ticks and 119 ms snapshots.
  - Visibility is now worked out once per look (`visibleTileMask`), only within sight range.
  - It is cached per player (`viewOf`) until their cell, position, sight or a door there changes.
  - Snapshots reuse that cache.
- **Snapshots:** they now carry the cell as rows of text (glyphs, visibility, heights in `ratwjson::HeightChar`) instead of one JSON object per tile.
- **The client:**
  - it drew all 65,536 tiles of a large cell every frame and now draws only those on screen;
  - its map always framed a cell's top-left 32×24 tiles and now **follows the wolf**, stopping at the cell's edges;
  - Shift/Ctrl + wheel still look around until the wolf moves.

**Measured with one player walking about the plaza** (07:00–07:30):

| Mean tick | p99 | That player's view (5 a second) | Motion frame (every tick) |
| --- | --- | --- | --- |
| 6 ms | 12 ms | 2.1 ms | 0.05 ms |

**In the game server itself** (found when the game would not start after a sluggish session saved a checkpoint):

- **The resident cap:** the save reader still skipped reading residents past 128, so a 157-resident save could not be restored. Every resident limit is now one constant, `ratw::MaxResidents`, and `RATW.Society.LargePopulationPersistence` saves and restores 200 residents.
- **The database stalls:** DEV's Postgres runs in a container, and any query after a second of quiet takes about 45 ms, where back-to-back queries take about 1 ms. The server did that on its game thread every second (polling Dungeon Master actions) and every 5 seconds (a 40–170 ms checkpoint write). Now:
  - the Dungeon Master queue is read only when `tools/dungeon_master.py` sends `NOTIFY ratw_dm`, plus once a minute;
  - the periodic autosave runs every 15 seconds and is written by a worker thread (`FRatwPersistence::SaveInBackground`), with explicit `Save()` calls still synchronous;
  - spawns are checked every 30 seconds.

`world_check EXPORT_DIR --simulate FROM TO` reproduces this. `RATW_TRACE=<id>` follows one resident.

## For the population phase (building manifest)

`--out` also writes `buildings.json`. For every building it records its kind, ward, door, rooms, **beds** (where residents sleep) and **work** spots (behind counters, at high seats, at forges). The current generation has about 170 interiors, more than 200 beds and more than 50 workplaces.

The residents phase (above) filled these with 157 people:
- shopkeepers and crafters;
- families;
- the city watch on patrol routes;
- Concord clerks and councillors;
- trainers and trainees;
- Wardens and their leader.

It will also add factions (the Warden Order, the Concord) and territory claims.

## Importing

`--import-dev` loads the one world in DEV, adds the region beside it, and moves the spawn to the city plaza.
- **Placement:** by default `--offset 1024 0`, clear of Greyfen at the origin.
- **Overlaps:** it refuses to overlap an existing cell.
- **Re-running:** it refuses to import a second time unless given `--replace`, which discards Atlas edits to the region.

The database must have migrations `0019_cliffs_and_weather` and `0020_terrain_catalog` applied first (`python3 tools/world_db.py migrate`).

## New building blocks

**Tiles** — every tile now comes from `Data/Terrain/terrain.json` (see `22-elevation-weather.md` and the catalog's own notes).
- Tiles added for this region: roofs (slate, shingle, old stone, pale stone), furniture, civic and training pieces, and natural features.
- The map draws their Unicode glyphs in the bundled DejaVu Sans Mono, with a plain-ASCII setting.

**Buildings** — `tools/worldgen/buildings.py` lays out interiors by type in each district's stone:
- houses and 24 shop trades;
- taverns and inns (the inns have an upstairs);
- senate, chapel, hearing chamber and refectory halls;
- barracks, guardhouses, infirmaries, warehouses, and the leader's hall;
- multi-level libraries and catacombs.

**Checking a build** — `Tests/world_check.cpp` (built as `build-core/world_check`) loads a streamed export folder exactly as the server does, and brings every place into memory.
