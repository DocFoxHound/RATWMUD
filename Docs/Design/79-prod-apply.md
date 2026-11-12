# Doc 79 on PROD: the apply plan

For the user, when the user chooses (D19 (a): PROD gets the staged economy only together with the new map). Nothing
here has been done to PROD: it was only read (2026-11-12) and copied. Written in doc 79's Phase 11; the plan's
section 14 ("Order") is the reason for each step's place.

## Where PROD stands (read only, 2026-11-12)

- Database `ratw_prod`, migrations through **0054**; world `upper_accord` ("Ridgemere") on **the original map**; three
  builds and three releases (release 3 the last); 1,083 rows in `live.npcs`; one `live.economy` row.
- **Closed for maintenance** since 2026-10-31 (`admin.server_state`: "Shut down by the user ... work moved to
  Ridgemere 2.0 on DEV"). The live server (`tools/live.sh server`) plays DEV, served as live (`world_db.py list`: dev
  `[live]`).
- **No players' data at all:** `game.checkpoints`, `game.characters`, `game.accounts`, `game.journal`, `game.events` and
  the rest of `game.*` are empty. So today no player purse, escrow or goods can be lost; the section "If PROD has players
  by then" is for a PROD that opens again on the old map before the move.

## What the move is

Three things, together, in this order, never one without the others:

1. **The code:** `ridgemere2` at doc 79's Phase 11 commit or later (server, tools, studio, client).
2. **Migrations 0055-0061** (schema only). 0055-0060 add the old loop's `live.economy` columns and tables; 0061 drops
   `live.business_settings`, redefines `live.people_manifest()`, adds `live.npcs.band_override` and
   `dm.queue_economy_sync`. No row is rewritten; the dead columns stay (D20). Tools from before doc 79 that write
   `live.business_settings` stop working after 0061, which is why the code goes with it.
3. **The map:** DEV's Ridgemere 2.0 imported into PROD, replacing PROD's map (`world_sync.py import dev --replace-map`,
   doc 66, "Import of a different map"). The import is refused while PROD lacks a migration DEV has: step 2 first.

**Hazard:** `python3 tools/world_db.py migrate` with no `--db` migrates *every* world, PROD included. Until the move,
always pass `--db dev` (or the world meant).

## The steps

1. **Choose the time.** PROD stays in maintenance throughout (it is now). If PROD was opened since, close it:
   `python3 tools/world_sync.py maintenance prod on --note "Ridgemere 2.0 and the new economy"`, and wait for its
   server to exit (status 76).
2. **Back up both worlds** (the import writes DEV too: it brings DEV level with PROD afterwards):

       pg_dump -Fc -d ratw_prod       > Saved/Backups/ratw_prod-before-79.dump
       pg_dump -Fc -d ratw_ridgemere2 > Saved/Backups/ratw_ridgemere2-before-prod-79.dump
       pg_restore -l Saved/Backups/ratw_prod-before-79.dump | head    # (it lists: the dump is whole)

   (Host, port and user as in `Database/.env`: `-h 127.0.0.1 -p 5433 -U postgres`.)
3. **The code:** check out `ridgemere2` at or after this commit in the main checkout and build it fresh, Release, in
   `build-core` (the directory the launchers use: `tools/game.sh`'s `ratw_server_binary`), then the full ctest once with
   the database (as Progress entries do):

       rm -rf build-core && cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release && cmake --build build-core -j"$(nproc)"

4. **Rehearse on copies,** with that build's server (writes only throwaway `ratw_test_s2stage_copy_*` databases, dropped
   after):

       set -a; . Database/.env; set +a
       RATW_MIGRATE_COPIES=ratw_prod,ratw_ridgemere2 RATW_TEST_SERVER=build-core/ratw_server RATW_TEST_DB_TAG=s2stage_ \
         python3 tools/test_migration_0061.py

   It copies PROD with `pg_dump`, applies 0055-0061 to the copy one at a time, checks every `live.economy` row byte
   for byte, reads the manifest for each world and starts a scratch server of the new code on the copy. It must pass.
   (Phase 11's run: Progress.) Run after step 3, so the server it starts is the code PROD will run.
5. **Migrate PROD alone:** `python3 tools/world_db.py migrate --db prod` (0055 to 0061, in order). Check:
   `SELECT max(version) FROM public.schema_migrations` is `0061`; `live.business_settings` is gone; the `live.economy`
   row is the dump's (`SELECT row_to_json(e) FROM live.economy e` before and after: the same but the new columns'
   defaults).
