# 71. The Country's industries (2026-11-01)

The user, 2026-11-01 (verbatim):

- "What we are going to need is the data and metrics on what industries the Country holds, what the income looks like,
  and start calculating how many NPCs can be supported in each industry that Country now covers, and also what levels
  of production are required to sustain Ridgemere as well as themselves. I would like multiple farms, if at all
  possible, as well as multiple facets for other industries where it makes sense. Let's start with food production,
  though, and start converting that into physical locations."
- (9) "Equate how many farms it would take and at what production levels to produce the food stocks that the Country
  now provides, including fishing and livestock."
- (10) "Equate how many NPC's would be needed, and whether or not funding is adequate to keep them employed through
  harvest and sowing without much disruption."
- "We aren't going to place any NPC'S or farms or other things from Country yet, just the map and physical locations
  for them."
- On the method, after a first draft sized from real yields: "I think what we need to figure out is the yield size. We
  don't need to stay realistic to actual yields, but I don't want it all from single sources. What we need to do is
  figure out how many wolves each area of food can pay for and go from there, and decide how many 'farms' could look
  reasonable, and how much their yield needs to be about to keep pay and prices stable and food going where it needs
  to."
- "make sure that yield is adjustable in Atlas' economy controls."

The user's decisions on the second draft, 2026-11-01 (verbatim), all applied below:

- "Yeah we can drop vegetables entirely. These are wolves after all." No market gardens (section 4 lists where
  vegetables are, for the build step; the item stays in the catalogue for the old map).
- "If there aren't enough pennies for sensible crew for those things, perhaps we collapse one source into the remaining
  and increase yield. So instead of 3 farms we will have 2 large ones with yield to cover it. Same for one vineyard and
  one salt mine." Two large grain farms with the water mills, one large vineyard, one salt works; the same rule for
  every other area. (With the corrected money the pennies would now pay more of them: open question 1.)
- The farms' basket (their upkeep buys the city's goods): "Yes, do your recommendation", sized to what the farms can
  afford (2.6).
- Iron: "right now iron will remain in the Country. We will get around to figuring it out later." Mining stays the
  Country's; its sites are reserved on the map (section 5).
- The fields on the map: "size them somewhere in the middle. They shouldn't be massive but a player should feel like
  they're big": about 14 cells, 6.5 of the farms' own ground and 7.5 of shared range (section 6).
- The map is widened, not remade: Ridgemere is the north-west-most city and region with the sea to its west; new land
  goes east and south (snowy mountains east of the city along the north, hills and plains in the middle, a marsh south
  of the city along the coast, a desert far to the south-east). Regions are given by description; their names are the
  map's.

**What changed since the second draft** (two independent checks' corrections, all applied, with one model of how the
game pays and feeds a wolf):

- A wolf costs its farm its wage; its food is bought out of it in the city's shops and comes back to the farms as draws
  (the second draft added the food on top). The farms' takings include the crews' own food and spending.
- **Wine in autumn was an artefact.** The autumn short run's wine vaults drew and sold no wine; the long run's sold it
  all year, and nothing in the code makes a vintage. Autumn's wine is filled in from the long run (How it was
  measured). The vineyard has no lean season: its crew is 1 + 6 and its reserve two days' wages, not 2,300p.
- **The baskets add up.** Each farm's basket follows its till over the year (a lean season's shortfall is earned back
  before the basket resumes), so wages and baskets come to the takings exactly. The basket's carriage out (12p a day)
  is shown.
- **The yields in `country_farms.json` are middle-day values** whose blend (`Society::seasonal`) has the season means of
  the tables (2.4).
- **One rule for how many farms**: a kind keeps its farms while each pays its smallest sensible crew in every season.
  Five orchards (not four), two of them with a drying house, which alone make the dried apples and preserves; the two
  lake and river boats stay.
- **Only the granary sells at a price a year**; fruit keeps the season's price, which steadies the orchards' takings
  (apples are fewest when dearest; at a price a year the plain orchards' winter would be 37p against 36p of wages).
- **The switch's other costs are counted**: the money the farms' floats and the crews' purses hold (section 3), and the
  wage support the trades that lose the Country's orders would draw from the treasury (2.6).
- Salt scales with the fish and meat it is salted for (107p, not 74p); each season is priced at its own mean factor;
  the residents eat NourishADay.
- Every works site of the other industries has a crew (the woods are 8 sites, not 13; the herbs have none of their
  own; the fibre is a field at each grain farm); per-area wages and crew controls; vegetables are out of the draws after
  the switch; the opening stock is created, not handed over; the NPC pool after the arrivals is counted by working age;
  the old map's names are gone; the footprints are re-derived to the user's scale.

**And the third check's corrections** (all applied):

- **The treasury after the switch** is worked out on the settled years' trades (their wage steps and orders: the short
  runs are the founding world) and night by night with its surplus and alms (2.6). It costs far more than the last draft
  said: the trades only the Country's basket kept reach the wage floor still short, so their support never ends.
- **The wolves the food can pay** on wages alone are solved with the extra crews' own food and spending: 256, not 212.
- **Without the farms' basket** the comparison keeps the money circulating (every penny on wages instead). The Country
  then about breaks even, so its margin rests on the farms spending their takings in the city.
- **The season caps**: after the switch the city draws more meat and grain than the caps allow. A farm's goods are
  limited by its stock, not by the caps, and an area's goods go into `country_off`, with no fallback to the Country
  (section 4).
- The other industries' wolves are whole wolves (30, 14, 7, 7), as `country_farms.json` has them.

Analysis only: no game rule, setting or world is changed, DEV is only read, no simulation was run for this draft.
Builds on doc 68 (the Country and the city's loop, all its Progress sections), doc 61 (industry of the realm) and doc 60
(the old map's farms; none of its names are used).

**The tool**: `tools/country_industry.py`. **The farms' settings and the placement list**: `Data/Economy/country_farms.json`
(nothing reads it yet; it is the defaults Atlas would edit, section 4).

## The lead table

What each food area pays for, its farms, their crews and yields. A day, the 12-year run's settled years. **Revenue** is
the farms' takings after the switch: the Country's part of what the city's shops pay for the area's goods (the cost of
the draw less the gate toll; the shop pays the carriage on top), the crews' own food and spending included (2.2).
**Wolves it can pay**: at the city's wages (a keeper 16p, a hand 10p) if every penny of its takings went on wages,
solved with the extra crews' own food and spending, which come back as takings. **Crew**: what about
half the takings pay, never under a sensible crew for the kind; the other half is the farm's basket of the city's goods
(2.6). **Yield**: a farm's pieces a day, all its goods but the herds' by-products, season means (each good: 2.4).

| Area | Farm | Revenue a day (all its farms) | Wolves it can pay | Farms | Crew a farm | Yield a farm a day, Sp / Su / Au / Wi | Region (distance) |
|---|---|---|---|---|---|---|---|
| grain and flour | grain farm with a water mill | 208p | 24 | **2** | 1 + 4 | 94 / 111 / 114 / 90 | the central plains, each on its own river east of the city (middle) |
| orchards and vines | orchard with a drying house | 174p | 19 | **2** | 1 + 3 | 102 / 87 / 89 / 126 | a south-facing slope of the central hills (middle); the sheltered valleys of the coast south of the city, cider country (near) |
| | orchard | 153p | 15 | **3** | 1 + 2 | 71 / 86 / 87 / 57 | a river valley of the central hills, and a valley where the hills meet the plains (middle); the coast's valleys south of the city (near) |
| | vineyard | 151p | 18 | **1** | 1 + 6 | 44 / 46 / 51 / 36 | the warm south-facing hills at the desert's edge (far) |
| meat, milk and the herds | cattle and dairy farm | 325p | 36 | **3** | 1 + 4 | 77 / 83 / 81 / 71 | the wet coastal pasture just south of the city (2), the meadows just east of it (1) (near) |
| | pig farm | 342p | 39 | **3** | 1 + 4 | 36 / 35 / 33 / 34 | the central hills' oak and beech woods, on the mast (middle) |
| | sheep and goat run | 192p | 22 | **2** | 1 + 3 | 24 / 27 / 24 / 26 | the central hills' downs; the foothills of the northern mountains, with the goats (middle) |
| | rabbit warren | 110p | 12 | **2** | 1 + 1 | 98 / 94 / 102 / 104 | the dunes and sandy heath of the marsh's coast (middle); the desert's sandy edge (far) |
| fish and shellfish | sea fishing boat | 269p | 30 | **4** | 1 + 2 | 70 / 57 / 60 / 82 | the sea off the city's wharf (2), a fishing cove on the coast south of the city (near); the marsh's coast (middle) |
| | lake and river boat | 72p | 7 | **2** | 1 + 1 | 30 / 29 / 28 / 35 | a lake under the northern mountains; the largest river of the plains (middle) |
| | eel and fowl marsh | 48p | 5 | **1** | 1 + 1 | 40 / 38 / 37 / 46 | the marsh south of the city (middle) |
| honey | beekeeper | 141p | 15 | **3** | 1 + 1 | 25 / 29 / 28 / 22 | among the orchards of the central hills (middle) and of the southern coast (near); the desert edge's thyme scrub (far) |
| salt | salt works | 111p | 13 | **1** | 1 + 4 | 103 / 96 / 97 / 104 | the marsh's coast: sea pans, sun in summer, boiled over peat the rest of the year (middle) |

**In all: 2,295p a day of takings. On wages alone the food areas could pay about 256 wolves, their takings and wages
then 2,730p a day** (the extra crews' own food and spending come back as takings; today's 2,295p split at 105 crew would
pay 212). **The recommended 29 farms with crews of 105 (29 keepers, 76 hands) cost 1,224p a day in wages; their basket
is the other 1,071p: 1,059p of the city's goods and 12p of carriage out.** The yields feed the city, the crews, the
Country's own 92 who stay in the Country, and 10% slack (2.4).
Every farm pays its crew in every season, and none has a lean season (section 3).

## Headline numbers

- **What the Country holds** (section 1): 16 industries. The city draws 2,512 pieces a day from it for 5,024p, of which
  1,595p is the gate toll and 3,429p the Country's. The food areas (the herds' by-products with them) are 2,220p of the
  Country's 3,429p (65%). The Country spends its 3,429p on its 100 posts (824p), its patrons (51p), carriage out (27p)
  and its basket of the city's goods (2,527p).
- **A wolf costs its farm its wage** (the model, below): 16p a keeper, 10p a hand. The wolf buys its food out of it in
  the city's shops (6.47p a day), and the shops draw that food from the farms: 2.66p of every crew wolf's wage comes
  back to the farms.
- **The food can pay about 256 wolves** at those wages if every penny went on wages (their takings then 2,730p a day:
  the extra crews' food and spending come back). **Recommended: 29 farms and 105 crew**, each farm's wages about
  half its takings or its smallest sensible crew; the other half, **1,071p a day, is the farms' basket**: tools, carts,
  barrels, rope, nets and building goods from the city's makers.
- **How many farms**: a kind keeps its farms while each pays its smallest sensible crew in every season (the user's
  rule). Two grain farms, one vineyard, one salt works (the user's decision; the corrected money would now pay three,
  two and two: open question 1); five orchards, two of them with a drying house; no gardens. Fish has three kinds of
  source (sea boats, lake and river boats, the eel marsh).
- **The Country after the switch** (2.6): it keeps its other goods (woods, mines and metals, stone, fibre, herbs, its
  workshops: 1,158p a day), the Houses' estates and their 100 posts (824p), its patrons (51p). That pays its posts in
  every season (+214p to +359p over them), and 281p is left for its basket: **11% of today's basket survives**, and of
  its food and drink (cider, salt and smoked fish, wine, brandy) 66p of 598p. The margin rests on the farms spending
  their takings in the city: their basket's makers draw iron, timber and stone from the Country (361p of its 1,158p).
  With every penny on wages instead, the Country would be about +17p a day (6p short in summer).
- **The makers' order book** (2.6): the Country's basket falls by 2,246p a day; the farms' basket (1,059p of goods),
  the crews' food (679p) and the crews' other spending (545p) bring 2,283p, +37p in all. The trades the farms buy from
  keep their orders within 5% (the carpenters, cartwrights, masons, brickworks, plasterers, coopers, sawmills,
  ropewalk); the smithies lose 22% (the steel bars, rivets and picks no farm buys), the tannery 27%. The food shops gain
  about 18%; the general shop, the chandlers, the vintner and the rag and bone shop 28-31%. The trades only the
  Country's basket kept (the foundry, the ironworks, the distillery, the scriptorium, the paper mill, the charcoal
  burners, the cartographer) lose 89%, the brewery 39%.
- **The treasury** (2.6): the toll is 1,595p now and 1,607p after (+0.8%; with every penny on wages instead of the
  basket, 1,685p). But the trades that lose the Country's orders go short of wages, and the treasury's wage support
  makes it up: **-248p a day at once** (toll +12p, the crews' services' share +10p, wage support +269p). Their wages
  drift down a penny a post every 28 days, but most of the trades only the Country's basket kept (the foundry, the
  ironworks, the distillery, the scriptorium and the paper mill, the charcoal burners, the apothecary, the cartographer,
  the paintworks, the armoury) reach the wage floor still short: **-61p a day for good**. Replayed night by night over
  each settled year, switching all at once leaves 11,000-36,000p of the first year's surplus unpaid (the alms to the
  poorest households and the works: half to nine tenths of it), runs the treasury 1,400-2,200p under its path, and
  leaves 3,900-29,100p of the losers' support unpaid. **Recommended: re-weight the Country's basket and switch an area
  at a time** (open questions 3 and 5): -201p a day once every area is on, -11p once the wages have drifted; in the
  first year 1,700-4,900p less surplus (7-12% of it), the treasury 120-770p under its path, and up to 3,190p of support
  unpaid in the years it touches its cap.
