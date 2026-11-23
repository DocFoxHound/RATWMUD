# Story Maker stress test: what a grand campaign plus quest packs needs, checked against the code (read only)

I changed nothing and wrote no files. The code I checked: `tools/story_program.py`, `tools/story_triggers.py`, `tools/story_quests.py`, `tools/story_runner.py`, `Core/RatwGameStorylines.cpp`, `Core/RatwGame.cpp`, `Core/RatwStoryArea.cpp`, `Core/RatwBattle.cpp`, `Core/RatwWorld.cpp`, `Core/RatwGameFame.cpp` and `Data/Storylines/trigger-catalogue.json`.

## A. Verified engine facts that shape the design (new, beyond the four surveys)

1. **Effect parameters are literals only.**
   - `story_program._param_ok` (line 263) accepts an identifier, or `$actor` for a wolf.
   - `story_triggers._act` (line 1242) also swaps `$each` inside `each`. The `$each` can be a wolf, or a resident when looping a `residents` list (check at line 561).
   - There is **no `{"var":x}` in any effect parameter**: not `npc`, not `coins`, not `text`, and not a quest `giver`.
   - Workaround for a resident chosen at run time:
     1. `pick {list, var}` (or `remember from: target`);
     2. `list clear L1`, then `list add L1 {"var":x}`;
     3. `each over list L1` with `npc:"$each"`.
   - This works for effects (npc.offer, npc.walk, npc.speak and the rest). It does **not** work for quest `giver` or `board`, which are static strings sent by `story_quests.offers()`.
2. **Text is never filled from variables.**
   - `story.tell`, `board.post`, `journal.note`, quest words and `npc.line` are sent as written.
   - Only the resident's own lines (`offer.say`, `offer.thanks`, `npc.offer.reply`) go through `#symbols#` (`inVoice`).
   - Workaround for progress messages: `if`/`elif` branches with pre-written texts, one per threshold.
3. **Who sees a giver's offer** (`Game::storyOfferOpen`, `RatwGameStorylines.cpp:1132`).
   - It checks only: the story is live, the quest is not `full`, `who` (participants/anyone), `region`, and the repeat gap.
   - A wolf with **no quest view sees every giver offer**. So a chain quest with a giver leaks to everyone in that phase.
   - Only journal offers (`quest.offer` with no giver) can be gated per wolf.
   - While a quest is active but not yet ready, the giver shows nothing.
4. **Story-scope quests are the shared progress bar.**
   - There is one copy for the story. Each wolf's matching event adds to `rec['n']`, up to `goal` (≤ 1e6), and the wolf joins `members` and `credit` (`story_quests.py:560-590`).
   - The journal shows `amount / goal` to every member (`view()`, line 820).
   - The `contributors` field (1–500) is the minimum number of distinct wolves before the quest settles.
   - **Weakness: everyone with credit gets the same reward**, because per-wolf amounts are not kept.
5. **Trackers** (`story_quests.trackers`, line 855) show `label: value` for shown number and switch variables of any scope, including `world` and `player`, in each participant's journal.
   - A tracker has no goal and no bar: it reads "Resolve: 34", not "34 / 100".
   - Only participants see trackers, so with an `everyone` audience that means every signed-in wolf.
6. **World variables** (`story_runner.world_load` and `world_save`, lines 921–957).
   - They are loaded at the start of each run's tick and written back at its end.
   - Runs are ticked **one after another** in `tick()`, so one runner process never loses an update.
   - **Another story does not hear a change**: there is no `variable changed` event.
   - So a campaign reading a counter that quest packs feed must poll it from a watched event (`hour` or `day begins`) with a `compare` condition.
7. **Cross-story communication today.**
   - `story started` and `story ended` (with `detail` = campaign or outcome) are the only events between stories.
   - The game's `deed` event is written for every `deed.award`, with `item: "award"`. But its `detail` is `"<phrase> (<deed id>)"` (`RatwGameFame.cpp:147`), and `match` is exact equality only (`story_program.matches`, line 1129). **So a campaign cannot tell one award from another.**
   - The `field` condition offers only `=` and `!=` on text.
8. **Triggers and audience.**
   - `participant` and `player`/`member` triggers ignore wolves outside the cohort (`story_triggers.py:1615`). Use an `everyone` audience for the main line, so every signed-in wolf counts.
   - `each` runs at most 500 turns, while the audience cap is 5000.
