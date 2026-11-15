# 83. Dialogue trees: a window with the NPC's face, and choices to pick

Draft, 2026-11-15, for Ridgemere 3 (`ridgemere3`, doc 82). Nothing here is built yet. It adds player-facing
conversation trees to doc 62's talk library. Doc 62's facts, rules and grammar stay, and so do the router, the NPC
Mind and free text. A tree is a new way of *asking* the library, through a window the player answers by choosing.

## The ask (the user, 2026-11-15)

> We need to make conversation trees for shop owners and mission givers. This may be a larger task, but we need to
> review all conversation trees and make sure the regional dialect matches characters, adjust the conversations
> appropriately, and also add interaction dialogue trees. When a player selects an NPC to trade or receive a mission
> it should open a dialogue window with that player that shows the NPC's face. Eventually these will be animated too.
> They should have appropriate dialogue trees as well, with the character selecting options. Trade dialogues should
> be short, of course, just like normal RPGs.

## What exists today (surveyed at ce77a750)

### Talk between residents (doc 62)

- **The engine** is `Core/RatwTalk.{h,cpp}` (2,146 lines). Doc 62 has the whole format. A query is a *concept* plus
  facts (`self.`, `other.`, `pair.`, `here.`, `world.`, `topic.`, `mem.`, `conv.`; the schema is
  `Data/Talk/facts.json`). The rules that hold are grouped in tiers by score, and the best tier is taken 70% of the
  time (`Data/Talk/settings.json`). The wording is Tracery grammar, layered from the person, to their dialect, to
  plain.
- **The library** is in `Data/Talk`:
  - 4,738 `rule`s in 30 files under `rules/`;
  - 3,736 `scene`s: 2,952 in `scenes/common`, 636 in `scenes/regions`, 138 in `scenes/barks`, 10 in `rules/`;
  - 23 library scenes (`library.talk`), the region groups (`groups.talk`), and about 16,100 scene turn lines in all.
  - **Grammars:** `plain.grammar` (93 entries) and six dialects of 67 to 72 entries each: Frosthollow, Coldwater,
    Oakhaven, Mirebridge, Larkfield and Southmeadow. Ridgemere is plain.
- **Trees, as far as they exist:** scenes branch on *marks* (`#worried` ... `a@worried :`). That branching is between
  two residents and is chosen by facts. **No player-choice tree exists anywhere.** Nothing in the game asks the player
  to pick a line.

### Talk with a player

- **Talk on the wheel.** The wheel's Social category has Talk (`Game::command`, action `talk`). It only sends
  `{"type": "talkTarget"}`, and the client adds the NPC to its talk targets (up to four). After that, what the player
  *types* goes to them.
- **What the player typed** goes to `Game::talk` (`Core/RatwGame.cpp`):
  1. `gameAnswer` (`Core/RatwGameVoice.cpp`) checks the router's patterns (`Data/Voice/router.json`: 11 intents and 6
     tones), which pick a `player.<intent>` concept. There are about 197 such rules in `rules/player.talk` and the
     `*7` files: greet 99, bye 36, thanks 26, trouble 11, day 10, repeat 6, trade 5, and one each for hours, job,
     where and wanted.
  2. If the router recognises the line, the answer can be put in the NPC's voice by the small model (`polish`).
  3. Otherwise `mind_.converse` sends it to the NPC Mind (`tools/npc_mind.py`, 936 lines; `Core/RatwMind.cpp`). The
     Mind writes a reply with the live model, given the voice card and the persona. When the Mind is off, slow or over
     budget, the NPC answers with an authored line (`generated=false`, logged as "written").
- **Doc 82** found that the generic answers in the playtest came from a hosted launcher that started without the Mind.

### Dialect

- **Where a resident's dialect comes from.** `World::talkOrigin` (`Core/RatwAmbient.cpp`) works out where they grew
  up (`from`) and how strong their accent is, by a hash of their id. About 85% grew up where they live (75% in
  Ridgemere). It only knows the seven old regions. A row in `live.npc_voice` overrides it.
