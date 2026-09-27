# World database: DEV and PROD in PostgreSQL

Status: steps 1–8 implemented 2026-09-25: Interiors workspace, database setup
and schema, the editor editing the one world live in DEV, expanding the world,
Push to live, the game server on PostgreSQL, NPCs as live data, and the game
save and the Storykeeper as PostgreSQL tables. Next: planning the PROD/LIVE
manager (a Dungeon Master tool for live players, NPCs, spawns and events). This replaces the file-based storage in `12-map-editor.md`.

## Goals

- One world, stored as rows in PostgreSQL rather than in local files. Everyone
  edits it live in the editor, every action saved as it happens; the game server
  loads from it.
- Two databases with the same schema: **DEV** (authors edit and playtest here) and
  **PROD** (the live game reads this). Nothing reaches PROD except through
  **Push to live**.
- The world can grow in any direction. There is no fixed canvas size.
- The player's client still receives only what it perceives in its current cell or
  interior, plus a minimap outline of places it has discovered.
- Patrol routes, NPCs and other simulation data are database rows the editor
  can place and edit, not part of the world geometry, so the server can change
  them freely at runtime.
- All game-server state moves to PostgreSQL too: accounts, player characters,
  NPC state and memories, relationships, map memory. So does the Storykeeper's
  data. SQLite is retired.

## Two kinds of data

| | Authored world content | Live simulation data |
| --- | --- | --- |
| Examples | terrain, cells, interiors, doors, places, resources, chapters, minimap | NPCs, schedules, jobs, patrol routes and their posts, NPC areas, spawn rules, factions and their claims, economy, memories |
| Who writes it | the editor (DEV only) | the game server; the editor may insert and edit rows in DEV |
| Reaches PROD by | Push to live (replaced as one release) | Push to live adds *new* NPCs, jobs and characters only; once an NPC exists in PROD, PROD owns it and pushes never change or remove it. DEV copies the live NPCs back after each push |
| Versioned | yes, every push is a release | no, it is current state |

Live data refers to authored content by stable IDs (a place, a cell, an
interior). A push that would leave live data pointing at something removed is
rejected with a list of what still refers to it, the same way the editor refuses
to delete an interior with people inside.

## Storage layout

Terrain stays a continuous landscape, as authors draw it today, but it is stored
in square **chunks** keyed by chunk coordinates rather than as one canvas. Chunk
size is a per-world setting (`worlds.chunk_size`, 8–1024 tiles, default 128);
chunks that are plain ground are not stored.
World cells remain rectangles over that landscape. This keeps cut, split and
merge as metadata-only edits (terrain never moves) and makes expanding the world
just "add chunks".

Core tables (every table has `world_id`):

| Table | Holds |
| --- | --- |
| `worlds` | id, name, spawn, settings, current release number |
| `terrain_chunks` | chunk x, y, size; size×size glyphs and sparse elevation (`text` + `jsonb`) |
| `cells` | id, name, description, tile rectangle in world coordinates, z, outdoors, weather, lighting, region and Chapter |
| `interiors` | id, name, description, size, glyph rows, elevation, lighting, overview position |
| `links` | doors, passages, stairs: kind, name, open, both endpoints (cell or interior + local tile) |
| `places` | named anchors: beds, counters, work posts, patrol waypoints (cell or interior + tile + kind) |
| `resources` | resource nodes such as the herb patch |
| `chapters` | story arcs |
| `minimaps` | small per-cell image, regenerated on save |
| `releases` (PROD) | number, pushed at, pushed by, note, and a full snapshot of the authored content for rollback |
| `admin_settings` (PROD) | publish password hash and similar settings |

