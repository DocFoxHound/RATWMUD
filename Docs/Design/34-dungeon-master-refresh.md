# 34. Dungeon Master refresh: running the world, and Storykeeper for its stories

Planned 2026-10-02. **Phase 1 started:** LIVE's first slice was built 2026-10-04 (1.1). Seven decisions were agreed on
2026-10-02 (listed under "Decisions"). This plan extends `Docs/Design/21-dungeon-master.md`, whose phases 1–4 are
built, and replaces its phase 5 ("Story Creator v1") with the larger story system below. Doc 21 still describes the
parts that are built.

This plan has four parts:

- **Dungeon Master (DM):** the live-world tool. It opens on *LIVE*, the world map as it is now, with every detail off until switched on, and keeps its curated workspaces for
  NPCs, factions, Chapters, roads, the economy and players. It can drive every live system the game has, or will have.
- **Storykeeper:** the name stays, but now for the place inside DM where stories are planned, run and watched. Stories
  are world-level events that reach players. They are made of steps with requirements, and they spawn quests,
  conversations, rumours and changes to the world.
- **Records:** the technical layer, one level down and for admins only. It edits a curated set of tables in detail.
- **One action list:** everything a person, Records or a running story can do to the game is the same validated,
  audited server action.

All numbers here are placeholders to be tuned. Each is marked *(placeholder)* the first time it appears.

## Where we stand (read from the code 2026-10-02)

| Area | State | What exists |
|---|---|---|
| DM app | Built | `Editor/src/dm/`, `tools/dungeon_master.py`, `tools/dungeon-master.sh`. Logins have the roles viewer, dm and admin (`dm.admins`, `dm.sessions`), there is a PROD/DEV switch, and PROD gets a red frame. |
| Reaching the game | Built | DM inserts a row into `dm.actions` and sends `NOTIFY ratw_dm`. `Game::applyDmActions` applies up to 50 rows a second and writes back `applied`/`refused` with a message. A row left waiting 10 minutes expires. Each action is also an `operator` row in `game.events`. |
| Live content | Built | NPCs and routes go through the live-edit log; areas, spawns and factions go straight to `live.*`. Then `npc.sync`, `layers.sync` or `factions.sync` makes the server load a candidate world and adopt only the changed part. |
| Players tab | Built | A character table and map, kill and resurrect, portrait review, the Life panel. |
| NPC Management | Built | Named NPCs: create, place, edit, kill, revive, delete. Routes, areas (wander, spawn, plan), spawn rules, the Calendar (*Call the festival*), the Life panel (chronicle, an AI life story, and read-only memories, bonds and rumours). Job holders are read-only. |
| Factions | Built | Kinds, painted claims, a relations matrix with history, members with ranks. The game uses only names, colours and claims. |
| Story Creator, LIVE | Placeholders | `DmApp.tsx` tabs with `ready:false`. |
| Old Storykeeper | Built, legacy | `DM/app.mjs` and `tools/dm_service.py`, with SQLite and the `Core/RatwDirector` file bridge. Campaigns, beats and an event desk that schedules one effect at a time. Four effects work (notice, weather, NPC relocate, money transfer). The C++ side also has `weather_front`, which the old app never sends. Brigands, assassins, war and faction collapse are always blocked. |
| Stories, quests | None | No triggers, no multi-step logic, no story tables. `dm.stories` holds only AI-written life stories. No quest system exists, and the NPC voice's prompt forbids giving quests. |
| Audit, accounts, undo | Data only | `dm.audit` is written, but nothing shows it. Accounts are managed only from the command line. There is no undo. |
| Ambient director | Built | `Core/RatwAmbient.*` runs NPC-to-NPC talk on its own. DM cannot steer it. |

**Rules this plan keeps (docs 15, 17, 21, 31):**

- The server is the authority. DM asks, and the server checks and applies.
- An intention is never shown as done until the game confirms it.
- Every action is idempotent and audited.
- Money is conserved: every DM or story money change is an explicit source, sink or transfer.
- Anything valuable (purses, items, crime, contracts, quests) that DM or a story changes goes through `Game::record()`
  and the journal (doc 31). Otherwise a journal replay could state an older "now is" over it. Everything else is
  picked up by the next fork snapshot, and the action asks for one soon.
- DM never edits terrain.

## Decisions (2026-10-02)

1. **A planner, not scripting.** Stories are graphs of steps with requirements. Conditions use a small expression
   language that cannot loop. There is no general scripting language.