- **The money the switch holds** (section 3): the farms' floats (2,448p) and the crews' purses once settled (6,280p,
  60p a head: a household buys comforts and services only from what is over 40p a head): **8,728p, 17% of the realm's
  money**, more than the treasury holds. Taken from the city's circulation it would push the residents' purses under the
  40p line. Recommended: found them at the switch, as the world's money was founded (open question 4).
- **Cash through the year** (section 3): employment is steady by design (fixed posts, fixed wages). The risk is cash by
  season, and the fix is that the basket, not the wages, takes the lean weeks. **Reserves: 2,448p in all**, two days of
  each farm's wages (52p to 152p a farm). A fixed basket would need 27,300 to 28,400p, more than half the realm's money,
  which nobody could fund.
- **Feeding everyone** (2.4): the yields cover the city's draws after the switch (the 105 crews buy theirs in the city),
  the Country's own 92 who stay in the Country (68 workers of its other industries, 24 of the Houses' armed retainers)
  and 10% slack.
- **The NPCs** (section 3): DEV has 524 unplaced, 401 of working age. The farms want 105 and the other industries 68;
  every old trade is covered but orchards and vines (24 wanted, 23 there: one from another trade). After the 64 arrivals
  of a twelve-year run, 337 of working age would be left, 164 more than all of them.

## The model: how the game pays and feeds a wolf

One model throughout, as the code has it:

- **A post costs its payer its wage, nothing more.** `Society::payOf` (Core/RatwCityLoop.cpp, about line 87) gives each
  post a payer and a wage; `payWages` (about line 1031) shifts the wage from the payer to the holder and nothing else.
  The wage drift's floor, `wageDriftFloor` (7p, country.json `rules`: "a day's food"), is the only place food enters a
  wage: no post is paid less than a day's food.
- **The holder feeds itself out of the wage.** The households' market (about lines 740-912) tops every member up to
  `food.days` (2) of NourishADay (45.8 nourishment, line 34), buying at the city's shops; then needs (crafts.json
  `households`), then services and comforts from a day's budget of `comforts.share` (20%) of what the household holds
  over `comforts.above` (40p) a head (line 876). In a steady state a household spends what it earns.
- **What a shop sells it draws from the store room.** `Society::drawFromStore` (Core/RatwSociety.cpp, about line 1424)
  pays the Country `cost - toll`, the treasury the toll and the carters the carriage, all from the shop's till: the
  carriage is the shop's, on top of the draw. A farm that replaces the Country gets the `cost - toll`.
- **A basket pays its carriage out.** `countryBuys` (about line 914) spends what the Country holds over its reserve and
  tonight's posts on its basket, at the catalogue's price, and pays the carriage out on what it carries away (1.08% of
  the basket). A farm's basket built the same way pays it too.
- **The treasury makes up short tills.** `supportWages` (about line 1117) pays `wageSupport` (100%) of what each till
  could not pay its posts tonight, from what the treasury holds over `treasuryCap` (2,000p); `judgeWages` lowers a till's
  wage step a penny every `wageDriftDays` (28) it was short on most days, each post to the floor at most (so once a
  till's hands are at the floor, a step lowers only its keeper). Its own posts and the alms (`giveAlms`) are paid from
  what it holds over its float (200p), not its cap; last each night, what it holds over `treasuryCap` and `surplusKeep`
  (6,000p in all) goes out as its surplus (`spendSurplus`: 80% to the poorest households, 20% to public works).

So a farm's crew are paid wages and are households in the city's loop. Their food (6.47p a wolf a day at the shops'
prices, the residents' diet) comes back to the farms as the shops' draws (2.66p a wolf after the toll), with 0.16p to
the Country (the bakers' and smokehouses' firewood). The rest of the wage goes on needs and comforts as the residents'
does: each penny of it draws 0.16p from the farms (the candles' tallow, the cider's apples, the scarves' wool) and 0.26p
from the Country. The farm's basket and the Country's are spent the same way, and their makers draw again. The tool
solves it season by season (`solve`), and each farm's basket over the year from its till (`cash_cycle`).

## How it was measured

- **The long run**: the 12-year run of doc 68, Progress, "Births follow deaths"
  (`artifacts/economy/r2/people_12year.jsonl` in the main repo), its **settled years 4-12** (days 1095-4379, 3,285
  days; residents 595 on average, 568 to 623). Its economy line has, per season group, the pieces drawn a day
  (`G_drawn`) and the season's price factor (`G_price`); not the pennies by good. Its loop file
  (`people_12year_loop.jsonl`) has each till's sales by good every 28 days.
- **The pennies by good, and who they end with**: four 14-day runs of the same binary and export (DEV build 52, read
  only), one starting in each season (`world_check EXPORT --simulate 6 342 --no-check --deterministic --threads 2
  --no-profile --day N --economy F --events E`, N = 30, 122, 213, 303). The `--events` file has every ledger record: a
  draw's three coin shifts (to the Country, the gate toll, carriage in) come just before the `made` record that names its
  good, so each draw's good, pieces, pennies and reason pair up exactly; every sale names its shop and buyer (the shop's
  business read from the export's work labels, as `items::businessFor` reads them); each trade's wages, keepers' profit,
  the treasury's wage support to its posts and the city's share of its services. Days 4-14 of each run are kept, and the
  run's economy lines for the same days (they match the events' sums to the penny).
- **Wine in autumn, filled in**: the autumn run's wine vaults (the city's one vintner) drew and sold no wine in its kept
  days (it held only cider), though the long run's sold wine all year (12.6 / 11.4 / 13.9 / 11.1 a day by season), its
  shelves holding about a week, and nothing in the code makes a vintage (wine is in no season group). Autumn gets the
  other seasons' mean of the vintner's wine draws and sales, times the long run's autumn over its other seasons (1.19):
  24.3 drawn a day (`fill_wine`). The rest of the scaling then takes it in.
- **Scaled to the long run** (`scale`):
  - a season group's goods: pieces by the long run's `G_drawn` over the short run's; pennies by that and by the season's
    own mean price (`G_price` over the settled years' season, against the short run's window, which sits just before
    the season's middle: fruit in autumn 0.809 in the window, 0.889 over the season);
  - a good outside the groups drawn for a craft whose other inputs are in a group follows that group: **salt** for salt
    fish follows the fish, for jerky and salt pork the meat (107p a day to the Country), firewood for bread the grain;
  - the rest (wine and cider resold, the non-food crafts) take what is left of the long run's drawn pennies (factor
    0.79-0.85: the long run's Country bought less than the short runs' did);
  - the toll at the long run's rate (31.7%);
  - the residents' food at NourishADay a head (the short runs' households ate down their starting stock, so bought
    less than they ate), their other buying by the rest of their spending, the Country's by its spending.

  The totals match the long run's own (5,024p drawn, 1,595p toll); that is by construction, so it confirms nothing.
