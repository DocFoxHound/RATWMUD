# 70. The realm: Ridgemere 2.0's map grown into a country (2026-11-01)

The user, 2026-11-01 (verbatim):

> The very first thing we need to do is expand the map. This is a big job, but here is what I want:
> 1. A very large map with multiple regions that blend and transition into one another.
> 2. Ridgemere should be a temperate seattle-like Pacific Northwest environment, with a lot of rain and related terrain.
> 3. North/Northeast side of the map should be mountainous and snowy.
> 4. Central part of the map should be rolling hills and plains.
> 5. Southwest part of the map should be marshy.
> 6. Southeast part of the map should be deserty.
> 7. Terrain should be interesting, beautiful, match the regions, and blend naturally with the other biomes.
> 8. There should be rivers and streams where it makes sense, lakes, etc etc.

> We aren't going to place any NPC'S or farms or other things from Country yet, just the map and physical locations
> for them.

Then, while it was being built:

> Keep in mind that I want you to use the map we already have and expand it, with Ridgemere being the northwest most
> city and region. Don't recreate the map.

> For Ridgemere itself keep it as is, the terrain around Ridgemere may need to be altered to fit the Pacific Northwest
> feel.

And for the farms' ground (doc 71's food plan, through its author): "size them somewhere in the middle. They
shouldn't be massive but a player should feel like they're big."