2. **The game server runs stories.** They keep running when nobody has DM open, and they are saved with the world.
3. **Quests extend what exists.** A quest is a generalised contract (doc 26 Phase 5) with objectives. It also covers
   doc 32's faction missions. It is not a separate system.
4. **AI is a co-author, reviewer and narrator of progress.** It drafts and reviews stories, summarises how each one is
   going, and offers suggestions. It never starts or changes anything on PROD by itself. Every paid call is confirmed
   first.
5. **Records reach a curated set of tables,** not the whole database.
6. **Storykeeper keeps its name** and becomes where stories are planned and where the detailed decisions are made. The
   old Storykeeper app and its file bridge are retired once their effects are moved into the one action list.
7. **Full memory editing.** DM can add, edit and delete an NPC's memories, bonds and rumours. Each edit is audited, with
   its before and after, so it can be traced and reversed.

## Part 1: The shape of the app

### 1.1 LIVE (the main screen)

DM opens on one screen: **LIVE**, the world map as it is now. It is the first tab, and where everyone lands after
signing in. It fills in doc 21's LIVE placeholder.

**First slice built 2026-10-04** (`Editor/src/dm/LiveTab.tsx`, `GET /api/live`, `POST /api/live/move`,
`Core/RatwWatch.*`, `Core/RatwGameWatch.cpp`, migration 0031):

- the map with live positions, gathered into counts below a zoom of 4 pixels a tile, and double-click to open a place;
- the layers Players, NPCs, Shops, Structures, Chapters, Factions, Routes & areas and Events, all off at the start;
  NPCs narrow by role and road folk, Players by those away, both by the dead, and a search finds anyone;
- moving any player or NPC (`npc.move`, `character.move`, through `World::teleport`), kill, revive and resurrect;
- spawning a new or copied named NPC on a tile;
- the side panel: how fresh the positions are, refused actions, recent events, the calendar.

Not yet: dragging a marker to move it, temporary NPCs, resurrecting at a chosen tile in one step (resurrect, then move),
the Stories, Quests, Rumours and Weather layers, and the rail's story entries and other quick actions. Roads (caravans
and bandits) show in the NPCs layer as folk of the road, and crime in the Events layer, until their own layers come.

**The map**

- **Zoom** from the whole world down to single tiles. The wheel zooms and dragging pans, as `Editor/src/dm/MapView.tsx`
  does today. Zooming into a place or interior opens it in place, and a breadcrumb leads back out. Elevation shading
  keeps its `E` toggle.
- **Live positions:** where every player and NPC is now, not where they spawn. Spawn points stay in the NPCs workspace,
  as doc 21 says. Positions are at most a couple of seconds old (see *The watch feed* below), and the map says how
  old they are.
- **Zoomed out,** markers gather into a count per place ("Ridgemere: 41 NPCs, 3 players"), so 1,000 players and every
  resident stay readable. Zooming in splits them into single markers.
- **Hover** shows a name and a line about the marker. **Click** opens the inspector beside the map: who or what it is,
  what they are doing, and links into the workspace that edits it.

**Layers: everything is off at the start**

The map opens showing only the ground. A layer panel switches on what the DM wants to see, and each layer can be
narrowed further. Every layer is off each time DM starts.

| Layer | Shows | Narrow by |
|---|---|---|
| Players | Online players; offline characters at their last saved place, dimmed | online or offline, alive or dead, party, Chapter, name |
| NPCs | Every NPC where they are now | named, job holders, spawned; role, faction, town; alive or dead; in a story; name |
| Shops | Stores, keepers and markets, with stock and price factor on hover | town, kind |
| Structures | Places and buildings, doors and interiors, and Chapter structures (doc 32 Part 5) | kind, owner |
| Chapters | Chapter sites, meeting places, rentals, camps and holds, with members online | Chapter |
| Factions | Painted territory claims | faction, kind |
| Stories | Each running story's places, its cast, and where its current step is waiting | story, step |
| Quests | Givers, objective places, and players holding quests | story, quest, status |
| Routes & areas | Patrol routes, wander, spawn and plan areas, spawn rules | kind |
| Roads | Caravans moving, bandit camps, contracts | town, kind |
| Crime | Recent incidents, warrants, the gaol | days back |
| Rumours | Where a chosen rumour is believed, town by town | rumour |
| Weather | Weather by region | none |
| Events | Notable events from `game.events`, pinned where they happened | kind, days back |

