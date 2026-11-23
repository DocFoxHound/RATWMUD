# Story Maker authoring format: survey (story-format area)

I changed nothing. All database reads ran against ratw_ridgemere3 as the `dm` role, read-only (`SET default_transaction_read_only = on`).

## 0. Live status on Ridgemere 3
- **The game server is running:** `build-core/ratw_server --port 7788 --database ridgemere3 ...` (pid 12304, started by `Saved/WorldAgent/run.sh`).
  - The binary was built at 09:46 today, after the Phase 10 merge `836d4fe4` (09:43). It therefore has every story order from doc 85.
- **The story runner is running:** `python3 tools/story_runner.py --world ridgemere3` (pid 12299). Do not start a second one.
- **Migrations:** every story table exists (`dm.story_releases`, `story_commands`, `story_player_state`, `story_world_vars`, `story_campaigns`, `game.story_facts`), so 0067–0079 are applied.
- **Nothing is approved yet:** `dm.story_releases` is empty on ridgemere3.
- **World id:** `story_runner.world_id(conn)` returns `upper_accord` for ratw_ridgemere3. This is the world id the approve and publish calls take.
- **How to connect:** `import world_db; c = world_db.connect('ridgemere3','dm')` from `tools/`. The key is the world key, not the database name. `psql` is not needed. The dm role cannot read `public.schema_migrations`.
- **Reference ids the world has** (`story_runner.refs(conn, world)`):
  - 1086 residents, with ids like `rm_orla_rainworth`, `rmx_ivo_wainwright`, `tn_fh_aster_coldfell`. The bundled examples use Greyfen ids (`rm111`, `rm119`, `harrow`), which do **not** exist in R3.
  - 6 factions: `council_of_houses`, `ridgemere_watch`, `house_grayrock`, `house_fell`, `house_greatly`, `house_mossgrave` (named "House Timberland").
  - 22 towns: cedar_sound, coldwater, fenshore, frosthollow, greenholt, greyteeth, larkdowns, larkfield, mirebridge, oakhaven, ochre_hills, rainward_hills, rainwood, red_waste, ridgemere, sallow_fens, saltpan, saltwell, southmeadow, sunfields, western_sea, whitefangs.
  - 8 markets: coldwater, fenshore, frosthollow, larkfield, oakhaven, ridgemere, saltwell, southmeadow.
  - 10 harvest areas: grain, fruit, meat, fish, honey, salt, fuel, stone, iron, lime.
  - 5 ships: s21–s25.
  - 414 doors (e.g. `link_rm_abbot_house_door_a`), 805 mantles, 1241 places (`world.areas`).
  - 5 player wolves (e.g. `wolf-3a21a58ad4853008ffc625570a53a5ce` "Bilbo").
  - 2 DM templates: `the-debt`, `a-gathering`.
  - 5 projects: mend, watchpost, cover, waystation, granary.
  - 458 items (`Data/Items/items.json`): `axle`, `cart_wheel`, `rope`, `bread`, `herbs`, `letter` and `iron_bar` exist; `lantern` does not.

## 1. Files
- **Data:**
  - `Data/Storylines/trigger-catalogue.json`: the single source of truth, 207 events, 39 conditions, 33 actions, 71 effects, with limits. `story_program.py` reads its tables from it.
  - `Data/Storylines/programs/*.json`: 8 bundled examples. Only `siege-at-greyfen.json` is version 2.
  - `Data/Storylines/scenes/greyfen-oath.json`: the scene example.
  - `Data/Storylines/event-library.json`: 100 mission and objective components (Greyfen ids).
  - `Data/Storylines/templates.json`: 24 doc-58 personal storyline templates; only `source: "dm"` ones work with `storyline.give` and `storyline.tick`.
  - `Data/Storylines/rules.json`: doc 58 storyline limits (not world stories).
- **Engine:**
  - `tools/story_program.py`: `validate`, `problems`, `review` (rehearsal), `digest`, `upgrade` (v1→v2), `version_diff` and `migration`.
  - `tools/story_triggers.py`: trigger, condition and action checks and their evaluation.
  - `tools/story_quests.py`: quests and audiences.
  - `tools/story_scenes.py`: scenes.
  - `tools/story_native.py`: ctypes bridge to `build-core/libratw_story.so`; the runner requires it.
  - `tools/story_runner.py`: approve, publish, `approve_campaign`, `references` (the world-id check), `program_problems`, `refs`, the runner loop and console commands.
  - `tools/story_author.py`: JSON-lines bridge.
  - `tools/story_library.py`: drafts from the event library.
  - `tools/story_drafts.py`: server drafts (migration 0073).
  - `tools/story_guide.py`: `--write` / `--check` the guide's reference section.
  - `tools/story_v2_samples.py`: v2 samples (LEDGER, FLOW, QUEST_SMALL, QUEST_CAMPAIGN, FACTS_WATCH).
- **Docs:**
  - `Docs/story-programs.md`: the format.
  - `Docs/Story-Maker-guide.md`: sections 2, 7–10 and 16 are the useful ones; the reference after line 392 is generated from the catalogue.
  - `Docs/Design/85-story-triggers-and-campaign-control.md`: "Phase N as built", Phase 6 at line 1904 and Phase 10 at line 2309.

