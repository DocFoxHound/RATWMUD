# 66. Worlds, Atlas and the DM: many worlds, NPCs without a map, one tool, safe pushes (2026-10-30)

The user, 2026-10-30:

- "We need to be able to switch servers easily, and we need the NPC'S to be unset to a business and home and
  essentially be able to exist in a state that isn't tied to a map."
- "Merge both Atlas and DM into one tool, but keep them as separate suites. Atlas is made for building and editing
  maps and all static components. It can also build and assign NPC's to buildings and jobs."
- "In some cases you may have 3 or 4 different worlds that are side projects, while 1 of them may be a static copy of
  a live server that you want to edit. ... we need to make sure there is some robust conflict management when the user
  decides to push the changes from the Dev world to the Prod world. That switch and conflict resolution should not be
  managed in Atlas but in DM. Atlas should have a way to pull the diff from the live server and choose which conflicts
  to retain."
- "If you wanted to add a few buildings to Ridgemere you would close the game server down for a little while for
  maintenance, tell Atlas to update the dev server from the prod server, manage conflicts, make your changes, and then
  save. Then in DM ... you would import the Dev world to Prod, resolve any remaining conflicts, and then turn the
  server back on."
- "The prod server should be able to switch worlds using DM. I would heavily suggest people don't switch worlds but it
  needs to be possible. ... Dev server can also switch between many worlds, but that is done in Atlas."
- "DM is the live game management tool without map editor tools, and Atlas is the dev management/development tool
  without storyline tools and mainly map/building/NPC/faction/etc tools."
- And: erase all the economy rules made so far; keep items, buildings and shop types.

Builds on doc 20 (the world database, Push to live), doc 21 and 34 (the Dungeon Master), doc 64 (the economy).

## Where it starts

- Two databases, `ratw_dev` and `ratw_prod`, one world each. "dev" and "prod" are written into some forty places:
  `tools/world_db.py`, the launchers (`live.sh`, `scratch.sh`, `mind.sh`), `world_build.py`, `publish.py`,
  `dungeon_master.py` (`TARGETS`), `map_editor.py`, `dm_service.py`, and the game server (`Game::loadFromDatabase`).
- 2026-10-30, by hand: a third database `ratw_ridgemere2`, "Ridgemere 2.0", copied from DEV with its map taken away
  (its 1,083 NPCs, their talk and relations, the 626 buildings kept; the old placements in
  `artifacts/ridgemere2/old-map-placements.json`). The tools take it as the working world (`world_db.WORKING`).
- An NPC's home, work and evening areas are required (`live.npcs`, NOT NULL), so 556 work and 128 evening areas
  point at a holding room, `r2_unplaced`, for now.
- Atlas (`tools/map_editor.py serve`, `Editor/index.html`) and the DM (`tools/dungeon_master.py serve`,
  `Editor/dm.html`; the Storykeeper `tools/dm_service.py`) are two hosts on two ports over one front-end codebase.
- Push to live (`tools/publish.py`) makes PROD's authored content equal DEV's, and live data add-only: an NPC that
  exists in PROD is never updated. No three-way comparison, no conflicts to choose.

## 1. Worlds: a registry, and switching

**The registry**, `Database/worlds.json` (in git), lists every world:

    {"worlds": [
      {"key": "prod",       "name": "Ridgemere (live)", "database": "ratw_prod",       "kind": "live"},
      {"key": "dev",        "name": "Ridgemere (dev)",  "database": "ratw_dev",        "kind": "dev", "upstream": "prod"},
      {"key": "ridgemere2", "name": "Ridgemere 2.0",    "database": "ratw_ridgemere2", "kind": "dev"}
    ]}

- `kind`: **dev** worlds are edited in Atlas and run on local and scratch servers; **live** worlds are what players
  join, run and managed from the DM.
- `upstream` (dev worlds only): the live world it is a working copy of. Only a dev world with an upstream can pull
  from live or be imported into it. Side projects have none.
- Each database holds one world, as now. Schemas never drift: `world_db.py migrate` covers every registered world.

**Local choices**, `Saved/Config/worlds.local.json` (not in git): `{"atlas": "ridgemere2", "live": "prod"}`.

- `atlas`: the dev world Atlas, the world builder, local and scratch servers and the talk/voice tools use by default.
  Switched in Atlas (or `world_db.py use KEY`).
- `live`: the world the live game server runs. Switched in the DM only, only during maintenance, with a warning.

