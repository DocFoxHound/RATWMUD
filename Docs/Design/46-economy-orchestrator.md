# 46. The economy orchestrator

Planned 2026-10-06. It follows doc 42 (money in circulation) and replaces most of what doc 42 built for the **big
holders**: shops, farms, great houses, churches, towns and cities. Residents keep their own decisions. Every number is a
*placeholder* for the balance pass.

## The user's brief (2026-10-06)

> "I've been having a hard time engineering and hard coding in an economy. … I think I'm having a hard time finding a
> system that automatically balances itself out."

- **Residents keep deciding** where to buy, what to buy, when to save and when to eat.
- **The big holders stop deciding.** Shops, towns, churches, cities, farms and houses, the places that collect a lot of
  money, lose their own spending rules.
- **A new economy manager process** decides for them:
  - it runs on its own thread, so it doesn't slow the game;
  - it manages prices, demand and the movement of money;
  - it orchestrates how those centres spend.
- **A little ham-fisted with hoarders.** A centre may build up some wealth. When too many wolves are hungry or poor, the
  orchestrator makes the rich centres spend.
- **Not a handout.** The money doesn't go straight to the poor. It goes to things that reach them at one or two removes:
  work, orders, food bought from their farms. "This needs to be a system, not just a direct take money from rich source
  and give it to poor source."

**This changes one earlier decision.** On 2026-10-05 the user said "no ham-fisted fixes". That now holds only for
**making money or food out of nothing**: the orchestrator never mints or destroys a penny. Forcing a rich holder to spend
is now wanted.

## Decisions (the user, 2026-10-06)

1. **Every business keeps its own till**, the keeper or farmer on a wage and a share of profit (Part 3).
2. **The church's coin dole stays,** as a floor beside the channels, paid from the church's purse as now. Food alms at
   the counter stay too.
3. **One price per good per town,** set by the orchestrator (Part 4).
4. **Invisible in the world.** Nobody issues the orders in the fiction; they just happen. No writs or news of them.

## Why the rules so far keep failing

Doc 42 grew about forty separate rules: surplus shares, valves, floors, flush discounts, takings caps, props, the dole,
the capital's share, wage scales, markdowns and trials. Each fixed the last pool found, and the money then pooled
somewhere else. The month runs show three reasons:

1. **Every holder decides alone, from its own purse.** No rule sees the whole land. A great house can't know that
   Saltreach is starving; it only knows its own floor.
2. **Inflow follows activity; outflow follows need** (doc 42, "What the runs showed"). A holder whose takings grow with
   trade but whose spending is set by a fixed need will always pool. Every fix so far patched one holder's outflow.
3. **The rules fight each other.** Markdowns, flush discounts, town food factors and supply factors all touch the same
   price. Wage scales, takings caps and wage shares all touch the same wage. Nobody owns the result.

The fix is structural: **one controller that sees the whole land and sets the dials, and holders that just carry
out what it sets.** The holders still do the real work: shops still craft and sell, farms still bring in, towns still
hire for their works. What changes is who decides how much, at what price, and where the surplus goes.

