# 68. The Country and the city's loop (2026-10-31)

The user, 2026-10-31 (verbatim):

- On the store room (doc 67, 6): "my idea is to use it in place of having farmers and mines and whatnot ahead of us
  actually implementing them." They asked how the store's money gets back into circulation.
- On the proposal below: "Yeah I like that idea for The County. Do that". (The Country.)
- "One more thing we need to make sure we are factoring into item costs from the country and sales TO the country is
  caravan/wagon deliveries. Those wolves need to be paid too."
- "I think it may be possible for the city to supplement the carters yard using taxes, too, just as a lever. The city's
  taxes should be used to supplement areas that help businesses I think. Services and whatnot."
- Also approved: the restocking rule is on (doc 67 Q2), and the smallest loop that makes an economy test mean anything:
  residents earn wages, buy food and household goods, shops restock from the Country, and the Country buys the city's
  goods back.
- A stable economy: hunger is rare; the poor can house and feed their families; shops, smiths and works stay open.
  Inequality (Gini) is measured over earning households only; above .4 is acceptable if the rest holds.

The proposal the user approved, "the Country":

1. Shops buy from the Country: what a keeper draws from the store room is paid to the Country's purse, as it would be
   to real farmers and miners. The money is conserved and stays in the world.
2. The Country spends it back in the city: each day it buys the city's goods with what it took in, keeping a small
   reserve, so the money returns within a day or two and the total stays the same. This also gives the forges, the
   foundry, the ropewalk, the cooperage, the chandlery and Greatly's works the buyers they lack inside the city.
3. Later, each real farm or mine takes over its goods: the store stops selling those (or sells them only when no real
   producer has stock), and the producers earn that money instead of the Country. Designed now (a per-good switch),
   not built.
4. The 200 unplaced country folk (doc 66, 2) are the Country's people, placed later as farmers: a name in the data.

Builds on doc 66 (Phase 5: the economy's rules erased), doc 67 (the city, section 6: the store room) and doc 65 (why the
old economy failed). Vocabulary from docs 35 and 42.

## Where it starts

All of this is the code on `ridgemere2` at ed9d4f3 (doc 67's dense city), and Ridgemere 2.0's export at build 43; the
numbers below are re-run on build 45 (doc 67's food rebalance, merged at 6189ee7: more food makers, the Carters' Yard).

- **Phase 5 removed the economy's rules** (doc 66, Progress, "Phase 5"). Kept: accounts in whole pennies and their
  primitives (`Society::shift`, `sale`, `create`, `consume`, `openAccount`, `cash`), the ledger and `conserved()`
  (`moneySupply() == minted - sunk`); shops' tills (`till:job:<founder>`, `foundShops`, `tillOf`); their wares at
  catalogue prices (`wares`, `catalogPrice`); players' trade (`quote`, `trade`); the catalogue (`Data/Items`:
  `crafts.json`'s crafts with their `hours`, `supplies`, `buys`, `households`, `institutions`).
- **Needs are paused.** `Society::NeedsPaused` is a `static constexpr bool` (true). In `Society::decideAuthored`
  (`RatwResidents.cpp`, the first half) hunger does not grow while it is true; the "eat" branch below it still eats what
  a resident carries. Nobody buys anything.
- **The store room** (doc 67, 6; `RatwSociety.cpp`): `storeGoods`, `storeCost` (`ceil(price × n × markup / 100)`,
  making 100%, resale 75%), `drawFromStore` (whole or nothing; the coins `shift` from the till to `fund:store`, the
  goods `create`d into it). The fund only takes money in: section 9, Q1 of doc 67 says the tills drain into it for good.
- **The restocking rule** (`restockFromStore`, behind `EconomySpec::storeRule`, off by default; the user has approved it
  and it is being switched on in Ridgemere 2.0): at work, once a game hour, a keeper brings each ware back to its
  starting stock, `GoodsKept` (4): one batch an hour where its crafts make it, else a draw. Four on a shelf and one batch
  an hour is far too little for 559 residents (below).
- **Who is in the city** (`tools/city_loop.py` on the export, build 45): 559 residents placed, 477 of them 16 or over,
  in 224 homes; 110 shop keepers (most alone in their room, and three rooms that hold several: Market Square's stalls,
  the Market Hall's counters, the Rag Market's), 30 guards, 116 hands working in a room with exactly one shop, and the
  rest civilians with posts at offices, the palace, the courts, the church, the wharf and the Houses.
- **What they would spend** at catalogue prices with everyone fed and every household need met: **4,329p a day**
  (food 3,517p, households' needs 813p; the needs are `crafts.json`'s `households.needs`). The shops would keep
  **1,554p** of it after the store room (makers keep their value added, resellers 25%). This is the capacity model of
  doc 65, section 4, at this city: `tools/city_capacity.py` (the other session's) gives the same ~4,900p and ~1,760p
  with the institutions' baskets in, which this loop leaves out.
- **What the city's makers could make** working full time (keeper and hands, 8 hours): about 8,900p a day of goods, of
  which the residents ask about 1,000p outside food. So there is room to sell to the Country, if its basket is spread
  over the trades (the coordinator's estimate, kept here: 3,300-3,900p a day spare).

## 1. The loop, as money flows

Every arrow is a `shift` or a `sale` (in the ledger and the journal), so `conserved()` holds at every step. Goods are
made only by the store room (`create`, as now) and leave only by being eaten, used up at home, or carried away to the
Country (`consume`).

```
                      wages (keepers, hands)                    wages (public posts, the watch)
        +-------------------------------------- shops' tills <------+         treasury ---------------+
        |                                     ^   |   ^   |          |            ^                     |
        |        food, needs, comforts        |   |   |   |  carriage|            | gate toll           |
        v      (sale, catalogue price)        |   |   |   +----> carters' till    | (a share of        |
    residents --------------------------------+   |   |      (its drivers' wages) |  each draw)        v
        ^                                         |   |            ^              |                residents
        |                         store draws     |   |  the Country's purchases  |
        |                     (the goods' cost)   |   |  (sale, catalogue price)  |
        |                                         v   |            |carriage out  |
        |  wages (the Country's posts)          the Country (outside:country) ----+
        +-----------------------------------------  keeps its reserve, spends the rest
```

The accounts:

| Account | Who | Takes in | Pays out |
|---|---|---|---|
| a resident's own (`<npc id>`) | every resident | wages; a till's profit (its keeper) | food, needs and comforts (to tills) |
| `till:job:<founder>` | each shop | residents' and the Country's purchases; players' | store draws (to the Country and the treasury), carriage in (to the carters), wages, profit to its keeper |
| the carters' till (`till:` of the `carters` business) | the Carters' Yard | carriage, in and out | its drivers' and keeper's wages |
| `outside:country` | the Country | store draws less the toll | its posts' wages, its purchases, carriage out |
| `treasury` | the capital | the gate toll | public posts' wages; players' welcome grants (as now) |
| `fund:carriage` | only when a world has no carters' yard | carriage | nothing (flagged in reports) |

**When.** The game's hour is the step. The loop runs in `Society::decide` (`RatwSociety.cpp`) when a game hour turns,
before `decideAuthored`, as doc 66 named:

- **8:00 to 18:00, each hour:** households due to shop do so; the Country spends a share of its day's money; keepers
  at work restock (the restocking rule, at the `atWork` hook as now).
- **18:00:** wages: every till, the treasury and the Country pay their posts; then each till over its cap pays its
  keeper the rest as profit.
- **0:00 (the day's turn):** each shop's shelves are reckoned again from the day's sales.

Hours missed (a fast-forward, a restore) run once each, at most a day's worth; hours before the first one the society
decides (a fresh world, a restore) are not run, so nothing is paid twice.

**How much** (`tools/city_loop.py` on build 45, everyone fed, at the defaults below):

| Flow | p a day |
|---|---|
| Residents to shops (food 3,517, needs 813) | 4,329 |
| Shops' store draws for those goods | 2,784 |
| The Country's purchases (its basket, at the makers' 40% margin) | 2,061 |
| Shops' store draws to replace them | 1,231 |
| All draws | 4,015 |
| of which the gate toll to the treasury (30%) | 1,204 |
| of which the Country's intake | 2,810 |
| The Country pays its posts | 728 |
| The Country's purchases, as above | 2,061 |
| Carriage on the draws (15,463 weight in) | 93 |
| Carriage on the Country's purchases (3,570 weight out) | 21 |
| of the carriage, the treasury's subsidy (25%) | 29 |
| Wages from tills (keepers 110 × 14, hands 116 × 8) | 2,468 |
| Wages from the treasury (134 × 8: 104 public posts, 30 of the watch) | 1,072 |
| The treasury's alms (households still short of a day's food after the wages) | measured |
| Wages from the Country (91 × 8) | 728 |
| **All wages** | **4,268** |

**The arithmetic that balances.** In a steady day every account takes in what it pays out:

- *The Country:* `(1 − toll) × draws = its posts' wages + its purchases + carriage out`. The draws include the inputs of
  its own purchases, `(1 − m) × P` where `m` is the makers' margin on its basket, so its purchases are
  `P = ((1 − toll) × D_residents − W_country) / ((1 + carriage a penny) − (1 − toll)(1 − m))`
  `= (0.70 × 2,784 − 728) / (1.006 − 0.70 × 0.597) ≈ 2,060p`.
  *The Country's purchases are its net intake divided by the makers' margin*: each penny it spends comes back to it as
  the shop restocks, so with no toll and no posts it would have to buy 2,784 / 0.40 ≈ 7,000p a day, more than the
  city's spare making. The toll and its own posts are what keep its buying inside what the city can make.
- *The treasury:* `toll × draws = 0.30 × 4,015 = 1,204p` against 1,072p of public wages and 29p of subsidy, about
  100p a day over, for the alms (section 4) and its balance; the run (Progress) measures it.
- *The shops:* their net is `1,558p` from the residents and `0.403 × 2,061 ≈ 830p` from the Country, `2,376p`,
  against `2,468p` of keepers' and hands' wages; the carters take `114p` against their own crew's.
- *The residents:* wages in, `4,420p`; spending, `4,329p`. Wages paid equal what residents spend because every other
  account spends what it takes in; the small gaps are inside what the rules below do when money is short (a till pays
  what it can above its float) or over (savings buy comforts).

These are estimates at catalogue prices with every need met; section 9 measures the game.

## 2. The Country

**Its account** is `outside:<market id>`, here `outside:country` (a new facility prefix, `outside:`, beside `fund:` and
`till:`). It replaces `fund:store`:

- `Society::CountryFund` names it; `StoreFund` stays as the old name, read only for old saves and exports.
- A save that holds `fund:store` loads with its coins shifted into `outside:country` and the old account closed
  ("fund:store is the Country's now", in the ledger): conserved, and the old save still loads.
- `world_check`, `/store` and the DM's money view print the Country's purse instead.

**A list of markets.** `Data/Items/country.json` (new; `crafts.json` is not touched) holds `markets`, a list with one
entry, `country` ("the Country"), each with its own purse (`outside:<id>`), basket and, later, the goods it supplies.
The smaller towns can become markets of their own (each its purse and basket) without changing the rules: a store
draw pays the market that supplies the good (the first in the list that does; one for now), and each market spends on
its own basket. The towns are not built.

**Its intake: store draws.** `drawFromStore` (unchanged checks, all before any money moves): the till pays the goods'
cost (as now) plus carriage (section 6); the cost is split, the toll's share to the treasury and the rest to the
Country, carriage to the carters. A good the Country no longer supplies is refused (below).

**Its spending: the basket.** Each hour from 8:00 to 18:00 it spends `(purse − reserve − its posts' wages due at
18:00) ÷ the hours left`:

- the money is shared over the basket's goods by their weights (`markets[].basket`: whole numbers, scaled to the money;
  57 goods);
- for each good, whole pieces at catalogue price from **its makers' open shops** (a till of a business whose crafts
  make it, whose keeper is at work), never a reseller: the Country buys the city's own make;
- **sellers are spread in proportion to stock**: a piece at a time, the seller picked at random weighted by what each
  holds (a fixed hash of the market, the good, the hour and the piece, so a run repeats). Never "the first in the list"
  (doc 65: the first shop in a list won every channel);
- **a good with nothing in stock** gives its share to the goods that have some that hour; what it couldn't spend at all
  waits in the purse for the next hour (so the purse grows only while the city has nothing it wants: a gate in
  section 9);
- what it buys is carried away (`consume`, "carried to the Country"), and it pays carriage on it (section 6);
- **the reserve** (default 200p, a setting) is never spent, so it can always pay its posts.

**The basket** is weighted toward the trades with time to spare, not a fixed list: `tools/city_loop.py --suggest`
takes each making trade's spare hours (70% of its crew's 8-hour day, less what the residents ask of it), shares them
over its goods the Country would buy (tools, iron, building goods, cloth, containers, drink, preserved fish; not food the
residents live on, weapons, armour or luxuries), and turns each into pennies of what those hours make. The basket in
`country.json` is that suggestion for build 45, the goods under 10p dropped, then trimmed where a good is shared with a
busy trade (salt and smoked fish, which the provisioners and smokehouse also make; the smithy's bars, hoops, rivets and
buckles; the weaver's neckerchiefs; the foundry's and the builders' goods; ledgers). It is re-run when the city changes,
and Atlas's Economy page shows it.

**Per trade: the Country's purchases against each trade's time** (build 45; `use` is all the hours the residents and the
Country ask of the trade over its crew's 8 hours; over 70% is flagged):

| Trade | Crew | Hours | Residents' | The Country's | Use |
|---|---|---|---|---|---|
| smithy | 10 | 80 | 9.1 | 48.6 | 72% (flagged) |
| foundry | 3 | 24 | 0 | 17.4 | 72% (flagged) |
| scriptorium, apothecary | 2, 2 | 16, 16 | 0, 2.9 | 10.5, 7.6 | 66%, 65% |
| builders' works, glassworks, distillery, plasterworks, cartographer, cartwright, tannery | | | | | 56-62% |
| carpenter, jeweler, paintworks, brickworks | | | | | 52-54% |
| ironworks, sawmill, herbalist, pottery, brewery, press, tinker, mason, general | | | | | 41-47% |
| cooperage, papermill, fishmonger, printing house, charcoal, saddlery, ropewalk | | | | | 21-39% |
| armory, perfumer, tailor | | | | | 11-18% |
| **Over 70% from the residents alone** | | | | | |
| bakery | 15 | 120 | 90.3 | 0 | 75% |
| butcher | 14 | 112 | 95.4 | 0 | 85% |
| inn | 25 | 200 | 125.3 | 22.2 | 74% |
| provisioner | 12 | 96 | 92.1 | 7.1 | 103% |
| preserver | 9 | 72 | 81.7 | 0 | 113% |
| dairy | 7 | 56 | 65.2 | 0 | 116% |
| smokehouse | 2 | 16 | 13.9 | 0.6 | 90% |
| weaver | 2 | 16 | 13.9 | 1.1 | 93% |
| chandlery | 5 | 40 | 32.1 | 5.8 | 95% |

- The Country takes the smithy and the foundry just past 70% (72%); every other flagged trade is flagged by the
  residents' own food (and the chandlery's candles, the weaver's scarves). The food rebalance (build 45) brought the
  bakers, butchers and inns under or near capacity; the preserver, dairy and provisioners are still over if everyone ate
  their make, so the residents also eat what the fishmongers and provisioners resell (fish, raw meat), which needs no
  making hours (section 3). A trade at capacity simply sells out: the Country's money for it goes to the goods in stock
  that hour, or waits.
- Trades the Country and the residents together leave with no buyers at all: the moneychanger, the pawnbroker, the
  shipwright, the stables, the Market stalls (their supply lists are flour, oats, milk, vegetables and apples, nothing
  eaten as it is), the vintner, the barber, the builders' yard and the rag-and-bone yards. They sell little or nothing,
  so their keepers' wages go unpaid once their tills run down (doc 65's "too few buyers"). Listed for the user and the
  rebalance; the loop doesn't prop them up.
- The Country spends about 2,060p a day, against about 2,460p the makers could spare at 70% after the food rebalance
  moved crews into food: the city's spare industry is mostly spoken for.

**The per-good replacement switch.** Each world keeps a list of goods the Country no longer supplies
(`live.economy.country_off`, Atlas's per-good switch; empty by default):

- **Built:** a good in the list is refused by the store room ("the city's own producers supply it now"), for making and
  resale alike; Atlas lists every good any business draws, with its switch.
- **Designed, not built:** when a real farm or mine is added, its goods go in the list, and its keeper's till sells
  them to the shops. A third setting, "Country when short", would let the store sell a good only while no real
  producer's till holds it; it needs a producer's till, which nothing has yet, so it waits for the first farm.
- The Country's own purse then takes in less, and spends less: no rule changes.

**The Country's people.** `markets[].people` names the 200 unplaced country folk as its people. Nothing reads it yet;
when they are placed on farms (the replacement above), they are the producers.

## 3. Residents

`NeedsPaused` becomes a switch, `EconomySpec::needsPaused`, **off** (the loop on) by default. While it is off:

- **Hunger grows** as it did before Phase 5 (`.0035` a second, about 50 a day), and a resident at 60 or more who carries
  food eats it (the existing "eat" branch). A resident needs about 46 nourishment a day.
- **A household** is the residents who share a home, joined by kin: a child under 16 and a spouse who live in another
  home (the generator gave some children a room of their own upstairs: 21 of them, alone on a floor) shop with their
  family, so whoever earns feeds them. In a communal home (barracks, quarters, bunkhouse) each resident is its own.
- **Food, once a day.** Each household has its market hour, 8 + (a hash of its home) mod 10. At that hour every member
  is topped up to `food.days` (2) days of food; a member still under `food.shortDays` (1) tries again every hour until
  18:00. Food goes into each member's own purse (where the eating branch looks); it is paid for by whichever member has
  the most money and can pay (children and the retired are fed by whoever earns).
- **What food.** Among the foods the household's community's open shops hold (an open shop: its keeper at its post,
  `trade`), or every open shop's where its own community's have none (the Old Keep's watch house has a shop or two
  but no food), picked at random weighted by nourishment for the price, so cheap filling food is bought most but every food
  shop sells; the seller weighted by stock, as the Country's are.
- **Needs**: the eight in `crafts.json`'s `households.needs` (firewood a day, twice in winter; candles every 2 days;
  cider every 4; clothes every 25 a grown head; bandages and nails every 20; a clay pot every 30; a cookpot every 60),
  bought at the household's market hour when due (due by a fixed hash of the home and the day, so nothing is saved),
  used up at home (`consume`). A household keeps `households.reserve` (6p) a head back for food before buying needs.
- **Comforts**: a household holding more than `comforts.above` (60p) a head spends `comforts.share` (a tenth) of what
  is over it a day on `comforts.goods` (ale, cider, wine, candles, cakes, cheese, scarves, soap, charms, pamphlets, pots),
  used up at once. This is the loop's only rule against hoarding: savings come back to the shops.
- Nothing walks to a shop for this: it is a market round, not a trip. Walking to buy is later polish; it would cost
  each resident a trip a day and change nothing in the money.

## 4. Wages

**Who pays a post** (`Society::payerOf`, worked out with the records and kept while the roster stands):

1. a shop's **keeper** (a merchant post): its till, `wageKeeper` (14p);
2. **the watch** (a guard post): the treasury, `wagePublic` (8p);
3. a **hand**: a post whose work room holds exactly one shop (the Washhouse's eight, the Ironworks' four, an inn's
   cook): that shop's till, `wageHand` (8p). (A room with several shops, Market Square, the Market Hall and the Rag
   Market, names no employer; those posts go on to the rules.);
4. otherwise the **first rule in `country.json`'s `wages.rules`** with a word in the post's title, lower case:
   - the Country (`wageCountry`, 8p): carriage by sea and its stores (unloading ships, the cranes, carrying up from
     the wharf, the Bonded Warehouse and the Iron, Long and New Stores, the Wharf Office); and the Great and Minor
     Houses' people, kept by their estates (heads, stewards, servants, the Houses' trades, the Heights' gentry);
   - the treasury (`wagePublic`, 8p): the Crown and the Council, the courts, the offices, the Records Office, the
     customs, the lamps, the markets' hands, the washhouse and baths, the teachers, the abbey, the cathedral and the
     chapels (clergy are paid: there is no tithe);
5. otherwise nobody: children, apprentices (they learn), the retired, those out of work, and posts such as "sits and
   watches the world", "lending", the Queen and the King's daughter (their households feed them).

On build 45 that is 110 keepers, 116 hands, 30 watch, 104 public posts, 91 the Country's: **451 paid posts**. (A rule word must not catch another post: "watch" alone caught "sits and watches the world"; the rule says "the
watch".)

**Paying, at 18:00.** Each payer pays every holder of its posts once a day, every day (a week's wage spread over its
days, Restday too). A till pays from what it holds above its **float** (20p, or what it drew from the store yesterday if
more: its working money); short of the whole, it pays each post the same share of its wage. The treasury keeps 200p
back (players' welcome grants); the Country its reserve. A till holding more than its float and a week of its wages
pays its keeper the rest as **profit**.

**The city's alms**, at 18:00 after the wages: a household still holding less than a day's food money a head
(`Society::AlmsADay`, 7p) is given what makes it up, from what the treasury holds over its float, paid to its member
with the least. This is the safety net under every rule above: the old who have retired (succession takes their posts:
19 "sit and watch the world", and more retire in a run), orphans (the generator placed some children alone), the out of
work living alone, and a family whose shop couldn't pay its wage. It is paid only from money the treasury has.

**The numbers** are set from the city's spending, not doc 64's 60p: at today's catalogue prices a resident needs about
6.3p of food and 1.5p of needs a day, and a household has about 2.1 earners for 2.5 mouths, so a hand's 8p feeds a
family with a little over, and a keeper's 14p is the value a busy maker adds in a day (crafts add 1.5-6p an hour,
`tools/city_loop.py`). All four are settings.

## 5. The treasury: the city's taxes, and what they pay for

The user: "the city's taxes should be used to supplement areas that help businesses ... Services and whatnot."

**Its income, the city's tax: a gate toll** (decided for now; the user may change it): `gateToll` percent (30) of every
store draw's cost goes to the treasury instead of the Country, as a city's toll on goods brought in through its gates.
The till pays the same; only the Country's share is smaller. Each till keeps its fraction of a penny owed until it makes
one, so many small draws pay what one large one would. It is conserved, it grows with trade, and it costs no new rule.

- Rejected for now: a toll on every sale (it changes what shops keep, and players would pay it); a tax on purses
  (doc 42's, removed by Phase 5); the Country paying the Crown a rent (the same money by a longer road).

**Its spending, in this order:**

1. **Public wages** at 18:00: the watch, the offices, the Records Office, the palace, the courts, the church, the lamps,
   the markets' hands, the pensions. It keeps a 200p float (players' welcome grants).
