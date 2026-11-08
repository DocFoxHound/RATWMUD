# Saves from before doc 79

Frozen copies of saves made before the staged economy (Docs/Design/79-a-staged-economy.md, section 16 item 7;
Phase 0's first step, agreed by the user). `Tests/save_compat_tests.cpp` (ctest `save_compat`) loads each one with
the code of the day and checks that it loads, that players' purses and escrow come through to the penny (the totals
below), and that the state round-trips through wire. It passed on the code at `cab65ee` (`ridgemere2`), the baseline:
every later phase keeps it green. What a later phase may change is how staged accounts are settled (section 9.2),
never a player's coin or escrow.

**Never regenerate these.** They are copies of what the old code wrote. Each was taken once, read only (one
`REPEATABLE READ, READ ONLY` transaction, `default_transaction_read_only=on`), on 2026-11-08.

## The fixtures

| Dir | What | Source | Size |
|---|---|---|---|
| `dev/` | (a) DEV's latest checkpoint | `ratw_ridgemere2` (key `dev`, Ridgemere 2.0) | 302 KB |
| `dev-old/` | (b) dev-old's latest checkpoint, the PROD-shaped save (the old map) | `ratw_dev` (key `dev-old`) | 370 KB |
| `package/` | (c) a world package of DEV, made with `tools/world_package.py export --world dev` | DEV | 294 KB |
| `tail/` | (d) a save and the journal written after it, by the code at `cab65ee` | `save_compat_tests --make-tail` | 10 KB |
| `reset/` | (e) a doc 78 "Reset economy rules" copy of `live.economy`, holding only `steerLean` | DEV's `live.economy` row | 0.3 KB |
| `road/` | (f) a synthetic save of roads_tests' strip, and (g) its package snapshot | `save_compat_tests --make-road` and `world_package.strip`, both at `cab65ee` | 12 KB |

All are gzip; 989 KB in all. (a)-(e) are copies of real worlds' saves; (f) and (g) are made by the old code, because no
real world has what they hold (below).

**(a) `dev/`:** `checkpoint.json.gz` is `game.load_checkpoint('upper_accord')` with the journal after it replayed as
`tools/world_package.py` (`read_source`, `journal_replay`) does; DEV's `game.journal` was empty, so it is the
checkpoint itself: revision 1185, saved 2026-10-31 21:23:06 UTC. `world.ratw.gz` is the manifest of DEV's newest build
(build 70, made 2026-11-06 02:49:55 UTC, 1,212 cells). `cells.json.gz` holds each cell's header (`world.build_cells.header`)
with its `height:` lines left out (the heights are 262 MB). The cells' tiles (568 MB) are not kept: the test gives each
cell open ground of its header's size, with a door's tile (`+`, `^` for steps) under each of the manifest's doors, so
the manifest loads with its authored roster and `World::restore` checks the save against the right cells and sizes.

This checkpoint is a fresh boot's (game time 59 s, day 0, `budgetDay` 0): DEV was moved to Ridgemere 2.0 on 2026-10-31
and its server hasn't run since. It holds `minted` (52,946), `sunk` (0), 559 residents, 110 tills, 896 larders and
`outside:country` (20p), but **no `loop`, steering state, `fund:relief`, `outside:wharf` or `farm:` accounts**: the city
loop hadn't run an hour. Section 16 item 7 asks for those; a save holding them needs DEV's world run past its first
hours and night, which is a run for the user to allow (see "A richer DEV save" below).

**(b) `dev-old/`:** the same from `ratw_dev`: revision 5,363,264, saved 2026-10-06 20:57:21 UTC (its `journal` field
22, `game.journal` empty). Its build: build 34, made 2026-10-13 16:51:52 UTC, 689 cells (the save is older than the
build, so a restore reconciles it as the server does). Day 19 (`budgetDay` 19). It has 13 `stores:` town treasuries
(all 0p), 9 caravans in transit (`caravan:` accounts, 0p), 201 road contracts and 86 `contract:` escrow accounts
(1,771p, NPC- and treasury-posted), 23 bandit camps, 41 `town:` accounts, the treasury (165,884p) and one player,
`player-tester` (20p, 1 meal, 2 herbs). It has **no `bandits:` hoards and no letters or escorts taken by players**:
dev-old never had them. PROD (old map, migration 54) wasn't read.

