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
  every other area.
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

**What changed since the second draft** (an independent check's corrections, all applied, with one model of how the
game pays and feeds a wolf):

- A wolf costs its farm its wage; its food is bought out of it in the city's shops and comes back to the farms as draws
  (the second draft added the food on top). The farms' takings include the crews' own food and spending.
- The switch is modelled: the Country's buy-back shrinks with its basket, the farms' basket replaces it, and every
  basket draws again from the farms and the Country (2.6), with the makers' order book by trade and the toll after.
- Salt scales with the fish and meat it is salted for (107p, not 74p); each season is priced at its own mean factor;
  the residents eat NourishADay, not the 49-50 the scaling gave them.
- The reserves are for this configuration, in days of crew cost (now the wages), and affordable (section 3).
- Labour: employment is steady by design; the risk is cash by season (section 3). The real-labour cross-check is a
  footnote, against these yields and the 98 crew, summer included.
- The opening stock is created, not handed over (the Country holds no goods; 2.4).
- The yields feed the Country's own people who stay in the Country too (2.4).
- Carriage is the shop's, not charged to the farms; no lever needs a fraction of a penny; the NPC pool after the
  arrivals is counted by working age; the other industries have two sources where the money pays them; the old map's
  names are gone; the footprints are re-derived to the user's scale.

Analysis only: no game rule, setting or world is changed, DEV is only read, no simulation was run for this draft.
Builds on doc 68 (the Country and the city's loop, all its Progress sections), doc 61 (industry of the realm) and doc 60
(the old map's farms; none of its names are used).

**The tool**: `tools/country_industry.py`. **The farms' settings and the placement list**: `Data/Economy/country_farms.json`
(nothing reads it yet; it is the defaults Atlas would edit, section 4).

## The lead table

What each food area pays for, its farms, their crews and yields. A day, the 12-year run's settled years. **Revenue** is
the farms' takings after the switch: the Country's part of what the city's shops pay for the area's goods (the cost of
the draw less the gate toll; the shop pays the carriage on top), the crews' own food and spending included (2.2).
**Wolves it can pay**: at the city's wages (a keeper 16p, a hand 10p) if every penny went on wages. **Crew**: what about
half the takings pay, never under a sensible crew for the kind; the other half is the farm's basket of the city's goods
(2.6). **Yield**: a farm's pieces a day, all its goods but the herds' by-products, by season (each good: 2.4).

| Area | Farm | Revenue a day (all its farms) | Wolves it can pay | Farms | Crew a farm | Yield a farm a day, Sp / Su / Au / Wi | Region (distance) |
|---|---|---|---|---|---|---|---|
| grain and flour | grain farm with a water mill | 206p | 19 | **2** | 1 + 4 | 94 / 111 / 113 / 90 | the central plains, each on its own river east of the city (middle) |
| orchards and vines | orchard (two with a drying house) | 327p | 30 | **4** | 1 + 2 | 104 / 107 / 115 / 105 | the central hills' south slopes and a river valley (2, middle); the sheltered valleys of the coast south of the city, cider country (2, near) |
| | vineyard | 116p | 11 | **1** | 1 + 4 | 43 / 45 / 23 / 35 | the warm south-facing hills at the desert's edge (far) |
| meat, milk and the herds | cattle and dairy farm | 324p | 31 | **3** | 1 + 4 | 77 / 83 / 80 / 71 | the wet coastal pasture just south of the city (2), the meadows just east of it (1) (near) |
| | pig farm | 339p | 32 | **3** | 1 + 4 | 36 / 35 / 33 / 34 | the central hills' oak and beech woods, on the mast (middle) |
| | sheep and goat run | 191p | 18 | **2** | 1 + 3 | 23 / 27 / 24 / 26 | the central hills' downs; the foothills of the northern mountains, with the goats (middle) |
| | rabbit warren | 109p | 10 | **2** | 1 + 1 | 98 / 94 / 101 / 104 | the dunes and sandy heath of the marsh's coast (middle); the desert's sandy edge (far) |
| fish and shellfish | sea fishing boat | 267p | 24 | **4** | 1 + 2 | 70 / 57 / 60 / 81 | the sea off the city's wharf (2), a fishing cove on the coast south of the city (near); the marsh's coast (middle) |
| | lake and river boat | 72p | 6 | **2** | 1 + 1 | 30 / 29 / 28 / 34 | a lake under the northern mountains; the largest river of the plains (middle) |
| | eel and fowl marsh | 48p | 4 | **1** | 1 + 1 | 40 / 38 / 37 / 46 | the marsh south of the city (middle) |
| honey | beekeeper | 140p | 12 | **3** | 1 + 1 | 25 / 29 / 28 / 22 | among the orchards of the central hills (middle) and of the southern coast (near); the desert edge's thyme scrub (far) |
| salt | salt works | 110p | 10 | **1** | 1 + 4 | 102 / 96 / 97 / 103 | the marsh's coast: sea pans, sun in summer, boiled over peat the rest of the year (middle) |

**In all: 2,247p a day of takings, 208 wolves they could pay on wages alone. 28 farms with crews of 98 (28 keepers, 70
hands) cost 1,148p a day in wages; their basket buys 1,094p a day of the city's goods.** The yields feed the city, the
crews, the Country's own 96 who stay in the Country, and 10% slack (2.4). Every area pays a sensible crew on every farm
in every season; only the vineyard falls under its wages, in autumn (section 3).

## Headline numbers

- **What the Country holds** (section 1): 16 industries. The city draws 2,526 pieces a day from it for 5,024p, of which
  1,595p is the gate toll and 3,429p the Country's. The food areas (the herds' by-products with them) are 2,195p of the
  Country's 3,429p (64%). The Country spends its 3,429p on its 100 posts (824p), its patrons (51p), carriage out (27p)
  and its basket of the city's goods (2,527p).
- **A wolf costs its farm its wage** (the model, below): 16p a keeper, 10p a hand. The wolf buys its food out of it in
  the city's shops (6.47p a day), and the shops draw that food from the farms: 2.66p of every crew wolf's wage comes
  back to the farms. The second draft added the food on top of the wage, which counted it twice.
- **The food can pay about 208 wolves** at those wages. **Recommended: 28 farms and 98 crew**, each farm's wages about
  half its takings; the other half, **1,094p a day, is the farms' basket**: tools, carts, barrels, rope, nets and
  building goods from the city's makers.
- **Collapsed by the user's rule**: two grain farms (not three), one vineyard, one salt works (not two each); four
  orchards (not five: the orchards lose a quarter of their takings when the Country stops buying cider); no gardens.
  Every other area has two or more sources. The eel marsh is the fish's third kind, so fish is never a single source.
- **The Country after the switch** (2.6): it keeps its other goods (woods, mines and metals, stone, fibre, herbs, its
  workshops: 1,194p a day), the Houses' estates and their 100 posts (824p), its patrons (51p). That pays its posts, and
  316p is left for its basket: **12% of today's basket survives**, and of its food and drink (cider, salt and smoked
  fish, wine, brandy) 75p of 598p. The farms' basket is what keeps the Country solvent: its makers draw iron, timber and
  stone from the Country.
