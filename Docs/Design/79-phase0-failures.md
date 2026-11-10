# Doc 79, Phase 0: what breaks with the loop off, and the kept readers

> **History:** the `staged` rule and `RATW_STAGED_ECONOMY` went with Phase 3; the staged economy is the only economy.

Worktree `.claude/worktrees/r2-staged`, branch `r2-staged` (from ridgemere2 at cab65ee), uncommitted. Build:
`build-p0` (Release). The switch is the rule `staged` (Rules[], default 0). `RATW_STAGED_ECONOMY=1` forces it on in
every Society; that is the test switch used for the "on" run.

## The two ctest runs

After the repair round (below), with `Database/.env` sourced so `save_compat_pg` runs:

- **Rule off** (`ctest -j 10` in build-p0): **84/84 passed** (83 and the new `save_compat_pg`).
- **Rule forced on** (`RATW_STAGED_ECONOMY=1 ctest -j 10`): **67/84 passed, 17 failed**: society, game, ticket,
  store_room, wharf, season, econ_fix, trades, workshops, treasury, steer, farms, selfcorrect, farm_people,
  people_flow, sim_fast, appearance. Each one's first failing check is the one in the table below (re-run
  2026-11-08 after the repair). `save_compat` and `save_compat_pg` pass with the rule on too.
- **Tools:** `tools/test_studio.py` 53 run, OK (16 skipped: no `Database/.env` in a worktree);
  `tools/test_map_editor.py` 1 failure (`test_greyfen_exports_residents_routes_and_economy`, 11 residents for 10),
  the same on a clean `cab65ee`; the Editor's model and lib tests 1 failure each (Greyfen's resident count), the same
  on a clean `cab65ee`.

## The repair round (2026-11-08, after the independent check)

1. **The rule's name in the tools: fixed.** `staged: 1` is in `tools/map_editor.py` RULE_LIMITS and RULE_SWITCHES,
   in `Editor/src/model/model.mjs` RULE_LIMITS, and `"staged": 0` in `Data/Items/city_loop.json` `rules` (with its
   line in the rules' `about`), so `test_every_rule_listed_with_core` passes and the studio's `ruleDefaults` still
   names every rule (`test_the_rules_and_the_services`).
2. **The perf baseline: NOT done.** The run of 2026-11-08T08:04 (`p0perf-out/runs.txt`) died at the disconnect: no
   window was measured. It is a `world_check --simulate` run, so it waits for the user's go-ahead (memory
   ask-before-simulations): `p0perf-scripts/run_windows.sh` against `build-p0perf` (`cab65ee`) for 6-8, 17-19 and
   23-25 at `--players 20`, mean, p99 and worst. **Phase 0's perf item stays open until then.**
3. **A DEV save with the loop's state: still none real.** Making one needs the `cab65ee` server run on DEV's world
   past 18:00 and a night (README, "A richer DEV save"): an economy run, the user's call. Added instead: fixture
   **(f) `road/`**, synthetic, made by the code at `cab65ee` (`save_compat_tests --make-road`): it holds
   `society.loop` with `loop.steer` (the steering on), `outside:wharf`, `estate:`, `stores:`, `caravan:` and `till:`
   accounts. **Still in no fixture: `fund:relief` and `farm:` accounts**, so Phase 3's drops of those two rest on
   hand-made states.
4. **Bandits, letters, escorts, a package with players:** fixture (f) has a `bandits:` hoard holding a player's 20p,
   a caravan with a player's letter (`contract:k1`, posted by `player-bo`) and a player's escort (`contract:k2`,
   taken by `player-ada`); fixture **(g)** is (f)'s save through `world_package.strip()` at `cab65ee` (the players
   poured into the treasury, 20p; not a penny lost). Both are synthetic. A read-only copy of PROD's checkpoint (the
   real PROD-shaped save) is the user's call; it wasn't taken.
