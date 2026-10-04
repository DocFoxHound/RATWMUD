# 39. Shops and settlements: shops with homes above, the basics, city markets

Planned and built 2026-10-04. Every settlement is rebuilt from scratch around its shops: Upper Accord's city, Ridgemere,
Ser Ferro, the seven towns, Hollowmere, the three fortresses and the Ghost Town. Everyone the generators made is new
(their old memories, bonds and rumours go, as the user chose); anyone else is kept.

## Decisions (2026-10-04)

1. **Rebuild from scratch**, every settlement, the cities included. Upper Accord's city is rebuilt without touching its
   ground: its generator makes exactly the ground DEV holds (checked tile by tile), so only its buildings and people
   change.
2. **Every shop has a flat upstairs** where its keeper's family lives.
3. **Towns have the basics:** a general store, a bakery or provisioner, a smithy, and an inn or tavern. Towns have **no
   market square and no market day**: their shops are permanent buildings. The fortresses keep their military makeup.
4. **Cities have a square of market stalls**, built on the ground, each kept by a stallholder selling food every day.
   Shopkeepers may still set up at the stalls on Marketday.
5. **Shops sell their kind's cheap goods**, a handful each, not every shop of a kind the same.
6. **Terrain:** only Upper Accord's three northern cells (Northwest Heights, Northern Ridge, Northeast Heights) are
   blended into the moors north of them. The rest of Upper Accord's ground is left alone; the one-tile cliff line on
   its southern edge stays.

## Shops

- **The building** (`tools/worldgen/buildings.py`, `shop_with_flat`): the shop below, as before (goods along the back,
  the counter, the keeper's place behind it, a customer floor and a street door), and the flat above (`<shop>_flat`,
  z 1): a hearth, a table, beds for two to four and a chest, no street door. Stairs join them on the customer floor,
  away from the door and the counter. The flat comes from its own random sequence (seeded by the shop's name), so a
  settlement's layout doesn't shift for having flats.
- **The household:** the keeper (a merchant, working the counter), most often a partner who helps in the shop, then
  children or an elder in the beds left. They are housed before anyone else, so every shop is kept.
- **The keeper's label names the trade:** "baker at The Amber Loaf", "shopkeeper at Saltreach Sundries"
  (`keeper_label`). The game server knows the kind of shop by it.
- **What it sells** (`Society::wares`, `Core/RatwSociety.cpp`): the shop's kind comes from its keeper's label and
  `Data/Items/businesses.json` (its `match` words; the first match in file order wins). It sells three to six of that
  kind's goods costing at most `Society::CheapPrice` (6 pennies), chosen by the shop and the same every day; where a
  kind has fewer than three that cheap, its three cheapest. A smith adds swords; a shop of food (general, provisioner,
  stall, bakery, butcher, fishmonger, brewery) adds meals, the town's daily food; an herbalist, apothecary, general
  store or stall adds herbs. Anyone else (an innkeeper, the demo's keeper) deals in meals and herbs as ever.
- **Stock:** each good starts on the shelves (`GoodsKept`, 4 of each) and the keeper makes one more of whatever runs
  low while working, as the smith forges swords: a placeholder until crafting takes materials (doc 35, Phase 5). Meals
  and herbs still come wholesale from the town's stores. Prices are the catalog's, with the usual scarcity and town
  factors.
- **Businesses with match words:** 26 kinds of shop, plus `stall` (food) added here; masons and glassworks match
  their keepers too. A moneychanger has no goods in the catalog and deals in meals and herbs.

## Towns

- **The basics** were added where missing: Saltreach (general store, smithy), Cinderbrook (general store), Westmarch
  (bakery), Lakeside (smithy), Fenhollow (bakery, smithy), Hollowmere (bakery, smithy); each town's population rose by
  the new shops' families.
- **No market square:** the middle of a town is a small crossroads with its well, not a square of stalls, and the
  server gives a place without built stalls no market day (below).
- **Fewer houses:** shop families live over their shops, so a town builds one house fewer for every three shops.

## Cities: the market squares

- **Ridgemere** (the Market Square) and **Ser Ferro** (the Mercato): two facing rows of four two-tile stalls each
  (`City.market_stalls`), with the aisle between; eight stallholders each.
- **Upper Accord**: its plaza already had four rows of stalls (its ground is unchanged); every other stall has a
  stallholder, sixteen in all (`city.STALL_PLACES`).
- **Stallholders:** merchants labelled by their food ("bread stall on the Market Square"), behind their stall every
  day from six to four, living in the city's houses and tenements; they sell food and herbs (the `stall` business).

## The game server: market days

- **A market's stalls are the ones built on the ground** (`World::square`, `Core/RatwSchedules.cpp`): the open places
  beside each stall tile (`u`) within 40 tiles of the market's middle, not where a stallholder stands. No stalls
  built, no market: Marketday is an ordinary working day (`dayPlan`). So towns have no market day and cities do.
- **Shopkeepers no longer keep a daily stall** in cities (doc 36's third of them): the stallholders do that now. On
  Marketday mornings, as before, shopkeepers set up at the free stall places.

## Terrain: Upper Accord's northern edge

`tools/worldgen/blend.py`: along the top edge of the three northern cells, tiles take the ground of the moors just
north of them, less and less over 40 tiles; north of the edge, over 16 tiles, a little of Upper Accord's ground mixes
in. Only natural ground changes (no water, ice or cliffs), and only for ground of the same kind (open for open, a tree
for a tree), so nothing becomes walkable or blocked; heights, roads and buildings are untouched. About 7,500 tiles.

## Running it

`cd tools && python3 -m worldgen.rebuild [--dry-run] [--out FILE]` (`tools/worldgen/rebuild.py`): DEV is backed up to
`artifacts/backups/`, then Upper Accord's city and people, the western world with its cities and towns (and the
cities' newcomers), and the blend, validated and saved with DEV's revision checked.

**Checked before the import (2026-10-04):** the rebuilt world validated by Atlas's checks; built by the game's exporter
in a scratch database; `world_check` loaded all 854 places with no failures; `--simulate 7 7.2 --players 20`:
1,346 residents, mean tick 14.8 ms, p99 23.8 ms after the first minute (DEV before: 1,063 residents, p99 29.2 ms over
the whole run), within the 50 ms budget. 1,325 residents, 236 of them merchants.

**What found no room** (the generators report it): Saltreach's salt pans, Westmarch's harness-maker (The Strap and
Buckle), a Lakeside tenement, two Ser Ferro family houses; and, as before this, The Muddy Oar, Tar and Tallow and the
Mirrormere Smokehouse.

## Open questions

1. **Shops for the fortresses:** they have armouries and forges; should a garrison have a sutler (a provisioner)?
2. **Goods beyond cheap:** when do shops carry dearer goods (doc 35, Phase 2's quality and weight)?
3. **Industry left out:** Saltreach's pans and the harness-maker found no room inside or outside their walls this
   time; give them room (a larger town box, or a fixed site outside), or leave them out?
