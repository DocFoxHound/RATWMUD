# 67. Ridgemere 2.0: the city (2026-10-30, revised 2026-10-31)

The user, 2026-10-30 (verbatim):

> Okay, let's start working on Ridgemere2. I want you to build this world out for me. What we need is to start with
> building JUST the city, and then we'll test its economy and go from there. Here is the city layout:
>
> A large, Seattle-like city in a rainy climate with heavy industry. It sits in a bay, the water bordering it on the
> western side. There are wharfs, docks, and related structures on the western side of the city. Immediately next to
> the wharf are the slums, known as the "sump". This area consists of densely packed buildings and an intricate system
> of alleys and narrow passages between buildings. Structures should vary in size and shape, here, and should allow
> for a great deal of inter-building movement through narrow areas.
>
> The Sump is bordered on the east by the middle-class buildings, and the south by the city administrative sector.
> The middle class area has shops and a marketplace. The sump has a separate, smaller marketplace that is much
> trashier in design. The middle-class marketplace is what you would expect, and is where players would be expected
> to conduct most of their sales and trade business. South of the middle class area is a smaller upper class area
> that may have only a handful of mansions. These are considered Ridgemere's "Minor Houses." At the border of the
> middle-class and Upper-class areas is the "Old Keep", which is now the jail cell, guard barracks, and guard office.
> It has a small courtyard and four walls surrounding it.
>
> The city administrative sector borders the sump and the upper class to the south, and consists of offices and other
> medeival/renaissance structures, one notably being a three-story "Records Office". South of here is the Great
> Palace, which has a central antechamber and two separate wings. It is more of a government building than an actual
> palace. The eastern wing is the "Administrative Wing". The Western wing is the King's Residence. The central
> antechamber is a court or senate-like area.
>
> There is a walled abbey and cathedral in the northwest part of town, on the edge of the sump and the middle class
> area.
>
> Ridgemere has 4 major families. Greatly, Fell, Grayrock, and the other I forgot but you have. The only Great House
> within the actual walls of the city is House Greatly, whose compound is in the Northeastern part of the city as far
> away from the wharf and palace as possible. It is a large estate and compound with a fountain in the front yard
> (south yard), and a forge and workshop in the backyard (north). It is bordered on the north side by the city wall,
> which it sits just inside of. House Greatly's business is construction and it is said they built the walls of
> Ridgemere.
>
> Outside of the city walls are the three other major houses: Grayrock, Fell, and the other which I want to rename to
> Timberland. House Fell produces fine wines, liquors, and fruit goods. House Grayrock owns the Wharf. House Timberland
> owns the city's forges.
>
> For now, build only this. Ensure that we are accounting for industry, business, and all manners of shops that may
> be needed for a city of this size. For now, build this city large enough to encompass 500-ish of our NPC characters.
> You may make multi-family homes to help with this, but no more than 3 homes in a single structure. They will need
> jobs throughout the city, to include the marketplace, forges, plasterworks, administrative buildings, and guard. Do
> not include any food growing capacity yet. For now, for all shops and forges and everything that needs raw material
> to produce recipes, create a simple internal function/store room (not physical) that each shop owner/vendor/smith/
> etc. can access that contains infinite resources that they need to produce their specific goods. These still cost
> money, but it allows them access before we have the raw resource suppliers within the world yet.
>
> This internal store function should be preserved as an option we can toggle on/off either en mas or individually in
> Atlas' economy controls. For now it will default on.

This doc builds on four earlier ones:

- doc 66: worlds, NPCs without a map, the studio, and the economy's rules removed;
- doc 60: the old map and its generator;
- doc 35: items and businesses;
- doc 59: lamps.

Read doc 66 first, including its Progress notes.

## Where it starts

- **The database.** `ratw_ridgemere2` has registry key `ridgemere2`. It is a dev world with no upstream: world id
  `upper_accord`, name "Ridgemere 2.0", revision 28, 54 migrations. It holds:
  - **No map:** no cells, no terrain, no places, no resources and no spawn.
  - **626 interiors:** 401 `rm_` (the old city), 122 `tn_` and 103 others. Their 103 links are all `stairs`, because
    the doors went with the map.
  - **1,083 NPCs** (904 civilians, 152 merchants, 27 guards), with 1,083 voices, 329 relations and 6 dialects. Every
    NPC has a home, and 556 have no work.
  - **6 factions,** among them `house_mossgrave`. There are 405 faction claims, all of them on interiors.
  - **10 patrol routes,** none with posts.
  - **No `live.economy` row.**
  - **No game save:** `game.checkpoints` is empty, so no save can be lost.
- **The old map's placements** are in `artifacts/ridgemere2/old-map-placements.json`. They hold:
  - the cells and the NPC places;
  - 523 doors, each with its interior door tile `b_x`/`b_y`;
  - 91 patrol posts, the claims and the spawn.
- **The old city's manifest** is `artifacts/realm/buildings.json`: 324 buildings, each with its district, kind, trade,
  beds, work spots and stations. It has no footprint field. It also lacks the three doc 61 works that the Greatly
  compound reuses (`rm_the_greatly_colour_works`, `rm_the_greatly_plasterworks`, `rm_the_greatly_masons_yard`).
- **The analysts' files** are in this session's scratchpad (`.../scratchpad/city/`): `selected.json` (512 ids),
  `hh.json`, `incity.json`, `mansions.json`, `labels.tsv` and `npcs.json`. Step 0 (section 8) copies them into
  `artifacts/ridgemere2/analysis/`, and the lanes read them only from there.
- **The economy.** Since doc 66 Phase 5, NPCs make nothing, buy nothing and restock nothing:
  - a shop has a till (`till:job:<founder>`), its wares, the catalogue's prices and conservation;
  - residents' needs are paused (`Society::NeedsPaused`).
- **The generator** (`tools/worldgen/realm.py`, `cities.py`, `buildings.py`, `site.py`, `canvas.py`, `preview.py`):
  - It builds the whole old realm on a 2,048 canvas. Its helpers are hard-wired to that size (`SIZE`, `SIZE//STEP`).
  - `assemble()` replaces the project's people. Run against this world, it would delete all 1,083 NPCs with their
    voices and families, because `save_world` makes `live.npcs` equal the project.
  - This build uses a new driver that merges into the existing people. It never replaces them.
  - The old generator is **frozen**. It still makes the old map (DEV and PROD), and its output must not change.

## 1. The checklist

The build is verified against these requirements. Each section below says which ones it meets, and the integration
checks (section 8) verify every one.

| # | Requirement (from the spec) |
|---|---|
| R1 | Ridgemere 2.0 is built: the city only. Outside the walls there are only the three Houses' estates and roads: those to the estates and those running on to the canvas edge |
| R2 | A large, Seattle-like city in a rainy climate: every cell's weather is `rain`, and the hills rise from the water |
| R3 | Heavy industry: a works quarter of forges, foundry, kilns and mills, on the water by the slipway |
| R4 | It sits in a bay, and water borders it on the west |
| R5 | Wharfs, docks and related structures on the western side, **inside the walls** (the west defence is a sea wall along the quay) |
| R6 | The Sump lies immediately next to the wharf, with no wall between them, and holds most of the wharf's frontage |
| R7 | The Sump is densely packed, with an intricate system of alleys and narrow passages between buildings |
| R8 | Sump structures vary in size and shape |
| R9 | The Sump allows a great deal of movement between buildings through narrow places |
| R10 | The Sump is bordered on the east by the middle class |
| R11 | The Sump is bordered on the south by the administrative sector |
| R12 | The middle class has shops and a marketplace |
| R13 | The Sump has its own marketplace: separate, smaller and much trashier |
| R14 | The middle-class marketplace is where players do most of their selling and trading: its square has the player stall places, and its keepers sell mixed trades, not just food |
| R15 | South of the middle class is a smaller upper-class area with a handful of mansions: the Minor Houses. It is at most half the middle class's area |
| R16 | The Old Keep sits on the middle/upper-class border. It holds jail cells, guard barracks and the guard office, with a small courtyard and four walls |
| R17 | The administrative sector borders the Sump (to its north) and the upper class (to its east). This is a reading of the spec, listed in section 9 |
| R18 | The administrative sector has offices and medieval/renaissance buildings, among them a three-storey Records Office |
| R19 | The Great Palace lies south of the administrative sector. It is a government building: a central antechamber (a court or senate), the Administrative Wing on the east and the King's Residence on the west |
| R20 | A walled abbey and cathedral in the north-west, on the edge of the Sump and the middle class |
| R21 | Four major Houses: Greatly, Fell, Grayrock, and the forgotten one (Mossgrave), renamed Timberland in this world's text |
| R22 | Greatly is the only Great House inside the walls. Its compound is in the north-east, as far as possible from the wharf and the palace |
| R23 | The Greatly compound is a large estate with a fountain in the front (south) yard and a forge and workshop in the back (north) yard |
| R24 | The Greatly compound is bordered on the north by the city wall, just inside it, with no lane between |
| R25 | Greatly's business is construction, and they are said to have built the walls (in the trades and in talk) |
| R26 | Grayrock, Fell and Timberland have their seats outside the walls |
| R27 | Fell makes fine wines, liquors and fruit goods |
| R28 | Grayrock owns the wharf |
| R29 | Timberland owns the city's forges |
| R30 | Industry, business and every kind of shop a city this size needs. Any omission is recorded with its reason |
| R31 | Large enough for about 500 NPCs |
| R32 | Multi-family homes are allowed, with at most 3 homes in one structure |
| R33 | Jobs throughout the city, including the marketplace, forges, plasterworks, administrative buildings and guard |
| R34 | No food-growing capacity: no fields, orchards, vineyards, gardens of crops, livestock pens, fishing boats, forage or resource nodes, or game spawns |
| R35 | An internal store room (not physical) that every owner, vendor, smith or maker can use: makers draw their recipes' inputs and vendors draw the goods they sell, without end |
| R36 | What is drawn from the store costs money |
| R37 | The store can be toggled on and off, all at once and per business, in Atlas's economy controls |
| R38 | It defaults on |
| R39 | It stays as a lasting option, not a temporary hack |

## 2. The map

**Canvas.** The canvas is 1,024 × 768 tiles in cells of 256, so 4 × 3 = 12 cells:

- Ids are `w_ridgemere_<cx>_<cy>` (cx 0–3, cy 0–2). North is up and y grows southward.
- Every cell is kept; none is more than 97% water.
- Every cell's `weather` is `"rain"`, so `climateOf` makes it Wet (R2).
- Every cell's `territory.region` is `ridgemere`. So is every placed interior, including new `r2_` ones (`Site.place`
  gets `region`). Otherwise `World::setupTowns` would make a second town of any region with 5 or more residents and
  merchants, such as Fellhall's.

**Coordinates.**

- All coordinates are canvas tiles, written `x0–x1, y0–y1` and half-open.
- They give **outdoor blocks**, not interior sizes. An interior is roomier than its block: `fp()` is ×0.72 wide and
  ×0.65 deep, with a 6 × 5 minimum.
- A reused interior's block comes from `fp()` of its size.
- A new set piece is built with an explicit `footprint` equal to its block, so the palace's touching blocks do touch.

```
   x: 0     168 172 222    260 320 330  364     446   494  540  580 640 660        1024
y:  0  ~~~~~                                   |N Road
  150  ~~~~~ +#####################################N####################+
       ~~~~~ #WHF| ABBEY &   | NORTHGATE (middle)         | GREATLY      |
       ~~~~~ #   | CATHEDRAL |                            | compound     |
  232  ~~~~~ #   +-----+-----+   [Market Hall]            |  (fountain)  |
  250  ~~~~~ #pier THE SUMP  +------------------+---------+--------------+ 270
       ~~~~~ #pier  (Rag      |     THE COMMONS (middle class)            E== East Road ==> [Fellhall]
       ~~~~~ #pier  Market)   |     [Market Square] (spawn)               |
       ~~~~~ #pier            |                               [OLD KEEP]  |
  470  ~~~~~ #CPI +-----+-----+------------------------------+---[KEEP]---+
       ~~~~~ #pier|CIN-| ADMINISTRATIVE (Records Square)     | THE RISE   |
  530  ~~~~~ #    |DERS+-----+-------------------------------+ (Minor     |
       ~~~~~ #slip|          | THE GREAT PALACE (Res|Ante|Admin) Houses)  |
  616  ~~~~~ +####H####C########################################S#########+
       ~~ [Grayrock Hall]   \__ [Timberland Court]                |S Road
  768
   (~ water; # wall (west: the sea wall); WHF wharf; CPI the Customs Pier; H Harbour Gate; C Cinder Gate; N/E/S gates)
```

### The bay and the water (R4)

- **The shore** is a polyline: (96,0) → (110,120) → (168,170) → (168,610) → (120,660) → (96,768). Everything west of
  it is `W`, so the bay is held between a north and a south headland. The quay is straight along x = 168.
- **Heights.** The water is at 0 and the wharf at +0.5. The city climbs eastward in tiers, as `cities.py` does:
  - tier edges are `#` retaining walls, with `^` stairs where a street crosses;
  - next to each other, two walkable grounds differ by no more than 1.

  | Height | Districts |
  |---|---|
  | 1.0 | the Sump and the Cinders (both 0.5 above the wharf, so they meet it freely) |
  | 2.0 | the abbey knoll (its own walls are its retaining edges), Northgate, the Commons, the administrative sector, the palace and the Old Keep |
  | 3.0 | the Rise and the Greatly compound |

  This gives the hills of a Seattle-like city (R2). Grounds 2 apart never touch, because a wall always lies between
  them.
- **Cell seams** are at x = 256, 512 and 768 and at y = 256 and 512. A seam is crossed only where both tiles are open
  and their heights differ by 0.55 or less. So every tier edge and every `^` stair stays at least 2 tiles off a seam,
  and 8A checks it.
