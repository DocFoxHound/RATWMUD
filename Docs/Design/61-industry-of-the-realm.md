# 61. Industry of the realm: the Houses' businesses, the towns' trades, the flow of goods

The user, 2026-10-12: get the economy right. The outlying towns need specialties, each with its own outputs and the
inputs it needs; Ridgemere is an industrial powerhouse, and each of its great Houses has a specific business. House
Greatly's is building materials: walls, paint, plasterworks and the like. Develop industry between the towns and the
city, don't neglect the farms, and test the economy.

Builds on doc 35 (items, crafting), 42 (money in circulation, the Houses), 46 (the orchestrator) and 60 (the new map).

## What there was (2026-10-12)

- **The Houses owned businesses by lot.** At the founding every works and yard, and about half the shops, went to a
  House by a hash of its keeper: the Fell Sawmill was as likely Grayrock's as Fell's. The generator's claims (Fell's
  sawmill, Grayrock's ironworks) weren't read for it.
- **The wrong Houses.** A House is found by its head's work label. Mossgrave's ("keeping Mossgrave Court") didn't
  count; the Rain Hall's caretaker ("keeping the Rain Hall") did, as a fourth great House.
- **Businesses doing nothing.** A business is known by a word in its keeper's work label; several had none the game
  knew: the Southmeadow Press, the Ropewalk, the Charcoal Store, the Fish Hall, the Greatly Yard.
- **No specialties.** What each town makes came only from which farms and sites the generator put near it; Ridgemere
  had its own quarries, mines, sheep runs and logging camps, so it needed little from the towns, and the roads carried
  mostly food. Building materials stopped at bricks, tiles, lime and plaster: no paint, whitewash, mortar or dressed
  stone, though the Town Works mend the towns with them.
- Measured: see "Testing" below (the 28-day baseline).

## The Houses' businesses (Data/Economy/houses.json)

Each House has a trade; it owns the businesses of that trade in its city, and any business named for it. The rest
are their keepers' own. (Rule, in order: a business whose workplace is named for a House is that House's; else one
whose kind is a House's trade is that House's; else its keeper's.)

| House | Trade | Its businesses in Ridgemere |
|---|---|---|
| **Greatly** | Building materials: walls, roofs, plaster and paint, and the ships that carry them | The Greatly Yard (the builders' merchant: everything a builder needs), the Tileworks (bricks, roof tiles), the Lime Kilns, the Greatly Plasterworks (plaster, whitewash), the Greatly Colour Works (paint), the Greatly Masons (dressed stone, mortar), the Cinder Glassworks (window glass); and the Greatly Bank, its counting house |
| **Fell** | Timber and woodwork | The Fell Sawmill (planks, beams), the Charcoal Store, the Wagon Yard, the coopers, the joiners |
| **Grayrock** | Stone and iron | The Grayrock Ironworks, the Grayrock Foundry, the forges and smiths, the armourer |
| **Mossgrave** | Wool, cloth and leather | The Mossgrave Wool Shed, the Blue Vats, the Wharf Tannery, the Ropewalk, the weavers, tailors and saddlers |

Bakers, butchers, brewers, fishmongers, apothecaries, the papermill and printers, the Salt Pans and the City Arsenal
stay their keepers' own (or the Council's). The Rain Hall is the Council's again, not a House.

## Building materials

New goods and crafts, so that a wall, a roof, a coat of plaster and paint can all be made in the city:

- **whitewash** (lime → whitewash; the Plasterworks), **paint** (linseed oil and a dye → paint; the Colour Works),
  **mortar** (lime and sand → mortar; the masons, the Plasterworks), **cut stone** (stone → dressed stone; the
  masons), **beams** (timber → beams; the sawmill), **window glass** (sand and wood ash → glass panes; the glassworks),
  **linseed oil** (flax → linseed oil; the mill).
- **The builders' yard** (the Greatly Yard): sells them all, bought from their makers.
- **Who uses them:** the Town Works mend every town with them (stone, cut stone, mortar, bricks, tiles, lime,
  whitewash, paint, planks, beams, nails, glass), more as a town's condition falls; the farms' upkeep (planks, nails,
  whitewash). So the towns' works are a market for the city's builders, carried out by caravan.

