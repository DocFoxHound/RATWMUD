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

**The registry**, `Database/worlds.json` (in git), lists every world. As planned (2026-10-30):

    {"worlds": [
      {"key": "prod",       "name": "Ridgemere (live)", "database": "ratw_prod",       "kind": "live"},
      {"key": "dev",        "name": "Ridgemere (dev)",  "database": "ratw_dev",        "kind": "dev", "upstream": "prod"},
      {"key": "ridgemere2", "name": "Ridgemere 2.0",    "database": "ratw_ridgemere2", "kind": "dev"}
    ]}

Since 2026-10-31 DEV is Ridgemere 2.0 (`dev` is `ratw_ridgemere2`, also called `ridgemere2`) and the old DEV is the
archive `dev-old`: Progress, "DEV is Ridgemere 2.0; the same map".

- `kind`: **dev** worlds are edited in Atlas and run on local and scratch servers; **live** worlds are what players
  join, run and managed from the DM.
- `upstream` (dev worlds only): the live world it is a working copy of. Only a dev world with an upstream can pull
  from live or be imported into it. Side projects have none.
- Each database holds one world, as now. Schemas never drift: `world_db.py migrate` covers every registered world.

**Local choices**, `Saved/Config/worlds.local.json` (not in git): `{"atlas": "dev", "live": "dev"}` while it is
missing (since 2026-10-31, DEV is the only server: Progress, "DEV as the server"; was `"live": "prod"`).

- `atlas`: the dev world Atlas, the world builder, local and scratch servers and the talk/voice tools use by default.
  Switched in Atlas (or `world_db.py use KEY`).
- `live`: the world the live game server runs (`tools/live.sh server` without a key): a live world, or a dev world
  run as the server. Switched in the DM only, only during maintenance, with a warning.
- `dm` (optional): the DM suite's world, any world, chosen in the DM's top bar (or `world_db.py dm KEY`); while unset,
  the live server's.

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

### Phase 4 engine: built (2026-10-30)

Section 4 without its screens: `tools/world_sync.py` (library and command line), migration 0054, the game server's
maintenance exit, `tools/live.sh`'s wait, and `tools/publish.py` turned into a thin layer over the import.