5. **Through Postgres: built.** `Tests/save_compat_pg.py` (ctest `save_compat_pg`): each loaded state (dev, dev-old,
   package, road) saved with `game.save_checkpoint` into a throwaway `ratw_test_p0stage_<hex>` (migrated, dropped
   after; checked gone) and loaded back with `game.load_checkpoint`: the same JSON, and `save_compat_tests
   --check-loaded` decodes it to the same society and accounts and re-encodes the same checkpoint. Skips without
   the settings.
6. **The welcome's cap (D23): not built** (Phase 2, in the Game, with an exploit_tests case). See the check-in below.
7. **`staged` is test-only:** see the check-in below.
8. **The class pass at the day's turn: measured and made one pass.** On the fixtures, `householdClass` one household
   at a time over every household took 1.8-3.3 ms on DEV (559 residents, 509 posts) and 13-24 ms on dev-old (1,083,
   1,083): that would have nearly filled the 23-25 window's 25 ms worst on its own. `stagedHouseholds` now calls
   `Society::householdClasses`, one `ClassPass` for the pass: each (wolf, depth) class once, each work room's shops,
   each home's residents and each wolf's written kin indexed once (same records, same order). Now 0.9 ms on DEV and
   2.6 ms on dev-old (worst of 20). The same classes: checked on every household of dev, dev-old and package
   (save_compat) and of the docks (people_flow `theStagedEconomy`). Rule off: `classOf`, `postClass`, `postKind` and
   `householdClass` without a pass are the old code, unchanged (the pass is a defaulted last argument, a local, never
   a member, so no thread shares one). `countFarmPosts` at the same turn is still unmeasured.
9. **`postRules()`' comment: fixed** to name people_flow_tests' `postKindsWithoutTheLoop`. **`wages` in
   city_loop.json is now dead for Core:** `payOf`, `postClass` and `payWages` read the C++ copy, so editing the
   file's wages changes neither pay nor class (a small change to rule-off behaviour, guarded only by that test
   failing when they drift). Delete the test when Phase 3 deletes the key.
10. **`stagedHour_`/`stagedDay_` aren't saved**, and the first hour after a start or restore isn't run, so a restart
    across 0:00 skips that night's purse and treasury set. Phase 2: settle out-of-range staged accounts at restore
    (9.2).

## Phase 0 check-in: `staged` is test-only

`staged` (Rules[], default 0; `RATW_STAGED_ECONOMY=1` forces it) must not be set in any served world's
`live.economy.rules` (DEV, dev-old, PROD) until Phase 1's morning restock and Phase 2's on-demand top-up, welcome cap
and settle-at-restore land. With it on today: the welcome is minted for every new character with no cap (character
churn is a 20p + 2 herbs + 3 meals faucet); the nightly treasury ceiling (2,000p) shrinks rewards paid from treasury
cash (festival purses, RatwFestivals.cpp:216); prices stop following stock; the store room and `/store` find nothing;
a restart across 0:00 skips the night's set. DEV's `live.economy.rules` is `{}` (read only, 2026-11-08).

Every test stops at its first failed check. To list every failing check, I built "soft" copies of the 17 failing
tests: their `expect` prints `SOFTFAIL` and carries on. They were built by hand in `build-p0/p0soft/` against the same
`libratw_core.a`. Counts below are failing checks from those soft runs (with the rule on). Where a soft run still
ended early, it ended on a `map::at` exception that followed a failed check (farms, selfcorrect, steer, wharf), so
later checks in that test were not reached.

## Every failure with the rule on (test, check, why)

The reasons fall into six groups:
- **W: the welcome.** The welcome is now made, not taken from the treasury.
- **L: the loop is off.** No household shopping, wages, alms, surplus, Country basket, patrons or drift.
- **F: farm money is off.** Farms get no till or crew account, nothing is grown into the farms' stores, and every area
  is on.