All of doc 42's institutional rules run today in one serial pass on the game thread, once a game day
(`Society::decide`, RatwSociety.cpp, when the day turns: reckoning, houses, surpluses, households, buyers, upkeep,
producers' sales). Only residents' deciding is threaded. That pass is where the orchestrator's brief will be applied.

## The shape of it

```
           game thread                                      orchestrator thread
  ┌──────────────────────────────┐                   ┌──────────────────────────────────┐
  │ residents decide (threaded)  │                   │                                  │
  │ holders carry out the brief  │──── snapshot ────▶│  read the land (sensors)         │
  │ ledger counts every flow     │   (23:00 daily)   │  where is it hurting? (distress) │
  │                              │                   │  set prices, wages, margins      │
  │ apply the brief as the day   │◀──── brief ───────│  set each holder's band          │
  │ turns, before the daily pass │  (at the turn)    │  send excess down the channels   │
  └──────────────────────────────┘                   │  learn which channels reach      │
                                                     └──────────────────────────────────┘
```

- **The snapshot** is what the orchestrator reads: a compact copy of the numbers it needs, taken on the game thread.
- **The brief** is what it writes back: prices, wages, margins, budgets and orders for every holder.
- **It measures every day and acts once a week** (the user, 2026-10-06). Each day's snapshot is a measure: distress,
  what each holder spent, where each stands in its band. The evening of the weekly reckoning, **after the taxes and
  tithes**, it also decides, weighing the whole week's measures: the pot, its orders, prices and the margin. Acting
  daily would overcorrect; a week gives it a better picture.

## Part 1: its own thread, with the same results

The game's runs are repeatable (`--deterministic`, the same digest on any number of threads; doc 31). The orchestrator
must keep that, so it can't read live state while the game changes it, and its answer can't depend on when it
finishes.

- **Measured once a day, an hour ahead; decided once a week.**
  - At 23:00 each game day (`SnapshotHour`), the game thread takes the snapshot and hands it over. This is a copy of
    flat arrays: about 2,000 purses and some 20,000 counters. It costs well under a millisecond.
  - The orchestrator works on it during the day's last hour, while the game plays on.
  - When the day turns, the game applies the brief at the start of the daily pass, before the reckoning and the
    holders' own work. If it isn't ready, the game **waits** for it; it should take a few milliseconds against an hour
    that lasts seconds even at full fast-forward. The game never goes on without it.
  - So the brief depends only on the snapshot, never on timing: same rules, same results.
  - A world started after 23:00 takes its snapshot at once.
  - **The week's decisions** come in the snapshot taken the evening of the weekly reckoning (Dawnday, when taxes and
    tithes are taken: `Society::reckon`). That brief is the week's **decision**; the others are the day's **measures**.
    The decision weighs each town's mean distress over the week, and the kind of trouble it mostly had.
  - A new or restored world counts a day before it puts holders in bands, and three before it judges who is idle.
- **A pure function.** `plan(snapshot, memory) → (brief, memory')`. Its memory holds the running averages, the
  channels' learned reach and the prices. It goes in the checkpoint with the society, so a restart carries on where it
  left off. Tests can feed it a made-up snapshot and check the brief.
- **Counters, not scans.** `Society::record` already sees every flow. It adds each flow to a counter keyed by
  kind, payer class, payee class, payee's wealth band and town. That costs the same as `noteOutgoing` does now. The
  snapshot copies the counters and clears them.
- **Off switch.** `--orchestrator off` (and `RATW_ORCHESTRATOR=off`) applies a fixed brief instead: catalogue prices,
  base wages, no channels. Unit tests run that way unless they test the orchestrator.
- **Never makes or loses money.** A brief only names moves between accounts that exist, each booked in the ledger
  under an `orders:` kind. `conserved()` is checked every day in tests and in econ_watch.

**In code:** `Core/RatwOrchestrator.{h,cpp}` holds the thread, `Snapshot`, `Brief` and `plan`. The snapshot is taken
and the brief applied in `Society::decide`: the snapshot at `SnapshotHour`, the brief at the start of the daily pass. Dials live in
`Data/Economy/orchestrator.json`.

## Part 2: reading the land (the sensors)

Everything is **relative**: to a day's food at the town's prices, to the land's median purse, or to a holder's own
outgoings. Then the same rules work with 200,000p or 2,000,000p in the land.

**For each resident** (copied flat):
- purse, and its household's purse;
- days of food at home, and whether it is hungry or starving;
- what it earned in the last 7 days, and from what;
- whether it is working, idle, a dependent (child, elder, homemaker) or looking for work.

**For each town each day**, worked out from those:

| Sensor | What it is |
|---|---|
| Food cost | What a day's plain food costs a grown wolf in this town's shops |
| Hunger | Share hungry; share starving |
| Short | Share of households with under a week's food, at home and in purse together |
| Poor | Share of residents whose purse is under 7 days' food cost |
| Idle | Wolves able to work who earned nothing in 3 days, against unfilled hires and odd jobs |
| Shop food | Days of food in the town's shops for its people |
| Takings | The town's shops' takings against their running costs (are its businesses failing?) |
| Inflow | Coins into the town from outside in 7 days, less coins out |

**For each holder** (every till, farm, house, church, treasury and buyer):
- its cash;
- its **need**: its own ordinary outgoings over the last 14 days (wages, stock, materials, upkeep, rent; never its
  orchestrated spending), with a floor by kind;
- its inflow and outflow averages.

