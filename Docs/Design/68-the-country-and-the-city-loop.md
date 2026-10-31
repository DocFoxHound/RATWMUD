# 68. The Country and the city's loop (2026-10-31)

The user, 2026-10-31 (verbatim):

- On the store room (doc 67, 6): "my idea is to use it in place of having farmers and mines and whatnot ahead of us
  actually implementing them." They asked how the store's money gets back into circulation.
- On the proposal below: "Yeah I like that idea for The County. Do that". (The Country.)
- "One more thing we need to make sure we are factoring into item costs from the country and sales TO the country is
  caravan/wagon deliveries. Those wolves need to be paid too."
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

All of this is the code on `ridgemere2` at ed9d4f3 (doc 67's dense city), and Ridgemere 2.0's export at build 43.

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
- **Who is in the city** (`tools/city_loop.py` on the export): 559 residents placed, 477 of them 16 or over, in 224
  homes; 108 shop keepers (88 shops alone in their room, and three rooms that hold several: Market Square's 8 stalls,
  the Market Hall's 6 counters, the Rag Market's 6), 30 guards, 112 hands working in a room with exactly one shop, and
  the rest civilians with posts at offices, the palace, the courts, the church, the wharf and the Houses.
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

**How much** (`tools/city_loop.py` on build 43, everyone fed, at the defaults below):

| Flow | p a day |
|---|---|
| Residents to shops (food 3,517, needs 813) | 4,329 |
| Shops' store draws for those goods | 2,776 |
| The Country's purchases (its basket, at the makers' 40% margin) | 1,830 |
| Shops' store draws to replace them | 1,091 |
| All draws | 3,866 |
| of which the gate toll to the treasury (32%) | 1,237 |
| of which the Country's intake | 2,629 |
| The Country pays its posts | 784 |
| The Country's purchases, as above | 1,830 |
| Carriage on the draws (13,669 weight in) | 82 |
| Carriage on the Country's purchases (2,536 weight out) | 15 |
| Wages from tills (keepers 108 × 14, hands 112 × 8) | 2,408 |
| Wages from the treasury (153 × 8: 123 public posts, 30 of the watch) | 1,224 |
| Wages from the Country (98 × 8) | 784 |
| **All wages** | **4,416** |

**The arithmetic that balances.** In a steady day every account takes in what it pays out:

- *The Country:* `(1 − toll) × draws = its posts' wages + its purchases + carriage out`. The draws include the inputs of
  its own purchases, `(1 − m) × P` where `m` is the makers' margin on its basket, so its purchases are
  `P = ((1 − toll) × D_residents − W_country) / ((1 + carriage a penny) − (1 − toll)(1 − m))`
  `= (0.68 × 2,776 − 784) / (1.008 − 0.68 × 0.596) = 1,830p`.
  *The Country's purchases are its net intake divided by the makers' margin*: each penny it spends comes back to it as
  the shop restocks, so with no toll and no posts it would have to buy 2,776 / 0.40 ≈ 6,900p a day, more than the
  city's spare making. The toll and its own posts are what keep its buying inside what the city can make.
- *The treasury:* `toll × draws = 0.32 × 3,866 = 1,237p` against 1,224p of public wages.
- *The shops:* their net is `1,554p` from the residents and `0.404 × 1,830 = 739p` from the Country, `2,293p`,
  against `2,408p` of keepers' and hands' wages; the carters take `97p` against their own crew's.
- *The residents:* wages in, `4,416p`; spending, `4,329p`. Wages paid equal what residents spend because every other
  account spends what it takes in; the small gaps (115p shops, 87p residents) are inside what the rules below do when
  money is short (a till pays what it can above its float) or over (savings buy comforts).

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
  62 goods);
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
`country.json` is that suggestion for build 43, the goods under 10p dropped. It is re-run when the city changes (the
food rebalance), and Atlas's Economy page shows it.

