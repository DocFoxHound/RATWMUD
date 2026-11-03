# 72. Pulling the Country into the world (2026-11-03)

The user, 2026-11-03 (verbatim, by speech to text):

> But yeah, the communities will become towns. I don't think I want to implement full towns yet because we haven't
> implemented everything from country into actual NPCs. But yeah, as we continue to grow, we will add more to the
> communities to make them towns. For now, I'm not too concerned about inns as much as I am about food, since that's
> what we're working on. And yeah, I think your suggestion about increasing the yields makes sense. That's kind of what
> we intended to do. So go ahead and implement those changes. And then let's look at some of the other things that are
> still left in the quote-unquote country, so that we can start pulling those out of the nebulous country entity and
> start putting them into the real world.

This doc is the second part: what is still "the Country", and in what order to make it real. The farms' yield
increase (the first part) is built separately, on doc 71's lane. Doc 71's Phase C open questions are partly answered
above: the communities **stay regions until they become towns** (question 2), and the community inns' and stores'
losses can **stay on the treasury's wage support for now** (question 1). Keeping them regions is not free: on a restart
`World::setupTowns` (Core/RatwRoads.cpp:112) makes any region with five residents and a market a town, and every
community already has more than five, so Phase 0 adds a rule that it skips the communities.

Builds on doc 71 (the Country's industries: the food farms, Phases A to C), doc 70 (the realm and its reserved sites),
doc 68 (the Country and the city loop), doc 67 (the Houses' trades in Ridgemere 2.0, R21-R29) and doc 66 (the
unplaced pool). Doc 61's trades are the old map's: in Ridgemere 2.0 **Greatly** is construction (R25), **Fell** fine
wines, liquors and fruit goods (R27), **Grayrock** owns the wharf (R28) and **Timberland** the city's forges (R29). Iron stays the
Country's (the user, doc 71: "right now iron will remain in the Country").

**How it was measured.** All money is the mean of a day over **years 2 and 3 of `people3`** (the first 3-year run with
the farms' real crews, all six food areas on; `artifacts/economy/r2/people3.full.jsonl.gz`, days 365-1094), unless
marked. The Country's intake is what it keeps **after the gate toll** (30%, to the treasury). The toll on a good is
therefore 3/7 of the Country's intake from it. The reserved sites and distances are from `artifacts/realm2/realm-report.json`
(as built) and `Data/Economy/communities.json`. The unplaced people were read once from DEV's `live.npcs` (read only,
rolled back). **No simulation was run and nothing was written to DEV** for this doc. The aggregation scripts are in the
session's scratchpad (`country_next/agg.py`, `flows2.py`, `daymeans.py`, `dist.py`, `group.py`; `season23b.py` for
section 3's seasons).

## Headline

- **The food has left the Country.** It sells no food and its basket buys no food or drink. What is left is **1,393p a
  day** of non-food goods (3,429p before the farms), against **824p a day of posts** it pays to about 100 people.
- **The largest piece left is the woods: 436p**, nearly all firewood (283p, 450 pieces a day, 586 in winter). Firewood
  is the city's only fuel: households' fuel need takes firewood **or peat**, but nobody cuts peat, so firewood fills it
  all.
- **The Country's posts are the blocker, not the crews.** Every industry moved out takes takings from the payer of those
  100 posts. With nothing else changed, the woods alone cut the Country's buy-back of city goods from 569p to about
  160p; the woods with stone and fibre leave it unable to pay its posts in summer.
- **Recommended: the estates' rent.** Each new works pays its crew at the farms' fixed wages and pays **what is over to
  the House that holds its land**. At first that is the Country's own purse, relabelled "the Houses' estates", so the
  posts keep their payer and only the crews' wages leave it. Those wages are spent in the city's shops.
- **Recommended order**: (1) herbs and fibre onto the food farms that already exist; (2) **fuel**: two logging camps,
  two coppices with a charcoal hearth, and a peat cutting (a second fuel, in place of part of the firewood); (3) stone,
  clay and sand (the lime quarry is too far from any home, 4.4); (4) the city's makers first for craft inputs (new
  `suppliers` entries only); (5) the country workshops, when the communities become towns; later, iron.
- **What stays the Country**: iron and metals (for now), limestone and lime (for now), the ships' goods from beyond
  the realm and the city's exports, the arrivals and their purses, and the unplaced pool. The wharf's posts are
  Grayrock's (doc 67 R28): the Country pays them only until the estates are split (Phase 6).

## 1. What the Country still is

### 1a. What it sells to the city (shops to the Country, a day, its intake after the toll)

**1,393p a day in all.** The Country supplies **100% of what the shops draw** of every good below. Its share of all
sales (shop sales to households included) is 55% of firewood by pennies, 84% of iron bars by pieces, and 40-46% of
pamphlets, soap, scarves and pots.

