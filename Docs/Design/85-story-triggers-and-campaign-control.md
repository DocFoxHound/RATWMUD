# 85. Story triggers and campaign control: a trigger editor like StarCraft's and Warcraft III's, and a console to run a campaign live

Draft, 2026-11-21. A survey and a design; Phase 1 is built (section 6, "Phase 1 as built"), the rest is not. Surveyed at `9902c12b` on `ridgemere2` (the
Story Maker's commits `55ab323d..7300fc7d`, with our fixes `a86daab3` and `9e875df0`). Read
`Docs/story-programs.md` first: it is the author's guide to what exists. This doc says what is there in the
terms of a map editor's trigger system, what is missing, and what to build. Docs 17 (Storykeeper), 34 (the DM
refresh, Part 5 "Stories"), 58 (player storytellers) and 66 (worlds, Atlas and the DM) give the intent; where
this doc disagrees with doc 34's Part 5, this doc is newer.

Decisions are marked **decided (mine)**: I chose, the user can overturn. What only the user could settle was an
**open question for the user**, collected in section 8. **The user answered them on 2026-11-21** (section 8): build
everything, all ten phases, plus moving a running campaign onto a newly approved version (a new Phase 5b); small
quests are made here; steering a running campaign on a hosted world is the owner's, and a hosted DM's only with a new
"may steer campaigns" permission; and the recommendations stand for the rest. Where a decision below was waiting on one
of these answers, it now says **decided (user, 2026-11-21)**.

**Built so far:** Phase 1 ("See it"), on branch `r3-camp-p1`: see "Phase 1 as built" at the end of section 6.

## The ask (the user, 2026-11-21)

> Take a look at the triggers setup in the Story Maker, and the UI for triggers. Very simply, I am trying to make
> a trigger system akin to Starcraft or Warcraft's triggers on its map editor to allow a user/admin to either
> create small quests, or entire campaigns. I think I need to expand the user interface and controls so that an
> admin can more actively track and effect a campaign that is running in their world. I think they should have UI
> support for that. Also we need to analyze the existing trigger options and see if they need to be expanded.

## The short version

1. **The engine is already a trigger system.** A rule is Event + Conditions + Actions, as in Warcraft III. It has
   160 world events (plus named signals and timers), 7 kinds of condition, 7 kinds of action, 25 world effects,
   shared and per-wolf counters, named timers, rectangles on the map, journal missions and directed scenes. It is
   deterministic, rehearsed before it runs, and every world effect has a receipt.
2. **It is thin where a map editor is thick.** No typed variables, no if/else, no wait, no loops, no turning a
   trigger on or off, no conditions on the state of the world (only on the event that just happened and on
   counters), no dialogs outside a scene, no message to players, no item or coin reward to one wolf, no doors, no
   weather, no parties. A quest cannot be picked up by a wolf who was not in the audience on the day the story was
   published, which is what stops "small quests".
3. **The editor is a form over one JSON draft in the browser.** It works, and it has a map for regions and a
   library of 100 examples. It has no folders, no copy and paste, no undo, no pickers for residents, items or
   places (ids are typed), and it shows one validation error at a time.
4. **Almost nothing can be done to a running story.** An admin can read its history, counters, timers and
   receipts (by pressing Refresh), stop it, retry its last refused action, and issue one-use tickets. They cannot
   pause it, set a counter, fire a trigger, move it to another phase, see who is taking part or where they are,
   or see why it is not moving. This is the user's main ask and it comes first in the build.
5. **The build is eleven phases in three slices** (section 6, about 147 hours of agent time): a Campaigns console
   (see it, steer it, explain it), then the trigger editor, the richer trigger language and moving a running
   campaign onto a new version, then quests anyone can pick up, new events and effects from the game, and speed.
   The smallest useful first slice is **Phase 1: a read-only Campaigns tab that refreshes itself**, about 10 hours,
   with no engine change and no migration. **The user asked for all of it (2026-11-21).**

---

## 1. What exists today

### 1.1 The parts

| Part | Where | What it does |
|---|---|---|
| The rules | `tools/story_program.py` (1,154 lines) | The format (`ratw-story-program`, version 1), validation, missions, exits, timers, dispatch, rehearsal (`review`). |
| Triggers | `tools/story_triggers.py` (335) | Event-condition-action rules: `validate`, `satisfied` (conditions), `fire` (actions). The Python twin. |
| The native engine | `Core/RatwStoryTriggers.cpp` (203), `Core/RatwStoryRegion.h` (33) | The same `fire` and `satisfied` in C++, built as `libratw_story.so` with `ratw_server` (`CMakeLists.txt:276`). Symbol `ratw_story_dispatch_v1`: JSON in, JSON out. |
| The bridge to it | `tools/story_native.py` (101) | Finds and loads the library (`RATW_STORY_LIBRARY`, else `build-core/` and others) and passes one fact at a time. |
| The runner | `tools/story_runner.py` (872) | One process per world. Reads the world's event log, advances each run, queues effects, waits for receipts. Also approval, publication, status, directions. |
| Scenes | `tools/story_scenes.py` (307), `Core/RatwGameCinematics.cpp` (722) | The `ratw-scene` format (cast, beats, votes, written replies) and its playback in the game. |
| The library | `tools/story_library.py` (186), `Data/Storylines/event-library.json` | 100 event types in 10 categories, each a mission that can be dropped into a campaign or run alone. |
| The author bridge | `tools/story_author.py` (97) | A JSON-lines process for an AI or a player to draft, validate and rehearse offline, and to redeem one-use tickets. |
| In the game | `Core/RatwGameStorylines.cpp` (830), `Core/RatwStoryArea.cpp` (94), `Core/RatwStorylines.{h,cpp}` | Applies the runner's orders: the journal (`applyWorldStoryPhase`), residents walking, speaking and posing for a story, area effects, region watching. The journal is doc 58's storyline book; a run is shown in it as a storyline of kind `world`, source `storykeeper`. |
| The route | `tools/dungeon_master.py:2717-2755` | `POST /api/dm/story-program` with `{op, target, ...}`. `tools/studio.py:613` mounts it; `studio.py:1354` and `1822` add the hosted checks. |
| The UI | `Editor/src/dm/Story*.tsx`, `Scene*.tsx`, `EmbeddedScenes.tsx`, `scenes.ts` (about 1,350 lines) | Section 1.12. |
| The data | Migrations `0067` to `0070` | Section 1.9. |
| Examples | `Data/Storylines/programs/*.json` (8), `Data/Storylines/scenes/greyfen-oath.json` | Complete programs with their rehearsal cases. |

Two things with similar names are **not** part of this: `Core/RatwGameStoryteller.cpp` is doc 58's player
storytellers (tales run by a player at a table), and `tools/dm_service.py` is the old separate Storykeeper
service of doc 17. And `Editor/src/lib/storyRehearsal.ts` with the "Write draft" and "Inspect rehearsal" buttons
is an older three-trigger draft that lives only in the browser and that no tool reads.

### 1.2 The shape of a story

A **program** has an id, a title, an **audience**, a list of **phases**, one opening phase, optional **campaign
triggers** (shared by the phases), optional **regions**, optional embedded **scenes**, and **rehearsal cases**.

A **phase** is the unit a player sees: it has a journal entry, effects applied when it opens, **exits** (the older,
simpler kind of trigger: "on this event, go to that phase"), **triggers**, up to 16 **missions**, and optional
guest voices. A phase with no exits and no triggers is an ending. Entering a phase again is a new **visit**: its
entry effects run again, its missions start fresh, and its once-per-visit triggers may fire again.

A **trigger** is:

```json
{"id": "inspect-landing", "on": "story signal", "signal": "inspect",
 "match": {"target": "rm111"},            // optional: exact filters on the event's fields
 "audience": "world",                     // or "participant": only wolves of the story's audience
 "once": "each",                          // each | visit | player | campaign | member
 "if": {"test": "any", "conditions": [ ... ]},
 "actions": [ {"do": "emit", "signal": "open-crossing"} ]}
```

Within a phase the engine tries triggers in the order they are listed, campaign triggers first. A counter changed
by one trigger is seen by the next. Signals sent while handling a world event are delivered after the triggers
for that event, in the order sent, at most once per signal name per world event; cycles of signals are refused
when the program is checked. A trigger that enters a phase ends the handling of that event: signals still queued
are dropped.

### 1.3 Events: 160, plus signals

Every event is one row of the world's log, `game.events`, with the same fields: `kind`, `actor`, `target`,
`cell`, `item`, `detail`, `quantity`, `coins`, the tile `x`,`y` (migration 0070; filled from the actor when the
game knows where they stood), `game_day`. A trigger names a kind and may filter on `actor`, `target`, `cell`,
`item` and `detail` by exact text (`FIELDS`, `story_program.py:171`). A gift's item matches its base item, so a
marked or crafted rope counts as rope.

The list is `EVENTS` (`story_program.py:17-170`):

| Group | Count | Events |
|---|---|---|
| Story engine | 3 | `clock` (a delay in game days since the phase opened: `afterDays`), `dm signal` (exits only, never triggers), `story phase` |
| Scenes | 8 | `scene arrived`, `scene spoken`, `scene posed`, `scene blocked`, `scene finished`, `scene choice`, `scene response`, `scene failed` |
| Regions | 2 | `region entered`, `region left` |
| Coming and going | 8 | `arrival`, `departure`, `enter place`, `character created`, `welcome`, `welcomed back`, `spawn`, `relocation` |
| Talk, ties and company | 23 | `conversation`, `introduced`, `together`, `shared meal`, `groomed`, `tended`, `vouch`, `vouch broken`, `promise`, `promise broken`, `tie ended`, `matchmaking`, `marriage`, `nickname`, `nickname dropped`, `witnessed`, `trouble heard`, `trouble solved`, `story told`, `performance`, `table game`, `table game begun`, `sermon` |
| Gifts, items and loans | 7 | `gift`, `gift again`, `gift used`, `lent`, `loan returned`, `loan unreturned`, `forage` |
| Fights, hunting, death | 11 | `fight`, `fight ends`, `downed`, `death`, `revival`, `old age`, `mourning`, `hunt`, `huntRole`, `hunted`, `assault` |
| Bandits and the roads | 14 | `raid`, `bandits gather`, `bandits called`, `bandits creep`, `bandits demand`, `bandits scattered`, `bandit falls`, `camp cleared`, `cleared`, `paid off bandits`, `patrol`, `carter robbed`, `carter hurt`, `robbed` |
| Caravans, wagons, carts | 15 | `caravan arrives`, `caravan departs`, `caravan lost`, `caravan dropped`, `caravan home`, `caravan passes`, `wagon posted`, `wagon overdue`, `wagon held up`, `wagon delayed`, `wagon cancelled`, `cart arrives`, `cart departs`, `cart missed`, `cart stocks the stalls` |
| Ships | 6 | `ship docks`, `ship lost`, `ship sails`, `ship stormbound`, `ship posted`, `ships cleared` |
| Money, work and markets | 17 | `economy`, `contract posted`, `contract taken`, `contract done`, `contract given back`, `stall rented`, `stall sale`, `stall closed`, `lease`, `lease ended`, `prices reset`, `times cause`, `times nudged`, `nudge over`, `harvest called`, `harvest over`, `archive work` |
| Letters and notices | 8 | `letter sent`, `letter delivered`, `letter returned`, `resident letter`, `notice posted`, `notice answered`, `notice removed`, `crier` |
| Law, crime and pacts | 10 | `warrant`, `arrest`, `fine paid`, `reported`, `told the wardens`, `vouched to the wardens`, `stopped by the watch`, `stolen goods smelt`, `released`, `pact sealed` |
| Town life | 16 | `festival`, `festival called`, `festival entered`, `festival won`, `project posted`, `project proposed`, `project built`, `project gift`, `project cancelled`, `project removed`, `project ruined`, `event added`, `event begins`, `event notice`, `event posted`, `event rolled` |
| Fame, journal and tales | 12 | `deed`, `deed revoked`, `milestone`, `storyline begun`, `storyline objective`, `storyline step`, `storyline done`, `tale begun`, `tale joined`, `tale ended`, `held for a story`, `story prize` |

Beside these a trigger can listen for a **`story signal`** (a name sent by another trigger or by a timer), and an
exit can wait for **`missions completed`**.

What the fields mean is known for the events the story engine itself makes:

| Event | actor | target | detail | Other |
|---|---|---|---|---|
| `region entered`, `region left` | the wolf | the region's id | the run | the first tile inside, or the last tile inside |
| `scene arrived`, `scene spoken`, `scene posed`, `scene blocked` | the resident | the run | the author's cue | `quantity` = the visit; the tile, except when blocked |
| `scene choice` | the initiating wolf, or none | the run | `cue.option` | `quantity` = the visit |
| `scene response` | as above | the run | `cue.beat.submitted` or `cue.beat.timeout` | the typed words are never logged |
| `scene finished`, `scene failed` | as above | the run | the cue | |
| `story phase` | the run | the phase | the story's title | |
| `enter place` | the wolf | the cell entered | the cell left | |
| `gift`, `gift again` | the giver | the receiver | | `item`, `quantity`, `coins` |
| `conversation` | the wolf | the resident | | |
| `bandit falls` | the wolf who landed the last blow | | | the tile where the bandit fell |

**For the other 140 or so, nothing says what the fields hold.** The catalogue the tools return
(`story_runner.catalogue()`) is a list of names. An author has to read the game's source or copy an example. This
is the first gap in "the existing trigger options": not too few events, but events with no description.

48 of the 160 can be evidence for a mission objective (`MISSION_EVENTS`, `story_program.py:238`): the ones a
wolf does. 13 of those record a `quantity` and 18 record `coins` that an objective can add up
(`MISSION_MEASURES`).

### 1.4 Conditions: 7

A condition is a tree (`satisfied`, `story_triggers.py:229`; the same in `RatwStoryTriggers.cpp:48-91`). Groups
nest five deep with at most eight children.

| Test | Parameters | True when |
|---|---|---|
| `all` | `conditions` (1 to 8) | every child is true |
| `any` | `conditions` (1 to 8) | one child is true |
| `not` | `condition` | the child is false |
| `field` | `field` (`actor`, `target`, `cell`, `item`, `detail`, `quantity`, `coins`), `op`, `value` | the event's field compares so. Text takes `=` and `!=`; `quantity` and `coins` take `= != > >= < <=` against a number. |
| `counter` | `scope` (`story` or `player`), `key`, `op`, `value` | the counter compares so against a number. A `player` counter is the acting wolf's; for a wolf outside the audience it is false. |
| `region` | `region` | the event's tile is inside that rectangle. An event with no tile is *unknown*, and unknown never fires, not even under `not`. |
| `mission` | `id` | that mission of the current phase has reached its completion threshold. Phase triggers only. |

That is all. A condition can look at **the event that just happened, the story's own counters, and its own
missions**. It cannot look at the world: where a wolf is now, what they carry, the hour, the weather, a
faction's stance, whether a resident is alive, how many wolves stand in a region, what another story is doing.
It cannot compare two counters, or a counter with an event field, and it has no arithmetic.

