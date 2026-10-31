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
