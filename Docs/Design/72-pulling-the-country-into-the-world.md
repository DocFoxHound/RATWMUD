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
losses can **stay on the treasury's wage support for now** (question 1).

Builds on doc 71 (the Country's industries: the food farms, Phases A to C), doc 70 (the realm and its reserved sites),
doc 68 (the Country and the city loop), doc 61 (the Houses' trades) and doc 66 (the unplaced pool). Iron stays the
Country's (the user, doc 71: "right now iron will remain in the Country").

**How it was measured.** All money is the mean of a day over **years 2 and 3 of `people3`** (the first 3-year run with
the farms' real crews, all six food areas on; `artifacts/economy/r2/people3.full.jsonl.gz`, days 365-1094), unless
marked. The Country's intake is what it keeps **after the gate toll** (30%, to the treasury). The toll on a good is
therefore 3/7 of the Country's intake from it. The reserved sites and distances are from `artifacts/realm2/realm-report.json`
(as built) and `Data/Economy/communities.json`. The unplaced people were read once from DEV's `live.npcs` (read only,
rolled back). **No simulation was run and nothing was written to DEV** for this doc. The aggregation scripts are in the
session's scratchpad (`country_next/agg.py`, `flows2.py`, `daymeans.py`, `dist.py`, `group.py`).

## Headline

- **The food has left the Country.** It sells no food and its basket buys no food or drink. What is left is **1,393p a
  day** of non-food goods (3,429p before the farms), against **824p a day of posts** it pays to about 100 people.
- **The largest piece left is the woods: 436p**, nearly all firewood (283p, 450 pieces a day, 586 in winter). Firewood
  is the city's only fuel: households also want about **301 peat a day that nobody sells**.
- **The Country's posts are the blocker, not the crews.** Every industry moved out takes takings from the payer of those
  100 posts. With nothing else changed, the woods alone cut the Country's buy-back of city goods from 569p to about
  160p; the woods with stone and fibre leave it unable to pay its posts.
- **Recommended: the estates' rent.** Each new works pays its crew at the farms' fixed wages and pays **what is over to
  the House that holds its land**. At first that is the Country's own purse, relabelled "the Houses' estates", so the
  posts keep their payer and only the crews' wages leave it. Those wages are spent in the city's shops.
- **Recommended order**: (1) herbs and fibre onto the food farms that already exist; (2) **fuel**: two logging camps,
  two coppices with a charcoal hearth, and a peat cutting; (3) stone, lime, clay and sand; (4) the city's makers first
  for craft inputs; (5) the country workshops, when the communities become towns; later, iron.
- **What stays the Country**: iron and metals (for now), the sea trade (the wharf's carriers, the ships' goods and the
  city's exports), the arrivals and their purses, and the unplaced pool.

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
  builders' yard, the wharf's hulls at the shipwright) fell from 50p a day to **0** from year 2. The shipwright is a
  till short of its wages (17,696p over 3 years).
- **Its basket is what gives.** Wages come first, so every penny of intake lost comes off the basket (1b).
- **Arrivals** (about 4 a year) and the community markets' people get their purses (up to 30p) from the Country's purse.
  The ~369 unplaced NPCs are the Country's people too.

### 1d. The treasury, for reference (people3, years 2-3, a day)

Toll in **1,651p** (of it ~597p on the Country's goods, the rest mostly the farms'); alms 318p; the surplus paid out
only **124p**; the short tills' wage support wanted 245p; its level about 5,300p. **The treasury has little to spare**:
it can't take over posts of any size without a new source (open question 1).

### 1e. Wanted and short outside the farms

- **Peat**: households want about **301 a day** (middle 182, lower 103, upper 16) and none is sold, on all 1,096 days.
  `crafts.json` has the producer ("cuts peat", 4 peat a spell, line 145) and the need ("any firewood or peat", line
  208), but no post makes it. Firewood is bought instead.
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
| + stone out, no rent | 810 | 824 | **-14: posts unpaid** |
| + fuel out, **with the estates' rent** (Phase 2) | 1,159 | 824 | 335 |
| + stone, with rent (Phase 3) | 1,077 | 824 | 253 |
| + the city makers first for craft inputs (Phase 4) | 890 | 824 | **66: too thin** (summer's intake runs ~150p under the year's mean) |

**Options for the payer**, from the least change:

1. **The estates' rent (recommended).** A works' till pays its crew's fixed wages, keeps a reserve (two days' wages),
   and pays the rest each night to the House that holds its land. At first all Houses share one purse: the Country's,
   relabelled in reports "the Houses' estates and the sea trade" (`outside:country` unchanged in code). The Country then
   loses only the new crews' wages, which come back to the city's shops as the crews' spending. Doc 61 already gives the
   Houses their trades: **Fell** timber and woodwork (the woods, the fuel), **Greatly** building materials (the quarries,
   the clay; `quarry_granite` is already marked Greatly's), **Grayrock** stone and iron (iron, later). Phase 6 can split
   the one purse into each House's estate, each paying its own people.
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
makers that only the Country buys from (paper, steel, rivets, glass beads, maps, handbells: ~340p of the 569p) go short
as its basket shrinks, and the treasury pays their wages down to the drift's floor.

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

### 4.3 Fuel: the woods and peat (House Fell)

The map has five timber cuts and two charcoal hearths reserved, but **no coppice, sawmill or peat site**. The city
already has its own sawmill (Fell's) and charcoal works, so **no country sawmills** are needed: planks come from the
city's sawmill (Phase 4).

| Works | Site (as built) | Goods | Crew | Lives in | Walk |
|---|---|---|---|---|---|
| Rainwood logging camp | `timber_rainwood` (652,1092) | timber, firewood, oak bark, resin | 1 + 3 | **Ridgemere** | ~470 from the South Gate |
| Front logging camp | `timber_front` (1692,948) | timber, firewood, oak bark | 1 + 2 | **Coldwater** | ~1,040 (as the Greyteeth fold's) |
| City coppice | **new**: the wood pasture by the walls (`range_city`, about 620,636) | firewood | 1 + 2 | **Ridgemere** | under 200 |
| Greenholt coppice and charcoal hearth | `charcoal_greenholt` (1260,2124) | firewood, charcoal | 1 + 2 | **Larkfield** | ~570 |
| Fens peat cutting | **new**: the Sallow Fens near Fenshore | peat | 1 + 1 | **Fenshore** | under 400 (fen walking) |
| **in all** | 5 works | | **5 keepers, 10 hands (180p a day)** | Ridgemere 7, Coldwater 3, Larkfield 3, Fenshore 2 | |

- **Takings**: firewood 283p, timber 84p, charcoal 32p, oak bark 7p = **406p**. Peat takes part of firewood's place (it
  is the same 1p and households take either), so the fuel's pennies stay about the same; firewood draws fall by
  whatever peat fills. Over wages: **~226p to Fell's estate** (the Country's purse at first).
- **Yields**: timber ~65 a day, firewood ~320 (winter ~430), peat ~200-300 (winter more), bark ~12, charcoal ~12. The
  winter peak (586 firewood) is the gate: the fuel must never run out.
- **Crew**: 13 from the **woods** pool (25 loggers, 5 woodcutters; 36 in all), 2 for the peat from **keeps the house**
  (nobody in the pool cuts peat). 21 of the woods' people stay unplaced.
- **Shops**: the general store, provisioner and bakeries (city and community) draw fuel from the works; the sawmill,
  ironworks and foundry timber; the tannery bark. The city's charcoal works keeps its kilns (its only buyer is now the
  Country: 8 a day).
- **Why not Oakhaven now.** The pool's loggers are Oakhaven's, and Oakhaven (1012,1220) is within ~420 of the Rainwood
  sites. But the Rainwood camp is close enough for Ridgemere, and the user wants towns later. Oakhaven is the woods'
  town when the communities grow (open question 4).

### 4.4 Stone, lime, clay and sand (House Greatly)

The map has four quarries (granite, slate, lime, sandstone) and two clay pits, both in the southern fens; **no sand
site**. The game's clay pit yields clay and sand together (4 + 1 a spell), so the pits can be the sand's source.

| Works | Site (as built) | Goods | Crew | Lives in | Walk |
|---|---|---|---|---|---|
| Granite quarry | `quarry_granite` (1028,356), House Greatly's | stone | 1 + 2 | **Ridgemere** | ~320 from the East Gate |
| Lime quarry | `quarry_lime` (1588,788) | limestone, lime | 1 + 1 | **Ridgemere** (or Coldwater) | ~960-1,140 |
| Mirebridge clay pit | `clay_mirebridge` (1180,3276) | clay, sand | 1 hand | **Fenshore** | ~440 |
| Fens clay pit | `clay_fens` (1636,3332) | clay, sand | 1 hand | **Southmeadow** | ~600 |
| **in all** | 4 works | | **2 keepers, 5 hands (82p a day)** | Ridgemere 5, Fenshore 1, Southmeadow 1 | |

- **Takings 123p**, flat across the year; over wages **~41p to Greatly's estate**.
- **Crew**: 7 of the 10 in the pool's stone and clay trades (Greyscar's stone cutters, the lime kilns' man, Clayholt's
  diggers). The slate and sandstone quarries stay reserved (nothing draws slate; the sand comes from the pits).
