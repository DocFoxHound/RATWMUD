# 60. The new map: Ridgemere and its country

Asked for 2026-10-11; decisions below are the user's. Built and imported into DEV the same day (revision 22; see
"Built"). The world is replaced entirely: one city, Ridgemere, twice its old size, with the great Houses' estates
outside its walls, six towns out in the country, livestock farms near the city and grain farms and orchards further
south. The old world was an excellent testbed and is kept (see "The old world").

```
cd tools
python3 -m worldgen.realm --land --preview DIR      # the land alone (Ridgemere as a plain walled box), ~1 minute
python3 -m worldgen.realm --preview DIR --out DIR   # the whole world, city, towns, sites and people, ~3 minutes
python3 -m worldgen.realm --import-dev              # replace DEV's world with it (DEV backed up first)
```

## The ask

> "We are going to change the map entirely. I want you to save the current world that we have since it was an
> excellent testbed, but we're going to start over with a completely different map. [...] we are going to have only a
> single city (Ridgemere), several outlying estates, and further afield in the map several smaller towns. Ridgemere
> will be a large Seattle-like city in the northwestern part of the map. The northeastern part of the map will be
> mountainous and snowy. The southwestern part of the map will be warmer forests and even some swamp, and the
> southeastern part will be rolling hills and open fields. [...] I want to make Ridgemere's total area to increase by
> twofold. [...] Ridgemere should have very distinct City Office area, slums, three food markets (one near the upper
> class area, one near the middle class, and one near the slums, each of which matching the area). Ridgemere should
> have a large wall surrounding the majority of the city, but also more houses on the outside of the wall, and at
> least two gates. The western side of Ridgemere should follow the coast, and should have docks and a wharf. The Great
> Houses (Fell, Grayrock) should have estates outside the walls and their grounds should be beautiful. The Great House
> Greatly (Greatly is the family name) should be inside the walls. Make a third Great House outside the walls with Fell
> and Grayrock and give it another name. There should be an industry sector within Ridgemere, too, closer to the
> Wharf. For now, the outlying towns should be generic, but there should be farms further to the south and orchards,
> too. Farms closer to Ridgemere should be livestock (sheep, rabbits)."

## Decisions (2026-10-11)

1. **Only Ridgemere.** Upper Accord (and Concord Hall, the Warden Order, the Training Grounds), Ser Ferro, every old
   town, Hollowmere, the fortresses and the Ghost Town are dropped entirely. Text that names them is rewritten.