- **On Ridgemere 3's database:**
  - All 1,083 residents have a `live.npc_voice` row. 1,073 were written by `seed` and 10 by `gpt-5.6-luna` (those 10
    have a persona, a grammar and rules). 621 have a Ridgemere quarter. By `home_region`: Ridgemere 547, Larkfield
    107, Southmeadow 99, Frosthollow 91, Mirebridge 82, Oakhaven 80, Coldwater 77. By accent: light 666, broad 213,
    none 204.
  - The six `live.dialects` rows match the files word for word. The database copy wins at load.
- **Every voice row is dated 2026-10-13, which is the old map.** Residents moved twice after that (Ridgemere 2.0, doc
  67, then Ridgemere 3), and their origins were never worked out again. This is what "the dialect doesn't match the
  character" looks like in the data:
  - **The city is right.** Of the 356 residents with a city home (`rm_`), 277 (78%) are from Ridgemere.
  - **The towns are wrong.** In the town cottages (`cn_`), Coldwater has 6 locals out of 45, Larkfield 7 of 31,
    Southmeadow 5 of 13, Frosthollow 4 of 9 and Oakhaven 2 of 8. Most of Coldwater sounds like Larkfield or
    Frosthollow.
  - **Fenshore (17) and Saltwell (9),** the Country's towns (doc 72), have no dialect. `talkOrigin`, `talk_report.py`
    and the region groups don't know them.
  - **Mirebridge was erased in doc 72** and has no buildings, so its 82 natives can only be people who moved away.
    Fine, but the 108 entries gated on `here.region` including Mirebridge never play (of 1,118 entries gated on a
    region).
  - 544 residents have no home or work area in `live.npcs`. The server places them at load, so a check must read the
    world as the server places it (`talk_check --residents --json`), not the table.
- **Literal dialect words outside the grammar.** A few lines write a dialect's word into the text instead of using a
  symbol, so whoever says the line speaks that dialect. One example is "Dry feet to you, then." in
  `scenes/common/moment7_lore.talk:187`, which has no `from=` gate. Others are gated properly (`b[from=mirebridge]`).
  About 300 lines carry a `from=` gate. A plain text search turns up false alarms ("a good harvest" is ordinary
  speech), so the check below uses each dialect's distinctive phrases and keeps a reviewed allowlist.
- **Old-map place names in regional lore.** Regional scenes name old-map places ("the pass road", the Frozen Lantern,
  Lord Aldric Grayrock). Most of those places carried over into Ridgemere 3's interiors (two "Frozen Lantern" rooms
  exist), but nothing checks it.
- **No check that a character's lines match their region.** `talk_check --validate` checks the syntax, the facts and
  the symbols. `talk_report.py` counts coverage. Neither looks at who says a dialect's words.

### Shops (1d083c1a, r3-wheel)

- The wheel's **Trade** sends `{"type": "shop"}` for the chosen keeper. The player walks over if needed, and the shop
  window opens (`Client/src/ui/hud/shopWindow.ts`, with `game/shop.ts` holding the basket).