**(c) `package/`:** `python3 tools/world_package.py export --world dev --out DIR` at `cab65ee`: package
`dev-b70-202611081506` (id `78b797c72faf4ead80eb9de46b4aa362`), made 2026-11-08 15:06:22 UTC from build 70. Kept:
`package.json`, `snapshot.json.gz` (the save, stripped of players: DEV had none, so "Strip: removed nothing"),
`world.json.gz`, `live.json.gz` and `SHA256SUMS` (the kept files' sums still match it). Left out: `build.tar.gz`
(45 MB; the same build 70 as `dev/`, whose manifest and headers the test uses).

**(d) `tail/`:** made by `save_compat_tests --make-tail DIR`, built from `cab65ee`: today's game server (`game::Game`,
the file store, as `ratw_server --save` runs) on town_fixture.h's three-town strip, its files frozen in
`world.json.gz`. Two players (`player-ash`, `player-bo`) enter; the server saves (`save.json.gz`, journal field 2);
then Ash buys 2 meals at the inn and sells 1 back, writes Bo a letter with 5p in it (`letter:doc-1@player-ash`), and Bo
gives Ash 3p, which she accepts. `save.json.journal.gz` holds the 4 records those wrote (seqs 3-6), after the save;
each sets `society.minted`, `sunk`, `nextEntry`, `budgetDay` and the rest, as pre-79 records do. Loading replays them
over the save. A world from files keeps no letters across a restart (they live in `game.documents`), so a server
loading it hands the letter's 5p back to Ash (`Game::orphanedEnclosures`); the test counts that as the same pennies.

**(e) `reset/`:** `economy.json.gz` is DEV's `live.economy` row (from the package's `live.json.gz`) with `rules` set to
`{"steerLean": 60}`: what doc 78 section 12.6's "Reset economy rules" copy of `live.economy` would hold from before doc
79 if a host had moved only the slider. It is made by hand: doc 78's Reset isn't built, so no world has such a copy,
and DEV's own `rules` is `{}`. Today the test turns it into the line `live.people_manifest()` writes
(`rule "steerLean" 60`), loads DEV's world with it (steerLean 60) and restores DEV's save over it. After doc 79 it
opens at prosperity 0.

**(f) `road/` (synthetic):** what (a) and (b) lack and no real world holds: DEV's only save is a fresh boot, dev-old
never had bandits robbing players or players' letters and escorts, and PROD wasn't read (a read-only copy of PROD's
checkpoint is the user's call). Made once by `save_compat_tests --make-road DIR` built from `cab65ee` (Core at
`cab65ee`, the test file of this phase; 2026-11-08): roads_tests' strip (`EE.....WW`, the capital "east", wild
country, the town "west"; 9 open cells of 16x16, frozen in `world.json.gz` as {manifest, cells, seams}) with the
steering and both floors on (`rule "steer" 1`, `neverHungry`, `neverBroke`). Two players enter; the city's loop runs
past 18:00 (wages paid) and midnight; Bo (`player-bo`) posts a 4p letter for Wren in the west (`contract:k1`); a week
passes; Ada (`player-ada`) is stopped by camp_mid's bandits and pays them her 20p (`bandits:camp_mid`, 20p); she takes
the treasury's 12p escort (`contract:k2`) and a caravan (`caravan:cv3`, 6 meals) makes ready in the capital with her
as its escort and Bo's letter with its carters. `save.json.gz` is `checkpoint::encode` of `World::save()` then. It
holds `society.loop` (wages, level, toll) with `loop.steer` (the steering on), `outside:wharf`, four `estate:`
accounts, `stores:west`, `caravan:cv3`, two `till:` accounts, `minted` 20,560 and `budgetDay` 8. **Still in no
fixture:** `fund:relief` (opened only by a skim, which this short run never needed) and `farm:` accounts (the strip
has no farms), so Phase 3's dead-prefix drops for those two rest on hand-made states; and a DEV-shaped save with the
loop's state, which waits for "A richer DEV save" below.

