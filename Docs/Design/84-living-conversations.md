# 84. Living conversations: a voice of its own for every wolf, talk that answers itself, and players only in the window

Draft, 2026-11-15, revised 2026-11-16 with the user's answers to all six questions of §21 (the window's free text,
mantles as world database rows, the richer size written by Claude agents, newborns inheriting honesty, births kept
realm-wide, ticking listening runs), for Ridgemere 3 (`ridgemere3`, database `ratw_ridgemere3`; doc 82). **Phase 0 (the
instrument), Phase 1 (truth fixes), Phase 2 (moves and tags), Phase 3 (the turn engine), Phase 4 (beats when spoken,
knowledge, a third voice), Phase 5 (topics, agendas, goals, the ledger), Phase 6 (opinions, temperament, moods,
accommodation) and Phase 7 (converting the library; coverage) are built** (2026-11-16, branches `r3-talk-p0` to
`r3-talk-p7`; §23, §24, §27-§32 say what and what they measured); nothing else is. It extends doc 62 (the talk library: facts, rules, grammar) and doc 83 (dialogue trees and the window) and
replaces neither: every one of today's 8,525 library entries and 10 trees keeps loading and speaking while it is
converted. It was written from five read-only surveys of the code, the data and the live world at `ridgemere2` 1034c1fc
(the engine, the trees, the lifecycle, what residents know, and prior art) and three competing designs; §4 says how they
were judged and what was taken from each. Decisions are marked **decided** (mine, for the user to overturn) or **the
user's (Qn)**, from the answers recorded in §21.

## 1. The ask (the user, 2026-11-15)

> Okay so next I am going to ask for a pretty significant amount of work on dialogue. [...] This next task, or rather
> set of tasks, is to really bring out the life of NPC's.
> 1. Make sure NPCs all have unique dialogue trees that follow the dialect of their region.
> 2. Make sure we have an extra set of dialogues for each region to cover NPCs that are yet to be born.
> 3. Make sure the dialogues can interchange their own names so that we can reuse dialogue trees if we run out of
>    premade dialogue trees
> 4. Make it so that dialogue trees of old dead wolves can be reused for newborns in the same dialect region.
> 5. Expand dialogue trees into more discussions, and make sure they make sense and sound natural with other dialogue
>    trees. The goal, here, is to make an intelligent system of dialogues that can respond to other chatting NPC's
>    naturally without plugging an AI into the backend.
> 6. Make this dialogue system robust. I am aiming for having some of the most believable NPC conversations in history,
>    all preprogrammed and very intelligent.
> 7. For NPC-to-player interactions we should have some unique and believable dialogues as well, but they should all
>    be done through that specific social window interaction we designed earlier. That is to say, I do not expect NPCs
>    to be responding to players in general conversation

## 2. The goals, one for one

| # | The user's words | What it means here (decided) | Where | Measured by (§15) |
|---|---|---|---|---|
| 1 | unique dialogue trees that follow the dialect of their region | Every placed resident wears a **mantle**: their own lines, opinions, stories and window branches, written without their name and natively in the dialect of where they grew up, and kept as rows in the world database, editable live in Atlas and the DM. The role trees keep their shape; the mantle fills their nodes in that wolf's voice and adds branches of its own. | §9, §10, §17 | U1, U2, D1-D3, W1 |
| 2 | an extra set of dialogues for each region for NPCs yet to be born | **88 spare mantles** across the eight dialect pools, each with a pup's and an adolescent's lines, plus a stage kit per dialect for pups whose mantle has none | §12 | pool health log; `mantle_tests` |
| 3 | dialogues can interchange their own names, so trees can be reused | No name, kin, trade or place is written into a mantle; all are **slots** filled from the wearer's facts. A line whose slot can't be filled doesn't fit. A lint refuses literal names. | §11 | `mantle_check`, T3 |
| 4 | dialogue trees of old dead wolves reused for newborns in the same dialect region | A dead wolf's mantle **rests** a game year, then passes to a newborn of the same dialect. Habits the neighbours would remember are drawn again; kin may hear an **echo**. | §12 | `mantle_tests`; a harness run shows the handoff |
| 5 | more discussions that respond to other chatting NPCs naturally, without AI | A **conversation engine**: every line declares the dialogue **move** it makes and the answer it **owes**; each beat is chosen when it is due, from fresh facts; agendas over a topic graph; opinions with reasons; memory of earlier talks; a third voice may join. All authored, no model. | §8 | C1-C9, K1-K3, H1 |
| 6 | robust; the most believable NPC conversations in history, all preprogrammed | **Measured, not asserted**: an offline conversation simulator runs the game's own director over the real world; a scorecard with hard gates in ctest; truth checks on every line; blind ratings against today's system and a shuffled control. | §15, §16 | the whole scorecard |
| 7 | NPC-to-player only through the social window; NPCs don't answer general conversation | Residents **never answer chat**. Everything a resident says *to* a player is said in the dialogue window, which gains their own voice, introductions, opinions, choices unlocked by what the player has heard, and authored understanding of typed words. | §13 | W2, W3 |

## 3. What exists today (surveyed at 1034c1fc)

### 3.1 Talk between residents

- **The library** (`Data/Talk`, 133 `.talk` files): 4,742 rules and 3,783 scenes (8,525 entries; `talk_check` loads 8,652
  with the trees and the 10 character tables), 21,503 line variants, 58,802 text alternatives. 2,778 `then` hand-ons,
  2,841 `remember` effects under 79 keys, 1,901 cooldown or `once` limits, 175 concepts (36 `talk.*` topics, about 121
  answer and close concepts).