**Per trade: the Country's purchases against each trade's time** (build 43; `use` is all the hours the residents and the
Country ask of the trade over its crew's 8 hours; over 70% is flagged):

| Trade | Crew | Hours | Residents' | The Country's | Use |
|---|---|---|---|---|---|
| smithy | 10 | 80 | 6.8 | 43.7 | 63% |
| weaver | 5 | 40 | 12.4 | 15.5 | 70% |
| inn | 26 | 208 | 124.7 | 16.7 | 68% |
| foundry | 3 | 24 | 0 | 11.4 | 47% |
| plasterworks | 3 | 24 | 0 | 11.1 | 46% |
| apothecary, scriptorium | 4, 2 | 32, 16 | 3.2, 0 | 10.9, 7.0 | 44% |
| builders' works, pottery | 3, 3 | 24, 24 | 0.6, 3.0 | 8.6, 6.5 | 39% |
| fishmonger | 8 | 64 | 17.5 | 6.8 | 38% |
| brickworks, glassworks, distillery, tannery, cartographer, cartwright, fletcher | | | | | 35% |
| general, jeweler, tinker, carpenter, mason, ironworks, paintworks, herbalist | | | | | 31-33% |
| brewery, press, sawmill, cooperage, tailor, papermill, saddlery, printing house | | | | | 20-30% |
| ropewalk, charcoal, perfumer, armory | | | | | 12-15% |
| **Over 70% from the residents alone** | | | | | |
| bakery | 8 | 64 | 90.3 | 0 | 141% |
| provisioner | 7 | 56 | 88.6 | 6.0 | 169% |
| smokehouse | 2 | 16 | 24.4 | 0.8 | 158% |
| butcher | 4 | 32 | 64.3 | 0 | 201% |
| dairy | 2 | 16 | 65.2 | 0 | 408% |
| preserver | 2 | 16 | 81.7 | 0 | 511% |
| chandlery | 5 | 40 | 32.1 | 3.4 | 89% |

- No trade is pushed over 70% by the Country. The flagged trades are the food makers (and the chandlery's candles),
  over capacity from the residents' own food: if everyone ate the city's made food, the bakers, butchers, dairy and
  preserver would need two to five times their crews. That is the food rebalance the other session is doing now. Until
  it lands, the residents eat what the fishmongers and provisioners resell (fish, raw meat), which needs no making
  hours (section 3).
- Trades the Country and the residents together leave with no buyers at all: the moneychanger, the pawnbroker, the
  shipwright, the stables, the Market stalls (their supply lists are flour, oats, milk, vegetables and apples, nothing
  eaten as it is), the vintner, the barber, the builders' yard and the rag-and-bone yards. They sell little or nothing,
  so their keepers' wages go unpaid once their tills run down (doc 65's "too few buyers"). Listed for the user and the
  rebalance; the loop doesn't prop them up.
- The Country spends 1,830p a day, about half of the 3,664p the makers could spare at 70%: room to grow.

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
- **A household** is the residents who share a home (`household()`); in a communal home (barracks, quarters,
  bunkhouse) each resident is its own.
- **Food, once a day.** Each household has its market hour, 8 + (a hash of its home) mod 10. At that hour every member
  is topped up to `food.days` (2) days of food; a member still under `food.shortDays` (1) tries again every hour until
  18:00. Food goes into each member's own purse (where the eating branch looks); it is paid for by whichever member has
  the most money and can pay (children and the retired are fed by whoever earns).
- **What food.** Among the foods the household's community's open shops hold (an open shop: its keeper at its post,
  `trade`), picked at random weighted by nourishment for the price, so cheap filling food is bought most but every food
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

On build 43 that is 108 keepers, 112 hands, 30 watch, 123 public posts, 98 the Country's: **471 paid posts**.

**Paying, at 18:00.** Each payer pays every holder of its posts once a day, every day (a week's wage spread over its
days, Restday too). A till pays from what it holds above its **float** (20p, or what it drew from the store yesterday if
more: its working money); short of the whole, it pays each post the same share of its wage. The treasury keeps 200p
back (players' welcome grants); the Country its reserve. A till holding more than its float and a week of its wages
pays its keeper the rest as **profit**.

**The numbers** are set from the city's spending, not doc 64's 60p: at today's catalogue prices a resident needs about
6.3p of food and 1.5p of needs a day, and a household has about 2.1 earners for 2.5 mouths, so a hand's 8p feeds a
family with a little over, and a keeper's 14p is the value a busy maker adds in a day (crafts add 1.5-6p an hour,
`tools/city_loop.py`). All four are settings.

## 5. The treasury

**Funded by a gate toll** (decided for now; the user may change it): `gateToll` percent (32) of every store draw's cost
goes to the treasury instead of the Country, as a city's toll on goods brought in through its gates. The till pays the
same; only the Country's share is smaller. It is conserved, it grows with trade, and it costs no new rule. At 32% it
matches the public wages (1,237p against 1,224p).

- Rejected for now: a toll on every sale (it changes what shops keep, and players would pay it); a tax on purses
  (doc 42's, removed by Phase 5); the Country paying the Crown a rent (the same money by a longer road).
- The institutions' baskets (`crafts.json`'s `institutions`: the Town Works, the watch's bread, the church's candles)
  are not bought: the treasury pays people, not goods, for now (section 10).

## 6. Carriage

The user: deliveries from the Country and to it are carried, and the carters are paid.

- **The rate**, `carriage` (6p a wagon load), a load being 1,000 of items.json's `weight`. Every store draw pays
  carriage on the weight it brings in; every Country purchase pays carriage on the weight it takes out. Pennies are
  whole: each payer keeps its fraction of a penny owed (not saved) and pays the whole pennies as they come.
- **Inbound, the shop pays** on top of the goods' cost (it is the shop's delivery). **Outbound, the Country pays**, out of
  its purse, so its spending on goods shrinks by that much (the buyer pays the freight). The seller nets the full
  catalogue price: makers' margins are thin (40% on the basket, 25% for resellers), and carriage taken from them would
  fall hardest on the trades with the fewest buyers.
- **To the carters**: the till of the `carters` business (the Carters' Yard the other session is adding by a city gate:
  a keeper and drivers, "driving for the Carters' Yard"); its drivers are its hands (they work in its room) and are paid
  like any business's. With several yards, carriage goes to the one holding least. A world without one pays
  `fund:carriage` instead (conserved; flagged in `world_check` and Atlas).