**For each good in each town:** stock in shops, made or brought in, sold, and missed sales (doc 42's `soldToday_`).

### Distress

Each town gets a **distress score**, 0 (all well) to 1 (crisis), and a **kind** of distress, since each kind needs a
different remedy:

| Kind | Signs | What helps |
|---|---|---|
| **Empty shelves** | Hunger with little food in the shops | Food bought and brought in; work for its farms |
| **Empty purses** | Hunger with food in the shops | Work: works, hires, orders to its makers; cheaper food |
| **No work** | Many idle, few hires | Works, hire grants, opening a shut business |
| **Failing trade** | Its shops' takings below their costs | Orders to its makers; trade from the cities |
| **Draining** | More coins leave than come in, week on week | Trade orders for what it makes |

Distress = the weighted sensors, smoothed over three days so one bad Restday doesn't swing it. The **land's distress**
is the towns' weighted by their people. The orchestrator spends hard only when the land's distress is high, or when a
holder is over its cap whatever the land's state.

## Part 3: what the big holders stop deciding

Each holder now carries out its **brief**. The work stays real; only the deciding moves. From doc 42's rules (`Core/`
file:line as of 70db3a4):

| Today the holder decides | Where | Now |
|---|---|---|
| Shop prices: markdowns, flush discounts, town food factor, shelf factor | `shopPrice` RatwDemand.cpp:270, `tendPrices` :213 | **The town's price** for each good (Part 4) |
| Market price factors | `World::tendPrices` RatwRoads.cpp:410 | Folded into the town's price |
| What producers are paid (0.55 × catalogue × markdown) | `producersSell` RatwDemand.cpp:908, `sellBroughtIn` | **The margin** (Part 4) |
| Shop help's wage share, takings cap | RatwSurplus.cpp:462 | **The wage table** (Part 5) |
| Hire pay rising and falling | RatwSurplus.cpp:99 | The wage table |
| Town wage scale, town budget | RatwSurplus.cpp:133, :40 | The wage table, and the town's budget |
| `wageFor`: wage by the payer's wealth | RatwWages.cpp:159 | The wage table; the payer only pays it |
| Surplus reserve and spending (towns, church, houses) | `spendSurpluses` RatwSurplus.cpp:283–458 | **Bands and channels** (Parts 6, 7) |
| Owner-keeper surplus, rich producers' investment | RatwSurplus.cpp:497, :554 | Bands and channels |
| House props, house surplus, patronage | RatwHouses.cpp:146, RatwSurplus.cpp:382 | Bands and channels; props become a channel |
| The capital's share for the poor | RatwReckoning.cpp:138 | Bands and channels |
| The church's surplus | RatwSurplus.cpp:348 | Bands and channels |
| Odd-job pay by richness | `postOddJobs` RatwOddJobs.cpp:28 | The wage table; odd jobs become a channel |
| The subsidiser | RatwOddJobs.cpp:13 | Wage support, a channel |
| Farmhand hires when the farm feels rich | `postFarmHires` RatwFarmhands.cpp:54 | A hire budget in the farm's brief |
| Every `RATW_TRIAL` | | Gone |

**What stays as it is:**
- **Residents:** eating, buying food and choosing where, the household purse and stocking, stipends, wants, odd jobs
  and hires taken, the plate, beggars' pennies, saving.