6. **PROD's own economy settings.** The import never brings `live.economy`'s kept settings across (the live game's: "PROD
   keeps its own"), and PROD's differ from DEV's: `store_herbs` 1500, `store_meals` 900, `treasury` 20000 against DEV's
   100, 50 and 1000 (`plan-import`'s warning, read 2026-11-12). They are the treasury's start on a world's first boot (PROD
   has no checkpoint, so its first boot is one): 20,000p of staged coin, clamped the first night to `status.json`'s
   treasury ceiling (2,000p), and 1,500 herbs and 900 meals in the treasury's own stock (`Society::resetAuthored`), which
   nothing in the staged economy draws on that this repair's reading found, and the nightly trim (residents' wallets
   only) leaves. Nothing a player could lose either way, but PROD would start a little apart from the world the soak
   measured. **Recommended:** set them to DEV's before the first boot, in the DM's economy settings for PROD or, after the
   step 2 backup, `UPDATE live.economy SET treasury = 1000, store_herbs = 100, store_meals = 50` on `ratw_prod` (the
   user's decision; the rest of the row stays PROD's).
7. **The map:** `python3 tools/world_sync.py plan-import dev --json > /tmp/plan.json` and read it: `replacesMap`, the
   two maps, `letGo` (the old map's NPC areas, claims and patrol posts that go), the conflicts (a field of the live
   game's is live's by default) and the NPCs whose home, work or evening place the new map hasn't (each needs a choice:
   unplace, dev's, or a place on the new map). Write the choices file, then
   `python3 tools/world_sync.py import dev --replace-map --choices choices.json --note "Ridgemere 2.0 and the staged economy (doc 79)"`
   (the publish password; or the DM's Import from dev with "Replace PROD's map" ticked). It records a release with its
   build and its `map` and `replacedMap`.

   **Its scale today** (`plan-import dev`, read only, 2026-11-12, before step 5 so still "blocked" by the migrations):
   3,382 changes from DEV and 19 from PROD; **1,068 conflicts, every one an NPC** (`live.npcs`), because there is **no
   sync base** between the two (the base reads "none (live as it is now; where the live game's fields differ, not
   known)": PROD and DEV were never synced on one map, so every NPC whose fields differ is a conflict). Of them:
   - **750 resolve by their defaults** (each field the side that owns it: an authored field such as a description is
     dev's, a field of the live game's such as an evening place or a purse is live's); an import with no choices for
     them takes those;
   - **318 NPCs are let go** (`letGo.npcs`): PROD's NPCs whose **work** place (each of the 318: `gone`, for instance
     `w_folds_0_3`) is on the old map only; DEV has every one of them. Each **needs a choice** (no default):
     `"unplace"` (the row by its defaults, its work unset: kept, with names, talk, relations and memories, until the
     census or the DM gives it a post), `"dev"` (DEV's whole row, the live game's fields too: not wanted here), or a
     field-by-field `{"work": "dev"}` (the row by its defaults, its work DEV's place: 133 of the 318 have one on the new
     map, the other 185 none in DEV, so theirs is unset as by `"unplace"`), or `{"work": {"cell": ..., "x": ..., "y": ...}}`
     for a place chosen by hand. The import refuses until all 318 have one, and refuses choices it can't use (naming
     them) without changing anything. Writing 318 by hand is not sensible: make the file from the plan
     (`{"work": "dev"}` for each conflict whose `gone` is non-empty) and read it before the import;
   - also let go: one territory claim (`letGo.claims`); `letGo.names` lists the first 30 of the 318 by name.
   After the import DEV is brought level with PROD and takes it as its base, so the next sync is three-way again.