- **The window shows:**
  - the keeper's goods, their prices and the shelf stock;
  - the player's goods the shop deals in, and what it pays for them;
  - a basket priced by the server (`trade_quote` → `tradeQuote`);
  - CONFIRM, which makes each trade (`tradeResult` receipts, refusals in the world's words).
- The window has no face and the keeper says nothing.

### Missions and work (ce77a750 and earlier)

- **A merchant's work: contracts** (`Core/RatwRoads.{h,cpp}`, `struct Contract`). There are three kinds: `escort`
  (guarding a wagon from one market to a town), `courier` (a letter to a resident) and `bounty` (clearing a bandit
  camp).
  - The wheel offers up to three of them as `offer kNNN` ("Ask about: ... (12p)"). The server answers with
    `{"type": "missionOffer"}`: the giver's name and `appearance`, the title, the reward and the directions.
  - The client's new `mission_offer` modal (`Client/src/ui/hud/dialogs.ts`, `missionOffer()`) shows the giver's painted
    face (232×276), the words, and DECLINE or ACCEPT. ACCEPT sends `take kNNN` (`World::takeContract`).
  - Completion is automatic: an escort arriving with the wagon, a courier standing within 3 tiles of the recipient, a
    bounty clearing the camp.
- **Faction missions** (`Core/RatwFactions.h`, `struct Mission`) come from a faction's official, through "Ask for
  missions" (Chapter members only). There are three kinds:
  - `deliver` (goods to the official);
  - `message` (to someone);
  - `guard` (a continuous watch within 12 tiles of the official).

  The board lists them (modal `missions`), and ASK ABOUT IT opens the same `mission_offer` modal. Goods and messages
  are handed in through the wheel ("Deliver the message", "Deliver 3 bread"). A watch completes by itself.
- **The other work on the wheel:**
  - `ask for work` (lists contracts in the system log);
  - `apprentice`;
  - `lend a paw`;
  - doc 57's troubles (`trouble:help|peace|speak|sponsor`);
  - storyline objectives of kind `talk` (doc 58, fired from `Game::talk`).
- ce77a750 also put every taken job on the minimap (`self.missions`).

### Faces (doc 81)

- `drawPortrait` (`Client/src/ui/portrait.ts`) paints any wolf from the `appearance` the server sends, with
  `view: 'face'`. The painting is `Data/Wolves/sources/face.png`, recoloured per pixel through the wolf's fur ramp and
  zone map, and every look is cached.
- Every resident has an appearance. One without `fur` gets the look derived from its older fields (`furOf`). An
  approved uploaded portrait takes precedence for players.
- The NPC's face can be shown today. The `missionOffer` message already carries the appearance.
- **There is no animation.** There is one face painting, with no expressions, no closed eyes and no open mouth.

## Decisions (mine, for the user to overturn)

1. **One window for every conversation with a resident**, the *dialogue window*. Trade, the work actions and Talk
   all open it. It replaces the `mission_offer` modal. The shop window stays as it is and is opened *from* the
   dialogue.
2. **The server runs the tree.** The client only shows the current node and sends back the number of the chosen
   choice. The server checks every condition when it shows a choice, checks again when the choice is picked, and
   performs the action. Every action calls the code that today's wheel command calls (`takeContract`, the shop
   request, `deliver`, `report`...), so the rules exist once. A refusal can be given its own node, so the NPC can
   voice it.
3. **Every NPC line in a tree is a talk concept.** Each node's line is the concept `dlg.<tree>.<node>`. The text
   written in the tree is that concept's default rule, and any more specific rule anywhere outranks it:
   `self.trade=baker`, `self.from=frosthollow`, a character table's `self.id=...`, `pair.disposition=fond`. Trees
   need no per-dialect copies, residents with their own lines say them in the window too, and the DM can tune one
   keeper's greeting through `live.npc_voice` as today. These concepts always take their best tier (`best_only`), so
   a tree reads the same twice.
4. **The NPC's lines use grammar symbols, and the player's choices are plain words.** `#greet#`, `#farewell#`,
   `#address#`, `#money#` and new trade and work symbols carry the dialect. A player has no `from` yet (doc 62, open
   question 6), so the choices are the same everywhere.
5. **Trade is two clicks to the goods.** Trade opens the window on the keeper's greeting. The first choice, "Let me
   see your wares.", is on key 1 and Enter, and opens the shop window. The trade tree has four choices at most:
   wares, work (only when there is some), "What's dear today?" (one line from the prices facts) and Goodbye.
   Closing the shop window ends the conversation with the keeper's farewell, with no extra click.
6. **Talk opens the window too,** on the resident's hub: the role's topics, "Say something else..." (free text) and
   Goodbye. Typing to a talk target without the window still works as it does now.
7. **Free talk is the only place the live model speaks.** A node marked `free` shows a text field. What is typed there
   goes through `Game::talk` unchanged (router, then `player.*` rules, then the Mind), and the reply appears in the
   window. The Mind is told which node the player is on and what was just offered. It never takes or gives anything:
   only a tree's choices perform actions. Tree lines are never polished by the model.