2. **The city's alms** (section 4), after the wages.
3. **Service subsidies**: services that help businesses, each a share of its cost the treasury pays
   (`EconomySpec::subsidies`, by service; `Society::Subsidised` lists the services). The first is **carriage**: the
   treasury pays `subsidies.carriage` percent (25) of every delivery's carriage, in (a shop's store draw) and out (the
   Country's purchases), to the carters, so the shop or the Country pays the rest. Later services join the list the
   same way (the Town Works' road upkeep, market upkeep, lamplighting).

**Safe by construction.** A subsidy is paid only from money the treasury has: today's room is the smaller of a twentieth
of what it held at the day's turn (`SubsidyDay`, as the old institutions drew "a twentieth of its money") and what it
holds over its float and tonight's public wages. When the room is gone the shop pays the whole carriage: **the subsidy
shrinks first, before public wages**. Nothing is made; every penny is a `shift` from the treasury to the carters'
till. Tested in `country_tests` (`theSubsidyGivesWayFirst`).

**The default, 25%**: carriage comes to about 114p a day, so a quarter is about 29p, 2% of the toll and well inside a
twentieth of a 1,000p treasury (50p). It halves what carriage adds to a heavy cheap good (firewood, bricks).

## 6. Carriage

The user: deliveries from the Country and to it are carried, and the carters are paid.

- **The rate**, `carriage` (6p a wagon load), a load being 1,000 of items.json's `weight`, of which the treasury pays
  its subsidy's share (section 5). Every store draw pays
  carriage on the weight it brings in; every Country purchase pays carriage on the weight it takes out. Pennies are
  whole: each payer keeps its fraction of a penny owed (not saved) and pays the whole pennies as they come.
- **Inbound, the shop pays** on top of the goods' cost (it is the shop's delivery). **Outbound, the Country pays**, out of
  its purse, so its spending on goods shrinks by that much (the buyer pays the freight). The seller nets the full
  catalogue price: makers' margins are thin (40% on the basket, 25% for resellers), and carriage taken from them would
  fall hardest on the trades with the fewest buyers.
- **To the carters**: the till of the `carters` business (the Carters' Yard, by the East Gate, build 45: a keeper,
  "keeping the Carters' Yard", and six drivers, "driving for the carters"); its drivers are its hands (they work in its
  room) and are paid like any business's. With several yards, carriage goes to the one holding least. A world without
  one pays `fund:carriage` instead (conserved; flagged in `world_check` and Atlas).
- **Sized** (build 45, `tools/city_loop.py`): 15,463 weight in and 3,570 out a day, **19 wagon loads**; at two round
  trips a driver a day, **10 drivers**. Carriage brings in 114p a day (93p in, 21p out; 29p of it the treasury's)
  against a keeper's 14p and ten drivers' 80p, 94p. Carriage adds 3% to the cost of the city's inputs (93p on 2,784p),
  more on the heavy cheap goods (firewood 0.09p on a 1p piece, bricks 0.24p on a 4p one). The drivers' wages come back
  as spending like everyone's. **For the city: one Carters' Yard with a keeper and about 10 drivers**; the yard as built
  has 6, so it is paid for 10 drivers' work and pays 6 (it keeps the rest as profit, section 4). Four more drivers, or
  three-trip days, fit it.
- **Seen in the world**: not built in this pass. `World::sendCaravan` (`RatwRoads.cpp`) moves a caravan between two
  towns; Ridgemere 2.0 has one town and the gates lead to the estates, so a daily wagon out of a gate needs a "town" at
  the canvas edge for it to go to. Left for later; the money doesn't wait on it.

## 7. Shops' shelves (the restocking rule, revised)

The rule doc 67 built (one batch an hour, shelves of 4) cannot feed 559 residents: the city eats about 25,600 nourishment a day,
some 1,300 pieces of food. Revised, behind the same `storeRule` switch:

- **Shelves follow sales.** A ware's shelf is `max(its starting stock, ⌈1.25 × what sold of it yesterday⌉)`, at most
  1,000, reckoned at the day's turn (not saved: after a restart the shelves start again from the starting stock and
  grow back in a few days).
- **Making takes hours.** A maker has a crew (its keeper and its hands, section 4). Each working hour its crew's hours
  go to batches, the ware furthest below its shelf first, each batch taking its craft's `hours`; hours left over carry
  to the next hour, up to two hours' worth. So a bakery of two makes about 9 batches of bread (54 loaves) a day, as
  `tools/city_loop.py` counts.
- **Reselling takes none.** A vendor draws up to its shelf at once: buying from the Country is its work.
- What it pays and refuses is the store room's, as before (whole draws, the till's cash).

## 8. Rent

Left out. Homes are the households' own for now; nobody pays for a roof. Rent would move money from the poor to
landlords (the Houses), who would need to spend it back; it waits until the loop is stable.

## 9. Atlas, the DM, and the settings

**Settings** (`EconomySpec`, each world's own in `live.economy`, never synced, as doc 67's store switches):

| Setting | Default | Range | What |
|---|---|---|---|
| `needsPaused` | off | | Phase 5's pause: hunger held, nobody buys, wages and the Country stop |
| `country` | on | | the Country spends what it takes in; off, it only takes in (as `fund:store` did) |
| `countryReserve` | 200 | 0-100,000 | pennies the Country keeps |
| `gateToll` | 30 | 0-90 | percent of each draw's cost to the treasury (the city's tax) |
| `subsidies` | carriage 25 | 0-100 each | the share of a service's cost the treasury pays (carriage now) |
| `carriage` | 6 | 0-1,000 | pennies a wagon load (1,000 weight), in and out |
| `wageKeeper`, `wageHand`, `wagePublic`, `wageCountry` | 14, 8, 8, 8 | 0-1,000 | a day's wage |
| `countryOff` | none | goods | goods the Country no longer supplies |

**Migration 0056** (`Database/migrations/0056_country.sql`): the columns on `live.economy` (`needs_paused`, `country`,
`country_reserve`, `gate_toll`, `carriage`, `wage_keeper`, `wage_hand`, `wage_public`, `wage_country`,
`country_off text[]`, `subsidies jsonb`), and `live.people_manifest` again with one line after the `store` line, only
when a value differs from its default:

    loop needs on|off country on|off reserve N toll N carriage N wages K H P C

one `countryoff "<item>"` line for each good the Country no longer supplies, and one `subsidy "<service>" PERCENT` line
for each service away from its default. Core parses both
(`RatwAuthoring.cpp`, beside `store`); `Game::withoutPeople` strips them. Worlds without 0056 read the defaults, and a
save of a non-default setting there is refused ("this world lacks migration 0056"), as doc 67's 0055 does. Applied only
to `ratw_ridgemere2` (and test worlds); DEV and PROD are the user's to migrate.

**Atlas's Economy page** (`Editor/src/studio/EconomyControls.tsx`) gains:

- **The city's loop:** residents' needs on or paused; the four wages.
- **The Country:** on or off, its reserve, the gate toll, the carriage rate and the treasury's carriage subsidy; its basket (from `country.json`: each good,
  its share, its makers); per good, whether the Country supplies it (every good any business draws, with "All on").
- A note when the restocking rule is off: "the loop needs the restocking rule on, or shops sell out and stay empty".

Routes: `GET /api/atlas/economy` returns the new settings and the basket; `POST /api/atlas/economy/loop` sets them;
`POST /api/atlas/economy/goods` `{set: {item: bool}}` the per-good supply.

**The DM's money view** (`tools/dungeon_master.py`, `Editor/src/dm/MoneyTab.tsx`) lists the `outside:` accounts beside
`fund:` ones, and the carters' tills under their own heading, so the Country's purse and the carters' takings show on a
live world. **The DM's levers** (`GET /api/dm/economy?world=KEY`, `POST /api/dm/economy/loop {world, ...}`, dm and admin,
audited): the gate toll, the wages, the subsidies, the Country's switch and reserve on a live world, written to its
`live.economy` with the DM's own role; they take effect when the game server next starts. `world_check`
prints the Country's purse, the treasury, the carters' takings and the economy's day (below).

## 10. Stability gates and how to measure them

