# Runs Against the World

A native Unreal prototype of a persistent, text-first roleplaying world inhabited by quadrupedal wolves. The terrain is a logical glyph tilemap; wolves move freely over it as an upright `W` with an orbiting `>` facing marker. Roleplay prose stays in the narrative pane.

## Requirements

- Unreal Engine **5.8.2** (Linux x64 is the verified development environment).
- CMake 3.16+ and a C++17 compiler for portable simulation tests.
- Python 3 for the local Atlas/Storykeeper services and verification scripts.
- A modern browser for Atlas Workshop and Storykeeper; Node.js is needed only for their model tests.

Set `RATW_UNREAL_ROOT` if your engine is installed elsewhere. The local default is `/home/martinb/Applications/UnrealEngine/5.8.2`.

## Build and play

An already-built Linux development package is available locally:

```bash
bash tools/run-packaged.sh play
```

It runs without opening the Unreal editor. To rebuild the package, use
`bash tools/package.sh`. The archive is under `artifacts/package/Linux/` and is
intentionally excluded from Git.

For editor-based development:

```bash
bash tools/build.sh
bash tools/play.sh
```

The default launch now opens **login → character selection → character creator**.
Register a local test account with a unique test password (at least 12 bytes),
create up to six characters, then select one to enter. The creator has five wolf
species, four age stages, three statures, sex, natural coat gradients and markings.
The same portrait appears on the character sheet and visible-character inspection;
map actors stay `W>` glyphs. Appearance and ownership are saved on the authority.

These are **trusted-local test accounts**, not public authentication. Native
credential exchange is permitted only in standalone play or over loopback on this
computer; it is blocked for remote peers until encrypted transport is implemented.
There is no password recovery or character deletion yet.

For two players, start the headless development server in one terminal, then two clients:

```bash
bash tools/server.sh
bash tools/connect.sh 127.0.0.1:7787
bash tools/connect.sh 127.0.0.1:7787
```

Sign in separately and select different characters. Saves are local SQLite files
under `Saved/`; pass `-RatwSave=/absolute/path.sqlite` to the server or standalone
game to isolate a session. Automated smoke tests use disposable saves. Legacy
development identities are available only with explicit `-RatwDevIdentity` on
both authority and clients; this bypass is for controlled tests, not real accounts.

The installed engine can host a headless authoritative server through its Editor
executable. The package also supports a separate headless **listen host** with no
local player character:

```bash
bash tools/run-packaged.sh server
bash tools/run-packaged.sh connect 127.0.0.1:7787
bash tools/run-packaged.sh connect 127.0.0.1:7787
```

Launch each command in its own terminal. The server scripts bind only to
127.0.0.1 by default. `RATW_BIND` can select another interface for controlled
development-identity tests, but remote account login remains deliberately blocked.
Do not publicly expose this development server.

`Source/RATWMUDServer.Target.cs` defines a stripped dedicated-server target, but
this installed engine explicitly rejects Server targets. That distribution
requires a compatible source engine. The tested packaged listen host is a useful
local development option, not a production dedicated-server release.

## Greyfen Crossing — testbed town