A layer that depends on a system not built yet (Chapters, structures, quests) appears when that system is built.

**Moving and spawning, from the map**

- **Move an NPC:** select them, then *Move* and click a tile, or drag the marker. This is `npc.move`: the server checks
  the tile is open and walkable, places them there, and they carry on with their day from there. Changing where they
  live, work or spawn stays in the NPCs workspace.
- **Move a player:** the same for a character. An online player is moved at once and told so. For an offline
  character, the saved position changes, and they wake there. This is doc 21 phase 6's teleport, done from the map.
- **Bring a dead character back where they lie,** or at a chosen tile (resurrect with a place).
- **Spawn an NPC:** *Spawn* and click a tile. Choose one of:
  - a **new named NPC** (the NPC form opens with the place filled in);
  - a **copy of an existing NPC** (role, looks, hours, personality), like a spawn rule's newcomer;
  - a **temporary NPC** for a while or a story (a messenger, a crowd), who leaves when their time or their story ends.

  Named and copied NPCs go through `live.npcs` and `npc.sync`, as today. Temporary ones are a server action and are not
  added to the live tables.
- Moves and spawns are audited. On PROD, moving a player and spawning ask for confirmation, and moves can be reversed
  (Part 8).

**Around the map**

A collapsible rail beside the map holds:

- running stories, each with its step and the AI's one-line summary (Part 9);
- alerts: refused or expired actions, stalled stories, a cast member killed, a story that failed its check;
- the world today: date, weekday, weather, festivals, the next Marketday and Restday;
- quick actions: post a notice, call a festival, set the weather, pause all stories.

Clicking an entry shows it on the map and turns on its layer.

**The watch feed**

Positions do not exist anywhere DM can read today. Players are saved only as they change, and NPCs only in the
5-second snapshot. So:

- While a DM has the map open, the server writes a compact frame of every player's and NPC's position and state
  *(placeholder: every 2 s)*. It goes to an unlogged table, `dm.watch`, one row per world. The DM host reads it when the
  map asks, every 2 s, so no notification is needed.
- The DM backend passes frames to the browser. Layers that change slowly (shops, claims, routes) are read from their
  tables as today.
- The server writes frames only while a DM has asked for one in the last minute (`dm.watchers`) *(placeholder)*, so
  the feed costs nothing when nobody is watching. The frame is made on the game thread (a list of names and places) and
  written by a thread of its own on its own connection, so the tick never waits on the database.
- The feed never carries chat or what anyone said.

### 1.2 Workspaces

The curated tools, one level down from LIVE, each a map with an explorer, inspector and command palette, as
today:

| Workspace | Covers |
|---|---|
| **NPCs** | Today's NPC Management, plus jobs and positions, households, apprentices, needs, purse and goods, and the mind (Part 4). |
| **Factions & Chapters** | Today's Factions, plus Chapters (doc 32 Part 3: members, ranks, renown, treasury, sites), standing bands and burdens, treaties, and player factions once guilds exist. |
| **Roads** | Caravans, bandit camps, contracts and quests, town price factors, and rumours travelling. |
| **Economy** | The treasury, ledgers, mint and sink counters, explicit transfers, sources and sinks, and keeper stock. |
| **Players** | Today's tab, plus doc 21 phase 6 (teleport, items, coins, mute, kick, ban, notes) and the quests each player holds. |
| **Storykeeper** | Parts 2–6. |
| **Chat** | The game's chat as it happens (doc 21's LIVE sketch had it; LIVE is now the map). |
| **Audit** | Every action and edit: who, when, why, what the game did, and before and after. |
| **Records** (admin) | 1.3. |

### 1.3 Records: the technical layer

Records is a schema-aware browser and editor for a **curated** list of tables. It is one level down from the
workspaces, and only the admin role sees it. It is for detail the curated forms do not cover, and for correcting
mistakes.

- **Which tables:**
  - `live.*`: NPCs, routes, areas, spawns, factions, claims, relations, members.
  - Story and quest definitions.
  - Game state, through server actions only: `game.npc_memories`, `game.bonds`, `game.beliefs`, contracts and
    quests, Chapter tables (doc 32), character fields that already have actions.
  - Read only: `game.events`, `dm.audit`, `dm.actions`, `game.checkpoints` metadata.
