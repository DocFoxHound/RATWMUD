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
| **The Houses' estates** (until Phase 6) | The land the works stand on is the Houses'; the estates collect the works' rents and pay the Houses' people (the wharf's posts among them). Not a producer any more, a landlord. Split since Phase 6 (Progress, "Phase 6, the estates split"): each House its own purse, Grayrock's the Country's own (the wharf and the sea trade). |
| **Arrivals and the unplaced pool** | People come from beyond the realm (about 4 a year, with up to 30p each). The pool is the city's and the works' labour reserve. |
| **Goods no one in the realm makes** | The ten farm goods no farm grows (brandy, apple brandy, aqua vitae, wheat, butter, cheese, offal, eggs, sheep, sheepskin) could come by sea as imports instead of going without (open question 8). Butter and cheese no longer: since country3b the dairies make them of their spare milk (the user's decision; Progress, "The dairies' butter and cheese"). |

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

### The land rent's 3-year run (country3b, 2026-11-03)

Merged `r2-country-p3b` into `ridgemere2` first (reviewed for money and for rent out of wages: the rent moves by
`shift`, after `payFarms`, from what is over the float, and only from a farm that paid its crew in full; the treasury's
support comes after it and goes only to the short; nothing found). `ctest` in `build-core` 78 of 78. Then the user's
go-ahead: on `artifacts/economy/r2/export60`, `--simulate 6 26298 --farms 0 --deterministic --threads 4
--economy-level full`, as country3. Report `artifacts/economy/r2/country3b-report.md`, patch `country3b.patch.json`,
log `country3b.out`. Year 3 a day unless said; country3's beside it.

- **Money conserved** every day; **nobody hungry** (the hungriest 5 hours, in year 1; 1 in years 2-3; residents 559
  to 735, births 54, deaths 55).
- **The Country pays its posts every day of all three years** (country3: 47 days of year 3 unpaid). Its intake
  **1,176p** a day against 824p (sales 666p, the works' rents 183p, **the land rents 327p**: the Crown 270p, House Fell
  31p, House Grayrock 26p); its leanest season summer, 1,127p (+303p). Its purse **flat to up**: years' ends 1,820,
  1,836, 1,783p, least 1,008p (day 959), year 3's first four weeks' mean 1,673p to its last's 1,794p. What is over its
  two nights of posts goes on its basket: 339p a day (country3 15p).
- **The land rent** as sized: 311p a day over the run, 327p in years 2-3 (the size's 336-347p, on country3's takings),
  each farm at or under its percent (the thin ones under: a lean night's rent is what is over the float). **Every food
  farm paid its crew every day** (100% in all three years), every farm's takings less wages less rent positive: the
  least vineyard_2 4.8p (year 3), salt 8.2-8.6p, the lake boats 7.6p, the orchards 7.5-9.0p.
- **The works**: the five fuel works and both clay pits every day. **The quarry fails again: 88.5% of year 2's days,
  86.3% of year 3's** (22,558p of 23,374p, support 727p). Its stone's price held (1.37p a piece; country3 1.33p), but
  **the mason bought less**: 19.4 stones a day in years 2-3 (country3 24.9), so takings 26.6p against the 1 + 1 crew's
  26p. The cause is the rent: the farms' basket is smaller by it, and buys fewer building goods (mortar from the
  builders' works 20.1 to 14.0 a day, lime 25 to 19), so the mason sells and draws less. Its yield is untouched
  (27.7 grown a day to 19.4 sold: stone to 7,392 and rising).
- **The treasury over its float every night** (least 1,269p, day 0; years 2-3 least 3,414p and 3,785p); 1,291 to
  6,037p, year 3 flat (5,925p to 5,948p, four-week means). **The wage support fell: 247p a day in year 3** (country3
  323p); the tills short 23.2 a day (26.3). The ironworks' shortfall halved (13,222p, 30,928p: the Country's larger
  basket buys its iron), the shipwright and the foundry left the list; the builders' works joined it (14,131p: the
  farms' smaller basket, its mortar), as the smoke's brickworks and ropewalk did.
- **The fuel and the stone: fails.** **Charcoal short**: the farms' stock 0 at every year's end, ~100,000 draws short
  over the run, supply-bound in all four seasons (country3 piled it, 1,691: the patch's half step down met the
  ironworks' larger demand). **Peat piles again** (2,208 then 4,248 at the years' ends; country3 fell to 438): the
  half step up overshot. **Stone, clay and sand pile hard** (7,392, 13,286, 7,294; country3 2,347, 5,744, 2,978: the
  mason's and builders' smaller draws), **firewood** too (28,166; 23,512). Timber now about level (814), oak bark still
  piles (3,584). Salt piles slower (4,382; 5,873).

**Verdict: the Country is stable enough for Phase 4; the stone and the fuel aren't yet.** Phase 4 (4.5, ~187p a day of
craft-input sales) played on year 3: intake 1,176p to ~989p, still **~165p a day (20%) over its posts**; its leanest
season summer ~940p, **~116p (14%) over**; posts-first's two nights besides. What Phase 4 takes comes out of its basket
first (339p to ~150p a day), so the city makers it buys from (iron, charcoal, paper) lose about that much, the
ironworks most. Before Phase 4: (1) **the quarry** to the mason's 19-20 stones a day: a crew of 1 (the keeper's 16p
against ~26p of takings) and its yield to what sells (the patch: 22.1 / 18.0 / 19.6 / 22.7), or the farms' basket's
building goods share back up (the user's lever); (2) **charcoal** up its whole step (supply-bound: the patch's 9.0 /
8.6 / 10.1 / 10.9 on coppice_greenholt) and **peat** half way down again; stone, clay, sand, firewood, oak bark down
(`country3b.patch.json`); (3) the builders' works, brickworks and ropewalk: the city makers' levers (the Country's and
the farms' basket shares), the user's.

### The quarry's buyers, the dairies' butter and cheese; country3b's patch (2026-11-03, branch `r2-country-p3c`)

Branch `r2-country-p3c`, from `ridgemere2` (019c93b). **No map change, nothing written to DEV, no NPC made**; the
treasury's levers (toll, support, float) untouched. Year 3 a day below unless said (`country3`: before the land rent;
`country3b`: with it); the aggregation scripts in the session's scratchpad (`p3c/agg.py`, `basket.py`, `tills.py`).