**How to run it now:** see `Docs/Design/69-simulation-speed.md`, section 10 (no player, `--no-profile`, `RATW_DATA_DIR`
set to the checkout's `Data`; 60 days in about a minute, `--fast` opt-in). The runs below were made the old way, with
world_check's benchmark player.

`world_check EXPORT --simulate 6 HOURS --economy FILE` (new flag) writes one JSON line a game day: residents hungry
(hunger 80 or more) at each hour, households with a hungry member, shops holding goods and food shops holding food at
noon, tills short of their float, wages owed and paid, the Country's purse, the treasury, the carters' takings, money in
all, conserved, and the Gini of cash over earning households (households with a paid post; children and the retired
not counted as households of their own). Runs of 30 game days, and 60 if 30 is clean; never a year (memory: no long
runs).

| Gate | Pass |
|---|---|
| Hunger is rare | residents hungry, the mean of the hourly samples over days 5-30: under 2%; no day over 5% |
| Families fed | households with nobody hungry at noon: 98% or more every day after day 3 |
| Shops open and stocked | 90% of shops holding something they sell at noon; 90% of food shops holding food |
| Tills | under 10% of tills short of their float at 18:00; the median till not falling over the last 10 days |
| Wages | 90% of posted wages paid |
| The Country | its purse at 18:00 within its reserve and two days' intake; no rising trend |
| The treasury | above its 200p float; within ±25% of where it started over the run |
| Money | conserved at every day's end (exact) |
| Inequality | Gini of cash over earning households: reported (above .4 acceptable if the rest holds) |

## 11. Open questions for the user (each decided for now)

1. **The toll.** The treasury lives on a 30% gate toll on what the shops draw from the Country. Decided for now; a toll
   on sales, or the Country paying the Crown a rent, are the other choices.
2. **Who the Country pays.** The dockers, cranes, Sump carriers and the Country's warehouses, and the Houses' people
   (91 posts) are paid by the Country, as the people of its trade and its estates. Decided for now; the other choice is
   the treasury, or no wage.
3. **Clergy are paid by the treasury** (no tithe). Decided for now.
4. **Wages are flat by kind** (keeper 14, hand 8, public 8, the Country's 8), every day including Restday. Decided for
   now; a House head and a docker earn the same.
5. **Trades with no buyers** (moneychanger, pawnbroker, shipwright, stables, the Market stalls, vintner, barber,
   builders' yard, rag-and-bone): their keepers go unpaid once their tills run down. Decided for now: the loop doesn't
   prop them up; the food rebalance or a change of trade can.
6. **The institutions' baskets** (Town Works, the watch's bread, the church's candles) are not bought. Decided for now.
7. **Rent** is left out (section 8).
8. **Carriage** is 6p a wagon load each way, the shop paying in and the Country paying out (section 6). Decided for now.
9. **Shelves follow a day's sales and making takes its hours** (section 7). Decided for now.
10. **Alms, not a pension.** A household still short of a day's food money after the wages is topped up by the
    treasury (7p a head). Decided for now; the other choice is a pension for the retired (19 now, more as they retire).
11. **The carriage subsidy** is 25% of every delivery's carriage, from the treasury, capped at a twentieth of it a day and
    giving way before public wages. Decided for now; more services (road upkeep, market upkeep, lamps) can join.
12. **The Carters' Yard has 6 drivers; the flows want about 10.** Decided for now: the yard keeps the difference as
    profit. Four more drivers would fit.

## 12. Trades we could add for exports

Doc 67 Q7 left out four trades; the user says they could be added if the city needs more industry for the Country.
Recommendation only; nothing is added here.

| Trade | What it needs | What it would sell the Country | Needed? |
|---|---|---|---|
| **Locksmith** | a `locksmith` business (`businesses.json`); `lock` (15p) already exists with an NPC craft (iron bar, 2.8 h) whose makers are the smithy and tinker; add `locksmith` to its makers, and to `chain` and `fittings` | locks, chain, fittings | the cheapest to add (no new item or craft) |
| **Cobbler** | a `cobbler` business; footwear exists as paw wear (`paw_boots` 16p, `hobnail_boots` 22p, `leather_bindings`) with player recipes; NPC crafts for `paw_boots` (leather) and `hobnail_boots` (leather, nails) in `crafts.json` | boots for country folk | would take load off the tannery and saddlery |
| **Dyer** | the `dyeworks` business exists (sells `dyed_cloth`, `scarlet_cloth`) with no NPC crafts; the player recipes take three inputs (wool cloth, a dye, alum) and NPC crafts take at most two (`RatwItems.cpp`), so a two-input craft (wool cloth and alum, say) or a wider craft | dyed cloth | needs a catalogue change |
| **Bookseller / stationer** | a reseller of paper, ink, pamphlets, ledgers and maps (a `supplies` list); no new items | nothing of its own: the Country buys the city's make from its makers | not for exports |

**Are they needed?** Yes, on build 45, for two of them: the food rebalance moved crews into food, so the Country's
2,060p a day now takes most of the 2,460p the makers can spare at 70%, the smithy and the foundry are just past 70%
(72%), and the weaver is over from the residents alone (93%). A **locksmith** would take locks, chains and fittings off
the smithy, and a **cobbler** would give the Country a new good (boots) and its own crew; both are cheap. The dyer needs
a catalogue change and the bookseller adds no making. Whether the Country's money goes unspent is measured in the run
(Progress: `country_unspent`).

## Progress

### Built (2026-10-31)

Branch `r2-country`, from `ridgemere2` (ed9d4f3), with `ridgemere2` merged at 6189ee7 (the food rebalance and the
Carters' Yard). The plan was committed first (d249de1).

- **Core.** `Core/RatwCityLoop.cpp` (new): who pays a post (`payOf`), crews, the households (by home, joined by kin),
  their food, needs and comforts, the Country's buying spread over its makers by stock, wages, a till's profit, the
  alms, the restocking rule revised (shelves from yesterday's sales, making by the crew's hours), the gate toll and
  carriage with the treasury's capped subsidy. `RatwSociety`: `drawFromStore` splits its cost (the Country, the toll) and
  pays carriage; `CountryFund` (`outside:country`) replaces `fund:store`, whose coins an older save hands over;
  `needsPaused()` a switch (off); the loop runs hourly from `decide`. `RatwItems`: `country.json` and `crafts.json`'s
  households. `RatwAuthoring`: the `loop`, `countryoff` and `subsidy` records; `Game::withoutPeople` strips them.
  `/store` and `world_check` print the Country's purse, the treasury and the carters; `world_check --economy FILE`
  writes the day's measures.
- **Data.** `Data/Items/country.json` (new): one market, the Country, with its 57-good basket; the wage rules; the
  residents' food and comforts. `crafts.json` is not touched.
- **Migration** `Database/migrations/0056_country.sql`: **not applied to any world.** Tested on a scratch copy of
  `ratw_ridgemere2` (made by `CREATE DATABASE ... TEMPLATE`, migrated with `world_db.py migrate`, read and written
  through Atlas's routes, then dropped): it applies cleanly after 0055, writes no new record at the defaults, and
  writes `countryoff` and `subsidy` lines when set.
- **Tools.** `world_store` (0056's columns, `STORE_TABLES` for a world with 0055 only, refusals naming 0056),
  `live_edit`, `map_editor` (the checks and the exported records), `studio` (`POST /api/atlas/economy/loop` and
  `/goods`; the DM's `GET /api/dm/economy`, `POST /api/dm/economy/loop` on live worlds), `dungeon_master` (the money
  view lists `outside:` funds and the carters' tills), `tools/city_loop.py` (the plan's arithmetic and `--suggest`).
- **Atlas.** `Editor/src/studio/CountryPanel.tsx` (new) on the Economy page: residents' needs, the Country on or off,
  its reserve, the toll, carriage, the carriage subsidy, the four wages, the basket, per-good supply. The DM's money
  tab names the Country's purse and the carters.
- **Tests.** `Tests/country_tests.cpp` (565 checks: who pays each post; a draw's split and carriage, to the carters' yard
  and to `fund:carriage`; small draws' fractions; the subsidy's cap and its giving way before public wages; a day of the
  loop conserved, everyone fed, a child upstairs and an orphan fed, wages from all three payers, the Country buying from
  both smithies and only makers' goods; needs paused; the Country off; an older save's `fund:store`; the records and
  their refusals). `store_room_tests`, `shops_tests`, `town_tests`, `project_tests` and `game_tests` updated (the store
  room's tests pause the loop; `/store` shows the Country). All 69 C++ tests pass. Python: `test_world_store` (the
  loop's settings round trip, a world with 0055 but not 0056), `test_studio` (the loop and goods routes, the DM's
  levers on a live world); the suites run (`test_world_store`, `test_studio`, `test_live_edit`, `test_map_editor`,
  `test_dungeon_master`, `test_world_sync`, `test_world_build`, `test_item_catalog`, `test_city_capacity`) all pass but
  one test, `test_map_editor`'s Greyfen resident count (11, not 10), which is not this work's (Greyfen's data is untouched). Editor: `tsc` clean; `npm test` 76 of 77, the same Greyfen count failing.

**Found on the way.**

- A wage rule word must not catch another post: "watch" caught "sits and watches the world"; the rule says "the watch".
- The generator gave 21 children a home of their own (a floor upstairs from their parents), and placed a few orphans
  and the retired alone: households are joined by kin, and the alms catch the rest.
- Succession retires the old from their posts as the run goes on: they lose their wage, so the alms grow.
- The Old Keep's watch house is its own community with a shop or two but no food: a household buys from every open
  shop when its own community's have none.
- Two short runs before the last code (a pension instead of alms, households by home only, community-bound shopping)
  showed hunger rising to 20-24 residents by day 7; those runs were replaced by the two below.

### Measured: 30 days (2026-10-31)

`world_check` on build 45's export (`--simulate 6 726 --no-check --deterministic --threads 4 --economy`), the defaults
above. 8.64 million ticks: mean 0.37 ms, p99 2.16 ms, worst 59 ms; 3 of 559 held over ten minutes.

| Gate | Pass | Result |
|---|---|---|
| Hunger is rare | under 2% mean, no day over 5% | **pass**: mean 0.08% (0.42 of 559) over days 5-29; the worst hour 3 (0.5%) |
| Families fed | 98% every day after day 3 | **pass**: lowest 99.2%, mean 99.8% |
| Shops stocked | 90% of shops, 90% of food shops | **pass**: lowest 95.5% of shops; food shops 100% every day |
| Tills | under 10% short; the median not falling | **fail**: 23% of tills short at 18:00 on average (most 43 of 110); the median till 89p on day 20, 107p on day 29 |
| Wages | 90% paid | **fail**: 88.0% |
| The Country | within its reserve and two days' intake, no trend | **pass**: 516p to 704p, at most 790p; 68p a day left unspent on average |
| The treasury | above 200p, within ±25% | **fail on the band**: 986p to 4,121p (most 4,683p); it rose for three weeks, then held at 3,400-4,700p as the alms grew |
| Money | conserved | **pass**: 52,946p at the start and end, every day |
| Inequality | reported | Gini of cash over 209 earning households: .489 to .624 |

Where the money went, a day on average: residents spent 4,878p (1,196 pieces of food, 292 needs, 270 comforts); the
Country bought 2,373p; shops drew 4,490p from the store (the toll 1,345p); wages owed 4,052p, paid 3,566p; profit to
keepers 983p; alms 339p (rising from about 140p in week 1 to 500-580p in week 4); the carriage subsidy 31.5p (always
funded: the treasury never came near its cap's limits); carriage 125p a day to the Carters' Yard.

**Reading it.** The residents are fed and the shops stocked, with money conserved: the loop works. What does not hold
is where the money settles. A fifth of the tills can't pay their posts (the trades with too few buyers, section 2), while
the busiest tills pay their keepers about 1,000p a day of profit; so keepers' households grow rich (the Gini rising), the
hands of the poor shops go short, and the treasury's alms make up the difference. The treasury takes more in toll than
it pays out until the alms catch up.

### 60 days: stopped

A 60-day run of the same world and code was started beside the 30-day one and stopped at the user's request after 32
game days (its first 30 days are the run above, which is deterministic). It is not restarted here: the fast harness
being built on `r2-sim-fast` (from d4dd755) will redo the long runs in minutes.

### Proposals for the user (none applied; each needs a run to confirm)

The coordinator asked for no further runs: these are the tuning I would make next, for the user to approve.

1. **The toll to 27%.** The treasury gained about 105p a day over the 30 days with the alms paid; 27% would hold it near
   where it starts, leaving more with the Country (and so more of the city's goods bought).
2. **Profit shared with the hands, and a lower cap.** A till over its float and three days' wages (not seven) pays the
   excess half to its keeper and half among its hands. Money then stays with the people who spend it on food rather than
   piling with keepers; it should lower the Gini and the alms.
3. **The poor trades.** The 20-40 short tills are the trades with too few buyers (section 2: moneychanger, pawnbroker,
   shipwright, stables, the Market stalls' supply-only lists, vintner, barber, builders' yard, rag-and-bone, and
   whichever smithies and works the Country passes over). Either give them goods people buy (a stall that sells food it
   makes, the vintner's wine in the comforts), or let the city place fewer of them. The loop doesn't prop them up.
4. **Comforts sooner.** A household over 40p a head (not 60p) spends a fifth (not a tenth) of the excess: savings
   come back to the shops faster.
5. **Four more drivers** at the Carters' Yard (it has 6; the loads want about 10), or carriage at 4p a load.
6. **A locksmith and a cobbler** (section 12), if the smithy and foundry stay near 70%.

### Seasons and the DM's rules (2026-10-31)

The user, 2026-10-31 (verbatim): "One thing we're going to have to keep in mind is harvest seasons, and there should be a
fluctuation in specific food types throughout the year. Not to say they always all go to zero, but there may be more
dried and preserved fruits and fresh fish in the winter and more bread in the summer. Etc etc. Livestock should probably
be a year-round sort of thing. I want to start simple and small, and just make sure that the simulation works and things
are running, then go into figuring out supply and demand, and applying fixes in rulesets that can be managed or tweaked
from DM. We're trying to find a good supply/demand balance along with cost/pay."

Branch `r2-seasons`, from `ridgemere2` (4863fb6).

**The seasonal table** (`Data/Items/country.json`, `seasons.groups`; Core `items::SeasonGroup`, `Society::seasonTable`).
Each group lists its goods and, for Spring, Summer, Autumn and Winter, in percent of normal: `supply` (how much the
Country can send a day, of the group's `normal` pieces a day), `price` (what its goods cost) and `shelf` (how full the
shops keep their shelves of them). Each season's value stands at its middle day (0-based days 46, 138, 229.5 and 320 of the
365: `RatwCalendar.h`'s 92/92/91/90); the days between blend in a straight line, Winter's running on into Spring's across
the year's turn (`Society::seasonal`). A good is in one group at most (the first that names it); goods made of a group's
goods sit in it where their price should follow it (bread with the grain). Placeholders, each world's to change:

| Group | Goods | Supply % (Sp/Su/Au/Wi) | Price % | Shelf % | Normal a day |
|---|---|---|---|---|---|
| grain and flour | wheat, flour, oats, bread, fine bread, porridge | 85 / 115 / 140 / 75 | 110 / 95 / 85 / 115 | 100 | 150 |
| fresh fruit | apples, berries, grapes | 50 / 130 / 150 / 30 | 120 / 90 / 80 / 140 | 100 | 950 |
| vegetables | vegetables, mushrooms | 70 / 140 / 130 / 40 | 110 / 85 / 90 / 130 | 100 | 0 (no cap) |
| fresh fish | fish, shellfish | 100 / 85 / 100 / 140 | 100 / 110 / 100 / 85 | 100 | 300 |
| livestock and meat | raw meat, mutton, pig, sheep, rabbit, offal, eggs | 100 | 100 | 100 | 330 |
| milk and dairy | milk, butter, cheese | 105 / 110 / 100 / 85 | 100 / 95 / 100 / 110 | 100 | 260 |
| honey | honey | 80 / 120 / 115 / 60 | 105 / 95 / 95 / 115 | 100 | 75 |
| dried and preserved fruit | dried apples, fruit preserves | 100 | 100 | 100 / 120 / 250 / 150 | 0 (no cap) |

`normal` is about what build 48's city drew of the group a day with no cap (days 2-3 of a 3-day run); vegetables are
drawn by nobody (no resident eats them as they are), so they have no cap.

**What the table does:**

- **Supply: a cap on store draws.** All the shops together may draw at most `normal × supply` pieces of a group a day
  (`Society::countryLeft`, `groupCap`); a draw past it is refused ("The Country has only 12 grain and flour to send
  today (the season)"), and the restocking rule draws only what is left (resale and makers' inputs alike). The day's
  count starts again at the day's turn (not saved: a restart starts the day's supply again). A supply of 0 sends none;
  a `normal` of 0 is no cap.
- **Price: draws and the shops' prices.** A store draw of a group's good costs its catalogue price × markup × the price
  factor (`storeCost`); a resident buying it pays the catalogue price × the factor: the sale at the catalogue's price,
  then the difference shifted (to the till when dearer, back to the buyer when cheaper), each payer's fractions of a
  penny carried until they make one (`priceCarry_`, starting at half a penny), so a 1p loaf costs its season's price over
  many buys. Residents' choice of food already weighs nourishment for the price, now the season's: they buy what is in
  season. The Country's purchases and players' trade stay at catalogue prices.
- **Shelf: stocking up.** A ware's shelf (`shelfOf`: its starting stock or 1.25 × yesterday's sales) times the group's
  shelf factor, at most 1,000.
- **The preserver's winter stock** (the simplest workable rule): dried apples and fruit preserves don't follow the season
  themselves (supply and price 100%), but their shelf does: 120% in summer, 250% at autumn's middle, 150% at winter's,
  100% by spring. So the preserver makes up to two and a half times its sales while fruit is cheap and plentiful, its
  stock carries into the winter (they keep 180 and 365 days), and as the shelf falls through the winter it sells the
  stock down before making more, while fresh fruit is short (30% supply) and dear (140%). No look-ahead and nothing
  saved: a seasonal shelf target.
- **Before the date is known** (a society that hasn't decided yet, a test drawing straight away) no season applies.

**The DM's rules.** Everything the loop and the store room have as settings is editable in the DM suite's new
**Economy** tab (`Editor/src/dm/EconomyTab.tsx`) on the DM's chosen world, as on Atlas's Economy page (which stays, with
the same settings and a new seasons panel, `Editor/src/studio/SeasonsPanel.tsx`, shared by both): the store room's
switches and markups and the restocking rule; residents' needs; the four wages; the Country on or off, its reserve,
the gate toll, carriage and its subsidy; the per-good Country supply; **the seasonal table** (a group's supply, price
and shelf by season, and its normal; "As country.json" puts it back); and **the comforts** (the threshold a head and the
share a day; empty: country.json's). A business's own store-room switch stays Atlas's (it travels with the world's
businesses).

- Routes (`tools/studio.py`): `GET /api/dm/economy?world=KEY`; `POST /api/dm/economy/store`, `/loop`, `/goods`,
  `/seasons` `{world, set: {group: {supply?, price?, shelf?, normal?} | null}}`, `/comforts` `{world, comfortsAbove?,
  comfortsShare?}`; for dm and admin, audited in the world's DM audit (`economy.store`, `economy.loop`, `economy.goods`,
  `economy.seasons`, `economy.comforts`). Atlas: `POST /api/atlas/economy/seasons` and `/comforts`, as its others.
- **Applied live.** Every economy change, the DM's and Atlas's, queues `economy.sync` in the world's `dm.actions`; the
  world's running game server (DEV run as the server included) reads its `live.economy` again (`loadWithLivePeople`) and
  applies it at once (`Society::adoptEconomy`: every rule the loop and the store room read; the treasury's and stores'
  starting amounts kept). With no server running the action expires unapplied after ten minutes, and a server started
  later reads the row. Scratch servers ignore DM actions (as for every DM action). **Not live:** a business's own switch
  (Atlas's; on the next start) and `Data/Items/country.json` itself (the groups, their goods and the defaults: on the next
  start).

**Migration 0057** (`Database/migrations/0057_seasons.sql`): `live.economy.comforts_above` and `comforts_share`
(nullable: country.json's), `seasons jsonb` (`{}`: the groups as country.json has them), and `live.people_manifest` with
a `comforts ABOVE SHARE` line ("-" for country.json's) and a `season "<group>" supply .. price .. shelf .. normal N` line
a changed group. Core parses both (`RatwAuthoring.cpp`); `Game::withoutPeople` strips them. A world without 0057 reads
country.json's; the tools refuse a season or comforts setting there ("this world lacks migration 0057", `world_store`'s
`LOOP_TABLES` and `NO_SEASONS`, the studio's 409), while its loop settings still change. **Applied to DEV
(`ratw_ridgemere2`) only**, 2026-10-31, after `Saved/Backups/ratw_ridgemere2-before-seasons.dump` and with no other
writer running: DEV reads the defaults (no new records); PROD is untouched (still at 0054).

**The economy file** (`world_check --economy`) gains, a day: `season` and `year_day`, and per group `G_supply`, `G_price`
and `G_shelf` (the factors), `G_cap` (today's cap; -1 for none), `G_drawn` (pieces drawn from the Country), `G_sold`
(pieces sold to residents), `G_stock` (on the shops' shelves at 23:00) and `G_paid` (residents' mean price a piece).
`world_check --day N` starts a run on day N of the year (1-365) without ageing anyone.

**Tests.** `Tests/season_tests.cpp` (110 checks; a catalogue whose seasons are the test's own): the factors by day, at
the middles, between them and across the year's turn; a winter draw at twice the price and capped at the season's five,
refused past it, the next day's supply, autumn's plenty, money conserved; a day of the loop in winter and in autumn:
residents paying the season's price and eating more bread in autumn; the preserver's shelf three times fuller in
autumn, making more then and only its usual in winter; the season and comforts records and their refusals,
`withoutPeople`; `adoptEconomy` changing the rules at once. All 71 C++ tests pass. `test_world_sync` (a real server on
a throwaway dev world applies a queued economy.sync: "applied"). Python: `test_world_store` (the
seasons and comforts round trip, records from both the export and the database's manifest; a world with 0056 but not
0057 saving its loop settings and refusing a season setting, naming 0057), `test_studio` (the DM's five routes, audited,
each queueing economy.sync, Atlas's too; for real on a scratch world: the manifest lines, `dm.actions`, back to
country.json's, refusals, a world without 0057 refused by both suites); `test_live_edit`, `test_dungeon_master`,
`test_world_sync`, `test_world_build`, `test_item_catalog`, `test_city_capacity`, `test_world_db` pass;
`test_map_editor` fails only its Greyfen resident count (11, not 10), as before. Editor: `tsc` clean; the model's
economy check takes the new keys (`content.test.mjs`); `economy.test.ts` (four percents typed in, the factor by day as
Core reckons it, a change sent only when it changes something, the comforts' text); `npm test` fails only the same
Greyfen count.

**Short runs** (build 49's export, ridgemere2's jobs by class merged; `--no-check --deterministic --threads 4
--no-profile`; the coordinator runs the long ones). Three days from Spring 1, and one day near each season's middle on the same weekday and off the festival
(`--day 44, 142, 226, 324`; the middles themselves, days 46 of Autumn and Winter, are festivals, and a day 140 Restday):
nobody hungry, every household fed, money conserved every day.

| | Spring 44 | Summer 142 | Autumn 226 | Winter 324 |
|---|---|---|---|---|
| grain supply / price | .85 / 1.10 | 1.16 / .95 | 1.39 / .85 | .75 / 1.15 |
| grain drawn of cap | 121 of 127 | 141 of 174 | 134 of 208 | **113 of 113** |
| grain sold (bread, porridge) | 430 | 499 | 502 | 412 |
| fresh fruit drawn of cap | 436 of 470 | 370 of 1,243 | 440 of 1,417 | **293 of 293** |
| fresh fish supply / price, sold | 1.01 / 1.00, 160 | .86 / 1.10, 117 | .99 / 1.00, 167 | 1.38 / .86, **190** |
| fresh fish paid a piece | 2.00p | 2.20p | 2.00p | 1.76p |
| honey drawn of cap | 58 of 59 | 55 of 89 | 53 of 86 | **45 of 45** |
| preserves: shelf factor, sold, on shelves | 1.01, 142, 38 | 1.26, 93, 51 | 2.45, 153, 39 | 1.48, 150, 6 |

- The factors move with the season and the caps bind in the lean ones (grain, fruit and honey in winter, fruit and honey
  in spring), not in the plentiful ones. More bread is eaten in summer and autumn, and the most fresh fish in winter, by
  the residents' own choice.
- The price paid follows the factor but lags it in a one-day run (grain 1.06p in winter against 1.15): each payer's
  carried fraction starts at half a penny and most buys are of 1p bread; over a run it comes to the factor.
- The preserver's stock-up shows only over a run from autumn into winter (a one-day run starting in winter starts with
  no autumn stock): its stock fell to 0 by day 3 of the three-day spring run, with fruit capped at 40% and the cider
  makers drawing it too. Watch `preserved_stock` across autumn and winter, and `fruit_drawn` against `fruit_cap`; the
  preserver may lack the crew's hours to stock up (doc 68's table: it was over capacity if everyone ate its make).
- Restdays and festivals move a day's spending more than the season does (a day-46 festival day drew a third of an
  ordinary day's): compare runs on the same weekday, or over weeks.

### The first simulations: supply and demand, cost and pay (2026-10-31)

The user, 2026-10-31 (verbatim): "start simple and small, and just make sure that the simulation works and things are
running, then go into figuring out supply and demand, and applying fixes in rulesets that can be managed or tweaked from
DM. We're trying to find a good supply/demand balance along with cost/pay."

Branch `r2-econ-diag`, from `ridgemere2` (8729094). **Analysis and proposals only: no economy rule or setting is
changed**; the fixes below are for the user to choose. Build 50's export (`artifacts/economy/r2/export50`, main repo),
from Spring 1, no player, `--no-check --deterministic --no-profile` (doc 69, 10).

#### The instrument

`world_check ... --economy FILE --economy-detail FILE2` (new, committed) writes a second JSON line a game day, from the
ledger (every coin is a `shift` or a `sale`, and `World::setEventWatcher` sees each one; only read):

- **per business**: its trade, title, keeper, crew, wages owed a day, till, stock (pieces and catalogue value), and what it
  took in and paid out by kind: sales to residents (`residents`) and to the Country (`country`), store draws (`draws`, the
  Country's and the toll's parts), carriage, wages, profit;
- **per household** (`Society::households()`, made public for it): its posts (keeper, hand, public, watch, country,
  unpaid or none), members and adults, cash at 23:00, members hungry now and hungry member-hours that day, income by kind
  (wages by payer, profit, alms, inheritance...), spending on food and on other goods;
- **the treasury, the Country and `fund:carriage`**: in and out by kind.
- `RATW_DETAIL_TRACE=HOME,...` adds those households hour by hour (each member's hunger, cash, food and task).

Checks: the `--economy` file is byte-identical with and without the flag (60 days compared, and the same as the first
60-day run's); every till's and household's cash moves exactly by its recorded flows, every day of every run
(`tools/econ_detail.py --check`: 0 mismatches). `tools/econ_detail.py` prints per trade, households by class (the class
of the building they live in, doc 67's rule, from `artifacts/ridgemere2/buildings.json`), the hungriest households, the
treasury and the Country; `tools/econ_weeks.py` averages economy files by week and summarises runs against section 10's
gates. A detailed run costs little more (60 days in about 75 s; the file is about 100 KB a day).

#### The runs

| Run | Days | What | Files (main repo, `artifacts/economy/r2/diag_*`) |
|---|---|---|---|
| base60d | 60 | the 60-day baseline again, with detail | `diag_base60d*.jsonl` |
| year | 365 | a full year from Spring 1, with detail | `diag_year*.jsonl` |
| trace50 | 50 | four hungry households hour by hour | `diag_trace50_detail.jsonl` |
| w_* | 60 each | 13 what-ifs (A-M), one change each (below) | `diag_w_*.jsonl` |
| y_set1-3, y_a1-a4 | 365 each | the combined sets and what each part does | `diag_y_*.jsonl` |

Settings that exist were changed in a copy of the export (a `loop`, `comforts`, `season` or `store` record after its
`store` line); rules that don't exist yet were tried in a **throwaway prototype build** (a copy of the source in the
session's scratch folder, each rule behind an environment variable, off by default: with all of them off it gives the
baseline byte for byte; its patches are kept, uncommitted, in the main repo's `artifacts/economy/r2/prototype/`). Nothing
of the prototype is committed. The summaries of every run are committed in `artifacts/economy/r2/diag-summary.txt`; the
big jsonl stay in the main repo's ignored `artifacts/economy/r2/`. The prototype with its rules off was run too
(`diag_w_protobase`): identical to the baseline.

#### 1. The 60-day baseline, again

Money conserved every day; 2 births, 2 deaths; nobody newly placed. By ten-day blocks:

| Days | Hungry (mean) | Worst hour (mean) | Households fed | Residents spent | Country spent | Wages paid of 4,068 | Tills short | Profit | Alms | Treasury | Purses | Gini |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0-9 | 0.2 | 0.5 | 243 | 4,636 | 2,442 | 3,590 | 23 | 763 | 179 | 4,037 | 34,801 | .54 |
| 10-19 | 1.6 | 3.1 | 241 | 4,757 | 2,680 | 3,321 | 42 | 741 | 404 | 5,723 | 31,862 | .58 |
| 20-29 | 2.2 | 4.1 | 239 | 4,781 | 2,627 | 3,305 | 42 | 1,017 | 543 | 5,708 | 32,649 | .62 |
| 30-39 | 5.4 | 7.8 | 237 | 4,868 | 2,864 | 3,289 | 43 | 826 | 592 | 6,345 | 30,933 | .63 |
| 40-49 | 10 | 14 | 231 | 4,633 | 2,508 | 3,276 | 43 | 923 | 556 | 6,004 | 31,941 | .65 |
| 50-59 | 6.6 | 9.4 | 233 | 4,711 | 2,898 | 3,342 | 41 | 653 | 601 | 6,682 | 30,656 | .66 |

(Treasury, purses and Gini at each block's end; the rest a day.)

#### 2. The year

The full year from Spring 1 (`diag_year.jsonl`; 11 born, 4 newcomers, 15 died; money conserved every day):

| Days | Season | Hungry (mean) | Households fed | Wages paid | Alms | Treasury | Purses | Gini |
|---|---|---|---|---|---|---|---|---|
| 0 | spring | 0 | 243 of 243 | 3,858 | 22 | 1,181 | 36,592 | .49 |
| 60 | spring | 6.3 | 230 | 3,362 | 587 | 6,610 | 30,839 | .66 |
| 120 | summer | 2.0 | 231 | 3,372 | 607 | 2,219 | 35,986 | .68 |
| 150 | summer | 8.6 | 223 | 3,253 | 708 | 329 | 37,595 | .68 |
| 210 | autumn | 32 | 208 | 3,395 | 7 | 226 | 39,256 | .73 |
| 270 | autumn | 4.4 | 225 | 3,192 | 537 | 220 | 38,864 | .75 |
| 330 | winter | 53 | 189 | 3,238 | 712 | 219 | 39,678 | .79 |
| 364 | winter | 46 | 195 | 3,316 | 199 | 213 | 39,877 | .82 |

Hungry by season (the mean of the hourly samples): spring 5.4, summer 5.8, **autumn 21, winter 35** (6% of 559). The
treasury rose to 6,700p by day 60, then the alms drained it: it reached its 200p float on day ~126 and stayed there, the
alms became whatever was left after the public wages (7p one day, 700p another), and hunger climbed. Meanwhile the
**merchants' purses rose from 14,190p (day 60) to 25,705p with a median of 19p**: a handful of keepers hold most of the
money, and the Gini of earning households ran .49 to .82.

#### 3. Why hunger rises

Hunger is 199 member-hours a day over days 40-59 (51 households ever hungry), 989 a day over days 300-364 (133
households). Four causes, in the order they matter:

1. **Families whose wage is never paid.** The hungriest households are keepers' and hands' of businesses that sell
   nothing or almost nothing (the Soundings cartographer, the Greatly Bank's moneychanger, the Clearwater glassblower,
   the Harness Shop's saddler, the Pawn and Ledger, the Cedar Phial's perfumer, the Council Press): the till
   pays its posts nothing once it is down to its float, so a family of three or four lives on the alms. Middle-class
   keepers' households carry 44% of the hungry hours in days 40-59, though they are the richest class on average (cash 299p):
   the class is split between the rich (food, provisions, the general store) and the ruined.
2. **The alms are a day's money with no reserve.** A household holding under 7p a head at 18:00 is topped up to 7p a
   head; it spends it the next day. A day's food is about 6.2p a head at the prices paid, so the alms just cover a day,
   and on a Restday or festival (when the household doesn't go to market, or its shops are shut) or when the pick goes
   wrong (below), the household has nothing in hand. The trace: Sybil Pickering, an orphan at lessons living alone, holds
   7p, buys one 6p meal at her 17:00 market hour, eats it, and is at hunger 100 the rest of the time, every day.
3. **Within a household, food goes to the first members, not the hungriest.** At market hour each member in turn is
   topped up to two days of food; the money runs out before the last. In the Soundings flat Ilse and Imogen hold 100+
   nourishment each while Yrsa, the hand, holds 1p and nothing to eat, at hunger 100 for days; in Mallory House Cressida
   (a public wage) holds 88-118 while Dirk (an unpaid hand) sits at 84-100. The money for Yrsa's food was in the house.
4. **A pick the household can't pay for buys nothing.** The food is picked at random, weighted by nourishment for the
   price, among everything the open shops hold; if the pick costs more than any member has (a 12p roast with 7p), the
   purchase stops for that hour. A household whose market hour is 17:00 gets one try a day.

Behind all four: **the treasury runs dry** over a year (below), so the alms stop.

#### 4. Why the treasury hoards, then runs dry

- Its income is the 30% gate toll on draws, **about 1,480p a day** (days 40-59); its posts cost **872p** (the 134 public
  and watch posts as held now, not the 1,072p planned; some are vacant) and the carriage subsidy 33p. So it gains about
  575p a day before alms; **the alms grew from 22p to about 600p** as unpaid families ran out of savings, and by day 40
  the treasury was level, by day 60 falling.
- Over the year the toll falls with trade (draws 4,500-5,000 a day in spring, 3,900-4,400 from autumn) while the alms
  keep growing: from day ~60 the treasury loses 100-300p a day and is empty by day ~126.
- **Where the money goes**: the treasury's 6,000p went as alms to the poor, who spent it on food at the shops, whose
  tills paid it out as profit to a few keepers, who don't spend it (5). The treasury is not hoarding at the end of the
  year; the keepers are.

#### 5. Tills, profit and hoarding

**Which trades can't cover their wages** (days 40-59, pennies a day, all the trade's tills; value added = sales to
residents and the Country less draws and carriage; cover = value added / wages owed):

| Trade (tills) | Residents | Country | Value added | Wages owed | Paid | Cover | Why |
|---|---|---|---|---|---|---|---|
| moneychanger, shipwright, stables, builders' yard, barber, pawnbroker (6) | 0-2 | 0 | 0 | 180 | 1 | 0 | nothing anyone buys |
| cartographer, perfumer (2) | 0 | 0 | 0 | 44 | 0 | 0 | **can make nothing**: every craft takes longer than the two hours a lone crew may bank |
| saddlery, press, herbalist, armory (4) | 0-4 | 6-22 | 0-6 | 80 | 7 | .01-.25 | few buyers; most crafts too long for the crew |
| inns (15) | 411 | 196 | 113 | 290 | 114 | .39 | many tills, thin margins (resale 25%, porridge and stew) |
| dairy (4) | 221 | 0 | 33 | 80 | 33 | .41 | cheese from milk at full price: a thin margin |
| printing house, apothecary, glassworks, preserver, ropewalk, brewery, cooperage, distillery, builders' works, charcoal (16) | | | | | | .43-.73 | margins under their crews' wages |
| brickworks, smithy, sawmill, mason, butcher (12) | | | | | | .84-1.02 | about even |
| bakery, provisioner, fishmonger, carpenter, chandlery, foundry, cartwright, weaver, tailor... (30) | | | | | | 1.1-2.3 | comfortable |
| general store (1) | 348 | 12 | 99 | 22 | 14 | 4.5 | rich, but short of wages 40% of days (the float, below) |
| **All 110** | **4,672** | **2,703** | **2,362** | **2,468** | **1,711** | **.96** | |

- **The shops as a whole add less value than their wage bill** (2,362p against 2,468p), and 788p a day of it leaves as
  profit, so 1,711p is paid (69%). Doc 68's own arithmetic had this gap (2,376p against 2,468p, section 1).
- **The work bank** (doc 68, 7: "hours left over carry to the next hour, up to two hours' worth"): a craft longer than
  twice the crew can never be made. Four businesses can make **none** of their goods (the cartographer's 13.9-hour map,
  the perfumer's 6-16-hour oils and soap, the jeweller's 2.9-8-hour charms and rings for a crew of one (it lives on what
  it resells), the Clearwater glassblower's 4.2-7.4-hour glass for two), and 30 more can't make some (the smiths' swords and farm gear, the butchers'
  pig, the provisioners' jerky, the brewers' wine, the armoury's arms). They sell their starting stock and stop.
- **The float** (section 4: a till keeps back 20p or what it drew yesterday, if more): a reseller that draws 300-500p a
  day holds all its takings as float and pays nothing above it. The general store, with value added four and a half times
  its wages, is short of them 40% of days; the vintner and the rag-and-bone yard likewise.
- **Lumpy profit** is the Restday: on a Restday few sell and few draw, so the next day's shelves (1.25 times yesterday's
  sales) are low and its float (yesterday's draws) is small; the day after a Restday the tills take in the catch-up
  shopping, draw little, and pay everything over their float and a week's wages out as profit: **1,600-2,400p on each day
  after a Restday against 450-900p on other days**. The rest of the week they are short of working money again.
- **Hoarding**: the profit goes to 51 of 110 tills, half of it to ten (the general store 90p a day, the carters 71p, two
  chandlers, two provisioners, a fishmonger, the foundry, a carpenter). The comforts rule (a household over 60p a head
  spends a tenth of the excess a day) **stops at 20 purchases a day** of 1-6p goods, so a keeper holding thousands spends
  at most 60-100p a day and the rest piles up. This is doc 65's "money settles where it isn't spent" again, through
  profit rather than wages.

#### 6. The Restday and festival dips

Every seventh day (days 6, 13, 20...) residents spend 2,700-3,000p against 4,800p, and on the festivals (Spring's middle,
day 46 of the year) 2,100p; 62 households buy no food on a Restday and 106 on a festival day. The tills selling are about
the same (72 against 75): most households simply have a day's food and don't need to shop, and buy again the next day
(5,800-6,100p). The Country's makers are fewer at work (57 against 64-70), so it spends 520-1,000p and holds 900-1,400p,
spent over the next days: it is a dip, not a leak. What it does harm: the next day's profit flush (5), and the households
with no food reserve (3).

#### 7. The Carters' Yard

It takes 133p a day of carriage (76p in, 23p out, 33p the treasury's subsidy), pays its keeper and six drivers 62p, and
pays the rest, **71p a day, to Orm Carlisle as profit**; its till sits at its cap, 454p (20p + 7 × 62p), every day. So the
yard is sound, and the treasury's 33p a day of subsidy goes to its keeper's profit rather than to cheaper carriage.

#### 8. The Country's spending

It takes in about 3,456p a day of draws less the toll and spends 2,703p on the city's goods, 728p on its posts' wages and
23p on carriage: level (470-500p at 18:00, its reserve 200p; it holds 900-1,400p only on Restdays). Its money reaches the
smithies (387p a day), the carpenters (190p), the inns (196p, drink), the brewers (171p), the foundry, the brickworks,
the mason, the provisioners, the ironworks, the cartwright and the fishmongers (98-133p each); none of it reaches the
trades with nothing it buys (the moneychanger, the shipwright, the stables, the barber, the builders' yard, the
pawnbroker), nor the four that can make nothing. It is working as designed; its basket can only buy what is made.

#### 9. The seasons over the year

By season (means a day, the base year; set 2 and 3 below behave the same way):

| | Spring | Summer | Autumn | Winter |
|---|---|---|---|---|
| bread and porridge sold (grain) | 468 | 546 | 565 | 473 |
| grain drawn of the Country's cap | 123 of 131 | 138 of 171 | 139 of 192 | **118 of 126** |
| fresh fish sold (price paid) | 109 (1.99p) | 92 (2.15p) | 93 (1.98p) | **125 (1.78p)** |
| fresh fruit drawn of cap (all by makers: residents eat none fresh) | 459 of 550 | 451 of 1,166 | 413 of 1,255 | **374 of 447** |
| preserves sold | 97 | 107 | 101 | 92 |
| preserves on the shelves (most) | 49 (183) | 164 (320) | 269 (365) | 81 (296) |
| meat sold (no season) | 89 | 87 | 83 | 94 |

- **More bread in summer and autumn, more fresh fish in winter, livestock level**: as the user asked, by the residents'
  own choice of food for the price.
- **Preserved fruit is not eaten more in winter**: the stock-up works (the shelves hold 270 on average in autumn, at most
  365) but it is three days' sales, not a winter's (90 days at about 100 a day is 9,000 pieces), and in winter fresh fruit
  is capped (30% supply) and shared with the cider makers, so the preservers can make little. A what-if with the autumn
  shelf at 10 times and winter's price at 80% (`y_set3`) stocked up to 1,039 pieces but still sold out within about ten days
  of winter (winter sales 46 a day against autumn's 126).
- Hunger was worst in autumn and winter in the base year because the treasury was empty by then, not because of the
  seasons: in the fixed runs (12) hunger is near nil in every season.

#### 10. Cost and pay, by class

A day, by the class of the building a household lives in (doc 67), days 40-59 of the baseline: wages as posted and as
paid, food at the prices paid, other spending (household needs and comforts):

| Class | Households | Size | Earners | Wages posted | Paid | Profit | Alms | Food | Food a head | Needs and comforts | Cash | Hungry (share of member-hours) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| lower | 112 | 2.30 | 1.96 | 16.7 | 14.4 | 0.8 | 2.0 | 14.3 | 6.2 | 3.0 | 96 | 0.9% |
| middle | 106 | 2.45 | 1.68 | 18.5 | 14.1 | 6.6 | 2.8 | 16.1 | 6.6 | 7.1 | 189 | 2.1% |
| upper | 7 | 3.58 | 1.72 | 16.3 | 13.7 | 0.4 | 6.3 | 21.2 | 5.9 | 0.5 | 54 | 2.1% |
| the Watch's barracks | 15 | 1.07 | 1.01 | 8.1 | 7.3 | 0 | 0.8 | 6.8 | 6.4 | 1.2 | 17 | 0.1% |

- **A day's food is about 6.2p a head, needs about 1.3p**: 7.5p a head. A hand's or a public post's 8p keeps one; a family
  of 2.3 needs 17p, so **a lower household needs two earners**, and has 1.96 on average: it breaks even (income 17.2p,
  spending 17.3p) with nothing over, and a lost wage is hunger within days.
- **Households that can't feed themselves even when every wage is paid** (posted wages under 7.5p a head; day 40): 17
  lower, 42 middle (a keeper's 14p and children), 6 upper; and 34 with no wage at all (the retired, orphans, the
  displaced: alms only).
- **The upper class is poor in this loop**: the Minor Houses' family members hold posts no wage rule pays ("lending",
  "overseeing the Grayrock wharf", "at leisure"; the Queen and the King's daughter), so a House household of four lives
  on one public wage and the alms; over days 300-364 its members were hungry 21% of the time. Only Orm Carlisle (the
  Carters' Yard) has a House income. Also unpaid: "running the City Office" (no rule word catches it).
- **The middle class splits**: shopkeepers of the profitable trades take 6.6p a day of profit on average (concentrated in
  a few), while keepers of the trades in 5 have nothing.

#### 11. What-ifs, 60 days each

One change each, days 5-59 (base: `base60d`). Wages = wages paid of wages owed; treasury and Gini start to end.

| What-if | Rule or setting | Hungry (mean) | Worst | Fed (lowest) | Wages | Tills short | Profit | Alms | Treasury | Gini |
|---|---|---|---|---|---|---|---|---|---|---|
| base | | 4.70 | 21 | 92.2% | 81.8% | 40.8 | 824 | 514 | 3,448 → 6,682 | .50 → .66 |
| A. the work bank holds a business's longest craft | prototype | 3.24 | 16 | 94.2% | 84.0% | 37.0 | 803 | 457 | → 7,352 | → .64 |
| B. fair food in a household | prototype | **1.92** | 15 | 95.5% | 81.9% | 40.5 | 823 | 529 | → 6,161 | → .65 |
| C. alms 10p a head (not 7p) | prototype (`AlmsADay` is a constant) | **0.15** | 3 | **98.8%** | 82.5% | 39.8 | 907 | 639 | 3,043 → 2,316 | → .64 |
| D. treasury over 2,000p pays short tills' wages | prototype | 3.31 | 16 | 93.8% | **87.6%** | 39.2 | 910 | 441 | 1,998 → 1,490 | → .66 |
| E. treasury over 2,000p buys the Country's basket | prototype | 4.30 | 21 | 94.2% | 83.2% | 37.7 | 1,010 | 490 | 1,904 → 1,547 | → .67 |
| F. the float a 7-day mean of draws | prototype | 4.49 | 17 | 93.4% | 82.5% | 41.6 | 773 | 507 | → 7,330 | → .66 |
| G. shelves from a 7-day mean of sales too | prototype | 4.00 | 17 | 93.8% | 81.9% | 41.4 | 813 | 515 | → 6,420 | → .65 |
| H. profit cap 3 days, half to the hands | prototype | 3.49 | 17 | 94.2% | 79.7% | 46.9 | 997 | 493 | → 10,089 | → .62 |
| I. profit tax 30% to the treasury | prototype | 4.69 | 19 | 93.0% | 81.1% | 42.6 | 500 | 520 | → 13,896 | → .62 |
| J. comforts above 40p, a fifth, no 20-a-day limit | setting + prototype | 4.35 | 19 | 93.0% | 82.9% | 38.2 | 1,074 | 515 | → 16,959 | .45 → .59 |
| K. gate toll 27% | setting | 4.00 | 21 | 93.4% | 83.0% | 38.6 | 927 | 496 | 2,829 → 2,632 | → .66 |
| L. resale markup 65% (not 75%) | setting | 4.06 | 19 | 93.0% | 81.9% | 41.1 | 943 | 515 | 3,164 → 4,191 | → .68 |
| M. markups making 90%, resale 70% | setting | 4.15 | 21 | 91.8% | 85.0% | 35.3 | 949 | 481 | 2,705 → 2,311 | → .67 |

- **Hunger answers to the household rules** (B, C): ten pennies of alms a head ends it in 60 days; fair food halves it.
- **Wages answer to support** (D) and to making (A): the treasury paying short tills raises wages to 88%; letting long
  crafts be made brings the cartographer, perfumer, jeweller and glassblowers back (cover .58-1.60) and frees 60p a day of
  alms.
- **The treasury needs an outlet**: alone, a profit tax (I) or more spending (J) only piles it higher (13,900p,
  17,000p); the toll at 27% (K) holds it level over 60 days (the doc's earlier proposal 1, confirmed), but not over a year
  once the alms grow.
- **Markups** (L, M) move money from the Country and the toll to the tills: a few points of wages, more profit.
- **Not worth it alone**: the 7-day float (F) and shelves (G) smooth the post-Restday flush a little (2,377p → 1,562p)
  and change little else; profit shared with hands on a 3-day cap (H) starves tills of working money (wages 80%).

#### 12. The combined sets, a year each

| Year run | Rules | Hungry (mean) | Worst hour | Fed (lowest) | Wages | Tills short | Profit | Alms | Treasury (day 5 → 364) | Purses | Gini (day 5 → 364) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| year (base) | | 16.9 (3.0%) | 109 | 72.5% | 80.2% | 42.9 | 697 | 488 | 3,448 → 213 | 34,329 → 39,877 | .50 → .82 |
| set 1 | A + B + C + D | 15.8 (2.8%) | 81 | 82.6% | 83.0% | 36.9 | 804 | 505 | 1,741 → 214 | 36,083 → 38,590 | .48 → .78 |
| **set 2** | **set 1 + J** | **0.07 (0.0%)** | 56 | 89.6% | **99.7%** | 28.5 | 1,523 | 388 | 4,380 → 3,760 | 31,108 → 31,498 | **.42 → .59** |
| set 3 | set 2 + the preserves' season (9) | 0.09 | 52 | 88.3% | 99.8% | 28.7 | 1,519 | 384 | 4,333 → 3,029 | 31,077 → 31,846 | .43 → .58 |
| a4 | J's setting only (40p, 20%; the 20-a-day limit kept) | 9.81 (1.8%) | 111 | 71.9% | 81.7% | 40.7 | 791 | 519 | 6,026 → 214 | 29,591 → 39,765 | .45 → .85 |
| a1 | J alone (no limit) | 4.10 (0.7%) | 65 | 82.9% | 82.8% | 38.6 | 920 | 533 | 6,239 → **18,351** | 29,385 → 19,590 | .45 → .62 |
| a2 | J + B + C | 0.08 | 49 | 90.8% | 83.7% | 36.6 | 1,102 | 654 | 5,881 → 14,241 | 29,593 → 22,982 | .44 → .62 |
| a3 | J + B + C + A (set 2 without D) | 0.09 | 56 | 89.9% | 86.1% | 32.1 | 1,082 | 577 | 5,766 → 14,251 | 29,588 → 23,056 | .43 → .59 |

What each part does (a1-a4, the same year):

- **The comforts setting alone is not enough** (a4): with the 20-a-day limit kept, 40p and 20% change nothing that
  lasts. The limit is what binds.
- **Without the limit (a1)** the keepers' hoard comes back to the shops, but the treasury takes it in as toll and piles
  it up (18,351p by year's end, a third of the money): the hoard moves from the keepers to the treasury.
- **Fair food and alms 10p (a2) end hunger**, the work bank (a3) adds 2-3 points of wages, and **the treasury's cap paying
  the short tills (set 2) brings the pile back as wages**: 99.7% paid, the treasury level.

- **Set 2 holds for a year**: somebody was hungry on only 5 of 365 days, one resident on three of them; the other two
  are the Autumn and Winter festivals (run days 231 and 322), when 56 and 40 were hungry for some hours (households that
  don't shop on a festival day and have no reserve). Every wage paid; Gini of earning households .42-.59 (.47-.55 by
  four-week means); the treasury between 1,405p and 5,543p with no trend; money conserved. Its
  treasury takes 1,786p a day of toll and pays 871p of wages, 488p of wage support, 388p of alms and 40p of subsidy.
- **Without the comforts' spend-back (set 1) the year fails as the base does**: the profit piles up with the keepers,
  the treasury empties by summer and hunger returns. The spend-back is what keeps the money moving; the household rules
  then keep the poor fed and the wage support keeps the poor trades' families paid.
- What still doesn't cover itself in set 2: the six trades with nothing to sell, the dairies (.37), the inns (.58), the
  saddlery (.68), the cooperage and the preservers (.83-.85): the treasury's wage support pays their posts (488p a day).

#### 13. Proposals: rules the DM can tune

Ranked by what they do for the user's three tests (hunger rare; the poor house and feed their families; shops and works
open). Each: what it changes, its setting and default, the effect expected, and what was measured.

1. **Comforts that actually spend savings** (anti-hoarding). *Change*: drop the 20-purchases-a-day limit (a household
   buys comforts until its day's budget is spent) and lower the threshold. *Settings*: `comfortsAbove` and
   `comfortsShare` exist (DM, Economy tab); new `comfortsMost` (purchases a day; default none). *Proposed defaults*: above
   **40p** a head, share **20%**. *Measured*: the settings alone with the limit kept do nothing over a year (a4); without
   the limit, over 60 days Gini .66 → .59 and spending +9% (J), and over a year hunger 16.9 → 4.1 and Gini .82 → .62, but
   the treasury piles up the toll (a1); with the household rules and the treasury's cap the whole year holds (set 2).
   **The single biggest lever**, and it needs (4) beside it.
2. **Fair food in a household.** *Change*: at market hour the members with least food first, everyone to `shortDays`
   (1 day) before anyone to `foodDays` (2), and the food picked among what some member can pay for. *Setting*: none
   needed (a rule fix); optionally `food.fair` on/off in country.json. *Measured*: hunger 4.7 → 1.9 over 60 days (B), and
   part of set 2.
3. **Alms as a setting, and 10p a head.** *Change*: `AlmsADay` (7p, a constant) becomes `almsADay`, DM-tunable;
   **10p**, so a household on alms keeps a day in hand for Restdays and festivals. *Measured*: hunger 4.7 → 0.15 over 60
   days (C) at 639p a day of alms (+125p); over a year it needs (4) and (1) to pay for it.
4. **The treasury's cap: what is over it pays the short tills' wages** (the user's "supplement areas that help
   businesses"). *Change*: at 18:00, after its own wages, the treasury pays from what it holds over `treasuryCap` what each
   till could not pay its posts, shared as the till shares. *Settings*: `treasuryCap` (default **2,000p**, about two and a
   half days of its posts and alms), and which trades it supports (`subsidies.wages`, percent of the shortfall, by trade
   or all; default 100% for all). *Measured*: wages 82% → 88% over 60 days (D); 99.7% for a year in set 2 (488p a day),
   the treasury level. The alternative outlet, buying the Country's basket (E), raised profit more than wages.
5. **The work bank holds the longest craft.** *Change*: a crew's hours in hand keep to its business's longest craft plus
   an hour (not two hours'), so a lone cartographer makes a map in two days. *Setting*: `workBankHours` (default: the
   longest craft; 2 is today's). *Measured*: four dead businesses come back, wages +2 points, alms -57p (A). A fix as much
   as a setting.
6. **The float: what a till keeps back.** *Change*: the float is the smaller of yesterday's draws and a share of the
   till's takings, or a running mean (F); either way a busy reseller pays its wages before keeping a day's draws.
   *Setting*: `floatDays` (days of mean draws; default 1 now). *Measured*: the 7-day mean alone smooths the flush
   (2,377p → 1,562p) and +0.7 points of wages; the general store, vintner and rag-and-bone still short 40% of days in set
   2: worth a second look (a float of half a day's draws, say).
7. **Profit to the hands, and a profit tax**, only with an outlet. *Change*: a share of a till's profit to its hands
   (`profitHands`, percent) and to the treasury (`profitTax`, percent). *Measured*: neither helps alone (H, I); a tax
   pays for (3) and (4) once (1) is in. Not in the first set.
8. **The trades with nothing to sell.** The moneychanger, shipwright, stables, barber, builders' yard and pawnbroker sell
   nothing to residents or the Country. Choices for the user: (a) public services, their posts paid by the treasury (a
   wage rule: "the city's services", as the clergy are), (b) give them goods (the stables' hay and tack to the Country's
   basket, the builders' yard's materials to the treasury's works), or (c) fewer of them in the city. Set 2 pays them
   through (4), about 180p a day.
9. **Wages that feed a family**, by class or trade. *Settings*: the four wages exist (keeper 14, hand 8, public 8,
   Country 8). A lower household needs two 8p earners; the households short even when every wage is paid (65 of 243)
   are mostly middle keepers with children. Not tested: raising wages without (1) and (4) only moves more money into
   tills that can't pay it. Revisit after the first set.
10. **The Minor Houses' income.** Their family members' posts ("lending", "overseeing the Grayrock wharf", the Queen, the
   King's daughter) and "running the City Office" have no wage rule: add rule words (the Country's estates for the
   Houses, the treasury for the City Office). A data change in `country.json`'s `wages.rules`.
11. **Festivals and Restdays.** A household going into a Restday or a festival buys a day more (`food.daysBeforeRest`),
   or food shops keep their hours on those days. The only hunger left in set 2 is the two festivals.
12. **The gate toll at 27%** (setting, exists): holds the treasury level over 60 days (K) without the cap; with (4) the
   cap does it, so the toll can stay at 30%.
13. **The Carters' Yard**: four more drivers (the loads want ten; it has six), or the carriage subsidy to 0% (it pays Orm
   Carlisle's profit, 71p a day, not cheaper carriage). Settings exist (`subsidies.carriage`, `carriage`).
14. **Winter preserves.** The shelf stock-up can't carry a winter. Either the Country's stores send preserved fruit in
   winter (a `supply` for the preserved group with a `normal`, and the provisioners reselling it), or a reserve of the
   autumn's fruit for the preservers (a season group's cap kept for makers of a list). Needs a small rule; not tested
   beyond (9).
15. **Inns and dairies**: 15 inns and 4 dairies can't cover their wages at resale 25% and cheese at full cost. Either
   fewer tills, or the store's markups (`store` making 100%, resale 75%; settings exist). Measured over 60 days: resale
   at 65% (L) changes little (the resellers' extra margin goes to profit); making 90% and resale 70% (M) raises wages to
   85% and cuts the tills short from 41 to 35, at the cost of a fifth less toll. A cheap second step after the first set.

**The first set to try together** (set 2, measured for a year): **1** (comforts 40p, 20%, no daily limit), **2** (fair
food), **3** (alms 10p), **4** (treasury cap 2,000p paying short tills' wages) and **5** (the work bank). Then **10**
(wage rule words), **11** (festivals) and **6** (the float) as small follow-ups, and **8** and **15** as the user's
choices about the city's trades.

### First fixes (2026-10-31)

Branch `r2-econ-fix1`, from `ridgemere2` (f9ef617), with `ridgemere2` merged at 8c7692c (a few inns, dairies later). The
first set of section 13 (set 2: proposals 1-5), the wage rule words (10) and the user's decision on the trades that sell
nothing (8), each a rule with a setting the DM can tune, its default as set 2 measured. The diagnosis's prototype (its
patches in the main repo's `artifacts/economy/r2/prototype/`) was the starting point; nothing of it is behind an
environment variable now.

The user, 2026-10-31 (verbatim), on the moneychanger, shipwright, stables, barber, builders' yard and pawnbroker: "Make
them public services that NPC's have the occasional need to purchase from, but make it items that NPC's buy when they
have excess money that isn't needed for other things. The money goes partly to the shop owner and to the city."

#### The rules

`Data/Items/country.json`'s new `rules` (the defaults; Core `items::Rules`, its range and a fallback each), a world's own in
`live.economy.rules` (migration 0058) and its `rule "<name>" N` records; `Society::rule(name)`:

| Rule | Setting | Default | Was | What it does |
|---|---|---|---|---|
| 1. Comforts that spend savings | `comfortsMost` (0-100,000), with `comforts.above` and `share` | 200 a day; above **40p** a head, share **20%** | 20 a day; 60p, 10% | A household buys comforts until its day's budget is spent, not stopping at 20 purchases. |
| 2. Fair food in a household | `fairFood` (0/1) | on | (the old order) | At market the members with least food buy first, everyone to `food.shortDays` (1 day) before anyone to `food.days` (2; that only at the household's market hour), and only foods some member can pay for are picked. |
| 3. Alms | `almsADay` (0-1,000) | **10p** a head | 7 (a constant, `AlmsADay`, gone) | After the day's wages a household holding less is made up to it, from what the treasury holds over its 200p float. |
| 4. The treasury's cap | `treasuryCap` (0-100,000,000), `wageSupport` (0-100%) | **2,000p**, 100% | none | At 18:00, after every payer has paid, what the treasury holds over the cap pays `wageSupport` percent of what each short till could not pay its posts, each post the same share of its wage; when it can't cover them all, every short till the same share of its shortfall (the prototype went till by till). Counted as `wage_support`, and in `wages_paid`. Only short tills: a till that pays its posts gets nothing. |
| 5. The work bank | `workBankHours` (0-1,000) | 0: the business's longest craft and an hour of the crew (at least two hours' worth) | 2 hours' worth | A bug fix: a lone cartographer's 13.9-hour map, the perfumer's oils, the jeweller's rings and the glassblowers' glass can be made (over two days). N > 0: N hours of the crew. `Society::workBankMost`. |
| 8. The city's services | `services` (0/1), `servicesCityShare` (0-100%), per service on/off, price and frequency | on, **30%** | none | Below. |

#### The city's services

Twelve catalogue items (`items.json`, category `service`, weight 0, source `service`), two for each of the six trades,
listed in `country.json`'s `services.list` with their business and how often (`every`: a household buys one about once
every so many days, a fixed roll of the home, the service and the day: nothing saved):

| Business | Service | Price | Every |
|---|---|---|---|
| barber | A groom and trim | 1p | 4 days |
| barber | A tooth drawn | 3p | 60 days |
| moneychanger | Coin changed | 1p | 7 days |
| moneychanger | A letter of credit | 5p | 30 days |
| pawnbroker | A pledge redeemed | 2p | 10 days |
| pawnbroker | A small loan's fee | 2p | 20 days |
| stables | A cart hired for a day | 4p | 20 days |
| stables | A night's stabling | 2p | 20 days |
| shipwright | A skiff patched | 4p | 20 days |
| shipwright | Oars mended | 2p | 20 days |
| builders' yard | A roof patched | 5p | 30 days |
| builders' yard | A wall limewashed | 3p | 20 days |

- **Only from what is left over.** At the household's market hour, after its food and its needs, the comforts budget
  (a fifth of what it holds over 40p a head) pays for the services first, each when its roll falls and the budget covers
  its price, then comforts with the rest. A household at or under 40p a head buys none.
- **From an open shop of its trade**, the household's community's or else any open one (a roll among them); paid by the
  member with most.
- **Done at once, never kept**: the till makes the item, sells it and the buyer uses it up in the same moment ("a service
  done", "a service", "a service, done at once"), so nothing piles up on a shelf or at home.
- **The split**: `servicesCityShare` percent of the price from the till to the treasury ("the city's share of a
  service"; the till's fraction of a penny kept until it makes one), the rest stays in the till, which pays its keeper
  and hands as any till does (keeper and hands: the posts in its room). The cap's wage support covers them only when the
  till is short.
- **A world's own**: `live.economy.services` (0058) and `service "<id>" on|off price N every DAYS` records; a service
  country.json no longer lists is kept and does nothing.

#### Wage rules: the upper posts

A wage rule may now say which of the world's four wages it pays (`wage`: keeper, hand, public or country; else its
payer's). Two new rules, first in the list:

- **The treasury, at a keeper's wage (14p)**: "the queen", "the king's daughter", "running the city office". The Crown's
  household is the treasury's already ("the king's household"), and the City Office is a public office; their heads are
  paid as heads, not clerks.
- **The Country (the Houses' estates), at a keeper's wage**: "lending" (Petra Vance) and "overseeing the grayrock wharf"
  (Aldric Grayrock). The Minor Houses' people are kept by their estates, as the Houses' rule already says; the Grayrock
  wharf is the House's, like the Wharf Office the Country pays.
- **No owner was left unpaid**: every House member who keeps a business (Orm Carlisle at the Carters' Yard) is its
  keeper, paid by its till, and takes its profit. The other upper posts in the city with homes are children ("at her
  lessons in the Hall") and the retired ("sits and watches the world"), who are fed by their households and the alms.
- The harbourmaster, the Counting House's overseer and the like stay at a public or Country wage (8p): a head's wage for
  them is the user's call.

Over the year the upper class's waged households take 21p a day in wages (13p of it the treasury's) and hold 115p; they
were hungry 21% of the time over the base year's last 65 days, and are not now.

#### The settings: the DM's Economy tab and Atlas's Economy page

- **Migration 0058** (`Database/migrations/0058_rules.sql`): `live.economy.rules` (`{name: N}`, `{}`: country.json's) and
  `services` (`{id: {on, price, every}}`, stored whole); `live.people_manifest` with a `rule` line a rule and a `service`
  line a service. `Game::withoutPeople` strips both. **Applied to DEV (`ratw_ridgemere2`) only**, 2026-10-31, after
  `Saved/Backups/ratw_ridgemere2-before-econ-fix1.dump` and with no server or other writer running: DEV reads the defaults
  (no records; its export is unchanged). PROD is untouched (at 0054).
- **Tools** (`tools/world_store.py`, `map_editor.py`): a tier for a world with 0057 but not 0058, whose rule or service
  settings are refused ("this world lacks migration 0058 (python3 tools/world_db.py migrate --db KEY): its rules and
  services stay as Data/Items/country.json has them"); its other settings still save. A project's export carries the
  records.
- **Routes** (`tools/studio.py`): `GET /api/dm/economy?world=KEY` gains `rules`, `services`, `ruleDefaults` (country.json's)
  and `serviceList` (each service with its name, price, business and frequency); `POST /api/dm/economy/rules` `{world,
  set: {name: N | null}}` and `/services` `{world, set: {id: {on?, price?, every?} | null}}` (null: back to
  country.json's), for dm and admin, audited (`economy.rules`, `economy.services`), each queueing `economy.sync`: the
  running server applies them at once (`adoptEconomy`). Atlas: `POST /api/atlas/economy/rules` and `/services`. A world
  without 0058: 409, naming it.
- **Editor**: `Editor/src/studio/RulesPanel.tsx`, in both the DM's Economy tab and Atlas's Economy page: the eight rules
  (empty: country.json's, shown as the placeholder; fair food and services as switches) and the services (on, price,
  every N days, "As country.json" a row).
- **`world_check --economy`** gains `wage_support`, `services_sold`, `services_spent`, `services_city` and per trade
  `svc_<business>_sold` and `_spent`; the detail file counts `services`, `services city` and `wages:support` apart.

**Tests.** `Tests/econ_fix_tests.cpp` (new, 549 checks, the real catalogue): the rules' defaults, records and refusals,
`withoutPeople`, `adoptEconomy`; fair food (a family of three with money for everyone's first day: every member a day's
food, where the old order gives the first two members two days' and the last none; only food someone can pay for, in a
hall of roasts); the alms at 10p and at a world's 13p a head; the cap's wage support (none under the cap, the till's whole
shortfall over it shared by wage, half at 50%, never into the cap, none to a till that pays); the work bank (maps made
with the default, none at two hours); the heads' posts and their payers; the services (bought by the household with
savings, never by one under its reserve, at the world's price, 30% to the treasury, 100% at a share of 100, none off or
when one is off, none ever kept); comforts no longer stopped at 20. Money conserved in each. `country_tests` changed
where the rules changed what it saw (a child given the household's alms "paid nothing" by the ledger now). All 72 C++
tests pass. Python: `test_world_store`, `test_studio`, `test_world_sync` (96, a real server applying a rule and a
service from `economy.sync`), `test_map_editor`, `test_world_build`, `test_dungeon_master`, `test_item_catalog`,
`test_city_capacity`, `test_world_db`, `test_live_edit` pass but for `test_map_editor`'s Greyfen resident count (11, not
10), as before. Editor: `tsc` clean, the lib tests 53 of 53, the model's 78 of 79 (the same Greyfen count).

#### The runs

DEV build 51 (the 5 inns, no dairies; its export, nothing written), from Spring 1, `--no-check --deterministic
--threads 4 --no-profile`, pinned four CPUs a run. Summaries committed in `artifacts/economy/r2/fix1-summary.txt`; the
jsonl in the main repo's `artifacts/economy/r2/fix1_*`. A year, days 5-364:

| Year | City | Hungry (mean) | Days anyone hungry | Worst hour | Fed (lowest) | Wages paid | Tills short | Profit | Alms | Wage support | Services (city's) | Treasury (5 → 364; low-high) | Gini (5 → 364) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| base (the old rules) | build 50 | 16.9 (3.0%) | 351 | 109 | 72.5% | 80.2% | 42.9 | 697 | 488 | - | - | 3,448 → 213 (200-6,725) | .50 → .82 |
| set 2 (the prototype) | build 50 | 0.07 | 5 | 56 | 89.6% | 99.7% | 28.5 | 1,523 | 388 | 488 | - | 4,380 → 3,760 (1,405-5,543) | .42 → .59 |
| the old rules | build 51 | 23.8 (4.3%) | 356 | 109 | 72.0% | 82.6% | 33.4 | 656 | 408 | - | - | 2,817 → 217 (200-4,440) | .50 → .81 |
| **first fixes (every default)** | build 51 | **0.09** | 5 | 70 | 89.7% | **99.6%** | 18.5 | 1,230 | 376 | 287 | 123p (37p) | 4,077 → 6,999 (2,534-7,972) | **.43 → .58** |
| the services off | build 51 | 0.08 | 5 | 59 | 88.8% | 99.6% | 18.2 | 1,380 | 375 | 353 | - | 4,040 → 5,012 (965-5,933) | .43 → .58 |

(Pennies a day but the treasury; 559 residents at the end of each; 11 born, 15 died; money conserved every day of every
run. Set 2's wage support is the doc's 488p, its file not having the field.)

- **The fixes hold the year as set 2 did.** On the new city the old rules fail as the base year did (hunger 23.8, the
  treasury empty by summer, Gini .81); with the fixes, somebody was hungry on 5 days of 360: the Autumn and Winter
  festivals (run days 231 and 322: 70 and 55 for some hours, households that don't shop on a festival with no reserve,
  as in set 2) and one resident on each of three other days. Wages 99.6% paid in every season, every household fed but
  on the festivals, Gini .47-.54 by four-week means.
- **The treasury**: no trend, a season's swing: it rises in spring and winter and falls in summer and autumn (by
  four-week blocks 3,638-7,203p; at the seasons' ends 6,424, 4,286, 4,401 and 6,999p). Its day: 1,583p of toll and 37p of
  the services in; 913p of its posts' wages, 376p of alms, 287p of wage support and 35p of the carriage subsidy out.
  It holds more than set 2's: fewer tills are short (18.5 against 28.5, the 5 inns and no dairies), so less support.
- **60 days** (days 5-59): nobody hungry at any hour, every household fed every day, wages 99.6%, Gini .43 → .53 (the base
  60 days: hunger 4.7, 52 days with someone hungry, wages 81.8%, Gini .50 → .66).
- **The six trades now sell**: 63 services a day, 123p, of which 37p the city's: the barber 20 a day (23p), the
  moneychanger 13 (21p), the pawnbroker 11 (23p), the stables 7 (20p), the shipwright 6 (18p), the builders' yard 5 (18p).
  Their tills pay 13-17p a day of their posts' 22p (a keeper and a hand) and 13p of the shipwright's and builders' yard's
  46p (a keeper and four hands); the cap's wage support pays the rest. What the services add (against the year with them
  off): the treasury's 37p a day and 66p less wage support (it ends 2,000p higher, and its low is 2,534p, not 965p); the
  money comes out of the comforts budget (comforts 373 a day against 432, profit 1,230 against 1,380), so the comfortable
  households' spending moves from the profitable shops' keepers to the six trades' posts and the city. Hunger, wages and
  Gini are the same.
- **The work bank**: the cartographer (cover 1.49), the perfumer (2.07), the jeweller (1.99) and the glassworks (1.27) now
  make their goods and pay their posts.
- **Still short of their wages** (cover = value added / wages owed, the year): the herbalist (.09), the armoury (.19), the
  saddlery (.53), the cooperage (.70), the apothecary (.87), the preservers (.88) and the 5 inns (.98, short on 29% of
  till-days; no hunger at inn hours). The general store, the vintner and the rag-and-bone yard are rich (cover 3-7) but
  short on a third of their days (the float, proposal 6).

#### Left for the user

- **The services' frequencies**: the defaults pay the barber's, moneychanger's, pawnbroker's and stables' posts about
  three quarters from their tills, the shipwright's and builders' yard's (four hands each) about a quarter; the DM can
  make them more often or dearer (the Economy tab), or the two yards could have fewer hands.
- **A head's wage** for the harbourmaster, the Counting House's overseer and the other heads of offices (8p now), as for
  the Queen and the City Office (14p).
- The smaller follow-ups of section 13: buying a day ahead before a Restday or festival (11: the only hunger left), the
  float (6: the general store, vintner and rag-and-bone short a third of days), the Carters' Yard (13: its keeper's 76p a
  day of profit is the treasury's subsidy), winter preserves (14), and the inns and preserver (the five inns cover .98,
  the preservers .88; city_loop.py puts them at 122% and 120% of their crews' hours). Wage support by trade (a share
  per trade rather than all at `wageSupport`) is not built.

### Trades: herbs to medicine, the armoury's patrons, office heads (2026-10-31)

Branch `r2-econ-trades`, from `ridgemere2` (6c72eba). The first fixes' year (above) left a few trades well short of their
wages (cover: the herbalist .09, the armoury .19, the saddlery .53, the cooperage .70, the apothecary .87) and the heads of
the city's offices on a clerk's 8p. The user's decisions, 2026-10-31 (verbatim where quoted):

1. **The saddlery**: "Saddlery doesn't make sense, nobody rides animals, they're all animals." Remove it; its people to
   posts of their class. The draught horses, the Stables and the Carters' Yard stay.
2. **The herbalist and the apothecary**, first as supplier and customer; then, before anything was run: "for the herbalist,
   that may be something we move to the country instead and just let the apothecary buy from it." Build the city-supplier
   mechanism only if the cooper needs it; keep the wolves moved to the fewest.
3. **Medicines**: households need the common ones, bought every so many days from the needs budget, more in winter where
   it makes sense, and the apothecary's shelves hold what players want (bandages, salves, stitch kits, splints, smelling
   salts, theriac as a rare dear one).
4. **The armoury**: "Armory should have houses and city as customers. Armory is providing what, though, just armor? They
   should be high cost items replaced occasionally, but repaired frequently which should be the main source of their
   business."
5. **The cooperage** (a barrel maker): keep it; the wine makers, fishmongers, salters or preservers buy their casks from
   it first if their goods use casks, or where it is natural to add, sparingly.
6. **Office heads** get a head's (keeper's) pay, 14p.

And five follow-ups from "First fixes", "Left for the user", each a rule the DM can tune: **buying ahead** before a Restday
or festival; **patrons for the shipwright and the builders' yard** (the wharf's hulls, the public buildings, the Houses'
halls; no wolves moved); **the float rule** (profit only after a float of a few days' wages); **the carters' subsidy** only
to what they lack, nothing to profit; **winter preserves**.

#### What was built

**1 and 2: no harness-maker, no herbalist** (doc 67, Progress, "No harness-maker, no herbalist"). Nobody moved home or
work place; three posts changed trade where they stand:

| Wolf | Was | Now |
|---|---|---|
| Halvard Ashley (keeper; lives over the shop) | saddler at The Harness Shop | apothecary at The Mortar and Pestle (the shop refitted and renamed) |
| Mogens Ashley (his family's helper, the same flat) | helping at The Harness Shop | helping at The Mortar and Pestle |
| Rupert Pitchford (a Market Square stall) | herbalist on Market Square | apothecary on Market Square (a remedy stall, the same spot) |

The city has three apothecaries now (Hollis Physic, keeper and hand; the Mortar and Pestle, keeper and helper; the remedy
stall), the same five posts and wages as the apothecary, the saddlery and the herbalist had. The herbs come from the
Country (the store room), as the apothecary drew them already; players buy herbs at the general store (Sump Sundries:
`crafts.json` `supplies.general` gains yarrow, willow bark, thyme, comfrey, valerian, chamomile, garlic, wormwood,
lavender and mint) and sell them to the apothecary as before (`buys`).

**3: the medicines.** `crafts.json` gains the apothecary's crafts (each a whole batch of at most two inputs, its `hours`
from `tools/econ_model/calibrate.py`'s ladder, as `apply_ladder.py` would give them; no other craft's hours changed):
willow-bark tea, wormwood flea powder, yarrow poultice and paw balm (the herbalist's simple remedies, the apothecary's
now too), thyme and honey syrup, comfrey salve, burn salve, garlic-honey salve, dewormer, eyewash, valerian draught,
charcoal draught, clove oil, leg splints (planks and bandages), stitching kits (gut and needles) and smelling salts;
bandages and scent-masking oil as before. **Theriac** is the Country's (an import, `supplies.apothecary`), one on the
shelf at a time (`country.json` `shelves`: a dear good keeps fewer than the usual four; `Society::startingStock`). So the
apothecaries' shelves hold all eighteen medicines of `items.json`.
The households' needs (`crafts.json` `households.needs`, after the eight there were, so their rolls are unchanged), from
the needs budget, kept back for food as needs are:

| Need | Every (days) | In winter |
|---|---|---|
| willow-bark tea (aches) | 60 | 30 |
| thyme and honey syrup (coughs) | 90 | 30 |
| paw balm (cracked pads) | 60 | 30 |
| wormwood flea powder | 60 | |
| dewormer | 120 | |
| garlic-honey or comfrey salve | 90 | |
| bandages (as before) | 20 | |

**4: the armoury's patrons**, and the follow-ups' shipwright and builders' yard: `country.json` `patrons.list`, each trade
with its patrons' groups (a payer and its units: the posts of `roles`, else a number `units`), and its patron services in
`services.list` (`for: "patrons"`: catalogue items of category service, done at once, never kept, and never a household's;
their prices and frequencies the DM's, as the household services are, in RulesPanel's table). Once a day at the first hour
the trade's shop is open, each unit has each service about once every `every` days (a fixed roll: nothing saved), paid by
its patron from what it can spare (the treasury over its float and tonight's posts; the Country over its reserve and its
posts), all to the shop's till (no city share). The armoury also sells each armed wolf a new piece about once every
`armourEvery` days (the rule, 120), one of its kit on the shelf at the catalogue's price (the quilted vest, padded collar,
quilted hood and leg wraps, leather cap, kettle helm, iron bit-sword, steel gorget, splinted greaves). `Society::patronsBuy`.

| Trade | Patron (payer) | Units | Work (price, every N days a unit) |
|---|---|---|---|
| armoury | the Watch (the treasury) | its guards' posts: **30** (the commander, two sergeants, two at the cells, four palace guards, ten at the gates, eleven on patrol and the wall) | a coat of armour mended (3p, 30), a dent beaten out (2p, 45), straps and buckles replaced (2p, 30), a blade ground and set (2p, 15); a new piece every 120 |
| armoury | the Houses' armed retainers (the Country: the Houses' estates) | **24** (none of the Houses' posts is armed: the retainers are the Country's people, a number until they are placed) | the same |
| shipwright | the wharf and sea carriage's hulls (the Country) | 12 | a hull caulked and tarred (6p, 6), a strake replaced (8p, 10), a spar or mast mended (5p, 5) |
| builders' yard | the city's public buildings (the treasury) | 20 | a wall repointed (4p, 10), a roof's slates relaid (6p, 15), a floor or stair mended (5p, 12) |
| builders' yard | the Houses' halls (the Country) | 9 | the same |

Sized so each covers its wages: the armoury's mending about .34p a wolf a day (18.6p for 54) and its new pieces about
.27p (14p), against its 22p; the shipwright's work 33.6p a day and the household services' 12.6p against its 46p; the
builders' yard's 35p and 12.6p against 46p.

**5: the cooper's casks.** No good in the catalogue used a cask. The user's own example, wine by the cask, sparingly:
`crafts.json`'s wine is 16 grapes and a cask for 8 bottles (it was 4 grapes for 2; the same 7.1 hours a 2 bottles,
28.4 a batch), so a cask costs a wine maker 1p a bottle of 12. Salt fish already has two inputs (fish and salt), the most a
craft may have, and the preservers' goods are jars and strings. The **city's suppliers** (`country.json` `suppliers`; the
rule `suppliers`, on): a keeper's craft input is bought first from a city maker of it that holds some, till to till at the
seller's price (the catalogue's, a dearer season's after; no toll, no carriage inside the walls), the seller with most
first, and from the store room only what the city lacks (`Society::buyFromCity`, in the restocking rule). Only craft
inputs: a good bought to sell on stays the store room's (at a shop's price it would leave the reseller nothing). The
brewery and the press buy their casks from the cooperage; later chains (the mill's flour for the bakers) are a line of
data.

**6: the office heads at 14p** (`country.json` `wages.rules`, the two keeper's-wage rules; a guard's post now takes a
treasury rule's wage where its words name it). Fifteen posts:

| Post | Payer |
|---|---|
| reigning at the Great Palace (the King) | treasury |
| chancellor of the Council; Chancellor of Ridgemere | treasury |
| royal secretary; the Crown's treasurer | treasury |
| hearing cases (the magistrate) | treasury |
| keeping the rolls at the Records Office | treasury |
| collecting the Crown's customs | treasury |
| warden of the Market Hall | treasury |
| abbot of the cathedral | treasury |
| keeping the Washhouse; keeping the Bathhouse | treasury |
| commanding the Watch (a guard: the watch's post) | treasury |
| harbourmaster of the Wharf Office; overseeing the Counting House | the Country (the Houses' estates) |

With the Queen, the King's daughter, the City Office's head, the House of Vance's lending and the Grayrock wharf (the
first fixes), twenty posts are paid as heads: 90p a day more in wages, 78p of it the treasury's. Not heads of an office:
the Houses' heads ("ruling House Fell", "keeping Greatly House"...: their Houses, not offices), the warehouses' keepers
(the Houses' stores: a middle post) and the sole priest, teacher and infirmarian.

**The follow-ups** (each a rule in `country.json` `rules`, a world's `rule` records, RulesPanel):

- **Buying ahead** (`buyAheadDays`, 1): the day before a Restday or a festival, each household's market hour tops its
  members up to a day more of food (three days). The fair rule's steps become shortDays, foodDays, then the days ahead
  (each member eats what it holds, so everyone reaches two days before anyone three: the first try, everyone straight to
  three, left the last members of poor households a day short and hungry after the Restday). A Restday's or festival's
  few sales no longer set the next day's shelves (the larger of its sales and the day before's), so the shops are
  stocked again when everyone shops as usual.
- **The float rule** (`floatDrawDays`, 7; `floatWagesDays`, 7): a till's working float, kept back before its wages and its
  keeper's profit, is its mean draws a day over the last seven days, not yesterday's alone (`Society::tillFloat`); its
  keeper's profit is what it holds over that float and seven days of its posts' wages (`floatWagesDays`, a week as it
  was). Before, a till stripped to a small float after a Restday drew heavily the next day and could not pay its wages.
- **The carriage subsidy only to need** (`subsidyToNeed`, on): the treasury's share of carriage is given only up to what the
  carters' till lacks, at the day's first carriage, of its crew's wages tonight and its float; nothing goes to the
  keeper's profit. No drivers were added.
- **Winter preserves**: the preserved fruit's season group has the Country's own stores (`stores`): a preserver whose
  fruit is capped by the season draws its dried apples and preserves itself from the Country (it sells them on:
  `supplies.preserver`), at most 150 pieces a day times the season's supply (25 / 10 / 40 / 100%: most in winter), and
  they are a tenth cheaper in winter (price 100 / 100 / 100 / 90%). Why winter had none more: the preservers make them
  only from fresh fruit, which the season caps at 30% in winter and the cider makers share, and their autumn stock-up
  (shelves to 2.5 times their sales) holds days, not a winter.

**Migration**: none needed: the new rules and the patrons' services are keys of `live.economy.rules` and `services`
(0058's JSON); `tools/map_editor.py` (`RULE_LIMITS`, `RULE_SWITCHES`, `service_list`'s `for`) and the Editor
(`economy.ts`'s `RULES`, `model.mjs`'s `RULE_LIMITS`, RulesPanel's patrons' rows) know them. DEV has none of its own
(country.json's).

**Reports**: `world_check --economy` gains `city_trade`, `city_trade_pieces`, `patron_jobs`, `patron_pieces`,
`patrons_treasury`, `patrons_country`, `bought_ahead` and per trade `patron_<business>_jobs` and `_spent`; the detail file
counts "patrons" and "city trade"; `tools/econ_detail.py` counts them in a trade's sales (and the inputs bought in its
draws), and shows the household services (`svc`, less the city's share) and `cover+` with them.

**Tests**: `Tests/trades_tests.cpp` (new, 908 checks, the real catalogue): the fifteen heads' wages and payers, the
Watch's commander and the rest of the watch; the cooper's casks till to till at its price (no toll, no carriage), only what
it holds, none with the rule off, and the restocking rule making wine by the cask from the cooper's cask (or the store
room's when the city has none); the patrons (the Watch's four kinds of mending and a piece for each guard a day at a
frequency of 1, the Houses' by the Country as far as it can spare, nothing kept, no household buying them, none with the
rule off or a treasury at its float); the medicines (the needs listed, every medicine on the apothecary's list, theriac
one from the Country, a household with money buying each over 121 days, now and then); buying ahead (three days before a
Restday, two otherwise); the float rule's profit at 7, 3 and 0 days; the subsidy only to need; the winter stores (a
preserver's shelves from the Country with no fruit in winter, made from fruit in autumn). `country_tests` and
`econ_fix_tests` run with the new rules as they were (they test the loop and the first fixes); `store_room_tests`'
fixture has the general store's herbs. All 73 C++ tests pass. Python: `test_item_catalog`, `test_city_capacity`,
`test_map_editor` (but the known Greyfen 11-vs-10), `test_world_store`, `test_studio`, `test_world_sync` pass. Editor: `tsc` clean, the lib
tests 53 of 53, the model's 78 of 79 (the same Greyfen count).

#### The runs

DEV build 52 (the write above; its export, nothing written), from Spring 1, `--simulate 6 8766` (a year) and `6 26298`
(three), `--no-check --deterministic --threads 4 --no-profile`, the year pinned to CPUs 0-5. Summaries committed in
`artifacts/economy/r2/trades-summary.txt`; the jsonl in the main repo's `artifacts/economy/r2/trades_*`. The three years'
first year is byte for byte the year's. Money conserved every day of every run; the detail's check finds no account
moved by other than its flows. (Pennies a day but the treasury and Gini; days 5-364 of each year.)

| Run | Hungry (mean) | Days anyone hungry | Worst hour | Fed (lowest) | Wages paid | Tills short | Profit | Alms | Wage support | Patrons (treasury, Country) | Carriage subsidy | Treasury (5 → 364; low-high) | Treasury, 4-week means a year apart | Gini, 4-week means a year apart |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| first fixes (build 51) | 0.09 | 5 | 70 | 89.7% | 99.6% | 18.5 | 1,230 | 376 | 287 | - | 35 | 4,077 → 6,999 (2,534-7,972) | 4,583 → 7,296 | .486 → .558 |
| **trades, every default** | **0.007** | 5 | **7** | **98.3%** | **99.7%** | 14.9 | 1,162 | 379 | 194 | 32, 55 | 0 | 3,878 → 8,631 (2,454-9,000) | 5,140 → 8,276 | .489 → .556 |
| the three years: year 2 | 0.006 | 5 | 2 | 99.1% | 99.7% | 16.1 | 1,148 | 366 | 204 | 33, 55 | 0 | 8,281 → 10,050 (4,451-10,310) | 8,808 → 9,700 | .551 → .569 |
| the three years: year 3 | 0.016 | 10 | 6 | 97.9% | 99.7% | 16.5 | 1,143 | 358 | 211 | 32, 53 | 0 | 9,351 → 11,122 (5,444-11,403) | 9,613 → 10,776 | .569 → .582 |

559 residents at the end of each year (the newcomers fill the city); born 11, 9 and 4, died 15, 19 and 13. Household
purses 29,187 → 25,843 → 24,897 → 23,398p over the three years.

**The what-ifs** (a year each, every default but one rule as it was), each follow-up's effect alone:

| What-if | Hungry | Days hungry | Worst hour | Festivals (run days 231, 322) | The trades it moves |
|---|---|---|---|---|---|
| every default | 0.007 | 5 | 7 | 2 and 7 at worst | |
| no buying ahead (`buyAheadDays` 0) | 0.104 | 12 | 63 | **54 and 63** | (none) |
| the float as it was (`floatDrawDays` 1) | 0.007 | 3 | 7 | 3 and 7 | short of wages: the general store 36%, the vintner 29%, the rag-and-bone yard 37% of days (every default: 1%, 1%, 4%); profit 1,240 (1,162); wage support 220 (194) |
| the subsidy as it was (`subsidyToNeed` 0) | 0.011 | 5 | 5 | 5 and 4 | the treasury pays 34p a day of carriage, the carters' keeper's profit 74p (every default: 0p and 73p); the treasury ends 670p lower |
| no patrons (`patrons` 0) | 0.011 | 3 | 6 | 5 and 6 | the armoury's cover .19, the shipwright's .28 and the builders' yard's .28 (with their services), as in the first fixes; wage support 255; the Country's basket trades a little better (the mason .86, the brickworks .94, the smithies .98) |
| no winter stores (data: `stores` false) | 0.012 | 6 | 7 | 4 and 7 | winter's preserved fruit 72 a day sold (every default: 145) |

**Each weak trade's cover** (value added / wages owed, the year; the first fixes' in brackets; `cover+` with the household
services' takings for the two yards):

| Trade | Cover | Short of wages (share of till-days) |
|---|---|---|
| the saddlery (.53) | gone: its shop an apothecary's | |
| the herbalist (.09) | gone: its stall an apothecary's | |
| the apothecaries, three tills (.87, one) | .98 (Hollis Physic 1.01, the Mortar and Pestle .97, the remedy stall .96); sales to residents 63p a day (15p), to the Country 60p | 11% |
| the armoury (.19) | **1.01**: its patrons 22p a day (5.5 mendings and 0.28 new pieces a day; the treasury 32p a day in all patrons, the Country 55p), the Country's basket 19p | 9% (97%) |
| the cooperage, two tills (.70) | .74: the casks 8.4p a day to the brewers and the press (1 a day); Staves & Hoops .97, the Market Hall's counter .39 (the casks go to the seller holding most) | 46% (50%) |
| the shipwright (cover+ .28) | **.95**: its patrons 31p a day (5.2 jobs) and the household services 12.6p | 13% (100%) |
| the builders' yard (cover+ .27) | **1.00**: its patrons 33p a day (6.8 jobs) and 12.8p | 2% (100%) |
| the general store, the vintner, the rag-and-bone yard (7.32, 3.03, 3.62) | 7.01, 2.73, 3.50 | 1%, 1%, 4% (36%, 32%, 36%) |
| the preservers (.88) | .99 (their winter sales from the Country's stores) | 32% (38%) |
| the Carters' Yard | its keeper's profit 73p a day (76p), now all from the fares: the treasury's subsidy 0p (35p) | 0% |

**Whether it is stable**, by the brief's tests:

- **Money conserved**: exactly, every day of every run.
- **Hunger**: 0.007, 0.006 and 0.016 a year against the first fixes' 0.09; the worst hour 7 hungry (year 1, the Winter
  festival; the first fixes' Autumn and Winter festivals had 70 and 55), 2 in year 2 and 6 in year 3 (both about the Spring
  festival); 5, 5 and 10 days with anyone hungry, nearly all at the festivals and the year's last weeks.
- **Wages**: 99.7% paid every year.
- **Gini** (earning households, 4-week means a year apart): .489 → .556 the first year (the first fixes' .486 → .558), then
  .551 → .569 and .569 → .582: still rising, by about a third as much each year (+.067, +.018, +.013).
- **No till cover collapsing, no runaway stock or prices**: the third year's covers are the first's within a few hundredths
  (the armoury .99, the apothecaries .95, the shipwright .92, the builders' yard 1.00); stock on the shelves level.
- **The treasury**: a season's swing (low in autumn, high in winter and spring) on a slow rise: its 4-week means +3,100p the
  first year, then +900p and +1,200p (about 3p a day on 1,600p a day of its flows); its day in year 3: 1,557p of toll and
  34p of the services in, 983p of wages, 358p of alms, 211p of wage support and 32p of patrons out. Year 1's larger rise is
  the carriage subsidy kept (35p a day: the carters no longer need it) and the shorter wage support (the trades fixed
  here pay their own posts). The first fixes' year rose the same way (+2,700p).
- The households' purses fall as the treasury rises (about 2,000p a year): savings move slowly from the comfortable
  households to the treasury, which keeps all over its 2,000p cap that no short till asks for.

So: **stable for hunger, wages, the trades and money**; the treasury and Gini **drift up slowly**, the first fixes' design
(the cap spends only on short tills' wages) and not these changes' doing, so they are left for the user (below) rather
than tuned.

#### Left for the user

- **The treasury's slow rise** (about 1,000p a year after the first): a lever exists for each way of spending it back:
  the gate toll (`toll`, 30%: each point is about 52p a day, more than the drift), the alms (`almsADay`), the wage support's cap (`treasuryCap`),
  or a new outlet (the public buildings' upkeep at the builders' yard is one already, 24p a day).
- **The Houses' armed retainers** are 24, a number in `country.json` (`patrons.list`): none of the Houses' posts is armed;
  placing them (the 200 country folk, doc 66) would make them posts like the Watch's.
- **The Country's basket**: the Country (the Houses' estates, the wharf) now pays 55p a day to its patrons out of the same
  money it spends on the city's goods, so the trades living on its basket sell a little less (the mason .86 → .84, the
  brickworks .94 → .90, the smithies .98 → .94, the builders' works .48 → .43, all paid by the wage support).
- **The Carters' Yard** keeps its keeper's 73p a day of profit from its fares alone (it takes about 135p of carriage for
  62p of wages): cheaper carriage (`carriage`, 6p a load) would pass it to the shops; more drivers were not added.
- **The Market Hall's cooper** sells few casks (the seller holding most is asked first); a rule that spreads the city's
  buying over its sellers by stock, as the Country's does, would share them.
- The food rebalance's three renamed shops keep their helpers' old labels ("helping at The Cough Jar" at The Honey Crock,
  "helping at Oilcloth & Wool" at The Apple Loft, "helping at Commons Sundries" at Commons Provisions): a label fix,
  not done here so as to change no other wolf.
- The Houses' heads ("ruling House Fell", "keeping Greatly House", "keeping Grayrock Hall", "keeping Timberland Court")
  and the warehouses' keepers are still at the Country's 8p: they head Houses and stores, not offices.

### The treasury's surplus: works and alms (2026-10-31)

Branch `r2-econ-treasury`, from `ridgemere2` (345d485). The trades' three years (above) were stable for hunger, wages,
the tills and money, but the treasury gained about 1,000p a year (its 4-week means 5,140 → 8,276, 8,808 → 9,700,
9,613 → 10,776), household purses fell about 2,000p a year, and Gini (earning households) rose about .015 a year: the cap
spends only on short tills' wages, and few tills are short now.

The decision (the user, 2026-10-31): a rule that sends whatever the treasury holds over its cap back out each day, at
18:00, after its own posts and the short shops' wages, as two things: **public works orders** (the city buys upkeep and
goods for public buildings and services from city businesses, at their prices, till to till: the builders' yard and other
trades that make sense, such as the smithy for ironwork, the chandlery for lamps and candles, the ropewalk and the cooper;
spread across trades so that the money becomes wages and doesn't pile up as one owner's profit; the patron or service
mechanism where it can be used) and **higher alms** (a share tops up the poorest households, per head, poorest first, up to a
level rather than to everyone). The split and the levels DM-tunable; never creating money; the treasury's own wages, the
wage support and the cap rule first. The goal: the treasury level with no year-over-year trend, household purses no longer
falling, Gini roughly flat, without hunger or short wages coming back; the money to the workers and the Sump rather than
to owners' profits.

#### The rule

`Society::spendSurplus`, last at 18:00 (after `payWages`, `supportWages` and `giveAlms`), five rules in `country.json`
`rules` (Core `items::Rules`; a world's own in `live.economy.rules`; the DM's Economy tab and Atlas's Economy page,
`RulesPanel`, applied live by `economy.sync`):

| Rule | Range | Default | What it does |
|---|---|---|---|
| `surplus` | 0/1 | on | The treasury's surplus goes out at 18:00; off, it stays. |
| `surplusKeep` | 0-100,000,000 | **4,000p** | Pennies over `treasuryCap` (2,000p) the treasury keeps before anything goes out: the surplus is what it holds over 6,000p. |
| `surplusWorks` | 0-100% | **20%** | Of the surplus, a day, to public works orders. |
| `surplusAlms` | 0-100% | **80%** | Of the surplus, a day, to the poorest households. |
| `surplusAlmsTo` | 0-1,000 | **40p** a head | The ceiling the poorest are raised to (the comforts line: under it a household buys no comforts, so it keeps what it is given). |

- **Why a reserve over the cap** (`surplusKeep`, a level the brief's list lacked): the treasury runs a deficit every summer
  and autumn (the toll falls with the harvest's prices, below its posts and alms) and makes it up in winter and spring;
  the trades' years swung about 4,000-5,000p from their winter high to their autumn low. Spending everything over the
  2,000p cap would leave the treasury at the cap each spring and under it by autumn, when the wage support (paid only over
  the cap) would stop and the short tills' posts go short again. 4,000p over the cap carries the lean half: the lowest the
  treasury fell in years 2 and 3 was 4,274p and 4,707p.
- **The alms** (`topUpPoorest`): every household under the ceiling (all its members' purses, per head) is raised to one
  level, the highest whole pennies a head the share can bring them all to; what is left the next penny a head, the
  poorest first; paid to the member with least ("alms from the surplus"). A household at or over the ceiling gets nothing.
- **The works** (`worksOrders`, `country.json` `works`): nine trades, each with its weight (its shares of the day's orders,
  of what is left, so that what one can't place goes on to the next) and what it supplies: the builders' yard (3; its
  patron services, a wall repointed, a roof's slates relaid, a floor or stair mended, done at once), the builders' works
  (2; beams, planks, fittings, nails), the smithy (2; fittings, chain, locks, nails), the masons (2; cut stone, mortar,
  lime), and 1 each the brickworks (bricks, roof tiles), the cooperage (barrels, hoops), the ropewalk (rope, canvas), the
  chandlery (lamp oil, tallow candles) and the paintworks (whitewash, paint). A good comes off the shop's shelves at the
  catalogue's price (one left for its customers) and is used up in the works, so the shop's shelves follow it and it makes
  more; the city fetches it from any shop of the trade with its post held. In a trade, the till with **most room under its
  keeper's profit** (its float and `floatWagesDays` of its posts' wages, as `payWages` keeps them) is ordered from first,
  up to that room, so the orders go to the tills that need them for their wages before any keeper's profit.
- **What one can't place goes to the other**: the alms first; what they can't place (no one under the ceiling) to the
  works; what the works can't place back to the alms; what neither places stays in the treasury. Shares over 100% in all
  are scaled to the whole.
- **Never makes money**: every penny moves by `shift` or `sale`, from the treasury only, never more than the surplus, so
  the treasury never ends a night under its cap and reserve by this rule.

**Reports**: `world_check --economy` gains `surplus` (what was over tonight), `surplus_alms`, `surplus_households`,
`works_spent`, `works_pieces` and per trade `works_<business>_pieces` and `_spent`; the detail file counts "works" (a
till's takings) and "alms from the surplus"; `tools/econ_detail.py` shows the works in a trade's sales (its own column) and
value added.

**Migration**: none: the rules are keys of `live.economy.rules` (0058's JSON) and default to country.json's. DEV has none of
its own and was not written. `tools/map_editor.py` (`RULE_LIMITS`, `RULE_SWITCHES`), the Editor (`economy.ts`'s `RULES`,
`model.mjs`'s `RULE_LIMITS`) and RulesPanel's note know them.

**Tests**: `Tests/treasury_tests.cpp` (new, 170 checks, the real catalogue): the defaults and the works' list; nothing under
the cap and reserve or with the rule off; the order (the ledger shows the treasury's posts, the wage support, the alms,
then the surplus; a treasury 20p over its cap gives its posts, the short till and the alms all of it and leaves no surplus;
200p over, the surplus is exactly what is left, and the treasury ends at its cap); the alms to a level (5p raises everyone
at 10p a head to 11 and the poorest to 12, nobody above them gets any; plenty raises all to the 20p ceiling and no
further, the rest stays; a ceiling at the day's alms gives none); the works (three fifths to the builders' yard's work,
done at once and never kept, the rest to the smithy, from the till with room under its profit, the full one given
nothing, one fitting left on the shelf, the treasury holding nothing); the split (what the alms can't place to the works;
shares over 100% never more than the surplus); five days of the whole loop, the cap and reserve kept every evening; money
conserved in every one. `country_tests` and `econ_fix_tests` run with `surplus` off (they test the loop as it was). All 74
C++ tests pass (`build-core`). Python `test_studio`, `test_world_store`, `test_item_catalog`, `test_map_editor` pass but for
the known Greyfen 11-vs-10; Editor: `tsc` clean, the lib tests 53 of 53, the model's 78 of 79 (the same Greyfen count).

#### The runs

DEV build 52 (its export, nothing written), from Spring 1, `--simulate 6 26298` (three years), `--no-check --deterministic
--threads 4 --no-profile`, each pinned to six CPUs; a first year of two settings, then three years of three (the
defaults are the third). Summaries committed in `artifacts/economy/r2/treasury-summary.txt`; the jsonl in the main repo's
`artifacts/economy/r2/treasury_*`. Money conserved every day of every run. Days 5-364 of each year; pennies a day but the
treasury, the purses and Gini; 4-week means: the year's first and last 28 days.

| Year | Run | Hungry (mean) | Worst hour | Wages paid | Treasury, 4-week means | Household purses (4-week means; day 5 → 364) | Gini (earners), 4-week means | Profit, owners' share of profit + wages | Base alms, wage support | Surplus: to the poorest, to the works |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | trades | 0.007 | 7 | 99.7% | 5,140 → 8,276 | 29,417 → 25,462 (29,187 → 25,843) | .489 → .556 | 1,162, 21.9% | 379, 194 | - |
| 1 | **surplus** | 0.005 | 5 | 99.8% | 5,053 → 5,961 | 29,476 → 26,802 (29,187 → 27,663) | .488 → .554 | 1,201, 22.4% | 340, 184 | 65, 16 |
| 2 | trades | 0.006 | 2 | 99.7% | 8,808 → 9,700 | 25,328 → 24,804 (26,893 → 24,897) | .551 → .569 | 1,148, 21.7% | 366, 204 | - |
| 2 | **surplus** | 0.003 | 2 | 99.7% | **5,966 → 5,964** | 27,273 → 26,962 (28,685 → 27,200) | .544 → .571 | 1,238, 23.0% | 292, 179 | 126, 31 |
| 3 | trades | 0.016 | 6 | 99.7% | 9,613 → 10,776 | 24,997 → 24,103 (25,905 → 23,398) | .569 → .582 | 1,143, 21.6% | 358, 211 | - |
| 3 | **surplus** | 0.006 | 5 | 99.8% | **5,965 → 5,972** | **27,194 → 27,232** (27,749 → 26,814) | .560 → .577 | 1,270, 23.5% | 262, 180 | 166, 41 |

The other settings, years 2 and 3 (treasury, purses, Gini by 4-week means; the owners' share):

| Setting | Treasury | Purses | Gini | Owners |
|---|---|---|---|---|
| works 50%, alms 50%, to 20p | 5,912 → 5,964; 5,928 → 5,967 | 26,996 → 26,820; 27,106 → 27,017 | .550 → .576; .567 → .579 | 23.0%, 23.6% |
| works 30%, alms 70%, to 30p | 5,939 → 5,957; 5,941 → 5,972 | 27,118 → 26,746; 27,087 → 26,946 | .548 → .573; .566 → .586 | 22.9%, 23.5% |
| **works 20%, alms 80%, to 40p** (the defaults) | 5,966 → 5,964; 5,965 → 5,972 | 27,273 → 26,962; 27,194 → 27,232 | .544 → .571; .560 → .577 | 23.0%, 23.5% |
| works 1%, alms 1% (a year only) | year 1: 5,136 → 7,956, still rising: too slow | | | |

559 residents at the end of each year; born 11, 9 and 4, died 15, 19 and 13, as in the trades' years.

**Where the works money went** (the defaults; pennies a day, years 1, 2, 3): the builders' yard 3.0, 5.9, 7.6; the masons
2.4, 4.6, 6.0; the builders' works 2.1, 4.3, 5.5; the smithies 2.1, 4.1, 5.3; the paintworks 1.8, 3.2, 4.5; the chandlery
1.5, 2.9, 3.9; the ropewalk 1.2, 2.4, 3.3; the cooperage 1.0, 2.1, 2.5; the brickworks 0.9, 1.8, 2.3 (16, 31 and 41p in
all). The surplus grows year on year (81, 157, 208p a day) because more of it comes back: the money it puts out is spent in
the shops, whose draws pay the toll (1,596, 1,615, 1,630p a day against the trades' 1,572, 1,560, 1,557), and the poorest,
held up to 40p a head, need less of the day's alms (340, 292, 262p against 379, 366, 358).

**Till covers that moved by more than .03** (year 3, against the trades' year 3; cover+ with the household services):
the works' trades: the builders' works .41 → .68 (short of wages on 92% → 57% of days), the builders' yard .72 → .89
(cover+ 1.00 → 1.18), the masons .85 → 1.12, the paintworks .89 → 1.18, the ropewalk .78 → 1.03 (short 52% → 13%), the
brickworks .83 → .94, the smithies .93 → 1.01, the cooperage .74 → .79; and from the households' spending, the charcoal
burner .79 → .87, the apothecaries .95 → 1.02, the armoury .99 → 1.04, the shipwright .67 → .70 (cover+ .92 → .95), the
ironworks, glassworks, papermill, distillery, cartwright, scriptorium, cartographer, smokehouse, tinker, perfumer,
pottery, printing house, brewery, carpenter and jeweller up .03-.14, the fishmongers 1.35 → 1.47, the tannery 2.12 → 2.28,
the vintner 2.52 → 2.98, the rag-and-bone yard 3.11 → 3.45, the general store 6.41 → 7.20. Down: the plasterworks
1.44 → 1.32 and the Market Square stalls 1.12 → 1.07. Year 1 moved the same way by about a third as much.

**Whether the drift is gone**:

- **The treasury**: yes. It holds 6,000p (its cap and reserve) in winter and spring and falls to 4,300-4,700p in autumn,
  the same each year: 4-week means a year apart 5,966 → 5,964 and 5,965 → 5,972 (the trades' +890 and +1,160). Year 1's
  rise is the start (3,878p) reaching its level. The wage support was paid every day of years 2 and 3.
- **Household purses**: mostly. By 4-week means they fall 311p in year 2 and rise 38p in year 3 (the trades': -524 and
  -894); from Spring 1 to Spring 1 28,685 → 27,749 (the trades' 26,893 → 25,905). What still leaves them goes to the tills,
  whose working floats grow with their sales (16,624 → 16,884p at the year's end, by 91-day means). The waged households
  now hold their own: 8,574 → 8,592p a year apart (the trades': 8,604 → 8,030), the keepers 18,211 → 18,245p.
- **Hunger and wages**: no worse: hunger 0.005, 0.003, 0.006 (the trades' 0.007, 0.006, 0.016), the worst hour 5, 2 and 5,
  wages 99.7-99.8% paid, tills short 14 a day (the trades' 15-16), the wage support 180p a day (the trades' 194-211).
- **Gini: not flat.** .488 → .554, .544 → .571 and .560 → .577: about .016 a year in years 2 and 3, the trades' .018 and
  .013, and the same in every setting tried (more to the alms or less). It is a little lower at each year's start
  (.544 and .560 against .551 and .569), but its rise is not the treasury's doing: the money the surplus puts out is spent,
  and the shops' keepers take its margin as profit (1,201, 1,238, 1,270p a day against 1,143-1,162; the owners' share of
  profit and wages 22.4% → 23.5%, the trades' 21.6-21.9%); the general store alone took 136p a day in year 3 (119p). The
  rule's levers move the money, not where spending ends up.

#### Left for the user

- **Gini's slow rise** (about .016 a year) comes from the keepers' profit on the households' spending, not the treasury:
  the levers for it are the profit rules (`floatWagesDays`, a share of profit kept as wages or paid to a till's hands),
  the comforts rule (`comforts.above` and `share`, which decide when a household spends its savings), or a tax on profit
  over a level paid back as the surplus is. None is built.
- **The owners' share** rose from 21.6% to 23.5% with the money moving again; the works' 41p a day is small beside the
  households' own spending.
- **The reserve** (`surplusKeep`, 4,000p) is sized to the lean seasons of this city; a world with a larger treasury or
  toll needs its own.
- **The works' trades and weights** are a line of data each (`country.json` `works`): the glassworks' lamp panes, the
  carpenters, the sawmill and the plasterworks could join.

### Self-correcting rules (2026-11-01)

Branch `r2-econ-selfcorrect`, from `ridgemere2` (37ab96f). The user, 2026-11-01 (verbatim): "It's looking good and balanced
right now, but it doesn't seem robust enough to survive us adding additional towns and converting the 'country' into
actual businesses just yet." The user approved building rules that correct themselves, so that the economy rebalances
itself rather than being retuned by hand when the city changes; shock tests are on hold (none built or run).

Four rules, each gentle, each its own switch and rate in `country.json` `rules` (Core `items::Rules`; a world's own in
`live.economy.rules`, the DM's Economy tab and Atlas's Economy page, `RulesPanel`, applied live by `economy.sync`). What each
has drifted to is the society's (`LoopState`, below), not a setting: the DM sees it, read only, beside the rules.

#### The rules

| Rule | Settings (defaults) | What it does |
|---|---|---|
| 1. Prices follow stock | `stockPrices` (on), `stockPriceBand` (25%), `stockPriceStep` (10: a hundredth a day) | At the day's turn, each ware residents buy (food, needs, comforts) at each shop open that day: its price factor up a step when it sold out, down a step when what is left is more than three days of the day's sales and over its starting stock (the season's stock-up allowed for), else a quarter step back toward 1. Then every factor moved that day is scaled so that their mean, weighted by sales, is 1. Within the band of 1, and never under what the store room charges the shop for the ware and a twentieth (a resold ware 0.80; an input of its crafts it also sells as it is, such as the fishmonger's fish, drawn at the making price, 1.05, so 1). Kept as they were over a Restday or festival. |
| 2. Shelves follow sales | `shelfFollow` (on), `shelfDays` (7), `shelfCover` (125%), `shelfGrow` (25%) | Each ware's sales a day, smoothed over about a week (a running mean, its first day its first day's sales); the shelf is 125% of it, never under its starting stock, at most a draw's worth (1,000). A day it sold out having sold at least its shelf counts as a quarter more than its shelf, so a shelf too small to show its demand grows (the small shelf's spiral); a maker that sold out short of its shelf was short of hands, not of shelf, and its shelf doesn't grow for it. A Restday's or festival's few sales never lower it (buying ahead's fix: the day before's count). Off: a quarter over yesterday's sales, as it was. |
| 3. Wages follow cover | `wageDrift` (on), `wageDriftDays` (28), `wageDriftSpare` (50%), `wageDriftMost` (6p), `wageDriftFloor` (7p) | Every 28 days a till that paid all its posts every day, its keeper's profit at least half their wages, raises each of its posts' wages a penny (keeper and hands alike); one short of them on more than half the days lowers them a penny, never a post under 7p (a day's food) and never one the world set lower; at most 6p either way. Between the two it stays. Judged on what the till itself paid, before the treasury's wage support. The treasury's and the Country's posts keep their fixed wages (they are the city's and the Houses' rates, not a business's takings). |
| 4. The toll and the alms follow the treasury | `treasuryDrift` (on), `treasuryTarget` (5,000p), `treasuryBand` (15%), `treasuryDays` (182), `tollStep` (5: half a point), `tollDriftMost` (5 points), `almsDriftMost` (2p) | The treasury's level is a slow mean of what it holds at 18:00, after the surplus (over about half a year, 182 nights; a plain mean of its first 182, so a new world's low start weighs no more than any other night), judged each fortnight once it has its 182 nights. Over 5,750p and not falling: the gate toll eases half a point and the alms rise a penny a head; under 4,250p and not rising: the toll firms half a point and the alms tighten a penny; inside the band, or already moving back toward it, nothing. The toll within 5 points of the world's; the alms up to 2p over `almsADay`, never under it (the line under hunger: tightened, they come back down to it and no further). |

**How the fourth shares the work with the surplus.** `spendSurplus` is the ceiling, each night, at once: whatever the
treasury holds over its cap and reserve (6,000p) goes to the poorest and the public works. The fourth rule is the slow trim
of the flows under it: a treasury that keeps reaching the ceiling (the surplus spending most nights) eases the toll and
raises the alms until it no longer does, and one that keeps running low (a toll shrinking as the Country's goods pass to the
city's own makers, or new towns' posts on it) firms the toll before the cap's wage support and the alms run dry. Its target
sits under the ceiling (5,000p against 6,000p), so the two never pull against each other: over 5,750p the trim eases while
the surplus clips; under 4,250p the surplus is already silent. The slow mean and the "not already moving back" test keep it
from chasing the season's swing (the treasury falls every summer and autumn and fills every winter and spring).

**Why the price level is held.** The first build let each factor move by itself. Over a year the city's food makers, every
one making as fast as its hands allow and selling out each day wherever residents turn, all climbed to the band's top
together: the price level, weighted by sales, rose to 1.25 (year a, below) and the households paid a quarter more for the
same food. Food is bought for nourishment, not by choice, so dearer food everywhere sells as much: a level that ratchets.
Holding the mean at 1 leaves what the rule is for: what is scarce dearer against what is plentiful, so the residents'
choice of food (nourishment for the price) moves them from the makers short of hands to the resellers the Country keeps
stocked.

**Money.** None of it makes money: a shop's price is paid as the season's is (the catalogue's in the sale, the difference
after, each payer's fraction of a penny carried), the wages by the payers they always had, the toll and alms by the draws
and the treasury. Players' trade and the Country's purchases stay at the catalogue's price.

**Saved.** `SocietyState::loop` (`LoopState`): by till and ware the price factors (only those not 1) and the smoothed sales;
by till the wage step and the days of its window (counted, short, owed, profit); the treasury's level, its nights, the level
and day it was last judged, and the toll and alms moved. It is the save's `society.loop` (only when there is any:
`wire::society`, `readSociety`, which keeps what is sound and drops the rest), so it rides in the game's checkpoint
(`game.checkpoints`, the save document's rest), which the game's snapshots write. **No migration**: the checkpoint is a
document, and the rules are keys of 0058's `live.economy.rules`. A save from before reads as nothing drifted; a world with all
four off writes no `loop` (its save as before). DEV was not written.

**The DM's view.** `GET /api/dm/economy` (and Atlas's) gains `drift` (`studio.Economy.drift`, from the world's save): the gate
toll and alms now against the world's, the treasury's level against its target, the price factors (how many, mean, range),
the shelves (how many, mean), the wage steps (up, down, mean, and each till moved by its keeper's name). RulesPanel shows it
under the rules ("Where the self-correcting rules stand"), read only.

**Reports.** `world_check --economy` gains, a day: `price_moved`, `price_mean`, `price_sd`, `price_low`, `price_high`,
`price_level` (the factors weighted by their wares' sales), `shelves`, `shelf_mean`, `shelf_sd`, `wage_step_mean`,
`wage_step_sd`, `wage_up`, `wage_down`, `wage_keeper`, `wage_hand` (the mean wage of a till's keeper and hand),
`toll_now`, `alms_now`, `treasury_level`. `--loop-state FILE` writes the society's `loop` every 28 days and on the last day.

**Tests**: `Tests/selfcorrect_tests.cpp` (new, 939 checks, the real catalogue): the defaults; prices (a ware sold out each
evening dearer each day by at most a step, none over a Restday or festival, to the band's top and no further; the price level
held at 1 while nothing is at a bound; quiet days back toward 1; a glutted ware of its own make to the band's foot, one drawn
at the making price never under 1; applied live; off, the season's price whatever the save holds; a resident paying the
season's price times the shop's over a day's buys, and only the season's with the rule off); shelves (a week's smoothing, a
seventh of the way a day, a Restday never lowering it, a full shelf sold out each day growing, never under its starting stock,
at most 1,000; off, a quarter over yesterday's whatever the save holds); wages (a penny a window up for a till with plenty of
profit, down for one never paying, at most 6p, a hand never under 7p, none in the band, the wage paid the drifted one, a
world's wage under the floor never raised to it, a narrower bound at once, the treasury's post untouched, off the world's);
the toll and the alms (eased and raised a step a fortnight to their bounds, the eased toll on a draw, nothing while falling
back toward the target, firmed under it, the alms tightened from 12 to the world's 10 and no further, nothing inside the band,
off the world's); a save (every drifted value through `wire::society`, `json::dump`, `parse`, `readSociety` and `restore`, the
same save again; an older save reads as nothing drifted; a value out of range dropped, the rest kept; all four off writing no
`loop`); money conserved in each. `country_tests` and `econ_fix_tests` run with the four off (the loop as they test it).
**The switches off are the old rules exactly**: on DEV build 52, 30 days with all four off are field for field the treasury
branch's run (every field of its economy file, every day). All 75 C++ tests pass after a clean build of `build-core`. Python:
`test_studio` (new: the DM's view from a world's save, and a rule off showing the world's own), `test_world_store`,
`test_world_sync`, `test_live_edit`, `test_dungeon_master`, `test_world_build`, `test_item_catalog`, `test_city_capacity`,
`test_world_db` pass, and `test_map_editor` but for the known Greyfen 11-vs-10. Editor: `tsc` clean; the lib tests 54 of 54
(new: the drift's lines), the model's 78 of 79 (the same Greyfen count).

#### The runs

DEV build 52 (its export; nothing written), from Spring 1, `--simulate 6 8766` (a year) and `6 26298` (three), `--no-check
--deterministic --threads 4 --no-profile`, pinned to six CPUs; the three years with `--economy-detail` and `--loop-state`.
Summaries committed in `artifacts/economy/r2/selfcorrect-summary.txt`; the jsonl in the main repo's
`artifacts/economy/r2/selfcorrect_*`. Money conserved every day of every run. Years of days 5-364; pennies a day but the
treasury, the purses and Gini; 4-week means: the year's first and last 28 days.

| Run | What |
|---|---|
| year a | a year, all four on, the first build: each price factor by itself, the treasury's level a plain slow mean from the first night |
| year b | a year, all on, the price level held at 1 |
| year ps, year wt | a year each on the first build's treasury rule: prices and shelves only; wages and the treasury only |
| 3 years a | all on, the treasury's level over 120 days, its band 10% |
| 3 years b | the level over 182 days, band 10% |
| **3 years c** | **the level over 182 days, band 15%: the defaults** |

**Three years, the defaults (c) against the treasury run (its defaults: every rule here off):**

| Year | Run | Hungry (mean) | Worst hour | Wages paid | Treasury, 4-week means | Household purses, 4-week means | Gini (earners), 4-week means | Owners' share | Profit, wages paid | Alms, wage support, surplus |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | treasury | 0.005 | 5 | 99.76% | 5,053 → 5,961 | 29,476 → 26,802 | .488 → .554 | 22.4% | 1,201, 4,158 | 340, 184, 81 |
| 1 | **self-correcting** | 0.005 | 4 | 99.71% | 5,193 → 5,925 | 29,414 → 25,561 | **.484 → .515** | **16.8%** | 879, 4,364 | 340, 171, 50 |
| 2 | treasury | 0.003 | 2 | 99.75% | 5,966 → 5,964 | 27,273 → 26,962 | .544 → .571 | 23.0% | 1,238, 4,151 | 292, 179, 157 |
| 2 | **self-correcting** | 0.001 | 1 | 99.79% | 5,921 → 5,956 | 25,960 → 25,795 | **.505 → .515** | **13.7%** | 707, 4,454 | 305, 129, 101 |
| 3 | treasury | 0.006 | 5 | 99.78% | 5,965 → 5,972 | 27,194 → 27,232 | .560 → .577 | 23.5% | 1,270, 4,138 | 262, 180, 207 |
| 3 | **self-correcting** | 0.002 | 2 | 99.82% | 5,952 → 5,961 | 25,998 → 25,955 | **.504 → .512** | **13.6%** | 703, 4,467 | 280, 127, 132 |

Tills short a day: 14.2, 14.5, 14.1 (the treasury run) against 15.4, 12.2, 12.3. The treasury's low: 2,487, 4,274, 4,707
against 1,896, 3,568, 4,223 (year 1's start under the cap for a few days: the wage support waited). Residents spent
5,651-5,726p a day in the treasury run, 5,408-5,496p here: the money not spent sits in the tills (18,496p at the third year's
end against 17,571p: their working floats and a week of their higher wages), not lost. The purses hold about 1,300p less and,
after the first year, hold level as the treasury run's do.

**Where each settles** (the third year of run c; the loop's state every 28 days is in the summary):

- **Prices**: the price level, weighted by sales, 0.994-1.000 every day; the factors' spread (sd) .08-.11, with 1-9 wares at
  the band's top (cider at four of its makers, the weaver's neckerchiefs: makers short of hands) and 7-21 at its foot (the
  apothecaries' slow medicines, smoked and salt fish, nails); the mean change between dumps .008-.026 (the larger in winter, as
  the seasons move supply). **Settles**, moving with the seasons. Without the held level (year a) every maker's food climbed to
  the top together: the level 1.25 by the year's end.
- **Shelves**: 10.0-12.5 on a shelf on average (sd about 23: the busy wares' hundreds), no runaway. **Settles.** (The first
  build counted any sold-out day as more than its shelf: the weaver, whose neckerchiefs the Country buys as fast as they are
  made, grew its count past a trillion in a year. Now a sold-out day grows only a shelf it emptied, and a count is at most
  1,000.)
- **Wages**: changes per 28-day window fall from 37-47 of 102 tills in the first months to 1-10 in years 2 and 3. At the end 27
  tills at +6p (eight provisioners, the general store, the vintner, the rag-and-bone yard, the foundry, a tannery, chandlers,
  carpenters, brewers, bakers, the Carters' Yard...), 15 at -6p (keepers at 8p, hands at the 7p floor: two inns, two stalls,
  two preservers, the builders' works, the shipwright, the cooperage, the moneychanger, the stables, a smithy, a bakery, a
  butcher), 11 at 0 and 49 between. Mean step +0.9p, sd 4.1-4.4; a keeper's mean wage 14.9p, a hand's 9.8p (from 14 and 8).
  **Settles**, each till where its takings pay its posts or at a bound; 42 of 102 at a bound, so the rich would raise further
  and the poor lower further if allowed.
- **The toll and the alms**: never moved in run c (the level 4,254-5,610 after its first 182 nights, inside 4,250-5,750). In
  runs a and b (band 10%) they **oscillated slowly**: the toll 30 → 32 → 31 → 29.5 → 30.5 in run a and 30 → 31.5 → 30.5 →
  28.5 → 30 in run b, the alms 10 → 12 → 10, a season behind the treasury's own swing (the first year's deep autumn, the
  surplus's spring ceiling); hence the wider band and the longer mean. In this city the rule watches; it acts when the level
  leaves the season's swing, as a changed city's would (a toll base that shrinks, posts that grow).
- **Covers** (value added / wages owed, year 3 against the treasury run's; 36 of 50 trades moved more than .05): down where
  wages rose, the rich most (the general store 7.20 → 3.60, the vintner 2.98 → 1.19, the rag-and-bone yard 3.45 → 1.74, the
  tanneries 2.28 → 1.30, the foundry 2.24 → 1.39, the chandlery 3.04 → 2.27; carpenters, provisioners, jewellers, weavers,
  tinkers, brewers, perfumers .5-.7 lower; the bakers 1.52 → 1.11; and the builders' yard .89 → .77, the shipwright .70 → .63,
  the smithies 1.01 → .95), up where they fell (the stalls 1.07 → 1.24, the charcoal burners .87 → 1.00, the cooperage .79 →
  .88, the butchers .88 → .96, the sawmill 1.06 → 1.12). From year 2 to year 3 of run c only 16 trades moved more than .05 (35
  from year 1 to 2): the covers converge toward 1 from both sides.

**Gini.** The wage rule does most of it: the year with wages and the treasury only ended at .532 against the treasury run's
.554, prices and shelves only at .548, all four .505-.515. Over three years the 4-week means at each year's start are .484,
.505, .504 and at each year's end .515, .515, .512 (the treasury run's .488, .544, .560 and .554, .571, .577): **about .05
lower and flat after the first year**, where it rose about .016 a year. The owners' share of profit and wages falls from
22-24% to 13.6-16.8%: a rich till's profit becomes its hands' and keeper's wages, and the poor tills lower theirs, so the wage
support they need falls (127-129p a day against 179-180p).

**Nothing is switched off by default.** All four settle at their defaults in run c; the fourth needed its band widened to 15%
and its level taken over half a year (not 120 days) to stop a slow oscillation of a point and a half.

#### Left for the user

- **The wage bounds.** 42 of 102 tills are at ±6p: the rich would pay more and the poor less. Wider bounds would move more of
  the profit to wages (and lower Gini further) and leave the poorest trades' keepers nearer the floor. The floor is a day's
  food (7p); a hand's 8p is a penny over it.
- **The treasury's target** (5,000p, within 15%) is this city's; a city with more towns or a larger toll needs its own (as the
  surplus's reserve, `surplusKeep`, does). The rule acts only outside the season's swing.
- **Purses and tills.** The households hold about 1,300p less than in the treasury run and the tills about as much more (a week
  of the higher wages is kept before profit): `floatWagesDays` (7) is the lever if the tills should keep less.
- **The fishmonger's fish** (and any input a shop also sells as it is) is drawn at the making price, 100% of the catalogue's:
  it sells at no margin, and the price rule can't take it lower. The store room's rule, not this one's.
- **Players' prices** stay the catalogue's: a shop's price for residents follows its stock, a player buying there pays the
  catalogue's (as with the season's price). Whether players meet the shop's price is the user's call.
- Shock tests (a town added, a Country good passed to a city maker) are on hold, as asked: the rules are built for them, not yet
  shown against them.

### Players in the shifting market; wider wages; the long runs (2026-11-01)

Branch `r2-econ-long`, from `ridgemere2` (ef796c1). The user's decisions on two of the self-correcting rules' open questions,
2026-11-01 (verbatim): on players' prices, **"Yeah of course, players should experience the economy shifts as well."**; on the
wage bounds, **"Yeah let the wage limit spread more."**; and the runs: **"Make those changes, and then let's run a 6 year test.
If the 6 year test looks stable, run a 12 year test."**

#### What changed

**Players pay the shop's price.** `Society::quote` (which the shop's list in the client, the keeper's word when asked about
trade, and `trade` itself all use) prices a player's trade at the catalogue's price times `shopFactor`: the season's price
factor times the shop's own factor for the ware from its stock (rule 1; a quality of a ware at the ware's factor). The lot is
priced, to the nearest penny and never under a penny a piece; a market stall a tenth off, rounded down, as before. A shop's offer
for what a player sells is `SellBackShare` (55%) of that same price, so a scarce ware fetches more and a glutted one less and
the shop keeps its margin. `trade` takes exactly the quote: the lot at its whole pennies a piece and the odd pennies after
(`local trade: the odd pennies`). The client shows `buyPrice`/`sellPrice` from the quote of one piece, so a piece shown is a
piece paid; its buttons trade one at a time.

- Why the lot, not the piece: most food is 1-5p, and a piece rounded to the penny hardly moves (fish at 2p is 2p from 0.75 to
  1.24). A lot shows the shift (ten fish at 1.2 are 24p, not 20p); one bread at 1p stays 1p, as a coin must.
- **Players' trade feeds the rules as residents' does, which it already did.** A player's purchase is a sale from the till
  (`transfer`): it counts in the day's sales (the shelf follows it) and, emptying the shelf, as selling out (the price rises
  at the day's turn). A player's sale to a shop goes on its shelf, where the day's-end glut test sees it; a shop takes only a
  few of a good from a player (`GoodsKept`, 4; a store's 40 of what it supplies or buys), so a player can't glut a ware alone.
  Nothing else was needed: players' purchases and sell-outs count, their sales count through the stock.
- **The Country's purchases** stay at the catalogue's price (its code is unchanged).
- A note: with prices that move with the seasons, a player who buys a ware in its cheap season and sells it back in its dear
  one can gain where the season's swing is more than about 1.1 times (0.75 × 1.0 against 0.55 × 1.25 × the dear season's).
  It's trade, bounded by what a shop takes from a player; left as it is.

**The wage bounds**, rule 3: `wageDriftMost` (the keeper's) from 6p to **12p** either way, and a new `wageDriftHandMost` (a
hand's) at **12p**, both in `country.json` `rules`, a world's own in `live.economy.rules`, on the DM's Economy tab and Atlas's
Economy page (RulesPanel). The 7p floor and the penny a 28-day window are unchanged. A till keeps one step; each post's wage
is within its own bound, and the step moves only while it moves some post (down: while one is over the floor and inside its
bound). Separate bounds, the same by default: a keeper takes the till's profit as well as its wage, so how far a keeper's wage
rises changes little; a hand's raise is what moves a rich till's profit to its many posts, so the hands' bound should not be
the narrower. The DM can hold the hands' lower (or higher) on its own. Downward the floor stops a post first: a 14p keeper at
-7 (7p), an 8p hand at -1, so the poorest tills sit at the floor, not the bound. `Society::wageAtBound(till)`: 1 at the top
(no post can rise), -1 at the foot (none can fall).

**Reports.** `world_check --economy` gains, a day: `wage_top` and `wage_foot` (tills at a bound), `wage_keeper_sd`,
`wage_hand_sd`; and the people: `pups` (under 16), `working_age` (16 to their own age to retire), `retired`, `age_mean`,
`posts`, `posts_held`, `household_most`.

**Tests**: `selfcorrect_tests` 1,088 checks (from 939): the new defaults; wages to +12p (26p and 20p paid) and the barber's
keeper to the 7p floor at -7, the step stopping there; at the top and the foot as `wageAtBound` says; the hands' bound apart
from the keeper's (a hand stopping at 11p while its keeper goes on to 26p); each bound narrowed by the DM at once. Players: a
fish at 1.2 and salt fish at 0.8, on and off: one piece and a lot of ten as the quote says, the shop's offer for three salt
fish, the trade taking exactly the quote (the odd pennies too) and saying so, money conserved; a player buying a shelf out
making the ware dearer the next day and counted in its sales. All 75 C++ tests pass after a clean build of `build-core`.
Python: `test_studio`, `test_world_store`, `test_world_sync` (once a transient error, then clean), `test_live_edit`,
`test_dungeon_master`, `test_world_build`, `test_item_catalog`, `test_city_capacity`, `test_world_db` pass, `test_map_editor`
but for the known Greyfen 11-vs-10. Editor: `tsc` clean; the lib tests 54 of 54, the model's 78 of 79 (Greyfen).

#### The runs

DEV build 52 (its export; nothing written), from Spring 1, `--simulate 6 8766` (a year, the check) and `6 52596` (six years),
`--no-check --deterministic --threads 4 --no-profile --loop-state`, pinned to six CPUs (taskset 0-5); no `--economy-detail`.
Six years took 50 minutes (about 1.4 s a game day); the economy file is 9 MB, the loop's 2.8 MB (twelve years would be about
18 and 6). The harness copes: the day and the calendar run on (day 2190 is Spring 1 of year 7), birthdays age everyone, the old
die by age (8-19 a year), pups grow into working age, newcomers come for vacant posts and births follow deaths. The year check
is the six years' first year exactly (deterministic). Summaries committed in `artifacts/economy/r2/long-summary.txt`; the
jsonl in `artifacts/economy/r2/long_*` (not committed).

**Money**: conserved every day (the ledger's `conserved`). The money supply grows only by the newcomers' purses (30p each,
made: 57 newcomers, 1,710p over six years), and that ends in the tills (below).

| Year | Hungry mean / worst | Wages paid | Treasury, 4 wk (low) | Purses, 4 wk | Gini, 4 wk | Owners' share | Residents / households / earning | Births / newcomers / deaths | Pups / working / retired, mean age | Keeper / hand wage | Tills at top / foot | Price sd (end); toll / alms |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | .009 / 5 | 99.74% | 5,193 → 5,940 (1,763) | 29,414 → 25,456 | .484 → .512 | 16.4% | 559 / 233 / 196 | 11 / 4 / 15 | 93 / 425 / 41, 36.5 | 15.34 / 9.89 | 3 / 6 | .107; 30.5 / 10 |
| 2 | .003 / 3 | 99.84% | 5,913 → 5,954 (3,569) | 25,896 → 25,464 | .503 → .507 | 11.6% | 559 / 238 / 202 | 9 / 10 / 19 | 97 / 420 / 42, 35.9 | 15.54 / 10.15 | 6 / 6 | .105; 30.5 / 10 |
| 3 | .002 / 2 | 99.87% | 5,942 → 5,962 (4,116) | 25,897 → 25,505 | .501 → .514 | 11.3% | 559 / 241 / 207 | 4 / 9 / 13 | 83 / 436 / 40, 36.2 | 15.53 / 10.32 | 6 / 9 | .111; 30.5 / 10 |
| 4 | .003 / 5 | 99.91% | 5,957 → 5,787 (4,690) | 25,594 → 25,915 | .504 → .519 | 11.4% | 559 / 250 / 218 | 2 / 14 / 16 | 83 / 432 / 44, 36.4 | 15.48 / 10.45 | 6 / 10 | .112; 28.5 / 12 |
| 5 | .005 / 3 | 99.93% | 5,621 → 5,798 (2,126) | 26,188 → 25,910 | .515 → .532 | 12.3% | 559 / 256 / 224 | 0 / 8 / 8 | 77 / 428 / 54, 37.2 | 15.69 / 10.54 | 7 / 10 | .112; 28.5 / 12 |
| 6 | .002 / 2 | 99.91% | 5,643 → 5,963 (1,930) | 26,340 → 25,480 | .524 → .543 | 12.4% | 559 / 261 / 230 | 2 / 12 / 14 | 66 / 443 / 50, 37.5 | 15.64 / 10.53 | 7 / 9 | .122; 29.5 / 10 |

(Years of days 5-364, then whole years; 4-week means the year's first and last 28 days; the people, the wages, the bounds and
the price spread at the year's end. Profit 859, 597, 583, 588, 646, 658p a day against wages paid 4,372-4,635p. The tills, 4
weeks at each year's end: 18,353, 18,672, 18,843, 19,039, 19,269, 19,923p.)

**What settles.**

- **Hunger, wages paid, purses**: hunger .002-.009 (a worst hour of 2-5), wages paid 99.74-99.93% and rising, the purses'
  end-of-year means 25,456-25,915p with no trend.
- **The wages** (this change): the steps spread through year 1 (the old bound's 42 of 102 tills at ±6p by year 3) and settle
  by year 3: a keeper's mean wage 15.3 → 15.5 → 15.5 → 15.5 → 15.7 → 15.6p, a hand's 9.9 → 10.2 → 10.3 → 10.5 → 10.5 → 10.5p.
  **At a bound: 7 tills at +12p and 9 at the foot (their keepers at the 7p floor) at the end, 16 of 102** (from 42); 86
  between, spread over every step from -7 to +12. The owners' share of profit and wages falls from 16.4% to 11-12% (13.6% with
  the old bound) and holds. The covers converge as before. **Settles.**
- **Prices**: the price level 0.992-1.001 every day of six years; 263-270 factors moved; the spread at each year's end .105-.122
  (its year's mean .079, .091, .095, .097, .097, .103: still widening a little in year 6); 0-12 at the band's top, 6-21 at its
  foot, moving with the seasons. **Settles, nearly**: the spread's slow widening is worth watching.
- **Shelves**: 418-423 wares, a mean of 11.3-11.5 a shelf, every year. **Settles.**
- **The tills** gain about 300p a year (1,570p in six), as much as the newcomers' purses add to the money: their floats and a
  week of their wages hold the money the new arrivals bring. Not a leak; a slow growth of the money supply that comes with the
  population's turnover.

**What doesn't: two drifts, neither from these changes. So the twelve-year run was not started.**

1. **Gini rises from year 4**: the end-of-year means .512, .507, .514, then **.519, .532, .543**; the start-of-year .484, .503,
   .501, .504, .515, .524 (the same seasons: about +.012 a year from year 4). Years 1-3 match the old bound's three-year run
   (.515, .515, .512), and the wages, the steps and the owners' share hardly move after year 3, so the wider bound isn't it.
   Two things move with it:
   - **The households multiply and the births stop** (demography). The population is held at 559, but births fall 11, 9,
     4, 2, 0, 2 a year and newcomers rise 4, 10, 9, 14, 8, 12: when a wolf with a post dies, the post's vacancy sends for a
     stranger first, and a stranger counts against the town's number, so the children a short town asks for are crowded out
     (`tendCareers`: newcomers are asked before births, both from the same shortfall). A newcomer whose predecessor's house
     still has family in it lodges at the work (`r.home = p.work`), a household of its own: households 233 → 261 and earning
     households 196 → 230 in six years, each a single young wolf with a 30p purse. Pups fall from 93 to 66, the retired rise
     from 41 to 50-54, the mean age from 36.5 to 37.5. The posts stay held (508 of 509), and the working age grows as the last
     pups come of age (425 → 443), so there is no shortage of workers yet; over twelve years the pups would keep falling and
     the city would run on strangers. More, poorer, one-wolf households over the same money raise the Gini of earning
     households by itself.
   - **The treasury rule's trim, a year's swing behind** (rule 4). The treasury's level rose year by year (4,828, 5,301, 5,575
     at the year ends; the wage support it pays falls as fewer tills run short), crossed the band's top (5,750p) in year 4, and
     the rule eased the toll two points (30.5 → 28.5) and raised the alms two pennies (10 → 12) over four fortnights, while
     the slow mean still read high. The treasury then ran lower: its lows 4,690p (year 4), **2,126p and 1,930p** (years 5 and
     6, under the 2,000p cap, so the wage support waited a few days), its level back to 4,356p, and the toll firming again
     (29.5 by year 6's end, the alms back to 10). One slow swing in six years, larger than the season's; the twelve-year run
     would show whether it damps. It moves the Gini too: the surplus to the poorest (80% of the surplus, up to 40p a head)
     fell from 117p a day in year 4 to 19p and 52p in years 5 and 6, and the flat 2p more alms gave back less than that.

**Proposed fixes (none applied; each needs a run):**

- **Births not crowded out by newcomers**: count the strangers sent for against the town's working posts, not its number,
  so that births still follow deaths (the user's rule, 2026-10-14: "births follow deaths"); or keep a town's number of pups,
  as it was made, with births. And **a newcomer lodges with a household** where there's room (or in the town's lodgings,
  communal), not as a home of its own at the work. Either holds the households near 233-241 and the age structure.
- **The treasury rule's target where the treasury sits**: its level settles at 5,300-5,700p under the surplus's 6,000p ceiling,
  so a 5,000p target's band top (5,750p) sits just over it and a small rise trips it. A target of 5,500p (band 4,675-6,325p,
  under the ceiling only in its top), or judging it every 28 days rather than 14 (so the 182-day mean answers a step before
  the next), would keep it watching rather than swinging.
- Then the six years again, and the twelve if they hold.

#### Left for the user

- The two fixes above, and whether to run the six years again with them (about 50 minutes) and then the twelve (about 100).
- The wage bounds: 12p each, 16 tills at a bound in year 6. The DM can narrow the hands' alone (`wageDriftHandMost`).
- Players' prices move with the shop's; a lot is priced, a piece never under a penny. Whether a shop should take more than a
  few of a good from a player (so players can glut a ware) is open; today they can't.

### Births follow deaths; arrivals from the Country; the treasury's target (2026-11-01)

Branch `r2-econ-people`, from `ridgemere2` (2f95972). The user's decisions after the long runs (2026-11-01, verbatim):
**"Okay so, stop simulating players. Only simulate NPCs. There should be no simulated players. Birthrates should match
death rates for this game, otherwise we are going to end up with a shitton of NPCs real fast. Player count should be
ignored for that count. Newcomers, I don't know what these are. I never said to simulate new NPCs or players, so don't do
that. If you need newcomers for the economy simulate someone from our preexisting NPC list coming in from the country.
Make your treasury fix as well. Then go ahead and simulate the next run"**

#### No simulated players

There were none, and there are none. `world_check --economy` adds no player unless `--players N` says (`playerCount`: 1
for a city check, 0 with `--economy`; doc 69, section 9); `World::addPlayer` (which opens a player's account,
`Society::addPlayer`) is called only by that loop, and by a save's restore, which the harness never does. Each run's
output says "N residents, no player", and the economy line now counts `players` (bodies that aren't NPCs, and player
accounts without one): **0 on every day of every run below**. The newcomer accounts of doc 52 (an account new until 15
hours played) are the game server's, for real sign-ins; nothing of them runs in the harness.

#### What a "newcomer" was, and what replaced it

A newcomer was a **made-up NPC**: when a post had stood empty eight days in a town with fewer living residents than it
was made with, `Society::tendCareers` sent for a stranger: a new resident with a placeholder name ("Wren Farwander"), 22
to 41 years old, cloned from the last holder's record, given a made 30p purse (`NewcomerPurse`, money from outside) and
the trade's skill, lodged in the last holder's house if empty and otherwise **at the work, as a household of its own**.
The long runs made 57 in six years, and they crowded out the births (both came from the same shortfall). That is gone,
in the game and the harness, and so is the "child while the town is short of its number" (and the towns' numbers).

- **Births follow deaths, one for one** (`Society::died`, `tendBirths`). A resident's death (never a player's: only
  residents are counted, and a player is never one) owes the world one child, due 1 to 7 days later by a lot of the
  dead's (`CareerState::birthsOwed`, saved with the society). Each day the children due go to married couples at home
  together, as births always chose parents (the mother 18 to 45, a year or so since their last, three at most), one to a
  couple, those with fewest children first, then a lot of the day. A child nobody can have today waits for tomorrow; a
  host that can't make one gives it back (`World::residentNotMade`). So over a run **births equal deaths, but for those
  still owed** at its end. A child's sex is its own, male or female half and half, as before.
- **A post empty eight days** (`PostWaitDays`), after the city's own steps (the apprentice, a master, the family, a
  local out of work, the best-trained), and only a post that pays (`payOf`: lessons, play and the like are no posts to
  fill): first **a wolf already in the city with no post that pays** (none, or a pup's or an elder's: "at lessons"),
  of working age, not apprenticed, **of the post's class**, grown pups first (a parent known, under 25), then the best at
  that kind of work; else **an unplaced NPC of the world's own list** (doc 66, 2; 524 in DEV), of working age (16 to
  its own age to retire), of the post's class, any sex: the one whose trade suits the post (its kind of work, the post's
  words in its label or description), then a lot of the post's. Then the same two of the next class (upper and middle,
  middle and lower; never upper and lower). Nobody new is ever made: **no NPC is created except by birth.**
- **Class** (`Society::postClass`, `classOf`; doc 67, Progress, "Jobs by class"): a post's from the city's own rules
  (`ridgemere2_people.py`'s `job_class`, its word lists): keeping one of the six independent larger businesses, a House's
  own affairs (the wage rules that pay a keeper's wage), at home in the Heights, high office: **upper**; any other shop,
  stall, counter or tavern, a Great House's works run for it, lower office, helping in the family's shop over which it
  lives: **middle**; a hand in someone else's business, waged, service and carrying work: **lower**; the Watch and the
  clergy any. A wolf's: its own post's as it was written (the class pass matched 94% of earners to their household's
  class), else its family's (a spouse, a parent), else its household's; an unplaced NPC's from its label as written (the
  Country's fields, woods, waters and mines, service: lower; a town's shop or inn: middle; the Heights: upper), else
  its family's.
- **Where an arrival lives**: the dead's home, if the last holder died and nobody lives there now; else **with a
  household of its class within reach of the work that has room** (fewer than six living there, so it never makes one
  over `HouseholdMost`), the smallest first; else the communal lodgings within reach (`CareerWorld::lodgings`: DEV has two,
  the Anchor and the Mud Lodgings, empty); with none, nobody is sent for that day. **Never alone at the work**, and no
  home is built (the three homes to a structure stay as they are). It is placed without work (doc 66, 2: it lives at
  home, keeps its talk, story and families) and holds the post it came for; its skill is the trade's (35 or its level)
  when its own trade is of that kind, else 5. Its town talks of it as a newcomer ("is new in town").
- **Money**: an unplaced NPC's purse is **outside the economy's count** (it has no account: doc 66, 2, "no money
  moving"; DEV's 524 hold 27,377p as written). So an arrival brings its own purse **up to 30p, paid by the Country's
  account** (`outside:country`, as far as it holds it): moved, never made. The money supply is the start's every day of
  every run (50,252p), where the newcomers' purses grew it 1,710p in six years.
- **The game server records the move** (`Game::makeResidents`, a world from the database): it gives the arrival's
  `live.npcs` row the home (`home_area`, `home_x`, `home_y`, only while it is unset: work and evening stay unset, so the
  row keeps its authored label and role), then places it as a world from files does (`World::arriveResident`); the post
  it holds is the checkpoint's, as any succession's. Births are as before (a row cloned from the mother's, origin
  `runtime`). Nothing was written to DEV: the runs are the harness's, on its export, and DEV holds no made NPC (no row of
  origin `runtime`) and needs no migration.

**The treasury's target** (rule 4): `treasuryTarget` from 5,000p to **5,500p**, about where the treasury's level settles
(5,300-5,700p under the surplus's 6,000p ceiling), and a new `treasuryJudgeDays`, **28** (it was a fortnight,
`DriftStepDays`): the half year's mean answers a step before the next. Both in `country.json` `rules`, a world's own in
`live.economy.rules`, on the DM's Economy tab and Atlas's Economy page (RulesPanel's "Judged every"). The band is
unchanged (15%: 4,675-6,325p).

**Reports.** The economy line: `arrivals` (for `newcomers`), `births_owed`, `unplaced`, `earning_posts`, `earning_held`,
`empty_longest` (days), `households_alone`, `filled_city`, `filled_city_next`, `filled_country`, `filled_country_next`,
`filled_local`, `filled_best` (how the empty posts were filled so far: `Society::fills`), `players`. `tools/people_summary.py
ECONOMY.jsonl [LOOP.jsonl]` gives the years.

**Tests**: `Tests/people_flow_tests.cpp` (new, in ctest as `people_flow_behaviors`, the real catalogue): the classes
(posts and wolves, resident and unplaced); births follow deaths (none while nobody dies, none for a player's death, one
due 1-7 days after a resident's, to a couple at home, in his place; a child for each death, one for one; dying twice owes
one; revived before the birth owes none; a child not made owed again; the owed through a save); no NPC made but by
birth (only births and arrivals asked for, an arrival one of the world's own); a post from the city first (a grown pup
at lessons takes the docker's post at eight days, not before, nobody sent for); from the Country (the fisher before the
servant, not the old, the pup, or one two classes away; lodging with a household of his class; his own purse up to 30p
from the Country's account; placed without work, no post of his own made, no longer unplaced; the money conserved);
where arrivals stay (the dead's empty home; the lodgings when every household of its class is full; with no lodgings
nobody sent for; the next class when none of its own is left). `sim_fast_tests`: on the small town Fen, an unplaced
fisher, comes in for the smithy; births equal deaths less those owed; nobody but the world's own and the born; no player
in the economy file. `selfcorrect_tests` (1,182 checks): the new defaults; the mechanism's tests at a fortnight, as built;
the new target and period (6,000p inside the band against 5,500p, where against 5,000p it eased; a step each 28 days and
only then; the DM's own period at once). `careers_tests`: a child for the one who died. All 76 C++ tests pass after a
clean build of `build-core`. Python: `test_studio` (the DM's view's target), `test_world_store`, `test_world_sync`,
`test_live_edit`, `test_dungeon_master`, `test_world_build`, `test_item_catalog`, `test_city_capacity`, `test_world_db`
pass, and `test_map_editor` but for the known Greyfen 11-vs-10. Editor: `tsc` clean; the lib tests 54 of 54, the model's
78 of 79 (Greyfen).

#### The runs

DEV build 52 (its export, as doc 69, section 10; nothing written), from Spring 1, `--simulate 6 8766` (a year, the
check), `6 52596` (six years) and `6 105192` (twelve), `--no-check --deterministic --threads 4 --no-profile --loop-state`,
pinned to six CPUs (taskset 0-5). The six years took 48 minutes, the twelve 115 (1.3 and 1.6 s a game day; the machine was
shared). Deterministic: the year is the six years' first year, and the six the twelve's first six, line for line.
Summaries committed in `artifacts/economy/r2/people-summary.txt`; the jsonl in the main repo's
`artifacts/economy/r2/people_*` (not committed).

- **Money**: conserved every day of every run, and the money supply the start's (50,252p) every day: nothing is made.
- **Players**: none, on any day (`players` 0).
- **Births equal deaths**: 15 and 15 in the year; 89 and 89 in six years; **188 and 188 in twelve**, none owed at the end
  (at most one owed at a year's end, for a few days).
- **Arrivals**: 4 the first year, 28 in six, 64 in twelve (65 by the run's last hour): about five a year, all of the
  post's own class (none of the next). The empty posts that pay went 65 times to a wolf of the city with no post that
  pays (grown pups mostly) and 64 times to an arrival; the city's own steps (a local out of work, the best-trained, the
  family, the apprentice) took the rest as before. **The unplaced pool: 524 → 520 (a year), 496 (six years), 460 (twelve)**:
  64 of 524 used in twelve years, 12%.

The twelve years (years of days 5-364, then whole years; 4-week means the year's first and last 28 days; the people, the
posts, the wages and the toll at the year's end; the alms the year's mean a day):

| Year | Hungry mean / worst | Wages paid | Treasury, 4 wk (low); level | Purses, 4 wk | Gini, 4 wk | Owners' share | Residents / households (alone) / earning | Births / arrivals / deaths (owed) | Pups / working / retired, mean age | Unplaced | Paying posts held (fewest), longest empty | Keeper / hand wage | Toll / alms; alms a day |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 0.003 / 3 | 99.73% | 5,193 → 5,918 (1,803); 4,857 | 29,414 → 25,470 | 0.484 → 0.505 | 16.4% | 563 / 231 (76) / 193 | 15 / 4 / 15 (0) | 97 / 425 / 41, 36.3 | 520 | 430/430 (420), 9 d | 15.34 / 9.90 | 31 / 10; 347 |
| 2 | 0.003 / 2 | 99.83% | 5,915 → 5,942 (3,834); 5,373 | 25,745 → 24,623 | 0.504 → 0.508 | 11.7% | 569 / 229 (74) / 194 | 19 / 6 / 19 (0) | 111 / 416 / 42, 35.2 | 514 | 430/430 (427), 9 d | 15.46 / 10.13 | 31 / 10; 317 |
| 3 | 0.004 / 2 | 99.87% | 5,941 → 5,937 (3,627); 5,510 | 25,165 → 24,875 | 0.498 → 0.506 | 10.7% | 568 / 228 (75) / 194 | 13 / 0 / 14 (1) | 105 / 423 / 40, 35.1 | 514 | 429/430 (429), 9 d | 15.41 / 10.16 | 31 / 10; 303 |
| 4 | 0.002 / 3 | 99.90% | 5,956 → 5,918 (4,214); 5,673 | 25,164 → 24,935 | 0.497 → 0.492 | 10.3% | 576 / 227 (70) / 194 | 19 / 7 / 18 (0) | 120 / 412 / 44, 34.3 | 507 | 430/430 (427), 8 d | 15.43 / 10.23 | 31 / 10; 278 |
| 5 | 0.002 / 1 | 99.89% | 5,947 → 5,925 (4,492); 5,788 | 25,224 → 24,638 | 0.483 → 0.501 | 10.2% | 582 / 227 (67) / 195 | 8 / 6 / 8 (0) | 122 / 405 / 55, 34.7 | 501 | 430/430 (428), 8 d | 15.49 / 10.27 | 31 / 10; 255 |
| 6 | 0.000 / 1 | 99.90% | 5,947 → 5,886 (4,942); 5,835 | 25,030 → 24,758 | 0.489 → 0.501 | 10.1% | 587 / 224 (65) / 194 | 15 / 5 / 15 (0) | 122 / 413 / 52, 34.4 | 496 | 430/430 (428), 9 d | 15.65 / 10.35 | 31 / 10; 282 |
| 7 | 0.008 / 2 | 99.90% | 5,938 → 5,822 (4,001); 5,544 | 25,311 → 24,676 | 0.481 → 0.503 | 10.5% | 593 / 223 (67) / 193 | 19 / 6 / 19 (0) | 131 / 414 / 48, 33.8 | 490 | 430/430 (427), 8 d | 15.68 / 10.34 | 31 / 10; 354 |
| 8 | 0.015 / 16 | 99.91% | 5,957 → 5,619 (1,942); 5,081 | 25,356 → 24,671 | 0.483 → 0.509 | 10.8% | 598 / 224 (68) / 193 | 16 / 5 / 16 (0) | 136 / 413 / 49, 33.4 | 485 | 430/430 (427), 8 d | 15.68 / 10.36 | 31 / 10; 406 |
| 9 | 0.004 / 3 | 99.92% | 5,796 → 5,901 (2,068); 4,667 | 25,313 → 24,694 | 0.482 → 0.503 | 11.0% | 601 / 224 (71) / 195 | 20 / 3 / 20 (0) | 142 / 411 / 48, 32.7 | 482 | 430/430 (428), 9 d | 15.81 / 10.44 | 33 / 10; 475 |
| 10 | 0.038 / 17 | 99.89% | 5,946 → 5,900 (2,839); 5,160 | 25,532 → 24,590 | 0.475 → 0.500 | 10.2% | 610 / 224 (72) / 198 | 17 / 9 / 17 (0) | 155 / 407 / 48, 32.2 | 473 | 430/430 (428), 8 d | 15.81 / 10.44 | 33 / 10; 464 |
| 11 | 0.010 / 4 | 99.93% | 5,961 → 5,924 (3,554); 5,343 | 25,039 → 24,350 | 0.474 → 0.492 | 10.1% | 617 / 224 (73) / 198 | 14 / 7 / 14 (0) | 166 / 401 / 50, 31.8 | 466 | 430/430 (426), 8 d | 15.89 / 10.54 | 33 / 10; 480 |
| 12 | 0.009 / 5 | 99.92% | 5,983 → 5,828 (3,021); 5,071 | 25,042 → 24,502 | 0.474 → 0.496 | 10.3% | 623 / 221 (73) / 197 | 13 / 6 / 13 (0) | 174 / 401 / 48, 31.7 | 460 | 428/430 (427), 8 d | 15.96 / 10.49 | 33 / 10; 495 |

(The previous long runs, for comparison: the treasury's lows 1,763-4,690p and 2,126p and 1,930p in years 5 and 6; the
Gini .512 → .543 from year 4; households 233 → 261 and pups 93 → 66 in six years; 57 newcomers and 1,710p made.)

**What settles (the six years, and the twelve's economy).**

- **Hunger, wages paid, purses, Gini**: hunger .000-.004 in years 1-6 and .004-.015 after, but for year 10's .038 (two
  one-day spikes, on winter days 322-323 of years 8 and 10: 16 and 17 hungry for a day, 2 and 8 households short); wages
  paid 99.73-99.93%, rising. The households' purses at each year's end 24,350-25,470p, no trend after year 1 (a slow
  drift of about -40p a year after year 6). **The Gini of earning households, the year's end: .505, .508, .506, .492,
  .501, .501, then .503, .509, .503, .500, .492, .496: flat**, where the newcomers' runs rose .012 a year from year 4.
  The owners' share 16.4% → 10.1-11.0%, flat after year 3.
- **The treasury** (the fix): its 4-week means at each year's end 5,820-5,940p; its lows 1,803p (year 1, the new world
  filling), then 3,834, 3,627, 4,214, 4,492, **4,942p by year 6: no swing**. Rule 4 moved once in six years, the first
  year's autumn (the level 4,280p, under 4,675p: the toll firmed a point to 31) and then watched: the level 4,857 →
  5,835p, inside the band. **Settles.** The twelve years: see below.
- **The posts**: every post that pays (430) held but for a few days each, the longest empty 8-9 days every year (the
  eight days' wait and a day), the fewest held 426-429 of 430. All posts (with the pups' and elders', lessons and play)
  508 → 443: the grown pups leave their lessons for work, and those aren't posts anyone is sent for.
- **The wages and prices**: a keeper's mean 15.34 → 15.65p in six years, 15.96p in twelve; a hand's 9.90 → 10.35p,
  10.49p (both still creeping up about 0.05p a year); 6-7 tills at the top, 10-13 at the foot. The price level 0.993-1.003
  every day; the factors' spread (the year's mean sd) .078, .090, .090, .090, .090, .096, then .100-.110 (still widening
  a little, as before). Shelves 419-428, a mean of 10.7-11.4. **Settle, nearly**, as in the last runs.
- **The city's people**: residents 559 → 587 (six years) → 623 (twelve), **the arrivals and nothing else** (births equal
  deaths); households 243 → 224 → 221 (arrivals lodge with households: none at the work, none alone), households of one
  76 → 65 → 73; earning households 193-198.

**What doesn't settle in twelve years: the age of the city, from births following deaths.** The deaths are of the old
(the founding city is old-heavy: 41 retired, 15-20 deaths a year), and each is replaced by a newborn, who is a pup for 16
years. So **pups rise 97 → 122 (year 6) → 174 (year 12)**, from 17% of the city to 28%; the working age falls 425 → 413 →
401; the mean age 36.3 → 34.4 → 31.7. The city keeps every post filled (the arrivals, five a year, are the working-age
wolves it lacks). It costs the treasury: the alms are per head, and the pups' households are the poor, so the alms rose
from 255-347p a day (years 1-6) to 354, 406, 475, 464, 480, 495p (years 7-12); the treasury's level fell 5,835p →
4,667p in years 7-9, its lows 1,942p and 2,068p in years 8 and 9, and rule 4 did what it is for: **the toll firmed four
half points, 31 → 33, over days 3,135-3,219 (year 9), and the level came back inside the band** (4,700-5,600p) and held
for the last three years without another step. The treasury's year-end means stay 5,620-5,980p. This is a transition,
not a spiral: once the first pups born in the run come of age (year 16 on) the working age rises again and the deaths
fall to what a younger city has. It is the rule as the user set it (births match deaths, in number); a population held
level by count, not by age, gets younger before it settles.

**Stable, then, by the brief's tests** for the six years (money conserved, hunger low, wages paid over 99.7%, no trend in
the treasury, the purses or the Gini, the drifting values settled or nearly, births equal to deaths, every post filled),
and for the economy over the twelve; **the age structure is still moving at year 12**, and with it the alms and the toll
(once). No balance change was made for it.

#### Left for the user

- **The city gets younger**: pups 17% → 28% in twelve years, the alms 75% higher, the toll firmed two points by year 9.
  If that's not wanted: the births could follow deaths by age band as well as by number (a death of the old owes a
  child only when the pups are fewer than the town had), or the alms could count a pup at half; or leave it to settle
  (about year 16-20).
- **The unplaced pool**: 64 of 524 used in twelve years (about five a year). At this rate the working-age ones of each
  class last for decades; Atlas could keep it stocked.
- **An Atlas save** opened before an arrival and saved after it would unplace the arrival again (a whole-world save
  writes every NPC's places, and the server's write raises no revision, as with births: `world_store`'s
  `keep_made_by_the_server` keeps only the NPCs the server made). Not met yet: no server has run DEV since; it needs a
  marker for "placed by the game" if DEV is served while Atlas edits it.
- **A world from files** (a scratch server, the harness) records an arrival only in its save: its export still has the
  NPC unplaced, so a restart from that save into the same export was not tested. Only the database's world records the
  move in its people (the files are an export).
- The wages and the price spread still creep a little (0.05p a year; the spread .096 → .110 in years 6-12), as in the
  last runs.

### The Country's food farms: Phases A and B of doc 71 (2026-11-01)

Branch `r2-farms`, from `ridgemere2` (268c455). The user, 2026-11-01, on doc 71's recommendations (verbatim): **"Yes to
all your suggestions"**. Doc 71, Progress, has the whole of it; for this loop:

- **The farms are producers of the loop** (`Core/RatwFarms.cpp`), each a till (`farm:<id>`) with a virtual crew, its
  household `crew:<id>`: an area switched on (its own switch, else the schedule: one area every `every` days from
  `start`; none by default) takes its goods from the Country. The shops' store draws of them come from the farms'
  stock (`drawFromStore`: refused past it, never capped by the season's supply; `countryLeft` is their stock), paid to
  the farms' tills less the gate toll, carriage in as before; the season's price, or the granary's price a year.
  This is section 2's per-good switch with its producers: an area's goods are refused to the Country while it is on.
  "Country when short" stays unbuilt.
- **New flows of the loop's day**: at the first loop hour of a day, the farms grow their yields (by season), the
  Country's own people eat at their tables and what is past keeping spoils, and the crews eat; each shop hour, after
  the Country's basket, the farms' baskets (the same routine, `basketBuys`); at 18:00, after the city's wages, the
  farms' (`payFarms`), whose short tills join the treasury's wage support (`supportWages`, the crew its post). The
  crews shop as households of their size, counted apart from the residents (no hunger measure counts them).
- **Money**: a farm's float and its crew's settled purse are made when its area first goes on, recorded as the
  world's founding money is (minted: `conserved()` stays exact). Nothing else is made.
- **The Country's basket** moves to its re-weighted shares (`country_farms.json` `countryBasket`: the goods the farms
  don't buy, no food or drink) as the areas go on, by their share of the food's takings; with none on, as it was.
- **The inns' stew** is raw meat and bones, and vegetables are gone from the city's crafts, supplies and season groups
  (the item stays for the old map).
- **Settings**: `live.economy.farms` (migration 0060, not applied to any world), the `farms`, `farmarea` and `farm`
  records, `POST /api/atlas/economy/farms` and `/api/dm/economy/farms` (audited, economy.sync), the Farms panel on Atlas's
  Economy page and the DM's Economy tab.
- **Reports**: `world_check --farms DAY [--farms-every N]`; the economy line gains the farms by area and by farm, their
  crews' spending, and the treasury's day (its least, its nights under the cap, the support wanted and unpaid, the
  switch's apart from the ordinary, the surplus paid, the toll's and alms' trim, the money minted);
  `tools/farms_summary.py` the years.
- **Tests**: `farms_tests` (536 checks), `sim_fast_tests` (the economy line with the farms), `store_room_tests`' fixture;
  all 77 C++ tests after a clean build; the Python suites (the known Greyfen 11-vs-10 apart), Editor `tsc` and tests.
- **No run was made**: the 3-year runs need the user's go-ahead (doc 71, Progress, "The 3-year runs"). With no area on,
  the loop is as before but for the stew's bones (about five a day: the inns' stew drew vegetables) and the vegetables'
  season group gone.
