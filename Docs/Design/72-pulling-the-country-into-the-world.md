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
   then Phase 4 waits for Phase 6. **Taken (2026-11-03)**, the runs having shown the Country short (country3's year 3):
   built on branch `r2-country-p3b`, sized for Phase 4 too (Progress, "The farms' land rent and the quarry").
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

## Siting (2026-11-03)

The user, 2026-11-03, on this doc: build Phases 1-5 in order; each works pays its crew and sends what is over to the
House whose land it is on (question 1); **Timberland holds the woods** (question 11); **peat yes**, as a second fuel
(question 3); the other questions take the recommendations. And: "erase all of the previous empty 'towns' that we
placed on the map unless they're needed as a community hub for any of these things ... find natural places for all of
these things and then place communities near them that make sense, and then build those communities out into towns."

**How it was judged.** The realm was laid again from its seed (`realm2.Realm2(...).build()`, DEV read only, nothing
written) and its tiles read round each candidate (a 60-tile box: trees, cedar, pine, broadleaf, bog = mud, bog water
and reeds, rock = bare rock, scree, rubble, boulders, sandstone; the biomes' shares). **Walks are measured**, not
estimated: realm2_places' own footpath route (`route` over `route_cost`, roads at 0.3, unwalkable dry ground 3), from
the community's centre or the city's gate (East 776,250; South 412,776) to the works' centre. They replace section 4's
estimates (most were close; the clay pits' and the Front camp's were not). Scripts: the session's scratchpad
`siting/` (`dump2.py`, `comp.py`, `walks.py`).

### The works

| Phase | Works | Site | Ground (60-tile box) | Crew | Lives in | Walk |
|---|---|---|---|---|---|---|
| 2 | Rainwood logging camp | `timber_rainwood` (652,1092), as built | rainforest: 35% standing trees, cedar 18%, fir and pine 13% | 1 + 2 | **Ridgemere** | 458 (South Gate) |
| 2 | **Oakhaven cut** (logging camp) | **new** `timber_oakhaven`, about (1056,1056), size 24 | the densest stand near the Sound's head: 46% trees, cedar 23%, fir and pine 17%; clear of `range_rainrun` (101 past its edge) | 1 + 3 | **Oakhaven** | 179 |
| 2 | Rainwood coppice | **new** `coppice_rainwood`, about (620,1040), size ~24, beside the Rainwood cut (37 past its edge), on realm ground | rainforest with alder and other broadleaf (7%, the most within 800 of a gate): alder and hazel stools on the cut-over | 1 + 2 | **Ridgemere** | 368 (South Gate) |
| 2 | Greenholt coppice and charcoal hearth | `charcoal_greenholt` (1260,2124), as built | the Greenholt's oak and beech woodland: broadleaf 17% (charcoal wants hardwood) | 1 + 2 | **Larkfield** | 537 |
| 2 | Fens peat cutting | **new** `peat_fens`, about (800,3616), size ~30, behind the dunes south of Fenshore (64 past the warren's edge) | open bog: 64% mud, bog water and reeds, marsh 82%, at sea level | 1 + 1 | **Fenshore** | 173 |
| 3 | Granite quarry (Greatly) | `quarry_granite` (1028,356), as built | the Rainward Hills' granite under pine: rock 11%, montane, height ~6 | 1 + 2 | **Ridgemere** | 335 (East Gate) |
| 3 | Mirebridge clay pit | `clay_mirebridge` (1180,3276), as built | river clay in the fens: bog 51%, water 8% | 1 hand | **Fenshore** | 611 |
| 3 | Fens clay pit | `clay_fens` (1636,3332), as built | the fens' east edge: bog 38%, water 8% | 1 hand | **Southmeadow** | 575 |
| 5 | soap boiler and chandler | in **Coldwater** | the herds' village: the sheep runs', piggeries' crews live there; the farms' tallow | ~4 | Coldwater | 0 |
| 5 | weavers' cottages | **Coldwater** (wool: the two sheep runs' village) and **Larkfield** (linen: the grain farms' flax) | | ~5 + 5 | Coldwater, Larkfield | 0 |
| 5 | village potteries | **Fenshore** (clay from `clay_mirebridge`, peat for the kiln) and **Southmeadow** (clay from `clay_fens`) | | ~2 + 2 | Fenshore, Southmeadow | 0 |
| 5 | country presses and paper mills | **Larkfield** (on the Long River) and **Oakhaven** (where the Rainrun and the Alder reach the Sound: the one nearer the city, doc 71's "on the plains' rivers, one nearer the city") | | ~5 + 5 | Larkfield, Oakhaven | 0 |
| 5 | village smithy | **Larkfield** (the grain farms' village) | | 1 | Larkfield | 0 |

**What changed from section 4**, and why:

- **The second logging camp is the new Oakhaven cut, not `timber_front`.** Measured, Coldwater to the Kestrel pines is
  **1,081**, over the 1,047 accepted; Frosthollow 745 and Oakhaven 996. `timber_sound` is no better: the Sound lies
  between it and everything (Oakhaven 869, the city 1,652). The stand above Oakhaven is the densest standing timber
  within 550 of the Sound's head and 179 from Oakhaven. It takes the bigger crew (1 + 3; the Rainwood camp 1 + 2), so
  the fuel's crew is the same 15. `timber_front`, `timber_upper`, `timber_sound` and `timber_coast` stay reserved.
- **The Rainwood coppice is a new site beside the cut**, as question 5 recommended (`range_city` untouched). The
  Rainwood is cedar and fir, which don't coppice; the site is where its alder is thickest near a gate, and the
  coppice is alder and hazel on the cut-over ground. The hardwood charcoal stays in the Greenholt's broadleaf.
- **The peat is at Fenshore's door**: the most open bog near a home (the bogs north of the delta, ~(1056,3168), are
  ~860 from both Fenshore and Mirebridge).
- **The clay pits keep their homes** (Fenshore, Southmeadow), at the measured 611 and 575 (the doc said ~440 and ~600).
  Mirebridge would be no nearer either (648 and 1,464).
- **Phase 5's homes**: the potteries are two (doc 71), one at each pit's community; the presses and paper mills are two,
  Larkfield's and Oakhaven's. Coldwater +9, Larkfield +11, Oakhaven +5, Fenshore +2, Southmeadow +2: 29.

**The beds.** Phases 2-3 put 9 in Ridgemere (city households of their class, doc 68's arrivals' rule), 4 in Oakhaven,
3 in Larkfield, 3 in Fenshore (peat 2, clay 1), 1 in Southmeadow. Coldwater takes none until Phase 5.

### The town sites: erased and kept

The realm has eight `town` sites: the five communities (Coldwater, Larkfield, Southmeadow, Saltwell on reserved town
sites; Fenshore, its own), and three empty: Oakhaven, Frosthollow, Mirebridge. No other leftover town ground.

| Site | Decision | Why |
|---|---|---|
| **Oakhaven** (1012,1220) | **kept: a sixth community**, the woods' | Home of the Oakhaven cut's crew (179) and, at Phase 5, a press and paper mill. Without it the second camp has no home within 1,047 but the city at 794, at the edge of the 800 rule. It is the nearest of all the realm's homes to the Rainwood's other reserved sites (`charcoal_rainwood` 510, `timber_sound` 869, the city 731 and 1,652), so the woods can grow there; and the pool's 25 loggers and 3 sawmill people are Oakhaven's by their old trade, so they can follow their town's name. |
| **Mirebridge** (1468,2868) | **erased** | Serves nothing nearer than an existing community: the clay pits are 648 and 1,464 from it (Fenshore 611, Southmeadow 575), the fens' bogs ~870. |
| **Frosthollow** (2308,796) | **erased** | Serves only iron and lime, which stay the Country's. It is the nearest home to `timber_front` (745) and `quarry_lime` (794), but neither is built now. It can be placed again (the same high valley) with the iron (4.7). |

**Erasing a site** (Phase 2's realm change): the site leaves `SITES`, its cleared level ground goes back to the country
round it (the region's own ground and trees, as `lay_ground` paints it), and its cells lose "A level stretch of cleared
ground here waits for a town". **The roads stay**: the Pass Road, the Pass, the High Road, the Coast Road and the Fen
Road keep their routes, ending at the same points (each road's end given as the old site's (x,y) instead of its id), so
the junctions are crossroads in the open where the towns were.

**Oakhaven, the community** (as `realm2_places.COMMUNITIES`, sized like Southmeadow and Fenshore): **3 cottages** (18
beds), **a store** with the keeper's flat, **an alehouse** with its keeper's flat (about 25 beds in all), kitchen
gardens, hens and goats; `country` style (timber under cedar shingle); a footpath to the Oakhaven cut; **room to grow
of about 8 cottage lots** for the press and paper mill (Phase 5) and the woods' families. Its market posts (shopkeeper,
alehouse keeper) are placed from the pool as the others' were. It stays a region under Phase 0's `setupTowns` rule,
like the five.

## Progress

### The map (2026-11-03, branch `r2-country-map`)

Places only, as the siting says: no posts, goods or people for the works yet (Phases 2-3 add them).

- **Erased** (`realm2.SITES`): Frosthollow and Mirebridge. Their cleared, levelled pads are the country's ground again
  (the region's grass, trees and heights as `lay_ground` laid them), and their cells no longer say a town waits there
  (`w_ridgemere_9_3` is now "The Greenholt, North-east", `w_ridgemere_5_11` "The Sallow Fens, North-east"). The roads
  keep their ends at the old towns' points (`realm2.FROSTHOLLOW` 2308,796 and `MIREBRIDGE` 1468,2868), so the Pass
  Road, the Pass and the High Road meet at a crossroads in the open, and the Coast and Fen Roads at a junction. The
  Coast and Fen Roads are laid exactly as before; near Frosthollow the Pass Road, the Pass and the iron mine's track
  move a few tiles, and a 250-tile stretch of the High Road, 300-560 tiles south of the old town, now runs 16 tiles
  to one side (the route search breaks a tie the other way). No other site moves: the realm was laid twice from DEV,
  the old generator's laying matching DEV tile for tile (0 differ) and the new one's every site, steading, farm,
  community and footpath of the five as they were (`communities.json`'s farms and five communities unchanged).
- **New sites** (at the end of `SITES`, so nothing before them moves; each within 50 tiles of where its ground was
  measured, with a track to the nearest road):

  | Site | Kind | Where | Tiles | The ground |
  |---|---|---|---|---|
  | `timber_oakhaven`, the Oakhaven cut | timber | 1052, 1060 | 1,808 | a cutting's pad (stumps, ferns, logs), as the five cuts |
  | `coppice_rainwood`, the Rainwood coppice | coppice (new kind) | 620, 1020 | 1,792 | not levelled: the conifers gone, alder and hazel stools (shrub, 40%), an oak or a willow left standing, ferns and grass |
  | `peat_fens`, the Fenshore peat cutting | peat (new kind) | 788, 3596 | 2,629 | levelled at the bog's 0.5: cut banks of mud and earth, reeds at the edge, its pools left as they were |

- **Oakhaven, the sixth community** (`realm2_places.COMMUNITIES`), on its town site at 1012,1220 in Southmeadow's and
  Fenshore's size: 3 cottages, Oakhaven Stores with the keeper's flat (4 beds), The Felled Cedar alehouse with its
  keeper's rooms (3): **25 beds**; 2 kitchen gardens, a hen run, a goat paddock; `country` look; **room to grow: 9
  cottage lots**. Interiors `cn_oakhaven_*` (7 rooms, 7 doors). Its region is `oakhaven`, kept plain like the five
  (`town` false); having no farm, its market is not staffed yet (a market is staffed with its community's first farm).
- **The works in the contract** (`communities.json` `works`, new): each of the siting's eight sites, its crew's home and,
  for a community's, the footpath's walk, laid after the communities so a path into a village adds no street and
  moves no house: Oakhaven to its cut 175, Larkfield to the Greenholt hearths 537, Fenshore to the peat 360 (the bog's
  creeks send it round; the siting's straight-line route was 173) and to the Mirebridge pits 606, Southmeadow to the
  Sallow pits 576. Ridgemere's three (the Rainwood cut and coppice, the granite quarry) have none.

**Written into DEV.** `python3 -m worldgen.realm2 write --world dev` (all 22 checks pass; the check of the town sites is
now "six town sites, each a community"), which backed up first: `artifacts/backups/ratw_dev_20261103-153309.pgdump` and
`upper_accord_dev_r75_20261103-153309.atlas.json` (DEV at revision 75; copies in the main checkout's
`artifacts/backups/`). The same map, its new layout recorded: **revision 76** (77 after the editors' reload); 16,527
tiles' codes and 8,061 heights changed, none in the city's nine cells. Then `world_build.py dev`: **build 60**, 1,181
places; `world_check` of its export loads all 1,181 with no failures; a scratch server of build 60 starts and serves.

**Tests.** `test_realm2` with `RATW_SLOW=1`, 28 of 28: new `test_doc_72s_works_and_the_erased_towns`; the communities' test
counts six. `farm_people_tests`: six communities read, the six kept plain, a store's own sales checked where the
community has a farm. A build of `build-core` and `ctest`: 78 of 78. `test_country_industry`, `test_farms_summary`,
`test_world_build`, `test_terrain_catalog` pass.

**Previews:** `artifacts/realm2/siting/before/` and `after/`: the overview and, a pixel or more a tile,
`frosthollow-erased`, `mirebridge-erased`, `oakhaven-and-its-cut`, `rainwood-coppice-and-cut`, `fenshore-and-the-peat`.
The write's own previews in `artifacts/realm2/` (the close-up `whitefangs-and-frosthollow` is now
`whitefangs-and-the-pass`) and `communities/oakhaven.png`; `communities-and-farms.png` draws the works as diamonds.

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

### Phase 0's rest and Phase 2, the fuel: the works, the estates' rent, peat; and Phase 1's tuning (2026-11-03, branch `r2-country-p2`)

Branch `r2-country-p2`, from `ridgemere2` (98ecd01). The user's decisions (2026-11-03): each works pays its own crew at
the farms' fixed wages (16p a keeper, 10p a hand) and sends what is over to the noble House whose land it is on; at first
one purse, the Country's (`outside:country`), named "the Houses' estates and the sea trade" in the reports, every rent
booked by House; **House Timberland holds the woods**; **peat yes**, as a second fuel in place of part of the firewood,
"so long as we find a good way to integrate it". **DEV was not written** (no map change: the works' sites are build 60's);
**no NPC was made**; one 30-day smoke run (below).

**Works as areas (Phase 0).** A `country_farms.json` area with `works: true` holds works, not food farms
(`Core/RatwFarms.cpp`, `RatwItems`): the same settings (yield, crew and wage scales, per area and per works), the same
till, virtual crew, founding (float two days of wages, the crew's purse, a week's opening stock) and switch (the schedule's
`order`, now ending in `fuel`, or the area's own switch), the same store draws (its goods leave `country_off` and the
Country sends them again when it is off). What nothing assumed about food: the Country's `table` (a works has none), the
season groups (the woods' goods are in none: the season's price factor 1, no cap), the stew, and spoiling (none of these
goods `keeps`: a works' store fills to the till's 10,000 cap, then "spoils" as store-full). Two differences:

- **The estates' rent** (`Society::payRents`, after the farms' wages at 18:00): what a works' till holds over its float is
  shifted to the Country's purse, "an estate's rent: House Timberland", and counted by works and by House
  (`LoopDay::Farm::rent`, `LoopDay::rents`). A works short of its wages pays none (the treasury's wage support as for a
  farm). A works buys **no basket** (`farmsBuy` skips it), and the works' areas are left out of the Country's basket's
  shift (`countryBasketShift`: their goods were never in its basket, and the rent keeps its purse).
- **The House**: an area's `house` (the fuel's: Timberland), a works' own `house` over it.

**The Country's lines.** Its wage rules are named (`country.json` `wages.rules[].name`: "the sea trade", "the Houses'
people", "the Houses' heads"; the treasury's two too), and the economy line carries the Country by its parts:
`country_rents` and `country_rents_by_house`, `country_sales` (its own goods' store draws after the toll: the sea trade
and what it still sends), `country_posts_owed`/`_paid`, and `country_posts_by_rule` with `country_posts_held`. Each farm
and area gains `rent`; the full level's `farm` table a `rent` column, its meta each farm's `house` and the `works_areas`.
`tools/econ_report.py` reports the Country's section ("In: its goods to the shops ..., the estates' rents ... (House
Timberland ...). Out: its posts ... by rule ...; its basket"), marks the works' area in the farms' table with a rent
column, and lists the works by name with their House. Atlas's and the DM's Economy panel (`FarmsPanel`, "The Country's
farms and works") lists the fuel area beside the farms, with its works, its House and the rent, editable as the farms'
(`studio.py` `farms_view` gives `works` and `house`). **Not done:** the posts counted by House (they are counted by rule).

**The fuel's works (4.3 as re-sited).** `country_farms.json`'s area `fuel` ("the woods and peat"; goods timber, firewood,
oak bark, resin, charcoal, peat), its five works on House Timberland's land, 5 keepers and 10 hands, 180p a day:

| Works | Kind | Crew, lives in | Yields (Spring / Summer / Autumn / Winter middles, a day) |
|---|---|---|---|
| `logging_rainwood` (the Rainwood cut) | logging camp | 1 + 2, Ridgemere | timber 27.1 / 23.3 / 24.0 / 30.3; firewood 54.4 / 62.8 / 56.1 / 90.0; oak bark 6.0 / 5.3 / 5.3 / 6.2; resin 0.4 / 0.25 / 0.25 / 0.4 |
| `logging_oakhaven` (the Oakhaven cut) | logging camp | 1 + 3, Oakhaven | timber, firewood and bark as the Rainwood camp's |
| `coppice_rainwood` (the Rainwood coppice) | coppice | 1 + 2, Ridgemere | firewood 92.4 / 106.8 / 95.3 / 153.0 |
| `coppice_greenholt` (the Greenholt hearths) | coppice and charcoal hearth | 1 + 2, Larkfield | firewood 70.7 / 81.7 / 72.9 / 117.0; charcoal 10.1 / 8.6 / 9.0 / 11.6 |
| `peat_fens` (the Fenshore peat cutting) | peat cutting | 1 + 1, Fenshore | peat 79.6 / 113.2 / 80.1 / 252.0 |

- **Sized** on country1 (years 2-3, `artifacts/economy/r2/country1.full.jsonl.gz`): the Country's firewood to the shops
  386 / 390 / 392 / 589 a day (season means), of which the households bought 201 / 200 / 201 / 400; timber 52 / 44 / 46 /
  58, bark 11, charcoal 10, resin 0.2. **Peat is the households' half of their fuel**: the need is "any firewood or peat"
  (`crafts.json`), and a household takes, each day, one of the goods its shops hold, by a fixed lot (`householdsShop`):
  half and half when both are held, the other when one is out. So peat is sized on half the households' fuel and
  firewood on the rest (the bakeries', smokehouses', brickworks' and the other makers' firewood is firewood). Each +5%,
  and the middles solved so that each season's **blended** mean meets its demand (the yields blend between season
  middles, while winter's doubled need is a step): played day by day through country1's two years, twice, from a
  founding on spring's first day, the stock **never runs short** (its least 2,519 firewood and 1,154 peat, the winter
  peak covered by what the late-autumn blend lays by). With no slack it would run short in a late autumn: the 5% is the
  margin. The firewood split: the Rainwood coppice 34%, the Greenholt 26%, each logging camp 20%; the camps' timber and
  bark half each; the resin the Rainwood's (conifers), the charcoal the Greenholt's (hardwood).
- **Peat in the shops**: the general store and the provisioner supply peat (`crafts.json` `supplies`), so it is drawn
  from the peat cutting with the fuel area on, and from the Country (as firewood is) with it off. Nothing else changed:
  the households' choice already took either. (`Tests/fixtures/business_labels.json`: the Sump general store's resale
  list with peat.)
- **Their people** (`Core/RatwFarmPeople.cpp`, `farm_people.json`'s kinds `logging camp`, `coppice`, `coppice and
  charcoal hearth`, `peat cutting`): the farms' rule, the woods' old trade first (the peat's: those who keep the house),
  then the fallback. A works has no farmhouse: its keeper is chosen of those **with no family among the unplaced first**
  (4.8, question 12) and comes alone, living with the hands: in the works' community, or for the Rainwood's two in the
  city with a household of their class (judged near from the city's farms' work rooms, the works' own cell being the
  realm's region). Their posts are **out of doors**: `communities.json`'s works give each built works its id
  (`works`) and its `workplace`, the six tiles of open ground (not solid, not a way) nearest its centre in its centre's
  cell, at least 3 apart (`realm2_places.work_spots`, which writes them; checked against build 60's tiles); a post in
  that cell is the works' only by its labels ("fells timber at the Oakhaven cut", "logger at ...", "woodward of the
  Rainwood coppice", "cuts wood at ...", "burns charcoal at the Greenholt hearths", "cuts peat at the Fenshore cutting",
  "peat cutter at ..."; none a business's word, at most 40 characters). **Oakhaven's market is staffed with its works**
  (a community's market with its first farm or works). The pool had enough of every trade: **no NPC was made**.

**Phase 1's tuning** (`artifacts/economy/r2/country1-report.md`, `country1.patch.json`):

- The patch applied **half way** (`econ_report.py --apply --damping 0.5`, 103 yields) to every good but milk.
- **Milk** half way back up from Phase 1's cut: each dairy's four values the mean of now and before Phase 1 (the three
  dairies ~107-123 a day, from 103-117; before Phase 1 112-129).
- **Flax: short and piling were two times, not two places.** The grain farms' flax ran out in **year 1's first ~250
  days** (the opening week's stock; the paintworks drew ~10 a day while the Country's basket still bought much paint and
  linseed oil) and **piled from then on** (400 to 820; sold 6.2 / 4.5 / 5.4 / 7.9 a day in years 2-3 against 7.7 / 6.2 /
  7.3 / 9.2 grown). The patch took the run whole, so year 1's draws short held the yield up. Now each farm's flax is
  **years 2-3's sales + 5%**: 3.54 / 2.57 / 3.08 / 4.51 before the grain area's 92% (the stock already there carries
  the first weeks).
- **The vineyards (84-97% of their wages paid, the treasury's support the rest): the cause is the wine's sales path.**
  Four of the city's five wine sellers (the press and three breweries) press their own wine from grapes (16 grapes and a
  barrel for 8 bottles); only the vintner sells the vineyards' casks on, about 5 bottles a day of the ~23 the
  households drink. So a vineyard's takings are its grapes, all sold (~17.5 a day at ~1.5p to its till), and ~2.5
  bottles (6.3p): **~42p a day against a 1 + 3 crew's 46p**, its wine piling (764 to 1,577 in a year; more grown than the
  vintner draws). Raising yields sells nothing more. **Fixed: each vineyard's crew 1 + 2** (36p: a vine hand fewer; its
  cellar hand stays), its float 72p, its crew's purse 178p (`communities.json`: Southmeadow's and Saltwell's hands 8 and
  5), and its wine half way to the patch's (sold, not piled). The other way, for the user: wine a season group with
  `stores`, and the press and breweries `supplies` it, so they sell on the vineyards' casks before pressing their own
  (as the preserver sells the drying houses' stores, doc 71, 2.4); it takes margin from the breweries and grape sales
  from the vineyards, and wants a long run to judge.

**Tests.** `farms_behaviors` (1,384 checks): seven areas (the fuel's works last in the order), 31 farms with 76 hands;
new `theWorks` (the area's data; founded; firewood and peat drawn from the works, nothing to the Country; at 18:00
the crews paid and the rest, over the float, to Timberland's estate, booked by House and in the journal; conserved; no
basket; a works short of its wages pays no rent and has the support; off, the Country's peat and firewood again) and
`firewoodOrPeat` (the households burn peat when no firewood is to be had, firewood when no peat, both when the store
holds both). `farm_people_behaviors` (2,719 checks): 36 places (5 works, no farmhouse, out of doors, six spots), 36
keepers and 86 hands placed, the six markets' 19 posts (Oakhaven's two); new `theWorksCrews` (every works' crew placed;
the woods' 13 posts with all 9 of the woods' trade; the peat's of those who keep the house; each post at its works'
cell, paid by its till; the crews in their community or the city; no family with a keeper; the oldest logger, with a
spouse in the pool, a hand and not a keeper, the spouse left unplaced). `ctest` in `build-core`: **78 of 78**.
`test_econ_report` (5: new the works and the Country), `test_realm2` (new `test_doc_72s_works_ids`; the realm's sites
with `RATW_SLOW=1`; the whole realm's needs DEV's read access, not run here), `test_item_catalog`, `test_farms_summary`,
`test_country_industry`, `test_world_store`, `test_studio` pass; the Editor's typecheck and `economy.test.ts` pass.
(`test_map_editor`'s Greyfen export counts 11 residents against 10: not this branch's.)

**The 30-day smoke** (`world_check` on build 60's export, `--farms 0 --farms-every 1`: every area on a day apart, the
fuel's works from day 6; `--deterministic --threads 4 --economy-level full`; report and patch in
`artifacts/economy/r2/country2-smoke-report.md` and `country2-smoke.patch.json`): money conserved every day; nobody
hungry; all 122 posts of the farms, works and markets held (36 keepers, the works' 15), no virtual crew left. **The works
paid every wage** (4,320p of 4,320p over their 24 days) and sent **~184p a day of rent** to Timberland's estate (each
27-46p a day; 147p a day over the run). **The fuel never ran short**: no draw of firewood, peat, timber, bark or charcoal
refused; the households bought 111.5 firewood and 112.3 peat a day; the works' firewood and peat stores grew in early
spring (the blend from winter's middle: 327 firewood grown a day to 275 sold, 134 peat to 110), as sized. Only resin
fell short of the general store's new shelves (412 draws refused over the run, at 0.4 a day; the patch asks 1.0 in
spring). **The Country** took 1,448p a day for its goods and paid all its 100 posts every night (824p: the sea trade
432p, 54 posts; the Houses' people 336p, 42; the Houses' heads 56p, 4), its purse 583 to 666p (least 163p), its basket
726p a day; the treasury 1,407 to 4,499p (least at night 1,379p). The vineyards still took ~31p a day in these first
weeks (their casks not yet drawn: the vintner's shelf held its founding stock), 87% of their wages paid.

Next: the 3-year full run of Phases 1 and 2 together (section 5's gates; its report's patch for the yields), the user's
go-ahead first.

### Phases 1 and 2's 3-year run (country2, 2026-11-03)

Merged into `ridgemere2` (with two test fixes the whole realm's tests on DEV found: the vineyards' work spots three for
their 1 + 2 crew in `communities.json`, and the hired-hands check's 21, 11, 8, 8, 5; Atlas's farms test counts seven
areas). No map change since build 60, so the run is on `artifacts/economy/r2/export60` (`--farms 0`: an area every 28
days, the fuel's from day 168; `--deterministic --threads 4 --economy-level full`). Report
`artifacts/economy/r2/country2-report.md`, patch `country2.patch.json`, log `country2.out`.

Section 5's checks:

- **Money conserved** every day; **nobody hungry** (the hungriest 4 hours; residents 559 to 730, births 54, deaths 55).
- **The fuel never short for a household** (every day, winter included: no household went without firewood or peat).
  Peat alone was refused to the shops' shelves on 25 days of year 1's autumn (days 226-250, the cutting's opening stock
  gone while the new shelves filled, 2,117 pieces; the households took firewood). Years 2-3: households about 97 firewood
  and 97 peat a day, twice that in winter; with the makers', firewood ~285 a day (380 in winter), peat ~97 (195).
- **Peat sold, but piling**: grown 5-10% over sold every season (the 5% slack plus the blend), its stock 404 to 9,493
  and still rising ~3,400 a year. Firewood level (7,300-9,600 from year 1's end). Timber level (~2,600-3,000). Charcoal
  piling (to 2,380; ~9-11 grown to 6-9 sold). Resin piling at 0.1 a day sold. **Oak bark short**: the works' stock
  empty from day 651, 2,552 tannery draws refused (~10 grown, all sold).
- **Every works paid its crew every day** (156,380p of 156,380p, no support); **rents** 192p a day on the days the area
  was on (163p over the whole run), all booked to House Timberland (each works 33-51p a day).
- **The Country paid its posts on all but 6 days** (1,128p unpaid in all, its purse to 16p at the least, days 230, 321,
  503, 776, 959, 1050); its purse 583 to 608p; its goods 1,041p a day after the toll; its basket 376p a day (country1's
  533p).
- **The treasury over its 200p float every night** (least at night 1,379p, on day 0); 1,407 to 6,020p.
- **Food**: the vineyards paid in full (country1: short); oats still short (~35,000 draws, as country1), milk ~20,000
  (as country1), salt better (43,000 from 54,000); flax now piling (1,016 a year, sold 3.8 a day).
- **The cost of the Country's smaller basket**: the ironworks' iron and the charcoal burner's and foundry's charcoal
  sold to the Country's basket about halved, so the city's short tills rose (the ironworks' shortfall 15,721 to
  24,688p, the charcoal burner's 11,144p), and the treasury's wage support 232 to 263p a day.
- **One works post left empty**: `logging_rainwood` held 2 of its 3 posts for 655 days (a hand gone and never
  replaced); every farm's empty post was filled within 13 days.

### Phase 3, the stone; and country2's carry-overs (2026-11-03, branch `r2-country-p3`)

Branch `r2-country-p3`, from `ridgemere2` (fae8fbf). The user's decisions (2026-11-03): each works pays its own crew (16p
a keeper, 10p a hand) and sends what is over to the House whose land it is on; **stone and clay are House Greatly's**;
the lime quarry waits (limestone and lime stay the Country's). **No map change and nothing written to DEV**: the
quarry's and the pits' sites are build 60's, and their work spots were laid by `realm2_places` from DEV's realm (read
only, `realm2 preview` to a scratch folder) and checked against build 60's exported tiles (all 18 open ground, not
solid, not a way). **No NPC was made**: the pool had the stone cutters and clay diggers. One 30-day smoke.

**The stone's works (4.4 as sited).** `country_farms.json`'s area `stone` ("stone, clay and sand"; `works`, House
Greatly; last in the schedule's `order`), its goods stone, clay and sand, which the Country no longer sends while it is
on (`farmAreaOf`, as every area's):

| Works | Kind | Crew, lives in | Yields (pieces a day, flat across the year) |
|---|---|---|---|
| `quarry_granite` (the Rainward granite quarry) | quarry | 1 + 2, Ridgemere | stone 28.27 |
| `clay_mirebridge` (the Mirebridge clay pits) | clay pit | 0 + 1, Fenshore | clay 30.69, sand 13.5 |
| `clay_fens` (the Sallow clay pits) | clay pit | 0 + 1, Southmeadow | clay 30.69, sand 13.5 |

- **Sized** on country2's years 2-3, the Country's sales to the shops + 5%: stone 26.92 a day (the mason's), clay 58.45
  (brickworks 42.9, pottery 15.3, paintworks 0.3), sand 25.71 (mason 16.2, plasterworks 5.8, glassworks 3.7); flat (the
  seasons were within 10% of the mean). The two pits alike, each half the clay and half the sand (the game's pit gives
  both). 1 keeper and 4 hands, 56p a day; floats 72p and 20p each; the crews' purses 57p a head.
- **A works of one hand and no keeper** works in the code as it was: no keeper's post wanted, its hand placed by
  `staffFarms`' hands pass, paid by its till (a virtual hand's wage to its crew's household until then), its yield in
  full (`working()` 1), its rent what is over its 20p float (`farms_behaviors`' `theStoneWorks`).
- **Their people**: `farm_people.json`'s kinds `quarry` (keeper "quarrymaster at the Rainward quarry", hands "cuts stone
  at ...") and `clay pit` ("digs clay at the Mirebridge pits", "... the Sallow pits"), both of the **mines, stone and
  clay** (Greyscar's stone cutters for the quarry, Clayholt's diggers for the pits, by their words), by the farms'
  rule; the quarry's keeper of those with no family among the unplaced first. `communities.json`'s works give each its
  id and six work spots (`realm2_places.WORKS_IDS`); nothing else in the contract changed.

**Carry-over 1: the Country pays its posts first.** In country2 it missed them on 6 days (1,128p unpaid): each time two
lean days running (sales ~430p and ~350p, every 91 days) after a day its basket had spent down to its reserve plus one
night's posts. Now it keeps **`countryPostsDays` nights** of its posts' wages over its reserve before its basket and
its patrons buy (a new DM rule, default **2**, 1 to 30: `country.json`, Core's `Rules`, Atlas's and the DM's Economy
rules, `map_editor.py`; no migration, `live.economy.rules` is by name). After tonight's wages it holds the next night's,
whatever comes in. `country_behaviors`' `itsPostsFirst`: two nights kept before the wages (one, as it was, spends
more), tonight's paid in full and the next night's in the purse after them.

**Carry-over 2: a works' post is filled again.** The cause: every way an empty post is filled (a local out of work, the
best-trained, the city's wolves, an arrival from the Country and its lodgings) asks `CareerWorld::near(work, home)`,
which is "the same region"; a works' post is out of doors in its cell, whose region is the realm's own (`cedar_sound`,
`sallow_fens`, ...), near no home. So a works' post, once empty, stayed empty (a farm's work room is in its community's
or the city's region, which is why the farms' refilled). Now `Society::reachOf` judges a works' post from its crew's
home ground, as `staffFarms` places them: a home of its community, or, for the Rainwood's two and the quarry, a city
farm's work room. `farm_people_behaviors`' `aWorksPostFilledAgain`: a Rainwood logger and the Sallow pits' digger die;
both posts are filled again, the new hands living with their crews; without the change neither is in 28 days. (The
test's lodgings are now the city's only, as the world's are, by region.) The review: an arrival's home, the smallest
household of its class within reach, could be a farmhouse (a keeper and a family hand are a "lower" household, and
smaller than the cottages), against `staffFarms`' rule that a farmhouse is its keeper's family's alone; `arrivalFor`
now passes over farmhouses (`fs_`), as `staffFarms` does.

**Carry-over 3: the yields.** From country2's patch (`country2.patch.json`), as the user asked:

- **peat** 8% under now in every season (not the patch's spring raise): 73.25 / 104.16 / 73.66 / 231.8;
- **charcoal 25% under** (7.56 / 6.46 / 6.77 / 8.66), rather than letting the city's charcoal burner resell the
  hearths' charcoal. Its buyers (the smithy, printing house, scriptorium) draw it from the store room; the burner's
  only buyer is the Country's basket. A resale would put a middleman between the hearths and the same shops (the
  `suppliers` rule: the shops pay the burner's price for the hearths' sack) and still leave the hearths' surplus
  unsold; the hearths grew 9-11 a day to 6-9 sold, piling to 2,380. The burner wants a buyer, not stock (carry-over 4).
- **resin** the patch's 0.07 / 0.08 / 0.09 / 0.12 (0.1 a day sold); **oak bark** 20% over now at both camps (7.19 / 6.3
  / 6.36 / 7.44); **flax** the patch's cut (2.79 / 1.92 / 1.79 / 2.75 at each grain farm);
- every other good of the patch **half way** (`econ_report --apply --damping 0.5`: 103 yields).

**Oats, milk and salt, short three runs running: the causes.** Measured on country2 (years 2-3):

- **No household went without.** The draws short are the makers' shelves, not wants: the inns' porridge (an oat and a
  milk make 4; the inns draw 99.9% of both) and the provisioners', butchers', fishmongers' and smokehouses' salt pork
  and salt fish (all the salt). No household wanted porridge, salt pork or salt fish and found none; porridge sold out
  at about 2 of the 11 inns a day, chiefly the day before a Restday (the buying ahead).
- **Supply-bound, so measured on the supply.** The farms of oats held none at the day's end on 64-85% of the days of
  each season, salt's on 58-100% of summer, autumn and winter, milk's on 42-56%. What sold was then what grew less the
  Country's table, so the patch (what was taken + 5%) asked a few percent more, and `--apply`'s half step half of that:
  the shortfall shrank a little each run and never closed. **Fixed in the tool**: `econ_report.py` marks a good-season
  **supply-bound** (its farms out on `BOUND_SHARE`, 40%, of its days while the draws fell short), lists it in the
  patch's `whole` (its seasons in `wholeSeasons`), and `--apply` takes its whole step in the seasons it was bound; its shelves' needs count even where some spoiled
  (`test_econ_report`'s `test_supply_bound`). On country2 (`country2-fixed-report.md`, `country2-fixed.patch.json`):
  milk, oak bark, oats and salt. Applied: **oats** 55.95 / 64.58 / 65.5 / 60.38 at each grain farm (6.5-9.6% up,
  bound in every season), **salt** 47.35 / 45.59 / 44.65 / 46.98 at each salt works (bound in summer, autumn and
  winter, those the whole step: summer and autumn up 9-12%, the yields' season shape was the last runs' supply-capped
  sales, so the seasons the salters lay by for winter stayed lowest; spring, not bound, half way). (The review: the
  first `whole` was by good, so salt's spring took the whole step too, 47.89; now by season.)
- **Milk spoils and runs short in the same week**: it keeps a day, and the inns draw about half on a Restday, so the
  next morning the farms' stores spoil the Restday's milk (~19 a day, a year's mean) while the other six days the inns
  are short. The patch, counting what sold, asked for *less*. Milk's only use is the porridge, a milk to an oat:
  **milk is raised with the oats it is cooked with** (each season by the oats' rise: 6.5 / 7.0 / 8.0 / 9.6%; the
  dairies 38-45 a day). (Turning the Restday's spare milk into cheese and butter, which no farm makes now, would end the
  waste: for the user.)
- **The Country's table ate for the works' crews.** It feeds the Country's own people (24 of the estates, 68 workers of
  its other industries: 8-12 oats, 8-13 milk and 10 salt a day in country2); the fuel's 15 and now the stone's 5 are
  those workers, living in the realm and buying in the shops, so they ate twice. `Society::tableShare`: the table times
  the share still in the Country (72 of 92 with both works areas on; `estatePeople` and `otherWorkers` now read by
  Core). `farms_behaviors`' `theTableShare`.
- **What the yields can't settle alone**: households choose food by nourishment for the price among what the shops
  hold, and porridge is the best value, so its sales follow the oats and milk to be had. The next run says whether the
  inns' shelves fill.

**Carry-over 4: the city's makers the Country's basket bought from** (country1 against country2, years 2-3, a day; read
only, nothing changed). The ironworks, the foundry and the charcoal burner sell **only to the Country's basket**:

| Maker | Its sales to the Country, country1 / country2 | Wages owed / paid, country1 / country2 | The treasury's support |
|---|---|---|---|
| ironworks | 37.1 / 18.4p (iron bars 9.6, charcoal 5.8, steel 3.1) | 34.9 / 18.6, 34.9 / 9.3 | 16.3 to 25.6p |
| foundry | 75.9 / 38.4p (handbells 10.2, iron bars 10.1, copper, charcoal, tin, bronze) | 41.4 / 38.4, 26.5 / 18.4 (drifted down) | 2.9 to 7.8p |
| charcoal burner | 15.1 / 6.2p (charcoal) | 14.0 / 6.7, 14.0 / 2.7 | 7.3 to 11.3p |

All three halved with the basket (533p to 376p a day: the woods' takings went to the works and their crews, 163p came
back as rent); their support rose ~18p a day together (26.5 to 44.7p). Phase 3 takes another ~97p of takings from the
Country for ~40p of rent, and posts-first holds back a night more, so the basket thins again. **The levers are the
user's** (none changed): the Country's basket's shares (`countryBasket`: iron bars, steel, charcoal), the treasury's
wage support, or buyers in the realm: the smithy, cooperage and cartwright draw ~38 iron bars a day from the Country's
store room (84% of the city's), which the ironworks makes; a `suppliers` entry (those three from the ironworks first)
is Phase 4's kind of change and would take ~237p a day of the Country's intake.

**Tests.** `farms_behaviors` (1,432 checks): eight areas, the stone's last in the order; new `theStoneWorks` (the area's
data; founded, House Greatly's; the mason's stone and sand from the works, nothing to the Country, its limestone the
Country's; a pit with no keeper pays its hand and sends the rest over its float to Greatly's estate, booked by House;
off, the Country's stone again) and `theTableShare`. `farm_people_behaviors` (3,307 checks): 39 places, 37 keepers and
90 hands; `theWorksCrews` with the stone's crews of their own words; new `aWorksPostFilledAgain`. `country_behaviors`
(648): new `itsPostsFirst`. `ctest` in `build-core`: **78 of 78**. `test_realm2` with `RATW_SLOW=1` against DEV (read
only): 29 of 29 (`test_doc_72s_works_ids`: every sited works built). `test_econ_report` (6), `test_farms_summary`,
`test_country_industry`, `test_item_catalog` and `test_studio` (the farms' eight areas) pass; the Editor's typecheck
and `economy.test.ts` (the new rule) pass. (`test_map_editor`'s and `content.test.mjs`'s Greyfen export counts 11
residents against 10: not this branch's.)

**The 30-day smoke** (`world_check` on build 60's export, `--simulate 0 720 --farms 0 --farms-every 1`: an area a day,
the stone's from day 7; `--deterministic --threads 4 --economy-level full`; `artifacts/economy/r2/country3-smoke-
report.md`, `country3-smoke.patch.json`): money conserved every day; nobody hungry; every works paid its crew every day
(the stone's 1,288p of 1,288p over its 23 days); **the Country paid all its posts every night** (824p of 824p), its
purse 1,375 to 1,478p (**least 906p**; country2's smoke 163p), its basket 585p a day, its rents 177p a day (Timberland
142p, **Greatly 35p**: the quarry 5.7p, its 41.7p of takings against its 36p of wages; each pit ~20p). **No draw of
stone, clay, sand, oats, salt or oak bark refused**, milk's 37 in the month (country2's spring ~190). The smoke's own
patch is a first spring's (shelves still filling: berries, garlic, cider and resin marked supply-bound) and is **not to
be applied**; the next 3-year run's is.

Merged into `ridgemere2` (`ctest` in `build-core` 78 of 78; `test_realm2` with `RATW_SLOW=1` against DEV, read only,
29 of 29; `test_econ_report`, `test_farms_summary`, `test_country_industry`, `test_item_catalog` pass).

### Phases 1-3's 3-year run (country3, 2026-11-03)

The user's go-ahead. On `artifacts/economy/r2/export60` (no map change since build 60), `--simulate 6 26298 --farms 0
--deterministic --threads 4 --economy-level full` (an area every 28 days: the fuel's from day 168, the stone's from day
196). Report `artifacts/economy/r2/country3-report.md`, patch `country3.patch.json`, log `country3.out`. Years 2-3 a
day unless said; country2's beside it.

Section 5's checks:

- **Money conserved** every day; **nobody hungry** (the hungriest 3 hours; residents 559 to 736, births 54, deaths 55).
- **The builders' draws filled**: no draw of stone, clay or sand refused in the whole run (nor limestone or lime, still
  the Country's). But all three **pile**: grown 5-12% over sold (stone 28.3 to ~25.4, clay 61.4 to ~55, sand 27.0 to
  23.7), their stock at the end 2,347, 5,744 and 2,978 and rising (the 5% slack on goods that never spoil).
- **The works pay their crews**: the five fuel works and both clay pits every day (no support); **the quarry on 729 of
  899 days (81%)**: 30,467p of 32,364p, the treasury's support 1,636p, its rent 1.0p a day. Its takings 34.8p against
  36p of wages (the smoke's 41.7p): the stone piling, its price fell. **Fails the 99% gate.**
- **Rents booked**: House Greatly 33.4p a day (each pit 16.7p, the quarry 1p; the smoke's 35p was the quarry's first
  month), House Timberland 146p (country2 189p: the Rainwood cut's third hand back costs 10p a day more, and the timber's
  and firewood's prices fell as they piled).
- **The Country pays its posts: fails.** Every day of years 1-2, then **47 days of year 3 unpaid** (8,964p in all,
  from day 776; its purse to 12p). Posts-first held it through years 1-2 (purse least 480p; country2 missed
  6 days from day 230), but the intake is now under the posts: year 2 its sales 684p + rents 184p = 868p, year 3 639p + 175p =
  **814p against 824p**; its basket fell to 43p and then 15p a day (country2 189p, 177p). Of its lost sales (818p to
  661p): stone 37p, clay 40p and sand 18p (95p, for 33p of Greatly's rent), iron bars 26p (the smithy's draws, its till
  short more often), linen cloth 14p (the flax cut).
- **The treasury over its 200p float every night** (least 1,277p, on day 0); 1,299 to 6,031p. The wage support **322p a
  day** (country2 248p): the inns 58,792p short over the run (46,512p), the ironworks 30,928p (24,688p), the foundry
  16,384p (now in the list), the quarry's 1,636p.
- **The fuel never short for a household**: peat refused to them on 23 days of year 1's autumn (days 214-242, 1,293
  pieces, the cutting's opening stock gone as in country2; they all took firewood instead), none after. **Peat now
  sold over grown** (120.5 grown to ~122 sold a day; its stock 1,294, 1,011, 438 at the years' ends, least 60): the 8%
  cut overshot, and the patch asks spring and autumn back up (99.5 / 102.4 / 105.5 / 203.5). **Firewood, timber and oak
  bark now pile** (firewood to 23,512, timber to 12,769, oak bark to 3,399): the Rainwood cut's third hand is back
  (country2's patch was sized on two of its three), with oak bark's 20% on top. Charcoal still piles, slower (7.4 grown
  to ~5.2 sold, to 1,691; country2 9.8 to 7.1, to 2,380).
- **Oats, milk, salt**: **oats** never refused (country2 17,102 pieces; the farms' stock level at 2,300-3,000);
  **salt** never refused (8,724) but now piling (81 grown to ~68 sold, to 5,873); **milk** still short, 7,528 pieces on
  616 days (10,983 on 649), ~10 a day spoiled (Restday's), supply-bound in spring, summer and autumn.
- **Works vacancies refilled**: no post of any farm or works empty more than 10 days (the Rainwood cut 9; country2 655).

**Verdict: not stable enough for Phase 4.** The Country's own intake no longer covers its posts in year 3, before Phase
4 takes another ~237p a day of it; the quarry can't pay its crew. Carried over: (1) **the second payer** (the user's
question 2, section 3: the Phase 3 row's 1,103p was ~260p over what the run measured) is now needed before Phase 4,
not with it; (2) the quarry: a crew of 1 + 1, or its stone's yield to what sells (the patch's), or its takings by the
mason's price; (3) the patch for the piling non-perishables (stone, clay, sand, salt, timber, firewood, oak bark,
charcoal, flax) and peat back up: `country3.patch.json` (`wholeSeasons`: milk spring-autumn, wine autumn); (4) milk
(cheese and butter, the user's); (5) the city makers' levers (carry-over 4, unchanged; their support up again).

### The farms' land rent and the quarry; country3's patch (2026-11-03, branch `r2-country-p3b`)

Branch `r2-country-p3b`, from `ridgemere2` (1fe8d43). The decision: open question 2's recommendation, now that the runs
show the Country short (country3's year 3: its sales 639p and rents 175p a day, 814p, against its 824p of posts; 47 days
unpaid). **No map change, nothing written to DEV, no NPC made**; the treasury's levers (toll, support, float) untouched.

**The land rent** (`Society::payRents`, after the farms' wages at 18:00). Each food farm that paid its crew in full
tonight pays a percent of its takings since the last night to the House whose land it is on, into the Country's purse
("the Houses' estates and the sea trade"), booked by House; only out of what its till holds over its float, so never
from its wages or its float. Through the day its basket keeps the rent owed so far back with its float and tonight's
wages (`farmsBuy`), so the rent is the share the basket would have spent. A farm short of its wages pays none, and those
takings' rent is let go (`FarmNow::rentTakings`, the base, starts again each night; not saved: a restart loses the day's).
The works pay their estates' rent as before and no land rent. The Houses (each area's `house` in `country_farms.json`,
a farm's own over it):

| Area | House | Why |
|---|---|---|
| fruit (orchards, vines) | **Fell** | fine wines, liquors and fruit goods (doc 67, R27; doc 67's "Fell for fruit and wine land") |
| honey | **Fell** | the hives stand with the orchards, and mead is Fell's kind of liquor |
| fish | **Grayrock** | the wharf and its boats (R28) |
| grain, meat, salt | **the Crown** | no House's trade fits (Greatly builds, Timberland the forges); booked "the Crown", named so in the reports |

**The rule** (DM-tweakable, like `countryPostsDays`: `country.json` `rules`, Core's `Rules`, Atlas's and the DM's
Economy rules, `map_editor.py`, the Editor's model; no migration, `live.economy.rules` is by name): **`landRent` 16**,
percent of a food farm's takings, for every food area without its own (honey, meat); **`landRentFish` 9,
`landRentFruit` 4, `landRentGrain` 28, `landRentSalt` 8**, 0 for none. `Society::landRentPercent` reads `landRent` plus
the area's id (`landRentFruit`), else `landRent`.

**Why those percents.** Each area's is about **half its thinnest farm's margin over its wages in its leanest season**
(country3's year 3, takings less wages over takings): fish 18% (the sea boats' autumn), fruit 9% (the vineyards'
winter), grain 57%, honey 32%, meat 32% (the pig farms' autumn), salt 17% (autumn). So every farm keeps at least half its
basket in its worst season; a flat percent can't, since the thin farms (vineyards, three orchards, salt, the sea boats)
have 9-18% over their wages while the grain and the dairies have 57-70%. Played on country3's year 3 by farm and season
(`size.py` in the session's scratchpad), the rent is **336-347p a day (about 14% of the food farms' ~2,390p of takings)**,
and every farm's takings less wages less rent stay positive in every season: the thinnest vineyard_2's winter 2.3p
(its basket 3.7p to 2.1p), orchard_5's autumn 2.6p, the salt works' autumn 4.2p, the sea boats' autumn 4.6-7.9p.

**The Country with it** (country3's year 3, its intake by season and the rent beside it; Phase 4, 4.5, takes ~187p a day
of craft-input sales):

| Season | Intake now (sales + works' rents) | + land rent | Over its 824p of posts | After Phase 4 (-187p) | Over its posts |
|---|---|---|---|---|---|
| spring | 805 | 1,152 | +328 | 965 | **+141** |
| summer | 772 | 1,108 | +284 | 921 | **+97** |
| autumn | 787 | 1,123 | +299 | 936 | **+112** |
| winter | 894 | 1,241 | +417 | 1,054 | **+230** |

Its worst season (summer) keeps ~97p a day (12% of its posts) over its posts after Phase 4, besides posts-first's two
nights. The quarry's fix adds ~6-8p of Greatly's rent. What moves: ~340p a day from the farms' baskets (carts, barrels,
hoops, tools, rope, nets, building goods) to the Country's purse, which pays its posts and buys back through its own
basket (iron bars, steel, charcoal, paper and the rest the Country's makers sell): the city's makers' sales shift
between the two baskets rather than vanish.

**The quarry: 1 + 1.** Its stone sold at a steady ~1.33p a piece (after the toll) in every season of country3; its takings
followed the mason's demand, 22-29 stone a day: 29.7-36.0p a day in year 3 against a 1 + 2 crew's 36p. The price was
not wrong (the mason's unchanged), the crew was too large: a stone cutter fewer (`country_farms.json`: hands 1, `minHands`
1, its float 52p, its crew's purse 114p), 26p a day, under its leanest season's 29.7p. Its yield is unchanged by the crew
(the patch's half step, below). Its keeper and hand are still of the pool's Greyscar stone cutters.

**country3's patch** applied (`tools/econ_report.py --apply artifacts/economy/r2/country3.patch.json
Data/Economy/country_farms.json --damping 0.5`, its `wholeSeasons` the whole step: milk spring to autumn, wine autumn):
122 yields; peat back up (86.4 / 103.3 / 89.6 / 217.7), the piling stone, clay, sand, salt, timber, firewood, oak bark,
charcoal and flax half way down.

**Reports.** The economy line gains `country_land_rents` and `country_land_rents_by_house`; `econ_report.py`'s Country
line "the farms' land rents 317p (the Crown 262p, House Fell 37p, House Grayrock 18p)" and a table of the food farms'
land rent by farm (House, takings, wages owed, rent, its percent, basket, and what was left over wages and rent); the
farms' table's `rent/farm-day` is a works' rent or a farm's land rent. Atlas's farms panel names each area's House ("land
rent to the Crown").

**Tests.** `farms_behaviors` (1,806 checks): new `theLandRent` (the areas' Houses; the percents, the world's own over
country.json's; 28% of a grain farm's takings owed; its basket bought but kept its float, tonight's wages and the rent;
at 18:00 the crews paid in full, then the rent to the Crown, booked as land rents, not works' rents, journalled
"a land rent: the Crown"; its float whole; conserved; a till 3p over its float and wages pays those 3p and no more; a
farm short of its wages pays none and owes none the next night; a works owes no land rent) and
`theCountryPaysItsPosts` (six dockers, the Country's purse emptied, the food areas on, only the nights run: with no rent
its posts go unpaid, with the rent each night's pays the next night's posts in full); `theStoneWorks` the quarry 1 + 1
and the patched yields; `theTableShare` 73 of 92. `farm_people_behaviors`: 37 keepers and 89 hands. `ctest` in
`build-core`: **78 of 78**. `test_econ_report` (7: new `test_the_land_rent`), `test_farms_summary`,
`test_country_industry`, `test_item_catalog`, `test_studio`, `test_world_store` pass; the Editor's typecheck and
`economy.test.ts` (the five new rules) pass. (`test_map_editor`'s and `content.test.mjs`'s Greyfen export counts 11
residents against 10: not this branch's.)

**The 30-day smoke** (`world_check` on build 60's export, `--simulate 0 720 --farms 0 --farms-every 1 --deterministic
--threads 4 --economy-level full`; `artifacts/economy/r2/country3b-smoke-report.md`, `country3b-smoke.patch.json`, a
first spring's, not to be applied): money conserved every day; nobody hungry; **every farm and works paid its crew in
full** (the quarry 598p of 598p, takings 31.8p a day, its rent 5.8p); **the land rent 317p a day** over the run (the
last three weeks ~340p; a Restday ~100p, the day before one ~530p, the buying ahead), each farm at or under its percent
(the thin ones under: a lean day's rent is what is over its float); every farm's takings less wages less rent positive
(the least: salt 5.0-5.2p, the sea and lake boats 5.8-8.9p, the orchards and vines 17-99p in spring). **The Country
paid all its posts every night** (24,720p of 24,720p), its purse 1,441 to 1,795p (least 1,278p; country3's smoke 906p),
its basket 824p a day (country3's smoke 585p); the treasury 1,264 to 3,362p (least at night 1,248p). The shipwright
and the ironworks left the short tills (the Country's larger basket); the **brickworks (24p a day short) and the
ropewalk (19p) joined them**: the farms' basket buys their bricks and rope, and it is smaller by the rent. Those are the
city makers' levers (carry-over 4, the user's: the Country's basket's shares), not changed here.

Next: the 3-year run (the user's go-ahead first) to judge the rent through the seasons and years 2-3 before Phase 4.