**country3b's patch** applied (`tools/econ_report.py --apply artifacts/economy/r2/country3b.patch.json
Data/Economy/country_farms.json --damping 0.5`, its `wholeSeasons` the whole step: charcoal, flax, milk and resin, all
four seasons): 122 yields. **Charcoal its whole step** (coppice_greenholt 9.04 / 8.61 / 10.11 / 10.86, supply-bound
with ~100,000 draws short); **peat half way down** (92.18 / 102.56 / 96.38 / 209.19); firewood, oak bark, clay, sand and
salt half way down; the quarry's stone then set as below.

**The quarry: its buyers back, not a smaller crew.** The rent did not make stone dearer (1.37p a piece; country3 1.33p):
it took the mason's buyers. What the baskets bought, a day:

| | farms' basket | its building goods (cut stone, mortar, bricks, lime, roof tiles, rope, whitewash, plaster) | the Country's basket | the mason's stone |
|---|---|---|---|---|
| country3 (no rent) | 1,117p | 323p (28.9%) | 15p | 24.9 |
| country3b (the rent) | 851p (x0.76) | 252p (29.6%): **-71p** | 352p: **+337p** (paper, steel, iron, charcoal...) | 19.4 |

Every good of the farms' basket fell by the same quarter (mortar 28.2 to 22.3 a day, lime 24.9 to 19.5, bricks 13.2 to
10.3, cut stone 12.5 to 9.7, rope 6.9 to 5.4): the shares didn't change, the money did, and it went to the Country's
basket, which bought none of them. Three ways to the quarry's wages:

1. **A keeper alone** (16p against 19.4 x 1.37 = 26.6p): pays, but leaves the mason's buyers, the brickworks (4.9p a
   day short) and the ropewalk (7.3p) where they are, and one man to cut 20-25 stones a day.
2. **The farms' basket re-weighted** (the building goods x1.455, back to 323p of its 851p): the 71p comes from its other
   makers, who lost the same quarter to the rent: the smithies (already 11.3p a day short), the cooperage (5.0p), the
   wheelwright, the carpenters and the tannery.
3. **The Country's basket buys the farms' repairs** (taken): the land rent goes to the Houses whose land the farms are
   on, and the landlord keeps up their buildings. The farms' building goods join `countryBasket` at **0.53 of the farms'
   shares** (cut stone 33, mortar 29, bricks 28, lime 26, roof tiles 21, rope 13, whitewash 11, plaster 11: 172 of its
   858 shares, 20%), so of country3b's 352p basket ~71p, what the rent took from the builders. It comes from the makers
   the rent gave 337p; the farms' other makers keep what they have. The rule "the Country's re-weighted basket buys
   nothing the farms' does" keeps, but for these eight (`farms_behaviors`).

**The quarry stays 1 + 1** (26p a day): at the mason's 24.9 stones a day before the rent, 34p of takings (its leanest
season in country3 29.7p), ~8p over its crew for Greatly's rent. **Its stone 27.76 / 23.93 / 24.63 / 28.09** (a mean of
26.1: the 24.9 + 5%, in country3's patch's seasons), not the patch's 19.4-based half step. The clay pits' patched clay
and sand (2 x 26.9 and 2 x 11.2 a day) already fit the builders' demand before the rent (country3: clay 54.4, sand 23.7).

**Phase 4's caveat.** Phase 4 (4.5) takes the Country's basket from ~339p to ~150p a day; the repairs then fall to ~30p,
and the farms' basket shares (or the repairs' 0.53) want another look.

**The dairies' butter and cheese** (the user's decision, 2026-11-03): **the dairies turn their spare milk into butter
and cheese** (cheese keeps a very long time). Butter and cheese are among the farm goods "no farm grows" (section 6), so
shops that want them can now get them.

- **What spare is.** Milk keeps a day: each morning what a dairy holds over a day's mean yield spoiled. In country3b's
  year 3 that was 7.2 a day across the three dairies, 41.5 of it the morning after a Restday (the inns draw half then),
  while the inns' draws went short the rest of the week (the milk is supply-bound; the patch took its whole step).
- **The making** (`country_farms.json` a farm's `makes`; Core `Society::makeOfSpare`, from `tendFarms`' spoiling): what is
  past keeping goes **half to cheese at 4 milk a piece** (crafts.json's dairy craft) and **the rest to butter at 2**
  (recipes.json's), whole pieces, while the dairy's store room holds under **60 cheese and 10 butter** (about their
  keeping days of a mean day's making at country3b's spare: 180 x 0.3, 14 x 0.6); what neither takes spoils as before.
  One good made into another at the farm: nothing sold, no money, journalled "made into cheese" and "made at <farm>"; the
  made counts as grown, the milk as `used`. The three cattle and dairy farms make them, no other farm.
- **Cheese keeps half a year**: `items.json` keeps 40 to **180** days (a hard cheese; "a very long time", the user).
  Butter keeps its 14 days.
- **Who draws them.** Nobody did: no shop's `supplies` held them (country3b traded none; doc 67's "no dairies for
  now"), so the residents' food and comforts skipped them and the Watch's 0.2 cheese a guard went unbought. Now **the
  stall and the provisioners resell butter and cheese** (`crafts.json` `supplies`; the Round and Rind and Vane &
  Daughters, the old dairy shops, are provisioners), drawn from the dairies' stores like their milk; households eat them
  (cheese 30 nourishment for 5p) and buy cheese as a comfort.
- **The Country stops selling them**: butter and cheese were already the meat area's goods, so with it on only the farms
  have them; it sent none before (nobody drew them), so nothing it sold is lost.

**A poor till draws what it can pay** (the restocking, Core/RatwCityLoop.cpp): a store draw was all or nothing, and a
till that couldn't pay for all its shelf lacked drew nothing, again each hour. The first smoke showed the trap: the
vintner's till, a little poorer on its first day, couldn't pay for its shelf of wine, drew none for the whole month,
sold only cider, lived on the wage support, and **both vineyards went unpaid** (their wine piling, 22 to 108);
ridgemere2's own data did the same with only the patch applied, or only the new `supplies`. Now a draw refused for the
till's cash is tried again at what the till can pay for (its cash over the draw's cost, a piece less for the carriage).
The city then sells 33.4 wine a day, the vintner 26% of it, and the vineyards pay their crews; on ridgemere2's data the change
alone moves little else (the treasury's support of the shops 33p a day either way).

**Reports.** `world_check`'s full level: `farm_good` gains a ninth column, `used` (made into another good at the farm),
and the meta's farms their `makes`. `econ_report.py` reads either width; a line "Made at the farms of what would have
spoiled" (each made good a day, of what, sold, held at the end; the milk used and spoiled); a made good isn't named
among the goods "no farm grows", and gets no yield patch.

**Tests.** `farms_behaviors` (1,898 checks): new `theDairy` (cheese keeps 180 days, butter 14; butter and cheese the meat
area's; the three dairies' `makes`, half to cheese at 4, the rest to butter at 2, stores 60 and 10; a night with no
sales: the spare made into both, about half to each, 4 milk a cheese and 2 a butter, a day's milk kept and a piece
spoiled at most, counted as grown and used; no money made or lost and every piece of milk accounted for; the stall's
cheese and butter drawn from the dairies, nothing to the Country, conserved; 70 nights on, the stores full at 60 and 10
and no fuller, the spare spoiling again; meat off, cheese the Country's again); `spoiling` (spoiled or made);
`theDefaults` (the Country's repairs: only the eight building goods shared with the farms' basket, each 0.53 of the
farms' share, 172 in all; the flour's summer mean 55.47 after the patch); `theStoneWorks` (the quarry's stone 23.93 at
summer's middle, 26.1 a year; the pits' patched clay and sand). `store_room_behaviors` (246): a stall whose till holds
5p draws 1-3 of the 4 flour it lacks, its till spent, conserved; the shared fixture's stall goods with butter and
cheese. `ctest` in `build-core`: **78 of 78**. `test_econ_report` (8: new `test_what_a_farm_makes_of_its_spare`),
`test_city_capacity` (without a dairy and without a vendor no cheese on offer; with the provisioners, cheese is),
`test_farms_summary`, `test_country_industry`, `test_item_catalog`, `test_studio`, `test_world_store`, `test_ridgemere2`
pass.

**The 30-day smoke** (`world_check` on build 60's export, `--simulate 0 720 --farms 0 --farms-every 1 --deterministic
--threads 4 --economy-level full`; `artifacts/economy/r2/country3c-smoke-report.md`, `country3c-smoke.patch.json`, a
first spring's, not to be applied; ridgemere2's data on this build beside it, days 7-29): money conserved every day;
nobody hungry; **every farm and works paid its crew in full**; the Country's posts paid every night (24,720p), its purse
1,459 to 1,859p (least 1,235p); the land rent 317p a day.

- **The builders**: the Country's basket bought 123p a day of the building goods (its basket 831p in a first month, its
  founding purse's; ~350p in year 3, so ~70p there), the farms' 263p as before; **the mason's stone 22.2 to 32.4 a
  day**, his sand 10.0 to 13.5, the brickworks' clay 31.2 to 47.1. Short of their wages (a day over the month, ridgemere2's
  data first): **the brickworks 24.1p to 10.5p** (46 short days to 25; their takings 87p to 133p), **the mason 13.6p
  to 6.7p**, the ropewalk 18.8p to 16.2p; the builders' works worse, 13.2p to 17.4p (it sells the farms planks,
  beams, nails and fittings, none of the repairs' goods: 25.6p to 17.6p a day of them, the farms' basket spread over
  their makers otherwise in a first month); the clay pits' clay and sand no longer piling. The quarry took 46p a day (598p of
  598p in wages, 20p a day of rent to Greatly); this month's 32 stones a day are over its 26.1 (the month's larger
  basket), year 3's ~25 under it.
- **The dairies**: milk spoiled **18.3 a day to 1.4**; 15.7 a day used: **butter 5.6 a day, cheese 1.1, all sold** (the
  stall drew most, the households bought them there), none held at the month's end: supply-bound, the shops' draws
  ~3,600 each short. Butter takes most of the milk: an ordinary morning's spare (2-5 a dairy) is under the 8 a cheese's
  half needs, so it goes to butter; the Restday's 14 a dairy makes both.
- **Still short or piling** in a first month: berries, flax, resin, wine (the vintner's shelf refilled, the vineyards
  out), small pelts; firewood (the two logging camps 519 to 837), apples and salt pile.

Next: the 3-year run (the user's go-ahead first) to see the repairs, the quarry and the dairies through the seasons.

### The repairs' and the dairies' 3-year run (country3c, 2026-11-03)

Merged `r2-country-p3c` into `ridgemere2` first (reviewed for money and goods: the dairies' making moves milk into
cheese and butter by `consume` then `create` in the farm's own till, no money, its pieces bounded by the store's room
and the till's kinds before either, what neither takes spoiling as before; `farm_good`'s `used` balances grown, sold,
eaten, spoiled and the stock by good; the Country's basket 686 + 172 = 858 shares, each repair good 0.53 of the farms';
the poorer till's draw retried only on the cash refusal and at fewer pieces each time; nothing found). `ctest` in
`build-core` 78 of 78; `test_econ_report`, `test_city_capacity` pass. Then the user's go-ahead: on
`artifacts/economy/r2/export60`, `--simulate 6 26298 --farms 0 --deterministic --threads 4 --economy-level full`, as
country3b. Report `artifacts/economy/r2/country3c-report.md`, patch `country3c.patch.json`, log `country3c.out`. Year 3
a day unless said; country3b's beside it.

- **Money conserved** every day; **nobody hungry** (the hungriest 3 hours, years 1-2; 0 in year 3; residents 559 to
  735, births 54, deaths 55).
- **The Country pays its posts every day of all three years.** Its intake **1,099p** a day against 824p (country3b
  1,177p: sales 611p (666p), the works' rents 172p (183p), the land rents 317p (327p)); its leanest season summer,
  **1,030p** (1,127p), +206p. Its purse **flat to up**: years' ends 1,845, 1,811, 1,755p, least 900p (year 3), year 3's
  first four weeks' mean 1,637p to its last's 1,771p. Its basket 263p a day (339p): the repairs it now buys come out of
  what it spent on the makers (its sales smaller too: 55p a day).
- **The farms and works.** Every food farm, the fuel works and both clay pits every day, but **vineyard_2 99.7%** of
  year 3 (one night, 8p of support). **The quarry: 100% of year 2, 93.7% of year 3** (country3b 88.5%, 86.3%; 9,330p
  of 9,490p, support 139p): spring and winter every day, summer 82.6%, autumn 92.3%. **The mason's stone 24.8 a day in
  year 2, 22.2 in year 3** (19.4), the quarry's takings 33.7p and 29.5p (26.6p) against its 26p.
- **The treasury over its float every night** (least 1,304p, day 0; years 2-3 least 3,084p and 4,665p); 1,394 to
  6,023p, year 3 flat (5,929p to 5,947p, four-week means). **The wage support rose: 281p a day in year 3** (country3b
  247p); the tills short 25.5 a day (23.2).
- **The tills short** (year 3, a day): the brickworks **7.2p** (4.9p), the ropewalk 6.6p (7.3p), the mason 1.7p
  (2.5p), the builders' works 10.7p (11.4p); worse, the makers the Country's basket bought less of: **the ironworks
  24.5p** (15.0p, every day but 16), the smithies 16.7p (11.3p), **the charcoal burner 11.2p every day** (8.6p), the
  shipwright 10.5p (7.5p).
- **Charcoal no longer short** (no draw refused in three years; country3b ~100,000) **but piling**: 383, 1,275, 2,418
  at the years' ends, 9.7 grown a day to 6.5 sold (the Country's basket bought 4.8, 7.8 in country3b; the whole step
  up met a smaller demand). **Stone, clay and sand no longer pile hard**: 1,895, 1,350, 674 (7,392, 13,286, 7,294);
  **peat** 3,456 (4,248), **firewood** 16,105 (28,166), oak bark 2,484 (3,584), salt 4,636 (4,382) still pile, slower;
  **timber piles now**, 7,142 (814: the foundry's, the ironworks' and the charcoal burner's smaller draws).
- **The dairies.** **Milk spoiled 0.3 a day** (country3b 7.2); 6.1 made into **butter 2.0 a day and cheese 0.5**, all
  sold, **none held at any year's end** (no cheese piling; its store of 60 never near). The shops want far more: some
  46,600 draws of each refused a year (an hourly retry each, the supply-bound seasons all four), milk's refused draws
  6,731 (5,909). In year 1, before the meat area is on, the Country's store sold the shops ~25 butter and ~26 cheese a
  day (the new `supplies`).

**Verdict: not yet for Phase 4; close.** Phase 4 (~187p a day) played on year 3: intake 1,099p to ~912p, ~88p (11%)
over its posts, but summer's 1,030p to ~843p, **~19p (2%) over**: thinner than country3b's 14% by the repairs' cost to
the Country's sales. Before Phase 4: (1) **the quarry** short in summer and autumn: its yield to what sells
(`country3c.patch.json`) and a crew of 1 + 1 still over the mason's summer draw, or the stone's carriage, the user's;
(2) **charcoal** and **timber** half way down, **peat** and **firewood** down again (the patch); (3) the Country's
basket: the repairs moved ~75p a day from the ironworks, the smithies and the charcoal burner, who are now the short
ones, and the support rose 34p; its repair share (0.53) back down a little, or Phase 4 sized against the Country's
smaller sales; (4) the dairies' butter and cheese are tiny next to the wants: more milk to the making (a larger share
past keeping, or the dairies' yields up) is the user's call.

### Phase 4: the city's makers first; the dairies' rent and their own milk; country3c's patch (2026-11-03, branch `r2-country-p4`)

Branch `r2-country-p4`, from `ridgemere2` (bac0d87). **No map change, nothing written to DEV, no NPC made**; the
treasury's levers (toll, support, float) untouched. Year 3 below is country3c's, a day; the 30-day smokes are on build
60's export as before (`base`: ridgemere2's data on this build). The scripts are in the session's scratchpad (`p4/`:
`p4goods.py`, `project.py`, `moved.py`, `stone.py`, `fuel.py`).

**Phase 4 (4.5): `suppliers` entries only** (`Data/Items/country.json`; `buyFromCity` was already on the craft path, no
code change). A craft input is bought from a city maker's till holding it, at its price, till to till, and from the
store room only what the makers lack:

| Good | Buyers (their crafts) | City makers |
|---|---|---|
| linen cloth | tailor, apothecary, herbalist, armoury, papermill, scriptorium | weaver |
| iron hoops | carpenter, cartwright (cart wheels) | smithy, cooperage |
| planks | cooperage, cartwright, carpenter, apothecary (splints), fletcher | the Timberland Sawmill |
| cord | ropewalk, fletcher, general store, jeweller, tinker (nets, snares, fang charms) | chandlery |
| paper | printing house, cartographer | papermill, scriptorium |
| leather | armoury, printing house, saddlery, scriptorium, smithy | tannery |
| ink | cartographer | scriptorium, printing house |

**What stays the store room's**: the resale goods (Phase 5: the general store's and the builders' yard's nails, the
rag-and-bone man's and the pawnbroker's cookpots, the barber's bandages: in year 3 ~12p a day; 4.5's table counted
them, and they go through the resale path, `!w.craft`), and a maker's input for its own crafts (the chandlery's cord for
its candles, 10.8p: a business is never its own supplier). So of 4.5's ~187p, **what Phase 4 can move in year 3 is
~99-119p a day by season** (the Country's sales of those goods to those buyers), and the smoke moved: hoops, paper,
leather and ink all, planks 75%, cord 75%, **linen 48%** (the weaver's crew of two makes ~10 linen a day of the ~15
wanted; with the grain farms' flax as it was, 18%).

**The weaver's flax**: the grain farms' flax **+12 a day each** (country3c's patch, then +12: 14.9 / 14.2 / 14.1 / 14.9
at the seasons' middles; it was 2-3): the weaver drew 20.4 a day in the smoke (9.5 at +6, 0.1 in year 3), the paintworks
7.7 (it was supply-bound). Flax keeps; the next run's patch sizes it.

**The test** (`trades_tests`, new `thePhase4Suppliers`, 921 checks): each pair above in `citySellers` (the chandlery's own
cord and the resale goods none); a tailor whose shelves lack neckerchiefs and a weaver holding linen: the linen bought
from the weaver's till at the catalogue's price, nothing drawn from the store room, the Country's purse, the treasury and
the carriage untouched (no toll, no carriage), conserved; the weaver out of linen: the store room's, the Country paid.

**The Country's margin: the dairies' land rent** (open question 2's lever, where the margin is). In country3c's year 3
the three cattle and dairy farms kept **67-70%** of their takings over their wages in every season, the meat area's pig
farms beside them 41% (its thinnest), so the area's 16% left the dairies far more than any other farm. A farm can now
have **its own rule**: `country_farms.json` a farm's `landRent` names it (`"dairy"`: the rule **`landRentDairy` 24**), over
its area's (`Society::landRentPercent`: the farm's own, then `landRent<Area>`, then `landRent`; Core's `Rules`, the
Editor's and Atlas's Economy rules, `map_editor.py`; no migration). 24% keeps each dairy ~45% of its takings over its
wages and rent; with their cheese and butter (below) their baskets hardly move (110p to 108p a day in the smoke). The
other rents (grain 28, honey and meat 16, fish 9, fruit 4, salt 8) are each about half their thinnest farm's margin
already and are unchanged; no farm is thinner for it.

**The repair share stays 0.53.** Lowering it moves the Country's basket from the builders to the ironworks and the
smithies, but it is the mason's buyers the quarry needs in summer (below), and Phase 4 with the dairies' rent leaves the
Country's basket about where it was (the smoke: 608p to 588p a day). The ironworks, smithies and charcoal burner stay the
city makers' levers (carry-over 4).

**Projected on country3c's year 3** (intake = sales after the toll + the works' rents + the land rents; Phase 4 = the
Country's year-3 sales of each covered good to its covered buyers times the share the smoke moved; the smithies' iron bars
back from the store for the hoops, a bar making three; the dairies at 24% on their takings plus the smoke's 18p a day each
from their own milk's cheese and butter, against 16% before; the stress column puts 4.5's whole 187p on Phase 4 instead):

| Season | Intake now | Phase 4 | Iron bars back | Dairies' rent | Intake after | Over its 824p of posts | Stress: 187p |
|---|---|---|---|---|---|---|---|
| spring | 1,119 | -80 | +15 | +56 | 1,110 | +286 (35%) | 1,003 (+22%) |
| summer | 1,030 | -68 | +12 | +54 | **1,028** | **+204 (25%)** | **909 (+10%)** |
| autumn | 1,038 | -68 | +13 | +55 | 1,038 | +214 (26%) | 919 (+12%) |
| winter | 1,211 | -82 | +15 | +58 | 1,202 | +378 (46%) | 1,097 (+33%) |

With all of each covered good moved (the weaver keeping up), summer is 997p (+21%). So the worst season keeps at least
10% over its posts in every case (the target), ~25% as the smoke ran. **The treasury loses the toll on what moves**: 3/7
of the Country's intake from it, ~29p a day in year 3's summer (the smoke: 17p a day less toll); its surplus is what
pays for it (year 3: 5,929p to 5,947p, the cap's surplus going out), the user's lever.

**The quarry's summer: the mason's cut stone.** Its stone sold at ~1.34p in every season of country3c; summer's 18.8 a
day was 25.1p against the 1 + 1's 26p: **demand-bound**, so no yield can raise its takings. Its buyers are the mason's cut
stone (two stone a piece) in the two baskets: **cut stone 62 to 80 of the farms' basket's shares, 42 of the Country's**
(0.53 of 80; the repairs 172 to 181 shares). Played on year 3 (the baskets after the dairies' rent and Phase 4, the shares'
weight): cut stone ~11.7 a day in summer, **~23 stone, ~30p against its 26p (+15-19%)**; the smoke's quarry took 55.4p a
day (46.0p before), the mason's stone 39.6 a day (33.0). **Its yield** country3c's patch half way, then ×1.1 for the new
demand: **29.87 / 25.32 / 26.93 / 31.36** (27.76 / 23.93 / 24.63 / 28.09).

**country3c's patch** applied (`tools/econ_report.py --apply artifacts/economy/r2/country3c.patch.json
Data/Economy/country_farms.json --damping 0.5`, `wholeSeasons` the whole step: milk all year, flax all year, clay in
spring): 122 yields; **charcoal half way down** (coppice_greenholt 8.34 / 7.63 / 8.63 / 9.77; 9.04 / 8.61 / 10.11 / 10.86),
**timber half way down** (20.95 / 18.42 / 19.46 / 23.27 at each logging camp; 22.09 / 19.77 / 20.96 / 24.87), oak bark and
the rest. **Peat and firewood** the patch left about level (its demand + 5% over what sold, which piled them: 3,456 and
16,105 at year 3's end), so instead **half way to what sold** in each season, no slack (`fuel.py`): peat 87.34 / 100.15 /
90.03 / 219.20 (92.18 / 102.56 / 96.38 / 209.19: sold over grown in winter, under the rest of the year), firewood 4-6% down
in spring to autumn and ~1% up in winter at all four works.

**The dairies' butter and cheese: milk of their own** (the shops wanted some 64 butter and 79 cheese a day while the
Country's store sold them, year 1's first eight weeks; the spare made 2.0 and 0.5). A farm's `makes` entry gains
**`aDay`**: pieces of its `from` a day at each season's middle, blended as a yield (times the yield scale and the crew at
work), **milked for that making alone and never in the till as milk**, so the shops' milk is the yield's as before;
made each morning into whole pieces while the store has room, and what the store can't take that morning isn't milked
(`Society::makeOfOwn`, after the yields; journalled "made at <farm> of its own milk"; counted grown and used, the made
good grown: `farm_good` balances as before; no money). **Each dairy 20 milk a day for 5 cheese and 8 for 4 butter**, all
year: 15 cheese and 12 butter a day of the three, about a fifth of the wants, besides the spare; **stores 120 cheese** (24
days of it; cheese keeps 180) **and 20 butter** (5 days; it keeps 14). The spare's halves are unchanged.

**Tests.** `farms_behaviors` (1,943 checks): `theDairy` (the stores 120 and 20, the own milk 20 and 8; a night with no
sales: 5 cheese and 4 butter of its own milk, journalled apart, the spare still about half to each, the milk grown the
yield's and 28 more, accounted for, conserved; 70 nights on, full at 120 and 20, the spare spoiling and nothing made);
`theLandRent` (the three dairies' own 24, every other farm its area's; a world's landRentDairy 5 over the meat area's
landRent 10, the pig farm beside it 10); `theDefaults` (the repairs 181 shares, cut stone 42 of 80; the flour's summer
mean 55.17 after country3c's patch); `theStoneWorks` (the quarry 25.32 at summer's middle, 28.37 a year; the pits' clay
25.8 and sand 10.17); `theCountryPaysItsPosts` with landRentDairy off too. `trades_behaviors` (921): `thePhase4Suppliers`.
`ctest` in `build-core`: **78 of 78**. `test_econ_report` (8; the made line now "of what would have spoiled and of their
own milk for it"), `test_farms_summary`, `test_country_industry`, `test_item_catalog`, `test_city_capacity`,
`test_world_store`, `test_studio`, `test_ridgemere2` pass; the Editor's typecheck and `economy.test.ts` pass.
(`test_map_editor`'s and `content.test.mjs`'s Greyfen counts, 11 residents against 10: not this branch's, as before.)

**The 30-day smoke** (`--simulate 0 720 --farms 0 --farms-every 1 --deterministic --threads 4 --economy-level full`;
`artifacts/economy/r2/country4-smoke-report.md`, `country4-smoke.patch.json`, a first spring's, not to be applied; `base`
beside it, days 7-29 a day): money conserved every day; nobody hungry; **every farm and works paid its crew in full**;
the Country's posts paid every night (24,720p), its purse 1,431 to 1,889p (least 1,266p; base 1,235p).

- **Phase 4: 174.5p a day till to till** (base 18.4p, the casks): hoops 36p (the carpenters' and the cartwright's, from
  the smithies and the cooperage), planks 29p (the sawmill: it was 4.8p a day short, now paid with 5.3p over), linen
  54p (the weaver), cord 18p, paper 11p, leather 6p, ink 2p. The Country's sales 909p to 830p, its land rents 338p to 401p, the works'
  rents 247p to 250p: its intake 1,494p to 1,481p (in this first month, its basket's founding purse besides).
- **The makers**: the sawmill and the papermill no longer short, the cooperage's takings 71p to 101p (still 6.3p short),
  the printing house's profit 6.8p to 13.7p, the tannery's 14.2p to 15.7p; **the smithies 21.8p to 25.1p a day short** (their
  hoops are a bar of 9p made into 12p, and the meat farms' baskets bought a little less of their goods), the general
  stores 60.9p to 64.7p. The wage support 391p to 401p a day; the toll 17p a day less (the treasury 3,612p to 2,973p at
  the month's end, least 1,369p on day 0 either way).
- **The dairies**: **butter 16.6 a day, cheese 15.3**, all sold, none held (base 5.3 and 1.2); their takings 201p to 219p
  each, their rent 31.7p to 52.3p, their baskets 110p to 108p. The stall's takings 204p to 294p (its profit 5.7p to
  34.6p), the provisioners' 1,206p to 1,222p. Still supply-bound (~3,600 draws of each short in the month: the wants are
  far larger). The households' food shifts a little from the other farms: the sheep runs' takings 96p to 88p, the pig
  farms' 1-3p less, the boats and salt 0-1p; every farm's takings less wages less rent still positive (the least: the
  sea boats and salt 4.9-5.1p, base 5.0-6.3p).
- **The quarry 55.4p a day** (46.0p), its rent to Greatly 29.4p.

Next: a 3-year run (the user's go-ahead first) to see Phase 4 through the seasons: the Country's summer against the
projection, the quarry's summer on the mason's cut stone, the weaver's linen (its crew), the dairies' cheese stores, the
fuel's piling, and the smithies, the general stores and the support.

### Phase 4's 3-year run (country4, 2026-11-03)

Merged `r2-country-p4` into `ridgemere2` first (reviewed for the suppliers' fall-back, double buys, money and goods, and
the rent: the new `suppliers` lines go through the same `buyFromCity` as the casks, the most-holding seller first, what
the city lacks from the store room, each piece bought once, till to till at the catalogue's price; every buyer and
seller is a business in the catalogue, and no good is bought back by its own seller; the dairies' own milk is counted
grown and used and made into whole pieces only while the store has room, what it can't take not milked, no money; a
farm's own `landRent` rule is only a name, read before its area's, the rent still from what is over its wages and float;
nothing found). `ctest` in `build-core` 78 of 78; `test_econ_report`, `test_farms_summary`, `test_country_industry`,
`test_item_catalog`, `test_city_capacity`, `test_world_store`, `test_studio` pass, the Editor's typecheck and
`economy.test.ts` pass; `test_ridgemere2`'s R35 (the food trades staffed 130% of their keeper-days) and
`test_map_editor`'s Greyfen count fail at `bac0d87` too (not this branch's). Then the user's go-ahead: on
`artifacts/economy/r2/export60`, `--simulate 6 26298 --farms 0 --deterministic --threads 4 --economy-level full`, as
country3c. Report `artifacts/economy/r2/country4-report.md`, patch `country4.patch.json`, log `country4.out`. Year 3 a
day unless said; country3c's beside it.

- **Money conserved** every day; **nobody hungry** (the hungriest 2 hours, year 3; country3c 3 and 0; residents 559 to
  735, births 54, deaths 55).
- **The Country pays its posts every day of all three years.** Its intake **1,086p** a day against 824p (country3c
  1,099p: sales 545p (611p), the works' rents 173p (172p), the land rents 368p (317p: the dairies' 46p each, 29p)); its
  leanest season summer, **1,018p** (1,030p), **+194p, 24% over** (the smoke's projection 1,028p); spring 1,106p,
  autumn 1,035p, winter 1,190p. Its purse **up**: years' ends 1,924, 1,840, 1,832p, least 923p (day 959), year 3's
  first four weeks' mean 1,688p to its last's 1,800p (1,637p to 1,771p). Its basket 248p a day (263p).
- **Every farm and works paid its crew in full every day, but orchard_5 99.5% of year 3** (two nights). **The quarry
  100% of all three years** (country3c 93.7% of year 3): the mason's stone 27.2 a day (22.2), **summer 23.1** (18.8),
  its takings 36.0p against its 26p; stone supply-bound in spring and summer now.
- **Phase 4: 147p a day till to till** (19p, the casks): linen 52p (the tailor 22p, the apothecary 20p, the papermill
  7p, the scriptorium 3p), hoops 26p (the carpenters' and the cartwright's), planks 25p, paper 9p, cord 14p, leather
  3p, ink 1p. The Country's sales of them 120p to 36p a day: linen 49p to 17p (the weaver 10.3 pieces a day, the store
  room still 5.1), cord 23p to 16p (the chandlery 14 of 39), hoops, planks, paper, leather and ink all but gone; its
  wool scarves, iron bars and soap up 20p (the households' money that was the makers').
- **The city's makers** (a day, short of their wages): the weaver **0** (its keeper's profit 9.6p, 11.6p; its wages
  drifted 34p to 28p), the chandlery 0 (but its candles sold 126 to 119 a day: its crew's hours on cord), the sawmill
  **0** (1.1p; profit 11.5p), the papermill **3.0p** (8.5p), the tannery 3.1p (2.8p), the cooperage 4.8p (5.0p), the
  smithies 16.1p (15.9p), the scriptorium 7.2p (5.0p: its paper and ink now bought by the printing house and the
  cartographer from the papermill first), the ironworks 25.3p (24.5p), the charcoal burner 11.3p (11.2p), the
  shipwright 8.9p (10.5p). **The wage support 295p a day** (281p), first and last four weeks 254p to 247p (flat); the
  tills short 26.6 a day (25.5).
- **The toll**: 1,727p a day in year 3 (1,710p): what moved till to till lost its toll (~20p), the goods the households
  bought instead paid more. **The treasury over its float every night** (least 1,352p, day 0; year 3 least 4,477p),
  1,370 to 6,040p, year 3 flat (5,952p to 5,950p, four-week means).
- **No good newly short** but candles (all shops: 28,800 draws refused to 37,700, the chandlery's cord) and the
  dairies' resellers out of cheese and butter on most days; **the linen neckerchiefs no longer short** (11,800).
- **Fuel and stone piling slower**: at the run's end charcoal 1,454 (2,418), timber 3,513 (7,142), firewood 12,129
  (16,105), peat 2,356 (3,456), oak bark 1,372 (2,484), stone 439 (1,895); **clay 3,468 and sand 2,138 pile harder**
  (1,350, 674: the cut stone's larger share of the baskets came partly out of the bricks'), salt 5,476 (4,636).
- **The dairies**: butter **13.9** and cheese **14.9** a day made (2.2, 0.6), all sold, **none held at the end** (the
  stores of 120 and 20 never full); milk 87.4 used, 0.4 spoiled. Still supply-bound in every season (the resellers'
  draws refused about as often: an hourly retry each). Their takings 194p each (182p), their rent 46p (29p), still the
  richest farms (92p left a day); the sheep runs' takings 87p to 78p, the boats and salt 4.8-6.0p left (the least).

**Verdict: stable enough for Phase 5**, with the summer margin (24%) the smoke projected and the quarry paid. To watch
in Phase 5: the clay and sand piling (the pits' yields down, the patch), the candles (the chandlery's crew: cord to the
others or a hand more), the scriptorium, the thinnest boats, orchards and salt (4.8-6.0p a day over their wages and rent).

### Phase 5, the map: the country workshops (2026-11-04, branch `r2-country-p5-map`)

Places only, as the siting says (the economy's branch gives them their posts, goods and people). `realm2_places.WORKSHOPS`:
doc 71 section 5's eight workshops by their fixed ids, each a workshop with its **keeper's flat over it** (4 beds: the
keeper now, the family later; stairs from the workshop floor), work spots on its floor for its whole crew, laid out from
its stations (`workshop`, on the building kit's layout). They are laid **last**, after the communities and the works'
footpaths, on the lots still free (a door onto a street, a lane or a footpath, within 30 of the core's edge), so no
house, garden, street, footpath or id laid before them moves: the farms, the works, the paths and every community's
homes, market, flats and gardens in `communities.json` are as they were; each community gains `workshops` (and the
workshops' flats in `flats`, `for` the workshop, as a store's).

| Id | Community | Building | Crew (doc 71) | Floor: stations | Work spots | Keeper's flat |
|---|---|---|---|---|---|---|
| `cn_coldwater_soapworks` | Coldwater | The Coldwater Boiling House: soap coppers, candle kettles, candle racks | 1 + 3 | rendering pot 2, chandler's kettle 2, drying rack 2, workbench | 4 | `_flat`, 4 |
| `cn_coldwater_weavers` | Coldwater | Coldwater Loom Shed (wool) | 1 + 4 | loom 6, spinning wheel 3 | 5 | `_flat`, 4 |
| `cn_larkfield_weavers` | Larkfield | Larkfield Linen Shed | 1 + 4 | loom 6, spinning wheel 3 | 5 | `_flat`, 4 |
| `cn_larkfield_press` | Larkfield | Larkfield Press and Paper Mill | 1 + 4 | paper vat 3, press, printing press 2, scribe's desk | 5 | `_flat`, 4 |
| `cn_larkfield_smithy` | Larkfield | Larkfield Smithy | 1 | forge, grindstone, workbench | 1 | `_flat`, 4 |
| `cn_fenshore_pottery` | Fenshore | Fenshore Pottery, its kiln yard beside it (two kilns, stacked pots) | 1 + 1 | kiln 2, potter's wheel 2, drying rack 2, workbench | 2 | `_flat`, 4 |
| `cn_southmeadow_pottery` | Southmeadow | Southmeadow Pottery, its kiln yard beside it | 1 + 1 | kiln 2, potter's wheel 2, drying rack 2, workbench | 2 | `_flat`, 4 |
| `cn_oakhaven_press` | Oakhaven | Oakhaven Press and Paper Mill | 1 + 4 | paper vat 3, press, printing press 2, scribe's desk | 5 | `_flat`, 4 |

- **Where in the village**: the potteries on the side towards their clay pits (Fenshore's west, towards the Mirebridge
  pits; Southmeadow's towards the Sallow pits), each with a fenced kiln yard (12 x 8) on dry ground beside it; the
  presses on the side towards the nearest water. **Neither Larkfield nor Oakhaven has a stream in its grounds**: the
  Long River is ~100 tiles from Larkfield's green and the Rainrun's mouth ~110 from Oakhaven's, so the presses stand on
  the village's edge towards them (90 and 82 tiles from the water), no wheel; the stamping mill is a treadle. A mill on
  the bank would be its own compound with a footpath, a later step if wanted. The rest near the green.
- **The hands' beds** (the keepers in their flats; 21 hands): a community's cottages hold its farms' hands, its works'
  crews and its market hands without a flat, first come first housed. Larkfield's four (24 beds) would be full to the
  last bed with its 8 workshop hands (11 farm, 3 charcoal, the pot hand and the baker before them), so it gets **two
  cottages more**, `cn_larkfield_cottage_5` and `_6` (36 beds; ~24 used). The others keep at least a cottage free:
  Coldwater 54 for ~32 (+7), Southmeadow 18 for ~10 (+1), Fenshore 18 for ~12 (+1), Oakhaven 18 for ~8 (+4).
- **Beds** (homes and flats): Coldwater 62 to 70, Larkfield 31 to 55, Southmeadow, Fenshore and Oakhaven 25 to 29,
  Saltwell 22. **Room to grow** (cottage lots): Coldwater 6 to 4, Larkfield 8 to 4, Southmeadow 6 to 5, Fenshore 9 to 8,
  Oakhaven 9 to 12 (the cut's footpath, laid after the count was first made, gives lots doors), Saltwell 6.
- **Checks**: the realm's own, 23 of 23 (new: "the country workshops: the eight, a keeper's flat and work for their 29
  each"; the doors-reached check counts the workshops' doors). `test_realm2`: the workshops' interiors (every kind,
  every look, crews 1-5) and, with `RATW_SLOW=1`, the eight by their ids, crews and flats on the realm. A dry run of the
  write (`write --world dev --dry-run`): every check and the Atlas validation pass. Previews:
  `artifacts/realm2/workshops/` (each workshop close up, the five villages; `before/` the villages as they were).
- **Not yet written into DEV** (the session's write was refused by its permissions): `communities.json` is the
  generator's (laid from DEV read only, `realm2_places.write_contract`), and `test_realm2` with `RATW_SLOW=1` passes 30
  of 30 against it; `farm_people_behaviors` and `farms_behaviors` pass (until the write, the game skips the workshops'
  and the two cottages' rooms, which DEV doesn't have). To finish: from `tools/`, `python3 -m worldgen.realm2 write
  --world dev` (it backs DEV up first), then `python3 tools/world_build.py dev`, and export the build for the sim to
  `artifacts/economy/r2/export<build>`.

### Phase 5, the economy's side: the country workshops, the resale path, cheese the luxury; country4's carry-overs (2026-11-04, branch `r2-country-p5`)

Branch `r2-country-p5`, from `ridgemere2` (4511cfa). **Nothing written to DEV, no NPC made, no map change** (the map's
side, the workshops' buildings and the cottages, is its own branch, at the fixed building ids below); the treasury's
levers (toll, support, float) untouched. Scripts in the session's scratchpad (`p5/`: `farms_p5.py`, `cheese.py`,
`smoke.sh`).

**The eight workshops** (`Data/Economy/workshops.json`, new): each a business of `businesses.json` (six new kinds, first in
the file so their keepers' labels are never another's: `country_soapworks`, `country_wool_weavers`,
`country_linen_weavers`, `country_press`, `country_pottery`, `country_smithy`), its crafts the city makers' own
(`crafts.json`: the new kinds added to the makers of fine soap, tallow candles, wool scarves, linen, pamphlets, paper,
clay pots, hoops, nails and cookpots), at the map's fixed building, merged into its community's `market` (the map's
entry for the building gives its work spots where there is one), staffed as the communities' markets are
(`farm_people.json`'s new posts; `Society::staffFarms`, the keeper in the flat `<building>_flat` or the flat `for` its
name, the hands in the community's cottages, a workshop's hands only once its keeper is there or sent for):

| Building | Workshop | Business | Crew | House | What it makes, of what |
|---|---|---|---|---|---|
| `cn_coldwater_soapworks` | Coldwater Soapworks | soap boiler and chandler | 1 + 4 | the Crown | fine soap and tallow candles: the herds' tallow, the marsh's herbs, cord from the chandlery and the ropewalk |
| `cn_coldwater_weavers` | Coldwater Looms | wool weavers | 1 + 1 | the Crown | wool scarves: the sheep runs' wool |
| `cn_larkfield_weavers` | Larkfield Looms | linen weavers | 1 + 1 | the Crown | linen: the grain farms' flax |
| `cn_larkfield_press` | Larkfield Print House | press and paper mill | 1 + 2 | the Crown | pamphlets and paper: linen, ink from the scriptorium and the printing house |
| `cn_larkfield_smithy` | Larkfield Smithy | village smithy | 1 | the Crown | hoops, nails, cookpots: the Country's iron bars |
| `cn_fenshore_pottery` | Fenshore Kilns | village pottery | 1 | Greatly | clay pots: the Mirebridge pit's clay |
| `cn_southmeadow_pottery` | Southmeadow Kilns | village pottery | 1 | Greatly | clay pots: the Fens pit's clay |
| `cn_oakhaven_press` | Oakhaven Print House | press and paper mill | 1 + 2 | Timberland | pamphlets and paper |

- **Crews: 18 posts (8 keepers, 10 hands)**, against the siting's ~29 (doc 71's plausible count, before the inputs were
  known), sized to their inputs and to what the Country sold of their goods in country4's year 3, at the crafts' hours
  and ten shop hours a post: the soapworks ~43 crew-hours a day of soap (the Country's ~21) and ~23 of candles (the ~80 a
  day the shops went short); the wool looms the sheep runs' ~12 wool (~10 of it piling); the linen looms the ~5 linen the
  store room still sold and the presses' paper; the presses ~60 crew-hours for the Country's ~19 pamphlets and ~4 paper;
  a potter alone at each kiln (the ~34 pots a day are ~27 crew-hours for both, and a pot sold on at the store room's
  price leaves ~1.2p an hour over its clay: a hand would stand idle; the `kiln hand` post is there for later); a smith
  alone. Coldwater +7, Larkfield +6, Oakhaven +3, Fenshore +1, Southmeadow +1.
- **The pool** (build 60's export after the smoke: of working age, the towns' trades and shops 23, keeps the house 47,
  service and labour 34): the keepers come from the towns' trades and shops (the post's words first: chandler, weaver,
  press, potter, smith), the hands from those who keep the house, so **8 of the 23 and 10 of the 47; no NPC made**.
- **Who owns them, and their wages.** Each is **its House's** (doc 42, Phase 5b: a workshop or yard is a House's, which
  takes all the profit), the land's: Coldwater's and Larkfield's the Crown's (its herds' and grain farms'), the potteries
  Greatly's (its clay pits: construction, doc 67 R25), Oakhaven's Timberland's (the woods'). Their posts are paid as **a
  city maker's** (the till's keeper's and hands' wages, 14p and 8p, following its takings by `wageDrift`), not the works'
  fixed 16/10: a workshop sells at prices to shops and households, its takings swing with its sales as a maker's do, and
  a wage that follows them is doc 42's "the manager's wage will later follow how well the shop does"; the works' fixed
  wage is for a crew whose yield the land sets. What its till holds each night over its float and `floatWagesDays` of its
  wages goes to **its House's estate** (the Country's purse, "the Houses' estates and the sea trade", booked by House with
  the works' rents; `LoopDay::workshops`, world_check's `workshops`), never its keeper's profit; a short night is the
  treasury's wage support, as any till's.
- **Selling to the city.** Till to till, which the code already did soundly (`buyFromCity`): the city's shops buy the
  workshops' goods first (`country.json` `suppliers`: the general stores their fine soap, pamphlets and linen, the
  rag-and-bone men their scarves and pots, the carpenters and the cartwright their hoops beside the smithies and the
  cooperage, the printing house and the cartographer their paper, the linen's buyers their linen beside the weaver), the
  store room only what they lack, so the Country no longer sells what they hold; and any household whose own community's
  shops lack a good buys it of them (the shops' rule: Coldwater's households buy their candles and soap at the
  soapworks; the city's candles, short every day, too).
- **A world without the buildings** (build 60's export, and DEV until the map branch lands): the workshop's market entry
  is the file's own, but `world.place` has no such interior, so nobody is placed and no till opens; `buyFromCity` finds
  no seller and the store room supplies the goods as before Phase 5 (`workshops_behaviors`, its last case).
- **The work spots are the map's** (the review): where `communities.json` lays a workshop (a community's
  `workshops`, the map branch), its work spots by its stations are the market's; `workshops.json`'s own `work` only
  where the map has none (`workshops_behaviors` checks it against whatever the contract holds).

**The resale path** (4.5 and 5): `Society::restockFromStore`'s `!w.craft` branch calls `buyFromCity` (its new `resale`
flag) before the store room, at **the store room's price for the good** (resaleMarkup of the season's, 75%: at the
seller's price it would leave the reseller nothing), or a supplier's own **`price`** (percent of the catalogue's): the
doc's leftovers from the city's makers, the general store's and the builders' yard's nails, the rag-and-bone man's and
the pawnbroker's cookpots and the barber's bandages, are at the catalogue's (at 75% a smith sells them at its iron's
cost, the bandages under their linen's), so the resellers' margin on those, ~6p of the ~25p a day, is gone. Counted apart
(`cityResale`, by good).

**Cheese, the luxury** (the user's decision): `country.json` `luxuries` (cheese; out of the comforts). **Never food**: a
household's food is chosen among the rest. **Every household wants it**, one a head a day (`luxuryMost`), bought **only
from what it holds over its comforts' keep (40p a head) after its food and its needs** (the fuel among them; the city's
households pay no rent, so nothing else is owed before it), at most **`luxuryShare` 10%** of that a day, before the
comforts (out of the same money). **Its price climbs with unmet demand**: the shops' existing price rule (`stockPrices`,
up a step on a day it sells out) in **its own band, up to `luxuryPriceBand` 300% over the season's price (four times),
by `luxuryPriceStep` 5% a day**, and out of the city's price level (whose scaling would cheapen everything else as
cheese climbed). The farms still sell it to the stall and the provisioners at the season's price; the dearer price is
the resellers'. New rules in Core's `Rules`, `country.json`, the Editor's and Atlas's Economy rules and `map_editor.py`
(no migration).

**country4's carry-overs.**

- **country4's patch half way** (`--apply`, damping 0.5, `wholeSeasons` the whole step) **but tallow, wool, clay and sand**
  (108 yields; stone its whole step in spring and summer: the quarry 31.79 / 26.80 / 27.99 / 32.75). **Tallow and wool
  kept** where they were: the soapworks and the wool looms are what draws their piles (16,900 and 3,600 a year). **The
  clay pits** half way to the patch, then **8.5 clay a day more each** for the potteries (37.05 / 33.84 / 34.33 / 37.59;
  sand half way, 11.84 / 9.79 / 10.12 / 12.45). **The grain farms' flax 8 a day more each** for Larkfield's linen (23.15
  / 22.25 / 22.23 / 22.77).
- **The candles**: the soapworks' 1 + 4 make them, of the farms' tallow and the chandlery's and the ropewalk's cord.
- **The thinnest farms**: their own land rent rule, **`landRentThin` 0** (`country_farms.json` `landRent` "thin": the two
  lake boats, the two salt works, orchards 2, 4 and 5; 1-3p a day each, ~13p a day to the Country in all).

**Tests.** `workshops_behaviors` (new, 114 checks): the eight as data (each at its building, in its community's market as
its business, its keeper's label its business's, its hands' no business's, at most 40 characters, its House); the
soapworks crafts fine soap and candles of its tallow, herbs and cord (conserved); the general store's soap bought from its
till at 75% of the season's price, none from the store room, the Country's purse and the treasury untouched (no toll, no
carriage), counted as resale; the soapworks out of soap, or `suppliers` off: the store room's, the Country paid; its crew
paid from its till at 18:00 (14p and 8p), what is over its float and a week's wages to the Crown's estate, booked by
House, nothing to its keeper; no building: the store room's soap; the resale prices (75%; the catalogue's for the nails,
cookpots and bandages; a craft input at the seller's); **cheese**: a household under its keep buys its food and no
cheese, and is fed, keeping its money; the rich buys one piece at three times the price, within its share; sold out, its
price climbs past the city's band (3.0 to 3.05) and never over four times; at the season's price a middling household
(200p) buys one too, at four times only the rich. `trades_behaviors` (921): the workshops beside the city's sellers, the
resale goods now from their makers. `farm_people_behaviors` (3,591): with every building standing, the eight workshops'
18 posts staffed beside the markets' 19. `farms_behaviors` (1,977): the patch's values, the thin farms' rule. `ctest` in
`build-core`: **79 of 79**. `test_econ_report` (a Phase 5 section: the resale, the workshops, the luxuries; the land
rent's table lists a farm whose rule is none), `test_item_catalog`, `test_city_capacity`, `test_country_industry`,
`test_farms_summary`, `test_world_store`, `test_studio`, `test_ridgemere2` pass; the Editor's typecheck and
`economy.test.ts` pass. (`test_map_editor`'s Greyfen count, 11 against 10: not this branch's, as before.)

**The 30-day smoke** (as before: build 60's export, `--simulate 0 720 --farms 0 --farms-every 1 --deterministic
--threads 4 --economy-level full`; `artifacts/economy/r2/country5-smoke-report.md`, `country5-smoke.patch.json`, a first
spring's, not to be applied; country4's smoke beside it, days 7-29 a day). **The workshops' buildings are not in that
export, so none was staffed or sold: what it shows is the rest.**

- Money conserved every day; nobody hungry; every farm and works paid its crew in full; the Country's posts paid every
  night (824p of 824p), its purse 1,438 to 1,839p (least 1,269p; country4's smoke 1,431 to 1,889p, least 1,266p): its
  goods to the shops 1,170p a day (1,180p), the estates' rents 194p (192p), the land rents 363p (375p: the thin farms'
  ~12p), its basket 788p (803p). The treasury 2,880p at the month's end (2,973p); the tills short much as before.
- **The resale path: 24.5p a day** from the city's makers at the catalogue's price (nails 4.0 a day, bandages 1.1,
  cookpots 0.8).
- **Cheese: 25.6 pieces a day bought as a luxury, 169p** (6.6p a piece: ~1.3 times the season's price at a month, the
  dearest shop 2.1 times), by 12.9 households a day: the middle class 18.5 a day, the lower 4.8 (its larger households),
  the upper 2.3; all the dairies made sold, none held; still supply-bound (the stall's and the provisioners' draws short
  ~3,650 a month), so its price was still climbing at the month's end. Comforts a home a day 2.0 / 2.5 / 3.9 to 1.9 /
  2.35 / 3.6 (the luxuries' share out of the same money).
- **The thin farms**: the lake boats 4.9-5.1p a day over their wages and rent to 6.5-6.6p, the salt works 4.8-4.9p to
  7.8-7.9p, orchards 2, 4 and 5 18-19p to 18-20p (spring).
- The flax the linen looms would take piles at the grain farms (161 to 350 each), the clay too (the potteries' 8.5 a day
  each): neither has its workshop in this world.

**Projected for the workshops** (on country4's year 3's summer, once the map branch has built them; rough: the crafts'
hours at ten shop hours a post, the shelves following what sells): the Country loses its sales of what they make (after
the toll: pamphlets and paper ~124p, fine soap ~38p, clay pots ~38p, scarves ~31p, linen ~17p, the smithy's goods ~10p:
**~258p a day**) and gains the workshops' takings over their wages and inputs through their Houses (the soapworks ~80p,
most of it the city's candles, new money; the presses ~35p, the linen looms ~8p, the others about nothing: ~120p), the
iron bars the smith buys (~25p), and the farms' and the pits' rents on the tallow, wool, flax and clay they sell the
workshops (~25p): **~90p a day less**, its summer ~1,018p to ~930p, **~13% over its 824p of posts** (24% now; the target
10%). The treasury loses the toll on what moves, ~110p a day (3/7 of 258p): its surplus over its cap is what pays for it,
the user's lever. All of this waits for the buildings and a run.

Next (the user's go-ahead first): merge with the map branch's buildings and cottages, then a 3-year run to see the
workshops' tills, their Houses' share, the Country's summer, the candles, the cheese's price where it settles and who
buys it, and the clay, flax, tallow and wool against their workshops' draws.

### Phase 5's 3-year run (country5, 2026-11-04)

Merged `r2-country-p5-map` and then `r2-country-p5` into `ridgemere2` (the one conflict, both branches' Progress entries
here, kept both). `ctest` in `build-core` **79 of 79**; `RATW_SLOW=1 test_realm2` 30 of 30 (read only),
`test_econ_report` and the Editor's typecheck pass; `test_map_editor`'s Greyfen count and the Editor's `content.test.mjs`
(11 residents, not 10) fail as on `4511cfa` (not these branches'). Then the map's write: `python3 -m worldgen.realm2
write --world dev` (DEV backed up first, `artifacts/backups/ratw_dev_20261104-010818.pgdump` and
`upper_accord_dev_r77_20261104-010818.atlas.json`): **DEV revision 78**, every realm check passed; `world_build.py dev`:
**build 61**, 1,199 cells; exported to `artifacts/economy/r2/export61` (the eight workshops and Larkfield's two cottages
in it; `communities.json` the generator's, unchanged by the write); a scratch server loaded build 61 (559 residents) and
was stopped. Then the user's go-ahead: on `export61`, `--simulate 6 26298 --farms 0 --deterministic --threads 4
--economy-level full`, as country4. Report `artifacts/economy/r2/country5-report.md`, patch `country5.patch.json`, log
`country5.out`. Year 3 a day unless said; country4's beside it.

- **Money conserved** every day; **nobody hungry to speak of** (4 days of year 3 with anyone hungry, the hungriest 6
  hours; country4 1 day, 2 hours; households short of a day's food money 0.16 a day, 0.04). Residents 559 to 754
  (735), births 55, deaths 56; the unplaced 328 (347): the workshops' 18 posts all held (672 posts, 662 held; 654 and
  644).
- **The workshops: every one staffed, crafting and selling** (year 3 a day: takings, wages paid of owed, to its House):
  the soapworks 186.6p, 86.0p of 86.0p (its keeper's wage drifted up), 26.7p; Larkfield's print house 76.1p, 39 of 39,
  12.8p; Oakhaven's 57.6p, **34.7 of 36.0 (96.4%)**, 4.6p; Larkfield's looms 68.4p, 22 of 22, 8.7p; **Coldwater's looms
  32.4p, 19.2 of 20.0 (95.8%)**, 0; the Fenshore kilns 20.6p, 14 of 14, 4.0p; Southmeadow's 15.7p, 13.9 of 14.0 (99.1%),
  0; **Larkfield's smithy 17.9p, 4.8p of 7.0p (68%)**, 0: its iron bars 13.1p a day of its 17.9p, its wage drifted to
  the floor and still short on 178 nights (the support 2.2p). **The resale path 186p a day** (24.5p in the smoke):
  pamphlets 18.2, clay pots 18.1, scarves 4.6, nails 3.1, soap 1.8 a day.
- **The Country pays its posts every day of all three years, but its margin is thin.** Its intake **937p** a day
  against 824p (country4 1,087p: sales **346p** (545p), the estates' rents 227p (173p: the workshops' 57p to their
  Houses), the land rents 363p (368p: the thin farms' 0)); its leanest season summer **893p** (1,018p), **+69p, 8.4%
  over** (24%; the target 10%; the smoke's projection ~13%: the Houses' share came to 57p, not ~120p). Spring 921p,
  autumn 924p, winter 1,009p. Its basket fell to 68-159p a day (187-335p), which is what keeps its purse: year 3's
  first four weeks' mean 1,668p to its last's 1,749p, least 947p (day 776), but **its years' ends 2,024, 1,950, 1,783p**
  (1,924, 1,840, 1,832p): falling.
- **Every farm and works paid its crew in full but the vineyards**: **vineyard_1 92.3%, vineyard_2 80.4% of year 3**
  (their takings 38.9p and 36.4p, 45.6p and 44.5p in country4: wine sold 22.3 a day, 28.0, 248p to 306p, likely the
  households' luxury money out of the same purses); orchards 2, 4 and 5 99.3-99.6% (rent 0 and still a few nights
  short), the quarry 99.8%.
- **The treasury over its float every night of year 3** (least 2,976p; 4,477p) but 5 nights of year 2 under its cap
  (country4 none); 1,432 to 6,025p, year 3 flat (5,961p to 5,937p, four-week means). The toll 1,702p a day (1,727p).
  **The wage support 326p** (295p), year 3's first and last four weeks 277p to 320p (rising; the city's press 33p of it
  at the run's end, the pamphlets' rival), the tills short 27.4 a day.
- **Candles still short, a third as often**: 4,280 pieces refused in year 3 (12,340); the soapworks sells 53% of the
  city's candles. **Its own tallow is short** (4,700 a season): the farms' tallow all drawn now (their stock 7,919 to 0
  at the run's end), but **the butchers' tallow (their cuts' by-product, ~36 a day) piles in their tills, 39,300 at the
  run's end** (39,900), because no `suppliers` line lets the soapworks (or the chandlery and the perfumer) buy it from
  them.
- **Wool no longer piling** (the farms' 9,326 to 0; the looms draw all 12 a day). **Clay piling harder** (3,468 to
  9,041: the pits' +8.5 a day each against the kilns' ~25 pots a day), sand as before (2,150), the flax the linen looms
  leave at the grain farms 3,069.
- **Cheese, the luxury**: 15.4 pieces a day for 226p (24.8 and 246p in year 1), 12.4 households a day; its price
  **about three times the season's** through year 3 (2.9 to 2.8 times, first and last months; the band's top four),
  all the dairies made sold (14.7 a day), none held. **Who buys it: the middle class 11.5 a day (170p), the lower 3.8
  (54p), the upper 0.1** (the Houses' wolves hold 49-55p a head, barely over their 40p keep); the provisioners and the
  stalls 15.4 to sell on. Nobody hungry for it (food and needs first by rule; no household pays rent in the loop).

**Verdict: stable, but not yet at Phase 5's mark.** Money, food, posts and the treasury hold for three years; the
workshops run. Before Phase 6: the Country's summer margin (8.4%, under 10%) and its falling year-ends, the vineyards
(80-92%), Larkfield's smithy (68%), the butchers' tallow for the soapworks (a `suppliers` line), the clay (the pits' extra
back off), and the cheese going to the middle class rather than the rich (the keep a head, or the share, the user's
call).

### country5's carry-overs: the Country's summer, the vineyards, the smithy, the tallow, the clay, the cheese for the rich (2026-11-04, branch `r2-country-p5b`)

Branch `r2-country-p5b`, from `ridgemere2` (cbf9ca0). **Nothing written to DEV, no NPC made, no map change**; the
treasury's levers (toll, support, float) untouched. Scripts in the session's scratchpad (`p5b/`).

- **The Country's summer** (893p, 8.4% over its 824p of posts). Where it went: its sales 545p to 346p as the workshops
  took the goods, their Houses' share back only 57p. Two causes, two fixes:
  - **A workshop's till kept a week of wages** (`floatWagesDays` 7, a city keeper's) before its House's share. That is
    a stock, not a flow (year 3's soapworks: takings 186.6p, its draws 42.1p, inputs 32.0p, wages 86.0p, to the Crown
    26.7p: all of it accounted), so the fix is only ~5 days of its wages (~1,100p across the eight) released to the
    Houses once, and its share sooner after a lean night. **`workshopWagesDays` 2**, the works' `floatDays`.
  - **The land rent where the margin is**: the cattle and dairy farms kept 68-72% of their takings after wages in
    country5's year 3 (summer 68%; the pig farms 45%, the grain 63%, every other farm thinner). **`landRentDairy` 24 to
    32**: ~+14p a farm a summer day, **~+42p**; they keep ~36-40% of their takings after wages and rent (more than the
    pig farms' ~30%). No thin farm touched.
- **The vineyards (92.3% and 80.4%): confirmed the cheese.** Year 3 against country4's: the middle class's cheese
  58p to 170p a day and its wine 193p to 154p (16.1 to 13.0 bottles), the lower class's wine flat (69p to 68p) and its
  cheese +18p. Wine (12p) is the dearest comfort: a household buys it only when its comforts' budget, what is left after
  the luxuries out of the same money, covers 12p, so the cheese came off wine first. The fix is the cheese's buyers
  (below). **And the wine's patch kept out** (vineyards' wine and grapes where they were): country5's measured demand was
  the depressed one, and in the smoke the half-way cut left the vintner short (317 refused) and the vines' takings under
  the baseline's.
- **Larkfield's smithy (68%)**: demand-bound, not hours- or price-bound: it sold 4.7 pieces a day (nails 3.5, hoops
  0.9, cookpots 0.3; ~18p), ~3-7p a bar over its ~9p bar (the toll a third of it), and the farms, the biggest buyers of
  nails, hoops and cookpots (~85p a day in their baskets), bought ~13p of it from it, the city's smiths the rest
  (picked by stock: the city's shelves the deeper). **A farm's basket buys from the country workshops first**
  (`basketBuys`, the farms' only: the village smith is the farms' own), the city's makers for what no workshop holds.
- **The butchers' tallow** (39,300 piled in their tills): `suppliers` lines, the soapworks, the chandlery and the
  perfumer buy it of the butchers first (`country.json`), till to till at the butcher's price (1p), the farms' and the
  store room's for the rest. The farms' tallow kept where it was (out of the patch: the butchers' ~36 a day is the new
  supply).
- **The clay** (9,041) and **the grain farms' flax** (3,069): to what country5 used (its patch's balanced yields less
  their 5%), the whole step: the pits 37.05 / 33.84 / 34.33 / 37.59 to 31.70 / 29.24 / 30.17 / 32.50 each (the potteries'
  +8.5 back to what they drew), the flax 23.15 / 22.25 / 22.23 / 22.77 to 22.59 / 20.98 / 21.04 / 21.21. **The rest of
  country5's patch half way** (`--apply`, damping 0.5, `wholeSeasons` whole: 110 yields) but tallow, clay, flax, wine and
  grapes.
- **Cheese for the rich and the noble** (the user's intent: "a high-class commodity... only the rich and noble being able
  to afford it... high desire... but careful that wolves don't break the bank for it"). country5: the middle class
  11.5 a day, the lower 3.8, the upper 0.1, the Houses' wolves holding 49-55p a head (the middle class as much), so a
  keep a head in cash could never tell them apart. Now:
  - **By class, not cash**: a household of the Houses (any member's class `upper`, `classOf`, kept a day) buys from what
    is over the comforts' keep (40p a head); any other only from what is over **`luxuryAbove` 200p a head** (a rich
    merchant's purse). The guard stays: after its food and needs, at most `luxuryShare` 10% of that a day, a piece a
    head.
  - **The Houses' own tables**: once a day, at the first hour an open shop holds a luxury, the Country's purse (the
    Houses' estates) buys with **`luxuryHouses` 40%** of what it holds over its reserve and its posts' nights (its
    basket's money, before the basket; never its posts'), at most **`luxuryHousesMost` 24** pieces, from the shop asking
    least, at that shop's own price (`housesLuxuries`).
  - **Its price may climb to eight times** (`luxuryPriceBand` 300 to 700), so that once it is scarce the middle class
    mostly can't.
  New rules in Core's `Rules`, `country.json`, the Editor's and Atlas's Economy rules (no migration); world_check's
  `luxury_classes` and `luxuries_houses`, the report's luxuries line by class and the Houses'.

**Tests.** `workshops_behaviors` (190): a workshop keeps two days of wages (a city till a week); the soapworks', the
chandlery's and the perfumer's tallow the butchers'; the soapworks buys a butcher's tallow at its price, none drawn,
conserved; cheese by class: the Houses' lady (120p) buys one at the season's price, the stallholder's household (200p)
none, the rich (2,000p) one at three, four and eight times within its share, the band's top no further, the poor fed and
none; the Houses' tables: ten at the stall's own price within 40% of the spare, never into the reserve, 24 at most, none
under a piece's price or with the rule off, counted apart, conserved. `farms_behaviors` (2,003): a farm's nails and
cookpots from Larkfield's smithy while it holds them, the city smith's only once it sold out and for what it doesn't
make; the dairies' 32%; the quarry's and the pits' new yields. `ctest` in `build-core` **79 of 79**;
`test_econ_report`, `test_farms_summary`, `test_country_industry`, `test_item_catalog`, `test_city_capacity`,
`test_world_store`, the Editor's typecheck and `economy.test.ts` pass (`test_map_editor`'s and the Editor's Greyfen counts
fail as before, not this branch's).

**The 30-day smoke** (build 61's export, `--simulate 0 720 --farms 0 --farms-every 1 --deterministic --threads 4
--economy-level full`; `artifacts/economy/r2/country5b-smoke-report.md`, `country5b-smoke.patch.json`, a first spring's,
not to be applied; beside it the same run on `ridgemere2`'s code and data, days 7-29 a day):

- Money conserved every day; nobody hungry; the Country's posts paid every night.
- **The Country's intake 1,443p a day against the baseline's 1,381p (+62p)**: the land rents 459p (408p; the dairies'
  +60p), the estates' rents 375p (367p), its sales 609p (606p); its purse 1,437 to 2,053p (1,940p).
- **The workshops' Houses**: the eight's share 110p a day (97p); **Larkfield's smithy paid 10.9p of its 14.0p (78%; the
  baseline 5.5p, 39%) and sent its House 4.5p (0)**.
- **The tallow**: the butchers' tills hold ~35 (the baseline's 98 to 1,158 in the month); the soapworks and the chandlery
  buy ~37 a day of it; candles short 203 a month (14 days). But the farms' tallow now piles (0 to 1,081, ~20 a day) while
  the city's tallow buyers' shelves still grow (country5's year 3: the soapworks 51 a day short): the 3-year run's to
  judge; if it keeps piling, trim the meat farms' tallow.
- **Clay** 417 to 216 at the pits (the baseline's 526 to 440), **flax** drawn to 0 by day 22 (the baseline's by day 29).
- **Cheese** (a month: its price only beginning to climb, ~1.3 times): **the Houses' tables 8.4 a day for 57p**, the
  households 8.5 for 53p (by household: the Houses' 7.5, the rest 1.0; by the payer's own class, as the trade tables
  count: the middle 4.7, the upper 3.7, the lower 0.04, the Houses' middle-class kin paying), against the baseline's
  15.7 households' for 127p (the middle class 12.2 a day, the lower 3.2, the upper 0.4). The households' wine 24.8
  bottles a day (25.4p); the vineyards' takings 54.2p and 53.2p (53.6p and 52.4p), each over its 36p of wages.

**Projected** (on country5's year 3's summer, 893p): the dairies' rent ~+42p, the smithy's and the workshops' shares
~+5p, the rest about even: **~940p, ~14% over its 824p of posts** (8.4%; the target 10%). The workshops' reserve adds
~1,100p to the Houses once, in the first weeks. As the cheese's price climbs toward its eight times, the households'
part should fall to the Houses' wolves and the few rich, the Houses' tables taking fewer, dearer pieces: the dairies'
cheese then follows what the resellers draw. All of it the 3-year run's to confirm.

### country5b's 3-year run (2026-11-04)

`ridgemere2` at `7e8085e` (the carry-overs merged), the same run as country5 (build 61's export, `--simulate 6 26298
--farms 0 --deterministic --threads 4 --economy-level full`). Report `artifacts/economy/r2/country5b-report.md`, patch
`country5b.patch.json`, log `country5b.out`. Nothing written to DEV or PROD. Year 3 a day unless said; country5's beside
it.

- **Money conserved** every day; **nobody hungry to speak of** (3 days of year 3 with anyone hungry, the hungriest 4
  hours; 4 days, 6 hours); households short of a day's food money 0.15 a day (0.16). Residents 559 to 754, births 55,
  deaths 56, the unplaced 328, 662 of 672 posts held: all as country5.
- **The Country: its posts paid every day of all three years, and its margin back.** Intake **1,070p** (937p): sales
  396p (346p), the estates' rents 251p (227p), the land rents 423p (363p: the dairies' 32% ~+21p a farm). **Summer
  1,061p, 28.7% over its 824p** (893p, 8.4%; the target 10%, the projection ~14%); spring 29.9%, autumn 26.8%, winter
  34.2%. Its basket, the spare's spending, took it: 193-216p a day (68-159p). Its purse: year 3's first and last four
  weeks 1,763p to 1,844p (1,668 to 1,749p), least 1,026p (947p); **its years' ends 1,986, 1,975, 1,900p** (2,024, 1,950,
  1,783p): the fall a third of country5's and its basket ~200p a day over what its posts need, so a spending rule, not a
  shortfall.
- **Every farm and works paid its crew** (99.6% and over): **vineyard_1 99.9%, vineyard_2 99.6%** (92.3%, 80.4%; their
  takings 37-42p over their 36p of wages, the households' wine 22.6 bottles a day, 19.6), orchard_5 99.6%, the quarry
  and orchards 2 and 4 in full now. **The workshops**: the soapworks, both presses, both potteries and Larkfield's looms
  100% (Oakhaven's press 96.4%, Southmeadow's 99.1%); **Coldwater's looms 96.3%** (95.8%, 52 nights short), **Larkfield's
  smithy 84.4%** (68.1%: 11.8p of 14.0p, 103 nights short of 178, 3.5p to its House, 0).
- **The treasury over its float every night of year 3** (least 2,983p; 2,976p; year 2's least 1,957p, 1,544p), 5,727p
  to 5,952p (four-week means). **The wage support 283p** (326p), year 3's first and last four weeks 271p to 256p
  (277p to 320p): falling, not rising. The toll 1,653p (1,702p); the butchers' support 8p (20p) now their tallow sells.
- **Candles: no shortage in year 3** (11.7 a day refused; 0 now); the soapworks holds stock at the season's price. **The
  butchers' tallow cleared** (their tills 39,300 to 32 at the run's end), but **the farms' tallow piles: 7,936, 17,730,
  27,901 at the years' ends** (~27 a day; the meat farms grow ~74, ~45 sold): the patch's largest lines cut it about a
  third (the sheep runs and the pig farms ~7 to ~5, the dairies ~12.7 to ~8.6).
- **Clay 9,041 to 791** at the pits (rising ~1 a day in year 3, slight), **flax 3,069 to 129** (now supply-bound; the
  linen neckerchiefs 10.1 a day short, 8.1, the one cloth short). Wine: the vineyards' cellars 31, 560, 1,547 at the
  years' ends (each pressing ~4 a day and selling ~2 at the run's end; the patch trims wine ~15%, the unsold part).
- **Cheese for the rich, but not scarce.** The households' luxuries 8.2 pieces a day for 41p, every one an upper-class
  household's (the middle 0.0, from 0.6 in year 1); the Houses' tables 3.4 a day for 18.5p (7.4 in year 1); by the
  payer's own class (the trade tables) the upper 4.8, the Houses' purse 3.4, the middle 2.7 (the Houses' kin), the lower
  0.8, against country5's middle 11.5, lower 3.8, upper 0.1. Nobody hungry for it. **Its price about the season's (1.0
  to 1.1 times; 2.8-2.9 in country5)**: the dairies make 15 a day, the rich take ~12, ~350 held at the dairies from year
  2 on. The dairies' takings still rose (~199p a summer day, 177p), the cheese a small part of them.

**Verdict: stable, at Phase 5's mark.** Money, food, posts and the treasury hold for three years; the Country's summer is
28.7% over its posts, the support falls, the vineyards and every farm pay in full, candles are no longer short. Left to
tune, none of it a risk to the balance: Larkfield's smithy (84%) and Coldwater's looms (96%); the farms' tallow (the
patch's cut, ~a third); the cheese (made ~3 a day over what the rich take, so its price never climbs: fewer from the
dairies, or the Houses' share up, the user's call); the flax (the neckerchiefs short) and a little of the clay; the
Country's basket, which spends its spare and lets its year-ends drift down ~75p a year.

### country5b's carry-overs: the workshops meet their goods, cheese scarce, the tallow and the wine (2026-11-04, branch `r2-country-6a`)

Branch `r2-country-6a`, from `ridgemere2` (549eb4b). **Nothing written to DEV, no NPC made, no map change** (every new
hand found a bed in its community's cottages); the treasury's levers (toll, support, float) untouched. Scripts in the
session's scratchpad (`c6/`).

**What limited each good** (country5b's year 3, from the full log: the Country's sales, the makers' tills and shelves,
the farms' stock):

| Good | The Country sold a day | The limit | The fix |
|---|---|---|---|
| Fine soap | 26.3 (83p) | **The herbs**: a herb a batch of three, the marsh's 2.8 a day all drawn (the soapworks 1.0 a day short), its soap 5 a day, its shelf always empty; then **the hours** (6.1 a batch: ~54 crew-hours more) | the marsh's herbs 2.8 to 13.1 a day; the soapworks 1 + 4 to **1 + 7** |
| Wool scarves | 28.0 (72p) | **The wool**: the sheep runs' 13 a day all drawn (the looms 1.7 short, the weaver 1.5); then the hours (1.6 a scarf) | the runs' wool 6.5 to ~26.5 a day each; Coldwater's looms 1 + 1 to **1 + 6** |
| Clay pots | 17.7 (18p) | **The hours**: a potter alone, sold out at the band's top (13.4 and 11.1 a day); clay plenty (791 at the pits) | a **kiln hand** at each pottery; the pits' clay +7 a day each |
| Cord | 26.4 (18p) | **No maker but the chandleries**, their hours on candles (their cord shelves ~5): the soapworks drew 15.4 a day of the Country's, the chandleries 8.5; the ropewalk (crew 3, 12.8p of 20.8p paid) made none | **the ropewalk makes cord** (`crafts.json`), `suppliers` lines to it from the chandleries, the general stores, the jeweller, the tinker and the fletcher (the soapworks' was there); the grain farms' **hemp +7.5 a day each** (the ropewalk's draw emptied it by day 23 of the first smoke) |
| Pamphlets | 2.3 (9p) | **The presses' hours** (7.7 a batch; both near sold out); then the linen their paper takes | a third vat hand at **each** press (1 + 3) |
| Planks | 2.0 (3p) | The sawmill alone in `suppliers` for the cooperage, the cartwright and the carpenters (its shelf dipping), while the builders' works (3 crew, 6.8p of 21p paid) and the carpenters sold a few | `suppliers` lines from **the builders' works and the carpenters** beside the sawmill (a business never its own: the loader drops a line to itself) |
| Linen neckerchiefs | (none: **10.1 a day short**) | **The tailor's hours** (2 crew, sold out; flax was not the limit in year 3: ~47 held at each grain farm) | **Larkfield's looms make neckerchiefs** beside the tailor and the weaver (`crafts.json`; the city's households buy them there when no city shop holds one); the looms 1 + 1 to **1 + 5** for them and the linen they then left short (9 a day of the Country's in the first smoke); the grain farms' **flax +14 a day each** (the patch's whole step and the looms') |

- **Larkfield's smithy (84%)**: alone, its nails (3.4 hours a batch) were **never made in year 3** (0.1 a day): each
  hour's bank went on a hoop (2.1) as soon as it held 2.1, so it never reached 3.4, while the farms bought ~11 nails a
  day of the city's smiths. A code limit: `Society::restockFromStore` now **saves the crew's hours for a sold-out
  ware's longer craft** (the crafts after it wait that hour) where the craft takes at most six of each post's hours
  and the bank can hold it (a sword's 19 hours at a city forge is never saved for; nothing is lost: a batch the inputs
  can't make leaves the hours to the crafts after it). And **a striker** (`farm_people.json`, "striker at Larkfield's
  anvil": a hand's label names no business, and any with "Larkfield Smithy" in it would be the city smithy's), so it
  makes nails, hoops and cookpots for the farms. Its float is a day's iron bars, so with two days of wages kept it was
  short on most Restdays: **`workshopWagesDays` 2 to 3** (a one-off ~1 day of wages more held at each workshop).
- **Coldwater's looms (96%)**: wool-bound, the hands' wages thin on 7 scarves a day; with the wool and the hands, paid in
  full.
- **Workshops' posts 18 to 35** (8 keepers, 27 hands): **17 more of the unplaced pool** (those who keep the house first,
  then service and labour), **no NPC made**; Coldwater +8, Larkfield +6 (the striker among them), Oakhaven +1, Fenshore
  +1, Southmeadow +1, all in their communities' cottages (no cottage added, no DEV write).

**Cheese scarce and dear** (the user's): the dairies' own milk for cheese 20 to **7 a day** each (~1.75 cheese; 5.3 a day
from the three, from ~15), **none of the spare milk** (`share` 0) and their store 120 to 40. The milk that no longer
becomes cheese goes to **butter**, short ~60 a day: its own milk 8 to **21 a day** each (10.5 butter) and the whole of
the spare, its store 20 to 40 (four days of its making; at 20 the spare after a Restday spoiled), so nothing spoils that
didn't (milk spoiled 1.1 a day, 1.0 before). The guard as it was (food and needs first, at most 10% of what is over the
keep; nobody hungry).

**The tallow** (27,901 piled in country5b): every farm's ×0.91 (74 a day to 67), the soapworks' soap now drawing ~17 a
day more. **The wine** (1,547 in the cellars): the vineyards' ×0.75 (their 2.1 a day sold kept; they pressed 3.5).
**country5b's patch half way** (`--apply`, damping 0.5, `wholeSeasons` whole: 103 yields) but tallow, wine, grapes,
wool, herbs, flax and clay (above).

**Tests.** `workshops_behaviors` (212): the smith alone makes nails within twelve hours and its hoops and cookpots
still (none without the saving), a city smith's iron sword never saved for (its nails made); Larkfield's looms make
neckerchiefs, the ropewalk cord; the cord and plank buyers' sellers; 35 posts; three days of wages kept.
`trades_behaviors` (923): the planks' and the cord's sellers. `farms_behaviors` (1,948): the dairies' makes (7 and 21 a
day, none of the spare to cheese, the stores 40 and 40), the patch's values, a farm's rent counted afresh after 18:00
(a shop open till 19:00 may draw a few pennies more). `farm_people_behaviors` (3,830): the workshops' 35 posts staffed
(the test's pool of those who keep the house 20 to 36). `ctest` in `build-core` **79 of 79**; `test_econ_report`,
`test_farms_summary`, `test_country_industry`, `test_item_catalog`, `test_city_capacity`, `test_world_store`,
`test_studio`, `test_ridgemere2` pass.

**The 30-day smoke** (build 61's export, `--simulate 0 720 --farms 0 --farms-every 1 --deterministic --threads 4
--economy-level full`; `artifacts/economy/r2/country6a-smoke-report.md`, `country6a-smoke.patch.json`, a first spring's,
not to be applied; beside it the same run on `ridgemere2`'s code and data, days 7-29 a day unless said):

- **Money conserved** every day; **nobody hungry** (no hungry hour); every workshop's 35 posts held.
- **The Country's sales of the workshops' goods**: fine soap 34.9 to **0.0**, scarves 35.4 to **0.4**, pots 27.3 to
  **0.0**, cord 44.1 to **0.0**, pamphlets 8.2 to **1.0**, planks 5.5 to **0.2**, linen 2.4 to **0.5**; **neckerchiefs
  short 11.5 a day to 0**. The farms' herbs, wool and flax drawn and not short (wool 439 and flax 735 held at the month's
  end; the herbs, 50, and the clay, 311, drawn a little faster than grown while the new shelves fill).
- **The workshops paid in full**: the soapworks 70.0p of 70.3p, Coldwater's looms 62 of 62 (100%; 22 of 22 before),
  Larkfield's looms 54.3 of 54.5, the presses and the potteries 99.5-100%, **Larkfield's smithy 22 of 22 (100%; 77.6%
  beside it)**, sending its House 7.4p (3.7p). The ropewalk 30 of 30 (42% beside it, 14.3p of support a day), the
  builders' works 76% (29%).
- **The Country**: over the month its goods to the shops 986p to 704p a day, the estates' rents (the workshops' Houses'
  share) 313p to 434p, the land rents 430p to 454p: **intake ~1,729p to ~1,592p (-137p)**, its basket 699p to 570p, its
  purse 2,053p to 2,248p at the month's end, its posts paid every night. (country5b's summer was 28.7% over its posts;
  ~-90p a day of it would leave ~18%.) The treasury 2,514p to 1,886p at the month's end (the toll on what no longer
  passes the gate), over its float.
- **Cheese**: the dairies 5.3 a day (16.6), all sold, none held; the shops' price for it climbing ~5% a day where it
  sells out (the dearest 2.09 times the season's at the month's end, 1.67 beside it), the households 2.3 a day (8.4) and
  the Houses' tables 3.0 (3.8 beside it, 8.5 over the whole run), 7.7p a piece (6.6p). **Butter** 37.3 a day made and
  sold (17.7), none held.
- **The tallow** 66.8 grown and 68.2 sold a day (73.1 and 52.5; held 480 at the month's end, 1,081), **the wine** all
  pressed sold (4.4 a day; the vintner short as beside it, 9.4 and 9.9), the vineyards over their wages.

Next (the user's go-ahead first): a 3-year run, to see the cheese's price settle (aimed at 3-6 times), the tallow and the
wine against their piles, the Country's summer with the workshops' sales gone, and the smithy's year.

### country6a's 3-year run (2026-11-04)

`ridgemere2` at `188b3cd` (`r2-country-6a` merged), the same run as country5b (build 61's export, `--simulate 6 26298
--farms 0 --deterministic --threads 4 --economy-level full`). Report `artifacts/economy/r2/country6a-report.md`, patch
`country6a.patch.json`, log `country6a.out`. Nothing written to DEV or PROD. Year 3 a day unless said; country5b's beside
it.

- **Money conserved** every day; **nobody hungry to speak of** (2 days of year 3 with anyone hungry, the hungriest 4
  hours; 3 days, 4 hours); households short of a day's food money 0.27 a day (0.15). Residents 559 to 771 (754), births
  55, deaths 56, the unplaced 311 (328), 679 of 689 posts held.
- **The Country no longer sells the workshops' goods** (the trade rows with the Country the seller): fine soap **0**
  (26.3 a day, 83p), scarves **0** (28.0, 72p), pots **0.02** (17.7), cord **0** (26.4), pamphlets **0.5, 2p** (2.3, 9p),
  planks **0** (2.0). None of them short; **the neckerchiefs no longer short** (10.1 a day), though their price sits at
  the band's top on 60% of till-days. The shops bought them of the makers instead (the resale 424p a day: pots 29.3,
  scarves 26.0, pamphlets 24.7, soap 23.3).
- **The Country: its margin gone with them.** Its sales ~398p to **~173p** a day (iron bars 156p, limestone 13p);
  intake **840p** (1,070p) against its 824p of posts: **summer 0.0% over** (28.7%), spring -1.2%, autumn 0.6%, winter
  8.6%. **Its posts went short on 4 nights of year 3** (days 777, 896, 931, 960: 308p, then 100p short each; none in
  country5b), its purse least 364p (1,026p), its years' ends 1,682, 1,679, 1,618p (1,986, 1,975, 1,900p), its basket ~13p
  a day (~207p): it has no spare left to spend. The workshops' Houses took 74p a day (57p) and their crews' wages rose
  ~170p a day (17 more hands): the pennies the Country made reselling now pay the villages' hands.
- **The workshops** (year 3, wages paid of owed): Coldwater's looms **100%** (96.3%), both potteries' kilns sold, the
  Larkfield press 100%, Fenshore's pottery 100%, the soapworks 99.3%; **Larkfield's smithy 98.0%** (84.4%: 21.6p of
  22.0p, 8.4p to its House); but **Southmeadow's pottery 89.6%** (100%), **Larkfield's looms 96.0%** (100%; 0.7p to its
  House), **Oakhaven's press 96.3%** (100%): the added hands more than their trade carries. Every farm and works 99.9%
  together; orchards 2, 4 and 5 97.6-99.1%, the vineyards in full.
- **The treasury over its 200p float every night** (least 1,568p; 2,983p; year 2's least 1,051p), 4,228p to 4,483p
  (four-week means; 5,727 to 5,952p). **The wage support 324p and rising** (283p, falling): 323p to 363p over year 3.
  The toll 1,672p (1,653p).
- **Cheese scarce, not yet dear.** The dairies make 5.4 a day (11.6), all sold, **none held** (~350); the shops short of
  it ~48 a day (their draws empty every season, 65% of its till-days at the band's top, stock factors to 5.3). The
  households paid **2.0-2.2 times** the season's price (1.0): the upper class 5.0 pieces a day for 54p (8.2 for 41p), the
  middle 0.0; by payer the Houses' kin 3.6 at 2.2x, the upper 1.3 at 2.0x; **the Houses' tables starved, 0.24 a day**
  (3.4). Its price still rising through the year (1.23 to 1.34 of base across all its trades, the shops' draws at 0.52):
  short of the 3-6x aim, the climb slow. Nobody hungry or behind for it.
- **Butter** 34.2 a day made (13.9), all sold, still short ~75 a day (63); the milk 0.1 a day spoiled.
- **Piles.** **The farms' tallow 7,366 at the run's end** (27,901), ~8 a day still growing (the dairies' 920 each).
  **The wine overcut: the cellars empty** (1,547), the vintner short 5.2 a day and wine supply-bound in every season, its
  price at the band's top 43% of till-days; the vineyards still paid in full. **The raised inputs overshoot and pile**:
  flax 19,960 (129; +7,000 a year), hemp 10,210 (2,821), clay 8,367 (791), wool 6,780 (0), herbs 453 (0). **Meal newly
  short** ~42 a day (the towns' bakeries and stores; the dairies' and pig farms' stock 17 each to 0: the patch's half
  step trimmed it), the pigs 4.4 (1.9).

**Verdict: not stable.** Money, food and the treasury hold; the workshops meet their goods and the Country's sales of
them are gone. But the Country's margin went with them: summer at its posts exactly, four nights short in year 3, the
purse sliding; the wage support rises again; three workshops under 99%; the inputs raised for the workshops (flax, hemp,
clay, wool) pile hard; wine and meal newly short. Next: the Country's intake (its posts or its rents: the workshops'
Houses' share, or a land rent on the workshops, the user's call), the inputs back toward what the workshops use (the
patch's flax, and hemp, clay, wool by the same), the wine's cut halved (x0.875, or summer and autumn only), the meal
from the patch, the three workshops' last hands back to the pool, and the cheese: a slower dairy still, or a faster climb
of its price where it sells out, to reach the 3-6x.

### country6a's carry-overs: the workshops' wages, the inputs, the wine, the cheese (2026-11-04, branch `r2-country-6b`)

Branch `r2-country-6b`, from `ridgemere2` (d036a8d). **Nothing written to DEV, no NPC made, no map change, no post
added or dropped**; the treasury's levers (toll, support, float) untouched. Scripts in the session's scratchpad (`c6b/`).

- **The Country's margin: where it went.** country6a's year 3 against country5b's: the Country's goods to the shops
  686p to 440p a day (the workshops' goods, ~225p), the estates' rents 239p to 262p; the eight workshops' wages **212p to
  434p**, their Houses' share only 62p to 111p. Half the rise was the 17 new hands; the rest **the wage drift**: a
  workshop's House takes the profit, and the drift (`judgeWages`, wageDriftSpare) raised its posts a penny each time
  that share reached half its wages, up to the city's 12p over the world's, so through years 1-2 the crews' wages climbed
  **~130p a day over their base** (the soapworks 118p for its 8 posts against 70p, Larkfield's looms 90p against 54p,
  the presses 54p and 50p against 38p) while the Houses' share fell (the soapworks' 51p to 15p a day). Never lowered
  again (only a till short on most days falls), so the House's share stayed what the wages left.
  **`workshopWageMost` 2** (a new rule, Core's `Rules`, `country.json`, the Editor's and Atlas's Economy rules, no
  migration): a country workshop's wages drift at most 2p over the world's, a keeper 16p and a hand 10p, **the works'
  and the farms' fixed wages**; down as any till's. Its House keeps the rest, the Country's purse (~+75p a day on
  country6a's year 3 at the same takings). The land rent on the workshops (the other option) would have come out of
  the same share: the drift was what took it. The Country's posts and the treasury untouched.
- **The three workshops under 99%.** Larkfield's looms (96.0%) and Oakhaven's press (96.3%): short on the drifted wages,
  not the hands (Larkfield's looms' ~48 crew-hours a day of linen and neckerchiefs need its 6 posts; the press's 13.3
  pamphlets a day ~37 of its 4), so kept; at the capped wages each over its posts. **Southmeadow's pottery (89.6%)**: its
  ~15 pots a day leave ~13p over their clay (a pot ~1.4p, its clay ~0.5p with the toll) against two posts' 18p, below its
  base already (the drift down 2p). **Kept 1 + 1**: a 150-day trial with the potter alone had the Country selling the
  rag-and-bone men ~7 pots a day again (days 100-150; 0.3 with the hand), and alone it was 96% in country5b, so neither
  reaches 99% at this trade (the pots' price or the clay's toll would; left). **Larkfield's smithy (98.0%)**, its float a
  day's iron bars, still short on some Restdays: **`workshopWagesDays` 3 to 4**.
- **The inputs back toward what the workshops draw** (country6a's year 3, a farm a day, grown against sold, and +5%),
  **half way**: the early years drew more (a 150-day trial at the whole step left the flax and hemp short ~20 a day
  in days 100-150, the Country selling cord and linen again): flax ×0.863 (31.2 grown, 21.6 sold), hemp ×0.932 (33.3,
  27.4), the pits' clay ×0.943 (37.9, 32.0), the sheep runs' wool ×0.955 (26.4, 22.9), the marsh's herbs ×0.978 (13.2,
  12.3).
- **The farms' tallow** to what sold (+2%): the dairies ×0.932 (11.6 grown, 10.6 sold), the pig farms ×0.870 (6.45,
  5.5), the sheep runs ×0.847 (6.5, 5.4).
- **The wine's cut halved**: the vineyards ×0.875 of country5b's (×1.167 of country6a's), the cellars empty and the
  vintner short 5.2 a day at ×0.75.
- **Cheese dearer** (the user's: a comic luxury of the rich and the noble, its price climbing to meet demand): country6a's
  5.4 a day sold out at 2.0-2.2 times, the rich taking it all, the Houses' tables starved (0.24 a day: the Country's
  purse had no spare). The dairies' own milk for cheese **7 to 5 a day** each (1.25 cheese; ~3.75 a day from the three),
  the 2 to the butter (**21 to 23**, still short ~75 a day), so no milk spoils for it; the guard as it was (food and needs
  first, at most 10% of what is over the keep). With the Houses' share back, their tables bid for it again.
- **country6a's patch half way** (`--apply`, damping 0.5, `wholeSeasons` whole: 101 yields; the meal, pig and milk among
  them, supply-bound) but tallow, wine, grapes, wool, herbs, flax, hemp and clay (above).

**Tests.** `workshops_behaviors` (216): a workshop's step of 6 pays its keeper and hand 2p over the world's, 6p with the
rule at 12, a penny under with a step of -1; four days of wages kept. `farms_behaviors` (1,948): the dairies' 5 and 23
a day, a stall's 3 cheese of the dairies' stores, the quarry's and the pits' new yields. `ctest` in `build-core` **79 of 79**; `test_econ_report`,
`test_farms_summary`, `test_country_industry`, `test_item_catalog`, `test_city_capacity`, `test_world_store`, the
Editor's typecheck and `economy.test.ts` pass (`test_map_editor`'s Greyfen count, `test_ridgemere2`'s two DEV checks and
`test_studio`'s copies, another session holding its database, fail as before, not this branch's).

**The 30-day smoke** (build 61's export, `--simulate 0 720 --farms 0 --farms-every 1 --deterministic --threads 4
--economy-level full`; `artifacts/economy/r2/country6b-smoke-report.md`, `country6b-smoke.patch.json`, a first spring's,
not to be applied; country6a's smoke beside it): money conserved, nobody hungry, the Country's posts paid every night;
its intake 1,556p (1,592p) and purse 2,262p at the month's end (2,248p); the Country's sales of the workshops' goods
days 7-29: soap, scarves and cord 0.0-0.1 a day, pots 0.2, pamphlets 0.9, planks 0.2, linen 1.4; the workshops paid in
full but the smithy (19.3p of 22.0p, its first month, as country6a's 19.1p); cheese 3.5 a day made (4.9), all sold, the
Houses' tables 3.0 a day (3.6), butter 40.2 (35.7). The month is too short for the drift (judged every 28 days):
**a 150-day trial** (`--simulate 6 3606`, as the 3-year run) beside country6a's first 150 days: conserved, nobody
hungry, the posts paid every night; days 100-150 the workshops' wages the same (the drift not yet past 2p:
the soapworks 84p, 85p), their Houses' share about the same (~160p a day); the Country's pots 7.5 a day with the potter alone (0.3), so
the hand kept; the flax ~12 a day short (1.7: the first year drew ~34-40 a farm, the third ~22), the half way kept.

### country6b's 3-year run (2026-11-04)

`ridgemere2` at `5c51abd` (`r2-country-6b` merged), the same run as country6a (build 61's export, `--simulate 6 26298
--farms 0 --deterministic --threads 4 --economy-level full`). Report `artifacts/economy/r2/country6b-report.md`, patch
`country6b.patch.json`, log `country6b.out`. Nothing written to DEV or PROD. Year 3 a day unless said; country6a's beside
it.

- **Money conserved** every day. **Hunger a little up, still small**: 5 days of year 3 with anyone hungry, the hungriest
  7 hours (2 days, 4 hours); households short of a day's food money 0.38 a day (0.27; country5b 0.15): the capped wages
  are ~65p a day less in the workshops' households. Residents 559 to 771, births 55, deaths 56, the unplaced 311, 679 of
  689 posts held: as country6a.
- **The Country's sales of the workshops' goods stay gone** (the trade rows with the Country the seller): soap, scarves,
  pots, cord, pamphlets and planks **0** (country6a: pamphlets 0.5, pots 0.02), linen 0.04; none of them short, the
  neckerchiefs not short (their price at the band's top on 57% of till-days, 60%).
- **The Country: its posts paid every night of year 3** (4 nights short). Intake **894p** (840p): its sales 176p (173p),
  the estates' rents 284p (234p: the workshops' Houses 144p a day, 111p), the land rents 434p (433p). **Summer 871p, 5.7%
  over its 824p** (0.0%); spring 8.2%, autumn 6.2%, winter 13.9%. Its purse least 893p (364p); **its years' ends 2,039,
  1,702, 1,643p** (1,682, 1,679, 1,618p): still falling ~60p in year 3, its basket 48p a day (13p). The capped wages gave
  the Houses ~33p a day of the ~65p they saved (the soapworks 86p of wages, 118p; Larkfield's looms 66p, 90p); the rest
  went with the crews' spending (the workshops' takings ~3% lower).
- **The workshops** (year 3, wages paid of owed): Larkfield's looms **100%** (96.0%), Larkfield's smithy **99.8%**
  (98.0%), the soapworks 100% (99.3%), Coldwater's looms and Fenshore's pottery 100%, Larkfield's press 99.5%; but
  **Oakhaven's press 94.3%** (96.3%: its pamphlets 11.9 a day, 13.7) and **Southmeadow's pottery 86.8%** (89.6%: 13.1 pots
  a day, 15.0). The ropewalk 98.9%. Every farm and works 99.8% together; orchards 2, 4 and 5 97.1-98.1% (97.6-99.1%), the
  vineyards in full (their takings 45p and 46p over their 36p).
- **The treasury over its 200p float every night** (least 1,637p; 1,568p), 4,186p to 4,799p (four-week means; 4,204p
  to 4,483p). **The wage support 318p** (324p), year 3's first and last four weeks 336p to 349p (323p to 363p): rising
  more slowly. The toll 1,664p (1,672p).
- **Cheese the rich's and the Houses', not dearer.** The dairies make 3.8 a day (5.2), all sold, none held, the shops
  short ~48 a day as before. Bought by the Houses' kin (middle-class payers) 1.6 a day for 15.3p, the upper class 1.2 for
  11.5p, the Houses' tables 0.9 for 4.3p (0.2), the lower 0.0; the households' luxuries 2.9 pieces a day for 27.2p (5.0
  for 53.9p). **Its price at the resellers 1.84 times the season's** (2.09; at most 2.55): the price is where the few
  buyers' budgets clear it (the rich's 10% of what is over their keep, the Houses' tables 40% of a thin spare), so less
  cheese brought fewer pieces bought, not a dearer piece. Nobody hungry or behind for it.
- **Butter** 37.3 a day made (34.2), all sold, still short ~76 a day. **Wine** 6.1 pressed and 5.3 sold, 296 in the
  cellars at the end (1), the vintner short 0.9 a day (5.2): the halved cut about right. **Meal** short 10.8 a day
  (42.6), the pigs 3.2 (4.3).
- **Piles, smaller but still growing**: the farms' tallow 5,672 (7,366), +6.4 a day; flax 13,219 (19,960), +19 a day (the
  grain farms' 31.5 grown each against 22.1 sold: country6a's 31.2 "grown" was net of its full store, so the half-way cut
  left more than meant); wool 5,972 (6,780), +6; clay 3,986 (8,367), +7.7; hemp 4,907 (10,210), +7.8; herbs 650 (453).

**Verdict: stable, short of the marks.** Money, food, the posts and the treasury hold for three years, the Country's
posts paid every night and its sales of the workshops' goods gone; the smithy and Larkfield's looms now paid. Short of
the marks: the Country's summer 5.7% over its posts (the target 10%) and its purse still drifting down; the wage support
still rising slowly; Oakhaven's press (94%) and Southmeadow's pottery (87%) thin trades for their crews; cheese ~1.8
times, not 3-6 (more money chasing it, not less cheese, would raise it: the Houses' tables' share, the user's call); the
flax still piling (cut it to ~0.70 of now), wool, clay, hemp and tallow slowly; butter short; hunger a touch up.

### Phase 6, the estates split (2026-11-04, branch `r2-estates`)

Branch `r2-estates`, from `ridgemere2` (4e018b6). **Nothing written to DEV, no NPC made, no map change, no post added or
dropped**; the treasury's levers (toll, support, float) untouched. Scripts in the session's scratchpad (`est/`:
`posts.py`, `books.py`, `smoke.py`, `support.py`).

**The books per House first** (country6b's year 3, days 730-1094, a day; `country6b.jsonl`'s rents by House and posts by
rule, the full log's trade rows for the Country's own sales; every one of the 100 Country posts matched to its House from
build 61's export by its label and its workplace's cell, `posts.py`; each post 8p, a head 14p):

| Estate | Its lands and trade (in) | In | Its posts | Out | Over (+) or short (-) |
|---|---|---|---|---|---|
| **House Grayrock** (the wharf) | the fish land rents 21.6p; the Country's own sales through the store room 176.3p (iron bars 159.1, limestone 13.4, lime 1.7, ores and copper 1.9, linen 0.1: no ships' goods beside them) | 197.9 | 58: the sea trade's 52 (the piers 31, the cranes 4, carrying up from Sump Stairs 10, the Wharf Office's clerks 2, the stores' keepers and tallies 5), Grayrock Hall's 4, the harbourmaster and the wharf's overseer | 476 | **-278** |
| **House Timberland** | the woods' rents and Oakhaven's press 130.1p | 130.1 | 9: the Iron Store's 2 (its keeper and its tally; their rule is the sea trade's, their store Timberland's since doc 67's renames), Timberland Court's 7 (the iron trade's runner among them) | 72 | **+58** |
| **House Greatly** | the quarry's, the pits' and the two potteries' rents 48.8p | 48.8 | 15: Greatly House's 10, the stables' 2, the Counting House's 2 clerks and its overseer | 126 | **-77** |
| **House Fell** | the orchards', vines' and hives' land rents 25.2p | 25.2 | 7: Fellhall's 6 and the Fell Cellars' keeper | 56 | **-31** |
| **the Crown** | the grain's, herds' and salt's land rents 386.9p; the workshops on Crown land 105.0p (the soapworks, both looms, Larkfield's press and smithy) | 491.9 | 11: the Minor Houses' 9 (Carlisle 1, Grantham 3, Stanmore 1, Thornbury 2, Vance 2 with its lender), the Harrowgates' 2 (the King's household) | 94 | **+398** |
| all | | 894.0 | 100 | 824 | +70 (its basket 48, its patrons 15, the Houses' tables 7) |

By season the three short estates want 368, 401, 402 and 374p a day (spring to winter) against the Crown's 385, 413, 420
and 373p over its own posts: **the Crown can carry them, by 12p a day over the year (-1p a day in winter)**; Timberland's
58p (116p in winter) is the only other spare. No estate but the Crown and Timberland can pay its own people: the wharf is
nearly six-sevenths of the posts and has a tenth of the land.

**The design, and why.**

- **Grayrock's estate is the Country's own purse** (`outside:country`, unchanged in code, saves and reports): Grayrock
  runs the wharf (doc 67, R28), and the sea trade's money is the wharf's: the goods landed and sold through the store room
  (the iron and the lime as well, while they stay the Country's), the basket of the city's exports it ships out, the
  arrivals off the ships. Splitting the wharf from the sea trade would need a new fee (a landing due) whose size is a
  lever nobody has set; one purse does it with none. "Beyond the realm" stays the Country's trade, kept by the wharf.
- **The other four each a purse of their own**, `estate:greatly`, `estate:timberland`, `estate:fell`, `estate:crown`
  (`country.json` `estates`): a works' rent, a workshop's share and a food farm's land rent go to the purse of the House
  whose land it is (`Society::houseFund`, by the same `house` as the reports); a Country post (a `wages` rule paid by the
  Country) is paid by the first estate whose `words` are in its label, else in its workplace's cell, else the Crown's
  (`estateOfPost`): the Minor Houses' people (no land in the realm) and the Harrowgates (the King's household). The wage
  rules and their report lines (the sea trade, the Houses' people, the Houses' heads) are as they were.
- **Posts first, then a float, then buying.** Each estate keeps countryPostsDays (2) nights of its own posts and its share
  of the Country's reserve (by its posts' wages: the wharf 115p of the 200p) before it buys anything (`countryKeep`). With
  what is over, every estate buys **the same basket** (the realm's trade beyond it, through the wharf), **its own House's
  tables** (the luxuries, luxuryHouses of its own spare), and the patrons' work (the Houses' retainers, halls and the
  wharf's hulls: each unit by the estate with most to spare; in a first smoke with the hulls the wharf's own, the
  Country's patrons fell to 20p a day from 38p, the wharf never having spare). So no House's money piles up idle, and the
  city's exporters keep their market.
- **The Crown's grants** (`settleEstates`, at 18:00 before the wages): any estate that holds less than tonight's posts
  gets what it lacks from the Crown's estate, out of what the Crown holds over its own posts tonight (its own paid
  first; short of the whole, each the same share of what it lacks), journalled "a grant from the Crown's levy on the
  realm's land: <estate>" and booked (grants in and out). Never the treasury. The Crown's keep counts what the others
  lack of their own two nights, so it buys only once its grants to come are in hand: its purse is the realm's buffer
  (~1,500p in the smoke), the wharf's stays at its posts.
- **Money conserved**: every move is a shift; nothing made. **The old purse's money split by the posts' wages** the first
  night the rule is on (a fifth of the posts is a fifth of the money): Grayrock keeps 58%, the Crown and Greatly 11% and
  15%, Timberland 9%, Fell 7%; the wharf's share is the largest because its posts are.
- **Switchable**: the rule `estates` (1, on; Core's `Rules`, `country.json`, the Editor's and Atlas's Economy rules, no
  migration). Off, every post, rent and purchase is the Country's one purse as before, no estate purse is opened, and an
  estate's money goes back into the Country's purse that night and the emptied purse is closed, so that switched on again
  the purses open anew and share the Country's money as on the first night (the review: left open and empty, the estates
  started with nothing and the Crown with nothing to grant, their posts unpaid that night); a purse opened later shares in
  the Country's money alone, never taking an open estate's.
- **The reports**: `world_check`'s economy lines carry `estates` (each estate's rents, grants in and out, posts owed,
  paid and held, what it bought, the money moved by a split, its purse); `econ_report` puts a table of them under the
  Country's lines. The DM's Money tab lists the estates' purses with the Country's, read only (Atlas's Economy page shows
  no purse, so nothing there).

**Grants expected** (from the books): about **386p a day** at country6b's year 3: the wharf ~278p, Greatly ~77p, Fell ~31p;
the Crown left ~12p a day over them (its winter ~1p short, carried by its buffer). Thin: 79% of the Crown's income is
the grain's, the herds' and the salt's land rents. If they fall, the next step is a levy on the other estates' spare (Timberland's
~58p), not the treasury; not built.

**Tests.** `country_behaviors` (700): each post its House's estate (by its label, then its cell; the Minor Houses' the
Crown's); until the first night all the Country's; the first night the purses opened and the money split a fifth each by
five 8p posts; each estate paid its own people; the Crown's grants exactly what Greatly (5p) and the wharf (8p) lacked,
recorded, every post paid, the Crown's own post first; the Crown short (10p): its own 8p paid, the 2p shared; an estate with
only its two nights buys nothing, with more it buys and keeps them; the rule off: no estate opened, every post the
Country's, and switched off after a split every estate's purse emptied into the Country's and closed, switched on again each estate its share anew and its own people paid; conserved throughout.
`workshops_behaviors` and `farms_behaviors`: a workshop's share, a works' rent and a land rent now to their House's purse.
`ctest` in `build-core` **79 of 79**; `test_econ_report` (the estates' table), `test_item_catalog`, `test_farms_summary`,
`test_country_industry`, the Editor's typecheck and `economy.test.ts` pass (`test_map_editor`'s and the Editor's model
test's Greyfen count, 11 for 10, fail as before; `test_dungeon_master`'s Money test needs the test database, skipped here).

**The 30-day smoke** (build 61's export, `--simulate 0 720 --farms 0 --farms-every 1 --deterministic --threads 4
--economy-level full`; days 7-29 a day unless said). **The rule off is today's single purse**: its Country's purse 2,262p
at the month's end, as country6b's smoke. **The rule on**: money conserved, nobody hungry, **every estate's posts paid every
night** (0 short); the first night 904p moved from the Country to the four new purses; a day the Crown took 653p of rents
(the first spring's) and granted 213p (the wharf 146p, Greatly 38p, Fell 26p, Timberland 3p); Timberland bought 127p of
the basket and the tables, the Crown 304p, the wharf, Greatly and Fell nothing; the purses at the end the wharf 38p (least
2p), Timberland 330p, Greatly 113p, Fell 33p, the Crown 1,530p: 2,044p in all (the one purse 2,262p). Beside the rule off:
the basket 367p (367p), the patrons 46p (38p), the Houses' tables 19p (19p), the treasury at the end 1,933p (1,944p), the
wage support 258p (239p: the first week's 324p against 285p, the last week's 84p against 92p; the Crown buys in larger
lots once over its keep). A first spring's month: the Country's sales were 300p a day, not year 3's 176p, so its grants
were half what the books expect; a 3-year run (asked first) would show the Crown's margin.

**Open.** (1) A 3-year run with the rule on beside country6b, to see the Crown's margin over its grants (the books: ~12p a
day). (2) If thin: a levy on the other estates' spare, or the Crown's land rents up (`landRentGrain`, `landRentDairy`).
(3) Iron, the lime quarry and Frosthollow stay the Country's (the wharf's purse) until the user says.