8. **What's said in the window is said in the world.** The NPC's line and the chosen choice's words are published as
   ordinary speech (`publish`, `Voice::Speak`), so bystanders hear them, the story log keeps them with faces (doc 80's
   speaker id), the NPC's memory records the choice as heard, and storyline `talk` objectives count. The window is a
   way of choosing what to say, not a private channel.
9. **The world doesn't pause.** The window is a panel at the foot of the screen, not a full-screen modal. WASD still
   moves, and walking more than 4 tiles from the NPC closes the conversation. It also closes when the NPC falls
   asleep, goes out of sight, starts a fight or is struck, or when the player is downed. The digits go to the window
   while it is open.
10. **The face slot is built for animation now.** The portrait is its own component with a `setState({speaking,
    mood})` the window already calls for each line. Phase 5 only has to draw something different. The server sends a
    `mood` with each line.

## The dialogue window

```
┌──────────────────────────────────────────────────────────────────────────┐
│ ┌────────────┐  HESTA MARL · BAKER · THE WHARF BAKEHOUSE                 │
│ │            │                                                           │
│ │  (painted  │  "Still water, traveller. Bread's warm, pies at noon.     │
│ │   face,    │   Looking or buying?"                                     │
│ │  her fur)  │                                                           │
│ │            │   1  Let me see your wares.                               │
│ └────────────┘   2  Any work going?                                      │
│                  3  What's dear today?                                   │
│                  4  Goodbye.                                             │
└──────────────────────────────────────────────────────────────────────────┘
```

- **Face.** The face is a canvas of 232×276 at 1×, the size `mission_offer` uses now. It is drawn with `drawPortrait`
  (`view: 'face'`) from the appearance the dialogue message carries, in the resident's own fur. A wolf whose painting
  hasn't arrived shows its paper until it does.
- **Name and role.** The heading shows the name as the player knows it (`labelFor`), their work and the place.
- **Line.** The NPC's current line is written out quickly, and a click or Space shows it whole. The last two exchanges
  stay above it, dimmed, so a short back-and-forth reads as one conversation.