2. **Size: 8 × 8 cells of 256 tiles** (2048 × 2048, about 64% of the old world's area). (The user wrote "8x8 cells of
   2560"; read as cells of 256 tiles, the size every cell has.)
3. **Six towns**, generic for now.
4. **The old world is kept as backup files only** (not a runnable third database).
5. The third House outside the walls is **House Mossgrave** (confirmed by the user) (wool: the sheep runs and warrens of
   the Folds, and the looms in the city's works).
6. *(new, to confirm)* The new world keeps DEV's world ID (`upper_accord`, an internal key the database, saves and
   game tables hang on) and is named "Ridgemere".

## The old world

Saved 2026-10-11 before anything changed, in `artifacts/backups/old_world_20261011/`:
- `ratw_dev_full_20261011-090552.pgdump`: the whole DEV database (world, live NPC data, game state), revision 17.
  Restore into a scratch database with `pg_restore` to run it again.
- `upper_accord_dev_r17_old_world.atlas.json`: the world as Atlas holds it (105 cells, 864 interiors, 1,350 residents).
- PROD held the old world as release 1. Its full dump is `ratw_prod_full_20261011-120059.pgdump` beside DEV's; its old
  residents, factions, claims, patrols and save were cleared before the new world went live as release 2.
- The generators that made it are in git (last at `25e404b`).

## The map

```
        0         256        512        768       1024       1280       1536       1792      2048
   0  ┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
      │  Sound   │ RIDGE-   │ Fellhall │ Rainward │ Rainward │   The Whitefangs (snow,       │
 256  │          │  MERE    │ Grayrock │  Hills   │  Hills   │   peaks)      Frosthollow     │
      │  wharf ▸ │ (walls)  │ Houses'  │          │ Coldwater│ Glasswater                    │
 512  │          │ suburbs  │ Lands    │ Greenholt│  ~lake~  │  The Greyteeth    Eastern     │
      │  coast   │ The Folds: sheep &  │          │          │                   Snows       │
 768  │          │ rabbits  Mossgrave  │          │          │                               │
      │          │                     │  Long River  ↘      │ The Larkdowns (rolling hills, │
1024  │  The Warmwood (warm broadleaf  │          │          │  open fields)  Larkfield      │
      │  forest)       Oakhaven        │ Rivermeet│ ← the Lark ──────────    High Downs    │
1280  │                                │          │          │  The Orchard Vale             │
1536  │  The Sallow Fens   Mirebridge  │   The Sunfields (grain, farms)   Southmeadow        │
1792  │  The Drowned Delta             │                     │  The Southern Meads           │
2048  └──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
```

`tools/worldgen/realm.py` lays it out from the plan (no drawing), reusing the western world's methods:

- **The sea and the Sound.** The coast runs down the west edge; Ridgemere Sound is the bay below the north headland,
  and along the city's waterfront the shore keeps nearly straight so the wharf can run along it.
- **Water.** **Glasswater**, a cold lake under the mountains, where **the Long River** rises and winds south-west to
  its delta in the fens; **the Rainrun** comes down from the Rainward Hills into the Sound just south of the city;
  **the Lark** runs out of the downs into the Long River.
- **Biomes**, fading into each other over a couple of hundred tiles: the rain coast (cedar rainforest, ferns, moss)
  round Ridgemere and its Houses' lands; snow in the north-east; temperate woods and meadows in the middle (the
  Greenholt); **warm broadleaf forest** (new: oak, beech, hazel, fern brakes, glades) in the south-west, with **fens**
  and the **delta**; **downs** (new: rolling grass, chalk, copses, hay meadows, gorse) in the south-east; and **the
  grain country** and orchards along the south.
- **Heights.** The Whitefangs (peaks to 16) and the Greyteeth in the north-east, with a valley for Frosthollow and a
  pass up to it; the Rainward Hills behind Ridgemere; the downs roll in long swells.
- **Roads:** the Southway (Ridgemere's South Gate to Oakhaven), the Fen Road (to Mirebridge), the High Road (the East
  Gate to Coldwater), the Pass Road (up to Frosthollow), the Downs Road (Coldwater to Larkfield), the Riverway
  (Oakhaven to Larkfield), the South Road (to Southmeadow), the Meadow Road (Mirebridge to Southmeadow across the
  grain country), the Eastmarch Road and the Mine Track; the Houses' drives to the city gates.
- **Points of interest** by biome as before, and cells named for their region: Ridgemere Sound, Ridgemere, the
  Rainwood, the Houses' Lands, the Rainward Hills, the Folds, the Whitefangs, the Frost Valley, Glasswater, the
  Greyteeth, the Eastern Snows, the Greenholt, the Warmwood, the Rivermeet, the Sallow Fens, the Drowned Delta, the
  Larkdowns, the High Downs, the Sunfields, the Orchard Vale, the Southern Meads, the Fenshore.

## Ridgemere

`tools/worldgen/cities.py`. A Seattle-like rain city on terraced hills above the Sound, about twice its old size
(walls round some 80,000 tiles, against 20,000; with the wharf and the suburbs about 120,000).

- **Tiers.** The city climbs from the water toward the north-east in steps, never more than one apart, retaining walls
  along each edge and stairs wherever a street crosses (Ser Ferro's way): **the wharf** at the water (outside the
  wall); **the works and the Sump** on the first tier, by the water; **the City Office quarter, Harbour Hill and the
  Commons** on the second; **the Heights** at the top.
- **The wall** encloses the city (not the wharf or the suburbs), with towers round it, **three gates** to the country
  (North, East, South) and **three water gates** onto the wharf.
- **The wharf and the docks**, between the west wall and the Sound: a street along the wall and one along the water,
  piers out over the Sound. The Customs House, the Harbourmaster's Office, the Fish Hall, two smokehouses, the four
  Houses' warehouses, the Tarred Rope and the Sailors' Rest, a chandlery and a fishmonger, sailors' lodgings; at the
  south end the docks: **the Greatly Yard** (shipwrights) and **the Ropewalk**.