- **Grammars:** `plain` (123 entry lines, Ridgemere's own speech) and seven dialects of 97-116 lines: Coldwater,
  Frosthollow, Larkfield, Mirebridge (also Fenshore's, by `dialect_of`), Oakhaven, Southmeadow, Saltwell. Each has 5 `!`
  habits and 1 `!!` habit. `live.dialects` holds the 7 rows and wins at load.
- **Only 799 of 58,802 alternatives (1.4%) contain a grammar symbol**, so the speaker's dialect can show in 1.4% of
  library text at all (scenes 0.5%, rules 3.9%).
- **The director** (`World::ambientPicks`, `Core/RatwAmbient.cpp:1000`; `Game::ambient`, `Core/RatwGameAmbient.cpp:23`):
  talk happens only where a player listens; at most 3 exchanges in the world at once (`AtOnce`, line 18), a look every 5
  s; the 12 nearest residents, standing still, within 3 tiles of each other, each at most every 300 s.
- **One topic per exchange, by fixed C++ scores** (`World::ambientTopic`, `RatwAmbient.cpp:171`; `sceneTopics`, :649):
  gossip 3 + confidence, deed 3.2, own news 2.8-3.0, crime 2.4 ... weather 1.0, smalltalk 0.8, the 11 everyday topics
  0.7. The highest wins. No memory of what a pair already discussed.
- **The whole exchange is rendered before the first word** (`Library::converse`, `Core/RatwTalk.cpp:1691`). A reply
  sees only the concept handed to it and short-lived pair-memory markers; it cannot see the opener's words or which
  alternative was chosen. Two speakers only (`Query`, `RatwTalk.h:59-65`). Replies always take the best tier (`!chained`,
  `RatwTalk.cpp:1814`) and are never marked heard (only the opener is, `RatwGameAmbient.cpp:247`).
- **Conversation state is faked:** 2,652 of the `remember` effects last an hour or less (`jk=look`, `thread=jest`,
  `went=...`), one home-made act vocabulary per file family. Only 23 keys last a day or more.
- **No model speaks between residents:** the ambient Mind path needs `--ambient-model-calls` > 0 (default 0,
  `RatwGame.h:170`), and no launcher passes it. NPC-to-NPC talk is already 100% preprogrammed.

**Measured** (the engine survey's harness, linking `libratw_core.a` and the real ridgemere3 residents; 3,000 exchanges,
one every 2 minutes, about 100 hours; pairs drawn at random in a cell, so if anything better than the director's):

| | Stationary in 4 busy Ridgemere places (L1) | Wandering (L2) |
|---|---|---|
| Topics | gossip and crime 99.8% | gossip and crime 87%; all everyday topics together 3 exchanges |
| Distinct openers | 137 of 3,000 | 382 of 3,000 |
| First repeated opener / first word-for-word identical conversation | exchange 38 / 59 (about 2 h) | 48 / 64 |
| Lines per exchange | mostly 3 | 3 lines 79%, 4 lines 17% |
| Replies | `n7_ga2_warm` ×81; "Good." spoken 139 times | 130 distinct reply rules across 974 beats |

Why: news always outranks everyday life and its pools are small (gossip 70 entries, event 49, harvest 11); replies
always take the best tier, so each kind of replier has one answer; cooldowns are per speaker; the `repeat` flag leaks
across concepts (`RatwTalk.cpp:1256`); 28.8% of alternatives are 3 words or fewer.

### 3.2 The window (doc 83)

- 10 trees, 71 nodes, 127 NPC `say` rules with 207 alternatives, 180 choices, 25 free-talk nodes. Talk opens `resident`
  for 737 of the 773 placed residents, `inn` for 15, `watch` for 14, `church` for 7.
- **Nothing is per NPC.** No `concept=dlg.*` rule exists anywhere. **115 of the 207 alternatives come out word for word
  the same in all 15 voices** (plain plus 7 dialects × light and broad). The trees test no trait, tone, quirk, wealth,
  family or `pair.*` fact: a hostile keeper says "Looking or buying?" with a cross face.
- The resident hub is at the 9-choice cap (`MostShown`, `Core/RatwTalkTree.cpp:15`).
- Free talk calls `Game::talk` (`Core/RatwGameDialogue.cpp:1230`): the router (10 intents, 9 words or fewer), then 197
  `player.*` rules, then the NPC Mind (live model). `heed` still records any promise the Mind reports.

### 3.3 Voices and the lifecycle

- `live.npcs` 1,083 rows: **773 placed, 310 unplaced (the Country)**. 254 distinct first names. Every backstory (1,083)
  contains the NPC's first name; once it is replaced, the backstories have only 57 distinct first sentences.
- `live.npc_voice` 1,083 rows (migration 0050): origin, accent, quarter, 7 trait axes, quirk (about 40 kinds, shared by
  up to 55 each). **1,073 are `seed`. 10 have a persona, a grammar and 4 rules each, by `gpt-5.6-luna`**: the whole of
  today's per-NPC talk. They are stale (Dagny Slaghand's "bread stall on the Heights Market"; she sells fish in the Rag
  Market now) and hard-code two other residents' names.
- Where placed residents grew up: Ridgemere 480, Coldwater 113, Larkfield 63, Fenshore 32, Southmeadow 32, Saltwell 24,
  Frosthollow 15, Oakhaven 13, Mirebridge 1. Accent: light 469, none 160, broad 144.
- The save stands at day 25.66: **0 deaths, 0 births** so far. The Gompertz expectation on today's ages is about **21
  deaths, so 21 births, a game year** (doc 68's 12-year Ridgemere 2.0 run had 188 of each). A game year is about 61 real
  days at speed 1.
- **A death** (`Society::died`, `Core/RatwCareers.cpp:873`) empties posts, settles the estate and owes one birth.
  Everything of the dead is kept: rows, voice, bonds, beliefs.
- **A birth** (`Society::tendBirths`, `RatwCareers.cpp:1299`) goes to a fertile couple anywhere in the realm, so a
  Frosthollow death can produce a Ridgemere pup. `ResidentRequest::inPlaceOf` (`Core/RatwSociety.h:218`) records the
  dead. The child's `live.npcs` row is cloned from the mother's (`Game::makeResidents`, `Core/RatwGame.cpp:1899-1910`):
  her personality text and tone, a name from 28 placeholders (`FirstNames`, `RatwCareers.cpp:35-38`), **no
  `live.npc_voice` row** (middle traits, no quirk), an origin hashed from the home town that flips for about 25% of
  city-born wolves at 18, and parents only in the checkpoint, never in `live.npc_relations`.
- `live.npc_voice` has **no `game_changes` trigger** (migration 0059 covers `npcs`, `profession_slots`,
  `npc_relations`, `faction_members`), so rows the game wrote would be invisible to Atlas's conflict detection.

### 3.4 Residents speaking to players outside the window

- **Chat is answered** (`Core/RatwGame.cpp:6413-6517`) by talk targets, any resident named in the line, a companion
  asked "what do you think", `npc_scout` on "road" or "danger", and, failing those, **the one resident within 6 tiles
  at hearing 0.5 or more** (lines 6470-6482). Each goes through `Game::talk`: router, `player.*` rules, the Mind.
- `react.customer` (74 rules) greets a player entering a shop; `welcomeAtInn` greets newcomers;
  `companionSays` (`RatwGameCompanions.cpp:95-103`) is hard-coded English with no dialect; a legacy `tavern` line
  (`RatwGame.cpp:3245-3262`) and 5 demo-NPC branches in `authoredReply` (`RatwMind.cpp:105-148`) remain.
- Introduce on the wheel sends a chat line; `noticeIntroduction` (6453-6460) is the only place a resident learns a
  player's name, and the window's speech never calls it.

### 3.5 Defects the surveys found (all fixed by Phase 1 unless marked)

1. **Raw ids spoken.** "You'll keep this to yourself? Camp_w_ridgemere_6_8 raids the road at w_ridgemere_6_8." Claims
   are built as text with cell ids (`Core/RatwRoads.cpp:939`, :1715), and `ambientTopic`'s `name()` falls back to the id
   (`RatwAmbient.cpp:178-181`). 1,453 such beliefs; 117 residents could say one today.
2. **The teller can be the victim named inside the claim**: only `belief.subject` is excluded (:211).
3. **Anyone sharing a home is "family"** (`World::pairBand`, :362-370, through `Society::household`): 971 home-sharing
   pairs, only 259 share a surname, 120 are in communal homes (the watch house alone is 105 pairs). Two unrelated wolves
   get "Have you thought about Father's tools?".
4. **The `repeat` flag is set per entry if any of its concepts repeats** (`RatwTalk.cpp:1256`): `n7_gr_smoke` ×74,
   `rg_rm_sound_fog` ×69 in a 100-hour listen.
5. **Replies always best-tier and never heard** (`RatwTalk.cpp:1814`, `RatwGameAmbient.cpp:247`): `w7_cheap_poor`
   ×203 in one run.
6. **Cooldowns share the 32-entry talk memory** (`Game::talkRemember`, `RatwGameTalk.cpp:455`), crowded by the short
   markers, so busy speakers lose cooldowns early.
7. **Gossip replies keyed on the claim's text** instead of its kind (`n7_ga2_warm`, `rules/town7_people.talk:183-185`):
   culprits are treated as victims.
8. **Lines untrue to the moment**: fog bells with no weather gate (`rg_rm_sound_fog`,
   `scenes/regions/ridgemere_lore.talk:93`); "Hear it coming down out there?" said indoors in any weather
   (`everyday_weather.talk:252`); **Coldwater town's homes hold 1 boatman and no fishers**, yet about 44 lines of
   `scenes/regions/coldwater.talk` speak of fish, nets, catch or boats, gated only on the region (`rg_ls_first_fish`:
   "Threw the first fish back this morning", said by a soap hand).
9. **`topic.news` garbled**: "I has heard that ..." (`mind::firstPerson`), and the day sentence leaks in when a topic has
   no facts (`Game::exchangeContext`, `RatwGameAmbient.cpp:180`).
10. **Births and Country arrivals never become news** (`World::noteNews`, `RatwAmbient.cpp:104`), and `news_` and
    `townNews_` are lost on a restart.
11. **`pair.named` is always "no" between residents**, spouses included.
12. **`#self.priest#` is Disa Raincot for 598 of 773**; `#self.leader#` exists for 11.
13. **17 answer concepts can dead-end** (Phases 2 and 8; no unconditioned fallback): crime.answer, dreams.answer, event.answer,
    event.close, faith.answer, friend.parting, friend.tease, harvest.answer, reply.close, season.answer, ships.answer,
    street.answer, times.answer, times.close, travel.from, travel.home, work.reply.
14. **Personal symbols break the trees' templates**: "Morning to you; the loaves are honest., neighbour. Looking or
    buying?" (four of the luna tables); 169 `live.npcs.greeting` rows are ungrammatical ("Good day to you. I am plays
    about the town today." ×47).
15. **The window's free talk can make promises** through `heed`, though its brief forbids it.
16. **Loading per-NPC tables is quadratic** (Phase 9 fixes it): `Library::setCharacter` copies the whole entry vector
    and reindexes twice per call (`RatwTalk.cpp:1948-1956`). Measured this session: 800 tables × 25 rules take **22.4
    s** one at a time against **0.85 s** in one parse, and with them in the shared bucket every pick costs **0.49 ms**
    instead of about 0.2 ms.

Worth knowing: the save has **no rivals** (lowest affinity −22.4 against the −25 line), so the 80 `talk.quarrel`
entries never play, and `live.figures` is empty, so the six `world.figure_*` facts are always absent.

## 4. How this design was chosen

Three designs were written independently from the same surveys: **engine-first** (a conversation engine, then voices
composed from authored packs), **content-first** (a hand-written "mantle" for every wolf, native in its dialect), and
**robustness-first** (build the instrument first, then annotate speech, every change held to a scorecard).

| Score, 1-10 | (a) fit to the 7 goals | (b) believability of NPC-to-NPC talk | (c) feasibility here | (d) robustness, measurability | (e) authoring cost realism | Total |
|---|---|---|---|---|---|---|
| Engine-first | 8 | 9 | 6 | 7 | 6 | 36 |
| Content-first | 9 | 7 | 7 | 6 | 4 | 33 |
| **Robustness-first** | 8 | 8 | 8 | 10 | 7 | **41** |

- **Engine-first** had the richest model of conversation (agendas and goals, stances with reasons, temperament
  weights, structured claims spoken from each point of view, group talk, town moods, players' knowledge unlocking
  window choices), but too many moving parts at once, an in-game identity composer, voices shared by about 19 wolves
  each through packs (weak on goal 1), and 131 hours that did not cover its own scope.
- **Content-first** was the only one that made every wolf truly unique and dialect-true in its own words, and it
  measured the quadratic load (defect 16). But about 0.7 million words (more than twice today's whole library) looked
  like an unrealistic review load, a third voice was optional, and measurement came late. **The user chose that size
  anyway (Q3)**, so its writing volume is in this plan, reviewed as §14.4 describes: every line through the automated
  gates, a stratified sample by eye.
- **Robustness-first** wins: measurement first, behaviour-neutral refactors with golden tests before any change, a
  scorecard whose metrics are checked against a shuffled control so they can't grade their own annotations, hard gates
  in ctest, and concrete truth findings (the Coldwater fishers). Its weak spots were thinner opinions and agendas and a
  short rest before reuse.

**Grafted in:** from engine-first, agendas and goals, stances with reasons and drift, temperament multipliers,
structured claims with point-of-view realizations, `#place.words#`, bound third parties (`x.*`), unheard daily gossip
writing to the ledger, town moods, a player's knowledge unlocking choices, residents noticing what players say in chat
without answering it, and the function lint for window lines. From content-first, the name **mantle**, the native
dialect content, tiers by role, batch loading with per-owner buckets, re-drawn marks and kin echoes, invitations and
talk-only promises the world checks, grafts and the "you" node in the window, dialect name pools, pool fallbacks by
neighbouring dialect, logging the words, and (by the user's answers) its size (Q3) and engine-first's home for the
text, the world database (Q2).

**Contradictions, resolved:**

| Question | Engine-first | Content-first | Robustness-first | Decided |
|---|---|---|---|---|
| Name of the per-NPC unit | identity | mantle | identity / `.voice` | **mantle**: `Core/RatwIdentity.h` already means a player's central identity, and "voice", "character" and "persona" are taken |
| Where the text lives | a DB table (`live.voice_identities`) | files, DB override text | files; DB bindings and per-NPC overrides | **world database rows** (the user, Q2): `live.mantles` and `live.mantle_lines` with revisions and a history, the bindings in `live.npc_voice`, per-NPC overrides as today; validated on save, hot-reloaded, exported to text for review (§7.12, §17). This keeps doc 62's decision 3 (what belongs to people lives in the world database) instead of revising it |
| How it is written | composed from ~900 shared packs, ~150 by hand | ~700-1,200 words each by hand | ~15-30 lines each by hand | **by hand, by tier, at the richer size** (the user, Q3): about 92 lines for the ~220 public roles, 55 for the rest, 64 a spare; about 0.7 million words, by Claude agents in sessions, no paid model calls |
| Rest before reuse | 365 game days | 365 | 120 | **365 game days** (a setting), with re-drawn marks for wolves still remembered |
| Spares | 71 | 70 | 88 | **88**, at least 6 per pool |
| Speakers at once | up to 4 | 2 (3 optional) | up to 3 | **up to 3** in this build; the cap is a setting |
| Exchanges at once | 6 | 3 | 3 until measured | **3**, raised only when budget B2 shows room |
| Replies | tiers, then fallbacks | tiers | best fit, then tiers | **fit first** (must answer what is owed), **then tiers** within the fit |
| The move keyword | `move` | `act` | `does` | **`does`**, with `owes`/`invites` for obligations |
| Tags on alternatives | after the words | after | before | **before** the words: `{kind=look} Have you ...` |
| `react.customer` | keep, de-duplicated | an emote | an emote + `here.came_in` | **emote + `here.came_in`**, so the window greets |
| Free text in the window | classifier, Mind optional | router v2, Mind off | understanding layer, Mind off | **the Mind when connected, else authored understanding** (the user, Q1) |

## 5. Decisions (summary)

All **decided**, for the user to overturn, except where an answer of the user's is named (Qn); each is explained where
it is built.

1. **Measure first.** Phase 0 builds the offline conversation simulator, the scorer and a committed baseline scorecard
   before any engine or content change. Every later phase reports its scorecard diff and may not break a hard gate (§15).
   Phase 0 records its fixtures from one ticking 25-game-day run, and each slice ends with a ticking 100-hour listen
   (the user, Q6); every such run is reported with its seed, length and wall time.
2. **Truth before variety.** Phase 1 fixes defects 1-15 and adds a presupposition lexicon checked on every line (§7.8).
3. **Every line declares its move.** About 85 dialogue moves in `Data/Talk/moves.json`; a line `does` one and `owes`
   the answers it makes due. A reply is chosen because it answers what is owed, so content from different files and
   different wolves answers each other. The 121 answer concepts become aliases, and a move nobody can answer is a load
   error (§7.2, §8.2).
4. **Beats are chosen when they are due**, from fresh facts, not rendered up front (§8.2).
5. **Topics are data**, with each speaker's agenda, a topic graph for stepwise pivots, novelty per pair, saturation per
   place and an everyday floor. The data first reproduces today's scores exactly (§8.5).
6. **Talk carries knowledge.** A line that asserts a claim makes its listeners believe it; bystanders overhear;
   corroboration, denial and "I heard already" are real. Every claim stays true; nobody lies in this build (§8.6).
7. **Memory a conversation needs**: conversation tags replace the short markers; a cooldown store no cap can evict; a
   per-pair **talk ledger** of what was discussed, how it went and what's left open; talk-only facts about third
   parties; daily unheard gossip writes there too. All saved in the checkpoint (§8.7).
8. **Opinions with reasons, temperament and mood** shape what is said and how it is taken (§8.8).
9. **A third wolf may join**, with a stake: named, kin or friend of the subject, deals in the goods, or a witness (§8.9).
10. **Every placed resident wears a mantle**: name-free, written in the dialect of where they grew up, by hand, sized by
    tier at the richer size (about 0.7 million words: the user, Q3), by Claude agents in sessions with no paid model
    calls. Its lines speak in NPC-to-NPC talk and in the window, where it also adds branches (§9).
11. **Names, kin, trades and places are slots only** (§11).
12. **Mantles pass on**: 88 spares in eight dialect pools; a dead wolf's mantle rests a game year and passes to a
    newborn of the same dialect; no NPC is ever invented and births stay one for one with deaths. Births stay
    realm-wide and the pools absorb the imbalance (Q5, the recommendation kept) (§12).
13. **A newborn gets a voice row at birth**: its mantle, a name from its dialect's pool, its birth town as its origin
    for life, its parents in `live.npc_relations`, a personality from its mantle instead of its mother's, and its
    mantle's honesty and temperament (the user, Q4) (§12).
14. **Residents never answer players' chat.** Everything a resident says *to* a player is in the window; unaddressed
    barks stay. Residents still notice what players say and may talk *about* it to each other (§13).
15. **The window speaks with the mantle's voice**: choices are moves; Ask about...; introductions; choices unlocked by
    what the player has heard; authored understanding of typed words (§13).
16. **Performance is budgeted and benched** (§16): batch loading and per-owner buckets before 1,000 mantles load.
17. **The ambient Mind path goes** once the final listen-through passes its hard gates (§13.6).
18. **The lean cadence**: one agent per phase in sequence, one build, targeted tests; the full suite, an end-to-end run
    and one review-and-fix at each slice's end (§18).
19. **Mantles live in the world database** (the user, Q2): `live.mantles` (the card) and `live.mantle_lines` (one row
    an item), with revisions and an append-only `live.mantle_history`, editable live in Atlas and the DM. Every save is
    checked by the game's own loader and `mantle_check` before it is written, and reaches a running server by a
    `mantle.sync` hot reload, never a new build. Agents' reviewed drafts reach DEV through `tools/mantle_load.py`; a
    text export, `Data/Talk/mantles-export/`, committed at every batch and slice end, keeps review and diffs possible;
    tests read a committed fixture (§7.12, §17).

## 6. Words used here

- **Mantle**: one wolf's way of talking, written without their name. Worn by one living wolf at a time; it rests when
  they die and later passes to a newborn of the same dialect. The **wearer** is the wolf wearing it.
- **Move**: what a line does (greet, tell news, decline, preclose...). **Owes**: the answer a move makes due (a question
  owes an answer; a farewell owes a farewell).
- **Beat**: one line in a conversation. **Frame**: the kind of meeting (strangers, kin, at work...).
- **Ledger**: the talk ledger, what pairs talked about and how it went.
- **Pool**: the mantles of one dialect not worn by anyone (spares, and resting mantles of the dead).
- **Kit**: a dialect's shared lines for pups and adolescents.
- **Item**: one piece of a mantle: a line, a full rule, a symbol, an opinion, a story, a window line or a graft. It is
  one `live.mantle_lines` row.
- **The export**: every mantle written out of the database as `.mantle` text under `Data/Talk/mantles-export/`, for
  review, diffs and tests. No server ever reads it as its mantles.

## 7. The data model

The shared library's new data are files under `Data/Talk/`, checked when the library loads (an unknown move, tone,
topic, frame, issue or claim kind is refused, as an unknown fact is today) and by `talk_check --validate`. **Mantles
are rows in the world database** (the user, Q2): written, exported and checked in the `.mantle` syntax (§7.9) by the
same loader, and stored one row per item (§7.12, §17). The database also keeps who wears which mantle and the per-NPC
overrides.

### 7.1 Additions to the `.talk` format (decided)

Backward compatible: every existing file loads unchanged, and `then other: <concept>` keeps working.

```
# A refusal is delayed, softened and explained (preference organisation, §8.2).
rule mv_decline_carry_nervy
does decline
when conv.request=carry self.job=carter self.temper=nervy world.bandits=near
tone serious
say I would. Truly I would. Only the cart's spoken for, and if I'm stopped I'd sooner lose the works' crates than your goods. Next run, if it's quiet. | #annoyed#. Not on that road, not this week. Ask me when it's quiet.
owes other: accept_decline

# Already knew: nobody says "Never!" to news they had yesterday.
rule mv_known_road
does react.news.known
when topic=bandits|caravan other.knows=yes
say I heard. #topic.heard_from.capitalize# had it yesterday. | So they're saying. #oath#. | I had it from #topic.heard_from#.

# The reply sees which tease was said. This replaces `remember pair: jk=look 30m`.
rule r7_tease_any
when concept=talk.friends
does tease
cooldown 1d
say {kind=look} Have you done something different with your fur? Don't. Whatever it is, don't. | {kind=ways} You walk like a wolf who's forgotten why they came out.
owes other: tease.return

# A line about a named third party is bound and remembered, so later talk can't contradict it.
rule n7_ro_neighbour_fiddle
when concept=talk.day
binds x=self.neighbour
when x.said.took_up!=hammer
does tell.news
say #x.name# has taken up the fiddle. Badly. Every night.
remember x: took_up=fiddle 30d
owes other: react.news
```

| Keyword | Meaning |
|---|---|
| `does <move>[.variant]` | The move this line performs (§7.2). A rule may have several (`does reassure request`). It is indexed by move as well as by concept. |
| `owes other\|self\|any: <move>[\|<move>]` | The answers this line makes due; `any` lets a bystander answer. `then other: <concept>` stays as sugar for a concept hand-on. |
| `invites other: <move>` | A soft obligation: answered only if a fitting line clears the replier's temperament roll; otherwise the speaker carries on. |
| `tone light\|even\|serious\|grave\|tender\|cross` | Defaults from the move. Checked by the tone rules (§8.11). |
| `asserts [<claim>]` | The line passes a claim to its listeners (§8.6). Default for `tell.news`, `tell.gossip` and `answer.detail` lines on a claim topic. |
| `denies [<claim>]` | The speaker denies the topic's claim. |
| `gist` | The point of this agenda item has been made (§8.10). |
| `binds x=<slot>` | Binds a third party (`self.neighbour`, `self.spouse`, `self.child`, `topic.subject`, `other.child`...) for `x.*` facts and `#x.name#`. |
| `{k=v}` before an alternative | Sets `conv.k=v` for the rest of the conversation. `{k=v keep 7d}` also keeps it in the ledger, read as `pair.was.k` by the pair and `self.was.k` by the speaker. |
| `remember x\|subject\|neighbour: k=v Nd` | A talk-only fact about a third party, kept in the ledger (§8.7), read as `x.said.k`. |
| `invite <place> <hour> <days>` | An invitation the world checks (§8.7): `pair.kept_invite=yes\|no` afterwards. |
| `promise self: <thing> <days>` | A talk-only promise. Never a `world_.promise`; when it lapses it becomes a nagging callback. |

### 7.2 `Data/Talk/moves.json`: dialogue moves and what they owe (decided)

About 85 moves in 12 families, after conversation analysis (adjacency pairs, preference organisation, pre-closings) and
Ryan et al.'s dialogue manager:

| Family | Moves |
|---|---|
| opening | greet, greet.return, kin.open (no greeting between kin), again.open ("Back again?"), hail, check_in, answer.check_in |
| inform | tell.news, tell.gossip, tell.complaint, tell.boast, tell.plan, tell.feeling, tell.worry, tell.self, tell.story, tell.memory |
| react | react.news (new, known, doubt, sorrow, glad, glee, again), react.complaint (agree, disagree), react.self, react.story, assess (good, bad), sympathise, reassure, congratulate, laugh |
| ask | ask.how, answer.how (well, poorly, busy, grieving, deflect), ask.detail, answer.detail, ask.opinion, give.opinion, ask.after, answer.after (better, worse, done, nothing), ask.plan, answer.plan, ask.whereabouts, answer.whereabouts |
| stance | agree, disagree, concede, insist, challenge, counter |
| social | tease, tease.return, tease.laugh, take_offence, apologise, accept_apology, decline_apology, thank, deflect_thanks, praise, deflect_praise, confide, keep_confidence, warn |
| requests | pre.request ("Are you busy?"), go_ahead, put_off, request, grant, decline, accept_decline, offer, accept, refuse, invite, accept_invite, ask.advice, give.advice |
| topic | raise, pivot (a step to a neighbouring topic), change_subject (marked: "Anyway..."), return ("As I was saying...") |
| repair | repair ("Eh? Who's wed?"), clarify |
| party | join (add, correct, defend, side, tease, object: "I'm standing right here"), welcome_join, aside |
| closing | preclose, accept_preclose, extend (once: "Oh, before you go..."), farewell, farewell.return, leave, break (interrupted) |
| ground | ack, backchannel, echo: they close a pair and owe nothing |

```json
{
  "about": "The moves a line can make (Docs/Design/84 §7.2): what it does, what it owes the one spoken to (or anyone), and how a dispreferred answer is shaped. A misspelt move, an `owes` naming a move that doesn't answer it, or an unknown tone is refused when the library loads.",
  "moves": {
    "greet":           {"family": "opening", "first": true, "expects": ["greet.return"], "optional": true},
    "tell.gossip":     {"family": "inform", "first": true, "expects": ["react.news"], "asserts": true, "fallback": ["ack"]},
    "react.news":      {"family": "react", "answers": ["tell.news", "tell.gossip"], "variants": ["new", "known", "doubt", "sorrow", "glad", "glee", "again"], "fallback": ["ack"]},
    "give.opinion":    {"family": "stance", "first": true, "expects": ["agree", "disagree", "ack"], "stance": true},
    "disagree":        {"family": "stance", "answers": ["give.opinion", "tell.complaint"], "dispreferred": true, "shape": ["hedge", "account"]},
    "request":         {"family": "requests", "first": true, "expects": ["grant", "decline"], "insertion": ["ask.detail"]},
    "decline":         {"family": "requests", "answers": ["request", "offer", "invite"], "dispreferred": true, "shape": ["hedge", "account", "remedy"], "expects": ["accept_decline"], "effects": {"other": {"affinity": -0.3}}},
    "sympathise":      {"family": "react", "answers": ["tell.worry", "tell.feeling", "answer.how.grieving"], "tone": "tender", "effects": {"other": {"affinity": 0.5, "trust": 0.3}}},
    "join.defend":     {"family": "party", "third_party": true, "tone": "cross", "expects": ["apologise", "insist", "concede"]},
    "preclose":        {"family": "closing", "first": true, "expects": ["accept_preclose", "extend"], "max_extend": 1},
    "farewell":        {"family": "closing", "first": true, "expects": ["farewell.return"]},
    "farewell.return": {"family": "closing", "answers": ["farewell"], "ends": true},
    "ack":             {"family": "ground", "answers": ["*"], "weak": true},
    "jest.answer":     {"alias": "tease.return"}
  },
  "tones": ["light", "even", "serious", "grave", "tender", "cross"],
  "tone_rules": {"after": {"grave": {"blocks": ["light"], "beats": 2, "bridge": ["change_subject", "assess", "preclose"]}, "tender": {"blocks": ["cross"], "beats": 1}}}
}
```

- **Dispreferred answers** (decline, refuse, disagree, doubt) are written with `#hedge#` and `#account#` or are at least
  6 words. "No." is never a decline. A cold or short-tempered wolf may drop the hedge (§8.8), and the bald answer costs
  the effect `moves.json` gives it.
- **`alias`** maps each of today's ~121 answer concepts onto a move, so all 2,778 `then` hand-ons keep working.
- **No dead ends.** The loader proves that every move any line can owe has an unconditioned plain line for a speaker
  with every fact empty, walking `fallback` down to `ack`. Today's 17 dead-ending concepts (defect 13) become a load
  error until filled.

### 7.3 `Data/Talk/topics.json`: the topic director in data (decided)

Phase 5 first moves today's C++ scores out of `World::ambientTopic` and `sceneTopics` **unchanged**, proved identical
by a golden test, and only then tunes them against the scorecard.

```json
{
  "about": "Topics (Docs/Design/84 §8.5): base scores as they were in C++, the topics each leads to (pivots), and how history and the place cool them.",
  "topics": {
    "gossip":  {"base": 3.0, "plus": "confidence", "player_bonus": 0.5, "news": true, "aspects": ["who", "what", "believe", "consequence"],
                "next": {"crime": "topic.claim_kind=theft|warrant", "family": "topic.subject_kin=yes"}},
    "bandits": {"base": 2.4, "news": true, "aspects": ["where", "fear", "watch"],
                "next": {"caravan": "", "watch": "", "prices": "topic.cause=road"}},
    "prices":  {"base": 1.65, "jitter": 0.4, "news": true, "next": {"caravan": "topic.cause=road|late", "times": "", "harvest": "", "work": ""}},
    "weather": {"base": 1.0, "fair_sky": 0.6, "jitter": 0.7, "everyday": true, "next": {"work": "", "land": "self.job=farmer|herder", "health": "", "season": ""}}
  },
  "pair_novelty": {"same_topic_within_days": 2, "factor": 0.4},
  "place_saturation": {"same_incident_per_day": 3, "factor": 0.3},
  "everyday_floor": {"window": 8, "news_share_over": 0.6, "boost": 1.5},
  "favourite": 0.5,
  "budget": {"chatty": [8, 12], "plain": [5, 8], "quiet": [3, 5], "friends_or_kin": 2}
}
```

About 45 topics and 120 weighted edges. `aspects` are a topic's tiers (Façade's three-tier dig): a pair that has
talked of it before starts one aspect deeper.

### 7.4 `Data/Talk/frames.json`: frames, goals and town moods (decided)

```json
{
  "frames": {
    "strangers":   {"when": "pair.band=strangers", "open": ["greet"], "budget": 0.6, "forbid": ["tease", "confide"], "goals": ["learn_name"]},
    "again_today": {"when": "pair.last_talk<0.3", "open": ["again.open"], "budget": 0.7},
    "kin":         {"when": "pair.kin!=no", "open": ["kin.open", "greet"], "accent": "+1"},
    "housemates":  {"when": "pair.band=housemates", "open": ["kin.open", "greet"]},
    "boss":        {"when": "pair.boss=a|b", "hedge": "the hand"},
    "at_work":     {"when": "self.working=yes self.shift=during", "budget": 0.6, "preclose": 1.5},
    "host":        {"when": "here.place=tavern self.job=innkeeper", "open": ["greet", "offer"]},
    "tavern_night":{"when": "here.place=tavern here.time=night", "budget": 1.4, "tones": {"light": 1.4}}
  },
  "goals": {
    "learn_name": {"when": "pair.named=no here.place!=work", "plan": ["smalltalk*2", "introduce", "ask.name"], "done": "pair.named=yes", "give_up": "other.talk=quiet other.warmth=cold"},
    "favour":     {"when": "self.wealth=poor|destitute pair.trust>=20", "plan": ["pre.request", "request"], "kept": "was.asked=favour 7d"},
    "vent":       {"when": "self.mood=low|sore pair.band=friends|spouses|family", "plan": ["tell.worry"]},
    "mend":       {"when": "pair.last_outcome=soured pair.last_talk<=7", "plan": ["greet", "apologise"]},
    "warn":       {"when": "topic.claim_kind=bandits.camp other.travelling=yes|soon", "plan": ["tell.news", "give.advice"]},
    "ask_after":  {"when": "pair.thread!=none", "plan": ["return"]}
  },
  "moods": {
    "grief":   {"when": "a death in the town within 3 days", "mutes": ["tease", "tell.boast"], "tones": {"light": 0.2}},
    "uneasy":  {"when": "bandits near, or a raid within a day", "boost": ["warn", "tell.worry"]},
    "festive": {"when": "a festival today", "tones": {"light": 1.5}},
    "edgy":    {"when": "an arrest within a day", "boost": ["tell.gossip"]}
  }
}
```

Moods generalise today's two-entry `salience` list (`settings.json`: failing business, destitute) into town-wide
moods, as Versu's "public mood" does. They are set from the town's news and read as `world.mood`.

### 7.5 `Data/Talk/temperaments.json`: how a trait leans (decided)

Weight multipliers per trait value on a candidate's move inside its tier, and on the chance of optional turns. Bounded
(0.3-2.0) so specificity still leads and a strict priority always wins.

```json
{
  "temper=short":    {"react.dismiss": 1.6, "take_offence": 1.8, "tease.laugh": 0.6, "preclose": 1.3, "hedge": 0.4},
  "temper=nervy":    {"tell.worry": 1.8, "react.news.doubt": 1.4, "repair": 1.4},
  "talk=quiet":      {"turn.more": 0.5, "pivot": 0.6, "ack": 1.6, "join": 0.4},
  "talk=chatty":     {"turn.more": 1.6, "pivot": 1.4, "join": 1.6, "extend": 2.0},
  "warmth=warm":     {"sympathise": 1.7, "offer": 1.4, "ask.after": 1.5},
  "warmth=cold":     {"sympathise": 0.4, "ask.after": 0.5, "hedge": 0.5},
  "humour=merry":    {"tease": 1.8, "laugh": 1.6},
  "humour=dry":      {"tease": 1.3, "assess": 1.4},
  "honesty=guarded": {"tell.gossip": 0.5, "answer.how.deflect": 1.6},
  "honesty=sly":     {"tell.gossip": 1.6, "react.news.doubt": 1.3},
  "honesty=frank":   {"disagree": 1.4, "hedge": 0.7},
  "piety=devout":    {"pivot:faith": 2.0},
  "drive=ambitious": {"pivot:work": 1.6, "pivot:money": 1.6}
}
```

### 7.6 `Data/Talk/opinions.json`: issues and stances (decided)

About 20 issues (the Watch, the Houses and each of Fell, Greatly, Grayrock and Timberland, the dole, newcomers, the
church, merchants and prices, the city, the road, festivals, drink, the Times, the Sump, strangers) and about 130 items
for likes and dislikes (`still_water`, `noon_heat`, `honey_cakes`, `fog_mornings`...).

```json
{
  "issues": {
    "watch": {"for": ["self.role=guard", "kin_role=guard", "self.class=upper"], "against": ["self.robbed=yes", "self.honesty=sly self.on_dole=yes"], "default": "mixed",
              "reasons": {"robbed": "they never caught who robbed me", "kin_role": "my #x.kin# walks with the Watch"},
              "drift": {"robbed_uncaught": -1, "fed_by_dole": 1}}
  }
}
```

A stance is `for`, `against` or `mixed` with a strength 0-3 and an optional reason, worked out by these rules from facts
and overridden by the wearer's mantle. Events drift it a step. Opinions of *people* need no storage: they are the bonds.

### 7.7 `Data/Talk/claims.json`: claims and who says them (decided)

`struct Belief` (`Core/RatwRoads.h:97`) gains `kind` (an id from this file), `object` (a second party: the victim) and
`place` (a cell). `World::believe` gets an overload; the free-text `claim` stays for `World::weigh` and the Watch.

```json
{
  "theft.attempt": {"they": "#x.name# tried to steal from #y.name#", "victim": "#x.name# tried to rob me", "to_victim": "#x.name# tried to rob you", "self": "I never laid a paw on #y.first#'s things", "grave": 2},
  "warrant":       {"they": "the Watch wants #x.name# for #topic.charge#", "self": "the Watch has my name up", "grave": 2},
  "bandits.camp":  {"they": "there's a camp #place.words#", "grave": 2},
  "newcomer":      {"they": "#x.name#'s new here, from #x.from_town#", "grave": 0}
}
```

- About 25 kinds, each with up to 5 points of view (they, victim, to the victim, self, kin).
- `#place.words#` comes from a new `World::placeWords(cell)`: "two days up the High Road", "past the lime quarry", from
  the nearest named road, place or landmark.
- **A claim with no realization is never spoken**, and `name()`'s raw-id fallback goes (defect 1). The teller's words
  are `topic.said`, which replaces `topic.news` and its garble (defect 9).

### 7.8 `Data/Talk/truth.json`: what a line presupposes (decided)

Checked twice: statically (the entry's criteria must imply what the words need) and on every simulated line (the
moment's facts must hold).

```json
{
  "rules": [
    {"match": "\\b(fog|mist|fog bell)\\b", "needs_any": ["here.weather=fog", "here.outdoors=fog", "conv.tense=past"], "example": "rg_rm_sound_fog"},
    {"match": "\\b(coming down|hear it)\\b.*\\b(out there|rain)\\b", "needs_any": ["here.weather=rain|storm", "here.outdoors=rain|storm"]},
    {"match": "\\b(Father's|Mother's) (tools|house|grave)\\b", "needs": ["pair.kin=siblings"]},
    {"match": "\\bmy (husband|wife)\\b", "needs": ["self.married=yes|self.widowed=yes"]},
    {"match": "\\b(this morning|at dawn)\\b", "needs": ["here.time=dawn|day"]},
    {"match": "\\b(the first fish|my (boat|nets|catch))\\b", "needs": ["self.job=fisher|sailor"], "example": "rg_ls_first_fish"}
  ],
  "ids": "\\b(camp|born|cty|rm|tn|w|cn|fs)_[a-z0-9_]{2,}\\b"
}
```

`here.weather` is `indoors` inside a building, so a new fact `here.outdoors` gives the weather outside it: a shopkeeper
can remark on the mist from behind the counter, truthfully.

### 7.9 The mantle text: the `.mantle` syntax, stored as rows (decided; the storage is the user's, Q2)

A mantle is written, validated, exported and shown in Atlas and the DM in this syntax, and kept in the world database
as rows (below, and §17). The example is written for a real resident: Myrtle Stillford (`tn_cw_myrtle_stillford`), 38,
shopkeeper at Coldwater Stores, from Coldwater, broad accent; calm, plain talk, warm, no humour, plain honesty,
content, comfortable; swims in the lake even in autumn. Her father is Orrin Stillford (`live.npc_relations`). It is
abridged: at the richer size (Q3) a full mantle holds about 92 lines (§9.3).

```
# cw017, the lake swimmer (doc 84). Written for tn_cw_myrtle_stillford; name-free; worn by one living wolf at a time.
mantle cw017
revision 4                         # the database's revision this text was exported at (§17.5); a new draft has none
state ready                        # draft, ready or retired (§9.2)
dialect coldwater
accent broad light                 # the accents a wearer may have; the first is as written
tier full
written_for tn_cw_myrtle_stillford # metadata, never spoken
title the lake swimmer             # for authors and the DM, never spoken
fits stage=adult|old               # hard: a wearer must match
prefers role=merchant warmth=warm  # soft: how well a wearer suits it
traits temper=calm talk=plain warmth=warm humour=none honesty=plain piety=ordinary drive=content
quirk swims_in_the_lake_even_in_autumn
likes still_water cold_mornings smoked_eel
dislikes hurry wet_floors
fears deep_winter_ice
hopes a_quiet_old_age
topics weather +.4 lore +.3 money -.3
styles tease=none refuse=soft comfort=practical gossip=reluctant
marks oath saying                  # the habits neighbours would remember (§12.4)

words
oath! = Glass and gods | Deep and dark
saying! = The lake keeps its own accounts
swim = every morning till the ice | in before the boats, every day

opinion road against 2 : The High Road? I'd not send my own soap up it.
opinion road against 2 [self.was.carry=declined] : The High Road? I'd not send my own soap up it, and I've asked.
opinion watch mixed 1 : The Watch walk that road like it bites them. But they do walk it.

grown
line tell.self [here.time=dawn|day] : {did=swim} Had my swim before I took the shutters down. #cold.capitalize#, and #very# glad of it.
line insist [conv.did=swim] : Every year since I was a minnow, and the lake hasn't had me yet. #oath#.
line reassure [pair.recall.road=worried] : You said the same last week, and you came home with every crate.
line @carry request [other.job=carter world.bandits=near] : {request=carry} Will you take a box of my candles up the road with you?
line accept_decline : Next run, then. I'll not have you holed for a box of candles.
line preclose [self.working=yes] : Well. The counter won't mind itself.
line farewell : #farewell#, #other.called#. Mind the step, it's slick.

story first_swim [self.from=here.region]
  : #mum!# threw me off the jetty when I was a minnow. Said Glasswater teaches what she can't.
  : I've gone in #swim# since.
  ask : Do you swim, #other.called#?

dlg shop.greet : #greet#. Salt, candles, string, soap. If the lake doesn't sell it, I do.
dlg shop.greet [topic.meeting=known] : #greet#, #topic.player#. In out of the wet. Looking or buying?
dlg resident.hub [here.came_in=yes here.outdoors=fog] : #greet#. Come in out of the mist; the floor's wet, mind. What's wanted?
dlg resident.bye : #farewell#, #other.called#. Mind the step; it's slick.

graft resident.about
choice You do swim in the lake? In this weather? [here.season=spring|autumn|winter] -> swim
node swim
say Every morning till the ice says no. #mum!# swam it till she was grey. It clears the head. You should try it.
mood pleased
free
choice Not for all the coin in #here.town#. -> about
choice Where do you go in? -> swim_where
node swim_where
say Off the end of the jetty, before the boats go out. Come at dawn and I'll show you, if you've the fur for it.
invite jetty dawn 3d
choice I might, at that. -> about
choice Goodbye. -> bye

young
line tell.boast : I swam out to the second post! Don't tell #mum!#.
```

**How it loads:**
- `words` becomes the mantle's grammar layer `m_<id>`; `opinion` gives `self.op.<issue>`, `self.op_why.<issue>` and a
  `give.opinion` line; `line <move> [criteria] : alternatives` compiles to a rule that `does <move>`; full `rule`
  blocks may be used inside a section for anything a line can't say (`owes`, `cooldown`, `remember`).
- `grown`, `young` and `old` sections add stage criteria (young: young|adolescent; grown: adult, and old too unless an
  `old` section exists).
- `story` becomes a chain of `tell.story` beats that owe `react.story` and continue through `conv.story`/`conv.part`;
  it is told once per listener (`pair.told:<story>`).
- `dlg <tree>.<node>` is a rule for the concept `dlg.<tree>.<node>`, which outranks the tree's default, as doc 83
  decision 3 intended. A node may declare `needs does=<move>`, and a mantle's line for it must do that move, so flavour
  never breaks a node's job.
- `graft <tree>.<node>` adds choices and nodes for this wearer only, namespaced `cw017.swim`; the loader checks
  reachability and that every exit leads back to a base node, `bye` or `end`.
- Every mantle rule gets `self.mantle=<id>` and +5 (`named_person`), as `setCharacter` does today with `self.id=`.

**How it is stored** (the user, Q2; the tables are in §17.1). The header and the card are columns of one
`live.mantles` row; every item below them is one `live.mantle_lines` row whose `body` is that item's text in this
syntax, exactly as the export writes it, so drafts, rows and exports share one parser and one set of checks:

| In the text | In the database |
|---|---|
| `mantle`, `dialect`, `accent`, `tier`, `state`, `written_for`, `title`, `fits`, `prefers` | `live.mantles` columns (`id`, `dialect`, `accents`, `tier`, `state`, ...) |
| `traits`, `quirk` | `live.mantles.traits` (jsonb, as `live.npc_voice.traits`), `.quirk` |
| `likes`, `dislikes`, `fears`, `hopes`, `topics`, `styles`, `marks` | `live.mantles.card` (jsonb) |
| the Mind's persona template (§11, §13.6) | `live.mantles.persona` |
| `revision` | `live.mantles.revision`: written by the export, never by hand |
| one `words` symbol | a row of kind `symbol`, key `words.oath` |
| one `opinion` | kind `opinion`, key `opinion.road.1` |
| one `line` or full `rule` in a stage section | kind `line` or `rule`, `section` any, grown, young, adolescent or old, `move` its `does`, key `grown.request.carry` |
| one `story`, `dlg` or `graft` block | kind `story`, `dlg` or `graft`, key `story.first_swim`, `dlg.shop.greet.1`, `graft.resident.about` |

**Keys** are stable names inside a mantle, so a diff, a history entry, a listener's "wrong" mark or a merge conflict
points at one item. A line's key is `<section>.<move>.<name>`, and other items are keyed by kind and name
(`words.oath`, `opinion.road.1`, `story.first_swim`, `dlg.shop.greet.1`, `graft.resident.about`). A line may name
itself (`line @carry request ...` is `grown.request.carry`); a draft may leave names out, and `mantle_load` gives a new
item the next free number (`grown.request.2`), which the export writes back (`line @2 request ...`). Order is the
row's `position`, so inserting a line moves no key.

### 7.10 Stage kits and name pools (decided)

```
# Data/Talk/kits/coldwater.young.kit: what a Coldwater pup says when its mantle has no young section.
kit coldwater.young
dialect coldwater
stage young
line tell.boast : I skimmed a stone six times off the jetty. Six! Ask anyone.
line ask.detail [other.stage=adult|old] : Is it true there's a pike in Glasswater longer than a boat?
line react.news.new : Never! Does #mum!# know?
words
pet!! = minnow-friend | my pearl
```

```
# Data/Talk/names/coldwater.names: Coldwater's names for the born. Drafts, to settle with the user.
names coldwater
female = Wenna | Ottilie | Merle | Linnet | Teal | Brenna | Ysolde | Marra
male = Orrin | Nab | Heron | Tarn | Kell | Osric | Garrow
named_for = .25    # a child born in place of a dead grandparent takes their first name this often
```

About 70 names per sex per dialect, replacing the 28 placeholders. No two living residents share a full name, and no
living parent or sibling shares the child's first name.

### 7.11 New facts (decided)

| Fact | Meaning | Source |
|---|---|---|
| `conv.*` (new namespace `Ns::Conv`, `RatwTalk.h:106`) | `conv.move`, `conv.prev`, `conv.tone`, `conv.topic`, `conv.covered`, `conv.beat`, `conv.phase`, `conv.size`, `conv.owed`, `conv.knew`, `conv.my_source`, `conv.subject_present`, `conv.subject_kin_present`, `conv.interrupt`, `conv.accent`, `conv.third.*`, and every `{k=v}` tag | the conversation |
| `x.*` (new namespace `Ns::X`) | a bound third party: name, first, sex, stage, job, workplace, injured, hungry, mood, likes; the speaker's bond to them (`x.affinity`, `x.disposition`); `x.said.<k>` from the ledger | `binds` |
| `self.first`, `self.surname`, `self.called` | split names; what this speaker calls the listener (first name for kin and friends, title and surname across a class gap in Ridgemere, `#address#` for strangers) | `live.npcs`, pair |
| `self.workplace`, `self.trade_words` | "Coldwater Stores", "shopkeeper": today talk sees only `job=merchant` | the post |
| `self.away`, `self.dialect`, `self.mantle` | grew up elsewhere; the grammar spoken; the mantle worn | voice row |
| `self.likes`, `dislikes`, `fears`, `hopes` | **list facts**: `self.likes=still_water` holds when the list contains it (a new `FactKind::Type::List`) | mantle card |
| `self.op.<issue>`, `self.op_why.<issue>`, `self.mood` | stance and reason; low, sore, bright or even from the last 3 days' events | §8.8 |
| `self.late`, `self.friend`, `self.rival`, `self.boss`, `self.hand` | the kin last mourned; the closest friend (affinity 40+) and worst rival (−25 or lower); over and under at work | records, bonds, posts |
| `pair.named` | residents know each other's names at familiarity 10+, or as kin, housemates or workmates | bonds |
| `pair.band=housemates` | a shared home without a kin tie (defect 3) | `Society` |
| `self.was.<k>`, `pair.last_talk`, `pair.talked.<topic>`, `pair.recall.<topic>`, `pair.recall_day`, `pair.thread`, `pair.was.<k>`, `pair.agree.<issue>`, `pair.last_outcome`, `pair.told:<story>`, `pair.kept_invite`, `pair.promise_talk` | the ledger, §8.7 | ledger |
| `pair.echo`, `pair.echo_of` | the listener knew the mantle's last wearer, who was the speaker's kin | §12.4 |
| `other.knows`, `other.told_me` | the listener already holds the claim; the claim kind the other last passed me | beliefs, ledger |
| `topic.said`, `topic.heard_from`, `topic.sure`, `topic.object`, `topic.subject_stage`, `subject_sex`, `subject_kin`, `subject_affinity`, `subject_trust` | the claim in the teller's words; its source in words ("a carter"); sure, heard or rumour from confidence; the item an opinion is about; the subject's facts and the speaker's bond to them | beliefs, claims.json |
| `here.came_in`, `here.outdoors`, `here.trades` | a player walked in within 120 s; the weather outside a building; the town's trades (for truth checks) | world |
| `world.mood` | grief, uneasy, festive, edgy, none | §7.4 |

### 7.12 Where everything lives (decided; the mantles' storage is the user's, Q2)

| What | Where | Why |
|---|---|---|
| The shared library, moves, topics, frames, temperaments, opinions, claims, truth, kits, names | `Data/Talk/` files | reviewed, versioned, gated in ctest |
| **Mantles**, spares included: card and items | **the world database**: `live.mantles` (one row a mantle: dialect, tier, state, card, persona, revision, author) and `live.mantle_lines` (one row an item, with its own revision), migration 0066 (§17.1) | the user's answer (Q2): editable live in Atlas (DEV) and the DM (live), checked on every save (§17.3), heard on the wearer's next line (§17.2); live data under doc 20's rules, so an import brings new mantles to live and a pull brings the DM's edits back (§17.9) |
| Every change to a mantle: who, when, from what source, before and after | `live.mantle_history` (append-only, written by triggers) | audit and revert; each world keeps its own, never synced or packaged |
| The export: every mantle as `.mantle` text, plus `index.tsv` | `Data/Talk/mantles-export/<dialect>/<id>.mantle`, written by `tools/mantle_export.py`, committed at every batch and slice end | review: an edit made in Atlas or the DM shows as a git diff; G1-G2 run over it in ctest; worlds without a database read it (`--mantles-from`). Never a server's source of mantles when a database is there (§17.5) |
| Agents' drafts | `.mantle` files in the agent's worktree, at their export path, loaded into DEV by `tools/mantle_load.py` (§17.4) | reviewed as a commit before and after loading |
| Test content | `Tests/fixtures/mantles/` (about 20 mantles in 3 dialects, committed) | ctest needs no database (§17.6) |
| Who wears which mantle, since when, how bound, when released | `live.npc_voice`, new columns referencing `live.mantles` (migration 0066, §17.1) | live data Atlas and the DM see; the game writes it at birth, death and arrival, and never writes a mantle's text |
| Per-NPC overrides by the DM or Atlas, and re-drawn marks | `live.npc_voice.grammar/rules/persona` (existing) | doc 62 decision 3 stays true for one person: an override is that wolf's alone and is never passed on; an edit to the mantle's rows passes on (§11) |
| Dialects | `Data/Talk/grammar/*` and `live.dialects` (existing) | unchanged, except that saving a dialect re-checks the ready mantles that speak it (§17.3) |
| The ledger, the cooldown store, news, residents' heard sets, players' claims, the mantle ledger for worlds without a database | the world checkpoint (`Core/RatwWire.cpp`) | game state; harness sims see it |
| Sim fixtures, transcripts, scorecards, listen sheets, the user's marks | `artifacts/talk/` | evidence, compared run to run |

## 8. The engine: NPC-to-NPC discussion

### 8.1 A conversation's state (decided)

New, in `Core/RatwTalk.h`:

```cpp
struct SaidLine { int who = 0; int to = -1; std::string text, entry, tone; int alt = 0;
                  std::vector<std::string> moves, owes, asserts; bool gist = false; };
struct Conversation
{
    std::vector<std::string> people;              // ids, 2 or 3; [0] opened
    enum class Phase : std::uint8_t { Opening, Body, Closing, Ended } phase = Phase::Opening;
    std::string frame, topic, tone = "even";
    int beat = 0, budget = 6, depth = 0;          // depth: the topic's aspect (7.3)
    struct Open { int by = 0, to = -1; std::vector<std::string> moves; bool soft = false; };   // to -1: anyone
    std::vector<Open> open;                       // answers owed, newest last
    std::vector<std::vector<AgendaItem>> agenda;  // one list per person (8.5)
    Facts conv;                                   // conv.* facts: tags, and the engine's own
    std::map<std::string, std::string> bound;     // x -> resident id
    std::set<std::string> said, asked, covered;   // entries, question keys and topics used in this talk
    std::vector<SaidLine> lines;
    std::uint64_t seed = 0;
};
```

`Library::begin(Conversation&, const Query&, const Freshness&, Trace*)` and `Library::next(Conversation&, const
std::function<Query(int speaker, int to)>&, const Freshness&, Trace*)` each return one `SaidLine` or nothing.
`Freshness` bundles the listeners' heard sets (by entry and by alternative hash), the place's recent list, cooldowns,
and the residents who have already heard an entry. `Library::converse` is rebuilt on `begin`/`next` over a fixed query,
so barks, reactions, `talk_check` and the tests keep working. `Query` stays two-speaker (`RatwTalk.h:59-65`); a third
person is carried as `conv.third.*`.

### 8.2 Choosing each beat (decided)

```
next(c, facts):
  if c.phase == Ended: nothing
  speaker := whoever the newest Open is owed by; for an Open to anyone, the one with the most stake
             (named in the last line, deals in the topic's good, kin of the subject), weighted by talk
             (chatty 1.5, plain 1, quiet 0.6); else whoever has the most urgent unmet agenda item; else a lull (8.3)
  moves   := what the newest Open expects; else, by phase:
             opening: the frame's opening moves
             body:    the speaker's agenda item (tell, ask after, opinion, story, request), or a pivot along the graph
             past the budget: preclose (the busier speaker first, or the one at work)
  q       := facts(speaker, addressee): stable person facts cached for the conversation (refreshed every 30 s or
             on an event); moment facts (here.*, injured, fighting, hour, weather), pair facts and conv.* fresh
  cands   := byMove_[m] for m in moves, plus the speaker's mantle bucket for those moves (plus byConcept_ for an
             opener's concept)
  drop:   doesn't hold; said already in this talk; muted by frame, mood or tone; in the place's recent list (by entry
          and by rendered-line hash); on cooldown; the speaker's mantle share above 35% of this talk's lines
  fit     := 1 if it answers what is owed; +2 if it answers the exact variant (react.news.known when other.knows=yes)
  keep the best fit only (coherence); inside it, tiers by score at 70/20/10 (variety); prefer what no listener has
  heard; then weight x temperament multiplier x stance fit
  render in that order until one can be spoken (today every candidate is rendered first, RatwTalk.cpp:1777)
  if nothing: walk the owed move's fallback list down to ack; if even that fails, a closing; count a gap
  apply:  tags to c.conv; owes to c.open; tone; topic and depth; asked; phase (preclose -> Closing; ends -> Ended)
```

Two changes from today carry most of the cure for "one rule answers everything":
- **Replies are no longer forced to the best tier** (`!chained`, `RatwTalk.cpp:1814`). Coherence lives in the fit
  score, variety in the tiers inside it.
- **Every beat is marked heard** per listening player, by entry and by alternative, not only the opener
  (`RatwGameAmbient.cpp:247`).

A scene taken as a beat queues its turns. Each queued turn is re-checked when it comes due and dropped if its criteria
fail (Valve's rule). A scene's last line may now `owe` a move, so a set piece can hand on into open talk (today a scene
can't use `then`, `RatwTalk.cpp:1757`). Marks and parallel alternatives inside scenes keep working.

### 8.3 Turn-taking (decided)

1. Whoever is owed an answer is answered, by the one it is owed by.
2. Else, if the last line `invites` a reaction, the addressee takes it with the temperament's chance, or passes.
3. Else the person with the most urgent unmet agenda item speaks. Ties go to the opener. A chatty speaker may go on
   (`owes self:`).
4. Else it is a lull: a `pivot` along the topic graph, or a `preclose`.

Nobody speaks twice running unless continuing their own turn. A turn may hold two lines when the first answers what
was owed and owes nothing, and the second is the speaker's own next move ("I heard. ... Will you take ...?"); they
count as two beats. Pacing stays `2.5 s + characters/25` between lines
(`RatwGameAmbient.cpp:52`), plus thinking time by temperament (quiet +0.8 s, chatty −0.3 s).

### 8.4 Frames, openings, closings, length (decided)

- **Frames** (§7.4) are worked out when a conversation starts and stacked: strangers greet properly, kin skip the
  greeting ("Mum."), a second meeting the same day opens "Back again?", a host offers a seat.
- **Length**: the budget is drawn from `topics.json` by both speakers' talk trait (the smaller of the two), +2 for
  friends and kin, and scaled by the frames. Range 2-14 lines; the 8-beat cap (`RatwTalk.cpp:1706`) rises to 14.
- **Closings** follow Schegloff and Sacks: preclose, then accept or one extend, then the farewell pair, in dialect
  (Coldwater "Fair winds", Saltwell "Go in peace"). A wolf leaving offended or called to work uses `leave`. Only a cold or
  short-tempered wolf cuts off without one, and that costs regard.
- **The shrugs** in `settings.json` `ends` ("Mm.", "Aye."...) stop ending chains: they become `ack` lines, and the other
  may carry on.

### 8.5 Agenda, topics, pivots, goals (decided)

`World::agendaFor(a, b, hearer)` replaces the single winner `ambientTopic` returns (`ambientTopic` stays as a wrapper
for `talk_check`). Each speaker gets a ranked list of items:

- **Tell**: a belief the listener lacks; one's own news; the town's news. Urgency from salience and confidence, changed
  by honesty (guarded holds back the unconfirmed, sly is eager, frank is plain) and by relation. **Never tell a victim
  their own misfortune as gossip**, and never gossip about one's own kin to others (defect 2): the victim speaks from
  the victim's point of view instead ("tried to rob me").
- **Ask after**: from the ledger (open worries, plans, invitations) and the world (the other is mourning, hurt, robbed,
  newly posted, has a newborn, a child apprenticed).
- **Callbacks**: open threads, favours offered, talk-only promises due.
- **Needs**: hungry, so complain or ask; poor while prices rise, so complain; owed money, so request.
- **Opinions** on the topic in play (§8.8); the mantle's favourite topics.
- **Small talk** (the weather, the day, the place), always available.

**Topic choice** reads `topics.json`: the base score, then ×0.4 if this pair discussed it within 2 days and nothing new
happened, ×0.3 for an incident already discussed 3 times in this place today, +0.5 for a speaker's favourite, and an
**everyday floor**: when more than 60% of a place's last 8 conversations were news, everyday topics get +1.5. News still
opens first when it is fresh; it can no longer take 99.8%.

**Pivots**: after a beat with nothing owed and budget left, the speaker with more interest may `pivot` along a `next`
edge whose condition holds (prices to the caravan by `topic.cause=road`, the road to bandits, bandits to the Watch),
said by a line made for that edge or through `#pivot#` ("Speaking of wagons..."). A `change_subject` may jump anywhere
but is marked (`#anyway#`). A conversation covers 1-3 topics.

**Goals** (§7.4) give a conversation a reason and an end: learn a name (small talk twice, introduce, ask), ask a favour,
vent a worry, mend a quarrel, warn of bandits, ask after an open thread.

Prices stay worked out at the listening player's list (`talkFor(hearer)`), fixed when the conversation starts, so one
conversation never mixes two lists.

### 8.6 Talk that carries knowledge (decided)

- **Asserting**: after a line that `asserts` a claim, the addressee and the other participants believe it through
  `World::believe` (`Core/RatwRoads.cpp:458`) at confidence × (0.5 + 0.4 × trust/100), source the speaker (today a flat
  ×0.7, only for gossip and deed topics, `RatwAmbient.cpp:1078`).
- **Overhearing**: residents at hearing 0.5 or more who aren't in the conversation believe it at ×0.4.
- **Corroboration**: a second, independent source raises confidence by (1 − c) × 0.4.
- **Denial**: a `denies` line from the subject or their kin multiplies the listener's confidence by (1 − 0.3 ×
  trust/100).
- **Already known**: a listener who holds the claim gives `other.knows=yes`, so "I heard. Terrible." fits instead of
  "Never!". Telling the same wolf twice is caught by the ledger's told set and draws `react.news.again` ("You told me
  that on Emberday.").
- **Truth**: every claim is true when first made, as today. Nobody lies or embellishes in this build, and nothing a
  line asserts reaches the Watch beyond today's hearsay cap (`World::weigh`, at most 0.5).

### 8.7 Memory, in four tiers (decided)

1. **The conversation** (`conv.*`): lives as long as the talk. The 2,652 short markers (`remember pair: k=v` for an hour
   or less) become `{k=v}` tags; `pair.mem.k` reads become `conv.k` (`tools/talk_conv.py`, Phase 7). They can no longer
   leak into the pair's next talk, and they leave the 32-entry memory.
2. **Talk memory** (`Game::talkMemory_`, as today) for the long-lived keys (`served`, `grumble`...).
3. **A cooldown store of its own**: `said:<id>` moves out of `talkMemory_`, whose cap (`RatwGameTalk.cpp:455`) evicts
   cooldowns early (defect 6).
4. **The talk ledger** (new, `Core/RatwTalkLedger.{h,cpp}`, owned by `World` so harness worlds have it, saved in the
   checkpoint):

| Record | Holds | Cap |
|---|---|---|
| pair note | day, topics, stories told, stances aired and whether they agreed, claims passed each way, kept tags (`keep Nd`), an open thread (topic, item, gist reached?), invitations and talk-only promises, the outcome (warmed, soured, reassured, conceded, quarrelled, interrupted) | 8 per pair, 30 days |
| told | (teller, listener, claim) | 64 per resident |
| person fact | what talk has asserted about a third party (`took_up=fiddle`), by whom, until when | 16 per subject |
| heard | distinctive entries a resident has heard | 64 per resident |
| all | | 30,000 records, least recently used first; about 2 MB in the checkpoint (today's is 1.3 MB) |

- **Unheard talk is recorded.** `World::gossip` (`RatwRoads.cpp:1802`) already passes beliefs once a day out of sight;
  it also writes an abstract pair note (talked of X, passed claim Y) with no words. So "as you told me yesterday" is true
  wherever a player arrives. About 2,300 notes a game day, none rendered. Off in fast economy sims (`--no-talk-ledger`).
- **Invitations are checked**: the hourly tick asks whether both were in that place's cell in that hour, and sets
  `pair.kept_invite`. Talk-only promises that lapse become callbacks ("You said you'd see the healer.").
- **Third parties stay consistent**: a line may say something about a named resident only through `x.*` facts, a
  recorded claim, or a person fact it records itself. `n7_ro_neighbour_name` ("taken up the fiddle") records it, so
  nobody later says the neighbour took up the hammer instead.
- **`news_` and `townNews_` are saved** in the checkpoint (defect 10).

### 8.8 Opinions, temperament, mood (decided)

- **Stances** (§7.6): `self.op.<issue>` and its reason, from facts and the mantle. `pair.agree.<issue>` is yes, no or
  unknown until aired. `disagree` fits only when the stances differ, so disagreement is real, repeatable and remembered
  ("We'll never agree on the Watch, you and I."). Friends, spouses and kin who differ on a salient issue may argue it as
  a topic; strangers who differ steer away.
- **Likes** are held on the mantle and stable from day to day (today `rules/pastimes_animals7.talk` picks a favourite
  pastime afresh each time).
- **Temperament** (§7.5) weights moves inside their tier and the chance of optional turns.
- **Mood**: `self.mood` (low, sore, bright, even) from the last 3 days' events (robbed, mourning, quarrel, fed,
  praised, a good sale), fading. **Town mood**: `world.mood` (§7.4).

### 8.9 A third voice (decided)

`Game::joiners(conv)` runs each beat. A candidate must be idle, within 3 tiles of both, at hearing 0.5 or more, not in
another conversation, at least acquainted with one of them, clear of the 300 s rule (60 s for joining), and must have a
**stake**: named in the last line, kin or friend of the subject (`join.defend`), a stance opposite to what was just said
(`join.side`, `join.correct`), dealing in the goods, a witness, or friends with both and chatty (`join.add`,
`join.tease`). They join only with a fitting `join.*` line. At most one joiner, three people in all (a setting). The
addressee owes; otherwise speakers self-select by stake and talk trait. A `leave` takes one wolf out and the rest go on.

### 8.10 Interruptions and resuming (decided)

| Event | What happens |
|---|---|
| Death, leaving the cell, more than 5 tiles apart | Ends with no line, as today; an open thread is kept if the gist wasn't reached |
| Rain starting outdoors, a fight in sight, a player opening the window with one of them | `conv.interrupt=rain\|fight\|called`; the next beat must be a `break` line ("Rain. Inside, quick." / "One moment, Wick.") |
| A player chatting nearby | No longer an interruption: residents don't answer chat (§13) |

An agenda item whose gist wasn't reached is kept as `pair.thread` for 2 game days. At the next meeting it is a
high-urgency `return`: "As I was saying, about the road..." (Façade's re-establishing lines).

### 8.11 Tone and town moods (decided)

Every line has a tone, defaulting from its move. Light is blocked for 2 beats after grave, and cross for 1 beat after
tender, unless a bridging move comes between ("Well. Enough of that."). Teasing is blocked when the listener mourns.
`world.mood` mutes and boosts moves town-wide (§7.4).

### 8.12 Freshness (decided)

1. Every beat is heard, not only the opener (defect 5).
2. `repeat` is decided at pick time by the concept asked, not by any of an entry's concepts (defect 4).
3. **The collision filter**: each place keeps a one-hour ring of hashes of rendered distinctive lines (6 words or more,
   ground moves excluded). A candidate whose words another speaker said there within the hour is skipped. This is the
   "arrow in the knee" metric, enforced.
4. Stories and teases are once per listener by default; each resident has a heard set of 64 distinctive entries, so
   five wolves don't tell one wolf the same joke in a week.
5. The place's recent list grows from 16 to 32 and holds rules as well as scenes.

### 8.13 Why lines from different files answer each other

Replies are indexed by move. Any `react.news` line in any file or mantle can answer any `tell.news` line from any other,
when its criteria hold. `talk_check --coverage` proves it per replier kind (§15.4), and the scorecard measures it in
practice: obligations answered (C1), replies at the best fit available (C2), and the share of coherent beats whose
reply came from a different file family than its opener (expected to rise from about 0 today).

### 8.14 Changes by file and function

| File / function | Change |
|---|---|
| `Core/RatwTalk.h` | `Ns::Conv`, `Ns::X`; `Entry` gains `does`, `owes`, `invites`, `tone`, `asserts`, `denies`, `gist`, `binds`, `owner` (the mantle); `Variant` gains per-alternative tags; `SaidLine`, `Conversation`; `FactKind::Type::List` |
| `Library::parse` (the keyword loop) | the new keywords and leading `{k=v}` tags |
| `Library::readSettings` / `loadFoundation` | read `moves.json`, `topics.json`, `frames.json`, `temperaments.json`, `opinions.json`, `claims.json`, `truth.json`; schema checks; the dead-end proof |
| `Library::reindex` (:1895) | builds `byMove_` and `byOwner_[mantle][concept or move]` |
| `Library::pickOne` (:1731) | split into gathering, tiering and lazy rendering; `!chained` best-only goes; `repeat` per concept (:1256) |
| `Library::begin`, `next`, `answer`, `coverage` | new (§8.1-8.2, §15.4) |
| `Library::expand` / `expandSymbol` (:1396) | accent from `conv.accent` when set (§10); habits seeded by mantle id (:1466-1468); a punctuation join (a symbol value ending in `.` or `;` before a `,` or `.` drops one: defect 14) |
| `Library::setCharacter` (:1932) | becomes a per-NPC override into `byOwner_`, with no copy of `entries_` (:1948); new batch `setMantles` (§9.4) |
| `Core/RatwTalkTree.cpp` | `graft`; `needs does=`; choices with `[does <move>]`; nodes marked `answers`; lists `here.opinions`, `here.kin`, `here.stories`, `player.knows`; actions `introduce`, `ask_name` |
| `Core/RatwGameAmbient.cpp` | `Game::ambient` split into `ambientStart(listeners)` and `ambientSpeak(dt)` (Phase 0, behaviour-neutral); `AmbientTalk` (`RatwGame.h:1840`) holds a `Conversation`; new `ambientBeat`, `joiners`, `beatQuery`; `talkPick` becomes `talkBeat`; `exchangeContext` (:145) fixed |
| `Core/RatwAmbient.cpp` | `agendaFor`; `ambientTopic` reads `topics.json` with novelty, saturation and the floor, and never returns an id; `pairBand` with `housemates`; `noteNews` handles births and newcomers; `ambientSpoken` split into start and end, applying each move's effects instead of the blanket +0.5 affinity |
| `Core/RatwRoads.{h,cpp}` | `Belief` gains `kind`, `object`, `place`; structured claims at :939 and :1715; `placeWords`; `gossip` writes ledger notes |
| `Core/RatwTalkLedger.{h,cpp}` | new; checkpointed in `RatwWire.cpp` |
| `Core/RatwGameTalk.cpp` | `talkSpeaker` (:24) gains the §7.11 facts and the layer order (§9.4); `talkPair` (:252) `pair.named` for residents, agreement, ledger and echo facts; `peopleOf` (:742) the priest of the resident's own district; `talkRemember` (:436) sends `conv` effects to the conversation and the rest to the ledger; `loadVoices` (:848) rewritten (§9.4) |
| `Core/RatwGameDialogue.cpp` | one shared `claimKind` (:172) with the ambient one, plus `bandits` and `newcomer`; free text to `dialogueUnderstand` (§13.5) |
| `Core/RatwGame.cpp` (the DM actions, beside `voice.sync` at :1210) | `mantle.sync <id>` (one mantle's rows read again, §17.2), `mantle.assign`, `mantle.release`, `mantle.retire` (§17.10) |

## 9. Mantles: every wolf's own dialogue (goal 1)

### 9.1 What "a unique dialogue tree" means here (decided)

Talk between residents isn't a tree but a web of moves, and the window's trees are role trees (shop, watch, inn...).
A wolf's own "tree" is therefore four things, all in their mantle:
1. **Their own lines for the moves they make most** (openers, answers, refusals, comfort, teasing, closings), which win
   by +5 wherever they hold, in talk with residents and in the window alike.
2. **Their card**: traits, quirk, likes, dislikes, fears, hopes, favourite topics, styles and opinions with reasons,
   which steer what they bring up and how they take what's said.
3. **Their stories**: set pieces only they tell, once per listener.
4. **Their branches in the window** (grafts) and their own words at the role tree's nodes.

The shared library is the floor beneath every mantle, so any two wolves can always talk. Uniqueness is enforced and
measured: no two mantles of a dialect share a 5-word run; no two wolves of a town share an oath or a saying; at most two
of a town share a quirk; and U2 (§15.3) checks that two residents with identical facts are still told apart by their
lines.

### 9.2 States (decided)

| Status | Meaning |
|---|---|
| spare | written for the unborn of its dialect, never worn |
| worn | held by one living wolf (a unique index enforces it, §17) |
| resting | its wearer died on `released_day`; it can't be worn again for `mantle_rest_days` (365 game days, a setting) |
| retired | taken out of use by the DM, or stale (`live.mantles.state = 'retired'`; refused while the mantle is worn) |

Status is worked out from the binding columns and the mantle's row: a `ready` mantle with no living wearer is spare
(never bound) or resting (bound before). There is no second store to drift. A `draft` mantle (one still being written
in Atlas or the DM) is invisible to the game: never loaded, never drawn for a birth. A mantle once worn is never
deleted, only retired (the bindings reference it).

### 9.3 Tiers and sizes (decided; the richer size is the user's, Q3)

| Tier | Who (placed) | Card | Words | NPC-to-NPC lines | Window | Stages | Lines, about |
|---|---|---|---|---|---|---|---|
| **full** | about 220 public roles: every merchant (127), guard (14), priest (7), innkeeper (15), master (60, overlapping), House family (15) | yes | 16-20 symbols | 8 openers (by frame: strangers, kin, at work, again today); 30 move lines, with variants by mood and by the listener's band; 5 stories of 3-4 beats; 6 barks; 8 opinions with reasons | hub, you, job, business, news, gossip, bye and the role tree's nodes, each with greetings by meeting and disposition; 2 grafts of 2 nodes | old section if 55 or older | 92 |
| **standard** | about 553: everyone else placed, 97 pups and adolescents included | yes | 12-14 symbols | 5 openers, 18 move lines, 3 stories of 3 beats, 4 barks, 5 opinions | hub, you, news, bye and the role tree's nodes; 1 graft of 2 nodes | young section for pups | 55 |
| **spare** | 88 (§12.1), sex-neutral | yes | 12-14 symbols | as standard | as standard | young and adolescent sections | 64 |

About **56,300 lines, roughly 690,000 words** (220 × 92 + 553 × 55 + 88 × 64), for the 773 placed residents and the
spares: the user's richer size (Q3), about 2.3 times the 24,500 lines (300,000 words) first recommended at 40, 24 and
28. For scale, today's whole library is 58,802 alternatives. The extra goes into breadth, not share: more openers by
frame, more answers per move, more stories and opinions, so a wolf is heard longer before repeating; the cap on a
mantle's share of a conversation (35%, §8.2) and U1's target are unchanged.

### 9.4 Loading (decided)

- **Batch, once.** `Library::setMantles(std::vector<MantleText>)` parses every mantle in one pass and reindexes once.
  Measured this session on the existing build: 20,000 rules in one `Library::parse` load in 0.85 s, against 22.4 s
  through 800 `setCharacter` calls (defect 16). At the richer size the living wearers' mantles are about 50,700 items,
  so about 2.2 s by that rate (B3, §16).
- **Per-owner buckets.** Mantle rules go into `byOwner_[mantle]`, never into the shared `byConcept_`, so a pick scans
  the shared bucket plus one speaker's own (about 55-90 rules at the richer size, of which a move's candidates are a
  few): about today's 0.2 ms instead of 0.49 ms.
- **The layer order**, nearest first: `p_<npc>` (the wearer's own overrides and re-drawn marks: `live.npc_voice.grammar`)
  → `m_<mantle>` → the dialect → `plain`.
- **Only living wearers load, from the rows.** `Game::loadVoices` (`RatwGameTalk.cpp:848`) reads, in one pass, the
  `ready` mantles worn by a living wolf (`live.mantles` and `live.mantle_lines` joined to the bindings in
  `live.npc_voice`), the cards alone of every other ready mantle (id, dialect, accents, fits, honesty: what the pools
  need), then the overrides. In a world without a database it reads an export directory (`--mantles-from DIR`) and
  its bindings from the checkpoint. A mantle that fails to load is skipped and logged (`RATW_MANTLE refused <id>:
  <why>`), and its wearer speaks from the library: a bad row never stops a server.
- **Hot reload.** `mantle.sync <id>` (a DM action, queued by every save in Atlas and the DM, §17.2) reads one mantle's
  rows again and swaps its `byOwner_` bucket and `m_<id>` layer; if the new revision fails to load, the old one stays
  and the action reports why. A new `Game::loadVoice(npc)` and `voice.sync <npc>` reload one wolf; `voice.sync` with
  no target reloads everything (today only the whole set reloads).
- **Mantle traits feed the society** as today's voice traits do (`setHonesty`, `setQuarters`), and its favourite topics
  feed `World::setTalkLeanings`.

### 9.5 Traits: kept, refined (decided)

A placed resident's mantle keeps their seeded `honesty` and `quarter` exactly, because those drive who turns thief and
the Market Board's district factor (doc 79 10.1; `RatwGameTalk.cpp:906-925`). The writer may refine the middle values
of the six talk-only axes (501 residents are "reserved", 592 have no humour) and may change a quirk where three or more
in a town share it (Coldwater has several lake swimmers). Nothing else in the simulation reads them.

**A newborn inherits its mantle's honesty and temperament** (the user, Q4; §12.5): all seven axes come from the card.
A reused mantle carries the honesty of the dead wolf it was written for, and births are one for one with deaths, so
reuse keeps each dialect's honesty mix. Spares have no one to copy, so P15 writes their cards to their dialect's mix
of placed residents (`mantle_check --pools`: each pool's share of `sly` and `guarded` within 5 points of its
dialect's).

### 9.6 The truth rules for mantle text (decided; enforced by `tools/mantle_check.py`, on every batch and every save)

1. **Opinions and feelings: freely.** "I can't abide the noon heat."
2. **Claims about the wearer only with a fact behind them**: "my shop" needs `self.role=merchant`, "my husband"
   `self.spouse_sex=male`, "back home" `self.away=yes`, "my nets" `self.job=fisher`. The patterns are in `truth.json`.
3. **The invented past only about people not in the world** ("my grandmother", unnamed), with age gates ("when I was
   twenty" needs `self.age>=25`).
4. **Named residents only through slots** and only for feelings, facts the game holds, or claims the line records.
5. **Trade talk carries `self.job=`/`self.role=` gates**, so a mantle passed to a wolf of another trade stays true.

### 9.7 The ten luna tables (decided)

The 10 `gpt-5.6-luna` tables become the pilot's first mantles (`written_for` kept). Their stale facts are rewritten
(Dagny Slaghand sells fish in the Rag Market; Berit Kelpwick builds at the Greatly works), "Priest Tamsin Icefell" and
"Aster Coldfell" become slots, the punctuation that breaks the window's templates goes, and the voice rows' `grammar`,
`rules` and `persona` columns are emptied once their mantles' rows hold them. Ported, they are also the core of the
test fixture (`Tests/fixtures/mantles/`, §17.6).

## 10. Dialect (goal 1)

"Follows the dialect of their region" is defined by three numbers (D1-D3, §15.3) and carried by seven levers.

1. **Mantles are written in their dialect** (decided). Writers write Coldwater speech for a Coldwater mantle: the
   lake's idiom, rhythm and sayings. `mantle_check` runs `dialect_check.py`'s lexicon in reverse: another dialect's words
   fail; the mantle's own dialect words are allowed literally only when its accents are light or broad; a mantle that
   allows `none` (160 placed residents speak with none) must reach dialect words only through symbols, so it sounds plain
   while its content stays local. Target: at least 60% of a non-plain mantle's alternatives carry a dialect marker.
2. **Short stock lines become symbols** (decided). `tools/talk_migrate.py` turns the most repeated short texts ("Good."
   ×207, "Always." ×168, "Thank you." ×112, "It is." ×104, "Ha." ×100...) into backchannel symbols whose `plain` value is
   exactly the old text, so plain speakers sound identical (checked by a diff), and each dialect gets its own.
3. **Discourse symbols** (decided): about 36 new symbols in `plain` and all seven dialects (`#hedge#`, `#account#`,
   `#ack#`, `#pivot#`, `#anyway#`, `#resume#`, `#preclose#`, `#excuse#`, `#doubt#`, `#knew#`, `#sympathy#`, `#marvel#`,
   `#concede#`, `#insist#`, `#warn#`, `#welcome_back#`...), about 860 alternatives. Coldwater `preclose = I'd best get
   the crates in | The lake waits for nobody, nor do I`; Saltwell `preclose = The pans won't rake themselves | The sun is
   moving, and so must I`. Agents draft; **the user tunes by ear**, as with doc 62's dialects.
4. **Mark banks** (decided): `mark.oath`, `mark.saying`, `mark.laugh` and `mark.pet` in each dialect (about 140
   alternatives each) for re-drawing a reused mantle's habits (§12.4).
5. **Dialect variants for the most-heard shared lines** (decided): the simulator lists the 300 most-spoken shared
   alternatives, and those get a `say[self.dialect=...]` variant where one is natural.
6. **Accommodation** (decided): per utterance, one accent step broader toward kin, same-dialect friends and fond pairs
   (code-switching home), one step plainer toward an upper-class or formal listener, a stranger from elsewhere or a
   hostile pair. `none` never rises above light. `conv.accent` is read before the speaker's accent
   (`RatwTalk.cpp:1396-1426`).
7. **Content true to each town's trades** (decided): `truth.json` and `here.trades` gate trade lines (Coldwater's fishing
   lines need a fishing job), and Coldwater's grammar gains words for its real work (soap, looms, swine, the still),
   settled with the user. Lines about one's home are gated by `self.from`, lines about where one stands by `here.region`.

Unchanged: Ridgemere's speech is `plain` (doc 62: "the most ordinary"), with each Ridgemere mantle carrying its
quarter's register (the Heights formal, the Sump rough, the Wharf salt); Fenshore speaks Mirebridge's and Saltwell its
own; the player's choices stay plain words (doc 83 decision 4). The English the C++ writes (`#job.directions#`,
`#topic.why#`, `#place.way#`) gets `#way.*#` and `#refuse.*#` symbols where it is templated, in Phase 13.

## 11. Names and roles as slots; reuse (goal 3)

**Slots a mantle may use** (new ones marked *):

| Slot | Is |
|---|---|
| `#self.first#`*, `#self.name#`, `#other.called#`*, `#other.first#` (only when `pair.named=yes`) | names |
| `#self.spouse#`, `#self.spouse_word#`* (husband or wife, by `spouse_sex`; 10 of the 58 couples are same-sex), `#self.child#`, `#self.mother#`, `#self.father#`, `#self.late#`* | kin |
| `#self.neighbour#`, `#self.friend#`*, `#self.rival#`*, `#self.boss#`*, `#self.hand#`*, `#self.priest#`, `#self.shopkeeper#` | people |
| `#self.workplace#`*, `#self.trade_words#`*, `#self.street#`*, `#self.from_town#`*, `#here.town#`, `#here.place_name#` | places and trades |
| `#x.*#`*, `#topic.subject#`, `#topic.heard_from#`* | a bound third party; the topic's people |
| `#other.they#`, `^m`/`^f` in grammars | pronouns, from sex |

**Rules** (decided):
- **A slot that can't be filled makes the line not fit** (the renderer already skips an alternative with a missing fact;
  `mantle_check` makes the gate explicit). A reused mantle cannot name a dead wolf's husband or a bachelor's wife.
- **`mantle_check` refuses**, in a batch and on every save in Atlas and the DM alike (§17.3): any first name or surname
  of any NPC in `live.npcs` (living or dead) outside a slot;
  capitalised words outside symbols that aren't sentence-initial or on the allowlist (the realm's places, Glasswater,
  the Houses, the gods such as the Bright One, weekdays, ships); `he`/`she`/`him`/`her` about the listener; a habit value
  with its own end punctuation (defect 14); the word "dog"; "tells" for private messages; another dialect's words.
- **Rebinding is free.** Mantle rules key on the mantle, never the wolf: `self.mantle=<id>`, `byOwner_[mantle]`,
  layer `m_<id>`, and rows keyed `(world_id, mantle_id, key)`; a binding is one `live.npc_voice` row pointing at the
  mantle. Habits defined in the mantle are seeded by the mantle id, so a mantle's oath survives into its next wearer
  unless its marks are drawn again (§12.4).
- **An edit to a mantle's rows passes on; an override does not.** Since the DM and Atlas now edit a mantle's own text
  (Q2), a change made there is heard from every later wearer, and `mantle_check` judges it as name-free text. A change
  meant for one wolf alone (a nickname for their own spouse, a word only they misuse) goes in that wolf's override,
  `live.npc_voice.grammar/rules`, which is never passed on. The editor asks which, on a worn mantle: "This mantle (its
  next wearers hear it too)" or "This wolf only".
- **Text that still names people stays per wolf and is never reused**: `live.npcs.backstory` (all 1,083 name the NPC)
  and the old persona. `#topic.greeting#` (`rules/player.talk:13-31`, the 169 broken greetings) is retired in favour of
  the mantle's window lines. Because the Mind answers in the window when connected (Q1), mantles carry a `persona`
  template with `#self.first#` slots (`live.mantles.persona`), checked by `mantle_check` like any other mantle text.

## 12. Spare pools, and the dead-to-newborn handoff (goals 2 and 4)

### 12.1 The pools (decided)

Pools are keyed by grammar (`dialectOf`), so Fenshore and the Country's 33 Mirebridge-born share the Mirebridge pool.
The sizes cover about 5 game years (10 real months at speed 1) from a cold start: 1.5 × (first-year births + 4 × the
yearly deficit of same-dialect deaths), plus first-year arrivals from the Country, at least 6 per pool, because a town
with no fertile couple today can get one by marriage (23 marriages in 26 days so far).

| Pool | plain (Ridgemere) | Coldwater | Larkfield | Southmeadow | Saltwell | Mirebridge (Fenshore) | Frosthollow | Oakhaven | All |
|---|---|---|---|---|---|---|---|---|---|
| Births a game year (expected) | 11.5 | 5.5 | 2 | 1.3 | 0.8 | 0 | 0 | 0 | 21 |
| Same-dialect deaths a year | 13.3 | 2.8 | 1.5 | 0.8 | 0.8 | 1.4 | 0.2 | 0.2 | 21 |
| **Spares** | **20** | **26** | **10** | **8** | **6** | **6** | **6** | **6** | **88** |

- **Pups first.** A newborn is a pup for 12 game years (about 2 real years), so every spare has young and adolescent
  sections, and every dialect has a young and an adolescent kit (§7.10) for mantles that have neither.
- **The pools are read from the rows**: a pool is its dialect's `ready` mantles with no living wearer (spares never
  bound, and resting ones), worked out from `live.mantles` and the bindings; `draft` and `retired` mantles are never
  in it.
- **Pool health**: the server logs `RATW_MANTLE pool coldwater spare=N resting=M ready=K` once a game day, and warns
  when spare plus ready falls below 1.5 × the births expected in the next 180 game days, plus 2. The warning is the
  signal for another authoring batch, loaded with `mantle_load` (§17.4); the DM can also write a spare by hand in the
  Mantles panel (§17.10), which counts once it is saved `ready`. `tools/mantle_pool.py forecast ridgemere3`
  (read-only) projects births by dialect from couples and ages, and reports each pool's honesty mix (§9.5). The DM's
  NPC tab shows the same.

### 12.2 Death: the mantle rests (decided)

`Society::died` (`Core/RatwCareers.cpp:873-897`) emits a `CareerNote{"mantle_release", resident}`. The host sets
`released_day` on the binding (the dead's `live.npc_voice` row, which the game may write; the mantle's own rows are
untouched) or in the checkpoint's mantle ledger (`CareerState`, saved through
`RatwWire.cpp:1011-1031`, for file, scratch and harness worlds), and records `knownBy`: the living residents with
familiarity 40 or more toward the dead, resolved through the `CareerWorld` interface. Memories, bonds, beliefs,
acquaintances and the ledger stay keyed to the dead and correctly don't pass on: **the pup inherits the voice, not
the life**.

### 12.3 Birth: choosing a mantle (decided)

`Society::tendBirths` (`RatwCareers.cpp:1299-1362`) chooses right after the couple. The newborn's dialect is
`dialectOf(region of the mother's home)`, through a new `CareerWorld::dialectOfCell`. A new `ResidentRequest::mantle`
sits beside `inPlaceOf` (`RatwSociety.h:218`). Candidates, in order:

1. **An echo**: a ready resting mantle (rested `mantle_rest_days`) whose last wearer was kin of a parent. Its marks are
   kept and echo lines become possible (§12.4).
2. **A ready resting mantle** of the dialect whose `knownBy` has no one living in the birth town, longest-rested first.
3. **Any ready resting mantle** of the dialect, with its marks drawn again.
4. **A spare** of the dialect, by a deterministic lot (`lotOf`).
5. **A neighbouring pool**, by `settings.json` `mantle_fallback` (Frosthollow→Coldwater, Oakhaven→Larkfield,
   Saltwell→Southmeadow, Mirebridge→Oakhaven), its marks drawn from the newborn's own dialect bank and its words
   layer filtered of the other dialect's distinctive words.
6. **Nothing**: the child is born with a seed row, as today, and `RATW_MANTLE none for <dialect>` is logged. A birth is
   never delayed or refused for want of a voice.

A mantle's `fits` must hold for the newborn's sex at any stage. A failed birth re-owes the child **and** returns the
mantle to its earlier status. Only `ready` mantles are candidates; one saved while the birth was being chosen is taken
at its newest revision. **The couple is still chosen realm-wide** (Q5: the user didn't take the question up, so the
recommendation stands): steering births toward the dead's region would keep pools balanced (Coldwater runs about 2.7 a
year short) but would change the economy sims, so the pools absorb the imbalance (§12.1's sizes, the neighbouring
pools, the health warning).

### 12.4 Marks, echoes and the uncanny (decided)

- A mantle names its 2-4 most recognisable habits (`marks oath saying laugh`). **Drawn again** means the wearer's
  `p_<npc>` layer gets new values for them from the dialect's mark bank; everything else carries over.
- **Echo**: when the newborn's mantle was last worn by its kin, `pair.echo=yes` holds for listeners who knew that wearer
  (familiarity 40+), with `#pair.echo_of#` the dead's name. Shared lines then allow "You've your grandmother's laugh, you
  know that?". The coincidence becomes a feature.
- A mantle is never given to a newborn in the household of its last wearer's kin unless it is an echo.

### 12.5 Writing the child (decided)

`Game::makeResidents` (`Core/RatwGame.cpp:1876-1926`), mirrored in memory by `World::makeRequestedResidents`
(`Core/RatwWorld.cpp:3433-3497`) for file, scratch and harness worlds:
- **Stop cloning the mother's** `personality` (a son today inherits "*she* counts things"); write the mantle card's
  ("Calm and warm; swims in the lake even in autumn."). The greeting is left empty (the window speaks from the mantle).
- **Insert `live.npc_voice`**: `home_region` **pinned** to the birth region (no flip at 18), an accent drawn from the
  mantle's allowed accents by the town's mix, **all seven traits, honesty included, and the quirk from the card**
  (the user, Q4: newborns inherit their mantle's honesty and temperament; `Society::setHonesty` takes the card's at
  birth, so a `sly` mantle's pup can turn thief at 16, more than 2 real years away), `mantle_id`, `bound_how` (echo,
  reused or spare) and `bound_day`.
- **Insert the parents into `live.npc_relations`**, so Atlas and the DM see them.
- **A name** from `names/<dialect>.names`, unique as §7.10 says; 25% of children born in place of a dead grandparent
  take their first name. The surname stays the mother's.
- **Reload that one wolf** (`loadVoice`).

### 12.6 Growing up, moving, and the Country (decided)

- Stage sections are criteria, so a wolf's lines change at 13, 18 and 65 with nothing to switch.
- A wolf who moves keeps their mantle and dialect; `self.away` turns on.
- **The Country's 310** keep their seed rows until they arrive in a town (`arrivalFor`, `RatwCareers.cpp:1394-1480`;
  about 5 a year in the Ridgemere 2.0 run). At arrival they bind a mantle from their dialect's pool, which the spare
  sizes include. Writing all 310 now is not part of this plan.
