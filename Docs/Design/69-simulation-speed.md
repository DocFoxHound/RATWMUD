# 69. Simulation speed: where a run's time goes, and a fast mode (2026-10-31)

The user, 2026-10-31 (verbatim):

> Okay, so we need a fast mode. There's no reason these simulations should be taking nearly an hour for two months.
> This is what I want you to do before we implement any changes though. Look at the code and the game process and
> measure places that we can make the simulation faster. Whether that's code efficiency, database retrieval, walking
> and pathing searching, we need to get the simulation WAY down so we can really crank on the speed

Sections 1-8 are the measurement and the plan (nothing in `Core/` or the harnesses changed for them; the instrumentation
lived in a scratch copy of the code, described under "How it was measured"). **Built 2026-10-31 on `r2-sim-fast`**:
section 9 says what, and how fast it runs now; section 10 is how to run an economy simulation.

## 1. The short answer

- **A game day of the economy run costs 98 s now** (`world_check --simulate ... --economy`, the r2-country loop on
  build 45, any number of threads). 60 days take about 100 minutes, as the user saw.
- **Almost 90% of it is one harness player that nobody needs, and 5% the harness's own sampling.** `world_check`
  always adds a benchmark player at the spawn in the city, walking east and west. The harness builds that player's
  view five times a second (47% of the run), the world keeps its map memory as it walks (21%), the harness sends it a
  motion frame every tick (4%), and the residents around it are onstage, where they decide every second and walk tile
  by tile (most of the rest). The held sampler, the road tally and two `loadedCells()` calls a tick are the 5%.
- **Without the player, and without the harness's per-tick sampling, the same world runs a game day in 2.8 s**
  (2.4 s of simulation and 0.4 s to start), **35 times as fast**. 60 days would take under 3 minutes. The economy's
  daily lines agree on day 0 to a penny or two and differ by 1-2% on a field on days 1 and 2, in no direction (table
  in 5): about what the opt-in fast steps move them.
- **Threads don't help because almost nothing runs on them.** Only the first half of the residents' decisions (what
  each one decides) is spread over the pool: 1-2% of the run with the player, 16% of the world's work without it. A
  day with the player takes 98.3 s on 1 thread and 99.4 s on 4; without it 2.41, 2.34, 2.33 and 2.18 s on 1, 2, 4 and
  8.
- **Pathing and the database are not where the time goes.** With no player there are no A* route searches at all;
  offstage walking (a road way per leg and the cell-to-cell steps) is a quarter of the world's work. The harness reads
  files (0.4 s to start); the server reads Postgres once at start (0.9 s) and then costs nothing a tick.
- **The game server's fast-forward is capped, not slow.** With nobody connected `--speed 1000` runs exactly 20,000
  ticks a second (a game day in 14.4 s), each tick 0.007-0.011 ms, so the game thread is busy 14-23% of the time.
  The cap (`Game::MaxSpeed`) is what stops it.
- **The plan** (section 6): first the harness (no player, no per-tick sampling), then cheap pure-efficiency fixes,
  then an opt-in `--fast` (the unseen step and straight walking), measured at a further 2.6 times. **Targets: 60 days
  in under 3 minutes with the game's rules unchanged, and under 1 minute with `--fast`; a year in about 17 and 4
  minutes.**

## 2. How it was measured

All on this machine: an i9-13900H laptop (6 P-cores with two threads each, CPUs 0-11; 8 E-cores, CPUs 12-19), 62 GB,
Fedora 43, GCC 15. Two other 30- and 60-day runs of the r2-country agent were going at the same time on two P-cores.
Timings that are compared with each other were run pinned to free P-cores (`taskset -c 0`, `2`, `8`, `10`): a run the
scheduler puts on an E-core is markedly slower, so unpinned timings on this machine are noisy.

**What was run.**

- **The representative run**: the r2-country agent's harness binary `wc_v4` (a Release build of `r2-country` at
  d4dd755; my RelWithDebInfo build of the same commit gives the same digest, `a58828164655098a` for 06:00-07:00) on
  the build-45 export of Ridgemere 2.0 (559 residents, 828 cells), exactly as that agent runs it:
  `wc_v4 b45 --simulate 6 30 --no-check --deterministic --threads 4 --economy FILE`. Its first economy line is
  byte-for-byte that of the agent's own 30-day run (`r30c.jsonl`).
  - **The Data directory matters.** The binary takes `Data/Items` (the Country, `country.json`) from `RATW_DATA_DIR`,
    else a `Data/` above the working directory, else its source tree. Run from a `ridgemere2` checkout it silently
    takes that checkout's Data and the Country is missing (`country_spent` 0). Every run below sets
    `RATW_DATA_DIR` to r2-country's Data.
