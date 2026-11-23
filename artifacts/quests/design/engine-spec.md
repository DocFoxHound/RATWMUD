# Story Maker additions for The Ninth Howl and the quest packs: trigger and engine specification

This spec covers every trigger, effect, field and engine change that the campaign (`campaign.md`), the small, medium, large and stage quest packs (`quests-small.md`, `quests-medium.md`, `quests-large.md`, and the `nh-pack.stage` program of `decisions.md` §7.3) and the talk plan (`cast-and-talk.md` §5.4) need and the Story Maker does not have yet. It was checked against the code on `ridgemere2` at `17f6db3b` (read only; nothing in the repo or the world was changed).

**Binding.** The coordinator's `decisions.md` (§5 names and items, §3 purses, §4 camps, §6 pacing by readiness) wins over this file. This revision applies it. Where a pack or talk file still says M1, M4, M6, T1, T2, T4 or T8, read the name in §0.5.

**The user's answers (2026-11-22), binding over `decisions.md` §6 where they differ, applied in this revision:**
- **(a) Build everything**: every engine phase, Phase 9 included (nothing is deferred any more), and all the content. **Progression is not tied to a 40-day calendar.** A chapter moves on by **participation** (the share of the active wolves who did the chapter's work) **plus time** (a least number of days in the chapter), and an admin **pushes it forward** from the Story Maker's Campaigns console. That is R1–R3 (§2, §3, §6), built first, in **Phase 1a**. The calendar (§7.4) now gives earliest dates, not deadlines.
- **(b)** Finished story quest lines count toward the Quickening's `closedStories`: **yes, capped**. Only the last chapter of a large line or a campaign side story counts, and at most 4 per program (J3, §3).
- **(c)** Foe strength is tuned by combat simulations, in several passes if needed, at most 250 players (C10, "Tuning the foes", §4).

**Checked against the world read only** (Ridgemere 3, `ratw_ridgemere3`, world id `upper_accord`, `tools/world_db.connect('ridgemere3', 'owner')` inside `SET TRANSACTION READ ONLY`, rolled back). Latest checkpoint: 2026-11-22 22:55 UTC, `calendarDays` 66.40.
- `society.accounts` has 2,164 accounts, 155 of them `till:job:<founder>`. There is no `town:*`, `house:*` or `*:works|watch|church` account. The purses this spec names hold: treasury 1,355; `stores:` coldwater 212, larkfield 248, southmeadow 236, fenshore 213, saltwell 224, oakhaven 247, frosthollow 247; `till:job:` rm_eleanor_saltcot 161, rm_osric_ropeworth 125, rm_brenna_slaggate 156, rm_vigdis_rainmore 112, rm_rupert_fogton 90, rm_corra_coalcot 141, rm_ilse_coalley 126, rm_edric_millton 197.
- Every resident this spec names is placed, with a reachable home (and a reachable work place where they have one), and alive, except the two named as dead on purpose: `rm_gunnar_harrowgate` and `tn_cw_iris_greywater`. Three placed residents are dead in all (the V1 alive check's test cases).

**Ids.** Items keep the numbers they already have:
- **C1–C15** from `survey/gaps.md` §C and **J1–J11** from `campaign.md` §11.2;
- **N1** (story choices overflow into "Ask about…"), **N2** (window facts of a wolf's own story state), **N3** (a tracker that mirrors a world variable), **N4** (a quest's per-wolf wait before taking it again, new from `decisions.md` §8) and **V1–V3** (approval and tooling fixes), from this spec;
- **R1–R3** (participation, readiness, and the console's Advance), from the user's answer (a); **C10t** (tuning the foes by simulation), from answer (c);
- **SQ1–SQ5** from `quests-small.md` §3.3; **M2, M3, M5** from `quests-medium.md` §5.2 (M1, M4 and M6 are merged away, §0.5); **L1–L5** from `quests-large.md` §13.2; **T3, T5, T6, T7, T9** from `cast-and-talk.md` §5.4 (T1, T2, T4 and T8 are merged, §0.5).

**Contents.**
- §0 Conventions: what an entry gives, the twin rule, flags, files touched by every item, and one name for each thing (§0.5)
- §1 Every addition at a glance
- §2 The trigger language (both engines): C1, C2, **R1**, C3, C12b, J4, J6, J10, J11, C13b, M2
- §3 The story's own parts (runner): **R2**, J1, J2, J3, C6, C7, C12a, C13a, N3, quest objectives (SQ1, SQ5, L3), N4, L5, SQ4
- §4 World effects (game orders): C4, C5, C8 (with the pair scope), N2, T7, J8, C9, C10 (with **C10t**, tuning the foes), C11, C14, C15, J5, J7, J9, L1, L2, L4, T3, M5, SQ3, M3
- §5 The dialogue window: N1, C6's display side, T5, T6
- §6 Approval and tooling: **R3** (the console's readiness and Advance), SQ2, V1, V2, V3, T9
- §7 Build phases: Phase 0 (today-only content), the phase table, the calendar (earliest dates), and what ships after each
- §8 Decisions for the user
- §9 The phase list

---

## 0. Conventions

### 0.1 What each entry gives
1. **Catalogue entry.** Exact JSON for `Data/Storylines/trigger-catalogue.json`, in its own style: `id`, `group`, `title`, `sentence` (the workbench sentence, `{key}` slots), `help`, `params` or `fields`, and the flags.
2. **Semantics.** What it does, step by step, including refusals.
3. **Where.** Files and functions to change.
4. **Checks.** Validation (offline, `story_program` / `story_triggers` / `story_quests`) and approval (`story_runner.references`).
5. **Undo.** What the game or runner lets go when the run ends (doc 85, guide §14).
6. **Tests.** The cases to add, and where.
7. **Editor and guide.** Workbench and console changes; guide sections.

### 0.2 Flags (as the catalogue spells them)
- `"v2": true`: version-2 stories only.
- `"story": true`: the story keeps it itself (runner state), never a game order.
- `"actsOnWolves": true` (**W**): needs "may act on players' wolves" on a hosted world.
- `"needsBuilder": true` (**B**): needs a builder or the owner to approve it on a hosted world.
- `"watch": true`: an event written only while a live story listens for it.
- `"needsMatch": [...]`: filters a trigger on the event must give.
- **U** (not a flag; listed in `story_program.UNDONE_AT_END` and guide §14): undone at run end.

### 0.3 The twin rule
`tools/story_triggers.py` and `Core/RatwStoryTriggers.cpp` (`Dispatch`, `Dispatch2`) are twins; `tools/test_story_native.py` holds them in parity. **Every change to conditions, operands, actions, `match` or `_act` is made in both,** and gets a parity case in `test_story_native.py`. The runner refuses to start without `build-core/libratw_story.so` (`story_runner.py:4787`), so the C++ side is not optional.
- Python: `operand_type` (303), `value_of` (931), `test_v2` (958), `fact_test` (1032), `_act` (1242), `_check_action_v2` (492), `_check_condition_v2` (350).
- C++: `Dispatch2::value` (324), `Dispatch2::test` (390), `Dispatch2::fact` (495), `Dispatch2::act` (739), `Dispatch::matches` (98) and `Dispatch2::matches` (982).
- Native request: `story_native.fire` builds it. It sends only the acting wolf's player counters. Any rule using `rank`, `tally`, `audience` or `mirror` makes the host send every cohort wolf's counters for the variables read (the same widening `each over participants` needs).

### 0.4 Touched by every item
- **Catalogue entry** → `story_program.py` reads `EVENTS`, `EFFECTS`, `EFFECT_PARAMS`, `WOLF_EFFECTS`, `BUILDER_EFFECTS` from it. New effect groups go in `actionGroups`; new event groups in `groups`.
- **Game order:**
  - `story_runner.WORLD_ORDERS` (645), or `action()` (459) for the older single-key orders;
  - the game's dispatch in `Core/RatwGame.cpp` (`applyDmAction` branch chain from 1397), or `Game::storyWorldOrder` plus `applyStoryWorld` (`Core/RatwGameStoryWorld.cpp:65`, 83) for orders that carry the run;
  - `tools/test_story_effects.py::test_the_lists_the_catalogue_names_are_the_games` checks that the two lists agree.
- **Undo:** add the key to `story_program.UNDONE_AT_END`; keep the held thing in `Storyline::held` (via `Game::noteStoryHeld`); release it in `Game::endStoryWorld` (457), and re-assert it in `Game::tickStoryWorld` (501) where a restart could lose it.
- **Guide:**
  - `python3 tools/story_guide.py --write` regenerates the reference section of `Docs/Story-Maker-guide.md` from the catalogue;
  - `tools/test_story_guide.py` and `story_guide.py --check` fail until it is regenerated;
  - the hand-written sections named per item are edited by hand;
  - `Docs/story-programs.md` (the format) gets each new field.
- **Workbench:** pickers come from the catalogue's param `type` (`Editor/src/dm/triggers/pickers.tsx:188-235`). A new `type` needs a picker branch there and a `refs` source (`story_runner.refs`, 4691). `Editor/src/lib/workbench.test.ts` checks that sentences render for every catalogue entry.
- **Migrations** (`Database/migrations/`): the next free number is **0080**. This spec uses 0080 (story memory, spares, greets, titles), 0081 (realm variables) and 0082 (closures receipts index). Each `references()` check that needs a table first asks `to_regclass(...)`, as Phase 9 does. R1–R3 need **no migration**: readiness lives in the run's `state`, and `dm.story_commands.kind` (0071, `CHECK (kind ~ '^[a-z][a-z-]{1,31}$')`) already accepts `advance`.

### 0.5 One name for each thing
Every design file uses the left column (`decisions.md` §5.1). The right column is retired.

| Say | Not | Note |
|---|---|---|
| C10 `foe.outlaw` (a resident downed, never killed) | "C10 `npc` mode" (campaign §3.1, §11.1; cast-and-talk §5.4) | |
| C12a `story.notify` / event `story notified` | "C12" meaning packs → chapter | |
| C12b `match` / `field` with `starts` / `in` | "C12" meaning deed matching | |
| C13a events `variable changed`, `month begins`, `succession`; C13b `resident has post` | "C13" | |
| L1 `hunt.stage` | M1 `wild.spawn` / `wild.leave` | **M1 is dropped**: a staged hunt fits the hunt arena; nothing roams the map. |
| L2 `camp` on `story.area.spawn` / `bandits.call` | M4 | one naming rule (§4 L2) |
| SQ2 till purses | M6 | the same fix |
| C8 `talk.remember` / `talk.forget` with `scope: "pair"` | T1 | |
| N2 window facts, including `player.story.<var>` | T2 | |
| V2 story words check, including the expander and the giver's dialect | T4 | |
| N1 choices overflow into "Ask about…" | T8 | |
| C2 arithmetic `div` | "divide" (medium §5.2's M2 example) | |
| T3 scene cast entries for strangers | `story.tell from_visitor` | **dropped**: a face banner is not the dialogue window (decisions §9, P2-22) |

**The dialogue-window rule** (decisions §9, P2-22) binds every item here: no effect sends a resident's words in a `story.tell` banner. A resident's words reach a wolf only through an offer's `say` / `thanks` / `during`, `npc.offer` and chain replies, `npc.line`, C5 `npc.greet`, a scene (each speaker in their own panel), or their own tree lines. V2 refuses a `story.tell` whose `from` names a resident and whose text holds quoted speech (§6).

---

## 1. Every addition at a glance

Sources: **C**/**J** as before; **N**/**V** this spec; **SQ** small pack, **M** medium pack, **L** large pack, **T** talk plan. "Merged" items have no row of their own (§0.5).

| Id | What | Kind | Needed by | Phase | Agent-hours |
|---|---|---|---|---|---|
| T9 | allow-list words (`tools/dialect_allow.json`) | data edit | talk library | 0 | — |
| C2 | operand `audience` (now with the program's `activeDays`, and the runner's `seen` map seeded at launch); arithmetic `times`, `div` (**moved from Phase 2**: readiness reads it) | language | readiness; scaling at hundreds; the garland rope (small #38) | **1a** | 3.5 |
| R1 | participation: operands `{"percent": …, "of": …}` and `{"among": …, "of": …}`, condition `participation` (a share of the active wolves, with a floor and a cap) | language | every chapter's readiness (answer (a)); packs' "half the town turned out" | **1a** | 4.5 |
| R2 | readiness: a phase's `ready` block (participation, least days, values, missions, quests; `auto`; a `then` anchor; `slowAfterDays`, `autoAfterDays`), the exit `ready`, the runner's readiness state, command `advance`, alerts `ready` and `slow`, the held clock | runner + format | every chapter (answer (a)) | **1a** | 8 |
| R3 | the Campaigns console: readiness bars per running chapter, Advance…, Hold beside them, the list's "ready to advance" alert and column, the chapters bar; guide sections | console + guide | the admin who pushes | **1a** | 5 |
| V1 | Approval checks what it misses today, **now with the `alive` check** | approval | every program | 1b | 3.5 |
| SQ2 | Till purses: `till:job:<id>` accepted and mapped; every purse checked against the checkpoint's accounts (was M6) | approval + game | campaign Purse path; large restitution; small/medium shop pay | 1b | 1.5 |
| V3 | Guide fix: `death` filters on `actor` | doc | authors | 1b | 0.5 |
| J1 | `npc.offer` keeps a list of wolves | story | Ch 1 on, packs | 1b | 1.5 |
| C11 | `npc.spare` / `npc.unspare`; `npc.protect` gets `lasts` | game | Ch 1 setup (§8.6) | 1b | 4 |
| C9 | `figure.set` / `figure.clear` (resident, name or wolf) | runner + game | Ch 1–3, 9 | 1b | 4 |
| C8 | `talk.remember`, `talk.forget` (realm, town, resident **and pair**, was T1), `belief.plant`; realm owner | game + talk | every chapter's talk; the 17 `pair.mem` gates | 1b | 9 |
| T7 | claim kind `story.cleared` beside `story.rumour` | talk data | clearing Ingrid Kendrick and the logger | 1b | 0.5 |
| N2 | window facts `player.quest.<story>/<quest>` **and `player.story.<var>`** (was T2) | game + runner | talk paths; "you pledged to Coldwater" | 1b | 2 |
| C1 | operands in effect params; `{name}` fills in text; goal operand; tracker "of" | language + game + client | Ch 1, 5, 7, 9; packs | 2 | 10 |
| M2 | operand `{"purse": "<id>"}`: what a purse holds now | language + game facts | rewards that follow the purse | 2 | 1.5 |
| C12b | `match` and `field` with `starts` / `in` | language | packs reading deeds | 2 | 2 |
| N3 | tracker `mirror` (player tracker of a world variable) | story + runner | "Your town's howl" | 2 | 2 |
| J4 | `calendar is moon`, event `moon` | language + game | Ch 1, 2, 4 | 2 | 2 |
| SQ1 | objective `where: {cells}` or `{region}` | story | small 18, 25; pack hunts | 2 | 2 |
| SQ5 | objective `credit: "target"` (and `"present"`, for L3) | story | stall sales, vouch, told the wardens | 2 | 1.5 |
| L3 | `camp cleared`, `caravan arrives`, `ship lost`, `ship docks`, `project built`, `festival` as mission events | catalogue + story | every "break the camp", wagon and festival objective | 2 | 0.5 |
| L2 | `camp: <name>` on `story.area.spawn` and `bandits.call` (was M4) | game | every story fight | 2 | 1.5 |
| M5 | `cell` on `trade done` | catalogue | "sell it in the city" (medium Q22) | 2 | 0.5 |
| N4 | quest `join.again: <days>` | story | the `join.cap: 1` quests (decisions §8.1) | 2 | 1.5 |
| C5 | `npc.greet` | game + window | every chapter | 3 | 4 |
| C4 | `title.give` / `title.take`, event `title given`, fact `player.title` | game + client | recognition | 3 | 6 |
| C3 | action `rank`; quest `credit: share`; operand `share` | language + story | recognition | 3 | 6 |
| J11 | operand `tally` | language | Ch 9 counting | 3 | 3 |
| J8 | `belief.forget` | game | Ch 1, 3 | 3 | 3 |
| V2 | story words check (symbols through the expander, dialect by the giver, forbidden words, face banners; was T4) | tooling | every program | 3 | 4.5 |
| T5 | `npc.line` through `inVoice` | game | phase greetings in each speaker's voice | 3 | 0.5 |
| C12a | `story.notify`, event `story notified` | runner | packs → chapters | 4 | 3 |
| C13a, C13b | events `variable changed`, `month begins`, `succession`; `resident has post` | runner + game | wake-ups, the crown | 4 | 5 |
| C6 | quest `requires`; `offer.during` | story + window | chains, packs | 4 | 6 |
| N1 | story choices overflow into "Ask about…" (was T8) | window | giver crowding | 4 | 3 |
| J3 | quest `closure: "line" \| "side"` (Quickening count; answer (b): only a large line's last chapter or a campaign side story, at most 4 per program, enforced by the game per wolf too) | story + game | every large line's last chapter; campaign side stories | 4 | 5 |
| J10 | `wolf has {tier, family}` | language + game | Ch 4, 6, 8; small 19, 42, 43, 45 | 4 | 2 |
| L5 | `quest.withdraw {quest, player}` | story | large chapter-3 roads (moot once C6 is in) | 4 | 1 |
| J5 | events `howled`, `chorus closed`; effect `npc.howl` | game | Ch 4, 7, 9 | 5 | 6 |
| J6 | `count in region` counts residents | language + game | Ch 8 haul | 5 | 3 |
| L1 | `hunt.stage {region, species, count, minutes}` (replaces M1) | game | the bear (`lq-old-brown`, medium Q13) and the boar (`lq-blight-and-boar`) | 5 | 4 |
| J9 | tagged story items | game + client | Ch 3, 5, 8 | 6 | 10 |
| C10 | `foe.add`, `foe.leave`, `foe.outlaw`; foe events | game | Ch 7, 9; packs' bounties | 7 | 16 |
| C10t | tuning the foes: `foe_sim` (new), `world_check --foes`, up to 4 sim passes at ≤ 250 players against a written bar (answer (c)) | sims + data | every foe fight | 7 | 6 |
| T3 | strangers in scenes: a cast entry `{id, visitor}` with the `like` face | game + scenes | the Captain at the parley, Hakon in the Turn, the Envoy | 7 | 3 |
| J2 | `scope: "realm"` variables | runner + language | Acts II–III | 8 | 8 |
| C14 | `npc.post` (one atomic post swap) | game | Ch 9 ending | 8 | 5 |
| L4 | `npc.sentence {npc, place, days}` (outlives the run) | game | `lq-wreckers-light` (Ilse's week in the cells) | 8 | 3 |
| C7 | `cast.pick` + quest `giver` from a variable | runner | packs at scale; campaign two | 9 | 6 |
| C15 | `mantle.assign` restores the binding it replaced | game | later | 9 | 2 |
| J7 | `npc.queue` (unplaced resident first for a vacancy) | game | campaign two | 9 | 4 |
| SQ3 | lend a paw to a resident at work (built, answer (a)) | game | small 5, 10, 13, 30, 40 | 9 | 5 |
| SQ4 | quest `rewardLimit` (built, answer (a); it writes the `nh-purses` counters) | story | packs that want a plain cap | 9 | 1 |
| M3 | `house:<faction>` purses (built, answer (a); its economy check is a sim and still needs the user's go-ahead, §8 D8) | game + economy | medium Q3, Q10; Greatly paying itself | 9 | 3 |
| T6 | a `story` talk topic for the director (built, answer (a)) | talk + game | NPC-to-NPC scenes on the campaign | 9 | 1 |

Phase totals, Phase 0 and the end-of-slice check are in §7. **Nothing is deferred** (answer (a)). Phase 9's items keep their fallbacks in their entries until they land, so no content waits on Phase 9.

---

## 2. The trigger language (both engines)

### C1. Values in effect parameters and texts

**C1a: operands in effect parameters.**

*Catalogue.* No new entry. Each effect param of type `int`, `resident`, `wolf`, `place`, `item`, `faction` or `town` gets `"operand": true` (all of them, except the exceptions below). The top-level `about` gains one sentence: "A parameter marked operand may be a value: {"var"}, {"field"}, {"each"}, {"share"}, {"audience"} or arithmetic, of the parameter's kind."

**Exceptions** (literal only):
- ids the story owns: quest, objective, offer, scene, cue, signal, region, visitor, foe, figure, story, camp (L2);
- `npc.line` (an entry effect);
- `story.start`;
- anything under `quests[].rewards` that names the quest.

*Semantics:*
1. In `_act` / `Dispatch2::act` (op `effect`), before the effect is appended to `triggerEffects`, each operand param is resolved with `value_of` in the trigger's context (`x`: actor, `$each`, the event).
2. An `int` is floored, then clamped to the param's `min`..`max`. If `nonzero` and the result is 0, the effect is dropped.
3. A `resident`, `wolf`, `place`, `item`, `faction` or `town` value must be a non-empty text.
4. If any operand is `MISSING` or empty, the effect is **not sent**. The trigger's history gets the note "skipped: <param> has no value" (`x.note`). This is the same rule as an unknown fact: nothing fires on unknown.
5. Quest rewards resolve their operands when the reward is given (`story_quests` `complete`/`settle`), with `$actor` the credited wolf and `{"share":true}` (C3).

*Checks:*
- `story_program._param_ok` (263) accepts a dict for a param with `"operand": true` and calls `triggers.operand_type(v, phase, each)`.
- The family must match: `int` → number; `wolf` → a wolf-typed variable, `{"field":"actor"}`, `$each` over wolves, or `$actor`; `resident` → a resident-typed variable, `{"field":"target"|"item"}`, or `$each` over residents; `text` params are not operands (use C1b).
- `references()` (`story_runner.py:83`) skips a param whose value is a dict. The game refuses unknown ids at run time, and the effect's `ifRefused` applies.

*Where:*
- `story_program._param_ok`, `validate_effect` (304);
- `story_triggers._act` and `_check_action_v2` (op `effect`, 556);
- `story_quests` reward giving;
- `Core/RatwStoryTriggers.cpp` `Dispatch2::act`.

*Tests:*
- `test_story_v2.py`: coins from `{"min":[20,{"var":"p_voice","of":"$actor"}]}`; an `npc.walk` whose `npc` is a resident variable; a missing value skips with a note; clamping.
- `test_story_native.py`: parity for each.
- `test_story_problems.py`: a wolf variable in a resident param is refused.

*Workbench:* `pickers.tsx` shows the existing `OperandPicker` beside the id picker ("or a value…") for params marked `operand`. `Sentence.tsx` renders `{var}` as a chip.

**C1b: `{name}` fills in words.**

*Semantics:*
1. In a **version-2** story, the text params of these effects may hold fills:
   - `story.tell.text`, `board.post.text`, `journal.note.text`, `npc.speak.text`, `deed.award.detail`;
   - the C5 `npc.greet.text`, the C4 `title.give.title`;
   - quest `summary`, objective `label`, and `offer.say` / `offer.thanks`.
2. A fill is `{name}` (a declared variable) or `{name.of}` (that variable of the acting or looped wolf). `{{` and `}}` are literal braces.
3. The engines fill it in `_act` (both twins): a number becomes its digits; a text is put in as it is.
4. A `wolf`, `resident`, `place`, `item` or `faction` value becomes a **name token** `{@w:<id>}`, `{@r:<id>}`, `{@p:<id>}`, `{@i:<id>}` or `{@f:<id>}`. The engines never read names, so they stay pure.
5. **The game** swaps each token for the display name when it applies the order. This is a new helper `Game::fillStoryNames(text, limit)`, called by:
   - `story.tell` (`RatwGame.cpp:1848`);
   - `board.post`, `journal.note` and `npc.speak` (`applyStoryWorld` and the scene orders);
   - deed award;
   - the quest views (`applyStoryQuests`, `RatwGameStorylines.cpp:~1100`).
   The names come from: a wolf's character name; a resident's or a place's name; an item's name from `items::`; a faction's name.
6. Quest words fill **per wolf**: `story_quests.view`/`views` fill with the viewing wolf as `of`.
7. A missing value fills as an empty string, and the history gets a note.
8. The game cuts the result at the param's limit on a word boundary (`json::utf8Prefix`).

*Checks:*
- Every fill must name a declared variable whose type is not a list or a timer, and `.of` only a personal one (`_check_of`).
- The length is checked with each fill at its widest:
  - number: 11 characters;
  - text: 200, so a text fill in a 300 limit leaves room;
  - a wolf name: 24 (`identity::NewNameMost`);
  - a resident, place, item or faction name: 40.
- A v1 story's braces are never read, so v1 texts are unchanged.

*Where:*
- `story_program.validate_effect` (text params);
- `story_quests.check_quest` (words);
- `story_triggers._act`; `Dispatch2::act`;
- `Game::fillStoryNames` (new, `Core/RatwGameStorylines.cpp`).

*Tests:*
- `test_story_v2.py`: digits; a wolf token; `{{`; an undeclared fill refused; too long once filled refused.
- `Tests/story_world_tests.cpp`: tokens become names, and the cut.
- `test_story_native.py`: parity.

*Workbench:* `TextPicker` (pickers.tsx:207) underlines fills, offers declared variables after `{`, and shows the widest length. `Rehearsal.tsx` shows tokens as `«wolf: Bilbo»` using the rehearsal cohort's ids.

**C1c: a quest objective's goal from a value.**

*Semantics:*
- An objective's `goal` may be an operand of number kind (`{"var":"ch_goal"}`, `{"max":[12,{"times":[{"audience":"active"},2]}]}`).
- It is read when a copy starts, and kept in the copy as `c.objectives[id].goal`.
- With the objective field `"goalLive": true` (story scope only), it is read again each time the copy is credited and at each journal sync. It is never set below the amount already made, and the objective completes the moment `n >= goal`.
- The result is clamped to 1..1e6.

*Where:* `story_quests.py`:
- objective check (90): allow a dict for `goal` and the new optional `goalLive`;
- the progress code (560–590);
- `view` (820) reads `goal` from the copy.

*Tests:* `test_story_quests.py`: a story-scope bar whose goal rises with `ch_goal` mid-run; a goal below n completes at once.

*Workbench:* `QuestEditor.tsx`: the goal field takes the operand picker, and "keeps up with it" is a toggle.

**C1d: a tracker "of" another value.**

*Semantics:*
- A declaration may add `"showOf": "<another declared number>"`. The tracker then reads "label: value / other", for example "This chapter: 34 / 80".
- `story_quests.trackers` (855) adds `goal` to the tracker row.
- The client prints `a / b` and a thin bar (`Client/src/game/quests.ts`; `Client/src/game/quests.test.ts`).

*Checks:* the variable must be a declared number with `show`. `showOf` must be a number of a scope the same wolf can read (story, world, or the same personal scope).

### C2. Operand `audience`; arithmetic `times` and `div` (Phase 1a)

Moved from Phase 2 to Phase 1a, because readiness (R1, R2) counts the active wolves.

*Catalogue* (`types.operand` text and the top-level `about` gain the forms):
```json
{"audience": "all|active|online"}
{"times": [a, b]}
{"div": [a, b]}
```
*Semantics:*
- `audience` gives one of three counts:
  - `all`: the size of the cohort (for an open story, the wolves who have joined);
  - `active`: cohort wolves seen (an `arrival`, or any event they were the actor of) within the program's **`activeDays`** game days. `activeDays` is a new optional top-level program field, 1–120, default **18**: three real days at the normal speed, so a wolf who plays every other evening still counts. (The first draft's 7 game days was 28 real hours, which counted a wolf who missed one evening as gone.)
  - `online`: cohort wolves signed in now.
- The runner keeps `state['seen'] = {wolf: game day}` (`story_runner` fact intake, before dispatch: every event the runner reads whose `actor` is in the cohort). It passes `audience: {all, active, online}` in the dispatch context (Python `x.audience`; native request key `audience`).
- **Seeded at launch:** a run that starts (including a chapter started by `story.start` with `audience: "same"`) fills `seen` with one query, the latest `game.events.day` per cohort actor within `activeDays`. That is the same shape as `_recent_wolves` (`story_runner.py:3981`), but by game day. So a new chapter does not open with 0 active wolves. A wolf who joins later is seen from their first event.
- `seen` holds at most the cohort (≤ 5,000). Wolves who leave the cohort are dropped from it.
- `online` is read from the facts' `wolves` section when the program watches it, and otherwise from the latest `story.cohort` answer. If neither is known, it is `MISSING`.
- `times` multiplies. `div` divides and floors toward zero; dividing by 0 is `MISSING`.
- Both are added to `ARITH` (`story_triggers.py:41`), with the same two-deep limit.

*Rehearsal:*
- `all` = len(cohort).
- `active` = cohort wolves with any event in the last `activeDays` game days of the case.
- `online` = cohort wolves whose last `arrival` came after their last `departure`.
- A case may override all three with `"audience": {"all":n,"active":n,"online":n}` (a new optional test key in `story_program` test shape).

*Where:* `operand_type`, `value_of`, `Dispatch2::value`; the runner's fact intake (`_tick_run`, 1580) and `launch` (366, the seeding); `story_program` (the `activeDays` field; test-case shape).

*Tests:* `test_story_v2.py` (each form, div by 0, an override in a case, `activeDays` 3 against 18); `test_story_native.py` parity; `test_story_v2_runner.py` (the `seen` map; a second chapter started with `audience: "same"` opens with its wolves already active).

*Guide:* §6 Variables gets "Goals that scale" with the campaign's `max(base, k × active)` example, and what "active" means.

### R1. Participation: what share of the active wolves took part (Phase 1a)

The user's answer (a): "progression should be made by participation (percentage of active users accomplishing specific tasks, for example), plus time". R1 is the measure. R2 uses it to move a chapter on, and any trigger may read it too.

*Catalogue* (`types.operand` text and `about` gain two forms):
```json
{"percent": {"list": "ch1_did"}, "of": "active"}
{"percent": {"where": {"test": "compare", "left": {"var": "p_ch1", "of": "$each"}, "op": ">=", "right": 3}}, "of": "active"}
{"among": {"list": "ch1_did"}, "of": "active"}
```
and one new entry in `conditions`:
```json
{"id": "participation", "v2": true, "title": "Enough wolves took part",
 "sentence": "at least {percent}% of the {of} wolves (and at least {floor}, {cap} always enough) {source} {list}{where}",
 "help": "Counts the story's wolves who are active (seen in the last activeDays game days; or all of them, or those online now) and, of those, the ones in a list of wolves or the ones for whom a test is true. True when they are at least the share asked and at least the floor (a cap, if given, is always enough, so a crowd of 3,000 does not need 1,000). With no active wolves it is false. A wolf who did it and then went quiet leaves both counts, so the share stays honest.",
 "params": [
  {"key": "source", "type": "choice", "label": "who counts", "values": ["list", "where"], "words": {"list": "are in the list", "where": "meet the test"}, "default": "list"},
  {"key": "list", "type": "variable", "label": "the list", "of": ["wolves"], "scope": ["story", "world"], "optional": true},
  {"key": "where", "type": "condition", "label": "the test for each wolf", "optional": true, "each": "wolves"},
  {"key": "of", "type": "choice", "label": "out of", "values": ["active", "all", "online"], "words": {"active": "active", "all": "story's", "online": "online"}, "default": "active"},
  {"key": "percent", "type": "int", "label": "the share (%)", "min": 1, "max": 100},
  {"key": "floor", "type": "int", "label": "at least this many", "min": 1, "max": 5000, "optional": true, "default": 1},
  {"key": "cap", "type": "int", "label": "this many is always enough", "min": 1, "max": 5000, "optional": true, "words": {"": "no cap"}}]}
```
There is a new param type `condition`: "a condition tree tested once for each wolf, the wolf as $each".

*Semantics:*
1. **D**, the wolves counted against:
   - `active`: the cohort wolves with `seen[w] >= day − activeDays` (C2);
   - `all`: the cohort;
   - `online`: as C2's `online`, and `MISSING` when it is not known.
2. **S**, the wolves who count:
   - with `list`: the wolves of that story or world `wolves` list who are also in D;
   - with `where`: the wolves of D for whom the tree is true, with `$each` = that wolf (`x.each`). An unknown result counts as false.
3. `among` = |S|. `percent` = floor(100 × |S| / |D|). When |D| = 0, `percent` is 0, not `MISSING`, so a bar reads 0%. Both are numbers.
4. The condition `participation`:
   - need = max(min(floor, |D|), ceil(percent × |D| / 100)), then min(need, cap) when a cap is given, and never below 1 (decisions §6.2: **the floor never asks for more wolves than are active**, so a world of 1 to 5 wolves can always advance);
   - it is true when |S| ≥ need;
   - it is unknown (None) when D is `MISSING` (online not known), so nothing fires on unknown;
   - when |D| = 0, it is false (the admin's push is for that case).
5. **What `where` may hold**: variables of any scope read with `of: "$each"`, `compare`, `switch`, `in list`, `days since`, and `quest is` / `objective is` with `of: "$each"`. It may not hold fact conditions (facts belong to an event; this is counted outside one), event fields, `each`, `rank`, `tally` or another `participation`. It is at most 3 deep with at most 8 leaves.
6. **Cost:** it is counted as one operand. It is not allowed inside `each`, nor more than one level deep in arithmetic. Each rule evaluation computes a given form once (it is memoised by its JSON within one dispatch). The bound is |D| ≤ 5,000 × 8 leaves.

*Checks* (`_check_condition_v2`, `operand_type`):
- exactly one of `list` and `where`;
- `list` must be a declared `wolves` list in story or world scope;
- `where` goes through `_check_condition` with a context that has `$each` (wolves) and no facts, under rule 5;
- `floor ≤ cap` when both are given;
- `of: "online"` makes the program watch `wolves` (as C2).

*Native* (§0.3): when a program uses R1, `story_program` marks it `usesParticipation`. `story_native.fire` then sends `audience.activeIds` (the D list for `active`; `online` the same way) and widens the counters it sends to every cohort wolf's values of the variables that a `where` reads (the widening `rank` and `tally` need).

*Rehearsal:* a case may give `"activeWolves": ["w1", "w2", …]` (a new optional test key beside C2's `audience` counts). Without it, C2's rule applies.

*Where:*
- `story_triggers.operand_type` (`percent` and `among` are numbers), `value_of`, `test_v2` (`participation`), `_check_condition_v2`;
- `Core/RatwStoryTriggers.cpp`: `Dispatch2::value`, `Dispatch2::test`;
- `story_native.fire` (the ids and the widening);
- the catalogue (the condition, the forms, the `condition` param type).

*Tests:*
- `test_story_v2.py`:
  - 12 active, 5 in the list: `percent` 41, `among` 5;
  - 40% with floor 6: need 6, so false; the same with floor 4: need 5, so true;
  - 2 active, floor 3, both in the list: need 2, so true (the floor is capped at the active count);
  - a cap of 3 with 2,000 active and 3 in the list: true;
  - |D| = 0 gives percent 0 and false;
  - online unknown gives unknown;
  - a `where` over a player number with `of: "$each"`;
  - a wolf who did it and went quiet leaves both counts.
- `test_story_native.py`: parity for every form and the condition.
- `test_story_speed.py`: 5,000 wolves with a `where` of 4 leaves, under 20 ms native and under 150 ms in Python (the runner's readiness path, R2).
- `test_story_problems.py`: a fact condition inside `where` is refused; a `numbers` list is refused; floor above cap is refused.

*Workbench:*
- `pickers.tsx` gets a branch for the `condition` param type: a nested `TriggerTree` with "each wolf" (`$each`) in scope and the fact conditions greyed out.
- `OperandPicker` gains "the share of the wolves who…" and "how many of the wolves…".
- `workbench.test.ts` renders the sentence.

*Guide:* §6, "Counting who took part": the share, the floor and the cap, why `active` is the fair count, and a list against a test.

### C3. Ranking, shares and the `share` operand

**C3a: action `rank`.**

*Catalogue entry* (`actions`):
```json
{"id": "rank", "v2": true, "group": "variables", "title": "Rank the wolves",
 "sentence": "Put the top {top} wolves of {over} {list} by {var} (at least {min}) into {into}",
 "help": "Orders wolves by a number each wolf has (a player variable), highest first, ties broken by the story's seed, and puts the first ones into a list of wolves (cleared first). Wolves without a value, or below the least, are left out. Use For each over that list to name, thank or reward them.",
 "params": [
  {"key": "var", "type": "variable", "label": "by", "of": ["number"], "scope": ["player", "realm"]},
  {"key": "top", "type": "operand", "label": "how many (1 to 500)"},
  {"key": "over", "type": "choice", "label": "among", "values": ["participants", "list"], "words": {"participants": "the wolves of the story", "list": "the wolves in"}, "default": "participants"},
  {"key": "list", "type": "variable", "label": "the list", "of": ["wolves"], "optional": true},
  {"key": "min", "type": "operand", "label": "at least", "optional": true},
  {"key": "into", "type": "variable", "label": "into the list", "of": ["wolves"]}]}
```
*Semantics:*
1. Read `var` for every wolf of `over`. With `over: participants`, that is the cohort (≤ 5000).
2. Drop wolves with no stored value, or below `min`.
3. Sort descending. Break ties by `fnv(seed + '|' + wolf)`, as `draw` does.
4. Clear `into`, then add the first `clamp(top, 1, 500)` wolves.
5. It is not allowed inside `each`. It counts as one action.
- *Cost:* O(n log n) on ≤ 5000 values, within the rehearsal budget.

*Checks:*
- `var` must be a player- or realm-scope number.
- `into` must be a story or world `wolves` list.
- `list` is required exactly when `over` is `list`.

*Where:* `_check_action_v2`, `_act`, `Dispatch2::act`; `story_native.fire` widens the counters sent (§0.3).

**C3b: quest field `credit: "share"` and operand `{"share": true}`.**

*Semantics:*
- A story- or party-scope quest with `"credit": "share"` keeps each wolf's own amount in the copy: `c['shares'] = {wolf: n}`, the sum of that wolf's progress on counted objectives.
- Its rewards are given to each credited wolf with `{"share": true}` resolving to that wolf's amount (C1a).
- `"creditMin": 1-1e6` (optional) leaves out wolves whose share is below it.
- Without `credit`, rewards stay flat as today.

*Checks:* `credit` only with `scope` story or party; `{"share":true}` only inside that quest's `rewards` / `bonus`.

*Where:* `story_quests.py`: `check_quest` (136), `credit`/`credited`/`complete` (478–520), progress (560–590); `story_triggers.operand_type` (a `share` form allowed only with a reward context flag).

*Tests:*
- `test_story_quests.py`: three wolves add 5/3/1 to a story bar of 9; with `creditMin: 2` two are paid; coins `{"min":[20,{"times":[{"share":true},2]}]}`.
- `test_story_v2.py`: rank with ties (seeded), min, over list, into a world list.
- `test_story_native.py`: rank parity.

*Workbench:*
- `TriggerView.tsx` gains a "Rank" action card (through the catalogue).
- `QuestEditor.tsx`: "Reward each by their share" toggle, plus the least share.

*Guide:* §7 "Rewards in proportion"; §5 adds rank to the flow list.

### C12b. `match` and `field` with `starts` and `in`

*Catalogue:*
- The `field` condition's `op` values gain `starts` and `in`. For `in`, `value` is a list of up to 32 texts.
- The `trigger` section's help says a `match` value may be `{"starts": "<text>"}` or `{"in": ["a","b"]}` as well as a text.

*Semantics:*
- `starts` is a plain, case-sensitive prefix test.
- `in` is membership.
- The gift item rule (`story_program.matches`, 1134: base id unless a mark is given) applies inside both.
- Quest objective `match` and phase `exits.match` accept the same forms, because they go through `story_program.matches`.

*Where:*
- `story_program.matches` (1129);
- the condition check in `story_triggers` (`_check_condition`, `test_v2` classic `field`);
- `Dispatch::matches`, `Dispatch2::matches`, `Dispatch2::classic`;
- the game's own watched-event match: `Game::storyListens` / the watch filters in `RatwGameStoryFacts.cpp` take only exact `needsMatch` values, which is fine because `needsMatch` fields stay exact.

*Checks:*
- `starts` needs 1–120 characters; `in` needs 1–32 values of 1–120 characters each.
- Neither is allowed on a `needsMatch` field of a watched event (`purse crossed`, `standing crossed`, `near resident`): the game must know the exact value to watch.

*Tests:*
- `test_story_program.py`: a deed `detail` matched by `starts` ("stood watch at the ford").
- `test_story_native.py`: parity for match, field and objective.

*Guide:* §8 notes that a deed's detail ends with "(<deed id>)", so match it with `starts`.

### J4. The moon

*Catalogue:*
- The `calendar is` condition's `what` gains `moon`, with words "the moon". Its `value` gains `new`, `waxing`, `full` and `waning`.
- New event in group `time`:
```json
{"id": "moon", "group": "time", "title": "The moon turns", "sentence": "the moon is {detail}",
 "help": "The moon passed into another quarter of its round (detail: new, waxing, full or waning; quantity: the day of the moon's round, 0 to 29). Written only while a live story listens for it (the story's watch list).",
 "fields": {"detail": {"type": "text", "label": "new, waxing, full or waning", "any": "any"}, "quantity": {"type": "number", "label": "the day of the moon's round"}},
 "tile": false, "watch": true}
```
*Semantics:*
- The phase is taken from `calendar::calendarAt(days).moonPhase` (`RatwCalendar.cpp:86`), using the same octant as `moonName`:
  - octant 0 → `new`; 1–3 → `waxing`; 4 → `full`; 5–7 → `waning`.
- The world facts (`RatwGameStoryFacts.cpp:222`) gain `moon`.
- The `moon` event is written when the four-way phase changes. It is remembered from the first look, as `hour` is (`storyHour_` pattern at 286–304).

*Where:*
- `story_triggers.CALENDAR` (53) and `fact_test` (`calendar is`);
- `Dispatch2::fact`;
- `Game::storyWorldWatch` and the facts writer in `RatwGameStoryFacts.cpp`.

*Tests:*
- `Tests/story_facts_tests.cpp`: days 88.6 → new and 103.4 → full, with the event once per change.
- `test_story_facts.py`: the condition.
- `test_story_native.py`: parity.

*Guide:* §8 (watched events and facts).

### J6. Counting residents in a region

*Catalogue* (`count in region` gains two optional params):
```json
{"key": "who", "type": "choice", "label": "who is counted", "values": ["wolves", "residents"], "default": "wolves", "optional": true, "words": {"wolves": "of the story's wolves", "residents": "of the residents in"}},
{"key": "list", "type": "variable", "label": "the residents", "of": ["residents"], "optional": true}
```
The sentence becomes "{op} {value} {who} {list} stand in {region}".

*Semantics:*
- With `who: residents`, it counts the residents of `list` who stand in the region now. The facts' `residents` section gives each watched resident's tile.
- The runner adds the current values of every residents list named by such a condition to the phase's watch (`story_runner.watch`, 527). When that list changes, it sends the watch again. Lists are ≤ 500, so the watch stays bounded.
- A resident not in the facts is unknown and is not counted. If **every** listed resident is unknown, the whole condition is unknown.

*Checks:* `list` is required exactly when `who` is `residents`; the list must be a story or world `residents` list.

*Where:* `FACT_SHAPES` (59), `fact_test`, `Dispatch2::fact`; `story_runner.watch`.

*Tests:* `test_story_facts.py` (3 of 7 at the ropes); `test_story_native.py` parity; `test_story_facts_live.py` (the watch is re-sent when the list changes).

### J10. What Gift a wolf has

*Catalogue entry* (`conditions`):
```json
{"id": "wolf has", "v2": true, "facts": true, "title": "A wolf's Gift",
 "sentence": "{who}'s {what} is {value}",
 "help": "The Gift a wolf was made with (doc 43): its tier (normal, gifted or quickened) or its family. Never tell it in a story's words: test it, and offer what fits.",
 "params": [
  {"key": "what", "type": "choice", "label": "what", "values": ["tier", "family"], "default": "tier"},
  {"key": "value", "type": "text", "label": "is", "max": 40},
  {"key": "who", "type": "wolf", "label": "the wolf", "optional": true}],
 "reads": "wolves"}
```
*Semantics:*
- The facts' `wolves[id]` gains `gift: {tier, family}`.
- `tier` is `normal` when the character's saved gift is empty, and otherwise its tier (`identity::gift`). Quickened is read from the standing record (`quickenedAt >= 0`, `RatwStanding.h:34`), so a wolf Quickened in play counts.
- `family` is empty for normal wolves.
- The value is never shown to other players by the facts; the facts are runner-only.

*Checks:* for `tier`, the value must be normal, gifted or quickened; for `family`, one of the playable families (`Data/Gifts` family list, read the way `RatwGifts.h` reads it).

*Where:* `FACT_SHAPES`, `fact_test`, `Dispatch2::fact`; the facts writer (`RatwGameStoryFacts.cpp:120-185`).

*Tests:* `Tests/story_facts_tests.cpp`; `test_story_facts.py`; parity.

### J11. Operand `tally`

*Catalogue* (`types.operand` and `about`):
```json
{"tally": {"sum": "p_voice", "where": "p_backs", "is": "lamp", "over": "participants"}}
```
with `over` either `"participants"` or `{"list": "nh_pledge_coldwater"}`.

*Semantics:*
1. Over every wolf of `over` (no 500 cap; ≤ 5000), keep the wolves whose personal variable `where` equals `is`. Both `where` and `is` are optional; without them every wolf counts.
2. Add up their personal number `sum`. Without `sum`, count them.
3. A wolf with no stored value adds 0.
- It is pure computation, and counts as one operand.

*Checks:*
- `sum` must be a player or realm number.
- `where` must be a player or realm text or switch, and `is` a literal of that kind.
- It is not allowed inside `each`, nor two deep in arithmetic (it counts as one level).

*Where:* `operand_type`, `value_of`, `Dispatch2::value`; `story_native.fire` widens the counters (§0.3).

*Tests:* `test_story_v2.py` (1,200 wolves in a rehearsal cohort: sum and count, under the budget); `test_story_speed.py` (5,000 wolves × 9 tallies under 50 ms native); parity.

*Guide:* §6, "Counting over every wolf" (`tally` against `each`).

### C13b. `resident has post`

*Catalogue:* the `resident has` condition's `what` values gain `post`, with words "post". Help: "…or the post they hold (its title as the careers book has it, e.g. reigning at the Great Palace)".

*Semantics:* the facts' `residents[id]` gains `post`, the title of `Society::jobOf(id)`, or "" if none (`RatwGameStoryFacts.cpp`, residents section). Equality on text.

*Where:* `RESIDENT_HAS` (52), `fact_test`, `Dispatch2::fact`, facts writer.

*Tests:* `story_facts_tests.cpp`; parity.

### M2. Operand `purse` (Phase 2, with C1)

*Catalogue* (`types.operand` and `about`):
```json
{"purse": "town:coldwater"}
```
*Semantics:*
- The pennies the purse holds now: the account `Game::storyPurseAccount` maps it to (after SQ2, so `till:job:<id>` works too). A purse the facts do not know is `MISSING`, so an effect that reads it is skipped with a note (C1a rule 4).
- The runner adds every purse named by a `purse` operand to the phase's watch, as `purse crossed` already is. The facts writer (`RatwGameStoryFacts.cpp:403` already reads `society.account(storyPurseAccount(purse))`) reports `purses: {<purse>: cash}` beside `wolves` and `residents`.
- It is read-only. Arithmetic is C2's: `{"min": [12, {"div": [{"purse": "town:coldwater"}, 20]}]}` (not "divide").
- It **does not replace** the `nh-purses` counter rule (decisions §3.2): a reward scaled to the purse still adds to `coin_<purse>_day` and still respects the floor switch.

*Checks:* the value must pass `check_purse` (SQ2); `references()` checks the account exists (SQ2 rule).

*Where:* `operand_type`, `value_of`, `Dispatch2::value`; `story_runner.watch` (527); the facts writer.

*Tests:* `test_story_v2.py` (a reward of `min(12, purse/20)` at 212 gives 10; an unknown purse skips); `test_story_facts_live.py` (the purse is watched and reported); `test_story_native.py` parity.

---

## 3. The story's own parts (runner)

### R2. Readiness: when a chapter may move on, and the admin's Advance (Phase 1a)

The user's answer (a): progression is "participation … plus time, and progression would be pushed forward by an admin in DM/Story Maker". A phase says what "ready" means. The runner measures it and shows it in the console. When the phase is ready, it either moves on by itself (`auto`) or tells the admin, who presses **Advance**.

**What an admin can already do** (doc 85 as built, `tools/story_runner.py`):
- Commands go through the inbox (`command()`, 2966; `COMMAND_KINDS`, 2651), each with a reason, audited, with a dry run (`explain` with `command`, 4402):
  - `hold` and `resume` (with or without catch-up), and `hold-all`;
  - `stop`;
  - `enter`, which goes to **any** phase, on, back or again, with or without its entry effects (`do_enter`, 3255);
  - `signal`, which fires a `dm signal` direction exit;
  - `fire`, `enable`, `disable`, `set`, `timer`, `mission`, `objective`, `quest`, `participant`, `tell`, `scene`, `order`, `migrate`.
- Exits already gate on participation of a kind: `players` or `percent` of the **cohort** who did the exit's event (`story_program.exit_for`, 1252), `prerequisites`, missions and `afterDays`.

**What is missing**:
- a share of the **active** wolves (R1);
- a least time combined with a share (today a `dm signal` exit cannot carry `afterDays`: `_check_exit`);
- a measured state the console can show as bars;
- a "ready, waiting for you" alert (today a phase that only an admin can move is reported `stuck`);
- an Advance that names **the** way on, instead of "Go to phase…" with any phase in a list.

**Format** (a phase field, version 2 stories; `Docs/story-programs.md` gains "Readiness"):
```json
{"id": "ch1_main",
 "ready": {
   "all": [
     {"participation": {"list": "ch1_did"}, "of": "active", "percent": 35, "floor": 5, "cap": 120, "label": "Wolves who did the chapter's work"},
     {"minDays": 10, "since": "chapter", "label": "Days in the chapter"},
     {"value": {"var": "nh_summons"}, "atLeast": 9, "label": "Summons carried"},
     {"missions": ["find_king"], "label": "The King found"},
     {"any": [{"quest": "alder_mere", "label": "The camp broken"}, {"minDays": 20, "label": "or twenty days"}], "label": "The camp, or time"}
   ],
   "auto": false,
   "then": {"on": "day begins"},
   "slowAfterDays": 30
 },
 "exits": [{"on": "ready", "to": "ch1_end"}, {"on": "dm signal", "to": "ch1_end", "match": {"detail": "Herald's Word now"}}]}
```

*The parts* (`all`: 1–8; a part may be `{"any": [2–4 parts], "label"}`, one deep). Each part has an optional `label` (≤ 80 characters; the default is made from its kind):
- `participation`: R1's condition fields (`list` or `where`, `of`, `percent`, `floor`, `cap`). Its progress is |S| of need, and |S| / |D| as a share.
- `minDays`: 0–3650 game days, `since` `"phase"` (this visit, the default) or `"chapter"` (the run's first visit: `state['opened']`, set at launch). Time while **held** does not count (rule 8). Its progress is days of minDays.
- `value` with `atLeast`: two number operands (no facts, nothing personal: story or world variables, `count`, `audience`, R1, arithmetic). Its progress is value of atLeast. Alternatively `test`: a condition tree under the same rule, which is only met or not met.
- `missions`: the phase's missions are complete (`rules.mission_snapshot` ready). Its progress is k of n.
- `quest`: a story-scope quest is `done`. Its progress is its objectives' n of goal.

*The block's other fields:*
- `auto` (default **false**): whether the exit is taken by itself when the block is ready.
- `then` (optional): the moment it waits for once ready, given as an event `{on, match}`. The events allowed are `day begins`, `hour`, `season begins`, `festival` and `dm signal`; with J4 (Phase 2) also `moon`, and with C13a (Phase 4) also `month begins`. Example: the next new moon is `{"on": "moon", "match": {"detail": "new"}}`, once J4 lands; until then `day begins` plus a `calendar is` part. (The Ninth Howl times its set pieces in its closing phases instead, decisions §6.3.)
- `slowAfterDays` (optional, 1–3650): after that long in the phase (held time not counted) without being ready, the console says so.
- `slowNeedsYou` (optional, default false; decisions §6.2): when true, the `slow` alert is "needs you" (it counts on the Campaigns tab and goes to the Dev Console) and reads "Ready to push: <the furthest part>. Advance anyway, or wait." Every Ninth Howl chapter sets it.
- `autoAfterDays` (optional, 1–3650, only with `auto: false`): once ready and not advanced for that long, it goes by itself. This is for a world with no admin about. There is no default.

*The exit:* `{"on": "ready", "to": "<phase>"}`. A phase has it exactly when it has `ready`, and at most one. The phase's other exits keep working beside it, so a branch exit or a `dm signal` may still fire first.

**Per chapter.** A chapter is one program of a `campaign.approve` campaign (doc 85, Phase 10). Its readiness is that of the phase it is in. A chapter whose work spans several phases puts `ready` on each, with `since: "chapter"` for the time. The chapter's ending phase starts the next chapter with `story.start {when: "ended"}` as today, so Advance on the last working phase moves the campaign to its next chapter.

*Semantics (the runner; `story_runner.readiness(program, state, day, x)`, new):*
1. **Who evaluates.** The runner does, in Python, through `story_triggers.value_of` and `test_v2`. R1's Python twin is held to the native engine by parity (`test_story_native`), so readiness needs no C++ path of its own.
2. **When.** On each tick in which the run read events or a command changed it, at least once a game hour, at most once a tick, and on `day begins`. A **held** run is not evaluated (it is frozen). A paused run (a refused order) is evaluated but does not move.
3. **State.** `state['ready'] = {phase, visit, met, metAt, pushed, parts: [{kind, label, met, value, need, of, share, days}], checkedAt, auto, then, waitingFor, slow, to}`. It is bounded (≤ 8 parts plus 4 nested). Each change of `met`, and each push, is appended to `state['readyLog']` (the last 20) and shown in the timeline as a `ready` item, for example "Ready: 42 of 118 active wolves (36%; 35% and 5 needed), 11 of 10 days".
4. **Latched.** Once every part is met, `metAt` is set and stays set for the visit. A crowd arriving later and lowering the share cannot take it back, and the bars keep moving for the eye.
5. **Met, `auto: true`.** With no `then`, the exit is taken in the same tick (`rules.leave_visit`, with cause `ready`): missions are kept, and the visit's timers and once-marks are dropped, as for any exit. With `then`, the `ready` exit waits for that event (`waitingFor`) and is taken when it comes.
6. **Met, `auto: false`.** The run is **not** moved. Alert `ready` (needsYou **true**): "Ready to advance: ch1_main → ch1_end, since day 93 (all 4 parts met)". With `autoAfterDays`, it moves by itself that many days after `metAt`, with cause `ready (no admin)`.
7. **Not met, after `slowAfterDays`.** Alert `slow` (needsYou false, or true with `slowNeedsYou`), naming the furthest part: "Slow: 21% of the 35% needed after 31 days; Advance anyway, or wait".
8. **Hold freezes the clock.** `do_hold` (3130) records `atDay` in `state['held']`. `do_resume` (3138) adds the held span to `state['heldDays']` (per visit and per run). `minDays`, `slowAfterDays` and `autoAfterDays` count only running time. The console says "Holding stops the chapter's clock".
9. **Command `advance`** (new in `COMMAND_KINDS`, `FLOW`, `RELEASING` and `VISITED`), payload `{"when": "now" | "then", "anyway": bool}`:
   - It is refused when the phase has no `ready` exit ("This phase has no Advance: use Go to phase…").
   - Not met and not `anyway`: refused with "Not ready yet: 21% of the 35% needed; 31 of 10 days. Advance anyway?". `anyway` needs the story's title typed in the console (`typedTitle`, as for going back).
   - `when: "now"`: takes the `ready` exit now (`Steer._transition(to, '')`, cause `advance`), the same as R2 rule 5 but at once.
   - `when: "then"` (only with `then`): sets `pushed = {by, reason, day}`, which counts as met, and waits for the anchor.
   - The result's before and after hold the readiness snapshot (every part's progress), so the audit row (`dm.story_commands`, `dm.audit`) records what the admin saw and why they pushed. The reason is required (`_reason`).
   - `_may_release` (5.7) applies, because it enters a phase. The dry run (`explain` with `command`) shows the destination's entry effects.
10. **Explain** (doc 85, Phase 3):
    - `listeners()` lists the `ready` exit with its parts, whether it is met, and `can` (true with `auto`, `then` or `autoAfterDays`; otherwise "waits for an admin");
    - `_waiting` gets `{on: "ready", ready, to}`;
    - the `stuck` alert is **not** raised for a phase whose way on is a `ready` exit: it says `ready` or `slow` instead.
11. **Overview.** `_row_view` adds `ready: {met, auto, to, metAt, waitingFor, parts: [{label, met, value, need, share}]}` for the list and the chapters bar.
12. **Moving a run onto a new version** (doc 85, 5b): readiness is evaluated afresh for the new version. `metAt` is kept when the phase and its `ready` block are unchanged (compared by digest).

*Checks* (`story_program`):
- the `ready` shape (`_check_phase_head`), with parts and bounds as above;
- R1's rules for `participation`;
- `value` and `test` hold no facts and nothing personal;
- `missions` names the phase's own missions; `quest` names a story-scope quest;
- `then.on` is one of the allowed events, and its `match` uses `FIELDS`;
- the `ready` exit is there exactly when `ready` is, with the shape `{on, to}` only (`_check_exit`);
- `_check_phase_paths`: a `ready` exit has no acting wolf, so a phase with `$actor` rewards may not be entered by it;
- `triggers.routes` includes it, for the graph check.

*Rehearsal:*
- a case step `{"check": "ready", "met": true}` asserts the block's state;
- `{"advance": "now"}` takes it (as the command would);
- `review(p)` reports a `ready` block that no case exercises as uncovered.

*Where:*
- `tools/story_program.py`: `_check_phase_head`, `_check_exit`, `_check_phase_paths`, `exit_for` (it skips `ready`, which the runner takes), and rehearsal;
- `tools/story_runner.py`:
  - `readiness` (new) and its call in `_tick_run` (1580);
  - `launch` (`state['opened']`);
  - `_waiting` (2075) and `_alerts` (2163; kinds `ready` and `slow`);
  - `_row_view` (2233);
  - `controls()` (gains `ready`);
  - `COMMAND_KINDS`, `_check_payload` and `Steer.do_advance`;
  - `do_hold` and `do_resume` (the held days);
  - `listeners()` and the stall test;
  - `do_migrate` (rule 12);
- the catalogue's `trigger` section: the exit kind `ready` is described.

*Tests:*
- `test_story_program.py`: two `ready` exits refused; `ready` without its exit refused; a fact in `value` refused; a `$actor` reward behind a `ready` exit refused; `then: {"on": "moon"}` refused before J4.
- `test_story_v2_runner.py` (DSN), on a chapter with `participation` 35% floor 3 and `minDays` 2:
  - 4 of 10 active on day 1: not met;
  - day 2: met, then latched when 6 more wolves become active;
  - with `auto: false`, alert `ready` and no move;
  - with `auto: true`, it moves in the same tick;
  - with `then: day begins`, it waits for the day;
  - 3 held days are not counted;
  - `slow` after `slowAfterDays`; `autoAfterDays` moves it;
  - after `migrate`, `metAt` is kept when the block is unchanged.
- `test_story_steering.py`: `advance now` moves, and the command row holds the snapshot and the reason; not met without `anyway` is refused; `anyway` moves; `when: then` waits; no `ready` exit is refused; `_may_release` refuses a releasing advance for an admin without the right.
- `test_story_explain.py`: the `ready` listener; no `stuck` on a ready phase; the dry run of `advance`.
- `test_story_campaigns.py`: Ch 1 ready, then Advance, then its ending phase, then `story.start` Ch 2 (`audience: "same"`, `carry: true`), whose `seen` is seeded (C2).
- `test_story_speed.py`: readiness over 5,000 wolves with two participation parts stays inside the tick budget once a game hour.

*Guide:*
- §4 "Building a campaign" gains "Chapters that move on when the wolves have done enough", with the block above as the template;
- §11 gains "Moving a chapter on" (R3);
- §15 gains "It says ready but did not move: auto is off; press Advance";
- §16 gains the limits.

*Catalogue:* `npc.offer` help adds: "Offered again to another wolf with the same name, it is added for them; each wolf sees it until they pick it (once) or the visit ends."

*Semantics:*
- In `story_quests.py:706-710`, an offer with a `player` is now stored as `players: [..]`. Applying `npc.offer` with the same `id` and a `player` **appends** that wolf to `players` (a set; ≤ 5000) instead of replacing the entry.
- `words` and `reply` are taken from the latest application.
- An offer with no `player` is for everyone (`players` absent), and replaces as today.
- `choices()` (896) sends `players` as the list.
- The game already reads `players` (`storyChoicesFor`, `RatwGameStorylines.cpp:1180`).

*Migration of a live run:* an older state's `player` is read as `players: [player]` (in `story_quests` load).

*Tests:* `test_story_quests.py`: wolf A then wolf B are offered the same id, and both see it; A picks it (once) and B still sees it. `Tests/dialogue_tests.cpp` storyQuestsInTheWindow: the `players` list.

*Guide:* §7, "A choice for one wolf".

### J2. Realm variables (`scope: "realm"`)

*Catalogue* (`variables.scopes`): add `"realm": "each wolf's own, kept after the story and shared by every story of the world that declares it"`.

*Semantics:*
- One value per wolf per name, shared by every story that declares the same name and type in `realm` scope. It behaves like `player` everywhere (`of: $actor|$each`, trackers, `rank`, `tally`, `carry` not needed).
- Storage is a new table (migration **0081**):
  `dm.story_realm_vars(world_id text, name text, wolf text, type text, value jsonb, run text, updated_at timestamptz, PRIMARY KEY (world_id,name,wolf))`, plus an index on `(world_id, updated_at)`.
- **Loading:**
  - At launch, the runner loads the cohort's values for the declared names into `state['counters']['realm'][wolf]` (one query).
  - At the start of each tick, it reloads only rows changed since the run's cursor (`updated_at > state['realmSeen']`) for its names.
  - A wolf who joins or arrives is loaded then.
- **Saving:** at the tick's end, `world_save` writes only the changed wolf-name pairs. Runs tick one after another (`tick`, 1767), so no update is lost between runs. Within one run, the in-memory value is the truth.
- Realm values survive the run, as world values do. The console's World variables dialog gets a "per wolf" tab: a filter by name and wolf, set with a reason, audited like `world-var`.

*Checks:*
- Same name, same type, as world variables are checked (`references()`, 88–96, extended to realm).
- `show` is allowed (personal tracker).
- `initial` applies to a wolf with no row.

*Where:*
- `story_triggers.SCOPES`/`PERSONAL`/`var_store` (885);
- `story_runner.world_load`/`world_save` (921–957), plus a `realm_load`/`realm_save` pair;
- `launch` (366); `_carried` (realm is never carried; it is shared);
- `story_quests.trackers`;
- `Dispatch2::store`/`writable`;
- `Editor/src/dm/triggers/Variables.tsx` (scope chip "each wolf, across stories");
- the campaigns console World variables panel.

*Tests:*
- `test_story_v2_runner.py` (`RATW_STORY_TEST_DSN`): two runs that declare `p_voice` realm both add, and both totals are kept; a reload picks up the other run's write; a type clash is refused.
- `test_story_campaigns.py`: Act I sets it, Act II reads it.
- `test_story_native.py`: parity of the scope.

*Guide:* §6, a fifth scope; §4, "Across campaigns".

### J3. Quest `closure` (Quickening count; answer (b))

The user's answer (b): finished story quest lines count toward the Quickening's `closedStories`, **capped**. Only the last chapter of a large line or a campaign side story counts, at most 4 per program.

*Semantics:*
- A quest may carry `"closure": "line"` or `"closure": "side"`. `true`, which the packs' today versions already carry, is read as `"line"`.
  - **`line`**: the last chapter of a large quest line (`quests-large.md`; one line is one program). The quest must **end a chain**: it `requires` (C6) `quest is done` of another quest of the program, and no quest of the program requires it to be done. A line's earlier chapters never count.
  - **`side`**: the closing quest of a campaign chapter's side storyline (`campaign.md` §0, "3 to 5 per chapter"). It is allowed only in a program approved as a chapter of a `campaign.approve` campaign. A chapter's main-line quests and its door quest for late joiners are not side stories and may not carry it.
- Either kind must be `scope` player or party, and `repeat: once`. **A program has at most 4 closure quests**, of both kinds together.
- When a wolf's copy completes, the runner adds the game order `story.closure` (target: the wolf; payload `{run, story: program id, quest, kind, title}`) to the reward batch.
- The game pays one social-ledger entry, reason `story_closure`, with session `quest:<program id>/<quest id>`.
  - It is keyed by program id, not run, so a re-run or a new version never counts twice.
  - It uses a deterministic `std::hash` seed (as `RatwSocialCore.cpp:1284`).
  - The amount is **3** social XP (D1's recommendation, adopted with answer (b); the user may change it in `Data/Social`, where the other closure amounts live).
- **The game holds the cap too.** `Game::storyClosure` refuses a fifth `story_closure` entry for the same wolf and the same program id (sessions `quest:<program id>/…`), whatever the versions say. So a new version that renames its quests cannot pay more than 4. The receipt's result reads "already 4 from this story", and the order is applied (not refused), so the run does not pause.
- `Game::standingMeasures` (`RatwGameSocial.cpp:955-981`) already counts distinct sessions with that reason into `closedStories`, so nothing else changes there.
- The entry is permanent and never undone, because it is a record of a finished story.
- *Note on scale:* the cap is per program, as the user said. Each campaign chapter is its own program, so one chapter's side stories alone can bring a wolf to the Quickening's `closedStories: 2`. That is by design: a side story is a finished story.

*Checks:*
- `story_quests.check_quest`: the field's values, the scope, `repeat`, at most 4 per program, and the `line` chain rule (C6 is built in the same phase);
- `story_runner.references` and the campaign approval (`campaign.approve`): a `side` closure in a program that is not a campaign chapter is refused ("A side story counts only in a campaign chapter").

*Catalogue:* no effect entry: `story.closure` is the runner's own order, like `story.finish`. The `quest completed` event help adds "(a closure quest also counts as a finished Story for the wolf)".

*Where:*
- `story_quests.check_quest`, `complete`;
- `story_runner` (the reward batch, `order_of`; the campaign approval's check);
- `Core/RatwGame.cpp` dispatch;
- a new `Game::storyClosure` in `RatwGameSocial.cpp` that counts the wolf's entries for the program and calls `social_.pay(...)`.
- Migration **0082**: none is needed if the ledger is checkpoint-only. If `game.social_entries` is a table, add an index on `(reason, session)`.

*Tests:*
- `Tests/storyline_tests.cpp`: one receipt per wolf per quest, and `closedStories` rises by 1; a fifth closure of one program for one wolf pays nothing, says so, and `closedStories` stays; a second program pays again.
- `test_story_quests.py`: the order is sent once; a party copy gives each member one; `true` is read as `line`; a `line` quest that does not end a chain is refused; a fifth closure quest is refused.
- `test_story_campaigns.py`: a `side` closure is approved in a campaign chapter and refused in a lone program.

*Guide:* §7, "Quests that count as a finished Story": the two kinds, the cap, and that **game text never claims it** (the Quickening checklist shows the count).

### C6. Quest `requires` and `offer.during`

*Semantics, `requires`:*
- A quest may have `"requires": <condition tree>`, using v2 conditions over variables and `quest is` / `objective is`. **Fact conditions are refused**, because facts are per event and offers are not.
- The runner evaluates it whenever it builds `offers()` (`story_quests.py:883`):
  1. If the tree reads nothing personal (no `of`, no personal variable, no `quest is ... of:$actor`), it is a gate for everyone: false means the offer is not sent.
  2. Otherwise, it is evaluated for each cohort wolf with `$actor` = that wolf, and the offer is sent with `players: [...]` (the wolves for whom it is true).
  3. A wolf outside the cohort never meets a personal requirement.
- The game's `storyOfferOpen` (`RatwGameStorylines.cpp:1132`) adds: if `offer.players` is an array and the wolf is not in it, false. The board view does the same.
- `quest.offer`, `quest.start` and taking a quest from a board check `requires` for that wolf, and refuse with the toast "Not yet." when it fails.
- *Cost:* at most 32 quests × 5000 wolves per sync. Syncs happen only when the fingerprint changes. The `requires` results are added to `fingerprint()` (930) as the sorted players lists.

*Semantics, `offer.during`:*
- `offer` gains `"during": text ≤ 300 (symbols allowed)` and `"duringAsk": text ≤ 120` (default "About <title>…").
- While the wolf's copy of that quest is `active` (not ready), the giver's hub shows one choice with `duringAsk`. Picking it shows `during` in the giver's voice, then returns to the hub. It raises the watched `dialogue node` event (`@story:<run>:during:<quest>`) and no story event.

*Checks:*
- `check_quest`: `requires` goes through `_check_condition` with a phase context with no facts.
- `during` needs a `giver`.
- `story_runner.references`: nothing new.

*Where:*
- `story_quests.check_quest`, `offers`, `fingerprint`, `start`/`offer`;
- `RatwGameStorylines.cpp` `storyOfferOpen` and `storyChoicesFor` (the `during` choice reads the wolf's view: status `active`, giver = npc);
- `RatwGameWindowTalk.cpp:370-399` (node `@story:<run>:during:<quest>`; the reply in voice through `inVoice`, 847).

*Tests:*
- `test_story_quests.py`: a chain quest whose giver offer shows only to the wolf who finished part 1; a global gate on `nh_stage`.
- `Tests/dialogue_tests.cpp`: the `players` check; the during choice and its line.
- `Client` needs no change (it is an ordinary window node).

*Workbench:* `QuestEditor.tsx` gets a "Only for wolves who…" condition box (the existing `TriggerTree` condition editor) and a "While under way, the giver says…" field.

*Guide:* §7, "Chains: requires" and "The giver while under way". Remove "Not built: a requires quest X field" from §16.

### C7. Casting at run time: `cast.pick`, and a giver from a variable (Phase 9)

*Catalogue entry* (effects; story):
```json
{"id": "cast.pick", "group": "residents", "title": "Cast a resident", "v2": true, "story": true,
 "sentence": "Choose a resident {job} {town} {faction} {quarter}, not in {notIn}, near {near}, as {var}",
 "help": "The story looks for a living, placed resident who fits (their work, their town, their House, their quarter), leaves out those in a list, prefers the one nearest a wolf, and puts them in a resident variable. Then 'cast chosen' happens (or 'cast failed' when nobody fits). Use it for quests any town can give.",
 "params": [
  {"key": "var", "type": "variable", "label": "into", "of": ["resident"]},
  {"key": "job", "type": "text", "label": "whose work is", "max": 40, "optional": true},
  {"key": "town", "type": "town", "label": "in the town", "optional": true, "operand": true},
  {"key": "faction", "type": "faction", "label": "of the House", "optional": true},
  {"key": "quarter", "type": "text", "label": "in the quarter", "max": 40, "optional": true},
  {"key": "notIn", "type": "variable", "label": "not one of", "of": ["residents"], "optional": true},
  {"key": "near", "type": "wolf", "label": "nearest to", "optional": true}]}
```
*New events (`story`):*
- `cast chosen`: `target` = the resident; `detail` = the variable; `actor` = the `near` wolf or "".
- `cast failed`: `detail` = the variable.

*Semantics:*
1. The engine appends a cast request to `state['castRequests']`.
2. The runner answers it in the same tick from `dm.story_residents` joined with the careers posts the checkpoint holds (`game.story_facts` residents section, or a new `dm.story_posts` view). It keeps only alive and placed residents (C7 rule).
3. It orders the candidates by distance to `near`'s tile (from facts) or by seed, writes the variable, and dispatches `cast chosen` / `cast failed` as a story fact.
4. Quest `giver` may be `{"var":"<resident var>"}` (story scope). `offers()` reads it each sync, so the offer moves when the variable changes. A copy keeps the giver it started with for its hand-in (`c['giver']`), and `view()` sends that giver.

*Checks:*
- At least one of job, town, faction or quarter.
- A giver variable must be a declared story-scope resident; `references()` skips it.

*Where:* `story_quests.apply` (story effects), `offers`, `view`; `story_runner._tick_run` (answer requests); `story_triggers` (events in the catalogue).

*Tests:* `test_story_quests.py` (a carter cast in Larkfield gives the quest; recast when they die); `test_story_v2_runner.py` (the query, the unplaced left out).

*Guide:* §7, "Quests any town can give".

### C12a. `story.notify` and the event `story notified`

*Catalogue entry* (effects; story):
```json
{"id": "story.notify", "group": "chapters", "title": "Tell another story", "v2": true, "story": true,
 "sentence": "Tell {story} the news {signal} about {player}, worth {amount}",
 "help": "Another live story of this world hears 'story notified' with this name, the wolf and a number, between two of its events. With no story named, every live story listening for that name hears it. A quest pack uses it to credit a wolf in whichever chapter of a campaign is running.",
 "params": [
  {"key": "signal", "type": "signal", "label": "the news (its name)"},
  {"key": "player", "type": "wolf", "label": "about the wolf", "optional": true, "words": {"": "nobody in particular"}},
  {"key": "amount", "type": "int", "label": "a number", "min": -1000000, "max": 1000000, "optional": true, "operand": true},
  {"key": "story", "type": "story", "label": "the story", "optional": true, "words": {"": "every story listening"}}]}
```
*New event* (group `story`):
```json
{"id": "story notified", "group": "story", "title": "Another story sends news",
 "sentence": "{target} sends the news {detail} about {actor} ({quantity})",
 "help": "A story's 'Tell another story' reached this one: detail is the news's name, actor the wolf it is about (may be empty), quantity its number, target the sending story's id.",
 "fields": {"actor": {"type": "wolf", "label": "the wolf it is about", "any": "any wolf"}, "target": {"type": "story", "label": "the story that sent it", "any": "any story"}, "item": {"type": "run", "label": "its run", "any": "any run"}, "detail": {"type": "signal", "label": "the news", "any": "any"}, "quantity": {"type": "number", "label": "its number"}},
 "tile": false, "story": true, "needsMatch": ["detail"]}
```
*Semantics:*
1. The story effect is queued in `state['notices']`.
2. At the end of the tick, the runner delivers each notice exactly as `tell_stories` (`story_runner.py:1157`) delivers `story started`: a `story-event` command into each target run's inbox. The targets are:
   - every other live run whose program listens for `story notified` with `match.detail` equal to the signal (`_listens`, extended to check the match);
   - or only the run of `story`, if one is named.
3. The fact carries `{kind, actor, target: sender program id, item: sender run, detail: signal, quantity: amount}`.
4. At most **200 notices per run per tick**; the rest wait for the next tick (kept in state).
5. A wolf named who is not in the receiver's cohort still reaches world-audience triggers, and is ignored by participant triggers (the existing rule).
- `once: member` on the receiver's trigger guards against double counting.
- A notice's `actor` is the wolf, so `player`/`$actor` effects in the receiver act on that wolf (participant triggers only, as today).

*Checks:*
- `story`, if given, must be a program id known in the world: checked at approval against `dm.story_releases` program ids (approved or running). It is a warning if the story is not approved yet, because a pack may be approved first.
- `story notified` is allowed as a rehearsal fact (unlike `story signal`).

*Where:* `story_quests` story-effect apply; `story_runner` (`tell_stories` generalised to `deliver_story_event`; `_listens`); catalogue.

*Tests:* `test_story_campaigns.py`: a pack's quest completed notifies, and the running chapter's `p_voice` rises; with no listener nothing is sent; the cap of 200 carries over.

*Guide:* §4, "Quest packs that feed a campaign".

### C13a. Events `variable changed`, `month begins`, `succession`

*Catalogue entries:*
```json
{"id": "variable changed", "group": "story", "title": "A world variable changes",
 "sentence": "the world's {target} becomes {detail}",
 "help": "Another story changed a world variable this story declares: target is its name, quantity the new number (for a number), detail the new value as text, item the run that changed it. Told once a tick at most, with the latest value.",
 "fields": {"target": {"type": "variable", "label": "the variable", "any": "the variable named"}, "detail": {"type": "text", "label": "its new value", "any": "any"}, "quantity": {"type": "number", "label": "its new number"}, "item": {"type": "run", "label": "the run that changed it", "any": "any run"}},
 "tile": false, "story": true, "needsMatch": ["target"]}

{"id": "month begins", "group": "time", "title": "A new month begins",
 "sentence": "month {detail} begins",
 "help": "The first day of the world's 28-day month (the economy's: doc 42). detail: the month's number in the year, quantity the same. Written only while a live story listens for it.",
 "fields": {"detail": {"type": "text", "label": "the month's number", "any": "any"}, "quantity": {"type": "number", "label": "the month's number"}},
 "tile": false, "watch": true}

{"id": "succession", "group": "town", "title": "A post changes hands",
 "sentence": "{actor} takes up {detail} after {target}",
 "help": "A resident took a post by the careers book's rules (a death, a retirement, a vacancy filled, a story's npc.post): actor the new holder, target the one before (may be empty), detail '<post title>: <how>'.",
 "fields": {"actor": {"type": "resident", "label": "the new holder", "any": "anyone"}, "target": {"type": "resident", "label": "who held it before", "any": "anyone"}, "detail": {"type": "text", "label": "the post and how", "any": "any"}},
 "tile": false}
```
*Semantics:*
- **`variable changed`:** in `world_save`, each world variable that changed is queued. After the run's tick, the runner delivers it to every **other** live run that declares that world variable and listens for `variable changed` with `match.target` = the name. It is coalesced: one delivery per variable per receiving run per tick, carrying the latest value.
  - A receiver never hears its own writes.
  - Cycles (A wakes B, B writes back) settle because nothing is redelivered within the tick, and each delivery is one fact.
- **`month begins`:** the game's watch writes it in `RatwGameStoryFacts.cpp` next to `day begins`. The month is `floor(dayOfYear / 28) + 1`, taken from the economy calendar the town purses use (doc 42; `calendar::` helper).
- **`succession`:** the game already records `succession` notes (`RatwCareers.cpp:1259`, 1594, in game.events through the careers notes path). The only work is the catalogue entry and adding `"succession"` to the game's event allow-list for story intake, if it is filtered.

*Checks:* `variable changed` with `match.target` must name a declared world variable.

*Where:* `story_runner.world_save` plus a delivery step (shared with C12a); `RatwGameStoryFacts.cpp` (month); the catalogue.

*Tests:* `test_story_v2_runner.py` (a pack adds to `nh_realm`, and the chapter's trigger fires the same tick sequence); `Tests/story_facts_tests.cpp` (month); `test_story_catalogue.py::test_every_event_is_described`.

*Guide:* §8; §4 replaces "poll on `hour`" with "listen for `variable changed`".

### N3. A tracker that mirrors a world variable

*Semantics:*
- A **player**-scope `number` declaration may add `"mirror": {"key": "<player text variable>", "names": {"<value>": "<world number variable>", ...}}` (≤ 16 names). It needs `show`.
- Its value is never stored. `story_quests.trackers` (855) computes it at each sync: for the viewing wolf, read `key` and look up the world variable named for that value. An unknown key or an empty value shows 0.
- Triggers may read it like any variable (`value_of` resolves it the same way); it can't be set.

*Use:* "Your town's howl" = `{"key":"p_howl","names":{"ridgemere":"nh_voice_ridgemere", ...}}`, with no hourly `each` over 5000 wolves.

*Checks:*
- Every name must be a declared world number.
- `key` must be a player or realm text.
- `set`, `add` and the other writes on a mirror are refused.

*Where:* `story_triggers` (declaration check, `value_of`, `Dispatch2::var`); `story_quests.trackers`; `Variables.tsx`.

*Tests:* `test_story_v2.py`; `test_story_quests.py` (two wolves pledged to different towns see their own); parity.

### Quest objectives: SQ1 `where`, SQ5 `credit`, L3 more mission events (Phase 2)

These three change one thing, what a quest objective may count, so they are built and tested together in `story_quests._check_objective` (89) and `story_quests.track` (535).

**SQ1. `where` on an objective.**
```json
{"id":"birds","label":"Take two pheasants in the Rainward Woods.","on":"hunted","match":{"target":"pheasant"},
 "where":{"cells":["w_ridgemere_2_0","w_ridgemere_2_2"]},"goal":2}
```
- `where` is `{"cells": [1–16 cell ids]}` or `{"region": "<story region>"}`.
- `track` tests it after `rules.matches`: with `cells`, the event's `cell` must be one of them; with `region`, the event's tile must be inside the region's rectangle (an event with no tile is tested by its cell: inside if the region's cell is the event's cell).
- An event with neither `cell` nor tile never counts for a `where` objective.
- *Checks:* each cell must be in `world.cells` (`references()`, beside the existing `match.cell` check, `story_runner.py:150`); a region must be the program's own.
- *Fallback until Phase 2:* a trigger on the event, `if any [field cell = …]`, then `objective.progress amount 1` (small P4/P6 style).

**SQ5. `credit` on an objective.**
- `"credit": "actor"` (the default, today's rule), `"target"` or `"present"`.
- `target`: the wolf credited is the event's `target`. Allowed only on events whose target is a wolf: `stall sale` (the stall's keeper), `vouch`, `told the wardens`, `vouched to the wardens`. So "sell three things at your stall" can tick.
- `present`: every cohort wolf standing in the event's `cell` when it is written is credited. It is for events with no wolf at all: `caravan arrives` (actor the wagon, `cv<n>`), `ship docks` / `ship lost` (actor the ship), `project built` (actor `town:<id>`), `festival` (actor the town). The runner adds the wolves section to the phase's watch while such an objective is under way (as `wolf in region` does). At most 500 wolves are credited by one event.
- *Checks:* `target` only on the four events above; `present` only on events with a `cell` field.
- *Fallback until Phase 2:* `trade done` with the provisioner (small #10); a trigger with `each over participants` + `wolf in region {who:$each}` + `objective.complete`.

**L3. Six more mission events.**
- `camp cleared`, `caravan arrives`, `ship lost`, `ship docks`, `project built` and `festival` get `"mission": true` in the catalogue, with the fields they already have. `rules.MISSION_EVENTS` is read from the catalogue, so `story_quests` takes them as any other.
- Who is credited:
  - `camp cleared`: its `actor` when that is a wolf ("by the watch" credits nobody); use `credit: "present"` to credit the whole band in the cell;
  - the other five: they never have a wolf as actor, so their objectives **must** carry `credit: "present"` (the validator refuses them otherwise, with the words "Nobody is the actor of a wagon arriving: credit the wolves present").
- Programs move from the trigger + `objective.complete` pattern (decisions §4.2) to plain objectives **in a new version**, at a chapter boundary.
- *Fallback until Phase 2:* a trigger on the event (match `cell`, story switch `spawned_<region>` on) that does `objective.complete` for each participant `wolf in region` (decisions §4).

*Tests:* `test_story_quests.py`: a `where` objective ignores a pheasant in 2_1 and counts one in 2_2; a `stall sale` credits the keeper with `credit: target`; a `caravan arrives` credits the two wolves in the cell with `present` and not the third elsewhere; `caravan arrives` without `credit` is refused; `camp cleared` by a wolf credits that wolf. `test_story_problems.py`: an unknown cell in `where` is refused (DSN). `test_story_catalogue.py`: the six are mission events.

*Workbench:* `QuestEditor.tsx` objective row: "only in…" (a cell or region picker) and "who is credited" (the actor / the one it was done for / every wolf there).

*Guide:* §7, "Where an objective counts" and "Who an objective credits".

### N4. Quest `join.again`: a wolf's wait before trying again (Phase 2)

*Semantics:*
- A quest's `join` gains `"again": 1–365` (game days).
- Today `story_quests.again` (349) gives a wolf who **failed or abandoned** the quest the day it ended, so they may retake it at once; only "done" waits for `repeat`. With `join.again`, every ending sets the wolf's next start no sooner than `ended + again`. For "done" it is the later of that and the `repeat` gap.
- The offer and the board show "Not yet." to that wolf until then (the existing `again` view field, `view` 831).
- It is per wolf. `join.cap` and `join.limit` stay as they are.

*Use:* the `join.cap: 1` quests (small #3, #23, #25, #30; medium Q4, Q17, Q18) get `join.again: 7`, so one wolf cannot hold the only copy every week (decisions §8.1).

*Checks:* `rules.shape(join, (), ('who', 'region', 'cap', 'limit', 'again'))` (172); an integer 1–365.

*Fallback until Phase 2:* `repeat: weekly`, a `fail: {afterDays: 1}` deadline, and an "it's taken" helpful line at the giver's town (decisions §8.1).

*Tests:* `test_story_quests.py`: a wolf who abandons waits 7 days; a wolf who finishes a `repeat: daily` quest with `again: 3` waits 3.

### L5. `quest.withdraw` (Phase 4, optional)

*Catalogue entry* (effects; story):
```json
{"id": "quest.withdraw", "group": "quests", "title": "Take back an offer", "v2": true, "story": true,
 "sentence": "{quest} is no longer offered to {player}",
 "help": "Takes an offered (not yet accepted) quest back from one wolf, with no 'failed' mark in their journal. Use it when a wolf takes one road and the others should close.",
 "params": [
  {"key": "quest", "type": "quest", "label": "the quest"},
  {"key": "player", "type": "wolf", "label": "from the wolf", "operand": true}]}
```
*Semantics:* the wolf is added to the quest's `withdrawn` set in the run state. `offers()` (883) and the board leave the quest out for that wolf (through the `players` list, as C6 does). A copy already accepted is untouched. `quest.offer` to the same wolf later clears the mark.

*Checks:* the quest must be the program's own.

*Note:* C6 `requires` (Phase 4) makes it unneeded for new programs: the other roads are never offered. It is built only if a today-version program stays live past Phase 4 and its authors want the cleaner journal.

*Fallback:* `quest.fail` with the journal text "another road taken" (large §1.3).

*Tests:* `test_story_quests.py` (withdrawn from A, still offered to B; re-offered clears it).

### SQ4. Quest `rewardLimit` (Phase 9)

`"rewardLimit": {"perDay": 1–1000, "scope": "quest" | "purse"}`: a quest's coin rewards stop after N hand-ins a game day, counted for the quest or for its purse across the program.
- **Built in Phase 9** (answer (a): build everything). The shared counter rule of decisions §3.2 (`nh-purses`, `coin_<purse>_day`, the floor switch) stays the standard for every program. `rewardLimit` is a shorthand for it: it writes the same `coin_<purse>_day` counters and reads the same floor switch, so the two never disagree.
- *Tests:* `test_story_quests.py` (a quest with `perDay: 2` pays twice and then gives the reward without coin; the counters it writes are the ones `nh-purses` resets).

---

## 4. World effects (game orders)

A new action group, `{"id": "talk", "label": "Talk and rumour"}`, holds talk.*, belief.*, figure.* and npc.greet. A second, `{"id": "foes", "label": "Foes"}`, holds foe.*. New event groups: none (they use `fame`, `fights`, `time` and `story`).

### C4. Titles: `title.give`, `title.take`, event `title given`, fact `player.title`

*Catalogue entries:*
```json
{"id": "title.give", "group": "wolves", "title": "Name a wolf by a title",
 "sentence": "{player} is named {title}, known {known} {town}, for {days} days",
 "help": "A title the wolf carries: shown on their character card and to residents, who may use it (the talk fact player.title). Known in the realm or one town. 0 days: for good. A wolf carries up to 8 story titles; the newest is the one used.",
 "params": [
  {"key": "player", "type": "wolf", "label": "the wolf", "operand": true},
  {"key": "title", "type": "text", "label": "the title", "max": 60},
  {"key": "known", "type": "choice", "label": "known", "values": ["realm", "town"], "words": {"realm": "in the realm", "town": "in the town of"}, "default": "realm"},
  {"key": "town", "type": "town", "label": "the town", "optional": true, "words": {"": ""}},
  {"key": "days", "type": "int", "label": "days (0: for good)", "min": 0, "max": 365, "optional": true}]}

{"id": "title.take", "group": "wolves", "title": "Take a title back",
 "sentence": "{player} is no longer called {title}",
 "help": "Takes back a title this story gave (or, with no title named, every title it gave this wolf).",
 "params": [
  {"key": "player", "type": "wolf", "label": "the wolf", "operand": true},
  {"key": "title", "type": "text", "label": "the title", "max": 60, "optional": true, "words": {"": "every title from this story"}}]}
```
*Event* (group `fame`):
```json
{"id": "title given", "group": "fame", "title": "A wolf is given a title", "sentence": "{actor} is named {detail}",
 "help": "A story named the wolf by a title (detail: the title; target: the story's run; item: realm or town:<id>).",
 "fields": {"actor": {"type": "wolf", "label": "the wolf", "any": "any wolf"}, "target": {"type": "run", "label": "the story run", "any": "any story"}, "item": {"type": "text", "label": "where it is known", "any": "anywhere"}, "detail": {"type": "text", "label": "the title", "any": "any"}},
 "tile": false, "mission": true}
```
Flags: neither W nor B, the same as `deed.award` (see D3).

*Semantics:*
- The game keeps `characters_[wolf].titles`: up to 8 entries of `{title, known, by: program id, run, since, until}`. Giving one already held renews it. A 9th drops the oldest that has an expiry, and otherwise the oldest.
- The talk fact `player.title` (in `windowFacts`, `RatwGameWindowTalk.cpp:156`) is the newest title known where the resident lives (realm, or their town). The library uses it as `#player.title#` in gated lines; `facts.json` "player" gains `title`.
- The client shows titles on the character card under the name (`Client`: character card component) and in the inspect panel for other players.
- A fame record is written: a deed with source `story`, detail "was named <title>". The fame machinery then carries it into gossip and the crier as any deed.
- The event `title given` is recorded.
- **Not undone at run end.** It expires by `days`. `title.take` removes it.
- Checkpoint: saved with the character record.

*Checks:*
- The title must be one line, with no `#` symbols, and must pass V2's forbidden-words check.
- `town` is required exactly when `known` is `town`; `references()` checks the town (`phase9_references`, board towns rule).

*Where:*
- the catalogue;
- `story_runner.WORLD_ORDERS` (`'title.give': 'player', 'title.take': 'player'`);
- `Game::storyWorldOrder` plus `applyStoryWorld`;
- the character record in `RatwGame.h` (`CharacterRecord`) and its save/load;
- `windowFacts`;
- `Data/Talk/facts.json`;
- the client card.

*Tests:*
- `Tests/story_world_tests.cpp`: give, renew, the 9th drops the oldest, take by title and take all, expiry.
- `test_story_effects.py`: each order; the town check.
- `Client/src/game/*.test.ts`: the card line.
- `talk_check --validate`: a gated `#player.title#` line.

*Guide:* §7 "Rewards" lists titles; §14 says titles stay.

### C5. `npc.greet`: a resident's words for one wolf

*Catalogue entry:*
```json
{"id": "npc.greet", "group": "talk", "title": "A resident greets one wolf",
 "sentence": "{npc} greets {player}: {text} (for {days} days)",
 "help": "The next time this wolf opens the resident's window (within the days), the resident greets them with these words, in their own voice (#symbols# fill), instead of their usual greeting: once. A later greeting for the same wolf from the same resident replaces it.",
 "params": [
  {"key": "npc", "type": "resident", "label": "the resident", "operand": true},
  {"key": "player", "type": "wolf", "label": "the wolf", "operand": true},
  {"key": "text", "type": "text", "label": "the greeting", "max": 200},
  {"key": "days", "type": "int", "label": "for days", "min": 1, "max": 28, "optional": true}]}
```
*Semantics:*
- The game keeps `storyGreets_[npc][wolf] = {text, until (game day), run}`, at most 1 per pair and 20,000 in all (the oldest go first).
- When the wolf opens a window with the resident, the hub's opening line is replaced by `inVoice(npc, wolf, text, facts)` (`RatwGameWindowTalk.cpp:847`), and the entry is erased.
- Allowed in triggers with `$actor` (`audience: participant`) and in quest `rewards`. A reward's greeting is from the giver unless it says otherwise.
- **Not undone at run end**: it is a word of thanks. It is kept in the checkpoint (migration **0080** is not needed if checkpoint-only; the checkpoint holds it beside `talkMemory_`, `RatwGame.cpp:7392`).

*Checks:* one line; symbols checked by V2; `references()` checks `npc`.

*Where:* the catalogue; `WORLD_ORDERS` (`'npc.greet': 'npc'`); `applyStoryWorld`; the window's opening line (the `DialogueSession` start in `RatwGameWindowTalk.cpp`); checkpoint save/load.

*Tests:* `Tests/dialogue_tests.cpp` (shown once, in dialect, expired after days, replaced by a later one); `test_story_effects.py`.

*Guide:* §7 "Thanks by name"; the campaign's Spark rung.

### C8. Talk memory and rumours: `talk.remember`, `talk.forget`, `belief.plant`

*Catalogue entries:*
```json
{"id": "talk.remember", "group": "talk", "title": "Residents remember something",
 "sentence": "{scope} {town} {npc} {player} remembers {key} = {value} for {days} days (after the story: {keep})",
 "help": "Talk memory residents' lines can test (world.mem.<key> for the realm or a town, self.mem.<key> for one resident, pair.mem.<key> for one resident about one wolf: Data/Talk lines gated on it play). Realm: every resident; a town: its residents (the whole city is one town); a resident: that one; a resident and a wolf: that resident, when that wolf is the one they talk to. Taken back when the story ends unless kept.",
 "params": [
  {"key": "scope", "type": "choice", "label": "who", "values": ["realm", "town", "npc", "pair"], "words": {"realm": "the realm", "town": "the town of", "npc": "the resident", "pair": "the resident, about the wolf,"}, "default": "town"},
  {"key": "town", "type": "town", "label": "the town", "optional": true, "operand": true, "words": {"": ""}},
  {"key": "npc", "type": "resident", "label": "the resident", "optional": true, "operand": true, "words": {"": ""}},
  {"key": "player", "type": "wolf", "label": "the wolf", "optional": true, "operand": true, "words": {"": ""}},
  {"key": "key", "type": "talkkey", "label": "what (a key)"},
  {"key": "value", "type": "talkvalue", "label": "its value"},
  {"key": "days", "type": "int", "label": "days", "min": 1, "max": 365},
  {"key": "keep", "type": "bool", "label": "kept after the story", "optional": true, "words": {"true": "kept", "false": "taken back"}}]}

{"id": "talk.forget", "group": "talk", "title": "Residents forget something",
 "sentence": "{scope} {town} {npc} {player} forgets {key}",
 "help": "Takes away a memory a story gave (any story's, by key).",
 "params": [
  {"key": "scope", "type": "choice", "label": "who", "values": ["realm", "town", "npc", "pair"], "words": {"realm": "the realm", "town": "the town of", "npc": "the resident", "pair": "the resident, about the wolf,"}, "default": "town"},
  {"key": "town", "type": "town", "label": "the town", "optional": true, "operand": true, "words": {"": ""}},
  {"key": "npc", "type": "resident", "label": "the resident", "optional": true, "operand": true, "words": {"": ""}},
  {"key": "player", "type": "wolf", "label": "the wolf", "optional": true, "operand": true, "words": {"": ""}},
  {"key": "key", "type": "talkkey", "label": "what (a key)"}]}

{"id": "belief.plant", "group": "talk", "title": "Start a rumour",
 "sentence": "{count} residents of {town} come to believe that {subject} {claim} ({confidence}% sure)",
 "help": "Residents of a town (chosen by the story's seed: placed, living, not the one it is about) believe something of a resident, as if they had heard it: it is told in gossip ('#x.name# <claim>') and spreads along friendships and caravans as any belief does. About residents only, never a wolf.",
 "params": [
  {"key": "subject", "type": "resident", "label": "about", "operand": true},
  {"key": "claim", "type": "text", "label": "that they (a phrase)", "max": 120},
  {"key": "town", "type": "town", "label": "in the town", "operand": true},
  {"key": "count", "type": "int", "label": "how many", "min": 1, "max": 50},
  {"key": "confidence", "type": "int", "label": "how sure (%)", "min": 10, "max": 90}]}
```
New param types: `talkkey` ("A talk memory key: lower-case letters, digits and _, up to 40, starting with a letter") and `talkvalue` ("A talk memory value: lower-case letters, digits and _, up to 40"). Both are added to `types`.

*Semantics, memory:*
- **A store of its own**, so talk's own memory is never evicted: `Game::storyMemory_`, a map from owner (`realm`, `town:<region>`, `person:<npc>` or `pair:<a>|<b>`) to key → `{value, until, run, keep}`.
  - Caps: 64 per owner and 8,192 in all for realm, town and person owners.
  - **Pair owners** have their own budget: 16 keys per pair and 20,000 pair entries in all (the size of C5's greet store). At 200 wolves × the campaign's 9 pair keys (cast-and-talk §5.3: `nh_found`, `nh_stone`, `nh_willa`, `nh_ledger`, `nh_spy`, `nh_purse`, `nh_thyme`, `nh_ingrid`, `nh_gudrun`), that is 1,800.
  - On overflow, the soonest to expire goes first, and the game notes it once a game day (`note("warn", ...)`) so the console's Problems shows it.
- **The pair owner key** is the one talk already uses: `"pair:" + min(npc, wolf) + "|" + max(npc, wolf)` (`RatwGameTalk.cpp:728`, `RatwGameAmbient.cpp:593` `pairKey`). So a pair key written by a story is recalled wherever talk recalls the pair: in the dialogue window (`dialogueQuery` → `talkQuery`, `RatwGameDialogue.cpp:447`). Ambient talk between two residents never sees it, because its pair is the two residents.
- **Recall:** `Game::talkRecall` (`RatwGameTalk.cpp:906`) and the window/ambient query builders (`RatwGameAmbient.cpp:571`, `RatwGameTalk.cpp:732`, `RatwGameWindowTalk.cpp`) recall into the same facts in this order:
  1. the realm into `world.mem.*`;
  2. the town into `world.mem.*`, which overrides the realm;
  3. talk's own `town:` memory, which overrides both;
  4. for a resident, `person:<npc>` into `self.mem.*`;
  5. for a resident talking with a wolf, the story's `pair:` entries into `pair.mem.*`, then talk's own `pair:` memory, which overrides them (`Library::remembered`, `RatwTalk.cpp:2749`).
  `RatwTalk.cpp:964` already lets lines gate on any `world.mem.<k>` / `self.mem.<k>` / `pair.mem.<k>`.
- **The town of a resident** is the region of their home cell, as `town:` owners already use.
- **The pair scope** (was T1) is for **participant** triggers and quest rewards only: `player` must be `$actor`, `$each` over wolves, a wolf variable, or `{"field":"actor"}`. It does not act on the wolf (no W flag): it changes only what one resident says to them. It is what the 17 `pair.mem.*` gates of `cast-and-talk.md` need: per-wolf greetings ("You're the one who brought my mother's stone"), "already pledged", Ingrid's truth or kindness, Rosalind's trust.
- **Undo:** at run end (`endStoryWorld`), entries of that run with `keep: false` are erased. Entries with `keep: true` stay until `until`. The held list is `held.memory` (owner|key pairs) for the run's own undo.
- **Checkpoint:** saved beside `talkMemory_` (`RatwGame.cpp:7392`, 7508, 7566, 7727), under a new key `storyMemory`.

*Semantics, rumour:*
- Chooses `count` holders: residents whose home region is `town`, who are alive and placed, and are not the subject. They are ordered by `fnv(run|subject|claim|id)`, so a re-run picks the same.
- Calls `World::believe(holder, subject, claim, "story:<run>", confidence/100, "", "")` with the kind `story.rumour`.
- `Data/Talk/claims.json` gains `"story.rumour": {"family": "rumour", "grave": 1, "they": ["#x.name# #claim.text#"], "text": {"starts": []}}`. `claims::Words` (RatwClaims.h:27) gains a `text` slot filled from the belief's free words, which hold the phrase.
- **Lasting**: beliefs fade by the world's rules, and J8 forgets. A rumour about a protected resident is allowed.
- `belief.plant` gains an optional param `kind` (T7, below), default `story.rumour`.

*Checks:*
- Scope parts: `town` exactly with `town` scope; `npc` exactly with `npc` or `pair` scope; `player` exactly with `pair` scope.
- A `pair` write in a world-audience trigger is refused ("Pair memory needs a wolf of the story: use it in a participant trigger").
- `key` and `value` must match their patterns.
- The claim must be a lower-case phrase with no ids, no `#`, and no names of residents (V2 checks names against `dm.story_residents` names at approval).
- `references()` checks the town and the subject.

*Where:*
- the catalogue;
- `WORLD_ORDERS` (`'talk.remember': None, 'talk.forget': None, 'belief.plant': 'subject'`; target = run for the first two);
- `applyStoryWorld`; `Game::storyMemory_` (`RatwGame.h`); the recall sites listed above;
- `World::believe` (`RatwRoads.cpp:460`); `claims.json`; `RatwClaims.cpp` (the `text` slot).

*Tests:*
- `Tests/story_world_tests.cpp`: realm, town, resident and pair recall order; both caps; undo at end; keep; checkpoint round trip.
- `Tests/dialogue_tests.cpp`: a tree `say[world.mem.nh_stage=reckoning]` line plays after `talk.remember` realm; a `say[pair.mem.nh_stone=brought priority=5]` line plays for the wolf it was written for and not for another wolf at the same resident.
- A belief test: 12 holders chosen deterministically, and gossip says "<Name> was seen at the Customs House at midnight".
- `test_story_effects.py`: orders and checks.
- `talk_check --validate`, `talk_scorecard_quick` and `talk_golden` unchanged: the test town has no story memory.

*Workbench:* `pickers.tsx` adds `talkkey` and `talkvalue` pickers (a name picker listing the keys this program already uses). The console gets a "What residents remember" panel per run (keys, owners, values, days left).

*Guide:* a new section, "Talk and rumour from a story" (after §8): the key table pattern, gating lines in Data/Talk, writing rules (no names in library lines, dialect), and the order of realm, town, resident and pair.

*Fallback until Phase 1b:* the DM seeds realm and town keys by hand (decisions §6.2, Phase 0); `self.mem` and `pair.mem` lines stay inert. For pair lines, C5 `npc.greet` (once per wolf, Phase 3) or `deed.award` gossip.

### T7. Claim kind `story.cleared` (Phase 1b, with C8)

*Catalogue:* `belief.plant` gains
```json
{"key": "kind", "type": "choice", "label": "the kind of claim", "values": ["story.rumour", "story.cleared"], "words": {"story.rumour": "that", "story.cleared": "that it is not true that"}, "optional": true, "default": "story.rumour"}
```
and `belief.forget`'s `kind` values gain `story.cleared`.

*Semantics:*
- `Data/Talk/claims.json` gains `"story.cleared": {"family": "rumour", "grave": 0, "they": ["#x.name# never #claim.text#"], "text": {"starts": []}}`.
- It is planted like `story.rumour`, so it spreads on bonds and caravans as any belief does. Gossip tells it as "<Name> never took the Customs House silver".
- It is the counter-claim to an old belief. It does not lower the old one: J8 `belief.forget` does that (Phase 3). The campaign uses both, in that order, to clear Ingrid Kendrick (803 theft beliefs) and the wronged logger.

*Checks:* the claim phrase follows `belief.plant`'s rules (lower case, no ids, no names, no `#`). The phrase is written as the thing **not** done ("took the Customs House silver"), and V2 warns if it begins with "never" or "did not" (the template adds it).

*Tests:* a belief test in `Tests/story_world_tests.cpp`: 12 holders, and gossip says "<Name> never …"; `talk_check --validate` accepts the claim template.

*Fallback until Phase 1b:* `deed.award` on the clearer, and the cleared resident's own gated tree line.

### N2. Window facts of a wolf's own story: `player.quest.<story>/<quest>` and `player.story.<var>`

N2 now holds T2 (`cast-and-talk.md` §5.4). Both facts are read only in the dialogue window, about the wolf who opened it.

*Semantics, `player.quest`:*
- `Game::windowFacts` (`RatwGameWindowTalk.cpp:156`) sets, for each live storykeeper story and each quest view the wolf has (`questViews[wolf].quests`): `player.quest.<program id>/<quest id>` = the view's status (`offered`, `active`, `ready`, `done`, `failed`, `abandoned` or `expired`).
- The program id is the storyline's `program` (kept in `Storyline` for storykeeper stories; if it is not kept, it is added by `applyStoryPhase`).
- `facts.json` "player" gains the pattern entry `quest.*`, so `talk_check` accepts the keys.

*Use:* `say[player.quest.ninth-howl.1/where_king=active priority=3]` on a resident tree.

*Semantics, `player.story`:*
- A **player**- or **realm**-scope declaration may add `"talk": true`. At most 8 per program; text, number or switch only.
- The runner sends those values for each cohort wolf with the wolf's quest views (`story_quests.views` / `trackers`, 841 / 855, which already go to the game for the journal), as a `talk` map: `{<var>: <value>}`. Only wolves with a stored value are sent.
- `Game::windowFacts` sets `player.story.<var>` = the value (a number as its digits, a switch as `yes` / `no`, a text as it is).
- The campaign marks `p_howl`, `p_backs` and `p_advice`. So a node can hide a choice the wolf already made: "You pledged to Coldwater, didn't you?" is `say[player.story.p_howl=coldwater]`.
- Two live stories that both mark the same name: the newest run's value wins, and `references()` warns at approval ("p_howl is also told to residents by <program>"). D2's `prefix` keeps names apart.
- `facts.json` "player" gains the pattern entry `story.*`.

*Checks:* a text marked `talk` may hold only lower-case letters, digits and `_`, up to 40 (as `talkvalue`), so a gate can test it. The declaration check refuses a literal that breaks the rule; a value that breaks it at run time is stored but not sent, with a note in the history.

*Fallback until Phase 1b:* the story ignores repeat picks; the node's reply stays generic.

*Tests:* `Tests/dialogue_tests.cpp` (each fact as the status or the value changes; another wolf at the same resident does not see it); `test_story_quests.py` (the `talk` map in the views; at most 8).

*Guide:* the talk section above.

### J8. `belief.forget`

*Catalogue entry:*
```json
{"id": "belief.forget", "group": "talk", "title": "Put a rumour to rest",
 "sentence": "Residents {town} believe {share}% less that {subject} did {kind}",
 "help": "Residents (of one town, or everywhere) grow less sure of one kind of claim about a resident (a theft, an attempted theft, a story's rumour...): each holder's belief loses that share of its strength, and one too faint is forgotten.",
 "params": [
  {"key": "subject", "type": "resident", "label": "about", "operand": true},
  {"key": "kind", "type": "choice", "label": "the kind of claim", "values": ["theft", "theft.attempt", "theft.unseen", "assault", "assault.unseen", "violent", "warrant", "promise.broken", "prowling", "story.rumour"]},
  {"key": "town", "type": "town", "label": "in the town", "optional": true, "words": {"": "everywhere"}},
  {"key": "share", "type": "int", "label": "by (%)", "min": 10, "max": 100}]}
```
*Semantics:* a new `World::doubt(subject, kind, region, share)` walks `beliefs_`. For a holder (in the region if given) whose belief has that subject and kind, it multiplies confidence by `(1 - share/100)`. At ≤ .05 the belief is removed. Lasting.

*Checks:* the kind values are kept in sync with `claims.json` kinds by `test_story_catalogue.py` (read `claims.json`).

*Where:* the catalogue; `WORLD_ORDERS` (`'belief.forget': 'subject'`); `applyStoryWorld`; `World::doubt` (`RatwRoads.cpp`, near `forgetClaim`, 611).

*Tests:* `Tests/story_world_tests.cpp` (share 50 twice; a town filter; 100 removes).

### C9. Named figures: `figure.set`, `figure.clear`

*Catalogue entries* (`needsBuilder`):
```json
{"id": "figure.set", "group": "talk", "title": "Name a figure of the realm", "needsBuilder": true,
 "sentence": "{npc} {player} {name} is the realm's {role}, {title}, known {known} {region} {house}",
 "help": "Who residents mean by the ruler, the lord, the captain, the high priest, the hero or the villain (#world.figure_<role># in their lines), where they are known. A resident, a wolf, or a name only. It stays after the story; Clear the figure takes it away.",
 "params": [
  {"key": "figure", "type": "figure", "label": "its id"},
  {"key": "role", "type": "choice", "label": "the role", "values": ["ruler", "lord", "captain", "high_priest", "hero", "villain"]},
  {"key": "npc", "type": "resident", "label": "a resident", "optional": true, "operand": true, "words": {"": ""}},
  {"key": "player", "type": "wolf", "label": "a wolf", "optional": true, "operand": true, "words": {"": ""}},
  {"key": "name", "type": "text", "label": "or a name", "max": 120, "optional": true, "words": {"": ""}},
  {"key": "title", "type": "text", "label": "their title", "max": 120, "optional": true},
  {"key": "known", "type": "choice", "label": "known", "values": ["realm", "region", "house"], "words": {"realm": "in the realm", "region": "in", "house": "among"}, "default": "realm"},
  {"key": "region", "type": "town", "label": "the town or land", "optional": true, "words": {"": ""}},
  {"key": "house", "type": "faction", "label": "the House", "optional": true, "words": {"": ""}},
  {"key": "about", "type": "text", "label": "about them", "max": 600, "optional": true}]}

{"id": "figure.clear", "group": "talk", "title": "Clear a figure", "needsBuilder": true,
 "sentence": "Residents no longer speak of the figure {figure}",
 "help": "Takes a figure away (one a story or a Dungeon Master set).",
 "params": [{"key": "figure", "type": "figure", "label": "its id"}]}
```
New param type `figure`: "A figure's id (live.figures): lower-case letters, digits and _, 3 to 40".

*Semantics:*
- Like `faction.relation`, this runs in the runner (`order_of`, 597):
  1. It writes `live.figures` with the same validation as `dungeon_master.save_figure` (837). That code moves to a shared function in `tools/world_db.py` or `story_runner.write_figure`, so both callers use it.
  2. Then it queues `voice.sync` with target = the figure id. The game reloads figures (`RatwGameTalk.cpp:1480`).
- A wolf's figure is written with `name` = the wolf's display name at that moment (from `game.characters`) and `npc_id` null.
- `known_in` = `realm`, `region:<town>` or `house:<faction>`.
- **Lasting.** The run's history records the figure and its previous row (`dm.story_trigger_history` note). The console's "Undo this figure" restores the previous row.
- **Exactly one** of `npc`, `player` or `name`.

*Checks:* the role must be a `facts.json` world `figure_*` (read as `save_figure` does); `region` exactly with `region`; `house` exactly with `house`; `references()` checks the town and the faction.

*Where:* the catalogue; `story_runner.order_of`; the shared `write_figure`; `dungeon_master.save_figure` calls it.

*Tests:* `test_story_effects.py` (`RATW_STORY_TEST_DSN`): the row is written, `voice.sync` is queued, a wolf hero, previous-row history; refused without a builder on a hosted world (`test_who_may_send_them_on_a_hosted_world`).

*Guide:* the talk section; §13 (builder).

### C10. Foes: `foe.add`, `foe.leave`, `foe.outlaw`; events `foe downed`, `foe defeated`, `foe fled`

*Catalogue entries:*
```json
{"id": "foe.add", "group": "foes", "title": "Bring a foe",
 "sentence": "{name} ({foe}), a {strength} foe with {band} followers, comes to {cell,x,y} for {minutes} minutes, looking like {like} {description}, and {stance}",
 "help": "A named enemy who can be fought and beaten: wolves may set on them without a crime, the Watch on duty helps the wolves, and their followers break and run when they fall. They come and go: never a resident, gone when the story ends or their time runs out (then 'foe fled').",
 "params": [
  {"key": "foe", "type": "foe", "label": "their id"},
  {"key": "name", "type": "text", "label": "their name", "max": 60},
  {"key": "cell", "type": "place", "label": "the place", "part": "tile"},
  {"key": "x", "type": "int", "label": "tile x", "min": 0, "max": 4095, "part": "tile"},
  {"key": "y", "type": "int", "label": "tile y", "min": 0, "max": 4095, "part": "tile"},
  {"key": "strength", "type": "choice", "label": "how strong", "values": ["tough", "fierce", "champion"], "default": "tough"},
  {"key": "band", "type": "int", "label": "followers", "min": 0, "max": 8, "operand": true},
  {"key": "stance", "type": "choice", "label": "what they do", "values": ["waits", "hunts"], "words": {"waits": "holds the ground", "hunts": "comes for the nearest wolf"}, "default": "waits"},
  {"key": "minutes", "type": "int", "label": "minutes", "min": 1, "max": 1440},
  {"key": "like", "type": "resident", "label": "looking like (a resident)", "optional": true, "words": {"": "a stranger"}},
  {"key": "description", "type": "text", "label": "how they look", "max": 200, "optional": true, "words": {"": ""}}]}

{"id": "foe.leave", "group": "foes", "title": "A foe leaves",
 "sentence": "{foe} and their followers leave", "help": "The foe and their band go (no event).",
 "params": [{"key": "foe", "type": "foe", "label": "their id"}]}

{"id": "foe.outlaw", "group": "foes", "title": "Make a resident a foe", "needsBuilder": true,
 "sentence": "{npc} turns on the wolves with {band} followers for {minutes} minutes ({strength}); killed if beaten: {lethal}",
 "help": "A resident (never a protected one) may be fought for a while without a crime, the Watch on the wolves' side. Beaten, they are downed and held, never killed unless the story says so. Afterwards they are themselves again.",
 "params": [
  {"key": "npc", "type": "resident", "label": "the resident", "operand": true},
  {"key": "minutes", "type": "int", "label": "minutes", "min": 1, "max": 720},
  {"key": "strength", "type": "choice", "label": "how strong", "values": ["tough", "fierce", "champion"], "optional": true},
  {"key": "band", "type": "int", "label": "followers", "min": 0, "max": 6, "optional": true, "operand": true},
  {"key": "lethal", "type": "bool", "label": "may die", "optional": true, "words": {"true": "yes", "false": "no"}}]}
```
New param type `foe`: "A foe's id, given by the story (letters, digits, - and _)".

*Events* (group `fights`; every field filled):
```json
{"id": "foe downed", "group": "fights", "title": "A foe's follower falls", "sentence": "{actor} brings down one of {target}'s band ({detail})",
 "help": "A follower of a story's foe is downed.", "fields": {"actor": {"type": "character", "label": "who landed the last blow", "any": "anyone"}, "target": {"type": "foe", "label": "the foe", "any": "any foe"}, "cell": {"type": "place", "label": "where", "any": "anywhere"}, "detail": {"type": "text", "label": "the follower's name", "any": "any"}},
 "tile": true, "mission": true},
{"id": "foe defeated", "group": "fights", "title": "A foe is beaten", "sentence": "{actor} beats {detail}",
 "help": "A story's foe (or a resident made one) is downed: actor landed the last blow, target is the foe's id (a resident's id for foe.outlaw), detail their name, quantity how many wolves fought.",
 "fields": {"actor": {"type": "character", "label": "who landed the last blow", "any": "anyone"}, "target": {"type": "foe", "label": "the foe", "any": "any foe"}, "cell": {"type": "place", "label": "where", "any": "anywhere"}, "detail": {"type": "text", "label": "their name", "any": "any"}, "quantity": {"type": "number", "label": "wolves who fought"}},
 "tile": true, "mission": true, "measures": []},
{"id": "foe fled", "group": "fights", "title": "A foe gets away", "sentence": "{detail} gets away",
 "help": "A story's foe broke off and fled, or their time ran out (detail: their name; item: fled or time).",
 "fields": {"target": {"type": "foe", "label": "the foe", "any": "any foe"}, "cell": {"type": "place", "label": "where", "any": "anywhere"}, "item": {"type": "text", "label": "fled or time", "any": "either"}, "detail": {"type": "text", "label": "their name", "any": "any"}},
 "tile": true}
```

*Semantics, `foe.add`:*
1. The game makes a **story camp** `BanditCamp{id: "foe_<run8>_<foe>", cell, x, y, story: true, active: true}` in `roads_.camps`. It is never counted with the world's camps, never re-formed, and never cleared by the Watch (`RatwRoads.cpp:1494` skips `story`).
2. The game adds road folk with `addRoadFolk(id, name, description, cell, at, "foe", campId)` (`RatwRoads.cpp:735`):
   - the leader is `<camp>:0` (the leader rule at `RatwBattle.cpp:2584` makes the band break and run when the leader falls);
   - the followers are `<camp>:1..band`, named "one of <name>'s band" (from `Data/Storylines/foes.json`).
3. `like` gives `appearance`, as `visitor.add` does (`RatwGame.cpp:2140`).
4. **Strength** comes from the new data file `Data/Storylines/foes.json`. The starting table, which C10t tunes: `{"tough": {"skill": 60, "strength": 60, "hp": 20, "flee": 25}, "fierce": {"skill": 70, "strength": 68, "hp": 24, "flee": 15}, "champion": {"skill": 80, "strength": 75, "hp": 30, "flee": 0}, "follower": {"skill": 52, "strength": 55, "hp": 16, "flee": 35}}`. The data sets `RoadFolk.skill`, `RoadFolk.hp` and `Entity.strength`. `temperamentOf` (`RatwBattle.cpp:290`) treats kind `foe` as `bandit` (aggressive).
   - Balance is tuned by simulation (C10t below; the user's answer (c) settles §8 D4).
5. **Fights:**
   - `startBattle` already takes the camp path for `campOf(target)` (586–598): there is no assault and no crime.
   - New: guards on duty in the cell join on the wolves' side against a **story** camp (at 620, add `b.camp` story → side of the wolf).
6. **Stances:**
   - `waits` stays put, and fights whoever comes within the camp's aggro (the existing encounter rules).
   - `hunts` walks toward the nearest wolf within 12 tiles, using the `bandits creep` logic in `RatwRoads.cpp`.
7. **Events:**
   - When a follower is downed: `foe downed`, written where `bandit falls` is written (2582) for a story camp, instead of `bandit falls`.
   - When the leader is downed: `foe defeated` with `quantity` = distinct wolves on the wolves' side. The camp is then closed: the remaining band flee and are removed when out of sight. Downed foes are removed after the fight (as bandits are).
   - When `minutes` run out, or the leader flees: `foe fled` (item `time` / `fled`). The camp and its folk are removed.
8. **Undo:** `held.foes` lists camp ids. `endStoryWorld` removes the camps and their folk, with no event. `tickStoryWorld` checks the time.

*Semantics, `foe.outlaw`:*
1. Refused if the resident is protected (`isProtected`), dead, in a fight, away with a party, or held by another story.
2. `World::storyOutlaws_[npc] = {camp, until, lethal, strength}`. A story camp is made at the resident's tile, with `<camp>:1..band` followers.
3. `campOf(npc)` returns that camp while the entry lasts, so the camp path applies: no crime, guards on the wolves' side. `temperamentOf` uses the given strength (or `tough`) and `aggressive`.
4. When the resident is downed:
   - if not `lethal`: they are held downed, then revived where they lie when the fight ends; `npc.hold` is applied (held until run end); `foe defeated` is raised with target = the resident id;
   - if `lethal`: the death path runs as for a bandit, and `death` follows.
5. When `minutes` pass, or the run ends, the entry is erased and they are themselves (bonds untouched).
6. Never on a resident whose `protectedNpc` flag is set.

*Checks:*
- `foe` id `[a-z0-9][a-z0-9_-]{2,39}`, unique in the program.
- The tile is checked (`check_tile`).
- `foe.outlaw` needs a builder, and its `npc` is checked.
- A foe event's `match.target` names a foe of the program (a warning otherwise).
- Rehearsal facts may be foe events.

*Where:*
- the catalogue; `foes.json`;
- `story_runner.WORLD_ORDERS` (`'foe.add': 'foe', 'foe.leave': 'foe', 'foe.outlaw': 'npc'`);
- `applyStoryWorld`;
- `World` (`RatwRoads.cpp`: story camps, `campOf`, the hunt walk; `RatwBattle.cpp`: temperament, guards' side, `foe downed`/`foe defeated` at 2560–2595, `checkOver`);
- `RatwWorld.h` (`BanditCamp.story`, `storyOutlaws_`);
- checkpoint (camps already save; the outlaw map is added).

*Tests:*
- `Tests/battle_tests.cpp` (and `story_world_tests.cpp` for undo): a wolf attacks a foe and there is no warrant; a guard joins the wolf; the leader falls, then the band flees, then `foe defeated` once; time runs out, then `foe fled`; run end removes everything.
- `outlaw`: a protected resident is refused; downed and not killed, then held; `lethal` kills.
- `test_story_effects.py`: orders, the builder flag.
- `test_story_v2.py`: rehearsal with foe events.
- The tuning passes and the load rows of C10t below (they replace the first draft's single `world_check --players 20` check).

*Workbench:* `pickers.tsx` gets a `foe` name picker (like `visitor`). The console gets a "Foes" panel per run (alive, followers left, time left, Send away).

*Guide:* a new section, "Foes" (after the talk section), with the population rule: foes are never residents and never stay.

#### C10t. Tuning the foes by simulation (Phase 7; answer (c))

The user's answer (c): "combat simulations allowed, multiple passes if needed (at most 250 players)". These sims are allowed without asking again. Economy sims are not covered by it (memory: ask before simulations), so the passes run with births, the economy and talk off.

*Tools:*
1. **`Tests/foe_sim.cpp` (new; not a test, like `level_sim`).** It is CMake target `foe_sim`, linked to `ratw_core`, built in `build-core`.
   - It runs real `World` fights on the real rules: `RatwBattle.cpp`, road folk and story camps from `RatwRoads.cpp`, and `foes.json`. It is built from the bandit-fight helpers of `Tests/battle_tests.cpp` and `Tests/battle_play.h` (allies against bandits, 1919–1940).
   - A row reads `foe <strength> band <n> vs <wolves> [guards <g>] [outlaw]`. The wolves use `level_sim`'s side syntax: `plain@new`, `plain@seasoned+` (doc 45's armed: iron and leather), `fireq@veteran`, joined by commas.
   - The bands (new, seasoned, veteran) come from `Data/Progression/skills.json`, and the gear from doc 47.
   - Per row it prints:
     - the wolves' win % with its 95% interval;
     - wolves downed (and injured) per fight;
     - fight seconds (median, p90);
     - leader-first %;
     - band broke-and-ran within 5 s %;
     - foe-fled %;
     - `foe defeated` raised exactly once %.
   - Fights are seeded (`SIM_SEED`) and threaded (`SIM_THREADS`, as `level_sim`). 2,000 fights a row, or 500 for rows of more than 8 wolves.
   - `SIM_FOES=<path>` reads a trial table, so a pass never edits `Data/` until it is accepted.
   - The suites: `foes` (the bar below), `band` (band 0–8 against 1–12 wolves), `guards`, `quick` (one Quickened in a party), `outlaw` (a resident made a foe), `scaled` (C1a's band from wolves present).
2. **`world_check --foes K[:strength[:band[:stance]]]` (new flag in `Tests/world_check.cpp`).** It places K story camps through the same `foe.add` path on the busiest road cells of the world, with `--players N` wolves who seek them. It reports the tick time (p50 and p99, against the same run without foes), fights per game hour, foes beaten and fled, wolves downed, and fights left open after their foe is gone (it must be 0). It runs as `world_check EXPORT_DIR --simulate 0 6 --players N --foes … --no-births --no-talk-ledger --fast`: six game hours (a siege night), on an export of Ridgemere 3 (`python3 tools/world_build.py export DIR --world ridgemere3`, which reads the world database and writes nothing to it). These short runs are the load rows of the combat sims the user allowed. They are never a 30-day or year-long economy run (memory: no week-long runs; ask before simulations).

*The bar.* A pass is accepted when every row's 95% interval lies inside its band:

| Row | Wolves | Band to pass |
|---|---|---|
| follower, 1 v 1 | one new wolf, bare | wins 50–65% |
| follower, 1 v 1 | one seasoned, armed | wins 75–90% |
| tough, band 0 | one seasoned, armed | wins 25–40% (a lone wolf should look for help) |
| tough, band 2 | three seasoned, armed | wins 85–95%, ≤ 0.5 wolves downed per win |
| tough, band 2 | three new, bare | wins 50–70% |
| fierce, band 4 | three seasoned, armed | wins 50–65% |
| fierce, band 4 | five seasoned, armed | wins 80–92% |
| fierce, band 4 | five seasoned, armed, + 2 guards | 8–15 points over the row before (guards help, never carry) |
| fierce, band 4 | two seasoned + one Quickened veteran | wins ≤ 85% (a Quickened lifts a party, never guarantees it; doc 45) |
| champion, band 8 | five seasoned, armed | wins 25–45% |
| champion, band 8 | eight seasoned, armed, + 2 guards | wins 70–85% (Ch 9's Pass) |
| champion, band 0 | one Quickened veteran | wins ≤ 35% (no wolf solos the Grey Captain) |
| fierce, band scaled `min(8, ceil(present / 2))` | 4, 8, 12 and 16 wolves present | each 60–85% (a crowd is not a walkover; Ch 7's waves) |
| outlaw, tough | three seasoned, armed | as tough band 2; the resident downed, never killed (not `lethal`): 100% |
| any | the leader downed | the band breaks within 5 s ≥ 95%; `foe defeated` once: 100% |
| any | fight length | median: tough 20–60 s, fierce 40–120 s, champion 90–240 s |
| load | `world_check --players 20 --foes 2:fierce:8:hunts` | inside the living-NPC perf gate (memory: living-npc-roadmap) |
| load | `--players 100` and `--players 250 --foes 6:fierce:8:hunts` (a siege night at a crowd) | tick p99 at most 10% over the same run without foes; 0 fights left open |

*The passes:*
1. **Pass 1** runs the starting table (C10 rule 4) through `foe_sim foes`, `band` and `scaled`. Each strength with a row out of its band gets **one** knob changed for the next pass: skill first, then hp, then flee. Every change is written down with the row that asked for it.
2. **Passes 2–4** re-run the rows that moved and their neighbours, then the whole `foes` suite after any pass that changed a value.
3. **The load pass** (`world_check` at 20, 100 and 250 players, never more: the user's cap, and memory "no 1000-player runs") runs once the 1 v 1 and party rows pass, and again only if a later pass changes hp or flee.
4. **The stop rule:** if a row is still out after pass 4, the agent stops and reports to the user the closest table and the rows that miss. No pass 5 runs without the user.
5. **Scope:** only `foes.json` changes. A row that cannot pass without a change to the fight rules (`RatwBattle.cpp`, docs 37, 45, 47) is reported, not fixed.
6. **Records:**
   - each pass writes `artifacts/combat/foes/pass<N>.txt` (the table used and every row's result);
   - the accepted table goes to `Data/Storylines/foes.json` with `"tuned": "<date>, pass <N>"`;
   - doc 85's "Foes" section (and doc 45's appendix) records the bar and the result.
   - The passes run in the background, and the agent says what is running (memory: ask before simulations).
7. **Time:** building `foe_sim` and the `world_check` flag is about 3 agent-hours. The passes are about 3 agent-hours over about 2–3 real hours of runs.

*Tests:* `Tests/battle_tests.cpp` keeps the behaviour (C10's tests). `foe_sim` and the flag get one smoke case each in `ctest` (`foe_sim foes 20` exits 0 and prints every row; `world_check --players 2 --foes 1` completes).

### C11. Sparing from old age; how long protection lasts

*Catalogue entries:*
```json
{"id": "npc.spare", "group": "residents", "title": "Spare a resident from old age",
 "sentence": "{npc} does not die of old age while {lasts}",
 "help": "Old age passes the resident by (stories and fights still can't kill a protected one: Protect them too). For this story, or for its whole campaign (let go when the campaign ends).",
 "params": [
  {"key": "npc", "type": "resident", "label": "the resident", "operand": true},
  {"key": "lasts", "type": "choice", "label": "for", "values": ["run", "campaign"], "words": {"run": "this story runs", "campaign": "the campaign runs"}, "optional": true, "default": "run"}]}

{"id": "npc.unspare", "group": "residents", "title": "Let old age take its course",
 "sentence": "{npc} is no longer spared by this story", "help": "Ends this story's (or its campaign's) sparing; another story may still spare them.",
 "params": [{"key": "npc", "type": "resident", "label": "the resident", "operand": true}]}
```
`npc.protect` gains:
```json
{"key": "lasts", "type": "choice", "label": "for", "values": ["ever", "run", "campaign"], "words": {"ever": "for good (until Unprotect)", "run": "while this story runs", "campaign": "while the campaign runs"}, "optional": true, "default": "ever"}
```
The default `ever` keeps today's meaning.

*Semantics:*
- The game keeps `storyHolds_[kind][npc] = set of owners`, where kind is `spare` or `protect` and the owner is `run:<run>` or `campaign:<id>`.
  - `npc.spare` adds an owner; `npc.unspare` removes this run's and its campaign's owners.
  - `World::tendOldAge` (`RatwWorld.cpp:3529`) skips a resident with any spare owner, through `World::spared(id)` set by the Game (`world_.setSpared(set)`).
  - `Game::isProtected` (`RatwGameTroubles.cpp:494`) is true for a resident with any `protect` owner, after the explicit `unprotectedMarks_` check.
- **The campaign:** the runner adds `campaign: <campaign id>` to the payload (`_campaign_of`, 985).
  - When `run_ended` marks the campaign ended (1188–1192), the runner sends `story.campaign.end {campaign}`. The game drops that campaign's owners.
  - The console's Campaigns panel gets "Let go of the campaign's holds" for an act stopped early. This replaces the manual `ninth-howl.cleanup` program, which stays as a fallback.
- **Undo:** `endStoryWorld` drops `run:<run>` owners.
- **Checkpoint:** `storyHolds_` is saved beside `protectedMarks_` (`RatwGameTroubles.cpp:176-205`).

*Checks:* `references()` checks `npc`. `lasts: campaign` in a program not approved as a chapter is a problem (it would never be let go): it is refused by `references(chapters=False)`.

*Where:* the catalogue; `story_runner.action()` (`'npc.spare': 'npc', 'npc.unspare': 'npc'`); `RatwGame.cpp:1687` branch, extended; `RatwGameTroubles.cpp`; `RatwWorld.cpp` `tendOldAge`; `story_runner.run_ended`.

*Tests:*
- `Tests/storyteller_tests.cpp` or `story_world_tests.cpp`: a spared 90-year-old survives 400 days of `tendOldAge` (seeded); run end lets go; a campaign owner survives the chapter's end and is dropped at campaign end; two stories spare one resident and one ends; protect `run` versus `ever`.
- `test_story_campaigns.py`: `story.campaign.end` is sent once.

*Guide:* §14 (what is undone); §4 (campaign-long holds).

### C14. `npc.post`: one atomic change of a post

*Catalogue entry* (`needsBuilder`):
```json
{"id": "npc.post", "group": "residents", "title": "Give a resident a post", "needsBuilder": true,
 "sentence": "{npc} takes the post {post}; whoever held it gives it up",
 "help": "A lasting change in the careers book, in one step: the resident leaves any post they hold and takes this one; the one before them is left without it, for the vacancy rules to look after. Never undone.",
 "params": [
  {"key": "npc", "type": "resident", "label": "the resident", "operand": true},
  {"key": "post", "type": "post", "label": "the post"}]}
```
New param type `post`: "A post in the careers book (its id)". `refs()` adds `posts: [{id, title, holder}]` from the checkpoint's careers positions, read from the game's `dm`-visible copy or `game.story_facts`. See D5 on where the list lives.

*Semantics:* a new `Society::swapPost(positionId, resident, world, day)`:
1. Unlike `appoint` (`RatwCareers.cpp:1565`), it does not refuse a taken post: it clears the holder (`lastHolder`, `vacantSince = day`) and then gives the post to the resident.
2. It refuses a `guard` role ("The watch chooses its own").
3. It records a `succession` note with how = "by the story".
- **Lasting, never undone.**

*Checks:* `references()` checks `post` against the post list and `npc` against residents.

*Where:* the catalogue; `story_runner.action()` (`'npc.post': 'npc'`); `RatwGame.cpp` dispatch; `RatwCareers.cpp`; `RatwSociety.h`.

*Tests:* `Tests/careers_tests.cpp`: swap of "reigning at the Great Palace" from Dagfinn to Ottilie, Dagfinn left with no post, a succession note, the economy check unchanged; a guard post is refused.

*Guide:* §7 note "the crown's change" (lasting effects list).

### C15. `mantle.assign` puts back what it replaced (Phase 9)

*Semantics:*
- `held.mantles` becomes an object `{npc: previousMantleOrEmpty}`. An array from an older save is read as `{npc: ""}`.
- `endStoryWorld` (`RatwGameStoryWorld.cpp:481`) and `mantle.release` from the same story re-bind the previous mantle (`assignMantle(npc, prev)`, `RatwGameHandoff.cpp:280`) instead of only releasing it.
- *Tests:* `Tests/story_world_tests.cpp`.
- *Catalogue:* the help gains "…and the mantle they wore before is theirs again".

### J5. Howls: events `howled` and `chorus closed`; effect `npc.howl`

*Catalogue entries:*
```json
{"id": "howled", "group": "talk", "title": "Someone howls", "sentence": "{actor} howls at {cell} ({item})",
 "help": "A wolf (or a resident a story asked to) howls: item is the town it was howled in, or wild. Written only while a live story listens for it.",
 "fields": {"actor": {"type": "character", "label": "who howled", "any": "anyone"}, "cell": {"type": "place", "label": "where", "any": "anywhere"}, "item": {"type": "text", "label": "the town, or wild", "any": "anywhere"}, "detail": {"type": "text", "label": "the chorus it opened or joined", "any": "any"}},
 "tile": true, "watch": true, "mission": true},
{"id": "chorus closed", "group": "talk", "title": "A chorus ends", "sentence": "a chorus of {quantity} ends at {cell} ({item}), begun by {actor}",
 "help": "A gathering howl closed: actor began it, quantity is how many howled in it (residents too), item the town or wild, detail the chorus. Written only while a live story listens for it.",
 "fields": {"actor": {"type": "character", "label": "who began it", "any": "anyone"}, "cell": {"type": "place", "label": "where", "any": "anywhere"}, "item": {"type": "text", "label": "the town, or wild", "any": "anywhere"}, "detail": {"type": "text", "label": "the chorus", "any": "any"}, "quantity": {"type": "number", "label": "how many howled"}},
 "tile": true, "watch": true, "mission": true, "measures": ["quantity"]},
{"id": "npc.howl", "group": "residents", "title": "A resident howls",
 "sentence": "{npc} howls", "help": "The resident howls where they stand: it opens or joins a chorus, wolves hear it as any howl, and the town remembers it.",
 "params": [{"key": "npc", "type": "resident", "label": "the resident", "operand": true}]}
```
(`npc.howl` goes in `effects`.)

*Semantics:*
- `Game::howl` (`RatwGameHowl.cpp:35`) is split into `howlFrom(Entity&, bool resident)`. Residents skip the cooldown message and the client checks, but keep the cooldown, so `npc.howl` spam is bounded to one per `howl.cooldown`.
- The new `Game::residentHowl(npc)` refuses a dead, downed or in-fight resident.
- `howled` is recorded in `howlFrom` when `storyListens("howled")`: item = `townFor(cell)` or `wild`, detail = the chorus id, tile.
- `chorus closed` is recorded in `tendHowls` (190) at close: actor = `howlers.front()`, quantity = `howlers.size()`.
- `residentsHear` is unchanged.
- The two events are added to `MISSION_EVENTS` (by the catalogue).

*Checks:* `references()` checks `npc`.

*Where:* `RatwGameHowl.cpp`; `RatwGame.h`; `story_runner.action()` (`'npc.howl': 'npc'`); `RatwGame.cpp` dispatch; the watch list in `RatwGameStoryFacts.cpp` (the watched kinds set).

*Tests:* `Tests/howl_tests.cpp`: a resident howls and a wolf joins, giving one chorus of 2 and `chorus closed` quantity 2; no event when no story listens; the cooldown holds. `test_story_v2.py`: a quest objective `chorus closed` with `minQuantity` 3.

*Guide:* §8 watched events.

### J7. `npc.queue` (Phase 9, campaign two)

*Catalogue entry* (`needsBuilder`):
```json
{"id": "npc.queue", "group": "residents", "title": "Put an unplaced resident first in line", "needsBuilder": true,
 "sentence": "{npc} is first in line for the next post that falls vacant in {town}",
 "help": "An unplaced resident (one the world knows but who has no home yet) takes the next vacancy in that town by the careers rules. It never makes a vacancy. Lasting.",
 "params": [{"key": "npc", "type": "resident", "label": "the resident"}, {"key": "town", "type": "town", "label": "the town"}]}
```
*Semantics:*
- `Society` keeps `queued_[town] = [resident]`. At the next vacancy in that town (`tendCareers`, where the "empty posts from the city, then unplaced NPCs from the Country" rule fills it; memory: births-match-deaths), the queued resident is taken first, if alive and unplaced.
- It never creates a vacancy, so the population rule holds.

*Tests:* `Tests/careers_tests.cpp`.

### J9. Tagged story items

*Catalogue:* `item.give` gains optional params:
```json
{"key": "tag", "type": "tag", "label": "a story mark", "optional": true, "words": {"": ""}},
{"key": "name", "type": "text", "label": "called", "max": 60, "optional": true, "words": {"": ""}},
{"key": "keep", "type": "bool", "label": "kept after the story", "optional": true}
```
The new param type `tag` is "A story's own mark on an item (letters, digits and _, up to 32)". The `carries` condition and the `gift`, `gift again`, `item picked up` and `item dropped` match accept `item: "<base>^<tag>"`.

*Semantics:*
- A tagged item's id is `<base>^<program-id-hash8>.<tag>`. The hash keeps two stories' tags apart. In `match` and `carries`, the author writes `letter^watch_list`, and the runner expands it to the hashed form when it builds the phase (`effective_phase`), so authors never see the hash.
- **Base id:** `items::baseOf` (`RatwItems.h:314`) and the 19 sites that split on `"~@"` use one helper that also stops at `^`. This is a careful sweep; it is listed as its own test gate.
- **Name:** `Game::storyItemNames_[id] = name` gives the display name in the pack, the trade window and gifts. It is saved in the checkpoint. A tagged item can't be sold to shops or stalls (refused: "That belongs to a story."), can't be crafted from, and has no wear.
- **Undo:** unless `keep`, at run end `endStoryWorld` takes every unit of the run's tags out of every account and every drop on the ground (`held.tags`). Kept ones stay with their name.
- The facts' `carries` (`RatwGameStoryFacts.cpp:141`) counts tagged items under their full id when the watch names it, and under the base otherwise (as today).

*Checks:* the tag pattern; `name` needs `tag`; `references()` checks the base item as today.

*Where:*
- the catalogue;
- `story_program.matches` (the `^` split beside `~@`);
- `story_runner.effective_phase` and `references`;
- `RatwItems.cpp`/`.h` (`baseOf`), the 19 sites, `RatwGameStoryWorld.cpp`, the shop and craft refusals, `RatwGameStoryFacts.cpp`;
- the client pack (display name from the item record the server sends).

*Tests:*
- `Tests/market_tests.cpp` (and `shops_tests.cpp` for the sale refusal): `baseOf("letter^ab12cd34.watch_list") == "letter"`, plus each split site.
- `Tests/story_world_tests.cpp`: give, gift to a resident matches the objective, sale refused, run end takes back, keep stays.
- `test_story_v2.py`: rehearsal match on `letter^watch_list`.
- The client pack test: the name shown.

*Guide:* §7, "Story items".

### L1. `hunt.stage`: an animal waiting in a story region (Phase 5; replaces M1)

*Catalogue entry* (effects; group `places`):
```json
{"id": "hunt.stage", "group": "places", "title": "An animal waits for the next hunt",
 "sentence": "The next hunt in {region} meets {count} {species} (for {minutes} minutes)",
 "help": "The next hunt a wolf begins inside the region has this animal in its arena, as well as whatever the ground gives. The kill is an ordinary 'hunted' with the animal as target. It lapses after the minutes, or when the story ends.",
 "params": [
  {"key": "region", "type": "region", "label": "the region"},
  {"key": "species", "type": "species", "label": "the animal"},
  {"key": "count", "type": "int", "label": "how many", "min": 1, "max": 3},
  {"key": "minutes", "type": "int", "label": "minutes", "min": 1, "max": 1440}]}
```
New param type `species`: "An animal of Data/Wild/animals.json (its id)". `refs()` lists them; `pickers.tsx` gets a species picker.

*Semantics:*
1. The game keeps `stagedHunts_` entries `{run, cell, rect, species, count, until}`, at most 8 per run.
2. When a wolf begins a hunt (`World::startHunt`, `RatwHunt.cpp:241`) on a tile inside a live entry's rectangle, the staged animals are **added** to the arena's line-up the ground gives. (The `only` list at :241, which the dev `/hunt` command uses, shows the arena can be built from named game. L1 adds to the ground's list and does not replace it.) The entry is then used up: one hunt per stage.
3. The kill raises the usual `hunted` (target the species), so objectives stay as written.
4. Nothing roams the map: the animal exists only in that one hunting battle. So no wild population is made and none is left behind.
- **Undo:** `held.hunts`; `endStoryWorld` drops the run's entries; `tickStoryWorld` drops lapsed ones.
- Flags: none (not W, not B).

*Checks:* the species must be in `animals.json`; the region must be the program's own. A species whose habitats (`animals.json` `habitats`) match none of the region's ground is a **warning** ("a bear on open water"), not a refusal.

*Use:* `lq-old-brown` (the bear at Greytarn, for a party that enters at dusk), `lq-blight-and-boar` (a boar in the rows), medium Q13 (the bear in the garth).

*Fallback until Phase 5:* the animal objective is optional and waits on the wild (bears are rarity 1); medium Q13 offers the honey path only.

*Tests:* `Tests/hunt_tests.cpp` (a staged bear joins the line-up once; a hunt outside the rectangle has none; lapse; run end); `test_story_effects.py` (the order, the species check); `test_story_v2.py` (a rehearsal `hunted` bear completes the objective).

*Guide:* §7, "A beast for the hunt".

### L2. A story camp's name: `camp` on `story.area.spawn` and `bandits.call` (Phase 2; was M4)

*Catalogue:* both effects gain
```json
{"key": "camp", "type": "camp", "label": "the camp's name", "optional": true, "words": {"": "a camp of its own"}}
```
New param type `camp`: "A story camp's name: lower-case letters, digits and _, 3 to 30, beginning with the story's prefix (nh_, sq_, mq_, lq_, st_)".

*Semantics:*
- Today `World::storyBandits` (`Core/RatwRoads.cpp:1608`) names the camp `camp_story_<n>` (:1622) and the DM path names it `camp_dm_<n>` (:1597), so an author cannot know the id.
- With `camp: "nh_alder_mere"`, the camp id is **`camp_story_nh_alder_mere`**, for both effects.
- **While that camp stands, a second spawn with the same name adds to it** (its bandits join the same camp, at the new rectangle or around the wolf). **Once it is cleared, the name may be used again**, and a new camp of that id is made.
- `bandit falls` and `camp cleared` then carry that id as `target`, so objectives and triggers match it exactly.
- Without `camp`, today's numbering is unchanged.

*Checks* (one rule, replacing medium M4's `{3,40}` and large L2's "unique per run"):
- `[a-z0-9_]{3,30}`;
- it must begin with the program's prefix followed by `_`. The prefix is the program's top-level `prefix` (2–8 lower-case letters; D2 uses the same field for talk keys). A program that uses `camp` must declare it. The design set's prefixes: `nh` (the campaign and `nh-purses`), `sq` (small), `mq` (medium), `lq` (large), `st` (stage);
- V1's objective check accepts `camp_story_<name>` for any name the program spawns.

*Fallback until Phase 2:* match `cell` plus the story switch `spawned_<region>`, set when the chapter spawns and cleared when it scores (decisions §4.1).

*Where:* `World::storyBandits` and the `bandits.call` branch (`RatwRoads.cpp:1590-1625`); `applyDmAction`'s two branches; `story_program.validate_effect`; the catalogue.

*Tests:* `Tests/roads_tests.cpp` (or `camp_tests.cpp`): the named id; a second spawn adds to the standing camp; a cleared name is reused; `camp cleared` carries the id. `test_story_program.py`: a name without the prefix is refused.

### L4. `npc.sentence`: a hold that outlives the run (Phase 8)

*Catalogue entry* (effects; `needsBuilder`):
```json
{"id": "npc.sentence", "group": "residents", "title": "Sentence a resident", "needsBuilder": true,
 "sentence": "{npc} is held at {place} for {days} days",
 "help": "The resident is taken to the place and kept there for the days, even after the story ends. They keep their post and their home. Never a protected resident.",
 "params": [
  {"key": "npc", "type": "resident", "label": "the resident", "operand": true},
  {"key": "place", "type": "place", "label": "the place (an interior)"},
  {"key": "days", "type": "int", "label": "days", "min": 1, "max": 28}]}
```
*New events* (group `town`): `sentence begun` and `sentence served` (`actor` the resident, `cell` the place, `quantity` the days).

*Semantics:*
- The game keeps `sentences_[npc] = {place, until, by: program id}`, saved in the checkpoint. It is **not** in `Storyline::held`, so `endStoryWorld` leaves it.
- The resident is moved and held there through the same hold path as `npc.hold`, re-asserted in `tickStoryWorld` (501). At `until` the hold ends by itself, and they walk home.
- Refused on a resident who is protected, dead, in a fight, away with a party, or already sentenced.
- The console's Campaigns panel lists sentences, with "Release now".

*Checks:* `references()` checks `npc` (alive and placed: V1) and `place` (an interior reachable by links).

*Use:* `lq-wreckers-light`'s week in the Watch cells for Ilse Coalley (`rm_ilse_coalley`), at `r2_the_keep_down`.

*Fallback until Phase 8:* `npc.hold` re-applied at each visit (large §13.3).

*Tests:* `Tests/story_world_tests.cpp`: sentenced, the run ends, still held; served after 7 days; a protected resident refused; checkpoint round trip.

### T3. Strangers in scenes: a cast entry for a visitor (Phase 7)

*Semantics:*
- A scene's `cast` entry may be `{"id": "<role>", "visitor": "<visitor id>"}`, beside `npc` and `selector` (exactly one of the three; `tools/story_scenes.py:72`).
- The visitor must be one the same program adds with `visitor.add`. When the scene starts, the game resolves the role to the visitor's entity (`Core/RatwGameCinematics.cpp:277-293`, the cast resolution). Its face is the visitor's `like` appearance, which the client already paints from looks. The speaker gets **their own panel in the dialogue window**, like any resident speaker.
- If the visitor is not in the world when the scene starts, the scene fails (`scene failed`, detail "a stranger is not here"), as an absent resident does today.
- **`story.tell from_visitor` is not built** (§0.5): a face banner is not the dialogue window.

*Checks:* `story_scenes` validation: the visitor id must be added by a `visitor.add` in the same program (a warning if only in a later phase).

*Use:* the Grey Captain at the parley, Hakon in the Turn, the Envoy's silent presence (the campaign's three strangers; their `like` residents are in decisions §1.6).

*Fallback until Phase 7:* the residents in the scene (Desmond, Gorm) speak, and the strangers appear in plain narration (`story.tell` with no `from` and no quoted speech).

*Tests:* `Tests/talk_tests.cpp`, beside the existing scene cases (a visitor speaks in a panel with the like face; absent visitor fails the scene); `test_story_scenes.py` (the cast form).

### M5. `cell` on `trade done` (Phase 2)

- The game already writes the wolf's cell into the event (`RatwGame.cpp:6146`: `recordEvent({"trade done", id, target, player->cellId, ...})`), but the catalogue entry has no `cell` field, so neither `match` nor SQ1 `where` can use it.
- **Fix:** add `"cell": {"type": "place", "label": "where the wolf is", "any": "anywhere"}` to `trade done`'s fields, and `"tile": false`. Check that the facts intake passes it through (it does for every event with a cell).
- *Use:* medium Q22 ("sell it in the city", not in Frosthollow).
- *Fallback until Phase 2:* a `wolf in region` check in a trigger on the sale.
- *Tests:* `test_story_quests.py` (a `trade done` objective with `match.cell`); `test_story_catalogue.py`.

### SQ3. Lend a paw to a resident at work (Phase 9)

- Today `World::mayLend` (`Core/RatwTogether.cpp:221-232`) refuses when either side is a resident (`h->npc || w->npc`), so `together` only ever pairs two wolves.
- **Proposal:** a wolf may lend a paw to a resident who is `atWork`, within `lendTiles`. The joint's beats are the resident's work activity. Its end writes `together {actor: wolf, target: resident, item: activity, quantity: beats}`.
- Rules: once a game day per pair (as `shared meal`); no wages from the resident's purse; the resident's own work output unchanged (so the economy's balance, docs 42 and 64, does not move).
- **Built in Phase 9** (answer (a)). It changes a game system that the economy sessions own, so it moves no coin and leaves the resident's output unchanged (no economy sim is needed). The packs use the fallback until it lands.
- *Fallback:* `gift`, `trade done` or `conversation` objectives at the resident (small 5, 10, 13, 30, 40).
- *Tests:* `Tests/together_tests.cpp` (a wolf beside a working carpenter; once a day; no coin moves).

### M3. House purses (Phase 9)

- The four Houses have no account a story can name (the checkpoint has no `house:*` account).
- **Proposal:** `house:<faction>` purses, mapped by `Game::storyPurseAccount` to an account each House keeps, filled by its businesses' tills (doc 61: Houses own their trades).
- **Built in Phase 9** (answer (a)). It adds a money flow to the economy (docs 42, 61, 64), so its economy check (one short run on a scratch world: a House's purse filled by its tills for 30 game days, with town purses and Gini compared against the same run without it) is a **simulation the user has not yet approved**. Answer (c) covers combat sims only. The Phase 9 agent asks before running it (memory: ask before simulations). Until then M3 is built and tested, but `house:` purses are refused at approval by a world switch, `storyHousePurses` (off).
- *Fallback:* the treasury, or after SQ2 a House business's own till (for Greatly, `till:job:rm_eleanor_saltcot`, the Greatly Bank), within the decisions §3 counters.
- *Tests:* `Tests/story_world_tests.cpp` (a `house:` purse maps to its House's account; a grant moves its cash); `test_story_problems.py` (refused while `storyHousePurses` is off; accepted, and its account checked as SQ2 rule 3, when on).

---

## 5. The dialogue window

### N1. Story choices overflow into "Ask about…" (also T8)

*Semantics:*
- Today, story offers, hand-ins and choices that don't fit the hub's 9 are dropped (`RatwGameWindowTalk.cpp:377`).
- New: when the room is short, hand-ins come first, then offers, then choices, in story-run order. What doesn't fit goes under the resident tree's `about` node, as mantle grafts already do (the `place` / `room` helpers at 405–420).
- When `about` is used, the hub gets one "Ask about something else…" choice, if it has none.
- Nothing is dropped unless the `about` node is full too (9). The game logs the drop once per resident per day (`note("warn", ...)`), so the console's Problems can show giver crowding.

*Tests:* `Tests/dialogue_tests.cpp`: 12 story choices at one giver give 9 on the hub path and the rest under about; the hand-in is always on the hub.

*Guide:* §7 "Many stories at one resident"; §16 limits.

*Fallback until Phase 4:* the giver budget: at most one pack quest per resident across all packs (`givers.tsv`, decisions §1.2), and 2 campaign offers per resident per chapter.

### C6 display side
See §3 C6: the `during` node, and the `players` check in `storyOfferOpen`.

### T5. `npc.line` in the speaker's voice (Phase 3)

*Semantics:*
- Today a phase's `npc.line` is kept as the cast part's `looks` and appended raw to the window's opening line: `Game::worldStoryLine` (`RatwGameStorylines.cpp:861`), called at `RatwGameDialogue.cpp:1233`.
- New: the joined line goes through `inVoice(npcId, playerId, line, q.facts)` (`RatwGameWindowTalk.cpp:847`) before it is appended, so `#symbols#` fill in the resident's dialect, as offer words already do.
- The `npc.line` text limit stays 200, and V2 checks its symbols.

*Fallback until Phase 3:* the dialect written out by hand (the speaker is fixed), with no `#symbols#`.

*Tests:* `Tests/dialogue_tests.cpp` (a `#greet#` in an `npc.line` fills in the resident's dialect).

### T6. A `story` talk topic (Phase 9)

- A `story` topic the director can pick, so NPC-to-NPC scenes can open on the campaign itself instead of riding `news`, `gossip` or `day`. It is a new entry in `Data/Talk/topics.json` and `topics::sources()` (`Core/RatwTopics.h:90`), live while any story has set a realm key with the program's prefix.
- **Built in Phase 9** (answer (a)). A new topic moves the talk scorecard, so the Phase 9 agent re-runs the scorecard and the golden tests and records the moved numbers (the test town has no `nh_` key, so the topic stays silent there).
- *Fallback:* ride `news` / `gossip` / `day`, gated on `world.mem.nh_*`.

---

## 6. Approval and tooling

### R3. The Campaigns console: readiness bars, Advance, Hold; the guide (Phase 1a)

What the admin sees and presses for answer (a). It is built on doc 85's console (`Editor/src/dm/CampaignsTab.tsx`, `Editor/src/dm/campaigns/*`, `Editor/src/lib/campaigns.ts`).

**Already there** (doc 85 as built; nothing to add):
- **Hold** and **Resume** (with or without catch-up) in `SteerBar` (`Steer.tsx:69-77`), and **Hold all…** in the list (`CampaignList.tsx:53`, `CampaignsTab.tsx:105`);
- **Go to phase…** (any phase, with or without entry effects), **Send signal**, **Tell the wolves…** and **Move onto a new version…**;
- each control asks first (`AskDialog`: a reason, a dry run, and the typed title for the risky ones), and is shown in the Timeline and the Audit;
- the alerts list (`camp-alerts`) and `needsYou`.

**New in `Editor/src/lib/campaigns.ts`:**
- the types `Readiness {met, auto, to, metAt, pushed, waitingFor, slow, parts: ReadyPart[], heldDays}` and `ReadyPart {kind, label, met, value, need, of, share, days, any?: ReadyPart[]}`;
- `RunRow.ready?: Readiness` and `RunDetail.ready`;
- `Alert.kind` gains `'ready' | 'slow'`, and `alertText` gives "Ready to advance: ch1_main → ch1_end since day 93" and "Slow: 21% of the 35% needed after 31 days";
- `CommandKind` gains `'advance'`; `commandLine('advance', …)` gives "Advance to ch1_end now (ready)", "… anyway (not ready: 2 of 4 parts met)" or "… at the next new moon";
- `typedTitle` asks for the title when `anyway` is set;
- `readyBars(r, names): {label, fill, mark, text, met}[]`: one bar per part, with `fill` the progress (0–1) and `mark` the need. For participation the text is "42 of 118 active wolves (36%); 35% and at least 5 needed"; for time, "11.2 of 10 days in the chapter (1.5 held days not counted)";
- `waitingLines` gains `{on: 'ready'}`: "waits for an admin to advance" / "goes on by itself when ready" / "waits for the next day after ready";
- `listCounts.needsYou` counts the `ready` alert.

**New `Editor/src/dm/campaigns/Readiness.tsx`**, on the run's **Overview** tab under the phase strip (`CampaignView.tsx`), shown whenever the phase has a `ready` block:
- **A header line**: green "Ready to advance to ch1_end (since day 93)", grey "Not ready: 2 of 4 met", "Waiting for the next new moon (ready since day 93)", or "Goes on by itself when ready".
- **The bars**, one per part, from `readyBars`. Above the participation bar: "active 118 · in the story 260 · online now 31" (`audience`, C2) and the `activeDays` window ("seen in the last 18 game days, 3 real days").
- **The buttons**:
  - **Advance…**: primary when met. It opens `AskDialog` with `{kind: 'advance', payload: {when: 'now'}}`, its dry run (the destination's entry effects) and the readiness snapshot listed under "This will do".
  - **Advance anyway…**: shown when not met, with `anyway: true`, a warning "Not ready: …", and the typed title.
  - **Advance at the next <anchor>…**: shown when the block has `then`.
  - **⏸ Hold / ▶ Resume** beside them. These are the same commands as the SteerBar's, with the line "Holding stops the chapter's clock: held days do not count toward its days".
  - The buttons follow `maySteer` and, on a hosted world, the owner or "may steer campaigns" (5.7). `_may_release` can refuse with its own words.
- `ready` items in the **Timeline** (`Timeline.tsx`): "ready", "no longer ready" (before the latch), "advanced by <who>: <reason>".

**The list** (`CampaignList.tsx`):
- a **Ready** column with a compact bar of the furthest part and a green "ready" tag;
- the alerts list puts "ready to advance" first (needsYou);
- the Campaigns tab title shows the count: "Campaigns (2 need you)".

**The chapters bar** (`Chapters.tsx`, `ChapterBar`): the running chapter of each campaign shows its readiness summary ("Ch 1 · 36% of 35% · day 11 of 10 · ready") and an "Advance…" link that opens that run on its Overview tab.

**Server ops** (`tools/dungeon_master.py`, ops routed at 2799, through `story_runner`):
- `command` takes `advance` (R2 rule 9);
- `overview` and the run detail carry `ready`;
- `explain` with `command: {kind: 'advance'}` gives the dry run;
- no new op.

*Tests:*
- `Editor/src/lib/campaigns.test.ts`: `readyBars` texts and fills (participation with a floor, time with held days, an `any` group); `alertText` for `ready` and `slow`; `commandLine` for the three advances; `typedTitle` with `anyway`; `listCounts.needsYou` counts `ready`.
- `tools/test_story_console.py`: the overview and the detail carry `ready`, and an `advance` sent through `command` lands in the audit.
- `npm run build` and the Editor's test run (doc 85's end-of-work check).

*Guide* (`Docs/Story-Maker-guide.md`; the DM suite's Guide tab reads it):
- §11 "The Campaigns console" gains **"Moving a chapter on"**:
  - what the bars mean, and what "active" counts;
  - Advance, Advance anyway and Advance at the next moment;
  - Hold stops the clock;
  - the alerts `ready` and `slow`;
  - who may press it;
  - what the audit keeps;
  - a worked example: Ch 1 at 36% of 35% on day 11 of 10, ready, so press Advance with a reason.
- §4 gains "Chapters that move on when the wolves have done enough" (R2's template and the recommended defaults, §8 D10).
- §6 gains "Counting who took part" (R1).
- §15 gains "It says ready but did not move".
- `Docs/story-programs.md` gains "Readiness" (the format).
- The reference section is regenerated from the catalogue (`story_guide.py --write`).

### SQ2. Till purses that pay, and purses that exist (Phase 1b; was M6)

**The bug** (critic P0-1; `quests-small.md` SQ2, `quests-medium.md` M6):
- `story_quests.check_purse` (`tools/story_quests.py:79-86`) accepts `till:<id>` only with two parts (`len(parts) == 2`). The game's 155 till accounts are named `till:job:<founder id>`, so `till:job:rm_eleanor_saltcot` is refused at validation.
- `Game::storyPurseAccount` (`Core/RatwGameStoryFacts.cpp:339`) passes `till:<id>` through unchanged, so the two-part form names no account at all.
- So no merchant's till can pay a story reward today. That includes the campaign's Purse path (`campaign.md` §2.7), all of `quests-large.md`'s restitution and contract payments, and the small and medium shop pay.

**The fix:**
1. `check_purse` accepts `till:job:<id>` (three parts, the second `job`) as well as `till:<id>`. The two refusal messages (`story_program.py:317`, `story_quests.py:241`) name it: "Name a purse that pays: town:<town>, till:job:<resident> or chapter:<id>."
2. `Game::storyPurseAccount` maps `till:<id>` to `till:job:<id>` when `till:<id>` is not an account and `till:job:<id>` is. Both spellings then reach the same account, for `character.grant` (`RatwGameEconomy.cpp:365`), `purse crossed` (`RatwGameStoryFacts.cpp:403`) and M2.
3. **`references()` refuses a purse whose account is not in the latest checkpoint's `society.accounts`** (`game.checkpoints.payload`, read once per approval as `payload::jsonb -> 'society' -> 'accounts'`, then looked up for each purse the program names). The account is the one `storyPurseAccount` would map to:
   - `town:<id>` → its town's store (`stores:<id>`; the city's is `treasury`);
   - `till:<id>` / `till:job:<id>` → `till:job:<id>`;
   - `town:<id>:works|watch|church` → that name, **which no world has today**, so they are refused ("Ridgemere 3 has no works, watch or church purse: pay from the town").
   - `chapter:<id>` keeps its V1 check (the chapters table).
   - No checkpoint yet (a new world): a warning, not a refusal.
4. The `purse crossed` help (catalogue) drops "its works, watch or church" and says `till:job:<resident>`.

*Use:* once SQ2 lands, a new version of each program swaps its treasury or town-store payment for the till (decisions §3.2.5). The tills and their balances at the checkpoint: the Greatly Bank `till:job:rm_eleanor_saltcot` 161, Vane & Daughters `till:job:rm_osric_ropeworth` 125, Grayrock Shipwrights `till:job:rm_brenna_slaggate` 156, Silver & Rain `till:job:rm_vigdis_rainmore` 112, Hollis Physic `till:job:rm_rupert_fogton` 90, the Fish Hall `till:job:rm_corra_coalcot` 141, Staves & Hoops `till:job:rm_edric_millton` 197. (The Lamp Store's `till:job:rm_ilse_coalley`, 126, is **not used**: decisions §2.2.) Every payment stays inside the till's `coin_till_<id>_day` cap and the floor of 60 (decisions §3.2).

*Fallback until Phase 1b:* every till payment names the treasury (city) or the town store, with the same counters; the reason text still reads as the shop paying ("Hollis Physic pays it back").

*Tests:* `test_story_quests.py` (`check_purse('till:job:rm_ilse_coalley')` is true; `till:job:` alone and `till:a:b` false); `Tests/story_world_tests.cpp` (`till:x` and `till:job:x` reach the same account; a grant moves its cash); `test_story_effects.py` / `test_story_problems.py` with DSN (an existing till passes; `till:job:nobody` and `town:coldwater:works` are refused; a world without a checkpoint warns).

### V1. Approval checks what it misses today
Extend `story_runner.references` (83). Each is a refusal unless marked as a warning, and each comes with the world's word for it.
1. **Quest objective `match` ids.** For objectives on `conversation`, `gift`, `gift again`, `near resident`, `enter place`, `camp cleared`, `bandit falls` and `foe *`:
   - a target must be a character (resident or wolf), a place, a camp id that exists (`camp_<cell>` with that cell in `world.cells`, or `camp_story_<name>` for a name the program spawns with L2), or a foe of the program;
   - an `item` must be in `Data/Items/items.json` (base id).
2. **`character.grant` purses:** as SQ2 rule 3 (the account must exist in the checkpoint). This replaces the old rule 2, which let `town:<id>:works|watch|church` and any `till:` through.
3. **A quest's `board`** must be one of the eight towns with a board: the world's notice boards (`Game` boards are per market town). Checked against `refs()['markets']` (8).
4. **Unplaced or dead residents.** Any resident named as `npc`, `giver`, `from`, `like`, `subject`, a scene cast or an objective target must:
   - have a `home_area` or a `work_area` in `dm.story_residents` (otherwise "<id> has no place in the world yet"), **and**
   - be alive: `live.npc_state.alive` is not false (otherwise "<id> has died"). `dm.story_residents` has no `alive` column, so the check joins `live.npc_state` on `(world_id, npc_id)`. On Ridgemere 3, 3 placed residents are dead, among them `rm_gunnar_harrowgate` (the dead King) and `tn_cw_iris_greywater` (the dead potter).
   - Exceptions:
     - `like`: an unplaced or dead resident is a warning (they have no looks, or old looks);
     - a dead resident named **only** as a `death` match `actor`, or only in words, is allowed: a program may speak of the dead. The campaign names Gunnar this way.
   - The `npc.kill` and `foe.outlaw` targets are also checked alive (a warning: they may die before the effect runs; the game refuses then and `ifRefused` applies).
5. **`program_problems(..., with_world=True)`** (`story_runner.py:4648`) runs `references()` and reports its refusals as problems, so the Editor's Problems panel shows them (gap (d) in the story-format survey).

*Tests:* `test_story_effects.py::test_the_worlds_references_are_checked_at_approval` and `test_story_problems.py` (with DSN): one case per rule, including a dead giver refused and a dead `death` target allowed, plus "a pack with all of it right passes".

*Until Phase 1b:* each reviser runs the giver check and the placed-alive-reachable query by hand (decisions §1.2.7, §1).

### V2. A check for story words (also T4)
New `tools/story_words.py` (CLI and `story_author.py` op `words`), run by approval as **warnings** and by the Editor's Problems panel. It reads every word field of a program: offers (`say`, `thanks`, `during`, `ask`, …), `npc.offer` words and reply, `npc.line`, `story.tell`, `board.post`, `journal.note`, `npc.greet`, quest titles, summaries and labels, `deed.award` details, titles, claims, and scene lines. It also runs over a directory of programs (`--stories DIR`), as T4 asked of `dialect_check`. It checks:
1. **Symbols, through the talk expander:** every `#symbol#` exists in the talk library, using the expander's symbol table (`talk_check` loader); a stray `[` and an unclosed `#` are refused. Fills `{x}` are left to C1b.
2. **Forbidden words** (refusals, not warnings):
   - `dog`, `dogs`, `doggy`, `puppy`, `puppies` (young are pups);
   - `god`, `gods`, `goddess`, `deity`, `deities` (only the Bright One and the Dark One);
   - `tell`/`tells` used as a noun for private messages (the phrase "send a tell", "a tell from");
   - each as a whole word, any case.
   The list is in `Data/Storylines/words.json`, shared with V2's Editor use.
3. **Dialect by the giver:** a `say` / `thanks` / `during` / `reply` / `npc.line` / `npc.greet` / scene line spoken by a resident whose dialect is known (`live.npc_voice` home region and accent; "none" is plain, Fenshore-raised speak mirebridge speech) and that contains another dialect's set phrases (`dialect_check`'s lexicon) → warning. An offer's words are the giver's.
4. **Ids in speech:** any token matching a resident id, place id or `wolf-…` id in a spoken text → refusal.
5. **Lengths** after fills (C1b): 300 for offer words, 120 for asks, 200 for `npc.line` and greetings.
6. **Face banners and speech aloud** (decisions §9, P2-22): a `story.tell` with `from` naming a resident is a warning, and a refusal when its text holds quoted speech ("…" or '…'). An `npc.speak` whose words are addressed to a wolf is a warning. Move the words to the window (§0.5).
7. **T7 claims:** a `story.cleared` phrase that begins with "never" or "did not" → warning.

*Until Phase 3:* the revisers run `tools/dialect_check.py --world ridgemere3`, `build-core/talk_check --data "$PWD/Data/Talk" --validate` and `--render-trees`, and `python3 tools/talk_lint.py` on the written files, and check story texts by eye (decisions §9, P2-24).

*Tests:* `tools/test_story_words.py`: each rule; the bundled examples pass, so `test_story_native`'s 108 count is unchanged.

### V3. Guide fix
`Docs/Story-Maker-guide.md` §7 uses `{"on":"death","match":{"target":"rm111"}}`. The `death` event has only `actor`. Change it to `actor` and add a sentence. Also replace the Greyfen ids in §2 and §7 with a note that examples use the test town.

### T9. Allow-list words (a data edit, Phase 0)
`cast-and-talk.md` §5.6's place names and words go into `tools/dialect_allow.json`, so `dialect_check` stops flagging the campaign's capitalised places in tree text. No code; no phase hours. It ships with the talk library files.

---

## 7. Build phases

Each phase is one agent's work in sequence (memory: lean workflow cadence). It reuses one build (`build-core`), runs **targeted** tests at its end, and leaves the tree green. The full suite, the end-to-end check and one review-and-fix run once, at the end of the slice, **after Phase 9**: everything is built (answer (a)), so there is one slice and one end check. The exception is Phase 1a, which also runs its own end-to-end, because Act I's chapters are paced by it.

**Every phase also:**
1. Regenerates the guide reference (`story_guide.py --write`) and runs `story_guide.py --check`.
2. Runs `test_story_catalogue.py`, `test_story_effects.py`, `test_story_native.py` (with `libratw_story.so` rebuilt), `workbench.test.ts` and `guide.test.ts`.
3. Adds the hand-written guide sections named in its items.
4. Rehearses one sample program in `tools/story_v2_samples.py` that uses every new feature of the phase (`review(p)['passed']`, nothing uncovered).
5. Never touches `ratw_prod`. DEV-world checks run on a scratch server or `ridgemere3` read only (CLAUDE.md). Live checks on Ridgemere 3's server happen only when the user asks.

### 7.1 Phase 0: today-only content (no engine change)

What can go live **before any engine change**, on the code at `17f6db3b`. It is content work by the design revisers, approved and published by the DM through the existing Campaigns console. It has no engine hours. It is the plan for **Act I**. There is **no target publication date** (answer (a); decisions §6): Act I ships whenever the DM publishes it, in any season, and Ch 1's chalk night is chapter game day 3, or a new moon on chapter game days 2–5 (campaign §3.2.4).

**Phase 1a may make the stand-in unneeded.** Phase 1a (readiness, about 21 agent-hours) is the first engine phase. If it lands and passes before Act I is published (about 2 real days from 2026-11-22), Act I ships with `ready` blocks (R2), and the pacing stand-in below is not used. Otherwise Act I ships with the stand-in, and moves onto a version with `ready` blocks by `migrate` as soon as Phase 1a lands. A run can move mid-chapter: doc 85, 5b lands it in the same phase.

**Goes live in Phase 0:**

| Content | Built to | How it works today |
|---|---|---|
| `nh-purses` (the counter program) | campaign §2.11, decisions §3.2 | `world-var declare` for `coin_<purse>_day` and `coin_<purse>_ok`; `day begins` resets the counters; `purse crossed` (down past 60, up past 80) flips the switch. Every paying program reads and adds. Publish it first. |
| Act I, Ch 1–3 | campaign §11.4 (today-only rows 1–3) | untargeted `npc.offer` with `once: true`; resident words only in offers, `npc.offer` replies, `npc.line` (dialect written out), scenes and tree lines; camps matched by `cell` + `spawned_<region>`; "break the camp" as a trigger + `objective.complete`; treasury and town-store coin only, under the counters; nothing computed from the publication day: each set piece is timed from its own closing phase (decisions §6.3, campaign §3.2.4). |
| Small pack, today version | quests-small §3.1 | fixed givers from `givers.tsv`; board-only quests (`sq-rope-for-the-drag`, `sq-lamp-oil-for-the-pit`, `st-pull-stake`); SQ1/SQ5 by trigger + `objective.progress`; town coin 2–4 under the counters. |
| Medium pack, today version | quests-medium §5.3 | Q13 offers the honey path only; fights by cell; House- and shop-paid coin from the towns and the treasury. |
| Large pack, today version | quests-large §13.3 | bear and boar objectives optional; camp and world-event objectives through triggers; roads withdrawn with `quest.fail` "another road taken"; Ilse's hold re-applied at each visit; `closure: true` written now (inert until J3). |
| Stage pack `nh-pack.stage` | decisions §7.3; small §2.10, medium §4b, large §12b | phases `quiet`, `s12`, `s3` live with Act I; the later phases' quests are already in the program and open when `nh_stage` reaches them. It needs no engine item beyond its tiers'. |
| Talk library (cast-and-talk §3, §4) and the T9 allow-list | Data/Talk, `tools/dialect_allow.json` | every line is gated on `world.mem.nh_*`, so it is inert until a key is set. The DM seeds `nh_stage` and the other realm and town keys by hand, and `nh_debts=grace` at day 88.6 if Ch 1 is not live by then (decisions §6.4). `pair.mem.*`, `self.mem.*`, `player.quest.*` and `player.story.*` lines stay silent until Phase 1b. The talk scorecard and golden tests do not move (the test town has no `nh_` memory). |
| Figures and protection | campaign §8.6, §11.5 | DM `save_figure` (`dungeon_master.py:837`) per stage; the authored `protected` flag on the old cast and both new fallback Keepers, as a listed DM data edit. |
| Scaling rules | decisions §8 | the `join.cap: 1` quests are `repeat: weekly` with `fail: {afterDays: 1}`; large chapter 1 `join.cap` 40, fight chapters 12 over two spawn regions. |
| Chapter pacing (answer (a)), the stand-in until Phase 1a | campaign §3.2 (each chapter's least days become its `minDays`) | Each chapter's working phase ends by a `dm signal` direction exit, "Move the chapter on" (doc 85, Phase 7), beside doc 85's `percent` exits where they fit. There are **no latest-end `clock` exits**. The chapter keeps a story `wolves` list of who did its work (`ch<n>_did`), shown on the Variables tab as a count. The DM sends the direction when about the chapter's share of the active wolves (§8 D10) is in the list and its least days have passed; the chapter's note in the console states that bar. |

**Cannot go live in Phase 0** (and what stands in):

| Needs | Stand-in until then |
|---|---|
| a till as a purse (SQ2) | the treasury or town store, reason text naming the shop |
| per-wolf choices (J1), pair memory (C8), quest and story window facts (N2) | untargeted offers; generic replies |
| named thanks (C5), titles (C4), ranks (C3) | `deed.award` phrases, pre-written bands |
| a staged bear or boar (L1) | optional animal objectives |
| foes (C10), strangers speaking in scenes (T3) | anonymous bandits; strangers in plain narration |
| tagged items (J9) | generic items |
| closure counting (J3) | `closure` is written (`"line"` on a large line's last chapter, `"side"` on a campaign side story; `true` reads as `"line"`) but inert |
| readiness bars, Advance, the `ready` alert (R1–R3) | a `dm signal` direction exit per chapter, the Variables tab's `ch<n>_did` count, and the DM's eye (the pacing row above) |

**The checks before each Phase 0 publication** (decisions §1.2.7, §9 P2-24), until V1, SQ2 and V2 do them:
1. the §1 placed-alive-reachable query for every named resident, and the giver check on `givers.tsv`;
2. every purse looked up in the latest checkpoint's `society.accounts` (no `till:`, no `town:*:works|watch|church`);
3. `tools/dialect_check.py --world ridgemere3`; `build-core/talk_check --data "$PWD/Data/Talk" --validate` and `--render-trees`; `python3 tools/talk_lint.py`;
4. `story_program.review(p)`, `problems(p) == []` and `story_runner.references(conn, 'upper_accord', p)`;
5. a read for the forbidden words and for any `story.tell` with `from` and a resident's quoted words.

### 7.2 The phase table

| # | Phase | Items | Size (agent-hours) | Independent test gate | What can ship after it |
|---|---|---|---|---|---|
| 0 | **Today-only content** | content only; T9 (data edit) | — | §7.1's checks | **Act I** and every pack's today version (§7.1), paced by the direction-exit stand-in |
| 1a | **Readiness and participation** (answer (a); first) | C2 (`audience`, `activeDays`, the seeded `seen`; `times`, `div`), R1, R2, R3 | **21** | Python: `test_story_v2` (C2 and R1 forms), `test_story_native` parity (every R1 form and the condition), `test_story_speed` (R1 at 5,000; readiness in the tick budget), `test_story_program` (`ready` shapes), `test_story_v2_runner` (DSN: readiness, latch, auto, `then`, the held clock, `slow`, `autoAfterDays`, migrate keeps `metAt`), `test_story_steering` (`advance` now, anyway, then; refusals; the audit snapshot), `test_story_explain` (no `stuck` on a ready phase), `test_story_campaigns` (Ch 1 Advance → Ch 2 with `seen` seeded), `test_story_console`. Editor: `campaigns.test.ts`, `npm run build`. Guide: `story_guide.py --check`. **Its own end-to-end**: a throwaway migrated DB and a scratch `ridgemere3` server; approve and publish a two-chapter sample campaign (Ch 1: participation 35% of active, floor 3, `minDays` 2, `auto: false`; Ch 2: `auto: true` with `then: day begins`); six wolves' events move the bars in the console; the `ready` alert shows; Advance with a reason; the audit row holds the snapshot; Ch 2 opens and moves by itself. | **Every chapter is paced by participation plus time, and pushed by an admin** (answer (a)). Act I ships on it (or moves onto it by `migrate`). Packs may use R1 in triggers ("half the town turned out"). The garland rope's audience goal (small #38). |
| 1b | **Foundations, purses and Act I's talk** (was Phase 1) | V1 (+ alive), SQ2, V3, J1, C11 (+ `protect lasts`), C9, C8 (+ pair scope), T7, N2 (+ `player.story`) | **26.5** | Python: `test_story_effects`, `test_story_quests` (`check_purse` forms; the views' `talk` map), `test_story_problems` (a dead giver; a missing account; a works purse), `test_story_campaigns` (DSN). C++: `story_world_tests` (memory incl. pair, spares, till mapping), `dialogue_tests` (J1 players, N2 both facts, a `world.mem` and a `pair.mem` line play), the belief tests (`story.rumour`, `story.cleared`). Sample: a two-phase program that pledges two wolves (J1), spares and protects for the campaign, sets a villain figure, remembers a realm key and a pair key, plants a rumour and a clearing, and pays from a till; rehearses and passes `references()` on a throwaway migrated DB. | The running Act I moves to a new version at a chapter boundary (or mid-chapter, by `migrate`): per-wolf offers, gossip without DM seeding, pair greetings, figures by the story, **till payments**. Pack authors get honest approval (V1, SQ2). **Small packs complete.** |
| 2 | **Values and objectives** | C1 (a–d), M2, C12b, N3, J4, SQ1, SQ5, L3, L2, M5, N4 | **25** | `test_story_v2`, `test_story_native` parity (every new operand and match form, `purse`), `test_story_quests` (goal operand, `showOf`, mirror, `where`, `credit` target/present, the six mission events, `join.again`), `story_facts_tests` (moon), `roads_tests` (named camps), `Client/src/game/quests.test.ts` (a / b tracker), `test_story_program` (R2's `then: moon` now accepted). Sample: a story-scope bar with `goal = max(12, times(audience active, 4))`, a board post with a fill, a `starts` match on a deed, a `ready` block whose `then` is the next new moon, a named camp cleared as a plain objective, and a wagon objective with `credit: present`. | Act I at full strength except titles and thanks; **packs scale** goals and pay to the purse; the moon as optional set-piece timing (Ch 2's Reckoning, Ch 4's relay) and as an R2 `then` anchor for other campaigns; camp, wagon and festival objectives are plain objectives in new versions; the `join.cap: 1` quests get `join.again: 7`. |
| 3 | **Thanks, titles, ranks and words** | C5, C4, C3 (a, b), J11, J8, V2 (+ T4), T5 | **27** | `dialogue_tests` (greet once, in voice; `npc.line` in voice), `story_world_tests` (titles, doubt), `test_story_v2` (rank, tally at 1,200), `test_story_speed` (tally at 5,000), `test_story_quests` (shares), `test_story_words` (incl. face banners and the giver's dialect), client card test, `talk_check --validate` on a `#player.title#` line. | **Act I complete as designed.** Recognition tiers without the 500 cap. Story words checked at approval. |
| 4 | **Stories that talk to each other** | C12a, C13a, C13b, C6, N1 (= T8), J3 (capped, answer (b)), J10, L5 | **25** | `test_story_campaigns` (pack notify → chapter; `variable changed` delivery and coalescing; a `side` closure in a chapter, refused alone), `story_facts_tests` (month, post, gift), `dialogue_tests` (requires players, during, overflow), `storyline_tests` (closure receipts; the fifth refused per wolf per program), `test_story_quests` (requires gates, withdraw, the `line` chain rule, `true` read as `line`). | Packs feed chapters directly; chained quests without giver leaks; **large lines' last chapters and campaign side stories count toward the Quickening** (answer (b)); `month begins` anchors for readiness; **medium packs complete**; **Act II minus howls and items**. |
| 5 | **Howls, crowds and the wild** | J5, J6, L1 | **13** | `howl_tests` (resident howl, chorus events, cooldown), `test_story_facts_live` (residents list watched and re-sent), parity for `count in region` residents, `hunt_tests` (a staged bear once; lapse; run end). | Act II's howl quests and the Ch 8 haul as designed; the bear and the boar staged. |
| 6 | **Story items** | J9 | **10** | `market_tests` / `shops_tests` (every `baseOf` site), `story_world_tests` (give, match, refuse sale, take back, keep), the client pack name, `test_story_v2` (match). | **Act II complete** (tagged list and letters); the Ch 8 writ; the Couriers' tube. |
| 7 | **Foes and strangers** | C10 (`foe.add`, `foe.leave`, `foe.outlaw`, events, `foes.json`), **C10t** (tuning sims, answer (c)), T3 | **25** | `battle_tests` / `story_world_tests` (no crime, guards' side, leader rule, events once, time out, undo; outlaw refusals, downed not killed, lethal), `talk_tests` (a visitor speaks in a scene panel), `test_story_effects` (builder), `foe_sim` and `world_check --foes` smoke cases in ctest, and **C10t's bar passed** (up to 4 passes at ≤ 250 players; past that, the user decides). | Ch 7 and Ch 9 fights and the parley as designed, with foes tuned to the bar; **large packs complete** except L4. |
| 8 | **Across acts and the crown** | J2, C14, L4 | **16** | `test_story_v2_runner` (realm load/save across two runs; cursor reload; type clash), `test_story_campaigns` (Act I → Act II per-wolf state), `careers_tests` (the swap), `story_world_tests` (a sentence outlives the run), migration 0081 applied to a throwaway DB. | **Act III complete.** The list fallbacks in `campaign.md` §4.4 can be dropped. Ilse's week is lasting. |
| 9 | **Casting, mantles, queues, paws, House purses, the story topic** (built, answer (a)) | C7, C15, J7, SQ3, SQ4, M3, T6 | **22** | `test_story_quests` (cast and recast; a giver from a variable; `rewardLimit` writes the counters), `test_story_v2_runner` (the cast query, unplaced left out), `story_world_tests` (mantle restore; `house:` purses mapped), `careers_tests` (queue), `together_tests` (a paw lent to a resident; no coin moves), `test_story_problems` (`house:` refused while `storyHousePurses` is off), the talk scorecard and golden tests re-run (T6). **M3's economy check runs only with the user's go-ahead** (§8 D8). | Packs that cast givers in any town; the sequel's Eskar hook; work-beside-them objectives; Houses that pay (once the switch is on). |
| — | **End of slice** (after Phase 9) | full ctest, every Python suite, Editor and client tests and builds, one end-to-end on a scratch `ridgemere3` server with a throwaway migrated DB (approve and publish a sample campaign of two chapters plus a pack; play one pack quest through a pledge, a till payment, a notify, a title and a foe; **move a chapter on by participation and an Advance**; a closure counted), one review-and-fix pass | **7** | everything green; the review's findings fixed | the whole campaign spec, all engine items |
| | **Total** | | **217.5** (Phases 1a–9 and the end of slice) | | |

The totals moved from 191.5 for these reasons:
- R1–R3: +17.5;
- C2: +0.5 (`activeDays` and the seeding);
- J3: +1 (the two kinds and the game's cap);
- C10t: +6;
- the end of slice: +1 (readiness and closure in the end-to-end);
- Phase 9 is no longer deferred (it was in the old total already).

### 7.3 Order and dependencies
- Phase 0 depends on nothing. **Phase 1a depends on nothing** and goes first. Phase 1b depends on nothing and may run beside 1a if two agents are wanted; in sequence it comes second.
- R2 uses R1 and C2. R3 uses R2. All three are in Phase 1a.
- R2's `then` anchors grow with later phases: `moon` with J4 (Phase 2) and `month begins` with C13a (Phase 4). Until then the anchors are `day begins`, `hour`, `season begins`, `festival` and `dm signal`, plus a `calendar is` part for the season.
- R1's `where` can use `quest is` today. With C6 (Phase 4) and J2 (Phase 8), it reads chain states and realm variables the same way.
- SQ2 comes before M2 (a `purse` operand reads the mapped account) and before any program swaps a town payment for a till.
- Phase 2's C1a is used by later phases' `operand` params. C4, C5, C8, C9, C10, C11, L4 and L5 mark params `operand: true` but work with literals until C1a exists, so Phase 1b can land first. C2 is now in Phase 1a, so C1c goals can use `audience` as soon as C1 lands.
- In Phase 2, SQ5's `credit: present` is built with L3 (the five wolf-less events need it), and L2 with V1's camp check.
- Phase 3's `rank` and `tally` widen the native request (§0.3), as R1's `where` already does in Phase 1a. Phase 8's realm scope reuses that widening.
- Phase 4's C13a `variable changed` and C12a share one delivery step. Build C12a first. J3's `line` rule needs C6, which is in the same phase.
- Phase 7's C10t needs C10 built and C1a (Phase 2) for the scaled band row.
- Phases 5, 6, 7 and 8 are independent of each other and can be reordered by the user's priorities.
- Phase 9 needs Phase 2 (C1a) for the giver variable. M3's economy check waits for the user.

### 7.4 The calendar: earliest dates, not deadlines (answer (a))

**The clock.** A game day is 4 real hours; game day *d* is 2026-11-22 22:34 UTC + (*d* − 66.32) / 6 real days.

**No fixed span.** The user's answer (a) drops the 40-day frame.
- A chapter moves on when it is ready (R2: participation plus its least days) and an admin advances it, or by itself where `auto` is set.
- There is **no latest end** for any chapter, and no act has a publication deadline.
- The campaign has no reference length. The fastest it goes without Advance is the sum of the chapters' `minDays` (124 game days, about 21 real days); typical runs are about 24 real days at 200 wolves and about 30 at 5 (`campaign.md` §3.2.3). These are not limits.
- The old launch calendar's engine dates (day 133 "Act II's engine landed", day 135 "Act II must be published", day 148 "the interlude never runs past", day 218 "Act III") are **no longer deadlines** for the engine. An act opens when the act before it is advanced and its engine phases have landed. If they have not, the admin either **holds** the interlude (it has no cap now) or publishes the act's today-only rows.

**Seasonal anchors** (moons, festivals, the winter freeze, Midwinter) are **flavour and optional timing** (decisions §6.3). R2's `then` can wait for the next one after a chapter is ready (for example `then: {"on": "moon", "match": {"detail": "new"}}`, Phase 2), but **the Ninth Howl does not use `then`**:
- each closing phase times its own set piece, and waits for a calendar moment only when one falls within 3 game days of its entry; otherwise the set piece plays at the next dusk at least 3 game hours away (campaign §3.2.4);
- Ch 6 forces fog and Ch 8 forces an early freeze out of season;
- **Midwinter is never waited for** (D12, settled by decisions §6.2): the Moot night is dusk the next game day after the call, named "the Midwinter Moot" in winter and "the Moot of the Nine Howls" otherwise.

**When the engine lands** (in sequence, about 12 agent-hours a real day, from 2026-11-23):

| Phase | Hours (cumulative) | About |
|---|---|---|
| 1a Readiness | 21 | 2026-11-24 |
| 1b Foundations | 47.5 | 2026-11-26 |
| 2 Values and objectives | 72.5 | 2026-11-29 |
| 3 Thanks, titles, ranks, words | 99.5 | 2026-12-01 |
| 4 Stories that talk to each other | 124.5 | 2026-12-03 |
| 5 Howls | 137.5 | 2026-12-04 |
| 6 Story items | 147.5 | 2026-12-05 |
| 7 Foes (with the sim passes) | 172.5 | 2026-12-07 |
| 8 Across acts | 188.5 | 2026-12-08 |
| 9 Casting and the rest | 210.5 | 2026-12-10 |
| End of slice | 217.5 | 2026-12-11 |

These are not tied to game days. Each item reaches a running chapter by `migrate` (doc 85, 5b), at a chapter boundary or mid-chapter.

If the engine runs behind the wolves (an act is ready before its phases have landed), the order of what to keep is unchanged:
1. **Phase 4** (packs feed chapters, `requires`, closure, `wolf has`);
2. then **Phase 5** (howls);
3. then **Phase 6** (tagged letters, which have the plainest fallback).

The admin may also simply **hold** the interlude until they land.

### 7.5 Mapping to the campaign's acts (`campaign.md` §11.3)
- **Act I: Phase 0, with Phase 1a** for its pacing if 1a lands first (§7.1). Phases 1b–3 are built **during** Act I, and Act I picks each up by `migrate`.
- **Act II: Phases 1a–6** when Act I is advanced past its interlude, or its today-only rows 4–6.
- **Act III: Phases 7–8** when Act II is advanced, or its today-only rows 7–9.
- **Phase 9** is built (answer (a)). The campaign needs none of it, and the packs and the sequel use it.

### 7.6 Mapping to the quest packs and the talk plan
- **Small packs** need Phase 1b (V1, SQ2, J1, C8), plus C2 (now Phase 1a) for the garland rope (#38). SQ1, SQ5 and N4 (Phase 2) are conveniences; SQ3 and SQ4 come in Phase 9.
- **Medium packs** need Phase 1b, plus Phase 4 for C12a / C6 / N1. M2, M5, L2, L3 and N4 (Phase 2) and L1 (Phase 5, Q13's bear) are conveniences. M3 comes in Phase 9, behind its switch.
- **Large packs** need Phase 1b, plus J3 (Phase 4, with `"closure": "line"` on each line's last chapter), L1 (Phase 5) and C10 with C10t (Phase 7). L2, L3 (Phase 2), L5 (Phase 4), J9 (Phase 6) and L4 (Phase 8) are conveniences.
- **The stage pack** needs nothing beyond what its tiers need. Its stages may carry `ready` blocks too (R2), with `auto: true` and `autoAfterDays` as the safety (§8 D11).
- **The talk plan** (`cast-and-talk.md` §5.4): C8 with the pair scope and N2 (Phase 1b) switch on everything in §3 T/N and §4; V2 and T5 (Phase 3); N1 (Phase 4); T3 (Phase 7); T6 (Phase 9); T9 in Phase 0.
- Run-time casting (C7, Phase 9) only matters once packs repeat across all eight towns with role-based givers. Until then, packs use the fixed givers of `givers.tsv`.

---

## 8. Decisions for the user

D1, D4 and D8 are answered (2026-11-22). D10–D12 are new with answer (a).

| # | Question | Answer or recommendation |
|---|---|---|
| D1 | **J3:** should finished story quests count toward the Quickening's `closedStories: 2`, and with what social-XP amount? | **Answered (b): yes, capped.** Only a large line's last chapter (`"line"`) or a campaign side story (`"side"`) counts, at most 4 per program, and the game also refuses a fifth per wolf per program. The amount is 3 (the recommendation, adopted; the user may change it). Doc 45's Quickening balance assumed social Stories only, so this lowers the bar, as the user chose. |
| D2 | **C8 realm memory** reaches every resident's lines. Should a story's realm key be limited to keys with a prefix the story declares (e.g. `nh_`), so that two campaigns can't collide? | Yes: the validator requires every `talk.remember` key (every scope, pair too) to start with the program's top-level `prefix` (2–8 lower-case letters) and `_`. L2 camp names need the same field whatever D2 says, so `prefix` is built in Phase 2 with L2 and only its use for talk keys waits on D2. N2's `player.story` names are warned on a clash either way. |
| D3 | **C4 titles** change what other players see about a wolf. Should `title.give` be marked "acts on wolves" (needing that permission on a hosted world)? | No, to match `deed.award`. Add a client setting so a player can hide titles from their card. |
| D4 | **C10 foe strengths** (`foes.json`) touch combat balance (docs 37, 45, 47). Who tunes them? | **Answered (c):** combat simulations, several passes if needed, at most 250 players. C10t is the plan: `foe_sim` and `world_check --foes`, the written bar, up to 4 passes, then the user decides. Only `foes.json` changes. |
| D5 | **C14 and C7** need the careers post list at approval and in the runner. The checkpoint has it, but no `dm` table does. Add a `dm.story_posts` view written at each checkpoint, or read it from `game.story_facts`? | A small `dm.story_posts` table refreshed with the checkpoint (part of migration 0080), because approval must not depend on a live server. |
| D6 | **J2 realm variables:** a new table (recommended) or a `wolf` column on `dm.story_world_vars` (which changes its primary key)? | A new table, `dm.story_realm_vars` (0081). |
| D7 | **N1 overflow** changes what every giver's window looks like when many stories crowd it. Is "Ask about something else…" the right words? | Yes, as the wording for mantle grafts already reads. |
| D8 | **SQ3 and M3** touch the economy. Build them? | **Answered (a): build everything.** Both are built in Phase 9. SQ3 moves no coin and leaves the resident's output unchanged. **M3's economy check is a simulation, and answer (c) covers combat sims only**, so the Phase 9 agent asks before running it (memory: ask before simulations). Until then `house:` purses stay behind the world switch `storyHousePurses` (off). |
| D9 | **L4 `npc.sentence`** lets a story hold a resident past its own end (up to 28 days). Is a builder's approval enough? | Yes: it is `needsBuilder`, refused on protected residents, listed in the console with "Release now". |
| D10 | **Readiness defaults** for the campaign's chapters (R1, R2): what share, floor and cap, and what least days? | `activeDays` 18 (3 real days). Each chapter's main part: **35% of the active wolves who did the chapter's work, floor 5, cap 150**, with `minDays` = the chapter's old shortest window (`campaign.md` §3.2: for example Ch 1 10 days, Ch 2 12 days). `auto: false` on every main-line chapter (the admin pushes), and `slowAfterDays` = twice `minDays`. These are the campaign reviser's to write into each chapter; the engine has no campaign-specific default. **Settled for the Ninth Howl by decisions §6.2:** 25–30%, floors 2–3 (never above the active count), `cap: 150`, `activeDays: 18`, `auto: true`, `slowAfterDays` = the chapter's soft maximum, `slowNeedsYou: true` (campaign §3.2.3). |
| D11 | **Auto-advance when no admin is about** (`autoAfterDays`): use it? | Not needed on the main line: decisions §6.1 gives every chapter `auto: true` (a ready chapter moves itself; the admin pushes the slow ones with Advance, and the soft maximum raises a needs-you alert). On the stage pack and the quest packs' stage-like programs: `auto: true`, or `autoAfterDays: 7` where the stage changes what residents say. |
| D12 | **Midwinter without a fixed calendar** (§7.4): when Ch 9 is ready after the year's Midwinter, wait for the next one (about 61 real days), or let the admin call the Moot night? | **Settled by decisions §6.2:** neither wait. Ch 9's run-up has a `ready` block (Advance is the admin's way to choose the night), and the call sets the Moot night at dusk the next game day, named "the Midwinter Moot" in winter and "the Moot of the Nine Howls" otherwise. No text says "the longest night". The engine still supports `then: festival` and `then: dm signal` for other campaigns. |

---

## 9. The phase list

0. **Phase 0, Today-only content** (no engine hours): Act I on campaign §11.4, `nh-purses`, every pack's today version, the stage pack, the talk library (inert until keys are seeded), DM figures and the `protected` flag, T9 allow-list. Chapters are paced by a `dm signal` direction exit and the DM's eye, until Phase 1a. There is no target publication date.
1. **Phase 1a, Readiness and participation** (21 h, **first**): C2 `audience` with `activeDays` and the seeded `seen` map, `times`, `div`; R1 the `percent` / `among` operands and the `participation` condition; R2 the phase `ready` block, the `ready` exit, the runner's readiness state, the latch, `auto`, `then`, `slowAfterDays`, `autoAfterDays`, the held clock, the command `advance`, the alerts `ready` and `slow`; R3 the console's readiness bars, Advance…, Hold beside them, the list's column and alert, the chapters bar, and the guide's "Moving a chapter on". Its own end-to-end.
2. **Phase 1b, Foundations, purses and Act I's talk** (26.5 h): V1 approval fills with the `alive` check, SQ2 till purses and the account check, V3 guide fix, J1 per-wolf offers, C11 `npc.spare`/`npc.unspare` and `npc.protect lasts`, C9 `figure.set`/`figure.clear`, C8 `talk.remember`/`talk.forget`/`belief.plant` with realm and **pair** owners, T7 `story.cleared`, N2 `player.quest` and `player.story` window facts.
3. **Phase 2, Values and objectives** (25 h): C1 operands in effect params, `{name}` fills, goal operands, tracker "of"; M2 `purse` operand; C12b `starts`/`in` matching; N3 tracker mirror; J4 the moon (and R2's moon anchor); SQ1 objective `where`; SQ5 objective `credit`; L3 six more mission events; L2 named story camps and the program `prefix`; M5 `cell` on `trade done`; N4 `join.again`.
4. **Phase 3, Thanks, titles, ranks and words** (27 h): C5 `npc.greet`, C4 `title.give`/`title.take`, C3 `rank` and quest shares, J11 `tally`, J8 `belief.forget`, V2 story words check (with T4's expander and giver's dialect, and face banners), T5 `npc.line` in voice.
5. **Phase 4, Stories that talk to each other** (25 h): C12a `story.notify`, C13a `variable changed`/`month begins`/`succession`, C13b `resident has post`, C6 `requires` and `offer.during`, N1 choice overflow (T8), J3 closure quests (`line` and `side`, at most 4 per program, the game's per-wolf cap), J10 `wolf has`, L5 `quest.withdraw`.
6. **Phase 5, Howls, crowds and the wild** (13 h): J5 `howled`, `chorus closed`, `npc.howl`; J6 residents in `count in region`; L1 `hunt.stage`.
7. **Phase 6, Story items** (10 h): J9 tagged items.
8. **Phase 7, Foes and strangers** (25 h): C10 `foe.add`, `foe.leave`, `foe.outlaw`, and the foe events; C10t `foe_sim`, `world_check --foes` and the tuning passes against the bar (≤ 250 players, up to 4 passes); T3 strangers in scenes.
9. **Phase 8, Across acts and the crown** (16 h): J2 realm variables, C14 `npc.post`, L4 `npc.sentence`.
10. **Phase 9, Casting, mantles, queues, paws, House purses, the story topic** (22 h; built, answer (a)): C7 `cast.pick` and the giver variable, C15 mantle restore, J7 `npc.queue`, SQ3 lend a paw to a resident, SQ4 `rewardLimit`, M3 House purses (behind `storyHousePurses` until the user approves its economy check), T6 a `story` topic.
11. **End of slice** (7 h): full suite, end-to-end on a scratch Ridgemere 3 server with a throwaway migrated DB (including a chapter moved on by participation and an Advance, and a closure counted), one review-and-fix.

**Total: about 217.5 agent-hours**, about 18 real days in sequence at 12 agent-hours a day (§7.4).
- Act I ships on Phase 0 and is paced by Phase 1a as soon as it lands. It picks up Phases 1b–3 by `migrate`, and is complete as designed after Phase 3.
- Every act opens when the act before it is advanced. It uses whatever engine has landed, or holds, or falls back to its today-only rows.
- No chapter has a latest end.
