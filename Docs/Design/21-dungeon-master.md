# Dungeon Master: running the living world

Status: plan agreed 2026-09-25 (decisions at the end). Phase 1 is built: the app
(`Editor/dm.html`, `Editor/src/dm/`, `tools/dungeon_master.py`,
`tools/dungeon-master.sh`), the three DM accounts, the `ratw_dm` database login,
the live action queue (`dm.actions`, applied by the game server), death in the
game, and the Players tab with kill and resurrect (migration 0013). Phase 2 is
built: the server builds its NPCs from the live tables (`live.people_manifest`,
migration 0014), NPC changes sync into a running server, and the NPC Management
tab creates, places, changes, kills, revives and deletes named NPCs.

## What it is

**Atlas builds the world. Dungeon Master runs everything alive in it.**

Dungeon Master (DM) is the live-world management tool. It replaces and absorbs
the Storykeeper (`Docs/Design/17-storykeeper-dm.md`): its campaigns, beats,
event desk, Chapters, faction opinions, migrations and audit all move into DM.
It looks and works like Atlas, with the same map, explorer, inspector, command
palette and live multi-person editing, but with different toolsets.

DM is **the source of truth for NPCs and everything that drives them**: which
NPCs exist, where they are, what they do, where they may go, who spawns where,
who belongs to which faction, what the factions think of each other, and the
stories that move the world. When Atlas and DM disagree about any of these, DM
wins.

DM **cannot change the world itself**: terrain, cells, interiors, doors and
places stay Atlas's. Later, natural disasters will be able to alter terrain,
but only as a story effect that produces a reviewed world release. There is no
terrain painting in DM.

## Who owns what

| Thing | Atlas (DEV) | Push to live | Dungeon Master |
| --- | --- | --- | --- |
| Terrain, cells, interiors, doors, places | Authors it | Copies DEV to PROD exactly | Read only |
| NPCs | Can sketch new ones in DEV | Adds new ones only (today) | Full control: create, place, change, kill, delete |
| Patrol routes, NPC areas, spawns | Can sketch new ones in DEV | Will add new ones only; never changes live ones | Full control; paints them |
| Factions, territory, relations | Plans them, so a new city arrives with its factions | Adds new ones only; never changes live ones | Full control; its changes always win |
| Player factions, guilds, clans | None | None | Manages standing, relations, war |
| Stories | None | None | Builds, saves, schedules, runs |
| Players | None | None | Admin actions |

After every push, DEV copies PROD's live layer back: today that's NPCs, jobs,
roster and economy, and it will extend to routes, areas, spawns and factions.
So Atlas users always see the live state they're building around.

## Where DM works: PROD, with DEV for rehearsal

DM points at **PROD** (the live world, shown with a red frame and requiring
confirmation for destructive actions) or at **DEV** (a rehearsal world, with a
DEV server running it). Stories and bigger operations can be tried on DEV
first, then run on PROD.

## Workspaces

Each workspace shows the world map read-only, with its own layers and tools on
top.

### 1. NPC Management

- **The map shows NPCs at their spawn points**: where they work, and their home
  and evening places when selected. It never shows where they happen to be
  walking; Atlas works the same way. Live positions belong to the LIVE tab.
- **The explorer** lists NPCs by place, faction, role or status, with search.
- **Tools:**
  - Place NPC (create one on the map)
  - Move/teleport
  - Patrol route (click posts, as in Atlas today)
  - NPC area brush: paint wander zones, spawn areas and planning zones, tile by
    tile (gathering spots, keep-out zones and work areas can follow as kinds)
  - Spawn rules: which NPC to copy, how many, and respawn time
- **The inspector** covers identity and appearance, role and schedule, home,
  work and evening places, route, faction membership, purse and goods, needs,
  and a read-only view of their memories and relationships.
- **Actions:** kill, revive, despawn, delete. Killing and deleting need a second
  confirmation on PROD. The dead stay in the records; deaths are part of the
  world's history.

#### Life: chronicles, stories and what they have in mind (built 2026-09-29)

Under every named NPC and job holder in NPC Management, and every character in Players, a **Life** panel
(`Editor/src/dm/LifePanel.tsx`, `GET /api/chronicle`):