- **The makers' order book** (2.6): the Country's basket falls by 2,211p a day; the farms' basket (1,094p), the crews'
  food (634p) and the crews' other spending (514p) bring 2,242p. Most trades the farms buy from keep their orders
  within 5% (the carpenters, cartwrights, masons, brickworks, plasterers, coopers, sawmills, ropewalk); the smithies lose
  19% (the steel bars, rivets and picks no farm buys), the tannery 24%. The food shops gain about 16%; the general shop,
  the chandlers and the vintners 27-29%. The trades only the Country's basket kept (the foundry, the ironworks, the
  distillery, the scriptorium, the paper mill, the charcoal burners, the cartographer) lose 87-88% (47-58% with the
  Country's smaller basket re-weighted to them, but the distillery: its brandies are drink, which the re-weighted
  basket doesn't buy), the brewery 39%.
- **The treasury's toll**: 1,595p now, 1,603p after (+0.5%). The farms' basket draws the same kinds of goods the
  Country's did. Without it the toll would fall to 1,418p (-11%).
- **Cash through the year** (section 3): employment is steady by design (fixed posts, fixed wages). The risk is cash by
  season, and the fix is that the basket, not the wages, takes the lean weeks. **Reserves: 4,484p in all**: two days of
  each farm's wages (52p to 112p a farm) and the vineyard's 2,300p for its autumn. A fixed basket would need 26,000 to
  33,000p, half the realm's money, which nobody could fund.
- **Feeding everyone** (2.4): the yields cover the city's draws after the switch (the 98 crews buy theirs in the city),
  the Country's own 96 who stay in the Country (72 workers of its other industries, 24 of the Houses' armed retainers)
  and 10% slack.
- **The NPCs** (section 3): DEV has 524 unplaced, 401 of working age. The farms want 98 and the other industries 72;
  every old trade is covered (orchards and vines now 17 of 23). After the 64 arrivals of a twelve-year run, 337 of
  working age would be left, 167 more than all of them.

## The model: how the game pays and feeds a wolf

One model throughout, as the code has it:

- **A post costs its payer its wage, nothing more.** `Society::payOf` (Core/RatwCityLoop.cpp, about line 87) gives each
  post a payer and a wage; `payWages` (about line 1031) shifts the wage from the payer to the holder and nothing else.
  The wage drift's floor, `wageDriftFloor` (7p, country.json `rules`: "a day's food"), is the only place food enters a
  wage: no post is paid less than a day's food.
- **The holder feeds itself out of the wage.** The households' market (about lines 740-830) tops every member up to
  `food.days` of NourishADay (45.8 nourishment, line 34), buying at the city's shops; then needs (crafts.json
  `households`), then comforts and services from what is over `comforts.above` a head. In a steady state a household
  spends what it earns.
- **What a shop sells it draws from the store room.** `Society::drawFromStore` (Core/RatwSociety.cpp, about line 1424)
  pays the Country `cost - toll`, the treasury the toll and the carters the carriage, all from the shop's till: the
  carriage is the shop's, on top of the draw. A farm that replaces the Country gets the `cost - toll`.