### 1.5 Actions: 7, and world effects: 25

An action list has 1 to 12 actions, run in order (`fire`, `story_triggers.py:261`).

| Action | Parameters | What it does |
|---|---|---|
| `set` | `key`, `scope` (`story`, `player`), `value` (a number, or `quantity` or `coins` of the event) | Sets a counter. Clamped to plus or minus 1,000,000,000. |
| `add` | the same | Adds to a counter. |
| `emit` | `signal` | Sends a named signal. It keeps the original event's actor, item, target, place, amounts and detail. |
| `schedule` | `signal`, `days`, optional `clock` (`game` or `real`), `owner` (`story` or `player`), `scope` (`visit` or `campaign`), `repeat` | Starts a named timer that sends the signal when due. The same name for the same owner replaces the deadline. The shortest delay is a minute (1/1440 of a day). |
| `cancel` | `signal`, optional `owner` | Stops that timer. |
| `effect` | `effect` (one of the 25 below) | Asks the game to change the world. `player` may be `$actor`, the wolf who caused the event, on a `participant` trigger. |
| `enter` | `phase` | Moves the story to a phase. Must be the last action. |

The world effects (`EFFECTS`, `story_program.py:173-199`). Each becomes one order in the game's DM queue
(`dm.actions`) and is checked again by the game when it is applied:

| Effect | Parameters | What the game does |
|---|---|---|
| `scene.play` | `scene`, `player` (a wolf, `$actor`, or `world` for a public scene), `cue` | Plays an embedded scene. |
| `npc.line` | `npc`, `text` | Phase entry only. The resident has this line for the story's wolves while the phase is open. |
| `npc.walk` | `npc`, `cell`, `x`, `y`, `pace` (`walk`, `run`), `cue` | The resident goes there; `scene arrived` (or `scene blocked` after 180 s without progress) follows. |
| `npc.speak` | `npc`, `text`, `cue` | The resident says it aloud; `scene spoken` follows. |
| `npc.pose` | `npc`, `posture` (`standing`, `sitting`, `lying`), `cue` | `scene posed` follows. |
| `npc.move` | `npc`, `cell`, `x`, `y` | Puts the resident there (no walk). |
| `npc.protect`, `npc.unprotect` | `npc` | Marks the resident as one who cannot be killed, or unmarks. |
| `npc.revive` | `npc` | Brings a dead resident back. |
| `story.area.spawn` | `region`, `count` (1 to 6) | A bandit camp in the rectangle. |
| `story.area.damage` | `region`, `points` (1 to 99) | Hurts the story's wolves standing in it. |
| `story.area.item` | `region`, `item`, `quantity` (1 to 100) | Gives each of them the item. A full pack is skipped and told. |
| `story.area.buff` | `region`, `buff` (only `fed`), `hours` (1 to 168) | The timed fed benefit. |
| `character.injury` | `player`, `add` (26 injuries), `severity` (1 to 3) | Injures a wolf. |
| `story.practice` | `player`, `skill` (`craft`, `commerce`, `labour`, `service`, `travel`, `watch`) | A lesson, under the ordinary practice rules. The wolf must be in the world. |
| `deed.award` | `player`, `detail` | A deed on the wolf's fame. The wolf must be in the world. |
| `storyline.give` | `player`, `template`, `resident`, `place` | Starts a personal quest from a DM template (`Data/Storylines/templates.json`: two exist, `the-debt` and `a-gathering`). Refused at three personal quests. |
| `bandits.call` | `player`, `count` (1 to 6) | Bandits come for that wolf. The wolf must be in the world. |
| `festival.call` | `town`, `name`, `inDays` (0 to 30) | Schedules a festival. |
| `project.post` | `town`, `project`, `cell`, `x`, `y`, `title` | Opens a town project. |
| `faction.relation` | `faction`, `other`, `stance` (six), `disposition` (-100 to 100), `reason` | One faction's stance toward another. The runner writes `live.faction_relations` itself, then asks the game to read it. |
| `times.nudge` | `market`, `points` (-100 to 100, not 0), `days`, `words` | Pushes a market's times up or down for a while. |
| `harvest.override` | `area`, `word` (`poor`, `fair`, `good`, `clear`), `days`, `words` | Sets a harvest. |
| `band.override` | `npc`, `band` (five, or `clear`), `days` (1 to 112) | A household's prosperity band for a while. |
| `ship.storm` | `ship` | A ship is stormbound. |