| Industry (doc 71 §5) | p a day | Goods: pieces a day / pennies a day (main buyers) | Spring / summer / autumn / winter |
|---|---|---|---|
| **woods** | **436** | firewood 450 / 283p (general store 43%, provisioner 12%, bakery 12%); timber 60 / 84p (sawmill, ironworks, foundry); charcoal 11.5 / 32p (smithy, printing house, scriptorium); planks 22 / 30p (cooperage, cartwright, carpenter); oak bark 11 / 7p (tannery) | 420 / 399 / 405 / **528** |
| **country workshops' goods** | **408** | pamphlet 27 / 114p (general 100%); fine soap 29 / 96p (general); wool scarf 34 / 92p (rag-and-bone); clay pot 48 / 51p (rag-and-bone); iron hoops 10 / 28p; paper 9p; nails 5p; leather 5p; cookpot 4p; bandages 2p; ink 2p | 388 / 413 / 445 / 378 |
| mining and metals (stays) | 271 | iron bar 38 / 237p (smithy 73%, cooperage, cartwright); iron ore 15 / 10p; tin ore 4 / 9p; copper ore 5 / 7p; copper bar 7p (jeweller); bronze 1p | 298 / 251 / 260 / 298 |
| **fibre** | **145** | linen cloth 24 / 82p (apothecary, tailor, papermill); hemp 51 / 36p (ropewalk, chandlery, weaver); cord 30 / 20p (chandlery); flax 10 / 7p (paintworks) | 159 / 138 / 142 / 155 |
| **stone, lime, clay, sand** | **123** | clay 59 / 41p (brickworks 72%, pottery 25%); stone 27 / 38p (mason); limestone 31 / 22p (brickworks, plasterworks, mason); sand 26 / 18p (mason, plasterworks, glassworks); lime 3 / 4p | 127 / 110 / 112 / 132 |
| herbs | 11 | herbs, wormwood, willow bark, thyme, garlic, comfrey, resin (apothecary, perfumer) | flat |

Season figures are the data survey's 3-year means; the season columns sum a little higher than the years 2-3 column.

### 1b. What it buys from the city (the Country's basket, a day)

**569p a day.** For most of these the Country is **84-100% of the buyers**: if its basket shrinks, these makers lose
their only market.

| Good | p a day | The Country's share of buyers |
|---|---|---|
| paper | 66 | 84% |
| steel bar | 62 | 100% |
| iron bar | 61 | 16% |
| charcoal | 53 | 54% |
| bandages | 34 | 55% |
| handbell | 32 | ~100% |
| map | 27 | ~100% |
| rivets, glass beads, ink, leather bindings | 25-26 each | 90-100% |
| ledger, copper bar, mining pick, charm, tin bar, bronze bar, masking oil, linseed oil, paint, ear ring, fang charm | 2-17 each | mostly 64-100% |

**It buys goods it also sells.** The Country buys iron bars (61p), charcoal (53p) and paper (66p) from city makers and
sells the same goods to other shops, because the `suppliers` rule (`Data/Items/country.json`) covers only barrels, and
the resale path (`restockFromStore`, `!w.craft`, Core/RatwCityLoop.cpp:1586) never calls `buyFromCity`.

### 1c. Its posts and its purse

| Who | Posts (about) | p a day | Rule (`country.json` `wages.rules`, payer `country`) |
|---|---|---|---|
| the wharf and the stores: unloading ships, cranes, carrying up from the wharf, the stores' tallies, the Wharf Office's clerks | ~56 | ~448 | "Carriage by sea and the Country's warehouses" |
| the Houses' people: stewards, servants, "in service to the", the Fell cellars, the Greatly stables, the Counting House's clerks | ~40 | ~320 | "The Great and Minor Houses' people" |
| the Minor Houses' heads, the harbourmaster, the Counting House's overseer (a keeper's 14p) | 4 | 56 | "The Minor Houses' family members…" |
| **in all** | **~100** | **824** | to households: lower 575, middle 136, upper 112 |

The split is from the city's placement file (`artifacts/ridgemere2/placement.json`: 54 matched the wharf rule and 11 the
Houses' rule by their labels; the rest of the Houses' people are matched in the live world). Phase 0 counts it exactly.

- **Its money a day**: in 1,393p; out 824p posts, 569p basket, 1p carriage.
- **Its purse** averages 391p (1p to 572p). That is always under its reserve plus tonight's wages (~1,023p), so
  **`patronsBuy` never has spare cash**: the Country's patrons (the Houses' retainers at the armoury, their halls at the
  builders' yard, the wharf's hulls at the shipwright) fell from ~50p a day to **0 within the first ~80 days of year 1**
  (the last on day 83; 7.6p a day over year 1) and stayed at 0. The shipwright is a till short of its wages (17,696p
  over 3 years).
- **Its basket is what gives.** Wages come first, so every penny of intake lost comes off the basket (1b).
- **Arrivals** (about 4 a year) and the community markets' people get their purses (up to 30p) from the Country's purse.
  The ~369 unplaced NPCs are the Country's people too.

### 1d. The treasury, for reference (people3, years 2-3, a day)

Toll in **1,651p** (of it ~597p on the Country's goods, the rest mostly the farms'); alms 318p; the surplus paid out
only **124p**; the short tills' wage support wanted 245p; its level about 5,300p. **The treasury has little to spare**:
it can't take over posts of any size without a new source (open question 1).

### 1e. Wanted and short outside the farms

- **Peat**: the run reports households wanting about **301 a day** (middle 182, lower 103, upper 16) and none sold, on
  all 1,096 days. That is not a shortage: the need is "any firewood or peat" (`crafts.json` line 208), and firewood
  meets it (the ~450 firewood a day, 586 in winter). `crafts.json` also has the producer ("cuts peat", 4 peat a spell,
  line 145), but no post makes it. Peat would be **a second fuel in place of part of the firewood**, not a want filled.
- **Tallow candles**: short by about 28 a day and pinned at the top of the price band 78% of the time, while **the farms
  pile tallow**. The chandlery makes them from tallow and cord; the cord comes from the Country's hemp.
- **Linen neckerchiefs**: short by about 11 a day.
- **The farms pile** hides (+5,930 a year), bones, tallow, salt and oats. Fine soap is tallow and herbs; leather is hides.

## 2. The people left to crew it

The pool is **369 unplaced** (524 in DEV less the 142 placed by Phase C and the 13 sent to city posts), of whom about
**257 are of working age**. By their old trade (`tools/country_industry.py` `POOL`):