- **S: steering is off.**
- **H: the Wharf is off.**
- **C: the nightly set.** It changes the money supply. `conserved()` still holds, because the set is counted as made
  and sunk, but `moneySupply() == before` does not.

None of these points to a bug in the staged code. Each is a test of a mechanism that Phase 0 switches off, and Phase 3
deletes or rewrites it (doc 79, 17 and 16).

| Test (first failure) | Failing checks (soft) | Why | Fate in doc 79 |
|---|---|---|---|
| society_behaviors: society_tests.cpp:58 "Welcome money and herbs come from the finite treasury" | 2 (:58, :107 "Many character creations exhaust the treasury's grants") | W | Rewrite: welcome made, cap per account holder per week (9.2, D23) |
| appearance_behaviors: appearance_tests.cpp:211 "Authenticated wolf IDs receive exactly one finite welcome purse" | 2 (:211, :217 re-entry doesn't repeat it, judged by the treasury's cash) | W | Rewrite the two checks against the faucet tally |
| ticket_behaviors: ticket_tests.cpp:541 "with the welcome from the treasury, and the money conserved" | 1 | W | Rewrite as above |
| people_flow_behaviors: people_flow_tests.cpp:291 "his 30p from the Country's account" | 2 (:291, :292 "the money conserved: nothing made") | H, C: an arrival's purse is set, not moved from `outside:wharf` | Rewrite postsFromTheCountry for band purses (6.5); the new staged tests in the same file pass |
| game_behaviors: game_tests.cpp:1787 "/store brann iron_bar 2 draws them: The farms have no iron_bar in store now" | 1 | F: the store room draws from the farms' stores, and staged farms are never founded or grown | The store room goes (17); `/store` becomes a raw add into the till (RatwGameDev, 17) |
| store_room_behaviors: store_room_tests.cpp:96 "the smith draws three iron bars" | 20 (:96-98, :107-108, :112, :121-122, :147, :152, :160, :184, :187, :239, :288, :364-365, :393-394, :401) | F, same: "The farms have no ... in store now" | store_room_tests deleted (17); Phase 1's morning restock replaces the draw |
| farms_behaviors: farms_tests.cpp:1691 "eggs: the meat area's, in the farms' stores: 0" | 10 (:1691-1692 eggs, sheep, sheepskin; :1154 copper and tin ore; :1721; :1751; :1736; :1782; :1695), then map::at | F | farms_tests deleted; its staffing cases move to life_status_tests (17) |
| farm_people_behaviors: farm_people_tests.cpp:291 "grain_farm_1 founded, all its crew virtual" | 183: 42 × :291 "<farm> founded" (no till or crew account); 132 × :507 "<post> paid 0" (no farm wages); 7 × :514 "<community>'s store sold to its own households" (no household shopping); :471 "only the market shops' floats made" (C); :721 "only the grain farms' people ...: 49" (all areas on, so more farms staffed) | F, L, C | Placement checks are kept. **The placements themselves pass** (posts are taken, works posts refill: see below). Pay, founding and area schedule checks go |
| wharf_behaviors: wharf_tests.cpp:283 "a day of the loop: money conserved" | 17 (:283 C; :285 food bought, :286 wages, :287 restock and toll, :313-314, :318, :321, :327, :329, :360, :369, 4 × estate fifths, :441), then map::at | L, H, C | wharf_tests deleted (17) |
| season_behaviors: season_tests.cpp:221 "a day of the loop in each season: money conserved" | 4 (:221 C, :222 bread sold, :224 season's price, :226 more bread eaten in season) | L, C | season_tests deleted; `theFactorsByDay` rewritten into market_tests (17) |
| econ_fix_behaviors: econ_fix_tests.cpp "fair: every member a day's food first (0, 0, 0)" | 21 (fair food, the old order, fish over roasts, alms ×5 with counts, low wages, wage support ×3, groom and trim, services' city share, comforts, conserved ×5) | L, C | econ_fix_tests deleted (17) |
| trades_behaviors: trades_tests.cpp "the Watch: four kinds of mending ...: 0, 0" | 21 (patrons and the Watch's kit, retainers :353, medicine bought for households :407-408 ×6, bindings :436, buy-ahead :459, wage float days :479-481, carriage draws :493-500, conserved ×2) | L | trades_tests deleted (17) |
| workshops_behaviors: workshops_tests.cpp:323 "the keeper's and the hand's wages from the till: 0, 0" | 24 (workshop wages and House shares :323-372, the ironworks :701-752, the still, cheese as a luxury ×8) | L | workshops_tests deleted (17); `classOf("lady") == "upper"` (:559) passes |
| treasury_behaviors: treasury_tests.cpp "ann has the day's alms only: 0" | 22 (alms, surplus, works orders, ceiling, conserved) | L, C | treasury_tests deleted (17) |
| steer_behaviors: steer_tests.cpp:195 "on, at the centre: the push 0" | 18 (:195; :281 ×2 purses at alms only; Never Broke credit and skim :300-310; tills guard mean; :547 ×3 Never Hungry "food up to a day's and no more", which fails because staged food gives up to two days; :549; :551), then map::at | S, L, C | steer_tests deleted (17); Never Hungry and Never Broke return as band clamps (Phase 2) |
| selfcorrect_behaviors: selfcorrect_tests.cpp:214 "day 1: sold out, up at most a hundredth: 1.000000 to 1.000000" | 1, then map::at | L: no `followStock`, so prices don't follow stock | selfcorrect_tests deleted (17) |
| sim_fast_behaviors: sim_fast_tests.cpp:308 "an area more each day: 10" | 11 (:308 ×3 farms_on schedule, every area now on; :309 ×3 "the founding minted"; :369 ×3 world_check's class and farm rows; :385-386 the steering lines) | F, S | world_check's economy report goes; the sim-speed checks (1-269) are kept (16.12) and pass |

What passes with the rule on, and matters for Phase 3:
- **Succession by class, with the loop off, from the city and from the Country.** These are the new
  `stagedPostsStillFill` and `theStagedEconomy` in people_flow_tests, plus careers, roads, crime and the other 66
  tests.
- **Farm posts still fill, and works posts refill after a death.** I traced farm_people's `aWorksPostFilledAgain`
  with the rule on: the Rainwood post refilled from the city at day 8, and clay_fens from the Country (an arrival) at
  day 11. The only failures were stale `heldHands` counts, because tendFarms no longer runs. I fixed that by
  recounting at the staged day's turn.
- Talk, ambient, roads, letters, projects, festivals, journal, checkpoint, wire, careers and the server end to end all
  pass with the welcome, purses and treasury staged.

Open items found on the way:
- **The rule's name in tools.** Fixed in the repair round (item 1).
- **The store room with staged farms.** `restockFromStore` (behind `storeRule`) and `/store` find nothing, because
  staged farms never grow into stores. `storeRule` is off by default in code, but any world whose economy line turns
  it on (DEV's may) loses restocking when `staged` is on, until Phase 1's morning restock.
- **Cost at the day's turn.** Measured on the fixtures and made one pass (repair item 8); the 23-25 window itself
  still needs the user's go-ahead.
- **The welcome's cap.** The once-per-account-holder-per-week cap (D23) is not built. Society doesn't know the account
  holder; it belongs in the Game, in Phase 2. Until then `staged` is test-only (the check-in above).

## Kept readers of city_loop.json (outside code that doc 79 deletes)

`items::cityLoop()` is loaded by `RatwItems.cpp:705-923` (keys: `markets`, `wages`, `estates`, `food`, `comforts`,
`luxuries`, `rules`, `services`, `suppliers`, `steering`, `houseBusinesses`, `patrons`, `works`, `wharf`, `shelves`,
`seasons`). Readers in RatwCityLoop/Steering/Wharf/Farms that go with those files are not listed.

| file:line | What it reads | Fate in doc 79 |
|---|---|---|
| Core/RatwCareers.cpp `postRules()` (new) | **None any more.** `wages` was read by `postClass` (old RatwCareers.cpp:446) and `payOf` (RatwCityLoop.cpp:139, 154, 1784); all three now read the C++ copy. people_flow_tests `postKindsWithoutTheLoop` checks the copy against the file; the file's `wages` is dead for Core | **Done in Phase 0** (6.3). The pay amounts go to `status.json` pay words (17); delete that check with the key (Phase 3) |
| Core/RatwCityLoop.cpp:517 `buildSeasons` | `seasons` → `seasons_`. These feed `priceFactor`, which `quote`→`shopFactor` (RatwSociety.cpp:461) reads on every player price | Moves to `Data/Economy/market.json` and `Core/RatwMarket.cpp` (Phase 1/3) |
| Core/RatwCityLoop.cpp:535, 577-578 `buildSeasons`/`ownRule` | `rules`: every rule's default, `staged` included, through `rule()` everywhere | Most rules go with `Rules[]` (17); the kept ones (`staged`, the clamps, the Board's) need a home: `status.json`/`market.json` defaults plus `live.economy.rules` |
| Core/RatwCityLoop.cpp:542 `buildSeasons` | `services` → `services_` | Kept: the 22-entry list moves to `market.json` (6.5) |
| Core/RatwSociety.cpp:1708 `startingStock` (via `shelfOf`) | `shelves` | Moves to `market.json` with `startingStock` (17) |
| Core/RatwResidents.cpp:87 `resetAuthored` | `markets`: opens an `outside:<market>` account per market | Dead prefix: goes (9.1) |
| Core/RatwSociety.cpp:1112, 1119 `restore` | `markets`: the old save moves `fund:store` → `outside:` | Goes with the restore gates (14) |
| Core/RatwCityLoop.cpp:90 `houseOfPost`; Core/RatwSteering.cpp:1055 `tillHouse`, :1079, :1111 | `houseBusinesses` (`houseTills`) | `houseBusinesses` moves to `market.json` for bands and talk (17). Today only loop and steering code reads it |
| Core/RatwCityLoop.cpp:160-275 `estatesOn`/`estateOf*` | `estates` | Read only from loop and steering pay paths today. Doc 79 keeps `estates` for the guarantees (6.3: "the Heights and estate residents ... at least comfortable") → `market.json` |
| Core/RatwCityLoop.cpp:2558 `luxury()`, :1509 (`housesLuxuries`) | `luxuries` | Kept for which goods the rich buy and `times` → `market.json` (17); today only loop readers |
| Core/RatwCityLoop.cpp:697, 703 `comfortsAbove`/`comfortsShare` | `comforts` | Only the loop and steering read them: goes |
| Core/RatwCityLoop.cpp:112 `marketFor`; RatwSociety.cpp:1517 (store room) | `markets.front()` | Goes with the store room (17) |
| Tests: season_tests.cpp:79, :281, :291; farms_tests.cpp:749, :2008, :2045; steer_tests.cpp ×8; trades_tests.cpp:341, :349; treasury_tests.cpp:111; wharf_tests.cpp:689, :724; workshops_tests.cpp:524, :574, :700, :793; world_check.cpp:488, :1214, :1225, :1488, :2089; people_flow_tests.cpp (new, `wages`) | various | All in tests that are deleted, except people_flow (delete its check when the file goes) and world_check's money report (goes) |
| tools: studio.py:418, :560, :690-942 (seasons, services, rules, comforts, steering); map_editor.py `city_loop_file`/`rule_defaults`; world_store.py:53-55; dungeon_master.py:980 (`wharfActionMost`); city_loop.py:46; econ_report.py:880; steer_calibrate.py; steer_floors.py; test_studio.py:1283, :1307; test_world_store.py:248-538 | as named | Seasons, services and rules are retargeted at `market.json`/`status.json` in Phase 9. Wharf, steering and the econ tools go (17) |

`patrons`, `markets`, `suppliers`, `works`, `food` and `steering` have **no kept reader**. Doc 79 can delete them with
the loop, as 17 expects. The only exception is `markets.front()` in restore and resetAuthored, and that code goes too.

## Kept readers of `house:`

| file:line | What it reads | Fate |
|---|---|---|
| Core/RatwCareers.cpp:153 `facilityAccount` | `house:` as a valid account prefix | Kept in the table; `classify` must sort it (9.1). **No Core code opens a `house:` account today**; only an old save can hold one |
| Core/RatwSociety.cpp:1289 `houses()`; Core/RatwSociety.h:1424 | `house:<slug>` as a **House's id** (people, not an account) | Kept: talk and the House guarantee read `houses()` |
| Core/RatwGameTalk.cpp:392 | figure known in `house:<the speaker's house>` (talk scope, not an account) | Kept |
| Core/RatwGameTalk.cpp:145, :236, :756 | `society.houses()` (wealth override, facts, figures) | Kept; :145-162 becomes guarantee 3 (6.3) |
| Tests/world_check.cpp:563 | `house:` accounts in the money report | Goes with `moneyReport` (17) |
| tools/dungeon_master.py:1013; tools/test_dungeon_master.py:1105, :1117 | `house:` accounts in the DM's money view | Rewritten for the Prosperity tab (17) |
| tools/econ_model/* | its own model | Already dead; goes |

Verdict for 9.1's "`house:` staged if a kept call site reads it": **no kept call site reads a `house:` account.** It can
go on the dead list (dropped at restore) unless the DM's Prosperity tab wants old saves' House purses.

## Kept readers of `towns_` (all kept: law, roads, newcomers, letters)

| file:line | What it reads | Fate |
|---|---|---|
| Core/RatwCrime.cpp:80-89 `lawTown` (and `townOf` :83) | the law town of a cell | Kept on `towns_` (law stays: 2, 5.1, Phase 4) |
| `lawTown` callers: RatwCrime.cpp ×9, RatwFestivals.cpp ×7, RatwGameFame.cpp ×10, RatwSchedules.cpp ×8, RatwArchive.cpp ×3, RatwStalls.cpp ×2, RatwGame.cpp, RatwGameLetters.cpp, RatwGameNewcomers.cpp, RatwGameTables.cpp ×1 each | through `lawTown` | Kept |
| `World::townOf`: RatwAmbient.cpp:91, :486; RatwGameNewcomers.cpp:96, :393, :461-462, :841, :869, :911, :939; RatwGameLetters.cpp:85; RatwHunt.cpp:138, :901, :1048; RatwTogether.cpp:654 | is this cell in a town / which town | Kept |
| Core/RatwGameProjects.cpp:677 `proposeProjects` | `world_.towns()`, one a day | Kept (towns for projects); with no towns in Ridgemere 2.0 it proposes none today |
| Core/RatwGameLetters.cpp:95 `townWords`, :129 `courierCells` | town market names, routes between markets | Kept; the courier legs move to markets in Phase 4 |
| Core/RatwGameNewcomers.cpp:39 `startTowns` | the starting towns this world has | Kept |
| Core/RatwRoads.cpp:56 `town`, :66-94 (spawn by town, `townOf`), :112-258 `setupTowns`, :453 `tendRoads`' two-towns gate, :1071-1086 bounties and the watch (`recoverHoard` into `town->store`), :1453 robbery rumours | the road layer | The road layer's uses move to markets (Phase 4); `setupTowns` keeps the law towns; `t.store` (`stores:`) stays as the staged town treasury (D24) |
| Tests: newcomer_tests.cpp:405, roads_tests.cpp:176, :211-212, :854, :894; world_check.cpp:2446-2448 | `towns()` | Kept; roads_tests' `townsAndCaravans` grows into `trade_show_tests` (16.5) |
