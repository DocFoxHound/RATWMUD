# Talk survey: quest dialogue, gossip, dialect, checks (Ridgemere 3)

Read-only. Nothing in the repo or the database was changed. I read `ratw_ridgemere3` as role `dm`, with `SET default_transaction_read_only=on`, through `tools/world_db.connect('ridgemere3','dm')`. Probe files went to /tmp and are deleted.

## 0. Short answers
- **Quest speech in the dialogue window** comes from the story program, not from the talk library:
  - the `quests[].offer` block (`say`, `ask`, `accept`, `decline`, `handIn`, `thanks`);
  - `npc.offer` effects (a choice and its reply);
  - `npc.line` (a line added to the greeting).
- The resident's lines (`offer.say`, `offer.thanks`, `npc.offer.reply`) are run through `Game::inVoice`. Their `#symbols#` are filled in the speaker's own voice, in this order: their mantle's words, then their dialect at their accent, then plain.
  - The wolf's words (`ask`, `accept`, `decline`, `handIn`, `npc.offer.words`) are not expanded. They are plain player speech.
  - `npc.line` is not expanded either: raw text.
- **Multi-node conversations** exist only in mantle grafts and in the `.tree` files. Neither can test anything about a story or quest. There is no talk fact for a run, its phase, its variables or a wolf's quest status.
- **Gossip cannot be conditioned on story or campaign state directly.** It can be reached indirectly:
  - `deed.award` (residents gossip about a wolf's deed);
  - `event.add`, `bandits.call`, `festival.call`, `ship.storm`, `harvest.override`, `times.nudge`, `npc.kill`, `warrant.issue` (they change the world facts that residents' talk already reads);
  - `mantle.assign` (a whole story-written voice for one resident);
  - `live.figures` rows (`world.figure_hero` / `world.figure_villain`). These are database rows plus a `voice.sync`, not a story effect.
- **What's missing (section 6):**
  - a story effect that writes talk memory (`world.mem.*`);
  - `belief.plant` (doc 85 §3.5 lists it as "Later", not built);
  - a `player.quest*` window fact.

  I proved the gating side works: a `.talk` rule, a scene and a tree `say[...]` gated on `world.mem.blight=spreading` all pass `talk_check --validate`. Only the writer is missing.

## 1. Files and formats
Every `*.talk`, `*.grammar` and `*.tree` under `Data/Talk/` is loaded recursively by `Library::load` (`Core/RatwTalk.cpp:2240`): groups first, then trees, then the rest.
- A new file such as `Data/Talk/scenes/common/story_<campaign>.talk` loads automatically. There is no registry to update.
- The library is read at server start only. Text changes in Data/Talk need a server restart; publishing a build restarts the central world.
- Mantle rows and figures reload hot through the `voice.sync` order (`RatwGame.cpp:1428`).
- `.mantle` files under `mantles-export/` are not library files.

**Talk facts** (`Data/Talk/facts.json`, namespaces `person` / `pair` / `here` / `topic` / `world` / `conv` / `x` / `player`):
- Useful for quests and gossip:
  - `self.id`, `self.mantle`, `self.job`, `self.role`, `self.from`, `self.house`;
  - `here.region` (ridgemere, frosthollow, coldwater, oakhaven, mirebridge, larkfield, southmeadow, fenshore, saltwell, wild) and `here.town`;
  - `world.event` / `event_words` / `event_when`, `world.news` / `news_from`, `world.bandits`, `wagons`, `ships`, `harvest`, `times`, `festival`, `death`, `arrest`, `mood`, `crime`;
  - `world.figure_{ruler,lord,captain,high_priest,hero,villain}`;
  - `topic.deed`, `topic.subject`, `topic.claim`, `topic.claim_kind`, `topic.meeting` (stranger, known, returning, famous, famous_look);
  - the memories `self.mem.<k>`, `pair.mem.<k>`, `world.mem.<k>`: any key, any value (`RatwTalk.cpp:964`).
- `player.*` holds only `named`, `met`, `knows` (claim kinds heard), `newcomer` and `asked`.

**A rule** (concepts must be in the `settings.json` list of 198, e.g. `talk.smalltalk`, `talk.gossip`, `talk.news`, `talk.event`, `talk.deed`, `bark`):
```
rule n7_fg_villain
when concept=talk.smalltalk|talk.lore|talk.bandits world.figure_villain
does tell.story
cooldown 3d
say {on=answer ans=villain} Lock up tonight. They say #world.figure_villain# has been seen on the roads. | ...
then other: town.answer
```
Other keywords: `owes other: <move>`, `asserts`, `binds x=<slot>`, `remember town|pair|self|x: k=v Nd`, `tone`, `priority N`.

**A scene** (whole NPC-to-NPC talks; they carry most of the ambient talk since doc 84 §37):
```
scene hy_ev_works_fire
when topic=event world.event=works_fire world.event_when=now
does tell.news / react.news / tell.self / react.self
tone serious / even / even / even
a : The fire at the works. Have you heard? | They're still damping down... | You could smell the works burning...
b[job=crafter|laborer|smith] #hands : I heard. Half of us are out of work... | ... | ...
b #plain : I heard. Was anyone hurt? | ... | ...
a@hands : That's hard. ... | ... | ...
a@plain : Not that I've been told. | ... | ...
```
- `|` alternatives are parallel threads.
- `does` names one move per turn.
- Always give a bare `b :`.
- Topics: the everyday ones (weather, day, smalltalk, work, food, family, home, friends, pastimes, animals, health, memories, dreams, faith, travel, land, season, lore, accent) and the news ones (gossip, crime, bandits, caravan, prices, ships, event, times, harvest, newcomer, festival, life, deed), plus player, quarrel, news, price_cause.
- The topic set is fixed in C++ (`topics::sources()`); `topics.json` can only weigh it. A new "story" topic needs C++.
- Library fallbacks (`library.talk`) use `priority -1`.

**A tree** (`Data/Talk/dialogue/*.tree`):
- The trees and their entries:
  - `resident` (entries talk/chat/news/way/introduce; for everyone);
  - `shop` (role=merchant), `inn` (job=innkeeper, p2), `watch` (role=guard, p3), `church` (job=priest, p1), `companion` (p4), `receiver` (here.deliveries>0, p5), `official`, `work`, `master`.
- Lines are `say[criteria priority=N] alt | alt` (each node needs one bare `say` or an `ask`).
- Choices are `choice words [criteria] -> node | end | do <action> args ; then N ; refused N`, plus `each x in <list> max N`, `free`, `mood`.
- A shared `.talk` rule cannot target `dlg.*` concepts. I tested it: "'dlg.resident.news' is not a concept (settings.json)". Shared window lines therefore go into the tree's `say[...]`. I validated this probe:
```
say[world.mem.blight=spreading priority=2] It's all blight talk. The orchards up the vale are black.
```
- The places that already print the news and gossip: `resident.tree` node `news` (it reads `world.event_words`, `world.news`, `world.wagons`, `world.death`, `world.wedding`) and node `gossip`. The gossip node uses `here.gossip=yes` with `#topic.subject# #topic.claim#`: the freshest belief, at most a week old. A rumour told there is stored as a claim the player heard.

**Grammar** (`Data/Talk/grammar/plain.grammar` plus coldwater, frosthollow, larkfield, mirebridge, oakhaven, saltwell, southmeadow):
- `sym = a | b`, `sym[facts] = ...`, `sym! = ...` (a habit), `sym!! = ...` (per listener), `sym.broad = ...` (dialect only), `words^m | words^f`.
- Every symbol must exist in plain.
- Fenshore speaks mirebridge (`settings.json` `dialect_of`). Ridgemere speaks plain.

**Claims** (`Data/Talk/claims.json`, 15 kinds: death, marriage, theft(.attempt/.unseen), assault(.unseen), violent, warrant, promise.broken/kept, prowling, newcomer, bandits.camp, bandits.robs):
```
"theft.attempt": {"family":"theft","grave":2,"they":["#x.name# tried to steal from #y.name#","#x.name# #claim.text#"],"victim":[...],"to_victim":[...],"self":[...],"text":{"starts":["tried to steal"]}}
```
A claim with no wording that can be filled is never spoken. Beliefs (`struct Belief`, `Core/RatwRoads.h`) spread along bonds every day and between towns with the caravans.

## 2. (a) Quest dialogue paths
**A. The quest offer block** (story program v2 `quests[]`, checked by `tools/story_quests.py:check_quest`; doc `Docs/story-programs.md` "Quests and a living audience"):
```json
{"id":"axle","title":"An axle for the carter","summary":"...","giver":"rm111",
 "offer":{"say":"#greet#, #address#. My axle split at the ford. Could you find me another?",
          "ask":"You look stuck.","accept":"I will find you one.","decline":"Not now.",
          "handIn":"Here's your axle.","thanks":"#thanks#. I'll be moving by noon."},
 "handIn":true, "objectives":[...], "rewards":[...]}
```
- Size limits: `say` 300, `thanks` 300, `ask`/`accept`/`decline`/`handIn` 120. `say` is required whenever `offer` is present, and `offer` needs a `giver`.
- Defaults:
  - ask: "Ask about: <title>";
  - accept: "I'll do it." / decline: "Not now.";
  - handIn: "About <title>: it's done.";
  - thanks: "#thanks#. That's a weight off.";
  - shown once taken: "That's seen to, for now.".
- Without a giver, the wolf accepts or declines in the journal. With `board: <town>`, the offer shows on that town's notice board (TAKE IT ON).

How the game shows it (`Core/RatwGameWindowTalk.cpp` `windowNode` / `storyNode`, `Core/RatwGameStorylines.cpp:1157` `storyChoicesFor`):
- The offer appears as a choice on the node the "talk" entry opens: the resident hub, or inn/church greet, watch/companion hub, receiver greet.
- It sits before the way out, in whatever room the node leaves (nine choices at most). Story choices come before the wearer's grafts, and **any that don't fit are dropped**. A resident hub usually shows 4-6 choices, so expect room for 3-5.
- The nodes are `@story:<run>:offer|thanks|reply:<id>`. The offer node shows `say` with Accept, Decline and "Goodbye for now." Accept and decline go back to the home node. Thanks and reply nodes get "Something else..." and "Goodbye for now.".
- Events: `quest accepted`, `quest declined`, `quest handed in`, `dialogue choice` (item = the resident).
- **Nothing is shown with the giver while a quest is under way but not ready.** Only the offer (while open) and the hand-in (when ready) appear.

**B. `npc.offer`** (a story effect; catalogue `Data/Storylines/trigger-catalogue.json`):
```json
{"kind":"npc.offer","npc":"rm119","id":"hale-news","words":"Any news of the ice?","reply":"Thin at the edges, #address#.","player":"$actor","once":true}
```
- Limits: `words` 120, `reply` 300; it lasts for the phase visit.
- Only wolves taking part in the story see it (`s.takesPart(wolf)`).
- Picking it fires `dialogue choice` with detail = the id. Chain the next step with a trigger on that event that adds another `npc.offer`; this is how a multi-step talk is built.
- The game holds up to 64 offers plus choices per story.

**C. `npc.line`** (phase entry only, at most 12 per phase, 200 characters): added to the resident's greeting (`RatwGameDialogue.cpp:1233`, `worldStoryLine`). Only the story's wolves get it. Raw text, no symbols.

Related effects:
- `npc.speak` (spoken aloud, 200 characters, fires "scene spoken");
- `story.tell` (300 characters, optional `from` resident with their face);
- `board.post` (300 characters, 1-28 days).

**D. Mantle grafts and window lines** (the only multi-node branches). They are written in a `.mantle` and stored as database rows (`live.mantles`, `live.mantle_lines`):
```
dlg @1 resident.hub [topic.meeting=known] : #greet#, #topic.player#. ...
graft resident.hub
choice Do you really keep every stone you find? -> stones
node stones
say Not every one. Only the smooth ones. ...
mood pleased
free
choice What do you do with them? -> stones_what
choice Goodbye. -> bye
```
- Nodes you can write `dlg` lines for:
  - resident: hub, town, job, business, news, gossip, trouble, helped, way, place, refused, bye;
  - shop: greet, prices, bye;
  - inn: greet, rooms, roomed, rest, refused, bye;
  - watch: hub, crime, reported, fine, paid, law, unpaid, bounties, refused, bye;
  - church: greet, service, poor, offering, thanked, refused, bye;
  - master: learn, work, wages, takes, taken, refused, bye.
- Moods: calm, formal, keen, pleased, stern, warm. A graft's choices may lead to its own nodes, the tree's nodes, `bye` or `end`.
- A story binds a mantle with `mantle.assign {npc, mantle}` and drops it with `mantle.release`; it is released when the story ends.
- **Problems with `mantle.assign`** (`Core/RatwGameHandoff.cpp:280`, `RatwHandoff.cpp:469`):
  - It **replaces** the resident's current mantle. 775 of the 1,086 voice rows (all of ridgemere3's residents) wear one.
  - At the story's end the mantle is released and **the old one is not restored**.
  - It is refused if the mantle isn't `ready`, is worn by someone else, or doesn't fit (`fits` stage, sex, role, class, job).
  - So bind story mantles to the 312 residents (of the 1,086) who wear none, or through `mantle_load --bind`.
- The `live.mantles` count is 805 ready: 491 plain, 119 coldwater, 68 larkfield, 34 mirebridge, 34 southmeadow, 26 saltwell, 17 frosthollow, 16 oakhaven (32 of them spares).

**E. Library window lines per resident without a mantle:** add `say[self.id=<npc> priority=N] ...` (or `self.mantle=`) to a tree node. `self.id` is a valid fact.

## 3. (b) Gossip and helpful-knowledge lines
What works today, without code:
1. **Deeds:** `deed.award {player, detail}` (200 characters; the game keeps 160) is recorded as a deed of kind `award` (weight great).
   - The game hands it to beliefs `deed:<id>`: witnesses, the helped one's household, the town crier's calls, and spreading after that.
   - Residents then talk of it: topic `deed` scenes (`scenes/common/deeds.talk`: `#topic.subject# #topic.deed#`) and the window greeting `topic.meeting=famous` ("You #topic.deed#, didn't you?").
   - The phrase is `{detail}` verbatim, so **write it lower-case, as a verb phrase with no full stop**: "drove the raiders off the Larkfield ford". The sample's "Got the carter moving." reads wrongly.
2. **World levers:**
   - `event.add` takes one of 12 fixed market events and sets `world.event` / `event_words` (scenes in `hybrid_events.txt`-style files such as `scenes/common/hybrid_events.talk`, `events_market_*.talk`, and the tree's news node);
   - `bandits.call` sets `world.bandits=near`; `festival.call` sets `world.festival`; `ship.storm` sets `world.ships`; `harvest.override` sets `world.harvest`; `times.nudge` sets `world.times`;
   - `npc.kill` gives the "died" beliefs and `world.death=recent`;
   - `warrant.issue` gives the Watch and warrant claims (not traced end to end).
   - These are generic: no free campaign words.
3. **Named figures:** `live.figures(world_id, id, position, name ≤120, title, role ∈ {ruler, lord, captain, high_priest, hero, villain}, known_in realm|region:<r>|house:<h>, about ≤600, npc_id)`.
   - Ridgemere 3 has **0 rows**.
   - Each row sets `world.figure_<role>` where it is known.
   - 15 existing lines use it (`rules/town7_people.talk:786-830`, `scenes/common/work7b.talk:84`).
   - Write rows through the DM (`tools/dungeon_master.py save_figure`, which queues `voice.sync`); there is no story effect for it.
4. **Story mantles** (section 2D): scenes, barks (`rule ... when concept=bark`) and window lines that the wearer speaks about the campaign. They are always on while bound; nothing can switch them by phase.
5. **For participants only:** `npc.offer` "Heard anything about X?" with a hint as the reply, and `npc.line` hints.

The pattern to write once the writer exists (validated with talk_check today):
```
rule cmp_blight_smalltalk
when concept=talk.smalltalk world.mem.blight=spreading
does tell.news
cooldown 1d
say They say the blight's past the mill now. #oath#. | The orchards up the vale are black, I hear.

scene cmp_blight_day
when topic=day world.mem.blight=spreading
does tell.news / react.news / assess
a : Have you seen the orchards? | Black leaves on every bough.
b : I've heard. | Don't. My cousin's trees are gone.
a : #bc_it_is# | Then pray to the Bright One.
```
Town memory belongs to `town:<here.region>` (the whole city is one owner). It holds at most 32 entries per owner and drops the soonest to expire first. It is saved in the checkpoint. Today only talk lines write it (`remember town: k=v Nd`, `Game::talkRemember`).

## 4. (c) Dialect
- **Each resident's voice:**
  - `live.npc_voice` holds `home_region`, `accent` (none/light/broad), `quarter`, `mantle_id`, `bound_how` and `released_day`;
  - a mantle's `dialect` and its own `words` come first;
  - an accent of none means plain words only.
- **In story text,** use only symbols that exist in plain:
  - `#greet#`, `#farewell#`, `#thanks#`, `#address#`, `#oath#`, `#very#`, `#good#`, `#bad#`, `#bc_it_is#`, `#talk_open#`, `#news_none#`;
  - facts such as `#self.first#`, `#other.called#`, `#here.town#`, `#topic.player#`.
- **Text that won't render:** an unknown symbol, or a `[` (Tracery's binding syntax), makes `inVoice` fail. The **raw text with its `#` marks is then shown** (`windowLine` falls back to `node.said`).
- **Things to avoid:**
  - Never write a dialect's phrase out ("Still water", "Dry feet", "peace on you") unless the giver speaks that dialect.
  - Never use `#greet.broad#`.
  - A symbol's value never ends its sentence (no `.;!?`); start sentences with `#x.capitalize#`.
- **Not checked by any tool:** `dialect_check` scans `Data/Talk` and `Data/Voice` only. Story programs are never scanned for dialect or truth, so check them by eye.
- **Grammars by town:** Saltwell has `saltwell.grammar`, Fenshore uses `mirebridge.grammar`, Ridgemere plain. Mirebridge itself was erased, so `here.region=mirebridge` gates never play.
- **Group names for regional gates** (`groups.talk`): cities, towns, north, south, mountains, lake, fens, woods, downs, farmland, warm, countryside, waste.

## 5. (d) Checks and commands (state today: all pass)
- **Library:** `build-core/talk_check --data "$PWD/Data/Talk" --validate FILE...` for `.talk`, `.tree` or `.grammar`; it takes under 0.1 s.
- **Probe a line:** `build-core/talk_check --data "$PWD/Data/Talk" dlg.resident.gossip here.gossip=yes topic.subject=Wren topic.claim="stole a ham"`, or with the world: `--world ridgemere3 A_ID B_ID talk`. Add `--all` to see what was passed over and why.
- **Trees in every voice:** `build-core/talk_check --data "$PWD/Data/Talk" --render-trees`. It fails on a line over 160 characters or a symbol left unfilled.
- **talk_lint:** `python3 tools/talk_lint.py`. It counts against the baseline `Tests/fixtures/talk_lint_baseline.json` and fails on any new finding. Today: 58 findings, 0 new.
  - Truth rules (`Data/Talk/truth.json`): fog or rain words need `here.weather` / `here.outdoors`; "this morning" needs `here.time`; "my husband" needs `self.married`; "my nets" needs `self.job=fisher`.
  - Also: `they_named`, `they_stranger`, `gossip_kind`, and no ids in text.
- **dialect_check:** `python3 tools/dialect_check.py` (lexicon, symbols, renders, reach, deities). Today: 68,496 lines, 0 failures, 14 lore warnings. `--world ridgemere3` adds origins and character tables.
- **Mantles (handbook §6.1):**
  - Brief: `python3 tools/mantle_brief.py ridgemere3 <npc>`. Town index: `python3 tools/mantle_index.py <town> --world ridgemere3`.
  - G1: `build-core/talk_check --data "$PWD/Data/Talk" --mantle FILE`.
  - G2: `python3 tools/mantle_check.py Data/Talk/mantles-export/<dialect> --world ridgemere3 --strict --facts Saved/Mantles/residents.jsonl`. That file is dated Nov 18; regenerate it with `talk_check --world ridgemere3 --residents --json`.
  - Everything at once: `python3 tools/mantle_batch.py gates FILE... --facts ...`.
  - Load: `python3 tools/mantle_load.py ridgemere3 FILE... --batch NAME --written-by claude-opus --bind [--dry-run]`. Exit 0 done, 1 refused, 2 written but not bound. It backs up and queues `voice.sync`.
  - Round trip: `python3 tools/mantle_export.py ridgemere3 --check`.
  - Rules:
    - name-free: slots only; capitals only for words in the `tools/mantle_rules.json` allow list;
    - never "dog" or "gods";
    - no he/she beside a person slot;
    - at least 60% of the wearer's lines carry a dialect marker;
    - habits in at most 25% of lines;
    - tier size: full at least 92 items, standard at least 55, spare 64.

  A villain's or hero's proper name in a mantle is refused unless it is allow-listed.
- **ctest:** `ctest --test-dir build-core -R 'talk_library|dialogue_trees|dialect_check|talk_lint|talk_topics|talk_scorecard_quick|talk_coverage|talk_p7_rules|talk_p8_rules|mantle_|storyline_behaviors|story_'`.
  - `talk_golden` replays the library from commit a46cbdac, so new content doesn't touch it.
  - `talk_scorecard_quick` scores 2,000 exchanges against a baseline. Content gated on facts the test town never has (`world.mem.*`, `world.figure_*`, `self.id=`) leaves it unchanged; ungated everyday content can move it.
- **Project text rules:** never dog/dogs (wolves, pups); the Bright One and the Dark One only, never "gods"; male or female only; "private messages", never "tells"; 160 characters per library alternative.

## 6. What's missing for campaign-aware talk (proposals)
1. **A story effect that writes talk memory** (`talk.remember` / `town.mem`). Proposed: `{scope: town|npc, town|npc, key, value ≤60, days 1-90}` calling `Game::talkRemember({Effect{scope:"town", owner:<region>, ...}})`.
   - Lines then gate on `world.mem.<key>=<value>`, or `self.mem.<key>` per resident.
   - Cost: one game order in `Core/RatwGameStoryWorld.cpp`, an entry in the trigger catalogue, a check in `story_program.validate_effect`, and an optional undo when the story ends (`noteStoryHeld`).
   - The talk side needs no change.
2. **`belief.plant`**, a rumour planted in residents. Proposed: `{subject: npc|wolf, text, kind?, town|npcs[], count, confidence}` calling `World::believe`.
   - It needs a `claims.json` kind whose wording fills, e.g. `"story.rumour": {"they": ["#x.name# #claim.text#"], "text": {"starts": [...]}}`.
   - Rumours about places only work with a place-worded kind, as `bandits.camp` is.
3. **A window fact for quest state.** The game already keeps each wolf's quest views (`storylines_ ... questViews[wolf].quests[{id, status, giver}]`).
   - Proposed facts: `player.quest.<id>=offered|active|ready|done` and/or `here.story_quest=active|ready` with this giver, set in `dialogueQuery`.
   - With it, grafts and tree `say[...]` lines could react to "you're still on my errand".
   - Also missing today: a giver's line while a quest is under way, and story choices beyond the hub's leftover room (more than 9 are silently dropped; consider overflow into `about`, as grafts do).
4. **Mantle restore:** `mantle.assign` should remember the previous binding and put it back when the story releases it.
5. **Figures from stories:** a `figure.set` / `figure.clear` effect, or authoring `live.figures` rows through the DM. Only the six roles are valid facts; any other role needs a `facts.json` entry.
6. **Checks for story text:** a check that runs `offer.say`, `offer.thanks`, `npc.offer.reply` and quest words through the talk expander (unknown symbol, `[`, lengths) and the dialect lexicon. Neither the story validator nor `dialect_check` covers story programs today.
7. Optional: a "campaign" or "story" topic in `topics.json` needs C++ (the director's source list is fixed). Without it, gate scenes on everyday or news topics (`day`, `smalltalk`, `lore`, `gossip`, `event`) with `world.mem.*`.

## Key files
- `/home/martinb/Documents/GitHub/MUDProject/Core/RatwGameWindowTalk.cpp` (windowNode 345, storyNode 463, windowLine 745, inVoice 847)
- `/home/martinb/Documents/GitHub/MUDProject/Core/RatwGameStorylines.cpp` (worldStoryLine 861, storyChoicesFor 1157)
- `/home/martinb/Documents/GitHub/MUDProject/Core/RatwGameDialogue.cpp` (1233 npc.line; 1526 story picks)
- `/home/martinb/Documents/GitHub/MUDProject/Core/RatwGameTalk.cpp` (talkRemember 862, talkRecall 906, figures 734/1479)
- `/home/martinb/Documents/GitHub/MUDProject/Core/RatwTalk.cpp` (load 2230, remembered 2738)
- `/home/martinb/Documents/GitHub/MUDProject/Core/RatwClaims.h`, `/home/martinb/Documents/GitHub/MUDProject/Core/RatwTopics.h`, `/home/martinb/Documents/GitHub/MUDProject/Core/RatwRoads.cpp:637` (rumoursFromEvent)
- `/home/martinb/Documents/GitHub/MUDProject/Core/RatwGameFame.cpp` (deeds into beliefs), `/home/martinb/Documents/GitHub/MUDProject/Core/RatwGameHandoff.cpp:280` (assignMantle)
- `/home/martinb/Documents/GitHub/MUDProject/tools/story_quests.py` (check_quest 138, offers 883)
- `/home/martinb/Documents/GitHub/MUDProject/tools/story_v2_samples.py:172` (QUEST_SMALL, QUEST_CAMPAIGN with npc.offer)
- `/home/martinb/Documents/GitHub/MUDProject/Data/Talk/dialogue/resident.tree` (hub, news, gossip nodes)
- `/home/martinb/Documents/GitHub/MUDProject/Data/Talk/facts.json`, `/home/martinb/Documents/GitHub/MUDProject/Data/Talk/claims.json`, `/home/martinb/Documents/GitHub/MUDProject/Data/Talk/topics.json`, `/home/martinb/Documents/GitHub/MUDProject/Data/Talk/settings.json`, `/home/martinb/Documents/GitHub/MUDProject/Data/Talk/truth.json`, `/home/martinb/Documents/GitHub/MUDProject/Data/Fame/deeds.json`
- `/home/martinb/Documents/GitHub/MUDProject/Docs/Design/84-mantle-writers-guide.md`, `/home/martinb/Documents/GitHub/MUDProject/Docs/Mantle-writing-handbook.md` (§4.4 format, §6 gates, §7 load), `/home/martinb/Documents/GitHub/MUDProject/Docs/story-programs.md` (Quests section)
- `/home/martinb/Documents/GitHub/MUDProject/Docs/Design/85-story-triggers-and-campaign-control.md` (3.5: belief.plant and npc.brief "Later"; Phase 6 as built, line ~1903)