- **Sized** (build 43, `tools/city_loop.py`): 13,669 weight in and 2,536 out a day, **16.2 wagon loads**; at two round
  trips a driver a day, **9 drivers**. Carriage brings in 97p a day (82p in, 15p out) against a keeper's 14p and nine
  drivers' 72p, 86p. Carriage adds 3% to the cost of the city's inputs (82p on 2,776p), more on the heavy cheap goods
  (firewood 0.09p on a 1p piece, bricks 0.24p on a 4p one). The drivers' wages come back as spending like
  everyone's. **For the city: one Carters' Yard with a keeper and 9 drivers.**
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
| `gateToll` | 32 | 0-90 | percent of each draw's cost to the treasury |
| `carriage` | 6 | 0-1,000 | pennies a wagon load (1,000 weight), in and out |
| `wageKeeper`, `wageHand`, `wagePublic`, `wageCountry` | 14, 8, 8, 8 | 0-1,000 | a day's wage |
| `countryOff` | none | goods | goods the Country no longer supplies |

**Migration 0056** (`Database/migrations/0056_country.sql`): the columns on `live.economy` (`needs_paused`, `country`,
`country_reserve`, `gate_toll`, `carriage`, `wage_keeper`, `wage_hand`, `wage_public`, `wage_country`,
`country_off text[]`), and `live.people_manifest` again with one line after the `store` line, only when a value differs
from its default:

    loop needs on|off country on|off reserve N toll N carriage N wages K H P C

and one `countryoff "<item>"` line for each good the Country no longer supplies. Core parses both
(`RatwAuthoring.cpp`, beside `store`); `Game::withoutPeople` strips them. Worlds without 0056 read the defaults, and a
save of a non-default setting there is refused ("this world lacks migration 0056"), as doc 67's 0055 does. Applied only
to `ratw_ridgemere2` (and test worlds); DEV and PROD are the user's to migrate.

**Atlas's Economy page** (`Editor/src/studio/EconomyControls.tsx`) gains:

- **The city's loop:** residents' needs on or paused; the four wages.
- **The Country:** on or off, its reserve, the gate toll, the carriage rate; its basket (from `country.json`: each good,
  its share, its makers); per good, whether the Country supplies it (every good any business draws, with "All on").
- A note when the restocking rule is off: "the loop needs the restocking rule on, or shops sell out and stay empty".

Routes: `GET /api/atlas/economy` returns the new settings and the basket; `POST /api/atlas/economy/loop` sets them;
`POST /api/atlas/economy/goods` `{set: {item: bool}}` the per-good supply.

**The DM's money view** (`tools/dungeon_master.py`) lists the `outside:` accounts beside `fund:` ones, and the carters'
tills under their own heading, so the Country's purse and the carters' takings show on a live world. `world_check`
prints the Country's purse, the treasury, the carters' takings and the economy's day (below).

## 10. Stability gates and how to measure them

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

1. **The toll.** The treasury lives on a 32% gate toll on what the shops draw from the Country. Decided for now; a toll
   on sales, or the Country paying the Crown a rent, are the other choices.
2. **Who the Country pays.** The dockers, cranes, Sump carriers and the Country's warehouses, and the Houses' people
   (98 posts) are paid by the Country, as the people of its trade and its estates. Decided for now; the other choice is
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

## 12. Trades we could add for exports

Doc 67 Q7 left out four trades; the user says they could be added if the city needs more industry for the Country.
Recommendation only; nothing is added here.

| Trade | What it needs | What it would sell the Country | Needed? |
|---|---|---|---|
| **Locksmith** | a `locksmith` business (`businesses.json`); `lock` (15p) already exists with an NPC craft (iron bar, 2.8 h) whose makers are the smithy and tinker; add `locksmith` to its makers, and to `chain` and `fittings` | locks, chain, fittings | the cheapest to add (no new item or craft) |
| **Cobbler** | a `cobbler` business; footwear exists as paw wear (`paw_boots` 16p, `hobnail_boots` 22p, `leather_bindings`) with player recipes; NPC crafts for `paw_boots` (leather) and `hobnail_boots` (leather, nails) in `crafts.json` | boots for country folk | would take load off the tannery and saddlery |
| **Dyer** | the `dyeworks` business exists (sells `dyed_cloth`, `scarlet_cloth`) with no NPC crafts; the player recipes take three inputs (wool cloth, a dye, alum) and NPC crafts take at most two (`RatwItems.cpp`), so a two-input craft (wool cloth and alum, say) or a wider craft | dyed cloth | needs a catalogue change |
| **Bookseller / stationer** | a reseller of paper, ink, pamphlets, ledgers and maps (a `supplies` list); no new items | nothing of its own: the Country buys the city's make from its makers | not for exports |

**Are they needed?** Not by these numbers: no trade is over 70% from the Country's buying, and the Country can spend
all it takes in (1,830p a day against 3,664p the makers could spare). They become worth adding if the Country's
spending grows (a lower toll, higher prices) or the smithy (63%) and the weaver (70%) fill up: the locksmith first, then
the cobbler.

## Progress

(Filled in as it is built.)