So a farm's crew are paid wages and are households in the city's loop. Their food (6.47p a wolf a day at the shops'
prices, the residents' diet) comes back to the farms as the shops' draws (2.66p a wolf after the toll), with 0.16p to
the Country (the bakers' and smokehouses' firewood). The rest of the wage goes on needs and comforts as the residents'
does: each penny of it draws 0.15p from the farms (the candles' tallow, the cider's apples, the scarves' wool) and 0.26p
from the Country. The farm's basket and the Country's are spent the same way (countryBuys: what is over the reserve and
tonight's posts, at the catalogue's price, from the makers holding the goods), and their makers draw again. The tool
solves it season by season (`solve`).

## How it was measured

- **The long run**: the 12-year run of doc 68, Progress, "Births follow deaths"
  (`artifacts/economy/r2/people_12year.jsonl` in the main repo), its **settled years 4-12** (days 1095-4379, 3,285
  days; residents 595 on average, 568 to 623). Its economy line has, per season group, the pieces drawn a day
  (`G_drawn`) and the season's price factor (`G_price`); not the pennies by good.
- **The pennies by good, and who they end with**: four 14-day runs of the same binary and export (DEV build 52, read
  only), one starting in each season (`world_check EXPORT --simulate 6 342 --no-check --deterministic --threads 2
  --no-profile --day N --economy F --events E`, N = 30, 122, 213, 303). The `--events` file has every ledger record: a
  draw's three coin shifts (to the Country, the gate toll, carriage in) come just before the `made` record that names its
  good, so each draw's good, pieces, pennies and reason pair up exactly; every sale names its shop and buyer (the shop's
  business read from the export's work labels, as `items::businessFor` reads them). Days 4-14 of each run are kept, and
  the run's economy lines for the same days (they match the events' sums to the penny).
- **Scaled to the long run** (`scale`):
  - a season group's goods: pieces by the long run's `G_drawn` over the short run's; pennies by that and by the season's
    own mean price (`G_price` over the settled years' season, against the short run's window, which sits just before
    the season's middle: fruit in autumn 0.809 in the window, 0.889 over the season);
  - a good outside the groups drawn for a craft whose other inputs are in a group follows that group: **salt** for salt
    fish follows the fish, for jerky and salt pork the meat (107p a day to the Country, not the second draft's 74p),
    firewood for bread the grain;
  - the rest (wine and cider resold, the non-food crafts) take what is left of the long run's drawn pennies (factor
    0.79-0.85: the long run's Country bought less than the short runs' did);
  - the toll at the long run's rate (31.7%);
  - the residents' food at NourishADay a head (the short runs' households ate down their starting stock, so bought
    less than they ate; scaled by their pieces, the second draft had them eating 49-50 a day), their other buying by the
    rest of their spending, the Country's by its spending.

  The totals match the long run's own (5,024p drawn, 1,595p toll); that is by construction, so it confirms nothing.
- **Who each draw ends with** (`streams`): a draw ends in its product (the good, resold; else the craft's output), and
  the product's buyers share it by their pieces: the residents' food, the residents' other buying, the Country's basket;
  a product neither buys is the city's (the treasury's works, the patrons, a maker's stock).
- `python3 tools/country_industry.py measure ... --world EXPORT/world.ratw --out MEASURED.json`, then `report MEASURED
  ECONOMY.jsonl --unplaced U.json --units Data/Economy/country_farms.json` prints every table below, and `--real-check`
  the real-yield cross-check (2.5). The measured file and both reports are kept, uncommitted, in the main repo's
  `artifacts/economy/r2/` (`country_measured.json`, `country-industry-report.md`, `country-industry-real-check.md`).
- **DEV's unplaced NPCs**: read from `live.npcs` (no home) with the editor role in a read-only transaction: **524**.

## 1. What the Country holds now

### 1a. By industry (a day; settled years)

Pieces / pennies the shops pay (the draw's cost, the season's price and the store's markup included: making 100%,
resale 75%), by season; the year's mean; the gate toll; carriage in (the shop pays the carters on top); the Country's
intake (after the toll); the swing of the season's pennies against the year's mean.

| Industry | Spring | Summer | Autumn | Winter | Year | Gate toll | Carriage in | The Country's intake | Season swing |
|---|---|---|---|---|---|---|---|---|---|
| livestock: meat | 343 / 1,011p | 332 / 1,025p | 327 / 993p | 340 / 1,036p | 336 / 1,016p | 323p | 3.6p | 693p | 98-102% |
| orchards | 532 / 737p | 513 / 521p | 519 / 534p | 503 / 805p | 517 / 649p | 206p | 11.7p | 443p | 80-124% |
| fishing | 309 / 570p | 264 / 562p | 275 / 573p | 360 / 649p | 302 / 588p | 186p | 2.2p | 402p | 96-110% |
| woods: timber, firewood, bark, charcoal | 451 / 548p | 435 / 508p | 446 / 522p | 578 / 645p | 477 / 555p | 176p | 49.6p | 379p | 91-116% |
| country workshops' goods | 133 / 502p | 128 / 482p | 152 / 590p | 126 / 469p | 134 / 511p | 162p | 1.5p | 349p | 92-116% |
| mining and metals | 71 / 424p | 57 / 340p | 59 / 356p | 69 / 412p | 64 / 383p | 122p | 2.7p | 261p | 89-111% |
| grain and milling | 129 / 280p | 152 / 231p | 156 / 248p | 124 / 272p | 140 / 258p | 81p | 4.0p | 177p | 90-109% |
| fibre: hemp, flax, linen | 111 / 211p | 95 / 180p | 98 / 187p | 108 / 201p | 103 / 195p | 63p | 1.0p | 132p | 92-108% |
| bees | 54 / 198p | 62 / 187p | 59 / 183p | 48 / 176p | 56 / 186p | 60p | 0.5p | 126p | 95-106% |
| vineyards | 36 / 222p | 39 / 197p | 20 / 44p | 30 / 191p | 31 / 164p | 52p | 0.7p | 112p | 27-136% |
| stone, lime, clay, sand | 139 / 166p | 119 / 142p | 118 / 143p | 142 / 173p | 129 / 156p | 50p | 17.2p | 106p | 91-111% |
| salt | 80 / 159p | 74 / 147p | 75 / 151p | 82 / 164p | 78 / 155p | 48p | 1.0p | 107p | 95-106% |
| livestock: wool, hides, tallow, bone | 84 / 115p | 79 / 107p | 83 / 112p | 78 / 110p | 81 / 111p | 34p | 0.8p | 77p | 97-104% |
| dairy | 55 / 85p | 75 / 76p | 79 / 80p | 54 / 82p | 66 / 81p | 26p | 1.0p | 55p | 94-105% |
| herbs | 7 / 9p | 7 / 9p | 8 / 10p | 10 / 11p | 8 / 10p | 3p | 0.0p | 7p | 88-116% |
| vegetables | 4 / 6p | 5 / 5p | 5 / 5p | 3 / 6p | 4 / 6p | 2p | 0.0p | 4p | 87-115% |
| **all** | **2,534 / 5,244p** | **2,436 / 4,719p** | **2,479 / 4,732p** | **2,655 / 5,404p** | **2,526 / 5,024p** | **1,595p** | **97p** | **3,429p** | |

- **The pennies follow the season prices more than the pieces do**: grain and fruit are cheapest at harvest (summer and
  autumn, 89-96%) and dearest in winter and spring (109-130%), so the orchards take 805p in winter for fewer apples than
  the 534p of autumn. Meat (no season) and fish (more in winter, cheaper then) are nearly flat.
- **Where the season caps bind** (the settled years): meat draws 317-330 a day against a cap of 330 all year; fish
  binds in spring, grain in winter and spring, honey and fruit in winter. The farms' yields carry 10% slack, so they can
  send a little more than the caps allow today (raise meat's `normal` with them).
- **Wine in autumn**: the city draws one wine a day in autumn against 17-20 in the other seasons: the city's own presses
  make their wine from grapes then (the vintage). That is the vineyard's lean season (section 3).

### 1b. The food goods, and why they are drawn

| Good | Catalogue | Spring | Summer | Autumn | Winter | Paid a day (year) | Drawn for |
|---|---|---|---|---|---|---|---|
| apples | 1p | 459 | 498 | 499 | 382 | 513p | cider 81%, dried apples 13%, preserves 7% |
| raw meat | 3p | 146 | 138 | 126 | 129 | 376p | jerky 37%, resale 34%, salt pork 27% |
| pig | 12p | 18 | 21 | 22 | 21 | 247p | the butchers' pigs |
| mutton | 6p | 32 | 37 | 33 | 36 | 207p | smoked mutton |
| rabbit | 1p | 134 | 128 | 139 | 142 | 136p | smoked rabbit |
| meal (the Country's cookshops' stew) | 6p | 13 | 8 | 8 | 12 | 51p | resale |
| fish | 2p | 186 | 174 | 173 | 216 | 403p | salt fish 57%, smoked fish 43% |
| shellfish | 2p | 123 | 90 | 102 | 144 | 186p | resale |
| honey | 3p | 53 | 62 | 59 | 47 | 184p | honey cakes 63%, preserves 30%, cordial 6% |
| flour | 2p | 72 | 81 | 81 | 67 | 180p | honey cakes 55%, bread 45% |
| salt | 2p | 80 | 74 | 75 | 82 | 155p | salt fish 44%, jerky 32%, salt pork 24% |
| wine | 12p | 20 | 17 | 1 | 17 | 127p | resale 91% |
| milk | 1p | 55 | 75 | 79 | 54 | 81p | porridge |
| oats | 1p | 54 | 69 | 72 | 53 | 76p | porridge |
| tallow | 1p | 54 | 51 | 55 | 50 | 52p | candles 90% |
| dried apples, preserves | 3p, 4p | 29, 13 | 0, 1 | 0, 3 | 48, 47 | 43p, 46p | resale (the Country's winter stores) |
| cider | 3p | 21 | 7 | 9 | 18 | 37p | resale 65%, apple brandy 35% |
| grapes | 2p | 15 | 22 | 20 | 13 | 37p | wine |
| hide, wool, bones | 5p, 2p, 1p | 6, 9, 14 | 5, 9, 13 | 5, 9, 13 | 6, 8, 12 | 27p, 17p, 13p | leather; scarves; charms |
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
**342p a day of the food areas' takings are behind the basket** (the apples behind its cider, the fish behind its salt
and smoked fish, the hides behind its leather), and **654p of the Country's other goods** (the iron behind its steel,
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
| cider | 6 | 3p | 64 | 65 | 82 | 67 | 1% | 2 apples |
| wine | 4 | 12p | 19 | 18 | 7 | 17 | 0% | 2 grapes (and a cask's share) |

A resident eats 45.8 nourishment a day (NourishADay) for **6.47p of food** at the shops' prices, and drinks and buys
comforts besides. Meat in all its forms is about 45% of what the city eats, grain (porridge, bread, honey cakes) 37%,
fish 12%, fruit and cider 6%. In drawn goods that is 1.74 pieces a head a day.

### 2.2 The farms' takings, now and after the switch

A day: "now" is the Country's part of today's draws of the same goods, at the season's own price; "after" is solved
with the recommended crews and baskets. The stored harvests (grain and fruit) sell from store at a price a year (fruit
101.5%, grain 101.0% of the catalogue, the year's mean of their season prices); meat, fish, honey and salt at the
season's, as now (section 3).

| Farm | Now (year) | After: Spring | Summer | Autumn | Winter | Year | Crew's wages (of the takings) | The farms' basket | Seasons under the wages |
|---|---|---|---|---|---|---|---|---|---|
| grain farm (2) | 177p | 207p | 196p | 223p | 199p | **206p** | 112p (54%) | 94p | - |
| orchard (4) | 443p | 320p | 278p | 317p | 395p | **327p** | 144p (44%) | 183p | - |
| vineyard (1) | 112p | 161p | 140p | 31p | 130p | **116p** | 56p (48%) | 66p | autumn |
| cattle and dairy farm (3) | 278p | 351p | 318p | 305p | 319p | **324p** | 168p (52%) | 156p | - |
| pig farm (3) | 291p | 326p | 341p | 345p | 344p | **339p** | 168p (50%) | 171p | - |
| sheep and goat run (2) | 162p | 180p | 203p | 183p | 196p | **191p** | 92p (48%) | 99p | - |
| rabbit warren (2) | 94p | 106p | 104p | 111p | 114p | **109p** | 52p (48%) | 57p | - |
| sea fishing boat (4) | 264p | 265p | 248p | 257p | 298p | **267p** | 144p (54%) | 123p | - |
| lake and river boat (2) | 83p | 66p | 74p | 71p | 75p | **72p** | 52p (73%) | 20p | - |
| eel and fowl marsh (1) | 55p | 44p | 49p | 48p | 50p | **48p** | 26p (54%) | 22p | - |
| beekeeper (3) | 126p | 149p | 141p | 140p | 130p | **140p** | 78p (56%) | 62p | - |
| salt works (1) | 107p | 115p | 106p | 105p | 113p | **110p** | 56p (51%) | 54p | - |
| **all** | **2,191p** | **2,290p** | **2,198p** | **2,136p** | **2,365p** | **2,247p** | **1,148p** | **1,094p** | |

**Where the takings come from**: now 2,195p, of it 1,853p the residents' and the city's draws and 342p behind the
Country's buy-back. After: the residents' 1,853p (unchanged); the Country's smaller basket 42p (so 299p of the buy-back's
draws are lost: 171p of it the orchards', the cider apples; 34p the sea boats', 22p the vineyard's, 21p the lake
boats', 14p the eel marsh's, 12p the salt works'); the crews' food 261p and their other spending 77p; the
farms' basket 17p (the hides in its leather); 2,252p in all. Vegetables (4p now) are dropped. So the switch gives the
farms a little more than the food earns the Country now, but not in the same places: **the orchards lose a quarter**
(the Country's cider), the lake boats and the eel marsh a seventh (its salt and smoked fish), while grain, the herds,
the bees and salt gain from the crews' own food.

- **Grain is cheap for what it feeds**: a measure of wheat is a flour is six loaves, 90 nourishment, two days of a
  wolf's food, and the Country gets about 1.6p for it after the toll (1.4p in summer and autumn, 1.9p in winter and
  spring). The bakers and inns keep most of bread's and porridge's value. With the toll and the crews' food, the two
  grain farms still pay 1 + 4 each.
- **Salt** is 107p to the Country, scaled with the fish and meat it is salted for: one works pays a keeper and four
  hands, with no price lever. (The second draft's levers, salt from 2p to about 2.3p and flour to about 2.1p, could not
  have been set: catalogue prices are whole pennies, and salt is in no season group. An area's `priceFactor`, in
  percent, is how a farm price would be moved, section 4.)

### 2.3 How many farms, and their crews

**The rule**: each farm's crew is what about half its takings pay at the city's wages (a keeper 16p, a hand 10p),
rounded, never under the smallest sensible crew for its kind; the other half is its basket (2.6). The crews' own food
and spending come back as takings, so the crews and the takings are solved together. Where an area can't pay a sensible
crew on each of several farms, its farms collapse into fewer, larger ones (the user's rule).

- **Grain, 2 large farms, each with its water mill** (the user's decision): 1 + 4 each (the floor: ploughing, reaping,
  threshing, the miller and his man). On two rivers, so never one source.
- **Orchards, 4, two with a drying house**: 1 + 2 each. Five would pay 1 + 2 too, but at 55% of their takings; four
  bigger ones keep a basket, since the orchards lose a quarter of their takings with the Country's cider. The dried
  apples and preserves are made in autumn from their own apples and sold in winter and spring, when fresh fruit is short.
- **Vineyard, 1 large** (the user's decision): 1 + 4, on the warm hills at the desert's edge. Its autumn is lean
  (section 3).
- **Livestock, 10 holdings in four kinds**: 3 cattle and dairy farms (all the milk: it keeps a day, so near the city;
  their calves are the beef), 3 pig farms (woodland pannage), 2 sheep and goat runs (the goats in the northern
  foothills' run, for meat and wool), 2 rabbit warrens. The raw meat is split 65% to the cattle and 35% to the pigs;
  the tallow, hides and bones as the herds are; each kind then pays 1 + 3 or 1 + 4.
- **Fish, 7 crews**: 4 sea boats (1 + 2: a three-wolf boat), 2 lake and river boats (1 + 1), 1 eel and fowl marsh
  (1 + 1). The fish is shared 50% to the sea boats (which also have all the shellfish), 30% to the lake and river boats,
  20% to the marsh. The lake boats are the thinnest (wages 73% of their takings) but pay their crew in every season.
- **Honey, 3 beekeepers** (1 + 1), two among the orchards, one in the desert edge's thyme scrub.
- **Salt, 1 works** (the user's decision): 1 + 4, on the marsh's coast, where the sea is let into clay pans, dried by
  the summer sun and boiled over peat the rest of the year.

**The farms' crews are 98 wolves** (28 keepers, 70 hands): about the second draft's 97, but now on half the takings
rather than all of them, since the food is no longer counted twice.

### 2.4 Yields per farm

The yields are not real-world yields (the user: "We don't need to stay realistic to actual yields"). Each farm's yield
of a good is its share of three things, plus 10% slack:

1. **The city's draws after the switch**, season by season. The 98 crews buy their food at the city's shops, so their
   food is among these draws; so is what the farms' and the Country's baskets draw.
2. **The Country's own people who stay in the Country** and eat from the farms without buying in the city: the 72
   workers of its other industries once they are placed (section 5) and the Houses' 24 armed retainers (country.json
   `patrons`), 96 in all, eating the residents' diet (1.74 drawn pieces a head a day). The Houses' other people hold the
   Country's 100 posts in the city and eat there, in the draws. No dependants are counted until families are placed.
3. The slack.

Pieces a day, a farm:

| Farm | Good | Spring / Summer / Autumn / Winter |
|---|---|---|
| grain farm (2) | flour (wheat ground at its mill) | 52.7 / 59.2 / 59.3 / 49.1 |
| | oats | 39.3 / 50.2 / 52.4 / 38.9 |
| | straw | 2.1 / 1.2 / 1.7 / 2.0 |
| orchard (4) | apples | 85.5 / 105.9 / 112.8 / 67.9 |
| | dried apples; fruit preserves (the two with drying houses carry them) | 10.7 / 0.0 / 0.0 / 17.4; 4.6 / 0.5 / 1.2 / 17.1 |
| | cider; berries | 2.5 / 0.7 / 1.0 / 2.2; about 0.3 |
| vineyard (1) | grapes | 19.3 / 26.5 / 23.3 / 16.0 |
| | wine | 23.9 / 18.6 / 0.1 / 19.3 |
| cattle and dairy farm (3) | raw meat | 46.0 / 43.8 / 39.7 / 40.7 |
| | milk | 26.9 / 36.5 / 38.3 / 26.1 |
| | meal; tallow; hides; bones | 2.5-4.1; about 12.5; about 1.2; about 2.6 |
| pig farm (3) | raw meat | 24.8 / 23.6 / 21.4 / 21.9 |
| | pigs | 8.7 / 10.1 / 10.7 / 10.4 |
| | meal; tallow; hides; bones | 1.3-2.2; about 7.5; about 0.8; about 1.6 |
| sheep and goat run (2) | mutton | 23.5 / 26.9 / 23.9 / 26.3 |
| | wool; tallow; bones | about 6; about 7.5; about 1.6 |
| rabbit warren (2) | rabbits | 98.0 / 93.6 / 101.3 / 103.5; pelts about 1 |
| sea fishing boat (4) | fish | 25.0 / 24.0 / 23.4 / 28.7 |
| | shellfish | 44.9 / 33.0 / 37.0 / 52.6 |
| lake and river boat (2) | fish | 30.0 / 28.8 / 28.1 / 34.4 |
| eel and fowl marsh (1) | fish (eels) | 39.9 / 38.4 / 37.5 / 45.9 |
| beekeeper (3) | honey | 24.2 / 28.7 / 27.4 / 21.2 |
| salt works (1) | salt | 102.2 / 95.8 / 97.1 / 103.1 |

- **The seasons' shape is the city's**: each yield follows what the city drew in that season (more grain in summer and
  autumn, more fish in winter, fruit short in winter, meat flat), so supply meets demand season by season with 10% over.
- **Stores, not fields**: for grain, apples, grapes, wine, honey and salt the yield is what the farm sells from its store
  in that season, not what it reaps that day. The game needn't hold the harvest as a year's stock (an account holds at
  most 10,000 of a good, `StockCap`; an orchard sells about 34,000 apples a year): the store is in the seasonal yields.
- **The opening stock** is created, not handed over: the Country holds no goods (a store draw creates the goods in the
  shop's till, and what the Country buys is consumed, "carried to the Country"). At the switch each farm's stock is
  created as a week of its yields (`create`, "grown at <farm>"; at most about 800 of a good, well under the cap). From
  then on the yields add to the stock daily, the Country's table takes its share ("eaten in the Country"), and stock
  over a cap spoils, so oversupply shows.

### 2.5 The cross-check: what real yields and labour would say

The tool's `--real-check` sizes food from pre-industrial yields (the first draft; ASSUMPTIONS and UNITS in the tool: a
tile about a metre, a game day's food about 2,500 kcal, a measure of wheat 1.5 kg; a three-field grain farm at 1 t a
hectare with a fifth kept for seed; orchards at 6 t a hectare; and so on). It is kept for land and labour, which the
money doesn't say:

- **Land**: these yields, drawn at a tenth of their real area (each tile of field 10 m^2), want about 303,000 tiles of
  the farms' own ground (4.6 cells of 256 x 256) and 1.27 million of shared grazing, heath and woodland (19.4 cells):
  **24 cells at 1:10** (the first draft's larger demand, with 488 Country people, came to 30.5), and ten times that at
  1:1. The placement list draws 14 (section 6).
- **Labour**: footnote to section 3.

### 2.6 The Country's money once the farms take the food

**What the Country still is**: its other goods (the woods, the mines and metals: iron stays the Country's for now,
stone, clay and sand, hemp and flax, herbs, its workshops' pots, scarves, soap and pamphlets), timber not yet placed, the
Houses' estates and their 100 posts in the city, its patrons, and its carriage. Its takings and spending (a day):

| | Now | After |
|---|---|---|
| Its takings: the food areas' goods | 2,195p | - (the farms') |
| Its takings: its other goods | 1,234p | **1,194p** |
| its 100 posts (the Houses' people and the wharf's) | 824p | 824p |
| its patrons (the Houses' retainers' kit and halls, the wharf's hulls) | 51p | 51p |
| carriage out | 27p | 3p |
| its basket of the city's goods | 2,527p | **316p (12%)** |
| of the basket, food and drink made from the farms' goods (cider, salt and smoked fish, wine, brandy) | 598p | 75p |

| The Country's takings by industry | Now | After |
|---|---|---|
| woods: timber, firewood, bark, charcoal | 379p | 361p |
| mining and metals | 261p | 198p |
| stone, lime, clay, sand | 106p | 108p |
| fibre: hemp, flax, linen | 132p | 91p |
| herbs | 7p | 8p |
| country workshops' goods (the cart wheels' and barrels' iron hoops among them) | 349p | 427p |

Without a replacement buyer the Country's basket would fall toward nothing (half its other goods are drawn to make what
its basket buys) and it could not pay its posts: its other goods alone, behind the residents' buying, bring about 580p
against 875p of posts and patrons. **The farms' basket is the replacement buyer**, and it keeps the Country solvent:
its cart wheels, barrels, nails, planks, stone and lime draw iron, timber and stone from the Country again, so the
Country's takings stay at 1,194p, its posts are paid, and 316p is left for its own basket.

**The farms' basket**: each farm spends what its till holds over its crew's wages and its reserve (two days of wages,
section 3) on the city's goods a farm uses: cart wheels and axles, barrels and iron hoops, sickles, farm gear, saws,
nails, fittings, locks, chains, buckles, cookpots, harness leather, rope, canvas and nets, planks, beams, cut stone,
bricks, roof tiles, mortar, lime, whitewash, plaster and glass, **in the shares the Country's basket buys them now**
(country.json; listed in `country_farms.json` `basket`), from the makers holding them, as the Country buys. It is
**1,094p a day**, about half the farms' takings: what the farms can afford after a sensible crew, and nearly all of the
1,212p the Country spends on those goods now.

**The makers' order book** (sales a day; the residents' own buying is the same before and after and left out; the
trades with Country orders or a change of 5p or more):

| Trade | Its sales now (all buyers) | The Country's basket now | After: the Country's basket | The farms' basket | The crews' food | The crews' other spending | Change (of its sales) | Or with the Country's basket re-weighted: its orders | Change |
|---|---|---|---|---|---|---|---|---|---|
| smithy | 379p | 355p | 44p | 232p |  | 7p | -71p (-19%) | 43p | -81p (-21%) |
| carpenter | 182p | 182p | 23p | 166p |  |  | +6p (+3%) |  | -23p (-12%) |
| brewery | 309p | 177p | 22p |  | 3p | 33p | -119p (-39%) |  | -141p (-46%) |
| provisioner | 1,274p | 139p | 17p |  | 187p |  | +66p (+5%) |  | +49p (+4%) |
| brickworks | 120p | 120p | 15p | 108p |  |  | +3p (+3%) |  | -16p (-13%) |
| inn | 473p | 118p | 15p |  | 47p | 20p | -36p (-8%) |  | -51p (-11%) |
| foundry | 115p | 115p | 14p |  |  |  | -101p (-88%) | 52p | -63p (-55%) |
| mason | 110p | 110p | 14p | 99p |  |  | +2p (+2%) |  | -15p (-14%) |
| tannery | 102p | 102p | 13p | 65p |  |  | -24p (-24%) | 13p | -27p (-26%) |
| ironworks | 91p | 91p | 11p |  |  |  | -80p (-88%) | 38p | -53p (-58%) |
| cartwright | 84p | 84p | 11p | 77p |  |  | +4p (+5%) |  | -10p (-11%) |
| distillery | 78p | 78p | 10p |  |  |  | -69p (-88%) |  | -78p (-100%) |
| glassworks | 71p | 71p | 9p | 36p |  |  | -26p (-37%) | 13p | -23p (-32%) |
| plasterworks | 69p | 69p | 9p | 62p |  |  | +2p (+2%) |  | -10p (-14%) |
| cooperage | 76p | 68p | 9p | 59p |  |  | -0p (-1%) |  | -11p (-15%) |
| sawmill | 65p | 65p | 8p | 59p |  |  | +2p (+3%) |  | -8p (-12%) |
| scriptorium | 64p | 64p | 8p |  |  |  | -56p (-88%) | 27p | -37p (-58%) |
| fishmonger | 287p | 52p | 6p |  | 39p |  | -6p (-2%) |  | -13p (-4%) |
| apothecary | 101p | 50p | 6p |  |  | 15p | -29p (-29%) | 24p | -12p (-11%) |
| paper mill | 47p | 47p | 6p |  |  |  | -41p (-87%) | 20p | -27p (-57%) |
| press | 99p | 45p | 6p |  |  | 16p | -23p (-24%) |  | -29p (-29%) |
| ropewalk | 45p | 45p | 6p | 39p |  |  | -1p (-1%) |  | -8p (-17%) |
| paintworks | 37p | 37p | 5p | 12p |  |  | -21p (-56%) | 12p | -13p (-37%) |
| charcoal burners | 34p | 34p | 4p |  |  |  | -30p (-87%) | 15p | -20p (-57%) |
| builders' works | 32p | 29p | 4p | 26p |  | 1p | +2p (+7%) |  | -2p (-7%) |
| cartographer | 28p | 28p | 4p |  |  |  | -25p (-87%) | 15p | -13p (-47%) |
| tinker | 61p | 28p | 3p | 19p |  | 10p | +5p (+8%) | 3p | +4p (+6%) |
| printing house | 56p | 28p | 3p |  |  | 8p | -16p (-28%) | 11p | -8p (-14%) |
| armoury | 41p | 22p | 3p |  |  |  | -19p (-46%) | 9p | -13p (-31%) |
| jeweller | 46p | 19p | 2p |  |  | 8p | -9p (-19%) | 11p | +0p (+0%) |
| chandlery | 192p | 13p | 2p | 12p |  | 52p | +52p (+27%) |  | +50p (+26%) |
| weaver | 59p | 13p | 2p | 12p |  | 13p | +14p (+23%) |  | +12p (+20%) |
| pottery | 46p | 12p | 2p | 11p |  | 10p | +10p (+22%) |  | +8p (+18%) |
| smokehouse | 112p | 8p | 1p |  | 17p |  | +11p (+10%) |  | +10p (+9%) |
| general goods | 562p | 6p | 1p |  | 1p | 157p | +153p (+27%) | 3p | +156p (+28%) |
| bakery | 743p |  |  |  | 122p |  | +122p (+16%) |  | +122p (+16%) |
| vintner | 157p |  |  |  |  | 46p | +46p (+29%) |  | +46p (+29%) |
| butcher | 708p |  |  |  | 117p |  | +117p (+16%) |  | +117p (+16%) |
| stall | 179p |  |  |  | 30p |  | +30p (+16%) |  | +30p (+16%) |
| preserver | 435p |  |  |  | 71p | 1p | +72p (+17%) |  | +72p (+17%) |
| rag and bone | 203p |  |  |  |  | 59p | +59p (+29%) |  | +59p (+29%) |
| the services and small trades (tailor, perfumer, pawnbroker, barber, moneychanger, stables, builders' yard) | 222p | 4p | 1p |  |  | 54p | +50p (+23%) | 2p | +52p (+23%) |
| **all** | **8,235p** | **2,527p** | **316p** | **1,094p** | **634p** | **514p** | **+30p** | **312p** | **-13p** |

**What fills the gap**: the Country's basket falls by 2,211p a day. In its place come **the farms' basket** (1,094p, to
the trades that make farm goods: the carpenters, cartwrights, masons, brickworks, plasterers, coopers, sawmills and
ropewalk keep their orders within a few per cent, the smithies lose 19%, the tannery 24%, the glassworks 37%, for the
goods of theirs no farm buys), **the crews' food** (634p, to the food shops:
the bakers, butchers, preservers and provisioners gain about 16%), **the crews' other spending on needs and comforts**
(514p, to the general shops, chandlers, vintners, the rag and bone shop, the inns and the brewery's cider), and **the
Country's smaller basket** (316p). In all +30p, since the farms spend all their takings where the Country kept a purse.

**Who still loses**: the trades whose goods only the Country bought and no farm needs: the foundry and ironworks (iron
and steel bars), the distillery (brandies), the scriptorium and paper mill (paper and ledgers), the charcoal burners,
the cartographer (87-88%), the paintworks (56%), the armoury (46%), and the brewery its Country cider (39%). Their tills
would then go short, their wages drift down, and the treasury's wage support covers part of it. The Country's smaller
basket is the lever (open question 2): **re-weighted to the goods the farms don't buy and no food or drink**, its 316p
goes to those trades, their worst losses fall to 47-58% (the distillery loses all its Country orders: its brandies are
drink made from the farms' wine and cider), and the farm goods' trades give up the Country's last 11-15%.

**The treasury's toll**: 1,595p now (1,132p on the residents' and the city's draws, 464p behind the Country's basket).
After: **1,603p (+0.5%)**: the residents' 1,132p, the Country's smaller basket 58p, the crews' food 129p and other
spending 99p, the farms' basket 185p. The farms' goods still pass the gates as store draws do, so they pay the toll. An
independent check estimated about -300p (-19%) by taking the Country's basket away with only the crews' food in its
place; the farms' basket draws the same kinds of goods (timber, iron hoops, stone, lime) and puts most of it back.
Without the farms' basket the toll would be 1,418p (-11%). The toll drifts with the treasury's level (`treasuryDrift`),
so a change that small is within its trim.

## 3. Labour and money (item 10)

**Employment is steady year-round, by design.** The game has no seasonal hiring: a post has one holder at a fixed daily
wage (`payOf`), the work bank holds at most the longest craft (`workBankMost`), and the city's labourers and the wharf's
carriers already hold posts of their own. The yields are settings, not the work of the crew, so the harvest and the
sowing change nothing a farm pays: **98 crew are paid every day of the year**, and no harvest is late or lost. The
honest answer to "whether or not funding is adequate to keep them employed through harvest and sowing without much
disruption" is that employment can't be disrupted by the seasons; **the risk is cash by season**: a farm's takings
follow the city's draws and the season's prices, and its wages don't.

**Funding is adequate, farm by farm and season by season.** With the crews at about half the takings, every farm's
takings cover its wages in every season (2.2) but the vineyard's in autumn, when the city's presses make their own wine
from grapes (1a): 31p a day against 56p of wages.

### Reserves

Pennies, and days of the farm's crew cost. A crew's cost is now its wages (its food is bought out of them), so a day
of crew cost is a day of wages; the basket is spent only from what is over them:

| Farm | Farms | A fixed basket, the season's prices | A fixed basket, the stores at a price a year | **Recommended: the basket takes what is over the wages and two days' float** | A farm |
|---|---|---|---|---|---|
| grain farm | 2 | 2,573p (23) | 1,519p (14) | 224p (2) | 112p |
| orchard | 4 | 11,434p (79) | 6,139p (43) | 288p (2) | 72p |
| vineyard | 1 | 8,265p (148) | 7,721p (138) | 2,300p (41) | 2,300p |
| cattle and dairy farm | 3 | 2,550p (15) | 2,501p (15) | 336p (2) | 112p |
| pig farm | 3 | 1,143p (7) | 1,181p (7) | 336p (2) | 112p |
| sheep and goat run | 2 | 1,153p (13) | 1,153p (13) | 184p (2) | 92p |
| rabbit warren | 2 | 648p (12) | 648p (12) | 104p (2) | 52p |
| sea fishing boat | 4 | 2,870p (20) | 2,830p (20) | 288p (2) | 72p |
| lake and river boat | 2 | 474p (9) | 494p (9) | 104p (2) | 52p |
| eel and fowl marsh | 1 | 316p (12) | 329p (13) | 52p (2) | 52p |
| beekeeper | 3 | 946p (12) | 947p (12) | 156p (2) | 52p |
| salt works | 1 | 791p (14) | 766p (14) | 112p (2) | 112p |
| **all** | **28** | **33,161p** | **26,229p** | **4,484p** | |

- **A fixed basket can't be funded**: 26,000-33,000p is half the realm's money (50,252p, conserved), against a Country
  purse of 589p on average and a treasury of about 5,400p. Parked in farm tills it would also starve the city of the
  money its households and shops turn over.
- **The fix is the basket, not a fund.** Each farm keeps two days of its crew's wages (enough for a Restday, when the
  shops draw about half: the deepest dip in the long run's draws was 1.2 days of their mean) and spends what is over
  that on its basket, as the Country spends what is over its reserve and posts. A lean week cuts the basket, never the
  wages. That leaves 2,184p of floats, about a day of the farms' takings, and the vineyard's 2,300p.
- **Storage, for the stored harvests**: the granary, the fruit store and the cellar sell from store at a price a year
  (grain 101.0%, fruit 101.5% of the catalogue: the year's mean of their season prices). The city's seasonal shape then
  comes from the shops' prices to the residents (the season groups' `price`, as now) and the shelves, not from the farms'
  cash, and the orchards' and grain farms' baskets swing less (a fixed basket would need 6,139p for the orchards, not
  11,434p). Fish, honey, meat and salt keep the season's price: their season prices steady their takings (more fish in
  winter, but cheaper), and an even price would swing them more. Smaller seasonal swings in the farms' supply were the
  other choice; they would change what the city eats in each season, which storage doesn't.
- **The vineyard's 2,300p** (41 days of its wages) carries its autumn. It makes it in its own spring and summer (105p and
  84p a day over its wages, about 22 days of spring), so the farms should be switched on in spring (open question 3).
  Nobody need lend the reserves: every farm's float is two days of its own wages (about a day of its takings), and its
  basket waits until the float is full.

**The NPCs** (DEV's 524 unplaced, read only; working age 16-59):

| Old trade | Unplaced | Working age | The food farms' crews | The other industries (what they can pay, at most the plausible) | Spare (+) or short (-) |
|---|---|---|---|---|---|
| fields and mills | 60 | 60 | 10 | 7 (fibre) | +43 |
| orchards and vines | 25 | 23 | 17 | | +6 |
| herds and flocks | 61 | 61 | 42 | | +19 |
| bees | 6 | 6 | 6 | | 0 |
| fishing and the marsh (and salt) | 27 | 26 | 23 | | +3 |
| woods | 36 | 36 | | 11 | +25 |
| mines, stone and clay | 28 | 28 | | 24 | +4 |
| keeps the house | 52 | 52 | | | +52 |
| the towns' trades and shops | 41 | 37 | | 30 (the workshops) | +7 |
| the watch | 12 | 12 | | | +12 |
| service and labour | 39 | 37 | | | +37 |
| the Heights | 17 | 16 | | | +16 |
| children | 97 | 4 | | | +4 |
| the old | 23 | 3 | | | +3 |
| **all** | **524** | **401** | **98** | **72** | **+231** |

(Ages 6 to 80; 93 under 16, 30 aged 60 or over; 268 female, 256 male; ids `cty_` 200 (the Country's own), `tn_` 262
(the old map's towns), `rm_` 62.) The pool covers every crew by its own old trade. The arrivals of doc 68's rule (an
empty city post brings in an unplaced NPC of its class) drew 64 in the twelve-year run; each fills a city post, so is of
working age: **337 of working age would be left, 167 more than the farms' crews and the other industries' workers**.

**Footnote: real labour.** Sized from pre-industrial work for these yields (the cross-check's norms), the food would
want about 101 workers on the job in spring, 112 in summer, 122 in autumn and 80 in winter, against the 98 crew all year:
about 3,700 hand-days a year over the crew at the summer and autumn peaks and 1,600 under it in winter. The game's posts
don't follow it, and the yields are set as data.

## 4. What the code needs (an outline; another step builds it)

The user: "We aren't going to place any NPC's or farms or other things from Country yet", and "make sure that yield is
adjustable in Atlas' economy controls".

**Phase A: the farms as settings.** `Data/Economy/country_farms.json` (written by the tool, this doc's numbers) becomes
the defaults; each world keeps its own in `live.economy` (a migration: `farms jsonb` and `farm` manifest lines, as doc
68's seasons did), read by Core into `EconomySpec::farms`:

- **Defaults**: the farms' wages (a keeper 16p, a hand 10p; fixed: the basket, not a keeper's profit, takes what is over
  them, so the wage drift would never move them), `basketShare` (50: the crew rule's split), `floatDays` (2), `slack`
  (10), `estatePeople` (24) and `otherWorkers` (72: the Country's own table), `openingStockDays` (7), and the `basket`'s
  goods and shares.
- **Per area**: its goods, `on` (off: the Country still supplies them), `yieldScale` (percent on all its farms), `price`
  ("year" with a `priceFactor` in percent: the stored harvests; or "season": the season groups' price, as now). A
  percent factor is how a price is moved between whole pennies: no catalogue price changes.
- **Per farm**: its id, kind, area and place (unset until the map places it); `crew` (keepers, hands); `wages` (null:
  the defaults'); `basketADay` (the design's mean, for the reports: the basket itself is what is over the float);
  `reserve`; `yield` (pieces a day of each good for Spring, Summer, Autumn and Winter, blended between the seasons'
  middle days as the season groups are, `Society::seasonal`); its store's description.
- **Atlas's Economy page**: a Farms panel beside the Seasons panel: each area with its farms, their crews, wages,
  baskets and yields (four numbers a good), `yieldScale`, `price`, and a "from country_farms.json" reset, as the seasons
  have. **The DM's Economy tab** shows the same, editable on the DM's world, applied live by `economy.sync` (doc 68,
  "Seasons and the DM's rules"); both audited. The DM sees each farm's till, stock, last week's sales, wages and basket.
- **Tests**: the yields blend by day; a farm's yield scales with its area; a farm off returns its goods to the Country.

**Phase B: modelled producers with virtual crews (the simulation).** Each farm runs without NPCs:

- **A till** (`till:farm:<id>`), its crew as **virtual posts** paid at 18:00 from the till at the farm's fixed wages, to
  the farm's own household accounts (nobody holds them yet). Their wages are spent as a household's: food at the city's
  shops, needs and comforts by doc 68's rules. Money is conserved.
- **Production**: at the day's turn each farm adds its yield to its stock (`create`, "grown at <farm>"); the Country's
  table takes its share (`consume`, "eaten in the Country": `estatePeople` and `otherWorkers` at the residents' diet);
  stock over its cap spoils (`consume`, "spoiled"). At the switch the opening stock is created (`openingStockDays`).
- **Selling**: the store draw (`drawFromStore`) buys a good first from the farms that hold it (spread by stock, as the
  Country's purchases are), at the store's markup and the area's price (the year's factor or the season's), the toll and
  the carriage as now (the shop pays the carriage); the Country only where no farm has it ("Country when short", the
  per-good switch doc 68, 2 left for "the first farm"). An area's goods go into `country_off` once its farms are on.
- **The farms' basket**: like `countryBuys`: each hour the shops are open, what the till holds over its float and
  tonight's wages, spread over the basket's goods by their shares, from the makers holding them, a piece at a time.
- **The Country** keeps its purse, its posts, its patrons, its other goods and its basket (re-weighted, if the user
  agrees). A farm short of wages draws on its float, then the treasury's wage support (doc 68's rule, as for a till).
- **Vegetables go** from Ridgemere 2.0's economy (the item stays in the catalogue for the old map). They are listed in:
  crafts.json `crafts` (the inns' stew: raw meat + vegetables -> meal), `buys` (the inn), `supplies` (the stall and the
  provisioner); country.json `seasons` (the vegetables group, with mushrooms); recipes.json (meal stew, mushroom stew:
  the players' recipes); regions.json (the old map's places); Data/Economy/ladder.json (vegetables, a staple). The stew
  needs another second input (open question 5).
- **Reports**: the economy line gains per farm and area its stock, sales, wages, basket, float and spoilage;
  `tools/econ_detail.py` gains a farms section.
- **3-year runs** (each needs the user's go-ahead, as all simulations): the farms on area by area, then all, switched on
  in spring; the gates of doc 68, 10 plus: every farm pays its crew 99% of days, no area's stock runs out more than a
  day a season, spoilage under 10%, the city's prices within the long run's band, the Country pays its posts every day,
  and the order book within this doc's.

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
counted (what it can pay, at most the plausible) are the ones the farms' yields feed (2.4).

| Industry | Drawn a day | The Country's takings now | After | Wolves it can pay | Workers, the game's rates | Plausible | Counted | Units (where) |
|---|---|---|---|---|---|---|---|---|
| woods: timber, firewood, bark, charcoal | firewood 371, timber 58, planks 25, charcoal 13, bark 11 | 379p | 361p | 28 | 69 | 11 | 11 | 4 logging camps (two in the conifer forests of the northern mountains' foothills, two in the central hills' oak woods); 4 coppices with woodcutters' huts (beside the farm villages and two near the city: the city burns 340-460 firewood a day); 3 charcoal hearths (the foothill forests beside the bloomeries, two; the central hills, one); 2 water sawmills (one below each pair of camps) |
| mining and metals | iron bars 37, iron ore 17, copper ore 5, tin ore 4 | 261p | 198p | 16 | 38 | 38 | 16 | 2 iron mines (two valleys of the northern mountains); 2 bloomeries (beside them); 1 copper pit (the northern foothills); 1 tin streams (a stream of the central hills) |
| stone, lime, clay, sand | clay 53, limestone 28, stone 25, sand 22 | 106p | 108p | 8 | 13 | 13 | 8 | 2 quarries (the central hills' limestone; building stone in the nearest hills east of the city); 2 clay pits and sand banks (the plains' rivers; the coast south of the city) |
| fibre: hemp, flax, linen | hemp 39, cord 29, linen 23, flax 11 | 132p | 91p | 7 | 37 | 19 | 7 | 3 hemp and flax fields (a fourth field at the grain farms; retting ponds by the rivers) |
| herbs | 8 | 7p | 8p | 0 | 1 | 1 | 0 | 1 herb gatherers' cottage (the marsh's edge) |
| country workshops' goods | clay pots 35, wool scarves 30, soap 26, pamphlets 25, iron hoops 10 | 349p | 427p | 38 | 30 | 30 | 30 | 2 village potteries (by the plains' clay pits and the southern coast's); 3 weavers' and soap boilers' cottages (the farm villages); 2 country presses and paper mills (on the plains' rivers, one nearer the city) |

- **Two or more sources each**, as the user asked, where the money pays them: two logging districts and two sawmills,
  two iron mines and two bloomeries, two quarries, two clay pits, two potteries, two presses and paper mills, three
  fibre fields. **One each, and why**: the copper pit and the tin streams are one source of each ore, but the two ores
  are different places, and each ore is 4-5 pieces a day (10-13p), which can't pay a second crew; the herb gatherers'
  cottage (8p a day pays no one: it is the marsh people's gathering).
- **Iron stays the Country's** (the user). Its sites are reserved on the map: the two iron mines and their bloomeries
  in the northern mountains, their charcoal hearths in the foothill forests. Mining takes in 198p after the switch and
  could pay 16 wolves, against about 38 that smelting bars from ore takes at the game's rates and real ones alike; the
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
basket, reserve, yields by season and store; and the other industries' works with their counts, regions and
footprints.

**The scale** (the user: "somewhere in the middle"): about **14 cells** in all, **6.5 of the farms' own ground and 7.5
of shared range**, each kind's share as real land would have it (the cross-check on these yields, 2.5). That is between
the second draft's 8.3 cells (3.3 + 5, chosen to look right) and the real proportion (24 cells at 1:10 for these yields:
4.6 + 19.4). The farms' own ground is drawn at about 1:7 of its real area and the range at about 1:26.

| Kind | Count | Region (distance from Ridgemere) | Terrain, near | Own tiles each | Shared range each |
|---|---|---|---|---|---|
| grain farm with a water mill | 2 | the central plains, each on its own river east of the city (middle) | plain, meadow; road, river | 133,600 (about 365 x 365: several fields round the steading and the mill) | - |
| orchard (2 with a drying house) | 4 | the central hills (2, middle); the sheltered valleys of the coast south of the city (2, near) | hills, meadow; road | 8,000 | - |
| vineyard | 1 | the warm hills at the desert's edge (far) | hills; road | 14,200 | - |
| cattle and dairy farm | 3 | the wet coastal pasture just south of the city (2), the meadows just east of it (1) (near) | meadow, pasture; the city, road | 28,700 | 84,300 |
| pig farm | 3 | the central hills' oak and beech woods (middle) | forest, hills; road | 2,100 | 28,300 |
| sheep and goat run | 2 | the central hills' downs; the northern foothills (middle) | hills, downs, mountain; road | 2,100 | 46,700 |
| rabbit warren | 2 | the marsh coast's dunes (middle); the desert's sandy edge (far) | heath, dunes, desert; road | 1,100 (the walled warren) | 22,600 |
| sea fishing boat | 4 | the sea off the city's wharf (2), a cove on the coast south of the city (near); the marsh's coast (middle) | coast; sea | 450 (a landing) | - |
| lake and river boat | 2 | a lake under the northern mountains; the plains' largest river (middle) | lake, river | 240 (a jetty) | - |
| eel and fowl marsh | 1 | the marsh south of the city (middle) | marsh; water, road | 375 | 15,000 |
| beekeeper | 3 | among the orchards of the central hills (middle) and the southern coast (near); the desert edge's scrub (far) | hills, meadow, scrub; an orchard | 3,400 | - |
| salt works | 1 | the marsh's coast (middle) | coast, marsh; sea, road | 3,000 | - |
| logging camp, coppice, charcoal hearth, water sawmill | 4, 4, 3, 2 | the northern foothills' forests and the central hills' woods; beside the villages | forest; road, river | 1,200, 300, 900, 600 | 40,000, 10,000, -, - |
| iron mine, bloomery, copper pit, tin streams | 2, 2, 1, 1 | the northern mountains and foothills; a stream of the central hills | mountain, hills; road | 1,200, 600, 600, 600 | - |
| quarry, clay pit and sand bank | 2, 2 | the central hills, the hills east of the city; the plains' rivers, the southern coast | hills, river, coast | 4,800, 2,400 | - |
| hemp and flax fields, herb cottage, pottery, weavers' cottages, press and paper mill | 3, 1, 2, 3, 2 | the grain farms, the marsh's edge, the villages, the plains' rivers | | 4,800, 150, 500, 300, 600 | -, 22,500 |

The food farms' own ground is 6.5 cells and their shared range 7.5. Distances: near is within 2 cells of Ridgemere,
middle 3-8, far 9 or more (the desert). Carriage today is a flat 6p a wagon load whatever the distance, paid by the
shop that draws; if the realm's size makes the desert's wine visibly far, a rate by distance would cost the shops more,
and wine is light enough to bear it.

## Open questions for the user

Answered and applied (2026-11-01): vegetables dropped; collapse where crews can't be paid (two grain farms, one
vineyard, one salt works); the farms' basket; iron stays in the Country; the fields' scale; the map's geography.
Still the user's call:

1. **The split between crew and basket.** Recommended: half and half (98 crew, a 1,094p basket), so the trades the
   Country's basket kept busy keep most of their orders. More crew (up to the 208 the takings could pay on wages alone)
   would take orders from the makers and give them to the food and comfort shops.
2. **The Country's smaller basket (316p).** Recommended: **re-weight it** to the goods the farms don't buy and no food
   or drink (iron and steel bars, paper, ledgers, maps, ink, charcoal, glass beads, handbells, paint, bandages): the
   foundry's, ironworks', scriptorium's, paper mill's, charcoal burners' and cartographer's losses fall from 87-88% to
   47-58%, and the farm goods' trades give up 11-15% they would otherwise keep (the distillery loses its last 12%). Or
   leave its shares as they are.
3. **When to switch, and the vineyard's autumn.** Recommended: switch the farms on in spring, so the vineyard makes its
   2,300p for autumn from its own spring and summer, and every other farm's two days' float from its first days. Or a
   smaller vineyard crew (1 + 3 needs about 1,400p).
4. **The farms' wages.** Recommended: fixed at the city tills' settled means in whole pennies (a keeper 16p, a hand
   10p), editable per farm. Or let them drift, which needs the farm's basket counted as its keeper's spare (the drift
   raises a till's wages only when its keeper's profit is at least half its wages).
5. **The inns' stew without vegetables.** Recommended: raw meat and bones (a bone broth: the bones are the herds', so
   the farms keep the penny), with the meal's nourishment as now. Or raw meat alone.
6. **The Country's people the farms feed** (96: the other industries' 72 workers and the Houses' 24 retainers).
   Recommended: no dependants until families are placed; raise the areas' `yieldScale` when they are.
7. **The stored harvests' price.** Recommended: grain and fruit sell from store at a price a year (their mean of the
   season prices); fish, meat, honey and salt at the season's. Or every area at the season's price, as the store room
   charges now: the orchards' takings would swing 252-427p a day by season rather than 278-395p, and their basket with
   them.
8. **The grain farms' size on the map.** Recommended: keep real proportions (each about 365 x 365 tiles, 4.1 of the 6.5
   cells of the farms' own ground, drawn as several fields round the steading and mill). Or spread some of their ground
   to the orchards and the dairy farms.
