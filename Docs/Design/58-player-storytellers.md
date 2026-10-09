# 58. Player storytellers and personal stories

Drafted 2026-10-06 as an actionable plan for doc 48 (Part 9, §8.2, §8.3 and §6.8). Open questions answered 2026-10-08
(see "Open questions"); all six phases built the same day (see "Built"); committed, with migration 0049 applied (2026-10-08). Read doc 48 (Part 9,
§6.8, Part 8, Decisions) and docs 17 (the legacy Storykeeper and the DM's separation), 21 and 34 (the DM app and its
refresh: the story planner and quests, Parts 3–7), 26 (contracts, Phase 5, and promises), 08 (scenes), 32 (Chapters,
their treasury and the boards in rented places), 28 (AI cost) and 51 (stars and scene openness) first.

Marks: **(agreed)** is the user's decision, with the doc 48 section it comes from; *(placeholder)* is one constant to
change; **(new)** is a choice this plan makes that the user hasn't seen.

## The ask

From doc 48:

> "Let's design a level of access for the Dungeon Master tool that allows specific players to conduct and orchestrate
> stories for their Chapter/Party/Friends. This should be something they can apply for after, say, social level 5 or
> something."

> "We should build stories that individual players can follow and have trackable markers, so that players can see
> themselves progress along it."

> "For BIG, GLOBAL stories, each milestone should have a screen like this that shows who did what for the story, and
> let players assign a maximum of 3 stars to whoever they want that contributed?"

> "I need to find ways that put wolves working toward the same goal, but from two angles, together."

Doc 48 Part 9 settles that the access is **not the DM app** but an in-game tool with narrow, audited powers.

## Where we stand (read from the code 2026-10-06)

**No story engine, quests or planner exists.** Doc 34's Phases 3–5 aren't started: there are no `dm.story_defs`,
`game.story_runs` or `game.quests` tables (migrations stop at 0033), the DM app's Story Creator tab is `ready: false`
(`Editor/src/dm/DmApp.tsx`), and the NPC voice's prompt still forbids quests.

**What a storyline can build on:**

- **Contracts** (`Contract` in `Core/RatwRoads.h`: bounty, escort, supply, courier, procure). `World::takeContract`
  and `World::settleContract` record `contract taken` and `contract done` events. No chains, no objectives.
- **Promises** (doc 26): `World::promisesBetween`, kept or broken by deeds.
- **World events** (`World::recordEvent`, `Core/RatwWorld.cpp`): `fight ends`, `hunted`, `forage`, `tended`, `contract
  done` and more. `recordEvent` already feeds bonds, rumours, news and contracts; the game itself takes events only at
  save time (`World::takeEvents`, for `game.events`).
- **Talk with residents:** `logEvent("conversation", …)` in `Core/RatwGame.cpp`, whose actor is empty when the
  resident can't identify the player. A step must hook the talk path itself.
- **Doc 32's Stories** (`SocialStory`): named chains of qualified scenes, with no steps, markers or objectives.

**What the storyteller's tool can build on:**

- **The DM in the game** (doc 34 §1.1b, built): `Entity::dungeonMaster` and the Dev Console (`Client/src/ui/hud/
  devConsole.ts`, `Core/RatwGameDev.cpp`), granted by `character.dm` (admin). It is a full-power console. Its
  **pattern** is useful (a server-kept command list, each command checked on the server); its **permission** must not
  be reused, since doc 17 says ordinary character credentials never grant DM access.
- **Temporary visitors** (doc 34 §1.1): `World::addVisitor`, `sendVisitorAway`, DM action `visitor.add`: transient
  NPCs, no purse, never saved, looked at but not talked to.
- **Boards:** a Chapter's notice board only inside a place it rents (`estate::Lease::notices`, verb `notice` in
  `Core/RatwGameEstates.cpp`: 200 letters, 20 kept); faction mission boards (`Game::missionBoard`). Town boards are
  doc 54's.
- **Money:** a Chapter's treasury is a real purse (`chapter:<id>`; Officers and the Head draw, `RatwGameChapters.cpp`);
  transfers go through `Society::shift`, valuables through `Game::record()` and the journal (doc 31).
- **Social level** is per character (`SocialLedger::level`); doc 49 moves it to the account.

**The old Storykeeper** (doc 17: `DM/app.mjs`, `tools/dm_service.py`, `Core/RatwDirector`) keeps DM-side campaigns and
beats in SQLite; it is legacy, to be retired by doc 34.

**Name clashes:** `Core/RatwJournal.*` and `game.journal` are doc 31's write-ahead journal of valuables; `SocialStory`
is doc 32's Stories; `dm.stories` holds AI life stories; `Core/RatwScenes.*` written ambient scenes. So the screen
says "journal", and the code says **storylines** and **tales**.

**Missing:** everything here: personal stories, the journal, markers, storyteller standing and tool, narration, dice,
credits screens, multi-wolf objectives.

## Scope

This plan builds storylines (short chains of steps with markers, in a journal; one model for personal stories and
storytellers' tales); personal storylines from ties, troubles, contract chains and the DM, with the first step
ticked; storyteller standing (apply, approve, revoke, audit); the storyteller's tool (tales, narration, story
characters, open dice, visitors, prizes, board calls); credits screens for big stories' milestones, with up to 3 stars
each; steps with two angles.

### Build a small engine now, or wait for doc 34? Build it now (new)

Doc 58 builds a **minimal step tracker and the journal itself**, shaped so doc 34 later writes into them, not beside
them. Why:

1. **Doc 34 hasn't started** its story engine, Storykeeper or quests. It is a large DM-side system; waiting on it would
   hold this plan back for no gain.
2. **This plan needs far less:** linear steps, a fixed list of triggers and a player-facing journal. Doc 34's
   branching graph, expression language, cast queries, DEV rehearsal and AI drafting are for the DM's world stories.
3. **Storytellers couldn't use doc 34's planner anyway:** it is a DM tool with the whole action list behind it, and a
   storyteller's tool must be narrow by design (Part 9).