- **Chronicle:** their life compiled from the event log (`tools/chronicle.py`, doc 26 Phase 8), dated by the game's
  calendar. It shows milestones only, life and seasons (with each season's round of trade and talk), or everything
  (first meetings too).
- **Story:** *Tell their story* has the NPC model write a short life story from the chronicle only (dm and admin;
  one paid call, confirmed first). It is kept in `dm.stories` (migration 0026) with the last event it knew of, so the
  panel says how many events have happened since; *Rewrite* writes it again. Each one is audited (`story.write`).
- **In mind** (read only): the conversations they remember and those still open, their bonds and how others regard
  them (in words and numbers), the rumours they have heard, and what is said of them.

### 2. Factions

- **Factions of every kind:** NPC factions, cities and towns, and player
  factions (guilds, clans). A player faction is a first-class faction, so a city
  can go to war with a guild.
- **Territory brush:** paint a faction's claims on the map. Claims may overlap,
  and a claim is not control (as in design doc 16).
- **Relations:** a matrix of how each faction regards each other faction and
  individual players. That's a disposition (-100 to 100) plus a stance: allied,
  friendly, neutral, tense, hostile, at war. Each has a reason and a history.
- **Orchestration** (details later): declare war, truce, embargo, raid. Each is a
  logged action, usable directly or as a Story action.

#### Built in phase 4

- **Where factions live:** `live.factions` (with a kind: city, NPC faction, guild,
  clan, other, and a description), moved out of the authored tables. Atlas still
  makes and edits them on DEV (name and color) and still ticks which places each
  claims; a push adds only new factions, claims, members and relations, and once
  live the Dungeon Master's version wins. After each push DEV copies PROD's back,
  keeping DEV's own additions.
- **Territory:** `live.faction_claims` holds one claim per faction and place: its
  painted tiles, or the whole place. The Factions tab's Territory view shows every
  faction's claims (the selected faction's strongest), and for an open place:
  *Claim all of it*, *Paint tiles* (paint and erase, then save), or *Give up*.
  Atlas sees each place's claiming factions change as it happens; adding a claim
  in Atlas claims the whole place, and editing a place in Atlas keeps painted
  tiles. A place that someone claims can't be removed by a push until the claim
  goes.
- **In the game:** the server takes factions and claims from the live tables
  (`faction` and `claims` records in `live.people_manifest`, replacing the build's
  own) and adopts changes within a second (`factions.sync`). The game knows which
  factions claim each place; tile detail stays with the DM and stories for now.
- **Members:** named NPCs, each with a rank (`live.faction_members`), shown and
  edited in the faction's inspector.
- **Relations:** the Relations view is a matrix of how each faction regards each
  other (rows regard columns). A relation has a disposition (-100 to 100), a stance
  (which follows the disposition until chosen), a reason, and who set it and
  when; every change is kept in `live.faction_relation_log` and shown as history.
  One save can set both directions. Relations and members are not used by the
  game yet; they're there for the DM and for story triggers (phase 5).
- **Not yet:** player factions (guilds and clans the players belong to) need
  guilds in the game, and relations toward individual players need them too.
  Their kinds exist, so a planned guild can already be made and claim ground.

### 3. Story Creator

- **A library of stories** saved in the database: drafts, versions, scheduled,
  running, finished, archived. Build, save, load and duplicate.
- **A story is phases.** Each phase has **triggers** (a time; someone entering a
  place; an NPC dying; a faction relation crossing a threshold; enough players
  present; an item delivered...), **conditions**, and **actions** (spawn,
  move, or change an NPC; change relations; declare war; weather; notices; start
  another phase or story...).
- **Scope** can be one place, a region or the whole world. Stories can be major
  or minor, and many players can take part at once.
- **Test runs on DEV;** then schedule or start on PROD and watch runs live: phase
  reached, who's taking part, what fired.
- Storykeeper's campaigns, beats and event desk become the first, simplest kinds
  of story.

### 4. Players

A player-character manager, not a live view. It's mostly a spreadsheet: every
character with their stats (strength, dexterity, wisdom, stamina), skills
(sneaking, hearing, scent), senses, age, status (alive or dead) and last-saved
position. A map beside it plots where each one is. Sort, filter and search.

The first actions are **kill** and **resurrect**. The game has no death yet, so
this adds it: a dead character lies where they fell and cannot move, act or
speak until resurrected, and that state is saved. More actions (teleport, items,
coins, mute, kick, ban, notes) come later.

### 5. LIVE

Watch the game as it happens: every player and NPC moving on the map, and the
chat. Details (what tools it offers, how to keep it manageable at scale) come
later.

### 6. Watch and Audit

Who is online and where, what DM actions are pending or done, what stories are
running, and alerts. Every action records who did it, when, why, and what the
game actually did. As in Storykeeper, an intention is never shown as done until
the game confirms it.

### How NPC changes reach the game (built in phase 2)

- At start-up the server loads the world build without its NPC records and adds
  the ones `live.people_manifest` writes from the live tables (named NPCs, and
  profession slots filled by roster characters), so the game's own validated
  loader checks every NPC.