## 2. Program top level (v2)
Required keys:
- `format: "ratw-story-program"`
- `version: 2`
- `id`: identifier `[A-Za-z0-9_][A-Za-z0-9_.:-]{0,79}`
- `title`: 100 characters at most
- `audience`
- `start`: a phase id
- `phases`: 1–100
- `tests`: 1–32 rehearsal cases

Optional keys:
- `variables`: 64 at most
- `quests`: 32 at most
- `triggers`: campaign triggers, 64 at most, shared by every listening phase and run before the phase's own
- `regions`: 128 at most
- `scenes`: 32 at most, embedded `ratw-scene` documents
- `quietAfterHours`: 0–8760

Any other key is refused. Studio and the bridge accept at most 1 MiB per request.

**Audience:**
- `{"kind":"open","cap":N}`: nobody at first; a wolf joins by taking a quest or by `participant.add`.
- `{"kind":"everyone","days":1-90,"cap":N}`: the active wolves, plus each wolf who signs in.
- For both, `cap` goes up to 5000.
- `{"kind":"active","days":1-90}`: frozen when published, 500 at most.
- `{"kind":"players","players":[ids]}`: 500 at most.

**Variables:** `{"id","type","scope"}` plus optional `initial`, `note` (300 characters), `show` (a tracker label of 60 characters; numbers, switches and timers only), and `clock` (timers only: game or real).
- Types: `number`, `switch`, `text`, `wolf`, `resident`, `place`, `region`, `item`, `faction`; the lists `numbers`, `texts`, `wolves`, `residents` (500 values each); and `timer` (no initial value).
- Scopes: `story`, `player`, `party`, and `world` (kept in `dm.story_world_vars`, shared by stories that declare the same name and type).
- A misspelt variable name is an error. Text values are 200 characters at most.

**Region:** `{"id","cell","x1","y1","x2","y2"}`, inclusive, one cell, coordinates 0–4095. Approval checks the tile exists.

**Phase:** `{"id","journal","effects":[],"exits":[]}`, plus optional `triggers`, `missions`, `guestVoices`, `campaignRules` (bool).
- A phase with no exits, no triggers and no open quests is an ending, and entering it ends the run.
- A phase with open quests stays open by itself, so a small quest is one phase plus one quest.
- **Entry effects** (12 at most) are sent when the phase opens.
  - `npc.line` (`{"kind":"npc.line","npc":id,"text"≤200}`, one per resident per phase) is resident dialogue for that phase only, valid only as an entry effect.
  - The story's own effects (quest.*, npc.offer, participant.*, story.tracker, story.start) are **not allowed** as entry effects; they must go in a trigger.
- **Exits** (16 at most), the v1-style way to move on:
  - Shape: `{"on":event,"to":phase}` plus optional `match`, `afterDays`, `players`, `percent`, `minQuantity`, `minCoins`, `participant`, `prerequisites`.
  - Or `{"on":"missions completed","to":..,"missions":[ids]}`.
  - `on:"clock"` with `afterDays` is a deadline.
- **Missions** (16 at most per phase, the v1 "journal missions"; quests are better in v2): `{"id","title","mode":"individual|group|either","objectives":[1-8]}` plus `players`, `percent` or `contributors` (≥2, group only).

**Trigger:** `{"id","on","actions":[...]}` plus optional:
- `signal`: required if and only if `on:"story signal"`.
- `match`: `{actor|target|cell|item|detail: string}`, exact match.
- `if`: a condition tree.
- `once`: `each`, `visit`, `player` (once per wolf this visit), `campaign`, or `member` (once per wolf for the whole campaign).
- `audience`: `world` (default) or `participant`. Any `$actor` effect needs `participant`, except `participant.add`, `quest.offer` and `quest.start`, which may name `$actor` from a world trigger; that is how an open story is joined.
- `enabled: false`: starts off.
- `folder`, `note`.
- `afterDays`: only with `on:"clock"`, which also needs `once:"visit"`.

`dm signal` cannot be a trigger event. Events marked `needsMatch` must name those filters. Limits: 64 triggers per phase including campaign triggers; 24 actions per trigger counting nested ones, nested 3 deep (v1 allows 12 flat).

**v2 actions** (`{"do": ...}`):
- **Set and arithmetic:**
  - `set`, `add`, `subtract`, `multiply`: `{var, value, of?}`. `of` is `$actor` or `$each`, for player or party variables.
  - `clamp`: `{var, min, max}`.
  - `turn on`, `turn off`, `toggle`: `{var}`.
- **Remembering and lists:**
  - `remember`: `{var, from: actor|target|cell|item|detail|each}`.
  - `list add`, `list remove`: `{var, value}`. `list clear`: `{var}`.
  - `pick`: `{list, var}`, seeded.
- **Timers:**
  - `start timer`, `stop timer`: `{var}`.
  - `schedule`: `{signal, days, repeat?, scope: visit|campaign, clock: game|real, owner: story|player}`.
  - `cancel`: `{signal, owner?}`.
  - `shift`: `{signal, days, owner?}`.