- **Shops**: brickworks, pottery, mason, plasterworks and glassworks draw from the works. The clay comes 2,500 tiles to
  the city; carriage is a flat 6p a load, so the shops' costs don't move.

### 4.5 The city's makers first (no new sites)

Extend `suppliers` and call `buyFromCity` for a shop's resale draws too, so a shop buys from a city maker before the
store room. **Only craft inputs at first** (the store charges them at 100%, the city maker's catalogue price is the
same, and till to till there is no toll and no carriage, so the shops' costs don't rise):

| Good | Country intake now | City maker |
|---|---|---|
| linen cloth | 82p | weaver (flax from the grain farms, 4.2) |
| iron hoops, nails, cookpot | 37p | smithy, cooperage (iron bars still the Country's) |
| planks | 30p | Fell's sawmill (timber from 4.3) |
| cord | 20p | chandlery (hemp from 4.2) |
| paper, leather, bandages, ink | 18p | papermill, tannery (the farms' hides), barber, ink makers |
| **in all** | **~187p** | the makers gain ~267p of sales; the treasury loses ~80p of toll |

**The resale goods** (pamphlets 114p, fine soap 96p, wool scarves 92p, clay pots 51p) are left to Phase 5: from a city
maker they would cost the general store and the rag-and-bone man a third more than the store room's 75%, and both are
already short of their wages.

**The money**: with Phases 1-3 done this leaves the Country only ~66p a day over its posts (3), and less than nothing in a lean summer. So
Phase 4 needs a second payer first: a small land rent from the food farms (open question 2) or fewer wharf posts.

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
| A town | **Frosthollow** (2308,796), reserved: 220 from `mine_frost_iron`, 459 from `timber_upper`. The pool's iron people are Frosthollow's (12 ore diggers, 3 smelters, 3 traders, its shop people). It would be a sixth community. |
| Mines | `mine_frost_iron` (near Frosthollow). The second, `mine_grey_iron`, is 1,145+ from any home: either a Greyteeth miners' camp, or the `mine_frost_lead` site re-used as a second iron working (lead and silver have no demand). |
| Smelting | **No bloomery site is reserved.** Doc 71's earlier answer stands: the realm mines ore and the city's Grayrock Ironworks and foundry smelt it (38 workers at the game's rates against 14 payable). |
| Charcoal | the reserved hearths are in the Rainwood and the Greenholt, far from the mines. A hearth by `timber_upper` would serve Frosthollow. |
| Copper and tin | `mine_ochre_copper` (Saltwell, ~750): 3 workers on ~24p a day; not worth doing alone. No tin streams site; the Greyteeth tin mine is 1,300+ out. |
| Money | **271p a day** (iron bars 237p). Pulling it leaves the Country with only the workshops and the rents, so it needs the per-House split (Phase 6) first. House Grayrock's estate. |

### 4.8 Summary

| Industry | Phase | Takings | Crew (wages) | Over wages, to the estates | Sites new to the map | From the pool | Lives in |
|---|---|---|---|---|---|---|---|
| herbs | 1 | 11p | 0 | (to the marsh's till) | 0 | 0 | |
| raw fibre | 1 | 43p | 2 (20p) | (to the grain farms' till) | 0 | 2 fields and mills | Larkfield, Southmeadow |
| fuel: woods and peat | 2 | 406p | 15 (180p) | ~226p, Fell | coppice by the city, peat at the fens | 13 woods, 2 keeps the house | Ridgemere 7, Coldwater 3, Larkfield 3, Fenshore 2 |
| stone, lime, clay, sand | 3 | 123p | 7 (82p) | ~41p, Greatly | 0 | 7 mines, stone and clay | Ridgemere 5, Fenshore 1, Southmeadow 1 |
| city makers for craft inputs | 4 | 187p | 0 | (to city makers) | 0 | 0 | |
| country workshops | 5 | ~408p | ~29 | to be sized | the workshops in the communities | ~10 by trade, ~19 fallback | the five communities |
| iron and metals | later | 271p | ~14 | Grayrock | Frosthollow, a bloomery | 18 metals | Frosthollow |

Phases 1-3 place **24 people** (beds: Ridgemere 12, Coldwater 3, Larkfield 4, Fenshore 3, Southmeadow 2) and leave the
Country with ~253p a day over its posts (from 569p).

## 5. The recommended order

Food-adjacent and largest money first; each phase as doc 71 did it: **modelled producers with virtual crews first
(Phase B), then real people (Phase C)**. Every run needs the user's go-ahead.

| Phase | What it builds | Checks | Runs (each asked first) |
|---|---|---|---|
| **0. Works as areas, and the rent** | Generalise `Core/RatwFarms.cpp`'s areas to non-food works (verify nothing assumes food: the Country's table, `seasonal` groups, the stew); a works' `rent` rule (wages, reserve, the rest to the estates' purse); the Country's report lines split (estates' rents, the sea trade, posts by rule); count the 100 posts by rule and by House exactly; Atlas's and the DM's Economy panels show works beside farms. No world change. | C++ tests: a works yields, sells through the store, pays its crew, pays its rent, money conserved; switched off, the Country supplies again. | none |
| **1. Herbs and fibre on the farms** | Herbs as yields of the eel marsh; hemp and flax fields on the two grain farms (yields, +1 hand each, placed by Phase C's rule from fields and mills). | farm_people tests for the extra hands; the grain and fish areas' tills still pay their crews. | 30-day smoke; folded into the next 3-year run |
| **2. Fuel** | The five works of 4.3 as modelled producers (Phase B), the two new sites placed on the map (doc 70's tool, a DEV build), then their people (Phase C). Peat sold through the general store and provisioner. | The fuel never short (firewood or peat filled 99% of days, winter included); peat sold; every works pays its crew 99% of days; the Country pays its posts every day; the treasury over its float every night; no hunger; money conserved. | 30-day smoke; **3-year full run** (`--economy-level full`), with the report's patch for the yields |
| **3. Stone, lime, clay, sand** | The four works of 4.4, modelled then real. | As Phase 2, plus the builders' trades' draws filled. | 30-day smoke; 3-year run (can share Phase 2's second run) |
| **4. The city's makers first** | `suppliers` widened and `buyFromCity` for resale draws, craft inputs only (4.5); with the second payer the user picks (question 2). | Makers' tills, the general stores' tills; the Country's margin in summer; the treasury's toll and support. | 3-year run |
| **5. The country workshops** | When the communities become towns: the soap boiler and chandler first, then pottery, weavers, press, smithy. | As Phase 2, plus the candles no longer short. | per workshop group |
| **6. The estates split; iron** | Each House its own estate purse paying its own people; the sea trade alone the Country. Iron and Frosthollow when the user says. | | |

## 6. What stays the Country, and why

| Stays | Why |
|---|---|
| **Iron and metals** (271p) | The user's decision. Its sites are reserved; 4.7 says what pulling it needs. |
| **The sea trade**: the wharf's carriers and the stores (~56 posts) | Ships come from beyond the realm. The wharf is where the outside meets the city; its work is the outside's. |
| **The city's exports** (the basket's maps, handbells, glass beads, charms, steel) | What the city sells beyond the realm. These makers have no buyer in the realm; the Country's basket is their market. |
| **The Houses' estates** (until Phase 6) | The land the works stand on is the Houses'; the estates collect the works' rents and pay the Houses' people. Not a producer any more, a landlord. |
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
3. **Peat.** Recommended: **a peat cutting at the Sallow Fens**, crew at Fenshore from those who keep the house. It
   fills a want short every day and makes a second fuel. Or leave peat unsold (firewood stays the only fuel).
4. **The second logging camp and Oakhaven.** Recommended: **`timber_front`, crew at Coldwater**, and Oakhaven waits for
   the towns. Or `timber_sound` with its crew in Ridgemere (~920 walk), or build Oakhaven now as a sixth community for
   the woods (the pool's loggers are its people).
5. **Two new sites for the map**: a coppice by the city's walls (the `range_city` wood pasture) and the peat cutting at
   the fens. Recommended: **yes**, placed by doc 70's tool. Or coppice inside an existing timber cut (one fewer site).
6. **Stone.** Recommended: **two quarries (granite, lime) and the two clay pits as the sand's source**; slate and
   sandstone stay reserved. Or all four quarries (needs 3 more crew on 123p).
7. **The city's makers first.** Recommended: **craft inputs only** in Phase 4; the resale goods (pamphlets, soap, scarves,
   pots) with the workshops in Phase 5. Or route everything at once (the general stores and the rag-and-bone man pay a
   third more, ~150p a day more toll lost, and the Country loses another ~353p).
8. **The farm goods no farm grows.** Recommended: **imports by sea** from the Country (it keeps a small sale of them,
   seasonal), until farms grow them. Or leave the shops without them, as now.
9. **The works' wages.** Recommended: **the farms' fixed wages** (16p a keeper, 10p a hand), editable per works and per
   area in Atlas and the DM's Economy tab, as the farms'.
10. **The workshops' first step.** Recommended: **the soap boiler and chandler first** when the communities grow (the
    farms' tallow piles up and the candles are short). Or the potteries first (by the clay pits of Phase 3).

## Progress