- **Choices.** Choices are numbered 1 to 9 and can be clicked. Up, Down and Enter also work. A choice that is
  possible but not allowed right now can be shown greyed, with its reason ("Not enough coin (12p)", "For a Chapter
  the Lodge trusts"). The tree says which choices are shown when locked. Otherwise a choice whose conditions fail is
  hidden.
- **Free text.** "Say something else..." opens the field in place (Tab jumps to it). Enter sends it, and Esc returns to
  the choices.
- **Esc.** Esc says Goodbye (the tree's `bye` node, one line) and closes the window.
- **Phone width.** The face goes above the line at 96 px, and the choices fill the width.
- **Code.** The window is `Client/src/ui/hud/dialogue.ts`, modal `dialogue`, with state in `game/dialogue.ts`. It
  follows the HUD's existing modal pattern (`dialogs.ts`).

**Its messages** (all JSON, as the rest of the protocol):

| Direction | Message | Holds |
|---|---|---|
| server → client | `{"type": "dialogue", ...}` | `session`, `npc` (id), `name`, `role`, `place`, `appearance`, `line`, `mood`, `choices: [{n, text, locked?, why?}]`, `free` (bool), `history` (the last two pairs) |
| client → server | `{"type": "dialogue_open", "npc", "entry"}` | `entry`: `talk`, `trade`, `work`, `job:<id>`, `missions`, `deliver:<id>`, or a role's own entry |
| client → server | `{"type": "dialogue_choose", "session", "n"}` | the choice's number as shown |
| client → server | `{"type": "dialogue_say", "session", "text"}` | free talk, only where the node allows it |
| either | `{"type": "dialogue_close", "session", "why"?}` | Esc, walking away, or the server's reason |

**The wheel's entries.** Trade, Ask for work, Ask about: ..., Ask for missions, Deliver ..., Talk, and the doc 57
trouble actions open the window at their entry node. Inspect, steal, attack, give, groom, introduce, the party's
orders and the Chapter's actions stay as they are on the wheel. They aren't conversations.

**On the server,** a player has at most one session, kept by `Game` (`dialogues_`, keyed by player). A session holds
the NPC, the tree and node, the bound values (a job, a mission) and the time it last changed. A session isn't saved, so
a restart closes it. The reach and awake checks run on each choice and once a second, which costs nothing.

## The tree format

Trees live in `Data/Talk/dialogue/*.tree`, beside the library, versioned and reviewed like it. The same loader reads
them (`talk_check --validate` checks them), with the same facts, the same grammar and the same 160-character limit.
One tree is written for each role, and its lines are varied by facts, not copied for each kind of keeper.

    tree shop
    for self.role=merchant                       # who may speak it (a fact test, as a rule's `when`)
    entry trade greet                            # the wheel's Trade starts here
    entry work work                              # and Ask for work here

    node greet
    say #greet#, #other.address#. Looking or buying? | #greet#. #shop_offer#
    say[pair.familiarity>=3] #greet#, #other.name#. The usual?
    say[here.shop=closed] We're shut, #other.address#. Back at #self.opens#.
    choice Let me see your wares. [here.shop=open] -> do shop
    choice Any work going? [here.work>0] -> work
    choice What's dear today? -> prices
    choice Goodbye. -> bye

    node prices
    say #shop_dear# | Nothing's moved much. #shop_offer#
    say[world.bread_price=up self.trade=baker] Flour's up, so bread's up. Not my doing.
    choice Let me see your wares. [here.shop=open] -> do shop
    choice Goodbye. -> bye

    node work
    say There's this going, if you want it.
    each job in here.work max 3
      choice Ask about: #job.title# (#job.reward#p) -> job
    choice Never mind. -> greet

    node job
    say #job.directions#
    choice I'll do it. -> do take job.id ; then taken ; refused refused
    choice What does it pay? -> pay
    choice Not now. -> greet

    node bye
    say #farewell#. | #shop_bye#
    end

- **`say`** lines are the node's NPC line. The loader turns them into rules for `dlg.<tree>.<node>`: the bare `say` has
  the node's own score, and `say[...]` adds its criteria. `|` gives alternatives, as in any rule. A `say` may also
  `ask <concept>` to take its words from a concept already in the library (`ask player.trouble`, `ask talk.prices`).
- **`choice <words> [criteria] -> <target>`.** Criteria hide the choice when they fail, or, with `locked "why"`, show
  it greyed with that reason. A target is a node, `end`, or `do <action> <args>`. An action may add `; then <node>`
  (where to go when it succeeds) and `; refused <node>` (where to go when it's refused, with the refusal as
  `#refusal#`). Without `then`, the conversation ends after the action. Without `refused`, the system message is shown
  and the node stays.
- **`each <name> in <list> max N`** makes one choice for each item of a list the server supplies:
  - `here.work`: contracts near the player;
  - `here.missions`: a faction official's board;
  - `self.deliveries`: what the player has taken that this NPC receives.

  The chosen item is bound for the rest of the session (`#job.title#`, `#job.reward#`, `#job.directions#`, `job.id`).
- **`remember`** works as in rules (`remember pair: asked_work 1d`). **`regard <-1..1>`** nudges the NPC's regard,
  clamped as the Mind's nudges are (source `talk`, doc 62).
- **New facts** (added to `facts.json`):
  - `here.shop` (open, closed, asleep, away);
  - `here.work` and `here.missions` (counts);
  - `other.cash` (number);
  - `other.standing` (with this NPC's faction);
  - `other.wanted`;
  - `other.has_job_here` (a job of theirs this NPC gave or receives);
  - the bound `job.*` and `mission.*` (kind, title, reward, place, directions, state).

  Residents' facts are the ones `talkSpeaker` already works out. The player's are new and come from the session.
- **The actions** are a closed list, written in C++. Each one calls today's handler:

  | Action | What it calls |
  |---|---|
  | `shop` | the shop request |
  | `take` | `takeContract` or the faction take |
  | `decline` | nothing changes: the offer stays open |
  | `deliver` | the faction delivery |
  | `report`, `pay_fine` | the watch |
  | `lodge` | doc 54's lodging |
  | `apprentice` | the apprenticeship |
  | `trouble <verb>` | doc 57 |
  | `standing`, `buy_report`, `tithe` | the faction official's |
  | `free` | opens the field |

  A tree can't invent an effect.
- **Directions in one place.** The words in ce77a750's offer text are moved into one Core function,
  `Game::jobDirections`, which the tree reads as `#job.directions#`. Today they are split between `missionOffer` (for
  contracts) and the client (for faction missions).

**The new grammar symbols:**

- **For trade:** `shop_offer` ("What'll it be?"), `shop_dear`, `shop_sold` (said in the shop window after a sale),
  `shop_bye` ("Come again.").
- **For work:** `work_offer`, `work_taken` ("Good. Don't dawdle."), `work_paid` ("Here, as promised.").

All of them are written in plain and in each of the six dialects. Every symbol must exist in plain (doc 62's rule).

## The trees, by role

| Tree | For | Entries | What it holds |
|---|---|---|---|
| `shop` | every merchant | trade, work | greeting, wares, work, what's dear, goodbye (four choices at most); trade-specific lines by `self.trade`; closed, asleep and short-of-coin lines |
| `work` | merchants with contracts | job:<id> | the offer, its directions, accept, the pay, decline; on accepting, the reminder line and the minimap mark (ce77a750) |
| `official` | a faction's officials | missions, deliver:<id> | standing, a report (paid), the missions board as choices, a mission's details, accept, decline; handing in goods or a message; the reward line; tithe; treaty and recognition for a Head |
| `receiver` | anyone a job is for | deliver:<id> | "I've a letter for you." (courier), goods handed over; the thanks |
| `inn` | innkeepers and alewives | trade, talk | a bed, the upstairs, a meal or a drink (the shop, for the room's prices), the news of the house, goodbye |
| `watch` | guards | talk | report a crime, pay a fine (only while wanted), ask the way (the router's `where`), goodbye |
| `master` | a master with no apprentice | talk | the trade, asking to learn it (`apprentice`), what it takes |
| `resident` | everyone else | talk | the hub: greet, "What do you do?", "Any news?" (the town's news facts), troubles (doc 57, while one holds), free talk, goodbye |
| `companion` | a resident in the party | talk | the party's orders as words: follow, wait, go home, dismiss |

A resident whose role has several of these, such as an official who also keeps a shop, gets the hub of the most
specific role, with the other roles' entries as choices. The loader refuses a tree whose `for` overlaps another's
without a priority.

**What a turn-in looks like.** The player has taken "Carry a letter to Oda Fenn". When they open Talk with Oda, the
`receiver` tree's first choice is "I've a letter for you, from Hesta." `do deliver` completes the contract as standing
within 3 tiles does today, and Oda answers with `#work_paid#` or the courier's own line. Escorts and bounties still
complete by themselves. The poster's hub gains "About the work I took on." It gives the directions again, and, if
the user wants it (open question 5), "I can't do it after all." That would be a new `abandon` that reopens the
contract.

## Dialect: making every NPC's talk match their region

### What "adjust the conversations appropriately" means

1. **Get each resident's origin right for Ridgemere 3.**
   - Work `from` and `accent` out again from where each resident lives *now*, with doc 62's rule (85% local, 75% in
     Ridgemere; the old broad, movers faded).
   - Keep the 10 written character tables, any origin the DM has set by hand (when `written_by` isn't `seed`), and
     every accent that still fits.
   - Write the result to `live.npc_voice` on ridgemere3 (a DEV world, `tools/talk_seed.py --reorigin`).
   - The Coldwater cottages then sound like Coldwater.
2. **Give the Country's towns a voice.** Map Fenshore to Mirebridge's fen speech (it is on the same fens) and Saltwell
   to Coldwater's (open question 3). The map is added to `talkOrigin`, `groups.talk` and `talk_report.py` in one place
   (`Data/Talk/settings.json`, `"dialect_of": {"fenshore": "mirebridge", ...}`).
3. **No dialect words outside the grammar,** except where the speaker is gated to that dialect. The lines the check
   flags are either rewritten with a symbol (`#greet#` instead of "Dry feet") or given the gate
   (`b[from=mirebridge]`). A line that *quotes* another region's speech ("I said Dry feet.") is allowlisted with a
   reason.
4. **Trees in every voice.** Every tree node is rendered in all seven layers, and the result is read and tuned by ear
   with the user, as doc 62's listen-through was.
5. **Lore that names the world.** A regional scene that names a place, a lord or a road must name one that exists in
   Ridgemere 3. The check lists those it can't find, and each is fixed or kept as legend.

### The check: `tools/dialect_check.py` (and a test)

1. **Origins.** For each town and the city, it reads residents as the server places them (`talk_check --residents
   --json`) and reports the share who speak its dialect. It fails under 70% for a town and 60% for the city, and on any
   town with no dialect or mapping.
2. **The lexicon.** From each dialect's grammar it takes the *distinctive* entries: words neither plain nor another
   dialect uses ("Dry feet", "Still water", "Shade on you", "Fair price to you", "Warm hearth", "Good harvest" as a
   greeting...). It scans every rule, scene turn, tree line and character table's text outside `#symbols#`. A hit
   fails unless the line's speaker is gated to that dialect (`self.from=`, `a.`/`b.from=`, `b[from=...]`) or the line
   is in `tools/dialect_allow.json` with a reason. Ordinary phrases are matched only as greetings, at the start of a
   line.
3. **Character tables.** It fails when a resident's own grammar uses another dialect's distinctive word.
4. **Trees.** Every node is rendered in each of the seven layers at light and broad, and none may exceed 160
   characters or leave a symbol unfilled. The loader already checks most of this, and the tool shows the renderings
   for the listen sheet.
5. **Lore.** Place names in `scenes/regions/*.talk` are checked against `world.interiors`, `world.places` and the
   town and road names. This is a warning, not a failure.

`Tests/talk_tests.cpp` gains the lexicon rule on the files, which needs no database. The origin rule runs in
`dialect_check.py --world ridgemere3`, in the phase's checks and before a release.

**How big the pass is.** The library has about 8,500 entries (4,738 rules and 3,759 scenes) and 16,100 scene turn
lines. A search for six distinctive greetings finds a few dozen hits, so I expect a few hundred flags across all of
the lexicon, most of them mechanical (a symbol, or a gate). Working out the origins again is one script run. The
lore warnings are probably fewer than 100, in the 636 regional scenes. With the checks, about half a day for one agent,
plus the user's listen.

## Phases

The process is the fast one: one agent per phase, in order, reusing one build, with targeted tests. The full suite,
an end-to-end run and one review-and-fix come at the end. The times are rough, for one agent.

1. **The engine, the window, and the shop tree** (about 6 h).
   - **Core:**
     - the `.tree` loader in `RatwTalk` (trees, nodes, `say`, `choice`, `each`, actions, and `dlg.*` concepts
       compiled into rules);
     - sessions in a new `Core/RatwGameDialogue.cpp`;
     - the new facts in `facts.json`;
     - the five messages;
     - the actions `shop`, `free` and `end`.
   - **Data:** the `shop` tree; the `shop_*` symbols in plain and the six dialects.
   - **Client:** `ui/hud/dialogue.ts` with the face slot component (`setLook`, `setState({speaking, mood})`), keys,
     free text and phone width; Trade opens the window; the shop window gets a strip with the keeper's face and line
     (`shop_sold` after a sale, the farewell on close).
   - **Tests:**
     - `dialogue_tests.cpp`: loading errors (an unknown fact, action or node, an unreachable node, overlapping `for`);
       conditions hide and lock; checks again at choose; closes on walking away, sleep or a fight; the dialect
       renders; a character table's `self.id` line wins in the window.
     - `market_tests`: Trade → dialogue → shop gives the same quotes.
     - Client: `dialogue.test.ts` (choices, keys, Esc says goodbye, locked reasons).
2. **Mission givers** (about 5 h).
   - **Trees:** `work`, `official` and `receiver`.
   - **Actions:** `take`, `decline`, `deliver`, `standing`, `buy_report` and `tithe`, with `Game::jobDirections` as
     the one source of the words.
   - **The wheel:** "Ask about", "Ask for missions" and "Deliver" open the window.
   - **The modals:** the `mission_offer` modal and the missions board's ASK ABOUT IT go. The board's rows become an
     `each`.
   - **Data:** the `work_*` symbols.
   - **Tests:**
     - offer → accept takes the contract (minimap mark present); decline leaves it open; a job offered by letter to
       someone else is refused in words;
     - a faction mission locked by standing shows its reason; goods handed in pay out and the mission is done;
     - a courier letter is handed over through the recipient's tree;
     - the existing escort, bounty and watch completions are unchanged.
3. **The other roles** (about 5 h).
   - **Trees:** `resident` (Talk opens the hub), `inn`, `watch`, `master` and `companion`; doc 57's troubles as
     choices.
   - **Free talk:** routed through `Game::talk` with the node given to the Mind (`context.scene`); a choice's words
     published as speech and recorded in memory; storyline `talk` objectives fire from the window.
   - **Tests:**
     - free talk reaches the router and the Mind's fixture;
     - a fine is only offered while wanted; lodging through the inn matches `lodging.ts`'s results; apprenticeship
       through the master;
     - bystanders hear both sides; a sneaker who isn't made out hears nothing of the window they can't see (doc 80's
       rules).
4. **The dialect check and the pass** (about 5 h, plus the user's listen).
   - **The check:** `tools/dialect_check.py` and the lexicon test.
   - **Origins:** `talk_seed.py --reorigin` on ridgemere3; the `dialect_of` map for Fenshore and Saltwell.
   - **Fixes:** every flagged line, the lore warnings sorted into fixed or kept, and every tree rendered in all seven
     voices into `artifacts/talk/dialogue-listen.md` for the user.
   - **Tests:** the lexicon rule; origins at least 70% per town on ridgemere3; `talk_check --validate` clean.
5. **Animation hooks** (later; about 2 h for the hooks, and the animation itself unplanned).
   - **Server:** `mood` on each line (from the node's tag or the speaker's `mood` fact) and `speaking` time.
   - **Client:** the face slot gets its own frame loop with nothing yet to animate but a small "speaking" cue: the
     line writing out, and a slight lift of the paper.
   - **What real animation needs,** decided with the user: new face paintings for blinks, an open mouth and a few
     expressions, each with its own zone map so the fur recolour still works (doc 81's `avatar_prep.py` handles any
     painting), or a warp of the one painting (breath, ears, a nod).

**At the end:** the full suite, an end-to-end run on a scratch server (`tools/scratch.sh ridgemere3`: buy bread through
the window, take and finish a courier job, hand in a faction delivery, free talk with the Mind fixture), and one
review-and-fix. Altogether about 23 hours of agent work, before the user's listen and any animation art.

## Open questions

1. **Does Talk open the window?** I've decided yes, on the hub with free text inside it, and typing to a talk target
   still works. The other way is that only Trade and work open it, and Talk stays as it is.
2. **Are the window's lines said aloud?** I've decided yes, as ordinary speech, so the world hears a purchase being
   asked for. Would you rather have trade lines quiet (only the two of them)?
3. **The Country's towns' speech.** Fenshore would speak as Mirebridge and Saltwell as Coldwater, or each could get a
   dialect of its own (about 70 entries each, with you).
4. **Working out origins again** changes how about 150 town residents sound. Is that wanted, or should the origins
   stay and the residents be moved instead? I recommend working them out again, since DEV isn't precious.
5. **Giving work back.** Should a player be able to abandon a taken contract or mission in conversation? It would be
   new, and today a job only expires.
6. **The player's own voice.** The choices are plain words for now. Later, a player's home town (doc 62, open question
   6) could colour them.
7. **Haggling.** Not in this design: the server's price ladder decides prices. A choice like "Can you do better?",
   answered by disposition, could come later.
8. **The animation:** what kind (blinks and mouths, expressions, the whole head), and whether to paint new frames or
   warp the one painting.