| Old trade | Working age left | Who they are | Fits |
|---|---|---|---|
| **woods** | **36** | 25 "logging at" five old camps (Cedarfall, Fellside, Oakfell, Snowpine, Beechcombe); 3 of Oakhaven's sawmill; 5 woodcutters; 2 trappers; 1 herb gatherer | the woods |
| **mines, stone and clay** | **28** | 12 ore diggers, 3 Frosthollow smelters, 3 iron traders; 5 Greyscar stone cutters, 1 lime kilns; 3 Clayholt clay diggers, 1 brickworks | stone and clay now (10); iron later (18) |
| fields and mills | 48 | field hands of 11 old farms, 9 water-mill people | fibre; spare hands |
| herds and flocks | 19 | dairies, pigs, rabbits, sheep, stables | the farms' families later |
| keeps the house | 47 | the fallback | peat; any short works |
| the towns' trades and shops | ~25 | shop people of Frosthollow, Oakhaven, Mirebridge and Glasswater (6 smiths, 1 carpenter, 2 press and brewers, innkeepers, bakers) | the workshops, when towns come |
| service and labour | 32 | 19 "in service to [a House]", 4 wharf, old towns' shop hands | the Houses' estates, city arrivals |
| the watch | 12 | the old towns' watch | a community watch, when towns come |
| the Heights | 16 | never placed (rule) | |

Every industry below except the workshops can be crewed from its own old trade, with people to spare. Orchards, bees
and fishing are used up.

**Room to live.** Beds now against people placed in the smoke run: Coldwater 62 / ~26, Larkfield 31 / ~14, Southmeadow
25 / ~9, Fenshore 25 / ~10, Saltwell 22 / ~9, with room to grow (cottage lots, 6 beds each) 6 / 8 / 6 / 9 / 6. These
beds were sized for the farm hands' families later, so new crews compete with them. Ridgemere already lodges 23 farm
hands with city households of their class (doc 68's arrivals' rule).

**Walking.** At 2.6 tiles a second, 800 tiles is about half a game hour. The farms' own walks reach 1,047 tiles
(Coldwater to the Greyteeth fold); "near the city" in practice is within about 800 tiles of its edge. A walk is about
1.17 times the straight line (1.04 to 2.26 in the fens).

## 3. The money problem: who pays the Country's posts

| Path (cumulative, a day) | The Country's intake | Posts | Left for its basket (now 569p) |
|---|---|---|---|
| now | 1,393 | 824 | 569 |
| + herbs and fibre to the farms (Phase 1) | 1,339 | 824 | 515 |
| + fuel out, **no rent** | 933 | 824 | 109 |
| + stone, clay and sand out, no rent | 836 | 824 | **12: posts unpaid in summer** |
| + fuel out, **with the estates' rent** (Phase 2) | 1,159 | 824 | 335 |
| + stone, clay and sand, with rent (Phase 3) | 1,103 | 824 | 279 |
| + the city makers first for craft inputs (Phase 4) | 916 | 824 | **92: too thin** (summer's intake runs ~65p under the year's mean: 852p, so ~28p over its posts) |

Today summer's intake is 1,306p against the year's 1,393p (87p under). The Phase 4 row's seasons: spring 871, summer
852, autumn 923, winter 1,021 (the kept goods, plus the fuel's and the pits' takings less their crews' fixed wages;
`season23b.py`). Limestone and lime (26p) stay the Country's in every row (4.4).

**Options for the payer**, from the least change:

1. **The estates' rent (recommended).** A works' till pays its crew's fixed wages, keeps a reserve (two days' wages),
   and pays the rest each night to the House that holds its land. At first all Houses share one purse: the Country's,
   relabelled in reports "the Houses' estates and the sea trade" (`outside:country` unchanged in code). The Country then
   loses only the new crews' wages, which come back to the city's shops as the crews' spending. The Houses' trades are
   doc 67's (R25-R29): **Greatly** construction (the quarries and the clay pits; `quarry_granite` is already marked
   Greatly's), **Timberland** the city's forges (the iron, later, and the Timberland Sawmill), **Grayrock** the wharf
   (its ~56 posts), **Fell** wine and fruit (no works here). **Who holds the woods is not decided** (open question 11;
   recommended Timberland, whose sawmill and forges take the timber and charcoal). Phase 6 can split the one purse
   into each House's estate, each paying its own people, the wharf's posts from Grayrock's.
2. **The treasury pays the wharf** (the Wharf Office is a public office). ~448p a day against a surplus of 124p: the
   treasury would go under its float without a higher toll. Not recommended.
3. **Fewer posts**: cut the wharf's and the Houses' posts as industries leave (the ships unload less once goods come by
   road). Their holders become post-less city wolves (doc 68: they take an empty post near them). Realistic, but it
   puts 30-50 city people out of work at once. Kept as a lever.

**With the rent, the money each phase moves** is: the crews' wages from the Country's basket (the makers of 1b) to the
crews' households (the food and comfort shops). That is the same trade doc 71 made for the farms (half crew, half
basket), done through the Country's existing basket instead of a new one.

**The treasury.** The new works sell through the store room at the gate like the farms, so **the toll is unchanged** in
Phases 1-3. Phase 4 loses the toll on the goods routed till to till (~80p a day). The real risk is the wage support:
makers that only the Country buys from go short as its basket shrinks, and the treasury pays their wages down to the
drift's floor: paper, steel, rivets, glass beads, maps and handbells are ~238p of the 569p; every good where the Country
is 84-100% of the buyers (those six, ink, leather bindings, ledger, mining pick, charm, tin and bronze bars, masking oil,
linseed oil, paint, ear rings) comes to ~400p.