8. **Serve PROD.** `serve-world` (`world_sync.switch_live`) is refused while the current live world (DEV today, "running")
   is not in maintenance, and it puts the world it switches to into maintenance itself; so PROD is opened only after the
   switch (checked by reading `tools/world_sync.py` and `tools/world_db.py`, and run on two throwaway worlds in a scratch
   registry, 2026-11-12: the order below works; opening PROD first is refused while DEV runs, and undone by the switch
   when DEV is closed). In this order:

       python3 tools/world_sync.py maintenance dev on --note "Moving the live server to PROD (doc 79)"
       python3 tools/world_db.py serve-world prod --yes
       python3 tools/world_sync.py maintenance prod off

   The first drops anyone playing DEV (they are told; DEV's server exits with status 76 within a minute). A launcher
   already waiting (`bash tools/live.sh server`, which waits while its world is in maintenance) follows the switch and
   starts PROD's server when PROD opens; with none waiting, start it: `bash tools/live.sh server`. Its first boot logs
   `RATW_ECONOMY economy staged (doc 79)` once. DEV stays in maintenance: it is no longer served, so that only matters
   if it is served again (open it then); Atlas and the builds work on it as before.

## What happens to players' purses, escrow and goods

Today: nothing, because PROD has none. If PROD has players by then (doc 79, 14 and 16 item 7; `save_compat_tests`,
the PROD-shaped fixture from `dev-old`):

- **Player purses and escrow survive to the penny.** Real and escrow accounts are never touched by the reconcile.
- The old loop's accounts (`loop`, `steer`, `fund:relief`, `outside:wharf`, `farm:`, `house:` and the other dead
  prefixes) are dropped with their goods; residents' and shops' staged accounts are set to their bands' targets; a
  departed resident's staged account goes (not paid into a treasury); a newcomer's purse is its band's target (not
  minted).
- Goods the new catalogue doesn't know, in a player's or an escrow account, go to the account's quarantine ("goods the
  world no longer knows") and come back if the item returns; nothing is made or lost.
- A caravan out when the old save was taken loses the old economy's account and comes home empty; the letters it
  carried and its escorts' contracts open again, their pay still in escrow. A `bandits:` hoard holding players' coins
  is kept with them (escrow), as are town treasuries (`stores:`), tills and contracts' escrow.
- A character standing in a place the new map hasn't wakes at its spawn (`World::restore`, doc 60). Not handled by the
  game (doc 66): players' camps and lodgings (`game.camp_sites`, `game.lodgings`) name old cells. With players, list
  them first (`SELECT * FROM game.camp_sites; SELECT * FROM game.lodgings`) and refund or clear them by hand before
  step 7.
- To check afterwards: each player's `cash` in the first checkpoint after the move equals the dump's (the save's
  `society.accounts` for the player ids, both sides).

## The old map and Ridgemere 2.0

After the import PROD's map is Ridgemere 2.0, DEV and PROD share one map identity and Atlas's Update from live works
again. The old map stays in two places: `dev-old` (`ratw_dev`, the archive, never written by this) and the dump from
step 2. PROD's NPCs are kept (nothing live has is removed); those whose places are gone are unplaced or moved by the
choices of step 7, keeping their names, talk, relations and memories.

## The world package

A host's package (doc 77) is made from a world as it is: after the move, packages are made again from PROD
(`python3 tools/world_package.py export --world prod --out DIR`, then `scan DIR --source prod`). A package made before
doc 79 still loads on this code (`save_compat_tests`: "players poured into the treasury"), but it carries the old map;
hosts should take the new one.

## What to check after

- The server's log: the world loaded, `economy staged (doc 79)` once, no `RATW_ECONOMY skipped`, "listening on port".
- The DM's Prosperity tab: eight markets, the slider at 0, wagons posted two days ahead on both ends' boards, ships due
  at the port, the farms' carts.
- An export of PROD's new build (`python3 tools/world_build.py export DIR --world prod`, read only):
  `world_check DIR --simulate 6 8 --players 20 --no-check` inside section 15's bars, and a three-day
  `--players 0 --coarse-ticks --stage FILE` run whose lines say real money balanced (`tools/stage_report.py FILE`).
- `dm.actions`: none failed. `live.economy`: the row as before.
- With players: their purses as in the dump (above).

## Rolling back

- **Before step 7, after step 5:** `Database/rollback/0061_staged_economy_down.sql` takes 0061 back (its header says
  what an older server reads after it); 0055-0060 have no way back but the dump.
- **After step 7, or once the new code has run on PROD:** stop the server, close PROD for maintenance, restore the dump
  into a fresh database and point the registry at it, or over PROD itself:

      pg_restore --clean --if-exists -d ratw_prod Saved/Backups/ratw_prod-before-79.dump

  and run PROD's old build (release 3's). Nothing is lost but play since the dump (none while PROD is closed). Restore
  DEV's dump too if the import's levelling of DEV is to be undone.