- Saving an NPC in DM checks it against the whole world (the same checks as
  Play/Export), writes it through the live-edit log (so Atlas users on DEV see
  it, and simultaneous edits conflict rather than overwrite), and queues
  `npc.sync`. The server loads a complete candidate world with the NPC as the
  tables now say, and only if that passes does it adopt that one NPC: a newcomer
  joins with their authored purse, a changed NPC keeps their purse and needs and
  re-plans, and a deleted NPC leaves, their purse returning to the treasury.
- `npc.kill` and `npc.revive` act on a running server at once and are written to
  `live.npc_state` for the next start. The dead keep no schedule.

### NPC layers (built in phase 3)

A layer switch on the NPC Management map picks what is being edited: **NPCs**,
**Routes**, **Areas** or **Spawns**.

- **Patrol routes** (`live.patrol_routes`/`patrol_posts`): click open ground to
  add posts, reorder or remove them in the inspector. Checked against the whole
  world like an NPC, written through the live-edit log, then `layers.sync`. A
  route a guard still walks cannot be deleted.
- **NPC areas** (`live.npc_areas`): tiles painted (and erased) with a brush, all
  in one cell or interior, up to 4096. Kinds:
  - *wander*: a civilian given the area (the NPC's “Wanders in”, the
    `live.npcs.wander_area` column) roams its open tiles during work hours and
    evenings, moving to another tile every half hour of game time;
  - *spawn*: where spawn rules bring NPCs in;
  - *plan*: a marked zone, no effect in play yet.
  Walls and water inside an area are ignored by the game. Saving queues
  `layers.sync`: the server loads a candidate world from the tables and, if it
  passes, adopts its routes and everyone's wander tiles (`World::adoptLayers`).
- **Spawn rules** (`live.spawns`): an area, a named NPC to copy, how many to
  keep alive (1–50), the respawn time in minutes, and on/off. The game server
  reads its rules every ten seconds, so no sync is queued:
  - short of the count, one newcomer arrives per check on a random free open
    tile of the area, as a copy of the template (role, looks, hours,
    personality) who lives, works and spends evenings there and wanders the
    area;
  - a fallen spawned NPC is cleared once the respawn time has passed since the
    server saw them dead, and a newcomer takes their place;
  - a newcomer is always someone new: their ID (`rule_` plus a stamp) is never
    reused, so no memories or history carry over; their name is the rule's
    name and the lowest free number (“Rat catchers 2”);
  - a rule that cannot place anyone waits five minutes; deleting a rule leaves
    the NPCs it made as ordinary NPCs, and deleting an area deletes its rules.
- **Push and copy back:** a push only adds routes, areas and rules PROD does not
  have yet (and never NPCs a DEV server spawned); PROD's own are listed as kept.
  After each push, and with *Copy NPCs from live*, DEV takes PROD's areas,
  rules, routes (through the live-edit log) and each NPC's wander area and
  spawn link, keeping anything DEV has that PROD does not.

## How DM changes reach the running game

This is the biggest new piece of engineering.

- **Today:**
  - NPCs come from the world build, loaded when the server starts.
  - Storykeeper talks to the server through a private file exchange with 4
    operations.
  - Pushes only land through a restart.
- **Plan: a live action queue in the database.**
  - DM writes an action (who, what, parameters) to `dm.actions` and notifies the
    server (`NOTIFY ratw_dm`).
  - The server checks it, applies it at its next tick, and writes back the
    result.
  - Each action is applied once, even if sent twice.
  - This replaces the file exchange.
- **NPC-layer data is loaded from the live tables, not the build.** The server
  loads NPCs, routes, areas, spawns and factions from `live.*` at start-up, and
  applies DM actions to its running copy and to the tables. A change is live
  within a second, with no restart. The build keeps only the world itself.
- **Terrain never changes this way.**

## Access and safety

- **DM logins:** separate admin accounts (`dm.admins`), with roles such as
  viewer, DM and admin. These are not player accounts and not the shared
  publish password. Later this can also replace the publish password for Push
  to live.
- **A database login of its own** (`ratw_dm`): it reads the world, runs the live
  layer and stories, sees player data, and changes players only through the
  action queue. It cannot write terrain. Creating it needs the database
  superuser once.
- **Everything is audited:**
  - PROD shows a red frame.
  - Destructive actions (kill, delete, war) ask twice.
  - Two-person approval for the biggest actions can come later.

## Data (first sketch)

- `live.npcs` gained the area it wanders and the spawn rule that made it (phase 3).
- Live tables built: `npc_areas`, `spawns` (phase 3); `factions` (moved from
  `world.factions`), `faction_claims`, `faction_relations`, `faction_relation_log`,
  `faction_members` (phase 4).