## The towns' trades (Data/Economy/specialties.json)

| Town | Its trade (what it sends out) | What it needs from elsewhere |
|---|---|---|
| **Frosthollow** (the mountains) | Iron: ore from its mines, iron bars from its smelter; charcoal, furs | Grain and flour, cloth, salt |
| **Coldwater** (the lake) | Fish, fresh and smoked; flour from its mill; horses | Iron goods, salt, cloth |
| **Oakhaven** (the warm forest) | Timber, planks and charcoal; joinery; pork; herbs | Iron goods, cloth, salt |
| **Mirebridge** (the fens) | Clay, bricks and tiles from the Clayholt Brickworks; reeds; eels | Grain, timber, iron goods |
| **Larkfield** (the downs) | Grain and flour, cheese and butter, wool, mutton, hides | Tools, salt, building materials |
| **Southmeadow** (the south) | Cider and wine, apples, honey and beeswax, grain | Iron goods, barrels, cloth |
| **Ridgemere** (the city) | Building materials, iron and steel goods, cloth and leather, glass, paper, salt, rope | Iron ore, timber, clay, grain, wool, food |

What a trade does in the game:

- **Its makers and land work harder at it**: a town's producers of its trade's goods yield more (`yield`, 1.5 by
  default), and its makers of them keep more in store, so it has the surplus to send.
- **It sells first**: when the roads look for a town with spare of a good (standing orders, doc 42 Phase 7), a town
  whose trade it is comes first; the orchestrator's export channel ("sent away in trade") buys its trade goods first.
- **It is told**: residents and the DM see the town's trade (the DM's Money tab, regions.json).

## The farms

The farms feed the towns' mills, bakers, butchers and dairies, and through them, by caravan, the city. Their own
needs are a market for the towns: tools and farm gear (the smiths), nails, planks and whitewash for their upkeep
(Fell's and Greatly's, through the towns' general stores), and hands (doc 42's farmhands). Larkfield and
Southmeadow, with most of the farms, are the granaries; the orchards and the vineyards make Southmeadow's cider and
wine.

## Phases

1. **Every business real; the Houses' businesses.** Match words for the businesses doing nothing; Mossgrave a House,
   the Rain Hall not; ownership by `houses.json`, the existing ownership in a save moved to it once (a House no longer
   a House gives its fortune to the treasury). Runtime and data: no change to the world.
2. **Building materials.** The goods, crafts and the builders' yard above; the Town Works' and farms' baskets.
3. **The towns' trades.** `specialties.json`; producers' yield; the roads and the export channel preferring a town's
   trade.
4. **The world** (a generator pass, then DEV): Greatly's Plasterworks, Colour Works and Masons with their crews; the
   towns' works staffed for their trades (Frosthollow's smelter crew, Mirebridge's brickworks, Oakhaven's charcoal
   burners); keepers' work labels the game knows. New buildings and people come last, from their own random
   sequences, so nobody already there moves.
5. **Testing and tuning.** econ_watch runs of 28 days, then 90, against the baseline: goods short, flows between the
   towns, the Houses' incomes, hunger and purses (doc 46's health targets).

## Built (2026-10-12)