- **The works** (the industry sector), inside the wall by the docks: the Grayrock Ironworks and Foundry, the Fell
  Sawmill, the Mossgrave Wool Shed, the Blue Vats, the Wharf Tannery, the Cinder Glassworks, the Tileworks, the
  Rainwater Brewery, the Rag Mill, the Wagon Yard, the Lime Kilns, the Charcoal Store, the Salt Pans and the City
  Arsenal; an armourer, a smith and a harness shop; the Slag & Anchor; the workers' lodgings.
- **The Sump** (the slums), above the wharf's middle: some forty tenements on narrow lanes, the Soup Kitchen, the Sump
  Infirmary, the Drowned Lantern and the Bilge, Sump Sundries, the Pawn and Ledger, a fishmonger and a provisioner, and
  **the Sump Market** (fish, soup, porridge, bread, sausage, pickles, eggs, nuts).
- **The City Office quarter**, in the middle: **the City Office** (the clerks' long counters: tolls, rents, fines,
  permits and the rolls of every household; the Chancellor's door at the back), the Rain Hall (the Council of
  Houses), the Magistrate's Court, the Watch House, the Gaol, the Hall of Records, the Chapel of the Tide, the Slate &
  Ledger, the scriveners, the Council Press, a cartographer, a chandler and an apothecary, round **Office Square**
  (a fountain and statues). Harbour Hill beside it is houses.
- **The Commons** (the middle town), to the south: family houses, everyday shops (bakers, butchers, smiths, coopers,
  tailors, weavers, a potter, carpenters, a brewer, a herbalist, a tinker, provisioners, a fletcher, general stores),
  the Grey Gull Inn by the South Gate, the Black Tun, the House of Mending, the Commons School, and **the Commons
  Market** (bread, sausage, cheese, eggs, fruit, pies, pickles, nuts).
- **The Heights** (the upper class), at the top: town houses with gardens (eighteen families), fine shops (a
  jeweller, a perfumer, a tailor, a glassblower, the Greatly Bank, a physician's apothecary, a pastry baker), the
  Rainbarrel, the Heights Watch Post, and **the Heights Market** (cheese, honey, fruit, pies, nuts, eggs, fine bread,
  pickles). **Greatly House** stands here inside its own walled garden.
- **Outside the walls:** houses along the roads beyond the gates: **Rainrun Bridge** (south, both banks of the
  Rainrun, with the Bridge Lamp, a farrier and a provisioner), **Eastgate Without** (the Last Lamp, the Eastgate
  Stables, a harness-maker) and **Cedar Row** (north, a joiner).
- **The great Houses** (the Council of Houses):

  | House | Trade | Seat |
  |---|---|---|
  | Greatly | ships, the wharf, the bank (the richest) | Greatly House, inside the walls at the top of the Heights |
  | Fell | timber: the Rainwood's logging camps, the sawmill | Fellhall, north-east of the city |
  | Grayrock | stone and iron: the quarry and the mine in the Rainward Hills, the ironworks | Grayrock Hall, east |
  | Mossgrave | wool: the sheep runs and warrens of the Folds, the wool shed | Mossgrave Court, south-east |

  **The estates' grounds** are laid out on the axis from the gate to the house: a drive between trees; the kitchen
  garden and orchard by the gate on one side; a parterre of clipped hedges round flowerbeds, statues at its corners,
  halfway up; a gravel forecourt with a fountain before the house; a pond with a temple on its island on the other
  side; lawns and specimen trees about the house; a belt of woodland inside the wall with a walk through it (cedar
  for Fell). Each has its manor (great hall, the family's chambers above, kitchens and servants' hall below),
  stables, a gatehouse, estate cottages and a summerhouse; Grayrock its Quarry Office, Fell its Timber Office,
  Mossgrave its Fleece Barn. Greatly House has the same in little, with the Greatly Counting House.

**People** (`tools/worldgen/citizens.py`): the four Houses' families (Lord Ambrose Greatly, Lady Celestine, Julian and
Rosalind; Lady Isolde Fell, Corwin and Gudrun; Lord Aldric Grayrock, *always travelling* (now the Grayrock Drive, the
High Road and the Pass Road to the mines above Frosthollow, and back), Lady Maren, *always at home*, Tobias and Ingrid;
Lord Halvard Mossgrave, Lady Signe, Wenna and Osric), each seat's steward and servants, the estates' gardeners, grooms
and gatekeepers; the Heights' households and their servants; the Chancellor, the clerks of the City Office, the
Speaker of the Council, the magistrate, bailiff and gaoler, the records clerks, the Customs officer, the harbourmaster,
the chaplain, healers and the schoolteacher; the Watch (its captain, patrols of the wall, the wharf and the streets,
day and night, and the gate guards); shopkeepers and their families over their shops; eight stallholders on each
market; the masters and hands of the works; the wharf's dockhands, fishers and net-menders; the Sump's washers,
rag-pickers, beggars, sweepers and children. Then 150 more townsfolk fill free beds (`towns.fill_city`).