## 4. The industries, one by one

Wages throughout are the farms' fixed ones (a keeper 16p, a hand 10p; doc 71 question 6). Yields are settings, not
labour (doc 71): each works' yield is set from people3's draws plus 5%, then tuned by `tools/econ_report.py`'s patch.
"Takings" is the intake the Country loses to the works.

### 4.1 Herbs: yields at the eel marsh (no posts)

| | |
|---|---|
| Sites | `marsh_eels` (Withy Creek Marsh, Fenshore's crew): add herbs, wormwood, willow bark, thyme, garlic, comfrey (resin goes with the woods) |
| Workers | none new (8-11p a day pays no one; doc 71 already pins herbs to this crew) |
| Money | ~11p a day to the fish area's till; toll unchanged |
| Shops | apothecary and perfumer draw from the marsh instead; no change in price |

### 4.2 Fibre: hemp and flax at the grain farms

| | |
|---|---|
| Sites | a hemp and flax field on each grain farm: `grain_long` (Longmead, crew at Larkfield) and `grain_lark` (Larkholm, crew at Southmeadow), the retting pond on the farm's river. No new site. |
| Workers | +1 hand at each farm (20p a day), from **fields and mills** (48 left) |
| Takings | raw hemp 36p + flax 7p = **43p**; the grain area's till keeps the 23p over wages for its basket |
| What stays the Country's | linen cloth (82p) and cord (20p) are made goods: the city's weaver makes linen from flax and the chandlery cord from hemp. They move in Phase 4. |
| Shops | ropewalk, chandlery, weaver and paintworks draw from the farms. Cheaper cord helps the chandlery's tallow candles (1e). |

### 4.3 Fuel: the woods and peat (the holder: open question 11)

The map has five timber cuts and two charcoal hearths reserved, but **no coppice, sawmill or peat site**. The city
already has its own sawmill (the Timberland Sawmill since doc 67's renames; its id is still `rm_the_fell_sawmill`) and
charcoal works, so **no country sawmills** are needed: planks come from the city's sawmill (Phase 4).

| Works | Site (as built) | Goods | Crew | Lives in | Walk |
|---|---|---|---|---|---|
| Rainwood logging camp | `timber_rainwood` (652,1092) | timber, firewood, oak bark, resin | 1 + 3 | **Ridgemere** | ~470 from the South Gate |
| Front logging camp | `timber_front` (1692,948) | timber, firewood, oak bark | 1 + 2 | **Coldwater** | ~1,040 (as the Greyteeth fold's) |
| Rainwood coppice | inside or beside `timber_rainwood` (outside Ridgemere's nine cells) | firewood | 1 + 2 | **Ridgemere** | ~470 |
| Greenholt coppice and charcoal hearth | `charcoal_greenholt` (1260,2124) | firewood, charcoal | 1 + 2 | **Larkfield** | ~570 |
| Fens peat cutting | **new**: the Sallow Fens near Fenshore | peat | 1 + 1 | **Fenshore** | under 400 (fen walking) |
| **in all** | 5 works | | **5 keepers, 10 hands (180p a day)** | Ridgemere 7, Coldwater 3, Larkfield 3, Fenshore 2 | |

- **Takings**: firewood 283p, timber 84p, charcoal 32p, oak bark 7p = **406p**. Peat takes part of firewood's place (it
  is the same 1p and households take either), so the fuel's pennies stay about the same; firewood draws fall by
  whatever peat fills. Over wages: **~226p to the woods' holder's estate** (the Country's purse at first).
- **Yields**: firewood and peat together are sized to today's fuel, **~450 a day (586 in winter)**, not on top of it:
  firewood ~270 (winter ~350) and peat ~180 (winter ~240), each +5%. The bakeries' and other makers' firewood (about a
  quarter of the draws) stays firewood unless their crafts take peat. Timber ~65 a day, bark ~12, charcoal ~12. The
  winter peak is the gate: the fuel must never run out.
- **Crew**: 13 from the **woods** pool (25 loggers, 5 woodcutters; 36 in all), 2 for the peat from **keeps the house**
  (nobody in the pool cuts peat). 23 of the woods' people stay unplaced.
- **The coppice is not on `range_city`.** `range_city` ("the city common", 612,644, size ~165) already exists: the
  wood pasture the dairies near the city share as their cattle range, inside Ridgemere's nine cells. A coppice there
  would re-purpose the dairies' common and write ground in the city's cells. Recommended instead: the coppice in or
  beside the Rainwood cut, on realm ground (open question 5).
- **Shops**: the general store, provisioner and bakeries (city and community) draw fuel from the works; the sawmill,
  ironworks and foundry timber; the tannery bark. The city's charcoal works keeps its kilns (its only buyer is now the
  Country: 8 a day).
- **Why not Oakhaven now.** The pool's loggers are Oakhaven's, and Oakhaven (1012,1220) is within ~420 of the Rainwood
  sites. But the Rainwood camp is close enough for Ridgemere, and the user wants towns later. Oakhaven is the woods'
  town when the communities grow (open question 4).

### 4.4 Stone, clay and sand (House Greatly; lime later)

The map has four quarries (granite, slate, lime, sandstone) and two clay pits, both in the southern fens; **no sand
site**. The game's clay pit yields clay and sand together (4 + 1 a spell), so the pits can be the sand's source.

| Works | Site (as built) | Goods | Crew | Lives in | Walk |
|---|---|---|---|---|---|
| Granite quarry | `quarry_granite` (1028,356), House Greatly's | stone | 1 + 2 | **Ridgemere** | ~320 from the East Gate |
| Mirebridge clay pit | `clay_mirebridge` (1180,3276) | clay, sand | 1 hand | **Fenshore** | ~440 |
| Fens clay pit | `clay_fens` (1636,3332) | clay, sand | 1 hand | **Southmeadow** | ~600 |
| **in all** | 3 works | | **1 keeper, 4 hands (56p a day)** | Ridgemere 3, Fenshore 1, Southmeadow 1 | |

- **Takings 97p** (stone 38p, clay 41p, sand 18p), flat across the year; over wages **~41p to Greatly's estate**.
- **Not the lime quarry.** `quarry_lime` (1588,788) is a walk of ~960-1,140 tiles from the city and ~1,250 from
  Coldwater (1956,1796). The user's rule (doc 70, the steadings and the communities) puts hands in the city only for work
  within about 800 tiles of Ridgemere, and every other crew in a community; ~1,250 is past the longest farm walk
  accepted (1,047). **Limestone and lime (26p) stay the Country's** until a home is near: Frosthollow (2308,796) is
  ~720 away, so the lime quarry goes with the iron's town (4.7), or the user makes an exception (open question 13).
- **Crew**: 5 of the 10 in the pool's stone and clay trades (Greyscar's stone cutters, Clayholt's diggers); the lime
  kilns' man waits for the lime quarry. The slate and sandstone quarries stay reserved (nothing draws slate; the sand
  comes from the pits).
- **Shops**: brickworks, pottery, mason, plasterworks and glassworks draw from the works. The clay comes 2,500 tiles to
  the city; carriage is a flat 6p a load, so the shops' costs don't move.

### 4.5 The city's makers first (no new sites)

Craft inputs already go through `buyFromCity` (Core/RatwCityLoop.cpp ~1643, the craft path's inputs); only the resale
branch (`!w.craft`, ~1586) skips it. So **Phase 4 is new `suppliers` list entries in `Data/Items/country.json` only**
(weaver -> linen, chandlery -> cord, sawmill -> planks, smithy and cooperage -> hoops and nails, ...), **no code
change**. Calling `buyFromCity` from the resale path is the resale goods' routing and moves to Phase 5. Craft inputs
first, because the store charges them at 100%, the city maker's catalogue price is the same, and till to till there is
no toll and no carriage, so the shops' costs don't rise:

| Good | Country intake now | City maker |
|---|---|---|
| linen cloth | 82p | weaver (flax from the grain farms, 4.2) |
| iron hoops, nails, cookpot | 37p | smithy, cooperage (iron bars still the Country's) |
| planks | 30p | the Timberland Sawmill (timber from 4.3) |
| cord | 20p | chandlery (hemp from 4.2) |
| paper, leather, bandages, ink | 18p | papermill, tannery (the farms' hides), barber, ink makers |
| **in all** | **~187p** | the makers gain ~267p of sales; the treasury loses ~80p of toll |

**The resale goods** (pamphlets 114p, fine soap 96p, wool scarves 92p, clay pots 51p) are left to Phase 5: from a city
maker they would cost the general store and the rag-and-bone man a third more than the store room's 75%, and both are
already short of their wages.

**The money**: with Phases 1-3 done this leaves the Country only ~92p a day over its posts (3), and about 28p in
summer (852p against 824p): no room for a bad season. So Phase 4 needs a second payer first: a small land rent from the
food farms (open question 2) or fewer wharf posts.

### 4.6 The country workshops' goods (when the communities become towns)

Doc 71 §5 sites 29 workers in potteries, weavers' and soap boilers' cottages, presses and paper mills and a village
smithy. Each has a raw source that is now, or will be, in the realm:

| Workshop | Goods (intake) | Inputs, and where they come from | Community |
|---|---|---|---|
| village pottery | clay pots 51p | clay from 4.4's pits | Fenshore or Southmeadow |
| soap boiler and chandler | fine soap 96p (+ the short tallow candles) | **the farms' piled tallow**, herbs from 4.1, cord from 4.2 | Coldwater (the herds' farms) |
| weavers' cottages | wool scarves 92p, linen | the sheep runs' wool, 4.2's flax | Coldwater, Larkfield |
| country press | pamphlets 114p | paper, ink from the city | Larkfield (nearest the city of the five) |
| village smithy | iron hoops, nails, cookpots | the Country's iron bars | Larkfield |

- **Workers**: the pool has only ~10 who fit by trade (6 smiths, 1 carpenter, 2 press and brewers, 1 herbalist). The
  rest would come from **keeps the house** (47) and **service** (32).
- **Money**: ~408p leaves the Country. This needs the Country's posts split per House (Phase 6) or a farm rent.
- **The soap boiler and chandler come first** of these: they are food-adjacent (they use the farms' tallow, which piles
  up, and the candles are short every day).

### 4.7 Mining and metals (stays the Country's; what pulling it later needs)

| Need | What it takes |
|---|---|
| A town | **Frosthollow** (2308,796), reserved: 220 from `mine_frost_iron`, 459 from `timber_upper`, ~720 from `quarry_lime` (the lime quarry's home, 4.4). The pool's iron people are Frosthollow's (12 ore diggers, 3 smelters, 3 traders, its shop people). It would be a sixth community. |
| Mines | `mine_frost_iron` (near Frosthollow). The second, `mine_grey_iron`, is 1,145+ from any home: either a Greyteeth miners' camp, or the `mine_frost_lead` site re-used as a second iron working (lead and silver have no demand). |
| Smelting | **No bloomery site is reserved.** Doc 71's earlier answer stands: the realm mines ore and the city's Timberland Ironworks (id still `rm_the_grayrock_ironworks`) and foundry smelt it (38 workers at the game's rates against 14 payable). |
| Charcoal | the reserved hearths are in the Rainwood and the Greenholt, far from the mines. A hearth by `timber_upper` would serve Frosthollow. |
| Copper and tin | `mine_ochre_copper` (Saltwell, ~750): 3 workers on ~24p a day; not worth doing alone. No tin streams site; the Greyteeth tin mine is 1,300+ out. |
| Money | **271p a day** (iron bars 237p). Pulling it leaves the Country with only the workshops and the rents, so it needs the per-House split (Phase 6) first. House Timberland's estate (R29: the city's forges). |

### 4.8 Summary

| Industry | Phase | Takings | Crew (wages) | Over wages, to the estates | Sites new to the map | From the pool | Lives in |
|---|---|---|---|---|---|---|---|
| herbs | 1 | 11p | 0 | (to the marsh's till) | 0 | 0 | |
| raw fibre | 1 | 43p | 2 (20p) | (to the grain farms' till) | 0 | 2 fields and mills | Larkfield, Southmeadow |
| fuel: woods and peat | 2 | 406p | 15 (180p) | ~226p, the woods' holder (question 11) | the peat cutting (the coppice in the Rainwood cut) | 13 woods, 2 keeps the house | Ridgemere 7, Coldwater 3, Larkfield 3, Fenshore 2 |
| stone, clay, sand | 3 | 97p | 5 (56p) | ~41p, Greatly | 0 | 5 mines, stone and clay | Ridgemere 3, Fenshore 1, Southmeadow 1 |
| city makers for craft inputs | 4 | 187p | 0 | (to city makers) | 0 | 0 | |
| country workshops | 5 | ~408p | ~29 | to be sized | the workshops in the communities | ~10 by trade, ~19 fallback | the five communities |
| iron and metals, limestone and lime | later | 297p | ~16 | Timberland (iron), Greatly (lime) | Frosthollow, a bloomery | 18 metals, the lime kilns' man | Frosthollow |

Phases 1-3 place **22 workers** (beds: Ridgemere 10, Coldwater 3, Larkfield 4, Fenshore 3, Southmeadow 2) and leave
the Country with ~279p a day over its posts (from 569p).

**The works keepers' homes and families.** The works have no premises (no farmhouse), so a works keeper lives with the
hands: in the city within 800 tiles, else in the crew's community. Under Phase C's rule a keeper with family among the
unplaced brings them (9 of the 31 farm keepers did). Recommended: **pick the 6 works keepers from those with no family
among the unplaced first**, and count any family that does come against the community's room to grow (cottage lots,
6 beds each) or, in Ridgemere, a city household of their class. Or a keeper's cottage at each works, like the farms'
steadings (6 new buildings on the map). Open question 12.

## 5. The recommended order

Food-adjacent and largest money first; each phase as doc 71 did it: **modelled producers with virtual crews first
(Phase B), then real people (Phase C)**. Every run needs the user's go-ahead.

| Phase | What it builds | Checks | Runs (each asked first) |
|---|---|---|---|
| **0. Works as areas, and the rent** | Generalise `Core/RatwFarms.cpp`'s areas to non-food works (verify nothing assumes food: the Country's table, `seasonal` groups, the stew); a works' `rent` rule (wages, reserve, the rest to the estates' purse); the Country's report lines split (estates' rents, the sea trade, posts by rule); count the 100 posts by rule and by House exactly; Atlas's and the DM's Economy panels show works beside farms; **`World::setupTowns` skips the five communities** (regions marked as communities in `Data/Economy/communities.json`) until the user makes them towns. No world change. | C++ tests: a works yields, sells through the store, pays its crew, pays its rent, money conserved; switched off, the Country supplies again; **a restart keeps the communities regions** (no towns, no town roads, bandit camps, caravans or festivals' purses for them). | none |
| **1. Herbs and fibre on the farms** | Herbs as yields of the eel marsh; hemp and flax fields on the two grain farms (yields, +1 hand each, placed by Phase C's rule from fields and mills). | farm_people tests for the extra hands; the grain and fish areas' tills still pay their crews. | 30-day smoke; folded into the next 3-year run |
| **2. Fuel** | The five works of 4.3 as modelled producers (Phase B), the new site(s) placed on the map (the peat cutting, and the coppice if beside the Rainwood cut rather than in it; doc 70's tool, a DEV build, outside Ridgemere's cells), then their people (Phase C). Peat sold through the general store and provisioner; firewood's yield cut by what peat fills. | The fuel never short (firewood or peat filled 99% of days, winter included); peat sold and fuel not piling; every works pays its crew 99% of days; the Country pays its posts every day; the treasury over its float every night; no hunger; money conserved. | 30-day smoke; **3-year full run** (`--economy-level full`), with the report's patch for the yields |
| **3. Stone, clay, sand** | The three works of 4.4 (granite quarry, two clay pits), modelled then real. Limestone and lime stay the Country's. | As Phase 2, plus the builders' trades' draws filled. | 30-day smoke; 3-year run (can share Phase 2's second run) |
| **4. The city's makers first** | New `suppliers` list entries in `country.json` for craft inputs only (4.5), no code change; with the second payer the user picks (question 2). | Makers' tills, the general stores' tills; the Country's margin in summer; the treasury's toll and support. | 3-year run |
| **5. The country workshops** | When the communities become towns (lifting Phase 0's `setupTowns` rule): the soap boiler and chandler first, then pottery, weavers, press, smithy; `buyFromCity` called from the resale path (`!w.craft`) for the resale goods. | As Phase 2, plus the candles no longer short. | per workshop group |
| **6. The estates split; iron** | Each House its own estate purse paying its own people: Grayrock's the wharf's posts, Greatly's the quarries' and pits' rents, the woods' holder's the fuel's, Timberland's the iron later; the ships' goods from beyond and the exports alone the Country. Iron, the lime quarry and Frosthollow when the user says. | | |

## 6. What stays the Country, and why

| Stays | Why |
|---|---|
| **Iron and metals** (271p) | The user's decision. Its sites are reserved; 4.7 says what pulling it needs. |
| **Limestone and lime** (26p) | The lime quarry is past the 800-tile rule from the city and ~1,250 from Coldwater (4.4); it goes with Frosthollow. |
| **The ships' goods from beyond the realm** | Ships come from beyond the realm. The wharf itself is Grayrock's (doc 67 R28): its ~56 posts are paid from the Country's purse only until Phase 6 gives them to Grayrock's estate. |
| **The city's exports** (the basket's maps, handbells, glass beads, charms, steel) | What the city sells beyond the realm. These makers have no buyer in the realm; the Country's basket is their market. |
| **The Houses' estates** (until Phase 6) | The land the works stand on is the Houses'; the estates collect the works' rents and pay the Houses' people (the wharf's posts among them). Not a producer any more, a landlord. |
| **Arrivals and the unplaced pool** | People come from beyond the realm (about 4 a year, with up to 30p each). The pool is the city's and the works' labour reserve. |
| **Goods no one in the realm makes** | The ten farm goods no farm grows (brandy, apple brandy, aqua vitae, wheat, butter, cheese, offal, eggs, sheep, sheepskin) could come by sea as imports instead of going without (open question 8). |

## Open questions for the user

1. **Who pays the Country's posts as industries leave.** Recommended: **the estates' rent** (3, option 1): each works
   pays its crew and gives what is over to the House holding its land, at first into the Country's purse relabelled
   "the Houses' estates and the sea trade". Or the treasury pays the wharf (it can't afford ~448p a day), or the posts
   shrink as industries leave (30-50 city people lose their posts).
2. **A land rent from the food farms too.** Recommended: **not before Phase 4**, and then only if the runs show the
   Country short (a few percent of the farms' takings, ~120p a day at 5%, from their basket, not their wages). Or never:
   then Phase 4 waits for Phase 6.
3. **Peat.** Recommended: **a peat cutting at the Sallow Fens**, crew at Fenshore from those who keep the house, as **a
   second fuel that replaces part of the firewood** (firewood and peat together sized to today's ~450 a day, 586 in
   winter; no shortage is filled, since households take either and firewood meets the need now). It spreads the fuel
   over two works, so a bad week at the camps doesn't leave the city cold. Or leave peat unsold (firewood stays the
   only fuel and the camps' yields are ~450 a day).
4. **The second logging camp and Oakhaven.** Recommended: **`timber_front`, crew at Coldwater** (~1,040 walk, within
   the 1,047 already accepted), and Oakhaven waits for the towns. Or build Oakhaven now as a sixth community for the
   woods (the pool's loggers are its people). (`timber_sound` with its crew in Ridgemere is dropped: a ~920 walk breaks
   the 800-tile rule.)
5. **The coppice and the new sites.** `range_city` ("the city common") is not free ground: it is the dairies' shared
   cattle range, inside Ridgemere's nine cells. Recommended: **the coppice in or beside the Rainwood cut**
   (`timber_rainwood`, on realm ground), so the only new site is **the peat cutting at the fens**, placed by doc 70's
   tool. Or re-purpose the city common as a coppice (the dairies lose their shared range, and ground in the city's
   cells is rewritten).
6. **Stone.** Recommended: **the granite quarry and the two clay pits as the sand's source**; the lime quarry waits for
   Frosthollow (question 13); slate and sandstone stay reserved. Or all four quarries (needs more crew on 123p, and
   homes for the far ones).
7. **The city's makers first.** Recommended: **craft inputs only** in Phase 4; the resale goods (pamphlets, soap, scarves,
   pots) with the workshops in Phase 5. Or route everything at once (the general stores and the rag-and-bone man pay a
   third more, ~150p a day more toll lost, and the Country loses another ~353p).
8. **The farm goods no farm grows.** Recommended: **imports by sea** from the Country (it keeps a small sale of them,
   seasonal), until farms grow them. Or leave the shops without them, as now.
9. **The works' wages.** Recommended: **the farms' fixed wages** (16p a keeper, 10p a hand), editable per works and per
   area in Atlas and the DM's Economy tab, as the farms'.
10. **The workshops' first step.** Recommended: **the soap boiler and chandler first** when the communities grow (the
    farms' tallow piles up and the candles are short). Or the potteries first (by the clay pits of Phase 3).
11. **Who holds the woods, and the old names.** Doc 67 gives Greatly construction (so the quarries and pits, as
    `quarry_granite` already says), Grayrock the wharf, Timberland the forges, Fell wine and fruit; no House is given
    the woods. Recommended: **Timberland** (its sawmill and forges take the timber and charcoal). Or Fell, or the Crown
    (the treasury). The city's ids still carry doc 61's names (`rm_the_fell_sawmill`, `rm_the_grayrock_ironworks`;
    the text says Timberland since doc 67's renames): keep the ids, as doc 67 did for Mossgrave?
12. **The works keepers' homes.** Recommended: **with their crews** (the city within 800 tiles, else the community),
    keepers picked first from those with no family among the unplaced. Or a keeper's cottage at each works, like the
    farms' steadings.
13. **The lime quarry.** Recommended: **wait for Frosthollow** (~720 away); limestone and lime stay the Country's until
    then. Or an exception to the 800-tile rule: the crew in Ridgemere, walking ~960-1,140.

## Progress

### Phase 1, the economy's side: herbs and fibre on the farms, and the patch damped (2026-11-03, branch `r2-country-p1`)

Branch `r2-country-p1`, from `ridgemere2` (2537b2d). The user's decisions (2026-11-03): phases 1-5 in this doc's order;
open questions not answered take the recommendations. **Data and the report tool only: no code in Core, no world change,
DEV not written, no simulation run.** The yields are sized from the people4 run (doc 71's confirming 3-year run,
`artifacts/economy/r2/people4*`; the fixed tool's report and patch `people4-fixed-report.md`, `people4-fixed.patch.json`), years 2-3 for the new goods.

- **Herbs (4.1).** The eel and fowl marsh (`eel_and_fowl_marsh_1`, Withy Creek Marsh, Fenshore's crew) yields herbs,
  wormwood, willow bark, thyme, garlic and comfrey, and they are the fish area's goods: with fish on, the apothecary and
  perfumer draw them from the marsh's store and the Country sends none (`farmAreaOf`, as every farm good). Sized on the
  Country's sales to the shops in years 2-3 + 5%, by season (pieces a day): herbs 3.2 / 3.0 / 3.3 / 3.3, wormwood
  3.6 / 3.2 / 3.2 / 3.5, willow bark 2.3 / 2.1 / 2.3 / 4.4, thyme 1.0 / 1.0 / 1.1 / 2.9, garlic 1.3 / 1.2 / 1.3 / 1.2,
  comfrey 1.0 / 1.2 / 1.1 / 1.1: about 11p a day to the marsh's till. No new post. Resin stays the Country's (the woods).
  None of them spoils (no `keeps`), so the marsh keeps what isn't drawn.
- **Fibre (4.2).** Each grain farm yields hemp 30.9 / 27.4 / 27.4 / 30.5 and flax 4.2 / 3.1 / 3.9 / 5.3 before the area's
  scale (92%; the two farms together the Country's years 2-3 sales + 5%: hemp ~54 a day, flax ~7), the grain area's goods:
  the ropewalk, chandlery, weaver and paintworks draw from the farms, about 40p a day. **A hand more each (1 + 5)**:
  `crew.hands` 5, so Phase C's placement (`staffFarms`) fills the sixth post by its rule, of the fields and mills first;
  its float two days of the new wages (132p) and its crew's purse by its size (352p, the other farms' 57p a head). The
  extra hands live in Larkfield (Longmead) and Southmeadow (Larkholm): `communities.json`'s hands 11 and 9 (the farms 5),
  and realm2_places' check with them. Linen cloth and cord stay the Country's until Phase 4. The areas' `revenueADay`
  carry the new goods' pennies (the Country's basket's shift weighs the areas by them).
- **The patch damped.** people4's patch swung back from people3's (the pig farms' meal 12.6 to 1.5, salt and oats far up)
  for two reasons, both in `tools/econ_report.py`'s demand basis: (1) the shops' draws cut short were counted as a day's
  want, but a draw's lack is a shelf's deficit, a stock counted again every day it stands (a craft's input, never sold,
  at its whole deficit each day: the provisioners' salt alone ~30 a day on 49 sold); and (2) each farm was sized by its
  own sales, but a draw takes from the farms by their stock, so whichever held more sold more (the dairies' meal sold, the
  piggeries' sat). Now: **what was taken** (sold, the Country's table, its lack) **of all the good's farms together,
  shared by their yields**, plus **the shelves left under refilled over a week** (`REFILL_DAYS` 7), and that only for a
  good the farms never spoiled that season. And `--apply` goes **half way** (`--damping`, default 0.5) from each yield
  now to the patch's, `--goods` limiting it to some. Applied from people4 (the tool's tests updated, a third added):
  - every good but salt and grain at 0.5: meal 138-147 a day (after the scales) to 109-115 (the patch 76-82, sold ~73,
    spoiled ~65), raw meat 264-286 to 211-229, bones 59-66 to 49-52, hides 7-9 to 6-7.5, shellfish 182-267 to 161-224,
    milk 112-129 to 103-117, honey and wax down (piling), tallow unchanged (~74: sold 70);
  - **the grain split at the whole step** (flour piled 6,500 a year, the shops' oats short): flour 107 / 117 / 121 / 115 to
    97 / 106 / 107 / 96, oats 84 / 99 / 101 / 87 to 93 / 107 / 108 / 97 (the grain farms' flour stock covers the cut);
  - **salt at yield scale 88** (from 80), its yields kept: the fixed patch's whole step is about +10% (salt 75 / 63 / 60 / 69
    to 83 / 69 / 66 / 76), the same as the scale.
  `Tests/farms_tests.cpp`'s pinned flour mean is 56.94 (from 63.10).
- **Checks.** `farms_behaviors` (668 checks): 78 hands; the marsh's herbs the fish area's and nobody else's, resin no area's;
  hemp and flax the grain farms', 1 + 5, the float two days' wages; salt's scale 88; with fish and grain on, comfrey drawn
  from the marsh's store (paid to its till, nothing to the Country) and hemp from the grain farms', conserved, and fish off
  again the Country's comfrey; the crew scales on five hands. `farm_people_behaviors` (1,751 checks): 78 hands placed; each
  grain farm's six posts all of the fields and mills, its five hands held. `test_econ_report` (4 tests): the stock need,
  the pooled farms, the meal that spoiled, the damping and `--goods`.

Next: Phase 1's 30-day smoke folds into Phase 2's first 3-year run (section 5), the user's go-ahead first.