Live data tables: `professions` and `characters` (the shared roster, formerly
`Data/Characters/roster.json`), `npcs`, `profession_slots`, `patrol_routes`,
`patrol_posts` (ordered tiles), `npc_areas`, `spawns`, `factions`,
`faction_claims` (a faction's painted tiles in a place, or the whole place),
`faction_relations` (with `faction_relation_log`), `faction_members`, `economy`.
Factions moved here from the authored tables in migration 0016 (Dungeon Master
phase 4): Atlas still edits their names, colors and which places they claim. These will grow a lot as NPC behaviour
develops. The design only fixes that they reference authored content by ID.

Game state: `game.checkpoints` holds the server's complete save as one versioned
JSON document per world, the same shape the SQLite `world_state` row has today
(`RatwGameMode::State()`). Porting the server to it first keeps the atomic save
and makes the switch small. After that, the parts move into their own tables one
at a time: accounts, characters, NPC memories, relationships, map memory.

Schema changes are numbered SQL migrations in `Database/migrations/` and applied
to both databases by `tools/world_db.py migrate`, so DEV and PROD never drift.

As built (step 2): schemas `world`, `live`, `game`, `admin`. IDs use the
`ratw_id` domain (the editor's ID rule). Cells and interiors share one ID space
through `world.areas`, which links, resources, places and all live data point
at. References into areas are checked at commit, so a transaction can replace an
area but cannot leave anything pointing at a missing one. The database itself
enforces that world cells never overlap (a GiST exclusion constraint), and
checks tile counts and glyphs. Access is granted per schema through default
privileges, so new tables need no grant statements.

## The editor: one world, edited live

There is exactly one world (a unique index on `world.worlds` enforces it). The
editor has no world picker, no "new world" and no Save: it opens the world and
every action is saved as it happens, shared with everyone else editing.

- `tools/map_editor.py` serves the editor and connects to DEV as `ratw_editor`
  (settings from `Database/.env`). The browser never sees database credentials.
  Several people can each run it against the same DEV database; the database is
  the meeting point.
- **Edits are keyed.** Each action becomes a batch of edits, one per thing it
  changed: `tile:X,Y` and `height:X,Y` (world tiles), `cell:ID`, `room:ID` with
  `rtile:ROOM:X,Y`/`rheight:…`, `link:`, `person:`, `slot:`, `route:`,
  `faction:`, `chapter:`, and `name`, `spawn`, `herb`, `economy`, `bounds`. Each
  edit carries the value the editor saw before and the value it wants
  (`Editor/src/lib/live.ts` builds them by comparing the world before and after
  the action).
- **Saving** (`tools/live_edit.py`): a batch is applied in one transaction with
  the world row locked, so writers take turns and the edit log `world.edits` is in
  commit order. If any edit's "before" no longer matches the database, the whole
  batch is refused and the reply names who last changed each of those things
  (from the log). Database constraints (no overlapping cells, nothing may point
  at a removed place, valid glyphs) are checked at commit.
- **Following others.** Every 1.5 s each editor sends a heartbeat (`world.editors`:
  name, color, workspace, view, pointer, what is selected) and receives every
  edit after the last one it saw, plus who else is online. Other people's edits
  are applied to its copy and briefly outlined in their color; their pointers show
  with name tags.
- **Conflicts** show as a popup naming the other editor and what they changed;
  the editor's copy then shows the current version. Undo sends the inverse of the
  editor's own last batch, so it is refused rather than overwriting later work
  by someone else.
- **Coordinates and atlas v3.** There is no canvas: each world cell holds its
  own ground (`terrain` rows and `heights`, like interiors) and sits at x, y in
  world tiles, anywhere within ±10⁹. Everything sent to the database is in world
  tiles, and the database's chunks were already keyed that way. The world
  overview's screen tiles are world tiles too, so adding or removing a cell never
  shifts anything. The world row's `min_x`/`width`… now just record the rectangle
  around the cells. Older v1/v2 files (one shared canvas) convert on load: each
  cell takes its ground from the canvas. New cells come in preset sizes and snap
  to their neighbours (`Editor/src/lib/cellPlacement.ts`). (World → Expand and
  the `bounds` edit were removed.)
- **Validation** no longer requires every tile to be in a cell; ground outside
  cells is empty (a note is shown only if painted ground is left there). An action is refused only for problems it
  introduces, so one person's unfinished work never blocks everyone else.
- **Edits cost what they change.** Cells an edit does not touch are shared,
  not copied, between the world before and after it (the model replaces only the
  cells it changes), so checks, the live-edit diff and redrawing skip them. On a
  test world of 4,000 cells of 64×64 (16 million tiles), a brush stroke takes
  about 50 ms end to end and another editor applies it in about 1 ms. The map
  caches each cell's ground as an image, patched row by row, and draws only the
  cells in view.
- Patrol routes, NPCs and profession slots are placed in the People workspace
  and saved as DEV live-data rows like everything else.
- **▶ Play** exports the current world and starts a playtest on it.

## Push to live

Implemented in `tools/publish.py` (the editor's **⇪ Push to live** dialog and
`python3 tools/publish.py` use the same functions). A push:

1. Checks the publish password against the salted scrypt hash in PROD
   `admin.settings` (initially `password`). Every attempt is recorded in
   `admin.publish_attempts`; five failures within 15 minutes lock pushing and
   rollback for the rest of that window.
2. Reads one consistent snapshot of DEV and validates it completely (the same
   checks as Play/Export, including the game's current size limits). Any error
   blocks the push; the dialog lists them.
3. In one PROD transaction, holding an advisory lock so pushes take turns:
   makes PROD's authored content (`world.worlds` and every `world.*` content
   table) equal DEV's, writing only rows that differ; then applies live-data
   seeds. Named NPCs, profession slots and roster characters that already exist
   in PROD are never updated or removed (player interactions, events and deaths
   shape them there); only new ones are added. The profession catalog follows
   DEV. Patrol routes, NPC areas and spawn rules are add-only too (Dungeon Master phase 3).
   The economy is only created if PROD has none.
4. Refuses, with names, if any NPC, profession slot, patrol post or NPC area in PROD would
   stand in a cell or interior the release removes, then checks every deferred
   constraint before committing.
5. Records release N in `admin.releases` with the note, who pushed, a summary
   of what changed, and the exact world content it published; and
   `NOTIFY ratw_release, '{"world": ..., "release": N, "kind": "push"}'`, which
   PostgreSQL delivers to listeners when the transaction commits. The game
   server will listen for it in step 6 and reload at a safe point.

After a push commits, DEV **copies the live NPCs back** (`pull_npcs`): PROD's
named NPCs and profession slots replace DEV's through the live-edit log (so open
editors see them change; NPCs whose places or route DEV lacks are skipped and
listed), and roster characters, the economy and `live.npc_state` are copied.
DEV-only NPCs are left alone. The editor's **Copy NPC state from live** button
and `python3 tools/publish.py pull` do the same on demand.

**Rollback** re-publishes an earlier release's stored world content through
steps 3 (authored content only), 4 and 5, as a new release of kind `rollback`.
Live data is left as it is, and DEV is not touched, so the next push brings
DEV's newer world back.

Admins change the password with `python3 tools/world_db.py set-publish-password`,
which needs the database owner login; the publisher role can read the hash but
not change it. An admin page, and real admin accounts replacing the shared
password, are later work.

## The game server and client

Implemented (step 6). `bash tools/live.sh server prod|dev` starts the server with
`-RatwDatabase=prod|dev`; the connection string for the `ratw_game` role comes
from `RATW_DATABASE_URL`, which `live.sh` builds from `Database/.env`.

- **Builds.** The server does not assemble the world from rows. It loads the
  newest row of `world.builds`: the manifest and cell files produced by the one
  exporter (`tools/map_editor.py`), with seams, door arrivals and profession-slot
  residents already resolved. Push to live compiles one per release (and per
  rollback) in the same transaction as the release; `tools/world_build.py dev`
  makes one for DEV. The runtime's existing validated loader reads it from
  memory (`World::loadWorldFiles`), with the same checks as files on disk. Keeping
  one exporter means the game and the editor's Play can never disagree about a
  world; reading rows natively in C++ becomes worthwhile once the server streams
  regions of a very large world, and needs no schema change.
- **Saves.** Everything the server saved to SQLite (accounts, player characters,
  NPC state, conversations and memories, relationships, map memory, doors, the
  clock) is saved to `game.checkpoints` for the world every few seconds, as the
  same versioned document (stored as text, byte for byte). Step 8 splits it into
  tables. `-RatwWorld` playtests and `-RatwTown` keep SQLite files.
- **New releases.** The server `LISTEN`s on `ratw_release`. When a build newer
  than its own is announced it tells connected players, and once nobody is
  connected it saves and exits with status 75; `live.sh` restarts it at once, and
  start-up loads the new build and restores the save. Reusing start-up rather
  than swapping worlds inside a running server keeps one tested path for loading
  and restoring.
- **libpq** is loaded at run time (`Core/RatwPg.cpp`, `dlopen`), not linked: the
  engine's toolchain uses its own older C library, and a server that does not use
  the database never needs PostgreSQL installed.
- **NPC running state** (step 7). Every save also writes each NPC's position,
  age, task, hunger, fatigue, purse and goods to `live.npc_state`, in the same
  transaction and with the same timestamp as the checkpoint. At start-up, rows
  newer than the server's own checkpoint were written from outside (DEV copying
  PROD's) and are applied over the restored state: positions only onto open
  ground in a cell that exists, and purse changes balanced against the town
  treasury so money stays conserved. `alive` is there for deaths when the game
  has them.
- **The client** is unchanged: snapshots carry only what the player perceives in
  the current cell. The existing **World Map** is the minimap: it is built from
  each player's own exploration memory (part of the save), so undiscovered places
  are never sent. `world.minimaps` stays available for a pre-rendered map later.

## Deployment

- Local development: `python3 tools/world_db.py up` starts PostgreSQL in Docker with
  `ratw_dev` and `ratw_prod` databases and separate roles (editor, publisher,
  game server).
- Hosted PROD: a managed PostgreSQL instance later. Only the connection strings
  change.
- Backups: `pg_dump` on a schedule. `tools/world_db.py dump` writes a readable
  text form of a world for review and diffs, since database rows are not
  git-diffable.

## Steps

1. **Interiors workspace** in the editor. Done.
2. PostgreSQL setup: compose file, roles, the two databases, migrations tool,
   initial schema. Done: `Database/`, `tools/world_db.py`, `tools/test_world_db.py`.
3. Editor backend on DEV: load and save through the database; import
   `Data/Worlds/Greyfen/greyfen.atlas.json` once. Done: `tools/world_store.py`
   (conversion, minimal-write saves with revision checks, roster, import) and
   `DbWorlds` in the editor host (`serve --files` keeps the old behaviour).
   Until step 6, saving a world that has a `Data/Worlds` folder regenerates it,
   and roster saves refresh `Data/Characters/roster.json`, because the game
   still loads those files. The editor still sends the whole world on save;
   the host compares it with the database and writes only the changed rows.
4. Chunked terrain and **World → Expand**. Done, together with live editing:
   one world, keyed edits with conflict detection (`tools/live_edit.py`,
   `Editor/src/lib/live.ts`), presence, the New cell tool, and an editor canvas
   of up to 4096 tiles a side. Migrations 0005–0008. Play and Export still need
   the world to fit the game's current limits (256 cells and interiors, 262,144
   tiles) until step 6.
