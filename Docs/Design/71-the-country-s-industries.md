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

Analysis only: no game rule, setting or world is changed, DEV is only read. Builds on doc 68 (the Country and the
city's loop, all its Progress sections), doc 61 (industry of the realm) and doc 60 (the old map's farms). The map is
being widened at the same time by another session: Ridgemere stays the north-west-most city with the sea on its west;
new land is added east and south of it (the N/NE mountains along the north, the central hills and plains, the SW marsh
south of the city along the coast, the SE desert in the far south-east). Regions and distances below use that.

**The tool**: `tools/country_industry.py` (new). **The placement list and the farms' settings**:
`Data/Economy/country_farms.json` (new; nothing reads it yet).

## How it was measured

- **The long run**: the 12-year run of doc 68, Progress, "Births follow deaths" (`artifacts/economy/r2/people_12year.jsonl`
  in the main repo), its **settled years 4-12** (3,285 days; residents 595 on average, 559 to 623). Its economy line has,
  per season group, the pieces drawn from the Country a day (`G_drawn`), the season's price factor (`G_price`, which
  scales both the store draws and the shops' prices) and the residents' mean price a piece (`G_paid`, retail, not what
  the Country gets); not the pennies per good.
- **The pennies per good**: four 14-day runs of the same binary and export (DEV build 52, read only), one starting in
  each season (`world_check EXPORT --simulate 6 342 --no-check --deterministic --threads 2 --no-profile --day N
  --economy F --events E`, N = 30, 122, 213, 303; about 18 s each). The `--events` file has every ledger record; a store
  draw's three coin shifts (to the Country, the gate toll, carriage in) come just before the `made` record that names
  its good, so each draw's good, pieces, pennies and reason (resale, or the craft it was drawn for) pair up exactly.
  Days 4-13 of each run are kept (the shelves fill in the first three).
- **Scaled to the long run**: each season's draws by good are scaled so that each season group's pieces are the settled
  years' `G_drawn` for that season, the goods outside the groups take the rest of the long run's drawn pennies, and the
  gate toll is at the long run's rate (the short runs start at the world's 30%; the drift firmed it to 31-33%). The
  totals then match the long run's own exactly: 5,024p drawn a day, 1,595p toll.
- `python3 tools/country_industry.py measure ...` then `report MEASURED ECONOMY.jsonl --unplaced U.json --units
  Data/Economy/country_farms.json` gives every table below. The measured file and the reports are kept, uncommitted, in
  the main repo's `artifacts/economy/r2/country_measured.json`, `country-industry-report.md` and
  `country-industry-real-check.md`.
- **DEV's unplaced NPCs**: read from `live.npcs` (no home) with the editor role in a read-only transaction: **524**.
  The 12-year run used 64 of them as arrivals inside the run only (DEV is untouched), so a world that had run twelve
  years would have about 460.

## The table: what each food area pays for, its farms, their crews and yields