**Factions:** the Council of Houses, the Ridgemere Watch, Houses Greatly, Fell, Grayrock and Mossgrave, with claims on
their buildings and cells.

## The towns

`tools/worldgen/towns.py` (`TOWNS`), as doc 39 built towns: walled, a crossroads with its well, no market square;
every one has the basics (an inn, a general store, a bakery or provisioner, a smithy), a watch post, its houses and a
tenement, fields outside the gate, and a trade or two that suits its country:

| Town | Where | Besides the basics |
|---|---|---|
| Frosthollow | a high snowy valley in the Whitefangs | the Snow Chapel; a smelter; miners' bunkhouse; trappers and woodcutters |
| Coldwater | Glasswater's shore, where the High Road starts to climb | a fishmonger; stables; lake fishers |
| Oakhaven | the Warmwood | joiners; a sawmill; woodcutters and foragers |
| Mirebridge | the Sallow Fens, by the Long River | a herbalist; eel-trappers and reedcutters |
| Larkfield | the Larkdowns | a butcher; a water mill; farmers and shepherds |
| Southmeadow | the far south, among fields and orchards | the Cider House and the Southmeadow Press; farmers and orchard hands |

## The country's farms and works

`tools/worldgen/country.py`: each site its yard and building, placed on ground that suits it, a bunkhouse beside it
(`<site> Bunkhouse`, which the game finds for hired hands), and new people: a farm's family in its farmhouse, a works'
keeper, and its hands in the bunkhouse. They belong to the settlement the site serves.

- **Near Ridgemere, livestock** (the Folds): four sheep farms, three rabbit farms, a dairy, a piggery and hives;
  a fishery on the Sound, two logging camps in the Rainwood, a quarry and a mine in the Rainward Hills.
- **Further south, grain and orchards:** Larkfield's three farms, two orchards, a dairy, a sheep farm and a water mill;
  Southmeadow's four farms, three orchards, a vineyard, hives and a rabbit farm (the Orchard Vale and the Sunfields).
- **The towns' own:** two mines and a logging camp above Frosthollow; Coldwater's lake fishery, water mill, farm and
  fold; Oakhaven's two logging camps, two farms, hives, piggery and orchard; Mirebridge's fishery, clay pits, farm and
  fold.

## What changes in the game

- **Scenes** (`Data/Voice/scenes`): the region whitelist in `Core/RatwScenes.cpp` is now the new towns and `wild`;
  the groups, the town scenes and every scene that named an old place are rewritten or retired.
- **Weather** (`Core/RatwWeather.cpp`, `climateOf`): climate words for the new regions. Fixed on the way: "mere" was a
  marsh word, so Ridgemere came out marsh, not wet.
- **Ambient talk** (`Core/RatwAmbient.cpp`): Upper Accord's sub-regions and Hollowmere's special name are gone.
- **Newcomers** (`Data/Social/newcomers.json`): start in Ridgemere, Larkfield or Oakhaven.
- **Sermons, lore fragments, item descriptions, the chronicle's vouch lines, training regions**: rewritten for the
  new world.
