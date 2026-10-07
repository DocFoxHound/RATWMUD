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

## Phase 3: prices and the margin (built 2026-10-06)

The orchestrator is **on** (`Data/Economy/orchestrator.json` `mode`): its prices and margin are applied. Its channels
and bands still only plan (Phase 5).

**Prices:**
- **One price per good per town:** `Society::townPrice`. Every shop of the town sells at it (`shopPrice`); a good of a
  quality at its quality's share (the catalog's crude, fine and masterwork prices against the plain good's).
- **When it is set:**
  - a good the orchestrator sees for the first time is priced at once, at any day's measure;
  - after that, at the week's decision, moving at most a quarter.
- **Its target:**
  - the town's shelves against what its shops mean to keep of it (`GoodSnap::kept`: a store of what they supply or
    trade, else a few). A fifth dearer when bare, a fifth cheaper at three times as much;
  - dearer again (up to 15%) when what is there would sell in under two days, missed sales counting;
  - within 0.6 to 1.6 of the catalog;
  - staples under their ceiling (Part 4).
- **What it reads:** every shop's wares, what it supplies to the makers, and any food it has in.
- **What isn't priced yet:**
  - a good it doesn't price goes at the catalog's;
  - meals, herbs and swords keep their old pricing: by the town's stores (`World::tendPrices`), and to players dearer
    as a trader runs short (`Society::storePriced`).

**The margin:**
- What the land is paid is the town's price times the orchestrator's margin (0.55 to start, 0.4 to 0.75, moved weekly
  toward whichever of shops and farms is short). The land means:
  - a farm selling food to the shops (`producersSell`);
  - a maker buying from a producer (`buyMaterials`);
  - a gatherer or hunter selling what it brought in (`sellBroughtIn`);
  - a player selling to a shop (`quote`).
- Shops buying from each other pay the town's price (carted in from elsewhere: `CartedIn` times the seller's town's).

**Everything else at the town's price:**
- the town's buyers, a farm's tools and upkeep, odd jobs' deliveries;
- caravans: they buy at the selling town's price and sell at the buying town's price times the road's markup (1.4);
- players buying, a tenth off at a stall.

**Removed:**
- the shops' markdowns (`markdown_`, `Markdown`) and the floor under them (`CostFloor`);
- the flush discount;
- the town food factor by median purse (`townPrice_`);
- the shelf factor in `shopPrice`;
- suppliers' and producers' `supplyFactor`;
- the World's six-hourly market factors (`marketPrices_`).

`priceFactor` now gives a good's town price against the catalog's; meals and herbs still come from the stores.

**Measured** (DEV build 26 export, 15 days from 06:00, against Phase 2):

| Day 15 | Phase 2 | Phase 3 |
|---|---|---|
| Starving | 0 | 0 (one wolf for a day, on day 7) |
| Grown short / broke | 1 / 2 | 1 / 4 |
| Residents' Gini | 0.335 | 0.333 |
| Median resident purse | 169p | 175p |