- **`ridgemere2` HEAD** (6189ee7) `world_check`, built by me: the same 98.5 s a day and the same people digest
  (`c9526293752a8806` at the day's end) as the r2-country build. The Country's hourly loop costs about 4% of the
  world's own work (below); with the player it is lost in the noise.
- **Instrumentation** (a copy of r2-country's tree in my scratch folder, never committed): `Tests/world_check.cpp`
  with
  - timers around each part of the harness's tick loop (the economy watch, `World::tick`, the road tally, the "held"
    sampler, the ambient look, bookkeeping, the motion frame, the views) and the schedules' seven stage totals printed
    in every run;
  - `--players 0` allowed (it is clamped to at least 1 today);
  - `RATW_BARE=1`: the road tally, the held sampler and the ambient look skipped (they only read the world);
  - `RATW_NO_LOADED=1`: the two `World::loadedCells()` calls a tick skipped;
  - `RATW_UNSEEN_STEP=N`: `Society::UnseenStep` made a variable (`static inline int`), set from it;
  - `RATW_STRAIGHT=1`: `World::setStraightOffstage(true)` (exists).
  None of these changes what the simulation does unless asked (the digest and economy lines are unchanged with them
  off).
- **Profilers.** `perf record -F 999 -g` (frame pointers: built `-O2 -g -fno-omit-frame-pointer`), reported with
  `perf report --children --sort symbol` (cpu_core samples); `valgrind --tool=callgrind` over a whole game day
  without the player (21.4 billion instructions, about 3 minutes).
- **The server**: `ratw_server` from `ridgemere2` HEAD, `--scratch` (writes nothing), nobody connected, `--speed
  1000 --perf-log 10`, once on the export (`--world-export b45`) and once from the database (`--database ridgemere2`,
  read only through the game role), 40-70 s each, with a 20 s `perf record -p` of the first.

**The scripts** (in the session's scratch folder,
`/tmp/claude-1000/-home-martinb-Documents-GitHub-MUDProject/da6a1181-05f7-491a-a334-c3e7343d5540/scratchpad/speed/`;
not kept in the repo: they belong to the instrumentation):

| Script | Does |
|---|---|
| `instrument.py FILE` | applies the harness timers and `--players 0` to a copy of `Tests/world_check.cpp` |
| `time_run.sh BIN FROM TO THREADS TAG [args]` | one timed run on `b45` (`/usr/bin/time -v`), Data set, output in `runs/TAG.*` |
| `b2.sh`...`b7.sh` | the batches below (a day with and without the player; threads; unseen step and straight; busy-hour digests; threads with the player) |
| `perf1.sh`, `perf2.sh`, `perf3.sh` | the perf profiles; `rep.sh NAME` prints one |
| `server_ff.sh`, `server_db.sh` | the scratch server at `--speed 1000` on the export and from the database |
| `econ_diff.py A B ...`, `econ_table.py FIELDS A B ...` | the economy lines' differences, and a table of chosen fields by day |

To repeat the headline without any instrumentation:

    export RATW_DATA_DIR=<r2-country checkout>/Data
    taskset -c 0 /usr/bin/time -v wc_v4 b45 --simulate 6 30 --no-check --deterministic --threads 1 --economy day.jsonl
    perf record -F 999 -g -- world_check b45 --simulate 9 11 --no-check --deterministic --threads 1 --economy x.jsonl
    perf report --children --sort symbol --stdio -g none

and, with the instrumented harness, `RATW_BARE=1 RATW_NO_LOADED=1 world_check b45 --simulate 6 30 --no-check
--deterministic --threads 1 --players 0 --economy day.jsonl`.

**Timing basics, confirmed.** 20 ticks a second (`server.tick(0.05)`); a game hour is 600 world seconds
(`world_check`: "A game hour is 600 real seconds"), so a game day is 14,400 s and 288,000 ticks. `World::tick` runs
two movement steps a tick (at most 1/30 s each) and the schedules every half second (28,800 passes a day); the
society decides once a second (14,400 a day), the unseen every `Society::UnseenStep` (5) seconds (2,880 a day). At
98 s a day a tick costs 0.34 ms.

## 3. Where the representative run's day goes

One game day, 06:00 to 06:00, one thread (the instrumented harness's timers; perf agrees within a few percent).
93.5 s of measured work in 98.3 s of wall time (0.4 s to start; the rest is a second `loadedCells()` call a tick
outside the timers, and the report at the end).

| Part | s a day | share |
|---|---:|---:|
| **The harness's view of its player** (`prepareViews` + `World::snapshot`, five a second) | 44.1 | 47% |
| **`World::tick`** | 41.5 | 44% |
| — the player's map memory (`views` inside the tick: sight rays each time it reaches a new tile) | 20.0 | 21% |
| — the schedules (half-second passes) | 13.3 | 14% |
| —— the society (residents deciding; onstage ones every second) | 4.5 | |
| —— errands (where each one goes; walking legs) | 5.1 | |
| —— routes planned (A*: 2,851 searches, 1.9 M nodes) | 1.5 | |
| —— the stage's cells kept loaded around the player | 1.3 | |
| —— crime | 1.0 | |
| — movement (walking tile by tile, onstage) | 3.6 | 4% |
| — separation (bodies apart) | 2.0 | 2% |
| — streaming (cells in and out of memory) | 1.3 | 1% |
| The motion frame for the player (every tick: who in the cell it sees) | 3.7 | 4% |
| The "held" sampler (every second, every entity) | 1.7 | 2% |
| The road tally (every second, every entity) | 1.1 | 1% |
| The economy watch (hourly) and one `loadedCells()` a tick | 1.2 | 1% |

**The top hot spots by perf** (09:00-11:00, a busy morning, inclusive share of samples):

| # | Where | Share | What it is |
|---|---|---:|---|
| 1 | `World::visibleTileMask` / `sight::lineOfSight` | 40% | the player's sight rays (map memory 29%, the harness's views 12%) |
| 2 | `World::snapshot` (excluding the sight it asks for) | 28% | the harness's view: entities, scents, the map |
| 3 | `World::scentCues` / `AirRoutes` | 7% / 6% | an air map of all 65,536 tiles of the cell, rebuilt for every view |
| 4 | `World::continueSchedules` | 8% | everything the residents do (the economy among it) |
| 5 | `Society::decideAuthored` | 2.3% | residents deciding (in 4) |
| 6 | `World::separate` | 2.0% | |
| 7 | `World::entitiesIn` | 2.0% | |
| 8 | `World::visionClarity` | 1.9% | mostly the harness's motion frame |
| 9 | `World::integrate` | 1.7% | |
| 10 | `World::headFor` + `moveOffstage` | 1.5% | walking |
| - | `loadedCells`, `tendAwareness`, `tendCrime`, `environmentAt`, `guardOnDuty` | about 1% each | |

So in the run as it is, the simulation proper (the society, errands, walking and the economy loop) is under a tenth.
Everything above it is the cost of watching a player who doesn't play.

## 4. The world without the player

The same day with `--players 0`, the harness's sampling off (`RATW_BARE`, `RATW_NO_LOADED`): **2.80 s wall, 2.41 s of
it the world**, the rest starting up (0.42 s; the harness loads the world twice, once for its check even with
`--no-check`). Everyone is offstage, the unseen tier: they decide every 5 s, each decision a step's worth, and walk in
legs (`moveOffstage`) with no tiles walked and no A* searches.

The harness's own sampling, without the player, still cost as much as the world: the held sampler 1.46 s a day, the
road tally 0.84 s, and two `World::loadedCells()` calls a tick about 2.4 s (each counts through the 828 cells'
`std::map`, large objects far apart in memory, 576,000 times a day; perf put over half of such a run's samples
there). The economy watch itself is 16 ms a day.

**`World::tick` a day, by stage** (ms): society 1,186; errands 906; crime 30; bonds 27; roads 2; streaming 82;
movement, separation and views 22; the rest of the tick about 80.

**Callgrind over the whole day** (21.4 billion instructions: 3.8 billion starting up, 16.1 billion in `World::tick`,
about 1.5 billion the harness's own). As shares of the simulation's 17.6 billion:

| What | Share | Calls a day | Notes |
|---|---:|---:|---|
| **The society's second** (`Society::tick` / `decide`) | **40%** | 14,400 (2,880 with the unseen due) | |
| — what each resident decides (the parallel half: the `decide` lambda) | 16% | 2,880 passes x 559 | the only part on the threads |
| — doing it, in ID order, and the pass's and the second's setup | 19% | | string-keyed lookups throughout: the cell → place map (1.46 M lookups, 3.9%), `restockFromStore` (154,000 calls, 1.5%), `items::producerFor` (976,000 calls, 1.0%), `account(string)` |
| — the Country's hourly loop (`loopHourly`: `householdsShop`) | 4.2% | 24 | the new economy loop: cheap |
| **Walking offstage** (`headFor` → `moveOffstage`) | **26%** | 23,595 legs | |
| — `World::roadWay` (a Dijkstra over the cell's 256 x 256 road map) | 17% | 3,733 (cache misses) | 0.81 M instructions each |
| — `cachedSteps` → `firstSteps` (cell-to-cell steps over 828 cells, string maps) | 6.4% | 330 misses of 4,201 | the cache is cleared whenever a cell loads (`rebuildFixtureIndex`) |
| `World::placeOnStage` | 5.2% | 16.1 million | every resident, every half second, nobody on any stage |
| `errand` + `crimeErrand` | 3.5% | 1.61 million each | |
| The errand pass's own string compares (`activity`) | 1.5% | | |
| `tendCrime`, `planDays`, `tendBonds` | 1.5%, 1.4%, 0.8% | 28,800 each | |
| `World::tick` outside the schedules (per tick, whatever happens) | 2.8% | 288,000 | 1,700 instructions a tick: weather, streaming, the awake lists, tendX, the profile's clocks |

**By kind**: string compares, hashing and copies and the allocations behind them (`memcmp` 8.3%, `_Hash_bytes` 4.7%,
`operator==` 2.3%, `strlen` 2.1%, string-keyed trees 1.9%, malloc/free about 4.6%, `memcpy` 2.5%) come to roughly
**28% of every instruction**. `roadWay`'s own loop is 13% (exclusive).

**Fixed cost and cost by resident.** Of a pure day's 2.4 s, about 0.18 s is per tick whatever happens (0.6 µs x
288,000) and 2.15 s is the schedules, which scale with the residents: about 3.8 ms per resident per game day (1.3 µs
per resident per unseen pass). A city of twice the people would take about twice as long; a quieter tick would hardly
change it. (Residents were not scaled in a run: the roster is in the export. This is from the profile.)

**Over days.** Three days without the player cost 0.0079, 0.0060 and 0.0059 ms a tick by day (the first includes the
morning's start). Growth over 30 or 60 days was not measured (the rules here allowed three days); the by-day cost line
`world_check` prints will show it.

## 5. Threads, behaviour-changing steps, and what stays the same

**Threads** (one day, the representative run and the world without the player):

| Threads | 1 | 2 | 4 | 8 |
|---|---:|---:|---:|---:|
| with the player, a day: s (wall) | 98.3 | | 99.4 | |
| with the player, 09:00-11:00 (pinned): s of work | 15.3 | 14.9 | 14.9 | 13.8 |
| without, a day: s of world work | 2.41 | 2.34 | 2.33 | 2.18 |
| without, a day: CPU used | 99% | 107% | 116% | 127% |

(The 8-thread morning ran alone; the others three at once.)

- **Why not**: the only work on the pool is `decideAuthored`'s first half, spread in shares of 64 residents (nine jobs a
  pass). With the player it is 1-2% of the run; without, 16% of the world's work, so eight threads can give at most
  1/(0.84 + 0.16/8) = 1.16 times, and give 1.10. Everything else (doing what was decided, errands, walking, crime, the
  views of a single player) runs on the main thread. Waking the pool costs 50-100 µs a round (doc 31), paid 2,880
  times a day. On this CPU a pool thread on an E-core also makes the main thread wait for the slowest share.
- **The deterministic run ends the same on any number**: the people digest is the same on 1, 2, 4 and 8 threads, and
  the economy file of 1 and 4 threads is byte-for-byte the same.
- **The cores are better used running several runs side by side**: each is one core. Six P-cores means six runs at
  full speed (pinned, one to a core).

**Behaviour-changing steps, measured** (three game days without the player and sampling, each pinned to a P-core;
seconds of world work for the three days):

| Run | base (step 5) | straight | step 10 | step 15 | step 30 | step 30, straight | step 60 | step 60, straight |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| s for 3 days | 7.80 | 8.15 | 5.53 | 4.71 | 3.61 | **2.98** | 3.20 | 2.60 |
| against base | 1.00 | 0.96 | 1.41 | 1.66 | 2.16 | **2.62** | 2.44 | 3.00 |
| society (ms) | 4,538 | 5,118 | 2,775 | 2,146 | 1,359 | 1,355 | 1,050 | 1,054 |
| errands (ms) | 2,122 | 1,786 | 1,694 | 1,540 | 1,333 | 700 | 1,237 | 628 |

- **The unseen step** (`Society::UnseenStep`, 5): the unseen decide every N seconds, each decision N seconds' worth
  (doc 31). 30 halves the run; past that, what is done every second regardless (the society's second, the hourly
  loop) and the errands are what is left.
- **Straight walking offstage** (`World::setStraightOffstage`: a leg's time by its straight line and the detour, no
  road way): nothing measurable on its own (the errands fell by 0.3 s, within the run-to-run noise of the
  society's), 17-20% more on top of a longer step. Together they are doc 64's `--fast` (2.4 times there, on the old
  world; 2.6 here).
- **Straight walking changed no economy figure in three days** (below: its columns are the base's, and step 30's
  with or without it), so it is the safer of the two.

**What they did to the economy** (the daily lines; the player's run is the r2-country agent's own `r30c`):

| Day | Field | with the player (`r30c`) | without, base | straight | step 15 | step 30 | step 30, straight | step 60, straight |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | residents spent | 5,003 | 4,941 | 4,941 | 4,941 | 4,944 | 4,944 | 5,003 |
| 1 | wages paid | 3,912 | 3,917 | 3,917 | 3,917 | 3,896 | 3,896 | 3,912 |
| 1 | profit | 675 | 659 | 659 | 655 | 685 | 685 | 666 |
| 1 | purses | 36,650 | 36,701 | 36,701 | 36,697 | 36,703 | 36,703 | 36,641 |
| 2 | drawn from the Country | 5,774 | 5,650 | 5,650 | 5,651 | 5,701 | 5,701 | 5,751 |
| 2 | profit | 265 | 289 | 289 | 295 | 246 | 246 | 257 |
| 2 | Gini, earners | .494 | .493 | .493 | .493 | .493 | .493 | .494 |
| 2 | households short | 2 | 2 | 2 | 2 | 0 | 0 | 2 |

Day 0 is the same in all (but a penny or so in carriage). From day 1 they differ by about 1-2% on a field, in no
direction; the player's run differs from the run without it by as much as the fast steps do. Three days don't say what
30 or 60 do: that is the side-by-side the user approves (section 7).

**What stays the same, and how it is checked.**

- **Pure efficiency** (caching, data structures, threads, skipping work whose result is thrown away) must leave a
  run exactly as it was: the economy file byte-for-byte and the digest, over three days, at 1 and 8 threads.
- **The digest as it is can't tell the unseen apart.** `world_check`'s people digest (each entity's cell, position,
  activity, posture and task) came out the same for the base run, straight walking and step 30, ended at 06:00
  (everyone in bed) and at 15:30 alike (`9b8b892e023e16c5`), though their economies differ. It should also mix
  the society's state: each account's cash and stock, each life's hunger, fatigue, progress and goal, and the legs
  under way. Until then the economy file is the check.
- **Behaviour-changing steps**: no harness player, a longer unseen step, straight walking, coarser ticks. Each is
  checked against a run without it, side by side over 30 days, by the user.

## 6. The speedups, ranked

Gains are on the economy run. "Measured" means a run above; "estimated" is from the profile's share, assuming the
work it names mostly goes.

| # | What | Gain | Basis | Effort | Risk | Changes results? |
|---|---|---|---|---|---|---|
| 1 | **No harness player in an economy run** (`--players 0`), and the harness's per-tick sampling off or thinned: the held sampler and road tally once a game minute or not at all, `loadedCells()` once a game minute, the `getenv`s once | **98 s → 2.8 s a day (35x)** | measured (sections 3 and 4) | small: `Tests/world_check.cpp` only | low | slightly: the residents near the spawn are unseen like the rest. 1-2% on a field by day 2; validate 30 days |
| 2 | **The server's cap**: `--speed max`, or a speed above 1000, when nobody is connected | 14.4 s → about 2-3 s a game day | measured: 20,000 ticks/s at 14-23% busy | small (`Game::MaxSpeed`, the server loop) | low | no (the same ticks) |
| 3 | **Cell-to-cell steps kept**: `stepsCache_` not cleared when a cell loads (routes don't depend on what is loaded: `firstSteps`' own comment), or indexed by number | 6% | callgrind: 330 `firstSteps` a day, 3.4 M instructions each | small | low | no (must check the digest) |
| 4 | **`placeOnStage` only when the stage can have changed** (nobody onstage: everyone stays offstage) | 5% | callgrind: 16.1 M calls a day | small | low | no |
| 5 | **The profile's clocks off in a fast run** (`World::tick` reads `steady_clock` about ten times a tick, and each stage) | 2-3% now; 9% with `--fast` | perf (`steady_clock::now`; 9% of a step-30 straight run) | small | none | no |
| 6 | **Road ways cheaper**: `roadWay` is a Dijkstra over 65,536 tiles per leg not in its cache (3,733 a day). A graph of the cell's road junctions searched instead, or the way from a tile to the nearest junction plus a table between junctions | 10-15% | callgrind: 17% (0.81 M instructions a call) | medium | medium (ways must come out the same, or the change is behaviour) | no if exact |
| 7 | **Text keys to numbers in the society's second**: the cell → place map, `restockFromStore` asked 154,000 times a day for a few shops' hours, `producerFor`, `account(string)`, `hasFood`, `items::good` | 10-15% | callgrind: strings and allocation are 28% of all instructions | medium-large (many small changes) | low | no |
| 8 | **The unseen step as a knob** (`--unseen-step N`, `Society::setUnseenStep` back) | **2.16x at 30** | measured (3 days) | small | medium | yes: decisions in coarser steps (doc 31's rule: a step's worth) |
| 9 | **Straight walking offstage** (`--straight`, exists) | 1.2x on top of 8 | measured | none | low | yes: legs' times by the line |
| 10 | **Coarser ticks when nobody is onstage**: the harness ticks 0.5 s, not 0.05 s, while the stage is empty | about 7% now, about 17% after 7 and 8 | 0.6 µs a tick of fixed cost, 288,000 ticks a day | small-medium | medium (the half-second schedules already run on 0.5 s; anything timed by tick count would change) | yes, a little |
| 11 | **More on the threads**: a pass's legs planned in parallel (`roadWay` per walker is independent; its scratch made per thread), the "doing" half's independent parts | up to 1.2-1.3x | Amdahl from the shares | medium-large | medium (data races; ThreadSanitizer) | no |
| 12 | **The Country's hourly loop** (`householdsShop`) indexed | 3-4% | callgrind 4.2% | small | low | no |
| - | For runs and servers **with** players (the city checks, real play): the sight rays (`visibleTileMask`, 40%), the air map rebuilt for every view (`AirRoutes`, 65,536 tiles), the snapshot | large for those | perf, section 3 | medium | medium | no |

What doesn't need doing for speed: the database (0.9 s to start from Postgres, 0.17 s from the export; nothing a
tick on a scratch server: `database=0.00 ms`, saves 2.7-3.0 ms every 30 real seconds, which at 1000x is every 8 game
hours; the real server writes behind on its own thread, doc 31), the export's
loading (0.4 s), the economy loop, and route searches (none without a player).

## 7. A fast mode

**Flags** (`world_check`; the server's in brackets):

- `--players N` takes 0, and **`--economy` without `--players` means 0**: an economy run has nobody watching. (A
  city check that wants the player says `--players 1`, as now by default without `--economy`.)
- **With no player, the harness samples nothing per tick**: the held sampler and the road tally once a game minute
  (or behind `--sample`), `loadedCells()` once a game minute, the trace `getenv`s read once.
- `--fast`: `--unseen-step 30 --straight` (opt-in; results change). `--unseen-step N` and `--straight` on their own.
- `--no-profile`: `World::tick`'s per-stage clocks off (the by-day cost line then comes from the harness's own clock).
- The by-day cost line stays, so a cost that grows over a long run shows by name.
- `--threads` stays (default 8); the docs say that more than one buys about 10%, and that six runs side by side, each
  on its own P-core (`taskset -c 0`, `2`, `4`, `6`, `8`, `10`), use the machine.
- [Server: `--speed max` with nobody connected; and, if the user wants economy runs to include what only the game
  does (below), `ratw_server --world-export DIR --scratch --speed max --for-days N --economy FILE`.]

**Defaults**: an economy run is fast in the pure sense (no player, no sampling) by default; the behaviour-changing
`--fast` is never a default until the user has compared it.

**Targets and the path** (60 game days; a year in brackets):

| Stage | s a game day | 60 days | Basis |
|---|---:|---:|---|
| now | 98 | 98 min | measured |
| 1. the harness (no player, no sampling) | 2.8 | **2.8 min** [17 min] | measured |
| 2. pure efficiency (3, 4, 5, 7, 12; then 6) | about 1.5 | about 1.5 min [9 min] | estimated: -35-45% of the world's work |
| 3. `--fast` (unseen step 30, straight) on 1 | 1.0 | 1.0 min [6 min] | measured |
| 3 on 2 | about 0.6 | **about 40 s** [4 min] | estimated |

**Target: 60 days in under 3 minutes with the game's rules as they are (step 1 alone), and under 1 minute with
`--fast` once step 2 is in.** Step 1 is a day's work on the harness; step 2 a few days; neither touches a rule.

**How each step is validated**:

- Pure efficiency (step 2): three days, 1 and 8 threads, the economy file byte-for-byte and the digest (with the
  society's state mixed in, section 5) against the code before. No user run needed.
- Step 1 (no player) and `--fast`: a 30-day run side by side with one as it is today, compared field by field by day
  (Gini, purses, tills, profit, food and needs bought, wages paid, tills short, households short, hungry), the user
  deciding whether the difference is within what a run's start alone moves (as doc 64 did: two starts of the same
  code). **The r2-country agent's 30- and 60-day runs going now (`r30c`, `r60c`) are the "as it is" side**: the same
  binary and export with the player; a 30-day run without the player takes about a minute and a half.

## 8. Open questions for the user

1. **The harness player.** Is it right that an economy run has no player? (The city checks keep theirs.) The
   residents near the spawn then live like everyone else out of sight.
2. **`world_check` or the server for long runs?** `world_check` runs only `World::tick`. `Game::tick` also runs
   newcomers and births (`makeResidents`: in `world_check` they are only asked for, never made), letters, chapters,
   factions, estates, camps, projects and storylines. Over 60 days the population's changes may matter to the
   economy. A headless server run (the export, `--scratch`, `--speed max`) would include them, at about the same cost
   (its `Game::tick` is about a sixth of a tick on top of the world's).
3. **The unseen step for `--fast`**: 30 (doc 64's choice; measured 2.2 times here) or 60 (2.4 times)?
4. **Coarser ticks** (item 10): worth trying once 7 and 8 have made the fixed cost the larger part?
5. **Checkpoints**: a run that saves the world at day N and starts from it (doc 64's econ_watch had `--from`) would
   let several variants share their first days. Wanted?
6. Growth over long runs is unmeasured here (three-day limit). The first 30-day run of step 1 will show it.

## 9. Progress: built (2026-10-31, branch `r2-sim-fast`)

The user's decisions (2026-10-31, verbatim): "No player in the economy runs. We don't need the player at this point."
Long runs in the harness (world_check), as recommended, "but I do want to simulate births and deaths." "You can build
out all of the suggestions for speed that you found."

Branch `r2-sim-fast`: `r2-country` (d4dd755) with `ridgemere2` (this doc) merged, then the work. Measured on the build-45
export (559 residents), `RATW_DATA_DIR` the branch's `Data` (the Country present), deterministic, each run pinned to its
own P-core (the 8-thread runs to CPUs 0-11, one at a time), the machine otherwise idle.

### What was built

| Flag (`world_check`) | Does | Results |
|---|---|---|
| `--players N` | 0 allowed; **`--economy` without `--players` means 0** (a city check without `--economy` keeps its 1) | the spawn's residents are unseen like the rest |
| *(no player)* | nothing sampled tick by tick: no held sampler, road tally, crowds, per-tick times or `loadedCells()` (the most in memory once a game minute); the trace `getenv`s read once; the road tally's map not loaded | exact: the same economy file and digests as with `--sample` |
| `--sample` | the per-tick sampling back without a player (the old behaviour, for the city's checks) | |
| *(each run day)* | its wall time printed as it ends (`run day N: S s wall, T ticks; S s so far`), and a table of them at the end, with the stages' seconds while the profile is on | |
| *(births, newcomers)* | made after every tick by `World::makeRequestedResidents`, the game server's own path for a world from files (`Game::makeResidents`); `--no-births` only asks for them, as before | changes results once anyone is born or sent for |
| *(deaths)* | nothing to add: old age (`World::tendOldAge`, daily, in `tendBonds`) is the world's, run the same in the harness and the server; the game has no death by hunger or illness, and a fight's deaths need a player | |
| *(economy file)* | `births`, `newcomers`, `deaths` each day | |
| *(digest)* | a second digest, **society**: every account's cash and goods, every life's needs, task, goal and home, the posts' holders, and the legs under way (`World::legsDigest`) | it tells apart runs the people digest can't (step 30, coarse ticks) |
| `--no-profile` | `World::setProfiling(false)`: `World::tick`'s and the schedules' clocks off (with `--deterministic` nothing reads the clock at all) | exact |
| `--unseen-step N` | `Society::setUnseenStep` (back from Phase 5): the unseen decide every N s, each decision N s' worth | changes results |
| `--straight` | `World::setStraightOffstage(true)`: offstage legs timed by their straight line and the detour, no road way | changes results (none in three days' economy, by chance: below) |
| `--coarse-ticks` | while nobody is onstage (`World::anyoneOnstage`), the harness ticks half a second at a time (on the half seconds, so the hours fall where they did) | changes results (positions; three days' economy came out the same) |
| **`--fast`** | `--unseen-step 30 --straight --coarse-ticks` (an explicit `--unseen-step` wins). Opt-in, never a default | changes results |

The server (`ratw_server`): **`--speed max`** (and `/speed max` in the Dev Console) lifts `Game::MaxSpeed` while nobody is
connected: the game thread ticks back to back in 20 ms batches between the network's turns and tells the game each second
how fast that is (`Game::setMeasuredSpeed`), so what keeps to real time (saves, polls, the director, voices) still does. A
client connecting brings it to 1000x; leaving lifts it again; `/speed N` ends max. On the export, nobody connected
(`ratw_server --world-export b45 --scratch --speed max`, 45 s): **225,000 ticks a second, about 11,000x: a game day in
1.3 s** (14.4 s at `--speed 1000`). (Tried on the export with `--scratch`, not through `tools/scratch.sh ridgemere2`,
which needs the database's passwords from the main checkout's `Database/.env`.)

Core, exact (the economy file byte for byte, and both digests, over three days at 1 and 8 threads):

- **`placeOnStage` (item 4)**: with no stage anywhere, only an NPC onstage or keeping with a leader is looked at.
- **`World::errand` (item 7)**: out at once when nobody is seated at a table, no occasion is on and no contract is taken
  (1.6 million calls a day, each two hash lookups and string compares).
- **The hourly shopping (items 7, 12)**: the open shops' till accounts kept with them through `householdsShop` (436,000
  account lookups by name a day, about half the hourly loop).
- **`--no-profile` (item 5)**, above.

### The speedups, item by item

| # | Item | Status |
|---|---|---|
| 1 | No player, no per-tick sampling | **built**, the economy run's default |
| 2 | The server's cap | **built**: `--speed max` |
| 3 | `stepsCache_` kept across cell loads | **nothing to build**: counted over one day and over three, the cache is cleared 6 times, all as the worlds load; cell loads don't clear it (`indexSeams` keeps it; only a seam leading somewhere new, or a dead end found or forgotten, does). Its misses are cold ones: 330 the first day (each starting cell's first ask), 14 a day after. Section 4's "cleared whenever a cell loads" was wrong |
| 4 | `placeOnStage` only when the stage can change | **built**, exact |
| 5 | The profile's clocks off | **built**, exact: `--no-profile` |
| 6 | A cheaper `roadWay` | **dropped**: no cheaper search gives the same ways. Its result depends on more than the tiles: the straight runs start from the walker's exact point, and the cache (by tile) keeps the first walker's, so even the cache's order is part of the result; a junction graph or A* breaks ties differently. `--straight` (in `--fast`) does without it |
| 7 | Number keys in the society's second | **in part**, exact: the errand's way out and the till accounts (above). The rest left: the costliest (catalog lookups in `hasFood`, 1.6 million a day; `skyAt`, 1.8 million) are in the threads' half already, and the accounts by name can't change container without changing the ledger's order |
| 8 | The unseen step | **built**: `--unseen-step`, 30 under `--fast` |
| 9 | Straight walking | **built**: `--straight` (existed), under `--fast` |
| 10 | Coarser ticks while nobody is onstage | **built**: `--coarse-ticks`, under `--fast` |
| 11 | More on the threads | **dropped** (nothing new): what is left on the main thread must go in order: the doing half (the ledger's order), the errand pass (its `roadWay` misses, about 1.3 a pass, whose cache order is part of their result: 6), the hourly sales. A pool round costs 50-100 µs, 2,880 times a day, against a miss of about 0.2 ms. The existing parallel half pays better than section 5 measured, now the rest has shrunk: 8 threads take a day from 1.52 to 1.17 s |
| 12 | The Country's hourly loop indexed | **built**, exact: the till accounts |

### Time a game day, by stage

Three game days (06:00 to 06:00), the mean a day (the first includes the start: 0.4 s loading, the first day's cold
caches); then the 30-day runs.

| Stage | 1 thread | 8 threads | 60 days | Basis |
|---|---:|---:|---:|---|
| as it was: the harness's player, sampling every tick | **84.6 s** | | 85 min | one day (98 s in section 3, with two other runs going) |
| 1. no player, no per-tick sampling | 1.68 s | 1.37 s | 1.4 min (8 threads) | 3 days |
| 2. + the exact speedups and `--no-profile` | **1.52 s** | **1.17 s** | **1.2 min** (8 threads) | 3 days; two game days on one thread: 29.1 billion instructions against 32.8 (-11%) |
| 2, a 30-day run (4 threads) | | 1.02 s | 61 s | 31.0 s for 30 days |
| 3. `--fast` | **0.51 s** | **0.40 s** | **24 s** (8 threads) | 3 days |
| 3, a 30-day run (4 threads) | | 0.37 s | 22 s | 11.4 s for 30 days |
| (`--straight` alone) | 1.39 s | | | 3 days |
| the server, `--speed max`, nobody connected | 1.3 s | | 1.3 min | 45 s measured |

A year: about 6 minutes (stage 2, 4 threads) and 2.3 minutes (`--fast`). **Growth over 30 days: none**: the run days of
the 30-day runs cost 0.85-1.31 s (stage 2; the first 1.52) and 0.30-0.40 s (`--fast`; the first 0.53), flat. The day
with the player (84.6 s) is what the city's checks still cost.

### Byte-identity checks (three game days, `--simulate 6 78`)

| Run | Economy file (md5) | Digest: people, society |
|---|---|---|
| no player (stage 1), 1 thread | `abc255643162` | `ba95b9e62ed86fb`, `3a3afb83d45699f0` |
| the same on 8 threads; with `--sample`; with `--no-profile` | the same | the same |
| stage 2 (the exact speedups), 1 and 8 threads, with and without `--no-profile` | **the same** | **the same** |
| `--straight` | the same (by chance: no leg's time moved a purchase in three days) | the same |
| `--coarse-ticks` | the same | `b1cb00d83460aa8d`, `a72c74a2b89d2d44`: differ |
| `--unseen-step 30` | `3a6c68e9143d`: differs | `ba95b9e62ed86fb`, `4b01486ccfe2d722` |
| `--fast` | `cdcb5f10f851`: differs | `b1cb00d83460aa8d`, `d0ea20e3bdaf68ef` |

The no-player run's first days are those of section 5's base without the player (day 1 profit 659, purses 36,701; day 2
profit 289).

### Births and deaths

- **Births and newcomers**: `Society::tendCareers` asks for them (a stranger for a post empty eight days in a town under
  its number; a child to a couple at home while the town is short); the game server makes them every tick
  (`Game::makeResidents`: for a world from files `World::makeRequestedResidents`, from the database its own rows for the
  same). world_check now calls `World::makeRequestedResidents` after each tick, so a harness run makes the ones the
  server would from the export. `sim_fast_tests` runs it on a small town: two of 100 die of old age the first day, a
  stranger comes for the smithy after eight days, a child is born to the married couple, deterministically, the same on
  1 and 4 threads and with the clocks off.
- **Deaths**: old age only (`World::tendOldAge`: the yearly Gompertz chance, a day's share, by a fixed lot for each
  resident and day), the same code in both. There is no death by hunger or illness in the game; deaths in fights need
  a player. **None fell in the 30-day runs**: by the deterministic lots the first two fall on calendar days 41 and 56
  (Gunnar Harrowgate, 62; Aud Kemp, 80), 20 in the first 400 days (1.5 expected in 30 days from the ages). A 60-day run
  is the first to show deaths, and the births and newcomers that follow them.
- **Found, not changed**: a child is made from its mother's record (`ResidentSpec r = *model`; the database path copies
  her appearance), so **every child born is female**, on the server as in the harness. Choosing a child's sex (male or
  female) is a rule for the user.

### 30 days, normal against `--fast`

`n30`: stage 2 (no player, births on, the exact speedups); `f30`: `--fast`; `r30c`: the r2-country agent's own run of
d4dd755 with the harness player (no births made), for reference. Selected days, and the mean of days 1-29.

| field | day 1: n30 / f30 / r30c | day 10 | day 20 | day 29 | mean of days 1-29 | f30 against n30 |
|---|---|---|---|---|---|---|
| residents_spent | 4,941 / 4,944 / 5,003 | 4,797 / 4,842 / 4,827 | 2,992 / 3,045 / 3,013 | 5,246 / 5,227 / 5,298 | 4,859 / 4,858 / 4,859 | -0.0% |
| food_bought | 1,101 / 1,098 / 1,097 | 1,127 / 1,151 / 1,142 | 865 / 877 / 867 | 1,308 / 1,322 / 1,329 | 1,189 / 1,190 / 1,186 | +0.2% |
| needs_bought | 431 / 431 / 432 | 330 / 326 / 331 | 105 / 103 / 103 | 292 / 294 / 298 | 295 / 294 / 296 | -0.4% |
| wages_paid | 3,917 / 3,896 / 3,912 | 3,420 / 3,454 / 3,448 | 3,459 / 3,347 / 3,395 | 3,427 / 3,523 / 3,410 | 3,551 / 3,553 / 3,552 | +0.1% |
| profit | 659 / 685 / 675 | 715 / 566 / 613 | 814 / 914 / 899 | 742 / 761 / 881 | 929 / 926 / 927 | -0.3% |
| purses | 36,701 / 36,703 / 36,650 | 36,141 / 35,852 / 36,081 | 36,686 / 36,539 / 36,569 | 36,090 / 35,998 / 36,142 | 36,196 / 36,056 / 36,174 | -0.4% |
| tills | 13,722 / 13,732 / 13,786 | 12,640 / 12,867 / 12,635 | 12,212 / 12,162 / 12,333 | 11,942 / 11,910 / 11,959 | 12,707 / 12,741 / 12,690 | +0.3% |
| gini_earners | .494 / .495 / .495 | .540 / .541 / .540 | .584 / .585 / .581 | .627 / .627 / .624 | .563 / .564 / .562 | +0.1% |
| households_short | 0 / 0 / 0 | 13 / 9 / 7 | 7 / 10 / 12 | 28 / 13 / 48 | 16 / 16 / 15 | +2.0% |
| tills_short | 5 / 6 / 5 | 29 / 31 / 30 | 36 / 38 / 35 | 34 / 29 / 36 | 27 / 26 / 26 | -3.0% |
| hungry_mean | 0 / 0 / 0 | 0 / 0 / 0 | 0 / .125 / 1.208 | 1.542 / 1.333 / 1.333 | .351 / .378 / .366 | +7.8% |
| drawn | 5,880 / 5,881 / 5,887 | 4,670 / 4,616 / 4,485 | 1,733 / 1,872 / 1,694 | 7,104 / 7,386 / 7,543 | 4,532 / 4,531 / 4,535 | -0.0% |
| treasury | 1,811 / 1,813 / 1,800 | 3,538 / 3,634 / 3,617 | 3,416 / 3,571 / 3,400 | 4,183 / 4,317 / 4,121 | 3,424 / 3,521 / 3,439 | +2.8% |
| births, newcomers, deaths | 0 in all | | | | | |

The money fields' 30-day means agree to half a percent; day by day they differ by 1-3% (more on the small counts:
households short, tills short, the hungry, a handful each), in no direction, as the run with the player differs from
both. Whether that is close enough for `--fast` to stand in is the user's call (section 7). (The earners' Gini climbs
from .49 to .63 over the 30 days in all three: the economy's, not the mode's.)

### Answers to section 8

1. No player in economy runs: done (the user). City checks keep theirs (`--players 1`, the default without `--economy`).
2. The harness, with births and newcomers made as the server makes them (the user). Letters, chapters, factions, estates,
   camps, projects and storylines stay the server's (`Game::tick`); `--speed max` runs them all at about 1.3 s a day.
3. 30 under `--fast` (doc 64's); any other with `--unseen-step`.
4. Built: `--coarse-ticks`, under `--fast`.
5. Checkpoints: not built.
6. Growth: none over 30 days (above).

### Tests

`sim_fast_tests` (new; in ctest as `sim_fast_behaviors`, given world_check's path it runs the harness too): births, a
newcomer and deaths on the harness's path; one thread, four, and the clocks off end the same; world_check with no player
(an economy line a day, deaths counted, the same file on 1 and 4 threads), with `--players 1`, and with `--fast`.
`game_tests`: `--speed max` (the cap while anyone is connected, the measured speed only while uncapped, connecting and
leaving, `/speed N` ending it). All 70 tests pass (`build-fast2`).

## 10. Running an economy simulation

The default way, from a checkout of the branch (nothing is written to any database; the export is only read):

    python3 tools/world_build.py export EXPORT --world ridgemere2     # the build to run (reads the database: Database/.env)
    cmake --build build --target world_check
    export RATW_DATA_DIR=$PWD/Data                                     # (else the Country may be missing: section 2)
    taskset -c 0-5 build/world_check EXPORT --simulate 6 1446 --no-check --deterministic --threads 4 --no-profile \
        --economy run60.jsonl > run60.out

- `--simulate 6 HOURS`: from 06:00 for HOURS - 6 game hours (30 days: `6 726`; 60: `6 1446`; a year: `6 8766`). A game
  day takes about 1 s on 4 threads: 60 days in about a minute, a year in about 6.
- **No player and no per-tick sampling** come with `--economy`. **Births and newcomers are made, and the old die**, as on
  the server; the economy line counts them (`births`, `newcomers`, `deaths`).
- `--fast` (opt-in: results change; compare it with a normal run before trusting it): 60 days in about 25 s.
- Each run day's wall time is printed as it ends (`run day N: ...`): a long run's cost by day.
- **Side by side**: 8 threads buy about 1.3x over one; runs pinned to their own P-cores (`taskset`, CPUs 0-11 here) don't
  slow each other, and six at once use the machine.
- Compare runs by the economy file (and the two digests): byte for byte for a change that should change nothing; field
  by field by day for one that changes results: `python3 tools/econ_diff.py A.jsonl B.jsonl` (the fields that differ,
  by day) and `python3 tools/econ_table.py profit,purses,gini_earners A.jsonl B.jsonl` (a table), section 2's scripts,
  kept in `tools/` now.
- With the game's own systems too (letters, chapters, estates...): `ratw_server --world-export EXPORT --save F --scratch
  --speed max` with nobody connected, about 1.3 s a game day; it writes no economy file.