- **Gates between heights.** Wherever a gate joins ground more than 0.5 apart, its floor sits at the half step, with a
  `^` run on the low side. `G` alone is not a ramp. This covers:
  - the abbey's south gate (2.0 to the Sump's 1.0);
  - the Greatly gate (3.0 to Greatly Square's 2.0);
  - the stairs on Keep Street and Rise Lane up to the Rise;
  - any city gate whose outside ground differs.

### The wharf and docks (Grayrock; R5, R6, R28)

- **The sea wall.** The west defence runs along the waterline, 4 thick, at x 168–172 (y 150–620). The wharf lies
  inside it, so the wharf is inside the walls (R1, R5).
- **The wharf:** x 172–222, y 154–616, paved with the wharf paving.
  - **The Wharf Road** runs along it (x ≈ 197, 3 wide) from the north wall down to the **Harbour Gate**.
  - The wharf's east side is open. The Sump's alleys run straight onto it (R6), and so do the Cinders' yards. The
    abbey's west wall also faces it.
- **Eight piers,** each 6 × 56, at x 112–168 and y 200, 245, 290, 335, 380, 425, 470 and 515:
  - They are `8` over `W`, laid by new code (`piers()` picks its own spots and sizes, so it is not used).
  - Each pier leaves the sea wall through a 6-wide **pier arch**, a `G` at +0.5.
  - Two arches are guarded: the **Customs Pier** (y 470), at the Customs House, and **Sump Stairs** (y 335), where the
    Gut meets the wharf. Only the Customs Pier has a fixed gate post.
- **Four cranes,** 3 × 3 solid blocks, stand at the ends of the piers at y 245, 335, 425 and 515, each with a work
  post beside it.
- **The shipwright's slipway:** a 12-wide opening in the sea wall at y 574–586. A ramp of `:` runs into the water at
  x 150–172, with the yard's building behind it on the wharf (y 560–600).
- **The Sump holds most of the wharf frontage.** Its frontage is y 232–470, which is 238 of 462 tiles (52%). The abbey
  holds 78 tiles and the Cinders 146. The palace and the administrative sector do not touch the wharf. The Customs
  House is the only civic building on it.
- **The wharf's buildings** are listed in section 3:
  - warehouses, the Grayrock Wharf Office and the Customs House;
  - the Fish Hall, the smokehouse and the tannery;
  - the ropewalk, along the sea wall;
  - the chandler and the cooperage;
  - the Sailors' Rest, two taverns and the wharf lodging.

### The wall and the gates

- **The wall** is a band 4 tiles thick:
  - west x 168–172 (the sea wall), north y 150–154, east x 660–664 and south y 616–620;
  - it has 7 × 7 `#` towers at the corners and about every 100 tiles;
  - the inside of the wall is x 172–660, y 154–616.
- **The tools.** The wall uses `ring()`, `corners()` and `gate()` through the `RidgemereCity` subclass (8A).
  `compound()` is **not** used inside the walls, because its `flatten_mask` would smear the tiers.
- **Gates:** `G` archways, recorded in `gates` and `gate_levels`.

  | Gate | Tile | Leads to |
  |---|---|---|
  | North Gate | (476, 150) | the North Road, to the canvas edge |
  | East Gate | (660, 360) | the East Road, past Fellhall to the canvas edge |
  | South Gate | (600, 620) | the South Road, to the canvas edge |
  | Cinder Gate | (290, 620) | the Cinder Road, to Timberland Court |
  | Harbour Gate | (197, 620) | the Wharf Road's south end, to Grayrock Hall |

- **The inner ring** is a lane 3 wide just inside the wall, the wall-walk patrol's line:
  - It runs inside the north wall from the abbey's east wall (x 330) to the Greatly compound's west wall (x 540).
  - It runs inside the east wall from y 270 to the south wall, and inside the south wall west to the Cinders.
  - On the west, the Wharf Road is the ring.
  - **It stops at the Greatly compound** (R24) and at the abbey, which sit directly against the wall. The wall-walk
    route goes round them by Gate Street and Greatly Square. Wall tiles are solid, so no route runs on the wall top.
  - A check makes sure no public walkable tile lies between the north wall and the Greatly north yard.

### The districts

The districts **partition** the inside of the wall: every inside tile belongs to exactly one district, and 8A checks
this. Polygons are given as vertices.

| District | Extent | Height | Ground | Streets |
|---|---|---|---|---|
| The wharf (Grayrock; R5) | x 172–222, y 154–616 | 0.5 | wharf paving | the Wharf Road |
| Abbey and cathedral close (walled; R20) | x 222–330, y 154–232 | 2.0 | `f` | its own paths |
| Northgate quarter (middle class) | x 330–540, y 154–250 | 2.0 | `.` | grid (26,19), width 3 |
| Greatly compound (R22–R24) | x 540–660, y 154–270 | 3.0 | `,` gardens, `f` yards | its own drive |
| The Sump (R6–R9) | (222,232) (330,232) (330,250) (364,250) (364,470) (222,470), about 33,200 tiles | 1.0 | `.` and `D` | see below |
| The Commons (middle class; R10, R12) | (364,250) (540,250) (540,270) (660,270) (660,470) (364,470), less the Keep's north half; about 61,200 tiles | 2.0 | `.` | grid (26,19), width 3 |
| Old Keep (R16) | x 580–640, y 445–500 | 2.0 (one level) | `f` courtyard | its gate only |
| Administrative sector (R11, R17, R18) | x 260–540, y 470–530 | 2.0 | `f` | grid (28,21), width 3 |
| The Cinders (heavy industry; R3) | (222,470) (260,470) (260,530) (320,530) (320,616) (222,616), about 10,700 tiles | 1.0 | `>` | lanes (34,26), width 3, with yards between the works |
| The Great Palace (R19) | x 320–540, y 530–616 | 2.0 | `f`, `,` gardens | the forecourt |
| Upper class, the Rise (R15) | x 540–660, y 470–616, less the Keep's south half; about 15,700 tiles | 3.0 | `,` | grid (40,30), width 4 |

- **The borders the spec asks for:**
  - The Sump meets the wharf along its whole west side, with no wall (R6).
  - The Sump meets the middle class on its east at x = 364, and Northgate on its north at x 330–364 (R10).
  - The Sump meets the administrative sector on its south at y = 470 for x 260–364, which is 73% of that edge. The
    Cinders holds the last 38 tiles by the wharf (R11).
  - The administrative sector meets the Rise at x = 540 (R17).
  - The palace lies south of the administrative sector (R19). It is inland: the Cinders lies between it and the wharf.
  - The abbey close is in the north-west corner. The Sump is on its south and the middle class (Northgate) on its east
    (R20).
  - The Keep straddles the Commons/Rise line at y = 470 (R16).
  - Greatly is in the north-east corner, against the north and east walls. It is the furthest point from the wharf
    (west) and the palace (south-west) (R22, R24).
- **The upper class** (about 15,700 tiles) is under a fifth of the middle class (Northgate and the Commons, about
  81,400). 8A checks the ratio is at most 0.5 (R15).
- **The Cinders is in the south-west,** on the water by the slipway and the ropewalk, as a city's industrial waterfront
  is. The north-west corner is the abbey's (R20). The King's Residence looks west over the Cinders' smoke, past a
  garden wall.
- **Scaling.** If the coverage checks (8A) can't be met at this size, the implementer may scale every interior
  coordinate by one common factor between 0.85 and 1. Every relationship above must still hold.

### Streets and squares

- **Avenues** are 5 wide and painted `_` by `RidgemereCity`'s own street painter. Roads outside the walls use
  `road()`, which paints `d`.

  | Avenue | Route |
  |---|---|
  | High Street | East Gate → Market Square → west. It narrows to 3 as **the Gut** through the Sump, down `^` stairs at x 364, to Sump Stairs on the wharf |
  | Gate Street | North Gate → Market Square |
  | Crown Street | Market Square → Records Square → Palace Square |
  | Keep Street | Market Square → the Keep's gate |
  | Rise Lane | Keep Street, round the Keep's west side and up `^` stairs at y ≈ 474 → the South Gate |
  | Abbey Row | the abbey's east gate → Gate Street |
  | Greatly Way | the Greatly gate → High Street |

- **Squares:**

  | Square | Centre | Half-size | What is there |
  |---|---|---|---|
  | Market Square | (470, 360) | (24, 16) | the main market (R12, R14): 14 stall tiles `u` (8 NPC keepers and the 6 player places) and a well `U`. The covered Market Hall stands on its north side (block x 446–494, y 322–342) |
  | The Rag Market | (290, 350) | (12, 8) | in the Sump (R13): 6 makeshift stalls, a pump `U` against a wall, mud ground |
  | Records Square | (400, 500) | (16, 10) | |
  | Palace Square | (430, 541) | (60, 8) | the forecourt |
  | Greatly Square | (600, 282) | (14, 8) | outside the compound's gate, in the Commons |

  - The Rag Market is about 180 tiles from Market Square. That is more than `StallReach` (40), so the game keeps it as
    a second market.
  - The game takes the city's main market to be the area where the most merchants work. Market Square has 8 keepers
    and the Hall 6, so Market Square is that area. The Hall's doors are within 25 tiles of the square's stalls. 8A
    checks both.
  - Player stalls come only from an outdoor square's `u` tiles (`RatwStalls`, at most 6 to a square). Market Square
    offers its 6. The Hall has none (section 9, Q8).
- **The spawn** is at (470, 380), on the south edge of Market Square, in `w_ridgemere_1_1`.

### The Sump in detail (R7–R9)

- **Two through-lanes**, both 3 wide and crooked (their polylines are jittered ±3 tiles every 12):
  - the Gut, running east–west at y ≈ 350;
  - **Tarry Lane**, running north–south at x ≈ 292, from the abbey's south gate (300, 232) to `^` stairs onto the
    administrative sector at y 470.

  They cross at the Rag Market.
- **Alleys:** everything else is alleys 1–2 tiles wide.
  - Lay a jittered grid (spacing 9–16 at random, each line rotated ±10°). Cut about 25% of its segments for dead ends.
  - Offset a further 15% by 2–4 tiles to make doglegs.
  - Pass `gap` through `district()` to `Lots`: gap 0 makes terraces of 2–3 buildings, and gap 1 elsewhere leaves
    1-tile **snickets** between blocks.
- **Where doors go.** Every home's and workplace's door opens onto a lane or alley at least 2 wide. Snickets of 1 tile
  are shortcuts, ways into courts and through-building back doors only. `heldOff` has no room in a 1-wide passage, so
  two residents meeting head-on there could knot.
- **Courts:** at least 6 small enclosed yards, 6 × 6 to 10 × 8, each reached by a 1-wide snicket or through a building.
  Some have a pump `U` against a wall. Pumps are solid, so none stands in a passage.
- **Building shapes:**
  - Blocks run from 5 × 5 to 18 × 14. Narrow fronts are 5–7 wide and 10–14 deep. L-shapes are a rectangle with a
    notched corner, in both the outdoor block and the interior.
  - Buildings have 1 to 3 floors.
  - Small blocks such as 5 × 5 are made with an explicit footprint, because `fp()` cannot produce them.
- **Infill** is solid and has no interior:
  - sheds, privies and lean-tos, 3 × 3 to 6 × 4;
  - boarded-up buildings, 8 × 8 to 14 × 10;
  - walled back-yards closed by `|` fences.

  For coverage, "built" means solid tiles plus tiles fully enclosed and not public.
- **Moving between buildings** (R9):
  - **Through-buildings:** at least 14 ground floors have a front and a back door on different alleys, each door its
    own link.
  - **Passages:** at least 25 `passage` links join neighbouring interiors (back rooms knocked through, cellar runs).
    They form 4–6 chains of 3–5 buildings and stay open. Links have no lock.
  - **Courts** are reached only through a snicket or a building.
- **Coverage first.** `preview` computes the built coverage from the building list **before** it lays the alleys, and
  scales the counts until the target is met.
- **Targets** (8A checks them):
  - built coverage of at least **55%** of the Sump's land;
  - no open space wider than 12 tiles, except the Rag Market, the Gut, Tarry Lane and the courts;
  - at least 60% of the Sump's outdoor walkable tiles in alleys 2 wide or less;
  - at least 10 dead ends;
  - at least 12 distinct footprint sizes and at least 8 L-shaped buildings;
  - every home and work door on a passage at least 2 wide.

### The set pieces

**The Old Keep (R16).**

- It covers x 580–640, y 445–500 at one height (2.0). It straddles the Commons/Rise line at y 470, and its south walls
  are the retaining edge against the Rise.
- It is built in the `greatly()` pattern: the region and its ring are painted at the chosen height, then `gate()` cuts
  the gate.
  - Walls 3 thick on all four sides, with four 7 × 7 corner towers.
  - One gate, in the north wall at (610, 445), facing Keep Street.
- Inside:
  - a small courtyard of about 30 × 20;
  - **the Keep tower** (new, block 20 × 20, three floors): z 0 is the **Watch Office** (the guard office), z +1 the
    captain's rooms and the weapon racks, and z −1 **the cells**;
  - **the barracks range** along the east wall: reuse `rm_the_watch_house` (14 × 28, 15 beds). Its id contains
    `watch_house`, so `communalHome` treats it as communal.

**The administrative sector (R18).**

- The **Records Office** stands on the north side of Records Square (block x 388–412, y 472–488, three storeys).
- The City Office (reused) is on the east side and the Magistrate's Court on the west.
- The Couriers' Office, the scriveners, the Slate & Ledger and the clerks' houses fill the rest.
- The sector is built in the "civic" style: stone, `f` paving and arcades of `I`.
- **The Customs House** (the Crown's) stands on the wharf at the Customs Pier (block x 176–218, y 476–494).

**The Great Palace (R19).**

- Palace Square is its forecourt, entered from Crown Street by a gate pair. The square and the palace are at the same
  height.
- Three blocks touch with gap 0, each built with an explicit footprint:

  | Part | Block | What it is |
  |---|---|---|
  | **King's Residence** (west) | x 350–410, y 552–580 | two floors, with kitchens at z −1 |
  | **Antechamber** (centre) | x 410–450, y 552–580 | the court and senate: benches `p`, a dais, and its great door on the forecourt |
  | **Administrative Wing** (east) | x 450–510, y 552–580 | two floors |

- The wings join the Antechamber by `passage` links (section 3), and each wing has its own side door.
- Behind, at y 584–612, is an ornamental privy garden: lawns, hedges and a fountain, with no crops. Its west wall
  screens the Cinders.

**The abbey and cathedral close (R20).**

- It covers x 222–330, y 154–232. The city wall is its north side. A wall 2 thick on the west, south and east is its
  retaining edge.
- It has two gates:
  - the south gate at (300, 232), onto Tarry Lane and the Sump, a half step down with a `^` run;
  - the east gate at (330, 195), onto Abbey Row and Northgate, at the same height.
- Inside:
  - the cathedral (the kit's `cathedral()`: nave and crypt) along the north;
  - south of it, the cloister garth: an outdoor court of about 20 × 20 with an arcade of `I`, lawn only;
  - around the garth: the chapter house, the dorter (`r2_abbey_quarters`, communal), the refectory, the scriptorium,
    the brewhouse, the abbey school and the infirmary.
- If the kit's cathedral doesn't fit at scale 1, the close may grow east into Northgate up to x 350.

**The Greatly compound (R22–R25).**

- It covers x 540–660, y 154–270. The city wall is its north and east sides. A compound wall 2 thick closes the west
  and south. It is built in the `greatly()` pattern at 3.0.
- **North yard**, y 156–192 (the back yard):
  - the **Greatly Forge & Workshop** (new, block 24 × 14);
  - the Masons' Yard and the Colour Works (both reused);
  - the Greatly Stables.
- **The house:** Greatly House (reused interior 26 × 17, three floors), block x 590–609, y 202–213.
- **South yard**, y 220–268 (the front yard):
  - the drive (`estate_ground`);
  - lawns and hedges (`gardens(crops=False)`);
  - the **fountain** `F` (5 × 5 at x 598–602, y 243–247);
  - the Counting House on the east side.
- **The gate** is in the south wall at (600, 270), a half step down with a `^` run onto Greatly Square. The Greatly
  Yard (the builders' yard) faces the square.

**The estates outside the walls (R26).** Each is a `compound()`, which is allowed outside the walls, on ground of one
level and within one cell. Each has its seat, a gatehouse, stables and two cottages. There are no fields, orchards,
vineyards or pens. Each estate has one `rm_*_cottages` interior, so each gets one new `r2_` cottage.

| Estate | Extent | Cell | Buildings |
|---|---|---|---|
| **Timberland Court** | x 330–480, y 650–750, at the end of the Cinder Road | 1_2 | the seat (reuse `rm_mossgrave_court`, three floors), the gatehouse, the summerhouse, the stables, 2 cottages. The Timberland Iron Office is a room of the seat |
| **Fellhall** | x 780–940, y 300–430, on the East Road | 3_1 | the seat (reuse `rm_fellhall`), the gatehouse, the stables, 2 cottages, the **Fell Press House**, the **Fell Distillery**, the **Fell Preserving House** and the **Fell Cellars** (a reused 12 × 22 store) |
| **Grayrock Hall** | x 125–250, y 650–750, on the south headland at the end of the Wharf Road | 0_2 | the seat (reuse `rm_grayrock_hall`), the gatehouse, the summerhouse, the stables, 2 cottages |

**Roads outside** are 3 wide, `d` over the moorland `,`:

- the North Road (x 476, y 0–150);
- the East Road (y 360, x 664–1024), past Fellhall;
- the South Road (x 600, y 620–768);
- the Cinder Road, from the Cinder Gate to Timberland Court;
- the Wharf Road, from the Harbour Gate to Grayrock Hall.

**The rest of the canvas** is wet moorland and heath with scattered trees (R34). It has:

- no fields;
- **no resource or forage nodes** (`resources` stays empty);
- **no game spawns** (every cell's spawn tables are empty).

### Faction claims

The old claims are all on interiors. A writes the claims as follows:

- **New cell claims:**
  - `ridgemere_watch` and `council_of_houses` claim every cell the wall touches;
  - `house_greatly` claims 2_0;
  - `house_mossgrave` (Timberland) claims 1_2;
  - `house_fell` claims 3_1;
  - `house_grayrock` claims 0_0, 0_1 and 0_2.
- **Rewritten interior claims,** for buildings that changed House:
  - the Bonded Warehouse and the Timber Store go to `house_grayrock`;
  - the Ironworks, Foundry, Iron Store and Sawmill go to `house_mossgrave`.
- **Dropped:** claims on interiors that are deleted. None are, since unplaced interiors stay as stock and keep theirs.

`claims_for` is at `realm.py:1195`. A writes its own rule and doesn't call the frozen one.

Since doc 66 there is no ownership mechanic. A business's owner shows only in names, work labels and talk.

## 3. Buildings

**How buildings are made.**

- Every building goes through `Site.place()`. It makes:
  - a solid block on the outdoor map with its door or doors (`+`);
  - one interior per room or floor;
  - `door` and `stairs` links;
  - a manifest record.
- **A second door** gets the id `{bid}_door_2`, its own tile and its own arrival tile.
- `flats` joins `place_all`'s closed-door kinds. Closed doors open when walked into, since nothing is locked, so
  through-buildings still work.
- A **structure** is one placed building: one manifest record with all its floors and rooms. Touching buildings are
  separate structures, each with its own walls and door.
- **Reusing an interior:**
  - place a block of its `fp()` size and link the outdoor door tile to the interior's stored `+` tile (`doors[].b_x` and
    `b_y` in the placements file);
  - the interior's facing is cosmetic;
  - reused interiors keep their ids, so NPC homes and work spots that point at them stay valid.
- **New interiors** are prefixed `r2_` and reserved against the 626 existing ids. Each has region `ridgemere`.
- **Old interiors not reused** (old Sump tenements, cut mansions, farm buildings, the six towns' 194) stay in the
  database, unlinked, as stock.

**Multi-storey buildings.**

- Every floor is its own interior with its own `z`, and floors are joined by `stairs` links.
- Each stair pair puts `^` on the same (x, y) on both floors, and each stair end has an arrival tile within 0.55 of its
  height.
- A three-floor building uses two stair pairs on different tiles, because validation refuses two links on one tile.
- `z = 2` has not been used before. The scratch-server walk (section 8, step 6) climbs it.

| Building | Floors (keys, z) |
|---|---|
| Records Office | `''` z 0 (the counter hall and the public reading room); `up` z 1 (the clerks' office: desks, a worker post at each); `top` z 2 (the rolls: shelving `stacks`) |
| Keep tower | 0, +1, −1 |
| Greatly House and the three seats | 0, +1, −1 (`manor`) |
| Palace wings | 2 floors; the Residence also has kitchens at −1 |
| Villas, inns, shops with a flat | 2 floors |
| Flats | up to 3 floors |
| Cathedral | nave and crypt |

**Homes (R32).**

- **A home is one interior that NPCs name as `home_area`, together with its beds.** There is no other kind of home.
  - A house whose family sleeps on one floor and servants on another has two homes.
  - So a Minor House mansion, Greatly House, each estate seat and the King's Residence each count **2 homes**: the
    family's floor and the servants' floor.
- **`flats(rng, style, homes=1..3, beds=2..5, shape)`** is a new recipe:
  - it stacks up to three homes at z 0, 1 and 2, each its own interior with 2–5 beds;
  - stairs join them on distinct tiles;
  - the ground floor holds the door, or two doors for a through-building.
- A double house is `flats(homes=2)`.
- Communal homes count one home each: the Keep's barracks, the abbey's dorter and the wharf lodging
  (`rm_the_barracks`, a 6-bed tenement that counts as communal because its id contains "barracks").
- The old one-room `tenement` (6–10 beds) is not used for new buildings.
- **No structure holds more than 3 homes** (checked).

**New kit pieces** (A builds them in `tools/worldgen/buildings.py` and `site.py`; existing recipes keep their
output):

1. **Notched plans.** `Plan.notch(corner, w, h)` makes an L-shape. The outdoor block takes the same mask, through
   `Site.place(..., mask=)`.
2. **Two doors.** `Plan.back_door(edge, x)`. `Site.place` writes one door link per `+`, as above.
3. **`flats`**, as above.
4. **`market_hall(w=48, h=20)`**, a covered hall:
   - 6 NPC stall counters `=`, each with its keeper behind it and an open tile in front;
   - the warden's post;
   - doors on both short ends;
   - no player places.
5. **`keep_tower`:** an office floor, the captain's floor, and **`gaol_cells(cells=8)`**:
   - a corridor with eight 3 × 3 cells;
   - each cell is fronted by `|` bars with a 1-tile `+` opening (cosmetic: links have no lock and no glyph closes);
   - a gaoler's post, and a bench `p` in each cell.
6. **`office_floor(w, h, desks)`:** rows of desks, each with a worker post. It is used by the Records Office's `up`
   floor and the Administrative Wing.
7. **Palace parts:**
   - `palace_antechamber`: the `hall senate` layout scaled to the block, with benches `p`;
   - `palace_admin_wing`: office floors;
   - `palace_residence`: the royal floor and the kitchens from `palace()`.
8. **Abbey pieces:** `dorter(cells)`, a dormitory of cells whose id contains `quarters`; and `cloister`, outdoor and on
   the map only.
9. **Works kinds** (`WORKS`, `WORKS_STATIONS`, `WORKS_TRADE`), using only real station ids. `press_house`, `brewery`
   and `shipwright` already exist.

   | Kind | Stations |
   |---|---|
   | `distillery` | `still` |
   | `preserving_house` | `hearth`, `drying_rack` |
   | `builders_works` | `forge` and a joiner's bench, under one roof |
   | `washhouse` | tubs and coppers as furniture glyphs, no station |
   | `rag_and_bone` | no station |
   | `bathhouse` | no station |

10. **Small structures:** `shed(w, h)` and `boarded(w, h)`, both solid with no interior.
11. **`Ridgemere` overrides** (in the `RidgemereCity` subclass, so the old map's output is unchanged):
    - `market(stalls=N)`;
    - `waterfront()`, without `wharf_buildings()`'s old list;
    - new pier code;
    - `gardens(crops=False)`, with no kitchen garden `4`, orchard or pond;
    - `district(gap=)`.

**The building list.**

- Counts are targets. A may vary them by ±10%, and the Sump's counts further, to meet the coverage and capacity checks.
- "Reuse" means old interiors by id. Names are those after the renames in section 7.

| Building | # | District | Block (typical) | Homes | Made by |
|---|---|---|---|---|---|
| Sump flats (2–3 homes, 1–3 floors, varied and L-shaped, 14 of them through-buildings) | 56 | Sump | 7×6 to 18×14 | 130 | new `flats` |
| Sump cottages and one-room shacks | 32 | Sump | 5×5 to 9×7 | 32 | new `house` (small, notched) |
| Boarded-up buildings (no interior) | 18 | Sump | 8×8 to 14×10 | 0 | `boarded` |
| Sheds, privies, lean-tos (no interior) | 80 | Sump, wharf, Cinders | 3×3 to 6×4 | 0 | `shed` |
| Walled back-yards | ≥ 20 | Sump | 6×4 to 10×8 | 0 | `|` fences |
| Wharf lodging (communal) | 1 | wharf | as stored | 1 | reuse `rm_the_barracks` |
| Cinders row houses | 8 | Cinders | 13×9 interior | 8 | reuse old houses (Forge Row, Kiln Row) or `flats(1)` |
| Middle-class houses | 40 | Commons, Northgate | 16×11, 11×16, 13×9 interior | 40 | reuse `rm_` houses (133 available) |
| Middle-class double houses | 10 | Commons | 14×12 | 20 | new `flats(homes=2)` |
| Shops with a flat (1 home each) | 42 | Commons, Sump, wharf | as stored | 42 | reuse (of 46) |
| Clerks' houses | 8 | admin | 13×9 interior | 8 | reuse `rm_` houses |
| Minor House mansions | 5 | Rise | 20×14 interior (2 floors), walled garden lots of about 44×32 | 10 | reuse villas: Carlisle, Thornbury, Vance, Stanmore, Grantham |
| Greatly House | 1 | compound | 26×17 interior (3 floors) | 2 | reuse `rm_greatly_house*` |
| Great House seats outside | 3 | estates | 26×17 interior (3 floors) | 6 | reuse Mossgrave Court (Timberland), Fellhall, Grayrock Hall |
| Estate gatehouses, summerhouses, stables | 9 | estates | as stored | 3 (gatehouses) | reuse |
| Estate cottages | 6 | estates | as stored | 6 | reuse 3, new 3 |
| Taverns | 9 | Sump 3, wharf 2, Cinders 1, Commons 2, admin 1 | as stored | 0 | reuse all 9 |
| Inns | 2 | the Sailors' Rest (wharf), the Grey Gull (Commons) | 2 floors | 0 | reuse |
| Works (forges, foundry, kilns, mills) | 15 | Cinders | as stored | 0 | reuse (section 4) |
| Wheelwright | 1 | Cinders | 14×10 | 0 | new `works` |
| Warehouses | 4 | wharf 3, Cinders 1 | 12×22 interior; one new 14×24 | 0 | reuse 3, new 1 |
| Ropewalk | 1 | wharf | as stored | 0 | reuse |
| Shipwright's yard | 1 | wharf, at the slipway | 20×14 | 0 | new `works shipwright` |
| Fish Hall, smokehouse, Wharf Tannery | 3 | wharf | as stored | 0 | reuse |
| Grayrock Wharf Office | 1 | wharf | as stored | 0 | reuse the Harbourmaster's Office |
| Customs House | 1 | wharf, at the Customs Pier | 42×18 block | 0 | new `hall reading` and an office floor |
| Washhouse | 1 | Sump | 16×10 | 0 | new `works washhouse` |
| Rag-and-bone yard | 1 | Sump | 12×10 | 0 | new |
| Sump Infirmary, Soup Kitchen | 2 | Sump | as stored | 0 | reuse |
| Chapel of the Tide | 1 | Sump, by Sump Stairs | as stored | 0 | reuse |
| Lamp store (the lamplighters' post) | 1 | Sump | 8×6 | 0 | new `hall warehouse` |
| Market Hall | 1 | Market Square | 48×20 block | 0 | new `market_hall` |
| Market Square's 14 stall tiles and the Rag Market's 6 | | squares | `u` | | map (`market(stalls=)`) |
| Barber-surgeon, Fell Wine Vaults, cheesemonger | 3 | Commons | 12×10 | 1 each | new `shop_with_flat` |
| Bathhouse | 1 | Commons, by the Sump's edge | 18×12 | 0 | new `works bathhouse` |
| Eastgate livery stables | 1 | Commons, by the East Gate | as stored | 0 | reuse |
| Commons School, House of Mending | 2 | Commons | as stored | 0 | reuse |
| City Office | 1 | admin | as stored | 0 | reuse |
| **Records Office** (3 storeys) | 1 | admin | 24×16 block | 0 | new: `hall reading`, `office_floor` and `stacks` |
| Magistrate's Court | 1 | admin | as stored | 0 | reuse |
| Couriers' Office | 1 | admin | as stored | 0 | reuse the old Hall of Records |
| Scriveners | 1 | admin | as stored | 1 | reuse a scribe's shop |
| **The Great Palace:** King's Residence, Antechamber, Administrative Wing | 3 | palace | 60×28; 40×28; 60×28 blocks | 2 (Residence) | new palace parts, joined by passages |
| **The Old Keep:** tower, barracks, walls | 2 and walls | Keep | 20×20 block (3 floors); as stored | 1 (communal) | new tower; reuse `rm_the_watch_house` |
| **Cathedral** | 1 | abbey | the kit's | 0 | new `cathedral()` |
| Abbey: chapter house, dorter, refectory, scriptorium, brewhouse, school, infirmary | 7 | abbey | 10×8 to 20×12 | 1 (communal dorter) | new `hall` kinds, `dorter`, `works brewery` |
| Greatly Forge & Workshop | 1 | compound, north yard | 24×14 block | 0 | new `works builders_works` |
| Masons' Yard, Colour Works, Greatly Stables, Counting House | 4 | compound | as stored | 0 | reuse |
| Greatly Yard (the builders' yard) | 1 | Greatly Square | as stored | 0 | reuse, re-described (section 7) |
| Fell Press House, Distillery, Preserving House, Cellars | 4 | Fellhall | 16×12 | 0 | new `works press_house`, `distillery`, `preserving_house`; reuse a store as the cellars |

**Homes.** The total is about **314**:

- the Sump: 130 + 32 = 162;
- the rest of the city: 1 + 8 + 40 + 20 + 42 + 8 + 10 + 2 + 3 + 1 + 1 + 1 + 2 = 139;
- the estates: 6 + 3 + 6 = 15.

That is against about 260 household units (section 5), which leaves about 55 spare homes for growth.

**Beds.** Every home has at least ⌈people / 2⌉ bed tiles (`b` or `z`), and at least 2. The game's hard limit is 4
sleepers to a bed.

**Not built.** These are left out for R34 or as doc 65's dead trades:

- no farm building, bunkhouse, barn, rabbitry, logging camp, mine or quarry;
- no Salt Pans, City Arsenal, Wagon Yard, Quarry Office or Fell Timber Office;
- no Mossgrave Wool Shed, Wool Store or Fleece Barn;
- no net lofts and no fishing boats;
- no Blue Vats dyeworks: the `dyeworks` business has no crafts, so it would have nothing to make or draw. It is listed
  for the user in section 9, Q7.

## 4. Businesses and jobs

**How a job works** (doc 66, Phase 5):

- A job is one position per NPC: `job:<npc id>`. It is made from the NPC's work label, work spot, hours, route and role.
- The business type is the first `match` hit in `Data/Items/businesses.json`, lower-cased and by substring.
- A merchant's work spot is its counter, and each merchant position opens one till.

**So:**

- **One keeper per till business.** Every business that sells or makes has exactly one `merchant` keeper. Its helpers
  are civilians ("helping at X") whose work spots are in the same building.
- **Every business that uses the store** (one with crafts, or one with resale goods) has a merchant keeper, because the
  store draws into the keeper's till (section 6).
- **Offices and institutions** have no till, and their staff are civilians or guards. They are the City Office, the
  Records Office, the court, the palace, the Keep, the customs house, the warehouses, the Counting House and the
  bathhouse.

**Labels.** A label must resolve to its intended business. A checks every label against these rules:

| Rule | What breaks |
|---|---|
| At most 40 characters | `map_editor.py:413` refuses longer labels |
| No "keeps the " | it matches `inn`. Use "keeping the …", which doesn't (`inn` matches "keeping the inn" only) |
| No "feeding the " or "cooking for" | they match `inn`, except at the inns and the palace kitchens, where they are intended |
| No " press" | it matches `press`, except at the Fell Press House. "printing at the Council Press" hits `printing_house` first, as intended |
| No "forge" | it matches `smithy`, except at real smithies. So the Timberland Iron Office's clerks are "clerking at the Timberland iron office" |
| No "armour" or "armor" | they match `armory`, except at the armourer. So "keeping the Watch's weapon racks" |
| No "bank" | it matches `moneychanger`, except at the Greatly Bank |
| No "stall" | it matches `stall` (food only), except at real food stalls |
| No "stables" | it matches `stables` and the `stock_breeding` producer, except at real stables. Horses are not food (R34 check, below) |
| No " pit", "miner", "the workings" | producer `mine`. So the tannery's hands are "working at the Wharf Tannery" |
| No "burns charcoal" | producer `charcoal_burning`. So "working the Charcoal Store" |
| No "vats" | it matches `dyeworks` |
| No "holding court", "keeping X Court/House/Hall" unless the NPC is a House head | `houseHead()` makes it a House. The King is "reigning at the Great Palace" (section 9, Q3) |
| Clergy words | `clergy()` fires on "abbey" and "healer" as well as on priest, acolyte, deacon, sexton, choir, chapel and cathedral |

**Clergy, decided on purpose.** The abbey's scribes, brewers, teacher, porter and almoner are brothers and sisters of
the abbey, and so are the House of Mending's healers. They count as clergy and keep Restday. Nobody outside the abbey
has those words in their label.

**Minor House heads keep their own House labels** (for example "keeping Carlisle House"), so their `house:*` key and
`self.house` talk survive. Their roles at the court, the bank or the Council appear in their descriptions, not their
labels.

**New business types** (`businesses.json`; B adds them):

- They are inserted before `inn`, except `builders_works`, which goes before `smithy`.
- For each one, B checks that no current Ridgemere 2.0 label changes business except as intended.

| id | kind | match | What it does |
|---|---|---|---|
| `builders_works` | works | `greatly works`, `builders' forge` | the Greatlys' construction ironwork and joinery. It is a maker of the existing crafts whose outputs the builders' yard sells (nails, planks, beams, and any hinges or fittings in the catalogue). It is not a public smithy |
| `distillery` | works | `distiller`, `still house` | crafts aqua_vitae, brandy, apple_brandy and berry_cordial |
| `preserver` | works | `preserving house`, `preserver` | crafts fruit_preserves and dried_apples |
| `vintner` | shop | `vintner`, `wine vault`, `wine merchant` | resells wine, cider, mead, aqua_vitae, brandy, apple_brandy and berry_cordial (its `supplies` list) |
| `shipwright` | works | `shipwright`, `slipway` | no crafts yet |
| `laundry` | works | `washhouse`, `laundr` | no crafts |
| `bathhouse` | works | `bathhouse`, `bath house` | no crafts, no till: an institution |
| `barber` | shop | `barber` | no crafts |
| `pawnbroker` | shop | `pawn` | buys from players (a `buys` list) |
| `rag_and_bone` | yard | `rag-and-bone`, `rag and bone` | resells second-hand household goods (its `supplies` list) |

`cartwright` (match `wheelwright`) and `dairy` (match `cheesemaker`) already exist and are used as they are.

**New items** (`items.json`):

- Each item has every field the catalogue needs: `worth` (rung and at), `category`, `weight`, `stack`, `source`, `desc`
  and `playerSeconds`.
- Each has a recipe in `recipes.json` with an existing `trade` and a real station, and a craft in `crafts.json` with
  `makers`, `seconds` and `hours`.
- Every craft has at most 2 inputs, which is all Core keeps.
- Prices are first guesses. The final prices are whatever keeps a profit at Common quality in
  `tools/item_catalog.py --margins`, which measures in worth.

| Item | Craft (makers) | Recipe (trade, station) | Price |
|---|---|---|---|
| `aqua_vitae` (existing) | new craft: wine 1 + firewood 1 → 2 (distillery) | the existing one | 10p, unchanged |
| `brandy` | wine 2 + firewood 1 → 1 (distillery) | apothecary, `still` | about 45p, above what the same 2 wine makes as aqua vitae (40p) |
| `apple_brandy` | cider 3 + firewood 1 → 1 (distillery) | apothecary, `still` | about 14p |
| `berry_cordial` | berries 3 + honey 1 → 1 (distillery) | apothecary, `still` | about 9p |
| `fruit_preserves` | apples 2 + honey 1 → 2 (preserver) | cook, `hearth` | about 4p each |
| `dried_apples` | apples 3 + firewood 1 → 2 (preserver) | cook, `drying_rack` | about 3p each |

**The "Draws" column.** In the tables below it is **generated, not hand-written.** It is
`item_catalog.store_goods(business)`, computed from `crafts.json` (the union of `in` over the crafts whose `makers`
include the business) plus its `supplies` list. The value shown is today's output. A puts the generated list in
`city-report.txt`.

**The full list.** Staff are written as keeper + help. "(m)" means the keeper is a merchant with a till.

**The wharf (House Grayrock; R5, R28): 53 posts**

| Business | # | Staff | Business id | Draws (generated) |
|---|---|---|---|---|
| Grayrock Wharf Office (harbourmaster, clerks) | 1 | 1 + 2 | – | – |
| Customs House (the Crown's collector and weighers) | 1 | 1 + 2 | – | – |
| Bonded warehouses (the Grayrock Bonded Warehouse, Long Store and New Store) | 3 | 1 + 1 each | – | – |
| Piers and cranes: dockers and cranemen | 8 piers | 10 + 4 | – | – |
| Grayrock Shipwrights (m) | 1 | 1 + 4 | shipwright | – |
| Ropewalk (m) | 1 | 1 + 2 | ropewalk | cord, hemp |
| Cooperage (m) | 1 | 1 + 1 | cooperage | iron_bar, iron_hoops, planks |
| Ship chandler (m) | 1 | 1 + 1 | chandlery | beeswax, cord, hemp, resin, tallow |
| Fish Hall (m) | 1 | 1 + 2 | fishmonger | firewood, fish, salt, and its supply list |
| Smokehouse (m) | 1 | 1 + 1 | smokehouse | firewood, fish, mutton, rabbit, raw_meat, salt |
| Wharf Tannery (m) | 1 | 1 + 2 | tannery | hide, leather, oak_bark, sheepskin, small_pelt |
| Sailors' Rest (m) | 1 | 1 + 2 | inn | apples, milk, oats, raw_meat, vegetables |
| Drowned Lantern, Tarred Rope (m) | 2 | 1 + 1 each | inn | as inns |

**The Cinders (heavy industry, R3; Timberland's forges, R29; Greatly's kilns and plaster): 45 posts.** Every forge and
smelter here is named as Timberland's: in its interior name ("Timberland's Anchor Forge"), its label and its talk.

| Business | # | Staff | Business id | Draws (generated) |
|---|---|---|---|---|
| Timberland Ironworks (m) | 1 | 1 + 5 | ironworks | charcoal, iron_bar, iron_ore, timber |
| Timberland Foundry (m) | 1 | 1 + 2 | foundry | bronze_bar, charcoal, copper_bar, copper_ore, iron_ore, timber, tin_bar, tin_ore |
| Timberland Sawmill (m) | 1 | 1 + 3 | sawmill | timber |
| Timberland Charcoal Store (m) | 1 | 1 + 1 | charcoal | timber |
| Timberland Iron Store | 1 | 1 + 1 | – | – |
| Timberland's smithies: the Nailers' Forge, the Anchor Forge, Forge Row (m) | 3 | 1 + 1 each | smithy | bronze_bar, charcoal, iron_bar, leather, timber |
| Timberland's armourer, Rivet and Rain (m) | 1 | 1 + 1 | armory | charcoal, iron_bar, leather, linen_cloth, steel_bar, wool |
| Cinder Wheelwrights (m) | 1 | 1 + 1 | cartwright | iron_bar, iron_hoops, planks, timber |
| Greatly Plasterworks (m) | 1 | 1 + 2 | plasterworks | firewood, lime, limestone, sand, sheepskin, small_pelt |
| Greatly Lime Kilns (m) | 1 | 1 + 1 | brickworks | clay, firewood, limestone |
| Greatly Tileworks (m) | 1 | 1 + 2 | brickworks | clay, firewood, limestone |
| Cinder Glassworks (m, Greatly's) | 1 | 1 + 2 | glassworks | firewood, limestone, sand, tin_bar |
| Rag Mill (m) | 1 | 1 + 1 | papermill | linen_cloth |
| Rainwater Brewery (m) | 1 | 1 + 2 | brewery | apples, grapes |
| Slag & Anchor (m) | 1 | 1 + 1 | inn | as inns |

**The Sump (R13): 49 posts**

| Business | # | Staff | Business id |
|---|---|---|---|
| The Rag Market (m): 2 food stalls, a fishmonger, a tinker, a second-hand dealer (rag-and-bone) and a shopkeeper (general); tray sellers crying wares | 6 | 6 + 4 | stall ×2, fishmonger, tinker, rag_and_bone, general |
| Sump Sundries (general), Gullgut Fish (fishmonger), Harbour Provisions (provisioner), Mend & Make Do (tinker), Pawn and Ledger (pawnbroker and lender) (m) | 5 | 1 + 1 each | as named |
| Rag-and-bone yard (m) | 1 | 1 + 1 | rag_and_bone |
| The Washhouse | 1 | 1 + 5 | laundry |
| The Bilge, the Rainbarrel, the Knucklebones (a dice-and-card tavern) (m) | 3 | 1 + 1 each | inn |
| Sump Infirmary (the abbey's infirmarian) | 1 | 1 + 1 | – (clergy) |
| Soup Kitchen, "ladling soup" (m) | 1 | 1 + 1 | inn |
| Chapel of the Tide (a priest) | 1 | 1 | – (clergy) |
| Porters' stand at Sump Stairs (outdoor) | | 4 | – |
| Lamplighters (the lamp store) | | 4 | – |

**The abbey and cathedral (R20): 14 posts**

| Post | Staff |
|---|---|
| Abbot | 1 |
| Cathedral priests | 2 |
| Deacon | 1 |
| Sexton | 1 |
| Acolytes or choir | 2 |
| Abbey scribes (scriptorium) | 2 |
| Brewhouse (m): a cellarer, "brewer at the abbey brewhouse" (brewery and clergy), and one brother | 1 + 1 |
| Abbey school teacher | 1 |
| Porter and almoner | 2 |

**The middle class and the main market (R12, R14): 96 posts**

| Business | # | Staff | Business id |
|---|---|---|---|
| Market Square keepers (m), trade labels that name the trade and avoid "stall": 2 food stalls, a weaver, a potter, a tinker, a herbalist, a chandler and a general | 8 | 8 | stall ×2, weaver, pottery, tinker, herbalist, chandlery, general |
| Market Hall: a warden (City Office, civilian) and 6 hall keepers (m): a tailor, a saddler, a cooper, a carpenter, a jeweller and a provisioner | 1 | 1 + 6 | as named |
| Bakeries 3, butchers 2, the Wet Slab (fishmonger) 1, provisioner 1, general 2 (m) | 9 | 1 + 1 each | as named |
| Cheesemonger, "cheesemaker in the Commons" (m) | 1 | 1 + 1 | dairy |
| Hopyard, the brewer's shop (m) | 1 | 1 | brewery |
| Tailors 2, weaver 1 (m) | 3 | 1 + 1 each | tailor, weaver |
| Carpenters 2, cooper 1, potter 1 (m) | 4 | 1 + 1 each | carpenter, cooperage, pottery |
| Chandlers (m) | 2 | 1 + 1 each | chandlery |
| Apothecaries 2 (m, 1 + 1 each), herbalist 1 (m) | 3 | 5 | apothecary, herbalist |
| Saddler (m) | 1 | 1 + 1 | saddlery |
| Timberland's Eastgate Ironmongers and Eastgate Farriers, by the East Gate (m) | 2 | 1 + 1 each | smithy |
| Barber-surgeon (m) | 1 | 1 + 1 | barber |
| Fell Wine Vaults (m, Fell's) | 1 | 1 + 1 | vintner |
| Greatly Bank (m) | 1 | 1 + 1 | moneychanger |
| Council Press (m) | 1 | 1 + 2 | printing_house |
| Soundings (cartographer), the Grey Goose (fletcher) (m) | 2 | 1 each | cartographer, fletcher |
| Clearwater Glass, "glassblower at Clearwater Glass" (m) | 1 | 1 | glassworks |
| Jeweller, perfumer (m), on the Rise's edge | 2 | 1 + 1 each | jeweler, perfumer |
| Grey Gull Inn (m) | 1 | 1 + 2 | inn |
| Black Tun, Last Lamp (m) | 2 | 1 + 1 each | inn |
| Eastgate livery stables (m) | 1 | 1 + 1 | stables |
| Bathhouse | 1 | 1 + 2 | bathhouse (no till) |
| Commons School (teacher); House of Mending (the abbey's healers) | 2 | 1; 2 | – |

**The Greatly compound (R23, R25; construction): 23 posts**

| Business | Staff | Business id |
|---|---|---|
| Greatly House servants | 5 | – |
| Greatly Forge & Workshop (m), "working at the Greatly works" | 1 + 2 | builders_works |
| Masons' Yard (m) | 1 + 2 | mason |
| Colour Works (m) | 1 + 1 | paintworks |
| Greatly Yard (m) | 1 + 3 | builders_yard |
| Counting House | 3 | – |
| Greatly Stables | 2 | stables |
| Gatekeeper, "keeping the Greatly gate" | 1 | – |

The forge's text says the city's forges are Timberland's. The Greatlys forge only for their own building work.

**The administrative sector (R18): 22 posts**

| Business | Staff |
|---|---|
| City Office | 5 |
| Records Office | keeper of the rolls ("keeping the rolls at the Records Office", 39 characters) 1, clerks 4, copyists 2 |
| Magistrate's Court | magistrate, bailiff, clerk |
| Couriers' Office | 3 |
| Scriveners (m) | 1 + 1 (scriptorium) |
| Slate & Ledger (m) | 1 + 1 (inn) |

**The Great Palace (R19; the Crown): 19 posts, plus 4 palace guards**

| Part | Staff |
|---|---|
| Antechamber (the court and senate, where the Council of Houses sits) | chancellor 1, ushers 2, herald 1 |
| Administrative Wing | royal secretary 1, treasurer 1, clerks 5 |
| King's Residence | servants 6, cooks 2 (civilians, "cooking for the King's household") |

**The Old Keep and the Watch (R16, R33): 32 posts.** 30 have the `guard` role and 2 are civilians.

- Captain 1, sergeants 2, gaolers 2.
- Gate posts 9: the North, East and South Gates 2 each; the Harbour Gate, the Cinder Gate and the Customs Pier 1 each.
- Patrols 12:
  - wall walk 4 (`rm_wall_walk`, round the abbey and the Greatly compound by street);
  - streets 4 (`rm_street_patrol`: High Street, Market Square, Crown Street);
  - wharf 2 (`rm_wharf_patrol`);
  - Sump 2 (new `r2_sump_patrol`, along the Gut and Tarry Lane).
- Palace guard 4 (new `r2_palace_round`, round the forecourt and the Antechamber).
- Civilians: a watch clerk, and the keeper of the weapon racks ("keeping the Watch's weapon racks").
- **Posts:** A lays posts on every route in use: those four `rm_` routes, `rm_grayrock_journey` (along the Wharf Road
  to Grayrock Hall) and the two new `r2_` routes.

**The Minor Houses (R15): about 14 posts.** These are their existing servants, "in service to the Xs". The heads keep
their House labels (above).

**The estates (R26, R27): 25 posts**

| Estate | Posts |
|---|---|
| Timberland Court | servants 3, gatekeeper 1, groom 1, the Iron Office's clerks 2 |
| Fellhall | Fell Press House (m) 1 + 2 (`press`: apples, grapes); Fell Distillery (m) 1 + 2 (`distillery`); Fell Preserving House (m) 1 + 1 (`preserver`); Fell Cellars 1; servants 3 |
| Grayrock Hall | servants 3, gatekeeper 1, groom 1 |

**Totals.**

- About **392 fixed posts** and about **105 till businesses.** 106 merchants are kept; any merchant left over becomes a
  helper.
- **Elastic pools** absorb working-age NPCs beyond the fixed posts, each with a cap:
  - dockers, up to 10 more;
  - porters, 6;
  - each works' hands, 1 more each;
  - the Washhouse, 3;
  - house servants.
- **R33 is met:** the market square and hall, the forges, the plasterworks, the administrative buildings and the Watch
  all have posts.
- **R34, no food growing.** No placed NPC's label matches a **food producer** in `crafts.json`: farm, orchard,
  vineyard, apiary, dairy_herd, swineherd, sheep_farm, rabbit_farm, sheep_pasture, fishing, eel_traps or
  herb_gathering.
  - No placed NPC matches a raw-resource producer either: logging, woodcutting, mine, copper_pit, tin_streams,
    clay_pit, quarry, peat, reed_beds, charcoal_burning or salt_pans.
  - The one exception is `stock_breeding`, which matches the stables (horses, not food). The report lists those NPCs.
  - Food shops get their raw inputs (flour, fish, meat, apples) only through the store.
  - The four who "fish the Sound" become dockers.
  - The two who "tend the gardens" keep the palace's and the Greatlys' ornamental gardens.

**Selling to the city's keepers.** Players sell to each other at Market Square's 6 player places, and to the keepers
whose businesses have a `buys` list (`crafts.json` `buys`, and the new pawnbroker's). B lists in the report which
market businesses buy from players. Adding `buys` lists to more trades is the user's choice (section 9, Q8).

## 5. The people

### Who is placed (R31)

- **The in-city people.** For each NPC, A traces the home interior through `world.links` stairs to its street door,
  then that door's old cell in the placements file. The in-city people are those whose door was on a `w_ridgemere_*`
  cell, plus Grayrock Hall's (`artifacts/ridgemere2/analysis/incity.json`).
- **Placed: about 559.**

  | Group | People | Where |
  |---|---|---|
  | The analysts' selection (`artifacts/ridgemere2/analysis/selected.json`: 363 `rm_` and 149 `rmx_`) | 512 | inside the walls |
  | The Harrowgate household, moved whole to the King's Residence (section 9, Q3) | 5 | inside the walls |
  | The three estates' households, whole (seat, gatehouse, cottages) | 14 + 14 + 14 = 42 | the estates |

  That is 517 inside the walls, the wharf included, and 42 at the estates.
- **Unplaced: about 524.** They are:
  - all 262 `tn_` and 200 `cty_` NPCs;
  - the 10 farm-barn households;
  - 12 of the 18 Minor House mansions with their servants, about 62 people (`mansions.json`).

  The kept mansions are Carlisle, Thornbury, Vance, Stanmore and Grantham. Harrowgate goes to the palace.
- **What unplacing does:**
  - it sets `home_area`, `work_area` and `evening_area` to NULL (coordinates 0, 0);
  - voices, relations, talk and roster entries stay, so placing them later restores them whole;
  - the 12 town guards' routes stay without posts.
- **Households stay together** (`hh.json`):
  - A household is a connected group of `npc_relations`, plus anyone sharing its old home when that home held 4 or
    fewer people.
  - A household is all placed or all unplaced. No family is split, and this is checked.

### Homes (R32)

- **District by origin:**

  | Old place | New district |
  |---|---|
  | Old Sump, 1_1 | the Sump |
  | The Commons, 2_1 | the Commons and Northgate |
  | The Heights' kept mansions | the Rise |
  | Forge and Kiln Rows, 1_2 | the Cinders and the Sump |
  | Grey Gull Inn and the old Southgate | the Commons |
  | Greatly House | the compound |
  | Mossgrave Court, Fellhall, Grayrock Hall | their estates |

- **Shopkeepers** stay in their reused shop's flat, so their home and work ids don't change.
- **Crowded old rows** split into units. These are homes of more than 4 people with unrelated members, such as Fishgut
  Lane, Brine Yard, Dockside Row and the Hulks.
  - Each family becomes its own unit, and singles are put in twos and threes.
  - Each unit gets a home in a Sump `flats` building, at most 3 units to a building.
- **Communal homes:**
  - the Keep's barracks: unmarried guards, up to 15;
  - the abbey's dorter: monks and acolytes without a family;
  - the wharf lodging: single dock hands, up to 6.

  Married guards and clergy live at home.
- **A few people aged 18–32 live alone,** in at least 8 homes of their own, so that later `HouseholdMost` splits have
  free homes to go to.
- **Beds** are as in section 3. No home's people exceed 4 × its beds.
- **In a mansion,** the family's floor and the servants' floor are two homes, each with its own beds.

### Jobs

The allocator runs in this order:

1. **Kept posts.** An NPC whose business survives in a reused interior keeps its work area, spot, hours and label. The
   label is checked against section 4's rules and reworded only if it breaks one.
2. **Moved trades.**
   - The keepers and staff of the Grayrock Ironworks, Foundry and Iron Store, and of the Fell Sawmill, move to
     Timberland.
   - The Greatly Bonded Warehouse and the Fell Timber Store go to Grayrock as wharf warehouses.
   - Their labels follow the renamed interiors (section 7).
   - Wool and cedar traders get new posts by rule 4.
3. **New fixed posts** (section 4), filled from those whose posts were lost (227 of the 512):
   - **guards:** 15 adults aged 18–45 from the porters, messengers and idle, their role set to `guard`;
   - **clergy:** about 12, from the chapel and healer titles plus idle adults, retitled with a clergy word;
   - **the palace** (19), **the Records Office** (7), **customs** (3), **the shipwright** (5), and **the Washhouse**:
     the 13 launderers fill it first, and the rest go to the abbey, the bathhouse and the inns;
   - **markets:**
     - the 24 stall merchants go to Market Square and the Market Hall (from the Commons and the Heights) or to the Rag
       Market (from the Sump);
     - each is given a trade label by the stock it held (section 4);
     - the 18 tray sellers go to the Rag Market, the porters and the market porters;
   - **lamplighters** to the lamp store; **wharf hands** to the piers.
4. **Elastic pools.** Every remaining placed NPC aged 16–59 gets a post in a pool, by district.
5. **Children** aged 6–15 are "at lessons" at the Commons School or the abbey school. Those under 6 have no work.
6. **People aged 60 and over** keep a post they had if it survived. Otherwise they have none (retired).

**Merchants.** There is exactly one merchant per till business:

- A merchant whose business is gone keeps the role at a new business (barber, pawnbroker, vintner, rag-and-bone,
  distillery, preserver, cheesemonger, wheelwright), or becomes a helper (a civilian).
- Every business that uses the store has a merchant keeper.

**Labels at write time.** The placement plan records each post as a business and a role, not as finished text. The
write renders the label from the **freshly loaded** interior names, which already carry C's renames. No text comes from
`HOUSES`: A keeps its own text table, with Timberland, in `ridgemere2.py`.

### Evening places

Each adult goes to the nearest tavern or inn of the home's district. Each place takes at most about 40 people. The
overflow, and every parent of a child under 6, spends the evening at home.

| Home | Evening |
|---|---|
| Sump | the Bilge, the Rainbarrel, the Knucklebones, the Soup Kitchen |
| Wharf | the Drowned Lantern, the Tarred Rope, the Sailors' Rest |
| Cinders | the Slag & Anchor |
| Commons and Northgate | the Black Tun, the Last Lamp, the Grey Gull |
| Admin | the Slate & Ledger |
| Clergy | the cathedral |
| Guards | the Keep |
| Rise, compound, palace, estates | home |
| Children | home |

A checks that no evening place takes more than 40.

### Talk

- `talk_seed.py` reads the world's manifest, `artifacts/<world key>/buildings.json`, which is
  `artifacts/ridgemere2/buildings.json` for this world (section 8A writes it).
- The manifest's districts use the names `talk_seed.quarter_of` already knows, so Data/Talk needs no new quarter keys:

  | District | Talk quarter |
  |---|---|
  | Sump | `sump` |
  | Commons and Northgate | `commons` |
  | Admin and palace | `office` |
  | Cinders | `works` |
  | Wharf | `wharf` or `docks` |
  | Rise and Keep | `heights` |
  | The Greatly compound | `estate_greatly` |
  | The estates | `estate_<house>` |

- **Voices.** The `quarter` column of `live.npc_voice` is what talk keys on. For every placed NPC whose district
  changed, A updates it from the new manifest, in the write's transaction (section 8A, step 2). This is the one change
  A makes to a voice.

## 6. The internal store room (R35–R39)

### Behaviour

**What it is.** A business's store room is not physical. It holds without end two kinds of goods:

- **Making goods:** every input of the business's own crafts. This is the sorted union of `crafts.in` over
  `craftsFor(business)`, intermediates such as iron_bar, charcoal and lime included, so a smithy needn't wait for the
  ironworks.
- **Resale goods:** what a vendor sells. This is the business's `supplies` list in `crafts.json` together with the kinds
  of wares the keeper started with. A stall, a fishmonger, the vintner or the builders' yard can therefore keep its
  shelves (R35: "each shop owner/vendor/smith").

`storeGoods(keeper)` is the union of the two kinds for the modes that are on. It is cached per keeper.

**Who can draw.**

- The keeper of a merchant position whose business has making or resale goods, through the position's till.
- Offices, unplaced NPCs and civilians without a till cannot draw.

**The price rule.**

- A draw of `quantity` of `item` costs `ceil(catalogPrice(item) × quantity × markup / 100)`.
- The markup is the making markup (default **100**) for a making good, or the resale markup (default **75**) for a
  resale good. An item in both lists takes the making markup.
- At a resale markup of 100, a vendor would sell at cost and earn nothing, which is why its default is 75.
- **A markup of 0 is free:** no money moves, only the goods. Above 0, a draw costs at least 1p.
- Both markups run from 0 to 1,000.
- Drawn goods are always Common quality.
- A draw needs the whole cost in the till's cash, or it is refused.
- One draw is at most 1,000 of one item. The till's own limits hold: 64 kinds and 10,000 of each.

**Conserved money** (Option A, decided; Q1).

- The coins move from the till to **`fund:store`** by `shift`, and the goods are made by `create` (goods, never money),
  so `conserved()` holds.
- The `fund:` prefix is already a facility account, and `restore`'s sweep keeps facility accounts.
- The fund is opened empty in `resetAuthored`, and in `restore` when an older save lacks it.
- The money stays in the world but out of circulation until a rule spends it. Section 9, Q1 explains that this drains
  the tills over time.
- Rejected: Option B, the treasury, which is mixed with players' welcome grants; and Option C, a sink, where money
  leaves the world.

**Atomic draws.** `shift` can move only part of a sum when the receiver would pass `MoneyCap`, and `create` can fail on
`itemValid`, `StockCap` or `MaxGoodsKinds`. So `drawFromStore` checks exactly what both will check before it does
either:

- the item is valid;
- the till's stock of it plus the quantity is within `StockCap`;
- the till has room for a new kind, if the item is new to it;
- the till's cash covers the cost;
- `fund:store` plus the cost is within `MoneyCap`.

Only then does it shift and create. If `create` still fails, the coins are shifted back and the draw reports a refusal.
A test forces this path.

**A restocking rule, built but off** (`storeRule`; Q2).

- Doc 66 removed every NPC making rule at the user's request. The user's own words for this build are "then we'll test
  its economy", and a store that no NPC calls would show nothing in that test.
- So B builds the simple rule below behind its own switch, which is **off by default** and is turned on only when the
  user confirms it or gives another.
- At the `atWork` hook (`Society::decideAuthored`, second half, in `RatwResidents.cpp`), at most once per game hour per
  keeper, for each ware below its starting stock:
  - if one of the business's crafts makes it, draw one batch's inputs, consume them and create the batch's output
    (Common quality);
  - otherwise, if it is a resale good, draw up to the starting stock (at most 1,000).
- The rule touches no needs, wages or prices. `NeedsPaused` stays true.
- Until the switch is on, **no NPC uses the store.** Only the Dev Console command and the tests call it. The integration
  report says so plainly.

**If the founder's spec is missing** (the founder is unplaced or gone), the business's own flag counts as **on**.

### The Core API

B builds it in `Core/RatwSociety.h/.cpp`, beside `catalogPrice` and `tillOf`.

```cpp
// The store room (doc 67): a business's craft inputs and resale goods, without end, at the catalogue's price.
static constexpr const char* StoreFund = "fund:store";
enum class StoreUse { None, Make, Resell };
bool storeOpen(const std::string& keeper) const;          // world switch(es), the business's own, a till, goods to draw
const std::vector<std::string>& storeGoods(const std::string& keeper) const;   // cached per keeper, like waresCache_
StoreUse storeUse(const std::string& keeper, const std::string& item) const;   // Make wins when an item is both
std::int64_t storeCost(const std::string& keeper, const std::string& item, int quantity) const; // 0 at markup 0, else >= 1
EconomyResult drawFromStore(const std::string& keeper, const std::string& item, int quantity,
                            const std::string& why = "from the store room");
```

**`storeOpen`** is true when all of these hold:

- at least one world mode that applies to the business is on: `authored_.economy.internalStore` for making goods,
  `authored_.economy.storeResale` for resale goods;
- the position's founder's `ResidentSpec::internalStore` is true, or the founder's spec is missing. Positions outlive
  their holders, so the flag follows the business;
- `tillOf(keeper)` is a `till:` account;
- `storeGoods(keeper)` is not empty.

**`drawFromStore`** checks, in order:

1. the store is open;
2. the item is in `storeGoods`;
3. the quantity is 1–1,000;
4. the atomic pre-checks above.

Only then does it `shift(till, StoreFund, "", 0, cost, why)`, unless the cost is 0, and `create(till, item, quantity,
why)`. Both go into the ledger and the journal. A refusal changes nothing and says why.

**Specs.**

- `EconomySpec` gains `bool internalStore = true; int storeMarkup = 100; bool storeResale = true; int resaleMarkup = 75;
  bool storeRule = false;`.
- `ResidentSpec` gains `bool internalStore = true;`.

**Saved state is unchanged.** `fund:store` is an ordinary saved account. The toggles are authored, not part of
`SocietyState`, and `readSociety` still reads 12 fields.

**The Dev Console** has two commands:

- `/store KEEPER ITEM N` draws and prints the result;
- `/store KEEPER` lists the goods with their prices, which mode each is in, whether the store is open, and the balance
  of `fund:store`.

It is allowed only when the server's world is a dev world (`kind: dev` in the registry, scratch servers included) **and**
the player has the `dungeonMaster` flag. The console's 60-character limit is raised to 120 for `/store`. Keeper ids are
taken as they are, since ids are lower-case already.

**`world_check`** prints the balance of `fund:store` at the end of a run. It also reports **residents held for more
than 10 game minutes** in one place, for the Sump's narrow passages (section 8, step 5).

### The toggles and where they are stored (R37, R38)

**Migration `0055_store_room.sql`:**

```sql
ALTER TABLE live.economy
    ADD COLUMN internal_store boolean NOT NULL DEFAULT true,
    ADD COLUMN store_markup   integer NOT NULL DEFAULT 100 CHECK (store_markup BETWEEN 0 AND 1000),
    ADD COLUMN store_resale   boolean NOT NULL DEFAULT true,
    ADD COLUMN resale_markup  integer NOT NULL DEFAULT 75  CHECK (resale_markup BETWEEN 0 AND 1000),
    ADD COLUMN store_rule     boolean NOT NULL DEFAULT false;
CREATE TABLE live.business_settings (
    world_id       ratw_id NOT NULL,
    npc_id         ratw_id NOT NULL,          -- the business's founder (its keeper's NPC), position job:<npc_id>
    internal_store boolean NOT NULL DEFAULT true,
    updated_at     timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, npc_id),
    FOREIGN KEY (world_id, npc_id) REFERENCES live.npcs (world_id, id) ON DELETE CASCADE);
```

- **Grants:** ratw_editor and ratw_publisher read and write (the publisher for sync). The game and dm roles read.
- **The migration also replaces `live.people_manifest`:**
  - one `store MAKE MARKUP resale RESALE MARKUP rule RULE` line (for example `store on 100 resale on 75 rule off`)
    after the `economy` line, **only when a `live.economy` row exists and any of the five values differs from its
    default**;
  - one `nostore "<npc id>"` line per business with `internal_store = false`, **after the resident records**, as
    `protected` and `joinable` lines follow their residents. These lines are written only for founders who are placed
    (`home_area IS NOT NULL`).
- **Defaults:** no row and no line means everything is at its default: on, 100, on, 75, rule off (R38). A
  `business_settings` row exists only while it differs from the default, and the tools delete a row that is back to
  default.
- **Applied to:** `ratw_ridgemere2` and the tests' throwaway worlds only (`world_db.py migrate --db ridgemere2`).
  Migrating DEV and PROD is the user's to run, as 0054 was.

**Worlds without 0055.** The Python code is shared by every world, so every reader and writer must cope with worlds
that lack the migration:

- `world_store.store_supported(conn)` is an `information_schema` check of both the table and the new columns, like
  `fingerprints_supported`. Where 0055 is missing:
  - the economy entity reads the defaults;
  - the save skips the columns and the table;
  - a save that would store a non-default store setting is **refused**, with "this world lacks migration 0055".
- `world_sync` treats a table missing on either side as having no rows.
- `world_build.LIVE_LAYERS` skips the table where it is missing.
- DEV and PROD therefore load, save, build, pull and import exactly as before.

**In the project.** The economy entity carries the settings, so `save_world`, the exporter and Atlas's keyed edits all
carry them:

- it gains `internalStore` (bool), `storeMarkup` (int), `storeResale` (bool), `resaleMarkup` (int), `storeRule` (bool)
  and `businesses` (`{npc id: {internalStore: false}}`);
- **all six are optional, with defaults,** so older projects and test fixtures still pass.

| File | Change |
|---|---|
| `world_store.py` | `DEFAULT_ECONOMY` gains the six keys at their defaults. `economy_row` and `economy_entity` carry the five columns. `TABLES` gains `live.business_settings`, written from `economy.businesses` (all behind `store_supported`) |
| `live_edit.py` | the `economy` entity |
| `map_editor.py` | the checks: markups 0–1000, and every `businesses` key names an NPC. The exporter writes the `store` and `nostore` lines exactly as the manifest does: same conditions, same order |
| `Editor model.mjs` | the economy validation accepts the six optional keys as well as the 5 required ones |

**A danger, avoided.** `save_world` already creates a missing `live.economy` row from `DEFAULT_ECONOMY`
(`world_store.py:314`): treasury 1000, herbs 100, meals 50. It must keep doing so, and never fall back on the table's
column defaults (treasury 0). Ridgemere 2.0 has no `live.economy` row today, so its first save creates one this way.

**Core parsing** (`RatwAuthoring.cpp`, beside `economy`, `protected` and `joinable`):

- **`store …`:** at most once. Each value is `on` or `off`, and each markup is 0–1000. Anything else is rejected.
- **`nostore "<id>"`:** checked at the end of the parse, once every resident is known.
  - An id that is a resident is applied; a duplicate is rejected.
  - An id in `roster.unplaced` is accepted and ignored.
  - Any other id is rejected.
- **`Game::withoutPeople`** (`RatwGame.cpp:679`) strips the people's lines from the build before it appends
  `live.people_manifest`. It now strips `store ` and `nostore ` too, so a streamed build's own lines don't arrive
  twice.

**Sync** (decided):

- `live.business_settings` joins `world_sync.SYNCED`, with the NPCs' `ONLY` filter. It is authored configuration, and it
  travels with Update from live and Import from dev.
- The world switches and markups stay in `live.economy`, which is never synced. They are an operator's lever for each
  world, and the page says so.

**Reaching the game server:**

- The server reads the manifest at start (`Game::loadWithLivePeople`). A changed toggle takes effect when a server next
  starts on the world; for a scratch server, that means starting a new one.
- No rebuild is needed, because the manifest comes from the database. File exports carry the same lines.
- A live `economy.sync` DM action is not built, since no live world uses the store yet. It would be a `dm.actions` kind
  in the queue loop, like `npc.sync`.

### Atlas: Economy controls (R37)

**A page in Atlas's bar.** `{id: 'economy', label: 'Economy', title: 'The store room and other economy controls'}` is
added to `ATLAS_PAGES` (`Editor/src/studio/Studio.tsx`). The new `src/studio/EconomyControls.tsx` renders it
(`?suite=atlas&page=economy`), on Atlas's world.

**Routes** live in `tools/studio.py`, as a set `ECONOMY_ROUTES` dispatched like `SYNC_ROUTES`:

- **Reads** are open to any signed-in account.
- **Writes** need `dm` or `admin` plus Atlas's session token.
- Writes go through `live_edit`'s economy entity, so the edit log keeps open Workshop sessions current.
- Each write is audited in the world's `dm.audit` through `DungeonMaster.log`.

| Route | Does |
|---|---|
| `GET /api/atlas/economy` | returns `{internalStore, storeMarkup, storeResale, resaleMarkup, storeRule, supported, businesses: [...]}`. Each business is `{npc, name, label, business, district, interior, making, resale, internalStore, effective, why}`. Every placed merchant is listed; one with nothing to draw is greyed ("draws nothing") |
| `POST /api/atlas/economy/store` `{internalStore?, storeMarkup?, storeResale?, resaleMarkup?, storeRule?}` | the world switches (all at once) and the markups |
| `POST /api/atlas/economy/businesses` `{set: {npc: bool}}` or `{all: bool, business?: id, district?: name}` | per business, or every listed business at once (filtered or not), in one edit |

**Matching businesses.**

- The business is matched by `item_catalog.business_for(label)`, a new function that mirrors `businessFor`: lower case,
  first match in file order.
- `item_catalog.store_goods(business)` mirrors the making and resale lists.
- A shared fixture, `Tests/fixtures/business_labels.json` (a new directory), is tested in both Python and Core, so the
  two cannot drift. The Core test is given the source directory to find it.

**The page shows:**

- the world switches: "Making from the store room: on for this world" and "Vendors restock from the store room: on";
- the two markups, as percentages;
- the restocking rule's switch: "Restocking rule (off until you confirm it, doc 67 Q2)";
- the note "per world; not carried by Update from live or Import from dev";
- the table: keeper, label, business, district, the goods it would draw with their prices and mode, and on/off;
  - it filters by business and district;
  - it has "All on" and "All off" buttons for the rows shown;
- each business's effective state: on, "off (this business)" or "off (whole world)";
- "Takes effect when a server next starts on this world.";
- for a world without 0055: "This world lacks migration 0055: the store is on at its defaults and can't be changed
  here."

**Retiring the stale panel.** The People workspace's "Town economy" panel (`Editor/src/components/Inspector.tsx`)
describes rules that are gone. It becomes a short note linking to this page.

**The DM's money view** (`dungeon_master.py`) lists `fund:` accounts. It reads `game.checkpoints`, and the DM suite
serves only live worlds, so it will show `fund:store` only on a live world that saves. For Ridgemere 2.0 the fund is
shown by `/store` and by `world_check`.

## 7. Renames (R21, R25, R27–R29)

**House Mossgrave becomes House Timberland in Ridgemere 2.0's text.** Ids are kept, because the foreign keys don't
cascade on update and the sync base keys on them. Only the text changes. The kept ids are:

- `house_mossgrave`;
- `rm_halvard_mossgrave`, `rm_signe_mossgrave`, `rm_wenna_mossgrave`, `rm_osric_mossgrave`;
- `rm_mossgrave_court*` and the other interior ids.

The ids will still read "mossgrave" in Atlas's and the DM's screens. The report tells the user so.

**DEV and PROD keep their text.** They are other worlds, built by the frozen generator. Renaming them is the user's
call, and `rename_house.py` refuses them.

**In `ratw_ridgemere2`:**

- `live.factions.name`: "House Timberland".
- `live.npcs`:
  - the 4 surnames;
  - the 8 work labels, 18 descriptions, 55 backstories and 9 greetings that name Mossgrave;
  - the head's label becomes "keeping Timberland Court", so the house key becomes `timberland_court`.
  - Matching is by whole word. "Fell" also hits Fellside, Oakfell, Coldfell and Icefell.
- `live.npc_voice`: the 4 rows.
- `world.interiors`: 11 names and descriptions. Mossgrave Court becomes Timberland Court. The wool shed, wool store and
  fleece barn stay as unplaced stock, renamed.
- **The Greatly Yard** (`rm_the_greatly_yard`) was a shipwright's interior, and its text mentions a hull on the slip. It
  is re-described as the builders' yard: stacked stone, tile, lime and timber.

**The trades follow the spec.** Text, labels and interior names change so each House is known for its trade. Every
label stays within 40 characters.

| House | Its trade now | Changes |
|---|---|---|
| **Timberland** | the city's forges (R29) | "The Grayrock Ironworks", "Foundry" and "Iron Store" become "The Timberland Ironworks", "Foundry" and "Iron Store". The Fell Sawmill becomes "The Timberland Sawmill" (Q4). The city's smithies and the armourer are named as Timberland's. "buying Grayrock iron" becomes "buying Timberland iron". "trading Mossgrave grey" (wool) is reworded to iron, or to a new trade by section 5. The seat's "Forge Office" becomes the "Iron Office" |
| **Grayrock** | the wharf (R28) | "stone and iron" becomes "the wharf, its ships and warehouses". "The Greatly Bonded Warehouse" becomes "The Grayrock Bonded Warehouse", and "The Fell Timber Store" becomes "The Grayrock Long Store". The Harbourmaster's Office becomes "The Grayrock Wharf Office". "keeping the wharf's books" moves from Greatly to Grayrock |
| **Fell** | fine wines, liquors and fruit goods (R27) | the cedar and timber lines become wines, brandies, cordials and preserves. "selling Fell cedar" becomes "selling Fell wine" |
| **Greatly** | construction (R25) | brick, tile, lime, plaster, paint, stone and the city's building. Their lore says the Greatlys built the walls of Ridgemere. Their ship and wharf lines are reworded. The Greatly Bank and Counting House are kept as the House's purse (Q6) |

**In git:**

- **`Data/Talk`:** 16 files, about 38 lines, listed in the NPC report.
  - Every world served from this branch shares these files, so keys are **widened, not replaced:**
    `self.house=mossgrave_court` becomes `self.house=mossgrave_court|timberland_court` in all 6 places.
  - Lines whose text names Mossgrave's trade are reworded to Timberland's forges. "Mossgrave" in line text becomes
    "Timberland". DEV and PROD keep their own Mossgrave names in the database, and their talk lines still fire.
- **Elsewhere:** `tools/voice_tables.py:54`, and the comment at `Core/RatwSociety.cpp:1156`.
- **The old generator is frozen.** C makes no change to House text, ids or `HOUSES` in `cities.py`, `citizens.py`,
  `country.py`, `realm.py` or `test_realm.py`. Renaming a seat in `HOUSES` would change generated ids, for example
  `rm_mossgrave_court` to `rm_timberland_court`. That would break `test_realm` and the old map.
- **Historic design docs** (60, 61, 64) are records and keep their text. This doc notes the rename.

**Word fixes (project rule):**

- In the generator, `cities.py:912` ("wet dogs") and `:1261` ("a dog asleep") become wolves and pups. This changes text
  only, no ids, and `test_realm.py` still passes.
- The 15 interiors in `ratw_ridgemere2` whose text has the words (`rm_the_bridge_lamp`, `rm_judd_house` and the rest)
  are fixed the same way.

## 8. Build steps: three parallel implementers

Each implementer works on its own branch from `ridgemere2` (94e025d). **Nobody writes `ratw_dev` or `ratw_prod`,
starts the real server, or pushes.** Scratch servers only: `bash tools/scratch.sh ridgemere2`, in the background,
killed afterwards.

**Step 0, before the lanes start** (the orchestrator does it): copy `selected.json`, `hh.json`, `incity.json`,
`mansions.json`, `labels.tsv` and `npcs.json` from the scratchpad's `city/` folder into
`artifacts/ridgemere2/analysis/`, and commit them on `ridgemere2`.

### A. The city generator and the write

**Builds:**

- **`tools/worldgen/ridgemere2.py`** (new), with a **`MiniRealm`**. It holds only what `City` and `Ridgemere` use:
  - `c`: a 1,024 × 768 `Canvas`;
  - `water`, `ocean`, `lake`, `river` (empty) and `biome` masks;
  - `places`, `levels`, `gates`, `gate_levels`, `ua_mask`, `reserved_ids`, `city_cells`.
- **Its own copies of realm.py's size-bound helpers,** with `H, W = c.codes.shape` in place of `SIZE`:
  `flatten_mask`, `route_costs`, `route`, `road`, `settlement`, `_mask` and `causeway`.
- **Its own passes:**
  - `cells()`: ids `w_ridgemere_<cx>_<cy>`, weather `rain` and region `ridgemere` on all 12 cells;
  - `mark_cliffs()`.
- **A `RidgemereCity(Ridgemere)` subclass,** so that `market`, `corners`, `waterfront`, `gardens`, `estate` and
  `lift_around` are at hand. It lays out sections 2 and 3:
  - it overrides `market(stalls=)`, `waterfront()` (no `wharf_buildings()`), `gardens(crops=False)` and
    `district(gap=)`, and lays the piers with new code;
  - inside the walls, the walled pieces use the `greatly()` pattern; `compound()` is used only for the estates;
  - it doesn't change any method's behaviour for the old map.
- **The new kit pieces** in `buildings.py` and `site.py` (section 3). Existing recipes keep their output.
- **Commands:**
  - `preview [--seed N]` writes:
    - `artifacts/ridgemere2/preview/`: an overview with cell outlines and district labels, and glyph renders of the
      Sump, Market Square and the Hall, the Keep, the Records Office's three floors, the palace, the abbey, the Greatly
      compound and each estate;
    - `artifacts/ridgemere2/city-report.json` and `.txt`: counts per district (buildings, homes, beds, posts,
      businesses by id), links by kind, homes per structure, the Sump metrics (coverage computed **before** the alleys
      are laid), the generated Draws per business, every label with its resolved business, and every check;
    - **`artifacts/ridgemere2/buildings.json`**, the world's building manifest for `talk_seed`. It has a record for
      every placed structure, reused ones included (block derived from the interior's size), the three doc 61 works
      among them, with the district names of section 5's Talk table.
  - `place` writes the NPC plan, `artifacts/ridgemere2/placement.json`. For every NPC it records home, work, evening,
    business and role (not finished label text), route, or "unplaced" and why.
  - `write --world ridgemere2` runs steps 1–4 below.
  - The seed is fixed, so runs are deterministic.
- **The write:**
  1. **Back up.**
     - Run `pg_dump -Fc "$(python3 tools/world_db.py conninfo ridgemere2 --role owner)"` into
       `artifacts/backups/ratw_ridgemere2_<ts>.pgdump`.
     - Dump `load_world()` to `artifacts/backups/upper_accord_ridgemere2_r<rev>_<ts>.atlas.json`.
     - To restore, close every connection (studio, scratch servers), then run
       `pg_restore --clean -d "<owner conninfo>" FILE`.
  2. **Load the world fresh.**
     - Keep `people`, relations, voices, factions, chapters and the economy.
     - Replace cells, terrain, links, spawn, cell claims and patrol posts. Rewrite the interior claims (section 2) and
       add the new interiors.
     - Apply the NPC plan: areas, coordinates, roles and routes. Labels are **rendered now from the loaded interior
       names**. Unplaced NPCs are set to null.
     - In the same transaction, `UPDATE live.npc_voice SET quarter` for each moved NPC (section 5, Talk).
  3. **Check.**
     - Run `check_project(project, for_game=False, playable=True)`. The default `for_game=True` caps a project at 256
       areas and 262,144 tiles, which this world exceeds, so it is not used.
     - Run **`area_graph_reachable(project, spawn)`**, a new graph walk:
       - flood-fill each cell and interior;
       - cross every door, stairs and passage link at its ends;
       - require an arrival tile at each end;
       - require every link end, every home and work spot, and every gate to be reached from the spawn.
     - `canvas.reachable` alone doesn't follow links, so it is not enough.
     - Then `save_world(connect('ridgemere2', 'editor'), project, current revision)`.
  4. **Tell editors:** `world_sync.tell_editors(...)`.

  If any check fails, the write refuses and writes nothing.
- **`talk_seed.py`** reads `artifacts/<world key>/buildings.json`.

**Must not touch:**

- Core, `Data/Items`, `Data/Talk`, migrations, the Editor;
- House text in the database, or `HOUSES` (frozen);
- `realm.py`'s output for the old map (`tools/test_realm.py` still passes);
- any NPC's relations, description or backstory. Of a voice, only `quarter`. Labels change only through the placement;
- the 194 `tn_` interiors.

**Acceptance** (in `tools/test_ridgemere2.py` and the report).

**The map:**

- 12 cells, all with weather `rain` and region `ridgemere`; every placed interior has region `ridgemere`; no cell
  overlaps another;
- water lies west of x = 168 between the headlands, and the sea wall runs along the quay;
- the wharf, its 8 piers and arches, the cranes and the slipway are **inside** the wall line;
- the Sump touches the wharf with no wall between and holds at least 50% of its frontage;
- the 5 gates and the 2 guarded pier arches are cut and reachable;
- the districts **partition** the inside of the wall, and each extent is as in section 2, within the scale rule;
- the Rise's area is at most 0.5 × the middle class's;
- the abbey close is in the north-west corner and touches both the Sump and the middle class;
- the Keep is at one height, straddles the Commons/Rise line, and has four walls, a courtyard and one gate;
- the Greatly compound's north side is the city wall, with no public tile between them; its fountain is in the south
  yard, and its forge and workshop are in the north yard;
- every gate joining heights more than 0.5 apart has its half step and `^` run;
- no tier edge or stair lies within 2 tiles of a cell seam;
- the three estates are outside the wall, each within one cell, with no field, orchard, vineyard or pen glyphs;
- `resources` is empty and every spawn table is empty.

**The Sump metrics** (section 2) are all met.

**Buildings:**

- the Records Office has three floors (z 0, 1, 2), joined by two stair pairs on distinct tiles;
- the palace's three parts touch and are joined by passages, with the Residence west and the Administrative Wing east
  of the Antechamber;
- **no structure has more than 3 homes**;
- every home has at least ⌈people / 2⌉ beds, and at least 2;
- links number 2,048 or fewer.

**People:**

- 480–580 are placed, every one with a home;
- every placed NPC aged 16–59 has work: a post whose label resolves as intended, or an office or guard post. This check
  runs with B's `businesses.json`; on A's branch alone it uses a stub of B's new ids, and the real check runs at
  integration;
- every label is at most 40 characters and breaks none of section 4's label rules;
- no family is split;
- there is exactly one merchant per till business, and every business that uses the store has a merchant keeper;
- there are 30 guards, with posts on every route in use;
- clergy have clergy words, and nobody else has them by accident;
- no placed NPC works a food or raw-resource producer; stables are listed;
- the Rag Market is more than 40 tiles from Market Square;
- Market Square has more merchants than the Market Hall, and the Hall's doors are within 40 tiles of the square's
  stalls;
- no evening place takes more than 40.

**The write:**

- A dry run on a throwaway copy uses `test_world_db`'s throwaway-world helper. If that helper doesn't fit, a script
  runs instead:
  1. `world_db.py new r2scratch --from ridgemere2`, with nothing else connected to ridgemere2;
  2. write, reload and check;
  3. `DROP DATABASE` as the `Database/.env` superuser;
  4. `git checkout Database/worlds.json`.
- The dry run writes and reloads, and leaves the count of NPC voices and relations unchanged (1,083 and 329).

### B. The store room, the toggles, and Atlas's Economy controls

**Builds:**

- **Core:**
  - section 6's API, specs, parsing (`nostore` checked at the end of the parse, unplaced ids ignored) and the
    `/store` command (dev worlds and the DM flag);
  - the restocking rule behind `storeRule` (off);
  - `fund:store` opened in `resetAuthored` and `restore`;
  - `withoutPeople` stripping `store ` and `nostore `;
  - `world_check`'s fund line and its report of residents held for more than 10 minutes.
- **`Data/Items`:**
  - the new business types, with `builders_works` before `smithy` and its crafts' `makers`;
  - the `supplies` lists for `vintner` and `rag_and_bone`;
  - the new items, crafts (including the aqua_vitae craft) and recipes, with every field section 4 lists.
- **The database and tools:**
  - migration `0055_store_room.sql`;
  - `world_store` (with `store_supported`), `live_edit`, `map_editor` (checks and exporter) and `model.mjs` for the
    economy entity, with the new keys optional;
  - `world_sync.SYNCED` gains `live.business_settings`, with "absent on either side" treated as no rows;
  - `world_build.LIVE_LAYERS` skips the table where it is missing;
  - `item_catalog.business_for`, `item_catalog.store_goods` and the shared fixture;
  - `studio.py`'s `ECONOMY_ROUTES`;
  - `EconomyControls.tsx` and the Atlas page;
  - the Inspector panel retired;
  - the DM money view lists `fund:` accounts.

**Must not touch:**

- `tools/worldgen`, `Data/Talk`, or `ratw_ridgemere2`'s rows (only `migrate --db ridgemere2`);
- any need, wage or price rule. The only new rule is the restocking rule, off by default, and `NeedsPaused` stays true;
- `SocietyState`'s fields.

**Acceptance.**

**Core tests** (`Tests/store_room_tests.cpp`, registered like `shops_tests`):

- a smithy keeper draws 3 iron_bar for `ceil(9 × 3 × markup / 100)`;
  - the till's cash falls by that amount and `fund:store` rises by it;
  - the goods arrive, and `conserved()` holds;
- a stall keeper draws a resale good at the resale markup;
- at markup 0, a draw moves goods and no money;
- refusals:
  - the world is off for that mode;
  - the business is off (`nostore`);
  - the item is not in its goods;
  - the keeper is a civilian or an unplaced NPC;
  - the business has nothing to draw;
  - the till is short of cash;
  - the quantity is 0, or more than 1,000;
  - the till is over its kinds or its stock cap;
  - the fund is at `MoneyCap`;
- every refusal leaves the till and the fund unchanged, including the forced `create` failure after `shift`;
- the flag follows the position after succession, and a missing founder spec counts as on;
- with no `store` line, the defaults hold;
- the parser:
  - rejects a duplicate or bad `store` line;
  - rejects a `nostore` for an unknown id;
  - ignores a `nostore` for an unplaced founder;
- the build and the manifest together don't produce a duplicate `store` line;
- with `storeRule` on, a keeper at work below its starting stock makes one batch, and a vendor tops up. With it off,
  nothing changes;
- a save and restore keeps `fund:store`, and an older save without it gets an empty fund;
- 66 of 66 old tests still pass.

**Python tests:**

- `test_world_store`:
  - the economy round trip with `businesses` and the five settings;
  - a missing economy row is created from `DEFAULT_ECONOMY`, not 0;
  - a world without 0055 loads and saves unchanged, and refuses a non-default setting;
- `test_dungeon_master`'s "people_manifest matches the exporter", with `store` and `nostore` lines and without them;
- `test_world_sync`:
  - business settings are pulled and imported, and `live.economy` is not;
  - a side without the table counts as no rows;
- `test_studio`: the routes' roles, Atlas's token, the audit and the bulk filters;
- the business-label fixture, in Python and Core;
- `python3 tools/item_catalog.py` passes: no losing recipe, and every new item can be produced.

**Editor:** the type-check, build and tests pass, apart from the known Greyfen model failure.

### C. The renames

**Builds:**

- the git changes in section 7;
- **`tools/rename_house.py`:**
  - `--dry-run` lists every row and field it would change in a world;
  - `--apply --world ridgemere2` makes the changes in one transaction, through `load_world` / `save_world` at the
    current revision (or in SQL that raises the revision), then calls `tell_editors`;
  - it covers the Timberland rename, the trade rewording, the Greatly Yard's description and the word fixes, all with
    whole-word matching;
  - it keeps every label within 40 characters;
  - it refuses `dev` and `prod`.

**Must not touch:**

- any id;
- anything in `tools/worldgen` beyond the two word-fix strings in `cities.py`;
- `test_realm.py`;
- Core beyond the comment.

**Acceptance:**

- `grep -rwi mossgrave` finds nothing in `Data/`, `Core/`, `Editor/`, `Client/` or `tools/`, with these exceptions:
  - the frozen generator (`tools/worldgen/` and `tools/test_realm.py`);
  - id strings (`house_mossgrave`, `rm_*mossgrave*`);
  - `mossgrave_court` in Data/Talk's widened keys.
- In `ratw_ridgemere2`, no name, label, text, voice or interior text has "Mossgrave" outside ids.
- The 15 interiors have no "dog" or "dogs", and a whole-word search over world text finds none.
- `talk_check --world ridgemere2` passes, and the `timberland_court` key lines are reached.
- `test_realm.py` passes, and DEV's talk lines keyed on `mossgrave_court` still match.
- The dry run's list is reviewed into the branch as `artifacts/ridgemere2/rename-report.txt`.

### Integration (on `ridgemere2`, in this order)

1. **Merge B, then C, then A.** A's label checks need B's catalogue. Rerun `tools/test_ridgemere2.py` after the
   merge.
2. **Migrate:** `python3 tools/world_db.py migrate --db ridgemere2` (0055).
3. **Rename:** `python3 tools/rename_house.py --apply --world ridgemere2` (C).
4. **Write the city:** `python3 tools/worldgen/ridgemere2.py write --world ridgemere2` (A). It loads fresh, so it picks
   up C's text and renders labels from it, and it backs up first.
5. **Build:** `python3 tools/world_build.py ridgemere2`. A spawn exists.
6. **Export and check:**
   - `world_build.py export DIR --world ridgemere2`.
   - Run `world_check DIR --simulate 1 STEP --players 20` (the performance gate). It must show:
     - every interior and cell loaded, about 559 residents placed and about 524 unplaced;
     - no resident without a way to its places (there were 125 with no map);
     - no errors;
     - residents held for more than 10 minutes: at most 1% of placed residents;
     - `fund:store` at 0, since no NPC draws with the rule off.
7. **Walk it on a scratch server.** Start `bash tools/scratch.sh ridgemere2` in the background and open the client:
   - walk from the spawn through Market Square, the Gut, the Rag Market, Sump Stairs and the wharf, the Keep's gate,
     the Records Office's three floors (z 2 included), the palace's three parts, the abbey's gates and the Greatly
     fountain;
   - check that Market Square offers 6 player stall places;
   - `/store` a smithy keeper and a stall keeper, and see each draw and the fund;
   - kill the server.
8. **The report's checks, all green:**
   - no food-growing business;
   - every placed NPC has a home, and a job if of working age;
   - no structure has more than 3 homes;
   - R1–R39 are each marked with the check that shows it;
   - two plain notes: the store is **not yet used by any NPC** (the restocking rule is off, Q2), and the House ids still
     read "mossgrave".
9. **In Atlas's Economy page:**
   - toggle one business, then the world's making switch;
   - a new scratch server shows the store closed for that business, then for all makers;
   - restore both to on.

## 9. Open questions for the user (each decided for now)

1. **Where the store room's money goes.** Decided: the account `fund:store`. The money stays in the world and can be
   seen with `/store` and `world_check`.
   - **Note:** the fund only takes money in. Until a rule spends it, every draw drains the tills into it for good, so
     the city's money in circulation shrinks over time.
   - When raw suppliers exist, the fund could pay them, or a DM lever could return it.
   - The alternatives are the city treasury, or a sink where money leaves the world.
2. **The making and restocking rule. This blocks the economy test.** It is built but **off**: "at work, at most once
   an hour, when a ware is below its starting stock, a maker draws one batch's inputs and makes it, and a vendor draws
   back up to its starting stock". Until you turn it on in Atlas or give another rule, no NPC uses the store. Confirm
   this rule, or tell us yours.
3. **Who is King.** The old Ridgemere had no monarch; the Council of Houses ruled. Decided:
   - The Harrowgate household (a cut Minor House) moves whole into the King's Residence.
   - Gunnar Harrowgate (62) is King, "reigning at the Great Palace", with Oda and Ottilie and their two servants.
   - The Crown is not a House, so the Harrowgates lose their `house:*` key and their old Minor House talk. King's talk is
     a follow-up.
   - The Council of Houses sits in the Antechamber.
   - The other choice is a new royal family.
4. **The sawmill and timber.** Decided: Timberland's, because of the name and because the forges' charcoal comes from
   timber. The other choice is Greatly's, as a building material.
5. **What the store holds.** Decided:
   - makers draw every input of their own crafts, intermediates too;
   - vendors draw their supply list and the kinds of wares they started with, at a 75% markup so that they keep a
     margin;
   - both are on by default and can be switched separately for the whole world.
6. **Population and the Minor Houses.** Decided:
   - 517 inside the walls and 42 at the three estates, 559 in all;
   - the remaining 524 (the six towns, the country folk, the farms and 12 Minor House households) are left unplaced
     and kept whole;
   - five mansions: Carlisle, Thornbury, Vance, Stanmore and Grantham;
   - Greatly keeps its bank and counting house alongside construction.
7. **Shops left out.** Added: a bathhouse, a wheelwright, a cheesemonger, a dice-and-card tavern, and a pawnbroker who
   also lends. Left out:
   - a **dyer**: the dyeworks has no crafts, so it would make and draw nothing;
   - a **locksmith**: there are no lock items, and the tinker covers small ironwork;
   - a **bookseller or stationer**: the scriveners and the Council Press sell paper, pamphlets and copies;
   - a **cobbler**: footwear comes from the tannery and the tailor.

   Tell us if you want any of them.
8. **Players' selling.** Market Square has the 6 player stall places, the game's cap for a square. The Market Hall has
   NPC counters only, because player stalls indoors would need Core work. Players can also sell to keepers with a
   `buys` list; the report lists which market keepers have one. Should more market trades buy from players?
9. **The administrative sector's border.** "Borders the sump and the upper class to the south" is read as the Sump to
   its north and the upper class to its east, because that fits the other borders. The other reading has the upper
   class also wrapping round its south, beside the palace. Confirm which you meant.
10. **Mossgrave's ids.** Only the display text changes. Ids such as `house_mossgrave` and `rm_mossgrave_court` stay,
    and still show in Atlas and the DM's screens. DEV and PROD keep the name Mossgrave until you rename them.

## Review notes

**Feasibility review: accepted and changed.**

- 1: `check_project(for_game=False, playable=True)`.
- 2: a new graph walk across links replaces `reachable()`.
- 3: `MiniRealm` gets its own size-bound helpers, `cells()`, `mark_cliffs()` and masks.
- 4: `RidgemereCity` subclasses `Ridgemere`, with `market(stalls=)`, `waterfront()`, new piers, `gardens(crops=False)`
  and `district(gap=)`.
- 5: the `greatly()` pattern inside the walls; `compound()` only at the estates.
- 6: half-step gates with `^` runs, the Keep at one height, and the palace at the administrative sector's height so it
  needs none.
- 7: region `ridgemere` on every cell and interior.
- 8: `withoutPeople` strips the store lines; `nostore` comes after the residents and is checked at the end of the
  parse; unplaced ids are ignored.
- 9: `store_supported` everywhere, and sync treats an absent table as no rows.
- 10: step 0 copies the analysis files.
- 11: A writes `artifacts/ridgemere2/buildings.json` with records for reused buildings, the three doc 61 works and the
  known quarter names.
- 12: coordinates are blocks, and set pieces have explicit footprints.
- 13: coverage target 55%, more flats, boarded blocks and walled yards, computed before the alleys.
- 14: benches `p`, gaol bars and openings, the solid pump against a wall, `road()` paints `d`.
- 15: R34's check names the food and raw-resource producers, stables are exempt and listed, and the charcoal and tannery
  labels change.
- 16: the label-rule table, the clergy decision, the King's label, and Minor House heads keeping their labels.
- 17: the 40-character limit.
- 18: Draws are generated from `crafts.json`.
- 19: full item fields; trades `apothecary` and `cook`; aqua_vitae reused and brandy priced against it; stations and
  works kinds completed.
- 20: Market Square gets more merchants than the Hall, and the Hall's player places are dropped (Q8).
- 21: tiers and stairs at least 2 tiles off the seams.
- 22: the districts are redrawn as a partition, and the ring stops at Greatly.
- 23: 3 new cottages; Grayrock Hall moved into one cell; the Greatly Yard re-described.
- 24: cell claims plus interior claims rewritten; `claims_for`'s location corrected.
- 25: doors on alleys at least 2 wide, and a held-residents gate.
- 26: more Sump evening places, a cap of 40, overflow home.
- 27: `/store` gated by world kind, its limit raised, the fund shown in `/store` and `world_check`.
- 28: pre-checks and a refund path.
- 29: markup 0 is free, and the deflation is noted in Q1.
- 30: a missing founder spec counts as on.
- 31: the owner conninfo, and closing connections before a restore.
- 32: the throwaway helper or a scripted create and drop.
- 33: A updates the voices' `quarter`.
- 34: the old generator is frozen, the grep excludes it, and Data/Talk keys are widened.
- 35: merge B first, with a stub on A's branch.
- 36: labels are rendered at write time from the renamed rows, with no `HOUSES` text.
- 37: the economy keys are optional, and `DEFAULT_ECONOMY` creates the missing row.
- The smaller corrections are all taken: `Tests/fixtures`, `world_check` usage, z 2, `{bid}_door_2`, `flats` closed
  doors, the barracks and watch-house facts, and the routes in use.

**Spec review: accepted and changed.**

- 1: a sea wall along the quay puts the wharf inside the walls, with no wall between the Sump and the wharf; the water
  gates become pier arches, the Harbour Gate and the Cinder Gate.
- 2: the Sump holds 52% of the frontage; the palace is inland and only the Customs House is on the wharf.
- 3: the abbey takes the north-west corner, and the Cinders moves to the south-west waterfront by the slipway. Timberland
  Court moves south to be near it.
- 4: the partition and its check.
- 5: the ring stops at Greatly, with a check that no public tile lies between.
- 6: vendors draw their supply goods at their own markup, as a second mode.
- 7: a restocking rule is built **behind a switch that is off**, Q2 is marked blocking, and the report says plainly the
  store is unused.
- 8: market keepers get trade labels, and the Rag Market is a mixed junk market; buying from players is Q8.
- 9: the 55% target, with counts computed first.
- 10: the Rise is cut to about 15,700 tiles, with a ratio check.
- 11: the Greatly forge is `builders_works` (construction ironwork and joinery), and every other forge is named as
  Timberland's.
- 12: Q9.
- 13: the brewhouse has a cellarer keeper.
- 14: no resource or forage nodes and no game spawns, checked.
- 15: one home rule, so mansions count 2 and the homes are recounted.
- 16: a bathhouse, wheelwright, cheesemonger, gambling tavern and lending pawnbroker added; the rest recorded in Q7.
- 17: the ironmongers and farriers become "Eastgate", and the avenue becomes "Greatly Way".
- 18: noted in the report and Q10.

**Rejected, or changed differently, with why.**

- **The wall-top patrol at Greatly** (spec review 5). Wall tiles are solid and no route runs on them, so the wall-walk
  goes round by street, as the feasibility review suggests.
- **The Market Hall's 8 player places** (feasibility review 20). Supporting them would need Core work and lift the
  6-a-square cap. The square's 6 meet R14, and the hall keeps NPC counters only.
- **Turning a making rule on now** (spec review 7). The user removed the NPC rules in doc 66. A rule is built so the
  economy test can start at once, but it stays off until the user confirms it.
- **Renaming Mossgrave in DEV and PROD, or in the old generator** (feasibility review 34). They are other worlds, and
  renaming `HOUSES` changes ids and breaks `test_realm`. The rename is this world's, and the tool refuses DEV and PROD.
- **A dyer** (spec review 16). It has no crafts and is one of doc 65's dead trades. It is listed in Q7 instead of
  built.
- **Shrinking the Sump** (feasibility review 13, first option). The spec wants it to front the wharf and be large and
  dense, so the plan adds buildings and infill instead.

## Progress

### Lane A: built and written (2026-10-31)

Branch `r2-city-map`, from `ridgemere2` (94e025d). Step 0 was not on `ridgemere2` yet: the analysts' files and the old
map's placements were committed on this branch (`artifacts/ridgemere2/analysis/`, `old-map-placements.json`).

**Built.** `tools/worldgen/ridgemere2.py` (`MiniRealm`, `RidgemereCity`, the checks, the report, the write; `preview`,
`place`, `write`), `tools/worldgen/ridgemere2_people.py` (who is placed, homes, jobs, evenings, the label rules),
the kit pieces in `buildings.py` and `site.py` (old recipes unchanged), `talk_seed` reading
`artifacts/<world key>/buildings.json`, and `tools/test_ridgemere2.py`. The seed is fixed; a run takes about 30 s.

**Written into `ratw_ridgemere2`** (revision 28 to 30; backups `Saved/Backups/ratw_ridgemere2-before-city.dump` and
`artifacts/backups/ratw_ridgemere2_20261031-011822.pgdump`): 12 cells, 845 interiors (219 new `r2_`), 555 links
(340 doors, 187 stairs, 28 passages), 301 structures, 559 NPCs placed and 524 unplaced (262 towns, 200 country, 62 of
the Minor Houses not kept), 1,083 voices and 329 relations kept, 239 voices given their new quarter, 51 patrol posts on
6 routes. All 46 acceptance checks pass (`artifacts/ridgemere2/city-report.txt`). `world_build.py ridgemere2` builds;
`world_check` loads all 857 places with no failures and simulates 6:00-8:00 with 20 players (p99 5.5 ms, no resident
without a route); a scratch server starts on it (559 residents) and serves the client.

**Rerunnable.** The first write keeps the NPCs as they were before the city in
`artifacts/ridgemere2/people-before-city.json`; every later run plans from it (so the plan is the same) and strips the
city it wrote before (its cells, `r2_` interiors and links) and makes it again with the same ids. Kept labels are taken
from the world as it is then, so lane C's renames are kept. A second write of the same world changed no NPC's place.

**Differs from the plan, or found on the way.**

- Homes: 225 homes in use (most 2 in one structure; the Sump offers 151, of which 77 are used); 10 people live alone.
- The Knucklebones and the Fell Cellars are new interiors, not reused ones (the reused rooms would have kept the
  names "The Bridge Lamp" and "The Customs House"); Forge Row's smithy is new ("Timberland's Forge Row").
- The old Hall of Records' staff move to the Records Office; the old building is the Couriers' Office.
- Sump passages also join buildings across a snicket or an alley (cellar runs): wall-to-wall neighbours alone gave too
  few for 25 passages in six chains of five (24 at most on the ground floors), so floors 1 and 2 get passages too.
- Mansion servants' floors with one bed got a second (straw bedding), for the two-bed rule.
- The dead-end check counts the alley stubs laid that still end blind (51), not every corner.
- A's branch resolves labels with a stub of B's new business types (`STUB_BUSINESSES`); with B's
  `businesses.json` merged the real ones are read instead.