**Commands** (`tools/world_db.py`):

- `list`: the worlds, their kind, upstream, database, which is Atlas's and which is live.
- `use KEY`: Atlas's world.
- `new KEY --name NAME [--kind dev|live] [--upstream KEY] [--from KEY [--without-map]]`: a new database, empty with
  every migration or copied from another world (`--without-map` does what was done by hand for Ridgemere 2.0:
  NPCs, talk and buildings kept, their map places left unset), registered, roles granted.
- `serve-world KEY` (what the DM calls): the live server's world.

Every launcher and tool takes a world key and defaults to the local choice; the game server takes any registered key
(the launcher checks it against the registry; the server no longer hard-codes prod and dev).

## 2. NPCs without a map

`live.npcs.home_area`, `work_area` and `evening_area` (and `live.profession_slots`' three) become optional
(migration 0052, on every world).

| An NPC with | Is | In the world |
|---|---|---|
| home, work | placed | as now |
| home, no work | placed, without work | spawns at home; idle (no job, no wage) |
| no home | **unplaced** | no body: it exists (name, talk, relations, memories, roster entry, job if it has one) but stands nowhere, isn't simulated on the map and can't be met |

- Unplaced is a state, not a place: no holding room. Ridgemere 2.0's `r2_unplaced` goes and its NPCs' areas are
  unset.
- The build/export (`world_store`, `map_editor` exporter, `world_build.py`) writes unplaced NPCs into the roster
  without a body; the game server loads them as off-map residents and spawns only the placed. Placing an NPC later
  (Atlas, a push) spawns it at its home.
- Atlas lists unplaced and jobless NPCs and assigns a home, a business (a work area and post) and an evening place;
  the DM views tolerate NPCs with none.
- The simulation counts an unplaced NPC as a resident who is away: no needs, no work, no money moving.

## 3. One tool, two suites

One host, `tools/studio.py serve` (`bash tools/studio.sh`), on one port, serves one app with two suites and a switch
between them. One sign-in (the DM accounts, `dm.admins`, kept in PROD as now).

| | Atlas | DM |
|---|---|---|
| For | building worlds | running a live world |
| Works on | dev worlds (its world switcher) | live worlds |
| Has | map and terrain, cells, buildings and interiors, doors, places, resources, patrol routes and NPC areas, NPCs and their homes, jobs and businesses, professions and the roster, factions and their claims, talk and voice tables, playtests, **Update from live** (with conflicts) | the server (start, stop, maintenance, the live world switch), **Import from dev** (with conflicts), releases and rollback, live NPC life (alive/dead, moves), players, reports and artwork review, the calendar and festivals, money and the levers, health, storytellers, chapters and storylines (the Storykeeper) |
| Hasn't | storylines, the live server's controls | map editing tools |

- The two old hosts become modules of the new one; their routes keep their paths under `/api/atlas/...` and
  `/api/dm/...`. `editor.sh` and `dungeon-master.sh` start the one tool on its suite.
- Route and NPC-area editing and NPC creation move from the DM to Atlas; the DM keeps live life (kill, revive, move,
  visit) and the storyline tools.

## 4. Pull and import, with conflicts

**The base.** A dev world with an upstream keeps the snapshot of its upstream as of its last pull or import (table
`admin.sync_base` in the dev world: upstream key, the live release it saw, when, and the content: every synced row by
its key, as JSON). A world copied from live starts with that copy as its base.

**What is synced**, row by row, each by its key: authored content (cells, terrain chunks, interiors, links, places,
resources, chapters, the world row) and live data (NPCs, profession slots, professions, roster characters, patrol
routes with their posts, NPC areas, spawns, factions with their claims and relations, NPC voices, kin, dialects,
figures, story visitors). Never synced, and never touched by either: the game's player data and history
(`game.*`: accounts, characters, memories, the event log, the save), and the economy's state.

**Three-way**, for each key: base B, dev D, live L.

| | Result |
|---|---|
| D = L | nothing to do |
| D = B, L ≠ B | live changed: take live's |
| L = B, D ≠ B | dev changed: keep dev's |
| both changed, differently | **conflict**: shown field by field; the user keeps dev's or live's (or, for an NPC, field by field) |
| removed on one side, changed on the other | conflict |

**Atlas: Update from live** (a dev world with an upstream). Reads live, compares, applies every live-only change to
the dev world, lists the conflicts for the user to choose, applies the choices, and makes live's state the new base.
Warns if the live server is running (live data may still move; the DM's import will catch it).

**DM: Import from dev.** Only during maintenance. Compares again (live may have moved since the pull) and lists what
the import will change and the remaining conflicts; the user resolves them; one transaction writes live, records a
release (number, who, note, summary, the authored content for rollback, as now), and becomes the dev world's new base.
References are checked as now: nothing live may be left in a removed area. Players' data is never written.

**Maintenance and the switch.** The DM sets the live world to maintenance: the game server tells players, saves and
exits (status 76), and its launcher waits. The DM imports, or switches the live world (refused outside maintenance;
warns and asks again), then turns it back on: the launcher starts the server on the chosen world.

Push to live's add-only rules and its "copy NPCs back" are replaced by these.

## 5. The economy rules erased

On Ridgemere 2.0 (branch `ridgemere2`): the economy's rules come out of the code: docs 42, 61 and 63's mechanisms,
doc 64's thirteen phases and every balancing round (the WageRule switches). Kept: money (accounts and coins), the
item, recipe, craft, building and shop-type catalogues, shops holding stock at their catalogue prices, buying and
selling. NPCs' lives (schedules, talk, relations) stay; what their needs bought (food) is paused, so nobody starves,
until the user's own rules say how they are met. `econ_watch` and the economy tests that test removed rules go with
them.

## Phases

1. Worlds: the registry, the commands, every launcher and tool, the server.
2. NPCs without a map: the migration, the build, the server, Atlas's list and assignment; Ridgemere 2.0's holding
   room removed.
3. One tool, two suites.
4. Pull and import with conflicts; maintenance and the live world switch.
5. The economy rules erased.

Phases 1, 2 and 5 can be built side by side; 3 needs 1; 4 needs 1 and 3.

## Progress

### Phase 1: built (2026-10-30)

- **The registry**, `Database/worlds.json`, as section 1 shows it. `tools/world_db.py` reads it (read again when the
  file changes, so a running host sees a new world) and checks it: unique keys and databases, kind dev or live, an
  upstream only on a dev world and only to a live one, keys lowercase and none of the words the tools take in their
  place (`atlas`, `live`, `all`, `town`, `latest`, `export`, `demo`, `none`). `DATABASES` ({key: database}) is derived
  from it and kept up to date in place. Helpers: `worlds()`, `world(key)`, `kind(key)`, `upstream(key)`, `keys([kind])`,
  `atlas_world()`, `live_world()`, `local_choice()`, `resolve(name[, kind])` (`atlas` and `live` are the local
  choices), `push_worlds()`, `register(entry)`, `set_local(...)`. `WORKING` is gone; the name still answers (Atlas's
  world) for code on other branches.
- **Local choices**, `Saved/Config/worlds.local.json` (`/Saved/` is ignored), `{"atlas": "ridgemere2", "live": "prod"}`
  while it is missing. A choice naming a world that isn't registered, or of the wrong kind, is an error that says how
  to choose again, never silently swapped for another world.
- **Commands:** `list`, `use KEY` (dev worlds only), `serve-world KEY` (live worlds only; always prints the warning,
  changes nothing without `--yes`), `new`, `resolve atlas|live|KEY [--kind] [--with-kind]` (for the launchers),
  `migrate [--db KEY|all]` and `status` over every registered world, `conninfo`/`psql` with any key (or atlas, live),
  `set-publish-password [--db KEY]` (default Push to live's live world). The publish password is seeded on every live
  world (was: on prod).
- **`new`** uses the superuser from `Database/.env` (as `ensure_roles` and `up` do; `ensure_roles` now grants the DM
  every registered world). It creates `ratw_KEY` owned by ratw_owner, revokes PUBLIC and grants CONNECT, then applies
  every migration (empty) or copies with `CREATE DATABASE ... TEMPLATE` (refused, naming who, while anyone else is
  connected to the source); a failure drops the new database again. Differs: a **live** world grants CONNECT to
  ratw_publisher, game and dm but not ratw_editor, as PROD has always been (doc 20: Atlas never reaches a live world);
  a dev world grants all four.
- **`--without-map`** is the Ridgemere 2.0 recipe in one transaction as ratw_owner, saving
  `artifacts/<key>/old-map-placements.json` first. Differs from the hand recipe, as the schema needs: NPC areas
  (`live.npc_areas`) on cells are what wander areas point at, so an NPC whose wander area was an NPC area on a cell has
  it cleared and the spawn rules in those areas go with them (cascade); profession slots' home, work and evening on
  cells are treated like NPCs'; the placements also keep the NPC areas, spawn rules, places and resources removed.
  With migration 0052 (nullable areas) present the places are unset and no holding room is made.
- **Launchers:** `live.sh server [KEY]` (default the live world) and `live.sh play [KEY]` (default Atlas's), building
  dev-kind worlds first; `scratch.sh [KEY|town]` (default Atlas's; any registered world); `server.sh --database KEY`
  resolves the key and makes `RATW_DATABASE_URL` itself when no launcher gave it; `mind.sh` gives the Mind any world's
  history (was: only dev and prod); `play.sh` passes it on.
- **Tools:** `world_build.py [KEY]` and `export --world KEY` (any dev world, default Atlas's; `latest` lists every
  world), `world_store.py --world`, `apply_terrain.py --world`, `talk_seed.py --world` and `voice_tables.py --world`
  (their `talk_check` runs read the same world they write; they read `dev` and wrote the working world before),
  `chronicle.py KEY`, `perf_report.py --database KEY`, `npc_mind.py --database KEY`, `dm_service.py --database KEY`
  (default Atlas's world, as it was the working world), `game_run.GameServer(database=KEY)` (makes the connection) and
  `client_frame_smoke.py --database KEY`. `Tests/talk_check.cpp --world KEY|EXPORT_DIR`: a directory is an export,
  anything else is resolved by `world_db.py resolve`.
- **Atlas** (`map_editor.py serve [--world KEY]`, `live_edit.LiveWorld(key=...)`) edits Atlas's world (it edited DEV
  until now); `/api/session` returns `world` ({key, name, kind, upstream}) and the top bar shows it under the world's
  name; its big-world playtest plays that world. Push to live is offered only while Atlas is on Push to live's source
  world (DEV); on another world the dialog says why.
- **The DM**: `targets()` is every registered world, live ones first; requests without a target go to the live world
  (the local choice); the accounts stay in PROD (`ACCOUNTS`). `GET /api/worlds` lists them, and the DM front-end's
  world switcher, its confirmations ("... on Ridgemere (live)?") and its headings come from it instead of
  'prod' | 'dev'.
- **Push to live** (`publish.Publisher`) reads its two worlds from the registry (`push_worlds()`: Atlas's world if it
  has an upstream, else the first dev world with one: DEV and PROD now); its logic is untouched.
- **The game server** takes any non-empty database key (`Game::loadFromDatabase`); there was no prod-only behaviour to
  key on kind.
- Tests: `tools/test_world_db.py` (the registry, `use`, `serve-world`'s refusal, `list`, `push_worlds`, connection
  settings, and `new` on throwaway databases: empty dev and live worlds, a copy, a copy without its map, a copy refused
  while the source is in use), `test_map_editor.py` (the session's world), `test_dungeon_master.py` (targets).

### Phase 2: built (2026-10-30)

**The database.** Migration 0052 makes `home_area`, `work_area` and `evening_area` optional on `live.npcs` and
`live.profession_slots`; their foreign keys stay (a set place still names an area of the world). The coordinates stay
NOT NULL and mean nothing while their area is unset (the tools write 0, 0). The NPC records (`live.people_manifest`)
write an unset place as `"-" 0 0` (no area ID can be `-`), as the editor's exporter does. Nothing else in the schema
assumed them set; Push to live's orphan check (`publish.orphans`) now skips an unset place.

**The build.** `world_store` reads and writes an unset place as `null` in the editor's NPC (`home`, `work`, `evening`).
`map_editor`'s checks take `null` for an NPC's or slot's place (a merchant's counter is checked only when there is one).
Its exporter writes them as `"-" 0 0`. `check_project` has a `playable` switch: saving a world (and the DM's checks of an
NPC or layer edit) no longer needs a spawn or any cell (a missing spawn is a warning), while exports and builds still
need a spawn, in a cell **or an interior**: a world of interiors only builds.

**The game server.** An NPC with no home is **unplaced**: the world keeps it in `AuthoredRoster::unplaced`
(`Society::unplaced(id)`), with its job label, talk, story and families, but it has no entity, no `ResidentLife`, no
account, no job position, and nothing ticks for it (`spec()` never finds it, so no system can treat it as a resident).
Families naming an unplaced NPC are kept; the society takes them in only once both are residents. An NPC with a home and
no work is **placed without work**: it starts at home, its work place is its home, its work label `-` (no job of its own,
as one born here), unpaid, with no patrol, and in the game a civilian (no counter, no post); with no evening place it
spends evenings at home. The database keeps its authored role and label, so giving it work again restores them.

- Placing: a push or a DM's `npc.sync` that gives an unplaced NPC a home (`World::adoptResident`) brings it in **at
  home**, whatever the hour; so does a new build restored from a save made before (`World::restore`: any resident new
  since the save now arrives at home, not at the noon rule's work place). Unplacing live takes the body away and keeps
  the roster entry; its coins go to the town, as for one who leaves.
- Saves: unplaced NPCs are never in a checkpoint (it holds the society and bodies only); a save restores into a build
  that has placed or unplaced them since (tests: `town_tests`, `unplacedResidents`, through the checkpoint document).

**Atlas.** The people workspace's explorer has a filter (all, unplaced, without work, with counts) and notes each NPC
and slot as "unplaced" or "no work"; the inspector's Places section has Clear beside Pick for the home, work place (the
business: its area and the post or counter in it) and evening place, and says what an unset home or work place means.
An NPC without work is drawn on the map at home. A new NPC in a world with nowhere to put it starts unplaced.

**The DM.** Its NPC list, job holders and NPC panel show "unplaced" / "no work" for unset places (`holders` carry
`null`), and its money view counts only placed NPCs' homes.

**Differs from section 2.**

- Routes not laid out (migration 0053): Ridgemere 2.0's ten patrol routes lost all their posts with the map, and twelve
  placed NPCs still walk them. A route may now have no posts (Atlas and the checks); it is left out of the NPC records,
  and whoever walks it holds their work post until its posts are laid out again.
- "Placing an NPC later" in Atlas is the same Pick as before; the live server sees it at the next build or `npc.sync`.

**Ridgemere 2.0.** Migrated (0052, 0053, nothing else). In one transaction: 556 work and 128 evening places that named
`r2_unplaced` unset (no home was there, so none of its 1,083 NPCs is unplaced: 556 are placed without work), the room
deleted, the world's revision raised. `python3 tools/world_build.py ridgemere2` refuses it for one reason: **it has no
player spawn** ("Choose a player spawn in an existing cell or interior."). With a spawn chosen in memory only (nothing
written), it exports, and the game's own loader (`world_check`), with the NPC records from the database as the server
reads them, loads all 626 interiors and 1,083 residents; half an hour simulated runs (125 residents find no way to a
place in another building: there is no map between them). Once a spawn is set (in a cell or any interior), it
should build and load on a scratch server; nothing else stood in the way.

### Phase 3: built (2026-10-30)

**One host**, `tools/studio.py serve [--port N] [--suite atlas|dm] [--world KEY]` (`bash tools/studio.sh` builds the UI
when its sources changed, as the old launchers did, then serves it), on one port, 8765 by default, local only.
`editor.sh` and `dungeon-master.sh` start it opening on their suite, and so do `map_editor.py serve` and
`dungeon_master.py serve`; a second start on a port where a studio already answers prints its address and exits. The
old hosts are modules: `map_editor.make_handler(...)` and `dungeon_master.make_handler(dm, offers, world, kinds)` give
their request handlers (their `make_server`s stay, for the tests), and the studio hands each request, path rewritten,
to the right one:

| Path | Handler | Guard |
|---|---|---|
| `/`, `/assets/...`, `/portraits/...` | the built app (`Editor/dist/index.html`) | the local origin |
| `/api/dm/...` | the DM's, without the routes that moved to Atlas, and only on live worlds (a dev world named is refused) | the DM's sign-in and roles, as before |
| `/api/atlas/...` | Atlas's (`map_editor`), without Push to live (`/api/atlas/publish...` answer 404 with why) | a `dm` or `admin` sign-in, then Atlas's own session token on every write, as before |
| `/api/atlas/layers/...` | the DM's methods for what moved to Atlas, on Atlas's world only (another world named: 409) | a `dm` or `admin` sign-in; the DM's roles and audit |
| `/api/studio/...` | `ping`; `session` (who, the suite to open on, Atlas's world and the dev worlds, the live worlds and the one the live server runs); `atlas-world` (POST: switches Atlas's world, `world_db.set_local(atlas=KEY)` as `world_db.py use` does, and the Atlas handler's database) | a sign-in; switching, `dm` or `admin` |

Every request needs the local origin (the old hosts' check, which also lets the Vite dev server's proxy through). The
old hosts' paths (`/api/session`, `/api/players`, ...) are not kept: their front ends moved to the new prefixes and
both old hosts start the studio, so nothing calls them. The Storykeeper (`dm_service.py`) is untouched: nothing in the
DM's host reached it over HTTP, and it stays its own service.

**What moved** (`studio.MOVED_TO_ATLAS`): making, changing and deleting NPCs (`npcs/save`, `npcs/delete`), wander
areas (`npcs/wander`), patrol routes, NPC areas and spawn rules (`routes/*`, `areas/*`, `spawns/*`), factions with
their claims, relations and members (`factions/*`), and talk (`talk/voice`, `kin`, `dialect`, `figure`). The DM keeps
reading them (its LIVE map's layers). It keeps live life: kill and revive (`npcs/life`), move, visitors, players and
their actions, reports, artwork, the calendar and festivals, money and the levers (moved as they are: Phase 5 is
removing them), health, storytellers and milestones, chapters, chronicles.

**One app**, `Editor/index.html` (`src/studio/`; `dm.html` and the two old entries are gone), with a top bar: the suite
switch (Atlas | DM; Atlas greyed for a viewer), the suite's world (Atlas: a picker of the dev worlds, which switches
Atlas's world for everyone using the studio, after asking; DM: the live worlds, the live server's marked, read only,
"Switching it: Phase 4"), and the signed-in account with Sign out. One sign-in, the DM's (`dm.admins` in PROD), kept in
the tab as before; Atlas's calls carry it beside Atlas's session token. Atlas's editor name defaults to the account's.
Each suite and Atlas page is its own page load (`?suite=atlas&page=npcs`): Atlas's Workshop and the DM's maps share
one lazy-ground list and Atlas's live session runs as long as its page, so they are never mounted together.

- **Atlas suite**: the **Workshop** (today's Atlas: map, interiors, people, characters; no brand of its own, and no
  ⇪ Push to live button or command), **NPCs & areas** (the DM's NPC tab in its Atlas form: NPCs created, placed,
  changed and deleted with their routes, areas, spawn rules, wander areas, voices, the dialects and figures; no kill,
  revive, life story or calendar), and **Factions** (the DM's Factions tab as it was), the last two on Atlas's world.
- **DM suite**: LIVE, **NPC Life** (the NPC tab in its DM form: NPCs and their layers view only, kill and revive, each
  one's life, the calendar), Chapters, Story Creator (still to come), Players, Money, Server Health, and **Import from
  dev (coming in Phase 4)**, a placeholder. LIVE's spawn brings visitors only; an NPC who stays is made in Atlas.
  Factions left for Atlas. The DM's maps have no editing tools left: painting areas and claims went with their tabs.
- Calls go by world (`dm/api.ts`): a live world's to `/api/dm/`, a dev world's to `/api/atlas/layers/`, so the moved
  panels are the same components.

**Tests**: `tools/test_studio.py`: the app and the local origin; one sign-in and each suite's worlds; Atlas needs a
`dm` or `admin` account and still its session token on writes; Push to live is not in Atlas; Atlas offers no
storyline, live-life or sign-in routes; the DM suite offers none of the moved authoring (or Atlas's own routes) but
keeps kill/revive, players and storylines; the DM works on live worlds only, Atlas's NPC pages on Atlas's world only
(and audit as the signed-in account); switching Atlas's world (dev worlds only, `dm` or `admin`, a new Atlas session);
the old paths gone; with the local PostgreSQL, a real sign-in on scratch databases opening both suites, and an NPC area
painted from Atlas landing in Atlas's (scratch) world and refused from the DM.

**Differs from section 3, or found on the way.**

- The DM suite no longer rehearses on dev worlds: section 3 gives it live worlds, and its methods' writes to a dev
  world are Atlas's now. `DungeonMaster` itself still takes any registered world (its tests use DEV).
- PROD is behind on its migrations (`live.npc_relations` and others missing), so the DM suite's NPC Life, its world map
  and whatever reads the newer tables answer 500 there ("The studio could not answer (UndefinedTable ...)") until
  `world_db.py migrate --db prod`; the old DM host failed the same way (it dropped the connection). Players and the
  calendar read.
- `npcs/wander` and spawn rules count as NPC-area editing (Atlas); talk (voices, dialects, figures) and factions follow
  the table (Atlas).