Every number is a day, the settled years' mean. **Revenue** is what the city's shops pay the Country for the area's
goods at today's prices (the season's price factor included), less the gate toll and carriage in. **A wolf costs** a
keeper 23.5p or a hand 18.2p a day: the city tills' drifted wages over the settled years (keeper 15.7p, hand 10.4p),
the wolf's own food at city prices (6.8p, what a resident pays a day for what it eats, drink left out) and 1p of
upkeep (tools, nails, planks, whitewash). **Wolves it can pay**: a keeper a farm, the rest hands. **Yield**: the
farm's share of the city's draws of its goods plus what the 97 producers eat (the residents' diet), with 10% slack,
in pieces a day by season.

| Area | Farm | Revenue a day, all its farms (season low-high) | Wolves it can pay | Farms | Crew a farm | Yield a farm a day, Sp / Su / Au / Wi | A worker a day | Region (distance) |
|---|---|---|---|---|---|---|---|---|
| grain and flour | grain farm (two with a water mill) | 173p (155-191) | 8.6 | 3 | 1 + 1 (**short 7p** of 1 + 2) | 55 / 64 / 66 / 53 | 28 / 32 / 33 / 26 | central plains, on the rivers (middle) |
| orchards and vines; dried and preserved fruit | orchard (two with a drying house) | 429p (324-540) | 22.1 | 5 | 1 + 3 | 128 / 126 / 127 / 120 | 32 / 31 / 32 / 30 | central hills' south slopes (3); the NW coast's sheltered valleys, cider country (2) (middle, near) |
| | vineyard | 114p (28-154) | 5.7 | 2 | 1 + 1 (**short 6p** of 1 + 2) | 23 / 25 / 13 / 19 | 11 / 13 / 6 / 10 | the SE desert's warm edge; the central hills' sunniest slope (far, middle) |
| meat, milk and the herds' by-products | cattle and dairy farm | 271p (253-296) | 14.0 | 3 | 1 + 3 | 66 / 68 / 65 / 60 | 16 / 17 / 16 / 15 | the NW coast's wet pasture by the city; the foot of the Rainward Hills (near) |
| | pig farm | 293p (283-301) | 15.2 | 3 | 1 + 4 | 32 / 31 / 29 / 31 | 6 / 6 / 6 / 6 | central hills' oak and beech woods (2); the Rainwood's edge (1) (middle, near) |
| | sheep and goat run | 171p (163-179) | 8.8 | 2 | 1 + 3 | 26 / 31 / 28 / 29 | 7 / 8 / 7 / 7 | central hills' downs; the N/NE foothills, with the goats (middle) |
| | rabbit warren | 93p (89-98) | 4.5 | 2 | 1 + 1 | 87 / 82 / 88 / 92 | 44 / 41 / 44 / 46 | the SW marsh's dunes; the SE desert's sandy edge (middle, far) |
| fish and shellfish | sea fishing boat | 251p (236-278) | 12.6 | 4 | 1 + 2 | 66 / 53 / 57 / 78 | 22 / 18 / 19 / 26 | Ridgemere Sound (2), a north cove, the SW marsh's coast (near, middle) |
| | lake and river boat | 97p (89-102) | 4.7 | 2 | 1 + 1 | 40 / 38 / 37 / 47 | 20 / 19 / 19 / 24 | Glasswater under the N/NE mountains; the Long River (middle) |
| | eel and fowl marsh | 55p (51-58) | 2.7 | 1 | 1 + 1 | 46 / 43 / 43 / 54 | 23 / 22 / 21 / 27 | the SW marsh (middle) |
| honey | beekeeper | 126p (120-138) | 6.1 | 3 | 1 + 1 | 23 / 26 / 25 / 20 | 11 / 13 / 13 / 10 | among the central hills' orchards (2); the SE desert's thyme scrub (middle, far) |
| salt | salt works | 74p (69-83) | 3.5 | 2 | 1 + 0 (**short 10p** of 1 + 1) | 37 / 32 / 33 / 33 | 37 / 32 / 33 / 33 | the SW marsh's coast (brine pans); an SE desert pan (middle, far) |
| vegetables | market garden | 5p (3-7) | none | 2 | **can't pay a wolf** (short 78p of 1 + 1) | 4 / 4 / 3 / 3 | | outside the city's gates (near) |

**In all: 2,151p a day, 108 wolves it can pay; 34 farms with crews of 97 wolves (34 keepers, 63 hands), costing
1,947p a day.** (Milk is in the cattle and dairy farms, 85%, and the sheep and goat runs, 15%: alone, two dairies take
55p and can't pay a keeper and a hand each. The herds' tallow, hides, bone, wool and pelts, 79p a day, are the
holdings' too: "the farms' own sales of wool and hides".)

**Flagged, and the lever that fixes each** (pennies a day, all farms of the kind):

| Farm | A sensible crew costs | Revenue | Short | No gate toll on its goods | Its crew's food at the Country's own prices | Or its prices up by |
|---|---|---|---|---|---|---|
| grain farm | 3 x (1 + 2): 180p | 173p | 7p | +80p | +20p | 4% (flour 2p to about 2.1p) |
| vineyard | 2 x (1 + 2): 120p | 114p | 6p | +53p | +13p | 6% |
| salt works | 2 x (1 + 1): 83p | 74p | 10p | +34p | +9p | 13% |
| market garden | 2 x (1 + 1): 83p | 5p | 78p | +2p | +9p | 15 times |

- **The grain farms and the vineyards** are a few pennies short of a keeper and two hands each. The cheapest lever is
  the crews' own food: they eat what the Country grows, which costs 4.6p a day at what the shops pay the Country for
  it, not 6.8p at the shops' prices. That gives the grain farms 20p and the vineyards 13p, and closes both. A toll-free
  staple (no gate toll on flour and oats, 80p a day of the treasury's 1,595p) would pay a fourth grain farm instead.
- **The salt works** can pay one wolf each: a keeper who rakes his own pans. The crews' food lever (+9p) nearly pays a
  hand at one of them; a 13% dearer salt (2p to about 2.3p) pays both.
- **The market gardens can't pay anyone**: the city draws 5 vegetables a day, all for the inns' stew. Nobody eats
  vegetables as they are (they have no nourishment in items.json, and the market stalls' supply lists hold them with
  nothing to sell them to; doc 68, 2). Until vegetables are a food (a pottage, or vegetables with nourishment at the
  stalls), the two gardens are kept by the cattle and dairy farms' households without a wage. Their places by the
  gates are reserved.
- **Lean seasons and stores**: the orchards are lean in summer and autumn (apples and grain are cheapest at harvest,
  doc 68's season prices) and need a reserve of about 26 days of their wages, 10,000p for all five, which they make
  back in winter and spring. The vineyards' autumn (the city draws almost no wine in autumn) needs 60 days of theirs,
  5,000p. The pig farms, the sea boats and the beekeepers dip for a season by 2-6 days. Everyone else pays every season.
  So **each farm keeps a till with a reserve** (section 4): the orchards' and the vineyards' sized as above, the rest a
  week. The grain farms carry their lean season in the granary, not the till: they sell the harvest from store all year
  at the season's price.

## Headline numbers

- **What the Country holds**: 16 industries; the city draws 2,505 pieces a day for 5,024p, of which 1,595p is the gate
  toll and 3,429p the Country's (section 1). **Food is 63% of it**: 3,185p a day paid by the shops, 2,176p the
  Country's after the toll. **The food pays the treasury 1,010p of its 1,595p toll.**
- **The food can pay about 108 wolves at the city's drifted wages** (keeper 15.7p, hand 10.4p, plus each one's food at
  city prices and upkeep). Recommended: **34 farms, 97 crew** (above). With the crews eating at the Country's own prices
  (4.6p against 6.8p), about 12 more.
- **Funding is adequate for food as a whole** (2,151p against 1,947p of crews, 204p over, about 10%), but not farm by
  farm: grain, the vineyards and salt are a few pennies short of a sensible crew, the gardens can't pay anyone. Pigs,
  orchards, cattle and fish carry the area. The fixes are in the table: the crews' food at the Country's prices
  (closes grain and the vines), salt a little dearer, vegetables made a food.
- **Seasonal labour**: the money pays year-round crews; the work isn't year-round. Sized from real pre-industrial work
  (the cross-check, 2.5), the food needs about 103 workers in winter and 163 in autumn: the harvest wants about
  two-thirds more hands than the 97 crew for 60-90 days, spring (lambing, sowing) about a third more: some 11,000 hand-days a year, about 330 a
  farm. Waged at 10.4p that would be about 325p a day, more than the farms' 204p over, so **the extra hands must be
  mostly the crews' own households, fed by the farm and not waged** (as farm families were; DEV's pool has 52 "keeps
  the house" of working age), with a few day labourers from the Sump paid from the reserve. In the simulation nothing
  peaks: a farm yields its season's pieces from its store or herd (section 4).
- **The NPCs**: DEV has 524 unplaced, **401 of working age** (16-59; 93 under 16, 30 over 60; 268 female, 256 male).
  The food farms want 97 and the other industries about 51 (what they can pay; section 5): **148 of the 401**, every
  old trade covered but orchards and vines (1 short, from the households). After twelve years of arrivals (about 460
  left) there is still room.
- **The Country after the switch** (section 2.6): once the farms take the food, the Country keeps 1,253p a day for its
  91 posts (728p) and its basket, which falls from 2,527p to about 525p. The farms' 1,947p of wages, food and upkeep
  comes back to the city instead, but as households' spending (food, needs, comforts), not as the basket's cart wheels,
  iron and stone. **The trades the basket keeps busy (the smithies, cartwrights, mason, brickworks, foundry, the
  cider and salt-fish makers) lose about 2,000p a day of orders** unless the farms buy their tools, carts, barrels and
  building goods from them (section 4, "the farms' basket").

## 1. What the Country holds now

### 1a. By industry (a day; settled years 4-12)

Pieces / pennies the shops pay (the cost of the draw, the season's price and the store's markup included: making
100%, resale 75%), by season; the year's mean; the gate toll; carriage in (the shop pays the carters on top); the
Country's intake (after the toll); the swing of the season's pennies against the year's mean.

| Industry | Spring | Summer | Autumn | Winter | Year | Gate toll | Carriage in | The Country's intake | Season swing |
|---|---|---|---|---|---|---|---|---|---|
| livestock: meat | 347 / 1,024p | 331 / 1,020p | 327 / 992p | 346 / 1,053p | 338 / 1,022p | 325p | 3.6p | 697p | 97-103% |
| orchards | 532 / 753p | 519 / 529p | 521 / 492p | 497 / 806p | 518 / 645p | 204p | 11.7p | 440p | 76-125% |
| fishing | 312 / 570p | 265 / 573p | 275 / 584p | 369 / 648p | 305 / 593p | 188p | 2.2p | 405p | 96-109% |
| country workshops' goods | 134 / 508p | 133 / 502p | 163 / 635p | 130 / 486p | 140 / 533p | 169p | 1.6p | 364p | 91-119% |
| woods: timber, firewood, bark, charcoal | 420 / 518p | 403 / 479p | 430 / 512p | 549 / 618p | 450 / 532p | 169p | 47.6p | 363p | 90-116% |
| mining and metals | 71 / 429p | 59 / 355p | 63 / 384p | 71 / 427p | 66 / 398p | 127p | 2.8p | 271p | 89-108% |
| grain and milling | 130 / 287p | 151 / 232p | 156 / 238p | 124 / 276p | 140 / 258p | 81p | 4.0p | 177p | 90-111% |
| fibre: hemp, flax, linen | 112 / 214p | 99 / 187p | 106 / 202p | 112 / 209p | 107 / 203p | 65p | 1.1p | 137p | 92-105% |
| bees | 55 / 203p | 61 / 184p | 60 / 181p | 48 / 179p | 56 / 187p | 60p | 0.5p | 127p | 96-109% |
| vineyards | 36 / 226p | 40 / 204p | 21 / 42p | 30 / 197p | 32 / 168p | 53p | 0.7p | 114p | 25-135% |
| stone, lime, clay, sand | 140 / 168p | 124 / 148p | 127 / 154p | 147 / 179p | 135 / 162p | 52p | 17.9p | 110p | 91-110% |
| livestock: wool, hides, tallow, bone | 85 / 116p | 83 / 112p | 90 / 120p | 81 / 114p | 84 / 116p | 36p | 0.8p | 80p | 97-104% |
| salt | 59 / 119p | 52 / 103p | 52 / 104p | 53 / 106p | 54 / 108p | 34p | 0.7p | 74p | 96-110% |
| dairy | 58 / 89p | 75 / 75p | 78 / 78p | 56 / 87p | 67 / 82p | 26p | 1.0p | 56p | 91-109% |
| herbs | 7 / 9p | 7 / 9p | 8 / 11p | 9 / 11p | 8 / 10p | 3p | 0.0p | 7p | 86-113% |
| vegetables | 6 / 11p | 7 / 7p | 4 / 4p | 4 / 8p | 5 / 7p | 2p | 0.0p | 5p | 56-144% |
| **all** | **2,505 / 5,244p** | **2,409 / 4,719p** | **2,482 / 4,732p** | **2,626 / 5,404p** | **2,505 / 5,024p** | **1,595p** | **96p** | **3,429p** | |

(The toll and intake columns are at the settled rate; the report's 1a prints the same totals.) The long run's own a
day: drawn 5,024p, toll 1,595p, carriage 128p (with carriage out), the Country spent 2,527p.

- **The pennies follow the season prices more than the pieces do**: grain, fruit and vegetables are cheapest at harvest
  (summer and autumn, 85-95%) and dearest in winter and spring (110-140%), so the orchards take 806p in winter for
  fewer apples than the 492p of autumn. Meat (no season) and fish (more in winter, cheaper then) are nearly flat.
- **Where the season caps bind** (the settled years): meat draws 317-330 a day against a cap of 330 all year, so the
  city would take more meat than the Country sends; fish binds in spring (309 of 309), grain in winter and spring
  (121 of 126, 126 of 131), honey and fruit in winter. The farms' yields above carry 10% slack, so they can send a
  little more than the caps allow today: raise meat's `normal` with them.
- **Wine in autumn**: the city draws one wine a day in autumn against 18-20 in the other seasons (the vintage: its
  makers make it then). That is the vineyards' lean season.

### 1b. The food goods, and why they are drawn

| Good | Catalogue | Spring | Summer | Autumn | Winter | Paid a day (year) | Drawn for |
|---|---|---|---|---|---|---|---|
| apples | 1p | 460 | 504 | 502 | 376 | 510p | cider 81%, dried apples 13%, preserves 7% |
| raw meat | 3p | 147 | 138 | 125 | 131 | 378p | jerky 37%, resale 34%, salt pork 27% |
| pig | 12p | 18 | 21 | 22 | 22 | 248p | the butchers' pigs |
| mutton | 6p | 33 | 37 | 33 | 37 | 208p | smoked mutton |
| rabbit | 1p | 136 | 127 | 138 | 144 | 136p | smoked rabbit |
| meal (the Country's cookshops' stew) | 6p | 13 | 8 | 8 | 12 | 53p | resale |
| fish | 2p | 187 | 174 | 174 | 221 | 406p | salt fish 57%, smoked fish 43% |
| shellfish | 2p | 124 | 91 | 102 | 148 | 187p | resale |
| honey | 3p | 54 | 61 | 60 | 47 | 185p | honey cakes 63%, preserves 30%, cordial 6% |
| flour | 2p | 73 | 81 | 81 | 68 | 180p | honey cakes 55%, bread 45% |
| oats | 1p | 54 | 69 | 72 | 54 | 76p | porridge |
| wine | 12p | 20 | 18 | 1 | 18 | 131p | resale 91% |
| salt | 2p | 59 | 52 | 52 | 53 | 108p | salt fish 43%, jerky 32%, salt pork 25% |
| milk | 1p | 58 | 75 | 78 | 56 | 82p | porridge |
| tallow | 1p | 54 | 53 | 59 | 52 | 55p | candles 90% |
| dried apples, preserves | 3p, 4p | 29, 12 | 0, 1 | 0, 3 | 47, 46 | 42p, 45p | resale (the Country's winter stores) |
| cider | 3p | 22 | 7 | 9 | 19 | 38p | resale 65%, apple brandy 35% |
| grapes | 2p | 15 | 22 | 20 | 13 | 37p | wine |
| hide, wool, bones | 5p, 2p, 1p | 6, 9, 14 | 5, 9, 14 | 5, 9, 14 | 6, 8, 13 | 28p, 18p, 14p | leather; scarves; charms |
| berries, vegetables | 1p | 10, 6 | 7, 7 | 7, 4 | 9, 4 | 10p, 7p | cordial; stew |

### 1c. What the Country buys back

It spends 2,527p a day on the city's goods (2,579p as the short runs share it out, 28p of carriage out), and 728p on
its 91 posts (the wharf's carriers and the Houses' people, 8p each). Of the goods, **610p is food and drink made from
its own produce**: cider 312p (104 a day), salt fish 113p, smoked fish 89p, wine 35p, apple brandy 26p, berry cordial
24p, brandy 12p. The rest of the top of the basket: cart wheels 213p, paper 84p, steel bars 82p, iron bars 78p,
leather 74p, cut stone 69p, charcoal 68p, mortar 61p, planks 59p, barrels 58p, bricks 57p, lime 56p.

So the buy-back is the Country's spending, not a producer's income: adding it to the farms' revenue would count the
same pennies twice. What does go to the producers through it is already in their draws: **the apples behind the cider
the Country buys back are 178p a day of the orchards' revenue** (39% of the apples drawn), the fish behind its salt and
smoked fish 77p of the fish's, the honey, salt and tallow behind its cordial and goods 38p. When the Country stops
buying cider back (it becomes the farms), the orchards lose that 178p unless someone else drinks it: section 2.6.

## 2. Food, in depth

### 2.1 What the residents eat, and where it comes from

What the residents buy to eat (pieces a day, the short runs scaled to the settled years; what is bought is eaten in a
steady run):

| Food | Nourish | Price | Spring | Summer | Autumn | Winter | Of the nourishment | Made from |
|---|---|---|---|---|---|---|---|---|
| porridge | 20 | 1p | 232 | 265 | 298 | 242 | 18% | 1/4 oats + 1/4 milk (the inns) |
| bread | 15 | 1p | 235 | 261 | 277 | 228 | 13% | 1/6 flour (+ firewood); flour is wheat at the mill, 1 to 1 |
| raw meat | 30 | 3p | 106 | 106 | 104 | 112 | 11% | the Country's beef and pork, resold |
| meal (stew) | 50 | 6p | 62 | 58 | 56 | 63 | 10% | 1 raw meat + 1 vegetables |
| smoked rabbit | 18 | 2p | 136 | 130 | 131 | 144 | 8% | 1 rabbit (+ firewood) |
| salt pork | 35 | 6p | 61 | 57 | 55 | 56 | 7% | 1 raw meat + 1/3 salt |
| honey cake | 15 | 4p | 109 | 116 | 124 | 111 | 6% | 1/3 flour + 1/3 honey |
| jerky | 30 | 6p | 50 | 48 | 49 | 49 | 5% | 1 raw meat + 1/2 salt |
| shellfish | 10 | 2p | 125 | 99 | 103 | 131 | 4% | resold |
| salt fish | 18 | 4p | 68 | 64 | 61 | 69 | 4% | 1 fish + 1/3 salt |
| smoked fish | 20 | 5p | 51 | 57 | 49 | 56 | 4% | 1 fish (+ firewood) |
| smoked mutton | 32 | 8p | 34 | 39 | 33 | 38 | 4% | 1 mutton (+ firewood) |
| dried apples | 10 | 3p | 71 | 78 | 73 | 80 | 3% | 1.5 apples (+ firewood) |
| fruit preserves | 12 | 4p | 45 | 56 | 53 | 75 | 2% | 1 apple + 1/2 honey |
| cider | 6 | 3p | 101 | 108 | 127 | 106 | 2% | 2 apples |
| wine | 4 | 12p | 30 | 31 | 11 | 27 | 0% | 2 grapes (and a cask's share) |

A resident buys about 49-50 nourishment a day (hunger grows about 50 a day; doc 68 says it needs about 46) for 6.8p
of food and about 1p of drink. Meat in all its forms is about 45% of what the city eats, grain (porridge, bread, honey
cakes) 37%, fish 12%, fruit and cider 7%. (Meal is bought faster than the inns make it in a 14-day run, from the shelves' first
stock; in a long run what is bought is what is drawn and made.)

**The food goods by use** (pieces a day, the year's mean):

| Drawn good | For the residents' food | Made into what the Country buys back | All, by season (Sp/Su/Au/Wi) |
|---|---|---|---|
| apples | 280 | 180 (cider) | 460 / 504 / 502 / 376 |
| fish | 136 | 53 (salt and smoked fish) | 187 / 174 / 174 / 221 |
| rabbit | 136 | | 136 / 127 / 138 / 144 |
| raw meat | 135 | | 147 / 138 / 125 / 131 |
| shellfish | 116 | | 124 / 91 / 102 / 148 |
| flour (wheat) | 76 | | 73 / 81 / 81 / 68 |
| milk | 67 | | 58 / 75 / 78 / 56 |
| oats | 62 | | 54 / 69 / 72 / 54 |
| honey | 51 | 3 (cordial) | 54 / 61 / 60 / 47 |
| salt | 47 | 7 | 59 / 52 / 52 / 53 |
| mutton, pig | 35, 21 | | 33-37, 18-22 |
| dried apples, preserves, cider, wine, meal | 19, 16, 5, 12, 11 | cider 10, wine 3 | (the Country's own stores and presses) |
| grapes, berries, vegetables | 16, 0, 5 | grapes 2, berries 8 | 13-22, 7-10, 4-7 |

### 2.2 Revenue by area and season

Pennies a day, settled years. "The shops pay" includes the toll; revenue is the Country's part less carriage in.

| Area | Pieces a day | The shops pay | Gate toll | Carriage in | Revenue Spring | Summer | Autumn | Winter | **Year** | of it, behind the Country's buy-back |
|---|---|---|---|---|---|---|---|---|---|---|
| grain and flour | 140 | 258p | 81p | 4.0p | 191p | 155p | 159p | 187p | **173p** | - |
| orchards and vines (fresh fruit, cider, wine) | 515 | 725p | 230p | 12.2p | 588p | 484p | 346p | 514p | **483p** | 178p |
| dried and preserved fruit | 34 | 87p | 28p | 0.2p | 70p | 3p | 6p | 159p | **59p** | - |
| meat and livestock | 338 | 1,022p | 325p | 3.6p | 693p | 695p | 672p | 713p | **693p** | - |
| the herds' by-products (tallow, hides, bone, wool) | 84 | 116p | 36p | 0.8p | 80p | 76p | 82p | 78p | **79p** | 21p |
| fish and shellfish | 305 | 593p | 188p | 2.2p | 387p | 391p | 397p | 439p | **403p** | 77p |
| milk and dairy | 67 | 82p | 26p | 1.0p | 60p | 50p | 52p | 57p | **55p** | - |
| honey | 56 | 187p | 60p | 0.5p | 138p | 124p | 123p | 120p | **126p** | 7p |
| vegetables | 5 | 7p | 2p | 0.0p | 5p | 6p | 3p | 7p | **5p** | - |
| salt | 54 | 108p | 34p | 0.7p | 83p | 71p | 69p | 72p | **74p** | 10p |

- **Grain is cheap for what it feeds**: a measure of wheat is a flour is six loaves, 90 nourishment, two days of a
  wolf's food, and the Country gets about 1.3p for it after the toll. The bakers and inns keep most of bread's and
  porridge's value. That is why grain, the biggest share of what the city eats, pays the fewest wolves.
- **Meat and fruit pay best**: a pig at 12p, mutton at 6p and raw meat at 3p carry the livestock; apples at 1p are
  cheap a piece but the city takes 500 a day, mostly for cider.

### 2.3 How many farms, and their crews

The farm counts follow the user's ranges (never one source; several of each) and then the money: each kind's count is
the most farms whose revenue pays a keeper and a sensible crew each.

- **Grain, 3 farms, two with a water mill.** Alone, two mills take 62p (half of flour's 2p) and can't pay a miller and
  a hand each, while three farms and their mills together pay 1 + 1 each, nearly 1 + 2 (the levers). One mill would be
  a single source; a mill on each of two farms on different rivers isn't.
- **Orchards, 5, two with a drying house.** The dried apples and preserves (59p) can't pay a house of their own: they
  are made in autumn from the orchards' own apples and sold mostly in winter (159p) and spring, when fresh fruit is
  short. As part of the orchards they fill the orchards' lean months. Each orchard: 1 + 3.
- **Vineyards, 2**: 1 + 1, nearly 1 + 2 (the levers); one on the SE desert's warm edge (sun, dry: the desert's best use
  for food, with the warren, the bees and the salt pan), one on the central hills' sunniest slope.
- **Livestock, 10 holdings in four kinds**: 3 cattle and dairy farms (the dairies are the cattle farms: their calves are
  the beef, and milk must be near the city, so all three are on the NW coast's pasture within two cells), 3 pig farms
  (woodland pannage), 2 sheep and goat runs (the goats in the N/NE foothills' run), 2 rabbit warrens. The raw meat is
  split 65% to the cattle and 35% to the pigs, so each kind pays a crew of 1 + 3 or 1 + 4. The pig farms' pigs at 12p
  would otherwise make them the richest farms in the realm.
- **Fish, 7 crews**: 4 sea boats (1 + 2: the Sound's two at the wharf, a north cove, the SW marsh's coast), 2 lake and
  river boats (1 + 1), 1 eel and fowl marsh (1 + 1). The fish is shared 45% to the sea boats (they also have all the
  shellfish), 35% to the lake and river boats, 20% to the marsh, so each kind pays its crew.
- **Honey, 3 beekeepers** (1 + 1: a beekeeper and a hand keep about 150 skeps each, if anyone asks), two among the
  orchards, one in the SE desert's thyme scrub.
- **Salt, 2 works**: a keeper each (the levers for a hand). The SW marsh's coast boils brine over peat and wood (wet
  climate), and is a middle distance from the city; the SE desert's pan is raked in summer and is the realm's far end,
  so its carriage is dearest. Salt is light (54 pieces a day, about 110 weight), so it bears the distance.
- **Vegetables, 2 market gardens** outside the gates, kept by the dairy farms' households until vegetables are a food.

### 2.4 Yields per farm

The yields are not real-world yields (the user: "We don't need to stay realistic to actual yields"). Each farm's yield
of a good is its share of the city's draws of that good, plus its share of what the 97 crew eat (the residents' diet,
in the same goods), plus 10% slack, by season (pieces a day, a farm; the city's draws alone in brackets):

| Farm | Good | Yield a farm a day, Sp / Su / Au / Wi (the city's draws alone) |
|---|---|---|
| grain farm (3) | flour (wheat ground at the mill) | 31.0 / 34.5 / 34.7 / 28.8 (24.2 / 26.9 / 27.2 / 22.5) |
| | oats | 23.1 / 29.2 / 30.7 / 22.9 (18.1 / 22.8 / 24.0 / 17.9) |
| | straw | about 1 |
| orchard (5) | apples | 110 / 122 / 122 / 90 (92 / 101 / 100 / 75) |
| | cider, dried apples, preserves (two of the five) | cider 5 / 2 / 2 / 4; dried 7 / 0 / 0 / 12; preserves 3 / 0 / 1 / 12 |
| | berries (hedges) | about 2 |
| vineyard (2) | grapes | 9.7 / 14.1 / 12.4 / 8.1 (7.7 / 11.2 / 9.9 / 6.4) |
| | wine | 12.8 / 10.9 / 0.4 / 11.0 (10.2 / 8.8 / 0.4 / 8.8) |
| cattle and dairy farm (3) | raw meat | 40.9 / 38.1 / 34.7 / 36.2 (32.0 / 29.8 / 27.1 / 28.3) |
| | milk | 21.1 / 27.2 / 28.2 / 20.3 (16.5 / 21.3 / 22.1 / 15.9) |
| | meal; tallow; hides; bones | 2-4; about 10; about 1.3; about 2.5 |
| pig farm (3) | raw meat | 22.0 / 20.5 / 18.7 / 19.5 (17.2 / 16.1 / 14.6 / 15.3) |
| | pigs | 7.7 / 8.8 / 9.4 / 9.3 (6.1 / 6.9 / 7.3 / 7.2) |
| | meal; tallow; hides; bones | 1-2; about 6; about 0.8; about 1.5 |
| sheep and goat run (2) | mutton | 20.8 / 23.4 / 20.8 / 23.5 (16.3 / 18.3 / 16.3 / 18.3) |
| | milk (goats); wool; tallow; bones | 5.6 / 7.2 / 7.5 / 5.4; about 5; about 6; about 1.5 |
| rabbit warren (2) | rabbits | 87.0 / 81.5 / 88.4 / 92.2 (68.0 / 63.7 / 69.1 / 72.1); pelts about 1 |
| sea fishing boat (4) | fish | 25.9 / 24.2 / 24.0 / 30.5 (21.1 / 19.6 / 19.5 / 24.9) |
| | shellfish | 39.7 / 29.0 / 32.6 / 47.3 (31.0 / 22.7 / 25.5 / 37.0) |
| lake and river boat (2) | fish | 40.3 / 37.7 / 37.3 / 47.4 (32.8 / 30.5 / 30.4 / 38.7) |
| eel and fowl marsh (1) | fish (eels) | 46.0 / 43.0 / 42.6 / 54.2 (37.5 / 34.9 / 34.7 / 44.2) |
| beekeeper (3) | honey | 22.7 / 25.7 / 25.2 / 19.7 (17.9 / 20.3 / 19.9 / 15.6) |
| salt works (2) | salt | 37.4 / 32.4 / 32.6 / 33.2 (29.7 / 25.8 / 26.0 / 26.4) |
| market garden (2) | vegetables | 4.1 / 4.4 / 2.7 / 2.8 |

- **The seasons' shape is the city's**: each yield follows what the city drew in that season, which follows the season
  groups (more grain in summer and autumn, more fish in winter, fruit short in winter, meat flat). So supply meets demand
  season by season with 10% over, and prices stay where the long run settled them.
- **Stores, not fields**: for grain, apples, grapes, honey and salt the yield is what the farm sells from its store in
  that season, not what it reaps that day. The harvest fills the store once (wheat in late summer, oats, apples and
  grapes in autumn, honey summer to autumn, salt in summer) and the season's yield is drawn from it. So a farm's store
  must hold about a year's sales on the day the harvest is in: for a grain farm about 11,700 flour (as wheat) and 9,500
  oats. Livestock, fish and milk yield as they go.
- **A worker a day**: from 6 (pig farms: a pig is a big piece) to 44 (warrens). These are the numbers the game's own
  producers (`crafts.json` `producers`) would need if real NPCs worked the farms; most of today's producer rates are
  lower (the real-yield cross-check below: orchards 14 apples a worker-day against 128 here).

### 2.5 The cross-check: what real yields and labour would say

The tool's `--real-check` sizes the same food from pre-industrial yields (the first draft of this doc; ASSUMPTIONS and
UNITS in the tool: a tile about a metre, a game day's food about 2,500 kcal, a measure of wheat 1.5 kg; a three-field
grain farm at 1 t a hectare with a fifth kept for seed; orchards at 6 t a hectare; 50 worker-days a sown hectare, and
so on). It is kept for two things the money doesn't say:

- **Land**: the city's food and the Country's own people's (about 490 of them with their families) want about 220 ha of
  arable, 35 of orchard, 13 of vines, 45 of hay meadow and about 1,600 ha of grazing, heath and woodland pannage. Drawn at
  a tenth of its area (each tile of field stands for 10 m^2), that is about 390,000 tiles of the farms' own (six cells of
  256 x 256) and 1.6 million of shared range. **The footprints in the placement list are smaller, chosen to look right
  rather than to be real**: 218,000 tiles of the farms' own (about 3.3 cells) and 330,000 of shared range (about 5).
- **Seasonal labour**: real work needs about 103 workers in winter, 132 in spring, 147 in summer and 163 in autumn for
  the same food (the harvest and the vintage in autumn, lambing and sowing in spring); about 116 year-round and 75 more
  at the peak. The money-first crews (97) are about the real year-round number. The peak is the households' and the
  Sump's (section 3).

### 2.6 The Country's money once the farms take the food

| | Pennies a day |
|---|---|
| The Country's intake now (all its goods, after the toll) | 3,429p |
| of it, the food areas' (to the farms; they pay the carriage in, 25p, out of it) | 2,176p |
| The Country keeps (its other goods) | 1,253p |
| ... for its 91 posts in the city (8p) | 728p |
| ... and its basket of the city's goods (2,527p now) | 525p |
| The farms' crews' wages, spent in the city | 1,189p |
| their food (at city prices) | 661p |
| the farms' upkeep (city goods: tools, nails, planks) | 97p |
| The farms keep | +204p |
| The gate toll the food pays the treasury (of its whole 1,595p) | 1,010p |

The money is the same; where it goes changes. Today the Country turns the food's 2,176p into orders for the makers who
have time to spare (doc 68, 2: the smithies, the foundry, the cartwrights, the mason, the brickworks, the paper, the
cider and salt-fish makers). With real farms the same pennies go to the farms' 97 wolves, who spend them as households
do (food, needs, comforts). Two consequences, and the code must handle both (section 4):

1. **The basket's trades lose their buyer** unless the farms buy what farms buy: cart wheels and axles, farm gear,
   sickles, barrels and hoops (for cider, wine, salt pork), rope and sacks, nails, planks, lime and stone for their
   buildings. A farm's upkeep should be a basket of city goods sized like the Country's (a few pennies a worker-day,
   about 1,000p a day for all), so the city's makers keep their orders.
2. **The cider and salt fish the Country buys back** (425p a day) stop being bought unless the farms' people drink and
   eat them. The crews' food at city prices (661p) is the place: if the farms' people buy their food in the city, the
   cider makers and fishmongers keep part of their trade.

The treasury keeps its toll as long as the farms' goods still pass the gates as store draws do (they should: the farms
sell to the city's shops, 1,010p a day of toll on food). A toll-free staple is a lever for grain only (80p).

## 3. Labour and money (item 10)

**Can year-round crews do the work?** In the simulation, yes: a farm yields its season's pieces from its store or its
herd, and its crew is paid every day (as a till's posts are), so nothing peaks. In the world, with real NPCs working
the fields, the work peaks: the cross-check's autumn needs about two-thirds more hands than the crews for the harvest, the
vintage and the pigs' killing, spring about a third more for lambing, sowing and calving, for 60-90 days each
(about 11,000 hand-days a year, 330 a farm). Year-round
crews alone would make the harvest late, not lost, since the stores carry the year. Where the extra hands come from:

1. **The crews' own households**: a farm's keeper and hands have families; DEV's unplaced pool has 52 of working age
   who "keep the house". At the harvest they work for their board (the yields already feed them).
2. **The city's lower class**: the Sump's labourers and the wharf's carriers, hired by the day at the hand's wage from
   the farm's reserve. All 330 hand-days a farm waged would cost about 325p a day for all the farms, more than their
   204p over, so the day labourers are the few, the households the many.
3. **The unplaced NPCs** still left after the crews (below), as seasonal hands lodging at the farms.

**The NPCs** (DEV's 524 unplaced, read only; working age 16-59):

| Old trade | Unplaced | Working age | The food farms' crews | The other industries (what they can pay) | Spare (+) or short (-) |
|---|---|---|---|---|---|
| fields and mills | 60 | 60 | 8 (with the 2 gardeners) | 6 (fibre) | +46 |
| orchards and vines | 25 | 23 | 24 | | -1 |
| herds and flocks | 61 | 61 | 39 | | +22 |
| bees | 6 | 6 | 6 | | 0 |
| fishing and the marsh | 27 | 26 | 20 (with the salt works) | | +6 |
| woods | 36 | 36 | | 11 | +25 |
| mines, stone and clay | 28 | 28 | | 16 | +12 |
| keeps the house | 52 | 52 | (the harvest hands) | | +52 |
| the towns' trades and shops | 41 | 37 | | 18 (the workshops) | +19 |
| the watch | 12 | 12 | | | +12 |
| service and labour | 39 | 37 | | | +37 |
| the Heights | 17 | 16 | | | +16 |
| children | 97 | 4 | | | |
| the old | 23 | 3 | | | |
| **all** | **524** | **401** | **97** | **51** | **+253** |

(Ages 6 to 80; 93 under 16, 30 aged 60 or over; 268 female, 256 male; ids `cty_` 200 (the Country's own), `tn_` 262
(the old map's towns), `rm_` 62.) The pool covers every crew by its own old trade but orchards and vines (1 short:
from the households or service). The arrivals of doc 68's rule (an empty city post brings in an unplaced NPC of its
class, about five a year) draw on the same pool: 64 in twelve years, so after twelve years about 460 would be left,
still about 300 more than the farms and works need.

**Is the funding adequate?** For food as a whole, yes: 2,151p a day against 1,947p for 97 wolves, about 10% over. The
cash flow through the year:

- **Sowing before income** is not a problem once the farms start from the Country's stores. The Country's store room
  hands its stocks to the farms (the granary full at the switch, the herds standing), so the farms sell from their
  first day. A grain farm started from bare fields in Spring would pay its crew for about 165 days before its first
  wheat: 7,000-10,000p a farm, the three together half the realm's money (50,252p). So the farms shouldn't start that way.
- **Through the year**, the reserves above: the orchards about 26 days of wages (2,000p each), the vineyards 60 days
  (2,500p each), the rest a week or less. They come from the farms' own good seasons (the orchards' winter, the
  vineyards' spring), so the first year's reserve is the Country's to lend (its purse) or the treasury's (doc 68's
  surplus).
- **Lean farm by farm**: grain, the vineyards and salt are a few pennies short of a sensible crew; the gardens can't pay
  anyone. The levers, cheapest first:
  1. **The crews eat at the Country's own prices** (4.6p a day, not 6.8p): +2.2p a wolf, about 210p a day for all.
     It closes the grain farms and the vineyards. It is what farms do: they eat what they grow.
  2. **Salt a little dearer** (2p to about 2.3p, 13%) for a hand at each works, or a single keeper each as now.
  3. **Vegetables made a food** (nourishment, sold at the stalls), or the gardens kept unwaged by the dairy households.
  4. **No toll on flour and oats** (80p a day of the treasury's toll) pays a fourth grain farm, if more grain farms are
     wanted on the map.
  5. **The farms' own by-products** (tallow, hides, bone, wool, pelts: 79p) are already in the livestock's revenue; a
     tannery or wool shed in the Country would add value but takes it from the city's tannery and weavers.
  6. Not recommended: **fewer, bigger farms** (it pays, but the user wants several of each); **the Country's
     buy-back** as farm income (it's the Country's spending, not income: 1c).

**The recommended configuration** is the table at the top: 34 farms, 97 crew, with lever 1 (the crews eat at the
Country's prices) so the grain farms and vineyards take 1 + 2, and the salt works a keeper each until salt is dearer.
That is 3 grain farms (two with mills), 5 orchards (two with drying houses), 2 vineyards, 3 cattle and dairy farms,
3 pig farms, 2 sheep and goat runs, 2 rabbit warrens, 4 sea boats, 2 lake and river boats, an eel and fowl marsh,
3 beekeepers, 2 salt works and 2 market gardens. Then **102 wolves** with lever 1 (grain 9, vines 6), or 97 without.

## 4. What the code needs (an outline; another step builds it)

The user: "We aren't going to place any NPC's or farms or other things from Country yet", and "make sure that yield is
adjustable in Atlas' economy controls".

**Phase A: the farms as settings.** `Data/Economy/country_farms.json` (written by the tool, this doc's numbers) becomes
the defaults; each world keeps its own in `live.economy` (a migration: `farms jsonb` and `farm` manifest lines, as
doc 68's seasons did), read by Core into `EconomySpec::farms`:

- **Per area**: its goods, `yieldScale` (percent on all its farms), on or off (off: the Country still supplies it).
- **Per farm**: its id, kind, area and place (unset until the map places it); its **crew** (keepers, hands); its
  **wages** (keeper's and hand's; null: the world's, the city's drifting wages); its **yield** (pieces a day of each good
  for Spring, Summer, Autumn and Winter, blended between the seasons' middle days exactly as the season groups are,
  `Society::seasonal`); its **store** (the most it holds, and the day the harvest fills it, for the harvested goods);
  its reserve target.
- **Atlas's Economy page**: a Farms panel beside the Seasons panel: each area with its farms, their crews, wages and
  yields (four numbers a good), `yieldScale`, and a "from country_farms.json" reset, as the seasons have. **The DM's
  Economy tab** shows the same, editable on the DM's world, applied live by `economy.sync` (doc 68, "Seasons and the DM's
  rules"); both audited. The DM sees each farm's till, stock, last week's sales and wages paid.
- **Tests**: the yields blend by day; a farm's yield scales with its area; a farm off returns its goods to the Country.

**Phase B: modelled producers with virtual crews (the simulation).** Each farm runs without NPCs:

- **A till** (`till:farm:<id>`, a facility like the carters'), its crew as **virtual posts** (a keeper and N hands paid
  at 18:00 from the till as a shop's posts are, wages drifting by the same rule, but paid to the farm's own account,
  since nobody holds them yet). Their wages are spent as a household's: food at the city's shops (the crews' diet,
  lever 1 off) or from the farms (lever 1 on), needs and comforts by doc 68's rules. Money is conserved.
- **Production**: at the day's turn each farm adds its yield to its stock (`create`, "grown at <farm>"), or, for a
  harvested good, its store is filled on its harvest day and the season's yield moves from store to stock. Stock over
  its cap is wasted (`consume`, "spoiled"), so oversupply shows.
- **Selling**: the store draw (`drawFromStore`) buys a good first from the farms that hold it (spread by stock, as the
  Country's purchases are), at the store's price and markup and the season's factor, the toll and carriage as now; the
  Country only where no farm has it ("Country when short", the per-good switch doc 68, 2 designed and left for "the
  first farm"). Each area's goods go into `country_off` once all its farms are on.
- **The farms' basket**: each farm buys its upkeep from the city's makers (tools, cart wheels, barrels, rope, nails,
  planks, building goods), a few pennies a worker-day, so the basket's trades keep their orders (2.6).
- **The Country** keeps its purse, its posts, its other goods and its buy-back basket of what's left; a farm short of
  wages draws on its reserve, then the treasury's wage support (doc 68's rule, as for a till).
- **Reports**: the economy line gains per farm and area its stock, sales, wages owed and paid, reserve and spoilage;
  `tools/econ_detail.py` gains a farms section.
- **3-year runs** (each needs the user's go-ahead, as all simulations): the farms on area by area, then all; the gates
  of doc 68, 10 plus: every farm pays its crew 99% of days, no area's stock runs out more than a day a season, spoilage
  under 10%, the city's prices within the long run's band, and the basket's trades' covers unchanged.

**Phase C: real NPCs replace the virtual crews.** When the map has the farms' places:

- A farm's posts become real posts at its work area (`live.npcs` placed from the unplaced pool by their old trade, as
  the arrivals are, doc 68, "Arrivals from the Country"), with homes at the steading. Each post filled removes one
  virtual post; the wages are the same.
- The yield stays the setting (the simulation's number), but a farm with fewer crew than its setting yields in
  proportion (a farm short of hands yields less), and later the producers' work spells (`crafts.json` `producers`,
  retuned to section 2.4's "a worker a day") make the goods the NPCs are seen working for.
- Seasonal hands: at the harvest the farm hires the households' unwaged and the city's day labourers (the Sump's), for
  the days of its peak, from its reserve.

## 5. The other industries (more briefly)

What they can pay at the same costs a wolf, against the workers they'd need at the game's own rates (`crafts.json`'s
producers and the city's crafts' hours) and at plausible ones:

| Industry | Drawn a day | Revenue a day | Wolves it can pay | Workers at the game's rates | Plausible | Units (where) |
|---|---|---|---|---|---|---|
| woods: timber, firewood, bark, charcoal | firewood 339, timber 60, planks 26, charcoal 13, bark 11 | 315p | 14 | 65 | 11 | 4 logging camps (the Rainwood's cedar slopes on the NW coast, the central hills' oak woods); 4 coppices with woodcutters' huts (near the villages and the city: 340-510 firewood a day); 3 charcoal hearths (central hills, near the bloomeries); 1 sawmill (on a river below the camps) |
| mining and metals | iron bars 38, iron ore 17, copper and tin ore 5 each | 268p | 13 | 40 | 40 | 2 iron mines (the N/NE mountains); 2 bloomeries (beside them); 1 copper and tin pit (the N/NE foothills) |
| stone, lime, clay, sand | clay 55, limestone 29, stone 26, sand 23 | 93p | 4 | 13 | 13 | 2 quarries (the central hills' limestone; the Rainward Hills by the city); 2 clay pits and sand banks (the plains' rivers; the SW coast) |
| fibre: hemp, flax, linen | hemp 41, cord 30, linen 24, flax 12 | 136p | 7 | 39 | 19 | 3 hemp and flax fields (a fourth field on the grain farms; retting ponds by the rivers); spun and woven at home in the winter lull |
| herbs | 8 | 7p | 0 | 1 | 1 | 1 herb gatherers' cottage (the SW marsh's edge) |
| country workshops' goods | clay pots 37, wool scarves 31, soap 27, pamphlets 26, hoops 11 | 362p | 18 | 31 | 31 | 1 village pottery; 3 weavers' and soap boilers' cottages (the farm villages); 1 country press and paper mill (on a river near the city) |

- **The woods pay** (firewood is heavy and cheap but the city burns 340-510 a day): 11-14 wolves.
- **Mining can't pay its workers**: 38 iron bars a day at 9p bring 268p, enough for 13 wolves, where smelting them from
  ore with charcoal takes about 40 (the game's rates and real bloomery work agree). Either the Country sends ore and the
  city's ironworks and foundry make the bars (they have time to spare: doc 68, 2), so the mines need about 13, or iron
  is dearer. **Recommended: the Country mines ore, the city smelts.**
- **Stone, clay and fibre** pay about a third of their plausible crews at today's prices; the fibre is the grain
  farms' winter work (no extra hands), the quarries a keeper or two each.
- **The workshops' goods** (pots, scarves, soap, pamphlets) are the old map's towns' trades, not farms; they wait for
  the towns.
- The desert, beyond food: salt (above), and later glass sand and perhaps a caravan route; no timber, little grazing
  (goats at its edge).

## 6. The placement list for the map

`Data/Economy/country_farms.json`: per food area, each farm with its id, kind, region, distance from the city, terrain,
what it wants near (a road, a river, the sea, the city), its footprint (`own_tiles`: fields, orchard, steading, pans;
`shared_range_tiles`: grazing or woodland its herds share with the wild), its crew, wages, yields by season and store;
and the other industries' works with their counts, regions and footprints. In short:

| Kind | Count | Region (distance from Ridgemere) | Terrain, near | Own tiles each | Shared range each |
|---|---|---|---|---|---|
| grain farm (2 with a water mill) | 3 | central plains, on the rivers (middle, 3-8 cells) | plain, meadow; road, river | 29,400 (fields 180 x 150, steading 60 x 40) | - |
| orchard (2 with a drying house) | 5 | central hills' south slopes (3, middle); the NW coast's sheltered valleys (2, near) | hills, meadow; road | 6,900 | - |
| vineyard | 2 | the SE desert's warm edge (far); the central hills' sunniest slope (middle) | hills; road | 4,100 | - |
| cattle and dairy farm | 3 | the NW coast's wet pasture by the city (2), the foot of the Rainward Hills (1) (near, within 2 cells) | meadow, pasture; the city, road | 14,000 | 40,000 |
| pig farm | 3 | the central hills' oak and beech woods (2, middle); the Rainwood's edge (1, near) | forest, hills; road | 1,200 | 22,500 |
| sheep and goat run | 2 | the central hills' downs; the N/NE foothills (middle) | hills, downs, mountain; road | 750 | 62,500 |
| rabbit warren | 2 | the SW marsh's dunes (middle); the SE desert's sandy edge (far) | heath, dunes, desert; road | 12,000 | - |
| sea fishing boat | 4 | Ridgemere Sound at the wharf (2), a north cove (near); the SW marsh's coast (middle) | coast; sea | 450 (a landing) | - |
| lake and river boat | 2 | Glasswater under the N/NE mountains; the Long River (middle) | lake, river | 240 (a jetty) | - |
| eel and fowl marsh | 1 | the SW marsh (middle) | marsh; water, road | 375 | 15,000 |
| beekeeper | 3 | among the central hills' orchards (2, middle); the SE desert's thyme scrub (far) | hills, meadow, scrub; an orchard | 1,200 | - |
| salt works | 2 | the SW marsh's coast (middle); an SE desert pan (far) | coast, marsh, desert; sea, road | 2,400 | - |
| market garden | 2 | outside the city's gates (near) | meadow; the city, road | 2,400 | - |
| logging camp, coppice, charcoal hearth, sawmill | 4, 4, 3, 1 | the Rainwood (NW coast), the central hills' woods, beside the villages | forest; road, river | 1,200, 300, 900, 600 | 40,000, 10,000, -, - |
| iron mine, bloomery, copper and tin pit | 2, 2, 1 | the N/NE mountains and foothills (middle) | mountain; road | 1,200, 600, 600 | - |
| quarry, clay pit and sand bank | 2, 2 | the central hills, the Rainward Hills (near); the plains' rivers, the SW coast | hills, river, coast | 4,800, 2,400 | - |
| hemp and flax fields, herb cottage, pottery, weavers' cottages, press | 3, 1, 1, 3, 1 | the grain farms, the SW marsh's edge, the villages, near the city | | 4,800, 150, 500, 300, 600 | -, 22,500 |

All the food farms' own ground is about 218,000 tiles (3.3 cells of 256 x 256) and their shared range about 330,000
(5 cells). Distances: near is within 2 cells of Ridgemere, middle 3-8, far 9 or more (the SE desert). Carriage today is
a flat 6p a wagon load whatever the distance; if the realm's size makes the desert's salt and wine visibly far, a rate
by distance would cost them more, and both are light enough to bear it.

## Open questions for the user

1. **The crews' food at the Country's prices** (lever 1): the farms' people eat what they grow, at what the shops pay
   the Country for it (4.6p a day) rather than the shops' prices (6.8p). It gives grain and the vines their second hand.
   Recommended.
2. **The basket's trades**: when the farms take the food, should their upkeep be a basket of the city's goods (tools,
   carts, barrels, building goods), so the smithies and cartwrights keep the Country's orders? Recommended; otherwise
   about 2,000p a day of the makers' orders turns into household spending.
3. **Vegetables**: make them a food (with nourishment, at the stalls), or leave the gardens unwaged until then?
4. **Iron**: the Country mines ore and the city smelts it (recommended), or dearer iron?
5. **The map's scale for fields**: the footprints are chosen to look right (3.3 cells of farms' own ground, 5 of shared
   range), not real (real would be about 20 cells of farmland and grazing). Keep them so?
6. **More grain farms** on the map than the money pays (a fourth or fifth) would need the toll off flour and oats (80p a
   day of the treasury's toll) or dearer grain.