Builds on doc 67 (the city), doc 60 (the old realm's generator, whose methods this reuses), doc 66 (map identity),
doc 20 (terrain in the database) and doc 22 (elevation and weather). Doc 71 (the Country's producers) will place the
farms, works and towns' people on the sites reserved here.

```
cd tools
python3 -m worldgen.realm2 preview            # the realm round DEV's city: artifacts/realm2/ (previews, realm-report.json)
python3 -m worldgen.realm2 write [--dry-run]  # back up, then write it into DEV (the same map: an expansion)
python3 -m worldgen.ridgemere2 write          # the city writer lays the realm round the city too
```

## 1. Decisions

1. **Size: 16 x 16 cells of 256 tiles, 4,096 tiles square** (16.8 million tiles; the old realm of doc 60 was 8 x 8,
   the city alone 3 x 3). 241 cells are kept: the 15 cells of open sea off the fens' coast hold nothing and are left
   off the map (a sparse world, doc 20). The costs are measured in section 7; the server, the database, Atlas and the
   client all handle it.
2. **The map is grown, not made again.** Ridgemere's nine cells keep their ids (`w_ridgemere_<cx>_<cy>`) and
   coordinates (x and y 0-768); the realm is added east and south, so Ridgemere is the north-west corner, the sea and
   its bay on the west. The new cells continue the scheme: `w_ridgemere_<cx>_<cy>`, cx and cy 0-15.
3. **The city is untouched.** Every built tile of the nine cells, with its height, is kept exactly: the walls and all
   inside them, the bay, the wharf, the piers and the Mole, the three Houses' grounds, the roads from the gates, and a
   margin round every door end, NPC spot, patrol post and the spawn. Only the wild ground round them (the moor of the
   first build, 59% of the nine cells: 346,874 tiles) is made again, as the rain coast's cleared ground and woods. A check
   compares every kept tile's code and height with the city as its writer made it.
4. **The same map.** The write keeps the map identity (`map-20261031-6b080cecb7f6`, "Ridgemere 2.0: the city (doc 67)")
   and records the new layout on it (`stamp_for_layout(..., mode='same')`): the map is extended, add-only, not replaced.
   The fingerprint changes (it hashes every cell's terrain), which doc 66 allows for an expansion; pull and import
   between DEV and a world on this map keep working.
5. **Four new terrain kinds** (Data/Terrain/terrain.json, `python3 tools/terrain_catalog.py`): **Sandstone** `(`,
   **Salt flat** `)`, **Cactus** `?` and **Willow** `@`. The editor's tests used `?` and `@` as examples of codes that
   are not tiles; they now use `` ` ``.
6. **Towns' sites take the old map's town names** (doc 60) where the country suits them, so the 262 unplaced town
   residents can follow them: Oakhaven, Frosthollow, Coldwater, Larkfield, Mirebridge, Southmeadow; and a new one,
   Saltwell, the oasis. These are working names for reserved ground; nothing is built.

## 2. The regions

```
         0        768       1536      2304      3072      3840 4096
     0  RIDGE-   RAINWARD   T H E   W H I T E F A N G S  (snow)
        MERE     HILLS      Highmere  Frosthollow   Kestrel Tarn    Greytarn
   768  (city,   Rainmere   Slate Tarn    (pass to the north)    T H E
        bay W)   Oakhaven                                        G R E Y T E E T H
  1536  CEDAR SOUND~~~~  Alder Mere   THE GREENHOLT    Upper Mere      (dry spurs)
        THE RAINWOOD     (woods, meadow)  Coldwater  THE LARKDOWNS  Dustmere
  2304  (cedar, fern,    Glasswater~~~~          OCHRE  THE RED WASTE
        coast range)         Larkfield    (dry downs) HILLS  mesas  Saltwell (oasis)
  3072  THE SALLOW FENS  Mirebridge     THE SUNFIELDS  badlands  THE SALTPAN   dunes
        (delta, creeks,                 Southmeadow
  3840   dunes, sea)                                            dunes
```

| Region (`territory.region`) | Cells | Where | Ground | Weather |
|---|---|---|---|---|
| Ridgemere (`ridgemere`) | 9 | the north-west corner | the city; round it cleared ground and fir woods | rain |
| The Rainward Hills (`rainward_hills`) | 11 | east of the city | fir and cedar rain forest in the folds, mossy tops, the Whitefangs' first snow; Rainmere | rain |
| The Rainwood (`rainwood`) | 17 | the coast south of the city | cedar and fir rain forest over fern and moss, fallen logs, ferny ravines and hollows, mossy rocky shore, bluffs and coves | rain |
| Cedar Sound (`cedar_sound`) | 7 | an arm of the sea 900 tiles into the Rainwood | steep wooded shores; Oakhaven at its head | rain |
| The Western Sea (`western_sea`) | 6 | the coast's edge | rocks and islets | rain |
| The Whitefangs (`whitefangs`) | 30 | the north | ridged peaks worn into valleys: pine below the tree line (snowy near it), subalpine meadow and krummholz, scree under the rock, bare crests, snowfields (deeper on the north faces and in the hollows) and glaciers; tarns; the pass | snow |
| The Greyteeth (`greyteeth`) | 6 | the north-east | as the Whitefangs, with dry grass on the eastern spurs | snow |
| The Greenholt (`greenholt`) | 32 | the middle and west-middle | gentle swells of meadow and pasture, big woods, copses, hedged fields; Glasswater | clear (temperate) |
| The Larkdowns (`larkdowns`) | 29 | the east-middle | long dry swells of grass, gorse in the hollows | clear |
| The Sunfields (`sunfields`) | 12 | the south-middle | warm grassland and woods, hedged fields | clear |
| The Ochre Hills (`ochre_hills`) | 19 | north to south between the downs and the waste | broken hills of red and yellow sandstone laid in beds, ochre scree, the grass thinning to scrub and sand; no rain to speak of | sandstorm (arid) |
| The Sallow Fens (`sallow_fens`) | 23 | the south-west | reed beds, bog pools, creeks, willow carr, mud flats and dunes on the coast; the delta | fog |
| The Red Waste (`red_waste`) | 34 | the south-east | sand and gravel, scrub and cactus, dune seas, mesas and buttes, banded badlands | sandstorm |
| The Saltpan (`saltpan`) | 6 | the Red Waste's heart | a dry lake of salt round a bitter pool | sandstorm |

241 cells in all; their weather: 50 `rain`, 36 `snow`, 73 `clear`, 23 `fog`, 59 `sandstorm`. A cell's region is what most of it is (`region_of`), so a region's edge cell may hold some of its
neighbour's ground: the borders are blends, not the cells' edges. Cell names are the region's with the town, lake or
works in the cell, or a compass point within the region ("The Red Waste, North-east"); descriptions say what the
country is and which named rivers and lakes are there.

## 3. How the land is made (`tools/worldgen/realm2.py`)

Deterministic from its seed (20261101): every write makes the same realm round the same city.

- **The sea.** West of a coastline drawn from points (rain coast coves and points, then the fens' wide bay), bent by
  noise at two scales, the noise fading to nothing where it meets the city's own shore at y 768. Cedar Sound is a
  winding arm of the sea; 26 rocks and islets stand off the rain coast. Inside the city's cells the sea is the city's.
- **The shape of the land** (on a coarse grid of 8 tiles, then smoothed to tiles): rising from the sea; the fens low,
  the Red Waste a plateau falling to the Saltpan; long swells in the middle; peak chains (the Whitefangs, a front range
  with a gap for the pass, the Greyteeth, the Ochre Hills, the Rainward Hills, a low coast range) laid on bent
  coordinates as crests with saddles and foothills. The mountains (not the low Rainward Hills and coast range, which are
  rounded, with ridged noise for spurs and gullies) are then worn as water would wear them (`carve_range`): the plan's
  mass is drained, a valley runs wherever enough ground drains through it, the floors climb into the range and the land
  rises from each valley to the divides, so the crests are sharp lines between valleys and branch as they do; on the
  tiles, ridged multifractal noise on bent coordinates adds the spurs and gullies. The pass's high valley and
  saddle; basins for Glasswater, the tarns and the Saltpan; the Long River's and the Rainrun's valleys guiding the
  drainage. Heights above 10 are eased under the game's 16.
- **Rain and warmth.** A wind from the west-north-west crosses the land from the sea, dropping rain as it climbs and as
  it goes (and taking it up again over the open sea), so the coast and the mountains' west faces are wet and the far
  side of the Ochre Hills dry; blended with the plan's wetness (the rain coast and the fens wet, the middle fair, the
  waste dry). Warmth falls with latitude and height, the sea keeps the coast mild, the waste is hot.
- **Water.** Priority-flood drainage on the coarse grid (outlets: the sea, the Saltpan, and the map's north, east and
  south edges, where rivers leave for the lands beyond; never through the city's cells): every river runs downhill to
  one of them by construction. Flow is gathered weighted by rain; streams and rivers are traced from it, smoothed,
  given meanders (wider on flat ground), their surfaces made never to rise downstream and to ease into the sea, a lake
  or the river they join, and the land cut to their banks (gentle valleys on the plains, steep ferny ravines on the
  rain coast). Lakes are the basins the flood found, level, their shores above them. In the dry country a river is a
  dry wash of sand and gravel. The fens have their own web of winding creeks and pools.
- **No step but a meant one.** The land is held to a walkable half step a tile (row and column sweeps) except in crags:
  the highest peaks, the mesas' rims, the badlands, the rain coast's bluffs and ravines. What is left steeper is
  marked as cliff.
- **Biomes** by warmth and wetness, their thresholds jittered so they interleave over tens of tiles: snowfield, alpine
  rock and tundra, subalpine meadow and krummholz, montane pine, rainforest, woodland, meadow, heath, steppe, scrub,
  sand, marsh. **The mountains:** the snow line by height and by aspect (a north face or a hollow holds the cold and the
  snow, a crest is swept bare), bare rock on the steep faces and the crests and standing out of the snowfields along
  them, scree at the foot of the rock, glaciers in the high hollows, the pines snowy near the tree line. **Open
  country** (`CALM`) is painted calmly: each tile picked from a mix drifting slowly between a pasture's short grass and
  a hay meadow's tall grass and flowers (and to the meadow in the hollows), over hundreds of tiles, not in blotches;
  big woods and wide meadows by a slow noise; copses in every square of about a hundred tiles, lone trees, and
  hedgerows (shrub, a tree now and then, gaps to pass) round irregular fields, each field its own shade of hay or
  pasture. **The desert's edge** (`dry_edge`, within about 380 tiles of the Ochre Hills' line, south of the Greyteeth):
  no snow, pine, cedar or heather; the wetness eased down so the green thins to dry grass, then scrub, then sand over
  hundreds of tiles; the Ochre Hills themselves a broad belt of broken hills whose sandstone lies in beds (their slopes
  climb in steps), red sandstone on the crests and steep faces, ochre scree and sand below. Then overlays: banded
  badlands (risers red, yellow and grey by height, sandstone flats, sandy gullies), mesa tops of sandstone and dry grass
  with bedded flanks, dune seas, the Saltpan, the coast's mossy rock, boulders, shingle and coves, mud flats and dunes
  by the fens, ferny hollows in the rain forest, river banks (reeds, willow, alder, tall grass; a green strip in the dry
  country), lake shores, the oasis.
- **The city's edge.** The city's own heights by everything kept, easing into the realm's over 44 tiles. Outside the
  walls the rain forest of fir and cedar stands close (the rain coast's wetness makes rain forest of everything within
  about 500-1,100 tiles of the sea north of y 2,100-2,500), with fern and moss under it and ferny ravines; the ground
  is cleared within about 6-30 tiles of everything the city holds (the walls, the gates and the roads out of them, the
  Houses' grounds, every NPC's spot), and every realm road keeps 4 tiles of clear verge each side. The city's common is
  wood pasture (the cattle graze under the trees); the dairies' and orchards' clearings keep a tenth of their trees.
  The shore south of the city is mossy rock, boulders and shingle, with bluffs where the land meets the sea steeply.

## 4. Water

Every river's water surface falls (or holds level) all the way to its mouth: checked tile by tile along every river's
course, from its source to the sea and through the lakes on its way, with no rise at all (`no river runs uphill`). A
river's surface is held at or above a lake upstream of it and at or below one downstream, and the lakes' shore raise
leaves the water running in and out alone (before 2026-11-01's second pass the check sampled every 16th tile and allowed
half a step, and missed every lake's mouth, where the raise lifted the outflow half a step above the lake: 136 rises).

| River | From | To | Length (tiles) | Widest |
|---|---|---|---|---|
| **The Long River** | the Greyteeth's west face (3420, 980) | through the Upper Mere and Glasswater, past Larkfield, through the fens to the sea (900, 3130) | 7,000 | 15 |
| **The Alder** | the Whitefangs (2990, 520) | west under the mountains, through Alder Mere, to the head of Cedar Sound | 5,900 | 12 |
| **The Lark** | the Sunfields (2570, 2650) | west across the south-middle country into the fens and the sea | 3,600 | 15 |
| The Greybeck | the Whitefangs (1920, 470) | south-west into Alder Mere | 3,100 | 10 |
| The Shiver | the Sunfields' south (1970, 4000) | west into the fens and the sea | 2,500 | 8 |
| The Kestrel | the Greenholt (1060, 2100) | south into the fens | 1,700 | 12 |
| The Rainrun | the Rainward Hills (800, 520) | south past Rainmere to the head of Cedar Sound | 1,600 | 6 |
| The Coldbeck | the Sunfields (1920, 3570) | north-west into the fens | 1,400 | 8 |
| The Whitewater | the coast range (720, 1950) | north to the head of Cedar Sound | 1,400 | 12 |

and 33 more unnamed rivers, 49 streams and 4 dry washes (sand and gravel beds with a green strip along them) in the dry
country under the Greyteeth and the Ochre Hills. The fens have their own web of creeks and pools besides. Rivers 4 tiles wide or
more are deep in the middle (`W`, impassable but by bridge) with shallows at the banks; streams are wadeable (`~`).

| Lake | Where | Level | Tiles |
|---|---|---|---|
| **Glasswater** | the plains' great lake; Coldwater on its north shore, Larkfield below its outlet | 1.0 | 133,079 |
| **The Upper Mere** | on the Long River above Glasswater | 2.5 | 48,570 |
| Highmere | a big tarn under the Whitefangs | 5.0 | 27,885 |
| Dustmere | at the foot of the Greyteeth, on the edge of the Red Waste | 5.0 | 20,426 |
| Alder Mere | where the Greybeck meets the Alder | 1.5 | 17,472 |
| Rainmere | in the Rainward Hills behind the city | 2.5 | 8,921 |
| Kestrel Tarn, Greytarn, Slate Tarn, and two unnamed | mountain tarns (ice at the edges of the highest) | 4.0-6.5 | 8,400-10,400 |

**Cedar Sound** reaches 900 tiles into the rain coast; **the Saltpan** is a dry lake about 540 x 340 tiles of salt
flat round a brine pool; **the oasis** at Saltwell is a spring pool among palms and grass.

## 5. The reserved sites

"Physical locations" only: ground made ready, nothing built and nobody placed. Each site is in
`artifacts/realm2/realm-report.json` (`siteList`: id, kind, working name, centre, radius, what it is for, tiles) and
on `artifacts/realm2/overview-labelled.png`.

- **Towns** (7): a level pad of cleared ground about 120 tiles across, on the shore or a river where the town is a
  harbour, lake, river or fen town.
- **Fields and range** (doc 71's food plan): the trees, scrub and boulders cleared off (the country's own grass left,
  dry grass in the dry country), the ground eased, the clearing thinning over the edge. A grain farm is about 363 x 363
  tiles (the user's choice, doc 71 question 10), its corners rounded, laid out as nine fields with hedgerows between
  them round its middle, where the steading and the mill race will be. Range in woods (the pigs' pannage, and the city's
  common in the rain forest) keeps its trees, only the undergrowth grazed off. The eel and fowl marsh is left as it
  is.
- **Works' pads**: a small level pad of the works' own ground (rubble for a mine, rock for a quarry, mud for clay, salt,
  a landing's sand, a cutting's stumps and ferns, a charcoal hearth's bare earth) on the right ground: mines and
  quarries on the mountains' flanks, landings on the shore, mills on a river bank.
- **Roads and tracks.** Roads 3 wide (dirt, bridged over water) from Ridgemere's three gates (the North Road's end, the
  East Road at Fellhall's edge, the South Road) to the towns and between them, routed over the ground (dear on slopes,
  across rivers and in the fens) and graded to climb no more than a half step in two tiles; a footpath 2 wide from
  every other site to the nearest road. The North Road no longer ends at the map's edge for nothing: the Pass Road
  leaves it for Frosthollow.

(The table of sites, with counts and sizes, is in the Progress section.)

## 6. Weather

Each cell's authored weather says its climate (`Core/RatwWeather.cpp`, `climateOf`): `rain` (wet: the rain coast,
the sound, the Rainward Hills), `snow` (cold: the Whitefangs' and the Greyteeth's high cells), `clear` (temperate: the
middle, the downs, the Sunfields; snow in winter where the cold country is near, from `warmthAt`), `fog` (marsh: the
fens), `sandstorm` (arid: the Ochre Hills, the Red Waste and the Saltpan, and any cell mostly of the desert's edge).
The snow line moves with the seasons as doc 60 built it. The rain the wind brings from greener country dries out over
the arid cells (`drynessAt`, below), and a salt flat counts with sand in a cell's ground.

**What the weather system can't do yet:**
- **Heat.** Arid is only "dry, with sandstorms and some overcast"; there is no heat, no hot season, no heat haze or
  thirst. Its sandstorms are frequent (45% of its systems).
- **Mist** is fog: a marsh climate's fog is the same fog as anywhere.
- **Rain shadow** is authored (the dry cells are `sandstorm`) and, since 2026-11-01's second pass, kept by the weather
  too: rain, snow and storms born outside arid country fade out over it (`World::drynessAt`, the share of arid country
  about a point blended across the cells' edges as the warmth is), so the Ochre Hills and the Red Waste have only their
  own rare showers. It is not worked out from the land: the dry cells are those authored dry.
- **Weather is by cell** (256 tiles), blended only in warmth: a cell is wholly one climate.

## 7. Costs (measured)

Measured on DEV before the realm (revision 59, build 52) and with it (build 54, the second write; the later ones
changed only vegetation and shorelines), on this machine. The
tick figures are `world_check BUILD --simulate 6 8 --players 20` (the city's 559 residents, 20 walking players; only
the city has NPCs), on exports of the two builds.

| | City only | With the realm |
|---|---|---|
| Outdoor cells | 9 | 241 (of 256; 15 of open sea left out) |
| Their tiles | 590,000 | 15.8 million |
| Places in the build (cells and interiors) | 828 | 1,060 |
| Doors and seams in the build | 5,522 | 196,816 |
| `world.terrain_chunks` | 36 rows, 1.8 MB | 964 rows, 65 MB (glyphs 3.9 MB compressed, heights 56 MB of jsonb) |
| One build in `world.build_cells` | 4.2 MB | 119 MB (three builds are kept: about 360 MB) |
| The database | 49 MB | about 450 MB live with three realm builds kept (634 MB on disk after four writes, until autovacuum reclaims the rewritten rows) |
| Height overrides (the project's `heights`) | 480,674 | 15.1 million |
| A tick, mean / p99 / p99.9 / worst | 2.49 / 5.13 / 9.5 / 17.1 ms | 2.56 / 5.26 / 11.3 / 24.5 ms |
| After the first minute, p99 | 5.05 ms | 5.08 ms |
| Loading one place (the server streams them) | mean 0.06 ms, worst 7.6 | mean 1.7 ms, worst 8.7 ms |
| `world_check` (loads every place, for its checks) | 182 MB | 1.39 GB |
| A scratch server ready / resident memory | | 2.8-3.0 s / 580 MB (495 places loaded at the start) |
| Atlas: the lean world (outlines and previews) | | 0.2 s, 2.8 MB |
| Atlas: the ground of 32 cells (as they come into view) | | 1.1 s, 4.3 MB |
| Atlas: the whole world with its ground (validate, export) | | 10 s, 3.6 GB in Python |
| Atlas's validation of the project | | 11 s |
| `world_build.py dev` | | 40 s, 3.9 GB |
| The generator (`realm2.py`), a write | | about 135 s to lay out (95 s before the second pass), 35 s to save; 5.3 GB |

- **The server** streams cells (doc 20): only the cells someone is in or beside are loaded, so ticks barely change; the
  worst ticks are the first loads of a cell's ground (256 x 256 tiles and its seams, up to 22 ms). The realm's cells
  hold nobody yet; residents in them will cost what residents in the city cost.
- **The client** draws the cell the wolf is in, and near an edge the ground round it from the cells beside it (a band
  48 tiles wide: "The map across seams" below); its minimap and World Map show only cells it has seen within 512 tiles
  (`World::observe`), so neither grows with the map. A cell's ground is sent once as the wolf enters it.
- **The heights are most of the data.** Every outdoor tile of the realm has a height off its glyph's default (the land
  is nowhere flat at 0), and they are stored as one sparse `"x,y": height` object per chunk. A dense form (a row of
  height codes per chunk, as `encode_heights` already sends to Atlas) would cut the 56 MB to a few MB; not done here.
- **Atlas** loads lean and fetches ground as it comes into view, so editing is unaffected; whole-world actions
  (validate, export, Play) read every chunk: 10-20 s and several GB on the host.
- **Larger?** 24 x 24 cells (2.25 times the area) would cost about the same per tile (about 1 GB of database with its
  builds, 8 GB to generate). 16 x 16 is four times the old realm of doc 60 (8 x 8), and the roads from the city to
  the far towns run 3,000-5,600 tiles; it was chosen as the recommendation and built.

## 8. Open questions

1. **The North Road's last stretch** still runs to the map's north edge (the city's tiles are kept); the Pass Road
   leaves it near the edge for Frosthollow. It could be ended at a waymark, or the map grown north one row.
2. **The towns' names** are the old map's (so the unplaced town residents can follow); the user may want others.
3. **The open sea** south-west of the fens is left off the map (15 cells). Add it back if boats are to sail there.
4. **Spares.** A few extra landings and a copper mine are reserved and marked "spare"; doc 71's placement can take or
   drop them. (The vineyard's and the salt works' second places were spares too; with two of each, both are real.)
5. **A server built before this branch** doesn't know the four new tiles and refuses a cell holding one: the city's
   nine cells use none of them (so the city still loads), but 578 of the realm's 964 chunks do. Merge before running DEV's
   server, Atlas or the tools from another branch.
6. **The dense heights.** Storing a chunk's heights as rows of codes rather than a sparse object would cut the
   database's terrain by about a factor of ten (section 7).

## Progress

### Built and written (2026-11-01)

Branch `r2-realm`, from `ridgemere2` (0ac680c). Backup before anything was written:
`Saved/Backups/ratw_ridgemere2-before-realm.dump` (main repo; DEV at revision 59, build 52). Each write also backs up
into `artifacts/backups/`.

**Built.**
- `tools/worldgen/realm2.py`: the realm (`Realm2`: the city kept, the sea, the land, rain and warmth, drainage, heights,
  water, biomes, sites, roads), its cells, its checks, the previews and the report, `expand(project)` (the city's cells
  as the city writer made them, the realm laid round them), `write` (back up, check, save as the same map, tell
  Atlas). The city as its writer made it is kept in `artifacts/realm2/city-cells.json.gz`, so that writing the realm
  again starts from the city and not from the realm written before; a write refuses if the city's built ground has
  changed since (the city writer, which lays the realm itself, is then the one to run).
- `tools/worldgen/ridgemere2.py`: `write` lays the realm round the city after its own checks (and refuses if the realm's
  fail), so the city writer and the realm writer make the same map.
- `Data/Terrain/terrain.json`: Sandstone `(`, Salt flat `)`, Cactus `?`, Willow `@` (the C++, Atlas, DM and client
  tables regenerated). Tests that used `?`, `@` as "not a tile" now use `` ` `` (Editor, DM, `authoring_tests`,
  `world_tests`, `test_terrain_catalog`, `test_map_editor`, `test_live_edit`).
- `tools/test_realm2.py`: the pieces (components against a flood fill and the game's step rule, the flood draining
  every node downhill to an outlet, the slope limiter, the plan's ids and roads, the new tiles), and with `RATW_SLOW=1`
  the whole realm round DEV's city: every check, the regions the user asked for, the reserved acreage, Ridgemere in the
  north-west corner.

**Found and fixed on the way: a returning wolf crashed the server** (`Core/RatwGame.cpp`, `enterCharacter`). A
character coming back was put where it had left, without loading that place: in a streamed world a cell is let go two
game minutes after the last wolf leaves it, so a wolf that left anywhere but a cell kept loaded by others, and came
back later, was given a cell with no ground, and the server segfaulted building its view (`visibleTileMask`). This was
in the city-only world too (reproduced in `w_ridgemere_1_0`, the cell north of the spawn: walk in, leave, wait, come
back); the realm, with 232 cells nobody lives in, would have made it common. Now the place is loaded first (or, if it
can't be, the wolf wakes at the spawn). Verified on a scratch server: the same wolf leaves in `w_ridgemere_1_0` and in
`w_ridgemere_6_2`, comes back after the cells unloaded, and is there. (The guard first put into `visibleTileMask` for a
cell without its ground did not end the crash, only moved it, as review found: `observe` then wiped the wolf's memory
of the place and `snapshot` read tiles that weren't there; and the spawn the wolf fell back to might itself not be
loaded. Fixed properly below, "Sight across seams, and the map's look", item 1.)

**Written into DEV** (`ratw_ridgemere2`): revision 59 to 67 over four writes (the last the one described here; each
write raises it by two, the save and the editors' reload), builds 53-56 (56 is the realm as described). The map identity is kept, `map-20261031-6b080cecb7f6` ("Ridgemere 2.0: the city (doc 67)"), its recorded layout
now the realm's. All 14 checks pass (`artifacts/realm2/realm-report.json`, `checks`):

- every kept tile of the city as it was, code and height (0 differ);
- every door the city's own ground reached still reached from the spawn (three Sump back doors open on courts reached
  only through buildings, as before);
- every NPC's outdoor spot and patrol post still reached; the three gate roads go on into the realm;
- every reserved site reached from the city; 7 towns; in every cell most open ground reached from the city (99.2% of
  all open ground is one walk from the spawn);
- no river runs uphill; heights within +-16; no "dog"; the cells tile the map.

**Walked** on a scratch server (build 56, `--dev-identity --speed 40`), with the real client in headless Chromium: a
wolf leaves the spawn on Market Square, goes out of a gate and along the roads to a town, clicking a point every 20
tiles of a route worked out over the ground by the game's step rule, and crossing every seam on its own. All seven
walks arrived:

| Walk | Tiles | Seams crossed |
|---|---|---|
| North Gate, the Pass Road, to Frosthollow | 3,252 | 14 |
| East Gate, the East Road, to Coldwater | 3,184 | 17 |
| South Gate, the Sound Road, to Oakhaven | 1,546 | 6 |
| South Gate, the Sound and Coast Roads, to Mirebridge | 3,678 | 16 |
| East Gate, the East and Downs Roads, to Saltwell | 5,648 | 27 |
| East Gate, by Coldwater and Larkfield, to Southmeadow | 5,532 | 27 |
| East Gate, the East and River Roads, to Larkfield | 4,046 | 22 |

Then each town's wolf (the same wolves, gone and come back: the server's fix above) walked off the road, overland
by the step rule, into the country round it; all six arrived: from Frosthollow up into the Whitefangs' snow (379
tiles), from Saltwell into the dune sea (467) and on down to the Saltpan (723), from Mirebridge into the fens (577),
from Oakhaven into the Rainwood (719), from Coldwater onto the Larkdowns (567).

Two things the walks showed about the game, not the realm: a click 20 tiles along a road now and then left the wolf
standing short, and clicking again from where it stood went on (3-9 times a road walk, 11-14 in the mountains, the
fens and the Rainwood); and a wolf standing on a cell's last
tile, clicked just beyond the seam, finds no route (clicked from a tile or two back, it crosses). The walking scripts are
not committed (they hard-code this machine's scratch paths); the method is above.

Atlas's validation passes (`check_project(..., for_game=False, playable=True)`); `world_build.py dev` builds;
`world_check` loads all 1,060 places with no failures and runs 6:00-8:00 with 20 players (money conserved, no resident
without a route, the same 5 of 559 held as before the realm).

### The reserved sites, as written

68 sites: 7 towns; for doc 71's 29 food producers 2 grain fields with their 2 mill banks, 5 orchards, the vineyard (and
a spare place for it), 3 dairies with 3 cattle ranges, 3 piggeries with their 3 pannage woods, 2 sheep and goat runs,
2 warrens, 3 bee garths, 6 landings for the sea, lake and river boats (and 3 spare), the eel and fowl marsh, the salt
works on the Saltpan (and a spare salterns on the fens' coast); for the industries 5 timber cuts (3 in the rainforest, 2
in the foothills), 2 charcoal hearths, 4 quarries (granite and slate for the city, lime, sandstone), 2 clay pits and 6
mines (5 in the N/NE mountains: iron, lead and silver, silver, tin; a spare copper mine in the Ochre Hills).

**Acreage** (the user: "somewhere in the middle"): the farms' own fields 401,850 tiles (**6.1
cells**), the shared range 519,584 tiles (**7.9 cells**), together 14.1 cells, as asked
(6-7 and 7-8); the spares another 0.4. The two grain fields are about 105,000 tiles each (some 400
by 300 tiles), the sheepwalks 118,000-125,000, the cattle ranges 57,000-64,000: a field takes minutes to walk across.

Roads: 11, 14,729 tiles (the Pass Road from the North Road to Frosthollow 2,363; the
East Road to Coldwater 2,111; the Sound Road to Oakhaven 801; the Coast Road, River Road, Fen Road, High Road, South
Road, Waste Road and Downs Road between the towns; the Pass on north from Frosthollow to the map's edge), with bridges
where they cross rivers. Tracks: 60, 13,287 tiles, one from every other site to the
nearest road.

| Site | Kind | Working name | Centre (x, y) | Tiles | For |
|---|---|---|---|---|---|
| `oakhaven` | town | Oakhaven | 1012, 1220 | 11,167 | town |
| `frosthollow` | town | Frosthollow | 2276, 796 | 11,234 | town |
| `coldwater` | town | Coldwater | 1972, 1788 | 10,617 | town |
| `larkfield` | town | Larkfield | 1676, 2372 | 11,110 | town |
| `mirebridge` | town | Mirebridge | 1468, 2868 | 9,923 | town |
| `southmeadow` | town | Southmeadow | 2148, 3348 | 10,569 | town |
| `saltwell` | town | Saltwell | 3292, 2932 | 10,826 | town |
| `grain_long` | farmland | the Long River grain | 1884, 2556 | 105,610 | grain farm 1 (large, with a water mill) |
| `mill_long` | mill | the Long River mill race | 1670, 2581 | 292 | grain farm 1's water mill |
| `grain_lark` | farmland | the Lark grain | 2276, 2820 | 105,495 | grain farm 2 (large, with a water mill) |
| `mill_lark` | mill | the Lark mill race | 2513, 2697 | 396 | grain farm 2's water mill |
| `orchard_greenholt` | orchard | the Greenholt orchard | 1548, 1548 | 13,996 | orchard (central hills) 1 |
| `orchard_coldwater` | orchard | the Upper Mere orchard | 2164, 1700 | 13,623 | orchard (central hills) 2 |
| `orchard_larkfield` | orchard | the Larkfield orchard | 1900, 2852 | 14,045 | orchard (central hills) 3 |
| `orchard_rainward` | orchard | the Rainward orchard | 636, 900 | 13,360 | orchard (NW coast valley) 1 |
| `orchard_coastvale` | orchard | the Coastvale orchard | 484, 1564 | 13,698 | orchard (NW coast valley) 2 |
| `vineyard_ochre` | vineyard | the Ochre vineyard | 2756, 2700 | 31,835 | the vineyard (large; SE desert edge) |
| `vineyard_south` | vineyard | the Southmeadow vines | 2404, 3148 | 24,329 | spare: the vineyard (central hills) |
| `dairy_southgate` | dairy | the Southgate dairy | 428, 964 | 15,985 | cattle and dairy farm 1 |
| `dairy_rainrun` | dairy | the Rainrun dairy | 820, 996 | 17,489 | cattle and dairy farm 2 |
| `dairy_rainward` | dairy | the Rainward dairy | 892, 556 | 16,690 | cattle and dairy farm 3 |
| `range_city` | pasture | the city common | 620, 636 | 63,894 | cattle range (shared, near the city) |
| `range_southgate` | pasture | the Southgate range | 484, 1204 | 56,700 | cattle range (shared, NW coast) |
| `range_rainrun` | pasture | the Rainrun range | 1052, 884 | 61,353 | cattle range (shared, NW coast) |
| `pigs_greenholt` | pigfarm | the Greenholt piggery | 1404, 1692 | 6,306 | pig farm 1 |
| `pigwood_greenholt` | woodpasture | the Greenholt pannage | 1244, 1684 | 31,797 | pig farm 1's woods (shared range) |
| `pigs_alder` | pigfarm | the Alder piggery | 1692, 1148 | 6,315 | pig farm 2 |
| `pigwood_alder` | woodpasture | the Alder pannage | 1676, 988 | 32,196 | pig farm 2's woods (shared range) |
| `pigs_south` | pigfarm | the Southwood piggery | 1284, 2012 | 6,218 | pig farm 3 |
| `pigwood_south` | woodpasture | the Southwood pannage | 1124, 1988 | 31,384 | pig farm 3's woods (shared range) |
| `sheep_downs` | pasture | the Larkdowns sheepwalk | 2516, 2116 | 124,619 | sheep and goat run (central downs) |
| `sheep_foothills` | pasture | the Greyteeth sheepwalk | 2836, 1540 | 117,641 | sheep and goat run (N/NE foothills) |
| `warren_dunes` | warren | the Fenshore warren | 924, 3604 | 7,214 | rabbit warren (SW marsh dunes) |
| `warren_waste` | warren | the Ochre warren | 2988, 2620 | 8,348 | rabbit warren (SE desert edge) |
| `fishing_sound` | fishing | the Sound landing | 684, 1389 | 301 | sea fishing boat 1 (Ridgemere's sound) |
| `fishing_northcove` | fishing | North Cove | 132, 1060 | 233 | sea fishing boat 2 (a north cove) |
| `fishing_delta` | fishing | the Long River mouth | 933, 3054 | 339 | sea fishing boat 3 (SW marsh coast) |
| `fishing_fencoast` | fishing | the Fen shore | 694, 3800 | 338 | sea fishing boat 4 (SW marsh coast) |
| `fishing_glasswater` | fishing | Glasswater landing | 1923, 2273 | 223 | lake and river boat 1 |
| `fishing_uppermere` | fishing | the Upper Mere landing | 2364, 1627 | 227 | lake and river boat 2 |
| `marsh_eels` | marsh | the eel creeks | 1084, 3436 | 17,438 | the eel and fowl marsh |
| `bees_rainward` | apiary | the Rainward bee garth | 956, 300 | 1,635 | beekeeper 1 |
| `bees_greenholt` | apiary | the Greenholt bee garth | 1268, 1140 | 1,703 | beekeeper 2 |
| `bees_south` | apiary | the Sunfields bee garth | 2004, 3100 | 1,597 | beekeeper 3 |
| `salt_saltpan` | salt | the Saltpan workings | 3452, 3332 | 795 | the salt works (SE desert pan) |
| `salt_fencoast` | saltcoast | the Fen salterns | 868, 3346 | 1,041 | spare: the salt works (SW marsh coast) |
| `fishing_southcove` | fishing | Seal Rocks | 188, 1795 | 393 | spare: sea fishing |
| `fishing_larkfield` | fishing | the Larkfield weir | 1550, 2442 | 416 | spare: river boat |
| `fishing_rainmere` | fishing | Rainmere landing | 1004, 684 | 247 | spare: lake boat |
| `timber_rainwood` | timber | the Rainwood cut | 684, 1124 | 1,863 | timber (coastal rainforest) |
| `timber_coast` | timber | the coast range cut | 356, 1932 | 1,811 | timber (coastal rainforest) |
| `timber_sound` | timber | the Soundside cut | 700, 1500 | 1,709 | timber (coastal rainforest) |
| `timber_front` | timber | the Kestrel pines | 1724, 836 | 1,879 | timber (foothills) |
| `timber_upper` | timber | the Greyteeth pines | 2636, 1020 | 1,772 | timber (foothills) |
| `charcoal_greenholt` | charcoal | the Greenholt charcoal hearths | 1396, 1868 | 1,062 | charcoal |
| `charcoal_rainwood` | charcoal | the Rainwood charcoal hearths | 684, 1244 | 1,030 | charcoal |
| `quarry_granite` | quarry | the Rainward granite quarry | 1012, 372 | 1,265 | stone (granite) |
| `quarry_slate` | quarry | the Rainward slate quarry | 1316, 660 | 1,056 | stone (slate) |
| `quarry_lime` | quarry | the lime quarry | 1516, 772 | 1,263 | stone (limestone) |
| `quarry_sandstone` | quarry | the Ochre sandstone quarry | 2868, 2892 | 1,243 | stone (sandstone) |
| `clay_mirebridge` | clay | the Mirebridge clay pits | 1180, 3276 | 592 | clay |
| `clay_fens` | clay | the Sallow clay pits | 1636, 3340 | 624 | clay |
| `mine_frost_iron` | mine | the Frosthollow iron mine | 2444, 500 | 637 | ore (iron; N mountains) |
| `mine_frost_lead` | mine | the Kestrel lead and silver mine | 2052, 636 | 606 | ore (lead, silver; N mountains) |
| `mine_grey_iron` | mine | the Greyteeth iron mine | 3364, 1228 | 618 | ore (iron; NE mountains) |
| `mine_grey_silver` | mine | the Greyteeth silver mine | 3524, 956 | 631 | ore (silver; NE mountains) |
| `mine_grey_tin` | mine | the Greyteeth tin mine | 3748, 1348 | 625 | ore (tin; NE mountains) |
| `mine_ochre_copper` | mine | the Ochre copper mine | 2988, 2388 | 629 | spare: ore (copper) |

### Sight across seams, and the map's look (2026-11-01)

Branch `r2-map-display`, from `ridgemere2` (268c455). In-game screenshots of the realm showed six things wrong with
the map; the user approved fixing them all, in the server and the browser client. Nothing was written to DEV.

1. **The crash, fixed properly.** A wolf is never in a cell without its ground, on every way in: `World::addPlayer`
   puts it at the spawn when its place can't be loaded (it ignored that before); a streamed world never lets the
   spawn go (`World::stream`), so that fallback always stands on ground; `Game::enterCharacter` falls back to it;
   `World::restore` puts a player whose place can't be read at the spawn; a tournament bout loads its ring first
   (`World::tourneyBout`); the rest (crossings, `teleport`, the DM's moves, arrest) already loaded the place or
   refused. Should one ever stand in a cell without its ground, `observe` leaves what it remembers alone and
   `snapshot` shows that memory and nothing in sight. The review's reproducer is now `stream_tests`'
   `aWolfAlwaysStandsOnGround`.
2. **Sight and the local map run on across seams.** The view no longer stops at a cell's edge (lakes, rivers and the
   Saltpan were cut off mid-view).
   - *Server:* `World::sightBeyond` traces lines from the wolf to the tiles of the loaded cells beside its own, at
     the same level outdoors, within its sight, over a window of both cells' ground (and their closable fixtures).
     The cells beside each outdoor cell are found by where they lie, not by their doors (`indexBeside`, made with the
     fixture index). What it sees there is remembered in those cells, which it has then glimpsed, not visited (a
     glimpse outdoors may now keep what was seen of it; the minimap draws it). The cell across a corner, which no
     seam joins to the wolf's, is loaded while a player is within sight of it (`stream`).
   - *Wire:* near an edge (within 96 tiles), the snapshot carries the ground round the cell, a band 48 tiles wide
     (`Fringe`, past any wolf's sight): its rows, heights and (for a client that doesn't work out its own sight)
     visibility, as `cell.fringe`, `cell.fringeRows`, `cell.fringeHeights` and `fringeVisibility`, sent as row
     edits like the cell's own (`RatwSections.cpp`, `Client/src/net/sections.ts`). The world makes the band again
     only when the wolf's view changes, and shares it until then.
   - *Client:* `GameState` reads the band (`Client/src/game/fringe.ts`), answers for any tile in or round the cell
     (`glyphAt`, `seenAt`, `heightAt`), and the page's own sight runs over it (the walker's grid is the cell with
     the band round it, the band solid to walking: the server takes a wolf over a seam). The ground is drawn and the
     camera follows the wolf out to the band's edge, so the map runs on seamlessly; a click past the edge heads for
     the seam there.
   - *Cost:* see below.
3. **Ridgemere's wall now draws.** Its tiles stand at height 2 in a ditch below the road (3.5), so the line to a wall
   tile's foot passed under the bank before it and the wall was never seen, nor sent. An opaque tile now stands 2.5
   above its own ground as a thing to be seen (`RatwSight.h`, `WallTop`): a blocking tile one looks at is itself
   seen. From outside the South Gate the wolf now sees the wall's face along its length.
4. **Ground cover thins sight rather than walling it off** (doc 22, "Cover that thins sight"). Ferns (stature 0.7)
   on a half step above the wolf rose above its eye and hid everything past them, and every tree hid all behind it,
   so the Rainwood and the ground round the gate were murk. Each tile now has a `hides` in the catalog: trees' trunks
   hide part (0.2-0.35), low cover a little (ferns 0.06, reeds 0.1, shrubs 0.15), solid masses all, as before; enough
   of them together hide all. Outside the South Gate (on tall grass among ferns) the wolf sees 2,006 of the 2,820
   tiles within its sight, not 556; in the Rainwood (`w_ridgemere_1_7`, 182, 66) 902, not 20: the trunks and ferns
   round it and down the clearings. A body is made out only along a clear line, as before (`bodySeen`): a trunk or a
   fern between still hides a wolf, so noticing, sneaking and hunting are as they were (doc 40; `hunt_sim` below).
5. **Rock.** Bare rock drew as a tiny dot, and scree (`░`, a shade block) as a flat grey square with no glyph: the
   mountains looked like placeholders. Bare rock is now `▴` (crags, a lighter cool grey), scree `჻` (loose stones,
   warm grey); boulders keep `●`. Of the four new tiles, sandstone's `▞` drew as a wall of orange bricks and is now
   `▰` (bedded slabs, a little darker); salt flat `⋯`, cactus `⍦` and willow `Ϋ` draw well as they were.
6. **Height on the local map.** The server now sends the heights of remembered ground too (its shape is remembered
   with it), so the map shades what it has seen, not only what is in sight. Each tile is shaded by its slope, read
   over one, two and three tiles each way (`Client/src/game/terrainLayer.ts`, `Relief`): a wash of light on slopes
   turned to the north-west, of shade on those turned away, in the floor and a little in the glyph. A dune's steep
   lee reads as a dark band, its long back faintly lit, a valley's sides apart. Remembered ground gets faint contours
   too. Kept subtle: on level ground nothing changes.

**Cost** (`world_check` with this machine's export of build 56, one game hour from 6:00 with 20 players, before and
after, twice each; the new `--at-seams` puts the players after the first by the seams between outdoor cells, walking
over them and back):

| (two runs each) | Before | After |
|---|---|---|
| 20 players beside residents: a tick, mean | 2.73, 2.54 ms | 2.66, 2.95 ms |
| the same, p99 after the first minute | 5.47, 5.33 ms | 5.30, 5.95 ms |
| a player's view (5 a second), mean | 0.24, 0.22 ms | 0.24, 0.27 ms |
| 19 of them by seams: a tick, mean | 3.36, 3.39 ms | 4.26, 4.64 ms |
| the same, p99 after the first minute | 6.58, 5.59 ms | 7.04, 7.65 ms |
| a player's view, mean | 0.44, 0.44 ms | 0.57, 0.61 ms |
| their sight worked out in the tick (`views`), a tick | 0.20 ms | 0.28 ms |
| resident memory, by seams | 1.03 GB | 1.13 GB (the cells across corners) |

In the city the change is lost in the noise; with every player walking over a seam a tick costs about a millisecond
more (a fifth of it the sight past the seam, most of the rest the views: the wider sight, and each glimpsed cell's
remembered ground copied into the World Map as a visited one's always was), well inside the 50 ms tick.

A wolf walking by a seam (a snapshot on each new tile, its sight worked out again): 0.94 ms before, 1.34 ms after,
in the Greenholt by Glasswater; in the Rainwood 1.06 and 1.92 ms (it now sees much more there); standing still 0.11
and 0.16 ms. The band's rows, made again only as the view changes, take the game 0.13 ms.

**Hunting** (`hunt_sim`, 20 runs of 30 game minutes per suite, kills per wolf per hunter-hour; its ground is grass
with broadleaf trees and shrubs):

| Suite (stay / roam) | Before | Bodies seen through cover, dimmed (a first try) | As built |
|---|---|---|---|
| lone | 10.6 / 16.9 | 9.0 / 14.0 | 10.6 / 16.9 |
| pair | 14.5 / 18.3 | 15.8 / 14.9 | 14.5 / 18.3 |
| three | 18.0 / 20.3 | 12.9 / 18.7 | 18.0 / 20.3 |
| four | 22.0 / 22.3 | 15.2 / 16.9 | 22.0 / 22.3 |
| companion | 12.1 / 15.4 | 10.4 / 10.8 | 12.1 / 15.4 |

Letting the game see a stalker through thin cover (a fern, a shrub, one trunk dimming rather than hiding it) cost the
hunters a fifth of their kills, a third in a party of four; with a body needing a clear line, as before, the sim's
output is the same to the last figure.

**Screenshots**, before and after on scratch servers of DEV (build 56), with the screenshot tool the realm's
screenshots were taken with (a dev identity put at each place by a scratch copy's dev-tools `goto`, not committed):
outside the South Gate (the wall, the ferns), the Rainwood, the Saltpan's corner of four cells (the pan runs on over
both seams), Glasswater's south shore by a seam, the Greyteeth (rock), the Red Waste's dunes (shading) and sandstone
by the Saltpan.

**Tests.** `stream_tests`: `aWolfAlwaysStandsOnGround` (the crash's reproducer: a wolf back in a cell let go, one
whose place can't be read, a restore, and a wolf put in a cell without its ground keeping its memory, its view empty)
and `sightRunsOnAcrossSeams` (seeing past a seam, a wall there hiding what is behind it, the glimpse remembered and
restored, the cell across a corner loaded and seen into); `playersCrossAndReturn` now walks on to `c_4_0`, as the
spawn no longer unloads. `world_tests`: a wall's top over its bank, one pine and ferns thinning sight, a stand of
pines hiding it, a boulder hiding all, and the 40,000-ray check of the per-tile rule against every sample now with
ferns and boulders in the ground. Client: `fringe.test.ts` (the band's layout, read, drawn, clicked into, let go; the
page's sight over it, its walking kept to the cell). A clean build of `build-core` and `ctest`: 76 of 76; the client's
132 tests; Atlas's `tsc` clean (its tests fail only the Greyfen count, as before); `test_terrain_catalog`,
`test_realm2`, `test_live_edit`, `test_live_map` pass (`test_map_editor` only the Greyfen count, as before).

**Still open.**
- Wolves across a seam are not seen: sight runs on over the ground, but who stands in the cell beside is shown only
  once one is in it (`snapshot`'s wolves are its own cell's), so a wolf can look into the next cell and see it
  empty. Worth doing with the hearing and scent across seams.
- A click far past the edge walks to the seam opposite it and stops there, rather than on to the spot.
- The band is sent within 96 tiles of an edge: most of a 256-tile cell. It is made only when the view changes.

### Tests (2026-11-01)

- **C++:** a clean build of `build-core`, then `ctest`: 76 of 76 pass.
- **Python:** every `tools/test_*.py`. Two fail, both as before this work: `test_map_editor`'s Greyfen count (11, not
  10), and `test_ridgemere2`'s `test_the_food_trades_and_the_carters` (16 household businesses, the check allows 15:
  it fails the same on the project as it was before the realm, revision 59). `test_realm2` passes, with `RATW_SLOW=1`
  too (the whole realm round DEV's city, about two minutes).
- **Atlas** (`Editor`): `tsc --noEmit` clean; its tests pass but the same Greyfen count. **DM** tests and the **client**
  (`tsc`, 129 tests) pass.

### The terrain's second pass (2026-11-01, branch `r2-map-terrain`)

The user approved a review's findings and a new look for the land: ridged peaks, not flat white blobs; calmer
plains; a real blend at the desert's edge; dense Pacific Northwest rain forest round Ridgemere. Built on
`r2-map-terrain` from `ridgemere2` (268c455). **Not written into DEV** on its branch: DEV still held the first realm
(build 56); the write was a separate step (`python3 -m worldgen.realm2 write`, which backs up first), made after the
merge below.

**Fixed.**
- **The new tiles in the tables keyed by tile** (`Data/Wild`): hunting's ground kinds have willow `@` as forest, cactus `?`
  as shrub, sandstone `(` as rock and the salt flat `)` as sand (and rubble `X`, which the realm now lays as scree, as
  rock); foraging has the willows (firewood, reeds, mushrooms, herbs), cactus with the scrub, and the salt crust
  (salt). In the C++, a willow or a cactus screens a stalker as a tree does (`RatwBattle.cpp`, `screenGlyph`), and a
  salt flat counts with sand in a cell's ground for its climate (`RatwAuthoring.cpp`).
- **Water ran uphill at every lake mouth** (136 rises by the strict check): the lakes' shore raise lifted the river tiles
  at the outflow half a step above the lake. The raise now leaves rivers, washes and creeks alone, and a river's surface
  is held between the lakes it runs through. The check is strict: every tile of every river's course, through its
  lakes, no rise at all (it sampled every 16th tile and allowed half a step).
- **The city's snapshot was overwritten by every run** (`expand` called `keep_snapshot` itself), so a preview, a dry run
  or a refused write replaced `artifacts/realm2/city-cells.json.gz`. Only a write that saved keeps it now (`realm2.write`
  and `ridgemere2.write`, after the transaction).

**The look** (section 3 has the methods): the mountains worn into valleys and sharp branching crests, the snow line by
height and aspect, rock on the crests and steep faces and standing out of the snow, scree below, pine under the tree
line, glaciers in the high hollows; the plains calm (meadow and pasture drifting over hundreds of tiles, big woods,
copses, hedged fields); the Ochre Hills a belt of bedded red and yellow sandstone and ochre scree, the green thinning to
dry grass, scrub and sand across them, with no snow, pine, cedar or heather and arid weather (their 19 cells
`sandstorm`; the weather's own rain dries out over arid country, `drynessAt`); dense fir and cedar round Ridgemere with
clear ground by its walls, gates, roads and the Houses' grounds, mossy rocky shore and bluffs. The city's tiles are
untouched (0 differ).

**The sites.** The two grain farms are about 363 x 363 tiles each (the user's choice), laid out as nine hedged fields;
there are two vineyards and two salt works, both places of each now real sites (they were "the vineyard" and "the salt
works" with a spare each); the city's common is wood pasture in the rain forest; the mills sit on a river 3 tiles wide
or more. The farms' own fields are 446,002 tiles (**6.8 cells**), the shared range 522,564 (**8.0 cells**); spares
0.02. Roads 14,814 tiles, tracks 60 (12,756 tiles). The sites that moved more than 40 tiles or changed (centre and tiles
before in brackets; all 68 are in `artifacts/realm2/after/realm-report.json`):

| Site | Kind | Centre (x, y) | Tiles | For |
|---|---|---|---|---|
| `grain_long` | farmland | 1828, 2580 (1884, 2556) | 125,664 (105,610) | grain farm 1 (large, with a water mill) |
| `mill_long` | mill | 1628, 2612 (1670, 2581) | 260 (292) | grain farm 1's water mill |
| `grain_lark` | farmland | 2164, 2836 (2276, 2820) | 126,319 (105,495) | grain farm 2 (large, with a water mill) |
| `mill_lark` | mill | 2207, 3026 (2513, 2697) | 348 (396) | grain farm 2's water mill |
| `vineyard_ochre` | vineyard | 2756, 2692 (2756, 2700) | 17,577 (31,835) | vineyard 1 (SE desert edge) |
| `vineyard_south` | vineyard | 2396, 3148 (2404, 3148) | 17,036 (24,329) | vineyard 2 (central hills) |
| `range_city` | woodpasture | 620, 636 (620, 636) | 63,884 (63,894) | cattle range (shared, near the city) |
| `range_rainrun` | pasture | 1060, 940 (1052, 884) | 64,730 (61,353) | cattle range (shared, NW coast) |
| `pigs_greenholt` | pigfarm | 1420, 1636 (1404, 1692) | 6,227 (6,306) | pig farm 1 |
| `pigwood_greenholt` | woodpasture | 1260, 1628 (1244, 1684) | 32,371 (31,797) | pig farm 1's woods (shared range) |
| `pigs_alder` | pigfarm | 1644, 1428 (1692, 1148) | 6,227 (6,315) | pig farm 2 |
| `pigwood_alder` | woodpasture | 1604, 1268 (1676, 988) | 28,821 (32,196) | pig farm 2's woods (shared range) |
| `pigs_south` | pigfarm | 1356, 2060 (1284, 2012) | 6,364 (6,218) | pig farm 3 |
| `pigwood_south` | woodpasture | 1196, 2068 (1124, 1988) | 32,216 (31,384) | pig farm 3's woods (shared range) |
| `fishing_glasswater` | fishing | 1948, 2283 (1923, 2273) | 265 (223) | lake and river boat 1 |
| `fishing_uppermere` | fishing | 2412, 1596 (2364, 1627) | 281 (227) | lake and river boat 2 |
| `bees_rainward` | apiary | 948, 356 (956, 300) | 1,625 (1,635) | beekeeper 1 |
| `salt_saltpan` | salt | 3420, 3316 (3428, 3316) | 2,781 (808) | salt works 1 (SE desert pan) |
| `salt_fencoast` | saltcoast | 868, 3346 (868, 3346) | 2,030 (1,041) | salt works 2 (SW marsh coast) |
| `fishing_rainmere` | fishing | 964, 740 (1004, 684) | 253 (247) | spare: lake boat |
| `timber_front` | timber | 1692, 948 (1724, 836) | 1,739 (1,879) | timber (foothills) |
| `timber_upper` | timber | 2732, 972 (2636, 1020) | 1,764 (1,772) | timber (foothills) |
| `charcoal_greenholt` | charcoal | 1348, 2140 (1396, 1868) | 1,041 (1,062) | charcoal |
| `quarry_lime` | quarry | 1588, 788 (1516, 772) | 1,245 (1,263) | stone (limestone) |
| `mine_frost_iron` | mine | 2516, 724 (2444, 500) | 628 (637) | ore (iron; N mountains) |
| `mine_grey_silver` | mine | 3500, 1004 (3524, 956) | 599 (631) | ore (silver; NE mountains) |

All 14 checks pass (the whole realm round DEV's city, read only), 98% of all open ground one walk from the spawn.

**Tests.** `test_realm2`: the new tiles in the wild tables, the range carver and the ridged noise; with `RATW_SLOW=1`, a
run that keeps no snapshot, the strict river check again, the farms and works doc 71 asked for, the desert's edge dry
(no snow, pine, cedar or heather; red rock showing; the Ochre Hills' cells arid), and fir and cedar round the city. C++:
`weather_tests` has the rain shadow (rain on green downs, a quarter or less of it in the waste beyond), `hunt_tests`
the new tiles' ground kinds. A clean build of `build-core`, then `ctest`: 76 of 76 pass. `test_realm2` passes (17, with
`RATW_SLOW=1`: about two and a half minutes, 8 GB), `test_item_catalog` passes, and `test_ridgemere2` fails only its known
`test_the_food_trades_and_the_carters` (16 household businesses against 15, as before the realm).

### Both merged and written into DEV (2026-11-01, branch `r2-map-fix`)

`r2-map-display` and `r2-map-terrain` merged on `r2-map-fix` from `ridgemere2` (268c455), with no conflicts. The two
meet in `World::noticeSenses`: a willow or a cactus screens a stalker (`screenGlyph`), and a body is made out only along
a clear line (`bodySeen`); the willow's and the cactus's `hides` were already in the catalog.

**Tests.** A clean build of `build-core`, then `ctest`: 76 of 76. `test_world_sync`, `test_studio`, `test_world_store`,
`test_live_edit`, `test_dungeon_master` and `test_realm2` (with `RATW_SLOW=1` too) pass; `test_map_editor` fails only
the Greyfen count (11, not 10). Atlas: `tsc` clean, its tests fail only the same Greyfen count (its `src/lib` tests,
which `npm test` skips after that failure, pass: 65 of 65). The client: `tsc` clean, 132 tests pass.

**Written.** Backup first: `Saved/Backups/ratw_ridgemere2-before-map-fix.dump` (main repo; DEV at revision 67, build
56), and the write's own in `artifacts/backups/`. `python3 -m worldgen.realm2 write --world dev`: all 14 checks pass
(every kept tile of the city as it was: 0 codes, 0 heights differ); the same map, `map-20261031-6b080cecb7f6`, its new
layout recorded; revision 69 (68, +1 for the editors' reload); no truth lines (nothing the game server changed was kept
or in conflict); cells, interiors, links, residents and routes as before (241, 819, 513, 1,083, 12). The nine city
cells read back before and after: their 242,950 kept tiles (the built ground, the walls' box, the Houses' grounds)
identical, the wild ground round them remade (243,260 of the other 346,874 tiles). Then `world_build.py dev`: build 57.
`artifacts/realm2/after/` now holds the realm as written: the same realm as the branch's (its report differs only in
timings), drawn with the merged catalog's rock, scree and sandstone colours.

**Screenshots** (the fourteen places of the realm's in-game set, from a scratch server of build 57 with this branch's
server and client, at noon; each place's weather is the world's own and was the same before): the wall outside the
South Gate now draws and the ground past ferns shows; the Rainwood and Cedar Sound are dense fir, cedar and fern rather
than murk; the Greyteeth are rock (`▴`) with snow in the hollows; the Long River at Larkfield runs as one wide channel
(its bridge is now about 60 tiles south-west, out of the frame). Still ugly: the reed beds of the Sallow Fens read as a
barcode of `¦`; massed sandstone (`▰`) in the Ochre Hills reads as diagonal hatching, and sand (`∶`) as columns of dots
(the Red Waste's dunes show by their shading, but the waste is olive, not red); the Larkdowns' meadow is sprinkled
evenly with wildflowers; the Whitefangs above Frosthollow are mostly tundra rings (`∘`) with a band of snow, little
rock; light rain still falls at the Ochre Hills' northern edge, where the dryness blends with the green country.

### The review's fixes (2026-11-01, branch `r2-map-fix`)

A review of the branch as written found six faults (two of them one fault seen from each end of the wire); the user
approved fixing them all. **Not yet written into DEV**: DEV still holds revision 69's sites (the farms over Larkfield,
the vineyard on desert). A write (`python3 -m worldgen.realm2 write --world dev`, which backs up first) puts this
layout in.

1. **Pawsteps behind thin cover were heard as in the open.** `World::movementAudibility` (the "you hear pawsteps"
   notice, `movementHeard`) and a yell through a doorway or over a seam (`World::hearingClarity`) still asked
   `lineOfSight`, which since the cover change is any line not wholly hidden: a wolf walking five tiles off behind one
   pine was heard plainly, where before it was not heard at all, and a novice sneaker in ferns three tiles off at 0.8.
   Both now muffle by `bodySeen`, as sight, noticing and the voice in a cell do (doc 22, "Cover that thins sight").
2. **The World Map's places beside the wolf's went whole at every step.** What the wolf glimpses past a seam changes
   the place beside's memory on nearly every tile walked by the seam, and its World Map entry is its whole picture
   (65,536 glyphs for a realm cell; the entries are keyed by their whole content and never sent as row edits): about
   64 KB a step, three at a corner, every version kept by the browser (up to 4,096 of them). Now each player's map keeps
   a place beside as it was while the band round its own carries it (`World::mapHeld_`, within 96 tiles of it:
   `inBand`); the band already brings what it sees there, as row edits, and the minimap and the World Map draw the band
   round the wolf's own place, over the places beside. Once the wolf moves away from that place, or into another, the
   map has it afresh. The review's probe (a wolf walking 200 tiles by a 256-tile cell's seam, 10 tiles in): 3 whole
   entries in all, at the first look, and none after.
3. **The weather stopped in a straight line at the cell's edge.** Rain, snow, fog and dust are drawn over the band too,
   but the soft mask of the weather field (the server samples only the cell) cut them off half a sample past its
   edge, so the band was dry. The mask's edge samples now run on over the band (`paint.ts`, `maskFor`).
4. **Sites laid over sites.** `lay_sites` kept a new site's middle off the boxes round the earlier ones, never its whole
   ground: the bigger, squared-off grain farms reached over Larkfield (2,154 tiles, 341 of its level core, with hedges
   drawn on them) and over each other (976 tiles), the mills' pads over their farms, a dairy and a range, the piggeries
   and their woods; both sites counted the tiles they shared. Now no site's preparing reaches a tile an earlier site's
   reached (its shape at the most its noise pushes it, and the margin where its heights are eased into the land round
   it: `site_edge`, `site_kernel`, `spread`), landings and mills included, and each site's tiles are its own ground
   (`ground_of`). The mills look a little further for their river bank (330 and 340 tiles from their farm's middle):
   their farms are bigger. A new check: "no site laid over another's ground".
5. **The Ochre vineyard was on desert.** Laid where the desert's edge paints sand, sandstone and rubble (68% of its
   ground: its dryness about 1), and a field's preparing cleared only trees and scrub. Farms, orchards, vineyards and
   dairies (`GROWN`) are now placed off bare ground where they can be (`BARE`: sandstone, sand, rubble, salt, rock and
   scree), and what is left of it inside them is given back as the country's grass. The vineyard is now on the hills'
   dry western edge, 250 tiles west (dryness 0.28 across it: 59% dry grass, 31% grass, under 1% sand and sandstone). A
   new check: none of them more than 5% bare.

The sites that moved more than 40 tiles or changed by more than 5% (centre and tiles as DEV holds them in brackets);
all 68 are placed, the farms' own fields 443,300 tiles (**6.8 cells**), the shared range 508,509 (**7.8 cells**); roads
14,813 tiles, tracks 60 (12,848 tiles); all 16 checks pass, the city as it was (0 codes, 0 heights differ):

| Site | Kind | Centre (x, y) | Tiles | For |
|---|---|---|---|---|
| `grain_long` | farmland | 1988, 2452 (1828, 2580) | 121,609 (125,664) | grain farm 1 (large, with a water mill) |
| `mill_long` | mill | 1743, 2548 (1628, 2612) | 317 (260) | grain farm 1's water mill |
| `grain_lark` | farmland | 2156, 2884 (2164, 2836) | 126,128 (126,319) | grain farm 2 (large, with a water mill) |
| `mill_lark` | mill | 2414, 2854 (2207, 3026) | 426 (348) | grain farm 2's water mill |
| `orchard_larkfield` | orchard | 1852, 2740 (1876, 2844) | 14,423 (14,194) | orchard (central hills) 3 |
| `vineyard_ochre` | vineyard | 2508, 2652 (2756, 2692) | 17,554 (17,577) | vineyard 1 (SE desert edge) |
| `vineyard_south` | vineyard | 2396, 3156 (2396, 3148) | 18,193 (17,036) | vineyard 2 (central hills) |
| `range_rainrun` | pasture | 996, 796 (1060, 940) | 50,722 (64,730) | cattle range (shared, NW coast) |
| `pigwood_greenholt` | woodpasture | 1188, 1676 (1260, 1628) | 30,736 (32,371) | pig farm 1's woods (shared range) |
| `pigs_alder` | pigfarm | 1612, 1260 (1644, 1428) | 5,996 (6,227) | pig farm 2 |
| `pigwood_alder` | woodpasture | 1596, 1076 (1604, 1268) | 31,518 (28,821) | pig farm 2's woods (shared range) |
| `pigwood_south` | woodpasture | 1140, 2028 (1196, 2068) | 30,263 (32,216) | pig farm 3's woods (shared range) |
| `fishing_sound` | fishing | 603, 1359 (684, 1389) | 269 (301) | sea fishing boat 1 (Ridgemere's sound) |
| `fishing_glasswater` | fishing | 1828, 2012 (1948, 2283) | 271 (265) | lake and river boat 1 |
| `fishing_rainmere` | fishing | 1037, 613 (964, 740) | 359 (253) | spare: lake boat |
| `timber_rainwood` | timber | 652, 1092 (684, 1164) | 1,859 (1,867) | timber (coastal rainforest) |
| `charcoal_greenholt` | charcoal | 1260, 2124 (1348, 2140) | 1,032 (1,041) | charcoal |
| `charcoal_rainwood` | charcoal | 668, 1324 (684, 1228) | 1,002 (1,007) | charcoal |

**Tests.** `world_tests`: a pine between hides a walking wolf and muffles its pawsteps (heard five tiles off in the
open, not behind it); a novice sneaker in ferns three tiles off is neither seen nor heard; a pine between a yeller and
a doorway muffles the yell through it. `stream_tests`' `theMapKeepsAPlaceBesideAsItWas` (walking by a seam the wolf
remembers more of the place beside while its World Map picture stays as it was and the band carries the new ground; in
another place the map has all it saw). The client's `fringe.test.ts`: the map draws the band round the wolf's place on
top, and the weather's mask covers the band at the edge's strength. `test_realm2`'s new `RealmSites` (with
`RATW_SLOW=1`; the realm round the kept city snapshot as far as its sites, no database): every site placed, no tile
changed by two sites' preparing (DEV's layout: nine pairs), and the farms, orchards, vineyards and dairies on soil
through their middles. `WholeRealm` can now lay the realm round an Atlas export instead of DEV (`RATW_REALM_PROJECT`,
such as a write's backup), which is how it ran here: 8 of 8. Each new test fails without its fix. A clean build of
`build-core`, then `ctest`: 76 of 76. The client: `tsc` clean, 134 tests pass.

### The previews

`artifacts/realm2/after/` holds the second pass's (the same set, and its `realm-report.json`); the first realm's are
kept beside it for comparison. `artifacts/realm2/`: `overview.png` (the whole realm, a pixel for 4 x 4 tiles), `overview-labelled.png` (with the
regions, the towns, the roads and every reserved site by kind), `overview-large.png` (a pixel for 2 x 2 tiles), and
close-ups a pixel a tile: `ridgemere-and-the-rainward-woods`, `cedar-sound-and-oakhaven`, `whitefangs-and-frosthollow`,
`glasswater-and-the-greenholt`, `sallow-fens-and-the-delta`, `red-waste-saltpan-and-dunes`, `greyteeth`, and the
blends `blend-rainwood-to-whitefangs`, `blend-greenholt-downs-ochre-waste`, `blend-greenholt-to-fens`.
`realm-report.json` holds every figure above, the sites, roads, rivers and lakes; `city-cells.json.gz` the city's cells
as its writer made them (section "Built and written").

### The steadings and the communities (2026-11-03, branch `r2-communities`)

The user (doc 71's next step): every Country food farm gets **a farmhouse and a barn** (or the store its kind needs);
**only the keeper and the keeper's family live on the premises** (a farmhouse for up to six, `HouseholdMost`); the
hired hands live in **communities** and walk to work, except near Ridgemere (within about 800 tiles), whose farms' hands
live in the city; **five communities**, each a small town with housing, a small market (a store plus an alehouse or
inn) and a little food of its own, sized for the hands **and their families later** (Coldwater 55-65 people, Larkfield
about 30, the others 20-25), with room to add houses; Oakhaven, Frosthollow and Mirebridge stay reserved; the
communities joined to their farms and the roads; each in its region's look; no NPCs placed (the next step), none made.

**Built** (`tools/worldgen/realm2_places.py`, laid by `realm2.expand` after the realm itself and before the cells are
cut, so every realm write, and the city writer's, makes the same buildings with the same ids):

- **Fenshore**, a new town site (the eighth; the check is now 5-8 town sites): placed where the fens' dry ground is
  widest near the five fen farms (its middle at least 14 tiles from any creek or pool), prepared as a town's pad and given
  a footpath to the nearest road. It stands at **860, 3476** (`w_ridgemere_3_13`), where the user suggested.
- **31 steadings**, one on each farm's site: the farmhouse (six beds) and the farm's buildings round a yard, a kitchen
  garden beside it and a footpath from its gate to the nearest road or track. Each compound is placed on its own site's
  ground (or no site's), off water, roads and the city, as level as can be found near the site's middle; its box is
  levelled and the land eased into it, and it is turned whichever way fits best:

  | Kind | Buildings besides the farmhouse |
  |---|---|
  | grain farm | granary; and a **water mill** at its mill site (`mill_long`, `mill_lark`), its back on the river, the wheel (`}`) in the water, a race of shallow water where the bank is wide |
  | orchard with a drying house (`orchard_greenholt`, `orchard_rainward`); orchard | drying house (racks over a slow fire); apple store with a cider press |
  | vineyard | press house over a **cellar** (an interior at z -1, by stairs) |
  | cattle and dairy farm | byre and dairy |
  | pig farm; sheep and goat run; warren | pig yard (sties); shearing barn; rabbitry |
  | sea or lake boat | a cottage, the boat shed, and a **jetty** of planks (`8`) about 20 tiles out over the water from the landing |
  | eel and fowl marsh; beekeeper | a cottage and the eel house (smoking racks); a cottage and the hive shed |
  | salt works | the Saltpan: a cottage, the salt store; the fens' salterns: a cottage, the boiling house |

- **Five communities** on their town sites: a green with its well in the middle, streets from it to every road and
  footpath that reaches it, two lanes round the houses; cottages (six beds each) and the market facing the streets;
  kitchen gardens, a hen run and a goat paddock round the edge (Saltwell's by the oasis pool, watered from it):

  | Community | Where | Look | Homes | Market | Beds | Hands | Its farms (walk from the green, tiles) |
  |---|---|---|---|---|---|---|---|
  | Coldwater | 1956, 1796 | timber under cedar shingle (`#`, `L`) | 9 cottages | Coldwater Stores (flat over), The Glasswater Inn (10 guest beds), Coldwater Bakehouse (flat over) | 62 | 21 | Greenholt orchard 515, Meresend orchard 249, Oakhurst piggery 657, Alder piggery 688, Larkdowns fold 762, Greyteeth fold 1,047, Glasswater landing 254, Upper Mere landing 551 |
  | Larkfield | 1676, 2380 | timber under shingle | 4 cottages | Larkfield Stores (flat), The Sheaf alehouse (flat), Larkfield Bakehouse | 31 | 10 | Longmead farm 527 (its mill 256), Plumtree orchard 472, Southwood piggery 557 |
  | Southmeadow | 2148, 3348 | limewash under red tile (`]`, `[`) | 3 cottages | Southmeadow Stores (flat), The Vine and Sickle (flat) | 25 | 8 | Larkholm farm 569 (its mill), Southmeadow vineyard 427, Sunfields bee garth 396 |
  | Fenshore | 860, 3476 | stilted timber under reed thatch (`#`, `L`); boardwalks (`8`) half a step over the bog pools | 3 stilt houses | Fenshore Stores (flat), The Heron (flat) | 25 | 8 | Withy Creek marsh 357, Fenside salterns 196, Rivermouth landing 1,040, Fen Shore landing 524, Dune warren 198 |
  | Saltwell | 3292, 2932 | limewashed adobe under flat pale roofs (`]`, `V`); flagged streets (`f`) | 3 cottages | Saltwell Stores (flat), The Oasis Inn (10 guest beds) | 22 | 6 | Saltpan workings 487, Ochre vineyard 952, Redsand warren 597 |

  Beds are the homes' and the keepers' flats' (the inns' guest rooms aside). Room to grow, counted for the contract
  (`roomToGrow`): 6 more cottage lots by the streets at Coldwater, 8 at Larkfield, 6 at Southmeadow, 9 at Fenshore, 6
  at Saltwell. The cells' descriptions say what each community is.
- **Ridgemere's farms** (no community): the Rainward and Coastvale orchards, the three dairies (Southgate, Rainrun,
  Hillside), the Soundside and North Cove landings, the Heatherside and Alderbank bee garths: 23 hands, in the city.
- **Footpaths:** 24 from the communities to their farms and the two mills (12,980 tiles of walk, most of it along the
  roads: a path is painted only where no road or track already goes), and one from each steading's and mill's gate to
  the nearest road or track; routed over the land as the realm's tracks are (A* on its 8-tile grid, bridged over
  rivers), never through another compound or community; the Saltpan's crust is walked on.
- **Ids.** `fs_<site>_<building>` for a steading's interiors (`fs_grain_long_home`, `fs_grain_long_granary`,
  `fs_grain_long_mill`, `fs_vineyard_ochre_press_cellar` ...), `cn_<community>_<building>` for a community's
  (`cn_coldwater_cottage_1`, `cn_coldwater_store`, `cn_coldwater_store_flat`, `cn_coldwater_inn_up` ...); each door
  `<interior>_door`, stairs `<interior>_stairs_1`. An interior's region is its community (`ridgemere` for the city's
  farms), so the game counts a farm's keeper and hands as near their work (`World::near`), and each community becomes a
  town of its own once people live there and its store trades (`World`'s towns: residents by home, markets by work). No
  id holds "lodging", "bunkhouse" or "quarters": none is a communal home.
- **The contract** for placing NPCs: `Data/Economy/communities.json`, written by each realm write (and the city
  writer's): for each of doc 71's 31 farms its realm2 site, its workplace and every work room with its work spots and
  stations, its farmhouse with its six beds, its community (or `ridgemere`) and the walk from it; for each community its
  homes and flats with their beds, its market's workplaces with the posts they want and their work spots, its gardens and
  its room to grow; the work labels the game knows each business by, and the words a farm post's label must avoid. Doc
  71's farm ids differ from the sites': the mapping (`realm2_places.FARMS`) follows the map's own numbering where the
  file's region text says otherwise (the map wins), recorded per farm as `mapWins` (orchard 3, the lake boats, sea boats
  2 and 3, the beekeepers, the salt works).

**Checks** (with the realm's, in `realm2.checks`; all 22 pass): every steading's and community's door reached from the
city; every farm's workplaces in one walk with its community's homes (or the city); homes for at most six, farmhouses
for six; the hands where the user put them (21, 10, 8, 8, 6; 23 in Ridgemere); the ids world ids; no "dog". Everything
before the step is made exactly as it was: laid round DEV's city first, the realm reproduced DEV's 241 cells tile for
tile (0 differ); after the write every changed tile is one the step changed (38,118 tiles' codes: the communities, the
steadings, the paths, the jetties), and the city's cells, the 819 rooms, 513 links, 1,083 people, the routes, factions,
spawn and economy are as they were; five cells' descriptions name their community.

**Written into DEV.** Backed up first: `artifacts/backups/ratw_dev_20261103-071638.pgdump` (main repo; DEV at revision
71); then migration **0060** (`python3 tools/world_db.py migrate --db dev`: "applied 0060_farms"; 60 migrations). Then
`python3 -m worldgen.realm2 write --world dev` (twice, the second with the steadings' final names; each backs up into
`artifacts/backups/` first: `ratw_dev_20261103-075412.pgdump`, `ratw_dev_20261103-080134.pgdump`): the same map,
`map-20261031-6b080cecb7f6`, its new layout recorded; **revision 74** (75 after the editors' reload). Then
`world_build.py dev`: **build 59**, 1,174 places; `world_check` of its export loads all of them (1,060 and the 114 new
interiors) with no failures (no simulation was run).

**Tests.** `test_realm2`: `Places` (every farm of doc 71 with its site and steading, the hands where the user put them,
the interiors' beds and work spots, every compound's doors on its yard however it is turned and nothing overlapping, the
words); with `RATW_SLOW=1`, `WholeRealm`'s `test_the_steadings_and_communities` (every farm's workplace and home in the
world, their regions, work spots for the crew, beds in each community's range, the gardens, the market, room to grow,
the three reserved towns unbuilt) and `test_the_contract_written_is_the_one_made`: 27 of 27 with `RATW_SLOW=1`. The
city writer lays them too: `python3 -m worldgen.ridgemere2 write --world dev --dry-run` passes every check (the city's
and the realm's with the places). A build of `build-core` and `ctest`: 77 of 77. `test_country_industry`,
`test_farms_summary`, `test_world_store`, `test_world_build`, `test_terrain_catalog` pass; `test_ridgemere2` fails only
its known `test_the_food_trades_and_the_carters` (16 household businesses against 15, as before the realm).

**Previews** (`artifacts/realm2/communities/`): each community close up (`coldwater.png` ..., four pixels a tile),
six steadings and Longmead's mill (`steading-*.png`), and `communities-and-farms.png` (the realm, each community with a
line to each of its farms, Ridgemere's farms in white). No in-game screenshots: a scratch server of build 59 starts and serves the world, but taking a
wolf to the communities needs doc 70's walking scripts, which are not committed.

**For the NPC placement (next):** a resident's home is `live.npcs` `home_area` = a home's `area` (an interior id: in
`world.areas` and `world.interiors`, the project's `rooms`), `home_x`/`home_y` one of its `beds`; a post's `work_area` =
the workplace's `area`, `work_x`/`work_y` one of its `work` spots. Beware: a write of the city (`worldgen.ridgemere2
write`) places the city's people from its own plan and sets every other placed NPC's home to none (`compose`), so it
would undo placements in these interiors until its plan knows them. (Fixed with doc 71's Phase C, branch
`r2-farm-people`: `compose` leaves anyone homed in an `fs_` or `cn_` interior alone, `realm_homed`, and the city's checks
and reports count only the city's own.)

**Still open.**
- ~~The keepers' and hands' labels, wages, and the virtual crews they replace (doc 71, Phase C).~~ Built: doc 71,
  Progress, "Phase C".
- Talk's regions (`Data/Talk/facts.json`, `here.region`) know Coldwater, Larkfield and Southmeadow but not Fenshore or
  Saltwell.
- The communities' food (the gardens, hens and goats) is ground only: nothing grows or lays yet.
