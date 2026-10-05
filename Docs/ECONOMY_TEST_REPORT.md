# Economy test report and handoff (2026-10-05)

Where the work on the NPC economy stands, for the next session. The design and every rule built is in
`Docs/Design/42-money-in-circulation.md`; fast-forward and the deterministic mode are in
`Docs/Design/31-responsiveness.md` ("Fast-forward").

**The user's goal:** an economy left to its NPCs alone is incredibly stable:

- crafting materials get where they are needed;
- wages settle;
- nothing goes broke;
- nothing hoards the money.

## State of the work

- **Nothing is committed.** Everything below is in the working tree. All 45 C++ test suites pass (`ctest --test-dir
  build-econ`).
- **Other sessions share the tree.** Commit only these files. `Docs/Design/45-gift-balance.md` belongs to another
  session.
  - New: `Core/RatwFounding.cpp`, `Core/RatwOddJobs.cpp`, `Core/RatwHouseholds.cpp`, `Tests/econ_watch.cpp`,
    `Docs/ECONOMY_TEST_REPORT.md`, `artifacts/economy/2026-10-05/`.
  - Changed: `CMakeLists.txt`, `Server/ratw_server.cpp`, `Tests/{game_tests,roads_tests,world_check}.cpp`,
    `Docs/Design/{31,42}-*.md`, and in `Core/`: `RatwAuthoring`, `RatwCareers`, `RatwCheckpoint`, `RatwCrafting`,
    `RatwDemand`, `RatwGame(.h)`, `RatwGameDev`, `RatwHouses`, `RatwItems(.h)`, `RatwProcure`, `RatwReckoning`,
    `RatwResidents`, `RatwRoads(.h)`, `RatwSociety(.h)`, `RatwSurplus`, `RatwTrade`, `RatwWire`, `RatwWorld(.h)`.
- **Build directory:** `build-econ/` (Release), made for this work.

## How to run a test

1. `python3 tools/world_build.py export DIR`: DEV's newest build (build 24) as files.
2. `build-econ/econ_watch DIR 28.1 OUT --deterministic`: a month, no players, flat out.
   - About 3.5 minutes a game day on one core; a month takes about 1.7 to 2 hours.
   - `--deterministic` makes a run repeat exactly.
   - It prints a line a game day.
3. **What it writes to `OUT`:**
   - `days.jsonl`: money by holder, every treasury, church, house and till purse, the resident Gini, grown wolves short of
     a day's food money, the hungry with why, and repair levels.
   - `flows.csv`: every ledger flow, by day, kind, and from and to holder.
   - `wages.csv`.
   - `materials.csv`: maker and supplier hours short, by good.
   - `events.csv`.
4. **Following a run:** `artifacts/economy/2026-10-05/watch.py SCRATCH PID 0 RUNDIR` follows a run, printing a line a day
   with alarms.
5. **Summing a run up:** `analyse.py OUTDIR`.
6. **Tracing someone:** `RATW_TRACE=id1,id2` makes econ_watch trace residents (place, task, activity, contract).
7. **Stopping a run:** use `pgrep -x econ_watch`. `pkill -f` with the run's path kills your own shell, which has the
   same path on its command line.

## Results

Saved runs are in `artifacts/economy/2026-10-05/`, each with its `.log`. All start from DEV build 24's fresh world with
the starting money.

| Run | What it had | Day | Starving | Grown short | Gini | Median |
|---|---|---|---|---|---|---|
| week0 | Tool fixed, first fixes absent (the baseline) | 7 | 3 | 93 (all ages) | 0.60 | 44p |
| week1 | Roads, porters' provisions, standing orders, keeper reserve and surplus, market dues, seasonal stock | 7 | 2 | 89 (all ages) | 0.61 | 42p |
| week2 | + wages for up to 8 spells, rations | 7 | 2 | 71 (all ages) | 0.56 | 55p |
| week5 | + stipends, wants, odd jobs, subsidies, friend groups, relief jobs, fair shares, daily alms, shared fields | 7 | 0 | 66 | 0.58 | 58p |
| week7 | + takings share, larder errand, household purse and keeper, shop prices, hires, improvements, upkeep | 10 | 7 | 67 | 0.60 | 56p |
| month3 | + alms at the shop, a keeper's food money everywhere, stipends only when safe | 14 (stopped) | 1 (0 to day 12) | 143 | 0.67 | 39p |
| month4 | + takings cap with hires, deeper flush discounts, mother church | stopped at once | | | | |