- **`Data/Items/regions.json`**: worked out again from the new world (`tools/supply_balance.py --regions`).
- **Untouched and dormant:** the fortress wage rule (`RatwWages.cpp`, no fortresses now) and the Warden Order's tell
  and vouch (`RatwGame.cpp`, which want a `warden_order` resident; there is none). Quickened magic still draws
  "Warden attention": the Wardens are an order off this map for now (the user's decision). Upper Accord may
  come back later; its generators are in git before the new map (25e404b) and its world in the backups.
- **Retired generators:** `western.py`, `upper_accord.py`, `city.py`, `campuses.py`, `rebuild.py`, `blend.py` and the
  old passes' settings, and their tests (`test_western.py`, `test_cities.py`): they made the old world, which is in
  git and the backups.

## Built (2026-10-11)

**In DEV, revision 22** (each revision backed up under `artifacts/backups/` before the next): 63 cells, 623 interiors,
1,075 residents (149 merchants, 27 guards), 10 routes, 6 factions. Ridgemere 324 buildings and 670 residents (with the
estates, the suburbs and its farms); the towns 37–47 each; the country 51 sites and 200 people. The old residents'
saved state (bodies, bonds, beliefs, DM state) was cleared; players keep their characters, and the server lets go of
places the world no longer has. `Data/Items/regions.json` worked out again: Ridgemere's specialties are stone, mutton,
sheepskins, rabbits and pigs; Larkfield's and Southmeadow's wheat, apples, grapes and honey; Frosthollow's ore.

**Checked:**
- Atlas's validation; the game's build (686 places, all loaded by `world_check`, no failures).
- Every one of the 520 street doors can be walked to from the spawn by the game's step rules, and, as residents
  route (cell by cell), no one is left without a route.
- `world_check --simulate 7 7.2 --players 20`: mean tick 11.0 ms, p99 18.8 ms after the first minute (budget 50 ms;
  the old world: 12.4 and 23.5); a player's view 0.7 ms; money conserved.
- `tools/supply_balance.py`: every good the world uses has a source (the Salt Pans' rakers and the Charcoal Store's
  burners make salt and charcoal; tin ore has none, and nothing uses it).
- Tests: all 67 C++ tests; `test_realm` (the plan, and with `RATW_SLOW=1` the whole world against the ask),
  `test_towns`; the browser tests that read the changed data (newcomers, archive, letters, tavern, board, stall,
  tables, fame, festival, projects, storyteller). The test strips' three towns are named Ridgemere, Larkfield and
  Oakhaven now; one newcomer test holds its evening clear (Ridgemere's rain hid the far end of the inn).
  `test_map_editor`'s Greyfen resident count (11, not 10) fails as it did before this work.

**Found and fixed on the way:**
- The water gates were a whole step above the wharf, and an archway is no ramp: the wharf was cut off from the city
  inside its cell, and eight residents could find no route. The gateways now stand half a step above the wharf.
- A wall tower stood on the East Gate and walled it up; towers now keep clear of every gate.
- **A saved player in a place the world no longer has** made the server refuse DEV's whole save ("Invalid saved
  player"). Now such a player wakes at the spawn (`World::restore`); a player placed wrongly in a place that *is* there
  is still refused, as before (`Tests/world_tests.cpp`, `restoreOverARebuiltWorld`).
- **Residents' IDs.** A new resident never takes the ID of someone the game's save remembers, unless the save still has
  that resident's body, in the same home (`realm.saved_residents`): the Fell and Grayrock families, the same people as
  before, start their lives afresh under new IDs like everyone else. A DEV server now starts on the new world from
  DEV's save (one player character, moved to the spawn), ready in 1.9 s.

**The scenes** (`Data/Voice/scenes`): 2,302 scenes (2,517 before), 714 tied to a region. Upper Accord's, Ser
Ferro's, the fortresses' and the Ghost Town's are gone; the other old towns' were retold for the new town most like
them (Lakeside → Coldwater, Fenhollow → Mirebridge, Cinderbrook → Frosthollow, Hollowmere → Oakhaven, Accord
Crossing and Westmarch → Larkfield, Amberford → Southmeadow, Saltreach → Ridgemere's wharf); Ridgemere's own were
rewritten for the new city; new scenes for its quarters and each town. Sermons: the Tide stays (Ridgemere); the Iron
Saint's and the First Oath's are gone. Lore: Ridgemere's fragments rewritten, a few for the towns. Training: the
Watch House and watch posts.

## Open questions

1. **The towns' character**: they are generic for now; each could get its own trade, people and look later.
2. **Ridgemere's three markets**: the game has one market (and one market day) per town; the stallholders of all three
   squares sell every day, but only one square (the busiest) gets Marketday's crowd. Should every square have its day?
