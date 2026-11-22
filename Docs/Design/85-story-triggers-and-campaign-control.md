# 85. Story triggers and campaign control: a trigger editor like StarCraft's and Warcraft III's, and a console to run a campaign live

Draft, 2026-11-21. A survey and a design; Phases 1 to 5, 5b, 6, 7 and 8 are built (section 6, "Phase 1 as built" to "Phase 8 as built"), the rest is not. Surveyed at `9902c12b` on `ridgemere2` (the
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

**Built so far:** Phase 1 ("See it"), on branch `r3-camp-p1`, Phase 2 ("Steer it"), on branch `r3-camp-p2` (from `r3-camp-p1`), and
Phase 3 ("Explain it"), on branch `r3-camp-p3` (from `r3-camp-p2`), which closes Slice A, and Phase 4 (the catalogue and the
workbench), on branch `r3-camp-p4` (from `r3-camp-p3`), Phase 5 (the trigger language, version 2), on branch `r3-camp-p5`
(from `r3-camp-p4`), and Phase 5b (moving a running campaign onto a new version), on branch `r3-camp-p5b` (from `r3-camp-p5`),
which closes Slice B, Phase 6 (quests and a living audience), on branch `r3-camp-p6` (from `r3-camp-p5b`), and Phase 7 (batched
effects and a faster runner), on branch `r3-camp-p7` (from `r3-camp-p6`), and Phase 8 (world facts and new events), on branch `r3-camp-p8`
(from `r3-camp-p7`): see "Phase 1 as built" to "Phase 8 as built" in section 6.

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

Known problems. The first five are the ones left open by our review. (Phase 7 fixed 1, 2, 3 and 11, gave 6 to the author
(`ifRefused`), and brought 7 to about two seconds while a story is live: "Phase 7 as built".)

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

**Phase 1 as built** (2026-11-21, branch `r3-camp-p1`). No engine change, no migration, no new order.
- **The runner's reads** (`tools/story_runner.py`, the section "the Campaigns console"). `status()` with no run is
  now `overview()`: the 100 newest runs, and for each live one (`approved`, `running`, `paused`) its visit, day
  entered, audience, mission summary, `last` (the newest firing or visit, with its real time), `waiting`, `behind`
  and `alerts`; an ended run gives only its counts (its state is not read). `status(run, ..., since, have)` keeps
  every key it had and adds the program (left out when `have` is its digest), `names`, `orders` (the 60 newest of
  the run, each with its visit), `missionsNow`, `directed` (residents its effects, guest voices and scenes name),
  `regions`, `phases` (each with where it can go), `listening`, `waiting` and `alerts`; `since: {visit, trigger}`
  returns only newer history and firings. New `timeline(run, before, since, kinds, wolf, trigger, limit)` merges
  phase visits, firings and orders, newest first, with the world events behind the firings and names; `before`
  pages back per source, `since` returns what is newer **and the orders answered since the last poll** (from
  `dm.actions.done_at`, with five seconds' overlap: the console merges by key). One tick's work shares a moment, so
  the order is by time, then visit, then "the visit opened, its firings, its orders". `names()` reads residents,
  wolves, places and factions.
- **Which orders are a run's:** the union of every visit's `receipts`, the release's `receipts` and a pending
  `missionReceipt`/`triggerReceipt`; `dm.actions` has no run column and needs none.
- **What it waits for** is raw facts from the runner (`publish`, `admin` with the stuck order, `game` with the order
  and its age, `send` with the orders still to send, `timer`, `clock`, `direction`, `wolves` with what the phase
  listens for, `end`); `Editor/src/lib/campaigns.ts` words them and picks the headline (the admin, then the game,
  then the soonest timer or delay, then a direction, then wolves).
- **Alerts as built:** `paused` (needs you); `game` (an order the run waits on, queued over 60 s); `behind` (over
  2,000 events or the oldest unread over 60 s, and not while it waits on the game, since the cursor rightly stands
  still then; it says "is the story runner running?" until Phase 2's heartbeat); `quiet` (no firing, visit or order
  for 24 real hours **and** a wolf of the audience did something in the world since, read per wolf from
  `events_actor`). Thresholds are `story_runner.ALERTS`; per world and per program come with Phase 3.
- **Who is here now** comes from the LIVE frame (`dmApi.live`), which the console polls as LIVE does (so the game
  keeps writing frames while it is open); the runner knows nothing of who is signed in.
- **The route:** `op: 'timeline'` on `/api/dm/story-program`, and `since`/`have` on `status`; both need a DM
  (`viewer` gets 403), as `status` did.
- **The Editor:** `Editor/src/dm/CampaignsTab.tsx` (polls every 2 s while visible: the list, the frame, and for the
  open run `status` with `have` and `timeline` with `since`) and `Editor/src/dm/campaigns/` (`CampaignList`,
  `CampaignView` with Overview, Timeline, Variables & timers, Missions, Wolves & map and Orders, `Timeline`,
  `StatePanel`, `MissionGrid`, `WolvesMap`, `Orders`); a `campaigns` tab (⚑) after Story Creator. The Releases panel
  is gone from `StoryProgramPanel.tsx` (its "World references for an author" stays there); approving shows "Open in
  Campaigns" beside the publication ticket and after publishing. Stop, Retry (now for `expired` too, and named for
  what it retries), the direction and guest-line tickets moved to the campaign's header and Overview. The tab is
  called **Missions**, not Quests, until Phase 6.
- **Not in Phase 1, on purpose:** values before and after in the timeline ("watches 5 → 6": the trigger history
  keeps the action kinds only), parties (nothing tells the runner), the count on the DM tab bar and the line on
  LIVE (Phase 3), the runner's heartbeat (Phase 2).
- **Tests:** `tools/test_story_console.py` (a throwaway migrated database: the list, one run, the timeline's merge,
  `since` and paging, the four alerts, an approved and a stopped run, and the route for a DM and a viewer);
  `Editor/src/lib/campaigns.test.ts` (merging polls and pages, waiting-on, alerts, wolves on the frame, the mission
  grid, the list's counts); the render smoke renders the tab with a list, a running run, a paused run's orders and a
  viewer.
- **Seen:** on a throwaway Greyfen world with a real game server, three signed-in wolves (one signed out), a
  three-phase sample campaign driven by moving the wolves through two regions (a mission each, a group mission,
  timers, deeds) and a second one paused by a refused deed; screenshots in
  `/home/martinb/.claude/jobs/ee03c49e/tmp/shots-camp-p1/`.
- **For Phase 2:** add `held` and `stopping` to `LIVE_STATUSES`, `_waiting`, `_alerts` and `listCounts`; admin
  commands become a fourth timeline source (`dm.story_commands`, key `c:<id>`; give them a rank after orders at one
  moment); `waiting` gains `{on: 'admin'}` for a held run; the header's `camp-controls` row is where the new
  controls go (`atLeast(me, 'dm')` there becomes "owner, or may steer campaigns" on a hosted world); write the
  values before and after into each firing's history entry so the timeline can show them.

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

**Phase 2 as built** (2026-11-21, branch `r3-camp-p2`, on `r3-camp-p1`). Migration `0071`, one new game order, no change to
the program format.
- **Migration `0071_story_commands.sql`** (additive): `dm.story_commands` (the inbox: `run`, the console's own `key`, unique
  per run, so a command sent twice is done once; `visit` and `digest` it was made against; `kind`, `payload`, `reason`,
  `requested_by`, `status` `queued`/`done`/`refused`, `result` jsonb with the values before and after, `done_at`);
  `dm.story_runner` (the heartbeat: `beat_at`, `started_at`, `pid`, `version`, `behind`); the statuses `held` and
  `stopping`; `dm.admins.may_steer_campaigns`. All closed to the game; `ratw_dm` reads and writes them. Not applied to
  `ratw_ridgemere3` (it goes there at go-live). Reads of the new column go through `to_jsonb(a)->>…`, so an unmigrated
  world still signs in.
- **The runner** (`tools/story_runner.py`, "steering: the command inbox"). `command()` checks and queues (op `command`);
  `hold_all()` queues one hold per running run (op `hold-all`); `take_commands()` runs at the top of each run's tick, in
  order, each command in its own savepoint, against the run as it then stands (`Steer`: one method per kind), and writes
  the result and a `dm.audit` row (`story.<kind>`, the requester as `who`, the reason and the before and after in the
  detail) for done and refused alike. `tick()` now also takes paused and held runs that have queued commands, settles
  `stopping` runs, and writes the heartbeat first. The kinds, as built:
  `hold`, `resume {catchUp}` (false moves the cursor to now and says how many events were skipped; timers still fire),
  `stop` (sends `story.stop`, status `stopping`; `stopped` when the game applies it, or refuses it as unknown; sent again
  if it expires), `signal {name, actor?}` (a `story signal` fact, then the `dm signal` exit of that name; only names the
  phase listens for), `fire {trigger, actor?, check}` (the trigger's actions run by `story_triggers.perform`, now shared
  with the Python twin; checked: its once-rule, its conditions on a stand-in fact of its own filters, and "turned off";
  unchecked: none of them; marked `byHand` in the history), `enable`/`disable {trigger, phase?}` (`state.disabled`: a
  campaign trigger by its id, a phase's as `phase/id`, which stays off on later visits of that phase), `enter {phase,
  effects}` (a new visit as a transition makes one; "without its entry effects" is `state.quietEntry`, which the delivery
  and the journal's `end` honour), `set {key, scope, player?, value}` (only a counter the program reads or writes; a whole
  number), `timer {do: fire|cancel|shift|start, name|signal, days?}` (start only a signal the program schedules, with that
  action's clock, owner, repeat and lifetime), `mission {mission, player?, do: complete|reset}` (sends the journal's
  `story.missions`; a completed gate counts at the next thing a wolf of the story does, as play does: today's missions
  have no rewards of their own, so "without rewards" has nothing to hold back yet), `order {receipt, do: retry|skip}` (only
  the order a paused run is stuck on; skip is `state.skipped`, which the delivery treats as applied), `tell {to:
  participants|wolf|region, player?, region?, text}` and `scene {scene, player, cue?}` (orders beside the story's own, kept
  in the visit's receipts so the Orders tab and the timeline show them, never holding the story up).
- **The rules every command keeps** (5.8): a reason of two words or more; made for another visit is refused unless
  `force` (hold, resume and stop are about the run, not a visit); made against another digest is refused at once (Phase
  5b's `migrate` will rely on it); bounded by the program (`_check_payload`, at the request and again when taken). **Flow
  commands** (`signal`, `fire`, `enter`, `mission`, and `timer` fire) need the run to have sent and had answered every order
  of its visit: on a running run they wait (and the commands behind them) up to 60 s, then are refused; hold, resume, stop
  and order commands go past a waiting one; on a held or paused run part-way through sending they are refused ("resume it
  first", "retry or skip the stuck order first"). A command lands at the run's cursor, between two world events, whatever
  is waiting in the log (known problem 2 is gone for the console; `direct()` keeps its old rule for the author bridge).
- **Values before and after:** each firing's history entry now carries `changes` (counters and timers its own actions
  touched, before and after the fact it answered), for world events and commands alike; commands' rows carry `command` and
  `by`. A held run's `waiting` starts with `{on: 'admin', held: {by, reason, cursor, at}}`; a stopping run's is the stop
  order. New alerts: `held` (with how many events wait) and `runner` (needs you: no heartbeat for 30 s, or never on this
  world); `behind` no longer asks "is the runner running?" when the heartbeat answers that.
- **`state.disabled` in both engines:** `story_triggers.fire` skips it, `story_native.fire` passes it, and
  `RatwStoryTriggers.cpp` skips a rule whose key is in it; the parity test runs every combination on winter-ferries.
- **The game:** one new order, `story.tell` (`Game::applyStoryTell`, `Core/RatwStoryArea.cpp`): one line of up to 300
  characters, for the current visit only; to the story's wolves, one of them, or those standing in one of its regions (the
  bounds must match the published ones). A wolf in the world sees it as the storyline banner and a line in their log; one
  away is told when back (`tellActedOn`: kept with the wolf on a community world). Nobody away refuses it.
- **Who may steer** (5.7, question 3): the `command` op, on a hosted world, needs the owner or `maySteerCampaigns` on the
  session (`world_admin.session_now` reads it, `studio.HostedStudio.who` passes it); `hold-all` is the owner's. In the
  user's own studio a dm steers. A flow control on a program with `faction.relation` needs a builder or the owner, and one
  with `character.injury` or `story.area.damage` needs "may act on players' wolves" (the program-wide test: "could they have
  approved it"). **The grant:** `world_admin.set_steer` and the accounts verb `steer` (audited `account.steer`, a banner),
  `maySteerCampaigns` on `add` and in the list; the desktop shell passes the verb and the field
  (`Desktop/src-tauri/src/editor.rs`); My world's account list has a "MAY STEER CAMPAIGNS" box beside "may act on players'
  wolves" (`Client/src/ui/editorDoor.ts`).
- **The Editor:** the header's `camp-controls` row became `Steer.tsx`'s `SteerBar` (Hold or Resume / Resume from now,
  Stop…, Send signal, Go to phase… with or without its entry effects, Tell the wolves…), shown only to who may steer (a
  hosted DM without the grant is told why); every control opens `AskDialog`: what it will do, the reason, "even if it has
  moved on", and the story's title typed for stop, going back and skipping an order. The reply's command shows "sent",
  then "done" or "refused: why". New tabs **Triggers** (each trigger on or off as a switch, how often it fired in the
  timeline held, Fire… with a wolf and "check") and **Audit** (who, when, what, why, before → after, result). Variables &
  timers gained ✎ (set), Fire now, ± time, Cancel and + Start; Missions gained Complete… and Reset… per wolf or shared row;
  Orders gained Retry and Skip it and carry on… on the stuck order (the old `retry`/`stop` ops are no longer used by the
  console). The timeline has its fourth source (`c:<id>`, ranked after orders at one moment; filter "Admin") and shows each
  firing's `changes` ("stores 1 → 3"). The list has a Held filter, "N commands sent", Hold all… (the owner's on a hosted
  world), and the bar shows the heartbeat ("runner ● alive 1 s ago · 0 events behind"). The direction and guest-line
  tickets stay, folded, for the author bridge.
- **Tests:** `tools/test_story_steering.py` (a throwaway migrated database: every kind, hold all, stop waiting for the
  game, the disabled parity of both engines, made for an earlier visit, another version, sent twice, a command with world
  events waiting, a flow command waiting behind the game and hold going past it, the audit rows, the timeline's fourth
  source, the heartbeat and the runner alert; and through the route: a DM and a viewer in the user's own studio, a hosted DM
  without and with the grant, the owner, hold all, "may act on wolves"); `tools/test_world_admin.py` (the grant on the
  session, the list and the audit); `Tests/storyline_tests.cpp` (`story.tell` to all, one, a region, an altered region,
  another visit, two lines); `Editor/src/lib/campaigns.test.ts` (commands in the timeline, words, typed titles, held and
  runner alerts); the render smoke (the steering row, Triggers, Audit, a held run, a hosted DM without the grant); the
  shell's account test. `test_story_console.py` and `test_story_program.py`'s runtime tests pass unchanged but for one tick
  added for the heartbeat.
- **Seen:** on a throwaway Greyfen world with a real game server and three signed-in wolves (as Phase 1), the sample
  campaign held, resumed, a variable set, a trigger turned off and fired by hand, the wolves told, a phase jumped to, and
  the second story's refused order skipped; screenshots in `/home/martinb/.claude/jobs/ee03c49e/tmp/shots-camp-p2/`.
- **Not in Phase 2, on purpose:** `explain` and "this will do" from a dry run (Phase 3: the dialog lists what the program
  says the control does, not a simulation); `admin signal` and `variable changed` as events (Phase 5's language; a signal
  from the console is a `story signal`); quests' give, take, reveal and fail, and adding or removing a wolf (Phase 6: today's
  missions only complete and reset); "retry when the wolf is next here" and "change target" (3.5's `ifRefused`);
  `story.tell` to a party (nothing tells the runner about parties) or as a letter; "who else has this campaign open";
  per-world alert thresholds.
- **For Phase 3:** `Steer` is the place for `explain` to plug in (each `do_*` already refuses with the reason a dry run
  would give; run them on a copy of the state with a recorder for "this will do"); the `runner` alert exists in the tab
  only, and still needs its places outside it (the DM tab bar, LIVE, the Dev Console); `controls()` in `status` lists what
  the controls may name, and is where "cannot move on" can be worked out (every trigger off, used up or waiting on a wolf
  long gone); `state.disabled` is in both engines already for Phase 5's `enable`/`disable` actions.

**Phase 3: Explain it. About 10 hours.**
- `story_runner.explain` and the recording `satisfied`; Overview's "Waiting for", [Why not?], "this will do".
- `story_program.problems` (every problem, with paths) and the warnings from the world; the Problems list in
  today's editor; the references of known problem 4 checked.
- The remaining alerts (cannot move on, overdue timer, runner not running) and their places outside the tab.
- Slice end: the full suite; end to end on a test world: publish a bundled example, drive it with events, hold,
  set, fire, jump, stop from the console; one review-and-fix.

**Phase 3 as built** (2026-11-22, branch `r3-camp-p3`, on `r3-camp-p2`). Migration `0072`, no new game order, one program field.
- **The dry run** (`tools/story_runner.py`, "explaining: Explain it"; op `explain` on `/api/dm/story-program`, a read for a DM).
  `explain {run}` judges every trigger and exit of the run's phase (`listeners()`): on or off, used up and for which wolves, its
  conditions as a tree of true, false and unknown with the values behind each test (`story_triggers.judge`, the recording form of
  `satisfied`, Python only; the parity test checks it gives `satisfied`'s verdict), judged on a stand-in event of the trigger's own
  filters so a test of what the event brings is "unknown (the event brings it)"; its due day (a delay), its senders and timers (a
  signal), its gate (votes, prerequisites, missions ready and missing); `forWolves`, the wolves of the story it could fire for now
  (per wolf for participant and per-wolf triggers and conditions on player counters); and `can`, whether it can still fire by
  itself. With it: the run's timers soonest first, each with an estimate in real seconds from the world's speed (`world_speed`:
  game days per second over the last hour of the log), and the stall. `explain {run, trigger|exit}` is **Why not?**: the head as above
  and its kind's last 10 events up to the cursor, each with where it stopped (`_step`: a scene or region of another story, before
  this visit, a filter that did not match with what it held and wanted (gift's base item as the rules read it), a wolf not in the
  story, turned off, already fired (once-rules), its conditions with today's values, or "fired", from the trigger history); a signal,
  a delay, a direction or a mission gate is not in the log, and says what can send it. `explain {run, fact}` is **What if**: a made-up
  event or story signal through `rules.advance`/`dispatch` on a copy of the state at today's day: the triggers fired, the values
  changed, mission steps, the effects it would send, the phase it would land in with that phase's journal and entry effects, and
  for each trigger of its kind that did not fire, where it stopped. `explain {run, command}` is **"this will do"**: the command taken
  by `Steer` itself (unlocked: `Steer(..., lock=False)`) inside a savepoint that is rolled back (`psycopg.Rollback`), `queue()`
  recording the orders it would send (`_recording`): its verdict (done, refused with the runner's words, or "waits" for the game),
  its words, before and after, the firings, the orders at once, and what the run sends next (the new visit's entry effects, the
  triggers' effects). Nothing is saved, queued, notified or audited (the test counts the tables); a dry run's order numbers are
  not the ones the real command gets (the sequence moves).
- **Every problem at once** (`story_program.problems`, op `problems`). `validate` was cut into its own checks (`_check_header`,
  `_check_audience`, `_check_region`, `_check_scene`, `_check_phase_head`, `_check_entry_effect`, `_check_exit`, `_check_graph`,
  `_check_case`, `_check_fact`; `story_triggers._check_trigger` and `_check_condition`), called in the same order, so it says what it
  said. `problems` runs them part by part and lists every error with its `path` (`phases[1].triggers[0].actions[1]`) and `at` (for an
  editor: `{kind, phase, phaseId, index, id, part}`); a trigger is narrowed to its head, its conditions (`.if`) and each action
  (`trigger_problems`); a broken list is read as empty for the rest of the walk; reachability waits for every phase to be sound.
  `validate`'s own verdict is its safety net, and the seeded test (3,200 mutations of the examples) checks the walk agrees with
  `validate` on its own, net off. Rehearsal cases' errors are listed too. Then the warnings the program alone can tell (a signal sent
  that nothing hears, one heard that nothing sends, a timer cancelled that nothing starts, a counter read that nothing sets) and, for a
  Dungeon Master once the draft has no error, the world's (`reference_problems`): everything `references` checks, marked `blocks`
  (approval refuses it), and what it does not (known problem 4), as warnings: markets and harvest areas (the checkpoint's report),
  towns (regions), ships (due now), the tiles of `npc.move` and `project.post`, an exit's and a prerequisite's filters, wolves and items
  in filters. `references()` and what approval refuses are unchanged. A viewer gets the program's problems without the world's.
- **Alerts** (5.6). `_alerts` gained **stuck** ("cannot move on", needs you: an ongoing phase, nothing waiting on the game, and no
  trigger or exit that can still fire: each off, used up, waiting for wolves none of whom did anything for `goneDays`, or a signal
  that nothing left can send, worked to a fixed point), **overdue** (a timer due more than `overdueSeconds` ago in real time, from
  the world's speed) and **full** (the audience at `fullPercent` of 500). `ALERTS` gained those three rules; **each world's rules** are
  `dm.story_alert_rules` (migration `0072`; op `alert-rules`, the owner's on a hosted world, audited `story.alert-rules`; `null` puts a
  rule back), and a program may set `quietAfterHours` (0 to 8760; 0 means "slow on purpose"), a new optional top-level field of format
  1. A run's "waiting on" no longer lists triggers that are off or used up. A damaged release no longer breaks the list.
- **Outside the tab.** Op `alerts` (`alert_summary`): the runner down, how many need the admin, those runs in words. The Editor's
  `useCampaignAlerts` asks it every 10 s: a count on the Campaigns tab (⚠ when the runner is down; the words as its title) and a line
  on LIVE with "Open Campaigns". **The Dev Console:** every 10 s with its heartbeat the runner leaves its "needs you" alerts and how many
  stories are live in `dm.story_runner` (`alerts`, `alerts_at`, `live`: 0072); the game may read only
  `dm.story_alerts_for_game(world)` (SECURITY DEFINER: running count, the beat's age, the runner rule, the runner's alerts under five
  minutes old), every 15 s (`Game::watchStoryAlerts`, never on a scratch server), and tells each alert once to every Dungeon Master in
  the world (`readStoryAlerts`, a `devNotice`), the runner down worked out in the game itself; a Dungeon Master who comes in, or is
  made one, is told those still standing. The client keeps a notice in the console's log under "⚑ Campaigns" and toasts it.
- **The Editor.** `campaigns/Explain.tsx`: the Overview's *Waiting for* (each trigger and exit as a line with where it stands, its
  conditions marked ✔ ✘ ?, "would fire for: Ash, Birch (here now) and 1 away", Fire… and Why not?; those off or used up folded), *Next
  by itself*, *What if…* (an event or signal, by a wolf, its fields and place). `AskDialog`'s "This will do" is the dry run (the
  program's own words while it is asked, or if it cannot be; a refusal in red). The Triggers tab has Why not? per trigger. The list has
  *Alert rules…*. Story Creator's `ProblemsList` (asked half a second after each edit; a JSON that does not parse is a problem too; "the
  forms cannot show this draft" when they cannot) with **Go**: `StoryPhaseEditor` takes a `jump` and opens the phase, the trigger in its
  list, the section, and outlines it; Campaign settings has the quiet hours.
- **Tests:** `tools/test_story_explain.py` (judged triggers and the counter's value, `judge` against `satisfied` on every combination,
  why not with each verdict, an exit's, what if with nothing saved, the dry run of fire/enter/set and a refusal and a wait with the
  tables counted, stuck with goneDays from the world's rules, overdue at a measured speed, full, the rules and quiet hours, the summary
  and the alerts left for the game read as `ratw_game` (and the story tables still closed to it), reference problems with their paths,
  and the route: a DM, a viewer, a hosted owner and DM); `tools/test_story_problems.py` (every example and the 100 library drafts clean,
  a draft broken in seven places, duplicates and reachability, the 3,200 seeded mutations, the warnings); `Tests/storyline_tests.cpp`
  (the Dev Console notices: told once, Dungeon Masters only, the runner down, a new Dungeon Master told what stands); the client's
  `devNotice`; `Editor/src/lib/campaigns.test.ts` (conditions, judged states, wolves, why-not words, dry-run lines, the new alerts, the
  summary, problem places and order); the render smoke (explained, stuck, two dry runs, the Problems list, the tab bar with the count
  and LIVE's line). The P1/P2 suites pass unchanged. **The slice's full suite:** ctest 121 of 121 (one timing scorecard,
  `talk_scorecard_quick`, failed under `-j8` load beside the Python suite and passes alone); Python 1,140 tests with 9 failures, every
  one failing the same on `r3-camp-p2` (live edit, the map and terrain exporters, the progression catalogue, two of world admin's,
  two of the world package's, and the Dungeon Master mark's browser check, whose `devconsole.mjs` waits for a `.top.dev` button the
  client no longer has); the Editor's and the client's tests and type checks, and the render smoke.
- **End to end** (the slice's), on a throwaway Greyfen world with a real game server, its browser client and four wolves: the bundled
  "The breach at Greyfen" published (its bandit camp spawned by the game), driven by `bandit falls` events in and out of the breach
  (why not and what if checked against them), then through the route as the console does, each control's dry run asked first and
  compared with what the runner then did: hold (a held story read nothing), set, resume catching up, fire `count-defeat` unchecked (to
  the council, whose residents walked, posed and spoke in the game until it opened relief by itself), go back to `hold` without its
  entry effects, stop (stopped once the game acknowledged). A small "evening bell" left with nothing to move it (its timer cancelled
  from the console) raised "cannot move on"; the runner held off for a minute raised the runner alert on the tab bar, LIVE and a DM
  wolf's Dev Console. Screenshots in `/home/martinb/.claude/jobs/ee03c49e/tmp/shots-camp-p3/`.
- **The slice's review and fix:** a signal (or a mission gate) a running timer feeds is never "gone" (a timer sends as the wolf who
  started it: no false "cannot move on"); the heartbeat's alerts can no longer stop the runner (database and other errors are
  swallowed in their savepoint; a damaged run no longer breaks the list); a dry run's every failure is "refused", never a 500; each
  visit keeps `state.visitCursor` (the event that opened it), so Why not? tells this visit's events from the last one's by the log,
  not the day (older runs fall back to the day), and skips the story's own `story phase`; a made-up scene or region event is this
  story's and this visit's; the game keeps the runner's last alerts standing while it is down and does not tell again an alert that
  went for under five readings; the Problems list's "checking…" clears on a JSON that does not parse.
- **Not in Phase 3, on purpose:** "who else has this campaign open"; stepping through a rehearsal with `explain` (4.5, Phase 4's
  workbench); world variables, parties and quests in the judgements (Phases 5, 6, 10); a per-program `goneDays`; the Dev Console told
  of alerts other than "needs you".
- **For Phase 4:** migrations are now 0073 (drafts), 0074 (5b), 0075 (Phase 6), 0076 (Phase 8). `story_program.problems` and its
  `at` locations are the Problems panel's source (4.6): the workbench should keep `at`'s shape (`kind`, `phase` index or null for the
  campaign, `index`, `id`, `part` like `actions[2]`, `if`, `objectives[0]`, `events[3]`) or map it; `ProblemsList` and `problemPlace`
  (`Editor/src/lib/campaigns.ts`) can move to the workbench as they are. `story_triggers.judge` and `listeners()` are what "step
  through" (4.5) needs on a rehearsal's state. The catalogue (Phase 4's first item) should give the condition and event sentences that
  `condText`/`eventText` now make by hand. `program_warnings` is where signal and counter cross-checks live; the rename-a-signal tool can
  reuse `_signal_uses`. `quietAfterHours` is a top-level program field the workbench must keep on a round trip.

### Slice B: the editor and the language

**Phase 4: The catalogue and the workbench. About 18 hours.**
- `Data/Storylines/trigger-catalogue.json` for everything that exists (160 events with groups and what their
  fields hold, 7 conditions, 7 actions, 25 effects), and the validator's tables read from it.
- The workbench of section 4 on today's vocabulary: tree, folders, notes, sentences, pickers, search,
  duplicate, copy and paste, undo, drag, Problems, exits shown as triggers, "New small story" from a library
  entry. Op `refs`.
- Migration `0073` (Phase 3 took `0072`): `dm.story_drafts`; the draft ops. The two old tabs removed.
- Tests: the render smoke over all 8 examples and the 100 library drafts; a round trip (open, save, compare) for
  each, so the workbench never changes a program it did not edit.

**Phase 4 as built** (2026-11-22, branch `r3-camp-p4`, on `r3-camp-p3`). Migration `0073`, no engine change, no new game order;
two optional trigger fields.
- **The catalogue** (`Data/Storylines/trigger-catalogue.json`, 162 events, 7 conditions, 7 actions, 25 effects). Each event has its
  group (the 15 of 1.3), a title, a help line, a sentence whose slots are its fields (`{actor} gives {item} to {target}`), and for each
  field it fills its type (wolf, resident, character, place, item, faction, town, market, ship, area, project, notice, contract, letter,
  run, phase, region, cue, text, number), what it holds and the word for "anyone"; `tile`, `mission`, `measures`, notes. The fields of
  the ~140 undescribed events were read from every `recordEvent` call (findings worth knowing: `World::recordEvent` fills the cell and
  the tile from the actor when a call leaves them empty, so most wolf events carry a tile; the town-side project events write the town
  as `town:<id>` and lose their tile, since `project posted`/`built` pass x,y in the time slots; `bandit falls` has the camp as target;
  `cleared` is a DM spawn rule clearing a fallen resident, not bandits; `economy`'s actor and target are ledger accounts). Conditions,
  actions and effects carry typed `params` (`int` with bounds and `nonzero`, `choice` with values and words, `text` with a length, the
  reference types) and a sentence over them; a slot `{cell,x,y}` is one tile picker. **The validator reads its tables from it**:
  `story_program.EVENTS`, `MISSION_EVENTS`, `MISSION_MEASURES`, `EFFECTS`, `INJURY_TYPES`, and `validate_effect` checks each parameter
  by its catalogue entry (`_param_ok`); `tools/test_story_catalogue.py` compares it with the hand-written checks it replaced over every
  parameter of every effect with in- and out-of-bound values (3,000+ cases, same verdict and words). `catalog` serves it as `catalogue`.
- **Two trigger fields**: `folder` (up to 80 characters) and `note` (up to 300), words for the author that the engines ignore (both read
  rules by key; the digest covers them).
- **The workbench** (`Editor/src/dm/triggers/`: `Workbench`, `TriggerTree`, `TriggerView`, `PhaseViews`, `Sentence`, `pickers`,
  `Library`, `Rehearsal`, `Problems`; the logic, without React, in `Editor/src/lib/workbench.ts`). Left: the tree (campaign triggers,
  then each phase with its triggers grouped by folder and numbered in the order they are tried, its **Exits** shown as "when … → go to …"
  triggers, its Missions and "When it opens"; then regions, scenes, rehearsal cases, settings). Drag a trigger to reorder it or into
  another folder or phase; right-click for Duplicate, Copy, Cut, Paste after, Copy as text, Rename (rehearsal cases that expect it follow),
  Save as a template, Delete; Shift/Ctrl pick several; Ctrl+C/X/V/D, Delete, F2, Ctrl+F. Middle: the selected trigger as **Events /
  Conditions / Actions in sentences**, every parameter a slot that opens its picker in place (residents and wolves by name, a place then
  a **tile on the real map**, regions, items by category, factions, towns, markets, ships, areas, templates, projects, phases, missions,
  scenes, cues, signals and counters of the story, numbers within their bounds, words with their length; "the acting wolf", and "Enter
  an id" stays), the event chosen from the catalogue's groups with each one's help and fields, actions and effects from grouped lists,
  conditions as nested All/Any/Not groups, a slot red with its problem as its tooltip; the folder, the note, how it fires, for whom, how
  often the last rehearsal fired it, and its signal connections. Right: the **Problems list** (Phase 3's, moved, [Go] selects the node),
  search results, and the story's signals, counters and regions with **every use** and rename-everywhere. **Undo and redo** over the whole
  draft (typing in one field is one step; 200 steps). **Library…**: the 100 events' openings and objectives pasted in as triggers in a
  folder of their own (the opening sends `<id>-begins` instead of going to the library's "work" phase), a library event's mission added
  to a phase, a small story started from one; the DM's own templates (`ratw-triggers` fragments: triggers and the regions they use) kept
  in the browser, exported and imported as files. **New…**: a small story from a library event, a blank campaign, one of the 8 examples.
  Import and Export; **Source** (the JSON, undone with the rest), which is also what shows a draft the forms cannot (risk 6).
- **Rehearsal** along the bottom: each case pass or fail with its path; choosing one shows its counts in the tree and dims what it never
  fired; **Step through** (new op `rehearse-step`, a viewer's too: `story_program.rehearse_steps`, the review's loop now `_play_case` with a
  step callback, and `story_runner.listeners` on the rehearsal's state at each step) plays it fact by fact: the fact in the catalogue's
  words, phase and visit, journal, counters (changes marked), timers, missions, what fired, what was sent, and "Waiting for" judged as the
  console judges a running story.
- **Drafts on the server** (migration `0073_story_drafts.sql`, `tools/story_drafts.py`, ops `draft.list`, `draft.open`, `draft.save`,
  `draft.delete`, a Dungeon Master's): `dm.story_drafts` (name, owner, `program json`, `version`, saved by and when) and
  `dm.story_draft_versions` (the newest 20). The program is `json`, not `jsonb`, so a draft comes back with its own key order. A save names
  the version it was made on and is refused over a newer one ("ann saved this draft at 14:02 (version 2) after you opened it"); any DM
  opens and saves the world's drafts; the owner, a builder or the world's owner deletes. A broken program is kept as it is. Closed to the
  game. The browser keeps the unsaved text and which draft it is; a viewer's drafts stay in the browser as before.
- **Op `refs`** (`story_runner.refs`): residents, wolves, places (with size and town), factions, towns, markets, harvest areas, ships, and
  `static_refs()` (items with categories, DM templates, project kinds, injuries); a viewer gets the static part.
- **The console's words** come from the catalogue too: `condText`/`eventText` take a `Wording` (`workbench.wordingOf`), which the
  Campaigns tab passes once it has read the catalogue (`WordingContext`); without it they say what they said.
- **Retired:** "Write draft" and "Inspect rehearsal" (`lib/storyRehearsal.ts`), `StoryPhaseEditor`, `StoryTriggerEditor`,
  `StoryEffectFields` and `SceneCueFilter`.
- **Tests:** `tools/test_story_catalogue.py` (every event described, every parameter a slot, the tables read from the catalogue, the
  effect checks against the hand-written ones); `tools/test_story_drafts.py` (the round trip of all 8 examples and 100 library drafts
  through the server, key order included; versions, a save over a newer one, the newest 20; a broken draft kept; deleting; closed to the
  game; `refs`; a rehearsal stepped through; the route for a DM, a second DM, a builder and a viewer); `Editor/src/lib/workbench.test.ts`
  (every trigger, condition, action, effect and exit of the 108 programs as a sentence; the tree; round trips of a move and back, a cut
  and paste back, renames and back, edits undone and redone; unique ids, regions pasted along; uses, search, problem places; the console's
  words); the render smoke renders the workbench for all 108 programs with each trigger, exit, phase, missions, settings, cases and scenes
  selected (1,194 views; showing a program never edits it), the pickers and the rehearsal stepping. The P1-P3 suites pass;
  `test_dungeon_master`, `test_world_db`, `test_studio` pass; `test_world_admin`'s two known failures are the same on `r3-camp-p3`.
- **Seen:** on a throwaway Greyfen database (its map, roster, three wolves, two drafts; no game server, nothing published), the real DM
  suite: screenshots in `/home/martinb/.claude/jobs/ee03c49e/tmp/shots-camp-p4/` (a trigger, the resident picker, the event picker, the
  tile picker on the map, the library and its triggers added, the context menu, a duplicate id in Problems, a rehearsal's trace and step
  through, a pasted trigger with a note in a folder, an exit as a trigger, search and uses, unsaved and saved, New…, a small story, Source).
- **Not in Phase 4, on purpose:** the Enabled / "initially on" box and the Variables panel (Phase 5's `enabled`, declared variables;
  the right column lists signals and counters as they are used); nested If/Else/For each (Phase 5); a rehearsal case's facts written with
  the event picker and forms for expected effects and counters (the case form is today's), and "Rehearse here"; quests in the tree (Phase
  6); a draft's older versions opened from the page (the op takes `version`; the page opens the newest); the hosted desktop shell needs
  nothing new (the route is the same).
- **For Phase 5:** add the new conditions and actions to the catalogue's `conditions`/`actions` with params and sentences, and the tree
  and sentences follow; nested actions need `ActionList` to recurse (it is one list today) and `Ref` to name a nested action for
  Problems' `part`; `enabled` goes in the trigger head of `TriggerView` and as the tree's checkbox (`TriggerTree`'s `Row`); declared
  variables replace `countersOf` (the pickers' `counter` type and the right column); `withEvent`, `newAction`, `newCondition` are where
  defaults live; keep `editable()` in step with the validator so a version 2 program opens in the workbench; `wordingOf` gives the console
  the new sentences for free.

**Phase 5: The trigger language, version 2. About 16 hours.**
- Declared variables, types, lists; `compare`, `switch`, `in list`, `phase is`, `timer`, `fired`, `chance`,
  `days since`; `if`, `wait`, `each`, `random`, `run`, `enable`, `disable`, `stop`, `note`, `finish`; the state
  actions. Format version 2, version 1 read as before.
- Both twins and the parity test; rehearsal support (pinned rolls, waits, expected variables).
- The Variables panel, nested actions in the workbench, and variables with their types in the console.
- Slice end: full suite; the 8 examples rewritten where version 2 makes them shorter, with their rehearsals
  unchanged in outcome; one review-and-fix (moved to the end of Phase 5b, which closes the slice).

**Phase 5 as built** (2026-11-22, branch `r3-camp-p5`, on `r3-camp-p4`). Format version 2 beside version 1; migration `0077` (the
world's variables; 0074 to 0076 stay Phase 5b's, 6's and 8's); no new game order (`story.finish` carries the outcome).
- **The language** (`tools/story_triggers.py`, `tools/story_program.py`; `Docs/story-programs.md`, "Version 2"). A program of
  `"version": 2` declares `variables` (up to 64): `{id, type, scope, initial?, show?, note?, clock?}`; types `number`, `switch`,
  `text`, `wolf`, `resident`, `place`, `region`, `item`, `faction`, lists `numbers`, `texts`, `wolves`, `residents` (500 values), and
  `timer` (a moment on the game or real clock; "days since"); scopes `story`, `player`, `party` and `world`. An undeclared or
  mistyped name is an error; version 2 has no `counter` test and no key/scope Set and Add (it compares and sets declared
  variables). Values (operands): constants, `{var, of?}`, `{field}`, `{count}`, `{since}`, `{each}`, `plus`/`minus`/`min`/`max`
  of two, typed number / switch / text when checked. Conditions: `compare`, `switch`, `in list`, `phase is`, `visit is`, `timer`
  (running, or days left), `fired` (counts per visit and run: `state.counts`), `chance`, `days since` (phase, run, or a timer
  variable). Actions: `if` (+ up to 4 `elif`, `else`), `wait` (game clock from a minute, real from 5 s; not inside For each),
  `each` (participants, the actor's party, or a story/world list; at most 500 turns; not nested), `random` (2-8 weighted),
  `run` (`check`: on and its conditions; cycles refused, 8 deep at most), `enable`/`disable` (`phase` to name another phase's),
  `stop`, `note`, `finish` (`everyone`, last in its list: ends the run; or `$actor`/`$each`: that wolf's own ending), and the
  state actions `set`, `add`, `subtract`, `multiply`, `clamp`, `turn on`, `turn off`, `toggle`, `remember`, `list add`, `list
  remove`, `list clear`, `pick`, `start timer`, `stop timer`, `shift`. 24 actions a trigger counting nested ones, nested 3 deep;
  `$each` in effects (wolf and resident parameters) inside For each. A trigger's `"enabled": false` starts it off (its key is put
  in `state.disabled` at `begin`, so the console's on/off and the engines needed nothing new). A version 1 program is checked and
  run exactly as before: every new path is behind the phase's `_v == 2`.
- **Running it, both twins.** `Core/RatwStoryTriggers.cpp` gained `Dispatch2` (a request `v: 2` carries the run's state pieces and
  gets them back as the Python twin leaves them, key for key: `story_native.fire_v2`); the Python twin runs the same tree
  (`_run`, `_resume`, `_act`, `test_v2`). Rolls: FNV-1a 64 of `"<seed>:<n>"` (the seed is the run's id; a rehearsal's `seed` or the
  program's id), or a case's pinned `rolls` (0-99) first. A wait is `state.waiting` (`{trigger, scope, phase, at, clock, actor,
  event, next}`, `next` an address like `[3, "then", 2]`); `advance` resumes the due ones after the timers on each clock fact; a
  phase trigger's wait ends with its visit (`leave_visit`, now shared by the rules and the console's "go to phase"). Who is
  off is read once per event (an Enable takes effect from the next event; Run's `check` reads it live). Notes go in the firing's
  history entry (`notes`); Run and resumed waits are firings of their own (`via: run|wait`). `finish` for everyone stops the run
  reading: the runner sends what was queued, then `story.finish` (`story.stop` for "abandoned") with the outcome, then `complete`.
  Party scope: `state.parties` (a rehearsal's `parties`; the runner has no party data until Phases 6/8: a party of one).
- **World variables**: migration `0077_story_world_vars.sql`, `dm.story_world_vars(world_id, name, type, value, run)`; loaded into
  `state.counters.world` when a run is ticked, steered or explained (a value kept under another type is not read), written back
  when changed (`world_load`/`world_save`). The doc's Phase 10 item for it is done; `story.start` and chapters are still Phase 10's.
- **The runner and the console's side** (`tools/story_runner.py`): `variables()` (declared, typed; version 1's counters as numbers)
  in `controls`; `set` is typed by the declaration and works in all four scopes; values before and after carry party and world
  and their type; Why not / Explain judge the new conditions (`judge` with the phase and day; a chance is unknown, "rolled when it
  fires"; per wolf when a condition reads a wolf's variable); "cannot move on" is not raised while a wait is pending; every walk
  over actions (routes, effects, signals, timers, references, `studio.story_effect_kinds` for the hosted permission checks) now
  goes into nested lists. New op `upgrade` (a viewer's, pure): `story_program.upgrade`, version 1 to 2.
- **Rehearsal:** cases may add `seed`, `rolls`, `parties`, `expectVariables` (only the named ones; unset reads its first value) and
  `expectOutcome`; a version 2 report adds `outcome` and `outcomes`; step-through shows the variables and `startsOff`.
- **The workbench:** nested actions (If / Else if / Else, For each, Random choices with weights; each list its own numbered
  sentences; `ActionList` recurses, a problem's `part` is the whole path, `actions[3].elif[0].then[1]`, and Problems words it
  "action 4 › else if 1 › 2"); the **Enabled at the start** box in the trigger head and as the tree's checkbox (a hollow box
  starts off; version 2 only); the **Variables panel** (declare, type, whose, first value, shown as, note, uses, rename everywhere,
  remove) replacing "counters as used" (a version 1 story lists its counters and offers **Make it version 2…**); pickers for a
  variable (by the slot's types), a value (`OperandPicker`: number, on/off, text, variable, event field, count, days since, the
  looped value, variable ± a number), a trigger, whose; the catalogue's new entries (9 conditions, 26 actions, `v2`/`v1` marks, `do`,
  `nest`, `variables`) drive the sentences and the Add lists; `editable()` accepts version 2; `renameVariable`, `renameTrigger`
  follows Run/Enable/Disable/"fired", `mapActions`/`allActions` walk the tree.
- **The console:** Variables & timers shows every variable with its type and whose (story, the world, each wolf, each party),
  typed set controls, the actions waiting and when they resume, and the story's outcome; the timeline shows notes and "run by
  another trigger" / "after its wait".
- **Examples:** `siege-at-greyfen` is version 2 (two triggers and two signals fewer: count-defeat and the posts each test with an
  If; the same paths, effects and counters in all four rehearsals). The other seven stay version 1: five have no triggers to
  shorten, and reed-ford and winter-ferries are the console suites' fixtures (their version 2 forms are rehearsed by the tests).
- **Tests:** `tools/test_story_native.py` (parity: the 108 programs, the 108 made version 2, and the two version 2 samples of
  `tools/story_v2_samples.py` under five more seeds; also ctest `story_trigger_parity`); `tools/test_story_v2.py` (the checks:
  version 1 refusing what is new, undeclared and mistyped names, declarations, 16 flow rules; problems at nested parts;
  warnings; nested effects seen by the permission checks; rehearsals with pinned and seeded rolls, waits, expected variables and
  outcome, initially off, step through; upgrade of all 108 rehearsing the same; judge = satisfied on version 2 conditions);
  `tools/test_story_v2_runner.py` (a version 2 story published in a throwaway database: waits resumed by the world's clock, notes,
  the world variable kept and read by another story, finishing to complete; the console turning on a trigger that starts off,
  typed sets in three scopes, why not, dry run and fire). `Editor/src/lib/workbench.test.ts` (110 version 2 programs editable and
  in sentences, parts, renames and maps round trip, every action and condition the workbench adds passes the checks),
  `campaigns.test.ts` (nested problem places), the render smoke (the 10 version 2 programs' workbenches, 148 views, none edited).
  The P1-P4 suites, `test_dungeon_master`, `test_studio`, `test_world_db`, the live siege and scene tests pass.
- **Seen:** on a throwaway Greyfen database (drafts of the samples, the breach, a broken one and a version 1 one; two version 2
  runs published and driven by hand, no game server), screenshots in `/home/martinb/.claude/jobs/ee03c49e/tmp/shots-camp-p5/`
  (nested If and the Variables panel, the value picker, a trigger off at the start, For each with $each, the Add list's flow,
  a new variable, Random/Wait/Stop, a campaign wait, problems in nested actions, a version 1 story and Make it version 2, the
  breach rehearsed, the console's typed variables, notes in the timeline, triggers, actions waiting).
- **Not in Phase 5, on purpose:** trackers for `show` and `show timer` in the wolf's journal (Phase 6, with the journal's
  trackers); `{variable}` inside texts; wolf variables as an effect's target (`$each` and `$actor` only); `region:<id>` in For
  each and party membership from the game (Phase 8's facts); rehearsal facts and expected values written with forms; the
  console's Explain list does not show pending waits beside the timers (the Variables tab does).
- **For Phase 5b:** a run's state now may hold `waiting` (addresses into a trigger's action tree: a migration must drop a wait whose
  trigger is gone or whose actions changed, or resume at the same address only when the list it points into is unchanged),
  `counts` (by trigger key), `seed`/`began`/`draws`/`rolls`, `outcome`/`outcomes`, and `disabled` seeded from `enabled: false` (a
  new version's newly off triggers are not off in a running run unless the migration adds them). `story_program.upgrade` makes a
  version 1 program version 2 with the same counter names (a v1 run's `counters` read as declared numbers, except a name used in
  both scopes: the wolf's becomes `<name>.player`). World variables live outside the run (nothing to move).

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
  delivered by the move itself (a phase's entry effects are not run again). Migration `0074`
  (Phases 6 and 8 then take `0075` and `0076`; Phase 3 took `0072`, Phase 4 `0073`): `dm.story_versions(run, digest, program, approved_by, moved_at,
  moved_by, report jsonb)`, one row per version a run has been on; the release row keeps the current one.
- The console: "Move onto a new version…" shows the migration report (kept, reset, dropped) and waits for a second
  click and a reason; the timeline and the Audit say "moved to version b7e1 by martin"; the header names the
  version.
- Who may: as any steering control (5.7). Rewards and world changes already delivered stay (5.8, "not a rollback").
- Tests: kept, reset and dropped on fixture runs of the 8 examples edited three ways; a phase that no longer
  exists; a timer whose listener went; moving twice; a command made against the old version refused after the move.
- Slice end (B): full suite; one review-and-fix.

**Phase 5b as built** (2026-11-22, branch `r3-camp-p5b`, on `r3-camp-p5`). Migration `0074`; no new game order (`story.phase` gains
`revise`); no change to the program format.
- **The migration** (`tools/story_program.py`, "moving a running campaign onto a new version"; pure). `version_diff(old, new)`:
  triggers by the state's keys (a campaign trigger by id, a phase's as `phase/id`) added, removed and changed with the fields that
  changed (`wordsOnly` when only `folder`/`note`), variables (by scope and name; a version 1 story's counters as it reads and
  writes them: `declared()`) added, removed, retyped, rescoped, changed and, version 1 onto 2, `renamed` (a wolf's counter named
  in both scopes is `<name>.player`, as `upgrade` names it), phases, missions (and whether their objectives changed), timers (the
  signals the story can start, and how), regions, scenes, the header fields, `same`. `migration(old, new, state, land, run, began)`
  returns the new state and a report of what is **kept**, **reset**, **defaulted** (back to its first value), **dropped** and
  **added**, each line with why, plus `warnings`, the `phase` mapping (`from`, `to`, `choices`, `gone`) and `blocked`: the same
  story id; the run stays in its phase, and only when the new version has no such phase may (and must) the admin name one to
  land in, which is not an ending (the visit carries on as that phase; keys of the old phase's triggers follow it); scene initiators
  in the run's audience. Variables kept by name, scope and type when the value fits the new type (a region that still exists, a
  number in bounds); retyped, rescoped or not fitting: defaulted; gone: dropped; world variables are the world's (a retyped one is
  warned about). Timers kept while their signal still has a listener (a visit timer in the landing phase, a campaign one anywhere);
  a changed schedule keeps the running timer's time. Once-marks kept by trigger when its once-rule still uses that mark (else
  dropped: it may fire again); counts and turned-off keys by trigger; a trigger **newly off** (`"enabled": false` and new, or on
  before) is turned off, one newly on stays as the run has it (warned). A **wait** is kept only when its trigger is there and what
  it still has to run is unchanged (`_remaining`: the list its `next` points into, whole, and after each action it is inside the
  rest of that action's list), else dropped. A mission's progress is kept when its objectives and mode are unchanged, else reset;
  an exit's votes and steps when that exit (by index) is unchanged. Version 1 onto 2 gets `seed` (the run's id) and `began` (the day
  of its first visit); version 2 onto 1 drops waits, counts, rolls and personal endings. The audience stays the run's (warned when
  the new version's differs).
- **Approving "as a new version of run X"** (op `approve` with `forRun`): the ordinary approval (rehearsed, every phase covered,
  the world's references, the hosted checks: `faction.relation` needs a builder, wolves' harm "may act on wolves", the map's "ahead"
  check, since it is op `approve`), then `approve_version`: a live run of the same story id, not already on that digest; a row in
  `dm.story_versions` (`approved`; an older approval of the run waiting is `withdrawn`). The reply is the report against the run as
  it stands. No ticket, nothing published.
- **Migration `0074_story_versions.sql`**: `dm.story_versions(run, digest, program, approved_by, approved_at, status (approved,
  moving, current, past, refused, withdrawn), from_digest, moved_at, moved_by, command, visit, at_event, game_day, report,
  state_before, receipt, settled_at, problem)`, one row per version a run has been on or has had approved; the release row keeps the
  current program, digest and approver (`approved_at` stays the run's first). Closed to the game. Not applied to `ratw_ridgemere3`.
- **The move** is the inbox's command `migrate {version, phase?}` (visit- and digest-bound as every command; a flow command: it waits
  for the run to have sent and had answered its visit's orders, and on a held or paused run part-way through is refused). `Steer.
  do_migrate` works the report out again against the run then, refuses what is `blocked`, writes the version it leaves (the one it
  was published on gets its row now, as `past`), changes the release's program, digest and approver, sets the state (with
  `entrySent`: the visit's entry effects are those already sent, so a new entry effect of this phase is never sent by the move),
  and sends the game **`story.phase` with `revise: true`** for the same visit: the new title, journal, regions, guest voices, lines
  and missions (`state.moveReceipt`; also kept in the visit's receipts for the Orders tab). Until the game answers, the run reads no
  events (`_tick_run` returns) and takes no command but hold, resume and stop. `settle_move` (each tick, held and paused runs too):
  applied, the version is `current`; **refused, the run goes back**: the old program, digest, approver and `state_before` (a hold made
  meanwhile kept), the version `refused` with the game's words, an audit row `story.migrate` by the runner; expired, sent again.
  Commands queued against the old digest are refused once it has moved. Rewards and world changes already delivered stay.
- **The game** (`Game::applyWorldStoryPhase`, `Core/RatwGameStorylines.cpp`): `revise` is accepted only for the visit a live world story
  is in, never with `end`; checked whole (regions fit their cells, speakers exist) before anything changes; it replaces the title,
  regions (and the region watch), speakers, lines and the last step's title, journal and missions, with no new step, no toast and no
  event; scenes under way go on. `story.tell` and area effects then use the new regions.
- **Reads**: `status` gains `versions` (newest first; an approval not moved onto carries a fresh report against the run now) and each
  row `storyId`, `format`, `moving`; `timeline` a fifth source, `version` (`m:<id>`, ranked after commands; a poll gets new ones,
  one still moving and those settled since), with the counts, the phase landed in and the warnings. The dry run (`explain {command:
  {kind: 'migrate'}}`) returns the report as `migration`.
- **The Editor**: the Story Creator's approval has "or as a new version of a running campaign" (the live runs of this story id) and
  shows the diff and the counts, with "Open in Campaigns". The console's steering row has **⇪ Move onto a new version…** when one is
  approved: `MovePanel` (what changed, in lines; for this run as it stands: dropped, reset, back to its first value, new, kept, each
  line with why; warnings; "Land in:" when its phase is gone) and then `AskDialog` with the dry run's lines and a reason. The header
  says when it is moving; the timeline shows "moved to version b7e1c2d4 by martin (from a41c9e00): 9 kept, 4 dropped" and "the game
  refused version …: back on …" (under Admin); Audit has a **Versions** table; the phase strip ends in the phase it landed in.
- **Tests:** `tools/test_story_versions.py` (pure: the 8 examples at every step of every rehearsal, edited three ways: words only
  (the state unchanged, nothing reset, defaulted or dropped), made version 2 (every counter kept by upgrade's names, the rest of the
  rehearsal giving the same phases and story counters), a trigger of the phase taken away (its marks and waits gone, nothing else);
  a version 1 run onto version 2 with a counter named in both scopes; a phase taken away (blocked, then landed, keys following); a
  variable retyped and one rescoped; a wait part-way through a list kept and resuming in the new version, dropped when what it still
  runs changed, when an action before it changed, when its trigger went; a timer whose listener went; missions kept and reset; a
  trigger newly off; the diff. In a throwaway database: a version 1 run moved onto version 2 (the dry run saves and sends nothing,
  the revise order, nothing read while moving, other commands refused meanwhile, a command queued against the old version refused,
  the versions, the timeline, the audit, carrying on as version 2), refused by the game and back (held meanwhile, still held), moving
  twice with a phase taken away, entry effects not sent again and an older approval withdrawn, a version 2 run with a wait moved and
  resuming; the route: approve for a run (a viewer refused, another story refused), the move needing "may steer campaigns" on a hosted
  world); `Tests/storyline_tests.cpp` (a revise: a region that does not fit changes nothing, another visit, `end`, an unknown story
  refused; revised in place with one step, the new words and region; `story.tell` with the new region only); `campaigns.test.ts`
  (report lines, groups, the diff, the timeline line and filter, the cursor, the command, before and after, the dry run, the phase
  strip after a landing); the render smoke (the button, the panel, the panel with "Land in:", moved, its timeline, moving, the dry
  run).
- **Seen:** on a throwaway Greyfen database (three wolves; a version 1 tally, the version 2 rope ledger and the bell with its waits,
  published and driven by hand; a loop standing in for the game and the runner, refusing a revision with a region called "pier"),
  screenshots in `/home/martinb/.claude/jobs/ee03c49e/tmp/shots-camp-p5b/`: the bell's Move panel (four waits dropped, three kept, a
  new variable), the dry run, moved (timeline, audit, variables); the ledger moved onto a version the game refused and back; the
  ledger moved onto a version without its phase, landing in `muster`, and its Versions; the tally's version 2 approved from the Story
  Creator as a new version of its run, moved, `tally.player` in its variables.
- **The slice's full suite:** ctest 121 of 121 (`talk_scorecard_quick`, a timing scorecard, failed under `-j6` load beside the Python
  suite and passes alone); Python 1,193 tests with 8 failures, each one of Phase 3's known list, failing the same before this phase
  (live edit, the map and terrain exporters, the progression catalogue, two of world admin's, two of the world package's); the
  Editor's tests (118 and 78), type check and render smoke (43 panels; the workbench over 108 and 10 programs).
- **The slice's review and fix:** a refusal with no record of the version it came from leaves the run paused on the new one rather
  than stuck moving; the strip after a landing; the diff names upgrade's renames as renames, not a variable moved.
- **Not in Phase 5b, on purpose:** moving onto another story (a new id); changing the run's audience (Phase 6's participants); a
  wait kept when only the actions before it changed (the list it points into must be unchanged); converting a retyped value (it
  starts again); landing in another phase when the current one still exists (move, then go to phase); withdrawing an approval by
  hand (a newer one withdraws it).
- **For Phase 6:** migrations are now 0075 (Phase 6) and 0076 (Phase 8). A run may be on a program it was not published with: read
  the program from the release each time, never cache it by run id (the digest changes; `status`'s `have` already keys on it).
  Quests and `dm.story_player_state` must join `story_program.migration`: quest progress kept by quest id when its objectives are
  unchanged, reset otherwise, as missions are, and per-wolf state moved out of `state` must be carried (and put back by a refused
  move, `state_before`). `participant.add`/`remove` changes `state.cohort`, which a move keeps as it is. The game's `story.phase`
  `revise` replaces the last step's missions; the journal's quest payloads will need the same in-place update. `RECEIPT_KEYS` now
  holds `moveReceipt`.

### Slice C: quests, the world, and speed

**Phase 6: Quests and a living audience. About 18 hours.**
- Quests (3.6) in the rules and the journal payloads; audiences `open` and `everyone`; `participant.add` and
  `remove`; the game's `story.cohort` order; `npc.offer` and `dialogue choice` in doc 83's window. Small pick-up
  quests are the Story Maker's (**decided, user, 2026-11-21**, question 2), so the open audience and "New small
  quest" are in this phase in full; doc 58's templates stay and are offered as starting points.
- Coin rewards name a purse (**decided, user, 2026-11-21**, question 4): a quest's `character.grant` reward says
  which town, House or faction pays, and is refused (and, with `ifRefused`, held or skipped) when the purse is short.
- Migration `0075`: `dm.story_player_state`; the cap raised.
- The client's journal: optional, hidden, failed, trackers, abandon.
- The Quest editor, "New small quest", and the console's Quests grid with every control.
- Tests: rehearsals for each scope, any-order, fail, repeat; a game test for the offer in the window.

**Phase 6 as built** (2026-11-22, branch `r3-camp-p6`, on `r3-camp-p5b`). Migration `0075`; new game orders `story.cohort`,
`story.quests`, `item.give`, `standing.adjust` and `character.grant`'s `purse`; seven new game events; version 2 only.
- **Quests** (`tools/story_quests.py`, pure; checked by `story_program.validate` and `problems`, `Docs/story-programs.md`, "Quests and a
  living audience"). A version 2 story declares up to 32 `quests`: scope `player`, `party` (the party the wolf was in when it started;
  members fixed then; one who leaves keeps the credit earned and earns no more) or `story` (one copy, `contributors` different wolves);
  1-12 objectives (the mission events or a story signal, filters, goal and measure; `after` for what comes first, none for any order;
  `optional`; `hidden` until revealed or done); `fail` (an event, or `afterDays` on the game or real clock); `lifetime` campaign or
  phase (copies under way end "expired" with the visit); `phases` (when it is offered); `repeat` once, daily, weekly, always (a failed
  or abandoned one may be taken again at once); `giver` and `offer` (the wolf's words to ask, the resident's offer, accept, decline,
  hand in, thanks) and `handIn` (with a giver: ready, then handed in to finish); `board` (a town's notice board); `join` (`who`:
  anyone in an open story, else participants; a `region` to stand in; `cap` copies under way; `limit` starts in all); `abandon`;
  `rewards` (story effects: to each wolf with credit where they say `$actor`, once otherwise) and `bonus` (when every optional
  objective was done too). Copies are kept in `state.quests` (`{quest: {wolf | party:<id> | story: copy}}`), offers in `questOffers`,
  starts in `questStarts`. A phase with quests open in it goes on without exits or triggers (`story_program.ongoing`): a small quest
  is one phase and one quest, and "cannot move on" is not raised for it.
- **The living audience.** Audiences `open` (nobody at first) and `everyone` (the active wolves, and each who signs in: `arrival`),
  each with a `cap` (up to 5,000; a fixed or active audience stays at 500). A wolf comes in by taking a quest (the join rules checked
  by the story: open in this phase, who, cap, limit, repeat, the story's cap; refused, the wolf is told in a toast), by
  `participant.add`, or by the console; leaves by `participant.remove`, the console, or leaving the story from the journal
  (`storyline abandoned` of this run), their quests under way abandoned (what they did is kept).
- **The story's own effects** (catalogue `"story": true`): `quest.offer`, `quest.start`, `quest.complete`, `quest.fail`,
  `objective.complete`, `objective.fail`, `objective.reveal`, `objective.progress`, `participant.add`, `participant.remove`, and
  `npc.offer` (a story choice laid over a resident's window for one wolf or every wolf, once or always, for the visit). Both twins
  carry them as any effect (no C++ change to the engine; the parity test runs the quest samples), and `story_program.dispatch` takes
  them out of `triggerEffects` after each firing and keeps them (`quests.take_story_effects`). Quest, offer and start may name
  `$actor` from a trigger of anyone in the world (that is how an open story is joined). Quests' own facts, `quest started`,
  `quest completed`, `quest failed`, `objective completed`, follow the fact that caused them in the dispatch queue, as signals do.
  Optional effect parameters (`"optional": true`) and a `bool` type in the catalogue; types `quest`, `objective`, `purse`, `offer`.
- **Coins from a named purse** (the user's answer to question 4): `character.grant` gains `purse` (`town:<town>` its treasury,
  `town:<town>:works|watch|church`, `till:<id>`, `chapter:<id>`); the game pays from that account or refuses ("The purse is short
  (Np).", "There is no such purse to pay from."), the day's grant cap kept, recorded as a story reward. Also `item.give` (one wolf,
  1-20, a full pack refused) and `standing.adjust` (-10..10 with a faction; standing is a Chapter's, so a wolf with no Chapter is
  refused). A reward the game refuses is noted (`rewardsRefused`, a note in the history) and the story carries on (Phase 7 adds
  "when here").
- **Parties told to the runner**: the game writes `party joined` and `party left` (each member at a disband, both of a party of two
  losing one); the runner reads each wolf's party from the world's save when it is newer (`partiesAt`) and keeps it from the events
  (`state.parties`), for stories with party quests or party variables. Phase 5's "a party of one" is gone where the game says.
- **Each wolf's own state** (migration `0075_story_player_state.sql`, `dm.story_player_state(run, player, state)`): for a story with
  quests, an open audience or story effects, a wolf's variables, quest copies, ending and journal mark are kept in their row, merged
  in when a run is read (`_players_in`) and taken out when it is written (`_dump`, only the rows that changed; a wolf whose parts all
  went loses the row). Other stories keep their one JSON as before. A move onto a version with quests starts keeping rows; a refused
  move puts the rows back with `state_before` (and drops them if the old version kept none).
- **The game** (`Core/RatwGameStorylines.cpp`, `RatwGameWindowTalk.cpp`, `RatwGameDialogue.cpp`, `RatwBoards.cpp`, `RatwStorylines.*`):
  `story.cohort` (5,000 at most), `story.quests` (`offers`, `choices`, `wolves`: each wolf's quest views and trackers, and a toast;
  saved with the storyline; up to 64 offers and choices, 64 views and 16 trackers a wolf); `story.phase` takes 5,000 players. The
  journal (`journalView`) shows each quest and the story's shown variables as trackers; verbs `quest_accept`, `quest_decline`,
  `quest_abandon` write `quest accepted|declined|abandoned`. **The dialogue window** (doc 83/84): on the node a resident's talk opens
  (the resident hub), before the way out and before a mantle's grafts, each offer of this resident the wolf may take (`ask` → a story
  node where the resident says `say` through `inVoice`, the dialect and mantle voice, with accept and decline), each of the wolf's
  ready quests this resident takes back (`handIn` → `thanks`), and each story choice (`words` → `reply`); nodes `@story:<run>:offer|
  thanks|reply:<id>`; every pick checked again when made; events `quest accepted|declined|handed in` and `dialogue choice` with the
  resident as `item`. **The notice board**: an offer with `board` shows under QUESTS on that town's board with TAKE IT ON (`quest
  accepted` with item `board:<town>`).
- **The client** (`Client/src/game/quests.ts`, `ui/hud/archive.ts`): the journal's quests (state, "Ready: return to Linden", objectives
  ticked, struck when failed, "(optional)", "3 / 4", "later" for one that waits on another, "And one more thing to find out" for hidden
  ones, "To be done by day N", "It may be taken again from day N"), trackers, and ABANDON THE QUEST, or ACCEPT and DECLINE for a
  quest offered with no giver; the board's QUESTS (`board.ts`).
- **The runner** (`tools/story_runner.py`): publishes open (no wolves) and everyone audiences; after each tick that changed them it
  sends `story.cohort` (the audience's changes against `cohortSent`) and `story.quests` (the offers and choices when they changed,
  and each wolf's journal whose mark changed), and waits for both (`questReceipts`); reads only this run's quest events. Commands
  `objective {quest, objective, player, do: complete|progress|reveal|fail|reset, amount}`, `quest {quest, player, do: give|take|
  complete|fail}`, `participant {player, do: add|remove}` (flow commands; completing by hand gives the rewards as play does; the dry
  run lists them). `status` gains `quests` (the grid: each copy with its objectives, counts, starts, cap and limit), `audience`,
  `parties`, `rewardsRefused`, `controls.quests`; the list's rows `quests` (taken) and `audience`. The references check the givers and
  reward items. `story_program.migration` keeps a quest by id when its objectives and scope are unchanged, resets its copies' progress
  when they changed (a scope changed: its copies go), drops one the new version has not; the audience is kept; the diff has `quests`.
- **The Editor**: the workbench's **Quests (n)** in the tree and `triggers/QuestEditor.tsx` (every field above; the giver and the board
  pickers; objectives with the event picker, after, optional, hidden, reorder; fail rules; rewards and bonus with a purse picker),
  pickers for quest, objective, purse and offer slots, quests in search, uses and rename; Settings' audience open and everyone; **New… →
  A small quest** (`triggers/SmallQuest.tsx`: who gives it, the steps, the reward, the offer's words, doc 58's 24 templates as starting
  points; it makes the open story, its one phase, the quest and a rehearsal that takes it, does it and hands it in). The console's
  **Quests** tab (`campaigns/QuestGrid.tsx`: holders down, objectives across, each row's menu, Give the quest…, every control through
  the dry run and a reason) and **Wolves & map**'s Add a wolf… and Remove…, with each wolf's party and the audience.
- **Rehearsal**: a case may give an open story an empty `cohort` and assert `expectQuests` (`{quest: {holder: status}}`); a report
  carries `quests` and `cohort`; step-through shows the quest grid.
- **Tests:** `tools/test_story_quests.py` (the checks; rehearsals of each scope, any order and after, optional, hidden, the bonus,
  repeat, cap, who may take it, fail rules and a deadline, abandon, offers accepted and declined, a party left keeping its credit,
  the story's copy needing two wolves, the everyone audience, npc.offer and trackers, participants in and out, the story's cap; a
  move keeping, resetting and dropping quests; in a throwaway database: a small quest taken in a window, done and handed in, the
  orders, a coin reward refused by the game and the story carrying on, each wolf's row; the console giving, revealing, setting
  progress, completing by hand with its dry run, adding and removing a wolf; a party quest with parties from the save and a member
  leaving; a run with quests moved and the game refusing, its rows put back); `test_story_native.py` (the quest samples, both twins);
  `test_story_catalogue.py` (Phase 6's effects in and out of bounds); `Tests/storyline_tests.cpp` (`storyQuestsInTheGame`: the
  orders, journal views and trackers, the verbs and their refusals, rewards from a purse, items, standing, party events) and
  `Tests/dialogue_tests.cpp` (`storyQuestsInTheWindow`: an offer shown to an eligible wolf and not to one who is not, accept,
  decline, hand in and a story choice with their events and the resident's line); the client's `quests.test.ts`; the Editor's
  workbench and campaigns tests and the render smoke (the quest editor, the small quest, the console's Quests tab, the open audience).
- **Seen**, on a throwaway Greyfen database with a real game server (the built client served, the talk library loaded) and the real
  runner ticking: `/home/martinb/.claude/jobs/ee03c49e/tmp/shots-camp-p6/` — Linden's window offering "A word for Maple", his offer
  in his own voice, taken; the journal; the story choice in Maple's window and her reply; the hidden optional objective found; the
  hand-in and Linden's thanks (paid 6p from the town's treasury, a loaf of bread, a deed); the journal done ("may be taken again from
  day 2"); the quest on the notice board, taken; the console's Quests grid, a row's menu, the dry run of completing by hand and its
  reward sent, Wolves & map; the quest editor, a trigger with npc.offer, a board quest, and New… → A small quest from a template.
- **Not in Phase 6, on purpose:** `ifRefused` and "when here" for rewards (Phase 7; a refused reward is noted and skipped);
  quests for the hosted "may act on wolves" checks (none acts on wolves); a quest's goal read from a variable; `{variable}` in a
  quest's words; the per-wolf rows for stories without quests or an open audience (they keep their one JSON); the 500-wolf
  checks of fixed audiences (unchanged); objectives whose events the game does not record yet (Phase 8's); a quest's own
  timer shown as a countdown beyond its deadline line.
- **For Phase 7:** quest rewards are ordinary effects with a `reward` mark in `triggerEffects` (stripped before queueing) and
  `rewardReceipt` (in `RECEIPT_KEYS`): batch them with the rest and give them `ifRefused: "when here"` by default; journal orders
  wait in `questReceipts` (a list, not in `RECEIPT_KEYS`), sent by `sync_journal` at the end of a tick that changed something and
  at the top of the next; per-wolf rows are written by `_dump` on every state write of a quest story (keep every new write through
  it, and merge with `_players_in` before any read that runs the rules); a wolf's journal is resent only when its mark changes.

**Phase 7: Batched effects and a faster runner. About 12 hours.**
- `story.effects` (a list in one order, a result for each) and `ifRefused` (pause, skip, when here).
- The runner woken by a notification instead of sleeping 2 seconds, and reading until it has caught up.
- Scene outcomes no longer save on the game thread: they ask for an early save (`saveSoon`) and wake the
  runner; measure the delay a player sees between a vote and the next beat, bar 5 seconds.
- Old trigger history pruned by a rule (keep 90 days or the last 10,000 per run).
- Measured with a load of events on a test world (at most 250 players, by the standing rule): events a second a
  run can read, and seconds from an event to its effect. Bars: 1,000 a second; 5 seconds.

**Phase 7 as built** (2026-11-22, branch `r3-camp-p7`, on `r3-camp-p6`). Migration `0078`; one new game order (`story.effects`); one
optional field on every world effect (`ifRefused`, with `waitDays`). The rules stay in the outside runner (question 1); this phase makes
it fast. Known problems 1, 2, 3 and 11 are gone, 6 is the author's choice now, and 7 is about two seconds while a story is live.
- **Batched effects** (`tools/story_runner.py`, "sending effects to the game"; `Game::applyStoryEffects`, `Core/RatwGameStorylines.cpp`).
  What a turn of the runner has for the game goes in one order: `story.effects {run, visit, items: [{kind, target, payload, ifRefused}]}`
  (or the order itself when there is one), up to 64 items: 61 effects, then the journal's own orders (`story.cohort`, `story.quests`,
  `story.missions`). The game applies the items in order, each exactly as its own order would be (the community world's guards, the
  economy's journal, the purse), and answers with a JSON array, a result for each (`{ok, text}`); the row is `applied` once what it
  journalled is committed. An item refused whose `ifRefused` is `pause` (or absent) stops the batch there: the rest come back `{ok: false,
  held: true}` and wait with the run. A visit's opening effects go the same way, after its `story.phase` is applied (a phase with twelve
  entry effects opens in two round trips, not thirteen), counted in `state.entryDone` (an older visit counts its receipts as before), the
  ending's `story.finish` alone after them (`state.endSent`). `state.batch` holds the items as made (so a retry sends the same orders) and
  `state.batchReceipt` is one of `RECEIPT_KEYS`: every reader of a run's pending orders (the console's waiting-on and Orders, flow commands'
  wait, `_run_receipts`) sees it. **Journal orders are waited for like every other order** now: `questReceipts` is read only for runs an
  older runner left mid-wait (so a flow command right after another that changed a quest waits for the journal's answer: a few hundred ms).
- **Crash-safe, item by item.** In the game, the batch's receipt (`dmActs[id]`, with `batch: true` and `items` by index) is set as each
  item is applied, so every journal record an item writes carries what the items before it came to; an economy item's own receipt is
  `dmActs["<id>.<n>"]`. A crash part-way through leaves the row queued: taken again, the items with a receipt are answered from it and only
  the rest applied (none twice, none lost; an item that journals nothing is applied again, as a lone order would be). The restart's
  first pass skips batch receipts when it answers rows from `dmActs`, puts a batch the old server let expire back to `queued`, and never lets
  one with a receipt expire. In the runner nothing changed in kind: a tick (events read, history, state, cursor, the orders queued) is one
  transaction, so a runner killed anywhere leaves either all of a tick or none, and a batch in flight is waited for, never sent twice.
- **ifRefused** (`story_program.REFUSALS`, `check_refusal`; doc 3.5): on any effect the game applies (not the story's own effects, not
  `npc.line`): `pause` (the default, today's behaviour), `skip` (a note in the history, the story carries on), `when here` (an effect to one
  wolf, not a scene: held in `state.whenHere` and sent again when that wolf next comes into the world, an `arrival` event, for up to
  `waitDays` real days, 1 to 90, 7 when not said; past it, dropped and noted; at most 500 held). **A quest's reward is `when here` unless it
  says otherwise** (Phase 6's "noted and skipped" becomes "noted and held"; `rewardsRefused` still lists it). Version 1 programs may use it
  too (an optional field). The console's stuck order: **Retry** (the stuck item and those after it, as they were made), **Retry when <wolf>
  is next here** (a new `order` kind, `when-here`: the stuck item held, the rest sent), **Skip it and carry on**; a batch the game answered
  after the pause (from its journal, after a restart) resumes without sending anything again. `status` gains `whenHere`, and each batch in
  `orders`/`actions` its `items` with their own outcomes.
- **A faster runner.** It reads page after page of 1,000 events until it has caught up (not 128 a tick), for up to a second a run before
  the next run and the commands have a turn (`EVENT_PAGE`, `READ_BUDGET`), and stops early only for a full batch, a phase change or an
  ending. Effects gather while it reads and go together after; while a batch is out, the run reads nothing (so a refusal that pauses still
  stops the story before anything later is told the game). `serve()` replaces `sleep(2)`: it `LISTEN`s on `ratw_events` (every write of
  the game's events notifies it, a save's or an early one) and `ratw_story` (the console's commands), looks again at once while a run has
  events left, every 50 ms while one waits for the game, and every 2 s otherwise. The heartbeat is written once a second at most. The
  clock is never behind the newest event read (`state.lastDay`): events now reach the log between saves.
- **The game** (`Core/RatwGame.cpp`, `RatwDbStore.cpp`). While a world story of the runner is live (`storyLive`), the world's events go to
  the log every second between saves, in a small transaction of their own on the store's worker (`DbStore::flushEventsInBackground`,
  `Game::flushStoryEvents`; the game thread only hands them over), never while a save is asked for or a snapshot is being written, so an
  outcome's events always go with the checkpoint that holds its state. **Scene outcomes no longer save on the game thread** (known problem
  1): a beat settled or a resident's scene order done asks for a save within half a second (`saveSoon(StoryOutcomeSeconds)`), written in the
  background (a forked snapshot); a scene's request to the Mind is sent once that save is stored (`dispatchSceneRequests`), so a crash can
  never send it twice. A save asked for while a snapshot is still being written is taken as soon as that one is done (it used to wait for
  the next round save). The DM's orders are looked for every quarter of a second once notified (was a second).
- **A direction on a busy world** (known problem 2): `direct('signal')` (a ticket from the author bridge) puts a `signal` command for the
  direction only (`direction: true`, the exit's words may have spaces) in the inbox and returns `queued`; the runner lands it between two
  world events. Issuing a direction ticket no longer waits for the phase's effects to be acknowledged (a guest line still does).
- **History by a rule** (known problem 11): migration `0078_story_history_rule.sql`, `dm.prune_story_history(world, days, keep)` (security
  definer), called by the runner once an hour (`prune_history`): an entry older than 90 days goes unless it is among its run's newest 10,000.
- **The Editor**: each world effect in the workbench has an **if refused** box (pause the story · note it, carry on · wait for the wolf, up to
  N days; a reward says "wait for the wolf" when left alone); the console's Orders shows a batch's orders each with its outcome, NOT SENT for
  those held behind a refusal, the effects **waiting for their wolf**, and the three ways on.
- **Measured.** *The runner* (`tools/bench_story_runner.py`: a throwaway database, four live stories (winter-ferries, reed-ford, a tally
  with rules on every `economy` event, and a reward story giving a deed on about one in 25), a real stretch of Ridgemere 3's own log
  (20,001 events, read from its database, never written), the game stood in for at each version's polling): Ridgemere 3's busiest save is
  **2,681 events at once** (the day's turn; its busiest hour about 3,300). *Before* (Phase 6's runner, game polling every second): that save
  took **127 s** to read through (21 events a second for the story with effects), its effects reached the game **60 s** after their events
  (median; 117 s at worst); at a steady **1,000 events a second** for 20 s it fell behind for good (19,903 behind when the writing stopped,
  effects up to 57 s late, still behind a minute on). *After*: the busiest save read through in **0.96 s** (2,800 events a second, 11,200 a
  second summed over the four stories), effects applied **0.86 s** after their events; at 1,000 a second it never fell more than a second's
  write behind, caught up 0.35 s after the last, effects applied **0.14 s** after their events (median; 0.40 s p95, 0.42 s at worst); a
  backlog of all 20,001 at once read through in 8.2 s (**2,430 events a second**, 9,700 over the four). Bars met: 1,000 a second, 5 seconds.
  *The game thread* (`/home/martinb/.claude/jobs/ee03c49e/tmp/camp-p7/outcomes.py`: a throwaway copy of DEV's database, Ridgemere 2.0 with
  1,083 residents and 241 cells, the size of Ridgemere 3's 1,086 and 241, read only from DEV; a real game server and the real runner of each
  version; six residents each saying a line again as soon as the story sees the last one said, 90 s): *before*, each line's outcome saved the
  whole world on the game thread, **95-120 ms** a time (670 ms the first), and a resident's next line came **58 s** after the last (median;
  65 s at worst; 14 lines in 90 s); *after*, the early saves cost the game thread **about 9 ms** each (a fork; 19 ms the first), the worst pass
  43 ms was the schedules, and the next line came **1.0 s** after the last (median; 1.5 s p95, 1.75 s at worst; 510 lines). Bar (a vote to the
  next beat): 5 s. With no outcome to save, the same world's events reached the log a second after they happened (`flush_check.py`), and the
  runner's rules saw each within 0.6 s: about 2 s from an event in the game to its effect applied.
- **Tests:** `tools/test_story_speed.py` (a tick's effects in batches of 61 in order and the runner reading on while they gather, 1,200
  effects in 20 round trips; a refusal that pauses holding the rest, retry sending the stuck one and those after it, held for its wolf and
  sent again on arrival; skip; when here and its wait running out; a reward's default; the checks of `ifRefused`; a direction landing while
  world events wait; the history's rule; the loop woken by the world's events in well under its 2 s; and **the real runner process killed
  six times or more** (mid-read, mid-batch, with a batch waiting for the game, the game answering some while it is down) over 3,000 events:
  every one of the 200 effects sent once, no event answered twice, none lost, never paused); `Tests/events_tests.cpp` (`dm_story_batch`
  in the crash cases, through a file and through PostgreSQL: a batch cut short by a crash after its first order, taken again after the
  restart: the first answered from its receipt and paid once, only the second applied, taken a third time nothing paid twice);
  `Tests/storyline_tests.cpp` (a batch in the game: skip goes on, pause holds the rest, no batch inside a batch, an empty one refused).
  Updated for the new behaviour: `test_story_program.py` (effects gather while the runner reads; the entry effects in one batch; a
  direction queued and landed), `test_story_quests.py` (orders read inside batches; a refused reward held), `campaigns.test.ts`. The P1-P6
  suites, `test_story_scene_live`, `test_story_siege_live`, `test_studio`, `test_world_db`, the Editor's tests, type check and render smoke
  (a batch stuck, an effect waiting for its wolf) pass; `test_dungeon_master`'s two errors need `talk_check` built, as before.
- **Not in Phase 7, on purpose:** "change target" for a stuck order (3.5's fourth way); per-world settings for the batch size, the read
  budget or the history's rule; the early event writes for a world with no live story (it pays nothing, as before); facts with those writes
  (Phase 8).
- **For Phase 8:** events now reach `game.events` between saves while a story is live (`DbStore::writeEvents`, on the store's worker), so
  `game.story_facts` must be written in the same transaction as each write of events, the early ones too, not only with a checkpoint, or a
  fact is older than the event beside it. A new event that must agree with state the game keeps (as a scene's outcome does with the order it
  retires) should ask for `saveSoon(StoryOutcomeSeconds)` in the tick it is recorded, which holds the early writes until that save. The
  runner's clock is `max(checkpoint, newest event read)` (`state.lastDay`). Migrations: 0076 is still Phase 8's; 0078 is taken. A rule
  reading facts runs inside the paged read (`EVENT_PAGE` at a time): read the facts for a page at once, not per event.

**Phase 8: Facts and new events from the game. About 14 hours.**
- Migration `0076`: `game.story_facts`, written with each save for live runs from their watch lists.
- The conditions over facts (3.4); rehearsal cases that state facts.
- The new events of 3.3, the watched ones through the watch list; signing in inside a region counts.
- Tests: one game test per new event; a `world_check` run to show the cost with no story live is nil and with
  ten live is within the tick budget.

**Phase 8 as built** (2026-11-22, branch `r3-camp-p8`, on `r3-camp-p7`). Migration `0076`; no new game order (`story.phase` gains
`watch`); 23 new conditions (21 over the world's facts, 2 over quests), 24 new events; version 2 only for the conditions.
- **The facts** (`game.story_facts`, migration `0076_story_facts.sql`, `game.record_story_facts`). **Decided (mine): one document per
  world, not per run**: the union of what the live stories watch (a condition reads only what it names), so ten stories cost one
  document. The game writes it with **every write of its events, in the same transaction** (Phase 7's early writes on the store's
  worker and every checkpoint's: `Game::handEventsToStore` builds it on the game thread as the events are handed over,
  `DbStore::queueFacts`, `writeFacts` after `game.record_events`), as of the newest event written with it (`as_of_event`); a write whose
  facts are the same moves the newest row on instead of adding one; rows older than a day go as new ones come (the newest kept). An
  event's facts are those of the first row at or after it: never older than the event, at most one write newer (a second while a story
  is live). Sections: `wolves` (every wolf in the world, by id: `cell`, `x`, `y`, `indoors`, `town`, `party`, `partySize`, `carries` of
  the watched items by base item, `coins`, `standing` with the watched factions (their Chapter's, effective), `reputation` in the
  watched towns (residents who know them, liking less disliking, worked out at most every 10 s), `downed`, `hurt`, `wanted`, `held`;
  a wolf away is absent), `residents` (the named: `alive`, where, `indoors`, `job` and `mood` as their talk has them, `mantle`, home
  `town`), `world` (`day`, `hour`, `minute`, `weekday`, `season`, `period`, `dayOfSeason`, `year`), `weather` (the named places and,
  when asked, where the wolves stand; indoors the street's), `relations` (`a>b`: the stance). Nothing is written while no live story
  asks (`Game::refreshStoryWatch`, from each live storyline's `watch`, kept with the storyline and restored on load).
- **The watch list** (`story_program.watch_of`, sent by the runner with every `story.phase`, the revise of a move too): the sections and
  the items, factions, towns, residents, places and relations the conditions name, and the watched events the triggers, exits,
  missions and quests (objectives and fail rules) listen for. The 8 examples watch nothing.
- **Conditions** (`story_triggers.FACTS_V2`, `QUEST_V2`; `fact_test`, `quest_test`; `Dispatch2::fact`, `quest` in
  `Core/RatwStoryTriggers.cpp`): `wolf here`, `wolf in region`, `wolf in place`, `wolf in town`, `wolf indoors`, `count in region`,
  `carries`, `coins`, `standing`, `reputation`, `wolf is` (downed, hurt, wanted, held), `party size`, `in party with`, `resident is`,
  `resident in place`, `resident in region`, `resident has` (job, mood, mantle, town), `hour between`, `calendar is` (weekday, season,
  period), `weather is`, `relation is`, and `quest is`, `objective is`. `who` is `$actor`, `$each` or a wolf. The facts ride on the
  event as `facts` (the clock and its timers, signals and the story's quest facts carry those of the event that caused them); a wait
  keeps none and resumes with the facts of its moment. A missing fact is unknown (never fires, not under `not`); a missing item is 0,
  a missing town reputation 0, a missing party size 1. The native twin is sent only the wolves its conditions can read (the acting
  wolf, those named; all for a count or `$each`), and for quest conditions the copies, offers and each quest's scope and hidden
  objectives. **Story variables** were Phase 5's `compare`; nothing new was needed.
- **The runner**: reads the facts once per page of events (`facts_page`, `facts_at`), attaches each event's to it and its clock;
  the trailing clock and the console's commands (fire, what if) use the newest (`latest_facts`). Why not judges each logged event with
  its own facts; Explain returns the `watch` and the facts of its wolves; judged world conditions carry `fact` (what the world said)
  and `needs: facts` when unknown.
- **The new events** (catalogue `"watch": true`, each with typed fields and a sentence; written only while a live story listens):
  `door opened` (also walking through a closed door), `door closed`, `door locked` (tried) in `World::interact` and the walk-in;
  `building entered`, `building left` (`Game::followTransition`, beside `enter place`); `region occupied`, `region emptied` (the count
  of the story's wolves in its region, `Game::storyRegionCrossed`, recounted at each phase; **signing in inside a region now writes
  `region entered`** (known limit of 1.7), signing out inside one only lowers the count, it writes no `region left`); `weather changed`
  (the watched places, every 2 s; a first look writes nothing); `hour`, `day begins` (detail the weekday, item the season, quantity
  the day of the season), `dawn`, `night falls`, `season begins` (`Game::storyWatchTick`); `item picked up`, `item dropped` (a fight's
  ground: `World::takeItem`, `dropItem`); `trade done` (bought or sold, coins; the trade command); `dialogue opened`, `dialogue node`
  (detail the node, item the tree; a story's own nodes `@story:<run>:…`), `dialogue closed`; `crime seen` (actor the witness, target the
  offender when they could tell, item the victim, detail the kind); `wolf downed` (the wolf as actor, beside `downed`), `wolf revived`
  (target who tended them); `party formed`, `party ended`. Groups added: Doors and buildings; Time, weather and calendar; Dialogue
  windows. `item given` is `gift`; `signed in`/`signed out` stay `arrival`/`departure`; `resident died` stays `death`; `gaoled` is
  `arrest`. None asks for an early save: what they tell happened (as Phase 7 says of trades).
- **Rehearsal**: a case's `facts` (the world at the start) and a fact's `facts` (changed from then on, section by section; `null` takes
  a wolf or a resident away), checked (`check_world_facts`).
- **The workbench**: the catalogue's 23 conditions in sentences ("the acting wolf carries at least 2 rope", "the weather where the acting
  wolf is is snow"), Add a condition grouped (the story; the world, as of the event; quests), defaults from the catalogue
  (`newWorldCondition`: the story's first region and quest; what only the world names left red to pick), a place slot that can be "where
  the acting wolf is"; the 24 events in the event picker's groups. **The console**: Waiting for shows the world as the game last told the
  story (the clock, each wolf of the story, residents, weather, relations; what it listens for); every judged world condition says what
  the world said ("the world says 0 for Fern"); Why not says "the world as it was at this event".
- **Tests:** `tools/test_story_facts.py` (the checks of all 23; the night watch rehearsed; facts merged; a wait resuming with its moment's
  facts; the watch list; **parity: each condition over 30 made-up worlds under 12 seeds**, facts missing, partial, wolves away, both
  twins the same event by event; in a throwaway database: `record_story_facts`, the runner judging each event with the write it came in
  and never a newer one, the watch list in the phase order, Explain, Why not and What if with facts); `tools/test_story_facts_live.py`
  (a real game server and the real runner on Greyfen: two wolves walk into the square by their own clients' paths, the game writes
  `region occupied` once and the facts in the same early write between saves, every event since covered, Ash in the square in the facts
  of her event, the trigger with world conditions fires once and its deed is applied); `tools/test_story_native.py` (the night watch in
  both twins, and again with one wolf out); `Tests/story_facts_tests.cpp` (through the game: nothing watched writes nothing; each of
  doors opened, closed, locked, building entered and left, region occupied and emptied, signing in and out inside, weather changed, hour,
  dawn, nightfall, a new day, a new season, trade done, dialogue opened, node and closed, party formed and ended; the facts document;
  the watch saved; nothing after the story ends); `battle_tests` (`storyWatchedFightEvents`: wolf downed, item dropped, item picked up,
  wolf revived, and none unwatched); `crime_tests` (`crimeSeenForAStory`); the Editor's `workbench.test.ts` (every world condition the
  workbench adds passes the checks; the night watch in words), `campaigns.test.ts` (fact words and lines), the render smoke (the night
  watch's workbench; Campaigns with world facts). Updated counts: `test_story_catalogue`, `test_story_drafts`. ctest 123 (with the pg
  tests: all pass); the P1-P7 suites pass.
- **Seen:** on a throwaway Greyfen database with a real game server, two Node clients and the real runner, screenshots in
  `/home/martinb/.claude/jobs/ee03c49e/tmp/shots-camp-p8/`: Waiting for with the world's facts; Why not on the trigger that fired;
  Why not on `roped-at-night` (each event: no rope, the hour, the weather, as the world said then); the timeline; the workbench's
  trigger on `region occupied` with world conditions, the one with carries / hour / weather, Add a condition's world group, and the
  event picker's door events.
- **Not in Phase 8, on purpose:** the `world_check` run of the doc's test list (no simulation without asking; the cost with no story
  live is nothing by construction: no document, no watched event); `near resident`, `resident arrived`/`left`, `purse crossed`, `price
  crossed`, `standing crossed`, `relation changed`, `mantle bound`, `skill practised`, `injury healed`, `month begins` (3.3's rest);
  `world variable` as its own condition (Phase 5's `compare` reads world variables); `region:<id>` in For each; a game-page screenshot
  (the console's are of the real server's events).
- **For Phase 9:** a new order that changes what the facts say (`item.give`, `weather.set`, `door.lock`) is seen by a condition from the
  next write of events (the order's own effects are not events unless it records one); `door.lock`/`unlock` should record `door
  locked`-style events only if watched (`World::storyListens`). New watched events go in the catalogue with `"watch": true` and are
  gated by `world_.storyListens(kind)`; new fact sections by `Game::storyFactsJson` and `story_program.watch_of` together, with a
  rehearsal type in `WOLF_FACTS`/`RESIDENT_FACTS`/`WORLD_FACTS` and both twins.

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