5. **Push to live** with password, releases and rollback. Done: `tools/publish.py`,
   migration 0009, the editor's Push to live dialog.
6. Game server on PostgreSQL through `libpq`: load the world from it, save to
   `game.checkpoints` instead of SQLite. Done: builds (migration 0010,
   `tools/world_build.py`), `World::loadWorldFiles`, `Core/RatwPg`, the
   `-RatwDatabase` mode and `tools/live.sh`. The editor no longer mirrors DEV into
   `Data/`; those files are an offline fixture. The minimap is the existing
   exploration-based World Map. The file formats remain as the build format, and
   `-RatwWorld`/`-RatwTown` with SQLite remain for playtests and offline tests.
7. NPCs, profession slots, patrol routes and economy as live-data rows, with the
   runtime reading them from the database. Fix patrol routes as part of this.
   Done: definitions are rows (compiled into each build); running state is
   `live.npc_state` (migration 0011), written and read by the server; pushes
   never change live NPCs; DEV copies them back. Patrol routes: the editor could
   not create one (a new route was made with no posts, which the model refuses);
   now the first click creates it with its first post. Guards walking routes
   through doors are covered by `Tests/town_tests.cpp`.
8. Split `game.checkpoints` into tables (accounts, characters, NPC memories,
   relationships, map memory), and move the Storykeeper service off SQLite.
   Done (migration 0012). `game.save_checkpoint` / `game.load_checkpoint` store the
   server's save document as one row per thing in `game.accounts`,
   `game.characters`, `game.npcs`, `game.map_memories`, `game.conversations`,
   `game.npc_memories`, `game.relationships`, `game.social_recent` and
   `game.social_sessions` (which list goes where, and what identifies an entry, is
   the `game.sections` table), writing only rows that changed; the checkpoint row
   keeps the world-wide rest. The server hands over and gets back the same
   document, so its validation and restore are unchanged; older one-row saves
   still load. Logins are readable by the game server only. The Storykeeper
   (`tools/dm_service.py`) keeps its state in the `dm` schema instead of its own
   SQLite file.