**Phase 1 (runtime and data).** Match words for the businesses that did nothing (`businesses.json`: the Ropewalk,
the Charcoal Store, the Fish Hall, the Clayholt Brickworks, a cider and wine **press** for the Southmeadow Press, a
**moneychanger** for the banks). The head of a House: "keeping … Court" counts (Mossgrave), "keeping the …" doesn't
(the Rain Hall's caretaker; `Society::houseHead`). Ownership by `houses.json` (`Society::ownerFor`): checked on DEV's
world, each House owns its own trade in Ridgemere and nothing in the towns (Greatly 7: the Greatly Yard and Bank, the
Tileworks, the Lime Kilns and Greyscar's, the Cinder Glassworks and Clearwater Glass; Fell 7; Grayrock 7; Mossgrave
10). A save from before is sorted out once (`Society::sortHouses`, `HouseState::plan`): businesses to their House or
keeper, a House no more giving its money to the treasury, a new one its founding fortune.

**Phase 2.** Goods: whitewash, paint; crafts: whitewash, paint (linseed oil and clay, an earth colour), linseed oil,
mortar, dressed stone, beams, window glass, charcoal (timber, at the charcoal yard, the ironworks and the smelters).
Sand from the clay pits and quarries. The builders' yard; the Town Works' basket and the farms' upkeep.

**Phase 3.** `specialties.json` and `items::townTrade`: a town's producers bring in `yield` (1.5) times its goods and
keep `keep` (2) times as many; its makers keep twice the store of them (`Society::craft`); the roads sign a standing
order with a town whose trade the good is first (`World::tradeCaravans`).

**Phase 4 (generator; in DEV revision 26, build 33).** House Greatly's Plasterworks, Colour Works and Masons' Yard
(`cities.GREATLY_WORKS`), placed from their own random sequence, crewed last of the whole world
(`citizens.later_crews`): regenerated, the world differs from DEV's only by 3 buildings, their doors, 8 new residents
and 2 descriptions (House Greatly's, now building trade). Putting it in DEV: `python3 -m worldgen.realm --import-dev`
(keeps every resident by ID and name).
The first such import renamed eight of the Fell and Grayrock households (their IDs had been given trailing
underscores at the new map's import, which this one didn't recognise); fixed in `realm.py`: the IDs given are kept
in `artifacts/realm/ids.json` (by ID stem and name), written after every import and read by the next, failing that
the world's own. DEV revision 26 is revision 23 plus the 3 works, their doors, the 8 crew and House Greatly's two
descriptions; nobody else changed.

**Found on the way, and fixed.**
- **The iron chain was broke, not short.** The Grayrock Ironworks' till ran dry within a day (six hands' wages, and
  everything above a float of wages alone swept to its House each night), so it couldn't buy ore; Frosthollow's
  smelter had no charcoal at all. Now a business's float counts its materials (`Society::floatOf`), a House invests in
  its own trade (a business of its trade run low is filled up from the House's purse, and never sold for it), and the
  ironworks and smelters burn their own charcoal. (Other businesses are still the rescue fund's, doc 46.)
- Linseed oil at the water mills unsettled their buying of wheat; it is the colour works' alone.

**Iron on the roads (standing orders, 2026-10-12).** Iron bars were still short after the fixes above, though
Frosthollow had ore piled up (600 in store) and its smelter could make a bar in four minutes. Each link was choked:
- **What a town could spare.** A maker's want of a material was counted once for each craft that takes it: a smith's
  fourteen iron crafts made fourteen stores of bars wanted, so Frosthollow's own smith left it nothing to spare and no
  town but Ridgemere ever had iron from it. Now one store of each material a maker (`World::marketOf`).
- **What an order promised.** A trade town's makers keep making its goods all week, so an order takes only half of
  its spare from what is left to promise (`promised`): Frosthollow could promise twice what it had on the day.
- **The size of an order.** Signed for what a shop was short that moment, a few bars. Now for a week of what it
  uses (`Society::useRate`: through each of its crafts, as fast as their goods sell), when it is signed and when it is
  agreed again. It is raised only when the shop runs low (under half a week of its use): raised too when the seller
  fell short, or for use alone, orders for fruit and fish grew past what the shops could sell or the seller send, and
  the rescue fund kept the shops open.
- **How soon it grows.** An order signed on the first day is sized before anyone has sold anything, and was looked at
  again a month on. Now a new or raised order is looked at again in a week (`OrderSettles`), a settled one monthly.
- **Nobody to go.** An order is agreed again by a porter of the buyer's town walking to the seller's market; the towns'
  people all have work, so their orders never were. Now the trader whose caravan brings it agrees it again on arrival
  (`World::agreeAgain`, shared with the porters).
- **The smelter's store.** It stopped at its usual store (80 bars) and a weekly caravan could take only what was over
  half of it. Now a town's makers of a good that keeps share a week of the orders on it, kept besides (World tells
  Society the orders each day, `Society::setStandingOrders`). Shared: kept whole by each, Ridgemere's six smiths each
  made the towns' week of nails, and sat on 1,800 of them.
- **The buyer's till.** A house's smithy kept a float of three days' wages and a batch of materials; the rest went to
  its house each night, and a caravan's 77 bars cost four times what was left. Now its float holds a week of its
  standing orders too (`Society::floatOf`).

## Results (28-day runs, `econ_watch --deterministic`, compared by `tools/econ_compare.py`)

| | Before | Phases 1–3 | + new works | + the iron fixes |
|---|---|---|---|---|
| Median purse, day 28 | 172p | 187p | 184p | 188p |
| Gini | 0.356 | 0.355 | 0.362 | 0.370 |
| Broke / hungry / starving | 4 / 0 / 0 | 4 / 0 / 0 | 5 / 0 / 0 | 0 / 0 / 0 |
| Food on Ridgemere's shelves | 440 | 820 | 644 | 1,250 |
| Food on Frosthollow's shelves | 0 | 0 | 28 | 69 |
| Larkfield's trade takings | 17.0k | 26.2k | 25.8k | 27.9k |
| Makers short of iron bars (last week) | 17,269 | 17,447 | 17,682 | 14,523 |
| Makers short of charcoal | 453 | 417 | 441 | 0 |
| Houses' takings / invested back | 141k / – | 37k / – | 39k / – | 44k / 26k |

(The first two columns on DEV's build; the last two on the regenerated world, exported the same way; "+ new works" on
the old world gave the same figures within a few percent.)

**56-day runs** (the regenerated world; "makers short" counts each craft lacking a material each hour, so a smith's
fourteen iron crafts count fourteen times: read it for direction, not size):

| | Iron fixes, want once | + the standing-order fixes |
|---|---|---|
| Iron bars smelted / used, 56 days | 2,530 / 2,396 | 3,897 / 3,060 |
| Nails made / in store at the end | 4,896 / 747 | 5,040 / 842 |
| Makers short of vegetables / oats / fish (last week) | 1,152 / 1,281 / 1,218 | 524 / 434 / 619 |
| Food on Ridgemere's / Oakhaven's shelves at the end | 308 / 62 | 545 / 145 |
| Frosthollow's standing orders (coins; goods) | 10.6k; 3,896 | 16.4k; 6,629 |
| Median purse, day 28 / day 56 | 199p / 242p | 179p / 206p |
| Gini, day 56 | 0.399 | 0.397 |
| Broke / hungry / starving, day 56 | 0 / 0 / 0 | 4 / 0 / 0 |
| Rescue loans to shops, 56 days | 55k | 86k |
| Residents' money / shops' tills / houses' businesses' tills, day 56 | 244k / 56k / 4.8k | 208k / 81k / 11.8k |

Goods move: twice the iron reaches the smiths and the towns' food shortages fell by half or more. But residents end
the run poorer: what they spend on the goods now to be had stays in the shops' and the houses' tills (and the rescue
fund's loans to the shops that buy dear on the roads, at the road's 40% over the selling town's price, and sell to
the town at its own). The money isn't lost; it isn't paid back out as wages.

**Still to do.**
1. **Money back to the residents.** Shops' tills hold 25k more, houses' businesses' 7k more: their takings from the
   new trade aren't paid out. Candidates: a shop's help shares its takings over its float (doc 46), wages by takings
   in the city as in the towns, the road's markup (40%, a placeholder) lower for standing orders.
2. **The smelter's working money.** Keeping a week of the orders in bars ties up its till (6 rescue loans). Paid on
   delivery; perhaps half paid when the order is signed.
3. **PROD**: a push (the user's).
4. **Longer runs**: 90 days, then a year (the user runs year-long tests), and doc 46's health targets.
5. **The towns' own works** for their trades: Oakhaven's charcoal burners and joiners, Mirebridge's brickworks crew,
   Larkfield's dairy and tannery.

## Testing

`build-econ/econ_watch EXPORT DAYS OUT --deterministic` (a 28-day month takes a few minutes), compared by
`tools/econ_compare.py` (Phase 5): per town, hunger, purses and goods short; between towns, coins and goods moved by
caravan and what; per House, its businesses, takings and fortune.