**Migration 0054** (every world). `admin.sync_base` (one row: upstream key, world, the upstream release seen, taken
at, by, how: init, copy, pull or import, row count) and `admin.sync_base_rows` (table, key as a JSON array without the
world, sha1 digest, the row as JSON): the base, a row per synced row rather than one value, as DEV's terrain alone is
some 50 MB. `admin.server_state` (one row: running | maintenance, who, when, a note), whose trigger NOTIFYs
`ratw_maintenance` on a change. Releases and password attempts take the kind `import`. Grants: ratw_publisher (pull,
import, maintenance) reads and writes the base and updates the state; ratw_editor reads and writes the base and reads the
state; the game and the DM read the state (USAGE on admin for the three; admin.settings stays the publisher's).

**What is synced**, by each table's primary key (the world column left out): `world.worlds` (one row, never inserted
over or removed; `revision` and every `updated_at` not compared), cells, interiors (their `world.areas` rows follow
them), terrain chunks (cx, cy), links, places, resources, chapters; `live.professions` and `live.characters` (no world
column), patrol routes with their posts (one unit: the row's `posts`), NPC areas, spawn rules, factions, faction claims,
NPCs, profession slots, faction members, faction relations, NPC voices, families (`npc_relations`), dialects, figures,
story visitors. Left out: NPCs a spawn rule made (`spawn_id` set: each server's rules make their own) and their voices
and families; `world.minimaps` (unused; still in a release's rollback content); and, as asked, `game.*`, `live.economy`,
`live.npc_state`, `dm.*`, `admin.releases` (only written by recording a release).

**The comparison** is section 4's table on row digests. A field-by-field choice (any row both sides have, not only
NPCs) need only name the fields both sides changed: a field only one side changed goes that side's way unless chosen.
Conflict ids are `table/key` (`live.npcs/wren`, `world.terrain_chunks/0/1`, `world.worlds`; `/` and `%` in a key
%-escaped). A plan carries a `digest`; pull and import take it back as `expect` and refuse if the worlds moved since.

**Pull** writes the dev world directly (one transaction, holding the publish lock and the world row Atlas's edits take
turns on), checks references (`publish.orphans`, doors into removed places, every deferred constraint), raises the
world's revision and logs one edit with the key `reload`: `live_edit.sync` answers `reload: true` and open Atlas
sessions reload, as they already do when the world is replaced. (Going through the keyed edit log instead was
impractical: most synced tables have no edit key.)

**Import** refuses unless the live world is in maintenance; checks the password (Push to live's, attempts and lock as
before); validates the dev world; then in one live transaction takes the game server's own lock (`pg_try_advisory_
xact_lock(1380013143, 1)`: refused while a server still holds the world, so none can start mid-import either) and the
publish lock, compares again against the stored base, applies, checks references, records the release (number, who,
note, summary `{world: {label: {added, changed, removed}}, live: {...}}`, the authored content for rollback), builds and
NOTIFYs `ratw_release`. **Decided:** afterwards the dev world is brought level with live (every row nobody edited in dev
during the import takes live's version: live's own changes and the conflicts chosen "live"), so "becomes the dev
world's new base" holds without later diffs showing live's changes as dev's undoing them. A row edited in dev during the
import keeps the edit and its old base row (still compared three ways next time). If that second step fails, live keeps
its release, dev keeps its rows and its old base, and the next pull brings live's changes in (the result says so).

**Maintenance.** `world_sync.py maintenance KEY on|off`. The game server (`Game::watchMaintenance`, Core/RatwGame.cpp)
listens on `ratw_maintenance` and reads the table once a minute anyway; on maintenance it tells everyone connected "The
world is closing for maintenance in 60 seconds (note)...", again at 10 seconds, then saves and asks the host to exit
with 76 (at once if nobody is connected); called off before then, it says so and play goes on. At start
(`loadFromDatabase`), a world in maintenance is refused with a clear message and status 76. A scratch server ignores
maintenance (it writes nothing). `tools/live.sh server` on 76 waits (every `RATW_MAINTENANCE_POLL`, 5 s) until
`world_sync.py status KEY --state` says running, re-resolving `live` each time, and starts the server on the live
choice then (rebuilding a dev world first). **The live switch**, `world_db.py serve-world KEY --yes` (warning kept), is
refused unless the current live world is in maintenance; the new one is put in maintenance, so the launcher waits until
the DM opens it.

**The first base.** `world_db.py new KEY --upstream LIVE --from LIVE` takes the copy as its base (before
`--without-map`, so taking the map away is the copy's own change) and sets the copy running. DEV has no base yet:
`python3 tools/world_sync.py init-base dev` takes PROD's snapshot as it is then, so every difference between them at that
moment counts as DEV's change (an import would put DEV's versions live, including over anything PROD's NPCs did since
DEV was copied); `--prefer live` takes DEV's own instead, so every difference counts as PROD's and a pull makes DEV equal
PROD. Not run on DEV (left to the user, after `migrate`).

**`publish.py`** keeps `Publisher` (preview, push, pull, rollback) for Atlas's Push to live dialog and its command line:
push is an import (`choices` and `expect` may be passed; without maintenance it is refused), preview is `plan_import`
with the old keys (`release`, `summary` = what the import changes on live, `changed`), pull is a pull (with the old
dialog's `copied`/`skipped`). The release, password, rollback and table helpers moved into `world_sync` and are
re-exported. `plan_seeds`, `pull_npcs` and `copy_faction_details` are gone.

**The API the screens call** (`world_sync.Sync(connect=None)`, or the module functions of the same names; every world
reached as ratw_publisher unless `connect(key)` is given; errors are `PublishError` (422, `.errors` a list),
`WrongPassword` 401, `Locked` 429, `Unresolved` 409 (`.conflicts`: the plan items left), `NotInMaintenance` 409,
`S.Unavailable`):

- `status(key)` → `{key, name, kind, upstream, world, server: {state, by, at, note}, serverRunning, base: {upstream,
  world, release, takenAt, by, how, rows} | null, release}`.
- `diff(dev_key)` → the plan: `{dev, live, world, base, liveRelease, counts: {live, dev, conflicts}, summary: {live:
  {world: {label: {added, changed, removed}}, live: {...}}, dev: {...}}, liveChanges: [item], devChanges: [item],
  conflicts: [item], digest, warnings}`; an item is `{id, table, label, part: world|live, key, name, side:
  live|dev|conflict, dev: added|changed|removed|unchanged|absent, live: (the same), fields: [{field, base, dev, live,
  changed: dev|live|same|conflict|null}], fieldChoices}`; a long value (terrain glyphs) is `{"$long": length, digest}`.
- `pull(dev_key, choices={}, editor='', expect=None)` → `{dev, live, world, release, applied: {world: {...}, live:
  {...}}, conflicts, kept, warnings}`.
- `plan_import(dev_key)` → the plan plus `{devRevision, errors, warnings, server, serverRunning, blocked: [why],
  releases: [{number, at, by, note, kind, restored, summary}]}`.
- `import_(dev_key, choices={}, password, editor, note, expect=None)` → `{release, summary, conflicts, dev: {updated,
  editedMeanwhile: [id]} | {updated: 0, error}}`.
- `init_base(dev_key, editor, prefer='dev')` → `{dev, live, world, release, rows, prefer}`.
- `maintenance(key, on, editor, note)` → `status(key)`; `switch_live(key, editor)` → `{live, was, name, server}`.
- `Sync.releases(live_key)`, `Sync.rollback(live_key, number, password, editor, note)` → `{release, summary}`.
- Choices: `{conflict id: "dev" | "live" | {field: "dev" | "live"}}`.

**Command line:** `world_sync.py status KEY [--state] [--json]`, `diff DEV [--json]`, `pull DEV [--choices FILE]`,
`plan-import DEV [--json]`, `import DEV [--choices FILE] --note ...` (asks for the password), `init-base DEV [--prefer
dev|live]`, `maintenance KEY on|off [--note ...]`.

**Tests.** `tools/test_world_sync.py`: the table row by row, removed against changed, field-by-field choices (and stale,
missing and impossible ones), ids and stored-row round trips; on throwaway worlds (a live Greyfen and its copy): the
copy's base, a pull (live's changes in, dev's kept, the economy, NPC state and players' data untouched, Atlas told to
reload), conflicts field by field, removed against changed both ways, a first base both ways, routes with their posts,
an import (refused outside maintenance, with a wrong password, while a game server holds the world, and when it would
strand a live NPC; the release, its build, the notification, players' data untouched, rollback), conflicts at import
and dev level after it, maintenance announced on and off; a real `ratw_server` closing for maintenance with 76 and
refusing to start in it; and `live.sh` (with a stand-in server) waiting through maintenance and a live switch, then
starting the new live world. `tools/test_publish.py` rewritten for the import. `Tests/maintenance_tests.cpp`
(`maintenance_behaviors`): the grace, the reminders, at once with nobody connected, called off.

Not done: the screens (Atlas's Update from live, the DM's Import from dev, maintenance and the switch), and on the real
worlds `world_db.py migrate` (0054) and `world_sync.py init-base dev` (the user's to run). Atlas's old Push to live
dialog still works but cannot pass conflict choices or set maintenance: until the screens come, conflicts are chosen
with `world_sync.py import DEV --choices FILE`.

### Phase 5: built (2026-10-30)

Branch `r2-p5-no-economy`, from `ridgemere2` (6c4a1b5). About 40,000 lines out, 1,200 in: Core −18,400, Tests −21,800.

**Removed.** The simulated economy's mechanisms, whole:

- Doc 64's phases 0 to 13 and every balancing round: work by the hours and the realm's pace, the books, the worth
  ladder's pricing (costed prices, margins, markdowns, carriage), production by hours (makers' batches, producers'
  yields, seasonal fields, tools' wear, upkeep, laying in), businesses' tills as owned businesses (owners topping up,
  stopping, idling and returning, credit, rent, working money), the labour market (employers' wages, going rates,
  hands by sales, hires, poaching), odd jobs, crews and farmhands, households' purses, keepers, wants, pieces and works,
  children's stipends, institutions' budgets, relief and alms, taxes, the holdings tax, the mint, wages of every kind,
  kept hands' meals, NPCs' belongings and mending, the economy's monitor, and the WageRule and WageRule2 switches with
  every rule behind them. Files: `Core/RatwDemand`, `RatwPrices`, `RatwLabour`, `RatwInstitutions`, `RatwProcure`,
  `RatwReckoning`, `RatwWages`, `RatwOutwork`, `RatwHouses`, `RatwTills`, `RatwFounding`, `RatwOddJobs`,
  `RatwFarmhands`, `RatwBelongings`, `RatwMint`, `RatwHouseholds`, `RatwSurplus`, `RatwOrchestrator(.h, Json.h)`,
  `RatwOrchestrate`, `RatwTrade`, `RatwCrafting`; `RatwSociety.h` from 2,721 lines to 646, `RatwResidents.cpp`'s day
  from 1,500 lines to 450.
- Doc 42's: town purses (the treasury's share at founding), the weekly reckoning, the church's Restday plate and its
  care, rents, living costs, the rule against hoarding, the Town Works and the towns' repair, outwork, the daily
  carter's and the towns' caravans of food, supply runs, contracts for goods (procurement), store-priced meals and
  herbs.
- Doc 61's: Houses owning trades, the towns' specialties, standing orders and their porters (`Data/Economy/houses.json`
  and `specialties.json` deleted). Doc 63's model is gone from the game (`tools/econ_model` untouched).
- Doc 57, 5's trade that moves prices (price notes, "fed the town"), the town talking of prices, residents buying at
  players' stalls, town projects' hired hands and contracts for materials, the town's mending project.
- The DM's levers: `tools/dungeon_master.py`'s tax rate, holdings tax, mint and relief grant (and their routes), the
  money view's month, levers and monitor; `Editor/src/dm/OrchestratorPanel.tsx`; the game's `economy.*` actions and the
  Dev Console's `/reckon`; `world_check --reckon`.
- `Tests/econ_watch.cpp`, `econ_players*`, `econ_watch_stamp.cmake`, `crafting_tests.cpp`, `orchestrator_tests.cpp`,
  `founded_fixture.h`, `tools/econ_run.sh`, `econ_compare.py`; the economy's tests in `roads_tests` (4,774 lines to 830)
  and elsewhere.
- Saved state: the society's `books`, `houses`, `memory`, `orchestrator` and `craftingStocked`; the roads' standing
  orders, `sentEarly`, `failed`, `refused`, `stocked`, `purses`, a caravan's trader and orders, a contract's goods. An
  older save still loads: those fields are read past (`wire::readSociety`, `checkpoint`), and a procurement contract in
  it simply runs out.

**Kept.** Money (accounts in whole pennies, `shift`, `sale`, `create`, `consume`, the ledger and the journal,
conservation); players paying and being paid (contracts, couriers, bounties, escorts, lodgings, stalls between players,
tables, festivals' entries, letters, fines, the Hold's staff at `camp::HoldWage`, companions' hire); the catalogue
(`Data/Items`: items, qualities, recipes, crafts, producers, businesses, stations, regions; `Data/Economy/ladder.json`
for the items' worth and the catalogue's day); buildings; players' crafting and foraging; shops: each merchant post's
till (`till:<position>`, founded once with its keeper's wares and its founder's authored purse's worth), its wares
(`Society::wares`), at the catalogue's price (`Society::catalogPrice`; a tenth off at a stall), buying from players at
`SellBackShare` (.55) up to a store of a good (`SuppliesKept`, `GoodsKept`); NPCs' lives: schedules, posts as titles,
careers, skills and apprentices, succession, families, births, deaths, ageing, retirement, newcomers (with
`NewcomerPurse`), estates inherited, households splitting, Restday's service (the plate gone), festivals and market
days, patrols, travellers, wander areas, children's friend groups, talk, relations, troubles (the shop's loan gone), the
Mind. The Great Houses as people (a head, its family, a leader in talk), not owners. The demo roster's own little
economy (doc 26's six) is untouched. The caravan machinery is kept but nothing sends one (`World::sendCaravan`, public).
The DM's money view stays, as a read of where the money is.

Kept with a change: a player's farm or bench work beside a resident (doc 53) still practises and lends the paw, but
pays nothing and brings nothing in; the Hold's workshops and fields are built but make nothing; a talk speaker's
`on_dole` is "no" and `business` "none"; a trouble's household is reckoned in days of `troubles::PlainFoodADay` (5p) at
catalogue prices; the hungry thief marks a shop whose till holds meals. A town project's placement now loads its cell
first (no caravan walks a road to load it any more). No cap on what a single piece costs (the user, 2026-10-30): the
ledger's 5,000p cap, `Society::PieceMost`, is gone, from trades and story prizes alike (a trade refuses only a price
whose total would overflow, and what the buyer can't pay; a market stall keeps its own 999p limit).

**Paused.** Needs that buying met: `Society::NeedsPaused` (true). A resident's hunger neither grows nor sends it
anywhere (no shopping, larders, alms, rations or kitchens); one hungry from a save still eats what it carries. Home
stores are opened empty. Nobody restocks a shop: what it sells stays sold until rules say how goods come in.

**Where the user's rules plug in.**

- Needs: `Society::NeedsPaused` and the line in `Society::decideAuthored` (`RatwResidents.cpp`, the first half) where
  hunger would grow; the "eat" branch below it.
- A day's rules: the day's turn in `Society::decide` (`RatwSociety.cpp`, where `budgetDay` moves on), before
  `decideAuthored`.
- Work: `atWork` in `decideAuthored`'s second half: a resident at its post in its hours (trade, watch, its title).
  Making and paying go there; `Society::giftLiftOf` and `pawRateOf` keep the Gifts and paws players lend a maker.
- Prices and shops: `Society::catalogPrice`, `quote`/`trade`, `tillOf`, `foundShops`, `wares`.
- Goods between towns: `World::sendCaravan` (`RatwRoads.cpp`); a caravan's arrival unloads into the town's stores.
- Players' work for residents: `World::workBeat` (`RatwTogether.cpp`); a Hold's work: `Game::campTick`
  (`RatwGameCamps.cpp`); town projects: `Game::projectTick` (`RatwGameProjects.cpp`).
- The primitives every rule uses: `Society::shift`, `sale`, `create`, `consume`, `openAccount`, `cash`.

Tests: 66 of 66 (`shops_tests` new: the catalogue, a shop's till, a player's trade at catalogue prices, a day and a half
with nothing bought, made or paid and hunger held, an older save read). A scratch server on the town world started and
ran.

### Phase 4 screens: built (2026-10-30)

Section 4's screens, over the engine (`tools/world_sync.py`), in the one tool.

**The studio's routes** (`tools/studio.py`, answered by the studio itself; the local origin and the one sign-in as
before; reads for any account, writes for `dm` and `admin`; every pull, import, maintenance change, switch and rollback
audited in that world's `dm.audit` through `DungeonMaster.log`):

| Route | Does |
|---|---|
| `GET /api/atlas/sync/diff` | the plan of Atlas's world against its upstream, with both worlds and the live server's state; 409 with why when Atlas's world has no upstream (a side project) |
| `POST /api/atlas/sync/pull` `{choices, expect}` | the pull (also needs Atlas's own session token, as Atlas's writes do) |
| `GET /api/dm/server/status?world=KEY` | a live world's status (default the live server's), the live choice, the switch warning |
| `POST /api/dm/server/maintenance` `{world, on, note}` | maintenance on or off |
| `POST /api/dm/server/switch` `{world, confirm}` | the live switch: `confirm` must be the world's key typed again; refused for a dev world or the live one; the engine refuses it outside maintenance |
| `GET /api/dm/import/plan?dev=KEY` | the dev worlds whose upstream is the live server's world (Atlas's first) and the plan of one (default the first); a refusal such as "no base yet" comes back as `problem` with the worlds still listed |
| `POST /api/dm/import` `{dev, choices, password, note, expect}` | the import into the live server's world, from one of its copies only |
| `GET /api/dm/releases?world=KEY`, `POST /api/dm/rollback` `{number, password, note}` | releases and rollback |

The engine's errors keep their meaning: `PublishError` 422 with `errors`, `WrongPassword` 401, `Locked` 429,
`Unresolved` 409 with `conflicts` (the plan's items left), `NotInMaintenance` 409, `S.Unavailable` 503, an unknown
world 400. **Changed in the engine:** a stale `expect` or choices for conflicts no longer there raise
`world_sync.Moved` (a `PublishError`, 409) instead of a plain 422, so both screens can tell "the worlds moved: look
again" from a refusal.

**Atlas: ⇩ Update from live**, a page of its own in Atlas's bar (`?suite=atlas&page=sync`, `src/studio/
UpdateFromLive.tsx`). On a world without an upstream it says why (and how a copy of a live world is made). Otherwise:
the two worlds, the live release, the base (release, how, by whom, when), a warning while the live server runs, the
counts (live changes to take, dev changes kept, conflicts), live's changes grouped by table label (each group and each
row collapsible, a row opening to its fields base / dev / live), the conflicts, and dev's own changes (kept, folded
away). Each conflict: Keep dev's / Take live's, or for a row both sides have **Field by field** (each differing field
chosen; a field only one side changed starts on that side; the ones both changed are marked and must be chosen); all
conflicts at once with one click; long values shown as "long value (N characters, digest …)". Update sends the choices
and the plan's digest as `expect`; on a 409 (the worlds moved, or conflicts left) or a refusal it compares again, keeps
the picks that still apply and shows what is left. After a pull other Atlas sessions reload (the engine's `reload`
edit); the page offers the Workshop.

**DM: Server & import** (`src/dm/ServerTab.tsx`, the tab that replaced the placeholder):

- **The server**: the live world (name, key), running or in maintenance (by whom, when, the note), whether a game
  server holds it (watched every 5 s while it is closing). Start maintenance (a note; it says players get 60 seconds,
  again at 10, then the server saves and stops), End maintenance (turns the server back on: `tools/live.sh server`
  starts it on the live world).
- **Import from dev**: the dev world (its copies), the plan (what dev's changes will write to live, by table, rows
  opening to their fields; live's own changes, kept; the conflicts with the same choices as Atlas: Import dev's / Keep
  live's / field by field), the dev world's errors and warnings, the blocked reasons, a note and the publish password.
  Disabled with the reason outside maintenance or while the game server is still stopping. Then the release number, its
  summary, how the dev world was brought level, and a reminder to end maintenance.
- **Switch the live world**: the other live worlds, during maintenance only, under the warning "Switching the live
  world is rarely wise: players' characters and progress belong to each world.", the world's key typed to confirm. The
  new world starts in maintenance; the page reloads on it.
- **Releases**: the list, and Roll back to this (password, note) for the authored content.

The choices logic is one pure module, `src/lib/sync.ts` (`buildChoices`: the UI's picks to the engine's choices,
naming only the fields both sides changed and a one-sided field when the other side's is taken; `prunePicks` after a
new plan; the value and summary texts), tested in `src/lib/sync.test.ts`. The views both screens share are
`src/studio/SyncViews.tsx`.

**Removed:** Atlas's old Push to live dialog (`PublishDialog`, `api.publish`, `PublishRefusal` and their types) and
`map_editor`'s `/api/publish...` routes and `publisher` argument (nothing passed one since Phase 3); the studio still
answers those paths 404 with where things went. `tools/publish.py` stays, the engine's compatibility command line.

**Tests.** `tools/test_studio.py`: with stand-ins, who may call each route, what reaches the engine, a side project
refused, the pull needing Atlas's token, the switch's typed confirmation, the import's copies only, and every engine
error as HTTP; on throwaway worlds (`test_world_sync`'s live Greyfen and its copy; dropped after), the user's flow
through the routes: a pull choosing a conflict field by field (refused unresolved, refused on a stale plan), the
import refused outside maintenance, maintenance, a conflict made during maintenance chosen at import, a wrong password,
two releases and a rollback, maintenance ended, each step in the world's audit; and the live switch (refused outside
maintenance, the new world in maintenance, nothing left to import into it). The Editor's type-check, build and tests
(the one known model failure, Greyfen 11 against 10, aside).

**The user's flow in the tool** (adding a few buildings to Ridgemere; DEV is the copy of PROD, its upstream):

1. **DM ▸ Server & import ▸ Start maintenance**, with a note. Players are warned for a minute; the game server saves
   and stops ("No game server holds this world" once it has).
2. **Atlas** (dev world: DEV) **▸ ⇩ Update from live**: look at live's changes, choose each conflict (Keep dev's / Take
   live's, or field by field), **Update**. DEV now has everything live did, and live's state is its base.
3. **Atlas ▸ Workshop** (and NPCs & areas): make the changes. Atlas saves as you go.
4. **DM ▸ Server & import ▸ Import from dev**: choose DEV, read what will be written, resolve any conflicts left (live
   may have moved since the update), add a note and the publish password, **Import**. A release is recorded and DEV is
   brought level with live.
5. **End maintenance**: the launcher starts the server again and players come back.

(Switching the live world, rarely: during maintenance, **Switch the live world**, type its key; it opens when its
maintenance is ended.) Before the first use on the real worlds: `python3 tools/world_db.py migrate` (0054) and
`python3 tools/world_sync.py init-base dev` (the user's to run; not done here).

### DEV is Ridgemere 2.0; the same map (2026-10-31)

The user, 2026-10-31: "For what it's worth, this map and all of these changes should definitely touch Dev. We are being
too protective of it." They chose: **Ridgemere 2.0 becomes DEV**, the old DEV kept as an archive (nothing copied or
lost; easy to undo). And: "The update from prod should only work between Prod and Dev if the maps are the same."

**The registry** (`Database/worlds.json`), branch `r2-dev-switch`:

    {"worlds": [
      {"key": "prod",    "name": "Ridgemere (live)",             "database": "ratw_prod",       "kind": "live"},
      {"key": "dev",     "name": "Ridgemere 2.0 (dev)",          "database": "ratw_ridgemere2", "kind": "dev", "upstream": "prod", "aliases": ["ridgemere2"]},
      {"key": "dev-old", "name": "Ridgemere (old map, archive)", "database": "ratw_dev",        "kind": "dev"}
    ]}

- `dev` is the Ridgemere 2.0 database (at migration 56). Everything that defaulted to DEV reaches the new city: Atlas,
  `scratch.sh dev` (checked: build 46, 828 cells and interiors, 559 placed residents), `world_build.py dev`, `live.sh
  play`, `server.sh --database dev`, the DM's world list, the Storykeeper (`dm_service.py`, default `dev`). With no
  local choice Atlas's world is `dev` (was `ridgemere2`).
- **`ridgemere2` is another name for `dev`** (a registry entry's `aliases`): `world_db.world()`, `resolve()`,
  `conninfo()`, the local choices, the studio and the DM take it and mean `dev` (`world_db.canonical(key)`). It is
  never listed as a world of its own (`keys()`, the DM's targets, `migrate`), so nothing reaches the database twice.
  The one-server-per-world lock is the database's own advisory lock, so a server on `dev` and one on `ridgemere2` are
  one world and the second is refused. The generator (`python3 -m worldgen.ridgemere2 write`, default `--world
  ridgemere2`), `Tests/talk_check.cpp --world ridgemere2`, `tools/test_ridgemere2.py`, `rename_house.py` (now default
  `dev`) keep working unchanged. A local choice saying `ridgemere2` still works and is written back as `dev`.
- **`dev-old`** is the old DEV (`ratw_dev`, at migration 54, left there: an archive), a dev world without an upstream.
  Its sync base from PROD (the first import, release 3) is still in it, unused. Keys may now hold a `-`; a world's
  connection settings name it with `_` (`RATW_DEV_OLD_HOST`).
- PROD keeps the old map, untouched (migration 54), until the user chooses to import the new one.
- The generator and `rename_house.py` refuse the old map's worlds (PROD, `ratw_dev`) by database, not by key.
- Backups before any write: `Saved/Backups/ratw_dev-2026-10-31-before-dev-switch.dump` and
  `ratw_ridgemere2-2026-10-31-before-dev-switch.dump`. Undo: put the registry back as it was (and
  `Saved/Config/worlds.local.json`); no database was renamed.

**The same map: the map identity.** Pull and import compare a dev world with its upstream row by row (cells, terrain
blocks, interiors, NPCs by their keys). Between two different maps that comparison means nothing: Ridgemere 2.0 kept
the old world's id (`upper_accord`), its NPCs and many interior ids, so a pull would have tried to "take live's
changes", putting the old map's 57 cells back. So every world has a **map identity**:

| | |
|---|---|
| What | `{id, name, made, by, how}`, e.g. `map-20261031-6b080cecb7f6`, "Ridgemere 2.0: the city (doc 67)" |
| Made | when a map is generated new: the Ridgemere 2.0 generator's write (in its own transaction, as the database owner), `world_db.py new --without-map` (no map is a map of its own), or by hand, `python3 tools/world_sync.py map KEY --new --name "..."` |
| Kept | by every ordinary edit, a new cell included (nothing in Atlas or the DM touches it), and by a copy (`new --from`: the database is copied whole) |
| A dev world's | its stamp, `admin.settings` key `map` (the owner's table, readable by the publisher, as the sync reads it) |
| A live world's | the newest release that records one (`admin.releases.summary.map`): every import now records the dev world's identity, a rollback the one of the release it restores; else a stamp copied with the database |
| Neither | the world's **original map**, `original:<world id>`, named "<world name>'s original map": PROD, `dev-old` and every world made before 2026-10-31 (an empty world is `none`, which takes any map) |

**Why this design:** it needs nothing new in PROD's schema. PROD stays at migration 54 (the user's decision), and its
identity comes from what it already has: its releases (the publisher may insert releases, never update settings) or,
with none recorded, its world id. DEV's stamp lives in DEV's own database, in a table every world has had since
migration 0004. A fingerprint of the outdoor cell grid was the other candidate and was turned down: adding a cell in
Atlas would change it, and the identity must survive ordinary edits. No migration is needed for this, now or later.

Today: DEV is stamped (Ridgemere 2.0's city, dated by the generator's write, 2026-10-31 17:30 UTC; the stamp was given
by hand on the switch, as the generator's write came before this). PROD and `dev-old` are both `original:upper_accord`
("Ridgemere's original map"): the same map, so a pull between them would work if `dev-old` were given its upstream back.

**Pull (Atlas's Update from live) refuses two maps** (`world_sync.DifferentMap`, 409, with both maps), before anything
else (a missing base included): "PROD is on a different map (Ridgemere's original map) from DEV (Ridgemere 2.0: the
city (doc 67)): there is nothing to update from until DEV's map is imported into PROD (the DM's Import from dev,
replacing PROD's map)." Atlas's page shows that and the two maps; `world_sync.py diff/pull` print it.

**Import of a different map** (the DM's Import from dev, `world_sync.py plan-import` / `import --replace-map`):

- The plan says so (`replacesMap`, `maps`, a warning naming both) and what the new map lets go of (`letGo`, below).
  The DM's screen shows the two maps and a checkbox "Replace PROD's map (Ridgemere's original map) with DEV's
  (...)", and the button stays off until it is ticked; it sends `replaceMap: true` on top of the password. Without it
  the engine refuses (`DifferentMap`).
- **Refused while the live world lacks migrations the dev world has** (columns and tables of the synced schemas):
  a new map needs them all. Today PROD (54) lacks DEV's 0055 and 0056 (`live.business_settings` and sixteen
  `live.economy` columns: the store room and the Country), so the import of the new city into PROD is blocked with a
  message naming them, until the user decides to migrate PROD. (Imports on one map tolerate it as before.)
- **The map** (the world part: the world row, cells, interiors, terrain, doors, places, resources, chapters) becomes
  dev's whole; there are no map conflicts.
- **Live data** is compared three ways as ever, against the dev world's base of this upstream, or, with none (DEV has
  none: it was copied from the old DEV before bases existed), against live as it is, where a difference in an authored
  field counts as dev's and one in a field of the live game's is a conflict, live's by default (2026-11-01: "No base:
  the real port", in "The live world's truth" below; before, every difference counted as dev's). Conflicts are chosen
  as usual. **Nothing live has is ever removed:** a row dev lacks (an NPC born in PROD, the eight
  NPCs PROD got in release 3, a faction, a family) stays as live has it. Whatever it kept that stands in a place the new
  map hasn't is let go, in the same transaction: an NPC's or profession slot's home, work or evening place there is
  unset (unplaced or without work, section 2: it keeps its name, talk, relations and memories; spawned NPCs too); the
  old map's patrol posts, territory claims and NPC areas there (with their spawn rules) go. Players' data (`game.*`),
  the save, the economy and NPCs' running state are never written: a player who stood in a place the new map hasn't
  wakes at its spawn (`World::restore`, doc 60), residents at their homes. (Not checked: players' camps and lodgings,
  `game.camp_sites` and `game.lodgings`, name cells; on a new map theirs point nowhere until the game handles it.)
- The release records `map` (dev's identity), `replacedMap` (live's old one) and `letGo` (the counts). Afterwards the
  dev world is brought level with live as after any import (it takes the NPCs live kept) and that is its new base:
  both share one identity and Update from live works again.
- Today's plan of DEV into PROD: 3,206 dev changes (57 cells and 220 terrain blocks removed, 193 interiors added, 1,083
  NPCs changed), 19 of PROD's rows kept, one territory claim let go; blocked by the migrations (and maintenance).

Tests: `test_world_sync.MapTests` (a copy is on its upstream's map and keeps it through edits, and pulls; pull and diff
refused between two maps, naming both, nothing written; an import of another map refused without the confirmation,
then made with it: the cottage gone, an NPC dev removed kept, a newcomer and someone's evenings in the removed cottage
unplaced, players' data untouched, the release recording both maps, both worlds level and pulling again; a new map
refused while live lacks a migration; a live world's identity following its releases); `test_world_db` (the registry,
`ridgemere2` as `dev`, aliases checked, a `-` in a key; a copy on its source's map, a copy without its map on a map of its
own); `test_studio` (both maps in a refusal, `replaceMap` only when true); `Editor/src/lib/sync.test.ts` (the two
messages and the let-go line).

**Births: male or female.** Every child was a daughter: it was its mother's record copied (appearance and all). Now the
birth request carries the child's sex (`ResidentRequest::sex`, `birthSex(couple, day)` in `Core/RatwCareers.cpp`: half
and half from the couple's key and the day, the society's own deterministic mix, so a run repeats), and both makers use
it: `World::makeRequestedResidents` (files, the fast run) sets the record's and the body's sex, `Game::makeResidents`
(a database world) writes the row's `appearance.sex`. Pronouns follow the body's sex (`Game::pronounsOf`); the
placeholder names were never one sex's. Tests: `careers_behaviors` (each child a son or a daughter; 4,000 couples about
half and half; one couple's days about half and half; the same couple and day give the same child), `sim_fast_behaviors`
(a made resident's body is the sex its record says).

### DEV as the server: the DM on any world, Play, the local network (2026-10-31)

The user, 2026-10-31: "make sure dm is pointed to the dev server and can run it like a live server"; "I want this to be
a default thing ... restart Dev as our only server, with atlas and DM touching it, and reachable over my network"; and
"the Play button on Atlas should launch the browser to connect with the server its working on (Dev) ... You can set
Atlas and [the DM] to point to different servers at the same time, or even the same server at the same time." Branch
`r2-dm-dev`. PROD stays shut down (in maintenance) and selectable.

**The defaults.** With no local file, `{"atlas": "dev", "live": "dev"}`: `bash tools/live.sh server` runs DEV, the DM
works on DEV, Atlas edits DEV. The live choice (`world_db.live_world()`) may now be any registered world, a dev one
included (`world_db.py serve-world KEY --yes`, and the DM's switch, list every other world; a dev world is "run as the
server"); `world_db.served(key)`. Switching to the world already live changes nothing (it used to put it in maintenance).

**Each suite its own world.** Atlas's world (a dev world, its picker as before) and the DM's world (any world: a new
local choice `dm`, the DM's top-bar picker, `POST /api/studio/dm-world`, `world_db.py dm KEY`, `world_db.dm_world()`;
unset, the live server's) are chosen apart: the same world or two. Every DM page acts on the DM's world (its requests
name it; the DM handler's default is it too: `make_handler(..., allowed=, default=)`), every Atlas edit on Atlas's. The
choices are this computer's, so they hold across restarts and for every browser; a page already open keeps acting on
the world its header names (calls carry their world) until it is reloaded, and the picker asks first. The front end
routes the DM components' calls by suite (`api.ts` `setSuite`), not by the world's kind, so the DM suite reaches its
routes on a dev world while Atlas's NPC pages keep theirs; in the DM, its world counts as one players are in (the
confirmations).

**What the DM does on a dev world**: everything it does on a live one (players, NPC life, storylines and chapters, the
calendar, reports, artwork, money, health, storytellers; the server's state and maintenance). Factions, NPC and area
authoring and talk stay Atlas's (section 3), on the same world when both point at it. **Refused, with why**: Import from
dev (the Server tab shows "Atlas's edits to the server" instead: "{name} is a dev world: Atlas edits it directly, so
there is nothing to import into it ..."), releases and rollback ("it has builds, not releases"); `POST /api/dm/import`
and `/api/dm/rollback` answer 409. Pointing the DM at PROD brings Import from dev back.

**What `--world-kind` changes in the game server**: only the store room's console (`/store`, dev worlds' DMs, doc 67);
nothing about saving, players' progress, the DM's actions, the NPC Mind, maintenance, ownership or restarts. A dev world
run as the server (not `--scratch`) saves its checkpoint and journal, holds the one-server lock, applies the DM's queued
actions, runs the Mind and spawn rules, and closes for maintenance, as a live one does.

**How Atlas's edits reach the running server.**

| Edit | Reaches the running server |
|---|---|
| Atlas's NPC pages (NPCs & areas, factions, talk: the DM's methods, `/api/atlas/layers/`), the DM's kill/revive and visitors | at once: they queue `npc.sync` and the like in `dm.actions`, which the server applies |
| The Workshop (map, cells, interiors, doors, places, its people and slots), Update from live | with a new build: the DM's Server tab **⟳ Rebuild from Atlas's edits** (`POST /api/dm/server/rebuild`), or `python3 tools/world_build.py dev` |

A dev world's new build is now announced like a release (`world_build.build` NOTIFYs `ratw_release` with `{build,
release: null}`; `world_build.announce` re-announces a current one). The game server (`Game::watchReleases`) takes a
newer build as a new version: it tells connected players "A new version of the world (build N) has been made", and once
nobody is connected saves and exits 75; `tools/live.sh` brings the build up to date (`prepare`) and starts it again.
Saves, players' progress and the DM's actions carry over (the same start-up path as a release). **A scratch server
stays on the build it loaded** (`RATW_RELEASE_IGNORED`; it used to exit 75 on a release too). The Server tab shows the
newest build and whether it is current with Atlas's edits (`world_build.status`).

**Atlas and the DM on one world: conflicts checked.**

- Keyed edits (Atlas's Workshop, its NPC pages, the economy controls, from either suite) carry "before" values and
  raise the world's revision: two editors never overwrite each other silently (refused, naming who).
- The DM's own writes are `dm.*` (its queue, stories, audit), `live.npc_state` (kill and revive) and story visitors:
  nothing in Atlas, a build or a whole-world save writes them. A build writes only `world.builds`/`build_cells` and the
  roster's slot assignments.
- **Found and guarded**: a whole-world save (`world_store.save_world`: the Ridgemere 2.0 generator's write,
  `apply_terrain.py`) deleted every NPC row its project didn't list. The game server's births, newcomers and spawn
  rules' NPCs are inserted into `live.npcs` without raising the world's revision, so a project read before them (the
  generator runs for minutes) would have removed them silently. Now `world_store.keep_made_by_the_server` keeps NPCs
  with `origin = 'runtime'` or a `spawn_id` the save doesn't list; removing one is a keyed edit (Atlas's delete).
- Not guarded (reported): Atlas's open Workshop session doesn't learn of NPCs the server makes until it reloads (it
  can't edit what it doesn't show, so nothing is lost); a pull (Update from live) into DEV while its server runs moves
  live data under it (warned already; refused today anyway, the maps differ).

**Starting a server from the studio.** `POST /api/dm/server/start {world}` (the Server tab's **▶ Start its game
server**) and Atlas's Play start `bash tools/live.sh server KEY --port N` in the background (`studio.Servers`; the first
free port of 7788-7799; log `Saved/Logs/server-KEY.log`), once (a second press while it starts is told so); refused
while a server holds the world. It runs as if started by hand (restarts onto builds, maintenance), and outlives the
studio.

**Play.** Atlas's ▶ Play opens, in the user's own browser, the game of the server running Atlas's world, at the address
the studio was reached by (`http://192.168.0.121:7788/` from another computer; `gameUrl`), not a playtest on the studio's
computer. The game server now names its port in its claim on the world (`application_name` "ratw_server pid N on HOST
port P", " local" when bound to 127.0.0.1; `Options::webPort`/`webBind`); `world_sync.server_holder` reads it (also in
`status()` as `serverHolder`) and `GET /api/atlas/play/server` answers `{play: {port, host, local}, why, canStart, build}`
(host set only when the server runs on another computer). With none running it says why ("No game server is running
... Start it here, or on the studio's computer: bash tools/live.sh server dev") and offers **▶ Start its server**
(`POST /api/atlas/play/server/start`), then waits for it. The old playtest of an exported copy stays, folded away as
"Or playtest a copy on the studio's computer". The DM's Server tab has **▶ Open its game** too.

**The local network by default.** `bash tools/studio.sh` serves 0.0.0.0 (`--host 127.0.0.1` for this computer only;
the LAN names it trusts as before, 863f66a) and prints the addresses. The game server through `tools/live.sh server`
and `tools/server.sh` binds 0.0.0.0 (`RATW_BIND=127.0.0.1` for local only) and prints its LAN addresses; scratch servers
(`--scratch`), servers with `--dev-identity` and `tools/play.sh` (this machine's playtests) stay on 127.0.0.1, as do the
test harnesses (they run `ratw_server` directly). The game server has no Host check that refuses another computer: a
WebSocket's Origin must match its Host (`web::originAllowed`), which a page served by the server always does, and the
client connects to the page's own host (`gameUrl` in `Client/src/net/connection.ts`).

Start, for the user (from the repository; the studio and the server are separate processes):

    bash tools/studio.sh               # Atlas and the DM: http://THIS-COMPUTER:8765/ (both on DEV)
    bash tools/live.sh server          # DEV's game server: http://THIS-COMPUTER:7788/ (or the DM's ▶ Start)

**Found running DEV for real: its save didn't load.** The first real DEV server ran, saved on a new build and exited
75; `live.sh` started it again on the new build, and the restore was refused ("Invalid saved society or money ledger"):
a saved resident's task may be at most 40 characters, and an apprentice's is "apprenticed: " and its master's title
("apprenticed: cooking for the King's household", 45). Every restart of DEV after its first save would have failed. The
limit is now `MaxTaskText` (160; `society_tests`: a long apprentice's task restores, one past the limit is refused);
with it the saved checkpoint loads (checked on a scratch server). DEV's game data was put back as it was before that
run (it had no checkpoint; one stale `game.characters` row, which the first real save will drop, as any fresh start's
does).

Not changed: the DM accounts stay in PROD's database (`dungeon_master.ACCOUNTS`), so signing in reads and writes
`dm.sessions` there while PROD's game server is shut down.

Tests: `test_world_db` (the defaults, any world served, a dev world served and switched back, the DM's own choice,
`dm` reserved), `test_studio` (`ServedDevRouteTests`: the DM on DEV, import and rollback refused with why, Rebuild,
starting a server once, Play's address and its refusals; the DM's picker apart from Atlas's; the PROD flow as before with
the live choice on PROD), `test_world_store` (a whole-world save keeps a newborn), `test_world_sync` (a real server on a
dev world restarts on a new build with 75 and names its port; a scratch one stays), `Editor/src/lib/sync.test.ts`
(`gameUrl`).

### A generator's write keeps its map when the layout is the same (2026-10-31)

Branch `r2-class-jobs-2`. The user: "Yeah fix that map identity problem." The Ridgemere 2.0 generator stamped a **new
map identity on every write**, even when it only moved people: doc 67's jobs by class (revision 51 to 53) gave DEV
`map-20261031-2e244e4d06e9` although its layout hadn't changed, which would have refused every pull and import with a
world still on `map-20261031-6b080cecb7f6`.

- **The layout's fingerprint** (`world_sync.layout_fingerprint(project)`): a SHA-256 of the cells (id, place, size,
  terrain, heights), the interiors (the same, with their world place), the links (id, kind, open, both ends) and the
  spawn. Not people, names, descriptions, lighting, weather or claims.
- **The stamp records it:** `admin.settings` `map` is `{id, name, made, by, how, layout}`. `world_sync.stamp_for_layout`
  (the generator's write, as the owner, in its transaction) keeps the stamp when `map_for_layout` says so and records
  the layout on it, or stamps a new identity carrying the layout:
  - `auto` (the default): keeps a stamp whose layout is this write's; anything else is a new map (a stamp from before
    fingerprints can't be shown to be the same map, so it gets a new one, as before);
  - `python3 -m worldgen.ridgemere2 write --same-map` keeps the identity whatever the layout; `--new-map` stamps a new
    one whatever it is.
  The write says which: "the same map, ... (its layout unchanged)" or "a new map identity, ... (the layout changed ...)".
- **Still only the generator's:** Atlas and the DM never stamp, and an Atlas edit that changes the layout (a new cell)
  keeps the identity as before ("Why this design", above): the fingerprint only decides whether a *generated* map is
  new. `world_sync.py map KEY --new` is unchanged.
- **DEV restored:** its stamp was put back to the one the DEV switch gave it, `map-20261031-6b080cecb7f6`, "Ridgemere 2.0:
  the city (doc 67)", made 2026-10-31 17:30 UTC, exactly as recorded in `Saved/Backups/ratw_ridgemere2-before-class-jobs.dump`
  (as the owner, `world_sync.stamp_map(conn, ..., identity=...)`). The layout really is the same since then: the
  fingerprint of DEV at revision 51 (before the jobs by class) is that of DEV now,
  `sha256:0bdf406d...7efb77`. The next write (doc 67, "Rag Market first", revision 53 to 55) ran with `--same-map`,
  which recorded that layout on the restored stamp; a write from DEV now keeps it by `auto`.
- **Tests:** `test_world_sync.LayoutTests` (the fingerprint ignores people, text, weather and link order and sees
  terrain, a new interior and a moved link end; `auto`, `same`, `new` and a stamp without a layout) and
  `MapTests.test_a_generators_write_keeps_its_map_while_its_layout_is_the_same` (on scratch worlds: a first write stamps,
  a people-only write keeps, `--same-map` keeps a changed layout and records it, a changed layout is a new map,
  `--new-map` always is).

### The live world's truth: NPCs and markets in conflicts (2026-11-01)

Branch `r2-live-truth`, from `ridgemere2` (0ac680c). The user, 2026-11-01: **"We need to fix that Atlas issue so that
when we do get around to updating maps and porting them from dev to prod, that prod retains the truth of NPC's and
market data. This should be a conflict resolution thing."**

**The issue.** Since doc 68's arrivals from the Country, the game server writes an authored NPC's row: an unplaced NPC
who comes in for an empty post is given a home (`Game::makeResidents`, `UPDATE live.npcs SET home_area ...`). Births
insert rows (origin `runtime`), as they did. None of it raises the world's revision or goes through the edit log, so an
Atlas that read the world before the arrival still held the NPC unplaced: a whole-world save
(`world_store.save_world`: the generator's write, `apply_terrain.py`) put the old value back and unplaced it again; the
NPCs & areas page (it sends an NPC whole) would too; a keyed edit was refused as "someone else's", losing the author's
own change. An import or pull had no idea which side's fields were the live game's.

#### Who owns what

The live game owns its NPCs' live state and the market; Atlas, dev authoring, owns the map, the buildings and the NPCs
as written. In the tables both write, by column (`world_store.GAME_COLUMNS`, `GAME_FIELDS`; `world_sync.owner`):

| Table | The live game's | Atlas's (authored) |
|---|---|---|
| `live.npcs` | the home and the post: `home_area/x/y`, `work_area/x/y`, `evening_area/x/y`; what it carries: `purse`, `herbs`, `meals`; `age`; `origin`. **A row the game made** (origin `runtime`: a birth) is the game's whole | `name`, `role`, `description`, `greeting`, `personality`, `backstory`, `work_label`, `voice`, `appearance`, `route_id`, `paid`, `hours_start/end`, `joinable`, `protected`, `wander_area`, `position` |
| `live.profession_slots` | its three places, `purse`, `herbs`, `meals`, `character_id` (the roster character a build gave it), `origin` | `name`, `profession`, `work_label`, `route_id`, `paid`, the hours, `position` |
| `live.characters` (the roster) | `status`, `assignment`, `origin` | `name`, `age`, `profession`, `profile` |
| `live.npc_relations`, `faction_members`, `factions`, `faction_claims`, `faction_relations` | rows the game made (origin `runtime`), whole | the rest |
| `world.*` (cells, interiors, terrain, doors, places, resources, chapters, the world row), `live.npc_voice`, `dialects`, `figures`, patrol routes and posts, NPC areas, spawn rules, `business_settings`, story visitors | | all |

And the game's alone, in tables no save of Atlas's, no pull and no import ever writes:

| Where | What |
|---|---|
| `game.checkpoints` (the save) | households and their purses, marriages and the relations the game made, births owed, every till, its stock and shelves, the drifting prices and smoothed sales, the wage steps, the toll and alms moved, the treasury's level, the loop (`society.loop`), each resident's post (successions, arrivals) |
| `live.npc_state` | alive or dead (the DM's kill and revive too), cash, stock, position, task |
| `live.economy` | the rules' current values (`rules`, `services`, `seasons`, the comforts), the wages, the gate toll, carriage, subsidies, the Country's switch and reserve, the store room's switches, the treasury's start: each world's own (doc 67 and 68), never synced |

#### How a game change is detected: migration 0059, the game's own log

`Database/migrations/0059_game_changes.sql`: **`live.game_changes`**, written by a trigger on `live.npcs`,
`live.profession_slots`, `live.npc_relations` and `live.faction_members` whenever the role writing is `ratw_game` (the
game server): the table, the row's key, insert, update or delete, the row before and after (JSON, without
`updated_at`), and the world's revision then. Spawned NPCs (`spawn_id` set) are left out, as the sync leaves them out.
Read by the editor, the publisher, the game and the DM; written by the trigger only (`SECURITY DEFINER`).

**Why a log the database keeps, and not a revision the game raises, or a marker column.** Raising the world's revision
on every game write would refuse every whole-world save outright whenever the game placed anyone (the generator's write
takes minutes) and say nothing about which row or field. A "changed by the game" marker on the row says that it changed
but not what it replaced, which is what tells "the game moved it" from "you moved it". The trigger catches every write
of the game, whatever code makes it (an arrival today, a marriage or a death written tomorrow), with no change in the
C++; each change keeps the value it replaced, the base of a three-way merge. The game's writes are rare (arrivals and
births, a few a year of game time), so the log stays small; it is never pruned.

For pulls and imports nothing new was needed to *detect*: the three-way comparison already sees the live game's
changes as live's. What they lacked was the ownership above, for the defaults.

#### Atlas's saves on a world the game changed since it opened

- **Never lost under a save.** The rows the game writes (`live.npcs`, `live.profession_slots`) are read `FOR UPDATE`
  by every save of Atlas's: a keyed edit's (`Batch(..., lock=True)`) and a whole-world save's (`stored_rows(...,
  lock=)`), the game's log read after them. A game write made meanwhile waits for the save (an arrival's `UPDATE ...
  WHERE home_area IS NULL` then runs on what was saved) instead of being overwritten by a row read before it. The
  game's writes are rare and single statements; a save holds them for its own length only.
- **Keyed edits** (the Workshop, `live_edit.apply_edit`). When an edit's `before` isn't the row now and every field
  that differs is one the game changed (its log's last `after` is the value now), it is not refused: the edit's own
  changes are made on the game's row (`merge_games_change`), and the reply's `merged` gives the editor the row as
  written. Where the edit changed a field the game changed too, differently, the batch is refused with those fields as
  a conflict (`game: true`, each field's base, mine and the game's); **the game's stays** until the editor chooses.
  Another editor's change is refused as ever.
- **The NPCs & areas page** (`DungeonMaster.save_npc(who, target, person, saw)`) sends the NPC whole **and the NPC as it
  loaded it** (`before`; `null` for a new one), and saves as a keyed edit on that: a field the game changed since and
  the page left alone keeps the game's (the reply's `written` is the NPC as saved), one the page changed too is refused
  with a 409 naming it ("The game server changed Fennel since you opened them: home (yours ..., the game's ...)"),
  another editor's as ever. What the page changes is its own, even back to a value the game once replaced. (Without
  `before`, from an older page or a newcomer, the NPC is written whole as before.)
- **A whole-world save** (`world_store.save_world(..., seen=, choices=, outcome=)`; `keep_the_games_truth`): for each NPC
  or slot the game changed since the project was read, each game-owned field (a place, the purse, the stock, the age)
  the save would change back is the game's again where the save still holds a value the game replaced. **With
  `seen`** (`load_world(..., with_seen=True)` gives it, read in the project's snapshot) the reader saw the value before
  the game's first change after `seen`: a field the save changed from that, where the game changed it too, is a
  conflict, **by default the game's**. **Without `seen`** the game's changes at the project's revision or later may be
  before or after the read (the game never raises the revision): a field holding any value the game held meanwhile is
  untouched (the game's); one the save changed to something else is **its author's** (a generator re-homing everyone
  on purpose, after reading the arrivals), listed as a conflict for the caller. `choices` `{"live.npcs/ID": "atlas" |
  "game" | {field: ...}}` choose; `outcome` gets `{kept, conflicts}`, and `truth_lines(outcome)` prints them. Every
  value of the game's put back is checked against the save's map and limits (`map_editor.resident_problem`, the checks
  `check_project` makes, one field at a time): a place the save's map hasn't (a cottage the generator took away), a
  solid tile, a count out of range is not put back, the save's stands, and the conflict says why (`problem`). It works
  on the save's people and slots and their rows, after the project was checked. A birth the game took back is not
  brought back. The Ridgemere 2.0 generator, `realm.py`, `towns.py` and `apply_terrain.py` load with `with_seen`, pass
  it, and print the outcome.
- **The heartbeat** (`live_edit.sync(..., game_since=)`; `load` gives `gameSeq`): the game's changes come to every open
  Atlas as edits of their keys (`person:ID`, `slot:ID`, editor "The game", the row as it is now), so the map shows an
  arrival at home at its next heartbeat (1.5 s) and a later edit starts from it. Each is read in one snapshot with, as
  its `seq`, the newest edit its value holds (edits are numbered as they commit, under the world row's lock), so the
  editor's rule "a change older than the newest I hold of the key is dropped" never drops a game change whose value
  has the editor's edit in it. The newest the editor holds of a key (`startLive`'s `newest`) is its own saved edits'
  and every change it applied from the feed: an edit as old as a game change applied is in that change's value already
  (dropped), and another tab's edit older than one applied never replaces it. A game change to a key with a batch still
  unsaved in the editor's outbox **waits for it** (`held`): the editor's own later edits are diffed against its own
  values; once the batch is saved, the newest stands (the batch's `merged` reply, the game's change if it came after,
  or another tab's edit come meanwhile, over both), and a refusal's values replace it.

**What the user sees in Atlas.** Most of the time nothing: their edit saves and the NPC stays where the game put it.
When both changed one field, the dialog **"The game server moved what you changed"**: for each NPC and field, what
they saw, theirs and the game's, with **The game's** (picked) or **Yours**; **Keep the game's, save the rest** or
**Save with my choices** (the edit made again on the values there now: `againWithChoices`, `store.saveAgain`; each
entity of the batch, the contested one or not, with only the fields the editor changed, so one the host had merged
keeps the game's). Closing the dialog (Esc, the backdrop, ×) keeps the game's and saves the rest, as its first button.
One refusal is shown at a time: one arriving while another is open waits its turn (`queueRefusal`). The map shows the
game's values meanwhile.

#### Pull and import: the live game's truth by default

- **Defaults** (`world_sync.default_choice`, `field_default`): a conflict whose every field changed on both sides is
  the game's takes **live's** for those fields (each other field the side that changed it: dev's new words come across,
  PROD's placement stays); a row dev removed that live's game changed, or a row the game made, is **live's** whole. A
  conflict over an authored field changed on both sides still needs a choice. With no choice sent, the default is taken
  (`defaulted` in the result); the plan lists every conflict with its `default`, each field with its `owner` and
  `default`, and `defaults` by id. **A place is one field**: its area, x and y are never half dev's and half live's
  (`place_columns`). A choice on any of its columns is the place's, one the two sides happen to agree on too (shown as
  changed on both, as the place); two sides chosen for one place are refused ("a place is one field"). The screens
  pick the three together (`lib/sync.ts` `pickField`, `placeGroup`, and **The game's truth**). A field's choice is
  "dev" or "live", whatever the field: anything else (a place `{cell, x, y}` on a column, a list, a side for a field
  that doesn't differ) is refused, naming it, never ignored.
- **Rows hanging on an NPC go with it** (`world_sync.NPC_DEPENDENTS`: its voice, its family both ways, its faction
  memberships, its business setting). Where one side removed an NPC the other changed (a conflict), the rows the
  removal took with it are not removed on their own: they go as that conflict ends (`with_their_npcs`; the plan marks
  them `follows`), whichever side removed her and whichever way the write goes. Where she stays on the side written,
  the other side's removals of them are left out (`held`); where that side had removed her and she comes back, its own
  removals are undone (`back`: written with her, as the other side and the base have them; a faction member only while
  its faction stays too). Dev removed Linden, PROD's game moved her: an import keeps her in PROD **with** her voice,
  spouse and faction, an Update from live brings her back to DEV with them; chosen dev's, she goes and they with her,
  either way. PROD removed her and DEV's is chosen: she comes back to PROD with hers. An import whose every change ends
  as live has it (Linden kept, dev's only change her removal) is refused, saying so: "every change made in dev ends as
  live has it ... Update from live brings dev level with it".
- **What was there already, checked**: DEV's authored changes (map, buildings, NPC definitions) come across as dev's
  changes; PROD's game's (arrivals placed, births) are live's changes, kept by an import and taken by a pull. A birth is
  an addition on the live side (`added` live, `absent` in dev): pulled into dev, then in the base, never removed by an
  import, and its later changes default to live's.
- **The market** is never compared or written: `game.checkpoints`, `live.npc_state`, `live.economy`. The import's plan
  now says where the two economies differ (`economy`: the columns, both values) and warns "DEV's economy settings
  differ from PROD's (...): PROD keeps its own (the live game's; never imported)."

**What the user sees in the DM's Import from dev and Atlas's Update from live.** The conflicts start picked as their
defaults ("starts on the game's"; a **game** tag on each of the game's fields in the base / dev / live table) and the
user can change any to dev's, whole or field by field; **The game's truth** puts every field the live game changed
(live alone or both) back on live's, in every conflict, a whole-row dev's pick turned field by field, and keeps the
user's choices for the authored fields (`gameTruthPicks`; a place whole, its column the two sides agree on too, so an
earlier pick of dev's place is never left half). Only authored fields changed on both sides are left to choose
(`lib/sync.ts` `defaultPicks`, `buildChoices`). A row removed with its NPC says "goes as linden's conflict ends (kept
if linden is)", on an Update from live as on an import.

#### A map-replacing import: NPCs whose places the new map hasn't

`--replace-map` (the DM's "Replace PROD's map ..."): the map is dev's whole as before, live data is compared three ways
as before, and nothing live has is removed. What changed: an NPC or profession slot whose home, work or evening place,
as live has it (or as its conflict ends by default: each field the side that changed it, so dev's move of her into the
new map counts), isn't on the new map is **a conflict needing a decision for each such place** (`gone`;
`replacing(items, areas)`), never unset unasked as before. Per NPC: **Unplace** (`"unplace"`: the row as it ends by
default, each field the side that changed it, with each such place unset; it keeps its name, talk, relations and
memories), **dev's** (`"dev"`, its whole row, offered only when dev has the row: `wholeSides`), or per place `{"home":
"dev" | "unset" | {"cell", "x", "y"}}` (dev's place, unset, or **a new place** on the new map; the plan lists the new
map's places as `areas`). On a row both sides have, an authored field both changed still needs its choice alongside
(`{"home": "unset", "greeting": "live"}`): "unplace" alone is refused for it, naming the field, never settled unasked,
and the screen asks for it ("and fields to choose") before it lets the import go. Choosing live's for a place that
isn't there is refused, and the import is refused while any is left. NPCs keep their live state wherever their places
still exist.

**No base: the real port.** DEV's new map imported into PROD has no base of PROD (PROD was never DEV's upstream).
Compared against live as it is, every difference would count as dev's and DEV's rows overwrite PROD's whole. Instead the
base is live as it is but for each field of the live game's that differs on a row both have, whose base is not known
(`world_sync.baseless`, `UNKNOWN`; the plan shows no base for it): an authored difference is dev's (DEV's new words
come across), a game-owned one a **conflict, live's by default** (PROD keeps where its game put everyone and what they
carry) **and dev's when chosen** (DEV's generator re-homed someone on its new map, in a cell id the old map has too:
"Reused interiors keep their IDs"), and where the new map hasn't the place, the decision above, dev's place offered.
(The first fix took the game's fields as dev has them for the base, which made each such difference live's outright:
DEV's re-homing dropped, unchoosable.) A place kept on the new map is still checked tile by tile when the release is
compiled (`check_project`): one on a solid tile refuses the import, naming the NPC, until dev's place is chosen for
it. The plan's base says "none (live as it is now; where the live game's fields differ, not known)".

Let go without a decision, as before: spawned NPCs' places (each server's own), the old map's patrol posts, territory
claims and NPC areas. The release's `letGo` now counts the NPCs and slots the user unplaced (`npcs`, `slots`) and the
spawned (`spawned`). On the DM's screen: "not on the new map: home (cottage)" on the conflict, with Dev's / Unset / New
place (a list of the new map's cells and interiors, and the tile), and **Unplace the N without a place**.

**Kept**: maintenance and the game server's lock, the map identity and its refusals, and the refusal of a new map
while live lacks migrations dev has (with 0059 in DEV, PROD lacks `live.game_changes` too, and is told so). A same-map
import where dev removed a place live's game has put someone in is still refused by name (pull first, then move them in
Atlas).

**Round 1 of review (2026-11-01)** fixed, each with a test: the row locks above (a game write during a save or a keyed
edit was overwritten); a save without `seen` taking the author's re-homing for stale; the game's places put back
without a check, failing the whole save at commit on the map's foreign key; the NPCs & areas page reverting a value the
game had once replaced; the heartbeat's game changes tagged with an older edit number and dropped; game changes applied
over an unsaved batch; `againWithChoices` writing a merged entity's stale fields; the game-conflict dialog dropping the
batch when closed, and a second refusal replacing the first; a place half dev's and half live's, or refused, from
per-column picks; unplace dropping dev's own changes and settling authored fields unasked; the screens offering dev's
for a row dev hasn't, and calling a homeless conflict ready while an authored field was unchosen; **The game's truth**
leaving a whole dev's pick over the game's fields; a no-base map import overwriting PROD's live state; an NPC kept
losing her voice, family and factions to dev's cascade.

**Round 2 of review (2026-11-01)** fixed, each with a test: an Update from live bringing back an NPC dev removed
without her voice, family and factions (then saved as the base, so the next import removed them from PROD unwarned),
and the same for an NPC PROD removed and an import chose DEV's (`with_their_npcs`' `back`); an import whose only change
ended as live has it refused as though dev had changed nothing; a no-base map import taking every difference of the
live game's as live's outright, DEV's re-homing dropped and unchoosable (`UNKNOWN`); a malformed side in a
field-by-field choice failing with a TypeError, or ignored, instead of refused by name; **The game's truth** leaving a
place half dev's where one of its columns was the same on both sides; a held game change, or a batch's late reply,
applied over another tab's newer edit, and another tab's edit applied over a game change that already held it.

**Migration 0059** is written and applied only to the test suites' throwaway worlds; **not applied to DEV or PROD**
(`python3 tools/world_db.py migrate --db dev` when the map work allows). Until then DEV behaves as before.

**Tests.** `tools/test_world_sync.LiveTruthTests`, on throwaway worlds with the game's writes made as `ratw_game`: a
whole-world save after an arrival keeps the placement and Atlas's own edit, conflicts where both changed (the game's by
default, Atlas's chosen), and without `seen`; keyed edits merged on the game's row, the heartbeat bringing the game's
change, a conflict naming base / mine / game's and the game's kept, saved again with "mine", another editor still
refused, the NPCs & areas page's whole NPC; an import keeping PROD's arrival, birth, save, NPC state and economy
settings while DEV's authored changes come across (and DEV brought level); conflicts defaulting to live on pull and
import (a moved home, a removed NPC the game moved) and dev's when chosen; a birth as a live addition, in the base,
never removed, the game's whole; a new map listing the NPCs whose places it hasn't (a birth in the removed cottage,
someone's evenings, a home both moved), refusing live's, and taking unplace, dev's and a new place.
`MapTests.test_an_import_of_another_map...` now chooses unplace for its two. Round 1: a save without `seen` keeping its
author's re-homing, the game's places off the saved map or on a solid tile not put back, a game write during a save and
during a keyed edit waiting for it (threads, the save paused after its read), the heartbeat's tag, an NPC dev removed
keeping her voice, family and faction in PROD (and losing them when dev's is chosen), a no-base map import keeping
PROD's arrival and purse; `ThreeWayTests` for a place chosen by any column and never half, unplace and the fields both
changed, rows following their NPC; `test_dungeon_master.NpcTests.test_the_page_saves_over_what_it_saw_...`.
`Editor/src/lib/sync.test.ts` (defaults, places, `pickField`, `wholeSides`, `allPicks`, a homeless row's authored
fields, `gameTruthPicks`) and `live.test.ts` (`againWithChoices` on a merged key, `queueRefusal`, a game change held
for an unsaved batch). Round 2: an NPC dev removed and live's game moved coming back to DEV with hers on an Update from
live (and the import after keeping them in PROD; dev's chosen, they stay gone), an NPC PROD removed coming back to PROD
with hers when DEV's is chosen, the import of nothing but such a removal refused with what to do; a no-base map import
listing the game's differences as conflicts with no base shown, live's by default and DEV's re-homing taken when
chosen; `ThreeWayTests` for rows coming back with their NPC (a member of a faction removed too not) and malformed
sides refused; `sync.test.ts` for The game's truth on a place with a column the same on both; `live.test.ts` for a
change older than what the tab holds (a held game change, a late reply, an edit in a game change's value).