## Decisions

- Local PostgreSQL in Docker (port 5433) holds both databases for now; PROD
  moves to a hosted server later by changing `RATW_PROD_HOST`/`RATW_PROD_PORT`.
- The Unreal server links PostgreSQL's C client library, `libpq`. Unreal has no
  built-in PostgreSQL support, so the library ships with packaged server builds.
  A separate service can replace it later if one is needed.
- All game-server and Storykeeper data moves to PostgreSQL (steps 6 and 8).

## Open questions

- Where PROD will be hosted.

## Massive worlds (planned 2026-09-25)

The world has no size limit: it is its cells, which may lie anywhere, as far
apart as they like. The steps:

1. **Preset cell sizes with snapping.** Done: squares 32–256 and their wide and
   tall halves, snapping flush to neighbours or else to a 16-tile grid.
2. **Sparse world (atlas v3).** Done: every cell holds its own ground; no
   canvas, no edge. The editor, the Dungeon Master, validation, export, the
   database store and the live-edit protocol all work per cell. Older files
   convert on load.
3. **Edits cost what they change.** Done: shared unchanged cells, per-cell
   remembered checks, per-cell cached map images.
4. **Load only what is in view.** Still to do. Atlas and the Dungeon Master
   load every cell's ground at start. That's fine to tens of millions of tiles;
   beyond, the host sends cell outlines first and each cell's ground when it
   comes into view (and keeps edits flowing for loaded cells only). No schema
   change: chunks are already keyed by world tile.