The game has **about 40 more DM orders that a story cannot use** (`ACTIONS` and `LIVE_ACTS`,
`tools/dungeon_master.py:156` and `2253`), among them `character.move`, `character.grant` (coins to a wolf, with
a reason, journalled), `npc.kill`, `visitor.add` and `visitor.leave` (a temporary resident), `mantle.assign` and
`mantle.release` (doc 84: a resident's dialogue), `event.add` and `event.cancel` (a market event),
`project.complete` and `project.cancel`, `deed.revoke`, `storyline.tick`, `board.remove`, `estate.hold`. They are
the cheapest new actions there are: the game already applies them, with receipts.

### 1.6 Memory: counters, timers, phases, firing rules

- **Counters** are whole numbers named by the author. A `story` counter is shared; a `player` counter belongs to
  each wolf of the audience. They start at 0, live as long as the run, and survive a runner restart. A counter of
  0 or 1 is the switch. There is **no declaration**: a misspelt name is a new counter that reads 0, and nothing
  warns.
- **Timers** are named. Clock `game` counts days of the world's calendar; `real` counts 24-hour days since
  publication and keeps time when the game is down. Lifetime `visit` dies with the phase visit; `campaign`
  survives phase changes. Owner `player` gives every wolf their own deadline under one name. A repeating timer
  that missed several turns while the runner was down fires once. There is also the older `clock` event: a delay
  since the phase opened.
- **Phases** are the only "where are we" the story has. There is no way to hold two threads of a story open at
  once other than campaign triggers and counters.
- **Firing rules** (`once`): `each` (every match, StarCraft's "Preserve Trigger"), `visit` (once per phase
  visit), `player` (once per wolf per visit), `campaign` (once per run), `member` (once per wolf per run).
  `audience: participant` limits a trigger to wolves of the story.
- **What a trigger cannot do to another trigger:** turn it on or off, run it, or wait part-way through its own
  actions. The workarounds are a counter used as a switch in the other trigger's condition, a signal, and a timer
  plus a second trigger.

### 1.7 Regions

A region is a named rectangle of tiles in one cell or interior, both corners included (`regions`, up to 128;
`Core/RatwStoryRegion.h`). The game is told the story's regions with each phase and writes `region entered` and
`region left` for the story's wolves (`Game::observeStoryTile`, `RatwGame.cpp:2415`). A condition can ask whether
the event happened inside one. Four area effects act on the wolves standing in one.

Limits: one cell; a rectangle only; fixed for the life of the run (no "move location"); a wolf who signs in
already inside makes no `region entered`; and there is no "how many are inside now".

### 1.8 Missions: today's quests

A mission is the story's quest object (`validate_missions`, `track_missions`, `story_program.py:290-470`). Up to
16 per phase, each with 1 to 8 objectives **in order**. An objective is an event of a wolf of the audience (one
of the 48), with filters, and a goal: so many occurrences, or so much `quantity` or `coins` added up. A later
objective can require the same value as the first one saw (`sameAsFirst`: the same notice, the same loan).

| Play style | Who makes progress | Ready when |
|---|---|---|
| `individual` | each wolf has their own checklist | the set number or percentage of the audience has finished |
| `group` | one shared checklist | it is finished and enough different wolves contributed (2 or more) |
| `either` | one shared checklist | it is finished |

A mission is optional when no exit names it. Exits of the kind `missions completed` move the story on. Progress
is shown in each wolf's journal and kept per visit for the DM (`dm.story_transitions.missions`).

What a mission is not: it has no reward of its own (a trigger must give one), it cannot fail, its objectives
cannot be hidden or done in any order, it ends with its phase, it knows nothing of parties, and **only a wolf of
the audience frozen at publication can make progress**.

### 1.9 Where it is kept

| Table | Migration | Holds |
|---|---|---|
| `dm.story_releases` | 0067 | One row per approved program: `id` (`world-<hex>`), `digest`, the whole `program`, `approved_by`, `ticket_hash`, `status` (`approved`, `running`, `paused`, `complete`, `stopped`), **`state`** (one JSON: phase, visit, audience, counters, timers, votes, missions, what has fired, effects waiting), `cursor` (the last event read), `receipts`, `problem`. |
| `dm.story_transitions` | 0067, 0068 | One row per phase visit: `phase`, `game_day`, `cause`, `receipts`, `missions` (progress at the end of the visit). |
| `dm.story_trigger_history` | 0069 | One row per trigger fired: `visit`, and `entry` (phase, day, actor, the trigger, its actions, the event's id and kind, the signal, the tile). Written in the same transaction that moves the cursor. Never pruned. |
| `dm.story_signals` | 0067 | One-use tickets for a DM direction or a guest line: hashed, good for an hour. |
| `dm.actions` | 0013 | The DM queue the game reads: `kind`, `target_id`, `payload`, `status` (`queued`, `applied`, `refused`, `expired`), `result`. A row's id is the **receipt**. |
| `game.events` | 0021, 0070 | The world's log, written by the game inside each save. |
| `dm.story_residents` | 0067 | A view of residents for reference checks and the author's search. |

There are no tables for counters, timers or objectives: they are inside `state`.

### 1.10 From an idea to a running story

1. **Author.** In Story Creator, "World stories": start blank, from one of the 8 examples, from one of the 100
   library events, or import JSON. The draft is one JSON text in the browser's `localStorage`, one per world per
   account. An AI or a player can write one offline through `tools/story_author.py`.
2. **Rehearse.** "Rehearse all paths" sends the draft to `story_program.review`. Each rehearsal case is a made-up
   audience, a list of facts with game days, and the phase path expected (and optionally the effects, mission
   credits, triggers fired and final counters). The reply shows, per case, the path, a trigger trace, counters,
   credits and effects, and lists phases and triggers no case reached. A rehearsal has a budget: 200,000 steps or
   8 seconds, two at a time.
3. **Approve.** A DM approves the exact version rehearsed (the digest covers the whole document). Every phase
   must have been reached by a passing case. `references()` (`story_runner.py:76`) checks some ids against the
   world. The row is saved as `approved` and a publication ticket, good for a day, comes back.
4. **Publish.** The ticket starts the run. The audience is frozen now: the listed wolves, or every character
   active in the last 1 to 90 days, **at most 500**. One live run per program id per world.
5. **Run.** `python3 tools/story_runner.py --world KEY`, beside the game. Every 2 seconds, for each running
   story, in one transaction (`tick_run`, `story_runner.py:468`):
   - if an effect or a missions update is waiting for its receipt, look at it and stop there;
   - deliver the phase's entry effects, one per tick, each only after the one before was applied;
   - then one waiting trigger effect per tick, the same way;
   - then read up to 128 new events past the cursor and pass each to the rules (a `clock` fact first, then the
     event). Rules are evaluated by the native library; missions, exits, votes and timers by Python;
   - write the trigger history, the new state and the cursor together.
6. **In the game.** The game takes orders from `dm.actions` at most once a second (`Game::applyDmActions`,
   `RatwGame.cpp:1109`), applies each, and answers `applied` or `refused` with a reason. An order nobody took
   within ten minutes is `expired`. A refusal or an expiry **pauses the run** and records the reason.
7. **Direct.** While a run is in a phase with a `dm signal` exit, a DM can issue a one-use ticket for that
   direction, or for one guest line of up to 200 characters for a named resident. Tickets are redeemed through
   the author bridge, not in Studio.
8. **End.** A phase with nothing to wait for completes the run. "Stop run" ends it early; effects already
   delivered stay.

The Python twin and the native engine must agree: Studio and the author bridge may run without the library and
fall back to Python, but the runner refuses to start without it (`story_runner.py:860`).

### 1.11 Limits and known problems

Limits, all enforced when a program is checked:

| What | Limit |
|---|---|
| Phases per program | 100 |
| Triggers per phase, and campaign triggers | 64 each, counted together per phase |
| Actions per trigger | 12 |
| Conditions | 8 per group, 5 levels |
| Entry effects per phase; exits per phase | 12; 16 |
| Missions per phase; objectives per mission | 16; 8 |
| Regions; embedded scenes; guest voices per phase | 128; 32; 8 |
| Audience | 500 characters |
| Rehearsal cases; facts per case | 32; 256 |
| A request to Studio | 1 MiB |
| Phase steps the game keeps for the journal | the last 32 |

Known problems. The first five are the ones left open by our review.

1. **A scene's outcome saves the game on the game thread.** When a scene beat settles (a vote, a reply, a
   resident arriving) the game calls `Game::save` at once so the runner sees the event quickly:
   `RatwGameCinematics.cpp:640` and `RatwGameStorylines.cpp:566`. A crowded scene can hitch. (Noted in
   `Docs/story-programs.md`, "Known for later".)
2. **A DM's direction is refused on a busy world.** `direct()` refuses a signal while any event at all lies past
   the run's cursor ("World events are waiting", `story_runner.py:724-727`), and refuses to issue a ticket until
   every entry effect of the phase is acknowledged (`696-703`). On a busy world there is nearly always an event
   waiting.
3. **The runner reads about 64 events a second per run.** 128 rows per 2-second poll (`story_runner.py:517`,
   `868`). It reads nothing at all while one effect waits for its receipt, and each effect costs a runner tick
   plus a game poll. A phase with twelve entry effects takes about half a minute to finish opening. The whole state,
   audience included, is rewritten every tick.
4. **References that are not checked.** `references()` checks residents, players, factions, places and cells in
   effects, some tiles, and three catalogues. It does not check `ship`, `area`, `town` or `market` ids, the
   tiles of `npc.move` and `project.post`, anything in an **exit's** filters or prerequisites, or `actor`,
   `item`, `detail` in a trigger's filters. Signal and counter names are never cross-checked. A wrong id shows
   up as a refusal that pauses the live run, or as a trigger that never fires.
5. **Audiences over 500.** An "active players" story cannot be published on a world with more than 500 active
   characters (`story_runner.py:292`, `311`); the game has the same cap (`RatwGameStorylines.cpp:576`).
6. **One wolf's refusal pauses everyone's story.** `story.practice`, `deed.award` and `bandits.call` are refused
   when the wolf is not in the world; `storyline.give` at three personal quests; a host's own wolf is refused
   rewards when cheats are off. Any of these stops the run for the whole audience until a DM retries. Only the
   full-pack case was softened (`a86daab3`).
7. **Up to half a minute before an event is seen.** Events reach `game.events` only with a save: every 30
   seconds, or 3 seconds after `saveSoon()`. Then the runner's 2 seconds, then the game's 1. "A wolf walks into
   the region and the gate guard speaks" can take 35 seconds. Scenes avoid this by the synchronous save of
   problem 1.
8. **A stopped game is silent.** Orders are expired by the game itself. With the game down, an order stays
   `queued` and the run simply waits; nothing tells the DM. A scratch server never applies orders.
9. **No downtime of the runner is visible either.** Nothing records that the runner is alive.
10. **Stop does not wait.** The run is marked `stopped` before the game has acknowledged `story.stop`.
11. **The trigger history grows without end**, and the game keeps only the last 32 phase steps for the journal.

What doc 34's Part 5 planned and this engine does not have: a cast chosen by a query ("a merchant in Ridgemere
who trusts the wolf"), holds on residents so two stories do not fight over one, priorities between stories, a
story that starts on a schedule or on a trigger rather than by a DM's hand, conditions over the world's facts,
"pause all", and migrating a run to a new version.

### 1.12 The UI today

Screenshots are in `/home/martinb/.claude/jobs/ee03c49e/tmp/shots-story-ui/`. They are the real components
(`StoryCreator` and everything under it) mounted alone against a read-only stand-in for Studio's route, with
Ridgemere 3's map and roster read from its database; nothing was written. **Ridgemere 3 has no releases at all**
(`status` returned an empty list on 2026-11-21), so shots 08 and 09 show two SAMPLE runs, made by passing the
first facts of a bundled rehearsal case through the real rules.

**Dungeon Master, "Story Creator" tab** (`DmApp.tsx:86`, `StoryCreator.tsx`). Five buttons:

| Button | Component | What it is |
|---|---|---|
| World stories (the default) | `StoryProgramPanel.tsx` (162 lines) | The trigger system. The editor on the left, "Releases in <world>" on the right. Shot `01-world-stories-blank.png`. |
| Scenes | `SceneWorkspace.tsx`, `SceneBeatEditor.tsx`, `ScenePreview.tsx` | Write, preview and publish a scene. Shot `10-scenes-workspace.png`. |
| Write draft, Inspect rehearsal | inline in `StoryCreator.tsx`, `lib/storyRehearsal.ts` | The older browser-only draft. Reaches no server. Shot `11-old-write-draft.png`. |
| Personal storylines and storytellers | `StorytellersPanel.tsx` (Players tab) | Doc 58: applications, tales, "Give a personal storyline". |

**The editor, top to bottom** (`StoryPhaseEditor.tsx`, 153 lines):

1. A file bar: New blank story, Start from a story (the 8 examples), Import a program.
2. Embedded scenes (`EmbeddedScenes.tsx`): embed or update a scene from the scene library. No remove.
3. Campaign settings: id, title, audience (active for N days, or a text box of character ids), opening phase.
4. **Map regions** (`StoryRegions.tsx`, 102 lines): pick a cell, **drag a rectangle on the real map**
   (`MapView` in paint mode), or type the four corners. Renaming a region rewrites its uses. Shot
   `07-regions-on-the-map.png`.
5. Campaign phases: one button per phase, Add phase. No remove and no reorder except in the JSON.
6. The phase's journal text.
7. A switch between **Phase triggers** and **Campaign triggers**.
8. **The trigger editor** (`StoryTriggerEditor.tsx`, 266 lines). Shots `02-trigger-overview-reed-ford.png` and
   `03-trigger-inspector-conditions.png`.
   - A **trigger overview**: three columns, Event, Conditions, Actions, one row per trigger, each cell a short
     description (`rope >= 4 or charter >= 1`, `Award a deed → Enter crossing`). This is close to a map editor's
     view, read-only.
   - New trigger, Move earlier, Move later, Remove. A "Find a trigger" box once there are more than six.
   - **Components from the 100 examples**: search, pick an example, pick a part (an opening, 205 objectives, a
     participation rule, 13 outcomes), and copy it in as a trigger or a journal objective. Shot
     `04-components-from-100-examples.png`.
   - **Signal connections** for the selected trigger: who sends and who receives each of its signals, as buttons
     that jump there.
   - The selected trigger as three boxes of form fields. **Event**: a search box over one list of 160 names with
     no groups or descriptions; "Whose event"; "Fire"; five free-text filters. **Conditions**: nested boxes, each
     with a "Test" list; changing the test of a group throws its children away. **Actions**: a type list and that
     type's fields, with Move and Remove.
   - World effects are one generic form (`StoryEffectFields.tsx`, 47 lines) driven by the catalogue. A scene and
     a region are lists to choose from. **Every other id is typed by hand**: residents, wolves, cells, items,
     factions, towns, markets, ships, areas, templates, projects. The one aid for a tile is "Copy center tile
     from region".
9. Journal missions (`StoryMissions.tsx`): a browser of the 100 complete examples, and a form per mission. Shot
   `05-missions-exits-river-relief.png`.
10. Phase exits and entry effects: the older "when ... go to ..." list, guest speakers, the effects on opening.
11. Rehearsal cases (`StoryRehearsalEditor.tsx`): a form per case and per fact. Expected effects and credits can
    only be typed as JSON.
12. The whole program as JSON in a text box.
13. Export draft, **Rehearse all paths**, then the results, "Effects for approval", **Approve this version**,
    and **Publish approved story**. Shot `06-rehearsal-cases-and-results.png`.

What the editor does not have: folders or groups, comments, enable or disable, duplicate, copy and paste, undo,
multi-select, drag to reorder, a picker for anything but scenes, regions and phases, a list of problems (one
error sentence at a time, with no jump to its place), autocomplete or rename for signals and counters, a view of
the whole program's triggers at once, drafts kept on the server, or more than one draft per world. If the JSON
does not have the expected shape the whole form disappears without a word and only the text box is left. A
viewer can draft and rehearse but sees no Releases panel (shot `12-viewer-no-releases.png`).

**The running-story view** is the "Releases" panel beside the editor (`StoryProgramPanel.tsx:124-152`). Shots
`08-releases-running-sample.png` and `09-releases-paused-sample.png`.

| An admin can see | How |
|---|---|
| The runs of this world (100 newest): title, status, the problem if paused | a list of buttons |
| For one run: how many wolves are eligible (a number, no names) | a sentence |
| How many have "voted" on each counting exit | "Trigger 2 has 3 participants" |
| Counters | the raw JSON |
| Active timers | name, clock, due day |
| Run history: each phase visit, its day, its cause (`event 4102`, `clock`, `DM signal: accord`), mission completions | a list, 100 a page |
| Live trigger history: trigger, phase, day, actor, tile, actions | a list, 50 a page |
| The game's receipts: kind, status, result | a list |
| Guest lines issued | a list |

| An admin can do | Notes |
|---|---|
| Refresh | **By hand only.** Nothing refreshes itself. |
| Stop run | With a confirmation. |
| Retry refused or expired action | The last one only. The button shows for `refused` but **not for `expired`**, though the server accepts both (a small bug). |
| Allow direction "X" once; Allow one guest line for a resident | Shows a ticket to copy. Studio itself cannot use the ticket. |
| Search the world's names and ids; read a resident's history | For copying ids into the forms by hand. |

What an admin **cannot** do to a running story, none of which exists in the runner or the route either: pause
and resume it, set a counter, start, fire or cancel a timer, fire a trigger or send a signal directly, turn a
trigger off, move to another phase, complete or reset an objective for a wolf, add or remove a wolf, skip a
refused action, message the wolves taking part, see who they are, where they are or how far each has got, see
which triggers are waiting and why they have not fired, see the program a run is running, or be told that a run
has stalled. The current phase is not even labelled: it is the top row of the history.

---

## 2. StarCraft and Warcraft III, and the gaps

### 2.1 StarCraft (StarEdit, and SCMDraft after it)

A StarCraft trigger has three parts: the **players** it runs for, **conditions**, and **actions**. There are no
events. The game checks every trigger's conditions for each of its players about every two seconds, top to
bottom, and runs the actions of each trigger whose conditions all hold. A trigger runs **once** unless its
actions include **Preserve Trigger**.

- **Per player.** A trigger owned by "Force 1" runs separately for each player in it; "Current Player" in an
  action means the one it is running for.
- **Conditions** (all must hold; there is no "or", authors copy the trigger): Always, Never, Bring (a player has
  N units at a location), Command, Kill, Deaths, Accumulate (resources), Score, Opponents, Elapsed Time,
  Countdown Timer, Switch, and the comparisons Most, Least, Highest, Lowest.
- **State.** 256 **switches** (true or false; set, clear, toggle, randomize). **Death counters**: a whole number
  per player per unit type, which authors use as their variables and their per-player timers (add 1 every
  cycle). One **countdown timer** on screen.
- **Locations**: named rectangles on the map, plus "Anywhere". **Move Location** re-centres one on a unit, which
  is how a region follows something.
- **Actions**: Create, Kill, Remove, Move, Order and Give units (at a location); Modify hit points and energy;
  Set Invincibility; Set Doodad State (a door); Display Text Message; **Transmission** (a unit's portrait, a
  line of text, a sound, the unit flashes on the minimap) and Talking Portrait; Minimap Ping; Center View;
  **Set Mission Objectives**; **Leaderboards** (kills, points, resources, control at a location, with a goal);
  Set Countdown Timer, Pause Timer; Set Resources, Score, Deaths, Switch; Set Alliance Status; Run AI Script;
  Pause Game; **Wait** (milliseconds; it blocks that player's other waiting triggers, a famous trap);
  Preserve Trigger; Comment; Victory, Defeat, Draw; Set Next Scenario (the next map of a campaign).
- **The editor**: a flat list of triggers per player, each shown as its sentences ("Current Player brings at
  least 1 men to 'Bridge'."), with Copy, Move Up, Move Down. The parameters in a sentence are links: click one
  to choose a unit, a location, a number. SCMDraft adds text editing of triggers and larger limits. A **Comment**
  action renames the trigger in the list. No test mode: you start the map.

### 2.2 Warcraft III (the World Editor's Trigger Editor)

A Warcraft III trigger is **Events, Conditions, Actions**. The game calls a trigger only when one of its events
happens, checks its conditions, and runs its actions in order.

- **Events**: map initialisation; time elapsed; every N seconds; a timer expires; a unit enters or leaves a
  region; a unit dies, is attacked, picks up an item, starts a spell, comes within range; a player types a chat
  message; a player leaves; a **dialog button** is clicked; a player skips a cinematic; a variable's value
  becomes equal to something.
- **Conditions**: comparisons of any two values of one type (a number, text, a unit, a player, a boolean), with
  And, Or and Not. Values are built from **functions**: "(Owner of (Triggering unit))", "(Number of units in
  (Units in Region 001))", arithmetic.
- **Variables**, declared in a **variable manager** with a type, an initial value and optionally an **array**:
  integer, real, boolean, string, unit, unit group, player, player group, point, region, timer, dialog, quest,
  leaderboard, and many more.
- **Regions**: rectangles drawn on the map in the editor and used by name; they can be moved by a trigger.
  **Timers** as variables, with a timer window on screen. **Unit groups** and "Pick every unit in ... and do".
- **Flow**: If / Then / Else with several actions in each branch; For each integer from A to B; Pick every
  unit or player; **Wait** (game seconds or real seconds); **Run trigger** (checking or ignoring its
  conditions); **Turn on** and **Turn off** a trigger; Skip remaining actions; a trigger queue.
- **Quests**: Create Quest (required or optional, title, description, icon, discovered or not), **quest
  requirements** (the lines under a quest), mark a quest or a requirement completed or failed, quest messages
  ("QUEST UPDATED"), the Quests screen (F9).
- **Cinematics**: cinematic mode (letterbox, controls off), transmissions from a unit with a portrait, camera
  objects and pans, fades, and "skip cinematic".
- **Dialogs**: a window with a title and **buttons**; a trigger waits for the click. **Leaderboards** and
  **multiboards** (a table on screen). Floating text, pings, sounds, music.
- **The editor**: a tree of **folders** (categories) and triggers on the left; each trigger with a checkbox
  **Enabled** (off: ignored altogether) and **Initially On**; a comment box; the three lists as sentences with
  clickable parameters, each opening the right picker; **copy, cut and paste** of triggers, events, conditions
  and actions; copy as text; a **variable manager**; search; and **Custom script**: a line of JASS, or convert a
  whole trigger to text. Debugging is by "Game - Display text message". A **game cache** carries values from one
  map of a campaign to the next.

### 2.3 The gap table

"Have" is what the engine does today (section 1). "Matters" is a judgement for this game: a wolf MUD with quests,
campaigns, living residents, parties, factions, an economy, weather and a calendar, the dialogue window (doc 83)
and scenes.

| Capability | StarCraft | Warcraft III | We have | Matters here | Section |
|---|---|---|---|---|---|
| Event, conditions, actions | conditions polled | yes | **Have.** Events are the world's log; nothing is polled. | The core. Keep. | 1.2 |
| A trigger runs for each player on its own | "Current Player" | "Triggering player" | **Have**: `$actor`, `audience: participant`, per-wolf counters, timers and firing rules | Yes | 1.6 |
| Once, or every time | Preserve Trigger | on by default; Turn off | **Have, and finer**: each, per visit, per wolf, per run | Yes | 1.6 |
| And, or, not | and only | yes | **Have**: 5 levels | Yes | 1.4 |
| Switches | 256 | boolean variables | **Partly**: a counter of 0 or 1; no declaration, no toggle or randomize | Yes: a named, declared switch reads better and catches typos | 3.2 |
| Whole-number variables | death counters | integer | **Partly**: counters, undeclared, no counter-to-counter comparison, add and set only | Yes | 3.2 |
| Text, wolf, resident, place and item variables | no | yes | **No** | Yes: "the wolf who found the pup", "the witness we picked" | 3.2 |
| Arrays and groups | no | arrays, unit groups, player groups | **No** (only "the audience" and per-wolf counters) | Yes, small: "the wolves who signed", "each party member" | 3.2 |
| Arithmetic and functions in conditions | compare to a constant | full | **No** | Partly: compare two values, count, a little arithmetic. No scripting. | 3.4 |
| Conditions on the state of the world | Bring, Command, Accumulate | any function | **No**: only the event's own fields | **Yes, the largest gap**: where a wolf is, what they carry, the hour, the weather, standing, who is alive | 3.4 |
| Regions drawn on the map | locations | regions | **Have**: rectangles, one cell, drawn in the editor | Yes | 1.7 |
| Regions that move, or that are not rectangles | Move Location | Move Region | **No** | Small: "around a resident, N tiles" covers it | 3.3 |
| How many are inside a region now | Bring | Units in Region | **No** (only crossing events) | Yes: "three wolves at the gate" | 3.4 |
| Timers | one countdown; death-counter tricks | timers as variables, windows | **Have, and better**: named, game or real clock, repeating, per wolf, surviving phases | Yes. Missing only: shown to players. | 1.6, 3.5 |
| Periodic and time-of-day events | Elapsed Time | every N s; time of day | **Partly**: a repeating timer; no hour, weekday, season or festival-day events | **Yes**: this world has a calendar (28-day months, Marketday, Restday) | 3.3 |
| Wait inside an action list | Wait | Wait | **No** (a timer and a second trigger) | Yes, as sugar over timers; tight timing belongs in a scene | 3.5 |
| If / then / else inside actions | no | yes | **No** (two triggers with opposite conditions) | Yes: halves the trigger count | 3.5 |
| Loops: for each, pick every | no | yes | **No** | Yes, bounded: each participant, each party member, each wolf in a region | 3.5 |
| Run another trigger | no | yes | **Partly**: signals | Yes, as a readable form of a signal | 3.5 |
| Turn a trigger on and off; initially off | no | yes | **No** (a counter in its condition) | **Yes**: authors expect it, and a live admin needs it | 3.5, 5 |
| Random | switch randomize | random number | **No** | Yes, repeatable in rehearsal | 3.5 |
| Folders, comments | Comment | categories, comments | **No**: phases are the only grouping | Yes | 4 |
| Sentences with clickable parameters | yes | yes | **Partly**: the read-only overview row | **Yes**: the heart of "like the map editor" | 4 |
| Copy, paste, duplicate, undo, search | copy, move | all | **No** (search over six triggers) | Yes | 4 |
| Variable manager | no | yes | **No** | Yes | 4 |
| Pickers for the things a parameter can be | yes | yes | **No**, except scene, region, phase: ids are typed | **Yes** | 4 |
| Custom script | no | JASS | **No**, on purpose | **No.** A story must stay checkable and rehearsable. The JSON and the AI bridge are the escape hatch. | 3.1 |
| Test before it is live | start the map | Test Map | **Have, and better**: rehearsal cases with expected paths, coverage of phases and triggers, required for approval | Yes. A strength to keep. | 1.10 |
| Create, kill, move, order units | yes | yes | **Partly**: move, walk, pose, speak, protect, revive a resident; a bandit camp. No spawn of a named or temporary resident, no removal, no "follow", no "attack" | Yes, for residents and wild things. Never for a player's wolf beyond what a DM may do. | 3.5 |
| Give or take items and resources | Set Resources, Give Units | items, gold | **Partly**: an item to everyone in a region; a deed; a lesson. No item or coins to one wolf, none taken | **Yes**: quest rewards | 3.5, 3.6 |
| Doors, gates | Set Doodad State | destructibles, gates | **No** | Yes: "the gate opens when..." | 3.5 |
| Text to a player | Display Text Message | game messages, floating text | **No** (only the journal entry of a phase, and a resident's line) | **Yes** | 3.5 |
| Transmission: a face, a line, a ping | yes | yes | **Partly**: `npc.speak`; scenes have face panels. No ping, no line from someone far away | Yes, small | 3.5 |
| Mission objectives | Set Mission Objectives (text) | quests and requirements | **Have, and richer**: missions with counted objectives, three play styles, per-wolf progress in the journal | Yes. Missing: optional shown as optional, hidden, any order, fail, rewards, parties, a wolf joining later. | 3.6 |
| Quest discovered, completed, failed | no | yes | **Partly**: completed only | Yes | 3.6 |
| Leaderboard, multiboard | yes | yes | **No** | Small: a tracker on the HUD for a few counters ("Bandits felled 2 / 3") | 3.5 |
| Cinematics | no | yes | **Have**: scenes with a cast, lines with reading time, moves, postures, a camera point, a movement lock, private, party or public audience | Yes. Missing: camera moves and fades; minor. | 1.1 |
| Dialogs with buttons | no | yes | **Partly**: a scene's choice beat (first answer or majority vote). Nothing in the dialogue window | **Yes**: a story's choice in a resident's dialogue window is how a quest is offered | 3.5 |
| Chat message as an event | no | yes | **Partly**: a scene's written reply | No, beyond that. Players speak in the window (doc 84). | |
| Alliances | Set Alliance Status | yes | **Have**: `faction.relation` | Yes. Missing: a wolf's own standing. | 3.5 |
| AI scripts | Run AI Script | AI | **Different**: residents live by their own rules; a story may give a line, move one, or (new) bind a mantle | The living world is the AI. Stories steer it lightly. | 3.5 |
| Weather, time of day | no | yes | **No** | Yes for weather in a place for a while. **No** for the calendar: a story never moves the world's clock. | 3.5 |
| Victory, defeat | yes | yes | **Partly**: an ending phase | Yes: an outcome per run and per wolf, for the credits (doc 58) | 3.5 |
| The next map; values carried over | Set Next Scenario | game cache | **No**: one program, one run; a run cannot start another | **Yes, for "entire campaigns"**: chapters that start each other, and values kept for the world | 3.5 |
| Debugging messages | text | text | **Partly**: the trigger history, after the fact | **Yes**: section 5 is this | 5 |
| Watching and steering a running map | none (it is the game) | none | **Almost none** | **The user's main ask.** A map editor never needed it; a living world does. | 5 |

What we have that neither editor has: rehearsal with coverage before anything is live, approval by a named
person against an exact version, receipts for every change to the world, per-wolf everything, timers on two
clocks that survive restarts, and a library of 100 worked examples. None of that is to be lost.

---

## 3. Expanding the triggers

### 3.1 Rules for the expansion

1. **Still data, never script (decided, mine).** No custom script, no expressions typed as text. Every event,
   condition and action is an entry in one catalogue with typed parameters. That is what lets a program be
   checked, rehearsed, shown as sentences, written by an AI, and approved by a person who can read it.
2. **One catalogue file (decided, mine).** `Data/Storylines/trigger-catalogue.json` lists every event, condition,
   action and effect: its id, group, a **sentence** with typed slots ("`{npc}` walks to `{tile}` at a
   `{pace}`"), a line of help, what the fields of an event hold, and what it needs (a rank, "acts on wolves").
   `story_runner.catalogue()` serves it. The validator, the editor's sentences and pickers, the console, the
   author bridge and the guide are all generated from it. Today these facts are scattered over `EVENTS`,
   `EFFECTS`, `validate_effect`, `StoryEffectFields.tsx` and `describeAction`.
3. **Format version 2, version 1 still runs (decided, mine).** `story_program.validate` accepts both; a version
   1 program is read as version 2 with everything new absent. Exits and missions stay valid and are shown in the
   editor as what they are: triggers that enter a phase, and quests that last one phase.
4. **Python and C++ stay twins.** Every new condition and flow action is added to `story_triggers.py` and
   `RatwStoryTriggers.cpp` together, with the parity test that exists.
5. **Deterministic.** A new condition reads only what arrived with the event log (3.4), and "random" is seeded,
   so a rehearsal and a replay give the same answer.
6. **A story may do nothing a DM could not do by hand.** Every new world effect is a DM order in `dm.actions`,
   applied and refused by the game under the rules it already has (the host's-own-wolves guard, "may act on
   wolves", the economy's rules). No effect mints coins: a coin reward is paid from a named purse (3.5; **decided
   (user, 2026-11-21)**, question 4).
7. **Nothing kills or moves a player's wolf.** `character.kill` and `character.move` stay out of stories
   (decided, mine). `character.injury` and `story.area.damage` stay, behind "may act on wolves" as today.

### 3.2 State: variables, switches, counters, arrays

Today: undeclared whole-number counters in two scopes. Proposed: a `variables` list in the program.

```json
"variables": [
  {"id": "gate-open",   "type": "switch",  "scope": "story",  "initial": false, "note": "set by the smith"},
  {"id": "ropes",       "type": "number",  "scope": "story",  "initial": 0, "show": "Ropes delivered"},
  {"id": "hauled",      "type": "number",  "scope": "player", "initial": 0},
  {"id": "finder",      "type": "wolf",    "scope": "story"},
  {"id": "witness",     "type": "resident","scope": "story",  "initial": "rm119"},
  {"id": "signed",      "type": "wolves",  "scope": "story"},
  {"id": "found-clues", "type": "texts",   "scope": "party"}
]
```

| Piece | Definition | Engine support |
|---|---|---|
| Declared variables | Every variable has an id, a type, a scope and a first value. An undeclared name is a validation error. Version 1 counters are declared automatically as numbers. | `story_program.validate`; `state.counters` becomes `state.vars` with the same two stores, read both. |
| Types | `number`, `switch`, `text` (up to 200 characters), and references: `wolf`, `resident`, `place`, `region`, `item`, `faction`. A reference holds an id and is checked when the program is approved if it has a first value. | Twin and native: values are JSON scalars; comparisons by type. |
| Lists (the arrays) | `numbers`, `texts`, `wolves`, `residents`: an ordered set of up to 500 values. This covers "unit group" and "player group". | Twin and native. |
| Scopes | `story` (one value), `player` (one per wolf), **`party`** (one per party taking part: 3.6), and **`world`** (kept for the world after the run ends, shared by every story: the game cache). | `party`: the runner needs party membership (3.4, facts). `world`: a table, `dm.story_world_vars`. |
| `show` | A label. A shown number or switch appears in the wolf's journal under the story as a tracker (the leaderboard). | The `story.phase` and `story.missions` payloads carry shown values; the client's journal draws them. |

### 3.3 New events

An event is cheap when the game already does the thing and only has to write a row (`World::recordEvent`,
`RatwWorld.cpp:3324`). "Watch" means the game writes it only for what a live story asked it to watch, the way
`regions` are sent with `story.phase` today, so an unwatched world pays nothing.

| Group | Event | Definition | Engine support |
|---|---|---|---|
| Players and parties | `signed in`, `signed out` | A wolf of the story comes into or leaves the world. (`arrival` and `departure` exist; these are the same rows under names that say what they are, and they carry the tile.) | Catalogue aliases; no game change. |
| | `party joined`, `party left`, `party formed`, `party ended` | A wolf joins or leaves a party; target = the leader. | `Core/RatwGameParty.cpp`: record on each change. |
| | `wolf downed`, `wolf recovered` | `downed` exists; add the recovery. | One `recordEvent`. |
| | `skill practised`, `injury healed` | A lesson taken; an injury gone. | `RatwPractice`, `RatwInjury`: one row each. |
| Residents and the window | `dialogue opened`, `dialogue closed` | A wolf opens or leaves a resident's dialogue window (doc 83). | `RatwGameDialogue.cpp`. |
| | **`dialogue choice`** | A wolf picks a story's choice in a resident's window; detail = the choice's id (see `npc.offer`, 3.5). | `RatwGameDialogue.cpp`, with the offer. |
| | `resident arrived`, `resident left` (watch) | A watched resident reaches or leaves a watched place or region. | Watch list; the resident's own movement code. |
| | `resident died` | `death` with a resident as target, under a clear name, with who did it. | Alias, and fill `actor`. |
| | `mantle bound`, `mantle released` | Doc 84: a resident takes or drops a mantle. | `RatwMantle`: one row each. |
| Places, regions, doors | `region occupied`, `region emptied` (watch) | The count of story wolves in a region goes from 0 to 1, or back to 0; `quantity` = the count. Also `region count` when it crosses a number the story named. | `observeStoryTile` already tracks who is inside; add the count. Fixes "signed in inside the region": signing in counts. |
| | `near resident` (watch) | A story wolf comes within N tiles of a watched resident (the moving location). | The sight code already knows who is near whom. |
| | `door opened`, `door closed`, `door tried` | A wolf uses, or is stopped by, a watched door. | Doors have `open` and `locked` (`struct Door`, `RatwWorld.h:502`); record on use. |
| | `place entered` | `enter place`, kept, with the door used in `item`. | Alias. |
| Items, economy, shops | `item picked up`, `item dropped`, `item crafted`, `item used`, `item sold`, `item bought` | With `item`, `quantity`, `coins`, and the shop or resident as target. `stall sale` and `gift used` exist; these complete the set. | One row at each place the pack changes hands (`RatwPack`, `RatwMarket`). The noisiest group: watch only the items a story names. |
| | `purse crossed` (watch) | A named town, house or faction purse crosses an amount. | The economy's monitor has the numbers; watch list. |
| | `price crossed` (watch) | A good's price in a market crosses a factor. | As above. |
| Time, weather, calendar | `day begins`, `hour` (watch), `weekday` | The world's day turns; a named hour arrives; it is Marketday or Restday. | `RatwCalendar`: rows at the boundaries a story watches. |
| | `season begins`, `month begins` | | One row each; rare. |
| | `weather changed` (watch) | The weather in a watched cell changes; detail = the new weather. | `RatwWeather.cpp`. |
| | `night falls`, `dawn` | | Calendar; rare. |
| Factions, standing, crime | `relation changed` | A faction's stance toward another changes; detail = the new stance. | `factions.sync` and the game's own changes. |
| | `standing crossed` (watch) | A story wolf's standing with a named faction crosses a number. | `RatwStanding`. |
| | `crime witnessed`, `gaoled`, `warrant cleared` | To go with `warrant`, `arrest`, `fine paid`, `released`. | `RatwCrime`: rows. |
| Quests | `quest offered`, `quest started`, `objective done`, `quest done`, `quest failed`, `quest abandoned` | Made by the story engine itself (3.6), as signals with the quest and objective in `detail`. | Runner only. |
| Stories | `story started`, `story ended` | Another run of this world starts or ends; target = the program's id, detail = its outcome. | The runner already writes `story phase`; add the two, by program id and not run id, so one chapter can wait for another. |
| | `variable changed` | A named variable of this story changes, by a trigger or by an admin. | Runner only; no cycles (checked like signals). |
| | `admin signal` | An admin sent a named signal from the console (section 5). Triggers may listen to it, unlike `dm signal` today. | Runner only. |
| | `trigger fired` | Another trigger of this story fired. | Runner only. |

And for the existing 160: **write down what each one's fields hold** in the catalogue, by reading each
`recordEvent` call once. No engine change; it is the cheapest improvement in this section.

### 3.4 New conditions

Two families.

**Over the story's own state** (the runner has it all):

| Condition | Definition | Engine support |
|---|---|---|
| `compare` | Two values of one type compared: a variable, an event field, a constant, or a small function of them: `count(list)`, `a + b`, `a - b`, `min`, `max`. Replaces `counter` and `field`, which stay as spellings of it. | Twin and native. |
| `switch` | A switch is on, or off. | Twin and native. |
| `in list` | A value is in a list variable ("the acting wolf is one of `signed`"). | Twin and native. |
| `phase is`, `visit is` | The story is in a phase; this is the Nth visit. For campaign triggers. | Twin and native: already passed in. |
| `objective`, `quest` | An objective or quest is done, failed, open or hidden, for the acting wolf, their party, or everyone. Generalises `mission`. | Python (missions are Python today). |
| `timer` | A named timer is running; it has less than N left. | Twin and native: timers are passed in. |
| `fired` | A trigger has fired at least N times this visit or this run. | Needs a count beside the `fired` sets. |
| `chance` | True N times in 100, seeded by the run, the event and the trigger. | Twin and native: one hash. Rehearsal cases may pin the roll. |
| `days since` | N game days, or real days, since the phase opened, the run began, or a named moment (a variable set to "now"). | Twin and native. |

**Over the world's facts.** The runner sees only events, so today a condition cannot ask "is the wolf carrying
rope". Proposed (**decided, mine**): **facts travel with the event log.** At each save, in the same transaction
that writes `game.events`, the game also writes, for each live run, one small document of the facts that run's
program names:

```
game.story_facts(world_id, run, as_of_event bigint, facts jsonb)
facts = {"wolves": {"<id>": {"here": true, "cell": "...", "x": 31, "y": 36, "party": "<leader id>",
                             "carries": {"rope": 3}, "standing": {"ridgemere_watch": 42}, "injured": false}},
         "regions": {"breach": ["<id>", "<id>"]},
         "residents": {"rm111": {"alive": true, "cell": "...", "x": 12, "y": 9, "mantle": "carter"}},
         "world": {"day": 412.3, "hour": 14, "weekday": "Marketday", "season": "winter"},
         "weather": {"w_ridgemere_1_0": "snow"}, "relations": {"ridgemere_watch>road_company": "tense"}}
```

The runner reads the document whose `as_of_event` covers the event it is handling. A fact is therefore as old as
the event beside it and never newer, a replay gives the same answers, and a rehearsal case can state facts the
same way it states events. What goes into the document is worked out at approval by scanning the program's
conditions (the **watch list**, sent with `story.phase` as `regions` are today), so a story that asks nothing
costs nothing.

| Condition | Definition | Facts it reads |
|---|---|---|
| `wolf in region`, `count in region` | The acting wolf (or a named one) stands in a region now; at least N story wolves do. This is StarCraft's Bring. | `regions` |
| `wolf is here` | The wolf is in the world now (signed in). | `wolves.here` |
| `carries` | The wolf carries at least N of an item. | `wolves.carries`, for named items only |
| `in party`, `party size`, `party has` | The wolf is in a party; of at least N; with a named wolf. | `wolves.party` |
| `standing` | The wolf's standing with a faction compares to a number. | `wolves.standing` |
| `injured`, `downed` | | `wolves` |
| `resident is` | A resident is alive, is in a place or region, wears a mantle. | `residents` |
| `time is` | The hour is between two hours; the weekday is; the season is; it is night. | `world` |
| `weather is` | The weather in a cell is. | `weather` |
| `relation is` | One faction's stance toward another is at least or at most. | `relations` |
| `world variable` | A `world`-scope variable compares. | `dm.story_world_vars` |

Unknown stays unknown: a fact that is missing (the wolf is offline and has no tile) makes the condition unknown,
and unknown does not fire, as with `region` today. The editor says so beside the sentence.

### 3.5 New actions

**Flow** (all in the twin and the native engine; none touches the game):

| Action | Definition | Notes |
|---|---|---|
| `if` | `{"do": "if", "if": condition, "then": [actions], "else": [actions]}`. Nests three deep. | The 12-action limit becomes 24 counting nested ones. |
| `wait` | `{"do": "wait", "days": 0.5, "clock": "game"}` or `"seconds": 30` on the real clock: the rest of the list runs later, for the same acting wolf, with the same event. | Compiled to a hidden one-shot timer that remembers the trigger, the position and the event (`state.waiting`). It dies with the phase visit unless the trigger is a campaign trigger. Shortest real wait: 5 seconds, and it is coarse (the runner's tick); tight timing belongs in a scene. Never blocks other triggers (StarCraft's trap). |
| `each` | `{"do": "each", "over": "participants", "actions": [...]}`, where `over` is `participants`, `party`, `region:<id>` or `list:<var>`, with `$each` as the wolf. At most 500 turns; no nesting. | Needs effects delivered in batches (below), or 500 rewards take a quarter of an hour. |
| `random` | `{"do": "random", "choices": [{"weight": 3, "actions": [...]}, ...]}`. Seeded. | |
| `run` | Run another trigger's actions now, `check` its conditions or not. | A private signal underneath; cycles refused as for signals. |
| `enable`, `disable` | Turn a named trigger on or off. With a trigger's new field `"enabled": false` (initially off). | `state.disabled`, a set, honoured by both twins. An admin flips the same set (section 5). |
| `stop` | Skip the remaining actions. | |
| `note` | A line for the DM: it appears in the campaign's timeline and nowhere in the world. The debugging message. | Trigger history entry only. |
| `finish` | End the run with an outcome: `success`, `failure` or `abandoned`, and a line for the chronicle. For one wolf (`$actor`) or everyone. | Replaces "a phase with nothing in it" as the way to end; feeds doc 58's credits. |

**State:**

| Action | Definition |
|---|---|
| `set`, `add` (kept) | Now on declared variables, and the value may be a constant, an event field, another variable, or `count(list)`. |
| `subtract`, `multiply`, `clamp` | Arithmetic on a number. |
| `turn on`, `turn off`, `toggle` | A switch. |
| `remember` | Put an id from the event into a reference variable: `{"do": "remember", "var": "finder", "from": "actor"}`. |
| `list add`, `list remove`, `list clear`, `pick from list` | A list variable; `pick` takes one at random (seeded) into a variable. |
| `set world variable` | A `world`-scope variable. |
| `schedule`, `cancel` (kept); `shift timer`, `show timer` | Move a deadline; show a named timer to the story's wolves as a countdown in the journal. |

**World effects.** Each is a DM order. "Exists" means the game applies that order today and a story only needs
to be allowed to send it; "new" means C++ work in the game.

| Group | Effect | Definition | Engine support |
|---|---|---|---|
| Players, parties | `story.tell` | Text to `$actor`, a wolf, a party, the participants, the wolves in a region, or the world; as a line in the journal, a banner, or a letter. Optionally "from" a resident, with their face. | **New** order and a client message. The single most missed action. |
| | `story.ping` | Mark a tile on the minimap and World Map of the story's wolves for N minutes, with a label. | **New**; the maps already draw party members (`9902c12b`). |
| | `story.tracker` | Show or hide shown variables and a timer on the HUD. | With `show` (3.2). |
| | `participant.add`, `participant.remove` | Take a wolf into the story, or out (3.6). | **New** order `story.cohort`; the runner's audience becomes a living list. |
| | `character.grant` | Coins to a wolf, with a reason, **from a named purse** (a town, a House or a faction): refused when the purse is short. | **Exists**, journalled, capped (`GrantMost`); the purse is new (Phase 9). **Decided (user, 2026-11-21)**, question 4: never minted. |
| | `item.give`, `item.take` | An item to or from one wolf (not a whole region). A full pack is skipped and told, as today. | **New**, on the model of `story.area_item` (`RatwStoryArea.cpp`). |
| | `standing.adjust` | A wolf's standing with a faction, by a few points, with a reason. | **New**; `RatwStanding`. |
| Residents | `visitor.add`, `visitor.leave` | A temporary named resident appears at a tile for N minutes, or leaves: the spawn. | **Exists** (LIVE's "spawn"). |
| | `npc.offer` | While the phase is open, a resident's dialogue window shows a story choice to the story's wolves: the wolf's words, the resident's reply, once or always. Picking it makes `dialogue choice`. | **New**, in doc 83's trees: a story branch laid over the resident's tree. This is the quest giver, and the dialog with buttons. |
| | `mantle.assign`, `mantle.release` | Bind a mantle (doc 84) to a resident for the story, and release it. | **Exists**. |
| | `npc.hold`, `npc.release` | Keep a resident at their post or with the story, off their schedule, so two stories or their own day do not take them (doc 34's "holds"). | **New**; `RatwSchedules`. Released when the run ends, always. |
| | `npc.follow`, `npc.stop following` | A resident travels with a wolf or a party ("a story" is already a reason a resident may travel with a party: `RatwParty.h:24`). | **New** order over what exists. |
| | `npc.kill`, `npc.remove` | A resident dies, or a temporary one goes. | `npc.kill` **exists**; never on a protected resident. |
| | `belief.plant`, `npc.brief` | A rumour in a few residents; a topic and a stance for a resident for a while (doc 34, Part 6). | **New**; `RatwTopics`, `RatwTalk`. Later. |
| Places, doors | `door.lock`, `door.unlock` | A named door, for the story's duration at most. | **New**; doors have `locked`. Unlocked again when the run ends. |
| | `board.post`, `board.remove` | A notice on a place's board. | `board.remove` **exists**; post is **new**. |
| | `story.area.*` (kept) | And `story.area.tell`: text to everyone standing in a region. | With `story.tell`. |
| Items, economy | `item.place` | An item on the ground at a tile, for the story's wolves to find. | **New**. |
| | `event.add`, `event.cancel`, `prices.reset` | A market event; undo one; reset prices. | **Exist**. |
| | `project.complete`, `project.cancel` | Finish or cancel a town project the story posted. | **Exist**. |
| Time, weather | `weather.set` | The weather in a cell or a town for N hours, then back to the season's. **Decided (user, 2026-11-21)**, question 6: a story may set the weather. | **New** order; the game already has the command behind it for its dev tools (`wire::environmentCommand`, `RatwGame.cpp:5842`). |
| | (none for the calendar) | **A story never changes the day or the hour (decided, mine; kept by the user, 2026-11-21).** | |
| Factions, crime | `faction.relation` (kept) | | |
| | `warrant.issue`, `warrant.clear` | A warrant on a wolf, or lifted. Needs "may act on wolves". | **New**; `RatwCrime`. |
| Quests | `quest.offer`, `quest.start`, `quest.complete`, `quest.fail`, `objective.complete`, `objective.fail`, `objective.reveal`, `objective.progress` | 3.6. | Runner, and the journal payloads. |
| | `storyline.give` (kept), `storyline.tick`, `deed.award` (kept), `deed.revoke` | | The two beside the kept ones **exist**. |
| Scenes | `scene.play` (kept), `scene.stop` | End a playing scene. | **New**, small. |
| | New beats in a scene: `camera` (pan to a tile over N seconds), `fade` | | `RatwGameCinematics.cpp` and the client. Later. |
| Stories | `story.start` | Start another approved program of this world, with the same audience or the wolves in a list, when no run of it is live: the next chapter. | Runner: `publish` without a ticket, for a program approved together with this one as a **campaign**. |

**Delivering effects in batches (needed by `each`, and it fixes known problems 3 and 6).** Today one effect is
sent, awaited, then the next. Proposed: a trigger's effects go to the game as one order, `story.effects`, with a
list; the game applies them in order and answers with a result for each. And each effect says what to do when
the game refuses it: `"ifRefused": "pause"` (today's behaviour, and the default for world changes), `"skip"`
(the default for a reward to one wolf: note it and carry on), or `"when here"` (hold it until that wolf is next
in the world, for up to N days). The run pauses only for what the author said matters.

### 3.6 Quests

"Small quests" must be easy; "entire campaigns" must be possible. Today's missions are close for a campaign and
wrong for a small quest, for one reason above all: **the audience is frozen when the story is published**. A
quest board, a carter who always needs an axle, a newcomer's first errand: none can be made, because the wolf who
walks up tomorrow is not in the audience.

**A quest** replaces the mission (a mission is read as a quest with `lifetime: phase`):

```json
"quests": [{
  "id": "axle", "title": "An axle for the carter", "summary": "The carter at the ford is stuck.",
  "scope": "player",              // player | party | story
  "lifetime": "campaign",         // phase (today's mission) | campaign
  "repeat": "once",               // once | daily | weekly | always
  "objectives": [
    {"id": "ask",   "label": "Ask the carter which part gave way.", "on": "conversation", "match": {"target": "rm111"}},
    {"id": "bring", "label": "Bring a replacement axle.", "on": "gift", "match": {"target": "rm111", "item": "axle"},
     "after": ["ask"], "goal": 1, "measure": "quantity"},
    {"id": "wheel", "label": "A spare wheel would not go amiss.", "optional": true, "on": "gift",
     "match": {"target": "rm111", "item": "cart_wheel"}},
    {"id": "thief", "label": "Find who cut the axle.", "hidden": true, "on": "story signal", "signal": "thief-named"}
  ],
  "fail": [{"on": "resident died", "match": {"target": "rm111"}}, {"after": {"days": 3, "clock": "game"}}],
  "rewards": [{"kind": "deed.award", "player": "$actor", "detail": "Got the carter moving."},
              {"kind": "character.grant", "player": "$actor", "coins": 12, "reason": "the carter's thanks"}]
}]
```

| Primitive | Definition | Today |
|---|---|---|
| Scope `player` | Every wolf who has the quest has their own copy and progress. | `individual` |
| Scope `party` | The party the wolf was in when the quest started shares one copy; its members are fixed then (as a scene's `party` audience is). A wolf who leaves the party keeps their credit so far and no more. | no |
| Scope `story` | One copy for everyone; with `contributors: N` for "at least N different wolves". | `group`, `either` |
| Objectives in any order | `after: [ids]` says what must come first; with none, an objective is open from the start. The old strict order is `after` the one before. | ordered only |
| Optional | Shown as optional in the journal; not needed to finish. | a whole mission can be optional; an objective cannot |
| Hidden | Not shown until `objective.reveal`, or until its event happens. | no |
| Counted | `goal` and `measure` (occurrences, quantity, coins), as today, plus a goal read from a variable. | yes |
| Fail states | Events or a deadline that fail the quest; `objective.fail` and `quest.fail` as actions. A failed quest says so in the journal and sends `quest failed`. | no |
| Rewards | Effects delivered when the quest is done, to each wolf with credit, with `ifRefused: when here`. A reward list for optional objectives too. | by hand, in a trigger |
| Lifetime | `phase`: ends with the phase visit. `campaign`: lasts across phases. | phase only |
| Repeat | A wolf may take it again after a day, a week, or at once. | no |
| Offer and accept | `quest.offer` shows it in a resident's window (`npc.offer` underneath) or on a notice board; the wolf accepts or declines; `quest.start` gives it without asking. A wolf may abandon a quest from the journal. | no: every mission is given to everyone |
| Progress by hand | `objective.complete`, `objective.progress`, `quest.complete`, from a trigger or from an admin (section 5). | no |

**Who may take part** (the audience), version 2:

| Audience | Meaning |
|---|---|
| `players` (kept) | A fixed list. |
| `active` (kept) | Everyone active in the last N days, frozen at publication. |
| **`open`** | Nobody at first. A wolf becomes a participant when a trigger says so (`participant.add`: on accepting an offer, on entering a region, on signing in), up to a cap. This is the quest board and the standing errand. |
| **`everyone`** | `active`, and each newcomer who signs in is added. For a world campaign that should not shut out the wolf made yesterday. |

With these, **a small quest is one quest and no phases**: an open audience, a `quest.offer` on a resident, and the
quest. The editor's "New small quest" makes exactly that (section 4). **A campaign** is phases, campaign
triggers, several quests, scenes, and chapters started by `story.start`.

A living audience needs per-wolf state out of the one JSON blob: `dm.story_player_state(run, player, state
jsonb)` for each wolf's variables, quests and timers, so a tick rewrites only the wolves it touched and the cap
of 500 can go (known problems 3 and 5). Decided (mine): the cap becomes 5,000 per run, configurable, once this
table exists.

How this sits with doc 58: the game's journal (`storylines::Book`) stays the one place a wolf reads their quests,
and a story's quests are shown there as now. The 24 storyline templates and `storyline.give` stay. Small quests are written
**here**, in the Story Maker, with the templates kept as they are and offered as starting points (**decided (user,
2026-11-21)**, question 2).

---

## 4. The authoring UI: a trigger editor in the Warcraft III manner

### 4.1 Decisions

1. **One workbench replaces the stack of collapsible forms (decided, mine).** The left is a tree, the middle is
   the selected trigger as sentences, the right is what the sentences refer to (variables, regions, the map,
   problems). Rehearsal sits along the bottom.
2. **Sentences come from the catalogue (3.1).** No component knows what `npc.walk` is; it draws "`{npc}` walks to
   `{tile}` at a `{pace}`" and gives each slot the picker for its type.
3. **Nobody types an id.** Every slot has a picker: residents and wolves by name, places and tiles on the map,
   items from `Data/Items/items.json`, factions, towns, markets, ships, phases, triggers, variables, signals,
   scenes, quests. A typed id stays possible ("Enter an id") for the offline author.
4. **Exits and triggers are shown as one thing.** A phase's exits appear as triggers in a folder "Exits" of that
   phase. The file keeps both forms.
5. **Drafts are kept on the server (decided, mine)**, in `dm.story_drafts`, many per world, with a name, an
   owner and saved versions. The browser's copy becomes a cache for unsaved edits. A viewer's drafts stay in
   their browser, as now.
6. **The older "Write draft" and "Inspect rehearsal" tabs are retired (decided, mine).** Nothing reads their
   format. "New small quest" (below) is what they were for.
7. **The JSON box stays**, as a tab called "Source", for import, export, and whatever the forms cannot yet do.

### 4.2 The workbench

```
 Story Creator ▸ World stories ▸ The breach at Greyfen          [Saved 14:02 ▾] [Problems 2] [Rehearse ▶] [Approve…]
┌─ Triggers ───────────────────┬─ count-defeat ───────────────────────────────────┬─ Variables ───────────────────┐
│ [Find…                  ]    │ [x] Enabled  [x] Initially on   Fires: every time ▾ │ + New variable                │
│ ▾ Campaign (always on)       │ Whose: a wolf of the story ▾    Folder: Defence   │ line-held   number story   0  │
│     ☑ open-ledger            │ Note: each bandit felled inside the gap counts    │ defeats     number player  0  │
│ ▾ Phase: hold  ●opening      │                                                   │ gate-open   switch story  off │
│   ▾ Defence                  │ ─ Events ───────────────────────────────────────  │ finder      wolf   story   -  │
│     ☑ count-defeat        ◀  │   ⚑ A bandit falls                                │───────────────────────────────│
│     ☑ dispatch-runner        │   + Add event                                     │─ Regions ─────────────────────│
│   ▾ Exits                    │ ─ Conditions (all of these) ────────────────────  │ breach  Greyfen (29,34)-(34,38)│
│     ☑ three days pass        │   ◆ The event happened inside  breach             │ [ map: the cell, regions      │
│ ▾ Phase: council             │   + Add condition   + Add group (any / all / not) │   drawn, drag to draw ]       │
│   ▸ Muster (8)               │ ─ Actions ──────────────────────────────────────  │───────────────────────────────│
│   ▸ Scene cues (4)           │   1 Add  1  to  defeats  of the acting wolf       │─ Problems ────────────────────│
│ ▸ Phase: relief              │   2 Add  1  to  line-held                         │ ✖ council/call-birch: unknown │
│ ▸ Phase: secured   ■ending   │   3 If  line-held  ≥  3  then                     │   resident "rm_brich"   [Go]  │
│ ▸ Quests (2)                 │       Send signal  check-breach                   │ ⚠ signal "releif" is sent but │
│ ▸ Scenes (1)                 │     else                                          │   nothing listens       [Go]  │
│ [+ Trigger] [+ Folder]       │       Tell the acting wolf  "Hold the line."      │                               │
│ [+ Phase]   [+ Quest]        │   + Add action                                    │                               │
├──────────────────────────────┴───────────────────────────────────────────────────┴───────────────────────────────┤
│ Rehearsal ▾ "A shared defence"  ✔ pass   hold → council → relief     Trace: count-defeat ×3 → dispatch-runner …  │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

- **The tree.** Campaign triggers, then each phase, then quests and scenes. Folders are free (a `folder` text on
  each trigger; the engine ignores it). The checkbox is **Enabled**; a hollow box is "initially off". Drag to
  reorder or to move between folders and phases; order inside a phase is the order of evaluation, and the tree
  says so. Right-click: Duplicate, Copy, Cut, Paste, Rename (rewriting every `run`, `enable` and signal that
  names it), Disable, Delete, "Copy as text". Multi-select with Shift and Ctrl.
- **Sentences.** Every underlined word is a slot. Click it and the picker for its type opens in place. A slot
  that fails validation is red, with the problem as its tooltip. Conditions nest as indented groups headed "all
  of these", "any of these", "not". Actions nest under If, Else, For each, Random.
- **Adding** an event, condition or action opens a searchable list **grouped as in section 3** (Players and
  parties; Residents; Places, regions and doors; Items and economy; Time, weather and calendar; Factions,
  standing and crime; Quests; Scenes; Variables; Flow), each with its one line of help and, for an event, what
  its fields hold.
- **Undo and redo** (Ctrl+Z, Ctrl+Shift+Z) over the whole draft. **Search** (Ctrl+F) over ids, sentences, text,
  and "where is this variable, signal, resident or region used".
- **Problems** lists every problem at once, errors and warnings, each with [Go]. Errors come from the program's
  own check; warnings from the world ("unknown resident", "this tile is outside the cell", "nothing listens for
  this signal", "this trigger was reached by no rehearsal").

### 4.3 Pickers

```
 Choose a resident                              Choose a tile                           Choose an event
┌───────────────────────────────────┐          ┌────────────────────────────────────┐  ┌──────────────────────────────────┐
│ [carter                    ]      │          │ Cell [Ridgemere, Northgate ▾]      │  │ [gift                       ]    │
│ ○ the acting wolf                 │          │ ┌────────────────────────────────┐ │  │ Gifts, items and loans           │
│ ○ a variable ▾  witness           │          │ │   (the map; click a tile;      │ │  │  ⚑ A wolf gives a gift           │
│ ● a resident:                     │          │ │    regions and the residents   │ │  │     actor: the giver             │
│   Ivo Tarmarsh · carter · rm111   │          │ │    already placed are drawn)   │ │  │     target: who receives it      │
│   Hale Reedcart · carter · rm204  │          │ └────────────────────────────────┘ │  │     item, quantity, coins        │
│ ○ Enter an id                     │          │ (31, 36)   or centre of region ▾   │  │  ⚑ A gift is used                │
└───────────────────────────────────┘          └────────────────────────────────────┘  └──────────────────────────────────┘
```

A slot of type `wolf` offers: the acting wolf, the wolf of a loop, a wolf variable, or a search of characters.
`resident`: a variable or the roster. `tile` and `region`: the map (`StoryRegions.tsx` and `MapView` already do
the drawing). `item`: the item catalogue with its categories. `text`: a text box that shows the 200-character
limit and lets `{variable}` be inserted.

### 4.4 New story, and the library

```
 New…
 ( ) Small quest      one quest, anyone who asks may take it       → asks: who gives it, what to do, the reward
 ( ) Campaign         phases, missions, scenes                     → blank, or from one of the 8 examples
 ( ) From the library 100 events in 10 categories  [search…]       → as a small quest, or as a mission in this phase
 ( ) Import           a .json program, or a folder of triggers
```

The library's parts (an opening, 205 objectives, 100 participation rules, 13 outcomes) stay, and become
**templates in the Add lists**: "Add from library…" in the tree pastes a part as triggers into the selected
folder. Any selection of triggers can be saved as the DM's own template (a `ratw-triggers` fragment: triggers
plus the variables and regions they use) and exported or imported as a file.

### 4.5 Test and rehearse

Rehearsal keeps its rules. What changes is how it is shown and written:

- The bottom strip shows each case as pass or fail with its path; opening one shows the **trace beside the
  tree**: each trigger that fired gets a count in the tree, and each that did not is dimmed, so coverage is seen
  where the triggers are.
- A case's facts are written with the same event picker and slots as a trigger, with the day beside each.
  Expected effects, credits and counters get forms.
- **Step through**: play a case one fact at a time and watch the variables panel, the phase and the journal
  change. This is the same `explain` the console uses (section 5.6), on a rehearsal's state.
- "Rehearse here": make a new case that starts from the selected phase with given variable values.

### 4.6 How it maps onto the code

| New | Replaces or reuses | Server |
|---|---|---|
| `Editor/src/dm/triggers/Workbench.tsx` (layout, the draft store with undo) | the body of `StoryPhaseEditor.tsx`; `StoryProgramPanel.tsx` keeps the file bar, rehearse, approve and publish | |
| `triggers/TriggerTree.tsx` | the phase strip, the phase/campaign switch, the overview ledger of `StoryTriggerEditor.tsx` | |
| `triggers/Sentence.tsx`, `triggers/slots.tsx` | `describeCondition`, `describeAction`, `ConditionRow`, the action forms, `StoryEffectFields.tsx`, `SceneCueFilter.tsx` | `catalog` returns the catalogue |
| `triggers/pickers/*.tsx` | `StoryPicker.tsx`, `RegionTilePicker`, the datalists | new op `refs` (`story_runner.atlas` grown: towns, markets, ships, areas, items, characters, by kind) |
| `triggers/Variables.tsx` | nothing (new) | |
| `triggers/Problems.tsx` | the single `feedback` line | new op `problems`: `story_program.problems(p)` collects every failure with its path, where `validate` raises on the first; plus `story_runner.references` as warnings, covering what it skips today (known problem 4) |
| `triggers/Rehearsal.tsx` | `StoryRehearsalEditor.tsx`, the results list | `rehearse` unchanged; `explain` for stepping |
| `triggers/QuestEditor.tsx` | `StoryMissions.tsx` | |
| `StoryRegions.tsx`, `MapView.tsx`, `EmbeddedScenes.tsx`, the Scenes workspace | kept as they are | |
| Drafts | `localStorage` key `ratw.story-creator.v1.[world,user].world-program` | migration: `dm.story_drafts(id, world_id, name, owner, program jsonb, saved_at, version)`; ops `draft.list`, `draft.save`, `draft.open`, `draft.delete` |

`Editor/src/render.smoke.tsx` gains the workbench with each bundled example, so a program that makes it throw
fails a test.

---

## 5. Live campaign control: the Campaigns console

The user's main ask. A map editor has no such thing, because a StarCraft map is over in an hour and its author is
not there. Here a campaign runs for ninety game days in a world other people live in, and the admin is there.

### 5.1 Decisions

1. **A tab of its own, "Campaigns", in the Dungeon Master suite, beside LIVE (decided, mine).** Authoring stays
   in Story Creator. The "Releases" panel leaves `StoryProgramPanel.tsx`; approving a story there ends with a
   link "Open in Campaigns".
2. **It refreshes itself**, every 2 seconds while the tab is visible, as LIVE does (`POLL_MS`,
   `LiveTab.tsx:41`), by asking only for what is newer than what it has.
3. **Every intervention is a command in an inbox that the runner carries out (decided, mine)**, not a change
   made from Studio's request. A new table, `dm.story_commands`. The runner takes a run's commands first in each
   tick, in order, inside the same transaction as the trigger history. So the state has one writer, a command
   lands between two world events and never "while events are waiting" (known problem 2 goes away), and each
   command and its result are in the story's own history.
4. **An intervention can release only what was approved.** Firing a trigger, entering a phase or completing an
   objective runs effects that are in the approved program, under the approval it already has. Anything else
   ("do this now") is an ordinary DM action, sent the way LIVE sends one, with LIVE's permissions.
5. **Nothing is undone.** Going back to an earlier phase does not take back what the world was given. The
   console says so each time.
6. **Every intervention has a name, a time, a reason and a before and after**, in `dm.audit` and in the
   campaign's timeline.

### 5.2 The list

```
 Campaigns · Ridgemere 3                                         runner ● alive 1 s ago · 0 events behind      [↻ 2 s]
┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐
│ ⚠ 1 needs you   [All ▾] [Running 2] [Held 0] [Paused 1] [Ended 14]                              [Pause all…]      │
├────────────────────────────┬─────────┬──────────────┬──────────┬───────────┬──────────────────┬──────────────────┤
│ Story                      │ Status  │ Phase        │ Wolves   │ Quests    │ Last happened    │ Waiting on       │
├────────────────────────────┼─────────┼──────────────┼──────────┼───────────┼──────────────────┼──────────────────┤
│ Ninety days on the winter  │ running │ deep-freeze  │ 14 of 31 │ 2 open    │ log-watch-round  │ timer watch-round│
│ ferries                    │         │ day 28 of 90 │ here now │ 9 of 31 ✔ │ 3 h ago          │ in 2.1 days      │
│ Two ways across Reed Ford  │ PAUSED  │ crossing     │ 1 of 2   │ -         │ open-crossing    │ you: deed.award  │
│                            │         │              │          │           │ 6 min ago        │ refused [Open]   │
│ An axle for the carter     │ running │ (quest)      │ 3 of 8   │ 8 taken   │ quest done · Ash │ wolves           │
│                            │ ⚠ quiet │              │          │ 5 done    │ 2 days ago       │                  │
└────────────────────────────┴─────────┴──────────────┴──────────┴───────────┴──────────────────┴──────────────────┘
```

Each row answers: is it alive, where is it, who is in it, when did anything last happen, and **what is it
waiting for** (a wolf to do something, a timer, the game to acknowledge an order, or the admin).

### 5.3 One campaign

```
 ◀ Campaigns ▸ Ninety days on the winter ferries        running · visit 3 · world-9f2c…  v. a41c (approved by martin)
 [⏸ Hold] [⏹ Stop…] [Send signal ▾] [Go to phase ▾] [Tell the wolves…] [Do in the world ▾]          [Open program]
 enrol ──▶ ferry-watch ──▶ [deep-freeze] ──▶ ferries-reopen        Now: day 49.3 · in this phase 21.3 days
                                     └────▶ limited-crossings      Next: watch-round in 2.1 days (≈ 50 min)
┌ Overview │ Timeline │ Triggers │ Variables & timers │ Quests │ Wolves & map │ Orders │ Audit ──────────────────────┐
│ WAITING FOR                                                                                                      │
│  ⚑ log-watch-round      when signal watch-round            timer due day 56.0                          [Fire…]   │
│  ⚑ offer-a-coil         when a wolf gives  rope  to  rm111   would fire for: Ash, Birch (here now)      [Fire…]   │
│  ⚑ spring-council       when signal spring-council         timer due day 90.0                          [Fire…]   │
│     ◆ stores ≥ 12   ✘ (stores is 5)   → then enter ferries-reopen, else enter limited-crossings                  │
│ NEEDS YOU                                                                                                        │
│  (nothing)                                                                                                       │
│ LATELY                                                                                                           │
│  day 49.0  log-watch-round fired (timer) · watches 5 → 6                                                         │
│  day 42.0  log-watch-round fired (timer) · watches 4 → 5                                                         │
└──────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

**Timeline** is one list, newest first, made by merging what is kept in four places: phase visits
(`dm.story_transitions`), triggers fired (`dm.story_trigger_history`), orders and their receipts (`dm.actions`),
and the admin's commands (`dm.story_commands`).

```
│ [All ▾] [Phases] [Triggers] [Orders] [Quests] [Admin]   wolf [any ▾]   trigger [any ▾]            [Earlier…]     │
│ day 49.0 · 21:14  ⚑ log-watch-round   (campaign)  ← timer watch-round                                            │
│                     set watches 5 → 6                                                    event #48211 [Show]     │
│ day 44.6 · 20:31  ✎ martin set  stores  3 → 5      "two coils were handed over before the story saw them"        │
│ day 28.0 · 18:02  ▶ entered deep-freeze  (visit 3)  ← begin-deep-freeze ← timer                                  │
│                     → band.override rm204 poor 28 d      order #5512  applied 18:02:07                           │
│                     → story.tell participants "The river has closed."   order #5513  applied 18:02:09           │
│ day 10.2 · 15:40  ⚑ offer-a-coil  · Birch Oakenfall · Ridgemere, Northgate (31, 36)       event #47002 [Show]    │
│                     add stores 2 → 3 ; add hauled (Birch) 0 → 1                                                  │
```

[Show] opens the world event behind a firing (the row of `game.events`) and, when it has a tile, the place on
the map. Names are shown, never bare ids.

**Triggers** is the editor's tree, read-only, with for each trigger: on or off (a switch the admin can flip),
how often it has fired and when, [Fire…], and [Why not?].

**Variables & timers**:

```
│ Variable     Scope    Value                                           Changed                                    │
│ watches      story    6                                 [✎]           day 49.0 by log-watch-round                │
│ stores       story    5                                 [✎]           day 44.6 by martin                         │
│ hauled       player   Ash 3 · Birch 1 · (29 at 0)       [▾ per wolf]                                             │
│ gate-open    story    off                               [turn on]                                                │
│ Timer          Owner   Due                          Repeats   Lives                                              │
│ watch-round    story   day 56.0  (≈ 50 min)         every 7   campaign    [Fire now] [± time] [Cancel]           │
│ spring-council story   day 90.0  (≈ 13 h)           -         campaign    [Fire now] [± time] [Cancel]           │
│                                                                                              [+ Start a timer]   │
```

**Quests** is a grid, wolves down and objectives across, for each quest:

```
│ Provision the ferries (each wolf)            ask    bring 4 rope   (optional) spare wheel    ?hidden             │
│ Ash Greymantle        ● here   Northgate     ✔      3 / 4          -                         -        [▾]        │
│ Birch Oakenfall       ○ 2 d ago              ✔      ✔              ✔                         -   DONE [▾]        │
│ Elm Sandpaw           ○ 9 d ago              -      -              -                         -        [▾]        │
│ [▾] = Complete objective… · Set progress… · Reveal · Fail · Reset · Give the quest · Take it away                │
│ 9 of 31 done · needs 19 (60%) to move on                                                                         │
```

**Wolves & map**: the story's wolves on the world map (LIVE's map, `MapView`, with the positions LIVE already
gets from `dm.watch`), the story's regions drawn, the residents the story directs marked, and a list beside it:
who is here now, where, their party, what they last did in the story. [Add a wolf…] [Remove…] [Tell…].

**Orders** is the queue to the game, for this run:

```
│ #5514  deed.award → Ash Greymantle     REFUSED  "that wolf is not in the world just now"     6 min ago           │
│         [Retry] [Retry when Ash is next here] [Skip it and carry on] [Change target…]                            │
│ #5515  story.practice → Ash            waiting behind #5514                                                      │
│ #5513  story.tell → participants       applied  18:02:09                                                         │
```

**Audit** is the admin's commands on this run: who, when, what, the reason, before and after, the result.

### 5.4 The controls, and what each needs

All go through one new op, `command`, on `POST /api/dm/story-program`: `{op: "command", target, run, kind,
reason, ...}`. It writes a row to `dm.story_commands` and notifies the runner; the reply is the command's id, and
the console shows it as "sent", then "done" or "refused: why" when the runner has taken it (within a tick). A
command carries the visit it was made for and is refused if the story has moved on, unless the admin ticks
"even if it has moved on".

| Control | Command `kind` | What the runner does (`story_runner.py`) | New engine support |
|---|---|---|---|
| Hold (pause), Resume | `hold`, `resume {catchUp: true or false}` | New status `held`. A held run reads no events, starts no effects, fires no timers; effects already sent complete. Resume carries on from the cursor (the default: what happened meanwhile counts), or moves the cursor to now ("skip what happened while held"). | `tick` skips `held`. Distinct from `paused`, which keeps meaning "the game refused something". |
| Hold all | `hold` for every running run | One click, one reason, one audit row each (doc 34's "pause all"). | |
| Stop | `stop` (exists as `direct`'s `stop`) | As today, but the run is `stopping` until the game acknowledges `story.stop`. | Fixes known problem 10. |
| Send a signal | `signal {name, actor?}` | Dispatches a `story signal` fact (and, for a `dm signal` exit, that) as the next fact. No ticket for a signed-in DM. Tickets stay for the author bridge. | Triggers may listen to `admin signal`. |
| Fire a trigger now | `fire {trigger, actor?, check: true or false}` | Runs that trigger's actions as if its event had happened, for the chosen wolf, checking its conditions or not. Marked "by hand" in the history. | A direct entry into `fire` for one rule; twin and native. |
| Turn a trigger on or off | `enable`, `disable {trigger}` | Adds or removes it from `state.disabled`. | The set is honoured by both twins (3.5). |
| Go to a phase: advance, jump, go back | `enter {phase, effects: true or false}` | Calls `enter()` for that phase as a new visit, with or without its entry effects. The cause in the history is "martin: <reason>". | None: `enter` exists. |
| Set a variable | `set {var, scope, player?, value}` | Writes it; `variable changed` is dispatched so triggers can react. | |
| Timers | `timer {name, do: fire, cancel, shift, start, days?}` | Edits `state.timers`; "fire" dispatches its signal now. | |
| Quests | `objective {quest, objective, player, do: complete, progress, reveal, fail, reset}`, `quest {quest, player, do: give, take, complete, fail}` | Edits the ledger, sends the journal update, and delivers the quest's rewards when one completes. | With 3.6. On today's missions: complete and reset only. |
| Wolves | `participant {player, do: add, remove}` | Edits the audience and tells the game. | 3.6 (`story.cohort`). |
| Orders | `order {receipt, do: retry, skip, when-here, retarget}` | Retry: as `direct`'s `retry`, for any order, `expired` too. Skip: mark it skipped and carry on. | The pending-receipt keys in `state` become a small queue. |
| Tell the wolves | `tell {to: participants, a wolf, a party, a region, text}` | Sends `story.tell`. | The new order (3.5). |
| Start a scene | `scene {scene, player or world, cue}` | Sends `story.scene` for a scene embedded in the program. | None. |
| Spawn or move a resident, and any other world action | not a story command | The console opens LIVE's own action (`DungeonMaster.request`, `dungeon_master.py:465`) with the story's residents and regions offered, and notes it in this run's audit. | None. |

### 5.5 What would fire now: the dry run

A read, new op `explain`, answered by a new `story_runner.explain(conn, world, run, ...)` that copies the run's
state and runs the Python rules on the copy with a recorder; nothing is saved.

| Question | Request | Answer |
|---|---|---|
| What is this story waiting for? | `explain {run}` | Every trigger and exit that can fire in this phase: its event as a sentence, each condition with **true, false or unknown and the values behind it**, and, for wolf events, which wolves here now would satisfy it. The Overview's "Waiting for". |
| Why has this one not fired? | `explain {run, trigger}` | The last events of its kind and where each stopped: the filter that did not match, the condition that was false, "already fired for this wolf", "the wolf is not in the story", "turned off". |
| What if this happened? | `explain {run, fact: {...}}` | The triggers that would fire, the variables after, the effects that would be sent, the phase it would end in. Shown before every [Fire…] and every [Go to phase] as "this will do:". |
| What will happen next by itself? | part of `explain {run}` | Timers in order of due time, with the day and an estimate in real time from the world's speed. |

`satisfied` gains a recording form in the Python twin only: the console and rehearsal need the tree of results;
the live path does not.

### 5.6 Alerts: when a campaign stalls

Worked out in `status` from what is already kept, plus a heartbeat the runner writes each tick
(`dm.story_runner(world_id, beat_at, pid, version)`):

| Alert | Rule (first values, all adjustable per world) |
|---|---|
| The runner is not running | No heartbeat for 30 seconds. Shown on every DM tab, not only this one. |
| The game is not answering | An order `queued` for more than 60 seconds. (A scratch server never answers.) |
| Paused: the game refused an order | Status `paused`. Says which order, for whom, and offers the three ways on. |
| Falling behind | The cursor is more than 2,000 events or 60 seconds behind the log. |
| Quiet | Nothing fired and no quest moved for 24 real hours while at least one of its wolves was in the world. A program may set its own `quietAfterHours`, or 0 for "this one is slow on purpose". |
| Cannot move on | No trigger or exit of the phase can still fire: every one is turned off, used up, or waits for a wolf who has been gone 14 days. |
| A timer is overdue | Due more than 5 minutes ago and not fired. |
| The audience is nearly full | 90% of the cap. |

Where they show: a count on the Campaigns tab in the DM's tab bar; a line on LIVE; and, for "needs you" alerts
only, a message to Dungeon Masters in the game's Dev Console (doc 34, 1.1b), since that is where a DM who is
playing will see it.

### 5.7 Who may do what

Ranks are `viewer < dm < builder < owner < admin` (`RANK`, `dungeon_master.py:209`). In the user's own Studio a
`dm` may do everything below (the user is that world's owner).

**Decided (user, 2026-11-21)**, question 3: on a hosted world (a host's Editor, doc 78) **the world's owner may
always steer a running campaign; any other account may steer only when the owner has given it a new permission,
"may steer campaigns"**, granted and taken away the way "may act on players' wolves" is (doc 78, Q24). Without it a
hosted DM sees the console and steers nothing. **Every intervention is logged** (5.1, rule 6: `dm.audit` and the
campaign's own Audit and Timeline), whoever made it, the owner included.

| Action | Rank | Also needs |
|---|---|---|
| See the console (list, timeline, variables, quests, the map) | dm | As `status` today. A viewer sees none of it (**decided, mine**: it names wolves and where they are). |
| Hold, resume, stop; send a signal; tell the wolves | dm | Hosted: the owner, or "may steer campaigns". |
| Fire a trigger; turn one on or off; set a variable; timers; go to a phase; quests; retry or skip an order; start an embedded scene | dm | Hosted: the owner, or "may steer campaigns". And the effects this releases were approved with the program, under the checks approval already makes: `faction.relation` needs builder or owner (`dungeon_master.py:2737`), and `character.injury` and `story.area.damage` need the owner or "may act on wolves" (`studio.py:1354`, `1822`). If the admin now at the console could not have approved the program, a control that would release such an effect is refused for them. |
| Add or remove a wolf | dm | Hosted: the owner, or "may steer campaigns". |
| Move a run onto a newly approved version (Phase 5b) | dm | Hosted: the owner, or "may steer campaigns"; and whoever approves the new version needs what any approval needs. |
| "Do in the world": any LIVE action | whatever LIVE asks for that action | `character.kill`, `injury`, `resurrect`, `move` need the owner or "may act on wolves"; cheats as now (`studio.py:1811-1850`). |
| Hold all | owner on a hosted world; dm in the user's own | |
| Change the alert rules; grant or take away "may steer campaigns" | owner | |

Underneath all of it the game's own guard stays (`dmActRefused`, `Core/RatwGameTickets.cpp:867`): on a community
world with cheats off, a reward to the host's own wolf is refused whatever the console allowed.

### 5.8 Safety rules

1. **Say why.** Every command needs a reason of at least a few words. It is shown in the timeline.
2. **Show first.** [Fire…], [Go to phase…], quest and order controls show `explain`'s "this will do" and wait
   for a second click. Stop, go back to an earlier phase, skip an order and remove a wolf also ask for the
   story's title to be typed.
3. **Once.** A command has an id; sending it twice does it once. A command made for visit 3 is refused in
   visit 4 unless forced.
4. **Bounded.** A value is checked against the variable's type; a phase must exist; a trigger must be in the
   program. No command can add a trigger, change a condition or edit the program: a different program is a new
   approval, which a running campaign can then be moved onto (Phase 5b; **decided (user, 2026-11-21)**,
   question 5).
5. **No silent rewards.** Completing an objective by hand gives the quest's rewards exactly as play would, and
   the timeline says an admin did it. There is a box "without rewards".
6. **Held is safe.** Holding never loses events; they wait in the log. The console says how many are waiting.
7. **Not a rollback.** Going back re-opens a phase. Deeds given, items handed over and relations changed stay.
8. **Two admins.** The inbox is ordered; the second command sees the first one's result. The console shows who
   else has this campaign open.
9. **The live world's players are not test subjects.** "What if" never touches the world; a new story is
   rehearsed, then tried on a scratch copy, before it is published.

### 5.9 Tables and routes the console adds

```
dm.story_commands(id bigserial, run, visit, kind, payload jsonb, reason, requested_by, requested_at,
                  status ('queued','done','refused'), result, done_at)          -- the inbox, and the audit's detail
dm.story_runner(world_id primary key, beat_at, pid, version, behind bigint)     -- the heartbeat
dm.story_releases.status gains 'held' and 'stopping'
```

| Op (`POST /api/dm/story-program`) | Kind | Runner function |
|---|---|---|
| `status` (grown) | read | `status()`: the list gains visit, day entered, wolves here now, quests done, last firing, waiting-on, alerts, how far behind; one run gains its `program`, names for every id, and `since` cursors so a poll returns only what is new |
| `timeline` | read | new `timeline(conn, world, run, before, filters)`: the merged list of 5.3 |
| `explain` | read | new `explain()` |
| `command` | write | new `command()` inserts; new `take_commands()` at the top of `tick_run()` |
| `retry`, `stop`, `signal-ticket`, `voice-ticket`, `signal`, `voice` | write | kept for the author bridge and old clients; the console uses `command` |

---

## 6. Phases

One agent per phase, in this order, on a branch each, reusing one build; targeted tests in each phase; the full
suite, an end-to-end run and one review-and-fix only at the end of a slice. Hours are agent hours and rough.
**The user asked for every phase (2026-11-21)**, with Phase 5b added (question 5).

### Slice A: the Campaigns console (the user's main ask)

**Phase 1: See it. About 10 hours. The smallest useful first slice.** No engine change, no migration, no new
order to the game.
- `Editor/src/dm/CampaignsTab.tsx` and `Editor/src/dm/campaigns/` (`CampaignList`, `CampaignView`, `Timeline`,
  `StatePanel`, `MissionGrid`, `WolvesMap`, `Orders`), a `campaigns` entry in `TABS` (`DmApp.tsx:31`), and the
  Releases panel removed from `StoryProgramPanel.tsx`.
- `story_runner.status` grown as in 5.9 and a `timeline` op, both reads over the four existing tables.
  Names from `dm.story_residents` and `game.characters`. "How far behind" from the cursor and the newest event.
- The wolves on LIVE's map, from the frame LIVE already reads.
- Alerts that need nothing new: paused, the game not answering, falling behind, quiet.
- Self-refresh every 2 seconds.
- The existing controls moved here, and the Retry button shown for `expired` too.
- Tests: a `tools/test_story_console.py` for the grown `status` and `timeline` on a fixture run; `src/lib` tests for merging and
  for "waiting on"; the render smoke for the tab with a running and a paused run.
- Done when: an admin can leave the tab open and answer "is it alive, where is it, who is in it, what happened
  last, what is it waiting for" without pressing anything.

**Phase 2: Steer it. About 16 hours.**
- Migration `0071`: `dm.story_commands`, `dm.story_runner`, statuses `held` and `stopping`.
- `story_runner.command`, `take_commands`, the heartbeat; the commands of 5.4 that today's engine can carry:
  hold, resume, hold all, stop (waiting for the receipt), signal, fire, enable and disable, enter, set (counters),
  timers, missions (complete, reset), orders (retry, skip), scene.
- `state.disabled` in `story_triggers.py` and `RatwStoryTriggers.cpp`, with the parity test.
- One new order in the game, `story.tell` (text to the story's wolves, a wolf, or a region), and the console's
  "Tell the wolves".
- Audit rows; the Audit tab; reasons and confirmations (5.8).
- Tests: one per command on a fixture run, including "made for an earlier visit", twice-sent, and a command
  arriving with events waiting; a game test for `story.tell`.
- **Who may steer (decided, user, 2026-11-21, question 3):** the `command` op checks, on a hosted world, the owner
  or the new account flag "may steer campaigns" (a column beside "may act on players' wolves" in the same migration
  `0071`, set from My world's account list by the owner), and writes every command to `dm.audit` as well as
  `dm.story_commands`, the owner's too.
- **Shaped by Phase 5b:** `dm.story_commands` takes a `migrate` kind later, so its `payload` carries what a command
  was made against (the run's `digest` as well as its `visit`), and the trigger history and the audit name the
  version a run was on. A command made against an older version is refused like one made for an earlier visit.

**Phase 3: Explain it. About 10 hours.**
- `story_runner.explain` and the recording `satisfied`; Overview's "Waiting for", [Why not?], "this will do".
- `story_program.problems` (every problem, with paths) and the warnings from the world; the Problems list in
  today's editor; the references of known problem 4 checked.
- The remaining alerts (cannot move on, overdue timer, runner not running) and their places outside the tab.
- Slice end: the full suite; end to end on a test world: publish a bundled example, drive it with events, hold,
  set, fire, jump, stop from the console; one review-and-fix.

### Slice B: the editor and the language

**Phase 4: The catalogue and the workbench. About 18 hours.**
- `Data/Storylines/trigger-catalogue.json` for everything that exists (160 events with groups and what their
  fields hold, 7 conditions, 7 actions, 25 effects), and the validator's tables read from it.
- The workbench of section 4 on today's vocabulary: tree, folders, notes, sentences, pickers, search,
  duplicate, copy and paste, undo, drag, Problems, exits shown as triggers, "New small story" from a library
  entry. Op `refs`.
- Migration `0072`: `dm.story_drafts`; the draft ops. The two old tabs removed.
- Tests: the render smoke over all 8 examples and the 100 library drafts; a round trip (open, save, compare) for
  each, so the workbench never changes a program it did not edit.

**Phase 5: The trigger language, version 2. About 16 hours.**
- Declared variables, types, lists; `compare`, `switch`, `in list`, `phase is`, `timer`, `fired`, `chance`,
  `days since`; `if`, `wait`, `each`, `random`, `run`, `enable`, `disable`, `stop`, `note`, `finish`; the state
  actions. Format version 2, version 1 read as before.
- Both twins and the parity test; rehearsal support (pinned rolls, waits, expected variables).
- The Variables panel, nested actions in the workbench, and variables with their types in the console.
- Slice end: full suite; the 8 examples rewritten where version 2 makes them shorter, with their rehearsals
  unchanged in outcome; one review-and-fix (moved to the end of Phase 5b, which closes the slice).

**Phase 5b: Move a running campaign onto a new version. About 10 hours.** Added by the user, 2026-11-21
(question 5: yes). A ninety-day campaign that needs a fix on day 40 keeps its wolves, phase, variables and quests.
- Approve "as a new version of run X" from Story Creator: the ordinary rehearsal and approval, plus
  `story_program.migration(old, new, state)`, which says, for the live state, what is **kept** (the current phase
  and every phase by id; variables by id and type; timers whose signal still has a listener; missions and quests by
  id whose objectives are unchanged; the `fired` sets by trigger id; the audience), what is **reset** (a mission or
  quest whose objectives changed: its progress), and what is **dropped** (variables, timers and triggers the new
  version no longer has). The current phase must exist in the new version, or the admin names the phase to land in.
  A version 1 run moves onto a version 2 program (Phase 5) the same way.
- The move is a command, `migrate {release}`, in the Phase 2 inbox, taken between two world events; nothing is
  delivered by the move itself (a phase's entry effects are not run again). Migration `0073`
  (Phases 6 and 8 then take `0074` and `0075`): `dm.story_versions(run, digest, program, approved_by, moved_at,
  moved_by, report jsonb)`, one row per version a run has been on; the release row keeps the current one.
- The console: "Move onto a new version…" shows the migration report (kept, reset, dropped) and waits for a second
  click and a reason; the timeline and the Audit say "moved to version b7e1 by martin"; the header names the
  version.
- Who may: as any steering control (5.7). Rewards and world changes already delivered stay (5.8, "not a rollback").
- Tests: kept, reset and dropped on fixture runs of the 8 examples edited three ways; a phase that no longer
  exists; a timer whose listener went; moving twice; a command made against the old version refused after the move.
- Slice end (B): full suite; one review-and-fix.

### Slice C: quests, the world, and speed

**Phase 6: Quests and a living audience. About 18 hours.**
- Quests (3.6) in the rules and the journal payloads; audiences `open` and `everyone`; `participant.add` and
  `remove`; the game's `story.cohort` order; `npc.offer` and `dialogue choice` in doc 83's window. Small pick-up
  quests are the Story Maker's (**decided, user, 2026-11-21**, question 2), so the open audience and "New small
  quest" are in this phase in full; doc 58's templates stay and are offered as starting points.
- Coin rewards name a purse (**decided, user, 2026-11-21**, question 4): a quest's `character.grant` reward says
  which town, House or faction pays, and is refused (and, with `ifRefused`, held or skipped) when the purse is short.
- Migration `0074`: `dm.story_player_state`; the cap raised.
- The client's journal: optional, hidden, failed, trackers, abandon.
- The Quest editor, "New small quest", and the console's Quests grid with every control.
- Tests: rehearsals for each scope, any-order, fail, repeat; a game test for the offer in the window.

**Phase 7: Batched effects and a faster runner. About 12 hours.**
- `story.effects` (a list in one order, a result for each) and `ifRefused` (pause, skip, when here).
- The runner woken by a notification instead of sleeping 2 seconds, and reading until it has caught up.
- Scene outcomes no longer save on the game thread: they ask for an early save (`saveSoon`) and wake the
  runner; measure the delay a player sees between a vote and the next beat, bar 5 seconds.
- Old trigger history pruned by a rule (keep 90 days or the last 10,000 per run).
- Measured with a load of events on a test world (at most 250 players, by the standing rule): events a second a
  run can read, and seconds from an event to its effect. Bars: 1,000 a second; 5 seconds.

**Phase 8: Facts and new events from the game. About 14 hours.**
- Migration `0075`: `game.story_facts`, written with each save for live runs from their watch lists.
- The conditions over facts (3.4); rehearsal cases that state facts.
- The new events of 3.3, the watched ones through the watch list; signing in inside a region counts.
- Tests: one game test per new event; a `world_check` run to show the cost with no story live is nil and with
  ten live is within the tick budget.

**Phase 9: New world effects. About 14 hours.**
- Stories allowed to send the orders that exist: `visitor.add`, `visitor.leave`, `mantle.assign`,
  `mantle.release`, `event.add`, `event.cancel`, `prices.reset`, `project.complete`, `project.cancel`,
  `deed.revoke`, `storyline.tick`, `character.grant` (from a named purse: the order gains `purse`, and the game
  takes the coins from it or refuses), `npc.kill`.
- `weather.set` is in (**decided, user, 2026-11-21**, question 6); nothing touches the calendar.
- New orders: `item.give`, `item.take`, `item.place`, `story.ping`, `story.tracker`, `door.lock`,
  `door.unlock`, `weather.set`, `standing.adjust`, `npc.hold`, `npc.release`, `npc.follow`, `board.post`,
  `warrant.issue`, `warrant.clear`, `scene.stop`. Each undone when the run ends where the table in 3.5 says so.
- Catalogue entries, sentences and pickers for each; hosted checks for the ones that act on wolves.

**Phase 10: Chapters, world variables, and the guide. About 9 hours.**
- `story.start`, `story started`, `story ended`; several programs approved together as a campaign;
  `dm.story_world_vars`.
- The 100 library entries offered as small quests.
- `Docs/story-programs.md` rewritten for the workbench and the console.
- Slice end: full suite; end to end: a three-chapter campaign with a quest giver, a scene, a held phase and an
  admin's intervention; one review-and-fix.

Total: about 147 hours (the ten phases' 137, and Phase 5b's 10). Later, not planned here: `belief.plant` and
`npc.brief`, camera moves and fades in scenes, a cast chosen by a query (doc 34, 5.1), priorities between stories,
the rules inside the game server (question 1), and the editor and console for player storytellers (question 7).

---

## 7. Risks

1. **A console that makes it easy to break a story.** Jumping phases and setting variables can put a run into a
   state no rehearsal covered. Mitigations: "this will do" before every command, the command recorded in the
   timeline, and commands limited to what the program contains. An admin can still strand a story; they can also
   un-strand it, which today they cannot.
2. **Two engines to keep equal.** Every flow action is written twice, in Python and in C++. The parity test is
   the guard. If version 2 makes this heavy, the fallback is to make the native library the only evaluator and
   have Studio call it too (it is already built with the server).
3. **Facts are as old as the last save.** A condition over the world's facts sees the world as of the event it
   is handling, up to 30 seconds old before Phase 7 and a few seconds after. Good for "does the wolf carry
   rope"; wrong for "is the wolf still standing on this tile this instant". Tight things belong in the game
   (scenes, region watching), and the catalogue says which is which.
4. **Noise from new events.** Item and movement events could flood the log. Hence the watch list: the game writes
   them only for what a live story named.
5. **Quests anyone can take are an economy.** A repeatable quest that pays coins is a faucet. Rewards go through
   `character.grant`'s cap and the economy's journal, and the coins come from a named purse, never from nothing
   (question 4, decided 2026-11-21).
6. **The workbench must not eat programs.** A draft the forms cannot show must still open, in Source. The round
   trip test of Phase 4 is the guard.
7. **Per-wolf state at scale.** Moving state out of one JSON is a migration of live runs. Phase 6 reads the old
   shape and writes the new one once; a run started before it finishes on the old shape.
8. **Stories fighting over one resident.** Until `npc.hold` (Phase 9), two campaigns can direct the same
   resident; today the game refuses the second (`npc.walk` is refused while another story directs them) and that
   pauses a run. The console at least shows it.

---

## 8. Questions for the user: answered 2026-11-21

Only the ones that change the build. **All answered.** The user's words, 2026-11-21: build everything (all ten
phases) and say yes to question 5; small quests are the Story Maker's (2); steering on a hosted world is the owner's
always, and a hosted DM's only with a new permission the owner grants, every intervention logged (3). The questions
not put to the user (1, 4, 6, 7) take this doc's recommendation. Each answer is in bold after its question; the
phases of section 6 carry them.

1. **Where the rules run.** Today they run in a process beside the game, a few seconds behind it (up to 35
   seconds before Phase 7, about 3 to 5 after). The alternative is doc 34's: the engine inside the game server,
   reacting in the same tick. That is what a StarCraft map does and it would make "the gate guard speaks as you
   step through" instant, but it is a larger change, it puts story work on the game thread, and it ends
   rehearsing offline in Python. *My recommendation: keep the runner, do Phase 7, and look again only if 3 to 5
   seconds feels wrong in play.* It changes Phases 7 and 8.
   **Answer: the recommendation stands.** The rules stay in the outside runner; Phase 7 makes it fast.
2. **Small quests: here, or doc 58's storylines?** Phase 6 lets the Story Maker make a quest any wolf can pick up
   from a resident, repeatable, with rewards. The game already has personal storylines from templates
   (`Data/Storylines/templates.json`, 24 of them). *My recommendation: one tool, the Story Maker, with the
   templates kept as they are and offered as starting points.* If the answer is "leave personal quests to doc
   58", Phase 6 shrinks to quests inside campaigns and loses the open audience.
   **Answer (the user): here.** Small pick-up quests are made in the Story Maker; Phase 6 is built whole.
3. **Who may steer on a hosted world.** 5.7 lets any hosted DM hold, set, fire and jump, within what was
   approved. *Is that right, or should going to another phase and completing objectives for a wolf be the
   owner's only?* It changes Phase 2's checks.
   **Answer (the user): the world's owner always; other hosted DMs only with a new permission, "may steer
   campaigns", that the owner grants; every intervention logged.** Seeing the console stays any DM's (5.7). Phase 2
   adds the flag and the checks.
4. **Where a quest's coins come from.** `character.grant` exists and is capped, but it makes coins. The economy's
   rule has been "through channels, never handouts". *My recommendation: a story's coin reward names a purse (a
   town, a House, a faction) and is refused when the purse is short; item rewards as today.* It changes Phases 6
   and 9.
   **Answer: the recommendation stands.** A coin reward names a purse and is refused when the purse is short.
5. **Changing a campaign that is running.** Today a changed program is a new approval and a new run. A ninety-day
   campaign will need a fix on day 40. *Should an admin be able to approve a new version and move the live run
   onto it, keeping its phase, variables and quests when they still fit?* It is not in the ten phases; saying yes
   adds a phase after 5 (about 10 hours) and shapes Phase 2's tables.
   **Answer (the user): yes.** Phase 5b (section 6), about 10 hours; Phase 2's commands carry the version they
   were made against.
6. **A story changing the weather.** `weather.set` for a place for some hours is proposed; changing the calendar
   is ruled out. *Is weather acceptable, given that harvests and the cold follow it?* It decides one order in
   Phase 9.
   **Answer: the recommendation stands.** A story may set the weather for a place for a while (Phase 9), and
   never the calendar.
7. **Player storytellers.** Doc 58 lets trusted players run tales. *Should they ever get this editor and a
   console for their own stories, or is it for Dungeon Masters only?* *My recommendation: Dungeon Masters only
   for now.* Saying yes changes the permission model throughout.
   **Answer: the recommendation stands.** The editor and the console are for Dungeon Masters only, for now.