- The dead's voice rows stay; `loadVoices` loads only living wearers.

## 13. Players speak with residents only in the window (goal 7)

### 13.1 Switched off or rerouted, on the server (decided)

1. **Chat.** A new `GameOptions::residentsAnswerChat`, default false (`--residents-answer-chat` for tests). In the chat
   handler (`RatwGame.cpp:6462-6517`) residents are never `addressed`: not as targets, not named, not "the one resident
   within 6 tiles". What stays: `publish` (bystanders hear the player), `heardVoice` (sneakers give themselves away),
   `noticeIntroduction` (6453-6460), `social_.record`.
2. **Overheard, not answered.** A new `Game::overheard(npc, player, text, sense)` for residents at hearing 0.5 or more:
   the line goes into the NPC's memory (so the window can recall it), the router classifies it, and a light person fact
   about the player goes in the ledger ("asked after the smithy"). Residents may then talk *about* it to each other,
   never quoting: "That stranger was asking after the smithy, loud as you like." A targeted or named resident gives a
   non-verbal emote ("Myrtle glances your way.") and the player gets, at most once in 10 minutes per NPC, "Talk to
   Myrtle (T)".
3. **`action talk|speak` → `talkTarget`** (6012-6038) opens the window on the `talk` entry instead.
4. **`react.customer`** (74 rules, `RatwGameTalk.cpp:625-651`) becomes an emote ("Myrtle looks up from the counter.")
   and a 120 s `here.came_in` fact, so the window's own greeting fits when the player opens it.
5. **`welcomeAtInn`** (`RatwGameNewcomers.cpp:1127-1145`) becomes the inn tree's greeting variant `[here.newcomer=yes]`.
6. **Companions.** "What do you think" becomes a companion-tree choice (`does ask.opinion`) answered from the companion's
   stances; `npc_scout`'s interjection becomes an unaddressed bark; `companionSays` (`RatwGameCompanions.cpp:95-103`)
   becomes library concepts (`companion.arrive`, `.danger`, `.hurt`, `.wait`, `.refuse_along`) with dialect, said in the
   window when one is open and as barks otherwise.
7. **The storyline `talk` event** fires only from the window (`dialogueSpeak` already fires it). Naming an NPC in chat
   no longer counts as talking to them.
8. **Removed**: the legacy `tavern` line (`RatwGame.cpp:3245-3262`) and the 5 demo-NPC branches of `authoredReply`
   (`RatwMind.cpp:105-148`).

### 13.2 On the client (decided)

- `sightClicked` (`Client/src/game/state.ts:2160-2163`) opens the wheel instead of toggling a talk target.
- NPCs are dropped from talk targets (`addTarget`, 2170-2179), `speakingTo` (2182-2189) and the composer's `targets`
  (1895-1899); players can still target players.
- The `sendAction('talk')` fallback (2984-2990) goes; Introduce (1779-1785) becomes a window choice.
- While the window is open, a line typed in the chat composer goes to the window's free text.

### 13.3 Kept, and why (decided)

Unaddressed speech is not an interaction: barks to no one in particular, beggars' calls (never using a player's name),
sermons, the crier, howl answers, and residents talking *about* a player to each other (`talk.player`, 85 entries).
Resident letters (doc 55) stay: they are mail, read in their own view, not conversation.

### 13.4 What the window gains (decided)

1. **The mantle's voice** at every node it covers (§7.9), with the role tree's line as the floor.
2. **Choices are moves.** `choice How are you keeping? [does ask.how] -> how`: a node marked `answers` takes the NPC's
   line from `Library::answer` for that move, so a player's question gets the same answer pool an NPC's would, with the
   wearer's own lines on top, by disposition, mood and temperament (Ryan's symmetric design).
3. **A rebuilt resident hub**, inside the 9-choice cap: the role links (wares, work, teach, missions) when there are any;
   Heard any news?; **Ask about...** (yourself; your quirk; what do you make of X, `each opinion in here.opinions max
   3`; how's your kin, `each kin in here.kin max 3`; the town; the way); I'm <name>. / What's your name? (actions
   `introduce` and `ask_name`; the first calls `noticeIntroduction`); up to 2 choices unlocked by what the player knows;
   Goodbye. The loader refuses a node that could show more than 9.
4. **Greetings by meeting and disposition**: `say[topic.meeting=returning|famous]` and `say[pair.disposition=hostile|wary]`
   in every hub, which also brings doc 56's fame and "away" greetings into the window.
5. **Choices unlocked by what the player has heard.** A light per-character claim store, `Game::playerClaims_` (saved
   in the checkpoint), is filled when a player hears a line that asserts a claim, or is told one in a window. It unlocks
   `choice I heard a wagon of yours was hit on the road. [player.knows=bandits.robs:self] -> own_loss`.
6. **What only chat reached before**, now in nodes: the 36 bye and 26 thanks rules; "What day is it?"; the hours; "Am I
   wanted?"; "Where can I find <person>?"; the follow-ups of docs 52, 54 and 55 (matchmaking, vouching, letters).