5. **The game server loads cells on demand.** Done (2026-09-26):
   - **Builds are streamed.** `world.builds.files` holds only the manifest
     (`RATW_WORLD 3`): an `area` record per cell, `exits` records (which cells
     each cell's seams lead to), and the doors and stairs between places.
     `world.build_cells` (migration 0018) holds each cell's header (the cell file
     up to `grid:`, with a new `size:` line), its whole file, and its side of
     every seam. Cell rows are kept for the newest three builds.
   - **The server keeps every cell's header, and a cell's tiles and seams only
     while someone is in it or next to it.** `World::stream()` runs each tick:
     cells holding a player or NPC, and their neighbours, load from the database
     (one row); cells nobody has needed for two game minutes unload. Loading
     checks a cell's seams against its tiles.
   - **Routes work across unloaded cells.** NPC routes and remembered player
     routes plan over the cell graph (doors plus `exits`) and use actual seams
     only in the current cell, which is always loaded. Door lookups are indexed
     by cell instead of scanning every door each tick.
   - **The limits are gone for the server:** any number of cells (up to 4
     million areas), seams per cell not counted against the manifest's 65,536
     doors, world positions within ±10⁹. ▶ Play and file exports (a local game
     that loads every cell) keep the old 256-cell and 262,144-tile limits.
   - Measured on a real DEV server: a 906-cell, 3.7-million-tile world loads
     with 8 cells in memory and about 10 MB more than Greyfen alone; a resident
     commuting four cells into it loads at most 19 at a time.
   - Along the way: NPCs heading for a shared edge now aim just past it (a corner
     tile belongs to two edges) and take the nearest crossing, and cross only
     where their route leads out, so they no longer bounce between two cells.