- **Flow:**
  - `emit`: `{signal}`, delivered after the current event's rules, once per name per fact; cycles are refused.
  - `if`: `{if, then, elif: [{if, then}] (4 at most), else}`.
  - `each`: `{over: participants|party|list, list?, actions}`; not nested in another `each`; 500 turns at most.
  - `random`: `{choices: [{weight 1-100, actions}] (2–8)}`.
  - `wait`: `{days, clock}`; at least 1 game minute or 5 real seconds; not inside `each`.
  - `run`: `{trigger, check}`.
  - `enable`, `disable`: `{trigger, phase?}`.
  - `stop`.
  - `note`: `{text ≤300}`.
- **Ending and effects:**
  - `finish`: `{outcome: success|failure|abandoned, text ≤300, who: everyone|$actor|$each}`. For `everyone` it must be the last action.
  - `effect`: `{effect: {...}}`.
  - `enter`: `{phase}`, must be the last action of its list and not inside `each`.
- **Operands:**
  - A literal number, bool or string.
  - `{"var":x,"of"?}`, `{"field":actor|target|cell|item|detail|quantity|coins}`, `{"count":list}`, `{"since":timer}`, `{"each":true}`.
  - `{"plus"|"minus"|"min"|"max":[a,b]}`, two deep at most.
- v1 `add`/`set` use `{key, scope, value}`. In v2 that form is refused; use `var`.

**Effects:** `{"kind":..., params..., "ifRefused":"pause|skip|when here", "waitDays":1-90}`. `when here` works only for an effect aimed at one wolf; quest rewards default to it.

## 3. Conditions (`{"test": ...}`)

| Kind | Shape |
|---|---|
| all / any | `{conditions:[1-8]}` |
| not | `{condition}`; nesting goes 5 deep |
| field | `{field, op, value}`; text fields take = and != |
| counter | v1 only |
| region | `{region}`: the event's tile is inside |
| mission | `{id}`: a phase mission is done |
| compare | `{left, op, right}`: = != > >= < <= |
| switch | `{var, is, of?}` |
| in list | `{list, value, of?}` |
| phase is | `{phase}` |
| visit is | `{op, value}` |
| timer | `{signal, owner?, op?, days?}` |
| fired | `{trigger, op, value, in: visit\|run}` |
| chance | `{percent: 1-99}`, seeded |
| days since | `{from: phase\|run\|var, var?, op, days}` |
| Facts (a missing fact is unknown and never fires, not even under `not`) | `wolf here {who?}`, `wolf in region {region}`, `wolf in place {place}`, `wolf in town {town}`, `wolf indoors {is}`, `count in region {region,op,value}`, `carries {item,op,value}`, `coins {op,value}`, `standing {faction,op,value -100..100}` (needs a Chapter), `reputation {town,op,value}`, `wolf is {state: downed\|hurt\|wanted\|held, is}`, `party size`, `in party with {other}`, `resident is {npc, state: alive\|dead\|indoors\|out of doors}`, `resident in place`, `resident in region`, `resident has {npc, what: job\|mood\|mantle\|town, value}`, `hour between {from,to}` (wraps past midnight), `calendar is {what: weekday(Dawnday,Hearthday,Stoneday,Riverday,Emberday,Marketday,Restday)\|season\|period(dawn,day,dusk,night), value}`, `weather is {where?, weather}`, `relation is {faction, other, op: = != >= <=, stance: war<hostile<tense<neutral<friendly<allied}` |
| quest is | `{quest, of: $actor\|$each\|story, status: none\|offered\|active\|ready\|done\|failed\|abandoned\|expired}` |
| objective is | `{quest, objective, of, status: done\|failed\|to do\|hidden}` |

`who` is `$actor` (the default), `$each`, or a wolf id.

## 4. Events (207, by group; `[w]` = written only while a live story listens; `[s]` = the story's own)
- **Story engine:**
  - `story phase`, `clock`, `dm signal`.
  - `story signal` (`signal`, `actor`): not allowed as a rehearsal fact.
  - `missions completed`.
  - `story started`[s] and `story ended`[s]: `target` is the other story's id, `item` its run, `detail` the campaign or how it ended.