9. **Approval checks resident parameters** (`npc`, `resident`, `player`, `faction`, `place`/`cell`) against `dm.story_residents` and the other tables (`references()`, `story_runner.py:4562`).
   - **Strangers (`visitor.add`) can't be the `npc` of any effect**, so they can't speak, offer, walk or give quests.
   - A stranger can still be the `target` of an objective match, because objective matches are unchecked.
   - Strangers last at most 1440 minutes (`World::addVisitor`).
10. **Fights.**
    - A wolf attacking a resident or stranger is an assault, a crime: guards on duty join, and a warrant follows (`RatwBattle.cpp:593-630`).
    - A protected resident can't be attacked at all (line 316).
    - Residents are "timid" (`battle::temperament`).
    - The only legitimate foes are bandits: `story.area.spawn` (at most 6, camp `camp_<cell>`) and `bandits.call` (at most 6 near one wolf), plus `bandit falls` and `camp cleared`.
    - **There is no fightable named villain.**
11. **`standing.adjust`** (`RatwStoryArea.cpp:101`) refuses a wolf with no Chapter. Most solo wolves get nothing from it.
12. **The dialogue window.**
    - All live stories share the hub's 9-choice room, and choices that don't fit are dropped (`storyChoicesFor`).
    - Several quest packs at one popular giver (for example an innkeeper) will crowd each other out.
13. **`once: member`** fires once per wolf for the whole run (`campaignFired`). It does not carry into the next chapter; use `carry` with player variables for that.

## B. Gap table

| # | Need | Status | Evidence / how today | Gap |
|---|---|---|---|---|
| 1 | Shared world progress fed by many wolves | **Supported** | A story-scope quest objective (goal ≤ 1e6; `measure` count/quantity/coins), or a story/world number variable with `add`. Many quest packs can feed one `world` variable. | — |
| 2 | Progress shown to players | **Partial** | Journal: a story quest's "n / goal", trackers "label: value". `story.tell` world banners; `board.post` static (≤ 300, ≤ 28 days). | No variable text, no tracker goal, no crier announcement effect |
| 3 | Thresholds advance chapters | **Supported** | A trigger `if compare {var} >= N` with `enter`/`finish`; exits with `players`/`percent`; `story.start` (≤ 12 chapters, `carry`). Cross-program counters must be polled on `hour` or `day begins`. | `variable changed` event (convenience) |
| 4 | Goals scale with the population (R3 has 5 wolves) | **Missing** | No operand for the audience or online count; `{count:list}` only counts a list variable. | operand `audience` |
| 5 | Per-wolf contribution tracking | **Supported** | A `player`-scope number with `add … of $actor` and value `{field:quantity}`; `show` makes it a personal tracker. | — |
| 6 | Recognition by contribution (top N, tiers) | **Partial** | `each over participants` plus `compare {var,of:$each} >= T` gives fixed tiers (≤ 500 turns). Story-quest rewards are flat. | No ranking; no reward scaled by contribution |
| 7 | Titles, renown, NPC thanks | **Partial** | `deed.award` becomes fame, beliefs, crier calls and automatic nicknames (from the fixed families); `story.practice`; `character.grant` ≤ 2500 from a real purse; `standing.adjust` only for wolves with a Chapter; `story.tell from:` a resident. `npc.line` goes to all participants, not one wolf. | A campaign title; a resident's one-off thanks to one wolf; town regard |
| 8 | Personal side stories from choices | **Supported** | Player variables, `random`/`pick`, and `quest.offer $actor` from a world trigger; `dialogue choice` chains with `npc.offer {player:$actor, once}`; `storyline.give` (only the 2 DM templates on R3: the-debt, a-gathering). | — |
| 9 | Quest chains / prerequisites | **Partial** | `quest is` condition plus `quest.offer` (journal accept, no giver offer), or `phases`. A giver offer can't be gated per wolf (A3). | `requires` on quests |
| 10 | Repeatable small quests | **Supported** | `repeat` daily/weekly/always on **game** days (`again()`), `join.cap` (at once) and `join.limit` (in all), `lifetime: phase`. | — |
| 11 | Quests cast at run time (pick a resident by role) | **Missing** | Static `giver`; doc 85 lists "a cast chosen by a query (doc 34, 5.1)" as Later. The workaround of A1 covers effects only. | `cast` action; variable `giver` |
| 12 | Gossip and knowledge that follow campaign state | **Missing** | Only indirect levers (deeds, `event.add`, `bandits.call`, `npc.kill`, `figures` through the DM). `belief.plant` and `npc.brief` are Later. Talk can already gate on `world.mem.*` (validated by the talk survey). | `talk.remember`, `belief.plant`, `figure.set` |
| 13 | The giver speaks while a quest is under way | **Missing** | The window shows only the offer or the hand-in. | `offer.during`; fact `player.quest.<id>` |
| 14 | Multi-step quest conversations | **Partial** | `npc.offer` chains on `dialogue choice`, one step per pick, at most 64 offers and choices per story, within the shared 9-choice hub. | (optional) `npc.offer.next` |
| 15 | Heroes and villains who move and appear | **Partial** | `npc.walk` (with a cue, then `scene arrived`), `npc.move`, `npc.follow`, `npc.hold`, scenes, `visitor.add` (≤ 24 h, mute, re-added daily). | A stranger that talks; longer stays |
| 16 | Villain fought and defeated | **Missing** | Attacking a resident is a crime; there are no hostile named NPCs; bandits are anonymous, ≤ 6. | `foe.add` and its events |
| 17 | Key cast kept alive | **Partial** | `npc.protect` stops stories and attacks but **not** old age; only authored `live.npcs.protected` stops old age. | `npc.spare` (an undone hold on mortality) |
| 18 | Branching outcomes change the world | **Partial** | Lasting: `faction.relation` (B), `npc.kill`/`npc.revive`, `project.post`/`project.complete`, world variables, deeds, warrants. Temporary: weather ≤ 72 h, events ≤ 7 days, `harvest.override`, `times.nudge`, doors and strangers (undone at the end). | Lasting resident fates (post, home, stance); `figure.set` |
| 19 | Timed world events | **Supported** | `schedule` (game or real clock, repeat), `clock` exits with `afterDays`, `calendar is`/`hour between`, watched `day begins`/`season begins`/`hour`, `festival.call`, `event.add`. | `month begins` |
| 20 | Failures don't block everyone | **Supported** | Per-wolf quest `fail` (≤ 4 rules; deadline game/real), `finish who:$actor`, deadline exits to a "worse" phase instead of an ending, `ifRefused: skip`/`when here`. | — |
| 21 | Cross-story hooks (quest packs feeding the main line) | **Partial** | World variables (polled), `story started`/`story ended`. `deed` events can't be told apart (A7). | `story.notify`; `field` op `starts` |
| 22 | Many quest packs side by side | **Supported (with care)** | One live run per program id; ≤ 32 quests, ≤ 64 triggers/phase, ≤ 64 offers per program. Packs share each giver's 9 hub slots. | Spread givers; doc note |
| 23 | Rewards in proportion | **Missing** | `coins`/`quantity` are literal. | Operand parameters |