- **How edits land:** an edit to a `live.*` table is written through the live-edit log (so edits conflict instead of
  overwriting) and followed by a sync. An edit to game state becomes a server action, because the server owns that
  state and the journal would otherwise overwrite it. Records never writes `game.*` directly.
- **What each table needs:** an entry in a small registry (columns, types, references, which action applies it, which
  role may write it). A table without an entry is not shown. The curated list is extended by adding entries.

## Part 2: One action list

Today's actions (`character.kill`, `npc.sync`, `festival.call`…) grow into one catalog, defined once and shared by the
DM backend, the server and the story engine. Each action has a name, a parameter schema, a required role, whether it
touches valuables (and so goes through the journal), whether it needs a PROD confirmation, and, where possible, its
compensating action (Part 8).

The old Storykeeper's effects move in first: `notice.post`, `weather.set` (and `weather.front`), `npc.relocate`,
`economy.transfer`. Then, by area:

| Area | Actions (first set) |
|---|---|
| NPCs | `npc.sync`, kill/revive/despawn, `npc.move`, `npc.relocate` (travels there), `npc.schedule` (override for a time), `npc.goal` (go somewhere and do something), `npc.job`, `npc.household`, `npc.purse` (as a transfer) |
| Mind | `memory.add/edit/delete`, `bond.set`, `belief.plant/edit/delete`, `npc.brief` (a topic the NPC brings up, Part 5) |
| Factions | `factions.sync`, `faction.stance`, `faction.member`, `faction.claim`, `war.declare`, `truce`, `embargo` |
| Chapters (doc 32) | `chapter.renown`, `chapter.rank`, `chapter.standing`, `chapter.notice`, `treaty.approve` |
| Roads | `caravan.send/halt`, `camp.spawn/disband/strength`, `contract.post/complete/cancel`, `price.factor` |
| Quests | `quest.offer/assign/advance/complete/fail/cancel` (Part 3) |
| Talk | `scene.play`, `npc.approach` (Part 5) |
| World | `weather.set`, `festival.call`, `notice.post` (to a place, a region, a faction, a Chapter or a player) |
| Players | doc 21 phase 6, `player.reveal_name` (doc 32 1.5), `item.give/take` |
| Stories | `story.start/pause/resume/stop`, `story.advance` (move a run to a step by hand), `story.set` (a variable) |
| Combat (doc 33) | brigands, assassins and armies, once combat exists |

Every action carries an idempotency key. For a person that is today's action ID. For a story it is
`story:<run>:<step>:<n>`, so a story replayed after a restart never applies the same effect twice.

## Part 3: Quests

A quest is the player-facing side of a story: something a player is asked to do, with an outcome the story can wait
on.

- **It extends contracts.** Today a `Contract` has a kind (bounty, escort, supply, courier), a poster, a town, a target,
  a taker, a reward and a due day. A quest adds:
  - **objectives**, each one of the kinds below, done in order or in any order;
  - **givers**: an NPC, a notice board, a faction's mission board (doc 32 4.5), or a story;
  - **who may take it**: anyone, a player, a party, a Chapter, by standing or level;
  - **rewards**, conserved: coins from a named purse or the treasury, items, renown, standing, bond changes;
  - **failure**: a deadline, a cast member dying, a step in the story moving on without it.
- **Objective kinds (first set):** go to a place; talk to someone (about a topic); deliver an item to someone; bring
  back N of an item; escort someone or a caravan to a place; be present at a place at a time; see an NPC safely
  through a period; defeat a camp *(needs combat)*; something a DM judges done by hand.
- **Contracts become quests.** The existing four kinds become quest templates with one objective each, so caravans,
  bounties and couriers keep working through the same system.
- **For the player:** a quest log in the browser client, with offers from NPCs in conversation and from boards. The
  NPC voice may offer a quest only when a quest names that NPC as a giver. The prompt's rule against quests stays for
  everyone else.
- **Saved** as valuables, through the journal, like contracts.

## Part 4: The NPC mind in DM (full editing)

The Life panel's read-only "In mind" becomes editable:

- **Memories** (`game.npc_memories` and the server's `MemoryStore`): add, edit and delete, including memories of
  players, other NPCs and things that never happened.
- **Bonds** (`game.bonds`): affinity, trust, respect, familiarity, fear and owed, toward anyone, with a reason.
- **Rumours** (`game.beliefs`): plant, edit and delete a belief, with its confidence and source.
- **Open conversations and promises:** close, change or add them.

Each edit is a server action, so the running NPC's mind changes at once and the journal or snapshot keeps it. The audit
keeps the before and after. The NPC's chronicle gains an `operator` entry only when the DM chooses ("this happened
in the world") rather than for corrections.

## Part 5: Stories

### 5.1 What a story is

A story is a definition (kept by DM, versioned) and its runs (kept by the server). A definition has:

- **Scope and weight:** a place, a region or the world; major or minor; how many may run at once.
- **Cast:** named roles, each filled when the run starts or when a step needs it:
  - a fixed NPC, faction, place, Chapter or player;
  - or a query, for example "a merchant in Ridgemere whose trust toward the participant is at least 40, not in another
    story", with an order (nearest, most trusted, random) and what happens when nobody fits (wait, skip, fail).
- **Variables:** numbers, text, flags, chosen targets and counters the steps read and set.
- **Steps:** the graph. Each step has:
  - **requirements** to enter it: one or more triggers, and conditions that must also be true;
  - **actions** when entered, in order, each from the one action list, and each with its cast and variables filled
    in;
  - **exits**: the steps that may follow, each with its own requirements, so a step can branch ("the courier arrived" /
    "the courier was robbed" / "three days passed");
  - **a timeout** with its own exit;
  - **holds**: cast members this step keeps from other stories while it is active.
- **Start:** by hand, at a time, on a schedule (every Marketday, every season), or on a trigger (for example "a
  faction relation falls below −60").
- **End:** a step marked as an ending (success, failure, abandoned), with a short line for the chronicle.

### 5.2 Triggers

Triggers are events the server already produces. Most are the same events it writes to `game.events`, so a trigger
costs nothing until its event happens.

| Group | Triggers |
|---|---|
| Time | a game date, hour or weekday; Marketday, Restday, a festival; real UTC time; time spent in this step |
| Places | someone (a cast member, a participant, any player, any NPC) enters or leaves a place; N players present |
| People | a death, downing or revival; an arrival or departure; a birth, marriage or succession; a job taken or lost |
| Bonds and talk | a bond crossing a threshold; a conversation between two characters; a topic raised (tagged by the NPC voice) |
| Factions | a relation or stance change; a claim made or given up; Chapter renown or level reached |
| Roads and money | a caravan arrives, is robbed or is lost; a contract or quest taken, done or failed; a price factor crossing a value; a purse or the treasury crossing a value |
| Crime | an incident in a place; a warrant; a gaoling |
| Rumours | a belief reaching a town, or held by N characters |
| Items | an item delivered to, or held by, someone |
| Weather | a weather state in a region |
| Stories | another story reaching a step or ending |
| DM | a *Go* button: the step waits for a person's decision |

### 5.3 Conditions

A small expression language over the same facts:

```
cast.merchant.alive and bond(cast.merchant, participant).trust >= 40
count(players_in("rm_market")) >= 3 or var.attempts > 2
relation("ridgemere_watch", "bandits_east").disposition < -60
```

It has comparisons, `and`, `or`, `not`, counts and a fixed list of functions. It has no loops and no assignments
(actions set variables). It is checked when the story is saved: every name must exist, and every function must get the
right types. It is evaluated only when one of its step's triggers fires, and every evaluation has a cost limit.

### 5.4 Players in a story

- **Participants** are players who take part: they accept a quest, enter a story place while it runs, or are cast.
- **Touched** players are reached without taking part: they heard the notice, believed the rumour, saw prices rise, or
  were turned away by a gate the story closed. The run lists both, so the DM sees a story's reach.
- An absent player is never required for a step to move on, unless the DM sets it for a step that is personal to them
  (doc 16's open question on events for absent players).

### 5.5 How the server runs stories

- **Where:** a story engine in the game server. It holds the runs, an index from trigger kind to waiting steps, and a
  timer queue.
- **Each tick:** it takes the events that tick produced, wakes only the steps waiting on them, checks conditions, and
  carries out the actions of steps entered. A budget per tick *(placeholder: 2 ms)* keeps it from slowing the world,
  and work over budget waits for the next tick.
- **Versions:** a run keeps the version it started with. A new version applies to new runs only, unless the DM
  migrates a run by hand.
- **Saved:** run state (step, variables, cast, timers, participants, holds) goes in the fork snapshot. Actions that
  touch valuables go through the journal with their idempotency keys. After a restart, runs continue where they were
  and no effect is applied twice.
- **Reported:** the server writes each run's state and each step change to Postgres (`game.story_runs`,
  `game.story_log`), the way it writes action results today. DM reads those and never runs stories itself.
- **Definitions reach the server** as a `stories.sync` action, checked like an NPC: the server loads and validates the
  definition before adopting it.

### 5.6 Stories working together

- **Holds:** a cast member held by one step can't be cast by another story until released. A story can share a member
  on purpose (two stories about the same feud).
- **Priority:** a major story's actions win over a minor one's when both act on the same NPC in the same tick.
- **Ambient talk steps aside:** the ambient director does not pick an NPC that a story holds, unless the step allows
  it. It may also be given a story's topics to spread (Part 6).
- **Pause all:** one switch pauses every run (timers stop, triggers queue) for an emergency or maintenance.

## Part 6: How a story reaches players

The actions that make a story felt, beyond quests:

- **Notices:** posted to a place's board, called by a crier, or sent to a faction, a Chapter or a player.
- **Rumours:** `belief.plant` in a few NPCs, which then spread as rumours already do (doc 26 Phase 5) and reach players
  through talk.
- **NPC briefs:** `npc.brief` gives an NPC a topic, a stance and how long it lasts. The NPC voice works it into
  conversation ("the bridge at Coldwater is out, and I don't trust the toll men"). Briefs are short and have a word
  budget, so they cost no more than ordinary talk.
- **Scenes:** `scene.play` runs a scene from the library (2,507, doc 30) with the story's cast, or a scene written for
  the story.
- **Approaches:** `npc.approach` sends an NPC to find a player and open a conversation with a brief or a quest offer.
- **The world itself:** prices, caravans held or sent, bandit camps growing, gates closed, weather, festivals,
  relations and wars, NPCs moving or dying.

## Part 7: Storykeeper (the workspace)

Where stories are planned and where the detailed decisions are made:

- **Library:** drafts, versions, scheduled, running, finished, archived. Search, duplicate, export and import as JSON.
- **The planner:** a node graph of steps with their exits, beside a form for the selected step (requirements, actions,
  timeout, holds). There is a cast and variables panel, and a map that shows the story's places and cast.
- **Check:** validation as you edit, by the same rules the server uses, plus warnings: unreachable steps, a step with no
  way out, cast that can die before it is needed, money that does not balance, holds that clash with running stories.
- **Rehearse on DEV:** run it on the DEV server with a faster clock *(placeholder: up to 60×)*, fire triggers by hand,
  step forward and back, and see each action's result.
- **Run on PROD:** start now, at a time, or on its start trigger, with a confirmation, and a second DM's approval for a
  world-scope major story *(optional at first)*.
- **Watch:** each run's graph with the current step lit, the log of what fired and what each action did, its
  participants and touched players, and its holds. The DM can pause, resume, stop, move to a step, set a variable, or
  press *Go* where a step waits.
- **Quests and talk:** the quest editor (objectives, givers, rewards) and the brief and notice editor live here too,
  since stories use them most. A quest can also be made outside a story.

## Part 8: Safety

- **PROD:** a red frame. Kill, delete, war, starting a major story and editing memories ask for a second click with a
  reason.
- **Roles:**
  - viewer: reads everything except Records;
  - dm: runs the world and stories;
  - admin: also Records, accounts and two-person approvals.
  - Account management moves into the app (admin only).
- **Undo by compensation:** where an action has an inverse (move back, restore a relation, restore a memory, reverse a
  transfer), Audit offers *Reverse*, which sends that inverse as a new, audited action. Some effects can't be reversed
  (a death once doc 33's permanent death exists, a rumour already spread), and Audit says so.
- **Stopping a story** stops its future steps. It does not undo what has happened. The DM can run an ending step that
  tidies up (releases cast, cancels open quests).

## Part 9: AI in Storykeeper

Uses the project's NPC Mind provider (doc 28) and its limits. Each call is paid, so each one is confirmed, and none
runs in tests (`RATW_AI=off|fixture`).

- **Draft:** from a premise ("the eastern bandits grow bold and Ridgemere's Watch is split on whether to march"), the AI
  drafts a story definition. It drafts only from the real action list, triggers and functions, and the real NPCs,
  factions and places it is shown. The draft is checked like anything typed by hand and opens as a draft. It is never
  scheduled automatically.
- **Review:** reads a definition and reports dead ends, weak branches, cast at risk, pacing (steps that may take weeks
  of real time), and how many players it is likely to reach.
- **Write:** quest text, notices, NPC briefs and scene lines, in the setting's voice.
- **Summarise progress:** for each running story, a short account of what has happened so far and what is likely next,
  from its run log and the events it touched. It is shown in LIVE's side panel and in Watch, and refreshed on request or
  when a step changes *(placeholder: at most once an hour per run)*.
- **Suggest:** from the run state and the world, the next moves a DM could make ("the courier quest has had no taker
  for two days; post it in Ser Ferro too, or let the step time out"), and stories the world seems ready for (a relation
  falling, a starving camp, a vacant position). Suggestions are only text and drafts. A person always decides.

## Part 10: Data

Postgres, as the project prefers.

- **DM side:**
  - `dm.story_defs` (id, name, scope, weight, status) and `dm.story_versions` (definition JSON, author, notes, checked);
  - `dm.quest_templates`;
  - `dm.ai_notes` (summaries and suggestions, with the run and version they were about);
  - `dm.watch` (unlogged): the latest watch frame for each world (1.1).
  - Today's `dm.stories` (AI life stories) is renamed `dm.life_stories` to avoid confusion.
- **Game side, written by the server:**
  - `game.story_runs` (run, version, step, state JSON, started, updated, status);
  - `game.story_log` (run, step, what fired, actions and their results, at what game time);
  - `game.quests` (the quest and its objectives' progress, as valuables).
- **Old Storykeeper:** the useful parts of its SQLite store (Chapter records until doc 32 moves them, campaign notes)
  are imported once. Then `dm_service.py`, `DM/` and `Core/RatwDirector` are removed.

## Phases

Each phase passes the perf gate (`world_check --players 20`, and doc 31's measurements at up to 250 players) before
the next starts.

1. **One channel and LIVE:**
   - the action catalog;
   - the old Storykeeper's effects moved into `dm.actions`, and its app and file bridge retired;
   - LIVE (first slice built 2026-10-04): zoom, live positions through the watch feed, the layer panel with Players, NPCs, Shops,
     Structures (places and doors), Factions, Routes & areas and Events, and moving and spawning;
   - the rail (alerts, the world today, quick actions);
   - the Audit workspace;
   - account management in the app.
2. **Deeper curated editing:**
   - full mind editing (Part 4);
   - jobs and households;
   - the Roads workspace (caravans, camps, contracts, prices);
   - the Economy workspace;
   - notices and weather by hand;
   - Records with its first curated tables.
3. **The story engine:**
   - story tables, `stories.sync`;
   - the server engine with the first triggers (time, places, people, story, DM *Go*), conditions and actions;
   - saving and replay;
   - run reporting.
4. **Storykeeper v1:**
   - the library and the planner;
   - checking;
   - DEV rehearsal with a fast clock;
   - PROD start and Watch.
5. **Quests:**
   - quests from contracts;
   - givers, objectives and rewards;
   - the client's quest log;
   - NPCs offering quests they are named for;
   - steps waiting on quests.
6. **Reaching players:**
   - NPC briefs, approaches, scenes and planted rumours as actions;
   - participants and touched players;
   - ambient talk spreading story topics.
7. **AI in Storykeeper:** draft, review, write, summarise, suggest.
8. **Working together and safety:**
   - holds and priority;
   - pause all;
   - compensating actions and *Reverse*;
   - two-person approval;
   - the remaining triggers (bonds, factions, roads, crime, rumours, items, weather).
9. **Following other plans:**
   - Chapter, treaty and mission actions as doc 32 is built;
   - brigands, assassins and armies when doc 33's combat exists;
   - natural disasters (doc 21 phase 7) as story actions that produce reviewed world releases.

## Open questions

1. **Storykeeper's home:** a workspace inside the DM app, with the same login and action queue (assumed here), or a
   separate app that shares the backend?
2. **Who may run what:** may the dm role start a world-scope major story on PROD, or is that admin only, or
   two-person?
3. **Quest log in the client:** a panel of its own, or part of the character dialog?
4. **Player-written requests:** may players ask the DM for a story or a quest in game (a petition), feeding Storykeeper's
   suggestions?
5. **Story secrecy:** should players ever see that something is a "story" (a banner, a named event), or only its
   effects in the world?
6. **Memory edits and the chronicle:** this plan logs corrections only in the audit, and logs "this happened" edits in
   the chronicle too. Is that the right line?