- **Scenes:** `scene arrived`, `scene spoken`, `scene posed`, `scene blocked`, `scene finished`, `scene choice`, `scene response`, `scene failed`. `actor` is the resident; `detail` is the cue (for a choice, `cue.option`).
- **Regions:** `region entered` and `region left` (`target` is the region id); `region occupied`[w] and `region emptied`[w].
- **Coming and going:** `arrival` (sign-in), `enter place` (`target` the place), `departure`, `relocation`, `character created`, `spawn`, `welcome`, `welcomed back`, `near resident`[w] (needs `match.target`; fires within 3 tiles).
- **Talk** (25): `conversation` (`actor` the speaker, `target` who was spoken to; the dialogue window counts), `marriage`, `groomed`, `introduced`, `performance`, `shared meal`, `story told`, `table game`, `table game begun`, `tended`, `together`, `trouble heard`, `trouble solved` (`item`: short\|feud\|work\|child), `vouch`, `vouch broken`, `witnessed`, `matchmaking`, `nickname`, `nickname dropped`, `promise`, `promise broken`, `sermon`, `tie ended`, `mantle bound`[w], `mantle released`[w].
- **Gifts:** `gift` (`target` the recipient, `item`, `quantity`, `coins`; the first gift of the day between that pair), `gift again`, `gift used`, `forage`, `lent`, `loan returned`, `loan unreturned`, `item picked up`[w], `item dropped`[w]. A base item id in a filter also matches crafted and marked variants.
- **Money:** `economy`, `contract done` (`detail` is `"<kind>: <desc>"`), `contract taken`, `contract posted`, `contract given back`, `archive work`, `lease`, `lease ended`, `stall rented`, `stall sale`, `stall closed`, `harvest over`, `harvest called`, `nudge over`, `prices reset`, `times cause`, `times nudged`, `trade done`[w], `purse crossed`[w] (needs `target` the purse and `detail` the amount in pennies).
- **Fights:** `hunted` (`target` the species), `fight ends`, `death` (**`actor` is who died; there is no target**), `revival`, `hunt`, `huntRole`, `assault`, `downed`, `fight`, `mourning`, `old age`, `wolf downed`[w], `wolf revived`[w], `injury healed`[w].
- **Bandits:** `raid`, `bandits gather`, `camp cleared` (`target` is `camp_<cell>`), `bandits scattered`, `patrol`, `carter robbed`, `paid off bandits`, `bandit falls` (`actor` landed the last blow, `target` the camp), `bandits called`, `bandits creep`, `bandits demand`, `carter hurt`, `cleared`, `robbed`.
- **Caravans** (15): `caravan arrives`, `caravan departs`, `caravan lost`, `caravan dropped`, `caravan home`, `caravan passes`, `wagon overdue`, `wagon held up`, `wagon posted`, `wagon cancelled`, `wagon delayed`, `cart arrives`, `cart departs`, `cart missed`, `cart stocks the stalls`.
- **Ships:** `ship docks`, `ship lost`, `ship sails`, `ship stormbound`, `ship posted`, `ships cleared`.
- **Letters:** `letter sent`, `letter delivered`, `letter returned`, `notice answered`, `notice posted`, `notice removed`, `crier`, `resident letter`.
- **Law:** `fine paid`, `pact sealed`, `reported`, `told the wardens`, `vouched to the wardens`, `warrant`, `arrest`, `released`, `stolen goods smelt`, `stopped by the watch`, `crime seen`[w], `standing crossed`[w] (needs `target` the faction and `detail` the number), `relation changed`[w].
- **Town:** `festival`, `festival called`, `festival entered`, `festival won`, `project built`, `project gift`, `project ruined`, `project cancelled`, `project posted`, `project proposed`, `project removed`, `event added`, `event begins`, `event notice`, `event posted`, `event rolled`.
- **Fame:** `storyline objective`, `storyline step`, `storyline begun`, `storyline done`, `tale begun`, `tale joined`, `tale ended`, `deed`, `deed revoked`, `held for a story`, `milestone`, `story prize`, `skill practised`[w].
- **Quests:**
  - From the game: `quest accepted` (`target` the run, `item` the giver; empty when taken from the journal; `board:<town>` when taken from a notice board; `detail` the quest), `quest declined`, `quest handed in` (`item` the giver), `quest abandoned`, `dialogue choice` (`item` the resident, `detail` the choice id), `party joined`, `party left`, `party formed`[w], `party ended`[w].
  - The story's own: `quest started`[s], `quest completed`[s], `quest failed`[s], `objective completed`[s] (`item` the objective, `detail` the quest).