A small living town to walk through. It is currently the whole of the one
world in the database (see [World database](#world-database-postgresql)), so the
editor opens straight into it, and `bash tools/live.sh play dev` plays the
current DEV version. `Data/Worlds/Greyfen/` keeps a fixed copy for offline
testing, which `-RatwTown` loads; editor changes no longer update it. Play that copy with:

```bash
bash tools/play.sh -RatwTown
```

It also works with `tools/server.sh -RatwTown` (clients connect normally) and,
after `bash tools/package.sh`, with `bash tools/run-packaged.sh play -RatwTown`.
The town saves to its own `Saved/ratw-town.sqlite`, so it never touches the demo
save. The demo world refuses a town checkpoint (and the other way round).
`-RatwTown` cannot be combined with `-RatwWorld`.

| Place | Who lives or works there |
| --- | --- |
| Greyfen Crossing (walled outdoor square, fountain, stalls, herb garden) | Everyone by day |
| Tallow & Twine (general store) | **Wren**, shopkeeper: open 07:00–19:00 at the counter's east end; use **Trade** |
| Wren's Cottage | Wren's own house; she tallies accounts there in the evening and sleeps there |
| Greyfen Barracks (bunk bays, mess table, captain's room) | **Captain Harrow** and guards **Sloe** (day watch), **Birch** and **Tamsin** (night watch) |
| Birchwall Cottage | **Sorrel** (gardener) and **Linden** (water carrier) |
| The Long House (lodging hall, spare bay) | **Fennel** (stall keeper), **Rook** (carter), **Maple** (elder storyteller) |

Walk into a building's door to go in, and back out through the doorway to
leave (clicking a door and choosing **Enter** also works). Day guards rotate around nine posts every 15 game minutes, and the
captain watches the square. Night guards take over at 18:00. Off-watch guards sleep
in their bunks. Civilians work 08:00–17:00, gather at the fountain until 22:00, then
go home to sleep. Anyone hungry buys a real meal from Wren while the shop is open.
Wages come from the town treasury. Wren restocks from the town stores and pays
market dues back, so money stays conserved. A daily carter refills the stores with
goods, never money. Players can gather from the herb garden west of the square.

After editing that atlas file by hand, run `python3 tools/bundle_worlds.py` to
regenerate its game files.
Portable tests: `Tests/town_tests.cpp`.

## Atlas Workshop — world editor

```bash
bash tools/editor.sh
```

Open http://127.0.0.1:8765. The first run installs and builds the React UI
(Node.js required); later runs only rebuild when its sources changed. The
server listens on this computer only.

**There is one world, and everyone edits it live.** The editor opens straight
into the world in the DEV database, so start PostgreSQL first
(`python3 tools/world_db.py up`; it restarts with Docker after that). The first
time, it asks for your name and a color.

- **Every action saves at once**; there is no Save button. The title bar says
  *All changes saved*, *Saving…* or *Offline, retrying* (edits made while offline
  are kept and sent when the host is back).
- **You see each other.** Every 1.5 s the editor checks in: other people's edits
  appear on your map, briefly outlined in their color; their pointer shows with a
  name tag when they are on the same map; a dot per editor in the top bar tells
  where they are (click it to go there).
- **Conflicts.** If you change something someone else changed after you last
  saw it (two people painting the same tile in the same second, say), your change
  is not saved and a popup names who got there first and what they changed; the
  map then shows their version. Everything else in that action is not saved either.
- **Undo** (`Ctrl+Z`) reverses *your* last action only, and is refused the same
  way if someone has changed those things since.
- **The world is its cells, and it has no edge.** Each cell holds its own
  ground, and cells may sit anywhere within a billion tiles of 0,0, however far
  apart; a world costs only what its cells cost. The **New cell** tool (`C`)
  places a cell of a preset size: squares (32, 64, 128, 256) and their halves,
  wide or tall (32×16 … 256×128, 16×32 … 128×256), all multiples of 16. `X`
  (or ↻ Turn) swaps width and height. The ghost follows the pointer and snaps
  flush against the nearest edge of a nearby cell, closing the gap, while staying
  where you point along that edge (it doesn't jump to line up corners); dropped
  into a nook it meets both neighbours. With no neighbour in reach it sits on a
  16-tile world grid. Pointing
  into a cell is refused. Deleting a cell (select it, then `Delete`, or its
  inspector's Danger zone) takes its ground and the doors to it with it; a cell
  with people, posts, the spawn or the herb patch inside must be emptied first,
  and the last cell stays. Where no cell is, there is no ground. ▶ Play and
  Export (a local game that loads every cell) still need the world to fit 256
  cells and interiors and 262,144 of their tiles; Push to live and DEV servers
  (`tools/live.sh`) stream cells from the database as players and NPCs come near,
  so they have no limit (see the world-database design, "Massive worlds").

The editor has four workspaces (tabs at the top, or `Alt+1/2/3/4`). All of them
share a menu bar (File, Edit, View, World, Interiors, People, Tools, Help), `Ctrl+K` command
palette, undo/redo and a live Problems panel; `?` lists shortcuts. Picking a
tool from another workspace (e.g. `P` while in Map) switches to it.

**Map** — world terrain and places. Markers from People show faintly for context.
Opening an interior (from the explorer, or by double-clicking a door) moves to
the Interiors workspace.

- **Terrain:** brush, rectangle, line, fill, eyedropper and elevation tools
  (`1`–`0` pick terrain, `[` `]` resize). Painting crosses from cell to cell
  freely; ground outside every cell is left alone (the game shows one cell at a
  time). World → Re-cut the cells redraws the cell grid over the rectangle around
  them without moving any ground.
- **Buildings (U):** stamp a shop, house, cottage, lodging hall, barracks or
  tavern, with walls, a street door, a furnished interior and the door link.
- **Connections (D):** doors, passages and stairs between any two places. In
  play, walking into a door takes you through it (locked doors block).
  Double-click a door end in the editor to see the other side.
- **Player spawn (N)** and **resources (K):** the herb patch is the one spot
  where players choose **Gather** to collect herbs. It is a resource node with a
  shared stock that regrows daily (faster in spring), not terrain, so it is
  placed rather than painted; the game supports one per world for now.
- Cell details, lighting, territory, factions and Chapters.

**Interiors** — the insides of buildings, caves and other detached rooms. Each
interior is its own small map joined to the world by doors, passages or stairs.

- The explorer lists every interior with its size and door count, and the doors
  of the one that is open (with where each leads).
- Terrain tools (brush, rectangle, line, fill, eyedropper, elevation) and
  Connect (D) work as on the Map. Right-click to start a door or pick terrain.
- The inspector shows the open interior's name, scene description, lighting,
  size (resize keeps the top-left corner) and residents.
- **Interiors → New blank interior…** makes a walled empty room;
  **Interiors → Delete this interior** removes the open one and its doors. The
  Building tool (U) on the Map adds a furnished interior already connected.
- The workspace remembers which interior you had open.

**People** — who lives and works here.

- **Named NPCs (P):** specific characters designed for one place: role
  (merchant, guard, civilian), home, work place, evening place, hours, patrol
  route, appearance, voice, purse, and a personality/backstory for live dialogue.
  Drag their markers on the map to move them.
- **Profession slots (J):** jobs placed in the world (a gate guard, a stall
  keeper, a bandit hideout). The tool stays active, so click to drop as many as
  you like; `V` or `Esc` when done. Each is filled from the Characters roster.
- **Patrol posts (O):** a patrol route is an ordered loop of posts. **+ New
  patrol route** (or the post tool with no route selected) starts one with your
  first click on the map; keep clicking to add posts, then assign guards in the
  route's inspector. A guard on
  watch stands at one post and moves to the next every 15 game minutes (about
  2½ real minutes); several guards on one route are spread out. Guards without a
  route hold their work post. Click posts in walking order, then choose the route
  in a guard's or guard slot's inspector.
- **Town economy:** treasury, stores and daily deliveries that pay wages and
  stock the shops.

**⇪ Push to live** (top bar, or File → Push to live) publishes the DEV world to
PROD, which the live game reads. The dialog shows what will change (or the
problems in DEV that block publishing: the same checks as ▶ Play), takes an
optional note and the publish password, and makes it a numbered release. PROD's
terrain, cells, interiors and doors become exactly DEV's. **NPCs that already
exist in PROD are never changed by a push**: player interactions, world events
and deaths shape them there. Only new NPCs, profession slots and roster
characters are added; DEV's edits to live NPCs, and NPCs removed in DEV, stay in
DEV (the summary counts them as "kept"). Patrol routes, NPC areas, spawn rules,
factions and their territory claims, members and relations work the same way:
the Dungeon Master runs them in PROD, so a push only adds new ones. A push that
would leave live NPCs, areas or claims in a removed place is refused with their
names. After every push, DEV **copies the live NPCs back** (their definitions,
running state, roster characters, the town economy, their routes, areas and
spawn rules, and the factions), so the next edits start from what is really happening; **Copy NPC
state from live** in the same dialog, or `python3 tools/publish.py pull`, does it
any time. NPCs that exist only in DEV are left alone. **Release history** in the same dialog rolls PROD's
world back to any earlier release (as a new release; DEV and live NPCs are not
touched). Five wrong passwords in 15 minutes lock pushing for the rest of that
window. `python3 tools/publish.py preview|push|releases|rollback N` does the
same from a terminal.

**File → Download a copy** gives you the world as atlas JSON; **File → Import a
.cell file** adds a legacy cell as an interior. **Export** downloads a playable ZIP.
**▶ Play** exports a fresh copy into `Saved/Playtests/` with its own save and
starts the game on it (optionally skipping login with a test character).

**Characters** — the shared character roster (in the DEV database;
`Data/Characters/roster.json` is the fixed copy used by offline tests), used by
the world:

- Each character has a name, age, appearance, voice color, personality,
  traits, backstory, greeting and **job preferences** (never / would accept /
  good fit / ideal) for every profession in the editable **profession catalog**
  (guard, night watch, shopkeeper, innkeeper, smith, farmer, laborer, scribe,
  healer, bandit…). Each profession maps to the game's closest behavior until it
  gets its own AI (bandits currently behave like workers at their hideout).
- **✦ Generate** writes new characters with the LLM configured for live NPCs
  (`RATW Game/Saved/Config/RATWNPCAI.local.json`, or set `RATW_AI_CONFIG`). The
  API key stays in the local Python server. Nothing is added until you review,
  add and save.
- **Assignment is permanent.** When a world is exported, played or saved, each
  empty profession slot takes the free character who most prefers that job, and
  the assignment is written into the roster. From then on that character keeps
  that job, appears in exactly one place across all worlds, and holds that
  profession for life. Marking a character **dead** or **removed** frees their
  slot for the next export; they are never drawn again. Deleting a slot releases
  its character, who can only take another slot of the same profession.
  Characters who have ever worked can't be deleted, so their history is kept.
- Personality and backstory are sent to the live-dialogue model. The game keeps
  each NPC's memories of every player they meet in the world's save database.

Worlds are cells of at most **256×256** tiles; the game plays at most
**256 cells and interiors** today, **128 residents** and **256 profession slots**.
A custom world's default save is keyed by its manifest path; Play always uses a
fresh folder and save. Editing never hot-reloads a running game or migrates an
existing save. For UI development with hot reload, run `python3 tools/map_editor.py
serve` and `npm --prefix Editor run dev` side by side. See
[the editor design and workflow](Docs/Design/12-map-editor.md), [the format
contract](Docs/EDITOR_CONTRACT.md) and the [territory
contract](Docs/TERRITORY_AUTHORING_CONTRACT.md).

## World database (PostgreSQL)

Worlds, NPCs and game saves are moving into PostgreSQL, with a **DEV** database
the editor works in and a **PROD** database the live game reads. Nothing reaches
PROD except through a password-protected **Push to live**. The plan and the
current step are in [the world database design](Docs/Design/20-world-database.md).
The editor already works on DEV. The game still loads exported files until
step 6, so saving a world that has a `Data/Worlds` folder regenerates that
folder, and roster saves refresh `Data/Characters/roster.json`.

Requires Docker and the Python package `psycopg` (v3). PostgreSQL runs locally
on port 5433 so it does not clash with a system PostgreSQL on 5432.

```bash
python3 tools/world_db.py up        # start it; first run writes Database/.env with generated passwords
python3 tools/world_db.py migrate   # create or update the tables in both databases
python3 tools/world_db.py status    # migrations, row counts, publish-password state
python3 tools/world_db.py psql dev  # SQL shell (add --role editor|publisher|game)
python3 tools/world_db.py set-publish-password
python3 tools/world_store.py import  # one-time: copy Data/Characters and Data/Worlds into DEV (--replace to overwrite)
python3 tools/world_store.py list    # worlds in DEV
python3 tools/world_db.py down      # stop; data is kept in the ratw_pgdata volume
python3 tools/world_build.py dev     # compile DEV's world for a local DEV server (skipped if unchanged; --force)
python3 tools/world_build.py latest  # newest build in DEV and PROD
```

**Running the game on the database.** The server loads the newest *build* of
the one world (the compiled game files, stored in `world.builds`; every Push to
live makes one for PROD) and saves players, NPCs, memories and everything else
in the same database, one row per thing: `game.accounts` (logins, readable by
the game server only), `game.characters`, `game.npcs`, `game.map_memories`,
`game.conversations`, `game.npc_memories`, `game.relationships`,
`game.social_recent` and `game.social_sessions`, plus `game.checkpoints` for the
world-wide rest (clock, weather, doors, the town's books). Each save writes only
the rows that changed. NPC running state is also in `live.npc_state`:

```bash
bash tools/live.sh server prod   # the live server players join
bash tools/live.sh server dev    # builds DEV's current world (if it changed), then serves it
bash tools/live.sh play dev      # a local game window on DEV
RATW_PACKAGED=1 bash tools/live.sh server dev   # the same from the cooked package (tools/package.sh)
```

When Push to live publishes a new release, a running server tells anyone
connected, and once nobody is connected it saves and exits with status 75;
`live.sh` restarts it straight away on the new build with everything restored.
The server needs the PostgreSQL client library (`libpq`) installed on the
machine it runs on; it loads it at start-up. `-RatwWorld` (exported folders,
editor playtests, smoke tests) and `-RatwTown` still use their own SQLite saves.

The initial Push to live password is `password`; change it with
`set-publish-password` (it needs the owner login from `Database/.env`, so only
someone with access to the database server can change it). Schema changes are new numbered files in
`Database/migrations/`. Never edit one that has been applied, because
`migrate` refuses to run if an applied migration has changed.

| Role | DEV | PROD |
| --- | --- | --- |
| `ratw_editor` (Atlas Workshop) | reads and writes the world and live data | cannot connect |
| `ratw_publisher` (Push to live) | reads | writes the world and live data, records releases |
| `ratw_game` (game server) | reads the world; writes live and game data | same |
| `ratw_owner` | migrations and admin settings only | same |

## Dungeon Master — running the living world

Atlas builds the world; **Dungeon Master** runs everything alive in it, in PROD
(the live world, marked red) or DEV (rehearsal). The plan is in
[the Dungeon Master design](Docs/Design/21-dungeon-master.md); phases 1–4 are built.

```bash
bash tools/dungeon-master.sh        # http://127.0.0.1:8766 (local only)
```

Sign in with a Dungeon Master account. These are separate from player
accounts, and stored in PROD with their passwords only as salted hashes:

| Account | Role |
| --- | --- |
| `dm-admin` | everything, including DM accounts |
| `dm-master` | runs the live world |
| `dm-viewer` | sees everything, changes nothing |

`python3 tools/dungeon_master.py accounts create-defaults` made them and wrote
each one's password once to `Database/dm-accounts.txt` (owner-only, not in git).
Store them somewhere safe, delete that file, and change them with
`python3 tools/dungeon_master.py accounts set-password USER`. Five wrong
passwords lock that account's sign-in for 15 minutes.

- **Players** is a player-character manager: a sortable, searchable sheet of
  every character (status, age, place and position, strength, dexterity, wisdom,
  stamina, sneaking, hearing and scent skills) beside a map of where each was
  last saved. **Kill** and **Resurrect** go to the game server, which applies them
  within a second whether the character is online or not. A dead character lies
  where they fell and cannot move, act or speak until resurrected. Actions wait
  for a running server and expire after ten minutes; every one is audited.
- **NPC Management** shows the world (open any cell or interior) with each NPC at
  their spawn points: where they work, and home and evening when selected, never
  where they are walking. Place a new NPC on the map, edit everything about one
  (role, schedule, work label, spawn points, route, purse, appearance,
  personality), and save it to PROD or DEV; a running game server takes the
  change over within a second, and without one it applies at the next start.
  Kill, revive and delete are here too. Job holders (profession slots) can be
  killed and revived; their details are still edited in Atlas.
  The layer switch above the map edits what NPCs live by:
  - **Routes:** click posts on the map, reorder or remove them; guards take a
    changed route at once.
  - **Areas:** paint tiles (drag; right-drag pans) as *wander*, *spawn* or
    *plan* zones. Give a civilian a wander area (“Wanders in”) and they roam it
    in work hours and evenings instead of standing at one spot.
  - **Spawns:** keep a number of NPCs, copied from a named NPC, alive in an
    area, with a respawn time. The game server checks rules every ten seconds;
    the fallen are cleared and replaced by someone new.
  Once live, these belong to PROD: Push to live only adds new ones, and DEV
  copies PROD's back after each push.
- **Factions** lists every faction (cities, NPC factions, guilds, clans) with its
  territory, members and relations:
  - **Territory:** see who claims what (claims may overlap; a claim is not
    control). For an open place, a faction can *claim all of it*, *paint tiles*,
    or *give up* its claim. A running game server takes the change at once, and
    Atlas shows which factions claim each place.
  - **Relations:** a matrix of how each faction regards each other (-100 to 100,
    a stance from allied to at war, and a reason), with the history of every
    change. One save can set both directions.
  - **Members:** named NPCs with a rank.
  Factions made in Atlas on DEV are pushed like NPCs: new ones are added, and
  once live the Dungeon Master's version wins.
- **Story Creator and LIVE** are the next phases; after them, Atlas will also be
  able to plan NPC areas and spawn rules on DEV (phase 8).

## Storykeeper — separate local DM application (being retired)

The Dungeon Master is replacing Storykeeper; until its Story Creator is built,
Storykeeper keeps working as described here.

Storykeeper watches and directs a running authority; Atlas authors places.
Launch the native authority with an explicit **absolute private directory**,
then start the service in another terminal using that same directory. Replace
these example paths with private locations owned by your OS user:

```bash
bash tools/server.sh -RatwDMDirectory=/absolute/private/ratw-bridge -RatwSave=/absolute/private/storykeeper-playtest.sqlite
python3 tools/dm_service.py --exchange /absolute/private/ratw-bridge --state-dir /absolute/private/storykeeper --port 8780
```

Storykeeper keeps its campaigns, beats, events, audit log and statistics in
the `dm` schema of the DEV database (`--database prod` for the live one); the
state folder only holds the private session file. Start PostgreSQL first.

The bridge/state folders must have mode `0700`; newly created folders receive
that mode, and overly permissive existing folders are rejected. Open the `url`
stored in the private `/absolute/private/storykeeper/session.json` file in a
local browser. It contains a temporary bearer token in the URL fragment, not a
query parameter. Do not share or commit that file/URL. No player login is used;
restarting the service creates a new operator session.

The first slice includes an omniscient map/roster, campaigns and beats, Chapter
profiles/members, faction opinions, observed activity, fixed-UTC event scheduling,
and audited migration previews. Real effects are limited to announcements,
weather, physical relocation of eligible existing residents, and transfers of
existing money/herbs/meals. Armies, brigands, assassinations and faction collapse
are story plans with unavailable executors, not completed game mechanics.

Chapter sites and housing/jobs can be explicitly declared as operator overlays
where Atlas has no conflicting Chapter. These are not constructed buildings or
measured vacancies. Migration requires approval, uses a finite named population,
and applies source-claim resentment only after a fresh snapshot confirms arrival.
Opinions are operator records, not yet native NPC behavior. Automatic migration,
new jobs and self-sufficient player towns remain future work. The demo is grouped
as `demo_reach`; custom Atlas exports still contain no authored NPC population.

All edits become read-only when the native snapshot is stale. This is a
trusted-local, single-operator development tool: loopback HTTP, private files,
no public player RPC, no direct game-database writes, and no public/multi-admin
authentication. Use an isolated save for experiments; dispatched effects cannot
be cancelled as though they were never applied. See the
[Storykeeper design](Docs/Design/17-storykeeper-dm.md),
[service contract](Docs/DM_SERVICE_CONTRACT.md), and
[native bridge contract](Docs/DM_BRIDGE_CONTRACT.md).

The current Linux package includes the private bridge. Add the same
`-RatwDMDirectory` and isolated `-RatwSave` flags to
`bash tools/run-packaged.sh server` to use it without the editor. Run
`python3 tools/dm_smoke.py` or `python3 tools/dm_smoke.py --packaged` for an
isolated end-to-end check. Both paths passed the current 23-check suite;
[verification and limitations](Docs/DM_TEST_REPORT.md) and
[actual screenshots](Docs/SCREENSHOTS.md) are recorded separately.

## Calendar and settlement life

The demonstration now runs four-hour days, 365-day years, seasons and a natural
lunar sequence on accelerated **game days**, now confirmed. Night visibility
combines moon phase and weather. Characters age with the shared running calendar
even while logged out; birthdays notify the player and update the character
sheet on return. Server shutdown time does not currently advance the calendar.

Confirmed next requirements are player-chosen natural death with a persisted
mandatory deadline between ages 100 and 120, and sparse map combat effects plus
one collapsed/latest-action or expanded/full-perceived-log entry per encounter.
**Natural death and combat presentation are not implemented yet.** See
[lifespan design](Docs/Design/14-calendar-aging.md) and
[combat presentation](Docs/Design/18-combat-presentation.md).

Six residents follow deterministic needs and jobs independently of NPC chat.
Approach Rowan at the tavern counter and choose **Trade** to buy or sell herbs
and meals. Purse, stock and demand are finite. **Inventory** shows quantities
and lets you eat a meal; a visible herb patch in Juniper Yard supports **Gather**
from within reach. Resources feed the same gathering/cooking/trading chain used
by NPCs. Custom Atlas worlds do not yet author these demo NPCs or resource jobs.

Development settings (`-RatwDevTools`) expose next-day/next-year jumps and
seasonal weather for testing. These mutate the selected development save; use
a fresh `-RatwSave` for experiments. Normal clients cannot change the calendar.

```bash
python3 tools/society_smoke.py
python3 tools/society_smoke.py --packaged
python3 tools/aging_smoke.py
python3 tools/aging_smoke.py --packaged
```

The society check captures the graphical client and restarts its isolated save.
The aging check keeps a separate authority running across three client
connections to exercise logged-out aging and command-retry behavior.

See [calendar design](Docs/Design/14-calendar-aging.md),
[NPC/economy design](Docs/Design/15-npc-society-economy.md), and
[verification](Docs/SOCIETY_TEST_REPORT.md) for formulas, sources and limits.

## Controls

| Input | Action |
| --- | --- |
| WASD | Continuous movement |
| Click terrain | Intelligent path to position |
| Wheel over local map / Page Up / Page Down | Increase or decrease walking-to-sprinting pace |
| Shift-wheel / Ctrl-wheel over local map | Pan vertically / horizontally without changing pace |
| Hold Alt + move mouse | Preview a faded facing `>` while stationary |
| Alt-click (or Ctrl-click) | Turn gradually toward the point without moving |
| Click entity or door | Contextual action menu |
| Enter | Enter chat; while composing, send and return to movement |
| Shift+Enter | New line in a roleplay post |
| Escape | Leave chat while retaining draft; close a panel; otherwise cancel world travel |
| M | Local/world map |
| C / I | Character / inventory |
| L | Listen |

Use quoted speech mixed with `/sigh`, `/action`, `/pose`, `/sit`, `/lay`, `/stand` or `/me`. Unquoted ordinary text is spoken. `/me` declares a current state visible on inspection. Use `//` to write a literal slash word. Local OOC has its own channel.

During ordinary local navigation, walking into any connecting door, doorway or stair crosses into the connected cell and stops at its arrival anchor, beside the far door. A closed, unlocked door opens as you walk into it; a locked door blocks. Clicking a door and choosing Open or Enter still works. Crossing an unobstructed boundary exit also transitions and stops.

For longer journeys, open **World → Known Routes** and select a previously
visited cell. The wolf walks through remembered connections, pauses briefly at
each arrival anchor, and continues at the selected pace. Closed doors still
require **Open**. This explicit journey continues while writing or viewing
panels; ordinary local click movement still stops when entering chat. WASD, a
local destination, Stop/Wait, or navigation-mode Escape cancels the journey.

Dexterity determines top speed. The pace/stamina strip highlights sprinting and
shows recovery or drain: recovery continues during movement, a middle trot is
sustainable, and a full sprint spends stamina quickly. Crouching and exhaustion
limit speed without changing the selected notch. See [pace and world travel](Docs/Design/13-pace-and-world-travel.md)
for controls, tuning, privacy, and the cell-level destination boundary.

Moving from `/sit` first spends 0.65 seconds standing. Moving from `/lay` spends
0.45 seconds rising into a slow sneak (30% walking speed); `/stand` leaves the
crouch. Sneak skill limits how far away others can see you or hear your pawsteps;
their hearing skill and ear health affect detection. Speech still uses the chosen
whisper/speak/yell volume. See [movement and stealth notes](Docs/MOVEMENT_UPDATE.md).

Sight, hearing and smell are independent. Unseen wolves can leave a broad lavender
`~~` scent arc around your token without revealing their name or map position.
The wind label shows airflow (W→E carries western scents eastward); weather, nose
health and scent skill affect detection. **Smell** in the general-action row
describes what currently reaches you. This first pass covers live body scent in
the current cell, not lingering tracks. See [scent and wind](Docs/SCENT_AND_WIND.md).

## Weather and daylight

Weather is visible on the local map and affects the simulation: rain/snow mask
sound and scent and slow travel, fog conceals the distance, and night reduces
server-visible terrain and wolves. The shared clock runs a four-hour day with
gradual dawn/dusk. Indoors stays sheltered, but light is independent:
warm taverns can glow at night while staying clear, windowed unlit rooms darken
after sunset, and sealed unlit rooms remain dark even at noon. Soft glow and dark
edge fades follow the actual cell boundary without tinting the story pane. Atlas
Workshop's cell details expose artificial light, daylight access, and light tone.
These are whole-cell settings, not individual lamp/shadow simulation. Reduced
motion retains static weather cues without changing perception. For development
playtests, launch with `-RatwDevTools` and use Settings' weather/time/lighting presets;
ordinary players cannot change them. See [weather and daylight](Docs/Design/10-interactions-environment.md).

If Atlas Workshop was already running before an update, save your work, restart
`bash tools/editor.sh`, and reload the page. The Python host does not hot-reload.

## Verification

```bash
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Debug
cmake --build build-core -j 6
ctest --test-dir build-core --output-on-failure
bash tools/test-engine.sh
python3 tools/smoke.py network --headless
python3 tools/character_smoke.py
python3 tools/character_smoke.py --packaged
python3 tools/smoke.py gallery
python3 tools/smoke.py walkthrough
python3 tools/smoke.py persistence
python3 tools/smoke.py movement
python3 tools/smoke.py scent
python3 tools/smoke.py scent --packaged --headless
python3 tools/smoke.py network --packaged
python3 tools/travel_smoke.py
python3 tools/travel_smoke.py --packaged --headless
python3 tools/weather_smoke.py
python3 tools/weather_smoke.py --packaged --headless
python3 tools/lighting_smoke.py
python3 tools/lighting_smoke.py --packaged --headless
python3 tools/test_npc_bridge.py
npm --prefix Editor test          # model and live-editing tests
npm --prefix Editor run typecheck
python3 tools/test_map_editor.py
python3 tools/test_roster.py
python3 tools/test_world_db.py   # database tests need `world_db.py up`; skipped otherwise
python3 tools/test_world_store.py
python3 tools/test_live_edit.py
python3 tools/test_publish.py
python3 tools/test_game_tables.py
python3 tools/test_dungeon_master.py
RATW_TEST_DATABASE_URL="$(python3 tools/world_db.py conninfo dev)" ctest --test-dir build-core   # C++ incl. the PostgreSQL client
python3 tools/bundle_worlds.py --check
node --test DM/model.test.mjs
python3 tools/test_dm_service.py
```

Use `-DRATW_SANITIZERS=ON` in a separate CMake build directory for AddressSanitizer/UndefinedBehaviorSanitizer checks. Actual executed results and limitations are in [the test report](Docs/TEST_REPORT.md), and captures in `artifacts/screenshots/`.

## Design and status

- [Vision](VISION.md) and [finalized MVP plan](PLAN.md)
- [Component designs and architecture](Docs/Design/00-architecture.md)
- [Atlas Workshop map editor](Docs/Design/12-map-editor.md)
- [Territory, Chapters and migration](Docs/Design/16-territory-chapters-migration.md)
- [Storykeeper DM application](Docs/Design/17-storykeeper-dm.md)
- [Pace, stamina, and remembered world travel](Docs/Design/13-pace-and-world-travel.md)
- [Implementation status](Docs/IMPLEMENTATION_STATUS.md)
- [Questions for the next session](Docs/MORNING_QUESTIONS.md)
- [Actual game screenshots](Docs/SCREENSHOTS.md)
- Original supplied sources preserved in `Docs/References/`

NPCs use authored offline dialogue by default. An opt-in server-side bridge to
RATW Game's existing OpenAI provider has now passed live dialogue and persistent
recall tests; see [setup and results](Docs/LIVE_NPC_TEST_REPORT.md). Credentials
stay outside this repository and game clients. Conversation memory becomes
eligible for permanent consolidation one hour after the last interaction; it is
processed on startup or the next five-second checkpoint. Production model quality, public
authentication, Chapter construction, full combat, and Gifted/Quickened unlocks
have separate gates; see the status document for what is actually implemented
and tested.