**month3 is the latest measured code.** By day:

| Day | 7 | 8 | 10 | 12 | 13 | 14 |
|---|---|---|---|---|---|---|
| Hungry | 0 | 0 | 0 | 0 | 0 | 14 |
| Starving | 0 | 0 | 0 | 0 | 0 | 1 |
| Grown short | 34 | 67 | 83 | 119 | 120 | 143 |
| Gini | 0.55 | 0.58 | 0.61 | 0.64 | 0.66 | 0.67 |

From day 5 to day 12 (more in `days.jsonl`):

| Holder | Day 5 | Day 12 |
|---|---|---|
| Shop tills and keepers | 41,500p | 48,600p (about +2,000p a day) |
| Great houses | 19,400p | 21,700p |
| Town treasuries | 31,300p | 23,000p (draining toward their lean line) |
| Unpaid residents | 5,000p | 2,800p |
| Paid workers | 75,800p | 76,200p, concentrating within (busy shops' help, producers) |

The poorer half of residents went from 16% to 7% of residents' money. Saltreach's church ran dry paying alms on day
13, and Saltreach's poor went hungry on day 14.

## What was learned

- **The first month's collapse was the tool's fault.** econ_watch cleared the cell files after loading, so unvisited
  cells could never load, and travellers and caravans were stranded there. The game is fine; the tool is fixed.
- **Wages didn't cover living.** They were paid for at most 3 hours of a 9-hour day.
- **Money pooled where no rule sent it back out:** shopkeepers, then the busiest shops' help.
- **Dependents starve when nobody buys for them.** Children, apprentices and homemakers have no income, and the larder
  wasn't stocked unless an earner happened to be hungry.
- **Midnight alms miss small towns,** whose shops are sold out at night. Alms at an open shop, with the church paying,
  works.
- **Treasuries pay more in wages than they take in between weekly taxes.** They fall to their lean line and then pay
  half. That is by design, but it stops relief jobs just when poverty rises.
- **Iron isn't short.** `tools/supply_balance.py` shows ore 2.4x and charcoal 2.1x real use. econ_watch's "maker-hours
  short" counts smiths wanting full shelves.
- **Hunger is solved; spread is not.** Money keeps concentrating in shops, houses and earners while treasuries and
  non-earners drain.

## Next steps

1. **Measure the last three fixes** with a fresh month (the month4 setup): `TakingsCap` with hires from the excess,
   flush discounts down to 30% off, and the mother church. Does the Gini level off, do the tills stop growing, and do
   treasuries settle at their lean line?
2. **If money still concentrates:**
   - Income for wolves with no earner at home (the retired living alone, and others): relief jobs while treasuries are
     lean, or a church-funded allowance.
   - Houses' reserve: they keep 200p plus two floats a business before spending a surplus.
   - The treasuries' deficit between weekly taxes.
3. **Commit** this work, these files only, once the user is happy.

## User decisions this session

All are recorded in doc 42 with *user*.

- Everyone has starting money.
- Fast-forward rather than fewer ticks.
- Travel keeps to the roads; travellers who meet step around each other.
- Porters carry provisions given by whoever sends them.
- Standing orders, renegotiated by porters.
- Vineyards ship grapes (they exist in Ser Ferro; they only yield in summer and autumn).
- No ham-fisted fixes: no money or food made out of nothing.
- Taxes are weekly. No more taxation than that; sinks that pay for real work instead.
- Children's stipends only when safely affordable, growing with wealth; children spend freely.
- Grown wolves buy things they want, never weapons or armour.
- Households stock 5 to 8 days of food at a time.
- Odd jobs for 1 to 5 hands with split pay, funded more where more are poor; public job pay rises with full coffers and
  many poor.
- Towns and churches subsidise industry.
- Children keep in friend groups and would rather not work.
- Households share a purse; a comfortable one keeps a member at home, a poor one sends everyone to work.
- Businesses hire and improve their premises, which wear away without upkeep.
- Demand drives prices, wages and hiring: unfilled work raises its pay and pulls workers from worse-paid posts.
- Later: odd jobs and hires should teach skills toward apprenticeships and taking over businesses (not built).