## C. Proposed additions, in the catalogue's style

### C1. Variables in effect parameters and texts (fixes #2, #6, #11, #23)
- Allow an operand wherever an effect parameter has type `int`, `resident`, `wolf`, `place` or `item`: `{"var":"villain"}`, `{"var":"share","of":"$actor"}`, or `{"plus":[…]}`.
  - Resolve it in `story_triggers._act` before `triggerEffects`, then clamp to the parameter's min and max.
  - `references()` checks the declared type instead of the id.
- Text: `"text": "The ford holds. {resolve} wolves have stood watch."`, with `{name}` naming a declared variable (or `{name.of}` for the acting wolf's).
  - Fill it in `_act` (number → digits; wolf/resident → display name, through a name table the runner reads from `live.npcs` and `game.characters`).
  - The validator checks every name is declared and that the text still fits the limit once filled.
  - Applies to `story.tell`, `board.post`, `journal.note`, `npc.speak` and quest `summary`.

### C2. Operand `{"audience": "all|active|online"}` (fixes #4)
- Gives the cohort size, or the wolves signed in now (from `story.cohort`).
- Lets you write `compare {var:resolve} >= {"max":[20,{"multiply":[{"audience":"active"},4]}]}`.
- Needs a `multiply` operand alongside plus/minus/min/max.

### C3. Action `rank` (fixes #6)
```json
{"do":"rank","var":"watch_points","top":3,"into":"heroes"}
```
- Reads a `player` number variable over the cohort and puts the top N wolves (ties broken by seed) into a `wolves` list.
- Then `each over list heroes` hands out titles and deeds.
- Also add a quest field `"credit": "share"`: per-wolf amounts kept in `credit` as `{wolf: n}`, plus a reward operand `{"share": true}` for that wolf's amount.

### C4. Effect `title.give` / `title.take` (fixes #7)
```json
{"kind":"title.give","player":"$actor","title":"Warden of the Long River","known":"realm|town:<id>","days":0}
```
- A campaign title shown on the wolf's card and in residents' greetings.
- Sets the talk fact `player.title` (a `facts.json` entry) and adds a fame record.
- `actsOnWolves: false`.
- New event `title given`.
- Fallback with no code change: `deed.award` with a lower-case verb phrase.

### C5. Effect `npc.greet` (fixes #7, #13)
```json
{"kind":"npc.greet","npc":"rm_gerrit_mudhand","player":"$actor","text":"#thanks#, #address#. The Council heard what you did at the ford.","days":7}
```
- Like `npc.line`, but for one wolf, allowed in triggers, expanded with `inVoice`.
- Shown on that resident's window greeting until seen once, or for `days`. A story effect, kept in `state.greets`.

### C6. Quest fields `requires` and `offer.during` (fixes #9, #13)
```json
"requires": {"if": {"test":"quest is","quest":"ford_1","of":"$actor","status":"done"}},
"offer": {"say":"…","during":"Still looking? The second axle's at the wheelwright's."}
```
- The runner evaluates `requires` per wolf (facts allowed) and sends the offer with `players:[…]`.
- `storyOfferOpen` then needs a `players` check, as `choices` already have.
- `during` is shown as a choice-less line on the giver's hub while the wolf's copy is `active`.

### C7. Action `cast` (fixes #11)
```json
{"do":"cast","var":"carter","from":{"job":"carter","town":"larkfield","faction":"house_grayrock","quarter":"sump","alive":true,"placed":true,"notIn":"used"},"near":"$actor"}
```
- Fills a `resident` variable (or `{into: list, count}`) from `live.npcs` plus the current post holders (`society.careers.positions`), excluding the unplaced.
- The runner answers it through a `game.story_facts`-like query.
- No resident found makes it unknown, and `else` branches run.
- Pairs with C1, so `"giver": {"var":"carter"}` works: offers are rebuilt whenever the variable changes.

### C8. Effects `talk.remember`, `talk.forget` and `belief.plant` (fixes #12; the talk side needs no change)
```json
{"kind":"talk.remember","scope":"town|npc|realm","town":"larkfield","npc":"…","key":"ford_watch","value":"held","days":30}
{"kind":"talk.forget","scope":"town","town":"larkfield","key":"ford_watch"}
{"kind":"belief.plant","subject":"rm_oskar_gullworth","kind_":"story.rumour","text":"was seen at the Customs House at midnight","town":"ridgemere","count":12,"confidence":0.6}
```
- `talk.remember` calls `Game::talkRemember` (owner `town:<region>`, ≤ 32 entries per owner). Lines gate on `world.mem.ford_watch=held` or `self.mem.*`. It is undone at the run's end unless `keep:true`.
- `belief.plant` calls `World::believe` with a new `claims.json` kind, `"story.rumour": {"they":["#x.name# #claim.text#"]}`. It spreads along bonds and caravans as a normal belief.
- Adds the talk fact `player.quest.<id>=active|ready|done` (from `questViews`), for tree `say[...]` lines and grafts.

### C9. Effects `figure.set` and `figure.clear` (fixes #12, #18)
```json
{"kind":"figure.set","role":"villain","npc":"rm_ambrose_greatly","name":"Ambrose Greatly","title":"master of the Counting House","known":"realm","about":"…"}
```
- Writes `live.figures` and queues `voice.sync`. The 15 existing `#world.figure_villain#` lines then play.
- Needs a builder.

### C10. Effect `foe.add` and events `foe defeated` and `foe fled` (fixes #15, #16)
```json
{"kind":"foe.add","foe":"the-grey-captain","name":"the Grey Captain","like":"rmx_desmond_hollis","description":"…","cell":"w_ridgemere_6_8","x":120,"y":88,"band":3,"strength":"tough|fierce|champion","minutes":720}
```
- Builds on `addRoadFolk(…,"bandit",camp)`: hostile temperament, no crime for attacking it, and guards help the wolves.
- `band` is 0–6 followers. The foe is undone at the end.
- Events: `foe defeated` (`actor` the last blow, `target` the foe, `detail` the name), `foe fled`, and `foe downed`.
- Optional `npc: <resident>` makes a resident an outlaw for the length of the fight. The resident is then downed rather than killed, unless `lethal:true`.

### C11. Effect `npc.spare` / `npc.unspare` (fixes #17)
`{"kind":"npc.spare","npc":"rm_halvard_mossgrave"}` skips `World::tendOldAge` for that resident. Undone at the end (`noteStoryHeld`).

### C12. Effect `story.notify`; `field` operators `starts` and `in` (fixes #21)
```json
{"kind":"story.notify","story":"realm-ch1","signal":"ford_held","player":"$actor"}
```
- A story effect. The runner puts a `story signal` into that live story's command inbox, as `story started` is delivered today.
- Condition: `{"test":"field","field":"detail","op":"starts","value":"stood watch at the ford"}`, and `"op":"in","value":[…]`.
- Also `match` accepting `{"detail":{"starts":"…"}}`.

### C13. Events `variable changed` (watched), `month begins` and `succession`
- `variable changed`: `target` the variable, `quantity` the new value, `detail` the run. It wakes every story that declares that world variable.
- `month begins`: the 28-day month.
- `succession`: `game.events` already has it; `actor` the new holder, `detail` the post. Add `resident has … post`.

### C14. Lasting resident fates (#18; optional)
- `npc.stance {npc, topic, stance, days}` (doc 34 `npc.brief`).
- `npc.post {npc, post|none}` through the careers vacancy rules: the resident leaves or takes a post.
- Both are noted as lasting, never undone.

### C15. `mantle.assign` restore (from the cast survey)
`mantle.assign` should keep the previous binding and restore it on release or at the run's end.

## D. Recommended architecture with today's engine (no new code)

- **Main line:** a campaign of up to 12 chapters, each `audience: {"kind":"everyone","days":30}`.
  - World variables: `realm_resolve` (number, shown), `realm_threat`, and switches for outcomes.
  - Story-scope quests carry the shared goals: goal sized for 5 wolves now, with `contributors` 2–3.
  - A campaign trigger on `day begins` (or `hour`) runs `if compare realm_resolve >= N` with `story.tell` world, then `board.post` with pre-written threshold texts per town, then `enter`.
  - Each chapter ends with `story.start {audience:"own", carry:true}`.
- **Quest packs** (small, medium, large): separate open-audience programs, each ≤ 32 quests.
  - Each pack `add`s to the same world variable in its quest `rewards`. The quest-completed trigger does `add realm_resolve 1`; it is a v2 trigger action, not a reward.
  - Packs use different givers, so the 9-slot hubs don't overflow.
- **Personal side stories:** inside each chapter.
  - Player variables plus `random`/`pick` into `quest.offer $actor` (journal), chained with `quest is`.
  - Talk steps through `npc.offer {player:"$actor", once:true}` on `dialogue choice`.
- **Recognition:**
  - `deed.award` lower-case phrases, which bring automatic nicknames and crier calls;
  - `character.grant` from `town:<id>` purses (check they hold the coin);
  - tiered `each over participants` with `compare` on a player variable.
- **Gossip:** until C8 exists, use `figures` through the DM, `event.add`/`bandits.call`/`festival.call`, and `npc.line` and `npc.offer` hints for participants.
- **Villains:**
  - existing residents moved with `npc.walk` and scenes, then defeated by quest outcome (`npc.hold` or `npc.kill` with fallbacks);
  - their bands are `story.area.spawn` camps, beaten by `camp cleared`/`bandit falls` objectives (`target: camp_<cell>`);
  - `npc.protect` every key resident, and branch on `resident is alive`.
- **Authors must check by hand:** objective match ids, `board` towns, `character.grant` purses (approval misses all three), and that every name is a placed resident.

## Key files
- /home/martinb/Documents/GitHub/MUDProject/tools/story_program.py: `_param_ok` 263, `validate_effect` 304, `matches` 1129
- /home/martinb/Documents/GitHub/MUDProject/tools/story_triggers.py: `_act` 1242, effect check 556–576, `each` 591, once/cohort 1605–1640
- /home/martinb/Documents/GitHub/MUDProject/tools/story_quests.py: `check_quest` 136, `again` 349, `settle`/`complete`/`credited` 478–520, progress 560–590, `view`/`trackers`/`offers` 800–905
- /home/martinb/Documents/GitHub/MUDProject/tools/story_runner.py: `references` 83 and 4562, `world_load`/`world_save` 921–957, `tick_run` 1525, `tick` 1767
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwGameStorylines.cpp: `storyOfferOpen` 1132, `storyChoicesFor` 1157
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwGame.cpp: `npc.kill` 1833, `bandits.call` 1870, `npc.move` 2056, `visitor.add` 2128
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwStoryArea.cpp: 101 `applyStoryReward` (standing needs a Chapter)
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwBattle.cpp: 306–316 `attack` (protected refusal), 593–630 (an assault on a resident is a crime)
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwWorld.cpp: 4883 `addVisitor` (≤ 1440 min, no society spec)
- /home/martinb/Documents/GitHub/MUDProject/Core/RatwGameFame.cpp: 147 (`deed` event detail ends with "(deed id)")
- /home/martinb/Documents/GitHub/MUDProject/Docs/Design/85-story-triggers-and-campaign-control.md: 810, 2292, 2400 (the items listed as Later)