- **Who each draw ends with** (`streams`): a draw ends in its product (the good, resold; else the craft's output), and
  the product's buyers share it by their pieces: the residents' food, the residents' other buying, the Country's basket;
  a product neither buys is the city's (the treasury's works, the patrons, a maker's stock).
- **The trades as they stand in the settled years** (the treasury, 2.6): the short runs are the founding world (559
  residents, every till's wage step about nought), so each trade's posts are paid at its mean wage step over the
  settled years (the loop file's per-till records, every 28 days; their running sums of wages owed and profit are empty
  at those snapshots, which fall on the tills' judgement days), and its tills pay them from their margin on its orders
  now (the order book's sales, scaled, times the short runs' margin on a penny). **The treasury's nights**: the long
  run's economy lines, day by day (`replay`; with nothing switched it gives the long run back to the penny).
- `python3 tools/country_industry.py measure ... --world EXPORT/world.ratw --out MEASURED.json`, then `report MEASURED
  ECONOMY.jsonl --unplaced U.json --units Data/Economy/country_farms.json` prints every table below (the loop file is
  found beside the economy file, or `--loop`), and `--real-check` the real-yield cross-check (2.5). The measured file
  and both reports are kept, uncommitted, in the main repo's `artifacts/economy/r2/` (`country_measured.json`,
  `country-industry-report.md`, `country-industry-real-check.md`).
- **DEV's unplaced NPCs**: read from `live.npcs` (no home) with the editor role in a read-only transaction: **524**.

## 1. What the Country holds now

### 1a. By industry (a day; settled years)

Pieces / pennies the shops pay (the draw's cost, the season's price and the store's markup included: making 100%,
resale 75%), by season; the year's mean; the gate toll; carriage in (the shop pays the carters on top); the Country's
intake (after the toll); the swing of the season's pennies against the year's mean.

| Industry | Spring | Summer | Autumn | Winter | Year | Gate toll | Carriage in | The Country's intake | Season swing |
|---|---|---|---|---|---|---|---|---|---|
| livestock: meat | 343 / 1,011p | 332 / 1,025p | 327 / 990p | 340 / 1,036p | 336 / 1,015p | 323p | 3.6p | 692p | 97-102% |
| orchards | 532 / 737p | 513 / 521p | 518 / 532p | 503 / 805p | 517 / 648p | 205p | 11.7p | 443p | 80-124% |
| fishing | 309 / 570p | 264 / 562p | 275 / 573p | 360 / 649p | 302 / 588p | 186p | 2.2p | 402p | 96-110% |
| woods: timber, firewood, bark, charcoal | 451 / 548p | 435 / 508p | 420 / 490p | 578 / 645p | 471 / 547p | 174p | 48.9p | 373p | 90-118% |
| country workshops' goods | 133 / 502p | 128 / 482p | 138 / 538p | 126 / 469p | 131 / 498p | 158p | 1.5p | 340p | 94-108% |
| mining and metals | 71 / 424p | 57 / 340p | 54 / 325p | 69 / 412p | 63 / 375p | 120p | 2.6p | 255p | 87-113% |
| grain and milling | 129 / 280p | 152 / 231p | 155 / 248p | 124 / 272p | 140 / 258p | 81p | 4.0p | 177p | 90-109% |
| vineyards | 36 / 222p | 39 / 197p | 39 / 207p | 30 / 191p | 36 / 205p | 65p | 0.8p | 140p | 94-109% |
| fibre: hemp, flax, linen | 111 / 211p | 95 / 180p | 89 / 171p | 108 / 201p | 101 / 191p | 61p | 1.0p | 129p | 89-111% |
| bees | 54 / 198p | 62 / 187p | 59 / 183p | 48 / 176p | 56 / 186p | 60p | 0.5p | 126p | 95-106% |
| salt | 80 / 159p | 74 / 147p | 75 / 151p | 82 / 164p | 78 / 155p | 48p | 1.0p | 107p | 95-106% |
| stone, lime, clay, sand | 139 / 166p | 119 / 142p | 108 / 130p | 142 / 173p | 127 / 153p | 49p | 16.9p | 104p | 85-113% |
| livestock: wool, hides, tallow, bone | 84 / 115p | 79 / 107p | 76 / 102p | 78 / 110p | 79 / 108p | 33p | 0.8p | 75p | 94-106% |
| dairy | 55 / 85p | 75 / 76p | 79 / 80p | 54 / 82p | 66 / 81p | 26p | 1.0p | 55p | 94-105% |
| herbs | 7 / 9p | 7 / 9p | 7 / 10p | 10 / 11p | 8 / 10p | 3p | 0.0p | 7p | 90-119% |
| vegetables | 4 / 6p | 5 / 5p | 5 / 5p | 3 / 6p | 4 / 6p | 2p | 0.0p | 4p | 87-115% |
| **all** | **2,534 / 5,244p** | **2,436 / 4,719p** | **2,424 / 4,732p** | **2,655 / 5,404p** | **2,512 / 5,024p** | **1,595p** | **96p** | **3,429p** | |

- **The pennies follow the season prices more than the pieces do**: grain and fruit are cheapest at harvest (summer and
  autumn, 89-96%) and dearest in winter and spring (109-130%), so the orchards take 805p in winter for fewer apples than
  the 532p of autumn. Meat (no season) and fish (more in winter, cheaper then) are nearly flat.
- **Where the season caps bind** (the settled years): meat draws 317-330 a day against a cap of 330 all year; fish binds
  in spring, grain in winter and spring, honey and fruit in winter. **After the switch the city draws well over the
  caps**, the crews' food among it: meat 376-388 a day against 330 in every season (14-18% over, before the yields'
  slack), grain 148 against 131 in spring, 177 against 171 in summer and 142 against 126 in winter; the preserved goods
  in spring, honey in winter and fish in spring and summer come right to theirs. So a farm's goods are limited by its
  stock, not by the caps, which stay for the Country's own goods (section 4, Phase B).
- **Wine is drawn all year** (17-20 a day in every season), the autumn short run's gap filled in from the long run (How
  it was measured); the city's own press and brewery make another 7-12 a day from grapes.

### 1b. The food goods, and why they are drawn

| Good | Catalogue | Spring | Summer | Autumn | Winter | Paid a day (year) | Drawn for |
|---|---|---|---|---|---|---|---|
| apples | 1p | 459 | 498 | 499 | 382 | 513p | cider 81%, dried apples 13%, preserves 7% |
| raw meat | 3p | 146 | 138 | 126 | 129 | 376p | jerky 37%, resale 34%, salt pork 27% |
| pig | 12p | 18 | 21 | 22 | 21 | 247p | the butchers' pigs |
| mutton | 6p | 32 | 37 | 33 | 36 | 207p | smoked mutton |
| rabbit | 1p | 134 | 128 | 139 | 142 | 136p | smoked rabbit |
| meal (the Country's cookshops' stew) | 6p | 13 | 8 | 7 | 12 | 50p | resale |
| fish | 2p | 186 | 174 | 173 | 216 | 403p | salt fish 57%, smoked fish 43% |
| shellfish | 2p | 123 | 90 | 102 | 144 | 186p | resale |
| honey | 3p | 53 | 62 | 59 | 47 | 184p | honey cakes 63%, preserves 30%, cordial 6% |
| flour | 2p | 72 | 81 | 81 | 67 | 180p | honey cakes 55%, bread 45% |
| wine | 12p | 20 | 17 | 19 | 17 | 167p | resale 94% |
| salt | 2p | 80 | 74 | 75 | 82 | 155p | salt fish 44%, jerky 32%, salt pork 24% |
| milk | 1p | 55 | 75 | 79 | 54 | 81p | porridge |
| oats | 1p | 54 | 69 | 72 | 53 | 76p | porridge |
| tallow | 1p | 54 | 51 | 50 | 50 | 51p | candles 90% |
| dried apples, preserves | 3p, 4p | 29, 13 | 0, 1 | 0, 3 | 48, 47 | 43p, 46p | resale (the Country's winter stores) |
| cider | 3p | 21 | 7 | 8 | 18 | 36p | resale 65%, apple brandy 35% |
| grapes | 2p | 15 | 22 | 20 | 13 | 37p | wine |
| hide, wool, bones | 5p, 2p, 1p | 6, 9, 14 | 5, 9, 13 | 4, 8, 12 | 6, 8, 12 | 26p, 17p, 13p | leather; scarves; charms |
| berries, vegetables | 1p | 10, 4 | 7, 5 | 7, 5 | 9, 3 | 10p, 6p | cordial; the inns' stew |

### 1c. What the Country spends, and what it buys back

**The Country's money a day**: in, 3,429p (its part of every draw); out, its **100 posts 824p** (96 at 8p, 4 at a
keeper's 14p: the wharf's carriers and the Houses' people, paid by the Houses' estates), its patrons 51p (the Houses'
retainers' kit, the Houses' halls, the wharf's hulls), carriage out 27p, and its basket of the city's goods 2,527p. Its
purse averages 589p (372-1,983p).

Of the basket, **598p is food and drink made from its own produce**: cider 306p (102 a day), salt fish 110p, smoked fish
87p, wine 34p, apple brandy 25p, berry cordial 23p, brandy 11p. The rest of the top of the basket: cart wheels 209p,
paper 83p, steel bars 80p, iron bars 77p, leather 72p, cut stone 68p, charcoal 66p, mortar 60p, planks 58p, barrels 57p,
bricks 56p, lime 54p, beams 46p, iron hoops 43p, nails 43p, bandages 43p, roof tiles 42p.

The buy-back is the Country's spending, not a producer's income, but its makers draw from the Country to make it:
**344p a day of the food areas' takings are behind the basket** (the apples behind its cider, the fish behind its salt
and smoked fish, the hides behind its leather), and **642p of the Country's other goods** (the iron behind its steel,
nails and hoops; the timber behind its cart wheels, beams and planks; the stone, lime and clay behind its masonry).
When the farms take the food, both depend on who buys in its place (2.6).

## 2. Food, in depth

### 2.1 What the residents eat, and where it comes from

What the residents buy to eat (pieces a day; at NourishADay a head):

