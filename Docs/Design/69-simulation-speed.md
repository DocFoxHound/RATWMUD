# 69. Simulation speed: where a run's time goes, and a fast mode (2026-10-31)

The user, 2026-10-31 (verbatim):

> Okay, so we need a fast mode. There's no reason these simulations should be taking nearly an hour for two months.
> This is what I want you to do before we implement any changes though. Look at the code and the game process and
> measure places that we can make the simulation faster. Whether that's code efficiency, database retrieval, walking
> and pathing searching, we need to get the simulation WAY down so we can really crank on the speed

This is measurement and a plan only. Nothing in `Core/` or the harnesses changed; the instrumentation used lives in a
scratch copy of the code and is described under "How it was measured".

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