**(g) `road/package-snapshot.json.gz` (synthetic):** the road's save through `world_package.strip()` at `cab65ee`
(`tools/world_package.py`, the strip `export` runs; `money()` and `scan()` find nothing): the two players and their
purses gone, Bo's 16p and his letter's 4p escrow poured into the treasury (19,948p to 19,968p), Ada's escort reopened
and the caravan's escorts emptied; not a penny lost (20,560p before and after). The strip leaves the caravan's
`letters` naming `k1`, the contract it removed (a pre-79 quirk Phase 3's reconcile meets: "its letters reopen or
refund"), and the bandits' 20p of Ada's coin in their hoard.

## Totals

Players' purses (`player-...`, `wolf-...`) and escrow (`contract:`, `letter:`, `let:`, `stall:`, `table:`, `fest:`,
`project:`, `ground:`, `bandits:`, and `chapter:`, a Chapter's own money), as loaded by the code at `cab65ee`:

| Fixture | Player accounts | Player cash | Player goods | Escrow accounts | Escrow cash | Escrow goods |
|---|---|---|---|---|---|---|
| dev | 0 | 0p | 0 | 0 | 0p | 0 |
| dev-old | 1 | 20p | 3 | 86 | 1,771p | 0 |
| package | 0 | 0p | 0 | 0 | 0p | 0 |
| tail (replayed) | 2 | 125p | 11 | 1 | 5p | 0 |
| road | 2 | 16p | 10 | 3 | 36p | 0 |

The tail's accounts: `player-ash` 58p (2 herbs, 4 meals), `player-bo` 67p (2 herbs, 3 meals), `letter:doc-1@player-ash`
5p. dev-old's: `player-tester` 20p (1 meal, 2 herbs). The road's: `player-ada` 0p, `player-bo` 16p (10 goods between them),
`bandits:camp_mid` 20p, `contract:k1` 4p (Bo's letter), `contract:k2` 12p (Ada's escort). Each account is also checked
one by one (cash and every good).

## What the test checks

- **dev, dev-old, package:** the checkpoint decodes; its society restores against the world's authored roster
  (`Society::restore`, as `World::restore` does it); every player and escrow account is the save's exactly and none
  appears; the totals are the table's; the restored society round-trips through `wire::society`/`readSociety` and the
  whole document through `checkpoint::encode`/`decode` (a fixed point); the whole save restores into the world
  (`World::restore`) on the open-ground cells with the same accounts.
- **package:** no player and no player's account in it; the manifest is format 1 with a snapshot.
- **tail:** the journal has records after the save, one sets the society's counters; replayed and decoded, the totals
  are the table's; a game server starts from the files (`Game::start`, the journal replayed, decoded and restored)
  and takes the save, holding the same player and escrow money to the penny; both characters are there.
- **reset:** the copy holds one rule, steerLean; DEV's world loads with it; DEV's save restores over it.
- **road:** as the real worlds' (decodes, restores against the strip's roster, accounts exact, round-trips, the whole
  save into the world), and: the bandits' hoard holds coins, the caravan has Ada as its escort and Bo's letter, both
  contracts are taken, the loop's and the steering's state are in the save and the steering restores on, and the
  accounts with the prefixes `outside:`, `estate:`, `stores:`, `caravan:`, `till:`, `bandits:` and `contract:` are there.
- **road's package:** no player and no player's account; the players' purses and removed escrow are in the treasury;
  the money the same to the penny; the snapshot's society restores and adds up.
- **dev, dev-old, package:** the staged day's turn's one pass over the classes (`Society::householdClasses`) gives each
  household the class `householdClass` gives it.
- **Through Postgres** (`Tests/save_compat_pg.py`, ctest `save_compat_pg`; doc 79 16.7's "through the world_store
  Postgres tests"): each loaded state (dev, dev-old, package, road; `save_compat_tests --write-loaded`) is saved with
  `game.save_checkpoint` into a throwaway database (`ratw_test_p0stage_<hex>`, migrated, dropped afterwards) and loaded
  back with `game.load_checkpoint`: the same JSON, and `--check-loaded` decodes it to the same society and accounts and
  re-encodes the same checkpoint. Skipped (passing) without the PostgreSQL settings (`set -a; . Database/.env; set +a`
  in the main checkout). It opens only the maintenance database `postgres` and its own.
- **With `RATW_PRE79_BUILDS=DIR`** (DIR/dev and DIR/dev-old holding the builds as `tools/world_build.py export DIR
  --world KEY` writes them, read only): a game server starts from each real save on its full world, every tile
  checked, takes it, and holds the table's totals on the save's clock. On 2026-11-08 at `cab65ee` all three (dev,
  dev-old, package) loaded whole this way.

## Running it

    cmake --build BUILD --target save_compat_tests && ctest --test-dir BUILD -R save_compat
    BUILD/save_compat_tests --totals        # prints each fixture's totals
    set -a; . Database/.env; set +a; ctest --test-dir BUILD -R save_compat_pg   # the round trip through Postgres

## A richer DEV save

Section 16 item 7 asks for a pre-79 DEV checkpoint holding `loop`, `steer`, `fund:relief`, `outside:wharf` and `farm:`
accounts. None exists: DEV's only save is the fresh boot above. One can be made without touching DEV by running the
code at `cab65ee` on DEV's world from files (`ratw_server --world-export` the exported build, `--save` a copy of
`dev/checkpoint.json.gz`, in `/tmp`) past an evening's pay and a night, then copying its save here. That is an economy
run, so it waits for the user's go-ahead.