| Food | Nourish | Price | Spring | Summer | Autumn | Winter | Of the nourishment | Made from |
|---|---|---|---|---|---|---|---|---|
| porridge | 20 | 1p | 225 | 251 | 279 | 228 | 18% | 1/4 oats + 1/4 milk (the inns) |
| bread | 15 | 1p | 228 | 247 | 260 | 215 | 13% | 1/6 flour (+ firewood) |
| raw meat | 30 | 3p | 103 | 100 | 97 | 106 | 11% | the Country's beef and pork, resold |
| meal (stew) | 50 | 6p | 60 | 55 | 52 | 60 | 10% | 1 raw meat + 1 vegetables (the inns), or the Country's, resold |
| smoked rabbit | 18 | 2p | 132 | 123 | 123 | 136 | 8% | 1 rabbit (+ firewood) |
| salt pork | 35 | 6p | 59 | 54 | 51 | 53 | 7% | 1 raw meat + 1/3 salt |
| honey cake | 15 | 4p | 106 | 109 | 116 | 105 | 6% | 1/3 flour + 1/3 honey |
| jerky | 30 | 6p | 49 | 46 | 46 | 46 | 5% | 1 raw meat + 1/2 salt |
| shellfish | 10 | 2p | 121 | 94 | 97 | 124 | 4% | resold |
| salt fish | 18 | 4p | 66 | 60 | 58 | 65 | 4% | 1 fish + 1/3 salt |
| smoked fish | 20 | 5p | 50 | 54 | 46 | 53 | 4% | 1 fish (+ firewood) |
| smoked mutton | 32 | 8p | 33 | 37 | 31 | 36 | 4% | 1 mutton (+ firewood) |
| dried apples | 10 | 3p | 69 | 74 | 69 | 76 | 3% | 1.5 apples (+ firewood) |
| fruit preserves | 12 | 4p | 44 | 53 | 50 | 70 | 2% | 1 apple + 1/2 honey |
| cider | 6 | 3p | 64 | 65 | 72 | 67 | 1% | 2 apples |
| wine | 4 | 12p | 19 | 18 | 22 | 17 | 0% | 2 grapes (and a cask's share) |

A resident eats 45.8 nourishment a day (NourishADay) for **6.47p of food** at the shops' prices, and drinks and buys
comforts besides. Meat in all its forms is about 45% of what the city eats, grain (porridge, bread, honey cakes) 37%,
fish 12%, fruit and cider 6%. In drawn goods that is 1.73 pieces a head a day.

### 2.2 The farms' takings, now and after the switch

A day: "now" is the Country's part of today's draws of the same goods, at the season's own price; "after" is solved
with the recommended crews and baskets. The granary sells from store at a price a year (101% of the catalogue, the
year's mean of grain's season prices); fruit, meat, fish, honey and salt at the season's, as now (section 3). The basket
is each farm's till's spending on the city's goods, its carriage out included; with the wages it comes to the takings
exactly. The last column is the user's rule for how many farms (2.3).

| Farm | Now (year) | After: Spring | Summer | Autumn | Winter | Year | Crew's wages (of the takings) | The farms' basket | A farm's leanest season / its smallest crew's wages |
|---|---|---|---|---|---|---|---|---|---|
| grain farm (2) | 177p | 209p | 198p | 225p | 201p | **208p** | 112p (54%) | 96p | 99p / 56p (three would pay) |
| orchard with a drying house (2) | 214p | 194p | 104p | 111p | 288p | **174p** | 92p (53%) | 82p | 52p / 36p |
| orchard (3) | 229p | 165p | 151p | 154p | 141p | **153p** | 108p (71%) | 45p | 47p / 36p |
| vineyard (1) | 140p | 165p | 137p | 167p | 136p | **151p** | 76p (50%) | 75p | 136p / 46p (two would pay) |
| cattle and dairy farm (3) | 277p | 354p | 321p | 302p | 322p | **325p** | 168p (52%) | 157p | 101p / 46p |
| pig farm (3) | 291p | 330p | 344p | 345p | 348p | **342p** | 168p (49%) | 174p | 110p / 36p |
| sheep and goat run (2) | 161p | 182p | 205p | 182p | 198p | **192p** | 92p (48%) | 100p | 91p / 36p |
| rabbit warren (2) | 94p | 107p | 105p | 112p | 115p | **110p** | 52p (47%) | 58p | 53p / 26p |
| sea fishing boat (4) | 264p | 268p | 250p | 257p | 301p | **269p** | 144p (54%) | 125p | 62p / 36p |
| lake and river boat (2) | 83p | 67p | 74p | 71p | 76p | **72p** | 52p (72%) | 20p | 33p / 26p |
| eel and fowl marsh (1) | 55p | 45p | 49p | 47p | 51p | **48p** | 26p (54%) | 22p | 45p / 26p |
| beekeeper (3) | 126p | 151p | 142p | 141p | 131p | **141p** | 78p (55%) | 63p | 44p / 26p |
| salt works (1) | 107p | 116p | 107p | 106p | 114p | **111p** | 56p (51%) | 55p | 106p / 36p (two would pay) |
| **all** | **2,216p** | **2,352p** | **2,187p** | **2,220p** | **2,423p** | **2,295p** | **1,224p** | **1,071p** | |

**The farms' basket**: 1,071p a day of the tills' spending, 1,059p of goods and 12p of carriage out (countryBuys'
rate, 1.08%); `basketADay` in `country_farms.json` is the till's spending, the carriage in it. By season it follows the
takings: the two drying-house orchards together spend 102 / 12 / 19 / 196p, the three others 57 / 43 / 46 / 33p.

**Where the takings come from**: now 2,220p (4p of it vegetables), of it 1,872p the residents' and the city's draws and
344p behind the Country's buy-back. After: the residents' 1,872p (unchanged); the Country's smaller basket 38p (so 306p
of the buy-back's draws are lost: 175p of it the orchards', the cider apples; 35p the sea boats', 23p the vineyard's,
21p the lake boats', 14p the eel marsh's, 13p the salt works'); the crews' food 279p and their other spending 89p; the
farms' basket 16p (the hides in its leather): **2,295p in all, the farms' takings**. Vegetables are out of the draws
after the switch (nobody sells them: the inns' stew is open question 7). So the switch gives the farms a little more
than the food earns the Country now, but not in the same places: **the orchards lose about a quarter** (the Country's
cider), the lake boats and the eel marsh an eighth (its salt and smoked fish), while grain, the herds, the bees and
salt gain from the crews' own food.

- **Grain is cheap for what it feeds**: a measure of wheat is a flour is six loaves, 90 nourishment, two days of a
  wolf's food, and the Country gets about 1.6p for it after the toll (1.4p in summer and autumn, 1.9p in winter and
  spring). The bakers and inns keep most of bread's and porridge's value. With the toll and the crews' food, the two
  grain farms still pay 1 + 4 each.
- **Salt** is 107p to the Country, scaled with the fish and meat it is salted for: one works pays a keeper and four
  hands, with no price lever. (No lever needs a fraction of a penny: catalogue prices are whole pennies, and an area's
  `priceFactor`, in whole percent, is how a farm price would be moved, section 4.)

### 2.3 How many farms, and their crews

**How many farms** (the user's rule, one test for every kind): a kind keeps its farms while each pays its smallest
sensible crew out of its own takings in every season (the last column of 2.2); a kind that can't is collapsed into
fewer, larger farms. **Their crews**: what about half a farm's takings pay at the city's wages (a keeper 16p, a hand
10p), never under that smallest sensible crew; the other half is its basket (2.6). The crews' own food and spending come
back as takings, so the crews and the takings are solved together.

- **Grain, 2 large farms, each with its water mill** (the user's decision): 1 + 4 each (the floor: ploughing, reaping,
  threshing, the miller and his man). On two rivers, so never one source. With the corrected money three would pay 1 + 4
  each too, but with wages at 81% of their takings (open question 1).
- **Orchards, 5, two with a drying house**: the drying-house orchards make the dried apples and preserves from their
  own fruit in autumn and sell them through winter and spring, when fresh fruit is short; their takings swing with
  them (288p in winter, 104p in summer) and pay 1 + 3. The other three pay 1 + 2 (wages 71% of their takings; in
  winter 47p a farm against 36p of wages): they pay their crew in every season, so by the rule they stay, and five
  orchards are more sources than four.
- **Vineyard, 1 large** (the user's decision): 1 + 6, on the warm hills at the desert's edge. Its takings are steady
  (136-167p a day): the city draws wine all year. Two would pay 1 + 3 each (open question 1).
- **Livestock, 10 holdings in four kinds**: 3 cattle and dairy farms (all the milk: it keeps a day, so near the city;
  their calves are the beef), 3 pig farms (woodland pannage), 2 sheep and goat runs (the goats in the northern
  foothills' run, for meat and wool), 2 rabbit warrens. The raw meat is split 65% to the cattle and 35% to the pigs;
  the tallow, hides and bones as the herds are; each kind then pays 1 + 1 to 1 + 4.
- **Fish, 7 crews**: 4 sea boats (1 + 2: a three-wolf boat), 2 lake and river boats (1 + 1), 1 eel and fowl marsh
  (1 + 1). The fish is shared 50% to the sea boats (which also have all the shellfish), 30% to the lake and river boats,
  20% to the marsh.
- **Honey, 3 beekeepers** (1 + 1), two among the orchards, one in the desert edge's thyme scrub.
- **Salt, 1 works** (the user's decision): 1 + 4, on the marsh's coast, where the sea is let into clay pans, dried by
  the summer sun and boiled over peat the rest of the year. Two would pay 1 + 2 each (open question 1).

**The farms' crews are 105 wolves** (29 keepers, 76 hands), on about half the takings.

### 2.4 Yields per farm

The yields are not real-world yields (the user: "We don't need to stay realistic to actual yields"). Each farm's yield
of a good is its share of three things, plus 10% slack:

1. **The city's draws after the switch**, season by season. The 105 crews buy their food at the city's shops, so their
   food is among these draws; so is what the farms' and the Country's baskets draw.
2. **The Country's own people who stay in the Country** and eat from the farms without buying in the city: the 68
   workers of its other industries once they are placed (section 5) and the Houses' 24 armed retainers (country.json
   `patrons`), 92 in all, eating the residents' diet (1.73 drawn pieces a head a day). The Houses' other people hold the
   Country's 100 posts in the city and eat there, in the draws. No dependants are counted until families are placed.
3. The slack.

Pieces a day, a farm, the season means (what it sells across each season). `country_farms.json` holds the values at
each season's middle day, blended between them as the season groups are (`Society::seasonal`, Core/RatwCityLoop.cpp
about line 352), chosen so that the blend's season means are these:

| Farm | Good | Spring / Summer / Autumn / Winter (season means) | Middle-day values (the settings) |
|---|---|---|---|
| grain farm (2) | flour (wheat ground at its mill) | 53.0 / 59.5 / 59.5 / 49.3 | 52.8 / 60.2 / 61.5 / 46.7 |
| | oats | 39.4 / 50.4 / 52.6 / 39.1 | 37.9 / 51.7 / 55.5 / 36.6 |
| | straw | 2.1 / 1.2 / 1.6 / 2.0 | 2.3 / 1.0 / 1.6 / 2.1 |
| orchard, each of the 5 | apples | 69.2 / 84.8 / 85.8 / 54.9 | 70.2 / 86.0 / 92.4 / 46.2 |
| | cider; berries | 2.0 / 0.5 / 0.6 / 1.8; about 0.2 | 2.3 / 0.2 / 0.5 / 2.0; about 0.2 |
| orchard with a drying house, each of the 2 | dried apples | 21.4 / 0 / 0 / 34.9 | 21.2 / 0 / 0 / 41.8 (blend: 21.1 / 2.6 / 5.3 / 34.0) |
| | fruit preserves | 9.2 / 1.1 / 2.4 / 34.3 | 5.1 / 0 / 0 / 44.0 (blend: 9.4 / 0.6 / 5.5 / 33.7) |
| vineyard (1) | grapes | 19.6 / 26.8 / 26.5 / 16.1 | 19.3 / 27.7 / 28.5 / 13.6 |
| | wine | 24.2 / 18.8 / 24.5 / 19.6 | 26.8 / 16.1 / 27.1 / 17.2 |
| cattle and dairy farm (3) | raw meat | 46.2 / 43.9 / 39.9 / 40.8 | 47.6 / 44.1 / 39.1 / 40.0 |
| | milk | 27.0 / 36.7 / 38.4 / 26.2 | 25.8 / 37.7 / 41.0 / 23.8 |
| | meal; tallow; hides; bones | 2.3-4.2; about 12.3; about 1.1; about 2.5 | |
| pig farm (3) | raw meat | 24.9 / 23.7 / 21.5 / 22.0 | 25.6 / 23.8 / 21.1 / 21.5 |
| | pigs | 8.8 / 10.2 / 10.8 / 10.4 | 8.1 / 10.4 / 10.9 / 10.7 |
| | meal; tallow; hides; bones | 1.2-2.2; about 7.4; about 0.75; about 1.5 | |
| sheep and goat run (2) | mutton | 23.5 / 27.0 / 24.0 / 26.4 | 22.0 / 28.5 / 22.6 / 27.8 |
| | wool; tallow; bones | about 6; about 7.4; about 1.5 | |
| rabbit warren (2) | rabbits | 98.3 / 93.9 / 101.7 / 103.9; pelts about 1 | 98.3 / 91.7 / 102.8 / 105.0 |
| sea fishing boat (4) | fish | 25.1 / 24.0 / 23.1 / 28.8 | 24.2 / 24.4 / 21.7 / 30.8 |
| | shellfish | 45.1 / 33.1 / 37.2 / 52.8 | 45.5 / 30.7 / 34.9 / 57.0 |
| lake and river boat (2) | fish | 30.1 / 28.8 / 27.8 / 34.6 | 29.1 / 29.3 / 26.0 / 36.9 |
| eel and fowl marsh (1) | fish (eels) | 40.1 / 38.5 / 37.0 / 46.1 | 38.8 / 39.1 / 34.7 / 49.2 |
| beekeeper (3) | honey | 24.3 / 28.8 / 27.5 / 21.3 | 24.2 / 29.6 / 28.4 / 19.7 |
| salt works (1) | salt | 102.6 / 96.0 / 96.9 / 103.5 | 103.5 / 94.8 / 95.9 / 104.8 |

- **The seasons' shape is the city's**: each yield follows what the city drew in that season (more grain in summer and
  autumn, more fish in winter, fruit short in winter, meat flat), and the blend of the middle-day values has exactly
  these season means, so supply meets demand season by season with 10% over. Feeding the blend the season means instead
  would flatten the peaks (winter's preserves 13% under the city's demand, its dried apples 9%).
- **The dried apples and preserves** can't be held at nought through summer and autumn by a straight-line blend: the
  nearest has the drying houses selling a little then (2.6 and 5.3 dried apples a day; 0.6 and 5.5 preserves) and winter
  2-3% under its mean, still 7-8% over the city's demand. What they make in summer and autumn keeps (they are the
  preserver's goods), so each drying house's store carries about 700 dried apples and 240 preserves into winter.
- **Stores, not fields**: for grain, apples, grapes, wine, honey and salt the yield is what the farm sells from its store
  in that season, not what it reaps that day. The game needn't hold the harvest as a year's stock (an account holds at
  most 10,000 of a good, `StockCap`; an orchard sells about 27,000 apples a year): the store is in the seasonal yields.
- **The opening stock** is created, not handed over: the Country holds no goods (a store draw creates the goods in the
  shop's till, and what the Country buys is consumed, "carried to the Country"). At the switch each farm's stock is
  created as a week of its yields (`create`, "grown at <farm>"; at most about 750 of a good, well under the cap). From
  then on the yields add to the stock daily, the Country's table takes its share ("eaten in the Country"), and stock
  over a cap spoils, so oversupply shows.

### 2.5 The cross-check: what real yields and labour would say

The tool's `--real-check` sizes food from pre-industrial yields (the first draft; ASSUMPTIONS and UNITS in the tool: a
tile about a metre, a game day's food about 2,500 kcal, a measure of wheat 1.5 kg; a three-field grain farm at 1 t a
hectare with a fifth kept for seed; orchards at 6 t a hectare; and so on). It is kept for land and labour, which the
money doesn't say:

- **Land**: these yields, drawn at a tenth of their real area (each tile of field 10 m^2), want about 307,000 tiles of
  the farms' own ground (4.7 cells of 256 x 256) and 1.28 million of shared grazing, heath and woodland (19.5 cells):
  **24.1 cells at 1:10** (the first draft's larger demand, with 488 Country people, came to 30.5), and ten times that at
  1:1. The placement list draws 14 (section 6).
- **Labour**: footnote to section 3.

### 2.6 The Country's money once the farms take the food

**What the Country still is**: its other goods (the woods, the mines and metals: iron stays the Country's for now,
stone, clay and sand, hemp and flax, herbs, its workshops' pots, scarves, soap and pamphlets), timber not yet placed, the
Houses' estates and their 100 posts in the city, its patrons, and its carriage. Its takings and spending (a day):

| | Now | After |
|---|---|---|
| Its takings: the food areas' goods | 2,220p | - (the farms') |
| Its takings: its other goods | 1,209p | **1,158p** |
| its 100 posts (the Houses' people and the wharf's) | 824p | 824p |
| its patrons (the Houses' retainers' kit and halls, the wharf's hulls) | 51p | 51p |
| carriage out | 27p | 3p |
| its basket of the city's goods | 2,527p | **281p (11%)** |
| of the basket, food and drink made from the farms' goods (cider, salt and smoked fish, wine, brandy) | 598p | 66p |

| The Country's takings by industry | Now | After |
|---|---|---|
| woods: timber, firewood, bark, charcoal | 373p | 354p |
| mining and metals | 255p | 187p |
| stone, lime, clay, sand | 104p | 102p |
| fibre: hemp, flax, linen | 129p | 87p |
| herbs | 7p | 8p |
| country workshops' goods (the cart wheels' and barrels' iron hoops among them) | 340p | 420p |

Its takings over its posts and patrons are +214p to +359p in every season, so it pays its posts all year. **That
margin rests on the farms spending their takings in the city**: 361p of its 1,158p comes from the farms' basket's
makers (the cart wheels, barrels, nails, planks, stone and lime draw iron, timber and stone from the Country). If the
farms spent every penny on wages instead (about 256 crew, solved with their food and spending), the crews' buying
would draw less from the Country: it would take in 892p against 875p of posts and patrons (+6p, -6p, +61p and +9p by
season), so about 6p short in summer, its basket about 19p. The basket keeps the margin comfortable. (Farms that kept
what is over their wages, 105 crew and no basket, are not a steady state: about 1,016p a day would pile up in their
tills, out of circulation.)

**The farms' basket**: each farm spends what its till holds over its crew's wages and its reserve (section 3) on the
city's goods a farm uses: cart wheels and axles, barrels and iron hoops, sickles, farm gear, saws, nails, fittings,
locks, chains, buckles, cookpots, harness leather, rope, canvas and nets, planks, beams, cut stone, bricks, roof tiles,
mortar, lime, whitewash, plaster and glass, **in the shares the Country's basket buys them now** (country.json; listed
in `country_farms.json` `basket`), from the makers holding them, as the Country buys. It is **1,071p a day** (1,059p of
goods and 12p of carriage out), what the farms take in over their wages, and nearly all of the 1,212p the Country
spends on those goods now.

**The makers' order book** (sales a day; the residents' own buying is the same before and after and left out; the
trades with Country orders or a change of 5p or more):

| Trade | Its sales now (all buyers) | The Country's basket now | After: the Country's basket | The farms' basket | The crews' food | The crews' other spending | Change (of its sales) | Or with the Country's basket re-weighted: its orders | Change |
|---|---|---|---|---|---|---|---|---|---|
| smithy | 379p | 355p | 39p | 225p |  | 7p | -83p (-22%) | 40p | -82p (-22%) |
| carpenter | 182p | 182p | 20p | 161p |  |  | -2p (-1%) |  | -22p (-12%) |
| brewery | 306p | 177p | 20p |  | 3p | 34p | -121p (-39%) |  | -140p (-46%) |
| provisioner | 1,274p | 139p | 15p |  | 200p |  | +77p (+6%) |  | +62p (+5%) |
| brickworks | 120p | 120p | 13p | 105p |  |  | -2p (-1%) |  | -15p (-13%) |
| inn | 470p | 118p | 13p |  | 50p | 21p | -35p (-7%) |  | -47p (-10%) |
| foundry | 115p | 115p | 13p |  |  |  | -103p (-89%) | 49p | -66p (-57%) |
| mason | 110p | 110p | 12p | 95p |  |  | -2p (-2%) |  | -15p (-13%) |
| tannery | 102p | 102p | 11p | 62p |  |  | -28p (-27%) | 12p | -27p (-27%) |
| ironworks | 91p | 91p | 10p |  |  |  | -81p (-89%) | 36p | -55p (-60%) |
| cartwright | 84p | 84p | 9p | 75p |  |  | +0p (+0%) |  | -9p (-11%) |
| distillery | 78p | 78p | 9p |  |  |  | -69p (-89%) |  | -78p (-100%) |
| glassworks | 71p | 71p | 8p | 35p |  |  | -28p (-40%) | 13p | -24p (-33%) |
| plasterworks | 69p | 69p | 8p | 60p |  |  | -1p (-2%) |  | -9p (-13%) |
| cooperage | 76p | 68p | 8p | 57p |  |  | -3p (-4%) |  | -11p (-14%) |
| sawmill | 65p | 65p | 7p | 57p |  |  | -1p (-1%) |  | -8p (-12%) |
| scriptorium | 64p | 64p | 7p |  |  |  | -56p (-89%) | 26p | -38p (-60%) |
| fishmonger | 287p | 52p | 6p |  | 42p |  | -4p (-2%) |  | -10p (-3%) |
| apothecary | 100p | 50p | 6p |  |  | 15p | -30p (-30%) | 22p | -13p (-13%) |
| paper mill | 47p | 47p | 5p |  |  |  | -42p (-89%) | 18p | -28p (-60%) |
| press | 97p | 45p | 5p |  |  | 16p | -24p (-24%) |  | -29p (-30%) |
| ropewalk | 45p | 45p | 5p | 37p |  |  | -2p (-5%) |  | -7p (-16%) |
| paintworks | 37p | 37p | 4p | 11p |  |  | -21p (-58%) | 11p | -14p (-38%) |
| charcoal burners | 34p | 34p | 4p |  |  |  | -30p (-89%) | 14p | -20p (-59%) |
| builders' works | 32p | 29p | 3p | 25p |  | 1p | +1p (+3%) |  | -2p (-7%) |
| cartographer | 28p | 28p | 3p |  |  |  | -25p (-89%) | 14p | -14p (-50%) |
| tinker | 60p | 28p | 3p | 19p |  | 10p | +4p (+7%) | 3p | +4p (+7%) |
| printing house | 55p | 28p | 3p |  |  | 9p | -16p (-29%) | 11p | -8p (-15%) |
| armoury | 41p | 22p | 2p |  |  |  | -19p (-47%) | 8p | -13p (-33%) |
| jeweller | 45p | 19p | 2p |  |  | 8p | -9p (-20%) | 11p | -0p (-1%) |
| chandlery | 187p | 13p | 1p | 11p |  | 53p | +53p (+28%) |  | +52p (+28%) |
| weaver | 58p | 13p | 1p | 12p |  | 14p | +13p (+23%) |  | +12p (+21%) |
| pottery | 44p | 12p | 1p | 11p |  | 10p | +10p (+22%) |  | +8p (+19%) |
| smokehouse | 112p | 8p | 1p |  | 18p |  | +12p (+10%) |  | +11p (+10%) |
| general goods | 544p | 6p | 1p |  | 2p | 162p | +158p (+29%) | 3p | +160p (+29%) |
| the services and small trades (tailor, perfumer, pawnbroker, barber, moneychanger, stables, builders' yard) | 217p | 4p | 1p |  |  | 56p | +53p (+24%) | 3p | +55p (+25%) |
| bakery | 743p |  |  |  | 131p |  | +131p (+18%) |  | +131p (+18%) |
| shipwright | 38p |  |  |  |  | 5p | +5p (+13%) |  | +5p (+13%) |
| vintner | 204p |  |  |  |  | 63p | +63p (+31%) |  | +63p (+31%) |
| butcher | 708p |  |  |  | 125p |  | +125p (+18%) |  | +125p (+18%) |
| stall | 179p |  |  |  | 32p |  | +32p (+18%) |  | +32p (+18%) |
| preserver | 435p |  |  |  | 76p | 1p | +77p (+18%) |  | +77p (+18%) |
| rag and bone | 197p |  |  |  |  | 60p | +60p (+31%) |  | +60p (+31%) |
| **all** | **8,230p** | **2,527p** | **281p** | **1,059p** | **679p** | **545p** | **+37p** | **295p** | **+51p** |

**What fills the gap**: the Country's basket falls by 2,246p a day. In its place come **the farms' basket** (1,059p of
goods, to the trades that make farm goods: the carpenters, cartwrights, masons, brickworks, plasterers, coopers,
sawmills and ropewalk keep their orders within 5%, the smithies lose 22%, the tannery 27%, the glassworks 40%, for the
goods of theirs no farm buys), **the crews' food** (679p, to the food shops: the bakers, butchers, preservers, stalls
and provisioners gain about 18%), **the crews' other spending on needs, comforts and services** (545p, to the general
shops, chandlers, the vintner, the rag and bone shop, the inns and the brewery's cider), and **the Country's smaller
basket** (281p). In all +37p, since the farms spend all their takings where the Country kept a purse. The crews' other
spending is their settled rate: their comforts and services (416p of the 545p) start only once their purses hold 40p a
head (section 3).

**Who still loses**: the trades whose goods only the Country bought and no farm needs: the foundry and ironworks (iron
and steel bars), the distillery (brandies), the scriptorium and paper mill (paper and ledgers), the charcoal burners,
the cartographer (89%), the paintworks (58%), the armoury (47%), and the brewery its Country cider (39%). The Country's
smaller basket is the lever (open question 3): **re-weighted to the goods the farms don't buy and no food or drink**,
its 299p goes to those trades, their worst losses fall to 50-60% (the distillery loses all its Country orders: its
brandies are drink, which the re-weighted basket doesn't buy), and the farm goods' trades give up the Country's last
11-16%. It also keeps most of those trades off the treasury's wage support for good (below).

**The treasury**. The toll is 1,595p now (1,135p on the residents' and the city's draws, 459p behind the Country's
basket). After: **1,607p (+0.8%)**: the residents' 1,135p, the Country's smaller basket 51p, the crews' food 138p and
other spending 107p, the farms' basket 176p. The farms' goods still pass the gates as store draws do, so they pay the
toll; the farms' basket draws the same kinds of goods the Country's did (timber, iron hoops, stone, lime) and puts most
of the toll back. With every penny on wages instead of the basket (about 256 crew) the toll would be about 1,685p
(+6%): the crews' own food and spending pass the gates too. The crews' services bring the treasury its 30% share
(`servicesCityShare`): 10p a day.

But the toll is not the treasury's whole change. The losing trades' tills go short of wages, and `supportWages` makes up
100% of each till's shortfall from what the treasury holds over its cap. **Each trade as it stands in the settled
years** (the short runs are the founding world: 559 residents, every wage step at nought): its posts' wages at its wage
step there (the loop file: the brewery +9.7p a post, the foundry +6, the smithy -2.2, the apothecary and the charcoal
burners -5.2), and what its tills can pay them, its margin on its orders now (the order book's sales times the short
runs' margin on a penny: in the short runs a trade's wages and keepers' profit were its margin, 2,958p against 3,091p in
all). By the trades' sums they are short 143p a day and have 457p to spare, against the long run's own wage support of
117p and keepers' profit of 535p (within a trade, one till's spare and another's shortfall net). A lost penny of orders
costs a trade its margin on it. A loser short on most days lowers its wage step a penny every 28 days, each post to the
floor (7p) at most, so once its hands are there a step lowers only its keepers (the switch all at once, over its first
year):

| Trade | Tills / posts | Wage step now | Its posts' wages | Its margin on its orders now | Orders, change | Of it its margin | Wage support, change at once | Once its wages have drifted down | Months to drift | Wages the drift takes |
|---|---|---|---|---|---|---|---|---|---|---|
| brewery | 3 / 5 | +9.7p | 106p | 87p | -121p | 29% | +34p | -1p | 7 | 35p |
| foundry | 1 / 3 | +6.0p | 48p | 55p | -103p | 48% | +42p | +15p | 12 | 27p |
| smithy | 5 / 10 | -2.2p | 94p | 67p | -83p | 18% | +15p | 0p | 3 | 15p |
| ironworks | 1 / 5 | -1.0p | 41p | 43p | -81p | 47% | +36p | +30p | 6 | 6p |
| distillery | 1 / 3 | -2.5p | 25p | 23p | -69p | 29% | +20p | +16p | 4 | 4p |
| scriptorium | 1 / 2 | +1.0p | 24p | 27p | -56p | 42% | +21p | +11p | 7 | 10p |
| paper mill | 1 / 2 | -1.3p | 20p | 19p | -42p | 41% | +17p | +12p | 5 | 5p |
| charcoal burners | 1 / 2 | -5.2p | 16p | 14p | -30p | 40% | +12p | +11p | 1 | 1p |
| apothecary | 3 / 5 | -5.2p | 40p | 42p | -30p | 42% | +11p | +8p | 1 | 3p |
| glassworks | 2 / 5 | +0.4p | 54p | 58p | -28p | 81% | +19p | +1p | 7 | 18p |
| tannery | 1 / 3 | +6.7p | 50p | 58p | -28p | 57% | +8p | 0p | 3 | 9p |
| cartographer | 1 / 2 | +3.2p | 28p | 23p | -25p | 82% | +20p | +6p | 9 | 14p |
| press | 1 / 3 | +5.3p | 46p | 36p | -24p | 37% | +9p | 0p | 3 | 9p |
| paintworks | 1 / 2 | -1.6p | 19p | 18p | -21p | 49% | +10p | +5p | 5 | 5p |
| armoury | 1 / 2 | -2.6p | 18p | 20p | -19p | 49% | +8p | +4p | 4 | 4p |
| printing house | 1 / 2 | +4.8p | 32p | 32p | -16p | 58% | +9p | 0p | 5 | 10p |
| jeweller | 1 / 2 | +5.0p | 32p | 30p | -9p | 66% | +6p | 0p | 3 | 6p |
| cooperage | 2 / 3 | -3.5p | 28p | 24p | -3p | 31% | +1p | -1p | 1 | 2p |

**Most of the trades only the Country's basket kept can't drift far enough.** The ironworks' four hands are at the floor
already and its keeper at 13p, so its wages can fall 6p against a shortfall of 36p; the foundry, the distillery, the
scriptorium and the paper mill, the charcoal burners, the apothecary, the cartographer, the paintworks and the armoury
likewise reach the floor still short, and stay on the treasury's support for good (about 120p a day). The brewery, the
smithy, the tannery, the glassworks, the press, the printing house and the jeweller drift clear (the brewery in 7
months, its wages from +9.7p). The gainers' tills go short a little less (31p a day: the tailor, the perfumer, the
builders' yard, the shipwright, the weaver and the small services). In all:

| The treasury's flows a day | The Country's basket as it is | Re-weighted |
|---|---|---|
| the toll | +12p | +3p |
| the crews' services' share | +10p | +10p |
| the wage support, at once | -269p | -214p |
| **in all, at once** | **-248p** | **-201p** |
| the wage support once the losers' wages have drifted down (up to a year) | -82p | -24p |
| **in all, once drifted** | **-61p** | **-11p** |
| the wages the drift takes from the losers' posts | 189p a day | 198p a day |
| the extra support the switch wants in its first year (all at once) | 47,800p | 25,800p |

**The treasury night by night.** It holds 5,367p on average, about 3,370p over its cap, but the switch's cost is not
taken from that first: each night, last, what it holds over its cap and `surplusKeep` (6,000p in all) goes out as its
surplus (in the settled years 106p a day: 85p of alms to the poorest households and 21p of public works), and the alms
(388p a day) and its own posts are paid from what is over its float (200p), not its cap, so it can fall under the cap;
only the wage support stops there. So each settled year of the long run is replayed night by night from its first day
(spring) with the switch added (`replay`: the day's toll and services' share, then the wage support from what is over
the cap, what it can't pay going unpaid, the alms, then the surplus). Over the nine settled years, the least to the
most:

| Switched | The surplus not paid out in the first year | The treasury under its own path (the year's mean) | The most under it | Its lowest | The losers' support unpaid |
|---|---|---|---|---|---|
| all at once | 10,909-36,065p | 1,359-2,197p | 3,487-4,708p | 216-1,313p | 3,907-29,084p |
| all at once, re-weighted | 5,597-21,178p | 140-1,681p | 703-4,653p | 216-4,445p | 0-15,573p |
| an area at a time over six months | 4,023-16,047p | 763-1,626p | 2,835-3,940p | 216-1,955p | 1,557-10,342p |
| **an area at a time, re-weighted** | **1,698-4,888p** | **118-768p** | **924-2,811p** | **998-4,227p** | **0-3,190p** |

(The long run's own surplus paid out 15,676-66,280p a year, and its lowest was 1,942-4,942p.)

- **Switched all at once**, the extra support lands first on the surplus: half to nine tenths of the year's alms to
  the poorest households and works orders aren't paid out. The rest takes the treasury 1,400-2,200p under its path on
  average, to its float in the weak years, and the losers' posts go 3,900-29,100p short of their support.
- **An area at a time over about six months** (open question 5) lets the losers' wages drift while their orders go.
  With the Country's basket as it is, the trades only it kept still sit on support: the surplus 4,000-16,000p less, up
  to 10,300p unpaid.
- **An area at a time with the basket re-weighted** (open question 3) keeps those trades some orders: the first year's
  surplus 1,700-4,900p less (7-12% of it), the treasury 120-770p under its path, and 0-3,190p of support unpaid in the
  years it touches its cap; then -11p a day for good.
- Either way the drift moves about 190-200p a day of wages from the losers' posts to the gainers' (whose keepers'
  profit rises with their orders): that is what the switch does to the city's trades. The treasury's own trim
  (`treasuryDrift`: while its slow level is under 4,675p the toll firms half a point every 28 days; a point of toll is
  about 50p a day) is not in the replay; it would take up a lasting -11p a day, and part of -61p.

## 3. Labour and money (item 10)

**Employment is steady year-round, by design.** The game has no seasonal hiring: a post has one holder at a fixed daily
wage (`payOf`), the work bank holds at most the longest craft (`workBankMost`), and the city's labourers and the wharf's
carriers already hold posts of their own. The yields are settings, not the work of the crew, so the harvest and the
sowing change nothing a farm pays: **105 crew are paid every day of the year**, and no harvest is late or lost. The
honest answer to "whether or not funding is adequate to keep them employed through harvest and sowing without much
disruption" is that employment can't be disrupted by the seasons; **the risk is cash by season**: a farm's takings
follow the city's draws and the season's prices, and its wages don't.

**Funding is adequate, farm by farm and season by season.** Every farm's takings cover its wages in every season (2.2):
none has a lean season. The thinnest are the lake and river boats (wages 72% of their takings; 33p a boat in spring
against 26p) and the three orchards without a drying house (71%; 47p a farm in winter against 36p).

### Reserves

Pennies, and days of the farm's crew cost. A crew's cost is its wages (its food is bought out of them), so a day of crew
cost is a day of wages; the basket is spent only from what is over the reserve:

| Farm | Farms | A fixed basket, the season's prices | A fixed basket, the granary at a price a year | **Recommended: two days' float, and a lean season's shortfall on top** | A farm |
|---|---|---|---|---|---|
| grain farm | 2 | 2,608p (23) | 1,521p (14) | 224p (2) | 112p |
| orchard with a drying house | 2 | 12,175p (132) | 12,136p (132) | 184p (2) | 92p |
| orchard | 3 | 1,109p (10) | 1,091p (10) | 216p (2) | 72p |
| vineyard | 1 | 1,400p (18) | 1,409p (19) | 152p (2) | 152p |
| cattle and dairy farm | 3 | 2,697p (16) | 2,684p (16) | 336p (2) | 112p |
| pig farm | 3 | 1,085p (6) | 1,097p (7) | 336p (2) | 112p |
| sheep and goat run | 2 | 1,222p (13) | 1,222p (13) | 184p (2) | 92p |
| rabbit warren | 2 | 650p (13) | 651p (13) | 104p (2) | 52p |
| sea fishing boat | 4 | 2,928p (20) | 2,915p (20) | 288p (2) | 72p |
| lake and river boat | 2 | 455p (9) | 462p (9) | 104p (2) | 52p |
| eel and fowl marsh | 1 | 304p (12) | 308p (12) | 52p (2) | 52p |
| beekeeper | 3 | 972p (12) | 973p (12) | 156p (2) | 52p |
| salt works | 1 | 822p (15) | 814p (15) | 112p (2) | 112p |
| **all** | **29** | **28,426p** | **27,281p** | **2,448p** | |

- **A fixed basket can't be funded**: 27,300-28,400p is more than half the realm's money (50,252p, conserved), against a
  Country purse of 589p on average and a treasury of about 5,400p. Parked in farm tills it would also starve the city of
  the money its households and shops turn over.
- **The fix is the basket, not a fund.** Each farm keeps two days of its crew's wages (enough for a Restday, when the
  shops draw about half: the deepest dip in the long run's draws was 1.2 days of their mean) and spends what is over
  that on its basket, as the Country spends what is over its reserve and posts. A lean week cuts the basket, never the
  wages. No farm has a lean season, so each reserve is its float: **2,448p in all**, 52p to 152p a farm. Should a DM's
  settings give a farm one, its reserve is its float and that season's shortfall on top (`cash_cycle`), and its basket
  waits after the lean season until the till is back up: so its basket over the year is its takings less its wages.
- **Storage, and the price it sells at**: every farm sells from its store what the city draws, so the city's seasonal
  shape comes from the shops' prices to the residents (the season groups' `price`, as now) and the shelves, and the
  farms' cash only follows it. The granary sells at a price a year (101% of the catalogue, the year's mean of grain's
  season prices): grain is most plentiful when it is cheapest, so an even price steadies its takings (198-225p a day,
  against 187-224p at the season's). Fruit, fish, honey, meat and salt keep the season's price, which steadies theirs:
  apples and fish are fewest when they are dearest (the three plain orchards take 141-165p a day at the season's price,
  112-176p at a price a year, when their winter would fall to 37p a farm against 36p of wages). Smaller seasonal swings
  in the farms' supply were the other choice; they would change what the city eats in each season, which storage
  doesn't.

### The money the switch holds

The floats are not the only money the switch parks. **The crews are new households** (each farm's crew one household at
its steading), and the households' market buys comforts and services only from what a household holds over 40p a head,
a fifth of it a day (Core/RatwCityLoop.cpp, about line 876). The crews' wages (1,224p a day) pay their food (679p) and
needs (129p, crafts.json `households`); the other 416p goes on comforts and services, so **a crew's purse settles at 40p
a head and five days of that: 60p a head, 6,280p in all**. Their two days of food in the larder (1,358p) are bought once,
in the food shops: goods, not money held.

| Held by | Pennies |
|---|---|
| the farms' floats | 2,448p |
| the crews' purses, settled | 6,280p |
| **in all** | **8,728p: 17% of the realm's 50,252p, 1.6 times the treasury's 5,367p** |

- **From nought**, a crew reaches the 40p line in 12-14 days (its larder first), and its comforts reach their full rate
  about two weeks later. Until then the 416p a day of the order book's crew comforts and services isn't spent (their
  needs are), and each farm's basket waits until its float is full (about a day of its takings).
- **Where it would come from**: money is conserved, so from the city's purses (26,196p, 44p a head), tills (18,100p) and
  treasury. In proportion to what each holds, the residents' purses would give about 4,600p and fall to about 36p a head,
  under the 40p line where their comforts and services start: the order book's comfort trades would lose orders, and
  their tills would go short in turn. The fixed basket's argument (money parked away from the city's households and
  shops) applies to this money too.
- **The fix: found it at the switch** (open question 4). When an area is switched on, its farms' tills are opened with
  their floats and its crews' purses with their settled level, created as the world's money was created at its founding
  ("initial funding", Core/RatwSociety.cpp about line 97), and their larders with the farms' opening stock (goods). The
  realm's money grows by 8,728p (17%) for 105 more people in 29 households (18% more than the city's 595), and the
  order book is at its settled rate from the first day. Keeping money never made, the other way is to let the crews
  fill their purses from their wages over the first month, with the city's purses thinner by about 8p a head for good.

**The NPCs** (DEV's 524 unplaced, read only; working age 16-59):

| Old trade | Unplaced | Working age | The food farms' crews | The other industries (what they can pay, at most the plausible) | Spare (+) or short (-) |
|---|---|---|---|---|---|
| fields and mills | 60 | 60 | 10 | 7 (fibre) | +43 |
| orchards and vines | 25 | 23 | 24 | | -1 |
| herds and flocks | 61 | 61 | 42 | | +19 |
| bees | 6 | 6 | 6 | | 0 |
| fishing and the marsh (and salt) | 27 | 26 | 23 | | +3 |
| woods | 36 | 36 | | 11 | +25 |
| mines, stone and clay | 28 | 28 | | 21 | +7 |
| keeps the house | 52 | 52 | | | +52 |
| the towns' trades and shops | 41 | 37 | | 29 (the workshops) | +8 |
| the watch | 12 | 12 | | | +12 |
| service and labour | 39 | 37 | | | +37 |
| the Heights | 17 | 16 | | | +16 |
| children | 97 | 4 | | | +4 |
| the old | 23 | 3 | | | +3 |
| **all** | **524** | **401** | **105** | **68** | **+228** |

(Ages 6 to 80; 93 under 16, 30 aged 60 or over; 268 female, 256 male; ids `cty_` 200 (the Country's own), `tn_` 262
(the old map's towns), `rm_` 62.) The pool covers every crew by its own old trade but one orchard hand, who would come
from the house-keepers or the labourers. The arrivals of doc 68's rule (an empty city post brings in an unplaced NPC of
its class) drew 64 in the twelve-year run; each fills a city post, so is of working age: **337 of working age would be
left, 164 more than the farms' crews and the other industries' workers**.

**Footnote: real labour.** Sized from pre-industrial work for these yields (the cross-check's norms), the food would
want about 102 workers on the job in spring, 113 in summer, 124 in autumn and 81 in winter, against the 105 crew all
year: about 2,450 hand-days a year over the crew at the summer and autumn peaks and as many under it in winter and
spring. The game's posts don't follow it, and the yields are set as data.

## 4. What the code needs (an outline; another step builds it)

The user: "We aren't going to place any NPC's or farms or other things from Country yet", and "make sure that yield is
adjustable in Atlas' economy controls".

**Phase A: the farms as settings.** `Data/Economy/country_farms.json` (written by the tool, this doc's numbers) becomes
the defaults; each world keeps its own in `live.economy` (a migration: `farms jsonb` and `farm` manifest lines, as doc
68's seasons did), read by Core into `EconomySpec::farms`. **Precedence: a farm's own setting, then its area's, then the
defaults.**

- **Defaults**: the farms' wages (a keeper 16p, a hand 10p; fixed: the basket, not a keeper's profit, takes what is over
  them, so the wage drift would never move them), `basketShare` (50: the crew rule's split), `floatDays` (2), `slack`
  (10), `estatePeople` (24) and `otherWorkers` (68: the Country's own table), `openingStockDays` (7), and the `basket`'s
  goods and shares.
- **Per area**: its goods, `on` (on: its goods go into `country_off` and only its farms sell them; off: the Country
  supplies them), `yieldScale` (percent on all its farms' yields), `crewScale` (percent on all its farms' hands, rounded
  half up), `wages` (`keeper`, `hand`; null: the defaults'), `price` ("year" with a `priceFactor` in whole percent: the
  granary; or "season": the season groups' price, as now). A percent factor is how a price is moved between whole
  pennies: no catalogue price changes.
- **Per farm**: its id, kind, area and place (unset until the map places it); `crew` (keepers, hands) and `minHands`
  (the smallest sensible crew); `wages` (null: the area's, then the defaults'); `basketADay` (the design's mean, its
  carriage out included, for the reports: the basket itself is what is over the reserve); `reserve`; `yield` (pieces a
  day of each good at each season's middle day, blended between them as the season groups are, `Society::seasonal`;
  their blend's season means are 2.4's); its store's description.
- **Atlas's Economy page**: a Farms panel beside the Seasons panel: each area with its `yieldScale`, `crewScale`, wages
  and price, and its farms with their crews, wages, baskets and yields (four numbers a good), and a "from
  country_farms.json" reset, as the seasons have. **The DM's Economy tab** shows the same, editable on the DM's world,
  applied live by `economy.sync` (doc 68, "Seasons and the DM's rules"); both audited. The DM sees each farm's till,
  stock, last week's sales, wages and basket.
- **Tests**: the yields blend by day; a farm's yield scales with its area; a farm's crew scales with its area; wages
  fall through farm, area, defaults; an area switched on puts its goods in `country_off` and its farms sell them past
  the season's cap; an area switched off takes them out again and the Country supplies them.

**Phase B: modelled producers with virtual crews (the simulation).** Each farm runs without NPCs:

- **A till** (`till:farm:<id>`), its crew as **virtual posts** paid at 18:00 from the till at the farm's fixed wages, to
  the farm's own household (one a farm; nobody holds them yet). Their wages are spent as a household's: food at the
  city's shops, needs and comforts by doc 68's rules. Money is conserved after the founding.
- **The founding** (if the user agrees, open question 4): when an area is switched on, its farms' tills get their
  floats and its crews' households their settled purses, recorded as the world's founding money is, and their larders
  two days of food with the farms' opening stock (`openingStockDays`).
- **Production**: at the day's turn each farm adds its yield to its stock (`create`, "grown at <farm>"); the Country's
  table takes its share (`consume`, "eaten in the Country": `estatePeople` and `otherWorkers` at the residents' diet);
  stock over its cap spoils (`consume`, "spoiled").
- **Selling**: the store draw (`drawFromStore`) sells a farm-supplied good from the farms that hold it (spread by
  stock, as the Country's purchases are), at the store's markup and the area's price (the year's factor or the
  season's), the toll and the carriage as now (the shop pays the carriage). It is limited by the farms' stock, not by
  the season's cap (`countryLeft`, which stays for the Country's own goods): after the switch the city draws well over
  today's caps (meat 376-388 a day against 330, grain 148 against 131 in spring: 1a). Once an area is on, its goods go
  into `country_off` and the Country no longer sends them: the farm draw comes before that refusal, and a farm that runs
  out leaves the shops without until its stock comes back (the slack and the opening stock guard it, and the gate below
  counts it). Doc 68's third setting, "Country when short", is not needed and stays unbuilt.
- **The farms' basket**: like `countryBuys`: each hour the shops are open, what the till holds over its reserve and
  tonight's wages, spread over the basket's goods by their shares, from the makers holding them, a piece at a time; it
  pays its carriage out, as the Country does.
- **The Country** keeps its purse, its posts, its patrons, its other goods and its basket (re-weighted, if the user
  agrees). A farm short of wages draws on its float, then the treasury's wage support (doc 68's rule, as for a till).
- **Vegetables go** from Ridgemere 2.0's economy (the item stays in the catalogue for the old map). They are listed in:
  crafts.json `crafts` (the inns' stew: raw meat + vegetables -> meal), `buys` (the inn), `supplies` (the stall and the
  provisioner); country.json `seasons` (the vegetables group, with mushrooms); recipes.json (meal stew, mushroom stew:
  the players' recipes); regions.json (the old map's places); Data/Economy/ladder.json (vegetables, a staple). The stew
  needs another second input (open question 7).
- **Reports**: the economy line gains per farm and area its stock, sales, wages, basket, float and spoilage;
  `tools/econ_detail.py` gains a farms section.
- **3-year runs** (each needs the user's go-ahead, as all simulations): the farms on area by area, a month apart, from
  spring; the gates of doc 68, 10 plus: every farm pays its crew 99% of days, no area's stock runs out more than a day a
  season, spoilage under 10%, the city's prices within the long run's band, the Country pays its posts every day, the
  treasury stays over its cap, and the order book within this doc's.

**Phase C: real NPCs replace the virtual crews.** When the map has the farms' places:

- A farm's posts become real posts at its work area (`live.npcs` placed from the unplaced pool by their old trade, as
  the arrivals are, doc 68, "Arrivals from the Country"), with homes at the steading. Each post filled removes one
  virtual post; the wages are the same.
- The yield stays the setting. A farm with fewer crew than its setting may yield in proportion (a farm short of hands
  yields less); later the producers' work spells (`crafts.json` `producers`) make the goods the NPCs are seen working
  for. There is no seasonal hiring: the crew is the year's.

## 5. The other industries

The Country's after the switch: what each takes in, what it could pay at the farms' wages, and the workers it would
need at the game's own rates (`crafts.json` producers and the city's crafts' hours) and at plausible ones. The workers
counted (what it can pay, at most the plausible) are the ones the farms' yields feed (2.4), spread over its works by
each kind's share of the work, at least one a site (`crew_sites`).

| Industry | Drawn a day | The Country's takings now | After | Wolves it can pay (whole) | Workers, the game's rates | Plausible | Counted | Works (where): workers at each |
|---|---|---|---|---|---|---|---|---|
| woods: timber, firewood, bark, charcoal | firewood 366, timber 57, planks 24, charcoal 12, bark 10 | 373p | 354p | 30 | 68 | 11 | 11 | 2 logging camps (the conifer forests of the northern mountains' foothills; the central hills' oak woods): 1 + 1; 2 coppices with woodcutters' huts (beside the grain farms' villages; the woods nearest the city: it burns 340-460 firewood a day): 2 + 2; 2 charcoal hearths (the foothill forests, one beside each bloomery): 1 + 1; 2 water sawmills (one below each logging camp): 2 + 1 |
| mining and metals | iron bars 36, iron ore 16, copper ore 5, tin ore 4 | 255p | 187p | 14 | 38 | 38 | 14 | 2 iron mines (two valleys of the northern mountains): 4 + 4; 2 bloomeries (beside them): 2 + 1; 2 copper pits (the northern foothills; the central hills): 1 + 1; 1 tin streams (a stream of the central hills): 1 |
| stone, lime, clay, sand | clay 51, limestone 28, stone 24, sand 22 | 104p | 102p | 7 | 18 | 18 | 7 | 2 quarries (the central hills' limestone; building stone in the nearest hills east of the city): 3 + 2; 2 clay pits and sand banks (the plains' rivers; the coast south of the city): 1 + 1 |
| fibre: hemp, flax, linen | hemp 39, cord 28, linen 22, flax 11 | 129p | 87p | 7 | 37 | 18 | 7 | 2 hemp and flax fields (a field of each grain farm, its retting pond on the farm's river): 4 + 3 |
| herbs | 8 | 7p | 8p | 0 | 1 | 1 | 0 | no works of its own (8p a day pays no one): gathered at the marsh's edge by the eel and fowl marsh's crew and in the central hills' woods; the pennies stay the Country's |
| country workshops' goods | clay pots 34, wool scarves 29, soap 25, pamphlets 25, iron hoops 10, paper 4 | 340p | 420p | 37 | 29 | 29 | 29 | 2 village potteries (by the plains' clay pits and the southern coast's): 2 + 2; 3 weavers' and soap boilers' cottages (the farm villages: scarves, soap, bandages): 5 + 5 + 4; 2 country presses and paper mills (on the plains' rivers, one nearer the city): 5 + 5; 1 village smithy and tanyard (the grain farms' village: iron hoops, nails, cookpots, a little leather): 1 |

- **Two or more sources each**, as the user asked, where the money pays them: two logging camps, two coppices, two
  charcoal hearths and two sawmills; two iron mines and two bloomeries; two copper pits and the tin streams (the game's
  copper pit gives a copper ore and a tin ore a spell, so copper has two sources and tin three); two quarries, two clay
  pits; two fibre fields; two potteries, three weavers' cottages, two presses and paper mills. Every site has at least
  one worker: the woods' 11 crew 8 sites (the second draft's 13 could not be crewed, so they are collapsed, the user's
  rule). **One, and why**: the village smithy (its iron hoops, nails and cookpots are a worker and a half's work); the
  herbs have no works (8p a day pays no one).
- **Iron stays the Country's** (the user). Its sites are reserved on the map: the two iron mines and their bloomeries
  in the northern mountains, their charcoal hearths in the foothill forests. Mining takes in 187p after the switch and
  could pay 14 wolves, against about 38 that smelting bars from ore takes at the game's rates and real ones alike; the
  doc's earlier answer (the Country mines ore, the city smelts it) waits for the user's later step.
- **The Country's takings pay its 100 posts** now. "Wolves it can pay" is what each industry could pay if it were
  placed with its own crews; placing them would need the Country's posts paid another way (a later step).
- The workshops' goods (pots, scarves, soap, pamphlets) are the old map's towns' trades; they wait for the towns.
- The desert, beyond food: the vineyard, a warren, a beekeeper; later glass sand and perhaps a caravan route; no timber,
  little grazing (goats at its edge). The salt is the marsh's.

## 6. The placement list for the map

`Data/Economy/country_farms.json`: per food area, each farm with its id, kind, region, distance from the city, terrain,
what it wants near (a road, a river, the sea, the city), its footprint (`own_tiles`: fields, orchard, vines, meadow,
steading, pans; `shared_range_tiles`: grazing, heath or woodland its herds share with the wild), its crew, wages,
basket, reserve, yields by season and store; and the other industries' works with their counts, regions, workers and
footprints.

**The scale** (the user: "somewhere in the middle"): about **14 cells** in all, **6.5 of the farms' own ground and 7.5
of shared range**, each kind's share as real land would have it (the cross-check on these yields, 2.5). That is between
the second draft's 8.3 cells (3.3 + 5, chosen to look right) and the real proportion (24.1 cells at 1:10 for these
yields: 4.7 + 19.5). The farms' own ground is drawn at about 1:7 of its real area and the range at about 1:26.

| Kind | Count | Region (distance from Ridgemere) | Terrain, near | Own tiles each | Shared range each |
|---|---|---|---|---|---|
| grain farm with a water mill | 2 | the central plains, each on its own river east of the city (middle) | plain, meadow; road, river | 132,000 (about 363 x 363: several fields round the steading and the mill) | - |
| orchard with a drying house | 2 | a south slope of the central hills (middle); the sheltered valleys of the coast south of the city (near) | hills, meadow; road | 6,300 | - |
| orchard | 3 | a river valley of the central hills, a valley where the hills meet the plains (middle); the coast's valleys south of the city (near) | hills, meadow; road | 6,300 | - |
| vineyard | 1 | the warm hills at the desert's edge (far) | hills; road | 17,900 | - |
| cattle and dairy farm | 3 | the wet coastal pasture just south of the city (2), the meadows just east of it (1) (near) | meadow, pasture; the city, road | 28,300 | 84,300 |
| pig farm | 3 | the central hills' oak and beech woods (middle) | forest, hills; road | 2,100 | 28,300 |
| sheep and goat run | 2 | the central hills' downs; the northern foothills (middle) | hills, downs, mountain; road | 2,100 | 46,800 |
| rabbit warren | 2 | the marsh coast's dunes (middle); the desert's sandy edge (far) | heath, dunes, desert; road | 1,000 (the walled warren) | 22,600 |
| sea fishing boat | 4 | the sea off the city's wharf (2), a cove on the coast south of the city (near); the marsh's coast (middle) | coast; sea | 450 (a landing) | - |
| lake and river boat | 2 | a lake under the northern mountains; the plains' largest river (middle) | lake, river | 240 (a jetty) | - |
| eel and fowl marsh | 1 | the marsh south of the city (middle) | marsh; water, road | 375 | 15,000 |
| beekeeper | 3 | among the orchards of the central hills (middle) and the southern coast (near); the desert edge's scrub (far) | hills, meadow, scrub; an orchard | 3,300 | - |
| salt works | 1 | the marsh's coast (middle) | coast, marsh; sea, road | 2,900 | - |
| logging camp, coppice, charcoal hearth, water sawmill | 2, 2, 2, 2 | the northern foothills' forests and the central hills' woods; beside the villages and the city | forest; road, river | 1,200, 300, 900, 600 | 40,000, 10,000, -, - |
| iron mine, bloomery, copper pit, tin streams | 2, 2, 2, 1 | the northern mountains and foothills; the central hills | mountain, hills; road | 1,200, 600, 600, 600 | - |
| quarry, clay pit and sand bank | 2, 2 | the central hills, the hills east of the city; the plains' rivers, the southern coast | hills, river, coast | 4,800, 2,400 | - |
| hemp and flax field, pottery, weavers' cottages, press and paper mill, village smithy | 2, 2, 3, 2, 1 | the grain farms, the villages, the plains' rivers | | 7,000, 500, 300, 600, 500 | - |

The food farms' own ground is 6.5 cells and their shared range 7.5. Distances: near is within 2 cells of Ridgemere,
middle 3-8, far 9 or more (the desert). Carriage today is a flat 6p a wagon load whatever the distance, paid by the
shop that draws; if the realm's size makes the desert's wine visibly far, a rate by distance would cost the shops more,
and wine is light enough to bear it.

## Open questions for the user

Answered and applied (2026-11-01): vegetables dropped; collapse where crews can't be paid; the farms' basket; iron stays
in the Country; the fields' scale; the map's geography. Still the user's call:

1. **Your collapses, now that the money is corrected.** You collapsed to two grain farms, one vineyard and one salt
   works because the second draft said the pennies couldn't pay sensible crews. With the food no longer counted twice,
   the takings would pay two vineyards (1 + 3 each; 68p in the leanest season against 46p of wages) and two salt works
   (1 + 2 each; 53p against 36p), and three grain farms (1 + 4 each, but with wages at 81% of their takings).
   Recommended: **two vineyards and two salt works**, so neither is a single source ("I don't want it all from single
   sources"), and keep two grain farms. Or keep one, one and two as decided (this doc's numbers).
2. **The split between crew and basket.** Recommended: half and half (105 crew, a 1,071p basket), so the trades the
   Country's basket kept busy keep most of their orders. More crew (up to about 256 on wages alone) would move orders
   from the makers to the food and comfort shops and leave the Country about even (6p short in summer).
3. **The Country's smaller basket (281p).** Recommended: **re-weight it** to the goods the farms don't buy and no food
   or drink (iron and steel bars, paper, ledgers, maps, ink, charcoal, glass beads, handbells, paint, bandages): the
   trades only it kept lose 50-60% of their Country orders, not 89%, and draw 24p a day of wage support for good, not
   82p (their wages can't fall under the floor); the farm goods' trades give up the 11-16% they would keep. Or leave
   its shares as they are.
4. **The money the switch holds (8,728p).** Recommended: **found it at the switch**: each farm's float and each crew's
   settled purse created when its area is switched on, as the world's money was founded, so the city's purses, tills and
   comforts stay where they are (the realm's money grows 17% for 18% more people). Or keep money never made and let the
   crews fill their purses from their wages, the city's purses about 8p a head thinner, under the 40p comforts line on
   average.
5. **How to switch.** Recommended: **an area at a time, about a month apart, from spring** (six areas over about six
   months), with the basket re-weighted (3): in the first year the surplus pays the poorest households and the works
   7-12% less (1,700-4,900p), the treasury runs 120-770p under its path, and up to 3,190p of support goes unpaid in a
   year it touches its cap. All at once would cost half to nine tenths of the year's surplus and up to 29,100p of the
   losers' support (2.6).
6. **The farms' wages.** Recommended: fixed at the city tills' settled means in whole pennies (a keeper 16p, a hand
   10p), editable per farm and per area. Or let them drift, which needs the farm's basket counted as its keeper's spare
   (the drift raises a till's wages only when its keeper's profit is at least half its wages).
7. **The inns' stew without vegetables.** Recommended: raw meat and bones (a bone broth: the bones are the herds', so
   the farms keep the penny, about 3p a day), with the meal's nourishment as now. Or raw meat alone.
8. **The Country's people the farms feed** (92: the other industries' 68 workers and the Houses' 24 retainers).
   Recommended: no dependants until families are placed; raise the areas' `yieldScale` when they are.
9. **The stored harvests' price.** Recommended: the granary sells at a price a year (its mean of grain's season
   prices), which steadies the grain farms' takings; fruit, fish, meat, honey and salt at the season's, as the store
   room charges now, which steadies theirs. Or fruit at a price a year too (the last revision's choice): the plain
   orchards' winter would fall to 37p a farm against 36p of wages.
10. **The grain farms' size on the map.** Recommended: keep real proportions (each about 363 x 363 tiles, 4.0 of the 6.5
    cells of the farms' own ground, drawn as several fields round the steading and mill). Or spread some of their ground
    to the orchards and the dairy farms.