- New `dm` tables: `admins`, `sessions`, `actions`, `audit`, `stories`,
  `story_versions`, `story_runs`, `story_log`. The Storykeeper's current tables
  migrate into these.

## Phases

1. **Shell and Players:** the DM app from the Atlas codebase (second entry
   point), the three DM accounts and login, the PROD/DEV switch, the live action
   queue in the game server, death in the game, and the Players tab with kill
   and resurrect. Audit of every action.
2. **NPC live actions:**
   - The server loads NPC data from the live tables instead of the build.
   - Create and place, move, change behaviour, kill and revive, delete.
   - The LIVE tab, once its details are settled.
3. **NPC layers** (built):
   - Patrol routes, NPC area painting and spawns.
   - Push becomes add-only for all of these.
   - DEV copies them back after each push.
4. **Factions** (built, except player factions):
   - Factions and territory move to live data, run by DM; Atlas can still plan
     them on DEV.
   - Territory painting and the relations matrix.
   - Player factions (this needs guilds in the game).
5. **Story Creator v1:**
   - Story data, the editor, save, load and versions, and DEV test runs.
   - The server's story engine with a first set of triggers and actions.
   - PROD scheduling and monitoring. Storykeeper retired.
6. **More player actions** (teleport, items, coins, mute, kick, ban, notes).
7. **Natural disasters** as story actions that produce reviewed world releases.
8. **Atlas plans NPC layers:** everything DM runs live can also be set up in
   Atlas on DEV, so a new area goes live complete: NPC areas, spawn rules, faction
   members and relations, not only NPCs, routes and factions. See below.

## Phase 8 plan: Atlas plans NPC layers

**Why:** building a new area in Atlas should not mean pushing it live half-empty
and then setting up its NPC areas and spawns while players are already there.
(DM can already work on DEV, but world building happens in Atlas.)

**What Atlas gains, on DEV only:**
- **Areas** in the People editor, beside patrol routes: an *Areas* tool paints
  wander, spawn and plan areas tile by tile, with the same brush and rules as DM
  (one cell or interior each, up to 4096 tiles). The inspector names the area,
  sets its kind, and lists who wanders it and which rules use it.
- **Wanders in** on a named NPC's inspector (civilians), as in DM.
- **Spawn rules**: a list under Areas with the same form as DM (area, template
  NPC, how many, respawn minutes, on/off). A DEV server runs them for testing.
- **Faction details**: kind and description in Atlas's faction editor, and
  members and relations in a simple list (DM keeps the matrix).
- **Map overlays** for areas and spawn points, toggled like other Atlas layers.

**How it fits what exists:**
- These become live-edit keys (`area:ID`, `spawn:ID`, and `wander` and faction
  details inside the existing `person:` and `faction:` values), so several Atlas
  users and DM on DEV see each other's changes, conflicts included, and Undo
  works. Areas and rules stay live tables; nothing moves back into the authored
  world.
- The project JSON gains `npcAreas` and `spawns`, which Play and Export on DEV
  include (wander lines; a local game runs spawn rules like the server).
- Validation in both editors (the model's checks in `model.mjs` and
  `map_editor.py`): areas in existing places, tiles inside them, rules pointing
  at real areas and named NPCs.
- Pushing is unchanged: new areas, rules, members and relations are added; PROD's
  are kept. The copy back after a push brings PROD's into Atlas through the edit
  log, so they appear without a reload.

**Order:** areas and wander first (the most common setup), then spawn rules, then
faction details, then overlays and export.

## Decisions (2026-09-25)

1. **Factions and territory:** Atlas can build them for planning, so a new city
   is pushed with its factions already in place and players never wait for an
   admin. Push adds new ones only; once live, DM's changes always win.
2. **The server loads NPCs, routes and factions from the live tables,** so DM
   changes apply live. Agreed.
3. **Three DM accounts, each with its own password:** `dm-admin` (everything,
   including accounts), `dm-master` (runs the live world), `dm-viewer` (read
   only). They're stored in PROD (`dm.admins`), and passwords are only ever
   kept as salted hashes.
4. **Players tab:** built in phase 1 as a player-character manager with kill and
   resurrect.
5. **Storykeeper is retired** once its features are in DM. Until the Story
   Creator replaces its event desk, it keeps working.
6. **LIVE tab added:** watch all players, NPCs and chat. Details later.
7. **Atlas plans NPC layers too** (added after phase 3): NPC areas and spawn rules,
   like factions, can be set up in Atlas on DEV before a push, so new areas go
   live complete. Planned as phase 8.