- **Places:** `door opened`[w], `door closed`[w], `door locked`[w] (a wolf tried a locked door), `building entered`[w], `building left`[w].
- **Time:** `weather changed`[w], `hour`[w] (`detail` is the hour), `day begins`[w], `dawn`[w], `night falls`[w], `season begins`[w].
- **Dialogue:** `dialogue opened`[w] (`detail`: resident, trade, missions...), `dialogue node`[w] (`detail` the node, `item` the tree; a story's own nodes are `@story:<run>:offer|thanks|reply:<id>`), `dialogue closed`[w].
- **Objective and mission events:** 63 events may be quest objectives or mission objectives. They are the ones marked `mission` in the catalogue.
  - Measures: `quantity` comes from gift, gift again, hunted, project gift, deed, festival won, forage, hunt, lent, loan returned, stall sale, table game, together, item picked up, trade done.
  - `coins` comes from gift, gift again, contract done, project gift, festival entered, archive work, contract taken, festival won, fine paid, lease, lease ended, letter sent, notice posted, paid off bandits, stall rented, stall sale, table game, trouble solved, trade done.

## 5. World effects (71; `S` = the story keeps it itself, `W` = acts on wolves, needs the owner or "may act on wolves" on a hosted world, `B` = needs a builder or owner, `U` = undone when the run ends)

| Group | Effects with params |
|---|---|
| Telling | `story.tell {to: participants\|wolf\|party\|region\|world, player?, region?, text≤300 one line, from?(resident)}`; `journal.note {player?, text≤200}`; `story.ping {cell,x,y,player?,label≤40,minutes 1-240}`U; `story.tracker {shown,var}`S |
| Residents | `npc.line {npc,text≤200}` (entry effects only); `npc.walk {npc,cell,x,y,pace walk\|run,cue}`; `npc.speak {npc,text≤200,cue}`; `npc.pose {npc,posture standing\|sitting\|lying,cue}`; `npc.move {npc,cell,x,y}`; `npc.protect`, `npc.unprotect`, `npc.revive`, `npc.kill` (never a protected resident), `npc.hold`U, `npc.release` `{npc}`; `npc.follow {npc,player}`U and `npc.unfollow`; `npc.offer {npc,id,words≤120,reply≤300,player?,once?}`S; `mantle.assign {npc,mantle}`U and `mantle.release`; `visitor.add {visitor id [a-z0-9_-]{3,40}, name≤60, cell,x,y, minutes 1-1440, like?, description?≤200}`U and `visitor.leave` |
| Wolves | `deed.award {player,detail≤200}`; `deed.revoke`; `character.grant {player,coins 1-2500,purse,reason≤80}`; `item.give {player,item,quantity 1-20}`; `item.take`W; `standing.adjust {player,faction,points -10..10,reason}`B (refused for a wolf with no Chapter); `story.practice {player,skill craft\|commerce\|labour\|service\|travel\|watch}`; `character.injury {player,add,severity 1-3}`W; `bandits.call {player,count 1-6}`; `warrant.issue {player,town,fine 0-200,reason}`W; `warrant.clear`; `participant.add` and `participant.remove`S |
| Places | `story.area.damage {region,points 1-99}`W; `story.area.item {region,item,quantity 1-100}`; `story.area.buff {region,buff "fed",hours 1-168}`; `story.area.spawn {region,count 1-6}` (bandits); `door.lock`U, `door.unlock`U, `door.open`, `door.close` `{door}`; `item.place {item,quantity 1-5,cell,x,y}`; `board.post {town,text≤300,days 1-28}`U; `weather.set {place\|town, weather, hours 1-72}`U |
| Economy | `festival.call {town,name≤80,inDays 0-30}`; `project.post {town,project,cell,x,y,title}`; `project.complete` and `project.cancel`B; `times.nudge {market,points -100..100,days,words}`; `harvest.override {area,word poor\|fair\|good\|clear,days,words}`; `band.override {npc,band,days}`; `ship.storm {ship}`; `event.add {market,event,days 0-7,hour?}` (events: house_wedding, works_fire, good_catch, orchard_blight, festival_buying, lame_ox, rich_cargo, lost_ship, mill_stopped, drovers_in, salt_short, brewers_glut) and `event.cancel`; `prices.reset`B; `faction.relation {faction,other,stance,disposition -100..100,reason≤300}`B |
| Quests | `quest.offer`, `quest.start`, `quest.complete`, `quest.fail` `{quest,player}`S; `objective.complete`, `objective.fail`, `objective.reveal` `{quest,objective,player}`S; `objective.progress {...,amount}`S; `storyline.give {player,template,resident,place}`; `storyline.tick {player,template,step,objective}` |
| Chapters | `story.start {story,audience same\|own\|list,list?,carry?,when ended\|now}`S |
| Scenes | `scene.play {scene,player\|"world",cue≤64}`; `scene.stop {scene?}` |

Purses are `town:<town>`, `town:<town>:works|watch|church`, `till:<id>` or `chapter:<id>`. They are never minted: the game refuses when the purse is short. `Game::storyPurseAccount` maps `town:<id>` to that town's store account, then `stores:<id>`, then `treasury`.

## 6. Quests (`quests[]`, v2 only; `tools/story_quests.py`)
Required: `id`, `title` (120 characters), `objectives` (1–12, at least one required). Optional fields:
- `summary`: 300 characters.
- `scope`: `player` (default), `party` (members fixed at start), or `story` (one copy for everyone; `contributors` 1–500).
- `lifetime`: `campaign` (default) or `phase` (copies expire when the visit ends).
- `phases`: the phases it is offered in; none means every phase.
- `repeat`: `once`, `daily`, `weekly` or `always`. A story-scope quest can only be `once`.
- `giver`: a resident id, checked at approval.
- `board`: a town id, which puts the quest on that town's notice board.
- `offer`: `{say ≤300 (required; may use the talk library's #symbols#), ask ≤120, accept ≤120, decline ≤120, handIn ≤120, thanks ≤300}`. It needs a giver. Defaults: ask "Ask about: <title>", accept "I'll do it.", decline "Not now.".
- `handIn`: defaults to true when there is a giver. The quest goes "ready", then the wolf hands it in to the giver.
- `join`: `{who: anyone|participants, region, cap, limit}`. `anyone` only works with an open or everyone audience.
- `abandon`: default true.
- `fail`: up to 4 rules, each `{on, match}` or `{afterDays, clock}`.
- `rewards`: up to 8 effects, given to each wolf with credit where they say `$actor`. They cannot be `npc.line`, `npc.offer` or `participant.remove`.
- `bonus`: up to 4 effects; requires at least one optional objective.

**Objective:** `{"id","label"≤180,"on"}` plus `match`, `goal` (1–1e6), `measure` (count, quantity or coins), `signal` (with `on:"story signal"`), `after`: [ids] (no cycles), `optional`, `hidden`.

**The dialogue window** (Core: RatwGameWindowTalk, RatwGameDialogue):
- On the resident's hub node, the window shows each quest that resident offers to an eligible wolf: `ask`, then `say` in the resident's voice (dialect and mantle), then accept or decline.
- It shows each ready quest the resident takes back: `handIn`, then `thanks`.
- It shows each `npc.offer` choice: `words`, then `reply`.
- These raise the events `quest accepted`, `quest declined`, `quest handed in` and `dialogue choice`.
- A story caps out at 64 offers and choices; each wolf sees up to 64 quest views and 16 trackers.

**Notice board:** a quest with `board` appears under QUESTS with TAKE IT ON. Taking it raises `quest accepted` with item `board:<town>`. It is handed in to its giver if it has one.

**Journal:** shows each quest's state and objectives (ticked, failed, "3 / 4", optional, "later", hidden), the deadline, "Ready: return to X", the trackers (shown variables) and ABANDON. A quest offered with no giver gets ACCEPT and DECLINE (via `quest.offer`).

**Joining:** in an open story, a wolf joins by accepting. Otherwise a trigger can bring them in: e.g. `{"id":"greet","on":"arrival","actions":[{"do":"effect","effect":{"kind":"quest.offer","quest":"scout","player":"$actor"}}]}`.

**Not built:** `{variable}` substitution in quest words; a quest goal read from a variable; a "requires quest X" join field (use a `quest is` condition plus `quest.offer` instead).

## 7. Chapters, campaigns and scenes
**Campaign:**
- Approved with op `campaign.approve` and a body `{campaign: {id, title≤100, chapters: [{program, digest}] (1–12)}}`.
- The first chapter gets a publication ticket. Each later chapter is started by the one before it with `story.start` naming another chapter.
- A program containing `story.start` is refused if approved alone.
- `carry` copies story and player variables with the same name, scope and type into the next chapter.
- A rehearsal case can check this with `expectChapters`.
- Other live stories hear `story started` and `story ended`.
- World variables: op `world-var {do: declare|set|remove, name, type, value, note, reason}`.

**Scene** (`ratw-scene` v1):
- Top-level keys: `{id, title, visibility: public|individual|party, lockMovement, area (required for public), camera?, cast: [1-16 {id, npc | selector {role?, cell?}}], start, beats: [1-64]}`.
- Beat types:
  - `line {text≤400, seconds 1-600, next?, ai?}`
  - `choice {text, seconds, mode: first|majority, options [2-8 {id, label≤80, next?}], default}`
  - `move {cell, x, y, pace}`
  - `pose {posture}`
  - `response {text, seconds, mode: first|all, next, timeoutNext}`
- An `ai` block is `{personality≤600, intent≤600}` and uses the linked NPC Mind, with the fallback text.
- To use a scene in a story, embed it under `scenes` and play it with `scene.play`. Its outcome raises `scene choice` and `scene finished`.
- Scenes are the only place with speaker panels (painted faces) outside the dialogue window.

## 8. Rehearsal cases (`tests[]`)
- Shape: `{name≤100, cohort: [ids] ([] allowed for an open story; must equal the players for a players audience), events: [≤256], expect: [phase path]}`.
- Optional: `expectEffects` (in order, with `$actor` resolved), `expectMissions`, `expectMissionVisits`, `expectTriggers`, `expectCounters`, `seed`, `rolls` [0-99], `parties` {wolf: party}, `expectVariables` {story|player|party|world}, `expectOutcome`, `expectQuests` `{quest: {wolf|"party:<id>"|"story": status}}`, `facts`, `expectChapters`.
- A fact is `{id, kind, day}` plus `actor`, `target`, `cell`, `item`, `detail`, `quantity`, `coins`, `elapsedDays`, `x`, `y`, `facts`.
- Budget: 200,000 steps or 8 s, with 2 rehearsals at once.
- Approval requires every case to pass **and every phase to be reached** (`uncovered` must be empty). Triggers no case reaches are reported (`uncoveredTriggers`) but do not block.

## 9. From a file to a live run
1. **Validate and rehearse offline** (pure, no database):
   ```
   cd tools && python3 -c "import json,story_program as r; p=json.load(open('X.json')); rep=r.review(p); print(rep['passed'],rep['uncovered'],rep['uncoveredTriggers'],rep['digest']); print(r.problems(p))"
   ```
   Or use the bridge: `echo '{"op":"rehearse","program":{...}}' | python3 tools/story_author.py`. The ops are `validate` (returns the digest), `rehearse`, `catalog`, `draft`, `components`.
2. **Check world references** (reads only): `story_runner.program_problems(conn, world, p)` and `story_runner.references(conn, world, p)`, which is the check approval runs.
3. **Approve.** This needs a DM-rank account (on a hosted world, a builder for B effects).
   - Studio route: `POST /api/dm/story-program` with `{target, op: "approve", program, digest, forRun?}`, served by `tools/dungeon_master.py`. No Studio or DM tool process was running.
   - Direct route: `with world_db.connect('ridgemere3','dm') as c, c.transaction(): story_runner.approve(c, 'upper_accord', who, program, digest)`.
   - Either way, the approved release is inserted into `dm.story_releases` with status 'approved' and the call returns `{run, ticket}`. The ticket is valid for 1 day.
4. **Publish:** op `publish {ticket}` or `story_runner.publish(c, world, ticket)`. This calls `launch`, which freezes the audience (except open and everyone), enters the start phase and queues `story.phase`. The status becomes 'running'.
   - Only one live run per program id is allowed per world.
   - For a campaign, use `approve_campaign(c, world, who, {"campaign": {...}})`, then publish the ticket for chapter 1.
5. **Run:** it does need the runner, which is already running on ridgemere3. It reads `game.events` through LISTEN `ratw_events` and `ratw_story`, sends batched `story.effects`, `story.cohort` and `story.quests`, and waits for receipts in `dm.actions`.
   - The Campaigns console (Editor DM suite) steers runs: hold, resume, stop, signal, fire, goto, set, complete objective, and more.
   - A changed program is a new approval, or `forRun` plus a migrate command.

**What approval checks against the world:** quest `giver`; residents, factions, places and wolves named in effects; `conversation`, `gift` and `gift again` filter targets in **triggers and missions**; `enter place` targets; region and walk tiles; items in `item.give`, `item.take`, `item.place` and `story.area.item`; projects; DM templates; doors; mantles; towns for board, warrant, weather and project effects; scene cast and tiles.

**Gaps found by testing on R3, read-only. Approval did not catch:**
- (a) Quest **objective** `match` targets and items. The objective `conversation→nobody_here` passed.
- (b) `character.grant` purses: `town:nowhere` passed. The game refuses at run time, and the reward then waits "when here".
- (c) A quest `board` town: `nowhere` passed.
- (d) `program_problems(..., with_world=True)` reported **zero** problems for an unknown giver and an unknown reward item, though `references()` refuses both.
- Implication: authors must check objective ids, purses and boards themselves.
- Doc error: `Story-Maker-guide.md` §7 uses `{"on":"death","match":{"target":"rm111"}}`. The death event has only `actor`, so that rule never fires; use `actor`.

## 10. Where new content must not go
`Data/Storylines/programs/` is bundled as "examples". Three tests hard-code 108 (8 programs + 100 library entries):
- `tools/test_story_native.py:36`
- `tools/test_story_drafts.py:55` and `:63`
- `Editor/src/lib/workbench.test.ts:185`

Also, `test_story_native` upgrades and rehearses every example. Put R3 content in a new directory (e.g. `Data/Storylines/ridgemere3/`), or update those counts.

## 11. Tests
- **Python, under `tools/`:**
  - Format and rehearsal: `test_story_program.py` (17 more tests run with `RATW_STORY_TEST_DSN` set to a throwaway migrated database), `test_story_v2.py`, `test_story_v2_runner.py`, `test_story_problems.py`.
  - Quests, facts and chapters: `test_story_quests.py`, `test_story_facts.py`, `test_story_facts_live.py`, `test_story_campaigns.py`.
  - Catalogue, effects and guide: `test_story_catalogue.py`, `test_story_effects.py`, `test_story_guide.py` (also `python3 tools/story_guide.py --check`).
  - Engine and console: `test_story_native.py` (Python vs native parity; needs `build-core/libratw_story.so`), `test_story_versions.py`, `test_story_steering.py`, `test_story_console.py`, `test_story_explain.py`.
  - Other: `test_story_drafts.py`, `test_story_scenes.py`, `test_story_scene_live.py`, `test_story_siege_live.py`, `test_story_speed.py`.
- **C++:** `Tests/storyline_tests.cpp` (storyQuestsInTheGame), `Tests/dialogue_tests.cpp` (storyQuestsInTheWindow), `Tests/story_facts_tests.cpp`, `Tests/story_world_tests.cpp`, `Tests/storyteller_tests.cpp`.
- **Client and Editor:** `Client/src/game/quests.test.ts`; `Editor/src/lib/workbench.test.ts`, `campaigns.test.ts`, `guide.test.ts`.
- **A new program's real test** is `review(p)['passed']` with nothing in `uncovered`, `problems(p) == []` and `references()` passing on R3.

## 12. What the trigger system cannot do yet (relevant to items 6–8)
- **Story-side dialogue:** a story can add only one `npc.line` per resident per phase (200 characters), plus `npc.offer` choices (120/300 characters, one exchange) and quest offer and thanks text. It has no multi-node dialogue tree, no gossip, and no way to plant a belief: `belief.plant` and `npc.brief` are listed as "Later". Branching talk lives in Data/Talk (doc 83/84), outside the story format. Item 6 (more talking paths) and item 7 (gossip and helpful knowledge about quests) therefore need either Data/Talk authoring or a new effect.
- **Events doc 85 says were not built:** `month begins`, `price crossed`, `resident arrived` and `resident left`, `variable changed`, `admin signal`, `trigger fired`.
- **Quest limits:** no `{var}` substitution in words, no quest goal from a variable, no quest-prerequisite field.
- **Rewards available:** deed, coins from a purse (2500 at most), item (20 at most), standing (±10; builder; Chapter only), practice, storyline. There is no XP reward.

## 13. Example: one complete small program, verbatim
This is `tools/story_v2_samples.py` QUEST_SMALL, rendered as JSON; it rehearses `passed=True`. The quest example from `Docs/Story-Maker-guide.md` §7 is in section 14 below.
```json
{"format": "ratw-story-program", "version": 2, "id": "carter-axle", "title": "An axle for the carter", "audience": {"kind": "open", "cap": 50}, "start": "open",
 "phases": [{"id": "open", "journal": "The carter at the ford needs an axle.", "effects": [], "exits": []}],
 "quests": [{
  "id": "axle", "title": "An axle for the carter", "summary": "The carter at the ford is stuck.", "giver": "rm111",
  "offer": {"say": "My axle split at the ford. Could you find me another?", "accept": "I will find you one."},
  "repeat": "daily", "join": {"cap": 2},
  "objectives": [
   {"id": "ask", "label": "Ask the carter which part gave way.", "on": "conversation", "match": {"target": "rm111"}},
   {"id": "bring", "label": "Bring two axles.", "on": "gift", "match": {"target": "rm111", "item": "axle"}, "after": ["ask"], "goal": 2, "measure": "quantity"},
   {"id": "wheel", "label": "A spare wheel would not go amiss.", "optional": true, "on": "gift", "match": {"target": "rm111", "item": "cart_wheel"}},
   {"id": "thief", "label": "Find who cut the axle.", "optional": true, "hidden": true, "on": "conversation", "match": {"target": "rm119"}}],
  "rewards": [{"kind": "deed.award", "player": "$actor", "detail": "Got the carter moving."},
              {"kind": "character.grant", "player": "$actor", "coins": 12, "purse": "town:greyfen", "reason": "the carter thanks you"}],
  "bonus": [{"kind": "item.give", "player": "$actor", "item": "bread", "quantity": 2}]}],
 "tests": [{"name": "Ash takes it, does it and hands it in", "cohort": [], "events": [
   {"id": "e1", "kind": "quest accepted", "day": 1, "actor": "player-ash", "target": "run", "item": "rm111", "detail": "axle"},
   {"id": "e2", "kind": "gift", "day": 1.1, "actor": "player-ash", "target": "rm111", "item": "axle", "quantity": 2},
   {"id": "e3", "kind": "conversation", "day": 1.2, "actor": "player-ash", "target": "rm111"},
   {"id": "e4", "kind": "gift", "day": 1.3, "actor": "player-ash", "target": "rm111", "item": "axle", "quantity": 2},
   {"id": "e5", "kind": "quest handed in", "day": 1.4, "actor": "player-ash", "target": "run", "item": "rm111", "detail": "axle"}],
  "expect": ["open"], "expectQuests": {"axle": {"player-ash": "done"}},
  "expectEffects": [
   {"phase": "open", "kind": "deed.award", "player": "player-ash", "detail": "Got the carter moving.", "reward": "axle"},
   {"phase": "open", "kind": "character.grant", "player": "player-ash", "coins": 12, "purse": "town:greyfen", "reason": "the carter thanks you", "reward": "axle"}]}]}
```
How the rehearsal plays out:
- e2 arrives before `ask` is done, so it does not count.
- An expected effect carries `"reward": "<quest id>"`.
- For R3, every `rm111`, `rm119` and `town:greyfen` must become R3 ids.

## 14. Example: one quest, verbatim
From `Docs/Story-Maker-guide.md` §7. It shows `fail`, a hidden signal objective and `bonus`; its `death` fail rule has the filter bug noted in section 9.
```json
{"id": "axle", "title": "An axle for the carter", "summary": "The carter at the ford is stuck.",
 "scope": "player", "lifetime": "campaign", "repeat": "daily", "giver": "rm111", "handIn": true,
 "offer": {"say": "The axle split at the ford. Could you find me another?", "ask": "Is something wrong with the cart?"},
 "objectives": [
   {"id": "ask",   "label": "Ask the carter which part gave way.", "on": "conversation", "match": {"target": "rm111"}},
   {"id": "bring", "label": "Bring a replacement axle.", "on": "gift", "match": {"target": "rm111", "item": "axle"}, "after": ["ask"]},
   {"id": "wheel", "label": "A spare wheel would not go amiss.", "on": "gift", "match": {"target": "rm111", "item": "cart_wheel"}, "optional": true},
   {"id": "thief", "label": "Find who cut the axle.", "on": "story signal", "signal": "thief-named", "hidden": true}],
 "fail": [{"on": "death", "match": {"target": "rm111"}}, {"afterDays": 3}],
 "rewards": [{"kind": "character.grant", "player": "$actor", "coins": 12, "purse": "town:greyfen", "reason": "the carter thanks you"}],
 "bonus": [{"kind": "deed.award", "player": "$actor", "detail": "Brought the carter a spare wheel too."}]}
```

## 15. Other worked patterns to copy from
- **Many quests, party scope, npc.offer, a region:** QUEST_CAMPAIGN (`ford-campaign`) in `tools/story_v2_samples.py`. Its trigger `{"id":"roped","on":"quest completed","audience":"participant","match":{"detail":"ropes"},"actions":[{"do":"add","var":"ropes","value":1},{"do":"enter","phase":"freeze"}]}` moves the main line forward when a quest is done.
- **Every flow action:** LEDGER and FLOW in the same file.
- **Every fact condition and watched event:** FACTS_WATCH in the same file.
- **Regions, scenes, the area spawn and a shared counter:** `Data/Storylines/programs/siege-at-greyfen.json`, the v2 example.