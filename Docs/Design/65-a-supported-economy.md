# 65. A supported economy: as many shops, farms and mines as the residents can keep (2026-10-30)

> **Superseded by doc 79 (2026-11-08) except where doc 79 says it stays** (Docs/Design/79-a-staged-economy.md,
> section 21). The text below is kept as history.

The user, 2026-10-30: "I'm wondering if we should redesign shops/farms/smiths/etc. ... look at our residents, determine how many shops they need to be supported across the capital and outlying towns, and figure out what the best approach is to this economy. Do a deep dive on what we have, what we can support, and even what we might be missing so that we can set ourselves up to have a more realistic and stable economy from the start."

**Words used here:**
- **Shops** are everything that sells things: stalls, retail, inns, makers such as smiths and bakers, and services.
- **Farms** are everything that grows food, fishers included.
- **Mining** is everything that mines or gathers resources: mines, quarries, clay, salt, logging, charcoal, reeds, herbs, hunting and gathering.

**What matters (the user):**
- Hunger is rare.
- The poor can house and feed their families.
- Shops, farms and mines stay open.
- Inequality is reported but isn't a gate.
- A business may still idle or fail when nobody wants what it sells.
- Money is conserved, and nothing is handed out from nowhere.

Builds on docs 42, 59, 61, 63 and 64.

**Sources:**
- The exported world, `artifacts/economy/export/world.ratw`.
- The catalogue in `Data/Items`.
- Four runs: `2026-10-30-r16-6y-land` (six years, the main source; round 18's analysis in doc 64 used the same run), `r15-2y-fast`, `r15-2y-normal` and `r18-6y-fast` (read to day 1,337).
- Year 0 is days 0–363 and year 1 is days 364–727. "Years 1–5" means days 364–2,189.
- Scripts: `/tmp/claude-1000/-home-martinb-Documents-GitHub-MUDProject/da6a1181-05f7-491a-a334-c3e7343d5540/scratchpad/supported/` (`inventory.py`, `perf2.py`–`perf7.py`, `capacity.py`). The editor's re-checks are in its `verify/` folder.

## 1. Summary

**What we found:**
- **Too many businesses.** There are 384 businesses for 1,083 residents: 152 shops, 169 farms and 63 mining. That is one for every 2.8 residents. Their posts take 61% of the working-age wolves. In the six towns they take 60–84% (77% across the six).
- **The spending can't keep them all.** At today's prices, the money left after goods bought, rent and hands' wages would pay about **258 keepers**, if it were shared evenly within each trade in each town at that trade's own keeper's wage. (At each category's mean wage it is 278.) One trade's surplus can't pay another trade's keeper, so capped at the businesses each trade has, it pays about **165**. On average 244 are open in years 1–5. From the end of year 0 onward, 36–42% are stopped on any given day. Normal mode is no better than `--fast`.
- **Most stops are the same businesses stopping again.** 347 tills stopped at least once. The other 2,479 of the 2,826 stops came after a return.
  - **54% had too few buyers:** sales in the month before the stop were under the keeper's wage. Loggers lead (223), then grain fields (179, mostly at the look after the harvest), stalls, mines, swineherds, orchards, pastures and smithies.
  - **20% had no margin:** resellers and food makers such as general stores, smokehouses, stalls, provisioners and inns.
  - **14% couldn't pay the hands they hire:** rabbit farms, butchers, boats and loggers.
  - **12% were House tills** that their House didn't top up in full.
- **The return rule churns.** The stop rule itself causes few stops: 106 (4%) come from counting open posts. The return rule brings businesses back on gross sales they can't profit from. There were 2,665 returns, and 846 of the 1,200 shop returns stopped again within 35 days.
- **Grain runs on its starting stock.** The capital grows no grain.
  - In year 1 the realm grew 19k wheat against 43.7k milled, and 10.6k oats against 51k used.
  - The oats were gone by the end of year 2 (71k → 4k). Half the wheat was gone by the end of year 4 (148k → 72k).
  - In year 5 the fields grew 77k wheat and 37k oats. The cause was not checked.
- **Ridgemere's food shops thin out.** Its shops with food on their shelves fell from 36.7 in year 0 to 17.4 in year 4.
  - In year 5's famine, up to 500 wolves were hungry, and only 4–8 of those shops had food.
  - The realm still held 55k wheat at the time.