4. **Personal storylines are made by the server** from templates when a tie, a trouble or a contract calls for one,
   not authored in the DM app.

**Avoiding two engines:** the journal is the one place players see storylines and quests (answering doc 34's open
question 3: the quest log lives in the character dialog); doc 34's quests and runs will write storylines (`source:
storykeeper`); this plan's objective kinds use doc 34 Part 3's words (go to a place, talk to someone…) so its engine
can absorb them; and a doc 34 step marked as a milestone fires this plan's `milestone.credit`. Until then the DM gives
storylines and credits milestones by hand.

**Left to other plans:** doc 51 (the star book for tale and milestone stars; scene openness, which narration follows;
end-screen moments); doc 50 (friends, circles, block, reports, the Storyteller status); doc 49 (account social level;
what a finished storyline's award becomes); doc 52 (ties: this plan supplies the templates); doc 57 (residents'
troubles and their resolution: this plan supplies the wrapper); doc 54 (town boards); doc 56 (deeds, the chronicle);
doc 34 (world stories, the Storykeeper planner, quests from contracts).

## Design

### 1. Storylines: one model (new)

A **storyline** has an `id`; a `kind` (`personal`, `tale` for a storyteller's, `world` for a DM's); a `source` (tie,
trouble, contract, DM, storyteller or Storykeeper, with a reference); an `author` (account and character) for tales; a
`title` (60 letters) and, for tales, a `premise` (500); a `chapter` when a tale is a Chapter's; `steps`;
`participants` (character → joined, left); a `cast` (§6); a `state` (draft, running, paused, done, failed, abandoned);
and timestamps.

A **step** has a `title` (80), a `text` (300), an optional **marker**, and 1–3 **objectives**, all of which must be
done; with `distinct`, different wolves must do them (§8). Personal storylines have 3–7 steps (agreed, doc 48 §8.2);
tales 1–10 *(placeholder)*. An **objective** has a kind, its arguments, a journal line, a `suits` hint, and who did it
and when:

| Kind | Done when | Hooked at |
|---|---|---|
| `told` | The storyteller (tales) or the DM (`storyline.tick`) ticks it | the tool, or a DM action |
| `place` | A participant enters a place, and within an optional radius of a tile | the player's change of cell; the radius checked every 2 s only for players with such an objective where they stand |
| `talk` | A participant speaks with a named resident | the talk path in `Core/RatwGame.cpp`, with the player's ID even when unidentified |
| `scene` | A qualified scene settles with N of the storyline's participants paid in it, optionally at a place | `Game::afterSocial` |
| `contract` | A participant's contract of a kind or poster is done | `contract done`, through `World::onEvent` |
| `hunt` | A participant kills a named species at a place | `hunted` |
| `fight` | A participant's side wins a fight at a place | `fight ends`, with the battle's result |
| `gift` | A participant uses a Gift of a family at a place, outside a fight | a new `gift used` event |
| `deliver` | A participant gives a named item to someone | doc 55's gifts, when built |

`World::onEvent` is a callback the game sets, called from `World::recordEvent` beside its existing hooks, so events
reach storylines at once rather than at the next save.

- **First step ticked** (agreed, doc 48 §8.2: endowed progress): a personal storyline begins with step 1 done
  ("Arrived in Upper Accord", "Heard Hale's trouble").
- **Markers** are a place, a tile, a radius and a label ("the mill at Ser Ferro"). **Never a live position (new):** for
  a resident, their work or home place (public knowledge, as the speech router already tells, doc 28); never a player.
  The **tracked** storyline's next marker shows on the minimap and World Map as a small diamond, with a line under the
  minimap: "◆ Find the miller · the mill at Ser Ferro".
- **Limits** *(placeholders)*: 3 personal storylines at once; 3 tales a wolf takes part in; 2 running tales a
  storyteller; 12 participants a tale.
- **Never punish absence** (doc 48, Principle 7): storylines don't lapse with time. A personal one ends when finished,
  abandoned, or ended by its source ("Someone else paid Hale's debt"); a tale idle for 30 real days pauses and its
  storyteller is told *(placeholder)*.
- **Code:** a pure `Core/RatwStorylines.h`, `.cpp` (`Storylines`: begin from a template and cast, triggers, ticks,
  `distinct`, limits, save and load). Its trigger index maps (kind, key) to waiting objectives, so nothing scans
  storylines in the tick. Game glue in `Core/RatwGameStorylines.cpp`.

### 2. The journal (agreed, doc 48 §8.2)

A **JOURNAL** page joins Character, Inventory and Settings (`dialogs.ts`, a new `journal()`):

- **Under way:** each storyline and tale with its source ("a tie", "Hale's trouble", "a story by the grey wolf with a
  torn ear"); steps as ✓ done, ● current (text, objectives, marker, who is on what) and ○ later; TRACK; ABANDON for
  personal ones, LEAVE for tales.
- **Done:** ended storylines, a line each, and credits screens still open to stars (§7).
- **Each step done is a small win:** a toast ("Step done: you found the miller (The debt, 3 of 5)"), a `step` moment on
  the next scene's end card (doc 51 §8), and a chronicle line (doc 56 reads the storyline log).
- **Finishing** a personal storyline the server made (tie, trouble, chain, DM) pays a typed award `storyline` once, 10
  *(placeholder)*, which doc 49 maps into its practice-based world. **Tales pay nothing of their own**, since a
  storyteller can't grant XP (agreed, Part 9); their scenes pay as scenes always do.

### 3. Personal storylines and their sources (agreed, doc 48 §8.2)

Templates live in `Data/Storylines/templates.json` **(new)**: title, steps, objectives with cast roles (`tie`,
`resident`, `poster`, `place`), marker rules, an optional one-line `brief` for the Mind (§9) and `suits` hints. They
are begun by `Storylines::begin(template, owner, cast, source)`.

- **Ties (doc 52):** a template for each of doc 48 §7.4's eight starters. "You carry a letter for them": arrived
  (ticked); find the tie; hand over the letter (`talk` for a resident tie, `told` by the mentor for a player tie);
  share a scene. A mentor's tie puts the storyline in both journals, and its steps may need two angles (§8). Doc 52
  calls `begin` when the tie is made.
- **Residents' troubles (doc 57):** a template per trouble kind (debt, broken tools, sickness, a feud, no work, a child
  without an apprenticeship), begun when the resident tells the player. Doc 57 owns what solves a trouble; its
  "solved" and "solved by someone else" end the storyline.
- **Contract chains (new):** when a player finishes a contract for a resident who likes them (affinity 20 or more
  *(placeholder)*), the resident may offer a follow-up: a two- or three-step storyline whose later steps are contracts
  the resident posts **from their own purse** through the existing contract posting, only if they can pay. No money
  is made. It lives in `RatwGameStorylines.cpp` and calls the world's contract functions, changing no economy file.
- **The DM:** `storyline.give {character, template, cast}`, until doc 34's Storykeeper is a source.

### 4. Becoming a storyteller (agreed, doc 48 Part 9)

- **Who:** an account at social level 5 *(agreed placeholder)*: doc 49's account level, or until then the highest of
  the account's characters, with no upheld report in 30 days (doc 50) *(placeholder; doc 48 proposes the same for
  mentors)*.
- **Applying:** APPLY TO TELL STORIES on the character sheet, with a short note on what they'd like to run (500
  letters, for the DM). One application at a time.
- **Deciding:** a DM or admin approves or refuses with a reason (agreed), and can revoke at any time (agreed); the player
  is told in game. The standing is the **account's**, so any of its characters can use the tool. Revoking pauses the
  storyteller's running tales, tells the participants, and leaves the DM to stop them.
- **The mark:** approved storytellers carry a small ✦ on their card (doc 50 draws it) and may set the *Storyteller*
  status (agreed, Part 9). Doc 48 §3.5 lists *Storyteller* as a status anyone sets; this plan recommends doc 50 reserve
  it for approved storytellers, so it means something **(new)**.

### 5. The storyteller's tool (agreed: not the DM app, doc 48 Part 9)

- **Separate from DM access:** never `Entity::dungeonMaster` or the DM's logins (doc 17). Its permission,
  `Storytellers::approved(account)`, is checked on the server for every command.
- **On screen:** a STORYTELLER button in the top bar, for approved storytellers only. Its page has **My tales**
  (drafts, running, ended), **the editor** (title, premise, steps, objectives, markers picked on the World Map, the
  cast) and **the run panel** (participants and progress, tick, narrate, a story character's line, dice, visitors,
  prizes, calls, end).
- **Slash commands** in the composer mid-scene (`/narrate`, `/npc <name>: <line>`, `/roll`, `/tick`), never said
  aloud. The server keeps the list (`storytellerCommands`), after the Dev Console's pattern.
- **What it sees:** its own tales and what its character perceives. Participants show by the labels the storyteller's
  character knows (doc 32 §1.5), as "here", "away" or "offline", never where. It never carries residents' bonds,
  memories or rumours, hidden wolves, or anything from the DM's views (doc 17: "omniscient exports must never enter
  observer-filtered player snapshots").

### 6. Tales: writing and running them (agreed, doc 48 Part 9)

- **Whose stories:** the storyteller's own Chapter, party, circle (doc 50) or friends (doc 50), and **players opt in**
  (agreed): "The grey wolf invites you into their story 'The Drowned Bell' (3 steps) · JOIN · NOT NOW". Participants
  may leave at any time. A blocked wolf (doc 50), either way, can't be invited.
- **Calls on boards** (agreed, Part 9): on the Chapter's board (the rented place's notices, now) or the town board's
  work side (doc 54). **(new)** A call reaches strangers, which Part 9's scope doesn't cover, so a stranger may **ask
  to join**; the storyteller admits them, for that tale only. Only the storyteller's admit and the wolf's own opt-in
  widen the scope.
- **Sessions** are ordinary scenes (doc 08), and their openness (doc 51) decides who besides participants sees the
  story's lines. A tale's scenes start as **Knock** *(placeholder)*, so onlookers can ask in. Narration and story
  characters' lines reach participants anywhere in the storyteller's place, wolves in speaking range when the scene is
  Open or Knock, and nobody else when it is Private.
- **Narration** (agreed): `/narrate <text>`, up to 1,000 letters, one every 3 s *(placeholders)*. In the log it is a
  block of its own ("✦ STORY · The Drowned Bell", then the text), with the storyteller shown by the label each reader
  knows. Narration and story characters' lines count as the storyteller's roleplay in the scene under doc 08's rules
  and caps **(new)**: running a session pays like taking part in one, and no more.
- **Story characters** (agreed: "NPC lines clearly marked as the storyteller's"): a tale's cast of up to 6
  *(placeholder)*, each a name (40 letters) and a line of looks. `/npc the ferryman: "Two pennies, or swim."` shows as
  "✦ The ferryman (in the grey wolf's story) says: …". **Never a real resident (new):** residents keep their own voice
  and memory (doc 26), and a cast name matching any resident's or player's registered name is refused.
- **Open dice** (agreed): `/roll 2d6+1 for the crossing`; 1–10 dice of 2, 4, 6, 8, 10, 12, 20 or 100 sides, ±20
  *(placeholders)*, rolled by the server, seeded from the event as its fights are, shown to the same readers as
  narration ("✦ The grey wolf rolls 2d6 + 1 for the crossing: 3 + 5 + 1 = 9"). Participants may roll in the tale's
  scene too. Every roll is logged.
- **Visitors** (agreed: from a DM-approved list, no economy effect): from `live.story_visitors`, brought on stage within
  6 tiles of the storyteller for 10–60 minutes, at most 2 at once *(placeholders)*, through `World::addVisitor`;
  outdoors or in a public interior, never a resident's home, someone else's rented place or a seat of power (doc 54's
  list) **(new)**. Flagged `storyVisitor` (`World::isStoryVisitor`): fights, theft, trade and recruiting refuse it
  ("They're part of a story"); the Mind never answers it (the storyteller voices it with `/npc`); Look says "(part of a
  story)". No purse. It leaves when its time is up, the session ends, the storyteller sends it away, or the DM does.
- **Prizes** (agreed: their own or their Chapter's purse only): coins from the storyteller's purse, or from the
  Chapter's treasury when the tale is their Chapter's and they are an Officer or the Head (the rule drawing already
  follows), or an item from their own belongings; to participants only. They move through `Society::shift` or a
  trade's inventory move, with `Game::record()` and the journal (doc 31), logged as `story prize` events. Money is
  conserved (doc 15).
- **Ticking:** automatic objectives tick themselves; the storyteller ticks `told` ones, and may tick an automatic one by
  hand ("you judged it done"), which the log says.
- **Ending:** `end {tale, done | failed | abandoned}`. Every participant who did an objective or was paid in one of its
  scenes gets an end card: who did what (from the storyline log), and one star for the storyteller (doc 51's `tale`
  kind, the Storyteller tag already picked and changeable), one a participant a tale (agreed, Part 9). Fellow
  participants are thanked through ordinary scene stars.

**What a storyteller can't do** (agreed, Part 9), and how the server makes sure:

| Can't | Because |
|---|---|
| Grant XP, Gifts or standing (bonds, renown, faction standing) | No command touches `SocialLedger` pay, `Bonds`, `Chapters::addRenown`, factions or Gifts |
| Make items or coin from nothing | Prizes only move what exists, through `Society::shift` and inventory moves |
| Move or kill residents, or speak as one | No command targets a resident; visitors are separate; cast names can't match registered names |
| Edit terrain | Nothing in the tool touches cells or tiles |
| Act outside their participants | Invites need scope or an admit; prizes go only to participants; narration follows openness |
| See what their character can't | Its views carry only their tales and their character's labels |

### 7. Credits screens for big stories (agreed, doc 48 §8.3)

- **A milestone** of a world story is credited by the DM until doc 34's engine exists: `milestone.credit` with the
  story and milestone names, a weight (great or legendary), and the participants as a list or a rule (a storyline's
  participants; everyone paid in scenes at a place in a window; everyone who fought at a place in a window).
- **Who did what, from the ledger only** (agreed). The DM host (`tools/dungeon_master.py`) builds each participant's
  lines from Postgres: `game.storyline_log` (steps and objectives), `game.relationships` (scene and fight receipts)
  with doc 51's fight tallies, `game.events` (contracts done, `tended`, `hunted`), and deeds (doc 56) once built. Each
  line is typed IDs ("fought at the gate: 14 landed, 2 raised"; "carried the warning to the keep"; "shared 3 scenes
  with the cast"). The server checks every participant and renders the lines for each viewer with the names they know.
- **The screen:** `{"type": "credits", …}`, shown when the player isn't in a fight, and kept under Done in the journal
  for 3 days *(placeholder)* to star later. **Up to 3 stars each** (agreed) to any contributors on it, with tags: doc
  51's `milestone` kind, under its counting rules ("the usual pair decay and daily limits apply", doc 48 §8.3).
- **Deeds:** a great or legendary deed for the main contributors (agreed), named by the DM or the top 3 by credit lines
  *(placeholder)*, through doc 56; until doc 56, logged as `milestone` in `game.storyline_log` for it to read.
- Later, a doc 34 step marked as a milestone fires `milestone.credit` itself. Tales use the same screen at their end
  (§6), with the storyteller's star in place of the 3.

### 8. Steps with two angles (proposed in doc 48 §6.8; planned here)

- A step's objectives run **in parallel**; with `distinct`, no wolf may do two of them (the second by the same wolf
  doesn't count). The step finishes only when all are done (doc 48 §6.8).
- Each objective's `suits` hint shows in the journal: a talker (`talk`), a nose (`hunt`, or `place` with tracking), a
  fighter (`fight`), a Gift family (`gift:water`), or hands (`deliver`).
- A participant may **TAKE** an objective, so the others see "Wren is on it": the two angles are visible before anyone
  sets off.
- **Where they appear:** tie storylines for a newcomer and a mentor ("One of you asks the innkeeper; the other looks
  over the stables"); tales, by the storyteller's choice; storylines the DM gives; later, doc 34's quests, whose Part 3
  objectives should gain `distinct` and `suits` (this plan is the hand-off).

### 9. The Mind

The rules decide; the Mind only speaks (doc 32, Principle 2).

- **A step's brief:** when a participant talks with a resident named in their current step's `talk` objective and the
  template gives the step a `brief`, `Game::dialogueContext` adds one line ("This wolf has come about the letter from
  your sister"). It ends with the step.
- **Briefs come from templates** (doc 52's ties, doc 57's troubles) or the DM, **never from storytellers**, who can't
  touch residents. A tale's `talk` objective needs only the conversation; the resident talks as they always would.
- **Visitors and story characters are never voiced by the Mind.**
- **No model calls are added:** storylines come from authored templates, tales from their storytellers, credits from
  the ledger. Doc 28's costs don't change.

### 10. Data and wire

**Authored data:** `Data/Storylines/templates.json` (new: personal storyline templates) and
`Data/Storylines/rules.json` (new: every placeholder: step counts, limits, lengths, rates, visitor rules, retention,
the level to apply).

**Postgres** (one migration, the next free number at the time):

- **`game.storylines`**, a checkpoint section (`storylines`, keyed by ID), with generated `kind`, `state` and `author`
  columns for the DM's filters. Not valuables: saved with snapshots.
- **`game.storyline_log`**, append-only like `game.events`: `id`, `world_id`, `recorded_at`, `at`, `storyline`,
  `step`, `objective`, `kind` (begun, joined, left, objective, step, ended, prize, milestone, by hand), `actor`,
  `target`, `detail` (200, no prose). Written by `game.record_storyline_log` in the checkpoint's transaction
  (`Core/RatwDbStore.cpp`). Credits and the chronicle read it.
- **`game.storytellers`**, a checkpoint section keyed by account: state (applied, approved, refused, revoked), the
  application note, who decided, when and why, tales run.
- **`game.storyteller_log`**, append-only: `id`, `world_id`, `at`, `account`, `character`, `storyline`, `kind`
  (narrate, npc, roll, visitor, prize, call, invite, admit, tick, end), `target`, `text` (narration and story
  characters' lines only, 1,000 letters), `detail`. `game.trim_storyteller_log(world, days)` clears `text` older than
  30 days *(placeholder)*, at start-up and daily; the game role may only insert and run the trim. **(new)** This is
  narrative storage kept as evidence for the DM, an exception like doc 48 Part 11's reports, not a ledger: ledgers
  still hold no prose (doc 08).
- **`live.story_visitors`**, written by the DM like `live.npc_areas`: `id`, `name`, `description`, `appearance`,
  `enabled`, `approved_by`, `approved_at`; loaded at start and on `visitors.sync`.

**Wire:**

- `{"type": "storyline", "verb": "track" | "abandon" | "take", …}` for personal storylines.
- `{"type": "storyteller", "verb": …}`: for storytellers `apply`, `draft`, `save`, `start`, `invite`, `admit`, `tick`,
  `narrate`, `npc`, `roll`, `visitor`, `dismiss`, `prize`, `call`, `end`; for participants (no standing needed)
  `accept`, `decline`, `ask`, `leave`, `roll`.
- Snapshot: `self.journal` and `self.tracked` (rebuilt only on change); `self.storyteller` for applicants and
  storytellers (standing, tales with progress, the command list).
- Events: `{"type": "story", "kind": "narration" | "npc" | "roll" | "invite" | "ask" | "ended", …}` and
  `{"type": "credits", …}`.
- DM actions (`tools/dungeon_master.py` `ACTIONS`, `Game::applyDmActions`; all role dm): `storyteller.decide`,
  `storyteller.revoke`, `tale.pause`, `tale.resume`, `tale.stop`, `storyline.give`, `storyline.tick`,
  `milestone.credit`, `visitors.sync`.

### 11. The Dungeon Master's view (agreed: the DM sees everything, doc 48 Part 9)

- **A Storytellers panel** in the Players tab (`Editor/src/dm/DmApp.tsx`): applications with their notes (approve or
  refuse, with a reason), the approved storytellers (revoke), each storyteller's tales.
- **A tale's page:** steps and who did what, participants, narration and story characters' lines (within 30 days),
  dice, visitors, prizes; pause, resume, stop. It reads `game.storylines`, `game.storyline_log` and
  `game.storyteller_log`.
- **Forms:** *Story visitors* (the approved list), *Give a storyline* (template, character, cast), *Credit a
  milestone*.
- Doc 34's Storykeeper will later take the Story Creator tab and its LIVE Stories layer; tales should appear in both.
  Reports (doc 50) about a storyteller's lines carry the logged lines as evidence.

## Phases

Each phase passes `world_check --players 20` within noise. None needs a 1,000-player run.

### Phase 1: The journal and storylines

- **Goal:** a player carries a short personal story in a journal; its steps tick themselves, and its marker shows on
  the map.
- **Changes:**
  - **Server:** `Core/RatwStorylines.h`, `.cpp` (steps; objectives `told`, `place`, `talk`, `scene`, `contract`;
    parallel objectives and `distinct`; the trigger index; the first step ticked; limits; save and load);
    `Core/RatwGameStorylines.cpp` (hooks at the change of cell, the talk path, `afterSocial` and `World::onEvent`; the
    journal view; `track`, `abandon`, `take`); `World::onEvent`.
  - **Data:** `templates.json` (the eight tie starters, two DM samples), `rules.json`; the migration (the `storylines`
    section, `game.storyline_log`, `record_storyline_log`).
  - **DM:** `storyline.give`, `storyline.tick`, the *Give a storyline* form.
  - **Client:** the JOURNAL page (`dialogs.ts`); the tracked marker on `minimap.ts` and the World Map, and its line;
    step toasts; doc 51's `step` moment.
- **Tests:** `Tests/storyline_tests.cpp` (new, pure: begin with step 1 ticked; each trigger; parallel and `distinct`;
  abandon; limits; save and load); `Tests/storyline_game_tests.cpp` (new: a DM gives a storyline; walking into the
  place ticks it; talking to the resident ticks it, even unrecognised; a scene ticks it; a restart keeps it);
  `checkpoint_tests`; `tools/test_game_tables.py`; `tools/test_dungeon_master.py`; `tools/client/journal.mjs` (new;
  screenshots in `artifacts/screenshots/journal/`).
- **Done when:** a DM gives Ada "The debt" from the DM app; her journal shows step 1 ticked and step 2's diamond on the
  map; walking there ticks it with a toast, and it shows on her next scene's end card; a restart keeps it all.
- **Cost:** trigger-index lookups at events; the radius check every 2 s covers only players with such an objective in
  their place; the journal view rebuilt on change. Nothing scans storylines in the tick.

### Phase 2: Where personal storylines come from

- **Goal:** storylines arise from the world (ties, residents' troubles, contract chains), and finishing one counts.
- **Changes:** `Storylines::begin` called by doc 52 for ties (in both journals for a mentor's tie) and doc 57 for
  troubles, whose "solved" and "solved by someone else" end them; contract chains in `RatwGameStorylines.cpp` through
  the existing contract posting from the poster's purse; the `storyline` award (among `SocialLedger::award`'s kinds,
  for doc 49 to remap); steps' `brief` in `Game::dialogueContext`; objective kinds `hunt`, `fight`, `gift` and the
  `gift used` event.
- **Tests:** `storyline_tests` (a tie storyline shared with the mentor; a trouble solved by someone else ends it; a
  chain offered only with regard and a poster who can pay; the award paid once, never for a tale); `voice_tests` (the
  brief reaches only the named resident, only during that step); `storyline_game_tests` (a chain's second contract
  paid from the poster's purse, the purses balancing).
- **Done when:** with docs 52 and 57 built, a new character's tie appears, ticked, in both journals, and a resident's
  trouble starts a storyline; finishing a courier job for a friendly baker offers a second, paid from the baker's
  purse.
- **Cost:** as Phase 1; chains are checked only at `contract done`.

### Phase 3: Becoming a storyteller

- **Goal:** a player at social level 5 applies; a DM approves, refuses or revokes; everything is audited.
- **Changes:** the `storytellers` section; `Core/RatwGameStoryteller.cpp` (`apply`, eligibility, the command gate); DM
  actions `storyteller.decide` and `storyteller.revoke` and the Storytellers panel; `game.storyteller_log` with its
  record and trim; client: APPLY TO TELL STORIES on the sheet, the STORYTELLER button once approved; doc 50 reserves
  the status, if it agrees.
- **Tests:** `Tests/storyteller_tests.cpp` (new: eligibility by level and reports; one application at a time; decide;
  revoke; a revoked account's commands refused); `tools/test_dungeon_master.py` (roles, the panel's API);
  `checkpoint_tests`.
- **Done when:** Ada, at level 5, applies; the DM reads her note and approves; she sees the STORYTELLER button on any
  of her characters; the DM revokes and it is gone.
- **Cost:** a lookup per command; nothing per tick.

### Phase 4: Tales: writing and running them

- **Goal:** an approved storyteller writes a short tale, invites their party, Chapter, circle or friends, and runs it
  with narration, story characters and open dice.
- **Changes:** drafts (1–10 steps, objectives including `told`, markers from the World Map, a cast of up to 6 with
  names checked against every registered name); the verbs `draft` to `end`; scope checks (doc 32's parties and
  Chapters now, doc 50's circles and friends when built) and blocks; delivery by doc 51's openness with the `story`
  event; narration counted as the storyteller's roleplay; the `storytellerCommands` list and slash commands, never said
  aloud; logging; the end card and the `tale` star through doc 51's `StarBook`. Client: the STORYTELLER page; story
  lines in `story.ts`; invitations with JOIN and NOT NOW; tales in the journal.
- **Tests:** `storyteller_tests` (scope and opt-in; a blocked wolf can't be invited; narration reaches participants
  anywhere in the place and onlookers only while Open or Knock; a cast name matching a resident's or player's name is
  refused; dice limits and fairness over many rolls; ticks by hand logged; the end card; the tale star counted once a
  participant; a revoked storyteller's tale pauses); `wire_tests`; `tools/client/storyteller.mjs` (new: storyteller and
  participant in two pages).
- **Done when:** Kestrel writes "The Drowned Bell" (3 steps), invites her party, narrates at the pier, voices the
  ferryman, rolls 2d6 for the crossing and ticks step 2; her party sees each line marked as the storyteller's; a
  stranger nearby sees them only while the scene is Open; at the end each participant stars Kestrel as a Storyteller.
- **Cost:** narration reaches at most 12 participants plus onlookers in range; `Game::publish` already loops over
  clients, so this adds nothing new. Commands are rate-limited; log rows are batched with the checkpoint.

### Phase 5: Visitors, prizes and calls

- **Goal:** a tale can bring a visitor on stage, give a prize from a real purse, and find players through boards.
- **Changes:** `live.story_visitors`, its DM form and `visitors.sync`; `visitor` and `dismiss` through
  `World::addVisitor` with `World::isStoryVisitor` (fights, theft, trade and recruiting refused; the Mind skips it;
  Look marks it); `prize` (own purse, the Chapter's treasury by the Officer and Head rule, or own items; `Game::record()`
  and the journal; `story prize` events); `call` (Chapter notices now, doc 54's town board later), `ask` and `admit`;
  `tale.pause`, `tale.resume`, `tale.stop`.
- **Tests:** visitors (only from the approved list; placement rules; refused for fights and trade; gone on time; never
  saved; no answer from the Mind); prizes (purses and the treasury balance; only to participants; the Officer rule; a
  journal record replays once); calls (posted; a stranger asks and is admitted for that tale only); DM stop.
- **Done when:** Kestrel brings on a cloaked messenger beside the pier for 20 minutes and voices it; gives Wren 5
  pennies from her purse, both purses balancing; her call on the Chapter's board brings a stranger she admits; the DM
  pauses the tale from the DM app and everyone in it is told.
- **Cost:** at most 2 transient visitors per storyteller, never saved; prizes are ordinary journalled transfers.

### Phase 6: Credits screens and two angles

- **Goal:** a world story's milestone ends with a credits screen and stars; storylines ask wolves to do different
  things.
- **Changes:** `milestone.credit` (the DM host builds the lines from Postgres; the server checks them, renders them per
  viewer, sends `credits` and keeps them in the journal); up to 3 `milestone` stars each (doc 51); deeds for the main
  contributors (doc 56, or logged for it); `suits` and TAKE in the journal; two-angle tie templates; the hand-off note
  for doc 34 (storylines with `source: storykeeper`, `distinct` and `suits` on quest objectives, milestone steps calling
  `milestone.credit`).
- **Tests:** credits (lines only from ledger facts; names per viewer; at most 3 stars counted by doc 51's rules; deeds
  for the main contributors; a participant in a fight gets the screen afterwards); `distinct` (one wolf can't do two
  objectives of a step; hints shown; TAKE seen by the others); `tools/test_dungeon_master.py` (the builder's queries);
  `tools/client/credits.mjs` (new).
- **Done when:** the DM credits "The siege of Ser Ferro · The gate holds" to everyone who fought at the gate that
  evening; each sees who held the gate, who raised whom and who carried the warning, and gives up to 3 stars; a tie
  storyline asks Ash to speak to the innkeeper while her mentor looks over the stables, and neither can do both.
- **Cost:** the lines are built by the DM host from Postgres, off the game thread; the server only checks and delivers,
  once per milestone.

## Built

All six phases built 2026-10-08. Migration 0049 (`game.storylines`, `game.storytellers`, `game.storyteller_log`,
`game.credits` as checkpoint sections; `live.story_visitors`) applied to DEV and PROD.

### Phase 1: the journal and storylines

- **The model** (`Core/RatwStorylines.{h,cpp}`, new): storylines of kind personal, tale or world; steps of 1-3
  objectives (`told`, `place`, `talk`, `scene`, `contract`, `hunt`, `fight`, `gift`, `deliver`); markers (a place, a
  tile, a label: never a live position); `distinct` steps where no wolf does two parts; TAKE; a trigger index (objective
  kind -> storylines whose current step waits on one), so nothing scans in the tick; the first step of a personal
  storyline ticked at once; the limits (3 personal, 3 tales a wolf, 2 running tales a storyteller, 12 wolves, a cast of
  6); save and load. Rules in `Data/Storylines/rules.json`, templates in `templates.json` (new).
- **In the game** (`Core/RatwGameStorylines.cpp`, new):
  - a talk with a resident (recognised or not), a scene settled (with the storyline's wolves in it), a contract done,
    a kill, a fight won, a Gift used outside a fight (a new `gift used` event), an item given; a place checked every 2 s
    only for those with a place to be;
  - each step done: a toast, a `step` moment on the end card of the scene its wolves are in, a chronicle line;
  - the journal's stories (`self.journal`, rebuilt only when the book changes) at the top of the existing JOURNAL page
    (doc 54's), which now also has a top-bar button; the tracked storyline's marker on the local map and the minimap
    (a gold diamond, its words along the minimap's bottom);
  - wolves named as the reader knows them (`[[id]]` in templates).
- **The DM**: `storyline.give` and `storyline.tick` through `/api/live/action`, and the *Give a storyline* form.
- **Not built as planned**: no separate `game.storyline_log` table; storylines' steps, parts and endings are
  `game.events` rows (`storyline begun`, `storyline objective`, `storyline step`, `storyline done`...), which the
  credits and the chronicle read.

### Phase 2: where personal storylines come from

- **Ties** (doc 52): when a tie is made, `tie-<starter>` (with a resident: find them, see the town, talk it over) or
  `tie-<starter>-mentor` in both journals (a scene together; two ways round the town, one asking the innkeeper while the
  other looks over the market; another scene). A tie that ends ends it.
- **Troubles** (doc 57): `trouble-<kind>` when a wolf hears one; solved by that wolf it is done, by someone else (or
  passed) it ends.
- **Contract chains**: a contract done for a resident who likes the wolf (20+) brings a courier job posted from the
  resident's own purse (it keeps 20p for its food), offered to that wolf first, in a `chain` storyline whose step waits
  on that very job; then a talk with the resident, whose Mind is told the step's line.
- **Briefs**: a step's line for the resident named in its `talk` objective, while the step is current, never from a
  storyteller.
- **Recognition only** (the user): a finished personal storyline gives a chronicle line and a small deed
  (`finished_story`), no award.

### Phase 3: becoming a storyteller

- **Applying** from the character sheet (APPLY TO TELL STORIES, a note of 500 letters): social level 5 (doc 49's,
  summed over the account), no upheld report in 30 days, one application at a time.
- **Deciding** (DM actions `storyteller.decide` with a reason, `storyteller.revoke`): the standing is the account's; the
  player is told in game; revoking pauses its running tales and tells their wolves. The Storyteller status is now open
  to approved storytellers (doc 50's check).
- **The kept log** (`storyteller_log`, a checkpoint section): every apply, draft, start, invite, admit, tick, narration,
  story character's line, roll, visitor, prize, call and end; the text cleared after 30 days, the record after 180.
- **The DM app**: a Storytellers panel in the Players tab (applications, storytellers, tales with who did what and
  their words, visitors, *Give a storyline*, *Credit a milestone*).

### Phase 4: tales

- **Writing** on the STORYTELLER page (approved storytellers only): a title, a premise, 1-10 steps (each a title, a text
  and an objective), a cast of up to 6 whose names may be no real wolf's (residents, players, aliases).
- **Running**: BEGIN; INVITE (the storyteller's party, Chapter, circles and friends; never one blocked); JOIN or NOT NOW
  in the invited wolf's journal; TICK (a `told` part, or any by hand, which the log says); narration and a story
  character's line (`/narrate`, `/npc name: line`, or the page), at most 1,000 letters, one every 3 world seconds,
  shown as "✦ STORY · its title · the storyteller as you know them" to its wolves in the storyteller's place and to
  wolves in speaking range while its scene isn't Private; counted as the storyteller's roleplay in the scene; open dice
  (`/roll 2d6+1 for the crossing`: 1-10 dice of 2, 4, 6, 8, 10, 12, 20 or 100 sides, ±20), by participants too.
- **The end** (done, failed, abandoned): an end card for each of its wolves with who did what, and one star for the
  storyteller (doc 51's `tale` kind, tagged Storyteller), once.

### Phase 5: visitors, prizes and calls

- **Visitors** from the DM's approved list (`live.story_visitors`, saved from the panel, read at start and on
  `visitors.sync`): at most 2 at once, 10-60 minutes, within 6 tiles, outdoors or in an inn's common room; marked "(part
  of a story)"; a fight with any visitor is refused ("They're part of a story"), and the Mind never answers one.
- **Prizes**: coins from the storyteller's purse, or the Chapter's treasury for its Chapter's tale by an Officer or the
  Head, or an item it carries; to the tale's wolves only; journalled, a `story prize` event.
- **Calls**: POST A CALL puts the tale on its town's board; strangers see it in their journal (CALLS IN THIS TOWN) and
  ASK TO JOIN; the storyteller ADMITs them, for that tale only.
- **The DM**: `tale.pause`, `tale.resume`, `tale.stop`, its wolves told.

### Phase 6: credits screens and two angles

- **A milestone** (`milestone.credit`): the DM host builds the wolves and their lines from the ledger (a storyline's
  wolves and the parts they did; or everyone who acted at a place in a window, their fights, wolves tended, kills,
  jobs), the game checks each wolf, renders the lines per viewer, and keeps the screen 3 days (shown after a fight).
  Up to 3 stars each (doc 51's `milestone` kind, under its counting rules); a great or legendary deed for the main
  contributors (the first three, or those the DM names).
- **Two angles**: `distinct` steps, `suits` hints and TAKE in the journal; the mentor tie templates use them.

### Tests

- `Tests/storyline_tests.cpp` (45 checks) and `Tests/storyteller_tests.cpp` (54 checks), new; the Client's
  `storyteller.test.ts`; `tools/test_dungeon_master.py`, `test_chronicle.py`.
- `tools/client/storyteller.mjs` (new), in a real browser: a storyline in the journal with its marker; a storyteller
  approved (the dev console), a tale written and begun, a call answered and admitted, narration and dice from the
  composer marked as the story's, the end card and the storyteller's star.

### Not done, or done differently

- The storyline log is `game.events` rows (above).
- Ties' storylines aren't tested through doc 52's whole flow; the trouble and chain ones are.
- A storyteller's markers are a place picked by id on the page, not on the World Map; the editor gives each step one
  objective.
- Visitors' placement allows inns' common rooms only among interiors (no list of seats of power is needed).

## Depends on and feeds

- **Depends on doc 51** (`StarBook` for tale and milestone stars; openness for narration; the `step` moment), first in
  the build order; **doc 50** (friends, circles, block, reports, the ✦ and the status); **doc 49** (account social
  level); **doc 52** (ties), **doc 57** (troubles), **doc 54** (town boards, seats of power), **doc 56** (deeds).
  Phases 1, 3 and 4 work without 52, 54, 56 and 57; their sources and hooks switch on as those land.
- **Feeds doc 56** (chronicle lines from `game.storyline_log`; deeds from milestones), **docs 52 and 57** (the wrapper
  and templates), **doc 54** (calls on the work side), and **doc 34** (the journal as its quest log; the objective
  shape with `distinct` and `suits`; `milestone.credit`).

## Risks

- **Two engines:** doc 34 may later want its own. This one stays linear with a fixed trigger list, and the hand-off
  makes doc 34 a source, not a rival. If doc 34 is built first after all, Phase 1 becomes "the journal on doc 34's
  runs".
- **Storytellers behaving badly** (harassment in narration, impersonation, misleading players about the world): opt-in,
  scope, block, the ✦ on every line, the cast-name check, the 30-day log, reports and revocation.
- **Keeping narration** is prose stored for 30 days, a privacy cost the user should agree to (Open questions).
- **Prizes as a way to move money between accounts:** no worse than giving; conserved, logged, seen by the DM.
- **Visitors mistaken for residents:** the flag, Look's mark and the refusals. Check that DM visitors follow the same
  refusals.
- **Templates that read as chores:** steps should be few and tied to people (doc 48's "small daily wins");
  `templates.json` can grow.
- **Other sessions:** nothing here edits `RatwOrchestrator`, `RatwDemand`, `RatwOddJobs`, `RatwResidents`,
  `RatwSociety.h` or `Data/Economy`. Chains and prizes only call existing functions (`Society::shift`, contract
  posting).

## Decisions

**Agreed (doc 48):**

1. A storyteller access level for players, applied for after social level 5 (Decision 12; Part 9).
2. Not the DM app: player credentials never grant DM access; the tool is narrow and audited (Part 9; doc 17).
3. A DM or admin approves and can revoke at any time (Part 9).
4. Stories for the storyteller's own Chapter, party, circle or friends; players opt in (Part 9).
5. What storytellers can and can't do, as doc 48 Part 9 lists it; the DM sees everything.
6. Participants star the storyteller with the Storyteller tag (Part 9).
7. Personal storylines with markers: 3–7 steps from ties, troubles, contract chains or the Storykeeper, the first step
   ticked (§8.2; Decision 8).
8. Credits screens at big stories' milestones from the ledger, up to 3 stars each with tags, and great or legendary
   deeds (§8.3; Decision 7).
9. Quests with two angles that finish only when every part is done (§6.8; Decision 6).

**New placeholder choices (this plan):**

10. Build a minimal step tracker and journal now rather than wait for doc 34, which will write into them.
11. One model, storylines, for personal stories, tales and world stories: "journal" on screen; storylines and tales in
    code.
12. Tales have 1–10 steps.
13. Limits: 3 personal storylines, 3 tales a wolf, 2 running tales a storyteller, 12 participants, a cast of 6, 2
    visitors of 10–60 minutes.
14. Storylines never lapse with time; an idle tale pauses after 30 days.
15. Markers point at places, never live positions, and never at players.
16. Story characters are never real residents, and cast names can't match a registered name.
17. Narration follows the scene's openness; a tale's scenes start as Knock.
18. Narration counts as the storyteller's roleplay in the scene.
19. Calls on boards let strangers ask to join; admitted, they join that tale only.
20. Narration and story characters' lines kept 30 days for the DM, then cleared.
21. A finished server-made storyline is recognised (a chronicle line, a small deed), never paid (the user, 2026-10-08);
    tales pay nothing of their own.
22. Credit lines built by the DM host from Postgres, delivered by the server.
23. Recommend to doc 50: reserve the Storyteller status for approved storytellers.

## Open questions

1. **Keeping narration for 30 days** as the DM's evidence: agreed, or metadata only?
2. **Calls to strangers:** may a storyteller recruit strangers through boards (they ask, the storyteller admits), or
   should tales stay within Chapter, party, circle and friends?
3. **A finished personal storyline:** a small award, as planned, or recognition only?
4. **The Storyteller status:** approved storytellers only, or anyone, as in doc 48 §3.5?

**Answered (user, 2026-10-08):**
1. Keep narration and story characters' lines 30 days as the DM's evidence, then clear the text (metadata kept).
2. Yes: board calls reach strangers, who ask; the storyteller admits them, for that tale only.
3. Recognition only: a finished personal storyline gives a chronicle line and a small deed, no award (decision 21
   changes accordingly).
4. Settled by plan 50: the Storyteller status is for approved storytellers only.