- **Speed:** run side by side under the same load (another session's work was on the machine), 6 days at four
  threads: 0.0826 ms a tick against 0.0927 for the game before doc 46.

**Tests:** `roads_tests` `oneTownPrice`: two bakeries of a town sell at its price, a fine loaf at its share, the land is
paid the price times the margin, and an unpriced good goes at the catalog's. `orchestrator_tests`: a good is priced at
once when first seen, then only at the week's decision.

## Phase 4: the wage table (built 2026-10-06)

Every town has a table of a day's pay for each kind of post, set by the orchestrator (`orchestra::wageKinds`):

| Kind | Who | Starts at (a day) |
|---|---|---|
| help | a shop's or a house's help, a trade's hands, a household's servants | 16p |
| guard | the watch | 20p |
| labour | the town's own posts and its works | 12p |
| clergy | priests, chapel keepers, acolytes | 16p |
| keeper | an owner-run business's keeper, a house business's manager (their daily wage) | 8p |
| hand | a business's or a farm's hire, a day | 12p |
| odd job | a hand's share of a day's odd job | 4p |

**How it moves:**
- **The floor:** never under the living floor, a day's food at the town's prices for two (`wageFloorOverFood` 2), plus
  lodging (0 for now); an odd job, a quarter of it.
- **Seeded** at once for every town; **moved** at each week's decision:
  - a tenth up for a kind with places going begging (vacant paid posts, hires and odd jobs nobody took by the evening);
  - a twentieth down where a tenth or more of those able to work earned nothing lately and none of the kind go begging,
    unless the town is in distress for want of work or money;
  - at most three times its start.
- Saved with the orchestrator (`memory.wage`), applied with its prices (`applyPrices`); every start and step is a dial
  (`wages`, `wageRaise`, `wageEase`, `wageMost`, `lodgingADay`).

**Paying it:**
- **By the spell:** a worker is paid a `PaidSpells`-th of its kind's day pay for each spell of work, the fractions of a
  penny carried to the next (`wageCarry_`).
- **Who pays** is unchanged (`payerOf`): a shop or house that can't pay is covered by its town or church if it has
  plenty (Phase 6's wage support will take this over); otherwise the hand goes unpaid and, after a week, labours for the
  town.
- **Hires and odd jobs** pay the table's hand and odd-job rates; a keeper's and a manager's daily wage is the keeper rate.
- A wolf weighs a better-paid hire against its own post's pay from the table.

**Measured** (15 days, against Phase 3):

| Day 15 | Phase 3 | Phase 4 |
|---|---|---|
| Starving | 0 | 0 |
| Grown short / broke | 1 / 4 | 1 / 1 |
| Residents' Gini | 0.333 | 0.313 |
| Median resident purse | 175p | 173p |

At the second decision the fortresses paid more for odd jobs (theirs went begging) and less for other work (many
idle); Upper Accord eased once, then held, in distress for want of work; the other towns kept their starting tables.

**Removed:**
- `wageFor`: pay by the employer's wealth, 1p from a lean payer;
- the town wage scale by takings (`townWageScale_`);
- shops' daily shares of their takings (`TakingsShare`, `TakingsCap`), and the hires they paid from them;
- hire pay rising and falling by the business (`hirePay_`);
- odd-job pay by the poster's richness and the town's poor.

## Phase 5: bands and channels (built 2026-10-06)

The orchestrator now sends money out. Doc 42's surplus rules are gone, and so are the trials.

**At the week's decision** (`Society::applyOrders`, when the day turns after the reckoning's evening):
- Each holder sends what the brief says (never below what the plan has it keep).
- What goes to its own town goes straight into that town's funds; the rest goes into the land's fund (`fund:land`) and
  on to the towns by the orders.
- **A fund still holding what it was last sent gets nothing more.** That order isn't sent, and its holder keeps the
  money; what keeps growing there raises the pay of those who work for it (the wage table). Without this the funds
  became the new pool: 75,000p after five weeks, a sixth of the land's money, because the towns couldn't spend it.

**The growth rule** (added in this phase):
- A holder above its floor (a till's float; a town's or the church's few pennies a head) that gained over the week sends
  half the week's gain out (`gainShare`), more with the land's distress or a Dungeon Master's pressure, whatever its
  band.
- It is measured from what it kept after the last decision (`memory.weekStart`).
- It catches the pool doc 42 kept finding: takings that grow with trade while spending is set by need. A shop's need,
  14 days of its spending, is mostly materials, so a till could gain for weeks and still read *lean*.

**Its own pressure** (added in this phase):
- At each decision it compares the residents' share of the land's money with the week before.
- If the share fell by more than half a point, it presses 1.3 times harder (to 3 at most): bands tighten and more of a
  holder's week's gain goes out. If the share rose, it eases (to 1).
- This is the ham-fisted part steered by the outcome, not by a rule for each holder. A Dungeon Master's pressure
  multiplies it.

**Wages follow their payers' books** (added to Phase 4's table):
- Each kind's pay rises where its payers gained over the week (more than a fiftieth of what they hold): twice their gain
  against what they hold, a tenth at least and a quarter at most. It eases a twentieth where they drained:
  - help and keepers: the town's shop tills;
  - labour and the watch: its treasury (not its buyers, whose gains are the treasury's funding);
  - hired hands: its farms;
  - clergy: the land's church.
- So money that pools in tills goes back to those who work for them, and a draining treasury pays less rather than
  running dry.
- **This is the main road back.** Nearly every grown wolf already works. The channels' odd jobs and hires absorbed
  about 4,000p a day, against pots of 30,000 to 70,000p a week. Wages carry the rest.

**The channels** (`Core/RatwChannels.cpp`, `runChannels`, daily): each town's funds (`fund:<town>:<channel>`) spend a
quarter of what they hold a day (at least 20p), through the game's own work:

| Channel | What it does |
|---|---|
| works | Odd jobs for those without work (up to 60 a day), and materials for the town's works bought from its shops and used up |
| hires | Hands hired for the week at the town's shops and farms, three a day at most, at the table's pay for a hand, paid as they work |
| commissions | Goods ordered from the town's makers (not food), kept by those who ordered them |
| food | Food bought from the town's farms at the land's price into its granary (`town:<town>:granary`); the granary sells to its food shops when they hold under three days' food a head, and its takings go back to the fund |
| trade | The town's makers' and farms' spare goods bought (makers at the town's price, farms at the land's) and sent away |

- Price support, wage support, business rescue and opening come in Phase 6 (`orchestra::channelLive`); until then the
  plan sends nothing down them.
- A well town's ordinary demand is works and hires first, then commissions and trade: work that pays wolves at once
  before goods that pay them only through their tills.

**Removed:**
- the treasuries', the church's and the great houses' surplus spending (`spendSurpluses`'s collectors, relief, patronage,
  feasts, candles, the house's bonuses);
- the owner-keeper's surplus, the rich producer's investment, and the tills' interim surplus;
- the capital's weekly share for the poor;
- the town budget, and every `RATW_TRIAL` (`town_budget`, `church_share`, `church_valve`, `house_keeps`, `house_need`,
  `tithe_relative`).

**Kept:** the church's dole and food alms, farm hires, market dues, rent, the wealth tithe, house takings, and house props
(until Phase 6's business rescue).

**Measured** (DEV build 26 export, 35 days from 06:00; the final rules):

| | Day 2 | Day 8 | Day 15 | Day 22 | Day 29 | Day 35 |
|---|---|---|---|---|---|---|
| Residents' share of the land's money (at the decisions) | | 0.606 | 0.592 | 0.601 | 0.601 | |
| The week's pot | | 43,077p | 41,027p | 41,770p | 41,872p | |
| Its own pressure | | 1 | 1.3 | 1.1 | 1.1 | |
| Workers (paid civilians) | 160,210p | 145,341p | 140,287p | 141,659p | 139,397p | 131,745p |
| Shop tills | 8,653p | 25,349p | 34,749p | 44,113p | 53,423p | 62,243p |
| Town treasuries | 70,934p | 64,812p | 39,308p | 30,361p | 24,914p | 23,350p |
| Guards | 26,886p | 29,389p | 34,599p | 38,472p | 41,959p | 43,254p |
| The towns' funds | 0 | 0 | 17,224p | 11,254p | 9,941p | 11,164p |
| Residents' Gini | 0.214 | 0.249 | 0.296 | 0.343 | 0.396 | 0.428 |
| Grown short / starving | 0 / 0 | 1 / 0 | 1 / 0 | 2 / 0 | 8 / 0 | 5 / 1 |

- **Holding:** the residents' share of the land's money holds at about 60%; the funds stay bounded; the pot is steady;
  wages paid rose from about 10,000p to 17,000p a day as the wage table followed the tills.
- **Not yet:**
  - *Inside* the residents, money still spreads apart (Gini 0.25 to 0.43): guards and keepers gain; workers without
    rising pay, and those without work, lose.
  - The treasuries drain (their wages exceed their taxes: residents who lose money make no profit to tax).
  - Shop tills keep growing, though half their weekly gain goes out.
- **Next** (Phases 6 and 7): wage support for lean payers (the treasuries); the orchestrator's own pressure steered by
  the bottom half's share as well as the residents'; and learning which channels reach the poor.

(Before these rules, two tries: half a holder's gain kept half pooling in the tills; pressing harder filled the funds,
which the towns couldn't spend.)

**Fixed:** a farm's or site's float counted the other workers of the same ground as its help, so each farm worker's
till was founded with up to 1,242p (125,000p in all, a quarter of the land's money). A producer's float has no help now.

## Phase 6: the needs, and the poorer half (built 2026-10-06)

Three more channels, met **by need** before anything is shared by distress. From what is pooled, at most half, each
town's need less what its fund still holds:

| Channel | Its need | What it does |
|---|---|---|
| wage support | (its workers owed wages + those it paid today) × the town's labour pay × 7 | A payer that can't pay (a shop, a house, a lean treasury or church) has the wage paid from the town's fund (`"a wage supported"`). It replaces the town's and church's covering of a broke shop's wages (`subsidiser`, gone) and now covers lean treasuries too. |
| price support | each staple's gap under its price × its sales a day × 7 | Where the staple ceiling holds a food under its own price, every sale owes the shop the gap (`supportOwed_`, counted as it sells), paid the next day from the town's fund. |
| rescue | what the town's failing businesses (under half their float) lack of their floats | A failing business is lent back up to its float; it repays from what it holds above two floats; a month on, what it owes is written off (`EconomyMemory::loans`, saved). A great house's business counts its days rescued, and a business rescued `ProppedDays` times in a month is sold on as before. |

- **House props are gone:** a house no longer props its own businesses to keep the money in the family.
- **Opening a shut business** is deferred: it needs the careers to fill a post, and isn't a channel yet.
- **Steers:** a Dungeon Master's channel steer weighs these needs too (0 closes one).

**The poorer half** (Phase 5's open issue):
- At each decision it compares the poorer half's share of the residents' money with the week before.
- If it fell by more than half a point, the living floor rises a tenth (`floorLift`, to twice at most); if it rose, the
  floor eases back. The floor lifts the lowest pay first: labour, hands, odd jobs.

**Not saved:** what staples owe their shops between a sale and the next day's pass (a restart loses up to a day's).

**Measured** (35 days, as Phase 5's run):

| | Day 2 | Day 8 | Day 15 | Day 22 | Day 29 | Day 35 |
|---|---|---|---|---|---|---|
| Residents' share (at the decisions) | | 0.594 | 0.581 | 0.581 | 0.574 | |
| Poorer half's share of the residents' money | 0.368 | 0.336 | 0.300 | 0.259 | 0.222 | 0.203 |
| Floor lift | | 1 | 1.1 | 1.21 | 1.33 | |
| Residents' Gini | 0.213 | 0.251 | 0.294 | 0.346 | 0.395 | 0.418 |
| Great houses | 21,169p | 27,068p | 31,702p | 35,811p | 36,025p | 36,345p |
| Guards | 26,886p | 29,175p | 34,308p | 38,489p | 42,557p | 45,241p |
| Town treasuries | 72,020p | 68,529p | 42,577p | 33,764p | 27,537p | 23,418p |
| Grown short / starving | 0 / 0 | 1 / 0 | 1 / 0 | 2 / 0 | 8 / 0 | 2 / 0 |

- **The needs are met each week:** wage support 5,741p to 12,060p, rescue 4,271p to 8,733p, price support 1,385p to
  5,356p.
- **The poorer half was measured wrongly at first.** Nearly 400 of the 1,350 residents are children, the retired and
  others with no income, whose own purses are small because their households hold the money. It is measured by
  household now: the households holding least a head, half of everyone living in them.
- **By household it still falls** (a rerun: 0.39 on day 2, 0.36 at the first decision, 0.33, 0.29, 0.26). The floor
  lift (to 1.33) can't move it: the living floor, about 7 to 13p a day, is under what nearly every post pays.
- **Where the money goes on its way out:** the hires and the works' odd jobs pay wolves; commissions, trade and the
  works' materials pay shops' tills, the very holders that are pooling. *Next (Phase 7):* learn each channel's reach
  and weigh the channels by it.
- **The great houses gain** (21,000 to 36,000p) now that they don't prop their businesses: half of each week's gain still
  stays.
- **The guards save:** paid well and fed at the mess, they keep it (the wealth tithe takes a twentieth above a month's
  living). They are residents, so it is their own business (the user's rule).

## Phase 7: learning reach, and the granary (built 2026-10-06)

**Reach** (`Society::noteForOrchestra`, the snapshot; `plan`):
- Every penny a channel's fund pays is followed:
  - paid to a resident, it counts as reaching the poorer half if the resident's household is in it (the poorer half by
    purse a head, `poorHalf_`, made at each snapshot);
  - paid to a till, it counts at the share of that till's outgoings in the week that went to the poorer half.
- At each decision every channel paid 50p or more (`reachMinimum`) is judged: its weight moves a quarter of the way
  (`reachStep`) toward its reach against a fair half (reach / 0.5), between half and twice (`reachLeast`, `reachMost`).
  The learned weights multiply the channels' fit to each town's trouble, and are saved (`memory.learned`).
- The brief shows each channel's reach and learned weight.

**The granary through the seasons:**
- **Only food that keeps** (a fortnight or more: salt fish and pork, jerky, cheese, smoked meats, ship's biscuit) goes
  into a granary, from the town's farms at the land's price, then from its food shops at the town's. Fresh food no longer
  rots there.
- **Storing:** in summer and autumn each town's granary is a need, met first: `granaryDays` (5) of food a head, less what
  it holds.
- **Release:** the granary sells to the town's food shops when they hold under three days' food a head, and in summer and
  autumn only under a day and a half (the store kept for the winter).

**Measured** (35 days in spring, so no storing; the same start as Phases 5 and 6):

| Decision | Day 8 | Day 15 | Day 22 | Day 29 |
|---|---|---|---|---|
| Reach: hires | | 0.68 | 0.37 | 0.55 |
| Reach: rescue / wage support | | 0.35 / 0.35 | 0.38 / 0.27 | 0.34 / 0.37 |
| Reach: works | | 0.19 | 0.31 | 0.29 |
| Reach: commissions / trade / price support | | 0.21 / 0.06 / 0.09 | 0.30 / 0.12 / 0.10 | 0.09 / 0.02 / 0.10 |
| Learned: hires / works / commissions | | 1.09 / 0.88 / 0.88 | 1.00 / 0.81 / 0.81 | 1.03 / 0.75 / 0.73 |
| Poorer half's share (by household) | 0.36 | 0.33 | 0.29 | 0.25 |

- **The learning works:** hires reach the poorer half best, trade and price support least, and the weights follow.
- **The balance doesn't move.** Even the best channel reaches the poorer half with about half its money. The poorer
  half are mostly households with few or no earners (children, the retired, those keeping house, large families on one
  wage). Channels that pay for work can't reach wolves who don't work, and the living floor is under what posts pay.
  *The user accepted the spread* (open question 9).

## Phase 8: watching and steering (built 2026-10-06)

**Scenarios** (`Data/Economy/scenarios.json`): named bundles of steers a Dungeon Master starts with one click, on a town
or a holder when the scenario needs one. The Dungeon Master's service expands a scenario into ordinary steers (`economy.steer`
actions, noted "Scenario: <name>"); the game sees only those.

| Scenario | Needs | Its steers |
|---|---|---|
| Hard winter | | Staples ×1.5 everywhere; pressure ×1.5; four weeks |
| Famine in a town | a town | Its staples ×2; the town weighed ×2.5; the food channel ×2; three weeks |
| Boom town | a town | Its distress weighed ×0.5; trade and commissions ×2; four weeks |
| The miser | a holder | The holder spared; pressure ×1.3; four weeks |
| Squeeze the rich | | Pressure ×2 for a week |
| Work for all | a town | The town weighed ×1.5; works and hires ×2; two weeks |

- An item of "staples" stands for every plain food (3p or less) and firewood, twelve at most.
- A channel steer is the land's, not one town's (Part 10).

**The panel** (the Money tab's Orchestrator) shows besides Phase 1's:
- the residents' and the poorer half's shares, its own pressure and the floor lift;
- each channel's share of the week's pot, its reach and its learned weight;
- each town's wage table;
- the towns' funds (and the land's) and granaries;
- the holders' week's gain;
- a form to start a scenario.

`money()` reports `funds`, `landFund` and `granaries`; granaries aren't counted among a town's buyers.

**Seen in the browser** (headless Chromium, `tools/client/browser.mjs`, the built Money tab served by a stub of the
Dungeon Master's service fed with a 35-day run's real orchestrator output; not yet a live save):
- Every section draws: the header's shares and pressures, the towns, the holders, where the pot went, the channels'
  reach and learned weights, the prices, the wage table, the funds, the steers in force, the steer and scenario forms.
- **Fixed:** hundreds of holders may be *growing*; the panel shows the 20 sending most, with the rest summed.
- **Fixed, in the game:** shops rounded a price *up* to whole pennies, so a 1p good the orchestrator priced at 1.4p
  sold for 2p, twice its catalog's (apples, oats, milk, vegetables: every cheap good). Prices now go to the nearest
  penny, a penny at least (`Society::pennies`), everywhere a town's price becomes pennies.

**Measured after the fix** (35 days, as Phase 7's run):

| | Phase 7 | Phase 8 |
|---|---|---|
| Residents' share at the decisions (days 8, 15, 22, 29) | 0.593, 0.585, 0.582, 0.577 | 0.605, 0.599, 0.603, 0.613 |
| Shop tills, day 35 | 56,161p | 47,057p |
| Residents' Gini, day 35 | 0.419 | 0.411 |
| Grown short / broke / starving, day 35 | 3 / 3 / 0 | 1 / 1 / 0 |

## The long run: where it let go, and the fixes (2026-10-06)

The user: "I really am interested to know where Orchestrator is failing to keep wolves fed and balance this out. A little
poverty is fine, and a little starvation … but I'm worried that it'll just spiral out of control if we don't have the
proper system in place for Orchestrator to handle it on its own."

**What 112 days showed** (a year's run, stopped by the user to run the year themselves):

| Day | 2 | 29 | 57 | 85 | 112 |
|---|---|---|---|---|---|
| Median purse | 268p | 220p | 169p | 125p | 105p |
| Poorer half's share of the residents' money | 0.37 | 0.24 | 0.14 | 0.09 | 0.07 |
| Grown short / broke / hungry | 0 / 0 / 0 | 4 / 0 / 0 | 13 / 12 / 0 | 27 / 14 / 0 | 31 / 13 / 25 |
| Idle (able to work, nothing earned in three days) | | 148 | 264 | 284 | 257 |
| Guards' savings | 26,912p | 43,692p | 52,012p | 60,452p | 64,276p |
| Town treasuries / the capital's | 71,613p / 19,204p | 25,386p / 5,050p | 16,864p / 1p | 11,219p / 0p | 12,855p / 989p |

**Where it failed:**
1. **The wrong alarm.** It watched the residents' share of the land's money, which held at 0.60 to 0.63 throughout:
   the guards and keepers saved what the workers lost. So it never pressed harder.
2. **Raises for those with work, while the idle doubled.** Whenever payers gained, pay rose, whatever the idle. Help,
   guards and clergy reached three times their start (48p, 60p, 48p a day) while 300 idle lived on 4p odd jobs.
3. **The treasuries bled dry.** Their wages outran their taxes (a tax on profit, and the poor make none), so the Town
   Works couldn't pay the unemployed: the last safety net failed.
4. **Wage support paid the whole rate,** so it kept up the guards' 60p a day (4,200p in two weeks), which they banked:
   fed at the mess, a guard's pay is savings.
5. **Growing holders kept half of each week's gain,** so the shops' tills grew on (75,000p by day 112).

**The fixes (the user chose all four):**
- **Alarm on the poorer half:** its own pressure rises when the poorer half of households (by purse a head) loses share,
  as well as when the residents do; it eases when neither fell and one rose. The living floor rises with the poorer
  half's fall as before.
- **Jobs before raises:** a town where more than a tenth of those able to work earned nothing lately (`wageIdleLimit`)
  raises no pay for its payers' gains, and its hires and works weigh double in the pot's sharing. Pay rises with the
  payers' books only where labour is short. (Pay is not cut for the idle: tried, it took a thousand pennies a day from
  the keepers and gave the idle nothing.)
- **Support at a living wage:** wage support pays at most the town's living floor (a day's, a `PaidSpells`-th a spell),
  the rest owed as before, and its need is reckoned at the floor.
- **Odd jobs that feed:** an odd job pays at least a day's food at the town's prices, and the works post as many as the
  town has idle wolves (sixty at least).
- **Growth that keeps on:** a holder gaining week after week sends half the first week's gain, then three quarters, then
  all of it (`growthStep`, `memory.growing`).

**The second round (the same day).** A 47-day run with the four fixes cut the idle from 158 to 68, but the pot rose to
100,000p a week against channels that pay out some 5,000p a day, and the funds became the pool. So:
- **No channel is sent more than it can spend:** one and a half times what it paid out last week (`channelFloor`, 3,000p,
  for one new or idle), less what its funds still hold; but the works may always take a week of odd jobs at a day's food
  for every idle wolf, half again (they keep a third for their own work). What a channel can't take stays with its givers.
- **The idle limit is a tenth,** not a twentieth: at 5% almost every town counted as idle and no pay ever rose.
- **Each town's prices move against its shops' books:** while its shops' tills gain over the week, its price level comes
  down by twice their gain against what they hold (a tenth a week at most), and goes back up while they lose, between
  `priceLiftLeast` 0.6 and `priceLiftMost` 1.4 (`memory.priceLift`). The tills' surplus goes back to every customer by
  what they buy. It's the outlet the shops had lacked: capped channels and no raises left their money nowhere to go.
- **Pay isn't cut for the idle** (tried: it took a thousand pennies a day from keepers and gave the idle nothing).

**56 days, against the year's run (the old rules):**

| Day | 13 | 27 | 41 | 55 |
|---|---|---|---|---|
| Residents' share: old / now | 0.61 / 0.59 | 0.63 / 0.54 | 0.65 / 0.55 | 0.65 / 0.58 |
| Poorer half's share: old / now | 0.33 / 0.34 | 0.27 / 0.27 | 0.23 / 0.23 | 0.18 / 0.20 |
| Median purse: old / now | 249p / 236p | 236p / 200p | 219p / 186p | 183p / 185p |
| Idle: old / now | | 71 / 112 | | 330 / 85 |
| Starving (day 42, 56): old / now | | | 17 / 0 | 18 / 0 |

The residents' share turns back up after day 27 (the old run's held up only because the guards banked their pay), the
idle stay under a hundred, nobody starves, and the median holds at 185p. The poorer half still loses ground, but slower
each fortnight (−0.07, −0.04, −0.025): it looks to level out near 0.17, where the old run went on down to 0.07. What's
left to watch in the year's run: the capital's treasury still runs dry (by day 55 in both runs), the town treasuries
drain (36,000p to 23,500p in four weeks), the shops' tills and the farms still gain (71,000p and 55,000p), and its own
pressure sits at its most (3) from day 40.

## Money that stops: the income side and the savers (the user, 2026-10-06)

The user: "I'm afraid that orchestrator doesn't have the tools it needs yet to balance the economy out, especially if
there are places that are still allocating wealth without a proper means to distribute it." Then, of the five options
offered: "Do all five options."

**Where it stops** (the last fortnight of the 56-day run, a coin followed in and out): the shops and farms balance now
(+434p and +45p a day), but those who pay the land's wages run down what they hold (the great houses −556p a day, the
capital −334p, the church −325p, the town treasuries −148p), and every kind of resident gains, the most a head those who
earn more than they live on: the watch (+283p a day: fed at the mess, a guard's pay is savings), the keepers and farmers
(a wage and a third of the profit). Their purses are their own, and nothing draws their savings out again. The money
runs from the institutions to the steady earners' savings, and stops.

The five tools, each where the orchestrator lacked one:

**1. The orchestrator sets each treasury's tax level.** A treasury (and the capital's) is banded like any holder; at
each week's decision its level rises `taxRaise` (×1.15) while it holds under its need and eases `taxEase` (×0.9) while it
holds over its comfortable band, between `taxLeast` 0.5 and `taxMost` 3 (`memory.taxLevel`). The level multiplies
everything the town takes: the tax on a week's profit, the market dues, the levy (2), and, for the capital, the towns'
share sent to it. The rates themselves don't change (a tenth, and a fifth above the band): the level is the lever. (This
answers open question 5 the other way: the orchestrator does touch what the towns take.)

**2. The town levy.** At each reckoning, after the tax, the tithe and the wealth tithe, a resident pays its town
`levyShare` (a fiftieth) of what it holds (in purse and at the bank) above `levyLine` wealth lines (two: two months'
food), times the town's tax level: "a town levy". It reaches the savings the profit tax can't (a guard banks wages, which
are no profit to tax). Tills, houses and the church aren't levied: their money is the orchestrator's to send already.

**3. Things worth saving for** (the residents' own decisions, more of them for those who have the money):
- A want that finds nothing on the shelves tries its next fancy (two more), where before it bought nothing.
- **A hand about the home:** a resident holding more than `helpLine` wealth lines (one and a half) posts an odd job a
  day at its home (fetching, mending, scrubbing), paid at its town's odd-job rate from its own purse: the idle work for
  the comfortable, as wolves would. A town has as many as it has idle wolves, the best off posting first, and none is
  taken by the poster's own household (it would be paying itself).
- **A piece commissioned:** once a week (its own day of the week) a resident holding more than two levy lines buys a
  costly piece (10p or more, not food) from its town's makers, up to a quarter of what it holds above the line.
- **A feast:** on Restday a resident holding more than a levy line feeds its household from the town's food shops: a
  treat for each member at home, eaten there and then.

**4. The watch's pay reckons its board.** The wage table knows a guard is fed at the mess: its floor is the living
floor less a day's food (`board`: guard 1), and its pay starts at 14p (was 20p), so the treasuries pay less and the
guards bank less. (Miners and quarrymen eat the works' rations too, but their pay is labour's, already the lowest.)

**5. A bank in each town** (`bank:<town>`), its savers' books saved with the orchestrator's state:
- **Savings** (the residents' decision): at the reckoning, after its dues, a resident puts half of what it holds above
  `depositLine` wealth lines (one) in its town's bank. A resident whose purse falls under a week's food draws back up to a
  wealth line, as far as the bank has the coins ("drawn from the bank").
- **The bank is a holder like the others:** it keeps a reserve (its floor: four weeks of what its savers draw, twice
  over, and a tenth of what they have in at least; a third, at first, made the reserve a pool of its own) and the
  orchestrator sends all the rest out at each week's decision, through its channels. No loans, nothing owed, no interest
  (the user, 2026-10-07: "remove the loan system, at least we don't need to track it. We'll just use it as a black hole
  for money to go into and then gets funneled out into places that Orchestrator says to"). Its coins may be less than its
  savers have in: a saver short draws what the reserve allows.
- **What the channels can't take** (they are capped at what they can spend) goes as **grants** to those in its town who
  pay wages and are short: a treasury, the capital or a great house under its need, then a shop or a farm under half its
  own, up to their need, owed by nobody ("from the bank's savings"). Without them the savings pooled in the banks: by the
  fourth week they held 71% of what their savers had put in.
- Savings count as their savers' money in every measure (the residents' share, the poorer half, the median), and the
  levy and the wealth tithe reckon them (paid from the purse first, then drawn from the bank).

(The first build lent the savings to those under their need, with interest to the savers; it was replaced the next day.
The rescue channel's loans to failing businesses, from Phase 6, are another thing and stay.)

The Dungeon Master's Money tab shows each treasury's tax level and each town's bank (what its savers have in, its coins); `econ_watch` reports the banks and the tax levels on each orchestrator line, and counts savings as their
savers' money.

**Built (2026-10-06), and 56 days** against the old rules' year run and the long run's fixes alone:

| Day 55 | Old rules | Long-run fixes | And money that stops |
|---|---|---|---|
| Poorer half's share | 0.18 | 0.20 | 0.25 |
| Its fall each fortnight | −0.04, −0.05 | −0.04, −0.025 | −0.025, −0.021 |
| Median purse (day 56) | 170p | 174p | 222p |
| Gini (day 56) | 0.51 | 0.47 | 0.43 |
| Idle | 330 | 85 | 76 |
| Starving / broke (day 56) | 18 / 11 | 0 / 4 | 0 / 4 |
| The capital's treasury | 0p | 0p | 19,000p |

What each did over the eight weeks: the savers put 178,000p in the banks and drew 8,400p back; the banks lent 110,000p
(at first to the businesses, from the third week mostly to the great houses and the treasuries) and were repaid
4,000p; interest paid the savers 2,600p. The hands about the home paid the idle some 250p a day, the feasts some 1,400p a
week, the levy 5,300p in all, the commissions 2,800p. The tax levels split: eight towns at 0.5 and two near it (their
treasuries full), the capital and Accord Crossing at 2.7 and Ser Ferro at 2.3, still climbing.

Two things learned on the way: a bank lending only to treasuries left a third of the land's savings idle in its vaults
(lending to shops and farms under half their need, and a bank that can't lend taking no more savings, fixed it); and
a hand about the home for every comfortable wolf made 1,100 jobs a day for 150 idle (now as many as the town has idle).

(Those 56 days ran with the first build's loans. **Without them** (the user, 2026-10-07), the banks keeping a third and
the rest going out through the channels and as grants to those short, 56 days again: the poorer half 0.344, 0.296,
0.271, 0.251, as with the loans; the median 218p; Gini 0.42 (the lowest yet); nobody starving; idle 81; the banks holding
their third (48,600p of the 146,400p saved). The grants went to the shops' tills (31,000p), the houses' tills and farms
(11,000p each), the town treasuries (8,600p), the great houses and the capital (6,500p each).)

**To watch in the year's run:** whether the poorer half levels out (it looks to, near 0.22) or keeps sliding slowly. Whether
the climbing tax levels reach their most (3) and stay there.

## The playbook: tools it uses on its own (the user, 2026-10-07)

The user, after the year's run: "Orchestrator should be handling a lot of these issues itself automatically and should be
balancing out the economy. It needs to have the tools available and it needs to have the automation available to
automate these things itself."

**What the year showed** (365 days, `econ_watch`, the banks with grants): no spiral (the poorer half fell to 0.17 by day
250 and held there), but four things it couldn't mend:
1. **Starving on winter Restdays** (30 on day 70, 47 on day 343), with money in their purses. A big town's kitchens make
   a little less than it eats in winter, so its shelves hold under a day; on Restday nobody cooks and those with money lay
   in for it, and the shelves run bare. The granary didn't help: it sold to the shops whenever their shelves ran under a
   day and a half, all summer, so it never kept a store, and by winter it had nothing (it bought 5,700p to 22,000p a month
   from spring to autumn and released almost all of it).
2. **Money the channels couldn't move stayed put:** the church grew from 19,000p to 53,000p, the banks to 88,000p.
3. **The banks drained the savers:** half of what a saver held above a month's food went in each week; the residents' real
   share of the land's money fell to about 0.40, while, counting the savings at their face, it read 0.75 (fixed: savings
   now count at what their bank holds).
4. **Its own pressure sat at its most** (3) from day 140 to day 330 and it had nothing harder to do.

**The tools** (each week, at the decision; each logged in the week's report):

**1. Food security.** Each town's food days (its shops' food and its granary's, a head):
- **The granary keeps a store:** it sells to the shops only when their shelves hold under a day (a shortage) or on the
  eve of a Restday or a festival (to fill them to two days), never down past its reserve in summer and autumn but for a
  shortage. Its store's target is `granaryDays` (10) a head by the end of autumn, half that in spring.
- **Food is a need,** like wage support: the plan sends the food channel what the granary lacks of its target, at the
  town's prices, from what is pooled, in every season. The needs (food, wage and price support, rescue) aren't held to
  what their channel spent last week: capped so, the food channel got 1,000p a week all year and the granaries never
  filled.
- **Hands to the food:** a town under `foodDaysLow` (two days) sends its hires to its farms, fishers and kitchens first.
- **Only what is spare:** the store is bought from its farms' surplus, from its own shops only while their shelves hold
  over `foodDaysLow` days a head, and from elsewhere only from shops holding more (bought off bare shelves, it left a
  town's eaters short).
- **Food from elsewhere:** a town with no food shops of its own (the fortresses), or under `foodDaysLow`, has its granary's
  food brought by the carters from the land's best-stocked food shops (those with over two days' food a head).
- **The messes from the granary:** the watch's, the mines' and the quarries' mess, with no food in it, draws a day's from
  its town's granary. (In the fortresses nobody carried food to the mess, and the barracks went hungry on Restdays.)

**2. An outlet for every pool.** What any holder over its band can't send because the channels are full goes as grants to
those in its town who pay wages and are short (as the banks' did): a treasury, the capital or a great house under its need,
then a shop or a farm under half of it. A holder nobody in its town needs: to the land's short. (Not a town's buyer: the watch's mess and the works keep what
they buy with.) (The church's grants go
to its clergy's towns, and to alms first: food for the hungry, bought from the town's shops.)

**3. The bank's terms.** The share of a saver's money above its line that goes in each week (`depositShare`, 0.05 to 0.5,
starting at 0.25) is the orchestrator's: down a fifth while the residents' real share falls, up a tenth while it rises and
the land's holders are lean. (Saving is still each saver's own: the bank only offers.)

**4. The ladder.** When its own pressure has been near its most (85% of it) for `ladderWeeks` (three) decisions and the
poorer half or the residents still fell, it steps up a rung (0 to 3); after four decisions with neither falling, it steps down one:
1. the channels may take twice what they paid last week (their cap doubled);
2. the living floor may lift to three times (`floorMost` 2 to 3), and the lowest pay (odd jobs, hands, labour) rises with
   its payers' books even while many are idle;
3. the bands' cap and top come down a quarter: every holder keeps less.

**5. The week's report.** Each decision lists what it found and did, town by town ("Upper Accord: shelves 0.6 days, the
granary filled them for Restday, 2,300p of food bought for its store"; "the ladder: rung 2"), in the brief, on the Dungeon
Master's Money tab and in `econ_watch`.

## Phases

Each phase is measured with a 14 or 28 day `econ_watch` run against the plain run of 70db3a4.

1. **The shadow orchestrator** (built). Thread, snapshot, counters, sensors, distress, bands and the brief, all computed
   and written to `orchestrator.jsonl`, **and not applied**. The old rules run as now. We see what it would do, and test that
   it is deterministic and cheap. The Dungeon Master's panel shows it, and steers already change the shadow brief.
2. **Every business its own till** (built). Tills for owner-run shops, farms and sites; keepers' and farmers' draw.
3. **Prices and margins** (built). Town prices replace markdowns, flush discounts, food factors and market factors. The
   margin replaces 0.55.
4. **Wages** (built). The wage table and the living floor replace wage shares, takings caps, hire pay, `wageFor` by wealth and
   the town wage scale.
5. **Bands and channels** (built). Town works, hire grants, commissions, food purchase and trade orders. The surplus rules, house
   props and patronage, the capital's share and the trials are removed.
6. **The rest of the channels** (built, but for opening). Price support, wage support, business rescue, opening.
7. **Learning reach,** and the granary through the seasons (built).
8. **Watching and steering** (built): scenarios, the full DM panel, and a look at the panel in the browser against a real save
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
   *Recommendation:* yes. It manages what holders do with what comes in. **Changed (the user, 2026-10-06, "Do all
   five options"):** the rates stay, but the orchestrator sets each town's tax level (money that stops).
6. ~~Who issues the orders~~ **Decided:** nobody; invisible.
7. **How much should the granary soften winter?** *Recommendation:* enough that nobody starves in a normal year, but
   prices rise and the poor eat plainer.
8. **Caravans and standing orders** stay with the towns and traders, and the orchestrator only adds trade orders as a
   channel? *Recommendation:* yes for now; revisit after Phase 7.
9. **The poorer half: households with few earners** (found in Phases 6 and 7). The orchestrator holds the residents'
   share of the land's money and nobody starves, but the poorer half of households keeps losing ground (0.36 to 0.25 in
   four weeks). It is mostly households where few work. Ways forward:
   - a *household wage*: the living floor reckoned for a worker's household (its members at home), so one wage keeps
     a family; it binds where wages are lowest;
   - a *household channel*: the church's dole made an orchestrator channel, by need, for households short of a week's
     food (it is a handout, which the user kept for the dole);
   - or accept the spread, so long as hunger stays seasonal.

   *Recommendation:* the household wage first: it pays for work, and reaches the families of the poorest workers.

   **Decided (the user, 2026-10-06): accept the spread.** Inequality is fine so long as hunger stays seasonal; the
   orchestrator holds the residents' share and the needs, and nobody starves.