- **Missing:** home rent, light (doc 59 isn't built), copper and tin, hunting as a trade, and outside trade. Clothes wear and mending is built, but it brings only 88p a day. Makers never hear when a household goes without.

**What we recommend:**
1. Mend the stop and return rules.
2. Build a capacity model that can be re-run, and re-author the founding to it (approach a).
3. Let small trades work part days (approach c).
4. Let the count follow demand: dormant premises that an owner opens, and idle ones that close (approach b).
5. Add realistic needs (approach d) one at a time, measuring each.

## 2. What we have

| | Shops | Farms | Mining | All |
|---|---|---|---|---|
| Ridgemere city | 106 | 9 | 10 | 125 |
| Six towns | 36 | 19 | 8 | 63 |
| Country | 10 | 141 | 45 | 196 |
| **All** | **152** | **169** | **63** | **384** |

By community, each community's country included. Food shops are the trades on the `foodShop` list.

| Community | Residents | Businesses | Shops | Farms | Mining | Food shops | Residents a shop | Residents a food shop |
|---|---|---|---|---|---|---|---|---|
| Ridgemere | 683 | 187 | 109 | 49 | 29 | 53 | 6.3 | 12.9 |
| Oakhaven | 72 | 37 | 6 | 17 | 14 | 3 | 12.0 | 24.0 |
| Coldwater | 59 | 26 | 8 | 18 | 0 | 4 | 7.4 | 14.8 |
| Larkfield | 77 | 39 | 8 | 31 | 0 | 4 | 9.6 | 19.2 |
| Frosthollow | 56 | 21 | 6 | 0 | 15 | 3 | 9.3 | 18.7 |
| Mirebridge | 58 | 32 | 8 | 19 | 5 | 3 | 7.2 | 19.3 |
| Southmeadow | 78 | 42 | 7 | 35 | 0 | 5 | 11.1 | 15.6 |
| **All** | **1,083** | **384** | **152** | **169** | **63** | **75** | **7.1** | **14.4** |

**Posts against the working-age.** Working age is 16 up to each wolf's own retiring age. That gives 832 wolves: 1,083 less 175 under 16 and 76 elders.

| | Working age | Keepers | Help | Left over |
|---|---|---|---|---|
| Ridgemere | 538 | 178 | 103 | 257 (62 House staff and servants, 15 watch, 5 church, 19 town posts, 53 street posts, 103 household or for hire) |
| Six towns | 294 | 195 | 32 | 67 (7–17 a town for the watch, the Town Works and hires) |

- **Harvest wants more hands than exist.**
  - At harvest (day 250) the fields want 1,631 hands, and the orchards and vineyards 304. This is tills.csv `wanted`: the hours of work their sales want, ÷ 8.
  - Only 324 working-age wolves hold no business post.
  - That day the fields posted 113 hire slots and 11 were taken. The orchards and vineyards posted 67 and 7 were taken.
  - 35 of the 76 elders hold paid posts (11 keepers, 22 help, 2 House). Each needs a successor before it can retire.

**How the founding was laid out:**
- **The towns are alike.** Every town gets six shops whatever its size (`tools/worldgen/towns.py`). Four are the same basics from doc 39: a general store, a smithy, an inn, and a bakery or provisioner. The other two are workshops of the town's own trade.
- **The capital has no fields.** Ridgemere's community has 63% of the people and no fields, orchards or mill. Frosthollow has no farm at all.
- **Owners.** 34 shops belong to the Houses, all of them in Ridgemere. The other 350 businesses have lone keepers. 78 producers on a House's land pay it 15% rent.
- **Odd posts.** There are 24 market stalls. 54 street posts (porters, washers, lamplighters) are given to the first stall on each market cell as its help, and the stalls let them go within a month. Six shops have no craft in the catalogue.
- **Nobody ever has copper or tin.** No copper pit, tin stream or peat cutting exists, so the bronze runs down from 82 bars to 16 by day 364, and to none by day 728.

## 3. What the residents need

Spending a day, from flows.csv:

| Buyer | Year 1 | Year 4 |
|---|---|---|
| Residents: food | 22,363 | 29,604 |
| Residents: household needs (firewood, candles, pots, nails, cider…) | 3,456 | 3,847 |
| Residents: wants (treats, drink, pastimes, things for the home, soap) | 7,183 | 5,377 |
| Residents: works on the home, help about the home | 2,865 | 3,734 |
| Residents: children's trinkets | 1,153 | 847 |
| Residents: clothes mended or replaced | 88 | 78 |
| Town, church, watch, docks, feasts, alms | 4,803 | 5,511 |
| Houses' works; businesses' tools and upkeep | 2,271 | 2,449 |
| *Between businesses: materials, farm to shop, orders, caravans* | 41,455 | 48,717 |

- **Food is 60% of residents' spending in year 1 and 68% in year 4.** A wolf eats about 2.6 units a day: 46 nourishment, against the 50 it aims for.
- **Food costs more of a day's wage each year.** It cost 0.33 of a day's wage in year 1 and 0.40 in year 4. Doc 63 aimed at about 0.25.
- **Fish is the staple.** Fresh, smoked and salt fish are 49% of what's eaten in year 1 and 61% in year 4. Bread is 18%. Porridge fell from 16% to 4.5% as its oats ran out.
- **Grain is drawn from stock that isn't replaced.**
  - **Wheat:** 19k grown in year 1 against 43.7k milled. The stock fell from 148k to 72k by the end of year 4.
  - **Oats:** 10.6k grown against 51k used. The stock fell from 71k to 4k by the end of year 2.
  - **Year 5:** 77k wheat and 37k oats were grown.
  - **Bread as the staple:** at six loaves to a wheat, it would take about 200–220k wheat a year (estimate).
- **A quarter of what's eaten also spoils.** On top of what's eaten, 26% as much nourishment spoils in year 1 (20% in year 4), mostly fish and porridge.

By town, year 1, counted by the buyer's home. "Shops with food" is days.jsonl `food_shops`: the shops holding anything edible, as a daily mean.

| Town | Residents | Food units a day | Food p a day | Goods, wants, children p a day | Shops with food | Food bought in other towns |
|---|---|---|---|---|---|---|
| Ridgemere | 669 | 1,773 | 14,544 | 8,971 | 28.7 | 0.2% |
| Larkfield | 99 | 223 | 2,235 | 573 | 3.5 | 17% |
| Southmeadow | 82 | 193 | 1,095 | 698 | 3.8 | 9% |
| Oakhaven | 72 | 194 | 1,567 | 333 | 1.3 | 65% (91% in year 4) |
| Coldwater | 59 | 163 | 953 | 373 | 4.9 | 1% |
| Mirebridge | 58 | 161 | 912 | 296 | 3.9 | 1% |
| Frosthollow | 56 | 146 | 1,036 | 533 | 0.9 | 79% (95% in year 4) |

Household needs actually bought, against the schedule for about 334 homes. The needs are bought once per home, whatever its size. The schedule is an estimate.

| Need | Bought, year 1 | Bought, year 4 |
|---|---|---|
| Firewood | 79% | 93% |
| Nails | 79% | 78% |
| Bandages | 63% | 56% |
| Clay pots | 61% | 56% |
| Cider | 53% | 35% |
| Tallow candles | 18% | 24% |
| Cookpots | 8% | 9% |

- **Candles are the biggest gap.** Only 11.5k were made in year 1 (17–34k a year later), and nearly every one sold. The church's basket is 2 a day per 100 residents, about 7.9k a year. It got 76 in year 1 and about 600 a year in years 2–4.
- **Makers never hear of these gaps.** A household that finds nothing "goes without", and no missed sale is counted (`RatwDemand.cpp:1646`).

## 4. What we can support

### The capacity model

To stay open, a business needs its **viable minimum**:
- **At least**, its keeper's wage *K* (52–90p a day, median 60p) out of its net.
- **At best**, its full staffing: its keeper, its help and up to two open posts at the town's wages.

For each trade *t* in town *c*, over a year:

    Net(t,c)     = sales − goods bought − rent − hands' wages            (a day, the year's mean)
    Carried(t,c) = Net / K̄                     keepers it pays with today's hands kept (K̄ = that trade's mean keeper's wage)
    Most(t,c)    = (Net + hands' wages) / K̄    one-keeper businesses its work would fill, with no hands

A trade is over-built where Carried is under the businesses it has.

The same thing from the catalogue, without a run, with one rule per kind of business:

- **A reseller** (stalls, general stores, provisioners) can add at most the ladder's 15% to a good it sells on (ChainPricedOnce). So it keeps sales × .15/1.15 = 13% of what it sells.
  - A 60p keeper therefore needs **about 460p a day of goods sold on**. At year 1's average of 8.8p, that is about 52 food units.
  - That is about what today's average food shop sells (54 units, 476p a day in year 1). So a reseller of average size only breaks even, and a smaller one loses money (estimate; spoilage and markdowns not counted).
- **A maker** is paid for its own costed hours at the going rate × (1 + margin). It needs buyers for about **one keeper's day (8 hours) of its own work** each day.
- **A producer** needs buyers for **a keeper's day of its yield** every day of the year. For example:

  | Producer | A keeper's day of yield |
  |---|---|
  | Boat | 16.5 fish |
  | Dairy herd | 14.2 milk |
  | Logging camp | 5.9 timber and 11.8 firewood |
  | Mine | 3.8 ore and 3.8 lime |
  | Orchard | 14.2 apples, but only in two seasons |

- **The count for a town** is then:

      Count(t,c) = Σ goods demand(g,c) × own hours(g,t) ÷ 8 ÷ posts per business

  The demand comes from the residents and the baskets (`tools/supply_balance.py` already reckons it in units). Phase 1 adds the hours.

### Today, by category and town

Each cell shows *exists / open on average / carried*, from the six-year run, years 1–5. Carried is summed trade by trade, each trade at its own keepers' wage.

| Town | Shops | Farms | Mining |
|---|---|---|---|
| Ridgemere | 109 / 51 / 79 | 49 / 23 / 57 (the boats 43) | 29 / 23 / 26 |
| Oakhaven | 6 / 4 / 5.1 | 17 / 11 / 3.6 | 14 / 9 / 9.3 |
| Coldwater | 8 / 6 / 3.4 | 18 / 13 / 10.0 | – |
| Larkfield | 8 / 7 / 8.9 | 31 / 28 / 16.1 | – |
| Frosthollow | 6 / 5 / 3.4 | – | 15 / 8 / 6.8 |
| Mirebridge | 8 / 5 / 2.8 | 19 / 14 / 10.4 | 5 / 2 / 1.9 |
| Southmeadow | 7 / 6 / 4.2 | 35 / 27 / 9.3 | – |
| **All** | **152 / 84 / 107** | **169 / 117 / 107** | **63 / 43 / 44** |

- **Totals across all 384:** 244 open on average and 258 carried. At each category's mean keeper's wage instead, 278 are carried.
- **The r18 candidate** (days 364–1,337) has 253 open and carries 229 (239 at category level).
- **Shops fully staffed** (keeper, help and two open posts, at the shops' mean staffing) would carry 124 across the realm.

### By trade

Realm-wide, years 1–5:

| Trade | Exists | Open | Carried | Most | Reading |
|---|---|---|---|---|---|
| Boats | 22 | 19.8 | 59.1 | 150 | Ridgemere's surplus is mostly margin: .62 in year 1, .72–.81 in years 2–5 (Coldwater's and Mirebridge's .1–.4). 650 (year 1) to 1,080 (year 4) fish a day are turned away |
| Fishmongers | 4 | 3.9 | 14.5 | 19.7 | Room for more |
| Dairies (cheese) and dairy herds | 12 | 10.3 | 21.4 | 41 | Room for more |
| Inns | 18 | 16.3 | 21.4 | 28.1 | About right |
| Bakeries | 8 | 7.9 | 10.4 | 25.0 | About right, room in Ridgemere |
| Building and wood makers (11 kinds, builders' yard and charcoal kilns included) | 19 | 12.0 | 19.4 | 46.6 | Right in total; cooperage, cartwright and paint have no buyers |
| Logging, woodcutting | 31 | 20.9 | 21.8 | 42.2 | About a third too many |
| Salt, quarries, clay, charcoal | 17 | 13.0 | 15.3 | 26.2 | About right |
| Iron mines | 12 | 8.8 | 7.1 | 14.4 | A few too many |
| Rabbit farms | 16 | 6.0 | 6.7 | 29.7 | Plenty of work, but their hands' wages eat it |
| Swine, breeding, pasture | 22 | 12.2 | 11.9 | 20.8 | About half too many |
| Orchards | 20 | 19.0 | 6.2 | 12.3 | Owners keep them open |
| Metal shops (smithies, foundries, ironworks, armory, tinker, jeweller) | 16 | 10.0 | 8.7 | 30.3 | The towns' six smithies carry 0.6 between them |
| General stores, provisioners | 14 | 10.4 | 6.2 | 9.1 | Resale at 13% |
| Smokehouses, butchers, breweries, mills, press | 16 | 8.5 | 4.4 | 11.3 | Squeezed by fish bought at the boats' margin |
| Grain fields | 52 | 44.9 | 1.6 | 13.6 | Probably undercut by the starting stock (see below) |
| Market stalls | 24 | 3.9 | 0.4 | 0.5 | Can't carry anyone |
| Sheep farms | 15 | 0.4 | 0.4 | 1.2 | Wool and hides have no buyers |
| Cloth and leather (weaver, tailor, saddler, tannery, ropewalk, dyeworks) | 10 | 0.9 | 2.5 | 4.1 | No clothes or harness demand |
| No buyers at all (cartographer, fletcher, arsenal, moneychangers, herbalists, papermill, saltworks, reeds, herbs) | 12 | 0.3 | 0.1 | 0.2 | Dead end |
| Others (bees 6, vines 3, eel traps 3, chandlers 2, apothecaries 2, scriptorium, printing house, perfumer, pottery, stables 2, two works with no business type) | 24 | 14.4 | 18.6 | 45.7 | Mixed: eel traps and chandlers have room; bees and the perfumer don't |
| **All** | **384** | **243.8** | **258.1** | **572.5** | |

**Reading it:**
- **The money is in the wrong trades.** The realm's spending would keep more businesses than are open today, about 258 against 244, but not 384.
  - The surplus sits in fish, dairy and inns.
  - The shortfall is in stalls, grain, sheep, the towns' smithies and general stores, and the small trades that barely sell. 49 tills were stopped at least 94% of years 1–5: 12 sheep farms, 7 stalls, and 30 tills in 22 small trades.
- **Several surpluses are margin, not demand.**
  - If Ridgemere's boats' margin came down to round 18's held top of .405, they would carry an estimated 10–16 keepers rather than 43 (for today's margins of .81–.72).
  - The r18 candidate, which holds that top, measured 16 (days 364–1,337).
  - The difference wouldn't vanish: residents would pay less for fish and spend the money elsewhere. Total spending stays the same; only where it lands changes.
- **Grain is a broken chain, not too many fields.**
  - The 52 fields, worked every day in season, would grow about 47k wheat and 23k oats a year, plus about 8k of each at the winter yield (estimate). That covers about year 1's wheat and under half its oats.
  - They grew 19k and 10.6k in year 1, while the founding's 148k wheat and 71k oats were being sold down. That the stock undercut them is likely but not tested. In year 5 they grew 77k wheat and 37k oats.
  - A field's batch is 13 units of nine goods: wheat, oats, vegetables, straw, flax, hemp, apples, firewood and hay. The straw, flax, hemp, hay and vegetables mostly pile up. The batch sells at about 1.8p a unit (the analysts' figure, not re-checked).
- **Many small businesses or a few big ones.** The same work can keep 258 keepers with today's hands, or about 573 one-keeper businesses ("Most").
  - Fewer businesses with many hired hands send the margin to few owners. In years 0–4 of this same run, Ridgemere's boats added 1.84M to property income (doc 64, round 18).
  - More, smaller businesses spread that margin over more homes.

## 5. Why businesses stop today

All 2,826 stops in the six-year run, each judged on its 28 days before. 347 tills stopped at least once. The other 2,479 stops came after a return.

| Cause | All | Shops | Farms | Mining |
|---|---|---|---|---|
| A. Sales under the keeper's wage (median 11p a day): **too few buyers** | 1,537 (54%) | 341 | 703 | 493 |
| B. Sales over the wage, but goods bought and rent leave less (96% bought in): **no margin** | 558 (20%) | 482 | 39 | 37 |
| C. Nets the wage before its hands are paid, not after | 394 (14%) | 111 | 188 | 95 |
| H. A House's till its House didn't top up in full | 330 (12%) | 330 | – | – |
| Nets wage and hands, but stopped anyway | 7 | 6 | 1 | 0 |

**Who they are:**
- **A:** loggers 223, grain fields 179, stalls 116, mines 95, swineherds 94, orchards 84, pastures 83, smithies 81.
- **B:** general stores 134, smokehouses 90, stalls 58, provisioners 53, inns 41.
- **C:** rabbit farms 105, butchers 60, boats 41, loggers 40.
- **H:** smithies 101 and glassworks 56.

The rules' own flaws:

- **Open posts count against shops.** 106 shop stops (4%) held a week of the keeper's wages but not a week of the open posts that staffing counts.
- **Tills start below their own check.** Every producer's till starts with 270p, under its 420p check. So do 46 of the 152 shop tills.
  - 40 tills (10%) were stopped by day 28, short at each of their first four looks: 19 shops (6 of them inns) and 21 producers, 9 of the producers orchards.
  - How many the start money alone stopped is not measured. Most of them ran their tills to near nothing within days.
- **Orchards are judged weekly in their first season.** A farm counts as seasonal only after a week of sales, so all 20 orchards stopped in days 23–32. The fields are judged after their harvest, as intended.
- **The return test counts the wrong sales.** It counts gross sales at the shop's own prices, plus buyers turned away for lack of stock, against its hands' wages. It never subtracts what the goods cost (`Society::ownerReturns`).
  - There were 2,665 returns, and 2,479 of them stopped again. Shops stopped again after a median 28 days, the earliest the rule allows.
  - 846 of the 1,200 shop returns (71%) stopped again within 35 days.
  - Idle farms and mines mostly come back because idling refilled the till, not because buyers came back. For the 155 farms that did pass the sales test, a median 97% of the counted "sales" were buyers turned away.
- **The first shop in the list wins.** Household goods, tools, farm upkeep, the watch and the docks take the first shop in the town's list that has the good. That shop's share of those channels runs from .77 to .99.

**The cost:**
- Ridgemere's shops with food on their shelves fell from 36.7 in year 0 to 17.4 in year 4.
- Year 5's famine ran from day 2,029 to day 2,044, peaking on day 2,035 at 500 hungry and 448 starving. During it, only 4–8 of those shops had food, and their shelves fell to 253 units (day 2,038) against about 1,770 eaten a day.
- Ridgemere's median resident still held 360–470p, about two weeks of food (estimate).
- The realm held 55k wheat and 3.2k flour, untouched through the famine's fortnight.
- The buyers were there; the shops had shut.

## 6. What we might be missing

| Need or chain | Today | Status |
|---|---|---|
| **Home rent** | Homes are free; nobody is homeless; the only rent is businesses' 15% to the Houses | Planned (doc 59 Part 3), not built |
| **Clothes** | The per-person need is off with work by the hours. Clothes now wear and are mended or replaced instead (doc 64 Phase 9), 88p a day for the realm | Built, nearly nil |
| **Light** | Candles every two days, 18–24% met | Old rule built; doc 59 Parts 1 and 5 (lamps, oil, lamplighters) not built |
| **Fuel** | Firewood 79–93% met; peat has no site | Built |
| **Home upkeep** | Works on the home, 1.9–2.3k a day, from the want fund (wear-driven upkeep is doc 59 Part 2, not built) | Partly built |
| **Help and services** | Help about the home, 1.0–1.4k a day. Healers and a schoolteacher are written as posts, not as businesses a household pays; there is no barber | Partly built |
| **Medicine** | 17 of 18 medicines never made; bandages only | Planned (the ailments follow-up) |
| **Transport wear** | Caravans wear no carts, harness or saddles; the cartwright and saddlers sell nothing | Absent |
| **Grain** | The capital grows none; in year 1 the realm grew under half its use; stock carried it, with the oats gone by the end of year 2 | Broken chain |
| **Copper and tin** | No sites; bronze is gone in two years | Broken chain |
| **Wool and hides** | 3,684 hides made a year, 21 sold; sheep farms 97% stopped | Dead end |
| **Hunting and gathering** | Outwork only (2 wolves trapping on day 364); no posts | Absent as trades |
| **Luxuries** | 34 of 36 jewellery pieces never made; wants take the cheapest piece; soap and scent gone by year 4 | Built, thin |
| **Household shortfall signal** | "Goes without", no missed sale | Absent |
| **Outside trade** | The realm is closed; caravans run town to town. Doc 61's export channel went with the orchestrator's channels (doc 64 Phase 12) | Absent |
| **Money sources and sinks** | Conserved: 5,000,000p from day 2. Newcomers' purses add about 6k over six years (5,005,999 on day 2,190). Tax, tithe, holdings tax and rent are transfers | By design |
| **Population** | Births and newcomers (doc 64 Phase 8); players will add demand | Built |

- **Where a real household's other money would go.** A pre-industrial household spends most of its money on food, then on rent, fuel, light and clothes (a rough rule, not a measurement). Here only fuel is a real expense.
- **Why that hurts.** Rent, clothes and light are the needs that would keep weavers, tailors, tanners, chandlers and sheep farms open, and they are the ones missing or nearly nil.

## 7. Approaches, compared

**(a) Size the world's businesses to its demand.** Re-author how many of each trade each town has, from the capacity model, at the founding.
- **Fixes:** cause A at the start, the dead trades, the towns' identical basics, and the capital with no grain. Fewer, better-placed businesses free working-age wolves for hires and harvests.
- **Cost:** moderate. The capacity tool, then a worldgen pass and a DEV rebuild (a test world, so no data migration).
- **Risks:** a fixed count fits one level of demand. When prices, needs or population move, the count is wrong again, so it needs re-running whenever the world changes.

**(b) Demand-led founding and closing.** A business opens where demand outruns supply and an owner has the money. It closes when it has idled long enough.
- **Fixes:** the count follows demand by itself (BazaarBot's turnover, doc 63). Opening is paid from a resident's or a House's savings to existing makers (tools, stock), so it draws down hoards while conserving money.
- **Cost:** high. Premises must exist in the world, since a smithy room needs a forge. It needs a rule for who founds (savings, skill, a House), and a keeper who changes trade.
- **Risks:** boom and bust: too many open at once, then too many fail. It needs a slow rate and a margin test, not gross sales.

**(c) Businesses that scale down.** A keeper works the hours its sales pay and takes odd jobs for the rest of the day. A stall opens on market days.
- **Fixes:** the small towns' single smithy, general store and inn, which can't carry a full day but are needed. Fewer stops for thin demand. A basic stays open at, say, a third of a day.
- **Cost:** low to moderate. Work by the hours already reckons a business's hours from its sales (`hoursBySales`); the keeper's wage and the stop check would scale by them.
- **Risks:** keepers earn less, so the poor share may rise where shops are thin. The keeper must find other work, and the towns have only 7–17 spare wolves each.

**(d) More demand.** New needs (clothes by wear, light, rent), outside trade, more people.
- **Fixes:** the dead ends, and a realistic budget.
- **Cost:** varies. Needs are mostly catalogue and demand rules, and doc 59 is designed. Outside trade needs a money source and sink at the border.
- **Risks:**
  - Money is fixed, so new needs redirect spending rather than add to it. Each new necessity falls hardest on the poor (the poorer half held 4–12% of the money).
  - Outside trade breaks "no money from nowhere" unless it is balanced, goods out for coins in.

**(e) Mend the signals and rules first.**
- **What:**
  - Return on margin, not gross sales.
  - Seasonal from the founding.
  - Start money above the check.
  - Open posts not counted at the stop.
  - Household shortfalls heard as missed sales.
  - First-found channels moved onto the food channel's price tiers and rotating start.
- **Fixes:**
  - The false stops: 106 from open posts, the 20 orchards in their first season, and some of the 40 stopped by day 28. That is about 5% of stops (estimate).
  - The churn behind most of the rest: 2,479 of the 2,826 stops came after a return.
- **Cost:** low. Rules only.
- **Risks:** low. These rules only take out what doesn't belong. A business with no buyers still idles.

**(f) A better combination: premises by demand, keepers by the market.**
- **What:** author each town's premises from the capacity model with some headroom. The headroom premises are dormant, with no keeper. Then run (b) only on those: an owner opens a dormant one where the model and missed sales show unmet demand, and a business idle for a season closes and becomes dormant.
- **Why it's better:** it keeps (a)'s good start and (b)'s self-correction, without building or deleting rooms mid-game.

## 8. Recommendation and phased plan

We recommend **(e), then (a), (c), (f), with (d) one need at a time**. Every phase is measured on the user's goals:

- **Hunger rare.**
  - Days over 1% hungry (24 in the six-year run).
  - The peak hungry.
  - Anyone starving.
- **The poor can house and feed their families.**
  - `model.need` by town: the share of homes under 7 days of food money.
  - Food's share of the median wage.
  - Rent's share, once there is rent.
- **Shops, farms and mines stay open.**
  - The stopped share by category and town after the first quarter.
  - Returns that stop again within 35 days.
  - Shops with food in each town.
- **Inequality, reported:** the Gini and the poorer half's share.

Runs: 2 years normal, 6 years `--fast`, and the from3 variant.

Suggested bars for the user to set (estimates):
- No day over 2% hungry, and no wolf starving.
- `need` at or under .10 in every town.
- Under 15% of businesses stopped after the first quarter.
- Under a quarter of returns stopping again within 35 days.

| Phase | What | Measure besides the goals |
|---|---|---|
| **0. The rules** | (e): return on net margin after goods and hands; seasonal from the founding; start money at least one check; open posts left out of the stop; household shortfalls counted as missed sales; first-found channels rotated | Stops 2,826 → ?; re-stops after a return 2,479 → ?; shop returns stopping again within 35 days 846 of 1,200 → ?; the causes table again |
| **1. The capacity tool** | `tools/supply_balance.py --capacity`: Count(t,c) from the catalogue and the residents, and Carried from a run's CSVs, printed side by side | Its prediction against a run's Carried, by trade and town |
| **2. The founding re-sized** | (a) as a worldgen pass and DEV rebuild: trades per town by Count; grain fields sized to a year's use, growing grain rather than a nine-good batch; no founding stock bigger than a season; copper and tin sites, or no bronze; stalls cut to what resale carries; dead trades removed or given buyers; town basics sized to each town | Carried ≥ exists in every trade; grain stock steady year on year; fish's margin |
| **3. Part-time trades** | (c): keeper paid by hours sold, the rest odd jobs; stalls on market days | The towns' basics open; keepers' earnings and `need` in the towns |
| **4. Dormant premises** | (f): headroom premises; an owner opens one on margin and missed sales; a season idle closes it | Openings and closings a year (slow, no waves); property income's share |
| **5. Needs** | (d), each alone: clothes by wear; light (doc 59 Part 1); home rent (doc 59 Part 3) | Spending's shape (food under 60%); the poor's share after rent |

## 9. Questions for the user

1. **The staple:** fish or bread? Today fish is 49–61% of the diet. Making bread the staple needs about 200–220k wheat a year. That is about four times what the 52 fields could grow worked every day, and ten times what they grew in year 1 (estimate).
2. **Headroom:** size the founding exactly to demand, or leave dormant premises (say 10–20%) for owners to open?
3. **Dead trades** (cartographer, fletcher, dyeworks, saddlers, tailors, sheep): remove them, or give them buyers (clothes by wear, harness on the caravans, arrows for hunters)? Some may be there for players.
4. **Small towns:** must every town keep a smithy, a general store and an inn, even part-time, or may a town buy those goods from its neighbour?
5. **Big or small:** fewer businesses with many hands, or many small ones? Small spreads the margin over more homes.
6. **Home rent:** now (doc 59 Part 3) or after the founding is re-sized? Paid to the Houses, or to the towns?
7. **Copper and tin:** add the sites, or retire bronze?
8. **Outside trade:** keep the realm closed, or add a balanced border trade (goods out, coins in)?
9. **Re-authoring:** may Phase 2 rebuild DEV's businesses? The PROD push stays yours.