- **The work:** producing, crafting, restocking by what sells, curing, spoilage, caravans running, contracts carried.
- **The weekly reckoning:** tax and tithe on profit stay steady (the user's decision of 2026-10-06), and so do the
  wealth tithe, rent and market dues. They are flows **into** holders, and the orchestrator manages what holders do
  with it.
- **The church's dole** (2p a day a member to short households, RatwSurplus.cpp:207) and **food alms for the
  starving** at the counter, both paid by the church: the floor under everything (decision 2).

### Every business keeps its own till

A house-owned shop already has a till (`till:<position>`). An owner-run shop or a farm doesn't: its money is the
keeper's or farmer's own purse (`tillOf` returns the keeper). That is why "shopkeepers' own purses" and "producers" pool
in every run, and why the orchestrator can't touch it without touching a resident.

- **Every shop, workshop, farm and site gets a till.** What it sells goes in. What it buys, its wages and its upkeep
  come out.
- **The keeper or farmer draws a wage** from it, from the wage table (more than its help's), plus a **share of the
  week's profit** (placeholder: a third).
- What the keeper does with its own purse is its own decision, like any resident.
- The till is a holder like any other: it has a band, and its excess goes down the channels.
- Founding moves the business's cash above a month's living into the new till, once, and books it as a move, not new
  money.

## Part 4: prices and margins

**One price per good per town**, set by the orchestrator. Every shop in the town sells at it, times quality (crude ×0.6,
fine ×1.6, master ×3). Residents still choose which shop: the nearest, the one that has it, the one they like.

- **Supply and demand.** The price moves toward catalogue × *f*(days of stock against sales and missed sales):
  - a tenth more with under two days' stock, up to ×1.6;
  - a tenth less with over ten, down to ×0.6;
  - at most 25% at a decision (a week), so it never jumps.
- **Staples are kept affordable.** Plain food and firewood have a ceiling: a day's plain food may cost no more than
  half of what a wolf in the town's lowest quarter of earners makes in a day (placeholder). When the ceiling holds the
  price below what the food cost the shop, the shop's loss is covered by **price support**, a channel (Part 7). Without
  money for it, the ceiling lifts.
- **The margin.** What a shop pays for what it buys in (from producers, suppliers and other shops) is the town's price
  times a **margin**: today 0.55 for raw goods. The margin is the orchestrator's main lever between the land and the
  towns:
  - shop tills pooling while farms are short: the margin rises (up to 0.75), so more of each sale goes to the farms;
  - farms pooling while shops fail: it falls (down to 0.4);
  - at most 0.04 a week.
- **Between towns.** Caravans and standing orders sell at the destination town's price. The 1.4 markup becomes the
  margin seen from the other side, so a scarce good pulls a caravan by its price alone.
- **Players** buy and sell at the same prices (`quote`), with stalls as now.

## Part 5: wages

**One wage table per town:** a day's pay for each kind of post (shop help, keeper, farmhand, labourer, guard, clergy,
porter, odd-job hand).

- **A living wage floor.** No post pays less than a day's food and lodging at the town's prices, times 1.2
  (placeholder). This one floor carries a lot: it is the first way money reaches the poor without being handed to them.
- **The labour market moves it above the floor** (at each week's decision):
  - unfilled posts of a kind raise its pay by a tenth a week;
  - many idle against few posts lower it toward the floor;
  - the town's distress of kind *no work* or *empty purses* holds it up.
- **Payers pay the table.** A payer that can't (a till under its float) pays what it can. The rest comes from **wage
  support**, a channel, when there's money for it. Otherwise the wolf is owed and drifts to better-paid work, as now
  (`unpaidSince`).
- **The town's wage bill** is set against its takings by its budget (Part 6). The town hires fewer when it can't pay,
  rather than paying everyone less.

## Part 6: bands, and the ham-fisted part

Every holder has a **band**, measured against its own **need** (14 days of its ordinary outgoings, with a floor by kind):

| Where its cash is | What happens |
|---|---|
| Under 1× need: **lean** | It spends only on its running: no hires beyond its brief, no channels. Not topped up; under the floor, its brief shrinks (fewer posts, smaller baskets). |
| 1× to 2× need: **comfortable** | Its own brief, nothing more. A holder may grow wealthy here. |
| 2× to 4× need: **over** | At each week's decision, a share of what is above 2× goes down the channels over the week: 40% just over, rising to 90% at 4× (placeholder). |
| Over 4× need, or over 2% of all the money in the land: **the cap** | **Everything over the cap goes down the channels that week.** The ham-fisted part. |

- **The land's distress turns it up.** When distress is high, the comfortable top falls toward 1.5× and the share
  spent from *over* doubles. When the land is well, holders keep more.
- **Never below need.** No holder is made to spend below 2× its need (or 1.5× at high distress).
- **Who counts as a holder:** every till (house-owned or not), every farm and site, every great house, the one church,
  every town treasury and the capital's, and every town buyer. Residents never: what a wolf does with its own purse is
  its own business. The wealth tithe on rich residents already exists.
- **The church** is a holder like the rest. Its tithes come in by the reckoning; its excess goes down the channels like
  a house's.
- **A house's tills** report to the house, as now: what a till holds over its band goes up to the house first, and the
  house's excess goes down the channels. A house can no longer prop its own tills just to keep money in the family:
  props are the *business rescue* channel, decided by the orchestrator.

## Part 7: the channels

A **channel** is a real piece of economic work that the orchestrator orders and pays for from a holder's excess. Money
moves from the holder into a **fund**, a holder of its own (`fund:<town>:<channel>`), and the fund's executor spends it
through the game's existing machinery. The fund is never spent on handing coins to the poor. Every channel pays
someone for work or goods, and the orchestrator picks the ones whose payees are the poor, or whose payees pay the poor.

| Channel | What happens in the game | How it reaches the poor | For distress |
|---|---|---|---|
| **Town works** | The town's works hires hands (odd jobs, 1–5 hands, split pay) and buys materials from its shops: paving, mending, walls | The idle take the jobs; materials pay shops, which pay help | No work, empty purses |
| **Hire grants** | A business in the town is paid a hand's wage for N days to take on a hand it wouldn't: shop help, farmhands, workshop hands | A new post, filled from the idle | No work |
| **Commissions** | Goods ordered from the town's makers: furniture, cloth, tools, fine goods. Kept by the payer (a house's goods) or given to the town's works | Makers' tills pay help, and buy materials from the farms | Failing trade, draining |
| **Food purchase** | Food bought from the region's farms into the town's **granary** (a store of the town's), released to its shops at the town price when shop food is low | Farms are paid, farms hire hands; shelves stay full | Empty shelves |
| **Price support** | The fund pays the shop the gap between the staple ceiling and the shop's cost | The poor's money buys more food; the shop stays solvent | Empty purses |
| **Wage support** | The fund pays the part of the wage table a lean payer can't | Wages keep coming at failing posts | No work, failing trade |
| **Trade orders** | A standing order from a rich city for what the town makes or grows | Coins come into the town for its own work | Draining, failing trade |
| **Business rescue** | A failing business is kept open: its float is lent from the fund, repaid from takings above its band (or written off to the fund after 28 days) | Its posts aren't lost | Failing trade |
| **Opening** | A shut business to let in a town with many idle gets a float and a keeper from the idle | New posts | No work |

### Who pays, and where it goes

Each week, at the decision (the evening of the reckoning):

1. **What must be spent.** Each holder over its band gives the share its band says (Part 6). The total is the day's
   **pot**.
2. **Where.** The pot is shared out by the towns' distress × people, with some of the pot always held back for the
   land as a whole, not only the most distressed town. A town's own holders' excess goes first to its own town and
   neighbours (money stays local where it can); a city's excess is shared more widely.
3. **Which channels.** For each town, the channels that suit its kind of distress (the table above), weighted by their
   **reach** (below). No channel takes more than half a town's share, so one bad estimate can't take it all.
4. **Can it be spent?** A channel takes only what the town can carry: works pays only as many hands as are idle; food
   purchase only what the farms can spare; commissions only what the makers can make in a week. What can't be spent
   stays in the fund for tomorrow, and a fund that is still full a week later sends its money back to the land's pot.
5. **Booked:** each order is an `orders:` flow from the holder to the fund, and each fund's spending is booked under its
   channel.

**When the land is well,** the pot is only what the caps force out, and it goes mostly to **commissions** and **trade
orders**: ordinary demand, spread by the towns' people. Money keeps moving without any distress.

### Learning which channels reach

The orchestrator measures each channel, so the system improves itself rather than relying on the weights set here.

- **Reach** = the share of a channel's money that arrives within 7 days with residents in the land's poorer half:
  - first step: who the fund paid (wages to a wolf under the median count in full);
  - second step: for money paid to a till, the share of that till's outgoings in the next 7 days that went in wages to
    wolves under the median.
- The flow counters (Part 1) have all of this: kind, payer, payee, payee's wealth band.
- Each week, each channel's weight moves toward its measured reach, by a quarter, and stays between half and twice its
  starting weight. A channel that reaches nobody fades; one that works grows.
- Deterministic: it is arithmetic on the counters.

## Part 8: the seasons

The user's aim (2026-10-06): hunger only in season, or when work changes, and never for long.

- **The granary** (the food-purchase channel) is also the season's store. In summer and autumn, when farms bring in
  most and prices fall, the orchestrator buys more into the granaries, from any holder's excess. In winter and spring it
  releases it.
- **Lean months may still bite:** release is limited to the granary's stock, so a bad year still shows. How much it
  should soften winter is open question 7.

## Part 9: watching it

- **econ_watch** writes `orchestrator.jsonl` (each brief in full) and `orchestrator_towns.csv`: each day, each town's
  sensors, distress and kind; each holder's band; the pot; each channel's spending (and, from Phase 7, its reach); the
  margin; the wage floor.
- **The DM's Money tab** (`/api/money`) gets an **Orchestrator** panel: the distress map, holders against their bands,
  today's orders, the channels' reach, and the price and wage tables.
- **In the world,** nothing announces the orders (decision 4). Players see their effects: works hiring, full granaries,
  new hands at a farm. The events log keeps an `orders` event for the DM.
- **Health targets** for the runs (28 days, days 7 to 28, from the DEV export):

  | Measure | Target |
  |---|---|
  | Starving | 0 outside winter |
  | Hungry | under 2% |
  | Resident Gini | in a band (about 0.35 to 0.50), changing under 0.01 a week by week 4 |
  | Each kind of holder (tills, farms, houses, church, treasuries) | within ±10% in week 4 |
  | Average town swing | under 8% |
  | Median purse | at least 7 days of food |
  | Tick time | no more than 2% slower than today |

## Part 10: the Dungeon Master's hand (the user, 2026-10-06)

> "I do want to be able to exert some control over it, but I do want it to be autonomous. The control that I would
> exert over it would be to apply pressure or to make scenarios or things like that."

The orchestrator runs by itself. The Dungeon Master **steers** it, never drives it: a steer changes what the
orchestrator weighs, and the orchestrator still decides. It lives in the **Money tab** of the Dungeon Master, as an
**Orchestrator** panel.

**What the panel shows** (from the last save, like the rest of the tab):
- its mode (shadow, on or off), the day of its last measure and of its last decision;
- the land's distress, and each town's distress, kind and sensors;
- the holders over their band, with need, cash and band;
- the week's pot, and where it went (or would go, in shadow) by town and channel;
- the margin, the wage floor, and the prices furthest from the catalogue;
- the steers in force, with who set them and when they end.

**Steers.** Each has a strength, a length in days (1 to 56) and a note saying why. It ends by itself, or the Dungeon
Master ends it sooner.

| Steer | What it changes |
|---|---|
| **Pressure on the land** | How hard the bands squeeze everyone: ×0.5 (gentle) to ×3 (hard). Lowers the comfortable top and raises the share spent above it. |
| **Favour a town** | Weighs a town's distress up (or down), so more (or less) of the pot goes there. |
| **Squeeze a holder** | Treats a house, church, treasury or business as if it held more than it does (×1.5 to ×4), or **spares** it: it may hoard for a while (a house saving for a war). |
| **Close or favour a channel** | No business rescues this month; double the town works. |
| **Price shock** | One good in one town (or all towns) dearer or cheaper, ×0.5 to ×3: a scare, a glut, a blockade. The orchestrator works around it. |

**Scenarios** are named bundles of steers, kept in `Data/Economy/scenarios.json`, that a Dungeon Master starts on a town
with one click:
- **Hard winter:** food dearer everywhere, the granaries hold back, the pressure up.
- **Famine in a town:** food there ×2, the town favoured, the food purchase channel favoured.
- **Boom town:** the town's makers favoured with trade orders, the town's distress weighed down.
- **The miser:** one house spared for four weeks, and the land's pressure up to make up for it.
- **Squeeze the rich:** pressure ×2 on the land for a week.

Later scenarios may need the game itself to change (a drought cutting a town's harvest, a mine flooding). Those are
world events, not steers, and come with the Dungeon Master's event queue (doc 34).

**How a steer travels:**
1. The Dungeon Master sets it in the panel; `dungeon_master.py` checks it and queues a `dm.actions` row of kind
   `economy.steer` (audited like every action).
2. The game server takes it (`RatwGame.cpp`, with the other actions) and hands it to the society, which keeps it with
   the orchestrator's memory (saved).
3. The next day's snapshot carries it: it shows in the next day's measures, and acts at the next week's decision. A
   steer lasts at least a week to be sure of a decision. Its effects are deterministic, like everything else.
4. **Tests can steer too:** `econ_watch --steer FILE` reads a list of steers by day, so a scenario can be tried in a
   headless run before it is used in play.

## Phase 1: the shadow orchestrator (built 2026-10-06)

It runs beside the game, measures every day and decides once a week, and applies nothing. The game plays exactly as it
did: 2- and 6-day `econ_watch` runs give the same digest with it as without it.

**In code:**
- `Core/RatwOrchestrator.{h,cpp}`: `Snapshot`, `plan()` (pure), `Brief`, `Memory`, `Dials`, steers, the saved state
  (`RatwOrchestratorJson.h`), and `Runner`, the orchestrator's own thread.
- `Core/RatwOrchestrate.cpp`: the society's side.
  - **What it counts**, in `Society::record`: each holder's ordinary spending, coins between two towns, and what each
    resident earns (kept for seven days).
  - **The snapshot** at 23:00 (`SnapshotHour`; `needsBodies` asks the world for everyone then), handed to the thread.
  - **The brief** taken when the day turns, before the day's pass.
  - **Steers:** `Society::steer` and `unsteer`.
- **The week's decision:** the snapshot taken the evening of the reckoning (`reckonedDay_`, set in `Society::reckon`).
- **Threads:** `World::setParallel` gives the orchestrator its thread with the world's other threads. Without one it
  plans on the game thread when the day turns, with the same result.
- **Saved** with the society (`orchestrator`: steers, memory, the day's measures and the week's decision).
- **Dials:** `Data/Economy/orchestrator.json`. `RATW_ORCHESTRATOR=off` turns it off.
- **The Dungeon Master:**
  - `dm.actions` kinds `economy.steer` and `economy.unsteer`, applied in `RatwGame.cpp`;
  - `dungeon_master.py` `steer_economy` / `end_steer`, at `/api/economy/steer` and `/api/economy/unsteer`;
  - `money()` returns the orchestrator's block;
  - the Money tab's **Orchestrator** panel (`Editor/src/dm/OrchestratorPanel.tsx`): the day's measures, the week's
    decision, the steers in force, and a form to steer.
- **econ_watch:**
  - writes `orchestrator.jsonl` (every brief in full) and `orchestrator_towns.csv`, and a line a day;
  - `--steer FILE` scripts steers by day.
- **Tests:**
  - `orchestrator_tests` covers the plan on made-up land: distress and its kind, bands, the week weighed, the whole pot
    placed, every steer, prices, the saved state, and the same brief on a thread or not.
  - `roads_tests` `theOrchestratorWatches` covers a village through a reckoning's evening.

**What Phase 1 settles in the design:**
- **Until Phase 2 gives them tills,** owner-run shops and producers are measured by their keepers' own purses. Their
  floor is the keeper's comfortable line (`Society::wealthLine`), not the business's alone.
- **A new or restored world warms up.** It counts a day before it bands anyone (until then, *warming*), and three
  before it judges who is idle.
- **Idle** is a grown wolf under 65 that earned nothing in three days. Children, the retired, those keeping house and
  those in unpaid posts (lords, students) aren't looking for work.
- **Draining** is a town's coins to and from other towns, averaged over a week. The land's one church isn't a town:
  what it takes in tithes it gives back as alms and wages everywhere.
- **Hardship weighs most.** Hunger, starving, short households and the poor count fully; the signs of trouble coming
  (idle hands, bare shelves, failing trade, a drain) count a quarter to a half.
- **Up to half a town holder's excess stays in its own town:** half when the land is well, less the better off its town
  is than the land.

## Phase 2: every business its own till (built 2026-10-06)

Every owner-run shop, workshop, farm and site now keeps a till (`till:<position>`), as a great house's businesses did.
`Society::tillOf` finds it for every business, so the paths that already handled house tills (selling, crafting,
restocking, materials, contracts, caravans, repairs, players' trade) now handle them all.

**In code** (`Core/RatwTills.cpp`):
- **Founding** (`foundTills`, once):
  - A new world founds its tills when it is set up (`resetAuthored`); an older save the first time it runs
    (`EconomyMemory::tills`, saved).
  - The keeper's wares and materials, and a farm's goods for sale, move into the till.
  - So does the keeper's cash above a month's living (28 days of food, 140p). The keeper keeps that and its own food.
  - A till starts with at least its float, made once like a house till's. Its books open the same day.
- **The keeper's draw:**
  - each day `OwnerWage` (8p, the house managers' wage), as far as the till can spare above half its float;
  - at the weekly reckoning, after the till's own tax and tithe on the week's profit, a third of what is left
    (`OwnersShare`).
- **Until Phase 5:** above two floats, a fifth a day goes on hands and premises and on 2p shares for its help. This is
  the old owner-keeper rule, moved from the keeper's purse to the till. The keeper's food and goods for its home are
  now its own purse's business.

**What moved to the till:**
- **Wages:** a shop's help (and a trade's hands) are paid from the till, by house shops too. Before this, wages came out
  of the keeper's or manager's own purse.
- **Farms:**
  - the yield goes into the farm's till and is sold from it, to the town's food shops, the makers and caravans;
  - its hired hands, tools, upkeep and bunkhouse larder are paid from it;
  - the rich-producer rule applies only to producers without a till.
- **Taxes:** an owner-run till pays the weekly tax and tithe on its profit, and the wealth tithe above two floats
  (`wealthLine`).

**What the keeper's purse is now:** an ordinary purse.
- It joins the household purse.
- It pays for the household's shopping, children's stipends and wants.
- The keeper eats like anyone ("managed").
- `KeeperReserve` applies only to a keeper whose purse is still its till.

**Kept working:**
- A supply run is done by selling to a shop's till.
- Sales and wages still build trust between a till's keeper and its customers and help (`World::bondsFromEvent`).
- An unpaid hand names the keeper it is owed by.

**The orchestrator:** a farm's or site's till is a *producer* holder, a shop's a *till*. Personal purses count only for
a business without a till.

**Speed:** `tillOf` reads an index of tills by position (`tills_`, kept by `openAccount`, `closeAccount` and
`indexTills`). Before, it built and looked up the account's name. Runs are faster than before tills, though keepers now walk out to buy
their own food: 0.0447 against 0.0488 ms a tick over 6 days, and 0.0561 against 0.0641 over 15. The index changes no
result: the same digest with it as without.

**The 15 days against the game before tills** (DEV build 26 export, from 06:00):

| Day 15 | Before | With tills |
|---|---|---|
| Starving | 0 | 0 |
| Grown short / broke | 4 / 4 | 1 / 2 |
| Residents' Gini | 0.370 | 0.335 |
| Median resident purse | 258p | 169p (the shops' money is in their tills now, not their keepers' purses) |

**Tests:** `roads_tests`:
- `shopkeepersDontHoard`: the cookshop's till pays its keeper and help and the market dues;
- `theOwnersShare`: tax on the till's week and the keeper's third;
- several checks now read the till.

`crafting_tests` reads the shops' and farms' tills.

## Phases

Each phase is measured with a 14 or 28 day `econ_watch` run against the plain run of 70db3a4.

1. **The shadow orchestrator** (built). Thread, snapshot, counters, sensors, distress, bands and the brief, all computed
   and written to `orchestrator.jsonl`, **and not applied**. The old rules run as now. We see what it would do, and test that
   it is deterministic and cheap. The Dungeon Master's panel shows it, and steers already change the shadow brief.
2. **Every business its own till** (built). Tills for owner-run shops, farms and sites; keepers' and farmers' draw.
3. **Prices and margins.** Town prices replace markdowns, flush discounts, food factors and market factors. The margin
   replaces 0.55.
4. **Wages.** The wage table and the living floor replace wage shares, takings caps, hire pay, `wageFor` by wealth and
   the town wage scale.
5. **Bands and channels.** Town works, hire grants, commissions, food purchase and trade orders. The surplus rules, house
   props and patronage, the capital's share and the trials are removed.
6. **The rest of the channels.** Price support, wage support, business rescue, opening.
7. **Learning reach,** and the granary through the seasons.
8. **Watching and steering:** scenarios, the full DM panel, and a look at the panel in the browser against a real save
   (the user, 2026-10-06: the panel's checks wait for this phase).
9. **Clean-up:** every replaced rule and its constants removed from the code and from doc 42 (marked superseded there);
   month runs against the targets; balance pass.

## Open questions

1. ~~Every business its own till~~ **Decided:** yes.
2. ~~The church's coin dole~~ **Decided:** kept.
3. **Where the cap sits:** 4× a holder's need, or 2% of the land's money, whichever is lower? *Recommendation:* yes, as
   a starting point.
4. ~~Shop prices~~ **Decided:** one town price per good.
5. **Taxes and tithes stay steady** (the user's decision of 2026-10-06), and the orchestrator doesn't touch the rates?
   *Recommendation:* yes. It manages what holders do with what comes in.
6. ~~Who issues the orders~~ **Decided:** nobody; invisible.
7. **How much should the granary soften winter?** *Recommendation:* enough that nobody starves in a normal year, but
   prices rise and the poor eat plainer.
8. **Caravans and standing orders** stay with the towns and traders, and the orchestrator only adds trade orders as a
   channel? *Recommendation:* yes for now; revisit after Phase 7.