7. **Memory with the player**: the ledger records pair notes between a player and a resident ("You asked me the way to
   the smithy last week. Find it?"), and the resident's agenda can carry what was said into talk with other residents.
8. **Lines turn over**: on a node's `best_only` concepts the alternatives rotate per player, so the same NPC doesn't say
   the same line twice running.

### 13.5 Typed words without a model (decided)

`dialogue_say` (`RatwGameDialogue.cpp:1192-1242`) goes to `Game::dialogueUnderstand(d, text) -> {intent, slots,
confidence}` instead of `Game::talk`:
1. Normalise the text; if it matches a choice on show ("show me your stock" is the wares choice), take that choice
   with the player's words.
2. Run the existing router, then `Data/Voice/understand.json`: about 40 intents (ask_name, ask_self, ask_family,
   ask_feeling, ask_opinion:<issue>, ask_person:<name>, ask_place:<place>, ask_work, compliment, insult, condole, joke,
   apologise, thank, agree, disagree, yes, no, ask_help, offer_help...) with up to 20 words and slot lexicons built from
   the world (living residents' names, places, goods, the Houses, kin words, the wearer's quirk words).
3. Map the result to a node (`way` with the place chosen), a move with slots (`ask.opinion:watch`, answered from the
   mantle's opinion or a shared stance line), or an action.
4. Below the confidence threshold, the NPC answers with an in-character "didn't follow" line by temperament
   (`dlg.<tree>.unclear`, in dialect) and the choices are shown again.

The roleplay XP rule (`dialogueExperience`, `RatwGameDialogue.cpp:810-835`) is unchanged. A 500-line corpus of player
lines with their expected intents (`Tests/fixtures/understand_corpus.jsonl`) gates W2 in ctest. With the Mind out of the
window, `heed` can no longer record a promise from free talk (defect 15).

### 13.6 The NPC Mind's role

- **Between residents: none (decided).** The ambient Mind path (`--ambient-model-calls`, default 0 and passed by no
  launcher) is deleted in Phase 16 once the final listen-through passes its hard gates.
- **In the window's free talk: decided by the user (2026-11-16, Q1):** "It should be answered by live AI but ONLY if
  the AI is plugged in, otherwise it should be answered by the pre-generated responses." So a player's own words in
  the window go to the NPC Mind whenever the world runs with the Mind connected (the launchers start it by default, per
  the live-AI-by-default rule), and to authored understanding (§13.5) when it isn't, or when a call fails or times out.
  Authored understanding is built in full either way: it is the answer for every world without the Mind, and the
  fallback for every failed call. No `--window-mind` switch beyond "is the Mind connected". The Mind is given the
  mantle's persona template, the dialect and the window brief, never performs actions, and `heed` ignores any promise
  it reports. Between residents the Mind stays out (above).
- **Summaries, recaps, books and letter polish** are untouched by this design.

## 14. Authoring plan

### 14.1 How much (decided; the mantles' richer size is the user's, Q3)

| Content | Phase | Size |
|---|---|---|
| Converting the library: moves and tones tagged on all 8,525 entries, ~121 answer concepts aliased, 2,652 short markers turned into tags | P7 | no new lines; every family checked by hand |
| Backchannel and discourse symbols | P7 | about 76 symbols × 8 grammars, about 1,500 alternatives |
| Shared lines the coverage matrix asks for: closings and pre-closings, known/new/doubt for every claim kind, details per topic, preferred and dispreferred pairs, pivots for the ~120 topic edges, repair, joining, breaks and returns, callbacks | P8 | about 1,500 rules |
| Thin topics, gated to the facts | P8 | gossip 70→250, event 49→150, news 43→150, newcomer 52→120, harvest and ships 11→60 each, deed 21→80, accent 26→60, times 21→60: about 600 rules |
| The data files (moves, topics, frames, temperaments, opinions, claims, truth) | P2-P6 | 85 moves; 45 topics, 120 edges; 8-10 frames, 6 goals, 4 moods; 21 trait values; 20 issues, 130 items; 25 claim kinds × up to 5 points of view |
| Mantles, full tier | P11, P14 | about 220 × 92 = 20,200 lines |
| Mantles, standard tier | P11, P14 | about 553 × 55 = 30,400 lines |
| Spares | P15 | 88 × 64 = 5,600 lines |
| All mantles | | about 56,300 lines, roughly 690,000 words (the richer size; 24,500 lines and 300,000 words at the size first recommended) |
| Stage kits and mark banks | P15 | 16 kits × 60 = 960 lines; 4 banks × 8 dialects, about 1,100 alternatives |
| Name pools | P10 | about 1,100 names, drafts for the user |
| The window: new nodes, unclear lines, companion concepts, the understanding corpus | P13 | about 12 nodes, 80 say lines, 40 unclear lines, 60 companion lines; 500 corpus lines |

### 14.2 Who writes, and how (the user, Q3)

- **Claude agents in the user's sessions**, about 19 mantles (about 1,200 lines) a session of about 4 hours, a town or
  a share of a Ridgemere quarter at a time, behind automated gates. **No paid model calls**: `tools/voice_tables.py`,
  which drafts with a paid story model, is not used for this.
- **From draft to DEV** (§17.4): the agent writes `.mantle` drafts in its own worktree at their export paths
  (`Data/Talk/mantles-export/<dialect>/<id>.mantle`), runs G1-G4 on the files, then `tools/mantle_load.py ridgemere3
  --batch <name> --bind` loads them into DEV's rows (dumping what it replaces to `Saved/Backups/` first), binds them to
  their `written_for` residents and writes the export back, so the batch's commit carries the assigned keys and
  revisions and is the review diff. Nobody writes mantle rows by hand-made SQL.
- **The writer's kit** (Phase 11):
  - `tools/mantle_brief.py ridgemere3 <npc>` (read-only): the resident's facts (`talk_check --residents --json`), kept
    traits, quirk, accent and origin; their post and work label; household, kin, neighbours and bonds above 20; their
    personality and backstory, to honour but not copy; the town's lore (its scene file, doc 82); the dialect grammar and
    20 example lines.
  - `tools/mantle_index.py <town>`: what is already taken in the town (quirks, oaths, sayings, opinions, story kinds,
    distinctive 4-grams), so neighbours come out distinct.
  - A writer's guide: §9.6's truth rules, the dialect's register, the moves' etiquette (dispreferred answers hedge and
    give a reason; closings come in pairs), and the project's rules: never "dog" (wolves; pups for the young), "private
    messages" never "tells", characters male or female only.
- **Public roles first**, then neighbours side by side.

### 14.3 Order (decided)

Sessions are recomputed for the richer size (Q3): about 19 mantles a session instead of about 43.

1. **Pilot** (P11): Saltwell 13, Fenshore 23 and the 8 placed luna residents, about 44 mantles (about 2,900 lines).
   **The user listens; the format and the rows' shape are frozen.**
2. Coldwater 66 (4 sessions).
3. Larkfield 36 (2).
4. Southmeadow 19, Frosthollow 9, Oakhaven 8 (2).
5. Ridgemere by quarter (31): commons 211 (12), the Sump 179 (10), the Estate 51 and the Heights 42 (5), the Works 32,
   the Office 32 and the Wharf 12 (4), with the 40 who have no quarter shared among them.
6. Spares, kits and mark banks (5).

That is 39 sessions for P14 and 5 for P15, against 17 and 3 at the size first recommended.

### 14.4 Quality gates for every batch (decided)

1. **G1, the game's loader**: `talk_check --mantle` (syntax, facts, symbols, 160 characters, depth 6, graft
   reachability, `needs does=`), on the draft files and again on the rows once loaded.
2. **G2, `tools/mantle_check.py`**: names and the allowlist; truth and claims; stage, job and role gates; punctuation in
   habit values; no "dog", no "tells"; pronouns by symbol; the dialect lexicon by accent; dialect density 60%+;
   uniqueness (no 5-word run shared within a dialect, no oath or saying repeated within a town, a quirk at most twice
   in a town); habit saturation (`oath!`, `saying!`) in 25% of alternatives at most; the tier's minimum coverage; for
   spares, the pool's honesty mix (§9.5).
3. **G3, `talk_check --coverage --mantle`**: the batch answers and is answered by sampled real listeners of its town.
4. **G4, the frozen scorecard** on ridgemere3 with the batch bound: no hard gate fails, no soft metric regresses by more
   than 10%, and U1, U2 and D1 for that dialect rise.
5. **G5, listening**: a 60-conversation packet (about 20 minutes). The user listens to the pilot and to the first batch of
   each dialect; for the rest, the slice's review agent reads a stratified 20% sample, and the user's packet is there
   if they want it. "Wrong" marks are fixed and listened to again before the batch merges. At the richer size (Q3)
   nobody reads every line by eye: G1-G4 and G6 see every line, people see the samples.
6. **G6, the round trip**: after `mantle_load`, `mantle_export --check` finds DEV's rows identical to the committed
   files, and G1-G2 pass on the rows as the game reads them.

## 15. Measurement: the offline conversation simulator (goal 6)

### 15.1 `Tests/talk_sim.cpp` (decided)

A new target beside `hunt_sim` and `level_sim` (not a ctest).
- **It builds a scratch `game::Game`** exactly as `talk_check --world` does (`Tests/talk_check.cpp:416-444`,
  `o.scratch = true`): nothing is written.
- **Virtual listeners** are placed with `World::addPlayer` (`Core/RatwWorld.h:670`), as `Tests/ambient_tests.cpp:32` and
  :72 already do for `ambientPicks`.
- **It runs the game's own director**: Phase 0 splits `Game::ambient` into `ambientStart(listeners)` and
  `ambientSpeak(dt)`, a behaviour-neutral refactor proved by golden transcripts, so the simulator uses the same pair
  choice, pacing, breaks and heard sets as the server. (The survey's harness paired random residents in a cell.)
- **Modes**: `--frozen` (the default: the calendar steps 0.2 game hours between exchanges and the economy doesn't tick;
  events can be injected: a theft, a death, a festival, a raid); `--fixtures DIR` (talk replayed over checkpoint
  snapshots recorded once, in P0, from one ticking 25-game-day run; needs a small `Options::checkpointFile`); `--tick`
  (the world really runs: a simulation). **The user allowed the ticking runs this plan names (Q6)**: that one
  recording run in P0 and a ticking 100-hour listen at each slice's end (§15.8). No others; each is reported with its
  seed, length, speed and wall time.
- **Mantles**: from the world's rows, as the server reads them (a database world), or `--mantles-from DIR` (an
  export or the test fixture, §17.6).
- **Options**: `--profile`, `--seed`, `--exchanges`, `--ablate shuffle`, `--library-from REF` (an older library, for
  the old-against-new packet), `--out DIR`.
- **Output**: `lines.jsonl` and `convs.jsonl` (end reason, outcome, effects, participants):

```json
{"run":"p0-l1-s1","t":3612.0,"day":26.13,"cell":"cn_coldwater_store","listener":"sim_l1","conv":417,"beat":6,
 "who":"tn_cw_myrtle_stillford","to":"cty_wick_marshwade","entry":"cw017.request","alt":0,
 "moves":["reassure","request"],"owes":["grant|decline"],"answers":["tell.feeling"],"topic":"bandits","depth":1,
 "tone":"even","asserts":[],"dialect":"coldwater","accent":"broad","layers":["m_cw017","coldwater","plain"],
 "facts":"9f3c0b21","text":"You said the same last week, and you came home with every crate. Will you take a box of my candles up the road with you?"}
```

Before Phase 2 adds moves, `moves` and `owes` are empty and the text-only metrics still work: that is how the baseline
is measured.

### 15.2 Listener profiles (decided)

| Profile | Models |
|---|---|
| L1 | stationary in the 4 busiest Ridgemere places (the survey's 70-resident run) |
| L2 | a wanderer across the whole world |
| L3-L9 | one stationary listener in each town |
| L10 | a regular: the same tavern 2 hours a day for 30 game days, the hardest repetition case |

Each listens for 100 hours, an exchange every 2 minutes, about 3,000 exchanges.

### 15.3 The scorecard (decided)

Computed by `tools/talk_score.py`, compared with `artifacts/talk/scorecard-baseline.json` (committed in Phase 0).
"P0" means Phase 0 measures the baseline.

| Id | Metric | Today | Target | Gate |
|---|---|---|---|---|
| R1 | first word-for-word identical conversation, per listener | L1 exchange 59 (~2 h); L2 64 | 40 h+ | hard: 20 h+ |
| R2 | first repeated opener | L1 exchange 38 (~1.3 h) | 8 h+ | soft |
| R3 | distinct openers per 3,000 | L1 137; L2 382 | L1 1,500+ | soft |
| R4 | a distinctive line (6+ words) heard again by the same listener within 10 h | P0 | 5 or fewer per 100 h | soft |
| R5 | **collision index**: the same distinctive line from two speakers in one place within an hour | P0 | 1 or fewer per 100 h | hard: 3 |
| R6 | the most-spoken line's share | "Good." ~1.5% (L1) | 0.4% | soft |
| R7 | distinct reply entries per 1,000 reply beats | ~133 | 500+ | soft |
| R8 | habit saturation: a speaker's lines with their `oath!` or `saying!` | P0 | 15% or less | soft |
| C1 | owed answers answered (today: lines ending "?" answered, a heuristic) | P0 | 97% | hard: 92% |
| C2 | replies at the best fit available | n/a | 95% | soft |
| C3 | endings by a closing pair or a marked break / by the beat cap / by "nothing fits" | P0 | 80% / 3% / 5% | soft |
| C4 | tone clashes (grave to light with no bridge) per 1,000 beats | P0 | 0 | hard |
| C5 | unmarked topic jumps per 1,000 beats | n/a | 5 | soft |
| C6 | the same entry or question twice in one conversation | P0 | 0 | hard |
| C7 | lines per conversation, mean and 90th percentile | ~3.2; 79% are 3 lines | 5-7; p90 9+ | soft |
| T1 | raw ids spoken (`truth.json` `ids`) | present | 0 | hard |
| T2 | presupposition failures (weather, time, kin, death, trade) | P0 | 0 | hard |
| T3 | self-reference errors: the teller is the victim in the third person; an unbacked first-person claim | present | 0 | hard |
| T4 | identity drift: one speaker's incompatible stances or claims within 30 days with no concede | n/a | 0 | hard |
| T5 | telling a listener what they already believe, with no "known" answer | P0 | 2% | soft |
| D1 | lines carrying a dialect token (from the expansion trace), by accent | library reach 1.4% | none 0%; light 25-50%; broad 60%+ | hard for none |
| D2 | another dialect's words in a speaker's line | 0 statically | 0 in transcripts | hard |
| D3 | a classifier guesses the speaker's dialect from 20 lines | P0 | light and broad 85%+ | soft |
| U1 | a resident's lines from their mantle | ~0 | 20-35% (capped against overplay) | soft |
| U2 | **distinguishability**: for residents with identical 8-axis profiles, held-out lines attributed to the right one of two | P0 (expect ~55-60%) | 90%+ | soft |
| K1 | conversations on everyday topics | L1 0.2%; L2 0.1% | 35%+ | soft |
| K2 | the largest topic's share | gossip 86% (L1) | 25% or less | soft |
| K3 | the same news told twice to the same listener | present | 0 | hard |
| C8 | conversations with a third speaker | 0 | 5-15% | soft |
| C9 | callbacks (lines using the ledger) per 10 conversations | ~0 | 1.5+ | soft |
| W1 | window lines at the hub and greetings that are mantle lines | 0 | 40%+ | soft |
| W2 | free-text understanding on the 500-line corpus: right first, confidently wrong | n/a | 85%+; 5% or less | hard: wrong 8% or less |
| W3 | a resident answers chat with the option off | yes | 0 | hard |
| B1-B5 | performance budgets (§16) | | | hard |
| H1 | blind naturalness 1-5 (§15.7) | P0 packet of today's | 3.8+; new − shuffled 2.0+; new ≥ old + 1.0 | at slice ends |
| H2 | "written whole by a writer?" guessed for generated conversations | n/a | 40%+ | at slice ends |

### 15.4 Coverage, before anything is heard (decided)

`talk_check --coverage` asks, for every move that can be owed, every topic, and every **replier kind**: is there at least
one line that answers and holds? Replier kinds come from `Data/Talk/profiles.json`, about 150 clusters of Ridgemere 3's
real residents on the axes lines test (stage, job, temper, warmth, humour, wealth, dialect, band), regenerated by
`tools/talk_profiles.py ridgemere3` (read-only). Zero holes for the core moves (greet.return, react.news,
react.complaint, answer.detail, accept_preclose, farewell.return, accept_decline); elsewhere the hole rate may only
fall. About 90 × 45 × 150 fit checks, no rendering: seconds.

### 15.5 Are the metrics measuring anything? (decided)

Every C and T metric must separate real transcripts from an **ablation**: the same lines, turns shuffled within the
topic, criteria still enforced (the control in Ryan et al. 2016, where people rated managed conversations 4.05 and the
shuffled ones 1.34). A metric that scores the shuffled transcripts within 30% of the real ones isn't measuring
coherence and is fixed or dropped. The text-only checks (the truth lexicon, the id pattern, n-grams, the "?" heuristic,
the dialect lexicon) don't depend on tags, so they keep the tag-based ones honest.

### 15.6 Listening tools (decided)

1. **`tools/talk_listen.py RUN --html out.html [--packet 60]`**: one self-contained page; conversations laid out like
   the story log with speaker chips (name, job, origin, accent); hovering a line shows its entry, move, what it owed and
   why it fit; a fine / off / wrong button and a note per conversation. Marks export as JSON, are filed under
   `artifacts/talk/marks/`, and the scorer reports the "marked wrong" rate per file family (a mark on a mantle's line
   names the mantle and the item's key, §7.9, so its fix is one row). `--packet` samples by
   dialect × pair kind (kin, friends, strangers, boss and hand, rich and poor) × topic family × length.
2. **In the game, for DEV accounts**: `/talkwhy on` shows entry ids and moves beside NPC speech in that player's story
   log; `/flag [note]` saves the last 3 conversations heard, with their per-beat queries and seeds, to
   `Saved/Logs/talk-flags.jsonl` (the server keeps a ring of the last 200, about 5 MB); `talk_check --replay
   Saved/Logs/talk-flags.jsonl:N` reproduces one exactly, with the why trace.
3. **`--log-talk-words`** (on in DEV worlds): `Saved/Logs/npc-voices.jsonl` gains the words, entries and moves. Today it
   records only kind, route and NPC, so no real server speech could be checked for this design.

### 15.7 Blind ratings (decided)

`tools/talk_rate.py` makes a packet of 90 conversations (30 from the new system, 30 from the baseline library, 30
shuffled), in random order, each rated 1-5 for how natural it flows, with a "written whole by a writer?" toggle. It
produces an HTML page and a CSV; the scorer computes H1 and H2. Run at the end of Slice 2 and at the final listen.

### 15.8 Where the gates run (decided)

| When | What |
|---|---|
| Every commit (ctest) | the loader; `talk_lint`; `mantle_check` and `talk_check --mantle` over the test fixture (`Tests/fixtures/mantles/`) and over the whole committed export (`Data/Talk/mantles-export/`); coverage; golden transcripts (60 seeded conversations over a frozen copy of the library and the fixture's mantles, `Tests/fixtures/talk_lib/`, regenerated with `--bless`); `talk_scorecard_quick`: a synthetic town export (`Tests/fixtures/talk_town/`: 3 cells, about 40 residents, 3 dialects, wearing the fixture's mantles), 2,000 frozen exchanges in under 60 s, no database, enforcing the hard gates and failing a soft metric that regresses by more than 10% |
| With the local PostgreSQL (skipped without it, as `test_world_sync.py` is) | `test_mantle_load.py`: the round trip, refusals, revisions, the history, `mantle.sync` on a built server (§17.6) |
| End of each phase | ridgemere3, frozen, every profile, reading DEV's mantle rows; the scorecard diff goes in the phase's report |
| End of each slice | a ticking 100-hour listening run (Q6, reported), the frozen profiles beside it for the scorecard diff, a listen packet, `mantle_export --check` and the fresh export committed, and at Slices 2 and 5 the blind ratings |

## 16. Performance budgets (decided)

The server runs at 20 Hz (50 ms a tick). Measured on this laptop (i9-13900H, `build-core` of 2026-11-15):

| Measurement | Result |
|---|---|
| The library (8,652 entries) loading | 0.54 s, 26.5 MB RSS |
| One `talk.smalltalk` pick, rendering every candidate | about 0.2 ms |
| 800 tables × 25 rules through `setCharacter`, one at a time | **22.4 s** (the first 100 take 1.1 s: quadratic) |
| The same 20,000 rules in one `Library::parse` | **0.85 s** |
| A pick with those 800 tables in the shared bucket | **0.49 ms** |
| `talk_check --world ridgemere3 --residents` | 4.92 s, 749 MB RSS |
| The survey harness, per exchange (topic, query, converse) | about 6 ms |

| Id | Budget | How it's held |
|---|---|---|
| B1 | one beat (facts, pick, render): mean 0.6 ms or less, p99 3 ms; a director look every 5 s: p99 5 ms | `Tests/talk_bench.cpp`, ctest label `perf` (not run by default) |
| B2 | talk at most 1% of tick time | `RATW_TALK_STATS` every 10 minutes (beats, pick ms mean and p99, facts ms, conversations, ledger size, mantles loaded); `/talkstats` for admins |
| B3 | mantles add 3 s or less to start and to `voice.sync` (reading about 50,700 rows and one batch parse: about 2.2 s at the measured rate); `mantle.sync` of one mantle 50 ms or less | batch `setMantles`, one reindex; a one-mantle swap of its bucket and layer |
| B4 | RSS grows 150 MB or less (the richer size, Q3: at today's about 2.3 KB an entry, 50,700 entries would be about 115 MB) | interned strings for mantle entries, aiming at half that |
| B5 | the simulator runs 150+ exchanges a second per core | ten profiles as separate processes take about 2 minutes frozen |

**Mechanisms:** batch loading and per-owner buckets (§9.4); only the speaker's own mantle is consulted (about 55-90
rules); candidates are rendered lazily in the chosen order; person facts are cached per conversation, because
`peopleOf` and `neighboursOf` scan positions; concurrency stays at `AtOnce = 3`, only where a player listens, until
B2's numbers show room.

## 17. Mantles in the world database, and migrating `ratw_ridgemere3` (decided; the storage is the user's, Q2)

The user's answer (Q2): mantle text lives in the world database, editable live in Atlas and the DM tool, not in
repository files. This section is how: the tables (17.1), how an edit reaches a running server (17.2), the checks on
every save (17.3), how agents' drafts reach DEV (17.4), the text export that keeps review possible (17.5), where tests
get their mantles (17.6), the bindings (17.7), the checkpoint (17.8), PROD and hosted worlds (17.9), and the editors
(17.10).

### 17.1 Migration `Database/migrations/0066_mantles.sql`

The latest is 0065; the rollback is `Database/rollback/0066_mantles_down.sql`, run only after `tools/mantle_load.py
backup <world>` has dumped the three tables and the new columns to `Saved/Backups/` (its header says so). The server is
PostgreSQL 18.6. The tables are in `live`, beside
`live.npc_voice` and `live.dialects`, because mantles are live data under doc 20's rules (Atlas edits DEV's, the DM
edits live's, an import and a pull carry them) and `tools/world_sync.py` already compares `live` tables row by row.

```sql
-- Mantles (Docs/Design/84 §7.9, §17): each wolf's way of talking, name-free, passed on at death; the user's answer Q2
-- puts the text here, editable live in Atlas (DEV) and the Dungeon Master (live). One row a mantle (its card) and one
-- row an item (a line, a rule, a symbol, an opinion, a story, a window line, a graft) in the .mantle syntax, so the
-- game's one parser reads rows, drafts and exports alike. The game reads them at start, on voice.sync and on
-- mantle.sync <id>, and never writes them: it writes only the bindings in live.npc_voice.
CREATE TABLE live.mantles (
    world_id     ratw_id NOT NULL REFERENCES world.worlds ON DELETE CASCADE,
    id           text NOT NULL CHECK (id ~ '^[a-z]{2}[0-9]{3,4}$'),
    dialect      text NOT NULL CHECK (dialect ~ '^[a-z_]{1,40}$'),     -- plain, or a live.dialects name (checked on save)
    tier         text NOT NULL CHECK (tier IN ('full', 'standard', 'spare')),
    state        text NOT NULL DEFAULT 'draft' CHECK (state IN ('draft', 'ready', 'retired')),
    format       integer NOT NULL DEFAULT 1 CHECK (format >= 1),        -- the .mantle syntax's version: the server reads
                                                                        -- its own and the one before, as builds do (0065)
    accents      text[] NOT NULL CHECK (cardinality(accents) BETWEEN 1 AND 3
                                        AND accents <@ ARRAY['none', 'light', 'broad']),  -- the first is as written
    title        text NOT NULL DEFAULT '' CHECK (char_length(title) <= 80),      -- for authors and the DM, never spoken
    written_for  ratw_id,                                                         -- metadata, never spoken
    fits         text NOT NULL DEFAULT '' CHECK (char_length(fits) <= 400),      -- hard criteria a wearer must match
    prefers      text NOT NULL DEFAULT '' CHECK (char_length(prefers) <= 400),   -- soft: how well a wearer suits it
    traits       jsonb NOT NULL DEFAULT '{}' CHECK (jsonb_typeof(traits) = 'object'),   -- as live.npc_voice.traits
    quirk        text NOT NULL DEFAULT '' CHECK (quirk ~ '^[a-z_]{0,60}$'),
    card         jsonb NOT NULL DEFAULT '{}' CHECK (jsonb_typeof(card) = 'object'),     -- likes, dislikes, fears, hopes,
                                                                                         -- topics, styles, marks
    persona      text NOT NULL DEFAULT '' CHECK (char_length(persona) <= 4000),  -- for the Mind in the window (Q1)
    revision     integer NOT NULL DEFAULT 1 CHECK (revision >= 1),               -- rises with any change to it or its items
    written_by   text NOT NULL DEFAULT '' CHECK (char_length(written_by) <= 80), -- 'claude-<batch>', 'atlas', 'dm', 'seed'
    created_at   timestamptz NOT NULL DEFAULT now(),
    updated_at   timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, id)
);
COMMENT ON TABLE live.mantles IS 'Each wolf''s way of talking (doc 84): the card of one mantle; its items are live.mantle_lines.';

CREATE TABLE live.mantle_lines (
    world_id     ratw_id NOT NULL,
    mantle_id    text NOT NULL,
    key          text NOT NULL CHECK (key ~ '^[a-z0-9_.:]{1,120}$'),      -- <section>.<move>.<name> (§7.9), stable
    section      text NOT NULL CHECK (section IN ('any', 'grown', 'young', 'adolescent', 'old')),
    kind         text NOT NULL CHECK (kind IN ('symbol', 'opinion', 'line', 'rule', 'story', 'dlg', 'graft')),
    move         text NOT NULL DEFAULT '' CHECK (move ~ '^[a-z0-9_.:]{0,80}$'),  -- a line's `does`; a dlg's or graft's node
    position     integer NOT NULL,                                        -- order within the section
    body         text NOT NULL CHECK (char_length(body) BETWEEN 1 AND 6000),  -- the item in .mantle syntax, as exported
    revision     integer NOT NULL DEFAULT 1 CHECK (revision >= 1),
    updated_at   timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (world_id, mantle_id, key),
    FOREIGN KEY (world_id, mantle_id) REFERENCES live.mantles (world_id, id) ON DELETE CASCADE
);
CREATE INDEX mantle_lines_in_order ON live.mantle_lines (world_id, mantle_id, section, position);

-- Every change, for audit, review and revert. Append-only; each world keeps its own (never synced, never packaged).
CREATE TABLE live.mantle_history (
    seq          bigserial PRIMARY KEY,
    world_id     ratw_id NOT NULL,
    mantle_id    text NOT NULL,
    key          text NOT NULL DEFAULT '',          -- '' for the card (the live.mantles row)
    revision     integer NOT NULL,                  -- the mantle's revision after the change
    action       text NOT NULL CHECK (action IN ('insert', 'update', 'delete')),
    before       jsonb,
    after        jsonb,
    source       text NOT NULL DEFAULT '',          -- mantle_load:<batch>, atlas, dm, import, pull, migration
    account      text NOT NULL DEFAULT '',          -- who (an Atlas or DM login, or the session that loaded a batch)
    note         text NOT NULL DEFAULT '' CHECK (char_length(note) <= 400),
    at           timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX mantle_history_of ON live.mantle_history (world_id, mantle_id, seq);

-- live.mantle_changed() (in this migration): on each changed row, raises the item's and its mantle's revision, stamps
-- updated_at and writes one history row, taking source, account and note from the transaction's settings
-- (SET LOCAL ratw.edit_source / ratw.edit_by / ratw.edit_note, which tools/mantle_store.py sets). SECURITY DEFINER,
-- so nobody needs, or has, a right to write the history directly (as 0059 does for live.game_changes).
CREATE TRIGGER mantle_changed BEFORE INSERT OR UPDATE OR DELETE ON live.mantles
    FOR EACH ROW EXECUTE FUNCTION live.mantle_changed();
CREATE TRIGGER mantle_changed BEFORE INSERT OR UPDATE OR DELETE ON live.mantle_lines
    FOR EACH ROW EXECUTE FUNCTION live.mantle_changed();
REVOKE INSERT, UPDATE, DELETE, TRUNCATE ON live.mantle_history FROM ratw_editor, ratw_publisher, ratw_game, ratw_dm;
-- The game reads mantles and never writes them.
REVOKE INSERT, UPDATE, DELETE, TRUNCATE ON live.mantles, live.mantle_lines FROM ratw_game;
GRANT SELECT ON live.mantles, live.mantle_lines, live.mantle_history TO ratw_editor, ratw_publisher, ratw_game, ratw_dm;
GRANT INSERT, UPDATE, DELETE ON live.mantles, live.mantle_lines TO ratw_editor, ratw_publisher, ratw_dm;

-- Who wears which mantle. A binding is the wearer's voice row; a mantle's wearers, in order, are the rows naming it.
ALTER TABLE live.npc_voice
    ADD COLUMN mantle_id    text,
    ADD COLUMN bound_day    double precision,
    ADD COLUMN bound_how    text NOT NULL DEFAULT '' CHECK (bound_how IN ('', 'authored', 'spare', 'reused', 'echo', 'arrival', 'dm')),
    ADD COLUMN released_day double precision,
    -- A mantle once worn is never deleted, only retired.
    ADD FOREIGN KEY (world_id, mantle_id) REFERENCES live.mantles (world_id, id) ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED;
COMMENT ON COLUMN live.npc_voice.mantle_id IS 'The mantle this resident wears or wore (doc 84); released_day set when they died.';
-- A mantle is worn by one living wolf at a time (goal 1).
CREATE UNIQUE INDEX npc_voice_mantle_worn ON live.npc_voice (world_id, mantle_id)
    WHERE mantle_id IS NOT NULL AND released_day IS NULL;
-- Rows the game writes (births, deaths, arrivals) show in Atlas's conflict list, as live.npcs's do (migration 0059).
CREATE TRIGGER game_changes AFTER INSERT OR UPDATE OR DELETE ON live.npc_voice FOR EACH ROW
    WHEN (current_user = 'ratw_game') EXECUTE FUNCTION live.log_game_change('npc_id');
```

"Worn", "resting since" and "spare" are worked out from the bindings and `state`, so there is one truth (§9.2). The
database's own checks are only the cheap ones; everything the game's loader would refuse is checked before a write
(17.3), and the game skips, rather than trips on, anything that slipped past (§9.4).

### 17.2 From a save to a running server: hot reload, never a build

Mantles are not part of a world build: like `live.npc_voice`, `live.dialects` and the economy, the server reads them
at start, and a build's `LIVE_LAYERS` (`tools/world_build.py`) don't include them, so an edit never makes a new build
or restarts a DEV server.

1. A save in Atlas or the DM (or a `mantle_load` batch) is checked (17.3) and written in one transaction, raising the
   mantle's `revision` and writing the history.
2. The same transaction queues a `mantle.sync <id>` action in `dm.actions` for the world's server, as saving a voice
   queues `voice.sync` today (`tools/dungeon_master.py`). A batch queues one `voice.sync` instead of one per mantle.
3. The server takes it on its next action poll: `Game::loadMantle(id)` reads that mantle's rows, parses them alone and
   swaps its `byOwner_` bucket and `m_<id>` layer (B3: 50 ms or less). A conversation already under way keeps the lines
   it has spoken; its next beat sees the new revision. The action's result names the revision now loaded, or the
   reasons it wasn't and the revision still in use.
4. A `draft` mantle is saved and checked like any other but never loaded; saving it `ready` is what lets the game see
   it (a spare joins its pool at that moment). A `retired` one leaves the pools at once.
5. Scratch servers read the rows at start, read only; a later edit reaches one only when it is started again.

### 17.3 Checked on every save: the same checks as a batch (`tools/mantle_store.py`)

One module does every write to mantle rows, so Atlas, the DM tool and `mantle_load` can't disagree:

1. **The game's own loader**: the whole mantle as it would be after the save (the changed item spliced into its rows)
   goes to `talk_check --validate --mantle`, with the world's dialects (`live.dialects` over the files) and facts:
   syntax, facts, symbols, 160 characters an alternative, depth 6, graft reachability, `needs does=`, known moves and
   tones (G1). About a second.
2. **`mantle_check --one <id> --world <world>`** (G2), reading the world's own rows: names in `live.npcs`, living or
   dead, outside slots; the allowlist; truth patterns and the stage, job and role gates; punctuation in habit values;
   no "dog", no "tells"; pronouns by symbol; the dialect lexicon by accent and the density; uniqueness against the
   other mantles of the dialect and the wearer's town; habit saturation; and the persona template.
3. **The binding**: the current wearer still fits `fits`; `written_for`, if set, is a resident; `retired` is refused
   while the mantle is worn (release or reassign first).
4. **The state**: a `draft` may be under its tier's minimum (a warning beside the counts); `ready` needs the tier's
   minimum coverage and a clean G2.
5. **The revision**: a save names the revision it was edited from; if the mantle has moved on, it is refused with who
   changed it and when, and the editor shows the newer rows to merge, as Atlas's keyed edits do (`tools/live_edit.py`).

A refused save writes nothing and returns the problems by item key, which the editor shows beside the item. **Saving a
dialect** (`save_dialect`) also re-checks the `ready` mantles that speak it, and is refused, naming them, if a symbol
they use would go.

### 17.4 How agents author into DEV: `tools/mantle_load.py`

A committed, re-runnable tool, so DEV's mantle rows are only ever written by a tool that checks, backs up and records,
never by hand-made SQL:

```
python3 tools/mantle_load.py ridgemere3 Data/Talk/mantles-export/coldwater/cw0*.mantle --batch cw-2 --bind [--dry-run]
python3 tools/mantle_load.py check ridgemere3
python3 tools/mantle_load.py backup ridgemere3        # every mantle row and binding to Saved/Backups/; restore FILE puts one back
```

1. **Parse and check**: every file through 17.3's checks (G1, G2; G3 and G4 the agent ran before). Any problem refuses
   the whole batch, listed by file, mantle and line.
2. **Compare revisions**: a file's `revision` header is the revision it was exported from (none for a new mantle). If
   the row has moved on since (someone edited it in Atlas or the DM), that mantle is refused unless `--take-theirs`
   (keep the database's, skip the file) or `--take-mine` (overwrite; the history keeps the other) is given for it.
3. **Back up**: every row the batch will change (mantles, items, the `live.npc_voice` rows it binds) is dumped to
   `Saved/Backups/mantles-<world>-<batch>-<stamp>.jsonl`, which `mantle_load restore FILE` puts back.
4. **Write**, in one transaction as `ratw_editor` (the role Atlas writes with), with `ratw.edit_source =
   'mantle_load:<batch>'`: new mantles and items inserted, changed items updated, unchanged ones left alone (their
   revision doesn't move), and items absent from a file deleted only with `--prune`. `written_by` is the batch.
5. **Bind** (`--bind`): each mantle to its `written_for` resident (`bound_how = 'authored'`, `bound_day` = the world's
   day), unless they are dead or already wear another.
6. **Tell the server**: one `voice.sync` for a running DEV server.
7. **Write back**: the touched mantles are exported over their files (17.5), so the commit carries the assigned keys
   and revisions.

Loading the same files twice changes nothing. `check` reports placed residents with no mantle, mantles bound twice (in
checkpoint worlds; the database's index forbids it), `written_for` ids that no longer exist, `ready` mantles that fail
today's checks (after a dialect or a resident changed), and rows that differ from the export; the scorecard and
`voice_check` flag the same.

### 17.5 Review stays possible: the export (`tools/mantle_export.py`)

```
python3 tools/mantle_export.py ridgemere3 [--out Data/Talk/mantles-export] [--mantle ID] [--check]
```

- One file a mantle, `Data/Talk/mantles-export/<dialect>/<id>.mantle`, in canonical form: the header and card from
  the columns (with `revision` and `state`), then the items by section and position, each with its `@` key, each body
  verbatim. Plus `index.tsv`: id, dialect, tier, state, revision, `written_for`, `written_by`. Wearers are not in the
  export (they change with every birth); Atlas shows them.
- **Canonical, so diffs are clean**: loading an export and exporting again gives the same bytes (`test_mantle_load.py`
  proves it).
- **`--check`** lists every mantle and item where the rows and the files differ, and exits 1. It runs at every batch
  (G6), at every slice's end and before any import to a live world.
- **Committed**: each batch commits its files after loading; each slice's end exports afresh and commits, so every
  edit made in Atlas or the DM since shows as a git diff ("14 items in 6 mantles changed in the DM since the last
  export"), with `live.mantle_history` naming who and when.
- **Never a server's source** where there's a database: the library's loader reads `.talk` and `.grammar` files, not
  `.mantle`, and only `--mantles-from DIR` points a server at an export (for worlds without a database). ctest checks
  that the committed export loads and passes G2 (§15.8), so a broken text can't sit committed unnoticed.
- `tools/world_build.py export DIR` also writes the world's mantles to `DIR/mantles/` in this form, so `world_check`
  and `game_load` on an exported build hear them.

### 17.6 Where tests get their mantles

- **ctest, no database**: `Tests/fixtures/mantles/`, about 20 committed mantles in 3 dialects (the 10 luna tables
  ported, 6 spares, and a few written to exercise every item kind, stage section and graft), changed only on purpose,
  with `--bless`. `mantle_tests`, the golden transcripts and `talk_scorecard_quick` (its synthetic town wears them)
  read it through `--mantles-from`. Content that changes in DEV never breaks a unit test.
- **ctest over the real content**: G1 and G2 over the whole committed export.
- **The database's own tests** (with the local PostgreSQL, skipped without it, as `test_world_sync.py` is):
  `test_mantle_load.py` makes a throwaway database from the migrations, loads the fixture, exports it and compares
  bytes; refuses a name outside a slot, a stale revision, a `ready` under its tier's minimum and a retire while worn;
  checks the revisions and the history rows; and, with a built server, that `mantle.sync` loads a new revision and
  keeps the old one when the new one fails. `test_dungeon_master.py` and `test_world_sync.py` gain the mantle save,
  the import and the pull.
- **Harness runs** (`talk_sim`, `talk_check --world`, the scorecard) on ridgemere3 read DEV's rows, read only, as the
  server does.

### 17.7 The bindings

1. Each authoring batch binds its own mantles to their `written_for` residents (`mantle_load --bind`). Residents whose
   batch hasn't landed keep speaking from the library, as today.
2. The 10 luna rows have their `grammar`, `rules` and `persona` emptied once their mantles land (Phase 11), after a
   dump to `Saved/Backups/`.
3. Nothing else changes: `home_region`, accent, quarter, traits and quirk stay for placed residents (a mantle's
   refinements of talk-only traits are in its card, not written back; newborns take the card whole, §12.5). The
   Country's 310 rows are untouched until they arrive.
4. Births, deaths and arrivals write the bindings from the game (§12); the DM's `mantle.assign` and `mantle.release`
   write them from the tool (`bound_how = 'dm'`).

### 17.8 The checkpoint

`Core/RatwWire.cpp`, beside careers at :1011-1031, gains `talkLedger`, `talkCooldowns`, `news`, `townNews`, residents'
heard sets, `playerClaims`, and `mantles` (the society's ledger of bindings, for worlds without a database; a database
world reads its bindings from `live.npc_voice` instead, as it does for NPC rows). Mantle text is never in the
checkpoint. Older checkpoints load with these empty.

### 17.9 PROD and hosted worlds

- **PROD** is on the old map at migration 54, so 0066 waits behind 0055-0065 there, and the mantles, written for
  Ridgemere 3's residents, reach a live world only when it moves to Ridgemere 3. That push is the user's.
- **Import and pull** (`tools/world_sync.py`): `live.mantles` and `live.mantle_lines` join `SYNCED` as live data.
  Comparing item by item means an agent's edit to one line in DEV and a DM's to another in live don't conflict; an
  edit to the same item on both sides is a conflict in Atlas's list, chosen there as any live row's is. `revision` and
  `updated_at` are already among the fields sync doesn't compare (`VOLATILE`). `live.mantle_history` stays in each
  world. An import also runs `mantle_export --check` first and records the export's commit in the release note.
- **Hosted worlds** (doc 78): both tables join `world_package.py`'s `SEED_TABLES` (add-only: a host's own edits are
  never overwritten), and `written_by` follows `PROVENANCE` and `AUTHOR_COLUMNS` (a model's name, `dm`, `seed` or
  `package`, with `atlas` added to the allowed words; never a person's).

### 17.10 Atlas and the DM

- **The Mantles panel**, beside Voices, Dialects and Figures in the Talk panels both tools share
  (`Editor/src/dm/TalkPanels.tsx`; Atlas's "NPCs & areas" workspace and the DM's NPC tab): a list by dialect, tier,
  state and status (spare; worn by whom; resting until which day), with each pool's health and honesty mix. A mantle
  opens as its card (form fields) and its items by section, each a text box in `.mantle` syntax; problems from 17.3
  sit beside the item they concern. "Hear it" renders five samples of an item for the current wearer (through
  `talk_check`'s render, nothing spoken in the world). The history lists every change with who, when and the diff,
  and reverts any revision (a revert is a new revision). On a worn mantle, a save asks "This mantle (its next wearers
  hear it too)" or "This wolf only" (§11). New writes a spare from a blank card or a copy of another (keys kept, text
  to be changed: uniqueness refuses a near copy).
- **The People panel** shows each resident's mantle, its state and its pool; per-NPC overrides are edited as today and
  checked by `talk_check --validate` before saving.
- **DM commands**: `mantle.assign <npc> <mantle>`, `mantle.release <npc>`, `mantle.retire <mantle>` (refused while
  worn), `mantle.sync <mantle>`, `voice.sync <npc>`.

## 18. Phases

The cadence is the lean one: one agent per phase, in this order, reusing one build, with targeted tests; the full suite,
an end-to-end run and one review-and-fix at each slice's end. Every phase's report carries its scorecard diff (§15.8).
Hours are rough, for one agent.

| # | Phase | Needs | Delivers | Tests and gate | h |
|---|---|---|---|---|---|
| | **Slice 1: hear it, then make it true** | | | | |
| P0 | The instrument | – | `ambientStart`/`ambientSpeak` split; `Tests/talk_sim.cpp` (frozen and fixture modes, virtual listeners, the ten profiles, `--ablate`, `--library-from`); `tools/talk_score.py`, `talk_listen.py`, `talk_rate.py`; the synthetic `talk_town` fixture and `talk_scorecard_quick`; `--log-talk-words`; the ticking mode and **the one ticking 25-game-day run that records the fixtures** (the user, Q6; reported); **the committed baseline scorecard and a listen sheet for the user**; the most-heard residents list (it orders the authoring) | golden transcripts unchanged by the split; the run's report (seed, days, speed, wall time) | 13 |
| P1 | Truth fixes | P0 | defects 1-12, 14 and 15 (`heed` ignores promises from window free talk): structured claims (`Belief` kind, object, place; `claims.json`; `placeWords`), no raw ids, teller ≠ victim, `housemates`, `repeat` per concept, the cooldown store, every beat heard, `noteNews` for births and newcomers, one `claimKind`, `pair.named` between residents, the district priest, `exchangeContext`, news saved; `here.outdoors`; `truth.json` with `talk_lint` and the runtime check; the punctuation join; Coldwater's trade lines gated | T1 = 0, T3 = 0; T2 counted and frozen as the baseline | 10 |
| | Slice end: full suite, all profiles, a ticking 100-hour listen (Q6, in the background, reported), review and fix | | | | 3 |
| | **Slice 2: the conversation engine** | | | | |
| P2 | Moves and tags | P1 | `moves.json`; `does`/`owes`/`invites`/`tone`/`asserts`/`denies`/`gist`/`binds`/`{k=v}` parsed; `Ns::Conv`, `Ns::X`; `byMove_`; aliases for the answer concepts; the dead-end proof; `SaidLine` metadata into transcripts; moves tagged on the 40 most-spoken families | loader refusals tested; transcripts carry moves | 10 |
| P3 | The turn engine | P2 | `Conversation`; `begin`/`next`/`answer`; fit then tiers; `frames.json`; budgets; closings; shrugs become `ack`; tone rules; `converse` rebuilt on `begin`/`next` | golden for unconverted content; C3, C4, C6 measured | 12 |
| P4 | Beats when spoken; knowledge; a third voice | P3 | per-beat queries in `Game::ambient` with cached person facts; breaks and returns; `joiners` (3 at most); per-beat effects; asserting, overhearing, corroboration, denial, `other.knows`; `talk_bench` | B1 within budget; T5, C8 measured | 10 |
| P5 | Topics, agendas, goals, the ledger | P4 | `topics.json` (today's numbers first, proved identical, then tuned); novelty, saturation, the everyday floor; the topic graph and pivots; `agendaFor`; goals; `RatwTalkLedger` in the checkpoint; person facts; unheard notes from `World::gossip`; invitations and talk-only promises | K1 35%+, K2 25% or less, K3 = 0 | 12 |
| P6 | Opinions, temperament, moods, accommodation | P5 | `opinions.json` with drift; `self.op.*`, `pair.agree.*`; likes; `temperaments.json`; `self.mood`; `world.mood`; `conv.accent` | T4 = 0; D1 measured | 6 |
| P7 | Converting the library; coverage | P6 | `tools/talk_conv.py` (2,652 markers to tags), moves and tones on all 8,525 entries, `talk_migrate.py` (backchannel symbols, plain identical by diff), the discourse symbols drafted in 8 grammars; `talk_check --coverage` and `profiles.json` | equivalence test (the same possible lines, old against new); coverage holes listed | 12 |
| P8 | The shared lines (three sessions) | P7 | about 1,500 coverage rules and 600 thin-topic rules (§14.1), in dialect symbols | 0 holes on the core moves; R7 3× baseline; C1 92%+ | 18 |
| | Slice end: full suite, all profiles, a ticking 100-hour listen (Q6, reported), a listen packet, **the first blind ratings**, review and fix | | | H1 measured against the baseline | 6 |
| | **Slice 3: mantles and the lifecycle** | | | | |
| P9 | The mantle engine and its tables | P8 | the `.mantle` parser (card, words, opinions, stages, lines, stories, `dlg`, grafts, `@` keys, `revision`/`state` headers); batch `setMantles`; `byOwner_`; `setCharacter` as an override; the layer order; habits by mantle; **migration 0066** (`live.mantles`, `live.mantle_lines`, `live.mantle_history` and its trigger, the binding columns, grants) and its rollback, applied to DEV after a dump to `Saved/Backups/`; `loadVoices` reading the rows, `loadVoice`, `voice.sync <npc>`, **`mantle.sync <id>` hot reload**, `--mantles-from DIR`; `mantle_check.py` (with `--one --world`), `talk_check --mantle`; **`tools/mantle_store.py`, `mantle_load.py` and `mantle_export.py`**; the test fixture `Tests/fixtures/mantles/` with the 10 luna tables ported | B3 and B4 budgets; `mantle_tests` parse; `test_mantle_load.py` (round trip byte for byte, refusals, stale revisions, history, `mantle.sync` keeps the old revision when the new fails) | 18 |
| P10 | Passing mantles on | P9 | the mantle ledger in `CareerState`; release in `died`; choosing in `tendBirths`; `ResidentRequest::mantle`; the voice row, relations and personality at birth; names and their checks (draft pools); re-drawn marks and echoes; the `makeRequestedResidents` mirror; arrival binding; pool health and `mantle_pool.py forecast`; newborns' honesty and temperament from the card (Q4); DM commands; **the Mantles panel** for Atlas and the DM (§17.10: card, items, problems beside the item, history and revert, "this mantle or this wolf only", Hear it) and its save endpoints through `mantle_store`; Atlas's column; `world_sync` and `world_package` learn the tables | `mantle_tests` with synthetic deaths in a small `Society` (no economy run); a harness world shows a handoff; `test_dungeon_master.py` (a bad save refused with its reasons, a good one queues `mantle.sync`); `test_world_sync.py` (an item edited on both sides is a conflict, on different items not) | 15 |
| P11 | The writer's kit and the pilot | P10 | `mantle_brief.py`, `mantle_index.py`, the writer's guide; **about 44 mantles at the richer size (about 2,900 lines): Saltwell, Fenshore, the 8 placed luna residents**; Saltwell and Mirebridge kits; loaded into DEV with `mantle_load --bind`; the export committed; a listen packet | G1-G4 and G6 on the pilot; U2 and W1 measured | 14 |
| | Slice end: a ticking 100-hour listen (Q6, reported); **the user listens; the format and the rows' shape are frozen** | | | | 4 |
| | **Slice 4: the window only** | | | | |
| P12 | Window-only plumbing | P11 | `residentsAnswerChat`; `overheard`; the emote and the hint; talk opens the window; `react.customer`, `welcomeAtInn` and `companionSays` converted; the companion tree's "What do you think?"; the storyline event from the window only; legacy lines removed; the client changes (§13.2) | W3 = 0; `dialogue_tests`; `game_tests` (introductions still learned) | 8 |
| P13 | The window's talk and typed words | P12 | `[does <move>]` choices and `answers` nodes; the hub rebuilt; Ask about...; `introduce`, `ask_name`; greetings by meeting and disposition; bye and thanks; day, hours, wanted, where-is; docs 52/54/55 nodes; grafts shown; the player claim store and knowledge choices; the ledger with players; `understand.json` and the 500-line corpus; unclear lines; the Mind in the window when connected, authored understanding otherwise and on failure (Q1); line rotation; `#way.*#`/`#refuse.*#` symbols | W2 gates; the hub never shows more than 9 | 12 |
| | Slice end: end to end in the client on a scratch server (`tools/scratch.sh`); a ticking 100-hour listen (Q6, reported) | | | | 4 |
| | **Slice 5: writing every wolf** | | | | |
| P14 | Mantle batches (39 sessions, §14.3) | P11's freeze, P13 | about 729 mantles at the richer size (about 47,800 lines), loaded and bound as each batch lands, the export committed with each | G1-G6 per batch | 156 |
| P15 | Spares, kits and mark banks (5 sessions) | P10 | 88 spares at the richer size (about 5,600 lines), written to each dialect's honesty mix (Q4); 16 stage kits; the mark banks | pool health green for 5 forecast years; `mantle_check --pools` | 20 |
| P16 | The final listen-through | all | a ticking 100-hour listening run (Q6, reported); the second blind ratings; review and fix, over 2.3 times the mantle text; `mantle_export --check` and the final export committed; the ambient Mind path deleted | every hard gate; H1 3.8+ | 10 |
| | **Total** | | | | **373** |

About **179 hours of engine, tools, pilot and checks** and **194 of writing** (P8 18, P14 156, P15 20): **373 in all**.
Against the first draft's 257 (159 and 98), the user's answers add 116: the richer size (Q3) adds 96 to the writing
(P14 +88, P15 +8) and 6 to the pilot (P11); the database home (Q2) adds 11 (P9 +6 for the tables, the loader, the
export and the hot reload; P10 +5 for the Mantles panel, the save checks and sync); the ticking fixture run (Q6) adds
1 (P0); the larger final review adds 2 (P16). The ticking slice-end listens run in the background and add no hours.
The user's own time: the pilot's packet, the first packet of each dialect, an hour at each slice's end, the two blind
ratings, and tuning the discourse symbols and name pools by ear.

## 19. Risks

1. **The writing is large** (the user's richer size, Q3: about 56,300 mantle lines, roughly 690,000 words, and 2,100
   shared rules, over 44 sessions) and could drift in quality or sameness across that many sessions. *Against it:*
   the pilot freezes the format with the user; the town index and uniqueness gates stop neighbours echoing each other;
   U2 measures distinctness; every batch passes G1-G6, so every line is checked by machine while people read samples
   (§14.4); the extra length goes into breadth (more openers, answers, stories), not into each mantle's share.
2. **Rebuilding `converse` and the director breaks what works.** *Against it:* Phase 0's split and Phase 5's topic move
   are behaviour-neutral and proved by golden transcripts before anything changes; old content keeps loading.
3. **Load, pick time and memory with 1,000 mantles at the richer size** (measured: 22.4 s and 0.49 ms with today's
   `setCharacter`; about 50,700 items for living wearers, about 115 MB at today's entry size). *Against it:* batch
   loading, per-owner buckets and interned strings in Phase 9, benched against B1-B5 (B3 3 s, B4 150 MB) before any
   batch lands.
4. **Grading our own homework**: metrics computed from the tags authors wrote. *Against it:* the shuffled ablation must
   separate (§15.5), text-only checks don't trust tags, and people rate blind.
5. **Coherence makes talk predictable.** *Against it:* fit first, but tiers, temperaments and unheard preference inside
   the fit; the mantle share is capped at 35% of a conversation's lines.
6. **Lines asserting untrue things about real residents**, contradicting each other across days. *Against it:* third
   parties only through `x.*`, recorded claims or person facts; `truth.json` statically and per line; T2-T4 hard gates.
7. **Reuse feels uncanny** (a dead wolf's catchphrase from a pup while kin still grieve). *Against it:* a game year's
   rest, marks drawn again for anyone still remembered, no reuse into the dead's kin's household except as an echo,
   which is written to be noticed.
8. **Pools run dry where births concentrate** (Coldwater runs about 2.7 a year short). *Against it:* 88 spares,
   neighbouring pools, the health warning, and a seed-row birth rather than a delayed one.
9. **Typed words misunderstood** (Façade's understanding failed about 30% of the time). *Against it:* a choice on show
   is matched first, in-character "didn't follow" lines bring the choices back, and the 500-line corpus is a hard gate.
10. **Goal 7 removes things players use** (typing to a resident, introductions by chat, storyteller quests counted by
    naming). *Against it:* the hint, Introduce in the window, the window counting for storylines, and `game_tests`.
11. **Sims change through voice traits** (honesty chooses thieves; quarter feeds the Market Board). Newborns inherit
    their mantle's honesty and temperament (the user, Q4), so a `sly` mantle's pup can turn thief at 16, more than 2
    real years away. *Against it:* placed residents' honesty and quarter never change; a reused mantle carries its dead
    wearer's honesty and births are one for one with deaths, so reuse keeps the mix; spares are written to their
    dialect's mix and gated (`mantle_check --pools`); births stay realm-wide (Q5). Long sims will show it, year-long
    ones barely.
12. **The checkpoint grows** (about +2 MB on 1.3 MB). *Against it:* caps, least-recently-used eviction,
    `--no-talk-ledger` for fast economy sims.
13. **Measurement runs that tick are simulations.** The user allowed the ones this plan names (Q6): one ticking
    25-game-day run in P0 and a ticking 100-hour listen at each slice's end. *Against it:* frozen runs stay the default
    for everything else; no other ticking run without asking; each one reported with its seed, length, speed and wall
    time.
14. **Live edits drift from what was reviewed, or break a running server** (the price of Q2: anyone with Atlas or the
    DM can change a mantle's text at any moment). *Against it:* every save passes the game's loader and `mantle_check`
    first (§17.3) and a stale revision is refused; the history names who changed what and reverts it; the server keeps
    the loaded revision when a reload fails and skips a mantle that fails at start, its wearer falling back to the
    library; `mantle_export --check` runs at every batch, slice end and import, and the export is committed, so a DM's
    edits show as a git diff.
15. **Two copies of the text** (the rows and the export) could be mistaken for two truths. *Against it:* the rows are
    the only truth a database world reads; the export is written from them and read by no server there; agents' drafts
    go in through `mantle_load`, never past it; `--check` reports any difference.

## 20. Example conversations

Hand-composed to show the designed behaviour: no code produced them. The residents' posts, origins, accents, traits,
quirks, households and kin are read from `ratw_ridgemere3` (and the Ridge-Fern marriage from the checkpoint); the world
events each example needs are **assumed** and marked. Mantle ids are illustrative. Each line shows the moves it makes,
what it owes, and where its words came from.

### 20.1 Coldwater: two broad voices, a stepwise turn to the road, a refusal and a callback

```
Coldwater Stores, a spring morning (here.time=day, here.outdoors=fog). Frames: at work (Myrtle), acquaintances, customer.
Wick Marshwade: carter at Coldwater; from Coldwater, broad; nervy, chatty, merry; hums old marching songs. Mantle cw063.
Myrtle Stillford: shopkeeper at Coldwater Stores; from Coldwater, broad; calm, warm, content. Mantle cw017 (§7.9).
Assumed: both believe there's a bandit camp up the High Road (Myrtle .55, from a carter; Wick .70, from another);
the ledger holds a note from 4 days ago (Myrtle|Wick, road, Wick worried, reassured); the soap works have a load for
Ridgemere.
Agendas: Wick, a complaint (the mist), her run (Riverday), a worry (the camp). Myrtle, a request (her candles; she
deals in them, Wick is a carter, bandits are near), a callback (the note).

 1 Wick:   Still water, still water, Myrtle. Can't see the jetty for mist, and I've a cart to load in it.
           greet.broad + tell.complaint(weather)    owes react.complaint        [cw063; here.outdoors=fog]
 2 Myrtle: Calm water, Wick. It'll lift by noon; it always does. You're for the High Road in it?
           greet.return + reassure + ask.plan        owes answer.plan            [shared greet; cw017]
 3 Wick:   Riverday, if the soap works have the crates ready. You'll have heard about the camp up there.
           answer.plan + pivot(caravan > road > bandits) + pre-announcement     owes react.news [shared pivot line]
 4 Myrtle: I heard. A carter had it at my counter yesterday. Two days up the High Road, she said.
           react.news.known (other.knows=yes; conv.my_source=carter)            [shared known line; #place.words#]
           Corroborated: Myrtle .55 -> .73. No new belief for Wick.
 5 Wick:   That's the one. Took a whole wagon and let the driver walk home. Deep and dark, I don't sleep for it.
           answer.detail + tell.feeling(fear)        invites sympathise|reassure [cw063; temper=nervy; oath!]
 6 Myrtle: You said the same last week, and you came home with every crate.
           reassure (pair.recall.road=worried, 4 days)                           [cw017]
           Will you take a box of my candles up the road with you?
           request {request=carry}                   owes grant|decline          [cw017; a two-line turn]
 7 Wick:   I would. Truly I would. Only the cart's spoken for, and if I'm stopped I'd sooner lose the works' crates
           than your candles. Next run, if it's quiet.
           decline, dispreferred: hedge + account + remedy     owes accept_decline [shared decline family]
 8 Myrtle: Next run, then. I'll not have you holed for a box of candles.
           accept_decline (preferred, warm)          owes nothing                [cw017; Coldwater "holed"]
 9 Wick:   Well. The crates won't load themselves. Fair winds, Myrtle.
           preclose + farewell (self.working=yes)    owes farewell.return        [shared closing; #farewell#]
10 Myrtle: Fair winds, Wick. Mind the mist on the hill.
           farewell.return; ends
After: the ledger notes {weather, road, bandits; reassured; carry=declined, open: next run, 14 days}; Myrtle's belief
corroborated; +0.5 affinity each way (sympathy and the kind acceptance); nothing new spreads, because both knew.
```

### 20.2 Saltwell: three voices; the victim speaks as the victim; the subject's husband joins

```
The Oasis Inn's taproom, Saltwell, noon (here.outdoors=clear, hot). Frame: host and guest.
Oriel Marshditch: keeps the taproom; from Saltwell, light; even, reserved, frank, ambitious, poor; never minds wet feet. Mantle sw004.
Thistle Stonepick: keeps Saltwell Stores; from Saltwell, light; calm, reserved, dry humour, rich; collects smooth stones. Mantle sw002.
Ridge Eelbank: salt raker at Saltpan, at the next bench; from Saltwell, light; even, cold, guarded. Married to Fern
Merewick; Ridge, Fern and Oriel share cottage 1 (Oriel and Ridge are pair.band=housemates after defect 3's fix). Mantle sw011.
Assumed: on Hearthday someone tried Thistle's till; the Watch has put Fern's name up for it. Thistle holds
theft.attempt (subject Fern, object Thistle) at .9 and the warrant; Oriel holds neither.

 1 Oriel:   Peace on you, Thistle. Sit in the shade; the jar's cool. You've a face like a dry well.
            greet + offer (host) + check_in          owes accept, answer.check_in [Saltwell #greet#; sw004]
 2 Thistle: I'll sit. Someone tried my till on Hearthday, while I was in the back. The Watch has a name for it now.
            accept + answer.check_in + pre-announcement   owes ask.detail        [sw002; claims.json, victim's view]
 3 Oriel:   Stars above. Whose?
            react.news.surprise + ask.detail          owes answer.detail          [Saltwell #surprise#]
 4 Thistle: Fern Merewick's. Her name's up by the well.
            answer.detail; asserts the warrant        owes react.news             [claims.json, "they" view]
            Ridge: idle, 2 tiles away, hearing .9, the subject's husband: a stake. He joins.
 5 Ridge:   Her name's up because a clerk wrote it. Nobody saw her take a grain.
            join.defend; denies                       owes apologise|insist|concede [sw011; warmth=cold, so curt]
 6 Thistle: Ridge. I didn't see you there. I said what the Watch says, and not a word of my own.
            apologise, dispreferred: an account, no flattery   owes accept_apology|decline_apology [shared]
 7 Ridge:   The Watch's word is enough, in a place this size.
            decline_apology (cold)                    owes nothing                 [sw011]
 8 Oriel:   It is so. And a board is not a judge. Fern's under my roof, Thistle; I'd have heard her come in with a
            till's worth of silver.
            agree (with Ridge) + react.news.doubt with an account (housemate of the subject) [sw004; #agree#; #money#]
 9 Thistle: Then I'll let the Watch do its own counting, and keep her slate as it was. Salt keeps, and so does a promise.
            concede + saying! (sw002's mark)          owes nothing
10 Ridge:   ...My thanks. Go in peace.
            thank + leave (offended; budget spent)    he walks out                [Saltwell #farewell#]
11 Thistle: Sand in my teeth. I'd not have said it with him at my back.
            tell.feeling, tone serious (light is blocked after cross)  invites reassure [Saltwell #annoyed#]
12 Oriel:   Shade is for sharing, and so's a cool cup. Drink, and say less of it.
            give.advice + preclose; saying! (Oriel's own, a different one from Thistle's)  owes accept_preclose
After: Oriel believes the warrant weakly: .9 × (0.5 + 0.4 × .30) = .56 from Thistle, then × (1 − 0.3 × .50) = .47
after Ridge's denial (trust numbers assumed). Ridge → Thistle −2 affinity. The ledger notes Thistle|Ridge "soured"
with an open thread, so within 7 days the `mend` goal can open their next meeting ("Ridge. About Hearthday...");
Oriel|Thistle "disagreed" on Fern.
```

### 20.3 The window: a player and Myrtle Stillford, the same morning

```
Kestrel, a player wolf, has never been introduced to Myrtle (pair.named=no). Kestrel walked in 40 s ago: react.customer
was an emote ("Myrtle looks up from the counter.") and here.came_in=yes. On the road Kestrel overheard two carters on
the camp, so Kestrel's claim store holds bandits.camp. Kestrel opens Talk.

[Myrtle Stillford, shopkeeper at Coldwater Stores]                                   face: warm
Myrtle: Still water, still water. Come in out of the mist; the floor's wet, mind. What's wanted?
        (cw017 dlg.resident.hub [here.came_in=yes here.outdoors=fog]; broad, so #greet.broad#)
  1 Let me see your wares.   2 Heard any news?   3 Ask about...   4 I heard there's a camp on the High Road.
  5 I'm Kestrel.   6 Goodbye.   [Say something else...]
> 5  (do introduce: noticeIntroduction; pair.named=yes)
Myrtle: Kestrel. I'll keep it. Myrtle, of the Stores; not that the sign leaves you guessing.
> 4  (unlocked by player.knows=bandits.camp; does tell.news)
Myrtle: So they're saying. I had it from a carter yesterday, and Wick Marshwade the same. Two days up the road.
        (react.news.known; Wick named through #x.name#, bound from Myrtle's latest ledger note on the topic)
  1 Is the High Road safe for a lone wolf?   2 Back to the rest.   3 Goodbye.
> 1, rewritten by the player as "Would you walk the High Road alone, these days?" (their own words: roleplay XP)
        (does ask.opinion:road)
Myrtle: The High Road? I'd not send my own candles up it, and I've asked. The Watch walk that road like it bites them.
        (cw017's road opinion [self.was.carry=declined], from Example 20.1, then her Watch opinion)
> types "do you really swim in that lake?"    understood as ask_about:quirk (swim), confidence .91 -> graft cw017.swim
Myrtle: Every morning till the ice says no. Mam swam it till she was grey. It clears the head. You should try it.
  1 Not for all the coin in Coldwater.   2 Where do you go in?   [Say something else...]
> 2
Myrtle: Off the end of the jetty, before the boats go out. Come at dawn and I'll show you, if you've the fur for it.
        (invite jetty dawn 3d: the world checks whether Kestrel comes)
> types "can you read the stars for me?"    below the threshold -> dlg.resident.unclear
Myrtle: I'm not sure I follow you, neighbour. Ask me plainer, and I'll answer plainer.
> Goodbye
Myrtle: Calm water, Kestrel. Mind the step; it's slick.
Two days later, if no dawn visit was seen:  "Calm water, Kestrel. No sign of you at the jetty. Thought as much."
Later, to Wick, NPC to NPC: "A stranger called Kestrel was asking after the High Road this morning. Alone, if you please."
Had Kestrel typed "morning, Myrtle" in chat instead, Myrtle would only have glanced up (an emote), and Kestrel would
have seen "Talk to Myrtle (T)".
```

## 21. The user's answers (2026-11-16)

All six questions are settled; they were the ones that changed what gets built.

1. **The window's "Say something else...".** *Answered:* "It should be answered by live AI but ONLY if the AI is
   plugged in, otherwise it should be answered by the pre-generated responses." So the live AI answers when it is
   plugged in, and otherwise the pre-written responses (authored understanding, §13.5) answer. See §13.6.
2. **Where mantles live.** *Answered: world database rows, not repository files*: mantle text lives in the world
   database, editable live in Atlas and the DM tool. (The draft had recommended files, with the database holding only
   who wears which and the per-NPC overrides.) Built as §7.9, §7.12 and §17 describe: `live.mantles`,
   `live.mantle_lines` and `live.mantle_history` (migration 0066), checks on every save (§17.3), `mantle.sync` hot
   reload (§17.2), `mantle_load` for agents' drafts (§17.4), the committed export for review (§17.5), a committed
   fixture for tests (§17.6).
3. **How much per wolf, and who writes it.** *Answered: richer* (about 0.7 million words, about 2.3 times the
   recommended writing), *written by Claude agents in sessions, no paid model calls.* (The draft had recommended about
   40 lines for the ~220 public roles and 24 for the rest, about 98 hours of writing, and offered a leaner size or the
   paid story model through `tools/voice_tables.py`.) Built as §9.3's tiers of 92, 55 and 64 lines, §14's 39 + 5
   sessions, and §18's 373 hours.
4. **Newborns' honesty.** *Answered: newborns inherit their mantle's honesty and temperament* (the draft's
   recommendation, against always starting "plain"). See §9.5 and §12.5.
5. **Births toward the dead's region.** *Not taken up by the user*, so the recommendation stands: births stay
   realm-wide and the pools absorb the imbalance (§12.3), rather than preferring a couple in the dead wolf's dialect
   region, which would keep pools balanced but change the economy sims.
6. **Listening runs that tick the world.** *Answered: yes, as planned*: one ticking 25-game-day run in Phase 0 to
   record the fixtures, and ticking 100-hour listens at slice ends (world simulations), each run reported. No other
   ticking runs (§15.1, §15.8).

## 22. What I could not verify

- **Real server speech.** `Saved/Logs/npc-voices.jsonl` has no words, so every "today" figure rests on the survey's
  harness (random pairs in a cell rather than the director's pick, coarse ticks, no 32-entry cap), and the targets are
  set against it. Phase 0 re-measures with the real director.
- **The cost of building facts per beat** (`talkSpeaker`, `peopleOf`): not profiled. The load and pick timings in §16
  were re-run this session on synthetic rules; real mantle rules have more criteria.
- **The agents' writing rate** (about 300-320 gated lines an hour, about 1,200 lines a 4-hour session) is an estimate
  drawn from the three designs' own estimates, not a measurement, and the richer size's longer stories and opinions
  may write slower; P11's pilot measures it, and P14's sessions are re-planned from that if it's off by more than 20%.
- **Ridgemere 3's births and deaths.** It has never been simulated and has had none; the pools use the Gompertz
  expectation on today's ages, with a margin for violent deaths, which are unmeasured.
- **Fixture mode**: whether a checkpoint file can be loaded into a scratch world needs a small option that doesn't exist.
- **The examples' world events** (the camp beliefs and their sources, the ledger note, Fern's warrant, Thistle's till,
  the soap works' load, trust values) are assumed. The residents' facts, households and the Ridge-Fern marriage were
  read.
- **The client line numbers** (`state.ts`) come from the trees survey; I didn't read the client.
- **The router's full intent list**, and how many of the 500 corpus lines today's router already understands.
- **Doc 20's push semantics** for the new tables (that `world_sync`'s row-by-row comparison handles items keyed
  `(mantle_id, key)` as assumed), and the Talk panels' room for a 92-item mantle editor.
- **The database's costs**: reading about 50,700 item rows at start, a one-mantle `mantle.sync`, and a save's checks
  (a `talk_check` process plus `mantle_check` reading the dialect's other mantles, about a second) are estimates from
  §16's parse timings, not measured; B3 and the save's latency are measured in P9.
- **About 44 Coldwater fishing lines**: a word search finds 46 lines in `scenes/regions/coldwater.talk` matching fish,
  net, catch or boat (a few may be false matches, and some may be fine with a `self.job` gate); Phase 1 reviews each.

## 23. Phase 0, as built (2026-11-16)

**The split.** `Game::ambient(dt)` is now `ambientSpeak(dt)` (what is due said; broken exchanges ended) and
`ambientStart(listeners)` (the director's pick, written and begun), with the five-second look between them; `ambient`
is public, and `setTalkListeners` gives it listeners nobody plays (they hear as connected players do: `hearersOf`). It
is behaviour-neutral: transcripts recorded before the split and replayed after it match word for word (12,344 lines:
the talk town's 60- and 2,000-exchange runs, Ridgemere 3's L1 and L2 for 6 hours frozen, and 0.3 hours ticking), and
the ctest `talk_golden` keeps the 60-exchange one (`Tests/fixtures/talk_golden/town-60.jsonl`, over the library at
a46cbdac through `--library-from`; regenerate with `--bless` when an engine change means to change what is said).

**Observation only, never choosing:** `talk::Said::info` (each line's entry, alternative, the grammar layers whose
words filled it, the habits it used), `Game::onAmbientLine` / `onAmbientEnd`, `Game::checkpointText` and
`talkCarry`/`talkResume`, `Options::checkpointFile` (a scratch world from a fixture's checkpoint, no journal replayed),
`World::advanceClock`, `replayPlace`, `restage`, `playerStage`, `placeKindOf`, `ambientPacing`, `Society::replayTask`.
The cell prefetcher is shared by a second world in one process (the simulator's restarts). `--log-talk-words`
(`Options::logTalkWords`; `tools/server.sh` passes it for dev-kind worlds, `RATW_TALK_WORDS=off` leaves it out) adds
records of kind `words` to `npc-voices.jsonl` (words, to whom, entry, alternative, layers, habits, `moves: []`) for
exchanges, barks, reactions and window lines; `tools/ai_cost.py` skips them (`game_tests`: theLedgerKeepsWordsWhenAsked).

**The tools.** `Tests/talk_sim.cpp` (frozen, `--fixtures`, `--tick`, `--record`, `--freeze-at`, the ten profiles and
`town`, `--ablate shuffle`, `--library-from`, `--golden`/`--bless`); `tools/talk_score.py` (the scorecard, every metric
beside the shuffled control, gates by ratchet: a hard gate the baseline passed must pass, an open one may not worsen, a
soft metric may not regress 10%); `tools/talk_card.py` (every profile in both modes, in parallel, scored and diffed
against the baseline; `--rescore`, `--most-heard`); `tools/talk_listen.py` (the listen page with marks, and a Markdown
sheet); `tools/talk_rate.py` (the blind packet and H1/H2); `tools/talk_town.py` and `Tests/fixtures/talk_town/` (3
places, 40 residents, Ridgemere, Coldwater and Saltwell speech, 29 small homes); the ctest `talk_scorecard_quick` (2,000
frozen exchanges in the town with the control, 3 s, against `Tests/fixtures/talk_scorecard_quick.json`).

**Choices made while building** (decided): profiles listen only in public places (never a resident's home: the library
calls a cottage "work", so a resident's home cell counts as home); places are ranked by residents with someone within
talking distance, then by residents free to talk; L2 moves every game hour to a public place where two or more stand
together; L10 sits in Ridgemere's busiest tavern from noon to midnight (two real hours) each game day for 30 days (25 in
fixture mode); frozen runs start from the fixture's noon of its first whole day (`--freeze-at 28.5`), so every phase
freezes the same moment; the control replaces each line after the first with one said at the same side of another
conversation on the same topic by a speaker of the same dialect where one exists. T2's lexicon is provisional (weather,
time of day, kin, fishing; "the Rain Hall" is a place) until Phase 1's `truth.json`. A diagnostic, X1 (a reply sharing
a content word with the line before), shows the control discriminates (27% against 1.6%).

**The ticking run** (Q6; the only one): `talk_sim --world ridgemere3 --tick --days 25 --record
artifacts/talk/fixtures/p0-r3-25d`, seed 1 (the ticking world is deterministic from its save; the seed is unused),
from DEV's save at day 27.85 to day 52.85: 25 game days, 7,200,000 ticks of 0.05 s, 360,000 world seconds, 1,233 s of
wall time (20.5 minutes; speed 292), 754 MB. It wrote 27 checkpoints (one each game midnight, about 0.95 MB each) and
the track (where everyone was and what they were doing every 120 world seconds, only what changed: 0.8 MB). The
checkpoints hold DEV's state (not only residents), so the fixture stays out of git (`.gitignore`); `fixture.json` is
committed.

**The baseline** (`artifacts/talk/scorecard-baseline.json`, frozen, and `scorecard-baseline-fixtures.json`; the
transcripts in `artifacts/talk/runs/p0-baseline/`, 7.7 minutes for all twenty runs). Values with the shuffled control
in brackets:

| | Fixtures (13,021 conversations, 39,073 lines) | Frozen at noon (26,142, 66,651) |
|---|---|---|
| R1 first identical conversation, earliest listener | 2.2 h (2.3) | 0.42 h (0.14) |
| R2 first repeated opener | 2.2 h | 0.14 h |
| R3 distinct openers per 3,000 | 2,170 | 467 |
| R5 collisions per 100 h | 189 (208) | 909 (983) |
| R6 the most-spoken line | "Good.", 3.6% | "A day like any other.", 13.2% |
| R7 distinct replies per 1,000 | 53 (49) | 34 (32) |
| C1 questions answered (the "?" heuristic) | 96.1% (86.5%: within 30%, so it doesn't measure coherence; Phase 2's owed answers replace it) | 93.1% (87.2%) |
| C4 tone clashes per 1,000 beats (lexicon) | 0.03 (0.36) | 0 (0.15) |
| C6 the same entry or question twice, per 1,000 | 0 (40) | 0.12 (34) |
| C7 lines per conversation | 3.0, p90 4, 88% three lines | 2.6 |
| C3 ended by a farewell or a break | 0.8% (99.4% "nothing fits") | 0.1% |
| T1 raw ids per 1,000 lines | 13.4 | 0.1 |
| T2 presupposition failures per 1,000 | 0.20 (0.61) | 0.02 (0.09) |
| T3 the teller naming themselves, per 1,000 | 3.3 | 0 |
| K1 everyday topics / K2 largest topic | 3.8% / gossip 40% (crime 30%) | 24% / event 42% |
| K3 the same news to the same resident twice | 2,284 | 109 |
| D1 light speakers' lines with a dialect word | 0.21% | 0.23% |
| D3 dialect guessed from 20 lines | 67% (72%) | 13% (17%) |
| U2 look-alikes told apart | 46% (43%) | 39% (38%) |
| B5 exchanges simulated a second | 1.5 (restarts dominate) | 68 (the town: 1,300) |

Open hard gates at the baseline (each phase closes its own): R1, R5, T1, T2, D1, K3 and B5 in both modes, C4 and T3
in the fixture's, C6 in the frozen. Passing: C1 (heuristic) and D2 in both, C6 in the fixture's, C4 and T3 frozen. Frosthollow and Oakhaven (L8, L9) heard nothing in
100 hours in either mode: nobody but a keeper is ever in their public places, and the only place two stand together is
a cottage. Frozen at noon and the fixture differ sharply (topics, repetition), which is why both are kept.

**For Phase 1.** Run `python3 tools/talk_card.py --fixtures <the worktree's artifacts/talk/fixtures/p0-r3-25d> --out
artifacts/talk/runs/p1 --most-heard ...` with `RATW_DATABASE_URL` set (as `world_db.py conninfo ridgemere3 --role
game` prints it in the main checkout); it diffs against the committed baselines and exits 1 on a broken gate. Copy the
fixture if this worktree goes. Truth fixes will change what is said, so `talk_golden` needs `--bless` and the quick
card may need re-blessing (say why in the commit). The most-heard list is `artifacts/talk/most-heard-p0.tsv` (566 of 772
residents heard; Orm Blackwood, Rosamund Tarridge and Wenna Tarford first), the listen sheet
`artifacts/talk/listen/p0-baseline.md` (and `.html`), the blind packet `artifacts/talk/rate/p0/` (today's against
the control; no old library yet).

## 24. Phase 1, as built (2026-11-16)

Branch `r3-talk-p1`, on Phase 0. Defects 1-12, 14 and 15 of §3.5 are fixed; 13 (dead-ending answers) stays with Phases 2
and 8, 16 (the quadratic load) with Phase 9.

**Claims (defects 1, 2, 7, 9).** `Core/RatwClaims.{h,cpp}` reads `Data/Talk/claims.json` (15 kinds: death, marriage,
theft, theft.attempt, assault, violent, the two unseen ones, warrant, the two promises, prowling, newcomer,
bandits.camp, bandits.robs), each with its points of view (`they`, `victim`, `to_victim`, `self`, `kin`), the old
`topic.claim_kind` it answers as (`family`), the topic it is talked of under and the free words that are it (older
saves, the Watch). The talk library's loader sets it (a library from before Phase 1 leaves it as it was; `talk_sim
--library-from` copies the engine's own `claims.json` and `truth.json` in). `struct Belief` gains `kind`, `object` (the
victim) and `place` (a cell), saved in the checkpoint (and so in `game.beliefs`' rows). `World::believe(const Belief&)`
is the structured form; the old one fills the structure through `World::claimOf` (the kind from the words, the victim
from the incident or the name in the words, a camp's cell from its words), and `fillClaims` does it for a whole save
when it is restored. The camp beliefs are made structured (`RatwRoads.cpp`'s two), and the carters' and the daily
gossip's tellings carry the structure with them; a carter's source is "a carter from Coldwater", never a market's id.
`World::placeWords(cell)` says where a cell is as a resident would ("in the Greenholt, out by Glasswater", "in the
south-west of the Rainward Hills", "out by the Kestrel lead and silver mine") and `placeNear` names the spot for a line's
"out by #topic.where#". Gossip is said in claims.json's words from the teller's point of view: `topic.said`,
`topic.claim` (the words after the subject's name, for the 70 lines written "#topic.subject# #topic.claim#"),
`topic.victim`, and `topic.claim_kind` from the kind (`claims::family`, now the one shared by the ambient talk and the
window, with `bandit` and `newcomer`). A claim with no kind in the table, or whose wording can't be filled, is never
spoken; no fact a line can say holds an id (`claims::rawId`), and `ambientTopic`'s `name()` no longer falls back to one.
A camp is talked of on the road's topic (`talk.bandits`, `topic.kind=camp`), where three new rules say the claim in its
own words (`h7_rd_open_camp_heard`, `_merchant`, `_guard`); the listener believes it as gossip was believed. Nobody is
told their own misfortune as gossip and no victim tells it of themselves as gossip (the one robbed tells it as their own
news, naming a traveller only as they know them), and nobody gossips about their own kin; the window's gossip keeps the
same rules. The gossip answers that were keyed on the claim's words are keyed on its kind (`n7_ga2_warm` for victims,
newcomers and the rest, `n7_ga2_guard` for culprits, and the death and wedding answers). `topic.news` and `topic.said`
are the topic's own words (`AmbientTopic::said`), never the day's sentence a topic with nothing to tell put first, and
`mind::firstPerson` makes the verb agree ("I have", never "I has"); the Mind is never told a claim's id either.

**The rest.**
- **Housemates (3)**: `pair.band=housemates` for a shared home without a kin tie; the family topic is kin's only, and
  housemates get the home topic at the family topic's score (what they share is the house); `kn_sib_inheritance`
  ("Father's tools") needs `pair.kin=siblings`.
- **`repeat` per concept (4)**: decided by the concept asked at pick time, not by any of an entry's concepts.
- **Every beat heard (5)**: each listener hears every rule of a chain (`said.chain`), not only the opener, so a heard
  answer gives way to a fresh one of its tier. Replies still take the best tier: fit then tiers is Phase 3's.
- **The cooldown store (6)**: `said:<id>` cooldowns and onces live in `Game::talkSaid_`, which no cap evicts, saved
  in the talk memory's own shape (an older server reads them as memory) and carried by `talkCarry`.
- **Untrue lines (8)**: `here.outdoors` (the weather in the street a building opens onto), `here.hour` and
  `here.fishers` (a town's residents who fish or sail, once a game day) are new facts. The fog bells need
  `here.outdoors=fog`; the indoor "Hear it coming down out there?" needs `here.outdoors=rain|storm`; Coldwater's homes
  hold one boatman, so its twelve scenes that take a fishing town for granted need `here.fishers>=3`, four of its
  alternatives were rewritten true ("The Glass Lantern keeps its fire in all night for the carters") and its mist scene
  needs real fog; Ridgemere's fish-hall dawn needs its fishers too.
- **News (10)**: births (the parent's, so the 14 `topic=life topic.kind=birth` scenes and `world.birth` are reached at
  last) and arrivals from the Country are the town's news; `news_` and `townNews_` are saved (`talkNews` in the
  checkpoint).
- **`pair.named` (11)** between residents: familiarity 10 or more, kin, housemates or workmates.
- **The priest (12)**: the nearest church to home, the priest of one's own district: Ridgemere 3's 772 residents now
  have five priests (315, 137, 131, 15, 1; 173 live where there is no clergy) where one had 598. `#self.leader#`: one's
  own House's head, else the House of the district one lives in, else a House of the town (22 residents, from 11).
- **The punctuation join (14)**: a value that ends its own sentence (". " or ";") before the line's comma or stop gives
  up its mark; the 169 greeting rows whose "I am" was joined to a verb are never said (until mantles replace them).
- **`heed` (15)** ignores a promise the Mind reports from the window's free talk.

**Truth (§7.8).** `Data/Talk/truth.json`: 12 rules (rain and fog now, rain heard, snow, the night and morning
greetings, spouse, late spouse, children, a parent's things, a fisher's own words, a fishing town's), what a concept
holds of itself (`react.rain` is said in rain) and what an id looks like. `tools/talk_truth.py` reads it;
`tools/talk_lint.py` checks every line of the library and the trees statically (its criteria, tags and say[...] must
imply what its words need; no id in any line) against `Tests/fixtures/talk_lint_baseline.json`: the 20 findings left
(6 "my son/daughter", 5 rain, 3 "good morning", 3 "my nets/catch", 1 each of fog, goodnight and "my wife") were counted
and frozen there, and a new one fails ctest `talk_lint` (with `talk_lint_rules`, its own tests). `tools/talk_score.py`'s
T2 runs the same rules on every simulated line against the moment's and the speaker's facts (P0's provisional lexicon
is gone), and `talk_sim` records every `pair.*` fact for it. T2 was frozen again in both baselines with these numbers.

**Also.** Ten written scenes ended on a question nobody answered ("Can I go talk to them?"): each now has its answer
(C1 frozen 93.1% to 98.9%). `World::districtHouse` walked the doors for every speaker's leader and slowed the
simulator three to seven times: it is cached by cell, found again when the doors change and each game day.

**The scorecard** (`artifacts/talk/runs/p1/`, every profile in both modes over P0's fixture, 13,018 and 26,142
conversations; P0's baseline beside it, the shuffled control in brackets):

| | Fixtures, P0 | Fixtures, P1 | Frozen, P0 | Frozen, P1 |
|---|---|---|---|---|
| T1 raw ids per 1,000 lines | 13.4 | **0** | 0.1 | **0** |
| T3 the teller naming themselves, per 1,000 | 3.3 | **0** | 0 | **0** |
| T2 presupposition failures per 1,000 (truth.json from P1) | 0.20 (P0's lexicon) | 0.41 (0.95), frozen | 0.02 (P0's lexicon) | 0 (0.05), frozen |
| C1 questions answered | 96.1% (86.5) | 97.2% (84.3) | 93.0% (87.2) | 98.9% (86.7) |
| C4 tone clashes per 1,000 beats | 0.026 | 0.026 | 0 | 0 |
| C6 an entry twice in a talk, per 1,000 | 0 | 0 | 0.117 | 0.119 (the same 3 talks) |
| C3 ended by a farewell or a break | 0.83% | 0.92% | 0.084% | 0.068% |
| R1 first identical conversation | 2.2 h | 2.2 h | 0.42 h | 0.42 h |
| R3 distinct openers per 3,000 | 2,170 | 2,150 | 467 | 469 |
| R5 collisions per 100 h | 189 (208) | 185 (217) | 909 (983) | 926 (1,003) |
| R6 the most-spoken line | 3.6% | 3.5% | 13.2% | 13.2% |
| R7 distinct replies per 1,000 | 53 | 58 | 34 | 35 |
| K1 everyday / K2 largest topic | 3.8% / 52% | 3.8% / 50% | 24% / 42% | 24% / 42% |
| K3 the same news twice | 2,284 | 2,259 | 109 | 109 |
| D1 light speakers' dialect words | 0.21% | 0.22% | 0.23% | 0.24% |
| D3 / U2 | 67% / 46% | 69% / 46% | 13% / 39% | 13% / 40% |
| B5 exchanges a second | 1.5 | 1.5 | 68 | 59 (load; 72 in an earlier run) |

`talk_card` still exits 1, on open gates by noise: C4 (fixtures) and C6 (frozen) are the same counts over a few more or
fewer lines, B5 is the machine's load (the same build measured 72 the run before), C3 (frozen, soft) is 18 talks against
22 of 26,142, and R5 (frozen) rose 1.9%: every beat heard sends a place's speakers to the same unheard price answers,
which the collision filter (§8.12, Phase 4) is for. The phase's own gates hold: T1 = 0 and T3 = 0 in both modes, T2
counted and frozen. The golden transcripts were blessed again (every beat heard changes which scene a listener hears
next) and so was the quick card (its D1 moved from 0.425% to 0.413%: two lines of about 5,000). Tests: the full ctest
(the database tests skipped without a test database), with new checks in `ambient_tests` (claims in their own words,
camps, victims, kin, births, news across a restart, housemates), `talk_tests` (the punctuation join, `repeat` per
concept), `dialogue_tests` (`pair.named`, `here.outdoors`) and `test_talk_lint`.

**For the slice end and Phase 2.** The ticking 100-hour listen and the full review are the slice end's. The lint's 20
findings and T2's 0.41 are the lines Phase 7's conversion (tags) and Phase 8's writing should take to zero; the
`em_rm_bd_camp` "In this rain, even" and the caravan "my son/daughter" lines are most of what is heard. Replies still
take the best tier (Phase 3). `claims.json` is the place for Phase 4's point-of-view lines (`victim`, `to_victim`,
`self`, `kin` are written but only `they` and the victim's own news are spoken yet).

## 25. Slice 1's end: the full suite and every profile (2026-11-16)

Branch `r3-talk-s1`, on Phase 1. `origin/ridgemere2` had nothing new (it was still at a46cbdac, Phase 0's base), so the
merge was empty. Everything was built in one Release build and run:

- **Core**: the full ctest, 97 of 97, with the eight database suites (`*_pg`, `pg_client`) run against the local
  PostgreSQL this time, not skipped.
- **Client**: typecheck, 224 unit tests, the build, and the 11 browser tests (headless, a local central and a world).
- **Editor**: typecheck, the build, 165 tests, and the render smoke (17 panels).
- **Talk**: `talk_check --validate` on all 152 files of `Data/Talk`, `--render-trees`, and `tools/dialect_check.py
  --world ridgemere3` (59,232 lines, no failures, no warnings). `test_dialect_check` and `test_path_check` passed.
- **Central**: `test_central.py` (84) and `test_world_join.py` (2).
- **Every talk_sim profile** in both modes, over Phase 0's fixture (`artifacts/talk/runs/s1`). The transcripts are
  Phase 1's line for line, because the code is the same.

**The one fix: the scorecard's gate check (a test fault, not the game's).** `talk_card` exited 1 on the same five
"regressions" Phase 1 reported, and none of them was one. A card's value is a mean of the profiles' rates weighted by
their lines, so it moves when the profiles' line counts move, even with the very same events: C6 frozen was 3 talks
against 3, and C4 fixtures was 1 clash against 1. C3 frozen (soft) was 18 good endings against 22 in 26,142. R5 frozen
was 2,166 collisions against 2,156 (and fixtures fell from 685 to 666). B5 is a timing: the same build measured 59, 72
and 80 frozen. `check()` in `tools/talk_score.py` now counts "worse" only beyond noise:
- A rate that counts events (R4, R5, C3, C4, C6, T1-T3) has its events and exposure worked out from the card's own
  per-profile values and counts. It is worse only by more than one standard deviation of a Poisson count, and by at
  least one whole event, against the baseline's count scaled to today's exposure.
- B5 is worse only by more than 25%.
- A hard gate the baseline passed still breaks on a single event.

A card without counts compares values as before. The message now gives the counts. `tools/test_talk_score.py` (ctest
`talk_score_rules`) checks that a real regression is still caught: three repeats against nine, a soft metric halved,
collisions up 14%, B5 down 40%. It also checks that this slice's noise is not. With it, `talk_card` passes in both
modes: no gate broken.

**The scorecard against P0's baseline** (fixtures / frozen; the same as §24's table, which has the shuffled control):
T1 13.4 → 0 / 0.1 → 0; T3 3.3 → 0 / 0 → 0; T2 0.41 / 0 (frozen in Phase 1); C1 96.1 → 97.2 / 93.0 → 98.9; C3 0.83 →
0.92 / 0.084 → 0.068 (%); C4 0.026 → 0.026 / 0 → 0; C6 0 → 0 / 0.117 → 0.119; R1 2.2 → 2.2 / 0.42 → 0.42 h; R3 2,170
→ 2,150 / 467 → 469; R4 991 → 991 / 3,860 → 3,901; R5 189 → 185 / 909 → 926; R6 3.6 → 3.5 / 13.2 → 13.2%; R7 53 → 58
/ 34 → 35; K1 3.8 → 3.8 / 24 → 24%; K2 52 → 50 / 42 → 42%; K3 2,284 → 2,259 / 109 → 109; D1 0.21 → 0.22 / 0.23 → 0.24%;
D3 67 → 69 / 13 → 13%; U2 46 → 46 / 39 → 40%; B5 1.5 → 1.5 / 68 → 80.

**Not done here.** No ticking run was made. The slice end's ticking 100-hour listen (Q6) and its review are still to do.

## 26. Slice 1's review: what it found, and the fixes (2026-11-16)

Branch `r3-talk-s1`. The slice-end review confirmed 26 findings (some twice, from two lenses); all are fixed here, each
with a test where one could be written.

**The game's talk.**
- **The window's gossip keeps the kin rule** (`dialogueRoleFacts`): nobody gossips about their spouse or family there
  either, so a widow never says "Sad news. Orm Blackwood died." of her husband (`dialogue_tests`).
- **A promise from the window's free talk is let go however late the answer comes**: whether the words were said in the
  window is decided when they are said (`Game::windowAsked_`, carried with a queued line), not when the Mind's answer
  arrives; a line said there whose window has closed since is still briefed "never promise" (`dialogue_tests`, with a
  stand-in Mind that answers after the window closed; the check fails without the fix).
- **A theft or an attack is the victim's own news only once they notice it** (`World::victimNews`, from `witness()`),
  naming the one who did it only if they saw who: "someone robbed me" when they couldn't make them out, a traveller as
  they know them, nothing at all when the theft went unnoticed. It was told from `recordEvent`, before anyone had looked.
- **One's own news is never told to the one it is about** (`AmbientNews::about`, saved in the checkpoint): no "I married
  Orm Blackwood" to Orm, no "Fennel robbed me" to Fennel.
- **The town's news is never told by or to the subject's own** (spouse, kin, one under the same roof): a father isn't
  told his own pup's birth as the town's talk, nor does a family come in from the Country call each other newcomers.
- **The town's crime names the one who did it only if a witness saw who**, and a traveller only as the teller knows
  them (else how they look); one who only heard a struggle names "someone".
- **A camp heard of is talked of only while it is there**: a cleared camp's rumours no longer say "they're camped out by
  Glasswater, bold as you like".
- **"Did you hear? a stranger robbed me."**: the authored exchange (no line fitting) capitalises the sentence after its
  question. The library's own lines already did (`insert`).
- **The house a pair shares**: housemates were sent to the home topic, whose scenes and opening rules are one's own
  house told to one who lives elsewhere ("One room, one window, one chair. I'm the one." to the one who lives in it). A
  new fact, `pair.roof` (yes when the two share a home, kin or not), gates those 59 scenes and 32 rules on
  `pair.roof=no` (the family's chores scene on `pair.roof=yes`), and nine new scenes in `home7.talk` are the shared
  house's (whose turn at the fire, the door off the latch, who finished the bread).
  `tell_spouse` is never said to the spouse ("Varg Acorndell snores like a mill." to Varg).
- **"My net" from a soap hand**: truth.json's `fisher_self` needs the speaker to fish or sail (§7.8, §9.6), not a
  fishing town; Coldwater's two first-person lines are scenes of their own for fishers (`rg_ls_first_fish_own`,
  `rg_ls_netmaker_own`). `talk_lint`'s frozen 20 findings are unchanged.
- **The door walks' caches** (the district, the street outside, an entrance) are keyed on `World::doorsKey()`, the doors
  less a streamed cell's seams, so a player walking through Ridgemere no longer wipes them every time a cell streams in
  or out (`stream_tests`).

**The instrument.**
- **`talk_card` fails a failed run.** A talk_sim's exit status fails the card; a profile's old transcripts are removed
  before it runs, so a failed run is never scored from what an earlier one left; a profile asked for with no run, or
  one the baseline has and the card hasn't, is a problem; nothing is blessed or refrozen from such a card; a missing
  fixture is said plainly (`test_talk_score`, with a stand-in talk_sim that fails).
- **talk_sim stops** when a fixture checkpoint fails to load (it carried on in the day before's world and exited 0).
- **Fixtures are pinned and checked.** `--record` writes the world build (`fixture.json` `build`) and each file's FNV-1a
  hash (`fnv64`, the track's too); `--fixtures` refuses a missing or changed file before replaying anything and loads
  that build, not the newest (`Options::worldBuild`, scratch games only). P0's fixture was recorded on build 81, still
  ridgemere3's newest; its manifest now says so and carries the hashes. The checkpoints and track stay out of git (28 MB),
  only in the worktrees' `artifacts/talk/fixtures/p0-r3-25d/`: a copy is now checked, not trusted (ctest
  `talk_sim_fixture`).
- **`--library-from` cleans up** its copy of the library in /tmp (19 of them, 63 MB, were left; removed).
- **talk_listen** keys a conversation by its run's own name (`frozen-L1`, `fixtures-L1`), so the two modes' marks don't
  merge.
- **The scorecard's definitions.** D1, the hard gate, is accent none's share (§15.3, "hard for none"; light and broad
  are D1L and D1B, soft). C3 counts only farewells and marked breaks; a talk cut off with no line is counted apart. T3
  counts the listener named in the third person to their face and an eyewitness's words ("I saw...") from one who didn't
  see it (talk_sim records each conversation's `seen`), beside one's own name. A coherence metric the shuffled control
  beats doesn't separate, however far apart (frozen C3, 0.18% against the control's 0.27%, is now listed).
- **The noise bar.** One standard deviation failed about two cards in three on noise alone. "Worse" for a counted metric
  is now the exact test of two Poisson counts (today's events and the baseline's, each over its own exposure, so both
  cards' noise counts) at 1% one-sided; C1, K3, D1, D1L and D1B are counted metrics too, and R1 and R2 (first-time waits)
  may move by half. Simulated at Slice 1's counts, noise fails 2.4% of cards (`test_talk_score`); fifteen repeats against
  three, collisions up 14% and the rest of the real regressions are still caught.
- **Closed gates locked, changed ones measured again.** `--refreeze IDS --why TEXT` (talk_card, talk_score) takes only
  those rows into a baseline and says why there. T1 is locked at Phase 1's 0 in both baselines; T3, C3, D1, D1L and D1B
  were measured again on this review's run (`artifacts/talk/runs/s1r`) as P1 did T2; the quick card's likewise.

**The scorecard** (`artifacts/talk/runs/s1r`, every profile in both modes over P0's fixture, pinned to build 81;
13,019 and 26,142 conversations; `talk_card` passes in both, no gate broken): fixtures / frozen, against the slice end's.
T1 0 / 0; T2 0.41 → 0.31 / 0; T3 (new) 0.41 / 0.03 per 1,000, all of it "I saw someone near X's door that night" told by
one who only heard it (`events2.talk`, `events_town5.talk`: Phase 4's points of view); D1 (none) 0 / 0, D1L 0.21 / 0.25%,
D1B 0.12 / 0.13%; C1 97.2 → 96.9 / 98.9 → 97.8%; C3 (new) 0.89 / 0.18%; C4 0.026 / 0; C6 0 / 0.12; R1 2.2 / 0.42 h; R3
2,131 / 451; R5 185 → 186 / 926 → 924; K3 2,259 → 2,197 / 109 → 109; U2 48 / 39%; B5 2.1 / 58 (twenty runs at once).

## 27. Phase 2, as built (2026-11-16)

Branch `r3-talk-p2`, on Slice 1 (`ridgemere2` at 0ec6d092). Every line can now say what it does; nothing chooses by it
yet (that is Phase 3's turn engine), so **what is said is Slice 1's word for word**: the twenty profiles' 105,799 lines
are identical, entry, alternative and text, to `artifacts/talk/runs/s1r`.

**`Data/Talk/moves.json`** (`Core/RatwTalkMoves.cpp`, `talk::Moves`): 96 moves in §7.2's 12 families (the party's join
moves are moves of their own, `join.defend`...; react.news, react.complaint, react.self, assess, answer.how and
answer.after carry variants), each with what it `expects` (soft when `optional`), what it `answers` (`*`: anything), its
`fallback` (every answer walks down to `ack`), `insertion`, `shape`, `tone`, `effects`, and the flags; the six tones and
§7.2's two tone rules. **Every one of the 121 answer concepts is an alias** (`jest.answer` is `tease.return`), its
lines' commonest move: a coarse reading until Phase 7 tags each line. The book is checked when it loads: an unknown
field, family, move, tone or variant, an alias of something not a concept, a move that neither opens nor answers, and
an expectation that doesn't answer its move are refused.

**The `.talk` keywords** (§7.1), parsed and checked in `Library::parse`: `does` (a rule's moves; a scene's per turn,
`/` between turns, `-` for one not known, its count checked against the scene's turns), `owes`/`invites` (`other`,
`self`, `any`; an owed move that doesn't answer the line's own is refused, and so is one with no move to answer),
`tone` (one, or one per turn), `asserts [claim]`, `denies [claim]` and `gist` (a rule's; a scene's may name the turn,
else it asserts in its first, denies in its second and makes its gist in its last; a claim must be claims.json's),
`binds x=<slot>` (`self.`/`other.` spouse, child, mother, father, neighbour, shopkeeper, priest, leader;
`topic.subject`; a line that speaks of `x` with nobody bound is refused), and `{k=v [keep 7d]}` before an alternative
(the conversation's tags; one written anywhere else, or one setting the engine's own `conv.move`, is refused). A rule
with no `does` makes its answer concept's alias; a `then` hands on its concept's move as what the rule owes (a hand-on
to a topic owes nothing). Choices made: a scene's `does` is one line for all its turns (read from each turn's first
alternative where the tagger had to choose), and a scene's last line owes only what is written (§8.2: a set piece hands
on into open talk only on purpose), where a rule's owes what its moves expect.

**`Ns::Conv` and `Ns::X`.** `conv.*` facts (facts.json's new `conv`: `move`, `prev`, `tone`, `beat`, which take
moves.json's words, and any tag) and `x.*` (a person's facts, and facts.json's new `x`: `affinity`, `disposition`;
`x.said.*` for Phase 5's ledger) are criteria and words (`#x.name#`, `#x.they#`, `#conv.k#`). `Library::converse`
gives each beat the conversation's facts so far (the move just made and the one before, the tone, the beat, every tag);
`Query::bind` is how the game names a slot's person, and `Game::talkQuery` fills it from `peopleOf` (the speaker's
bond to them as `x.affinity`, `x.disposition`; `exchangeQuery` adds `topic.subject` when it is a resident). With no
binder, or nobody there, a line that binds doesn't fit.

**`byMove_`**: rules by their moves, scenes by their first turn's, each under every kind it is of (react.news.known is
under react.news too); `Library::byMove`.

**The dead-end proof** (`Library::deadEnds`, run when the whole library loads; a dead end refuses it): every concept a
line hands on to has a rule of its own that holds and speaks with nothing known, and every answer a line can owe
(written, handed on, or what its moves expect) has such a line among it and its fallbacks. It found **the 17
dead-ending concepts of defect 13 and an 18th, `moment.reply_home`** (its one plain rule needs `#topic.accent_town#`):
`rules/fallbacks.talk` gives each a plain rule at priority -1 (speaking only where nothing else holds; none of them was
needed in any profile, as the identical transcripts show). Defect 13 is closed for the talk as it runs today; Phase 3's
fallback walk is proved too.

**`SaidLine` into the transcripts.** `talk::LineInfo` is now `talk::SaidLine` (§8.1): beside its entry, alternative,
layers and habits, each said line carries its moves, what it owes and invites, which of the line before it it answers,
its tone, the claims it asserts or denies (topic.claim_kind's family, else "topic"), the gist, its tags, the speaker and
the one spoken to, and its words. `talk_sim` writes them in `lines.jsonl` (and the shuffled control carries the moved
line's own), `--log-talk-words` writes moves, owes and tone, `talk_check` prints them under each line and `--validate`
counts what says what it does. The authored exchange (the "written" route, 23% of lines) has its moves too (gossip:
tell.gossip, react.news; news: tell.news, react.news; a quarrel: challenge, counter; friends: greet, greet.return;
else give.opinion, agree).

**The 40 most-spoken families tagged.** A family is the entries whose ids differ only in their last part (`h7_crime_*`),
ranked by the lines Slice 1's s1r run spoke of them: et_gos, h7_stranger, ev_pl, e1_home_shared, h7_crime, h7_theft,
et_theft, pl2_stranger, w7c_down, w7_cheap, ev_pl_child, et_gs, pl_stranger, pr_general, et_rm_gossip, em_rm_pr_cheap,
et_sf_gossip, et_ua_gossip, ev_pl_two, gs2, et_sr_gossip, ev_pl_ser_ferro, ev_pl_ridgemere, em_bd_camp, n7_go, n7_ga2,
e8t_ev_ans, w7c_up, em_more_cheap, h7_camp, ev_pl_suspicious, em_more_bd, n7_close_go, gs, e8t_ev, et_theft_inside,
em_prices, e8p_down, em_pup, e8t_ev_any: **209 entries (137 scenes, 72 rules), all by hand**, a `does` line each (a
grave `tone` on the death lines). With the aliases, 3,080 of the library's 8,557 entries say what they do (209 written,
2,871 by alias); 5,477 wait for Phase 7. Of the 1,615 hand-ons with moves on both sides, 273 (17%) hand on a move that
doesn't formally answer the one before (mostly a closing after an answer: answer.how then farewell): Phase 7's list.

**The scorecard** (`artifacts/talk/runs/p2`, every profile in both modes over P0's fixture, pinned to build 81; 13,019
and 26,142 conversations; `talk_card`: **no gate broken** in either mode). Every metric is Slice 1's (§26) to the digit
but B5, the machine's timing (2.04 / 58 exchanges a second, against s1r's 2.13 / 58). Two rows are new, measured where
lines carry moves, and taken into both baselines and the quick card's with `--refreeze C1M,M1` (no existing row moved):

| | Fixtures | Frozen |
|---|---|---|
| **M1** lines carrying moves (the gate: transcripts carry moves) | **66.4%** (scenes 68-76% by profile, the authored lines 100%) | **74.3%** |
| **C1M** owed answers answered, by moves (control in brackets) | **81.3%** (40.3%) | **96.3%** (55.3%) |
| C1 the "?" heuristic it replaces in Phase 3 | 96.9% (84.9%: doesn't separate) | 97.8% (85.9%) |

C1M separates from the shuffled control where C1 never did (§24), so the coherence the moves describe is measured, not
asserted. It is soft until Phase 3 makes it C1 (hard 92%); its misses are mostly owed answers left at a rule chain's
end and turn tags read from first alternatives. The quick card: M1 37.6%, C1M 99.9% (control 27.1%).

**Tests.** `talk_tests` (`movesAndTags`): the book's refusals (9), the lines' (27: every keyword, the tags, `x` and
`conv` facts), aliases for every answer concept, entries by move, what a said line carries (moves, owes, invites,
answers, tone, asserts, tags, keep), the conversation's facts reaching the next beat, a bound third party and its
criteria, and the dead-end proof (a concept, a handed-on move and an expected move, each a dead end until filled).
`dialogue_tests`: the game's binder names a resident's people with their facts and the speaker's bond, and nothing for
an unknown slot or `topic.subject` outside an exchange. The ctest suite without the database (90 of 90) passed; the
database suites weren't run (no schema or migration changed).

**For Phase 3.** The turn engine chooses by `byMove`, the fallbacks (`Moves::chain`) and `Moves::answers`/`meets`;
what a line owes is `SaidLine::owes`. `C1M` is the measure to promote to C1's hard gate there. A scene's per-turn tags
were read from each turn's first alternative; where its alternatives differ in kind (a question beside a statement)
Phase 7 should split them. `Query::bind` resolves only peopleOf's roles and `topic.subject`; Phase 4's `joiners` and
Phase 5's ledger (`x.said.*`) build on it. Tags reach later beats only through `converse` today (not between a scene's
own turns).

## 28. Phase 3, as built (2026-11-16)

Branch `r3-talk-p3`, on Phase 2. The talk is now chosen a line at a time by what each line owes.

**The engine** (`Core/RatwTalk.{h,cpp}`). `Conversation` (§8.1: people, phase, stacked frames, budget, the answers owed
as `Open`s, `conv.*`, what was said and asked, the agenda's slot for Phase 5), `Freshness` (the heard set and the
place's recent list), `Library::begin` (the opener, as `pick` chooses it, a scene's later turns queued), `next` (one
line, or nothing once ended) and `answer` (the line a speaker says to what is owed). **Fit, then tiers**: the
candidates are the handed-on concept's own rules and the *move lines* doing an owed move (a rule with no concept,
written with `does`, found by its move alone; a line tied to a concept answers only for it until Phase 7 says what every
line does, because the aliases are coarse). Fit 3 is the concept's own or the exact variant owed, 1 another line doing
an owed move; only the best fit is kept, then tiers by score at 70/20/10 within it (replies are no longer the best tier
only), unheard first, by weight times the frames' tone weights. Nothing owed answered: the move's fallbacks walked down
to `ack`, then a gap and the closing. Never an entry, words or question already said in this talk. `converse` is
`begin` then `next` until it ends, over one query (the game's exchanges, barks, reactions, `talk_check` and the tests
all go through it; Phase 4 gives each beat its own query). `Said::ended` and talk_sim's conversation head say how it
ended (farewell, curt, cap, nothing, done).

**Turn-taking and closings** (§8.3-8.4). What is owed is answered by the one it is owed by; an invitation is taken half
the time (`frames.json` `invite_taken`, until Phase 6's temperaments). With nothing owed and no agenda yet (Phase 5)
it is a lull, and a resident talk on a topic (`talk.*`) closes: the one at work first, else the one who didn't speak
last; a preclose and its answer first when the talk has run four lines and its budget has room for the whole closing
(the frames' `preclose` makes that room smaller), then the farewell pair; a line may preclose and take leave at once
("Well. Fair winds."), and what a line owes by default is its last move's. A cold or short-tempered closer goes without
a word half the time (`curt`; Phase 4/6 make it cost regard). The most lines a talk has is 14, the farewell pair
included (the old 8-beat cap stays only for a library without moves.json). A reaction, a bark or an answer to a player
gets no closing or greeting: the other may be a player. **Shrugs** (settings.json `ends`) are `ack` lines: they owe
nothing, whatever their rule hands on, and the talk closes. **Tone rules** (moves.json): light is blocked two lines
after grave and cross one after tender unless a bridge (change_subject, assess, preclose) came between; nobody teases
one who mourns.

**`Data/Talk/frames.json`** (§7.4): strangers (greet; budget ×0.6; no tease or confide), kin and housemates (kin.open:
no greeting), boss (Phase 6's hedge), at work (budget ×0.8, preclose ×1.5), host (greet, offer), tavern at night
(budget ×1.4, light ×1.4); the budgets (§8.4: chatty 8-12, plain 5-8, quiet 3-5, the smaller of the two, +2 for
friends, spouses and kin, ×the frames, 2-14). A greeting is said in the opener's breath, one line, before an opener that
says what it does and doesn't open itself (an untagged opener is left alone until Phase 7). Checked on load (unknown
field, fact, move, tone; scales 0.2-3; budgets). The goals and town moods of §7.4 come with Phases 5 and 6, and "Back
again?" with the ledger's `pair.last_talk`. Choices made: `budget` lives in frames.json until topics.json (Phase 5);
`preclose` is how readily a closing begins with one.

**`Data/Talk/rules/turns.talk`**: 27 move lines (preclose 3, accept_preclose 2, farewell 5, preclose+farewell 2,
farewell.return 6, greet 2, greet.return, ack 2, agree, accept_apology, deflect_thanks, accept_decline), under six words
each (never a "distinctive" line for R4 and R5), few criteria and many alternatives so no one line is said most of the
time, in the speaker's dialect through `#farewell#`, `#greet#`, `#agree#`, `#later#`, `#bless#`. A first listen in the talk
town gave the one at work "Aye, and me. Back to it." 511 times in 2,000 talks; the spread fixed it. **Also**: a move of its own is no
longer another's kind (`Moves::kindOf`: farewell.return isn't a farewell, though react.news.known is a react.news),
which `byMove`, `answers` and the scorer now follow; `kn_sib_teach` asked "Like this?" twice (C6's three frozen talks).

**Golden for unconverted content.** A library without moves.json (the golden transcripts' a46cbdac) goes through the
same `begin`/`next` and is told word for word as before: `talk_golden` passes unblessed.

**The scorecard** (`artifacts/talk/runs/p3`, every profile in both modes over P0's fixture, pinned to build 81; 13,019
and 26,142 conversations, 61,109 and 95,071 lines; `talk_card`: **no gate broken** in either mode). Against Phase 2's
own transcripts rescored with this phase's definitions (`artifacts/talk/runs/p2r`), fixtures / frozen, the shuffled
control in brackets:

| | Phase 2 | Phase 3 |
|---|---|---|
| **C3** endings by a farewell (§8.4) | 0.89% / 0.19% | **80.8% (43.6) / 52.2% (30.6)**; curt 11.2 / 6.3%, cap 0 / 0 |
| **C4** tone clashes per 1,000 lines | 0.026 / 0 | **0 (0.16) / 0 (0.01)** |
| **C6** an entry or question twice, per 1,000 talks | 0 / 0.12 | **0 (143) / 0 (106)** |
| C1 owed answers answered (now the hard gate, 92%) | 81.3% / 96.3% | **92.2% (25.6) / 97.7% (37.6)** |
| C2 replies at the best fit that held | n/a | 100% / 100% (56 fallbacks of 14,930 answers / 0) |
| C7 lines per talk | 3.0 / 2.6 | 4.7 (p90 5-8) / 3.7 |
| R3 distinct openers per 3,000 | 2,130 / 451 | 2,340 / 504 |
| R4 / R5 per 100 h | 1,010 / 186; 3,920 / 924 | 750 / 139; 3,860 / 897 |
| R6 the most-spoken line | 3.5% / 13.2% | 3.5% / 9.3% |
| R7 distinct replies per 1,000 (closings apart) | 54.8 / 32.3 | 60.5 / 35.0 |
| T2 / T3 per 1,000 lines | 0.31 / 0.41; 0 / 0.03 | 0.23 / 0.26; 0 / 0.02 |
| D1L / D1B light and broad lines with dialect words | 0.21 / 0.12%; 0.25 / 0.13% | 2.5 / 0.7%; 1.4 / 0.5% |
| D3 / R8 | 83.4 / 0.024; 58.8 / 0.062 | 81.2 / 0.023; 49.0 / 0.089 (both within their counted noise) |
| B5 exchanges a second | 2.0 / 58 | 2.2 / 72 |

Frozen C3 is 52% because 42% of its talks are the authored exchange (route "written", the Mind's fallback when no
library line fits), which never reaches the engine; Phase 4's per-beat director can close those too. Every other
library talk closes or is curt. D3 frozen moved on seven L3 speakers (three guessed right, then two) and on L2's, where
135 speakers now have 20 lines or more against 80; R8 frozen is 79 habit lines against 48 (the tiers within the fit reach
replies the best tier never did, `e8t_ans_hard_any` and `e8t_ev_ans_bad`). K1-K3, U and T1 are unchanged: topics are
Phase 5's.

**Definitions changed, measured again on Phase 2's transcripts** (`--refreeze` from `p2r`, the why in each baseline):
C1 is Phase 2's C1M (owed answers by moves, the hard gate at 92%), the "?" heuristic is C1Q (a diagnostic) and C1M is
gone; C2 is measured; C3 counts a last line that takes leave by its moves as well as its words (the lexicon gained the
dialects' farewells; "fair price" and "good harvest" only as a line's last words) and its cap is the engine's `ended`;
C4 counts a grave-to-light clash by the lines' tones as well as the lexicon, and a bridge move stops it; R7 and X1
leave the closing pairs out (formulaic by design; their own variety is in R7_detail); D3 is not judged (None, not 0%)
where one dialect was heard, and a card's mean is over the profiles that judged a metric; R8 and D3 are counted metrics
for the noise test (habit lines; speakers guessed right). `test_talk_score` gained six checks (27). The quick card was
blessed again (`Tests/fixtures/talk_scorecard_quick.json`): its talk changed by design (C3 0.75% to 92%, C7 3.2 to 5.4,
C1 100%) and its old baseline had no R8 exposure to judge noise by (a handful of habit lines either way).

**Tests.** `talk_tests` `turnEngine`: begin and next a line at a time and converse the same whole; fit 3 over a move
line; tiers within the fit; the fallback down to ack; a reaction gets no closing; a shrug is an ack; the preclose by the
one at work, its answer and the farewell pair, and none where the budget has no room; budgets by talk, kin and frames;
frames stacked; a stranger's greeting in the opener's breath; strangers never tease, nobody teases one who mourns; light
blocked after grave until a bridge or two lines; 14 lines at most, each entry once, a farewell last; invitations taken
about half the time; frames.json's eleven refusals; a library without moves.json still talks as before (8 beats). The
chain test's merchant answers as a merchant most of the time now, not always. The non-database ctest suite passed 90 of
90 (the quick card after its bless); `talk_check --validate` on every file and `talk_lint` (its 20 frozen findings,
unchanged) passed. The database suites weren't run (no schema change).

**For Phase 4.** The game still renders a whole exchange up front through `converse` over one query; Phase 4's
per-beat queries call `begin`/`next` from `Game::ambient` with the facts function (a and b as begin had them; a third
voice will need the facts function to map speaker and addressee). Re-checking a queued scene turn when it comes due
(Valve's rule) is a no-op over one query and comes with them. The authored "written" route (42% of frozen talks) never
closes. `Freshness` holds only the heard set and the recent list (alternatives, the collision filter and per-listener
stories are §8.12's). Curt endings and moves.json's effects cost nothing yet (per-beat effects). Extends work once
(`max_extend`) but no line extends yet, and nothing fills the body: agendas and pivots are Phase 5's. Scenes are beats
only at the opening: a scene whose first turn answers could be a reply once Phase 7 tags turns.

## 29. Phase 4, as built (2026-11-16)

Branch `r3-talk-p4`, on Phase 3. Every line of a resident talk is now chosen when it comes due, from the facts as they
are then, and does what it does as it is said.

**Beats when spoken** (§8.2; `Core/RatwGameAmbient.cpp`). With a library that has moves.json and no model writing it,
the director's talk is a `talk::Conversation` held in its `AmbientTalk`: `Game::ambientBegin` says the first line
(`Library::begin`; or, when nothing in the library fits, the authored lines through the new `Library::beginWith`, which
the engine then carries on, so they close too), and `Game::ambientBeat` chooses each later line at its `nextAt`
(`Library::next`). Its facts (`Game::talkBeat`) are the start's query (the topic's facts fixed, prices at the hearer's
list as §8.5 says) with the moment (`here.*`), each one's hurt and fighting, the bonds (`Game::talkBond`, split out of
`talkPair`) and what is remembered (person, pair, town) asked again every line, and each speaker's person facts worked
out again every 30 s. `peopleOf` and `neighboursOf` are kept 30 world seconds while the doors are the same
(`talkPeopleFresh`): a look's query fell from about 1.95 ms to 0.24 ms, and B5 rose with it. A library without
moves.json, a reaction and the model's lines are spoken whole as before (`talk_golden` passes unblessed). Each queued
scene turn is checked again when it comes due (Valve's rule, `Library::stillHolds`): its moment's criteria (`here.*`, a
speaker hurt, fighting, at work, mourning), never what is remembered, which its own first line changes; failing, the
rest of the scene is dropped and the talk goes on (`Conversation::dropped`; none in the card's runs, the unit tests
show it). `SaidLine` gains `turn` and `variant` for it.

**Per-beat effects.** What a line remembers (`talkRemember`) and that its rules were heard (every chain id, by all who
hear its speaker) are applied as it is said. moves.json's `effects` are applied per line through `Game::regard`
("talk", bounded): `self` the speaker toward the one spoken to, `other` theirs. Sixteen moves have them now (two did:
sympathise and decline; a farewell answered +0.3 each way, gossip +0.3 to the teller, thanks, praise, reassurance, apologies, a tease laughed at;
an offence taken, a refused apology, defending or objecting −0.5 to −1). `World::ambientSpoken`'s blanket warmth is gone
from this path: `World::ambientBegun` counts the talk from its start, `World::ambientEnded` makes everyone in it a
little more familiar (a quarrel's grudge still sours) and records it. A curt ending costs regard (frames.json `curt`:
the chance 0.5, the one left −0.5 toward the closer).

**Breaks and returns** (§8.10). Before each line `Game::interruption` looks for a player opening the window with one of
them ("called"), a fight breaking out in the place (not one under way as they began) and rain starting where they stand
in the open (the place's sky, as `here.weather` says it); `Library::interrupt` sets `conv.interrupt` and the one whose
turn it was (or the one called) says a `break` move line, and the talk ends (`ended` "break"; talk_sim marks it, C3's
marked break). A talk broken off or cut short before its point was made (no line asserting its claim or making its gist,
no closing begun), neither of them dead, is kept as `pair.thread` for frames.json's `thread` days (2), in the pair's
talk memory until Phase 5's ledger; the next talk of theirs on that topic returns to it in the opener's breath
(`Library::preface`, with the frames' greeting: "Good evening. Now, as I was saying. I keep hearing it was Maren
Tarhand."), and the thread is done with. Residents at the window are never picked for a talk or a join.

**A third voice** (§8.9; `Game::joiners`, `Library::join`). Between lines of a talk of two in its body, with room for
the joining line, its answer and the closing: one idle within 3 tiles of both, hearing the last speaker at 0.5 or more,
out of talk 60 s, knowing one of them (familiar, kin or under one roof), and with a stake: the one talked of, or named in
the last line (`named`: join.object); kin or a friend (affinity 40+) of a resident subject once the claim is said
(`kin`, `friend`: join.defend for an ill claim, else join.add); a merchant dealing in the topic's good (`goods`); a
witness of its incident (`witness`); or close to both (friends, kin, a shared roof) and chatty (`friends`); the nearest
two tried, and only with a line that fits (`conv.stake`). The joiner cuts in on the one who spoke last, who owes the
answer; three at most (frames.json `join`). The engine takes three: a line's `who` and `to` are indices into
`Conversation::people`, what is owed is owed by the one it was said to, `Library::partner` says who each is talking
with, and `next`'s facts function has the speaker as a and the one spoken to as b once a third is in.

**Knowledge** (§8.6). A line that asserts the topic's claim (or, for a line that says nothing of what it does, the
teller's first) passes it to everyone else in the talk at the speaker's confidence × (0.5 + 0.4 × trust/100) (0.1-0.9),
and to every resident standing by who hears it at 0.5 or more at × 0.4, only once it is said (a talk cut off before has
passed nothing). `World::told` corroborates a claim from a second, independent source, (1 − c) × 0.4, once a source
(never the one they had it from, nor one who had it from them; the sources counted are not saved: Phase 5's ledger's
"told" will hold them). A `denies` line from the subject or their kin multiplies each listener's confidence by
(1 − 0.3 × trust/100) (`World::doubted`, never above 1). `knows` is a person fact: whether one held the topic's claim
before the talk began (`World::holding`), the teller always. The engine answers react.news from one who knew with the
known variant as its exact fit, never says "I'd heard" for one who didn't nor "Never!" for one who did, and never
chooses a scene whose turn would (`Library::untrueScene`).

**Data.** `Data/Talk/rules/beats.talk`: 21 move lines (known 3, break 4, return 2, join 7, welcome, apologise, insist,
concede, decline_apology), the often-said ones under six words. facts.json: `knows`, `pair.thread`, `conv.interrupt`,
`conv.stake`. frames.json: `curt`, `join`, `thread`, checked on load. turns.talk: more night and pups' farewells (the
written route now closes too, and "Good night." had become the most-said line in the talk town). hard7.talk: the merry
answer to a robbery no longer laughs first (`h7_cr_ans_merry`: "Ha!" after "Robbed X?" was the one C4 clash frozen L2
met once the sampling moved).

**`talk_bench`** (`Tests/talk_bench.cpp`; ctest `talk_bench`, label perf, with `-DRATW_PERF_TESTS=ON`): a scratch game
(the talk town by default, `--world ridgemere3`), listeners where the most residents stand free, frozen, the director
for the hours asked, and B1 checked. The game keeps its timings when asked (`Game::setTalkTiming`): each beat (its
facts, an interruption looked for, a joiner, the engine's choice) and each look; talk_sim writes them into run.json
(`beat_ms`, `look_ms`) and talk_score reads them as B1, B1P and B1L, soft there, the slowest profile's: a card's runs
share the machine ten at a time and a fixture restarts a cold world every game midnight, so B1's gate is the bench's.

**The gate: met.**
- **B1 within budget**, `talk_bench --world ridgemere3 --hours 20` alone (2,400 talks, 9,806 beats, 14,400 looks): a beat
  0.079 ms mean, p99 0.78 ms (0.6 and 3); a look p99 3.67 ms (5). The talk town: 0.032, 0.071 and 0.73 ms. In the
  card's runs (load and cold restarts): beats 0.11 / 0.14 ms mean, p99 0.35 / 1.72; looks p99 22.7 / 8.4 ms, fixtures
  / frozen (cold `World::districtHouse` walks, which load cells, after each fixture restart and each game day: the
  next phase that touches the director should warm or keep them).
- **T5 measured**: 4.29% of 4,653 judged claim lines (fixtures) and 2.12% of 203 (frozen) told to one who already held
  the claim with no "I'd heard"; the rest of those knowers answered "I'd heard" (10 lines) or were never told: almost
  all of the misses are whole scenes whose answering turn is written ("Don't quote me." / "I won't."), which
  `untrueScene` can't see until Phase 7 tags each turn.
- **C8 measured**: 0.40% (fixtures, 55 joins: witness 37, named 10, friends 6, kin 2) and 4.15% (frozen, 1,153:
  friends 1,144, witness 8, named 1). Ridgemere 3's bonds are thin (nobody near a talk is a friend, affinity 40+, of
  both), so the household's chatty one standing by is most of it; stances (Phase 6) are the stake still missing.

**The scorecard** (`artifacts/talk/runs/p4`, every profile in both modes over P0's fixture, pinned to build 81; 13,021
and 26,120 conversations, 63,452 and 116,386 lines; `talk_card`: **no gate broken** in either mode). Against Phase 3's
own transcripts rescored with this phase's scorer (`artifacts/talk/runs/p3r`), fixtures / frozen:

| | Phase 3 | Phase 4 |
|---|---|---|
| **C3** endings by a farewell or a marked break | 80.8% / 52.2% | **88.3% / 89.8%** (the written route closes now; curt 11.7 / 10.4%, one marked break) |
| C7 lines per talk | 4.71 / 3.65 | 4.88 / 4.47 (frozen p90 6) |
| **C8** a third speaker | 0 / 0 | **0.40% / 4.15%** |
| **T5** told what they knew, no "I'd heard" | n/a | **4.29% / 2.12%** |
| C1 / C2 / C6 | 92.2 / 100 / 0; 97.7 / 100 / 0 | 92.6 / 100 / 0; 97.9 / 100 / 0 |
| C4 tone clashes per 1,000 lines | 0 / 0 | 0.047 (3 lexicon clashes in old content, an open gate since P0, within noise) / 0 |
| R1 first identical talk | 2.2 / 0.42 h | 2.5 / 0.84 h |
| R4 / R5 per 100 h | 750 / 139; 3,860 / 897 | 724 / 126; 3,820 / 883 |
| R6 / R7 | 3.5% / 60.5; 9.3% / 35.0 | 3.0% / 62.7; 7.6% / 34.1 |
| T1 / T2 / T3 per 1,000 lines | 0 / 0.23 / 0.26; 0 / 0 / 0.02 | 0 / 0.24 / 0.25; 0 / 0 / 0.009 |
| K1 / K2 / K3 | 3.8 / 51 / 2,200; 24.5 / 42 / 109 | 4.1 / 47 / 2,180; 24.3 / 42 / 131 (within noise) |
| D1L / D1B / D3 | 2.5 / 0.7 / 81; 1.4 / 0.5 / 49 | 2.6 / 0.8 / 90; 1.8 / 0.7 / 64 |
| B5 exchanges a second | 2.2 / 72 | 2.2 / 86 (121 alone; ten at once) |

T5, C8, B1, B1P and B1L were taken into both baselines from this card (`--refreeze`, the why kept there): they are
measured for the first time. The quick card passes unblessed.

**Tests.** `talk_tests` `beatsWhenSpoken` (144 checks): each beat asks the facts again; Valve's rule drops a scene's
later turn when the fog lifts and never re-checks its own memory; a break by the one named, and a talk that ends with no
break line; a third who joins (not once the closing begins, not without a fitting line, three at most), denies, is
apologised to and accepts, and the talk closes; "I'd heard" from one who knew every time, never from one who didn't, and
a "Never!" scene not chosen for one who knew; a thread returned to on its topic only; written lines carried on to the
farewell; frames.json's six new refusals. `talk_beat_tests` (new, ctest `talk_beats`, 18 checks): in the talk town, a
gossip talk's claim reaches the one told only when said, at the trusted confidence, and one standing by from the
teller; the one talked of cuts in with join.object (three in it, stake "named"); familiarity grows; rain breaks a talk
off with a marked break line. `ambient_tests` `talkCarriesKnowledge`: corroboration (independent, once a source, not
from one's own source or one who had it from oneself), denial, `holding`, begun and ended. `test_talk_score` (29): T5,
B1 (the slowest profile, timing noise, a passed hard timing gate). `talk_sim_fixture` looked at every
`/tmp/ratw-talk-lib-*` and failed when `talk_golden` ran beside it (`ctest -j`): it now looks for its own run's copy.
The non-database ctest suite passed 91 of 91; `talk_check --validate` on every file and `talk_lint` (its 20 frozen
findings) passed. The database suites weren't run (no schema change). One full card run failed (frozen L2: `joiners`
iterated two copies of a merchant's wares) and was run again after the fix; no ticking runs, no paid calls, no
servers.

**For Phase 5.** The thread lives in the pair's talk memory (`pair.mem.thread`, read as `pair.thread`) and the sources
corroboration counted are not saved: both belong in `RatwTalkLedger`, with the told set (`react.news.again`, K3).
Topics are still the director's single pick: a thread is returned to only when the same topic comes up, and an agenda
item would make it urgent. Joins have no stance to go on yet (Phase 6: `join.side`, `join.correct`), and Ridgemere 3's
bonds are thin. Most of T5 is whole scenes whose answer is written: Phase 7's per-turn tags let `untrueScene` see them.
The look's p99 under cold caches is `World::districtHouse`'s walk (it loads cells), cleared every game day.

## 30. Phase 5, as built (2026-11-16)

Branch `r3-talk-p5`, on Phase 4. What two residents talk of is chosen from data now, cooled by what they and the place
have already said, and a talk turns from one topic to the next; the talk ledger remembers it all.

**`Data/Talk/topics.json`, today's numbers first** (`Core/RatwTopics.{h,cpp}`, `topics::book()`, the process's, set by
the library's loader as claims.json's table is). Every score `World::ambientTopic` and `sceneTopics` had in C++ is a
source of a topic (`gossip/claim` 3 + confidence + 0.5 for a traveller, `caravan/late` 1.75 + the hour's draw,
`weather/fair` 0.6 + the pair's draw...), in the director's own formulas; a library without topics.json (the golden
transcripts' a46cbdac) uses today's, built in. **Proved identical** before anything was tuned: talk_sim `--dump-topics`
hashes every topic the director weighs (kind, score to the last bit, source, subject, claim, words, facts) each step,
recorded with Phase 4's code and compared after the move: on Ridgemere 3 (frozen L1 and L2 6 hours, fixtures L1 and L3
30 hours) and the talk town, 3,055 looks and 717,716 topics weighed the same, and the transcripts were byte for byte
Phase 4's. The ctest `talk_topics` keeps the proof: 400 talk-town looks (27,914 topics) over a46cbdac, the record made
before the move (`Tests/fixtures/talk_topics/town-400.topics`), weighed the same built in and from
`Tests/fixtures/talk_topics/topics-today.json`. topics.json is checked when it loads (unknown fields, a source the
director scores missing, an edge or a favourite naming no topic, a `pair_bonus` band that isn't one), and its edges'
criteria against facts.json (`Library::steps`).

**Then tuned** (the live file): 36 topics, 109 edges (`next`, with criteria: prices to the caravan when
`topic.cause=caravan|late|raid`, weather to the land for a farmer...), aspects for eight topics, each topic news or
everyday or neither (the player topic, a quarrel). The cooling (§8.5), applied to every topic as it is weighed
(`World::weighTopics`): the same two on the same topic and thing within 2 days ×0.4 (the ledger's notes; an everyday
topic by its name alone); one thing talked of 5 times in a place today ×0.5; the **everyday floor**, +2.0 to everyday
topics where more than 60% of a place's last 8 talks were news; a speaker's favourite +0.5 (13 trait and trade rules,
`favourites`, until mantles bring their own); closeness (`pair_bonus`: friends +0.6, spouses and family +0.5,
housemates +0.3, apprentices +0.2, rivals and strangers −0.2, added to every topic of the two: decided, not in §7.3);
and **nothing told a listener twice** (`told_once`: the ledger's told set, and a claim the listener holds in other words,
by its incident or its kind about the same victim, as `World::holding` finds it). Choices made while tuning: the doc's
saturation (3 a day, ×0.3) left news 16% of frozen talk, so 5 and ×0.5; its floor's +1.5 left fixtures K2 at 24%, so
+2.0. `AmbientTopic::key` says what a topic is about (a claim's subject and words, a wagon, a ship, a price's good, an
incident).

**Agendas and turns** (§8.5). `World::agendaFor(a, b)` is everything the two might talk of, the likeliest first, each
owned by the one who'd raise it; `ambientTopic` is its first (the same rule, so the proof holds). The game gives a talk
the 8 likeliest others as `Conversation::agenda` (each item's topic facts, through `exchangeFacts`, and who knew its
claim). In a lull with room for a turn, its answer and the farewell pair (`Library::lull`, before the closing), the talk
may turn, at most `topics_per_talk` (3) topics: back to a thread left on an item's topic (a `return` line first: "As I
was saying."), along the graph or on the same topic (a **pivot**, `#pivot#`: "Speaking of which."), or, half the time,
anywhere (a **change of subject**, `#anyway#`: "Anyway."); the move is put first among the opener's moves, so a turn is
never unmarked (C5). The item's facts lie over the old topic's in every later query (`Library::overlay`: topic.*,
`knows`, `conv.topic`); the game's pick follows it (its claim passes, its told set). A turn's opener is rendered lazily
(`Library::pickLazy`: chosen first, only the chosen rendered; never an entry or a question already said in the talk).
`conv.aspect` is a topic's tier: one deeper for a pair that has talked of it (`pair.talked.<topic>`).

**Goals** (frames.json's `goals`, §7.4): mend (the last talk soured or quarrelled within a week: an apology), comfort,
hurt and robbed (asking after a loss, a hurt, a robbing), nag (a talk-only promise let lapse), missed (an invitation not
kept), warn (the camp, to one about to travel), learn_name (introductions, not at work: the one spoken to then knows the
speaker's name, familiarity 10), favour (one poor asks one they trust: pre.request, then request on a go-ahead), visit
(friends or kin in a tavern: an invitation). In a lull the first goal that holds for the one whose turn it is, or for the
other, is pursued a step a lull by move lines testing `conv.goal`; a step no line fits gives it up; done, or tried, it is
kept between the two for its days (`pair.was.goal_<name>`). vent waits on Phase 6's `self.mood`. `again_today` is a
frame now ("Back again?", `pair.last_talk<0.3`).

**The talk ledger** (`Core/RatwTalkLedger.{h,cpp}`, owned by `World`, §8.7): pair notes (8 a pair, 30 days: topics and
keys, outcome, entries told, claims passed, kept tags, an open thread), the told set (64 a listener, with which kind and
whether a source was counted: `World::told`'s corroboration, which replaces `corroborated_`), person facts (16 a
subject: `remember x: k=v Nd`, read as `x.said.k`), the heard set (64 a resident: stories and teases told), a speaker's
kept tags, places (the last 16 talks, today's counts) and invitations and promises; at most 30,000 records, the least
recently used dropped when it is pruned each game day. Saved in the checkpoint (`talkLedger`), carried over talk_sim's
fixture restarts (`talkCarry`), and **written out of hearing** by the daily gossip (`World::gossip`: a note with no
words, one a pair a day, and the told set); `--no-talk-ledger` (the server, world_check; world_check `--fast` has it)
writes nothing. Its facts, asked every line: `pair.last_talk`, `pair.last_outcome`, `pair.thread` (Phase 4's thread
moved here), `pair.talked.<topic>`, `pair.recall.<topic>`, `pair.was.<k>`, `pair.kept_invite`; each one's `was.<k>`,
`told_me`, `promised`, `promise_what`. A line whose criteria read them is a callback (C9). **Invitations**
(`invite here <hour> <days>`) are checked each look (kept when both are in its place in its hour, missed when its days
run out); **talk-only promises** (`promise self: <thing> <days>`) lapse and are nagged once. The game writes the ledger
as lines are said (`ambientSaid`, `ambientTopicBegun`) and its note when a talk ends (`ambientNote`).

**Lines** (`Data/Talk/rules/agenda.talk`, 42 rules): again.open, introductions (by first name: the new person fact
`first`), asking after and the answers, the nags and the missed invitation, the apology, the warning, the favour's five
moves, the invitation and its answers, four callbacks, a third who joins for their favourite topic; the plain grammar's
`pivot` and `anyway`; moves.json's `introduce`, `introduce.return` and `ask.name`. A third may join with a new stake,
`favourite` (one who likes to talk, the talk on their favourite topic, who knows both). **Truth, newly heard**: the
everyday talk reached lines nothing had said in 100 hours: eight were untrue to the moment (rain, fog, "my catch" from a
labourer, "my son" from one with none: reworded), five read as grave then light (reworded), introductions said one's own
full name (first names now), and some lines named one's own spouse or child to that spouse or child (`notEachOthers`:
one's people by name are left out where they are the one spoken to; a line never binds the one spoken to as x). talk_lint
went from 20 findings to 9 (blessed). T2 now judges a line at its own hour (talk_sim's per-line `hour`): a talk begun at
17:59 may close with "Good night".

**Speed** (B1): the people and neighbours memo is kept five world minutes (30 s before: Phase 5's longer talks crossed it,
and with it a walk of every home, in most talks); the pair's ledger facts are read in one pass; the lull asks one query.
Alone, frozen L1 on Ridgemere 3: a beat 0.109 ms mean, p99 0.74 ms; a look p99 4.67 ms (budget 5). `talk_bench` (the
talk town): a beat 0.061 ms, p99 0.18; a look p99 0.93: within budget. (`talk_bench --world ridgemere3` found nobody
standing about in DEV's save this evening, and a fixture's checkpoints are at midnight; no world was ticked to warm one.)

**The gate: met** (`artifacts/talk/runs/p5`, every profile in both modes over P0's fixture, pinned to build 81;
13,041 and 25,964 conversations, 71,149 and 165,465 lines; 5.7 minutes). Fixtures / frozen:
- **K1** everyday talk: **44.8% / 74.7%** (Phase 4: 1.7% / 3.0% as K1 now counts; 4.1% / 24.3% as it counted before).
- **K2** the largest topic: **21.1% / 13.0%** (46.9% / 41.8%).
- **K3** the same news twice to one listener: **0 / 0** (2,179 / 131).

**The scorecard against Phase 4** (its transcripts rescored with this phase's scorer, `artifacts/talk/runs/p4r`),
fixtures / frozen:

| | Phase 4 | Phase 5 |
|---|---|---|
| R1 first identical talk | 2.5 / 0.84 h | **16.2 / 2.3 h** |
| R3 distinct openers per 3,000 | 2,299 / 484 | 2,517 / 1,674 |
| R4 / R5 per 100 h | 724 / 126; 3,823 / 883 | 460 / 83; 3,818 / 592 |
| R6 the most-spoken line / R7 distinct replies per 1,000 | 3.0% / 62.7; 7.6% / 34.1 | 1.9% / 236.7; 2.7% / 115.6 |
| C1 / C3 / C7 | 92.6% / 88.3% / 4.88; 97.9% / 89.8% / 4.47 | 96.9% / 89.4% / 5.59; 98.9% / 90.1% / 6.42 |
| C4 / C6 | 0.047 / 0; 0 / 0 | 0 / 0; 0 / 0 |
| **C5** unmarked jumps per 1,000 lines | n/a | **0 / 0** (talks turned: 1,497 / 9,047) |
| C8 a third speaker | 0.40% / 4.15% (55 / 1,153 joins) | 0.31% / 4.90% (51 / 1,493 joins; favourite 20 / 1,346) |
| **C9** callbacks per 10 talks | n/a | **1.21 / 0.76** |
| T1 / T2 / T3 per 1,000 lines | 0 / 0.24 / 0.25; 0 / 0 / 0.009 | 0 / 0.028 / 0.070; 0 / 0 / 0.006 |
| T5 told what they knew | 4.29% / 2.12% | 0.04% / 0% |
| D1L / D3 | 2.6% / 89%; 1.8% / 64% | 2.1% / 84%; 1.3% / 71% |
| M1 lines carrying moves | 78.6% / 85.3% | 66.1% / 55.9% |
| U2 / X1 | 46.5% / 26.2; 38.5% / 14.7 | 40.1% / 20.2; 29.4% / 16.7 |
| R8 habit lines (% of a speaker's, mean) | 0.019 / 0.066 | 0.104 / 0.134 |
| B1 / B1P / B1L (ms, the card's load) | 0.11 / 0.35 / 22.7; 0.14 / 1.72 / 8.4 | 0.19 / 0.62 / 13.7; 0.19 / 1.52 / 10.0 |

Goals pursued (steps said), fixtures / frozen: favour 286 / 427, mend 154 / 512, visit 144 / 311, learn_name 110 / 2,
nag 5 / 6, robbed 1 / 0.

**What the card still flags, and why** (soft rows; no hard gate is broken, open or closed): the talk moved to everyday
life, which is the gate, and everyday content is the library's least worked. **M1**: of the lines on everyday topics 49%
(frozen) and 51% (fixtures) carry moves, of the rest 79% (Phase 2 tagged the 40 most-spoken families, which were news;
Phase 7 tags every entry). **U2**: everyday scenes are written for anyone, so look-alikes are told apart less (mantles,
Phases 9-14). **X1** (fixtures; a diagnostic): news replies repeat the news's nouns ("Robbed? Who robbed her?"), everyday
replies less; frozen X1 rose. **R8**: everyday scenes use habit symbols more: 0.13% of a speaker's lines against a 15%
ceiling. **B1, B1P** (fixtures, under the card's load of twenty runs): a lull's goals and turns (a turn's opener is a
topic's choice, about 0.4 ms) and longer talks; alone, within budget (above). None was refrozen: Phase 5's gate names K1,
K2 and K3, which were taken into both baselines from this card with C5 and C9 (`--refreeze`, the why kept there), and K1
before that on Phase 4's transcripts (its definition: topics.json's everyday topics, so the player topic, life, times and
price_cause are no longer everyday). C8 is a counted metric now (EVENTS: 51 joins against 55 is noise). The quick card
was blessed again (talk-town talk moved to everyday topics: K1 95.5%, C7 7.0; M1 and U2 as above).

**Tests.** `talk_tests` `agendasAndGoals`: today's book read from its file equals the built-in one number for number;
topics.json's eight refusals; the steps; a pivot along an edge with its facts over the old, answered on the new topic,
then closed; a change of subject now and then, never unmarked; a thread returned to first; three topics at most;
learn_name said for its goal, done and then kept; favour given up when no line fits; invitations, promises, `remember x:`
and `x.said.k`, the ledger's facts as criteria and the refusals of the new keywords; frames.json's goal refusals.
`ambient_tests` `theTalkLedger`: every record and its cap, the least-recently-used cap past 30,000, expiry, save and load,
off; the ledger restored with the world; the director's saturation, the floor, novelty, the told set (and none with
today's numbers), closeness; the daily gossip's notes, and none when off. `talk_beat_tests`: a gossip talk's told set,
the place's count, the note for the two and the joiner, never told again; `pledgesAndCarry` (an invitation kept and one
missed, a promise lapsed, the checkpoint, the carry). `talk_topics` (new ctest). `test_talk_score` (31): C5, C9, K1 by
topics.json, T2 at a line's own hour. `game_tests`: a keeper's greeting waits out a longer talk (and `react` leaves one
who joined a talk alone). The non-database ctest suite passed (92 of 92, the quick card blessed); `talk_check
--validate` on every file and `talk_lint` (9 findings) passed. The database suites weren't run (no schema change). No
ticking runs, no paid calls, no servers.

**For Phase 6.** vent needs `self.mood` (facts.json hasn't it yet); frames.json has no moods. Joins have no stance stake
yet (`join.side`, `join.correct`); the favourite stake is most of frozen C8. `pair_bonus` and `favourites` are tuning knobs
of this phase, not §7.3's: mantles (Phase 9) bring their own favourites. The agenda is the director's topics and the
goals; "needs" (hunger, money owed) are the favour goal only. The heard set is written but the engine doesn't yet refuse a
story a listener has heard (§8.12.4). Threads are returned to from the agenda too, but the card's runs had none to return
to. Frozen K1 is high (75%) because a frozen world keeps the same events and prices all day, and saturation cools them.

## 31. Phase 6, as built (2026-11-16)

Branch `r3-talk-p6`, on Phase 5. Residents now hold views and say them, like and dislike things for good, lean by their
traits, carry a mood, feel the town's, and broaden or plane their speech toward the one they speak to. Four defects heard
in Slice 1's ticking listen are fixed, each with a check that keeps it fixed.

**`Data/Talk/opinions.json`** (§7.6; `Library::readOpinions`, `stances`, `likes`). 13 issues: the Watch, the Houses, the
Restday dole, newcomers, the church, the merchants and their prices, life in the city, the roads, festival days, drink,
the way things are going, the Sump (only for one with a Ridgemere quarter: an issue's `who`), strangers passing through;
each with the topics it comes up on (topics.json's), `for` and `against` leans over the resident's own stable facts
(post, class, quarter, temperament: never the moment, so a view doesn't flicker), each giving its reason in the wolf's
own words, true because its criteria hold ("I walk with them myself, and I know what the nights are like"), and `drift`:
events the game looks for once a game day (`Game::talkDrift`) and keeps in the talk ledger, once each, at most two steps
either way: a theft from them closed unsolved or charged, the Restday dole feeding them, kin of theirs under a warrant, a
wedding of their own, each with a reason of its own. The sum is the stance: `self.op.<issue>` (for, against, mixed),
`op_why`, `op_strength` (0-3; mixed and pulled both ways is 1, pulled by nothing 0), `op_drift`, `op_basis` (what it
stood on: the leans that held and the drift). Not the doc's 20 issues nor a House each: the four Houses wait for their
own facts (a House's trade, its workers) and the mantles' opinions (Phase 9). Checked on load: fields, criteria (the
resident's own only), reasons, events, topics, kinds. **Likes**: 48 likes in four kinds (pastime, food, weather, thing)
and 16 dislikes; each resident holds one of each kind and two dislikes, drawn from their id among those whose `when`
holds (a fisher's knots, no garden for the homeless): `self.likes` and `self.dislikes` are list facts (a new
`FactKind::Type::List`: `self.likes=honey_cakes` holds when the list has it), and `pastime_words`, `food_words`,
`weather_words`, `thing_words`, `dislike_words`, `dislike_kind` give their words. The pastime answer is now theirs every
day (`lk_pastime_own`, priority 3), where `pastimes_animals7.talk` drew a new favourite each time. Stances, likes and
dislikes are worked out once a game day per resident (`Game::talkDaily_`).

**Opinions in talk** (§8.8). In a lull, after a goal and before a turn of topic (`Library::opinion`), the one whose turn
it is may give their view on an issue the topic raises: chance 0.45 at strength 2+, 0.25 at 1, none with no view, times
their lean on give.opinion; one issue a talk, once a game day per speaker and issue (`self.was.aired_<issue>`). The engine
sets `conv.issue` and `conv.issue_words` and gives each speaker `self.stance`, `stance_why`, `stance_strength`, and
the two `conv.agree` (yes, no, half: one has no side); **stance fit** in `Library::answer`: agree (and join.side) only
where they don't differ, disagree (counter, join.correct) only where they do, a counter-opinion only from one who
differs and once. Friends and kin who differ argue a turn (`owes insist|concede`), strangers steer away ("Well. Each to
their own."), one with no view answers short ("Could be."). Whether two agreed is kept between them 30 days
(`pair.agree.<issue>`, read in the next airing as `conv.agreed_before`: "We'll never agree on the Watch, you and I."). A
third who holds the issue strongly (2+) joins with a new stake, `stance` (join.side, join.correct; `Game::stancesOf`, a
game day's cache). `Data/Talk/rules/opinions.talk` holds the lines: per issue (the Watch, the dole, the Houses,
strangers, drink, in dialect symbols: `#watch#`, `#dole#`, `#toffs#`, `#stranger#`, `#ale#`), general ones by side and
strength, the answers, insist and concede, the joins.

**`Data/Talk/temperaments.json`** (§7.5; `Library::lean`). 17 trait values leaning on moves (and `pivot:<topic>`, and
`hedge`: a line saying `#hedge#`, a new plain symbol for a dispreferred answer), each 0.3-2, a speaker's product held to
0.3-2. Applied to every candidate's weight inside its tier (`answer`, `pickOne`, `pickLazy`), to an invitation's chance of
being taken (the move invited), to a pivot and a change of subject in a lull, and to an opinion given.

**`self.mood`** (§8.8; `Game::talkMood`). low, sore, bright or even, from the last three days, fading: mourning, hunger,
a bad hurt, want (low); robbed (a theft, not an attempt), attacked, a quarrel (sore: a grudge aired, offence taken, an
apology refused; the ledger's mood events, 8 a resident, a week); a wedding, a pup born, a new post, praise or thanks
heard, a thriving trade, a festival (bright). The strongest pull of 2 or more wins; `mood_why` says why. **vent**, the goal
Phase 5 left waiting, is in frames.json (a grown wolf's low or sore mood told to a friend, spouse or kin; 3 days), with
its lines by `mood_why` (each led in: "Truth be told, ...", "Between us, ...") and sympathy and reassurance by the other's
mood; the day's talk and "How are you?" show it too. **`world.mood`** (§7.4): frames.json's `moods`, the first that holds
of the town's facts (grief: a death within four days; uneasy: bandits near; festive: a festival today; edgy: an arrest
within a day, the new `world.arrest`), muting moves (no teasing at a wake), boosting others, weighing tones; set by
`Library::accommodate` on each query. `world.mood` beside `self.mood`: a shared fact may now share a person's name, written
in full only.

**Accommodation** (§10.6; `Library::accommodate`, frames.json's `accent`). Each speaker's accent toward the other, every
line: a step plainer toward one a class above, a stranger from elsewhere, a hostile pair; else a step broader toward
kin, friends and the fond who grew up speaking the same dialect (`pair.same_speech`); none rises no higher than light (the
doc's §10.6, so a wolf who lost their accent slips back into it with their own); plain speech (Ridgemere's) is never
shifted. It is the speaker's `speaks`, read before `accent` when a line's words are chosen (a person fact rather than
`conv.accent`, because one query serves both speakers in a lull); `SaidLine::accent` records it, and the kin frame's
`"accent": "+1"` is retired (still read, ignored, for older libraries). **D1 now judges a line by the accent it was spoken
in** (the hard gate: none 0%), and D1A counts the lines shifted.

**The four defects of the Slice 1 listen.**
1. **Pronouns by sex.** `#topic.they#`, `#topic.their#`... (the subject) and `#topic.victim_they#`... (the victim), from
   `topic.subject_sex` and `topic.victim_sex` (`World::talkSexes`, at every topic and the window's gossip), and
   `#self.spouse_their#`, `#other.child_them#`... for one's people (their `<role>_sex`, from `peopleOf`); none of them is
   said without the one it stands for. 185 alternatives in 30 files were rewritten ("Finn Dripwick was robbed. Did you see
   his face, after?"; "Ask Osric. He does the dreaming for both of us."; verbs to agree: "she's", "does he"), and four
   cross-turn ones by hand ("Tell him yourself."). **talk_lint's `they_named`** finds "they/them/their" said after a slot
   naming one known wolf, the town's "they say" apart: 209 found, 185 fixed, 24 reviewed as someone else (the thief
   nobody saw, "all of them", "the little ones") and blessed. **T6**, the same at run time (a resident's name, then
   "they"): fixtures 0.98 → 0.22 per 1,000 lines, frozen 0.06 → 0.018 (what is left is those 24).
2. **News kinds.** et_gos_defend is three scenes: defending one talked of, for ill talk (theft, violence, wanted,
   prowling, a promise broken); pity, for bad news that befell them (death, victim); gladness, for a wedding or a promise
   kept ("Good for him, I say."). et_gos_guard, _chapel, _petty ("had the cheek to sit in the front pew"), _worry, _old,
   _merchant, gs2_heard_bad and et_gs_merchant_pair are held to ill talk too. truth.json's **pity** and **glad** rules,
   scoped by a new `topics` field (gossip, life, news), judge them at run time by the claim kind each line was said under
   (talk_sim now records `claim_kind` and `topic_kind`), and **talk_lint's `gossip_kind`** refuses a gossip line that pities
   or rejoices without its entry gating the kind.
3. **Weather true.** truth.json's **rain_claim** ("rain covers footsteps", "wet night", "soaked through"), **sun_now**,
   **heat_now** (summer) and **storm_now**. et_cr_coast_theft says its rain only under `here.outdoors=rain|storm` ("Dark
   night, quiet paws." otherwise); Ridgemere's quay wagons come in soaked only in the rain, and dry otherwise ("Mark the
   day."); a fisher is "salt to the skin", not "wet through"; a wet greeting answers only in the wet; the Long River and
   Saltwell's hens no longer claim the heat. On P5's own transcripts the new rules find 30 such lines (fixtures, 0.45 per
   1,000); P6's: 1 (0.014), and none frozen.
4. **Never told twice, in the engine.** `Freshness::told` asks whether a listener was told a thing before the talk began
   (the ledger's told set, `claim:<key>`; the heard set, `story:<entry>`, for stories, memories and teases; asked once per
   listener and thing, `AmbientTalk::toldBefore`), and `Conversation::claim` is what the topic is about. The opener is
   refused (`ended` "told", and the game then begins no talk at all, written or not), a turn to an agenda item is skipped,
   an answer that would tell them is passed over. Beside it, **the collision filter** (§8.12.3, `Freshness::collides`):
   another speaker's distinctive words (6 words or more) said in the place within the hour are passed over, the pivot's
   words in front included. It was built here because accommodation's dialect words collided more in the talk town (R5
   an open hard gate): frozen R5 592 → 5.1 per 100 h, fixtures 83 → 0.3.

**The scorecard's new rows** (`tools/talk_score.py`). **T4** (hard, §15.3): of the lines airing a stance (talk_sim's
`issue`, `stance`, `drift`, `basis`), one whose speaker's side changed from their last within 30 days on the same basis
(nothing in their life moved it) and no concede between, or an answer against their own side to the one it is said to.
**T6** (soft): above. **D1** by the accent spoken; **D1A** (diagnostic) the lines shifted; **O1** (diagnostic) opinions
given per 10 talks. T2's rules may be scoped to topics.

**The gate: met** (`artifacts/talk/runs/p6`, every profile in both modes over P0's fixture, pinned to build 81; 13,047
and 25,972 conversations, 71,278 and 168,207 lines; 6.3 minutes; the card was run five times while fixing what it found: the Houses' "great" was Saltwell's
word (D2), Ridgemere's plain speakers were shifted, two old jokes with "dead" and "drowned" in them read as tone clashes
after a pivot (C4), a repeated opinion, a pup venting about money). Fixtures / frozen:
- **T4 = 0 / 0**: 1,064 and 4,048 stance lines, none drifted, none answered against its speaker's side.
- **D1 measured**: none 0% / 0% (the hard gate holds); light 0.83% / 0.93% (P5 2.06 / 1.33), broad 4.02% / 2.87% (0.79 /
  0.48): light speakers among their own now speak broad (D1A: 29.2% / 32.8% of lines shifted, 19,856 / 55,106 a step
  broader, 950 / 112 plainer), so the broad lines carry the dialect words. Both stay far under their targets (25-50%,
  60%+) because 1.4% of the library's text has a dialect symbol at all: Phase 7's discourse symbols and Phase 8's lines.

**The scorecard against Phase 5** (its transcripts rescored with this phase's scorer, `artifacts/talk/runs/p5r`),
fixtures / frozen:

| | Phase 5 | Phase 6 |
|---|---|---|
| **T4** identity drift per 1,000 stance lines | 0 / 0 (no stances aired) | **0 / 0** (1,064 / 4,048 stance lines) |
| **O1** opinions per 10 talks | 0 / 0 | 0.48 / 0.83 |
| **T6** "they" of one known wolf, per 1,000 lines | 0.98 / 0.06 | **0.22 / 0.018** |
| **T2** presupposition failures per 1,000 lines (with the new rules) | 0.45 / 0.036 | **0.014 / 0** |
| D1 / D1L / D1B (%), by the accent spoken | 0 / 2.06 / 0.79; 0 / 1.33 / 0.48 | 0 / 0.83 / 4.02; 0 / 0.93 / 2.87 |
| D1A lines spoken in another accent (%) | 0 / 0 | 29.2 / 32.8 |
| D3 dialect guessed from 20 lines (%) | 84.5 / 70.9 | 86.7 / 81.2 |
| **R5** collisions per 100 h | 83.3 / 592 | **0.32 / 5.1** |
| R4 a distinctive line heard again within 10 h, per 100 h | 460 / 3,818 | 403 / 3,504 |
| R1 first identical talk (h) / R3 distinct openers per 3,000 | 16.2 / 2,517; 2.3 / 1,674 | 15.0 / 2,520; 2.4 / 1,704 |
| C1 / C3 / C7 | 96.9 / 89.4 / 5.59; 98.9 / 90.1 / 6.42 | 97.2 / 89.3 / 5.59; 98.5 / 90.8 / 6.53 |
| C4 / C6 | 0 / 0; 0 / 0 | 0.014 / 0 (one lexicon clash, an open gate in fixtures since P0); 0 / 0 |
| C8 a third speaker (%) | 0.31 / 4.9 | 0.25 / 5.3 (frozen stance joins: 249) |
| C9 callbacks per 10 talks / K1 / K2 / K3 | 1.21 / 44.8 / 21.1 / 0; 0.76 / 74.7 / 13.0 / 0 | 1.21 / 44.4 / 21.1 / 0; 0.79 / 74.2 / 13.1 / 0 |
| T1 / T3 / T5 | 0 / 0.07 / 0.04%; 0 / 0.006 / 0 | 0 / 0.042 / 0.06%; 0 / 0.024 / 0 |
| M1 / U2 / X1 / R8 | 66.1 / 40.1 / 20.2 / 0.104; 55.9 / 29.4 / 16.7 / 0.134 | 66.8 / 39.3 / 19.2 / 0.109; 57.2 / 29.9 / 16.3 / 0.126 |
| B1 / B1P (ms, the card's load) | 0.19 / 0.62; 0.19 / 1.52 | 0.27 / 0.82; 0.27 / 1.77 |
| B5 exchanges a second | 2.2 / 73 | 2.2 / 84 |

Goals pursued (steps), fixtures / frozen: favour 257 / 414, mend 155 / 411, visit 136 / 210, **vent 105 / 191**,
learn_name 101 / 2, nag 10 / 15.

**What the card still flags, and why** (soft rows; no hard gate is broken in either mode): R8, M1, U2 and X1 are the rows
§30 explained (the talk moved to everyday life, the library's least worked) and stand where Phase 5 left them (R8 frozen
0.134 → 0.126; M1 and U2 a little up; X1 a little down). **B1, B1P** (under the card's load of twenty runs): each beat now
also works out accommodation, the town's mood, the stances in play, leans on every candidate, the told set and the
collision filter. Alone it is within budget: `talk_bench` (the talk town) a beat 0.086 ms mean, p99 0.26 (Phase 5: 0.058,
0.19), a look p99 1.1 ms; `talk_bench --world ridgemere3 --hours 20` a beat 0.169 ms, p99 0.93, a look p99 4.9 ms (budget
5: the cold `districtHouse` walks of §29 are most of it, and the next phase that touches the director should warm them).
T4, T6, D1, D1L, D1B, D1A and O1 were taken into both baselines from this card (`--refreeze`, the why kept there): T4
and D1 are the gate's, the rest new rows. The quick card was blessed again (talk-town talk now airs opinions and vents,
speaks broader among its own, and its beats do more: B1 0.069 → 0.10 ms under ctest's load; R5 590 → 0). talk_lint: 34
findings (from 9: the 24 reviewed `they_named` and one `pity`, a dry wedding joke now reworded, are its new ones).

**Tests.** `talk_tests` `opinionsMoodsAccent`: opinions.json's seven refusals and temperaments.json's two; stances from
facts, reasons, strength, no view without a lean or outside `who`, drift with its own reason, events outweighing a lean;
likes the same every time and a list criterion; leans (chatty 1.4, quiet 0.6, a variant's own, held to 2); the moods; the
five accommodation cases, plain speech untouched, and the accent a line records; opinions aired on crime and answered as
the stances are (agree never across sides, disagree never on the same); vent to a friend and sympathy back; a claim and a
story never told twice, another's words passed over; pronouns by sex, and no line without the sex. `ambient_tests`
`theTalkLedger`: drift once an event, two steps at most, moods 8 a resident, saved and loaded, moods gone in a week and
drift kept. `game_tests`: every person fact the library knows, list facts included. `test_talk_lint` (pronouns, news
kinds, weather, scoped rules at run time), `test_talk_score` (T4, T6, D1 by the accent spoken). The non-database ctest
suite passed 92 of 92 (with the quick card blessed); `talk_check --validate` on every file and `talk_lint` passed. The database suites weren't run (no schema change). No ticking runs, no paid calls, no servers.

**For Phase 7.** The opinion lines are move lines, tagged; the 5,477 untagged entries still answer nothing by stance, and
an old line doing agree in a stance talk is held by the engine's stance fit only. The discourse symbols Phase 7 drafts
should include `#hedge#` in every dialect (plain has it now) and the vent lead-ins. Mantles (Phase 9) bring their own
opinions (`opinion road against 2 : ...`): they should set `op.<issue>`, `op_why` and `op_basis` over the computed ones,
and their own likes over the drawn ones (`Library::likes` is a stand-in). `pair.agree.<issue>` is kept in the pair's
note kept tags (`agree.<issue>`). The four Houses as separate issues, the hedge frame (`boss`), and "needs" beyond the
favour goal are not done. talk_bench's look p99 on Ridgemere 3 is at 4.9 ms of 5.

## 32. Phase 7, as built (2026-11-16)

Branch `r3-talk-p7`, on Phase 6. The library is converted: every entry says what it does, the short markers are the
conversation's own tags, the stock short answers and the turns every talk has are symbols each dialect says its own
way, and the coverage the shared lines lack is listed for Phase 8. What a plain speaker can say is what they could say
at Phase 6, word for word (the equivalence test), less four lines fixed by hand.

**The markers to tags (`tools/talk_conv.py`, §8.7).** All 2,652 short markers (`remember pair:` and `remember other:`
of an hour or less; 56 keys, in 2,140 rules) are `{k=v}` tags on every alternative of their rule's `say`, and their
1,837 reads `conv.k` (`self.mem.k` for the five `remember other:`). Every one was read only within its exchange; the
keys meant to last (`served` 7 days, `grumble` and `nb` 3...) were never markers and stay in the talk memory. A tag lives
as long as the talk, so nothing leaks into the same two's next talk or crowds the 32-entry memory. `--check` (ctest
`talk_conv`) fails on a short marker left.

**Backchannels (`tools/talk_migrate.py`, §10.2).** The 76 most repeated short lines ("Good." 201 times, "Always." 162,
"Thank you." 109, "It is." 106...; settings.json's shrugs left alone, because the engine knows them by their words) are
`bc_*` symbols whose plain value is exactly the old words: 2,796 alternatives in 126 files. Each dialect has its own,
1,007 alternatives in all ("Good.": Coldwater "Fair catch.", Frosthollow "Sound.", Larkfield "Full weight.", Mirebridge
"Dry enough.", Oakhaven "Good bark.", Southmeadow "Ripe.", Saltwell "Sweet as well water."); a light accent uses them half
the time, a broad one always. `--check` (ctest `talk_migrate`): plain's value one line, no dialect repeating it, none left
word for word. The often-said ones are under six words where they could be.

**Discourse words (§10.3).** 24 new symbols in plain (`account`, `ack`, `resume`, `preclose`, `excuse`, `knew`, `sympathy`,
`concede`, `insist`, `warn`, `welcome_back`, `welcome`, `sorry`, `forgive`, `deflect`, `no_matter`, `go_ahead`, `put_off`,
`vent`, `reassure`, `congrats`, `demur`, `doubt`, `listen`; 77 alternatives), and all 28 in each dialect with `hedge`,
`pivot`, `anyway` and `later` (399): Coldwater's preclose "The lake waits for nobody", Saltwell's "Stars move; so must I",
Mirebridge's insist "I'll not leave the bank". `rules/discourse.talk` says them (19 move lines: preclose, decline and
put_off with `#hedge#` and `#account#`, disagree, concede, insist, sympathise, reassure, congratulate, apologise and its
acceptance, thanks waved off, a refusal taken well, go_ahead, react.news.known, ack, return, again.open, welcome_join),
and the vent lines are led in by `#vent#` (seven; Phase 6's "Truth be told" and "Between us" among plain's values).
Drafts: §10.3 says the user tunes them by ear. Fewer than §14.1's estimate (about 1,500 backchannel and 860 discourse
alternatives): 1,083 and 476.

**Moves and tones on every entry (`tools/talk_tag.py`).** The 8,350 entries that said nothing of what they did (3,658
scenes, 4,692 rules) have a `does` line, a scene's per turn: all 8,723 say what they do (`talk_check --validate`: none by
alias, none not yet), and M1 is 100% in both modes. The reading: an answer concept's rules make its alias first, always
(a `then` that hands on to it owes that move, and C1 judges by it), and ask back too when most of their words are a
question; a topic's opener by its words (a question asks: how, after, plan, opinion, where, a request, an invitation,
else a detail; news tells it where it speaks of the topic's claim; a grumble, a memory, a plan, a worry, a boast, a view,
a tease only with a tease's words; else it tells of oneself); each later turn of a scene answers what the turn before
made due, chosen among the moves it expects as its words show (react.news.known for "I heard", .new for "Never!", .sorrow
for "Poor soul", answer.* to a question, agree, sympathise, laugh, tease.return...), with a second move when it asks back;
barks are asides; a shop's greeting greets. 102 turns are grave (a death) or serious (a crime) by most of their words,
and no light tone follows grave within a scene. Of the scenes' 1,596 middle turns that owe something, 1,566 are answered
by the next turn's moves as written (the 30 are Phase 2's hand tags). moves.json: `tell.gossip` expects `ask.detail` and
`give.opinion` too, as `tell.news` does ("Who told you that?" answers gossip). "Every family checked by hand" was not
done for all 3,503 families: the 400 most-spoken entries of Phase 6's run were read by hand
(`artifacts/talk/p7-tag-review.txt`) and what they showed went into the reading rules (requests and invitations,
teases only by their words, news only with its claim, a place's name no death); `--review OUT.md` writes every family
for the rest. `--check` (ctest `talk_tags`) fails on an entry with no `does`.

**Coverage (§15.4).** `tools/talk_profiles.py ridgemere3` (read-only: `talk_check --world --residents --json`) writes
`Data/Talk/profiles.json`: 150 replier kinds covering 716 of the 773 placed residents, clustered by stage, job, wealth and
dialect, each with its members' commonest temperament and the facts of the member nearest it, names left out.
`talk_check --coverage [--json] [--holes N]` asks, for every move a line can owe (55), every topic (36), every kind and
four bands (strangers, acquaintances, friends, family), whether a move line (no concept) doing it or a variant of it holds
for one who didn't know the news: 1,188,000 cells in 1.2 s, **663,264 holes (55.8%)**. The core moves: greet.return,
accept_preclose, farewell.return and accept_decline have none; **react.news, react.complaint and answer.detail are holes
in every cell** (no move line answers news for one who hadn't heard it, a complaint, or a question; react.news's four
move lines are "I'd heard"). 30 moves have no move line at all, 23 hold everywhere, answer.how and decline_apology in
part (`artifacts/talk/coverage-p7.md` and `.json`). ctest `talk_coverage` holds the count where it is: a move may not
gain holes (Tests/fixtures/talk_coverage.json).

**The gate: met.**
- **The equivalence test** (`tools/talk_equiv.py --against 1e159420`, ctest `talk_equiv`): every Phase 6 entry is there
  with the same criteria (a converted marker's read now `conv.k`), hand-on, limits, priority and weight, remembering less
  the converted markers; every alternative a plain speaker could say, they can say still, the new symbols expanded to
  plain's values; a converted marker's tag leads every alternative of its rule; a new alternative says a discourse word
  (the vent lines); 19 new entries (discourse.talk). Four entries changed by hand, named with why in its EDITED: the fog
  chowder that said "in this rain" in fog; a weaver's "My nets are tangled past saving" (talk_lint's frozen
  `fisher_self`, now "Look at those nets. Tangled past saving.", true for anyone at the shore; the lint's baseline is 33);
  and the two "Back again?" rules, once a day each (below).
- **The coverage holes listed** (above).

**Found by the card, fixed.**
- **"Back again?" before 83% of a busy place's talks.** With every opener saying what it does, Phase 5's `again_today`
  frame said it before nearly every talk of two who had talked that day (frozen L1: 83% of openers; Phase 6: 4%). The
  again.open lines are once a day per speaker (`cooldown 1d`): 16% of frozen openers, 26% of the fixture's.
- **Collisions in the opener's breath** (`SaidLine::bare`, `Library::preface`). The collision filter judged an opener's
  words before a greeting was said in front of them: two speakers could say the same greeting and the same opener in a
  place within the hour. The ring now keeps the opener's own words beside the whole line, and a preface whose whole line
  another said there within the hour is left out (the quick card's R5 went 7.7 to 0).
- **D2 across a sentence's end** (`tools/dialect_check.py`'s `hits`). A dialect's phrase was found in a line's words with
  the stops taken out, so "Back again? Stone Sunton's been at the wall" was Frosthollow's "Back again, stone" and "Here we
  are again. Mm." Oakhaven's "Again, mm?": eight such in the first card, none a dialect's words. A phrase is now found
  within a sentence, unless its own words run over one ("Too late. Sunk."). And its pattern cache was keyed by an id
  that a freed lexicon's successor can reuse (test_dialect_check gained the check). Phrases that collided with other
  dialects' words or the library's (dialect_check's lexicon) were rewritten before the card: 0 failures.

**The scorecard** (`artifacts/talk/runs/p7`, every profile in both modes over P0's fixture, pinned to build 81; 13,001 and
25,526 conversations, 71,259 and 163,071 lines; the card was run three times, each run finding something above).
Against Phase 6's own transcripts rescored with this phase's scorer (`artifacts/talk/runs/p6r`), fixtures / frozen:

| | Phase 6 | Phase 7 |
|---|---|---|
| **M1** lines carrying moves (%) | 66.8 / 57.2 | **100 / 100** |
| D1L / D1B light and broad lines with dialect words (%) | 0.83 / 4.02; 0.93 / 2.87 | 1.00 / 5.54; 1.28 / 3.49 |
| D1 (none, hard) / D2 (hard) | 0 / 0; 0 / 0 | 0 / 0; 0 / 0 |
| C1 owed answers answered (%) / C2 / C3 | 97.2 / 99.8 / 89.3; 98.5 / 99.9 / 90.8 | 97.9 / 99.8 / 89.6; 98.7 / 99.8 / 90.8 |
| C4 tone clashes per 1,000 / C7 lines per talk | 0.014 / 5.59; 0 / 6.53 | 0 / 5.60; 0 / 6.44 |
| C8 a third speaker (%) / C9 callbacks per 10 talks | 0.25 / 1.21; 5.27 / 0.79 | 0.25 / 2.58; 5.62 / 1.57 |
| R1 first identical talk (h) / R3 distinct openers per 3,000 | 15.0 / 2,520; 2.42 / 1,704 | 6.0 / 2,729; 2.42 / 1,931 |
| R4 / R5 per 100 h | 403 / 0.32; 3,504 / 5.1 | 265 / 0; 2,968 / 5.7 |
| R6 most-spoken line (%) / R7 distinct replies per 1,000 | 1.81 / 235; 2.67 / 113 | 1.85 / 237; 2.75 / 118 |
| T2 / T3 per 1,000 lines | 0.014 / 0.042; 0 / 0.024 | 0.042 / 0.070; 0 / 0.012 |
| T5 told what they knew (%) / T6 "they" of one known wolf per 1,000 | 0.06 / 0.22; 0 / 0.018 | 0.23 / 0.38; 0 / 0.006 |
| K1 / K2 (%) | 44.4 / 21.1; 74.2 / 13.1 | 44.4 / 21.1; 73.5 / 13.5 |
| D3 / U2 / X1 (%) | 86.7 / 39.3 / 19.2; 81.2 / 29.9 / 16.3 | 84.9 / 39.8 / 19.7; 72.7 / 30.7 / 16.2 |
| R8 habit lines / O1 opinions per 10 talks | 0.109 / 0.48; 0.126 / 0.83 | 0.105 / 0.45; 0.134 / 0.81 |
| B1 / B1P (ms, the card's load) / B5 | 0.27 / 0.82 / 2.2; 0.27 / 1.77 / 84 | 0.28 / 0.86 / 1.9; 0.27 / 1.74 / 91 |

`talk_card` exits 1 on soft rows only, the ones it flagged at Phase 6 against these baselines and about where Phase 6 left
them: **R8** (habit lines, 0.13% of a speaker's against a 15% ceiling), **U2** and **X1** (everyday talk, §30), **B1** and
**B1P** (the card's load of twenty runs; alone, `talk_bench` the talk town: a beat 0.089 ms mean, p99 0.24, a look p99
1.35 ms; Ridgemere 3, 20 hours: a beat 0.181 ms, p99 0.89, a look p99 4.81 ms of 5, Phase 6's 4.9). No hard gate is
broken, open or closed. What moved, and why: **M1** is the phase's own (refrozen into both baselines at 100%, the why
kept there, so an untagged line shows); **D1L/D1B** rose a little (the backchannels and discourse words; most of what is
said in Ridgemere 3 is plain, and 1.4% of the library's other words have a dialect symbol: Phase 8's lines and the
mantles); **C9** counts "Back again?" (now once a day each); **R1 fixtures** 15 to 6 h is one listener (L4) hearing the
authored fallback "A day like any other." / "So it is." and the same closing twice, at exchange 54 (the authored route,
not the library); **T2 fixtures** is three lines against one, each an old fault surfaced by a greeting said in front
(a luna table's "Morning to you" at 20h, the frozen lint finding "My son rides with the caravan", Coldwater's
endearment "my catch" read as a fisher's): Phase 8's and the mantles'; **T5 fixtures** is 7 lines of 2,200 told what the
listener knew (soft, within its noise); **D3 frozen** moved on L3's seven judged speakers (one guessed differently).
The quick card was blessed again (talk-town talk says what it does, M1 54% to 100%; its backchannels and discourse words
in dialect, D1L 1.9 to 4.2%, D1B 4.7 to 9.7%; R4 2,782 to 2,318; R6 1.17 to 1.30%: "Rest well." now the commonest night farewell rather than
"Bye, then!").

**Tests.** New ctests: `talk_conv`, `talk_migrate`, `talk_tags`, `talk_equiv`, `talk_coverage`, `talk_p7_rules`
(`tools/test_talk_p7.py`, 12: a marker to tags on every alternative and its read, a key also kept for days left as
memory, `remember other:` read as `self.mem`; whole alternatives only migrated and shrugs left; a dialect repeating plain
refused; questions, requests, invitations and their answers; news met as known, new, or with a question; every answer
meets what was owed; an answer concept's alias first; a death grave and no light after it; the expansions and the
converted read). `talk_tests`: the opener's own words kept beside a prefaced line. `test_dialect_check` (15): a phrase
within a sentence, and across one only where its own words run over. The non-database ctest suite passed, 98 of 98;
`talk_check --validate` on every file; `talk_lint` (33 findings); `dialect_check` (0 failures). The database suites weren't
run (no schema change). No ticking runs, no paid calls, no servers.

**For Phase 8.**
- **The core holes**: react.news (for one who hadn't heard), react.complaint and answer.detail have no move line at
  all, for anyone; then the 30 moves with none (`artifacts/talk/coverage-p7.md`). `talk_check --coverage` is the
  measure and `talk_coverage` holds the count; bless it as holes close.
- **Lines tied to a concept still answer only for it.** Now that every line says what it does, the engine could let a
  concept's rules answer by move as well (§8.13: replies from other files); not done here, because the readings of the
  answer concepts are their alias, coarse (all of `family.answer_child` is answer.detail).
- **Alternatives that differ in kind**: 2,792 of the 12,113 scene turns mix a question with statements, and the turn's
  tag is its majority's; a scene's parallel alternatives would need their own moves (`{does=...}` per alternative, or
  the scene split by thread).
- The discourse words and backchannels are drafts for the user's ear; the dialects' long ones (179 of six words or more
  when drafted) were cut where they're said most, but a broad speaker's "We'll see what nets up." is still the same words
  each time: more alternatives per dialect would help R4.
- The lint's 33 findings (24 reviewed "they") and the three T2 lines above; a luna table's greeting said at night.
- talk_bench's Ridgemere 3 look p99 is still 4.8 ms of 5 (`World::districtHouse`, §29).
