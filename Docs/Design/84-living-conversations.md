# 84. Living conversations: a voice of its own for every wolf, talk that answers itself, and players only in the window

Draft, 2026-11-15, revised 2026-11-16 with the user's answers to all six questions of §21 (the window's free text,
mantles as world database rows, the richer size written by Claude agents, newborns inheriting honesty, births kept
realm-wide, ticking listening runs), for Ridgemere 3 (`ridgemere3`, database `ratw_ridgemere3`; doc 82). **Phase 0 (the
instrument), Phase 1 (truth fixes), Phase 2 (moves and tags), Phase 3 (the turn engine), Phase 4 (beats when spoken,
knowledge, a third voice), Phase 5 (topics, agendas, goals, the ledger), Phase 6 (opinions, temperament, moods,
accommodation), Phase 7 (converting the library; coverage) and Phase 8 (the shared lines, in three sessions) are
built** (2026-11-16, branches `r3-talk-p0` to `r3-talk-p8`; §23, §24, §27-§33 say what and what they
measured), and **Slice 2's end** is done (2026-11-17, branch `r3-talk-s2`, §34: the full suite, every profile, the
ticking 100-hour listen and its packet, the review's fixes, and the first blind packet, rated blind by three raters:
**H1 not met**, the talk rated below the baseline, and why, §34's last part), and its review's 33 findings are fixed
(2026-11-17, §35), and **Slice 3's first round** is done (2026-11-17, §36: the causes of the failed ratings fixed,
a critic's read, the ticking listen, the card and the second blind packet), and §37's hybrid is live, and **Phase 9
(the mantle engine and its tables)** is built (2026-11-17, branch `r3-talk-p9`, §38: the `.mantle` format, migration
0066 applied to DEV, mantles loaded from the rows in one batch into buckets of their own, the hot reload, the checks on
every save, `mantle_load` and `mantle_export`, the test fixture with the ten luna tables ported), and **Phase 10
(passing mantles on)** is built (2026-11-17, branch `r3-talk-p10`, §39: the mantle ledger, release at death, the choice
at birth, the newborn's voice from the card, the name pools, re-drawn marks and echoes, arrivals' bindings, the pools'
health and `mantle_pool.py`, the DM's acts, Atlas's column, sync and packages; not the Mantles panel); nothing else is. It extends doc 62 (the talk library: facts, rules, grammar) and doc 83 (dialogue trees and the window) and
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

## 33. Phase 8, the shared lines (three sessions)

Branch `r3-talk-p8`, on Phase 7; all three sessions write on this branch and worktree. §14.1 sizes the phase at about
1,500 coverage rules and 600 thin-topic rules, in dialect symbols; §18's gate is 0 holes on the core moves, R7 3× the
baseline and C1 92%+. R7 (P0's baseline 54.8 fixtures / 32.3 frozen, so 164 / 97) and C1 already stood at 237 / 118 and
97.9% / 98.7% after Phase 7; the core holes (react.news for one who hadn't heard, react.complaint, answer.detail: 64,800
cells) are the gate's real work, and the rest of §14.1 is the variety the gate can't see.

### 33.1 The plan: who writes what (decided)

| Session | Moves, topics, dialects | Rules, about | Measured by |
|---|---|---|---|
| **1** (this) | **The core moves, deep**: react.news (new, sorrow, glad, doubt, glee) for every news topic and claim kind, by band, stage and temperament; react.complaint (agree, disagree) for every topic that carries a grumble; **ask.detail and answer.detail as pairs**, per topic, true to the topic's facts; and the openings and closings that were thin (greet.return had one line; pups' and the night's farewells are the most-said lines of all). New discourse symbols for them in all eight grammars (`marvel`, `pity`, `gladnews`, `uneasy`, `grumble`, `notsobad`, `dunno`, `hearsay`, `howso`, `rest`). Ridgemere 3's `live.dialects` brought up to the files (below). | 650 planned; **265 built**, with 209 grammar lines (779 alternatives) that say who says it (§33.2) | core holes 0; the card |
| **2** | **The other owed moves**: the 30 with no move line and the thin ones (react.self and its variants, react.story, laugh, assess, congratulate, thank, praise and deflect_praise, tease, tease.return, tease.laugh, take_offence, counter, give.opinion and give.advice everywhere, offer, accept, refuse, grant, answer.how, answer.plan, answer.whereabouts, answer.check_in, keep_confidence, repair and clarify, extend, decline_apology), about 480; then the moves nothing owes: **pivots for topics.json's 109 edges** (a line made for each edge where `#pivot#` alone reads thin, about 150), joins by stake (60), breaks and returns (40), callbacks on the ledger's facts (80). Every dialect through symbols, a few more where a move needs its own word. | 950 (850, and a share of session 1's); **515 built**, with 94 grammar lines (392 alternatives) that say who says it and the words they say it with (§33.3) | the hole rate under 10%; C9; C5 stays 0 |
| **3** | **The thin topics** (§14.1): gossip 70→250, event 49→150, news 43→150, newcomer 52→120, harvest and ships 11→60 each, deed 21→80, accent 26→60, times 21→60: openers and their answers, gated to the facts (claims.json's kinds, `topic.kind`, the towns' trades); the scene turns whose alternatives differ in kind (2,792 of 12,113: a question beside statements) given moves of their own or split; dialect variants (§10.5) for the most-spoken shared lines where one is natural; then the phase's gate on the whole. | 700 (600, and the rest of session 1's); **844 built**: every thin topic at its size, the caravan's thin kinds, more ways for session 2's most-heard lines, the night's goodbyes in every dialect; the scene turns not split (§33.4) | the phase gate; K1-K3 hold; R4, R6 |

Every session: the new rules in `Data/Talk/rules/shared8_*.talk`, move lines (no concept) unless a topic's opener is
the point; often-said lines under six words; criteria true to the moment (`truth.json`, `talk_lint`), the claim kind
gated wherever a line pities or rejoices; pronouns by `#topic.they#` and the like, never "they" for one known wolf;
`dialect_check` clean; `talk_check --coverage` re-blessed into `Tests/fixtures/talk_coverage.json` as holes close; the
card at the end, and `live.dialects` brought up to the grammar files when they change (`tools/talk_seed.py --only
dialects`).

### 33.2 Phase 8 (session 1 of 3), as built (2026-11-16)

**The core moves have no holes.** `talk_check --coverage`: the core moves' 64,800 holed cells are 0 (react.news 141 move
lines, react.complaint 43, answer.detail 48, greet.return 7, accept_preclose 4, farewell.return 13, accept_decline 5);
the whole matrix 663,264 → 555,264 holes of 1,188,000 (55.8% → 46.7%: ask.detail, react.news.doubt and the
complaint's and news's variants filled with them). `Tests/fixtures/talk_coverage.json` is blessed at the new counts, so
none of them may come back.

**What was written** (`Data/Talk/rules/shared8_*.talk`, 265 move lines, 1,017 alternatives, many of them symbols that
say more; every one a move line: no concept, found by its move):
- `shared8_news.talk` (124): react.news for one who hadn't heard, new, sorrow, glad, doubt and glee, by topic and claim:
  every claims.json kind in gossip and crime (theft, violence, victim, wanted, marriage, death, newcomer, prowling,
  promises kept and broken), the roads (a camp, a raid, by carter, guard, merchant, farmer, the poor, the nervy), the
  wagons by `topic.kind` (arrived, late with its `topic.why`, due, left, from `topic.where`), prices up and down by
  what and who pays, a price's cause, the ships by kind (in, due, stormbound, lost, sailed), the market's event, the
  Times, the harvest, festivals, the town's news (a birth, a wedding, a death, a post, an apprentice, an arrest), a
  traveller's deed, and everyday news; doubt for everything but a death, a wedding or a victim.
- `shared8_complaint.talk` (43): react.complaint, agree and disagree (hedged, with a reason), by topic (prices, the
  Times, work and the levy, the house, family, weather by season and the sky, health, food and hunger, the roads, the
  Watch, crime, a trouble told in passing) and by the speaker's lot (the poor, the comfortable, the rich, the merchant,
  the homeless, the farmer, the healer, the guard).
- `shared8_detail.talk` (74): ask.detail and answer.detail as pairs, topic by topic. A move line answers any question on
  its topic, so what it says is true whatever was asked: on news, where the teller had it (`#hearsay#`, never "I saw"),
  the topic's own facts (`#topic.subject#`, `#topic.victim#`, `#topic.where#`, `#topic.price#`, `#topic.cause_words#`
  but never a cause the move belies, `topic.cause=still`; `#topic.who#`, `#topic.ship#`, `world.event_words`,
  `world.times_cause`, `here.festival`), or that there's no more; on everyday talk, the ordinary evasions ("Now there's a
  question.", "Oh, this and that."). A traveller standing near (the player topic) is asked about and answered for.
- `shared8_turns.talk` (24): greet.return, accept_preclose, farewell, farewell.return and accept_decline beside
  turns.talk's; the pups' and adolescents' goodbyes to their own and to the grown.

**Who says it: a grammar's criteria, not a rule's** (decided). The first talk-town listen made the friends' farewell
"See you soon." the most-said line of all (249 of 14,213): a rule's criteria make it outrank every rule with fewer (the
tiers, §8.2), so a rule gated to friends was 70% of all friends' farewells, from four alternatives. Everything that says
*who* rather than *what about* is now a plain.grammar symbol with criteria entries (the most specific that holds is said;
ties are one list): `#s8_hello#`, `#s8_okay#`, `#s8_bye#`, `#s8_byeback#`, `#s8_nevermind#` (by band, hour, warmth,
humour, piety, work, age), `#s8_pupbye#`, `#s8_pupbyeback#`, `#s8_teenbye#` (a pup to a pup or to the grown), `#s8_wow#`,
`#s8_gripe#`, `#s8_evade#`, `#s8_ask#` (the merry laugh, the cold shrug, the chatty want everything, the nervy fret, a
pup asks if it's true): 94 entries, 467 alternatives. Rules gate on the topic and the speaker's facts (job, role, wealth), which are the
coherence §8.2 means to lead; temperament is the speaker's way of saying it. The five closings have each dialect's own
values (Coldwater "Off with the tide", Frosthollow "Dig safe", Larkfield "Keep a full purse", Mirebridge "Stick to the
boards", Oakhaven "Root well, old root", Southmeadow "Good gleaning", Saltwell "Shade on your way"): 35 lines, 133
alternatives, every one under six words (`talk_p8_rules`). The pups' own goodbyes and the night's stay rules (their tier
was turns.talk's already).

**Discourse words** (§10.3): ten new symbols in all eight grammars, `marvel` (news one hadn't heard), `pity`, `gladnews`,
`uneasy`, `grumble`, `notsobad`, `dunno`, `hearsay`, `howso`, `rest` (for the dead): 80 lines, 179 alternatives. Coldwater "Up from
the deep, that", Frosthollow "That's bad air", Larkfield "That's ill weight for trade", Mirebridge "Something's stirring
the mud", Oakhaven "There's a wind in the branches", Southmeadow "That's blight on the wind", Saltwell "Sand on that
wind, my friend". Drafts for the user's ear, as Phase 7's were; five collided with words the regional scenes say
(`dialect_check`) and were rewritten.

**Truth.** Pity and gladness only where the claim's kind (or the topic's kind, a price's direction, the harvest or the
Times) says so; a line that says the news is new is react.news.new (never said by one who knew) or tests
`self.knows!=yes`; pronouns by `#topic.they#`/`#topic.them#`/`#topic.their#` (he or she, with the verb to agree); no
"my" anything a fact doesn't back (an "at ours" for one with a home; "my stall" was rewritten "where I shop"); ships by
`#topic.ship.capitalize#` (its name carries "the"). `talk_lint` 33 findings, none new; `dialect_check` 0 failures.

**`asserts none`** (new, `Library::parse`, `Entry::NeverAsserts`): answer.detail asserts the topic's claim by its move's
default (§7.1), so "Now there's a question." passed the claim on again, and to one who had it told them what they knew
(T5). An answer that says nothing writes `asserts none`; the evasions and "That's all I was told." do.

**A scene's queued turn and the collision filter** (`Library::next`, §8.12.3). The quick card's R5 (a hard gate it
passed) broke on one event: two talks in a place chose the same scene within the hour, and the second reached a later
turn ("Every season, same as the lake.") after the first had said it; the filter judged a scene's words only when it was
chosen. A queued turn is now checked when it comes due, as Valve's rule checks its moment: another speaker's words there
within the hour drop the rest of the scene, and what was owed stays owed, so the engine answers it (`talk_tests`).

**Ridgemere 3's dialects were Phase 6's.** `live.dialects` (the game reads it over the files, decision 7) still held
the grammars as they were before Phase 7: the card's ridgemere3 runs never heard Phase 7's backchannels or discourse
words in any dialect (235 Coldwater broad lines said "shore-friend", none "Fair catch." or "The lake waits for
nobody"). `tools/talk_seed.py --only dialects` (new; re-runnable, the rows it changes dumped first) wrote the seven rows
from the files: `Saved/Backups/ratw_ridgemere3-dialects-20261116-182403.sql` (Phase 6's rows) and `-184231.sql` (one
Mirebridge fix since). Until this branch merges, the main checkout's `dialect_check --world ridgemere3` will report its
older files differ from the rows; a server already running keeps what it loaded. Two Mirebridge backchannels whose
", no?" turned a scene's echo into the same question twice (C6, "Mostly, no?" / "Mostly, no?") were made statements.
The dialects are at 12-16k characters of the column's 20,000 (Saltwell 15,745): sessions 2 and 3 should spend
dialect words where they're said most.

**The town's gossip answers** (`rules/town7_people.talk`, named in talk_equiv's EDITED with why): the card's first run
heard "Ha! I'll never look at Gunnar Harrowgate the same." answer a death (C4). The merry, dry, sly, chatty, old, young
and nervy answers now hold only for a claim that isn't a death, a hurt or a victim (`n7_ga2_death*` are the death's),
the cold, devout and priest's not for a death; and "they" of one wolf in `n7_ga2_warm` and the three death answers is
`#topic.they#`/`#topic.them#` ("I saw them only the other day" was an eyewitness who saw nothing: "No. Not him. Surely
not." now).

**The scorecard** (`artifacts/talk/runs/p8s1`, every profile in both modes over P0's fixture, pinned to build 81, with
Ridgemere 3's rows now speaking the files' dialects; run three times, each finding something above). Against Phase 7's
card (`artifacts/talk/runs/p7`; the scorer is unchanged), fixtures / frozen:

| | Phase 7 | P8, session 1 |
|---|---|---|
| core coverage holes (of 64,800) / all holes (%) | 64,800 / 55.8 | **0 / 46.7** |
| R7 distinct replies per 1,000 (gate: 3× P0's 54.8 / 32.3) | 237 / 118 | **242 / 121** (4.4× / 3.7×) |
| C1 owed answers answered (%, gate 92) | 97.9 / 98.7 | **97.9 / 98.7** |
| R6 the most-spoken line (%) | 1.85 / 2.75 | **1.29 / 1.32** |
| R1 first identical talk (h) | 6.0 / 2.42 | **27.5** / 2.42 |
| D1L / D1B light and broad lines with dialect words (%) | 1.00 / 5.54; 1.28 / 3.49 | **1.67 / 10.7; 2.01 / 6.80** |
| D3 dialect guessed from 20 lines (%) | 84.9 / 72.7 | **89.6 / 83.2** |
| D1 (none, hard) / D2 (hard) | 0 / 0; 0 / 0 | 0 / 0; 0 / 0 |
| R3 / R4 per 100 h | 2,729 / 265; 1,931 / 2,968 | 2,740 / 265; 1,931 / 3,060 |
| R5 collisions per 100 h | 0 / 5.65 | 0 / 6.56 |
| C3 / C7 / C8 / C9 | 89.6 / 5.60 / 0.25 / 2.58; 90.8 / 6.44 / 5.62 / 1.57 | 89.8 / 5.62 / 0.24 / 2.59; 90.8 / 6.45 / 5.93 / 1.56 |
| C4 per 1,000 / C6 per 1,000 | 0 / 0; 0 / 0 | 0.028 / 0; 0 / 0 |
| T1 / T2 / T3 per 1,000 | 0 / 0.042 / 0.070; 0 / 0 / 0.012 | 0 / 0.028 / 0.084; 0 / 0 / 0.012 |
| T5 (%) / T6 per 1,000 | 0.23 / 0.38; 0 / 0.006 | 0.78 / 0.41; 0 / 0.006 |
| K1 / K2 / K3 | 44.4 / 21.1 / 0; 73.5 / 13.5 / 0 | 44.5 / 21.0 / 0; 73.7 / 13.4 / 0 |
| U2 / X1 / R8 | 39.8 / 19.7 / 0.105; 30.7 / 16.2 / 0.134 | 39.6 / 19.6 / 0.110; 30.0 / 16.3 / 0.136 |
| B1 / B1P (ms, the card's load) / B5 | 0.28 / 0.86 / 1.9; 0.27 / 1.74 / 91 | 0.30 / 0.84 / 1.2; 0.30 / 1.85 / 86 |

**The gate, so far** (it is the phase's, judged at session 3's end): **0 holes on the core moves: met.** **R7 3× the
baseline: met** (4.4× and 3.7×). **C1 92%+: met** (97.9 / 98.7). `talk_card` breaks no hard gate the baselines passed
in either mode and exits 1 on the soft rows Phase 6 and 7 flagged and left where they were (R8 0.11-0.14% against a 15%
ceiling, U2 and X1: everyday talk, §30; B1 and B1P under the card's twenty runs at once). What moved, and why: **R6, R1
fixtures** fell and rose because the closings that were most of all talk's last lines now have hundreds of ways to be
said (the pups' "Bye!" and "See you!" were 2-3% of all lines each); **D1L, D1B, D3** rose because Ridgemere 3 now speaks
Phase 7's and this session's dialect words (above), D2 staying 0; **C4 fixtures** is two lexicon clashes in old content
(a grumble that the Houses' wagons are "robbed" answered "Ha! Get it all out now", `n7_ga_merry`; an open gate in
fixtures since P0, within its noise), after the death answered with a laugh was fixed; **R5 frozen** is the authored
route's "Quite a feast for the Blossom Fair." (the Mind's fallback, not the library: an open gate since P0); **T5
fixtures** is 17 lines of 2,192 (from 5), told to one who knew: scenes (`em_band_strangers_road`,
`em_rm2_watch_bandits`) the closings' new sampling reached, and two answers naming the one talked of to one who knew and
asked (they assert by design), soft and within its noise; **B5 fixtures** is the fixture's cold restarts under load
(1.2 against P0's 1.5; frozen 86). The quick card passes unblessed (its R6 1.30 → 0.76%, D1B 9.7 → 11.5%).

**Tests.** `talk_p8_rules` (new ctest, `tools/test_talk_p8.py`, 9): every shared line a move line with no concept and a
move; every symbol it says defined in plain; the ten discourse words and five closings in all eight grammars, plain's
who-says-it symbols in plain; every closing under six words in every voice; feelings for the one talked of gated to the
claim; news to one who hadn't heard never "I'd heard"; sorrow and glee for one who didn't know; no "they" after a named
wolf; no "dog", no "tells". `talk_tests`: a scene's queued turn another said here within the hour is dropped and what was
owed is answered; `asserts none` passes nothing on where answer.detail would. `talk_coverage` re-blessed (core 0). The
non-database ctest suite: 99 passed, the 8 database suites skipped (the quick card failed once under `-j8`, a timing
row under load, and passed alone and in the suite run again); `talk_check --validate` on every file; `talk_lint` (33, none new); `dialect_check` (0
failures); `talk_equiv --against 1e159420` (18 entries changed by hand, each named). No ticking runs, no paid calls, no
servers.

**For sessions 2 and 3.**
- Write *who says it* as grammar criteria (§33.2's `s8_*` symbols, or new ones), *what it's about* as rule criteria; a
  rule gated only to a temperament or a band becomes most of that speaker's lines.
- Move lines are the floor: a line handed on to a concept is answered by that concept's rules first (fit 3 against a
  move line's 1, §8.2), so the shared lines are heard where those don't hold (about 66 of 20,000 frozen L1 lines, beside
  the closings, which are heard everywhere; the holes left are `artifacts/talk/coverage-p8s1.md`). Their weight is coverage; their audience is every reply nothing else fits,
  and every mantle (Phase 9) whose line owes a move no rule of its own answers.
- `tools/talk_seed.py --only dialects --world ridgemere3` after any change to a dialect's grammar (with the main
  checkout's Database/.env in the environment from a worktree), and keep each under the column's 20,000 characters.
- answer.detail and other asserting moves: `asserts none` on a line that says nothing.
- The 30 moves with no move line (assess, react.self, react.story, laugh, tease and its answers, take_offence, counter,
  give.opinion and give.advice everywhere, offer, accept, refuse, grant, the other answers, keep_confidence, clarify,
  extend, praise, deflect_praise...) are session 2's (§33.1), with the pivots, joins, breaks, returns and callbacks.

### 33.3 Phase 8 (session 2 of 3), as built (2026-11-16)

**Every owed move is answered everywhere.** `talk_check --coverage`: the 25 moves with no move line and the two held in
part (answer.how, decline_apology) are written, so the whole matrix is 555,264 → **0 holes of 1,188,000** (46.7% →
0%: session 2's measure was under 10%). `Tests/fixtures/talk_coverage.json` is blessed at 0, so no move may gain a
hole again; `artifacts/talk/coverage-p8s2.md` and `.json` are the listing.

**What was written** (`Data/Talk/rules/shared8_*.talk`, **515 rules, 1,857 alternatives**; move lines but for the
callbacks' openers, §33.1):
- `shared8_react.talk` (87): react.self by the move before it (a boast is glad news, answer.how.poorly isn't), its tone
  (light, or serious, tender, grave) and its topic, with the glad, sorry and wry variants; react.story for tales,
  memories (by who remembers: `s8_memory`), lore told gravely; laugh, never at what was said gravely
  (`conv.tone!=serious|grave|tender`); assess, good and bad, by the move (advice, an opinion, a complaint) and the
  topic (a theft, a death, a wedding, prices up and down, the times, the road), never glad of ill news.
- `shared8_social.talk` (60): tease (at a boast, a view, oneself, by topic; strangers never, the frames'), tease.return,
  tease.laugh, take_offence (seldom: weight 0.15-0.3, six words or a hedge), counter (weight 0.25-0.5), praise,
  deflect_praise, thank by what it thanks (a warning, advice, comfort, praise, a favour, a secret kept, an offer),
  keep_confidence, congratulate (a boast, a plan, by topic, to a pup).
- `shared8_offers.talk` (70): offer (help, a hand, company: what a talk can give), accept, refuse (softened and
  explained), grant, give.advice by topic and by the asker's trade and state (hungry first, a pup, a plan), give.opinion
  on the everyday run of things by topic (never a claim, never of the subject of gossip but of the deed), and agree by
  topic, weighted over the rest so a view is mostly met with a nod.
- `shared8_answers.talk` (61): answer.how and its variants true to the speaker's own facts (well only where nothing ails
  them; hungry, hurt, low, sore, robbed, homeless, busy at work, grieving in mourning, bright with a pup born, a wedding,
  a good trade; else the deflections), answer.plan by the hour, the shift, Restday, a festival, the stage and the trade
  (fourteen trades), answer.whereabouts that fits both "where've you been?" and "where are you off to?",
  answer.check_in, clarify, extend (seldom, its moment the grammar's `s8_extend`: love to a friend's house, the ice,
  the rain, the dark, the road, the festival) and decline_apology (seldom).
- `shared8_pivots.talk` (115): a line for every one of topics.json's 109 edges and the commonest turns within a topic
  (another price, wagon, ship, crime, piece of news), each a bridge from the old topic to the new that claims nothing
  the opener after it might not bear out ("And it all comes in by wagon." then "The Larkfield wagon's late."), 3-5
  ways each, the most-heard edges more.
- `shared8_joins.talk` (67): joins by stake and topic (a friend of both who likes to talk, on food, work, prices,
  gossip, the road, the wagons, a festival, the market's doings, faith; a favourite topic's chatty wolf, on pastimes,
  beasts, small talk, food, health, travel), join.tease, one who deals in the goods, a witness (who says they were by,
  never an eyewitness's "I saw it": nobody's sight is claimed the game didn't give them), the one talked of (never a
  denial of a true claim: nobody lies in this build, §8.6) and their kin and friends (who may deny it as they believe
  it), the welcome by stake (`s8_welcome`); breaks by what broke the talk, where and when (rain where they stand in
  the open, a storm, a fight in a tavern or a market, a guard's duty, a customer at the window, a pup's "Rain! Run!");
  returns on the topic left unfinished.
- `shared8_callbacks.talk` (55): the two were on this lately, or how it went (below); long-missed friends' openers
  (`pair.last_talk>=7`, owing answer.whereabouts or answer.check_in); an invitation kept or missed; kin's first words
  of the day (kin.open, which had no line: "Mum.", "There's my pup.", "There you are, love."; after they've been apart
  a while, never again the same day, by the hour).

**Who says it: the grammar's** (§33.2's rule, kept). 94 new plain.grammar lines (392 alternatives): twelve discourse
words in all eight grammars (`funny`, `cheek`, `modest`, `secret`, `advise`, `helpout`, `wellsaid`, `poorshow`, `fine`,
`planless`, `pardon`, `tale`; 84 dialect lines, 168 alternatives: Coldwater's "Not a ripple from me", Frosthollow's
"Take it from an old pick", Larkfield's "That weighs right", Mirebridge's "Says the wet-foot", Oakhaven's "Hark at the
sapling", Southmeadow's "Sound as a sheaf", Saltwell's "That is wise as water") and plain's who-says-it symbols
`s8_self`, `s8_laugh`, `s8_teaseback`, `s8_thanks`, `s8_praise`, `s8_modest`, `s8_sure`, `s8_fine`, `s8_story`,
`s8_memory`, `s8_extend`, `s8_welcome` (a pup laughs "Hee!", a dry wolf "Very droll.", an old one "Creaking, but
here."; to a pup, "Clever pup!"). A rule that must weigh little (an extend, a welcome by stake) says a symbol and keeps
no criteria of its own, so it never outranks the line it stands beside: the first smoke run made "Oh, and give my love
at home." 57 of 20,000 lines from a rule gated to friends (§8.2's tiers), and the favourite's welcome 124.
Ridgemere 3's `live.dialects` were brought up to the files again (`tools/talk_seed.py --only dialects`, the rows dumped
first: `Saved/Backups/ratw_ridgemere3-dialects-20261116-193715.sql`); Saltwell's grammar is 16,620 characters of the
column's 20,000.

**The engine** (`Core/RatwTalk.cpp`, `Core/RatwGameAmbient.cpp`):
- **A pivot's own line** (§8.5: "said by a line made for that edge or through #pivot#"). In a lull's turn along the
  graph, `Library::lull` asks first for a `pivot` move line whose criteria hold with `conv.from` (new in facts.json:
  the topic it leaves) and the new topic's facts, and says it in front of the opener (the collision filter judging the
  whole line), else `#pivot#` as before.
- **Recall, a callback in the opener's breath** (§8.7). A new move, `recall` (moves.json, family topic): in
  `Library::preface`, when no thread is returned to, the ledger has the pair on the talk's topic (`pair.talked.<topic>`)
  or their last talk was broken off, half the time, a `recall` line that holds is said before the opener ("Every
  time we meet, it's money. Twenty-seven pence the dozen, still. You'd think it'd have come down by now."), and a prefaced line that stands on the
  ledger is a callback (`SaidLine::callback`, C9). The callbacks first written as topic openers replaced the topic's
  own opener with talk about talk ("Beasts again. We're a pair.") and were heard 15-18 times in 30 hours; as a preface
  the opener keeps its news.
- **Joins by topic.** `Library::join` gives the joiner's line the talk's topic (`conv.topic`), which it lacked: the
  topic's joins never held in the first smoke run. A friend of both who likes to talk may cut in with a tease
  (`Game::joiners`: the friends stake asks join.add or join.tease, §8.9's).
- **No everyday view against a stance.** Once one of the realm's issues is aired (`conv.issue`), a move line's
  give.opinion or counter that reads no issue, agreement or stance is left unsaid, and so is an everyday tease or piece
  of advice said straight back to a stance line: the shared lines' views ("The Watch should look sharper.") never
  speak against a speaker's own side (T4), and "Don't pay the first price asked." no longer answers a view on the dole.

**Truth.** Laughs gated by tone; how one is, by one's own facts; plans by one's own shift and trade; "lately", not "last
time", where the ledger says only that the two talked of a topic; every recall names the topic it's gated to (the first
run's "Here we are on pastimes again." on friends); no "Talking of X." bridges where X is the topic turned to (19
rewritten); pronouns by `#topic.them#`/`#topic.their#`, no "they" for one known wolf ("I miss them." for the mourned was
cut); a festival join only on a festival (one answered drovers in the yard). `talk_lint` 33 findings, none new;
`dialect_check` 0 failures (three of the new lines and two dialect words collided with other dialects' lexicons and
were rewritten); `talk_migrate --write` turned the session's word-for-word backchannels into their symbols.

**The scorecard** (`artifacts/talk/runs/p8s2`, every profile in both modes over P0's fixture, pinned to build 81, with Ridgemere 3's rows speaking the files' dialects; run three times: the first two found what is fixed above and the clash below, and the frozen profiles were run a fourth time after that fix, the fixtures' runs kept). Against session 1's card (`artifacts/talk/runs/p8s1`; the scorer is unchanged),
fixtures / frozen:

| | P8, session 1 | P8, session 2 |
|---|---|---|
| coverage holes (of 1,188,000) / core holes | 555,264 (46.7%) / 0 | **0 (0%)** / 0 |
| R7 distinct replies per 1,000 (gate: 3× P0's 54.8 / 32.3) | 242 / 121 | **250 / 125** (4.6× / 3.9×) |
| C1 owed answers answered (%, gate 92) | 97.9 / 98.7 | **97.9 / 98.6** |
| **C9** callbacks per 10 talks | 2.59 / 1.56 | **3.65 / 2.16** |
| **C5** unmarked jumps per 1,000 lines | 0 / 0 | **0 / 0** |
| C8 a third speaker (%) | 0.24 / 5.93 | 0.27 / 6.45 |
| R6 the most-spoken line (%) / R3 distinct openers per 3,000 | 1.29 / 2,737; 1.32 / 1,930 | 1.25 / 2,786; 1.29 / 2,049 |
| R1 first identical talk (h) | 27.5 / 2.42 | 27.5 / 1.77 |
| R4 / R5 per 100 h | 265 / 0; 3,059 / 6.56 | 249 / 0; 3,069 / 6.34 |
| C3 / C7 | 89.8 / 5.62; 90.8 / 6.45 | 89.4 / 5.64; 90.7 / 6.44 |
| C4 per 1,000 / C6 | 0.028 / 0; 0 / 0 | 0.014 / 0; 0 / 0 |
| T1 / T2 / T3 per 1,000 | 0 / 0.028 / 0.084; 0 / 0 / 0.012 | 0 / 0.042 / 0.098; 0 / 0 / 0.012 |
| T5 (%) / T6 per 1,000 | 0.78 / 0.41; 0 / 0.006 | 0.14 / 0.31; 0 / 0.012 |
| K1 / K2 / K3 | 44.5 / 21.0 / 0; 73.7 / 13.4 / 0 | 44.4 / 21.2 / 0; 73.9 / 13.2 / 0 |
| D1 / D1L / D1B / D3 (%) | 0 / 1.67 / 10.7 / 89.6; 0 / 2.01 / 6.80 / 83.2 | 0 / 1.79 / 10.4 / 89.7; 0 / 1.86 / 6.83 / 82.2 |
| U2 / X1 / R8 / O1 | 39.6 / 19.6 / 0.110 / 0.47; 30.0 / 16.3 / 0.136 / 0.79 | 40.0 / 19.5 / 0.104 / 0.45; 32.1 / 16.1 / 0.149 / 0.80 |
| B1 / B1P (ms, the card's load) / B5 | 0.30 / 0.84 / 1.2; 0.30 / 1.85 / 86 | 0.31 / 1.03 / 0.87; 0.22 / 1.10 / 108 |

**The gate, so far** (the phase's, judged at session 3's end): **0 holes on the core moves: met**, and on every move.
**R7 3× the baseline: met** (4.6× and 3.9×). **C1 92%+: met.** Session 2's own: **the hole rate under 10%** (0%),
**C9** up by 41% in both modes (the recalls, the kin's first words, the long-missed), **C5 0**. `talk_card` breaks no
hard gate the baselines passed in either mode and exits 1 on the soft rows session 1 left (R8 0.10-0.15% against a 15%
ceiling, U2, X1, B1, B1P), with B5 fixtures below session 1's on an open hard gate (below). What moved, and why:
- **C4 frozen** broke its hard gate (one clash in 163,000 lines) on the second card: an old chain the new pivot from
  the traveller to the road reached (`e5_been_old_broad`'s "Been to the next town, once. For a funeral. Didn't care for
  it." laughed at by tale.react's merry answer). The old line says "For a wedding." now (named in talk_equiv's
  EDITED), and the frozen card run again: 0. C4 fixtures is one old lexicon clash (an open gate since P0).
- **T3 fixtures** (0.084 → 0.098) is the old eyewitness lines ("I saw someone near X's door that night.") sampled more
  where a recall leads into them; the session's own witness joins said "I saw it with my own eyes." to the scorer, which
  backs only the teller's own sight, and were rewritten ("I was right there, as it happens."). **T2 fixtures** is
  Phase 7's two old faults (a luna table's "Morning to you" at 20h, Coldwater's "my catch"). **T6 frozen** is two old
  lines about one wolf's family ("All of them."). All open gates since P0, none the session's words.
- **R1 frozen** 2.42 → 1.77 h is L1's authored route ("A day like any other." / "It is at that."), the Mind's fallback,
  not the library (an open gate since P0, as Phase 7 found).
- **B5 fixtures** 1.21 → 0.87 exchanges a second: the fixture runs took 600 s against session 1's 460 with the machine
  shared (load 7); the frozen ones run alone came out faster than session 1's (108 against 86). A beat costs what it did:
  `talk_bench` (alone) the talk town a beat 0.10 ms mean, p99 0.27, a look p99 1.6 ms, within budget; Ridgemere 3, 20
  hours, a beat 0.195 ms, p99 0.97, a look p99 5.1 ms, over its 5 ms, **and the same without this session's seven
  files** (5.09 ms: the look is `World::districtHouse`'s cold walks, §29, at 4.8 when Phase 7 measured it). The
  recall is asked only where the ledger has the topic, so it costs a look nothing otherwise.

**Tests.** `talk_tests` `sharedLinesTwo` (new): the pivot from prices to the caravan said by its own line in front of
the opener and never another step's line; a recall in the opener's breath about half the time where the ledger has the
topic, a callback, and none without it; an everyday view left unsaid in an issue's talk and said where none is.
`talk_p8_rules` (12): the twelve words in every grammar and plain's who-says-it symbols; every pivot names a step
topics.json has; laughs gated by tone; no view or pushback reads an issue; the dispreferred and the seldom weigh little
and an extend keeps no criteria; how one is, by one's own facts; a concept only in the callbacks, and only where the
ledger is read. `talk_coverage` re-blessed at 0. The non-database ctest suite: 99 passed, the 8 database suites skipped, and the quick card failed on D1A alone (a diagnostic: the lines said a step broader or plainer, 17.1% against 19.7, and 18.4-23.4% over four other seeds of the same town), so it was blessed again with the town's talk as this session changed it (`talk_scorecard_quick.json`: R6 1.30 → 0.89%, R7 205 → 213, C9 1.32 → 1.37, D1B 9.7 → 11.6%, R4 2,318 → 2,550, the shared lines 163 of its 14,237 lines); `talk_check --validate` on every file; `talk_lint` (33, none new); `dialect_check` (0 failures); `talk_equiv --against 1e159420` (19 entries changed by hand, each named). No ticking runs, no paid calls, no servers.

**For session 3.**
- **The share left.** Session 2 wrote 515 of its planned 950 (the owed moves 278 of 480, the pivots 115 of 150, the joins, breaks and returns 67 of 100, the callbacks 55 of 80): the phase has 780 of §14.1's 2,100 rules after two sessions, so 1,320 remain against session 3's planned 700. The gate doesn't see the difference (every cell holds; the variety rows are met), so session 3 should spend it where it's heard: the thin topics' openers and answers first, then more ways to say the most-heard shared lines (the favourite's join and welcome, thanks for advice, agreement on prices, the extend), and only then more move lines.
- The thin topics (§33.1: gossip 70→250, event 49→150, news 43→150, newcomer 52→120, harvest and ships 11→60 each,
  deed 21→80, accent 26→60, times 21→60), the scene turns whose alternatives differ in kind, and dialect variants for
  the most-spoken shared lines; then the phase's gate on the whole, with the ticking slice-end listen after.
- Most of what is heard is still concept rules' answers and the authored route: move lines answer where no concept
  rule holds (the session's lines were 1-2.5% of what the card heard, the joins, welcomes, extends, thanks and agreement
  most); the thin topics' openers are where new lines will be heard most.
- Saltwell's grammar is at 16,620 of 20,000 characters; `talk_seed --only dialects --world ridgemere3` after any
  dialect change.
- talk_bench's Ridgemere 3 look p99 is 5.1 ms of 5 with or without the shared lines (`World::districtHouse`, §29): the director's, not the library's; whoever next touches the director should warm or keep those walks.

### 33.4 Phase 8 (session 3 of 3), as built (2026-11-16)

**The thin topics are at §14.1's sizes.** Openers by topic (`concept=talk.<topic>`, every file), before Phase 8 and
now: gossip 72 → **257** (§14.1: 250), event 49 → **159** (150), news 43 → **150** (150), newcomer 52 → **120** (120),
harvest 11 → **60** (60), ships 11 → **60** (60), deed 21 → **80** (80), accent 26 → **60** (60), times 21 → **60**
(60); and the caravan's thin kinds (a wagon due, late, the word it brought: 13, 5 and 2 openers) 160 → **187**, since
the caravan topic was the last that still fell to the authored route ("A day like any other.").

**What was written** (`Data/Talk/rules/shared8_*.talk`): **844 rules, 2,599 alternatives** (831 in ten new files: 727
openers and 104 answers; 13 in session 2's files). The topic's opener
is the point here (§33.1), so these carry the concept; a file's concepts are its topic's opener and the answer concepts
its openers hand on to, nothing else (`talk_p8_rules`).
- `shared8_events.talk` (133): the week's happenings (talk.event) by the event's kind (the drovers, the fire at the
  works, the brewers' glut, the lame ox, blight, the mill wheel, a good catch, the salt pans, the House wedding, a rich
  cargo, a ship lost, the festival trade), its moment (on, posted, over: an event over in the past) and the speaker's
  trade and lot, each handing on with what kind of news it was (`{ev=good|bad|loss|news}`, economy8_times.talk's
  tags); and 23 `event.answer` lines by the event's own kind, so a reply to the drovers speaks of the drovers. Never a
  price the list doesn't show (a good dear or cheap only under its `world.<good>_price`), never a figure.
- `shared8_gossip.talk` (185): gossip by the claim's kind (theft 36, a wedding 23, a death 22, wanted 21, violence 17, a
  victim nobody saw 12, prowling 11, promises broken and kept 15, new in town 7, another 2, any 19), by the place (tavern,
  work, market, street, chapel, shore), the moment (night, winter, market day, Restday, rain) and the teller's trade
  and lot; none hands on, so session 1's react.news lines by claim kind answer them. What the claim says and no more;
  had from someone, never seen.
- `shared8_ships.talk` (63): the Wharf's ships by kind (due today or soon, in, stormbound, lost, sailed) and the
  speaker's work on the Wharf, trade in what she brings, lot and age; 14 `ships.answer` lines by kind.
- `shared8_times.talk` (50): hard, lean, good and booming Times by trade (farmer, merchant by business, labourer,
  crafter, keeper, fisher, priest, guard), on the dole, a parent, old, young, and the cause where there is one
  (`{tm=moan|cause|boast}`); 11 answers by trade and age.
- `shared8_harvest.talk` (59): a good, fair or poor harvest by season, trade, lot and the land round the town (the
  downs, the city's carts, the north's short summers, the woods and fens, Saltwell's carts from the green country);
  10 `harvest.answer` lines.
- `shared8_ownnews.talk` (121): one's own news (married, a loss, work, a crime done to one, a guard's arrest) by the
  teller's own facts (`self.mourning`, `self.apprentice`, `self.master`, `self.robbed`, `self.injured`), the two of
  them, the place, the season and the trade; 14 move lines that answer it by kind (react.news.glad, sympathise,
  reassure, advice for an apprentice or a master). The lost one is named only in `#topic.news#`: no pronoun is known
  for them, so none is said.
- `shared8_newcomers.talk` (76): a newcomer by the teller's nature, trade, lot and age, the two of them, the place,
  the Times, the season and the town's own country (the lake, the Pass, the market, the fens' boards, the Rainwood,
  the fields, the Waste); 8 move lines that answer on the topic (session 1's read `topic.claim_kind`, which the town's
  news doesn't set). Nothing claimed of the newcomer the game didn't give: what the teller means to do, or thinks.
- `shared8_deeds.talk` (68): a traveller's good deed by name, by look and by nickname, by the teller and the place; 9
  move lines.
- `shared8_accents.talk` (43): a voice from elsewhere by whose voice it is (each town's), the speaker's own home beside
  it, their trade and the place, handing on to `moment.reply_home`; and 9 of its answers: Saltwell's, which had none,
  and a second for each of the rest.
- `shared8_caravans.talk` (33): a wagon due, late or bringing word, handing on as economy8_roads.talk's do; 6 answers.
- Beside them, the most-heard shared lines of session 2's card given more ways: agreement (prices up and down and the
  Times now each their own: "Dear as anything, aye." answered a price coming down), the favourite's joins, thanks for
  advice (13 rules); the extend and the welcome more alternatives (plain.grammar).

**Who says it: the grammar's** (§33.2's rule, kept). Two new lead-ins in plain.grammar with criteria, a whole sentence
said in front of the news: `#s8_heard#` for what everyone may know (a pup's "Guess what!", the dry wolf's "Brace
yourself.", to a stranger "Pardon me. Have you heard?") and `#s8_psst#` for what's whispered (the sly "Now, this stays
between us.", the nervy "Don't look now.", the devout "I don't hold with gossip. Still."): 24 entries, 101 alternatives.

**Dialect.** The night's goodbye and its answer (`s8t_bye_night`, `s8t_return_night`: 8,030 of session 2's card's
lines, all plain words) gain `#s8_night#` and `#s8_nightback#` in all eight grammars: Coldwater's "Moor up safe",
Frosthollow's "Bank your fire, stone", Larkfield's "Shutters up, friend", Mirebridge's "Dry feet till morning",
Oakhaven's "Sleep like an old root", Southmeadow's "Sweet hay to you", Saltwell's "A cool night to you", Ridgemere's
"Mind the gutters home"; every one under six words (`talk_p8_rules`). Two collided with lines the library says
elsewhere (`dialect_check`) and were rewritten. Ridgemere 3's `live.dialects` were brought up to the files again
(`tools/talk_seed.py --only dialects`, the rows dumped first:
`Saved/Backups/ratw_ridgemere3-dialects-20261116-212550.sql`); Saltwell's grammar is 16,811 characters of the column's
20,000.

**Truth.** No eyewitness words in the thin topics' lines (the library's own are gated to `topic.seen`); a cause named
only where `world.times_cause` holds; feelings for the one talked of gated to the claim, the event or the deed;
pronouns by symbol, never "they" for the one wolf (talk_lint found 8 in the drafts, rewritten); an answer never ends on
a real question; a past event in the past (`talk_tests` found "this week" in one); "I saw the notice" and "I counted
forty head going by" and their like were rewritten as what the town knows. `talk_lint` 33 findings, none new;
`dialect_check` 0 failures; `talk_migrate --write` turned two word-for-word backchannels into their symbols.
`talk_tests`' figure check read a symbol's name (`#s8_heard#`) as a figure; it skips symbols now.

**The scorecard** (`artifacts/talk/runs/p8s3`, every profile in both modes over P0's fixture, pinned to build 81, with
Ridgemere 3's rows speaking the files' dialects; run once, the machine otherwise idle; five own-news openers were added
after it, on a topic the fixture's 25 days never reach). Against Phase 7's card (`artifacts/talk/runs/p7`) and session
2's (`p8s2`); the scorer is unchanged; fixtures / frozen:

| | Phase 7 | P8, session 2 | **P8, the phase** |
|---|---|---|---|
| core coverage holes (of 64,800) / all holes (of 1,188,000) | 64,800 / 55.8% | 0 / 0% | **0 / 0%** |
| R7 distinct replies per 1,000 (gate: 3× P0's 54.8 / 32.3) | 237 / 118 | 250 / 125 | **269 / 126** (4.9× / 3.9×) |
| C1 owed answers answered (%, gate 92) | 97.9 / 98.7 | 97.9 / 98.6 | **98.9 / 98.7** |
| R1 first identical talk (h) | 6.0 / 2.42 | 27.5 / 1.77 | **47.3 / 8.29** |
| R2 first repeated opener (h) | 2.41 / 0.42 | 2.41 / 0.42 | **6.50 / 2.28** |
| R5 collisions per 100 h | 0 / 5.65 | 0 / 6.34 | **0 / 0** |
| R6 the most-spoken line (%) | 1.85 / 2.75 | 1.25 / 1.29 | **1.23 / 1.10** |
| R3 distinct openers per 3,000 / R4 per 100 h | 2,729 / 265; 1,931 / 2,968 | 2,786 / 249; 2,049 / 3,069 | 2,884 / 282; 2,159 / 3,151 |
| C3 / C7 | 89.6 / 5.60; 90.8 / 6.44 | 89.4 / 5.64; 90.7 / 6.44 | 89.0 / 5.63; 90.7 / 6.59 |
| C4 per 1,000 / C5 / C6 | 0 / 0 / 0; 0 / 0 / 0 | 0.014 / 0 / 0; 0 / 0 / 0 | 0.028 / 0 / 0; 0 / 0 / 0 |
| C8 a third speaker (%) / C9 callbacks per 10 talks | 0.25 / 2.58; 5.63 / 1.57 | 0.27 / 3.65; 6.45 / 2.16 | 0.22 / 3.73; 5.43 / 2.14 |
| T1 / T2 / T3 per 1,000 | 0 / 0.042 / 0.070; 0 / 0 / 0.012 | 0 / 0.042 / 0.098; 0 / 0 / 0.012 | 0 / 0.028 / 0.084; 0 / 0 / 0.006 |
| T4 / T5 (%) / T6 per 1,000 | 0 / 0.23 / 0.38; 0 / 0 / 0.006 | 0 / 0.14 / 0.31; 0 / 0 / 0.012 | 0 / 0.79 / 0.17; 0 / 0.22 / 0.006 |
| K1 / K2 / K3 | 44.4 / 21.1 / 0; 73.5 / 13.5 / 0 | 44.4 / 21.2 / 0; 73.9 / 13.2 / 0 | 45.0 / 20.8 / 0; 75.1 / 12.7 / 0 |
| D1 / D1L / D1B / D2 / D3 (%) | 0 / 1.00 / 5.54 / 0 / 84.9; 0 / 1.28 / 3.49 / 0 / 72.7 | 0 / 1.79 / 10.4 / 0 / 89.7; 0 / 1.86 / 6.83 / 0 / 82.2 | 0 / 1.73 / 10.6 / 0 / 89.9; 0 / **2.87** / 7.11 / 0 / 74.2 |
| U2 / X1 / R8 / O1 | 39.8 / 19.7 / 0.105 / 0.45; 30.7 / 16.2 / 0.134 / 0.81 | 40.0 / 19.5 / 0.104 / 0.45; 32.1 / 16.1 / 0.149 / 0.80 | 40.2 / 18.4 / 0.111 / 0.50; 28.4 / 16.5 / 0.113 / 0.82 |
| B1 / B1P (ms, the card's load) / B5 | 0.28 / 0.86 / 1.9; 0.27 / 1.74 / 91 | 0.31 / 1.03 / 0.87; 0.22 / 1.10 / 108 | 0.33 / 1.11 / 0.78; 0.33 / 1.98 / 66 |

**The gate, judged** (§18: P8's): **0 holes on the core moves: met** (and on every owed move: 0 of 1,188,000 cells;
`Tests/fixtures/talk_coverage.json` stays blessed at 0; `artifacts/talk/coverage-p8s3.md` and `.json` are the
listing). **R7 3× the baseline: met** (268.7 / 125.6 against 164 / 97:
4.9× and 3.9×). **C1 92%+: met** (98.9 / 98.7). §14.1's size: 1,624 rules of its 2,100 (265, 515 and 844 a session;
5,473 alternatives), every thin topic at its number, the remaining 476 the coverage rules sessions 1 and 2 planned and
didn't write; the gate doesn't see them (every cell holds, the variety rows are met). §33.1's session 3 measures: **K1-K3
hold** (K1 45.0 / 75.1, K2 20.8 / 12.7, K3 0), **R4** is 282 / 3,151 per 100 h (it counts distinctive lines heard
again; there are more distinctive lines to hear, the new openers being the topic's own), **R6 fell** to 1.23 / 1.10%.
`talk_card` breaks no hard gate the baselines passed in either mode and exits 1 on the soft rows Phases 6 and 7 and
sessions 1 and 2 left (R8 0.11% against a 15% ceiling, U2, X1, B1, B1P), with B5 below on an open hard gate. What
moved, and why:
- **The authored route is gone from the thin topics.** Session 2's card fell to the Mind's fallback ("A day like any
  other.", "The stalls are full today.") 1,788 times on the event, ships and caravan topics (1,395 of them "A day like
  any other."), their openers spent within
  the hour in a busy place; in the first 30 frozen hours of L1, 127 openers came that way before and 22 after, all of
  them caravans, which `shared8_caravans.talk` then took. So **R1 frozen** rose 1.77 → 8.29 h (still under the 20 h
  hard gate: open since P0, and L1's busiest places now repeat a library talk, not the fallback's two lines), **R5
  frozen 6.34 → 0** (the fallback's "Quite a feast for the Blossom Fair." was every collision: open since P0, closed
  now), **R2** 0.42 → 2.28 h frozen, and **R1 fixtures** 27.5 → 47.3 h.
- **T5 fixtures** 0.14 → 0.79% (soft, target 2%; session 1 stood at 0.78): 13 lines told the listener what they held (L1 3,
  L2 7, L3 1, L10 2), nearly all the gossip turn session 2 wrote ("That's not the only tale going.") reaching a second claim the listener had by the time
  it was said, now that every claim kind has openers to say it with. Frozen 0 → 0.22% is one line in 49.
- **C4 fixtures** 0.014 → 0.028 is two old lexicon clashes in L4 (an orphan lamb laughed at, `e2_r_an_own_larkfield`
  and `e2_r_an_react_nice_merry`; the Houses' "robbed" wagons and `n7_ga_merry`'s "Ha.", the one session 1 named): an
  open gate in fixtures since P0, neither this session's words. Frozen 0.
- **D3 frozen** 82.2 → 74.2% is one speaker of L3's seven guessed wrong (85.7 → 71.4%); L2's 162 judged rose 76.5 →
  78.4. **D1L frozen** 1.86 → 2.87% is mostly the night's goodbyes in dialect.
- **U2 frozen** 32.1 → 28.4% (soft, below P0's 39.2 since Phase 6): more of every resident's talk is shared lines, and
  look-alike residents share them; the mantles (Phase 9 on) are U2's work. **C8 frozen** 6.45 → 5.43% stays in its
  5-15% band.
- **B1P and B5 frozen** (1.98 ms, 66 exchanges a second) are the card's ten runs at once. Alone, `talk_bench`: the talk
  town a beat 0.102 ms mean, p99 0.29, a look p99 1.66 ms, within budget; Ridgemere 3 from the fixture's first
  checkpoint (`--checkpoint artifacts/talk/fixtures/p0-r3-25d/cp-0.json.gz --hours 20`: the world's own save is at
  night, and nobody talks), a beat 0.215 ms mean, p99 0.96, a look p99 5.12 ms, over its 5 ms, and 5.14 ms with this
  session's ten files moved out: the director's `World::districtHouse` (§29), not the library.

**Tests.** `talk_p8_rules` (14): the thin topics' files carry only their topic's concepts; every opener is said once a
day by one speaker, hands on with its kind (`ev=`, `tm=`), claims no sight, names a cause only where it tests one; no
answer ends on a real question (an echo, "New, is it?", is a reaction); the lost one in one's own news is never "they";
`s8_heard`, `s8_psst` in plain and the night's goodbyes in every grammar under six words; feelings gated to the claim,
the event or the deed. `talk_tests` (the figure check skips symbols). `talk_scorecard_quick` passes unblessed (its
R6, 1.03% on its seed against 0.89, was "Go well.": old lines' and the closings'; seeds 2-4 old and new were 0.76-1.02%,
and the closing's own "Go well." is "Go well, then." now). The talk ctests (20: the library, beats, golden, topics, the
fixture, coverage, lint, dialects, migrate, equivalence, tags, the quick card, the trees) pass; `talk_check --validate`
on every file; `talk_lint` 33, none new; `dialect_check` 0 failures; `talk_migrate --check` clean; `talk_equiv` passes.
No ticking runs, no paid calls, no servers. The full non-database suite and the slice-end review are the slice end's.

**Not done** (§33.1's session 3 asked it): **the scene turns whose alternatives differ in kind** were not split or
given moves of their own. Counted today: 2,447 turns in 1,948 scenes put a question beside statements (13,346 turns),
and those scenes were 56,572 of session 2's card's 234,698 lines (`w7s_gossip_boss` 1,317, `tr_any_borrow_tool` 1,005,
`ev_pl_handsome` 738). Most are parallel (alternative *i* answers alternative *i*), so the turn after fits whichever was
said; the rest want a tool that splits them and an equivalence check, which is engine-and-tool work for the slice end
or Phase 9, not lines.

**For the slice end.**
- The ticking 100-hour listen (Q6), the listen packet and the first blind ratings (H1) are next; the thin topics'
  openers are where the new words are heard most (the event and ship topics now speak the library's lines where the
  fallback spoke), and the night's goodbyes in dialect at every evening's end.
- The open gates the phase leaves: R1 frozen 8.3 h of 20 (L1's busiest places), B5 under the card's load, C4 fixtures'
  two old lexicon clashes, the Ridgemere 3 look's p99 (5.1 ms, `World::districtHouse`); and the soft rows U2, X1, R8.
- The scene turns above; and 476 of §14.1's coverage rules, if the listen asks for more ways to answer than it has.
- Saltwell's grammar is 16,811 characters of the column's 20,000. Until this branch merges, the main checkout's
  `dialect_check --world ridgemere3` sees rows newer than its files.

## 34. Slice 2's end: the full suite, every profile, the ticking listen and the first blind ratings (2026-11-17)

Branch `r3-talk-s2`, on Phase 8 (`r3-talk-p8` at c6acfdbf). `origin/ridgemere2` had nothing new (still 0ec6d092,
Slice 1's merge, which Phase 2 was built on), so the merge was empty. Everything was built in one Release build and run.

**The suites.**
- **Core**: the full ctest, 109 of 109 (`talk_bench`, label perf, among them), with the eight database suites (`*_pg`,
  `pg_client`) run against the local PostgreSQL (`RATW_TEST_DATABASE_URL` the game role on `ratw_ridgemere3`: the client
  tests only read and notify); run again after every fix below. `talk_scorecard_quick` failed once on its
  timing rows (B1, B1P, B1L) with a card of ten runs on the machine, and passed alone. One failed first: **`market_behaviors`, a test fault**.
  Its guard that a wagon's news is said once ("never the topic's first fact spliced in over it", doc 79 Phase 8) refused
  any line containing "brought word" or "The last wagon", so Phase 8's own caravan opener "The last wagon brought word:
  hides are cheap in Larkfield." failed it, though it says the news once and in the wagon's words. The guard now refuses
  what it meant: the topic's first fact ("The last wagon in brought word: ...") spliced in, or the news twice.
- **Client**: typecheck, 224 unit tests, the build, and the 11 browser tests (headless, a local central and a world).
- **Editor**: typecheck, the build, 165 tests (78 and 87), and the render smoke (17 panels).
- **Talk**: `talk_check --validate` on all 179 files of `Data/Talk` (10,347 library entries say what they do; the 127
  "not yet" are the dialogue trees' nodes, doc 83's, until Phase 13), `--render-trees`, `--coverage` (0 holes of
  1,188,000, 0 in the core moves), `talk_lint` (no new finding; below), `tools/dialect_check.py --world ridgemere3`
  (65,414 lines, 0 failures; 14 warnings, the place names it has always listed), `test_dialect_check` (15),
  `test_path_check` (17), `talk_equiv --against 1e159420`, `talk_migrate --check`, `talk_tags`, `talk_conv`.
- **Central**: `test_central.py` (84) and `test_world_join.py` (2).

**The listen's review, and its fix: a traveller is one wolf.** Reading the card's talk and the ticking listen, the most
heard fault no metric counted was "them" and "they" for the traveller the two can see: "Smiled? Or bared their teeth?",
"Don't bother them.", "Are they an adventurer? Do they have a sword?". The talk of a traveller near (`topic=player`) is
said in every real game, since every listener there is a player; T6 counts only a "they" after a name in the same line,
so none of it showed. **215 alternatives in 110 entries** (`everyday_player.talk`, `hard7_strangers.talk`, `hard7.talk`'s
stranger and deed answers and closings, `deeds.talk`, `people.talk`, `relations.talk`) now say `#topic.them#`,
`#topic.they#`, `#topic.their#`, `#topic.theirs#`, which the engine fills by the traveller's sex (Phase 6), each with its
verb made to agree by hand: "Ask him.", "She looks tired, that's all.", "Is she armed? She's armed.", "He has a walk all
his own."; the backchannel "Let them." (`#bc_let_them#`) said of the traveller is "Let him." there. Plurals were kept where the words mean many (travellers in general, the door watch, a pair of boots, the gods,
"they say"), and an unknown doer before anyone names them ("Was it a hero? Did they have a sword?"). One reworded line
clear of Saltwell's "to the well" (`dialect_check`): "Then show him where the well is." Each entry is named in
`talk_equiv`'s EDITED with why. To keep it so, `talk_lint` gained **`they_stranger`**: a line on the traveller's topic or
its `stranger.*` and deed answers saying "they", "them" or "their" with nothing plural before it to stand for, and not
the town's "they say" or "they're calling"; its 26 findings left are those plurals, each read, and frozen in
`Tests/fixtures/talk_lint_baseline.json` (59 in all, none new). `test_talk_lint` checks both ways. The fix changes no
metric of the card (its virtual listeners are men: "Smiled? Or bared his teeth?" is said 59 times there now), and the
ticking listen and the blind packet were spoken before it.

**The instrument.**
- **talk_sim says where it listens**, each listener's place, its kind and who stands there free, and warns when none of
  a profile's places has two who could talk. The first ticking run (below) heard nothing because of this.
- **talk_rate's packet is blind, and its key is kept apart.** `--key FILE` writes the answer key away from the packet
  given to the rater; the packet is also a Markdown sheet (`packet.md`, beside `rate.html` and `ratings.csv`); nothing
  on either says where a conversation came from (no place, day, run, entry or number but its place in the packet);
  a shuffled conversation keeps its real one's first line, so a conversation taken from one source is never taken from
  another and no two items open with the same words; a source with too few conversations is refused. `test_talk_rate`
  (ctest `talk_rate_rules`, 4) checks each.

**The scorecard** (`artifacts/talk/runs/s2`, every profile in both modes over P0's fixture, pinned to build 81, after
the review's fixes; 12,989 and 25,467 conversations, 71,209 and 166,415 lines). The card was run four times: twice
beside the ticking listen, before and after the traveller's pronouns (the same values but timings: the card's listeners
are never its talk), once stopped when three more alternatives changed under it, and this last one alone. Against the Slice 1 baseline (the review's card, `s1r`, §26) and Phase 8's last (`p8s3`, §33.4),
fixtures / frozen:

| | Metric | Slice 1 (`s1r`) | Phase 8 (`p8s3`) | **Slice 2 (`s2`)** |
|---|---|---|---|---|
| R1 | first identical talk (h) | 2.22 / 0.418 | 47.3 / 8.29 | **42.9 / 8.29** |
| R2 | first repeated opener (h) | 2.22 / 0.14 | 6.49 / 2.28 | **6.49 / 2.28** |
| R3 | distinct openers per 3,000 | 2,132 / 451 | 2,884 / 2,159 | **2,885 / 2,158** |
| R4 | a distinctive line heard again within 10 h, per 100 h | 1,010 / 3,922 | 282 / 3,151 | **276 / 3,180** |
| R5 | collisions per 100 h | 186 / 924 | 0 / 0 | **0 / 0** |
| R6 | the most-spoken line (%) | 3.51 / 13.2 | 1.23 / 1.1 | **1.21 / 1.1** |
| R7 | distinct replies per 1,000 | 54.8 / 32.3 | 269 / 126 | **269 / 125** |
| C1 | owed answers answered (%; Slice 1: the "?" heuristic) | 96.9 / 97.8 | 98.9 / 98.7 | **98.9 / 98.7** |
| C3 | ended by a farewell or a marked break (%) | 0.891 / 0.176 | 89.0 / 90.7 | **89.0 / 90.7** |
| C4 | tone clashes per 1,000 beats | 0.0256 / 0 | 0.0281 / 0 | **0.0281 / 0** |
| C6 | an entry or question twice, per 1,000 talks | 0 / 0.119 | 0 / 0 | **0 / 0** |
| C7 | lines per talk | 3.01 / 2.56 | 5.63 / 6.59 | **5.64 / 6.59** |
| C8 | a third speaker (%) | 0 / 0 | 0.224 / 5.43 | **0.217 / 5.44** |
| C9 | callbacks per 10 talks | - / - | 3.73 / 2.14 | **3.72 / 2.15** |
| T1 | raw ids per 1,000 lines | 0 / 0 | 0 / 0 | **0 / 0** |
| T2 | untrue presuppositions per 1,000 | 0.307 / 0 | 0.0281 / 0 | **0.0281 / 0** |
| T3 | self-reference errors per 1,000 | 0.41 / 0.03 | 0.0844 / 0.00601 | **0.014 / 0** |
| T5 | told what they knew (%) | - / - | 0.786 / 0.22 | **0.784 / 0.22** |
| K1 | everyday topics (%) | 3.79 / 24.4 | 45.0 / 75.1 | **45.0 / 75.2** |
| K2 | the largest topic (%) | 50.7 / 41.8 | 20.8 / 12.7 | **20.8 / 12.7** |
| K3 | the same news twice | 2,197 / 109 | 0 / 0 | **0 / 0** |
| D1 | accent none's lines with dialect words (%) | 0 / 0 | 0 / 0 | **0 / 0** |
| D1L | light (%) | 0.213 / 0.248 | 1.73 / 2.87 | **1.8 / 2.78** |
| D1B | broad (%) | 0.121 / 0.127 | 10.6 / 7.11 | **10.6 / 7.12** |
| D2 | another dialect's words | 0 / 0 | 0 / 0 | **0 / 0** |
| D3 | dialect guessed from 20 lines (%) | 68.4 / 13.2 | 89.9 / 74.2 | **87.2 / 65.2** |
| U2 | look-alikes told apart (%) | 48.0 / 39.4 | 40.2 / 28.4 | **39.4 / 29.6** |
| X1 | replies sharing a word with the line before (%) | 26.8 / 15.3 | 18.4 / 16.5 | **18.5 / 16.4** |
| R8 | habit lines (%) | 0.0242 / 0.0622 | 0.111 / 0.113 | **0.103 / 0.107** |
| B1 | a beat, mean (ms, the card's load) | - / - | 0.327 / 0.334 | **0.339 / 0.365** |
| B5 | exchanges a second | 2.13 / 57.6 | 0.778 / 65.8 | **0.761 / 70.9** |

`talk_card` breaks **no hard gate the baselines passed** in either mode. It exits 1 on the rows Phases 5 to 8 left, each
moved by design and none refrozen, since Slice 2's gate (H1 against the baseline) doesn't ask for them: **R8** (habit
lines, 0.10% of a speaker's against a 15% ceiling: more talk, a few more habit lines), **U2** and **X1** (everyday talk
and shared lines, §30; U2 is the mantles' work), **B1** and **B1P** (the card's twenty runs, ten at
a time), and **B5** below its baseline on an open hard gate (timing under that load). Against `p8s3` the transcripts differ
in their grammar picks (the same entries, other alternatives: Phase 8's last edits came after its card), and **D3
frozen** 74.2 → 65.2% is one of L3's seven judged speakers guessed wrong (5 of 7 then, 4 of 7 now; L2's 162 are 78.4 →
77.8%); **R1 fixtures** 47.3 → 42.9 h is one listener; **T3** fell (0.084 → 0.014 / 0.006 → 0) with the two eyewitness
lines below.

**The ticking 100-hour listen** (Q6, reported):
`talk_sim --world ridgemere3 --tick --profile L1 --places r2_the_knucklebones,rm_the_grey_gull_inn,rm_the_rainbarrel,
rm_the_bilge --hours 100 --seed 1 --ablate shuffle` (`artifacts/talk/runs/s2-tick`), from DEV's save as it was at 05:13
UTC (game day 32.05, an hour past midnight) to day 57.05: 100 world hours, 360,000 world seconds, 25 game days, 7,200,000
ticks of 0.05 s, world build 81, nothing written. **3,877 s of wall time** for the ticking (64 min 46 s with the load),
**speed 93** (Slice 1's was 123, alone; this one shared the machine with two cards of twenty runs for most of its hour),
**1.38 GiB peak memory**; 7,449 conversations, 48,446 lines, 162 speakers. The four places are Slice 1's, named, because
the save was at one in the morning: ranked by that moment's crowds, L1 chose two street cells, and **a first ticking run
there heard nothing in 16 world hours and was stopped after 6 minutes of wall time** (also reported: it is the second
world simulation this slice ran, of no use). Against Slice 1's ticking listen rescored with today's scorer
(`artifacts/talk/runs/s1-tick-r`), Slice 2 / Slice 1, the shuffled control in brackets:

| | Ticking L1, Slice 2 | Ticking L1, Slice 1 |
|---|---|---|
| R1 first identical talk, earliest listener (h) | **17.6** (median 94, censored) | 2.5 |
| R2 first repeated opener (h) | 10.1 | 2.5 |
| R3 distinct openers per 3,000 / R4 per 100 h / R5 collisions per 100 h | 2,803 / 1,582 / **0** (332) | 2,026 / 3,534 / 951 |
| R6 the most-spoken line / R7 distinct replies per 1,000 | 0.82% ("Sleep sound.") / 98 | 0.68% / 14 |
| C1 owed answers answered / C3 farewells and breaks (%) | **98.5** (19.4) / 89.2 (32.2) | n/a (no moves) / 1.3 |
| C7 lines per talk / C8 a third speaker / C9 callbacks per 10 | 6.5 / 2.6% / 4.1 | 3.2 / 0 / n/a |
| T1 / T2 / T3 per 1,000 lines | 0 / 0.02 / 0.23 | 0 / 4.15 / 0.54 |
| K1 everyday / K2 the largest (crime) / K3 | **46.9% / 21.2%** / 1 | 0% / 75.2% / 131 |
| D1L / D1B / D3 (%) | 1.0 / 2.0 / 78.7 | 0.14 / 0.27 / 38.4 |
| U2 / O1 opinions per 10 talks | 63.9 / 0.80 | 65.6 / 0 |

What the living world showed that the card can't, read from the transcripts: weddings told by the ones who married and
congratulated, the town's news passed on and not told twice to the same wolf but once (Signe Brineworth's gossip of Karin
Wetridge to Rune Wickwell, three weeks apart: K3's one, a hard gate that the card, frozen and replayed, holds at 0); the
everyday as often as crime in the busiest rooms. Its faults, and what became of them:
- **The traveller was "them"** ("I bet that grey stranger could lift a cart." / "Ask them to help unload."): fixed (above).
- **Eyewitness words from wolves who saw nothing** (T3's 9 "unbacked"): `et_arrest_relief`'s "I saw them marched off."
  and `cr2_theft_suspect`'s "I saw someone near X's door that night." are said true for anyone now ("Marched off by the
  watch, I heard."; "Someone was seen near X's door that night."), each in talk_equiv's EDITED; the rest are a kin's "I
  saw you at the well this morning" and the dead remembered ("I saw X only on Restday. Laughing."), which the heuristic
  counts and a listener wouldn't. T3's one `own_name` is a name given back in an introduction ("Sybil Tardale. And I'll
  confess I've lost yours too."): right as said.
- **K3 counted two weddings as one piece of news** (one's own news has no subject in the conversation's head, so "I
  married Runa" and "I married Maren", told to Osric Wetford by the two who married, were the same news twice). A scorer
  fault: own news is keyed by its teller now (`test_talk_score`: two weddings, then one told twice). 9 → 1 on the tick;
  the card's K3 was and is 0; Slice 1's tick 133 → 131.
- **Open:** "We're both from the downs, I think." said by a Southmeadow baker with no accent to a broad Coldwater voice
  (`s83a_south_pair`, listen packet conversation 3): the accent topic's opener, meant for the one who hears the voice,
  was said by the other, whose own origin matched the criterion; the director's `topic.accent_of` is the second
  speaker's origin, so the opener needs the one who hears (A) to speak, or a fact for who is heard. Not fixed here.
- **Open:** "My son rides with the Frosthollow caravan." from a wolf with no son (T2's one; the lint's frozen
  `cv2_left_worry` finding since Phase 1); "Sleep sound." the most-said line (395 times: the night's farewell in the
  taverns, where every talk ends at night); the authored fallback "A day like any other." / "So it is." still heard where
  no line fits (The Knucklebones' first repeat, at 29.5 hours); light and broad speakers mostly plain in Ridgemere's rooms
  (D1L 1.0%, D1B 2.0%: the mantles' work).

**The listen packet**: `artifacts/talk/listen/s2-tick.md` (and `.html`, the page with marks): forty of the ticking
listen's conversations across the kinds of talk (`talk_listen.py --packet 40 --seed 1`), with the run's report and its
scorecard beside L1 over the fixture and frozen.

**The first blind ratings (H1, §15.7).** `tools/talk_rate.py packet --new artifacts/talk/runs/s2-tick/tick-L1 --old
artifacts/talk/runs/p0-baseline/fixtures-L1 --each 30 --seed 2 --key artifacts/talk/rate/s2/key.json`: **90
conversations**, 30 from this slice's ticking listen, 30 from the Slice 1 baseline (P0's baseline transcripts, the L1
listeners over the fixture: today's talk before this design, the old library), 30 from the ticking listen's shuffled
control, sampled over topic family and length (2 lines or more), in a random order, numbered 1 to 90 and nothing else.
The packet is `artifacts/talk/rate/s2/packet.md` (and `rate.html`, `ratings.csv`), the key
`artifacts/talk/rate/s2/key.json` (don't open it before rating). When the ratings are in, `python3 tools/talk_rate.py
score ratings.csv --key artifacts/talk/rate/s2/key.json` gives H1 (the means, new − shuffled against 2.0+, new − old
against 1.0+, new against 3.8+) and H2. Rated blind by three raters: "H1, first blind ratings" below. §18 asks for the
user's own rating too; these don't stand in for it.

**For Slice 3.** The open gates are Phase 8's: R1 frozen 8.3 h of 20, B5 under load, C4 fixtures' two old lexicon
clashes, the Ridgemere 3 look's p99 (`World::districtHouse`); and the soft rows U2, X1, R8. The scene turns that put a
question beside statements (§33.4) are not split. The ticking listen's own findings are above. H1 is not met (below):
what it asks of the engine and the shared lines comes before Slice 3's mantles are heard.

### H1, first blind ratings (2026-11-17)

**Who rated, and how.** Three raters (A, B, C), each given only the packet (the same `packet.md`, byte for byte) and
never the key, rated all 90 conversations from 1 to 5 on two axes, **believable** (real people talking) and **coherent**
(each line follows the one before), with a note on every low one. None said whether a writer wrote the whole exchange,
so **H2 is not measured**. Their numbers and words are kept unchanged in `artifacts/talk/rate/s2/ratings-blind.json`.
`talk_rate.py score` now takes several raters and any number of axes. H1 is judged on **overall**, the mean of each
rating's two axes: `python3 tools/talk_rate.py score artifacts/talk/rate/s2/ratings-blind.json --key
artifacts/talk/rate/s2/key.json --json artifacts/talk/rate/s2/h1.json --report artifacts/talk/rate/s2/h1.md` (the
report has every table, the six lowest with their lines, entries and moves, and all the notes). Nothing in the engine or
the lines changed, so §34's scorecard stands, and `talk_card` wasn't run again.

| Source | Believable | Coherent | **Overall** | Items below 2.5 / at 4+ | Weighted to the lengths heard |
|---|---|---|---|---|---|
| Slice 2 (the ticking listen) | 3.31 | 3.44 | **3.38** | 6 / 11 of 30 | 3.13 |
| Slice 1 baseline (P0's, the old library) | 3.56 | 3.98 | **3.77** | 3 / 17 of 30 | 4.02 |
| The shuffled control | 1.83 | 1.54 | **1.69** | 26 / 1 of 30 | 1.29 |
| New − shuffled (target 2.0+) | +1.48 (95%: +1.11 to +1.83) | +1.90 (+1.42 to +2.37) | **+1.69** (+1.28 to +2.08) | | |
| New − old (target +1.0) | −0.24 (−0.59 to +0.12) | −0.53 (−1.03 to 0.00) | **−0.39** (−0.80 to +0.04) | | |

The intervals are a seeded bootstrap over the items. These are the scores each source got, across raters and items, 1
to 5. Believable: new 14/35/40/1 for 2/3/4/5; old 11/24/49/6; shuffled 32/44/11/3 for 1/2/3/4. Coherent: new 1/17/25/35/12
for 1-5; old 4/7/13/29/37; shuffled 63/13/6/8 for 1-4. Every rater ranks the three the same way, old over new over
shuffled. Each rater's overall means, for new, old and shuffled, were A 3.40, 3.88 and 1.78; B 3.38, 3.67 and 1.57; and
C 3.35, 3.75 and 1.72. **The raters agree closely.** Krippendorff's alpha (interval) is 0.92 for believable, 0.96 for
coherent and 0.96 overall (ordinal: 0.94, 0.95 and 0.96), and the mean pairwise Spearman is 0.94, 0.95 and 0.96. They
gave the same number 82% of the time on believable and 81% on coherent, and never differed by more than one point. The
means stand, and what varies is the conversations.

**H1 is not met on any of its three counts.** The new talk scores 3.38, against a target of 3.8. It beats the shuffled
control by 1.69, short of the 2.0 target, but its interval lies well clear of 0, so the control does separate. Against
the old library it falls 0.39 short of the target, and it isn't 1.0 above it. Its interval runs from −0.80 to +0.04, so
"no better" is sure and "worse" nearly so. The packet samples talks of 2, 3, 4 and 5 or more lines evenly, but most of
what a listener hears is longer: the ticking listen's talk is 54% at 6 lines or more, the baseline's is 88% at 3 lines.
Weighted to the lengths each run speaks, the gap widens to about −0.9 (3.13 against 4.02).

**Where the deficit is.** The deficit isn't in the replies at the heart of a talk. The new 2- and 3-line talks rate
3.54 and 4.15, against the baseline's 3.05 and 4.00. The 16 of 30 new talks that have neither fault below rate **3.84**,
which is above the 3.8 target and above the baseline's 3.77. The deficit is in two things the engine adds around those
replies:
1. **"Back again?" in the opener's breath.** When two wolves talked within the last 0.3 days, `frames.json`'s
   `again_today` puts `again.open` in front of whatever opens. Seven new items have it, and they rate 2.50 against 3.65
   for those without. **29.6% of the ticking listen's 7,418 talks open with it**, because the same pairs meet again and
   again in the four rooms. It comes before a lost house ("Back so soon? We had a proper house once."), a theft, the
   drovers' good news, and a line that starts with the same word ("Back again? Back home rich and poor..."). With a
   callback it stacks three openers in one breath ("Twice in a day, is it? We'd only just finished with the last of
   this. Edric Millton's been done."). In all, 5.8% of the listen's first lines carry three moves or more.
2. **A farewell pair after one exchange.** The 14 new items that close with a farewell rate 2.85, and the 14 that end
   curt rate 3.90. The 4- and 5-line talks rate 2.71, and they are 40% of the listen's talk. The baseline's talks of that
   length rate 4.25, and all their lines are talk. A quiet wolf's budget (3 to 5 lines) or a stranger's (scaled by 0.6)
   leaves room for one exchange, and the closing takes the rest. A bare "Aye." also counts as the answer to a confession.
   9.3% of the listen's talks are one exchange and then goodbyes. All seven "again" items also closed this way.

**The five lowest of Slice 2.** The rating is the three raters' overall mean. Items 7 and 89 tie for fifth, so both are
listed.

| # | Rated | The talk | What the raters said | What caused it |
|---|---|---|---|---|
| 23 | 1.83 | Aud: "Back again? Back home rich and poor drink at the same well. Here they've a well each." Hanne, joining: "Don't let me stop you. I'll just listen." Aud: "Be seeing you." Gwen: "Rest well." | The opener says "back" twice. "I'll just listen" answers nothing. Aud leaves, and a speaker who hadn't spoken says goodbye. | `again.open` goes in front of `n7_st_country_city` with no check on its first word. Hanne joins on her favourite topic (`s8j_favourite_smalltalk3`) as the second line. **`Library::join` clears what was owed** (`c.open.clear()`: "the joiner cuts in before whatever was owed"), so Gwen, who was spoken to, never answers, and the opener's `then other: town.station` is dropped. With nothing owed the talk closes, and Gwen's only line is her goodbye. Of the listen's 230 joins, 65 cut in before the opener was answered. |
| 6 | 2.00 | Corwin: "Hello, friend. She'll be tucked in a cove somewhere, the Kestrel. Waiting." Gwen: "I've watched a thousand come and go." Then goodbyes. | The Kestrel comes from nowhere. The reply only loosely fits, and the talk goes straight to goodbyes. | `s83s_late_sailor`, Phase 8's ships line, is written as a reply about a ship already known to be late, but here it opens the talk (`does tell.self`) after the strangers' greeting. `s83s_ans_old` answers any ship news for any old wolf. The strangers' budget, scaled by 0.6, leaves one exchange. |
| 66 | 2.00 | Orm: "Back so soon? We had a proper house once. Before things went wrong." Gerrit: "Aye." Then goodbyes. | "Back so soon?" is stapled to a heavy confession, which gets a bare "Aye" and an immediate goodbye. | `again.open` (`ag_again_any`) goes in front of a grave memory (`e3_mr_poor`, though it is tagged tone even). A quiet wolf's "Aye." (`e3_ma_quiet`) is taken as the answer. The budget is a quiet wolf's, so the closing comes at once. |
| 73 | 2.00 | Gwen: "Twice in a day, is it? We'd only just finished with the last of this. Edric Millton's been done. The Watch'll be down here looking by noon." Ansel: "Up the hill, that's news. Down here it's the weather." Gwen: "Nobody's blaming you." Then the night's goodbyes. | Three stacked openers. "Nobody's blaming you" doesn't follow, and the bedtime exit is abrupt. | Three moves go in one breath: `again.open`, the callback `s8cb_crime` ("the last of this" means the last crime, which is never named) and `h7_cr_open_sump`. `h7_cr_ans_sump` tags all four of its alternatives `{h7_said=blame}`, including this one, which isn't about blame, so `h7_cr_close_blame` follows it. |
| 7 | 2.33 | Soren: "Planks are dear in Frosthollow, so the Frosthollow wagon says." Sybil: "I'll hear about it when it hits my supper." | "My supper" is a retort about food prices, but the news is about planks. "Frosthollow ... Frosthollow wagon" says the town twice. | `e8r_cv_news`'s second alternative puts `#topic.where#` after `#topic.news#`, which already names the town. `s83v_ans_news_poor` answers any wagon's news for any poor wolf, with nothing about the goods, and it was said 139 times in the listen. The talk ended curt. |
| 89 | 2.33 | Hanne: "You again? There'll be meat on every table tonight, with the drovers in." Gwen: "In my day it was the same, and we managed." A preclose, then goodbyes. | "We managed" answers good news as if it were hardship. "You again?" is stilted, and the talk ends in an abrupt brush-off. | `again.open` goes in front of good news. `s83e_ans_old` answers any event happening now for any old wolf, with no `{ev=...}` criterion, so a hardship line meets the drovers' `{ev=good}`. |

**What it asks of the next round.** None of this is fixed here. It comes before Slice 3's mantles are heard, and the
second packet at the final listen will measure it again.
- `again.open` only where it fits. It should be a line of its own, answered with `greet.return`, not said in the
  opener's breath. It should never come before a grave or sorrowful line or news of a death or a crime, never start
  with the opener's own first word, and come far less often between the same two in one room.
- No more than two opening moves in one breath.
- A body before the closing. Close only after two exchanges, or end curt, as the well-rated talks did, when the budget
  leaves only one. A bare backchannel ("Aye.") shouldn't count as the answer to a story or a confession.
- `Library::join` keeps what was owed: the joiner cuts in, and the one spoken to still answers.
- Phase 8's answers by the replier's kind (`s83e_ans_old`, `s83v_ans_news_poor`, `s83s_ans_old` and the like) should
  depend on what the news is, whether good or bad, and whether it's about food. `h7_cr_ans_sump`'s tag should go only on
  the alternatives about blame. `s83s_late_sailor`'s lines should become answers, or openers that name the ship and say
  that it's late. `e8r_cv_news` should say the place once.
- Report the mean weighted to the lengths heard beside the packet's even one. The scorer does both now.

**The instrument.** `talk_rate.py score` takes any number of rating files. A `ratings.csv` is one rater. A JSON file
holds one rater or several, each with any number of 1-5 axes and a note per item. For each axis and source, the score
gives:
- the mean, the spread of the item means, the distribution of the scores, and each rater's means;
- the differences, with a seeded bootstrap interval over the items;
- agreement between the raters: Krippendorff's alpha, interval and ordinal (checked against Krippendorff's own 2011
  example, 0.849 and 0.815), the mean pairwise Spearman, and exact and within-one agreement;
- the rating by length, with the share of the run's talk at that length and the mean weighted to it;
- for the new source, the rating by how its talks ended and by the moves they made;
- the N lowest new items (`--worst`), with the raters' notes and the lines as the run spoke them;
- the same as Markdown (`--report`).

`test_talk_rate` (ctest `talk_rate_rules`) now has 7 tests: the reference alpha, several raters on two axes, and the old
one-CSV path.

## 35. Slice 2's review: what it found, and the fixes (2026-11-17)

Branch `r3-talk-s2`. The slice-end review confirmed 33 findings, from three lenses (correctness, believability and
truth, performance and operations); all are fixed here, each with a test where one could be written.

**The engine and the game's talk.**
- **A player is never given a line by the engine (§13).** Since Phase 3 `converse` runs `begin` then `next`, and `next`
  answers whatever is owed before it asks whose talk it is, so a keeper's greeting to a customer (`react.customer`,
  `does greet`, inviting `greet.return`) came back as two lines and the second was published as the customer's own wolf:
  "Hello there." about half of every shop entry, 2.5 s after the keeper's. `talk::Speaker::player` (set by
  `Game::talkSpeaker` for anyone not a resident, or facts `player=yes` as talk_check says it) marks them; the
  conversation keeps `Conversation::players`; a scene's turns stop before a player's, what a player owes is left theirs,
  and a talk with one in it ends when the resident has said their piece. `gameAnswer`'s `talkRemember(said.effects)`
  no longer carries a reply the player never made (`talk_tests` sliceTwoReview: 40 seeds, never a second line to a player,
  a resident greeted greeting back now and then; `dialogue_tests`: the query marks the player).
- **How a talk went is how it ended up, and who soured it says sorry.** `ambientNote` called any talk with a `counter`
  "quarrelled" and any with a `decline` or `refuse` "soured", though most were "I can't tomorrow. Another time, I promise."
  / "No matter." and a warm goodbye; `frames.json`'s `mend` then had either of them apologise next time (of 622 mends on
  the card, 591 followed a decline or a counter). Now a falling-out is an old grudge aired, a `challenge`, offence taken,
  an apology refused, a third objecting or defending, and not if an apology was accepted before they parted; the note
  keeps who made the sharp move (`TalkNote::souredBy`, saved as `soured_by`), the speaker's fact `self.soured` says so,
  and `mend` needs `self.soured=yes` (facts.json `soured`).
- **Two of a side air an issue to each other, and agree.** `answer`'s filter "an opinion answered with one's own only by
  one who differs" also refused the first airing whenever the two agreed, so no same-side pair ever spoke of an issue and
  every "agree" came from a wolf with no view (`pair.agree.<issue>=yes` was never kept). It applies now only once the issue
  has been aired in the talk (`talk_tests`: two guards air the Watch and agree, never disagree; the old test passed on
  the "against" runs alone).
- **A reason is said once a talk.** A stance's reason was always its side's first lean, so a joiner said the giver's very
  sentence back ("Hear, hear. I've worked my whole life in their shadow...") and a disagreement or an insist its speaker's
  own from two lines before (156 of 199 `op_join_side`, 31 of 31 `op_insist`). `Library::stances` keeps every reason of
  the side (`op_whys.<issue>`, the event's first), `stanceFacts` gives the first not yet said in the talk, and with none
  left the lines without one are said (`op_join_side_plain`, `op_disagree_close_plain`, `op_insist_plain`: "Hear, hear.",
  "You know I love you, but no.", "I've said my piece, and I'll not unsay it."). `talk_tests`: no sentence of six words
  or more said twice in a talk, either side.
- **A goal under way finishes.** The favour's plan is `pre.request` then `request`, a step a lull, and the budget often
  closed the talk between them: "Have you a moment? I've a favour to ask." / "What is it?" / "I'd better get on." (none of
  fixtures-L3's 59 favours were asked). A goal is begun now only with room for all of its plan, and one under way takes
  its next step whatever the budget, up to the most a talk may have (`talk_tests`: every favour begun is asked).
- **A quarrel that turns is still a quarrel.** A talk turned to an agenda item took that item as its topic to the end, so
  `World::ambientEnded` gave a turned quarrel +1 familiarity and a "conversation" event instead of the grudge. The talk
  keeps the topic it began on (`AmbientTalk::kind`), for the bonds, the event and its note.
- **The authored fallback keeps the agenda and the claim.** Where no library line fit, `ambientBegin` replaced the
  conversation with a fresh one, so those talks never turned (0 of 29 on the card) and could tell their claim again.
- **The town's crime and news are told each wolf once.** The told set covered claims, deeds and one's own news; the
  crime and the town's news (`life`) loops never asked it, and K3 couldn't see them: in the ticking listen Gerrit Hollis
  heard Wenna Tarford's robbery as news seven times in two days. `World::ambientTopic` now offers an incident or a town's
  news to two only if neither has told the other; one who has been told it tells one who hasn't (the topic's source);
  two who both have heard it have no news to give. `Game::toldTopic` (a claim, one's own news, a deed, `incident|`,
  `town|`) records the told set as the talk takes the topic up, and one who joins a talk is told it too (`ambient_tests`
  sliceTwoReview: told by the other, never again nor back; heard elsewhere, the one who knows tells; both, nothing).
- **The accent topic names the right home town.** `World::talkOrigin` worked out a hash of the id, while the speaker's own
  facts took the resident's `live.npc_voice` row, so 36 of 119 accent lines in the tick named a town the wolf wasn't from
  ("Oakhaven, if you must know." from a Coldwater voice). `World::setTalkOrigins` (filled with the voice rows) makes the
  director's origin the row's too (`ambient_tests`). §34's open item (the opener said by the one whose voice matched,
  `s83a_south_pair`) had this as a second cause.
- **One named in good news adds to it.** Anyone named in the last line joined with `join.object`, so a husband "objected"
  to his own wedding ("I married Gerrit Ropewick." / "Go on. Say it louder." / "No offence meant." / "I'm not ready to let
  it go yet."): 8 of the tick's 10 objections. For one's own news (but a crime or a loss) and the kind claims (a wedding,
  a promise kept, a newcomer) the stake's move is `join.add` now: `s8j_named_news_marriage` ("And I'd do it all again
  tomorrow."), `s8j_named_news`, and `s8j_named_kind` made a `join.add`.
- **A goodbye that says one is going is said true.** "Off home, then.", "Bed for me.", "Off to bed with me.", "Off I go,
  then." were said by anyone from six in the evening, three in four of them by a wolf who talked again in the same room
  within the hour, the Grey Gull's innkeeper among them. They are rules of their own now (`s8t_bye_leave_dusk`,
  `s8t_bye_leave_night` for bed, only at night, `cv_bye_both_leave`, `s8t_bye_young_leave`, `s8t_bye_adolescent_leave`),
  only for one not at work and not keeping the inn, each tagged `{leave=yes}`; said, `World::ambientLeave` takes the
  speaker out of the room's talk and its joins for the hour (`ambient_tests`). The grammar's goodbyes lost their "Off I
  go" and "I'll be off".
- **No recall in the breath of an opener that recalls.** `s8cb_prices` was said before `ag_cb_prices` ("Prices again. It's
  all anyone talks of. Prices again. You'd think they'd settle."): `preface` adds a recall only before an opener that
  doesn't stand on the ledger itself (`talk_tests`).

- **What the first card found** (two hard gates the baselines passed, broken by what the fixes made visible):
  K3, now line by line, saw a joiner told again what she had been told an hour before ("Bad news, is it? Tell me."):
  one who has been told what a talk is telling no longer cuts in, unless it is about them or their own (the stakes
  named, kin, friend); and a tavern regular told a claim again a week later, his told set's 64 rows filled by what he
  overheard (every claim said in his hearing): 256 a listener now (`TalkLedger::ToldPerResident`; the gossip replay's
  told rows grow 13,800 to 15,600). C6 saw "I know, no?" / "I know, no?", an echo scene's two backchannels in
  Mirebridge's tag questions: `renderWith` says a turn again with other words (up to 15 tries) when it would ask a
  question its exchange has asked (`talk_tests`).

**The lines.**
- **Claim openers pass the claim on.** After Phase 7 every entry says what it does, so an opener that states the claim
  under a move that doesn't assert (`gs_secret`, `does confide`: "Don't tell a soul, but X stole..."; `et_gs_apprentice`,
  `does ask.detail`) passed nothing, though the listener was marked told and never offered it again. Both `asserts` now,
  and `talk_tests` checks the whole library: an entry whose first line says `#topic.claim#` asserts it.
- **Reasons true of who gives them** (`opinions.json`): "I can't buy a loaf on what I'm paid" only from one with work
  ("there's nothing coming into my house, and still they put the bread up" from a poor wolf with none, "we go without half
  the week, and they never go short" from a pup); "I've worked my whole life in their shadow" from a working adult or an
  old one ("they'd sooner leave a bench empty than take on the likes of me", "they ride past our door and never once look
  down"); a jar among friends, the dole paid for, earned, queued for, a sermon that never put bread on one's table and a
  feast's day of work lost only from grown wolves, or ones with work (`talk_tests`: a merry, poor, lower pup's reasons).
- **Greetings and goodbyes by the hour** (`plain.grammar`): `#greet#` says "Morning" only before noon (after noon "Good
  day", "Hello"); `#farewell#` at dusk and night "Good night", never "Good day to you"; the strangers' and the cold
  wolves' goodbyes have their evening words; a new `#good_day#` (Good morning, Good day, Good evening) stands for the
  literal "Good day" of 36 greeting and goodbye alternatives (13 files); two "Morning, ..." said at any hour were reworded. `truth.json`'s
  `morning_greeting` sees a bare "Morning," at a line's start, and a new `day_greeting` a "Good day" at night (not "Good
  day today", "Good day for it"), so T2 counts them (`talk_tests`: 200 greetings at three, every goodbye at nine at night).
- **A death is never the better news**: `s8p_crime_life` isn't for a death or a succession, which has
  `s8p_crime_life_grave` ("And that's not the only sorrow this week."); `s8p_life_gossip`'s "That's the kind news" is
  "That's the town's news. There's other talk, too.".
- **One known wolf is he or she** in the converted scenes the review named: `e4_fa_priest_named` ("What did you tell
  #a.priest_them#?"), `et_rm_arrest_houses` ("One of Greatly's, they say.", "Then #topic.they#'ll be out by noon."),
  `et_appr_news`, `et_death_spouse` ("I'll take #topic.where# some supper.", "Bright One keep #topic.them#."),
  `h7_cr_ans_cold`, `h7_arrest_visit`.
- **True of who says it**: a pupil at school says "Teacher's looking." (`cv_pre_school`), not "This won't earn my keep."
  (`cv_pre_work` is a worker's); the pier's porters (`s83s_due_porter`, `s83s_ans_due_poor`) are poor grown civilians,
  never a pup or an innkeeper; the price callbacks follow the price (`ag_cb_prices_up` "dearer than when we spoke of it
  last", `ag_cb_prices_down`) and name the good; the tavern's mourners say `#tonight#` ("today" until dusk); "Long way from
  home" is never said to one born where they stand (`self.home_here`, from `from` and `here.region`;
  `m7_home_re_local_any` answers for any town that has no local answer of its own); a host greets a guest eight ways, not
  two ("Sit yourself down." was 7.1% of frozen-L6's lines).
- `talk_equiv`'s EDITED names each of the 18 entries changed by hand and why; `talk_lint` has no new finding (59, frozen).

**The ledger.**
- **Gossip out of hearing no longer pushes out what was heard.** World::gossip started a new note every day for every pair
  it told, and the 8 a pair were soon all of them, so a close pair's `pair.last_talk`, outcome, thread, kept tags and
  agreement lasted 8 game days, not 30. A pair keeps its 8 heard notes and, beside them, the last 2 days out of hearing
  (`TalkLedger::UnheardPerPair`, `trimNotes`), each claim once a note.
- **The cap drops gossip, not what was heard.** Gossip re-stamped its sets every midnight, so past 30,000 records the
  sets heard talk used went first (from day 48 of the tick, a pair that talked daily lost everything; K3's one repeat in
  the tick was this eviction). Gossip's writes stamp a set once and behind every heard one (`touchQuiet`); reads of the
  told and heard sets count as use; past the cap the notes out of hearing go first, then sets only gossip wrote, then the
  least recently used (`ambient_tests`).
- **The checkpoint**: replaying gossip's writes over P0's fixture (25 days), the ledger's notes, told set and stamps come
  to 2.56 MB by day 54 (18,523 records), from 7.16 MB (25,186): about 11,360 daily notes are 2,960 now, and days
  are saved to a ten-thousandth. §8.7's "about 2 MB" is near; the told set (15,600 rows at 256 a listener) is most of
  the rest.
- **The dead's rows go.** Drift, its events, moods and kept tags were never pruned: `prune` now takes who is gone
  (World: dead or no longer an entity) and drops theirs (`ambient_tests`).

**Speed.** `Library::checkText` bound `grammars_.count("plain") ? grammars_.at("plain") : Grammar{}` to a reference,
which copies the whole plain grammar for every alternative checked: three quarters of the load once Phase 8 made
plain.grammar 44 KB. A reference now: the library loads in 0.11 s (`talk_check talk.smalltalk`), from 3.58 s, and
Slice 1's 0.57 s was mostly this copy too. Every server, scratch server and talk_check --validate starts that much
sooner, and a fixtures profile, which starts a game at each of the fixture's 26 checkpoints, runs in 170-270 s (ten at a time) not
about 650 s.

**The instrument.**
- **R1, the hard gate, compares the talk, not its ceremony**: the opener's own words (talk_sim's `bare`, without what was
  said in its breath: a greeting, "Back again?", a recall) and the lines between, the closing left out. A goodbye said
  another way had made the same talk new ("Good evening. My jaw hit the floor. Finn Saltwell tried to steal..." heard at
  7.98 and 15.78 hours by one fixtures-L2 listener, only the goodbyes differing).
- **K3 counts line by line**: talk_sim writes each line's `told_key` (what it is telling, the talk turned to it or not),
  and K3 counts every piece of news a talk tells to everyone in it, a joiner too, the town's crime and news (`life` is in
  NEWSY) among them.
- **C9 doesn't count "Back again?"**: the engine no longer marks a line callback for its `again.open` alone, and the
  scorer counts callbacks and returns only.
- **C3 counts a curt ending as an ending** (the engine's own short close, §8.4, which the raters liked best).
- **R6 is the most-spoken sentence's share, R4 a distinctive sentence heard again** (a fragment spliced into longer lines
  counted for neither); R6_detail keeps the whole line's.
- **D1L and D1B judge what is said**: a line whose moves are all ceremony (a greeting, "Back again?", kin's first words,
  the closing) is left out (D1_detail's `body`); D1, the hard gate for none, still judges every line.
- **C4 sees a light bridge into grave news within a line**; **T6 sees "they" across a line** (the answer to a line naming
  one known wolf that names nobody).
- **talk_rate**: raters who share no item report "n/a" rather than crash, and a rank correlation of exactly 0 counts
  (`test_talk_rate`).
- `test_talk_score` has a test for each (R1, C3, C4, R6, K3, T6, D1B; 42 tests); `test_talk_p8` counts a tag as no word.
- The quick card's baseline (`Tests/fixtures/talk_scorecard_quick.json`) took R1, R4, R6, C3, C4, C9, K3, T6, D1L and
  D1B measured again (`--refreeze`, with why); scored with the old scorer, the new run passed it unchanged.

**The suites.** Core: ctest, the 101 suites that need no database, all pass (talk_tests 272,952 checks,
ambient_tests 252, game_tests, dialogue_tests, talk_beats, talk_golden, talk_coverage, talk_scorecard_quick, talk_bench
among them; run after every fix). The eight database suites (`*_pg`, `pg_client`) were not run: they need
`Database/.env`'s settings, which this session wasn't given; they skip without them. Talk: `talk_check --validate` on
the 169 `.talk` and `.grammar` files (10,362 entries say what they do; 15 new), `--coverage` (0 holes of 1,188,000, 0 in
the core moves), `talk_lint` (59, no new finding), `talk_equiv --against 1e159420`, `tools/dialect_check.py` (65,454
lines, 0 failures, the same 14 warnings; `--world` wasn't run, the editor role's password being the main checkout's),
`test_talk_score` (42), `test_talk_rate` (8), `test_talk_lint`, `test_talk_p8`.

**The scorecard** (`artifacts/talk/runs/s2r`, every profile in both modes over P0's fixture, pinned to build 81:
11,927 and 24,519 conversations, 65,774 and 160,720 lines; the card ran in 6 min 40 s, from about 12). It was run twice: the first broke K3 and C6
(above, "What the first card found"), and those were fixed before this one. Fixtures / frozen, against Slice 2's card
(`s2`, §34), Slice 2's own transcripts scored with today's definitions (what the definitions alone move), and this one:

| | Slice 2 (`s2`) | `s2`, today's definitions | **Slice 2's review (`s2r`)** |
|---|---|---|---|
| R1 first identical talk (h) | 42.9 / 8.29 | 15.5 / 4.0 | **6.63 / 4.09** |
| R2 / R3 | 6.49 / 2.28; 2,885 / 2,158 | the same | **6.47 / 2.60; 2,890 / 2,180** |
| R4 a sentence heard again in 10 h, per 100 h | 276 / 3,180 (lines) | 934 / 5,049 | **825 / 5,560** |
| R5 / R6 (%) | 0 / 0; 1.21 / 1.1 (lines) | 0 / 0; 2.69 / 2.64 | **0 / 0; 2.69 / 2.48** |
| R7 distinct replies per 1,000 | 269 / 125 | the same | **280 / 126** |
| C1 owed answers answered (%) | 98.9 / 98.7 | the same | **98.9 / 98.7** |
| C3 good endings (%) | 89.0 / 90.7 | 99.7 / 100 | **99.7 / 100** |
| C4 / C6 | 0.0281 / 0; 0 / 0 | the same | **0.0152 / 0; 0 / 0** |
| C7 / C8 (%) | 5.64 / 6.59; 0.217 / 5.44 | the same | **5.68 / 6.63; 0.178 / 5.40** |
| C9 callbacks per 10 | 3.72 / 2.15 | (old runs' flags) | **1.45 / 0.745** |
| T1 / T2 / T3 per 1,000 | 0 / 0; 0.0281 / 0; 0.014 / 0 | 0 / 0; **7.95 / 2.25**; the same | **0 / 0; 0.0456 / 0; 0.0152 / 0** |
| T5 (%) / T6 per 1,000 | 0.784 / 0.22; 0.239 / 0.006 | the same; 1.60 / 0.066 | **0.866 / 0; 2.07 / 0.075** |
| K1 / K2 / K3 | 45.0 / 75.2; 20.8 / 12.7; 0 / 0 | the same | **44.9 / 74.3; 22.6 / 13.2; 0 / 0** |
| D1 / D1L / D1B (%) | 0 / 0; 1.8 / 2.78; 10.6 / 7.12 | 0 / 0; 1.47 / 2.16; 7.55 / 4.70 | **0 / 0; 1.68 / 2.29; 8.28 / 4.74** |
| D2 / D3 (%) | 0 / 0; 87.2 / 65.2 | the same | **0 / 0; 85.0 / 82.5** |
| U2 / X1 / R8 (%) | 39.4 / 29.6; 18.5 / 16.4; 0.103 / 0.107 | the same | **37.5 / 34.5; 18.6 / 16.0; 0.110 / 0.109** |
| B1 (ms) / B5 | 0.339 / 0.365; 0.761 / 70.9 | | **0.368 / 0.345; 2.33 / 74.9** |

`talk_card` breaks **no hard gate**. R1, R4, R6, C3, C4, C9, K3, T6, D1L and D1B were measured again as redefined and
taken into both baselines (`--refreeze`, the why kept in each), as Slice 1's review did its own. What they say:
- **R1 is an open hard gate in both modes now** (6.6 h fixtures, the earliest of the listeners: L3's, at Larkfield, its
  101st talk; L1's four median 79 h; frozen 4.1 h). It passed in the fixtures only on the goodbye's variety; the same
  talks, scored the new way, were already 15.5 h. The body of talk repeating within a day is the open work (§34's "For
  Slice 3"), not hidden by its ceremony any more.
- **T2** is the new rules: Slice 2's own talk said "Morning" after noon or "Good day" at night 7.95 / 2.25 times in a
  thousand lines (572 and 270 lines); today's fixtures 0.0456 is three lines: "My son rides with the Southmeadow
  caravan." twice from wolves with no son (§34's open item, the lint's frozen `cv2_left_worry` family) and "Morning to
  you" at eight in the evening from one resident's own words (her `live.npc_voice` layer, not the library: the mantles'
  work). C4 0.0152 is one line, the old lexicon clash §34 names.
- **C9** is 1.45 / 0.745 without "Back again?", under its 1.5 target: the real callbacks the raters didn't punish, now
  measured honestly. **C3** is the curt endings counted. **D1L, D1B** fall as the goodbyes leave them; D3 frozen rises
  (65 → 83%) with other picks (L3's seven judged speakers).
- **C8** (fixtures, soft) 0.217 → 0.178% (22 talks with a third, 28 at `s2`; the baseline's 0.401 from Phase 4): joiners
  told the news already no longer cut in, and those gone home don't. Moved by design, not refrozen.
- **R8, U2, X1, B1, B1P** are flagged as at `s2` (§34: more talk, everyday talk and shared lines, the mantles' work, the
  card's load). **B5** fixtures 0.761 → 2.33 exchanges a second, the library's load.

**The findings, counted again on the card** (`s2` → `s2r`, both modes): mend talks 622 → 15; same-side airings answered by
agreement 0 of 178 → 1,019 of 1,303; a reason sentence said twice in a talk 153 → 1; favours begun and asked 85 of 528 →
143 of 230 (73 of the rest put off, "Later, if it'll keep. I'm working.", and 14 a step no line fitted); goodbyes that
say one is going followed by talk in the room within the hour 1,726 of 1,985 → 2 of 1,328, none by an innkeeper (273);
"Morning" after noon 572 → 0, "Good day" at night 270 → 0; written-route talks that turned 0 of 29 → 11 of 25;
a pup's untrue words 510 → 22 (all "Work won't wait.", `plain.grammar`'s `#excuse#`, left for the next round); "Long way
from home" to one born there, an objection to one's own news, a death called better news, the tavern's "tonight" by day:
0. K3 is 0 line by line in both modes.

## 36. Slice 3: the causes of the failed blind ratings, a critic's read, and the second blind packet (2026-11-17)

Branch `r3-talk-s2`, on Slice 2's review (49cc49dd). §34's raters scored Slice 2's talk 3.38 against the old library's
3.77 (the shuffled control 1.69), and said why. Each cause was checked against the branch as §35 left it; §35 had
fixed none of them (it stopped counting "Back again?" as a callback and counted a curt ending as an ending, which
changed what the card measured, not what was said). All are fixed here.

**The raters' causes, and their fixes.**
- **"Back again?" in ~30% of talks** (rated 2.50 against 3.65). `preface` says `again.open` now only before small talk
  (day, weather, smalltalk, food, pastimes, animals, work, travel, land, season, player, lore, home, friends, festival,
  dreams; never a memory, health, faith or anyone's news), only before a light or even line, a quarter of the times the
  frame holds, never beside a recall or a return, never when its words echo the opener ("Back again? Swallows are
  back.": no word of four letters or more twice in one breath, and not the opener's first word). Two who met today
  aren't greeted again by the other frames either (the innkeeper's "Sit yourself down." to the same regular at noon
  and at midnight; kin's "Oh. Da." every hour). Openers carrying it: 29.5% of the Slice 2 tick → 3.0% of this slice's tick (and 1-8% in the frozen and fixture runs read).
- **A farewell pair after one exchange** (rated 2.85; curt endings 3.90). A talk that closes with fewer than four
  lines said ends there, without ceremony (`ended` = `parted`: the game's outcome is "warmed", as a farewell's is, and
  it costs no regard; the scorer's C3 counts it, as it counts curt). A talk of two exchanges or more closes as before.
  One exchange and goodbyes: 37.5% of the Slice 2 tick → 0 (59% of the tick's talks part after one exchange, 35% close with goodbyes after two or more, 5% curt).
- **`Library::join` cleared what was owed** (item 23: "Don't let me stop you. I'll just listen.", and the one spoken
  to never answered). The join no longer clears it, and `sayNext` keeps the others' obligations through a line that
  adds a word, sides or teases (`join.add`, `join.side`, `join.tease`; one who objects, defends or corrects turns the
  talk to themselves, as before). One who joins for the pleasure of the talk (stake `favourite` or `friends`) does so
  only where the budget has room for talk after it, not only goodbyes; one who has just joined never closes the talk;
  `join.add` expects a welcome, not any ack ("Oh, let me in." / "So it goes.").
- **A reply-written scene as an opener** (`s83s_late_sailor`, item 6): its three lines now open, naming the ship and
  saying it's stormbound ("Still no sign of the Kestrel. She'll be tucked in a cove somewhere, waiting out the weather."; and "No the Kestrel." elsewhere in the file is "No sign of the Kestrel.").
- **Answers that don't check the news.** `s83v_ans_news_poor` answers by the way the price went (`topic.dir`, which
  `World::sceneTopics` now sets on the wagon's news from its words: dear, up; cheap, down; the Times, neither), and lost
  "I'll hear about it when it hits my supper." (planks); `e8r_cva_news`'s "Good for Coldwater." is two rules by the way
  it went; `s83e_ans_old` answers by `conv.ev` (bad or a loss: "In my day it was the same, and we managed."; good: "We'd
  have made a feast of less, in my day."; news); `s83s_ans_old` answers a ship due, in or sailed, and a ship late or
  lost has `s83s_ans_old_grave` ("The sea gives and the sea takes."); `h7_cr_ans_sump`'s `{h7_said=blame}` is on the
  three lines about blame only, so "Up the hill, that's news." is never answered "Nobody's blaming you." (item 73).
- **Several openers in one breath** (item 73, three; 5.8% of the tick's first lines with three moves or more). One
  word before the opener at most: a greeting the meeting asks for (strangers, kin, a host) goes before a recall, and a
  recall or a return leaves out "Back again?". First lines with three moves or more: 0 in the tick.
- **The place said twice** (`e8r_cv_news`, item 7): "Word off the last wagon: #topic.news#." (the news names its town).
- **A bare "Aye." for a confession** (item 66): hard memories (a house lost, a hard childhood: `e3_mr_poor`,
  `e3_mr_frank`) are tagged `{mk=hard}` and answered as hard (`e3_ma_hard`, `_warm`, `_poor`, `_quiet`, `_young`,
  never "That's lovely. Really." or "... Good times."); the quiet answer says a word more.
- **22 pups said "Work won't wait."** `ds_preclose` (`#excuse#`, each dialect's grown-up reasons: "The shift won't
  wait.", "Time's money") is for adults and the old; pups have `ds_preclose_young` (`#preclose#`). 0 in every run read.
- **"Morning to you" at night from a resident's own words** (§35's T2, her `live.npc_voice` layer, which tests no hour):
  `expandSymbol` passes over a layer's words that name another hour (good morning, morning to you, good day at night,
  good night or good evening by day, as truth.json judges them) and takes the layer below's, plain's, which say the
  hour. Her own words are still hers at dawn (`talk_tests`).

**What a critic's read found** (about 250 conversations, read whole, from frozen L1, L4, L6 and L8 and fixtures L2 and
L3 over P0's fixture, 20 to 30 world hours each, after every round of fixes), and fixed:
- **A question answered by an answer to another** ("Was much taken?" / "Orla Wickby had the worst of it."; "But why?" /
  "Never you mind who."; "Did anyone see?" / "Bar your door at night."). Every question in `shared8_detail.talk` says
  what it asks (`{ask=who|source|sure|what|when|where|why|many|more|other}`), the answers that name a who, a what, a
  where or a when are said only to the question that asks it (28 rules gated by `conv.ask`), and the engine forgets
  the kind when a question that doesn't say is asked; `ask.detail` no longer expects advice.
- **A name said twice in one line** ("I heard something about Corwin Bilgestack. That Corwin Bilgestack tried..."):
  six alternatives (`gs_heard`, `et_gos_shocked`, `et_gos_guard`, two of `shared8_gossip.talk`, `h7_theft_named_friend`)
  name them once, then say he or she; `h7_theft_named`'s "Then why aren't they in the gaol?" of one wolf is
  `#topic.they#`.
- **A price that names nothing** ("101 pence! I can afford it this week."): five lines name the good.
- **A reaction that asks and is never answered** ("Poor Rupert Fogton. What was taken?") is a statement now. (Answering
  every question left at a talk's end with a fitting `answer.detail` was tried and taken out: "Is it scary?" / "Not
  mine, I'll tell you that." read worse than a question let fall.)
- **A goodbye that closes again** after a preclose was taken ("Back to it, me too." / "I'll let you be."): the farewell
  after one is never a preclose-and-farewell line.
- **Two opinions, one after the other**, from one wolf: one view a talk.
- **An apology for nothing** (`st2_apology`, "I'm sorry about what I said." to one nothing was said to): only after a
  talk that went sour.
- Three bridges and a recall that didn't follow ("Not that there's much spare time." into pastimes; "It doesn't pay for
  itself, mind."; "We'd only just finished with the last of this.").
- **A pup's single line answering the third of three parallel lines** ("My shutters are swollen shut again." / "I like
  the rain. The gutters are rivers."): written parallel in that scene. Across the runs read, 3-5% of a scene's turns
  cross its parallel thread this way, the speaker's own one-line answer to whichever line came; a renderer rule that
  kept to the thread was tried and taken out (the quick card's R1 fell 4.62 → 1.86 h: those one-line answers are much
  of what makes the same scene sound different in other mouths). It is the mantles' work.
- The scorer: C3 counts `parted`; truth.json's `fisher_self` doesn't take Coldwater's endearment "my catch" (", my
  catch.") for one's own catch.

What it leaves (read, not fixed): gossip is the largest topic in the taverns and the same few thefts are told in many
rooms (the world's news, K2); some pivot bridges are loose; "Lock your door." and "Sleep sound." are the night's
commonest goodbyes; a scene's alternative chosen by kind of speaker can still miss its line (above).

**Tests.** `talk_tests` has `sliceThree` (one exchange parts, two say goodbye; "Back again?" before small talk 10-80
times in 200 and never before a crime, an echo or beside a recall; a resident's own "Morning to you" never at nine at
night and always hers at dawn; a joiner's line and the one spoken to still answering; no closing line after a preclose
taken); five expectations of a farewell after one exchange now expect the parting. `talk_equiv`'s EDITED names the 16
entries changed by hand that the conversion check sees (a tag taken off by hand is one of them). The quick card's C7
(lines per talk) was refrozen with why (`Tests/fixtures/talk_scorecard_quick.json`), the rest of it unchanged.

**The suites.** Core: ctest, all 103 that need no database pass (the eight `*_pg` and `pg_client` skip without
`Database/.env` in the worktree). Talk: `talk_check --validate` (all 179 files; 10,375 entries say what they do),
`--coverage` (0 holes of 1,188,000), `--render-trees`, `talk_lint` (59, no new finding), `talk_equiv --against
1e159420`, `talk_migrate --check`, `talk_tag`, `talk_conv`, `tools/dialect_check.py` (65,490 lines, 0 failures, the 14
warnings; `--world` not run: the editor role isn't the worktree's), `test_talk_score`, `test_talk_rate`,
`test_talk_lint`, `test_talk_p7`, `test_talk_p8`.

**The ticking 100-hour listen** (Q6, the one the user allows at a slice's end; reported):
`talk_sim --world ridgemere3 --tick --profile L1 --places r2_the_knucklebones,rm_the_grey_gull_inn,rm_the_rainbarrel,
rm_the_bilge --hours 100 --seed 1 --ablate shuffle` (`artifacts/talk/runs/s3-tick`), Slice 2's four rooms (named, as
then: the save was at night), from DEV's save as it was at 09:22 UTC (game day 33.09) to day 58.09: 100 world hours,
world build 81, nothing written. **3,422 s of wall time** for the ticking (57 min 6 s with the load), alone on the
machine; **1.18 GiB peak memory**; 6,798 conversations, 31,638 lines. It ran on the commit before the last three line
edits (the ships' "No the Kestrel", the leaving goodbye's words, a bridge gated off crimes; the card below has them).
Against Slice 2's tick (`s2-tick`, rescored with today's scorer), the shuffled control in brackets:

| | Ticking L1, Slice 2 | **Ticking L1, Slice 3** |
|---|---|---|
| Talks opening with "Back again?" / three moves or more in the first line | 29.5% / 5.8% | **3.0% / 0** |
| One exchange, then goodbyes | 37.5% | **0** (parted: 59%) |
| C7 lines per talk (lengths: 2 / 3 / 6+ lines) | 6.5 | **4.65** (12% / 47% / 37%) |
| R1 first identical talk, earliest listener (h; median) | 10.1 | **6.1** (6.4) |
| R4 a sentence heard again in 10 h, per 100 h / R6 the most-spoken sentence | 3,789 / 1.89% | **2,417 / 1.15%** ("Off I go, then.", 365: widened since) |
| R7 distinct replies per 1,000 | 98 (90) | **118** (105) |
| C1 owed answers answered / C3 good endings (%) | 98.5 (19.4) / 98.8 (39.0) | **97.6** (24.7) / **98.7** (70.8) |
| C4 tone clashes per 1,000 beats / C8 a third / C9 callbacks per 10 | 0.29 / 2.6% / 4.07 | **0.16 / 1.9% / 1.65** |
| T2 untrue presuppositions / T3 self-reference per 1,000 lines | 3.96 / 0.23 | **0.03 / 0.10** |
| K1 everyday / K2 the largest (crime) / K3 the same news twice | 46.9% / 21.2% / 1 | **45.5% / 24.2% / 2** |
| D1L / D1B / D3 (%) | 0.68 / 1.03 / 78.7 | **0.49 / 0.41 / 84.2** |
| O1 opinions per 10 talks | 0.80 | **1.25** |

What it says: the ceremony the raters punished is gone (the again-opener ten times rarer, no stacked breaths, no
goodbyes after a single exchange), and talk is shorter for it: most talks are one exchange and part, as the
baseline's were, and a third run on past a second. R1 fell (10.1 → 6.1 h): the same three-line scene told again by
other mouths is the same talk, now that the goodbyes that followed it don't count and nothing else follows it; it was
and is an open hard gate. C3's control rises because a shuffled talk that parts is a good ending too: C3 no longer
separates (the scorer's flag). T2 fell to one line (a wolf with no son, "My son rides with the Oakhaven caravan.":
§34's open `cv2_left_worry`); K3's two are an incident and a theft told to one wolf twice (Wenna Tarford, Hanne
Sootham), left for the ledger. C9 is the callbacks without "Back again?" and with one breath before an opener.
Read from its transcripts: "They'll come kicking our doors in for it." / "Nobody's blaming you." still follows (a line
about blame, rightly tagged); an agreement chosen by topic, not issue, can miss ("You want to know who's to blame? The
quality." / "Good. My purse could use the rest."); a bridge said "Not all the news is bad" before a theft (gated since).

**The scorecard** (`artifacts/talk/runs/s3`, every profile in both modes over P0's fixture, after every fix here; 8 min
34 s, run once, after the tick). Fixtures / frozen, against Slice 2's review (`s2r`, §35):

| | Slice 2's review (`s2r`) | **Slice 3 (`s3`)** |
|---|---|---|
| R1 first identical talk (h) / R2 first repeated opener (h) | 6.63 / 4.09; 6.47 / 2.60 | **6.75 / 4.14; 6.35 / 2.50** |
| R3 distinct openers per 3,000 / R4 a sentence again in 10 h, per 100 h | 2,893 / 2,177; 825 / 5,555 | **2,772 / 1,991; 767 / 5,462** |
| R5 / R6 the most-spoken sentence (%) / R7 | 0 / 0; 2.69 / 2.48; 280 / 126 | **0 / 0; 1.52 / 1.34; 278 / 128** |
| C1 owed answered (%) / C3 good endings (%) | 98.9 / 98.7; 99.7 / 100 | **98.5 / 98.7; 99.8 / 100** |
| C4 / C6 | 0.015 / 0; 0 / 0 | **0 / 0; 0 / 0** |
| C7 lines per talk / C8 a third (%) / C9 callbacks per 10 | 5.68 / 6.63; 0.178 / 5.40; 1.45 / 0.745 | **4.57 / 5.81; 0.092 / 1.29; 1.20 / 0.825** |
| T1 / T2 / T3 per 1,000; T5 (%); T6 per 1,000 | 0 / 0; 0.046 / 0; 0.015 / 0; 0.87 / 0; 2.07 / 0.075 | **0 / 0; 0.037 / 0; 0 / 0; 0.67 / 0; 2.70 / 0.071** |
| K1 / K2 / K3 | 44.9 / 74.3; 22.6 / 13.2; 0 / 0 | **45.0 / 74.3; 20.9 / 13.0; 0 / 0** |
| D1 / D1L / D1B / D3 (%) | 0 / 0; 1.68 / 2.29; 8.28 / 4.74; 85.0 / 82.5 | **0 / 0; 0.78 / 1.10; 5.04 / 3.75; 83.7 / 81.7** |
| U2 / X1 / R8 (%) | 37.5 / 34.5; 18.6 / 16.0; 0.110 / 0.109 | **34.8 / 31.5; 18.8 / 16.5; 0.164 / 0.141** |
| B1 (ms) / B5 | 0.368 / 0.345; 2.33 / 74.9 | **0.457 / 0.431; 1.73 / 52.5** |

`talk_card` breaks **no hard gate** (exit 1, as at `s2` and `s2r`, on soft rows only), and nothing was refrozen. Moved
by design: **C7** (one exchange parts), **C8** (a third who joins only for the pleasure of the talk does so where it
has room to go on: 22 → 13 talks with a third in the fixtures; frozen 5.4 → 1.3%), **C9** (one breath before an
opener: a recall gives way to the greeting a meeting asks for), **R6** (the most-spoken sentence 2.69 → 1.52%). Moved
with them and left open: **D1L, D1B** (dialect words in what is said: shorter talks say fewer of the lines the dialects
colour most; the mantles' work), **U2, X1, R8** (as at `s2r`), **B1, B1P, B1L, B5** (timings under the card's load;
`talk_bench` passes alone). T2's one line in the fixtures is §34's "My son rides with the ... caravan."; T5 and T6
moved with other picks (no new kind of finding). R1 is the open hard gate it was (fixtures 6.75 h, frozen 4.1 h).

**The second blind packet (H1 again, §15.7).** `tools/talk_rate.py packet --new artifacts/talk/runs/s3-tick/tick-L1
--old artifacts/talk/runs/p0-baseline/fixtures-L1 --each 20 --seed 3 --out <the rater's folder> --key
artifacts/talk/rate/s3/key.json`: **60 conversations**, 20 from this slice's ticking listen, 20 from the Slice 1
baseline (P0's, the old library: the same source as the first packet) and 20 from the tick's shuffled control,
sampled over topic family and length, in a random order, numbered and nothing else. The packet (`packet.md`,
`rate.html`, `ratings.csv`) was handed to the raters outside the repository; the key is
`artifacts/talk/rate/s3/key.json` (don't open it before rating). Scored as the first: `python3 tools/talk_rate.py score
RATINGS... --key artifacts/talk/rate/s3/key.json --json artifacts/talk/rate/s3/h1.json --report
artifacts/talk/rate/s3/h1.md`, with the mean weighted to the lengths heard beside the packet's even one.

## 37. The hybrid: the engine chooses, a written scene says it (2026-11-17)

Branch `r3-talk-s2`, on Slice 3 (9d69ff60). **Why.** The second blind packet (§36, 60 conversations, three raters, the
same two axes) was rated: Slice 3's ticking talk **3.48** overall (believable 3.33, coherent 3.62), the old library
(P0's baseline) **3.79** (3.48, 4.10), the shuffled control **1.38**; new − old −0.31 (95%: −0.85 to +0.24). The
raters' scores are `artifacts/talk/rate/s3/ratings-blind.json`, scored as `h1.json` and `h1.md` beside the key. What
they marked down: the same wolf speaking twice running, jumps to another topic ("festival days come out of nowhere"),
cryptic replies ("Two empty pots."), muddled openers stacked in one breath. Read against the key, the pattern is plain:
**the 9 of the 20 new talks that were one whole scene (with or without a goodbye pair) rated 4.11 on average (eight
of them 3.67 or more); the 11 stitched from rules, or a scene with a second scene or an opinion after it, rated 2.95**,
and all four rated under 2.5 were stitched (a scene followed by a second scene or an opinion, a farewell said by the
one who had just spoken, a rule's opener answered by a line written for any opener). The old system mostly plays whole hand-written scenes, coherent by
construction; Slice 2's engine stitched talk line by line from separate pieces (in Slice 3's tick 47% of lines came
from rules, against 13% in the baseline), and the stitching is what lost the flow.

**The design (the user, 2026-11-17).** The engine decides *what* is said: the topic, when, between whom, the truth
checks, who has heard what (the ledger), opinions, moods and temperament for choosing, no repeats. **Written whole
scenes carry the body**: the engine selects an authored multi-turn scene that fits (topic, pair, facts, truth, not
heard, mood and kind), binds names and facts, and plays it as written. **Line-by-line stitching only at the edges**: a
greeting in the opener's breath, the goodbyes, a joiner's word and its answer, a short answer to a direct question, a
thread's return; never two lines running by one speaker unless the scene is written that way; never a change of topic
mid-talk unless a written scene carries it. `settings.json`'s `"hybrid": true` turns it on for the library
(`"hybrid_stitch": 2`, the most lines stitched after a body, goodbyes aside); a library without it (the tests' small
ones) keeps the line-by-line engine.

**How a talk is chosen now** (`Core/RatwTalk.cpp`):
- **The body.** `Library::begin` asks `pickOne` with `scenesFirst` for a talk of two residents on a topic. Of all that
  holds, a whole scene (two turns or more, rendered for these two) carries the talk, whatever a single line's score; a
  strict priority (a death) holds only against scenes of that priority. **The director's facts decide between them**:
  each option is credited with the topic's facts its rendered lines say (the words of `topic.price`, `topic.news`,
  `topic.who`, a ship's name, case aside) and the flags it tests (`topic.bare=yes`); where a scene says as much as any
  line could, the scenes that do carry it; where only a rule says more (the shelf bought bare, "fourteen pence the
  dozen", a price's cause), **that rule opens a short exchange** (its opener and the answer owed it, always taken, and
  nothing after unless asked). Only where no scene fits at all does a rule open. When every scene that fits would tell
  the one spoken to again (the ledger), the last two of begin's six tries may take a rule.
- **Freshness.** A scene the listeners have heard is told in another of its parallel threads where it has one they
  haven't: the game keeps `entry#alt` of each talk's first line in `scenesHeard_` beside the entry
  (`Core/RatwGameAmbient.cpp`), and `pickOne` re-renders a heard scene until its first line is a thread they haven't
  heard (up to six tries) before tiers and weights choose. The place's recent list, the collision filter, the ledger's
  told set and the cooldowns stand as they were.
- **Played as written.** A scene's turns answer each other as written, so in the hybrid none owes by default (its next
  turn is its answer; its last owes only what is written). A parallel line never answers past a speaker's own line
  chosen by their kind: after a one-alternative line chosen by the speaker's facts (`b[old] : In my day we hanged
  thieves at the gate.`) the next parallel line would answer the line before it ("Right on their own doorstep."), so
  the scene ends on the kind's line (Slice 3 had measured 3-5% of scene turns crossing their thread this way).
- **The edges.** No lull at all after the body: no opinion, pivot, goal, change of subject or extend ("Oh, before you
  go..."). What a line owes past the body is answered only when it is a direct question, a joiner's word, or a rule's
  opener where no scene fit; at most `hybrid_stitch` such lines. Nobody speaks twice running outside a scene: an
  obligation owed by the one who has just spoken is passed over, the closer is never the last speaker, and a break for
  rain or a fight is said by the other (a call away stays the called one's). The goodbyes: a talk of fewer than four
  lines parts without ceremony (as Slice 3 had it); a longer one gets the farewell pair alone, no preclose before it
  (four lines of leave-taking after four of talk was the ceremony Slice 2's raters marked down); a scene that ends on a
  question nobody answers ends there ("Yes. Sometime. Tomorrow?" is never met with "Right. I'm away.").
- **The opener's breath.** Nothing asked in it that nobody answers: a greeting that asks ("Morning. Sleep well?",
  "Back again?") is left out; no greeting or recall before a goodbye said as a talk of its own (`st_farewell`, retagged
  `farewell / farewell.return`) or before a repair (`m7_acc_sorry_what`, retagged `repair / clarify / assess`: "Good
  evening, friend. Come again?"). The thread's return ("As I was saying...") and a recall of the last talk in its breath
  stand.
- **Kept from Slices 1 and 2:** the truth checks and the lint, the ledger and never told twice, pronouns by sex,
  dialect grammar in every line, the collision filter, told-once stories, joins (their word and its answer),
  interruptions and breaks, the topic director and its everyday floor. **Given up, by the design:** opinions aired
  mid-talk (O1 0), goals begun mid-talk (learn a name, ask a favour), pivots and marked changes of subject; a third who
  joins only for the pleasure of talk mostly finds no room (C8).
- **`part`**: every line says what part of the talk it is (`SaidLine::part`, talk_sim's `part`): `scene`, `exchange`
  (a rule's body where no scene fit), `edge` (stitched past the body) or `close`.

**Where whole scenes were thin, and the scenes written for them.** A first hybrid listen (fixtures L1, 100 h) showed
the topics that had no scene for what the director chose: ships (no scene at all), the week's events (none), the
Times (none) and three caravan kinds (a wagon due, a wagon late, the word the last wagon brought: 348 of Slice 3's
452 caravan talks in the taverns were stitched from single lines). **66 scenes** in four new files, each 3-5 turns in
three parallel threads with lines by the speaker's kind, in the dialect symbols (`talk_migrate --write` took 52 of
their backchannels into symbols), under the world-text rules (no "dog", male or female, a ship is "she", nothing of
crew, cargo or road the facts don't give, the lost grieved): `scenes/common/hybrid_ships.talk` (16: due, in, late,
lost, sailed), `hybrid_caravans.talk` (20: due, late, the road's news by the way the price went), `hybrid_events.talk`
(23: each of the twelve events while it is on, more for the drovers and the works fire, any event coming or over) and
`hybrid_times.talk` (7: hard and good times, with and without a cause). After them a scene carries 89-100% of the
talks on every topic the listens heard (the caravan, the events and the ships ~90%, the rest of the misses the
recent list and the told set at work).

**What the critic's read found and fixed** (about 250 conversations read whole before the ticking listen, from frozen L1-L4 and fixtures L1 of
four rounds of hybrid listens, the taverns first):
- A stitched answer to a scene's last statement ("You didn't." / "There's always something."): past the body only a
  direct question is answered.
- The parallel thread crossed after a kind's line (above).
- "Evening, neighbour. Mind how you go, Brenna Abbot." (a greeting before a goodbye), "Good evening, friend. Come
  again?", "Morning. Sleep well? Down here they'd rob...", "Back again? I've written a poem.": the opener's breath
  (above).
- "Right. I'm away." after "Tomorrow?"; four lines of goodbyes after a four-line scene: the closing (above).
- "My son rides with the Frosthollow caravan." from a wolf with no son (T2, open since Phase 1, and heard far more once
  scenes carry the talk): `cv2_left_worry` says "The Frosthollow caravan's gone. Long road, that." / "They'll be back
  soon." "Down here they'd rob Corra Ropewell and sell them back their own shoes": `#topic.victim_them#`,
  `#topic.victim_their#`. "Good day, then." said at dusk as the last line of a scene rendered by day
  (`m7_wx_strangers_polite`, the quick card's T2): "Take care, then." Each in `talk_equiv`'s EDITED.
- The same pair telling each other the same scene in the same words within the hour, and one wolf telling the same
  gossip to three neighbours in a listener's hearing: the parallel threads by what was heard (above).
- A rule that said what no scene did (the shelf bought bare, the price, `market_behaviors`) lost to scenes of the topic
  in general: the director's facts (above).
Left as read: the old library's own scenes still have their faults now that they are most of the talk (a scene's
fact never backed: "Bo Gilbey's offering a reward for his things."; a cat in a wolf's house; generic gossip scenes
told to pups); the joiner's word is sometimes left unanswered when no welcome fits; a scene rendered whole at its
start can say an hour's words a minute past the hour.

**Tests.** `talk_tests` has `hybridTalk`: a whole scene carries every talk it fits though a rule scores higher (60 of
60; line by line the rule opened most), a short exchange where none fits, the scene as written and then its goodbyes
(no line stitched after its last statement), a question the scene owes answered at the edge, nobody twice running
outside a scene (a five-line scene's goodbye said by the other), a scene ending on a kind's line, and over the real
library 400 talks on ten everyday topics: 90%+ carried by scenes, no pivot, opinion, request or extend, never more
than two lines stitched past the body. The tests of the line-by-line engine turn the hybrid off
(`Library::setHybrid(false)`, in `foundation()` and after each load of the real library). The quick card
(`Tests/fixtures/talk_scorecard_quick.json`) is refrozen on R7, C7, C9, D1A, O1 and K2 with the why (by design: a
scene and its goodbyes), its hard gates passing; the lint's baseline is blessed at 58 (the "my son" finding gone).
**The suites**: Core, the full ctest (all 101 that need no database pass; the eight `*_pg` and `pg_client` skip
without `Database/.env` in the worktree, as at §36), `talk_bench` and the quick card among them; Talk: `talk_check
--validate` on every file (10,441 entries say what they do), `--coverage` (0 holes), `talk_lint` (58, none new),
`talk_equiv --against 1e159420` (169 entries changed by hand, each said why), `talk_migrate --check`,
`tools/dialect_check.py` (66,395 lines, 0 failures, the 14 warnings), and the Python tests (`test_talk_score`,
`test_talk_rate`, `test_talk_lint`, `test_talk_p7`, `test_talk_p8`, `test_dialect_check`, `test_path_check`).

**The scorecard** (`artifacts/talk/runs/s4`, every profile in both modes over P0's fixture, on the final commit, alone;
the same values but timings as its run beside the ticking listen). `talk_card` breaks **no hard gate** (exit 1 on soft rows only, nothing refrozen). Fixtures /
frozen, against Slice 3 (`s3`, §36):

| | Metric | Slice 3 (`s3`) | **The hybrid (`s4`)** |
|---|---|---|---|
| R1 | first identical talk (h; an open hard gate) | 6.75 / 4.14 | **6.83 / 2.30** |
| R2 | first repeated opener (h) | 6.35 / 2.50 | **2.99 / 1.42** |
| R3 | distinct openers per 3,000 | 2,772 / 1,991 | **2,824 / 2,080** |
| R4 | a distinctive line heard again within 10 h, per 100 h | 767 / 5,462 | **456 / 2,080** |
| R5 / R6 | collisions per 100 h / the most-spoken line (%) | 0 / 0; 1.52 / 1.34 | **0 / 0; 1.47 / 0.83** |
| R7 | distinct replies per 1,000 | 278 / 128 | **142 / 94** |
| C1 | owed answers answered (%) | 98.5 / 98.7 | **95.1 / 97.6** |
| C3 / C4 / C6 | good endings (%) / tone clashes / a line twice | 99.8 / 100; 0; 0 | **99.8 / 100; 0; 0** |
| C7 | lines per talk | 4.57 / 5.81 | **3.47 / 3.35** |
| C8 / C9 | a third (%) / callbacks per 10 | 0.09 / 1.29; 1.20 / 0.83 | **0.09 / 0.07; 1.12 / 0.94** |
| T1 / T2 / T3 / T5 | raw ids / untrue presuppositions / self-reference / told what they knew | 0; 0.037 / 0; 0; 0.67 / 0 | **0; 0 / 0; 0; 0 / 0** |
| T6 | "they" of one named wolf, per 1,000 | 2.69 / 0.07 | **1.64 / 0.07** |
| K1 / K2 / K3 | everyday (%) / the largest topic (%) / news twice | 45.0 / 74.3; 20.9 / 13.0; 0 | **43.7 / 69.8; 22.4 / 15.4; 0** |
| D1 / D1L / D1B / D2 | dialect words: none / light / broad (%) / another's | 0; 0.78 / 1.10; 5.04 / 3.74; 0 | **0; 1.18 / 0.95; 3.46 / 2.85; 0** |
| D3 / U2 / X1 | dialect guessed / look-alikes told apart / replies sharing a word (%) | 83.7 / 81.7; 34.8 / 31.5; 18.8 / 16.5 | **86.3 / 65.2; 38.8 / 32.9; 24.0 / 19.6** |
| R8 / O1 | habit lines (%) / opinions per 10 talks | 0.16 / 0.14; 0.85 / 1.24 | **0.013 / 0.022; 0 / 0** |
| B1 / B5 | a beat (ms) / exchanges a second | 0.457 / 0.431; 1.73 / 52.5 | **0.208 / 0.190; 2.24 / 85.1** |

What moved, and why. **By design**: C7 (a scene and, if it ran past three lines, its goodbye pair), R7 (replies are
the scenes' own lines), O1 and C8 (no opinion or joiner stitched past the body), C1 (now measured only on what the
edges owe: a statement stitched past a scene is no longer answered, a scene's turns owe nothing by default). **Better**:
R4 (a distinctive line again within 10 h: 767 → 456 in the fixtures, 5,462 → 2,080 frozen), R6, T2 and T5 to 0, T6,
X1 (replies that share a word with the line they answer: 18.8 → 24.0%, near the old library's 26.8), U2 and D3 in the
fixtures, and the beat's cost halved. **Worse, and open**: R1 frozen (4.1 → 2.3 h: the frozen moment replays one hour's
crowd for a hundred hours, and its few topics' scenes come round; the fixtures' R1 held at 6.8 h) and R2 (a scene's
first line is the opener, so fewer openers than lines that could start a talk); D1B (broad speakers' dialect words:
the old library's scenes say fewer of the symbols the dialects colour than Phase 8's shared lines did; the mantles'
work); K2.

**The share of whole scenes.** Over all twenty card runs (124,791 lines, 36,841 talks): **83.4% of lines are written
scenes' turns, 9.4% goodbyes, 7.1% a rule's short exchange where no scene said the topic's facts, 0.1% stitched
edges** (a joiner's word and its answer, a question answered); 87.9% of talks open with a whole scene. The exchanges
are nearly all the prices (84% of price talks: the scenes of prices don't say the price or the good, the rules do,
"Have you seen theriac? Down to 101 pence." / "About time."), the ships (42%) and the caravan (33%), where the rules
say the ship's day or the wagon's goods the scenes leave out. **Thin, and the next scenes to write**: prices that name
the good and its price, ships and wagons by their day and goods.

**The ticking 100-hour listen** (Q6, the one the user allows at a slice's end; reported):
`talk_sim --world ridgemere3 --tick --profile L1 --places r2_the_knucklebones,rm_the_grey_gull_inn,rm_the_rainbarrel,
rm_the_bilge --hours 100 --seed 1 --ablate shuffle` (`artifacts/talk/runs/s4-tick`), Slice 2's and 3's four rooms, from
DEV's save as it was at 11:47 UTC (game day 33.69) to day 37.86: 100 world hours, 360,000 world seconds, world build 81,
nothing written. **3,021 s of wall time** (50 min 25 s), beside the card for its first ten minutes; **1.12 GiB peak
memory**; 6,876 conversations, 24,208 lines. **Also reported**: a first start of it, on the commit before the
director's-facts rule below, was stopped by hand after about ten minutes of wall time when
`market_behaviors` showed a rule's facts losing to scenes; nothing of it was kept. And one fault the listen found came
after it: the game's authored exchange (`beginWith`, "A quiet day, for once." / "It is at that.", 72 of its 6,876 talks)
was not played as the hybrid, so six of them ran on into an opinion, an invitation or a change of subject stitched
line by line (all six of the listen's "twice running" outside a scene, and its O1 0.02). `beginWith` is the hybrid's
now (`hybridTalk`'s fifth case); none of the 20 new talks in the packet is one of the 72. Against Slice 3's ticking
listen (`s3-tick`, rescored with today's scorer), the shuffled control in brackets:

| | Ticking L1, Slice 3 | **Ticking L1, the hybrid** |
|---|---|---|
| Lines from whole scenes / goodbyes / a rule's exchange / stitched edges | 53% from scenes, 46% from rules | **84.3% / 11.0% / 3.8% / 1.0%** |
| Talks a scene carries | 66% | **99%+ of crime, smalltalk, gossip, travel, faith, food...; caravan 77%, ships 58%, prices 16%** |
| Two lines running by one speaker outside a scene | 1,334 lines in 31,638 | **6** (the authored exchange's, above) |
| C7 lines per talk (2 / 3 / 6+ lines) | 4.65 | **3.52** (11% / 65% / 19%) |
| R1 first identical talk (h) / R2 first repeated opener (h) | 6.1 / 6.0 | **4.3 / 3.7** |
| R4 a sentence heard again in 10 h, per 100 h / R6 the most-spoken | 2,417 / 1.15% | **1,331 / 0.93%** |
| R5 collisions / R7 distinct replies per 1,000 | 0 / 118 | **0 (142) / 74** |
| C1 owed answers answered / C3 good endings (%) | 97.6 / 98.7 | **95.3 (24.3) / 98.9** |
| C4 tone clashes / C6 a line twice | 0.16 / 0 | **0 / 0** |
| C8 a third / C9 callbacks per 10 / O1 opinions per 10 | 1.9% / 1.65 / 1.25 | **0.64% / 1.68 / 0.02** |
| T2 untrue presuppositions / T3 self-reference / T5 told what they knew | 0.03 / 0.09 / 0.95 | **0 / 0 / 0** |
| K1 everyday / K2 the largest (crime) / K3 the same news twice | 45.5% / 24.2% / 2 | **45.3% / 24.3% / 2** |
| D1L / D1B / D3 (%) | 0.49 / 0.41 / 84.2 | **0.69 / 0.69 / 85.6** |
| X1 replies sharing a word with the line before | 18.7% | **23.9%** (2.5%) |

What it says: the talk is now what the raters scored highest, a whole scene and, when it ran past three lines, a
goodbye pair; nobody speaks twice running outside a scene; the truth rows are all 0; a line heard again within ten
hours is half as common, and replies share the words of what they answer as the old library's do. What it cost:
shorter talks, fewer opinions, joiners and distinct replies (by design), and R1 and R2 fall (a scene is the same talk
when it comes round, where stitching hid a repeat behind a different second half). K3's two are Slice 3's (an incident
and a theft told one wolf twice, the ledger's). Read from its transcripts (60 of them whole, the four rooms): the
caravan's and the events' new scenes heard in their rooms ("When the Coldwater wagon comes there'll be lavender
again." / "Again? When did we last have any?"); gossip and thefts by the old library's scenes with the names bound
and the truth checked; prices by a rule's two lines ("Have you seen theriac? Down to 101 pence." / "About time.").
Its faults, left: the old scenes' "they" of one named wolf ("Bo Rainmarsh must be heartbroken." / "They are.": T6, the
lint's `they_named`); `ac_homesick` asked in Ridgemere of Ridgemere ("What's it like, Ridgemere? I've never been." /
"Better than here"), the accent topic's who-hears-whom left open since §34.

**The third blind packet (H1, §15.7).** `tools/talk_rate.py packet --new artifacts/talk/runs/s4-tick/tick-L1 --old
artifacts/talk/runs/p0-baseline/fixtures-L1 --each 20 --seed 4 --out <the rater's folder> --key
artifacts/talk/rate/s4/key.json`: **60 conversations**, 20 from this ticking listen, 20 from the Slice 1 baseline (P0's,
the old library: the same source as the first two packets) and 20 from the tick's shuffled control, sampled over topic
family and length, in a random order, numbered and nothing else. The packet (`packet.md`, `rate.html`, `ratings.csv`)
is handed to the raters outside the repository; the key is `artifacts/talk/rate/s4/key.json` (don't open it before
rating). Scored as the second: `python3 tools/talk_rate.py score RATINGS... --key artifacts/talk/rate/s4/key.json
--json artifacts/talk/rate/s4/h1.json --report artifacts/talk/rate/s4/h1.md`, with the mean weighted to the lengths
heard beside the packet's even one.

**For the next round.** The packet's ratings decide whether the hybrid beats the old library (H1: new 3.8+, new − old
+1.0, new − shuffled 2.0+). Before them, what this section leaves: whole scenes that say the price and the good (the
prices topic is a rule's exchange 84% of the time), ships and wagons by their day and goods; R1 and R2 frozen (more
scenes on the topics a frozen hour repeats, or a scene's threads written further apart); the old library's own faults,
heard far more now (a fact a scene invents, "they" of one named wolf, the accent topic's who-hears-whom); opinions,
goals and joiners back only as written scenes that carry them (a scene of two who disagree on the Watch, an invitation
written whole), never stitched on.

## 38. Phase 9, the mantle engine and its tables, as built (2026-11-17)

Branch `r3-talk-p9`, on `ridgemere2` at 46b6ad88 (Slices 1-2 and §37's hybrid). Mantles exist end to end: written as
`.mantle` text, checked by the game's own loader and by `mantle_check`, written to a world's rows by one module that
checks, backs up and records, loaded by the game in one batch into buckets of their own, reloaded one at a time by
`mantle.sync`, and exported back to text byte for byte. No mantle is in DEV's rows yet: Phase 11's pilot writes them.

**The format** (§7.9; `Core/RatwMantle.{h,cpp}` reads it for the game, `tools/mantle_text.py` for the tools, key for key
the same, which `test_mantle_check` proves on every fixture mantle). The header is the card (§7.9's lines, plus
`written_by`, `persona` on one line, `format`). `words` holds the symbol lines; `opinion`, `line`, `dlg` are one line
each; `rule`, `scene`, `story` and `graft` are blocks that run to the next blank line or the next keyword at a line's
start; `grown`, `young`, `adolescent`, `old` (and `any`) are sections. Keys as §7.9 says, with `@name` after an item's
keyword (first on a symbol's line) and the next free number otherwise; a symbol's first line is `words.<symbol>`. The
canonical form (the export) writes every item with its name, so the text the game assembles from the rows gives every
item the key its row has. Decided while building:
- **A `scene` kind** (a mantle's own whole scene, which its wearer opens), beside §17.1's seven: §37 made written scenes
  the body of every talk, so a mantle that is to be heard must hold scenes, not only lines. The table's `kind` check has it.
- **A story is a whole scene** (§37): `: words` is a beat of the wearer's, `ask : words` a question of theirs, `b : words`
  (and `b[old] : ...`) the listener's answer, written whole; one with no listener's line is refused. It does `tell.story`,
  `ask.detail`, `react.story` and `answer.detail` turn by turn, is said on its criteria's topic (`talk.memories` unless
  they name one) and, as a `tell.story`, once to each listener (the heard set).
- **An opinion** compiles to a `give.opinion` line under `conv.issue=<issue>`, and its unconditioned first one per issue
  is the wearer's stance: `op.<issue>`, `op_strength`, `op_basis mantle` over the worked-out one (`op_why` kept only where
  the sides agree). Its issue must be opinions.json's.
- **`dlg <tree>.<node>`** is a rule for `dlg.<tree>.<node>` (the node must exist); +5 makes it the wearer's line there.
- **A graft** is checked whole (its tree and node exist; every choice leads to one of its nodes, the tree's, `bye` or
  `end`; every node is reached, says something, and has a way back) and its nodes' says compile to rules for
  `graft.<tree>.<mantle>.<node>`; its shape (choices, nodes, moods, invitations) is kept on the card for the window
  (Phase 13 shows grafts).
- **Stage sections** are criteria: grown `self.stage=adult|old` (adult only where an `old` section exists), young
  `young|adolescent` (young only beside an `adolescent` section), adolescent, old.

**Migration 0066** (`Database/migrations/0066_mantles.sql`; rollback `Database/rollback/0066_mantles_down.sql`). §17.1's
tables, trigger, grants, binding columns, unique index (`npc_voice_mantle_worn`) and `game_changes` trigger, and:
- `live.mantle_changed()` raises a mantle's revision **once a transaction** (a 60-item batch is one revision; a
  transaction-local flag), each changed item's own, stamps `updated_at`, writes one history row with the transaction's
  `ratw.edit_source`/`edit_by`/`edit_note`; an update that changes nothing is left alone (loading twice moves nothing);
  SECURITY DEFINER, and nobody may write the history.
- `dm.queue_talk_sync(who, kind, target)`, as 0061's `queue_economy_sync`: Atlas's role can't write `dm.actions`, so this
  queues `mantle.sync <id>` or `voice.sync [npc]` only, and notifies the server.
- **Applied to `ratw_ridgemere3`** (DEV, Atlas's and the live server's world) at 07:36 after `live.npc_voice` was dumped
  to the main checkout's `Saved/Backups/ridgemere3-before-0066-npc_voice-20261117-073615.sql` (1,083 rows; no row was
  changed: new tables, four nullable columns and a constraint). The server on 7788 kept running (it reads the voice rows
  by name, at start and on `voice.sync`); it needs no restart. `mantle_load check ridgemere3`: 773 placed residents, 0
  mantles. The rollback is tested on throwaway databases (`test_mantle_load`: backup, down, 0066 again), not on DEV.

**The engine** (`Core/RatwTalk.{h,cpp}`, `Core/RatwMantle.cpp`).
- **Buckets** (§9.4's `byOwner_`): each mantle is a bucket of its own (`Library::Owned`: its rules and scenes, by concept,
  move and id, its words as the grammar `m_<id>`, its card), and so is each NPC's own lines (`setCharacter`, now an
  override: no copy of the library, no reindex: defect 16 is closed). A pick, an answer and a lazy pick look at the
  shared lines and the speaker's own buckets only (`forConcept`, `ownersOf`, by the speaker's `mantle` and `id` facts);
  `find` resolves a mantle's or an NPC's entry by its id. Mantle rules carry `self.mantle=<id>` and +5.
- **The share** (§8.2): at the edges (`answer`), a mantle's move lines are left out once its stitched lines would pass
  35% of the talk's; a whole scene of its own is the talk's body and is not counted against it.
- **The layer order** `p_<npc>` → `m_<mantle>` → the dialect → plain: a mantle's words are its wearer's whatever their
  accent (its `.broad` words for a broad one); its layer lives in its bucket, not among the dialects, so saving a dialect
  checks no mantle's words and a mantle never slows the library's own grammar checks.
- **Habits by mantle**: a habit drawn from `m_<id>` is seeded by the mantle, so its oath passes to its next wearer; the
  wearer's own layer (re-drawn marks, Phase 10) still wins.
- **Loading** (`setMantles`: every mantle at once, each compiled item by item so a problem names its key; one that fails
  is left out and named; only `ready` ones), **`setMantle`** (one swapped whole; a failure keeps the revision loaded),
  `checkMantle` (any state, kept nowhere), `dropMantle`, `mantle`, `wear` (the card's opinions, likes over the drawn ones,
  dislikes, fears, hopes). A mantle's lines are proved answerable (what they owe has a shared or own line that holds with
  nothing known, walking the fallbacks: §7.2's dead-end proof, per mantle).
- **The game** (`Core/RatwGameTalk.cpp`): `talkSpeaker` gives a wearer `mantle=<id>`, the `m_` layer, the card's
  talk-only traits and quirk (never honesty: §9.5), and once a day `wear`; `talkFavourites` puts the mantle's favourite
  topics first and its shunned ones out. `loadVoices` reads the bindings (where 0066 is applied; a world before it reads
  as before) and `loadMantles` reads, in two queries, the ready mantles worn by a living wearer and their items, plus the
  cards of every other ready one (`mantleCards_`, for Phase 10's pools), assembles each text (`talk::mantleText`) and
  loads them in one batch (`RATW_MANTLE loaded N mantles (E rules and scenes, R rows) worn by W residents in T ms`,
  `RATW_MANTLE refused <id>: <why>`). **`mantle.sync <id>`** (`Game::loadMantle`): one mantle's rows read again (any
  state: a draft or a retired one is unloaded and its wearer speaks from the library), swapped, its wearer found again;
  the answer names the revision loaded, or why not and the revision still in use. **`voice.sync <npc>`**
  (`Game::loadVoice`): that resident's voice row, own words and lines and mantle; any other target (a dialect, a figure,
  a batch, nothing) reads every voice again, as the DM tool's existing saves expect. **`--mantles-from DIR`**
  (`Options::mantlesFrom`; the server, `talk_sim`, `talk_check --world`): every ready `.mantle` under DIR, each worn by
  the resident it was written for, in place of the rows (Phase 10's checkpoint ledger will carry bindings for worlds
  without a database). facts.json: `mantle`, `fears`, `hopes`.

**The checks.** G1 is `talk_check --mantle PATH... [--dialects DIR] [--json]` (the game's loader, the world's dialects
over the files'). G2 is `tools/mantle_check.py PATH... | --one ID --world KEY [--strict] [--json]`, with its data in
`tools/mantle_rules.json` (not under Data/Talk, whose scans would read its own forbidden words): names of the world's
NPCs outside slots and capitalised words that aren't the realm's; no "dog", "gods", "tells" for private messages, male
or female only; he/she/him/her beside a person named by a slot; a word's value that ends its own sentence (defect 14);
truth.json's needs behind every line (with the section's stage); another dialect's words, and the own dialect's written
out where the accents allow none; no 5-word run shared within a dialect; a town's oaths, sayings and quirks (with
--world); habits in at most 25% of alternatives; and the tier's **minimum** (symbols, lines, an answer of its own to
greet.return, farewell.return and react.news, window lines and a story for the full tier, a spare's young and
adolescent sections): required of a `ready` mantle, warned for a draft. The tier's **size** (§9.3's 92, 55, 64) and the
**dialect density** (60%) are warnings until `--strict` (Phase 11's batches on): the fixture is far under both by design.

**The tools.** `tools/mantle_store.py` (every write: G1 and G2 first, `ready` needs its minimum, a retire refused while
worn, a stale revision refused naming who moved it and when, one transaction with the history's settings, items
inserted, updated or left alone, deleted only with `prune`, `bind`, `queue`), `tools/mantle_load.py` (§17.4's seven
steps; `check`, `backup`, `restore`; refuses a live world; a new unnamed item never takes a key the rows hold),
`tools/mantle_export.py` (§17.5; `--check`). `Data/Talk/mantles-export/` holds DEV's export: `index.tsv` alone, since DEV
has no mantles. The DM tool knows the `mantle.sync` act.

**The test fixture** (`Tests/fixtures/mantles/`, 16 mantles in all eight speeches but Fenshore's): the **ten luna tables
ported** (`rm001` Dagny Slaghand, `rm002` Ada Rainmore (a pup: a young section), `lf001` Berit Kelpwick, `lf002` Ada
Aldwych, `lf003` Agna Bilgecot, `fh001` Bramble Snowhearth, `oh001` Agnes Millford, `cw001` Dray Merewell, `sm001` Ada
Coalstack, `mb001` Brock Marshholm), each with `written_for` kept and its stale facts made true (Dagny sells fish in the
Rag Market, Berit's stall, Agnes's greens, Agna at the preserving house, Dray with no boat to speak of, Ada's bees at
Heatherside, Brock's fold at Larkdowns), the names they gave made slots or taken out (a priest, a trader, a husband,
neighbours, "the Maker": the Bright One), the punctuation that broke the window's templates gone, Dagny's "Morning to
you" said only at dawn, the barks kept as rule blocks with their cooldowns, the small talk turned into whole scenes, the
player greetings into window lines, and the lines they lacked for the tier's minimum written; `cw017` (§7.9's lake
swimmer, written for the talk town's keeper: every item kind, a story, a scene, a graft of two nodes), `sw001` and
`rm003` (the talk town's Saltwell storekeeper and Ridgemere pup), the spares `cw090` and `sw090` (young, adolescent and
grown sections) and the draft spare `rm090`. All pass G1 and G2 (27 warnings: size and density).

**Budgets** (§16; `mantle_tests --bench`, ctest `mantle_bench`, label perf: 805 mantles at the richer size, cw017 with five
more copies of its grown lines, **50,715 items**, 48,300 rules and scenes):
- **B3**: the batch parse and index **449 ms**; reading the rows (805 cards and 51,520 items, the game's two queries, a
  throwaway database on the local PostgreSQL 18) **about 0.53 s**: about 1 s added to start, of 3. One mantle swapped:
  **0.58 ms** median in the bench, **1 ms** for `mantle.sync` end to end (below), of 50.
- **B4**: **133 MB** more in use on the heap (133 MB resident), of 150: about 2.75 KB a compiled entry. Strings are not
  interned; it is the lever if the written mantles' longer alternatives push the real figure over.
- A pick for a wearer of a mantle costs 0.081 ms against 0.077 ms for one without (talk.weather, 400 picks).

**End to end, on a copy of ridgemere3** (a throwaway database restored from DEV's dump, dropped after): the ten luna
ports loaded with `mantle_load --batch e2e --bind` (10 written, 10 bound, the server told), a scratch game on the copy
(`mantle_tests --db`, which applies the DM's acts through `Game::applyDmAct` as the server does; a non-scratch server on
the copy stopped at its account checkpoint, which the copy lacks): `RATW_MANTLE loaded 8 mantles (77 rules and scenes, 110
rows) worn by 8 residents in 3 ms` (two of the ten aren't residents of the save); Dagny's shop greeting is `rm001`'s
(in her own words layer's greeting: the luna row's `p_` layer still sits above the mantle until Phase 11 empties it);
an Atlas-style save of one item (`mantle_store.save`, revision 2) then `mantle.sync rm001`: "revision 2 is loaded (11
rules and scenes, worn by rm_dagny_slaghand, 1 ms)", heard at once; a broken row written past the checks, then
`mantle.sync`: refused, "revision 2 is still in use", and she says revision 2's line; `voice.sync rm_dagny_slaghand`
reads her alone; `oh001` saved retired: unloaded; `rm999`: "No mantle rm999".

**Tests.** New: `mantle_tests` (ctest `mantle_tests`, 270 checks: the syntax and its refusals, keys and the text from
the rows, the loader's refusals by key, buckets and wearers, the window greeting the wearer's, a scene of its own as a
talk's body, the layer order and habits by mantle, answers within the share, the override, the hot reload keeping the
old revision, and the talk town wearing the fixture with `mantle.sync`, a draft unloaded and `voice.sync <npc>`),
`mantle_fixture_loads` (G1 over the fixture and the export), `mantle_fixture_rules` (G2 the same),
`mantle_check_rules` (`tools/test_mantle_check.py`: the tools' keys are the game's, canonical text stable, G2's
refusals), `mantle_load_pg` (`tools/test_mantle_load.py`, with the local PostgreSQL: 0066's trigger, grants, unique
wearer, restrict, `game_changes`, `queue_talk_sync`, the rollback and 0066 again; the fixture loaded, bound, exported,
loaded again with nothing changed, exported to the same bytes, `--check`; an Atlas edit found by `--check`; stale
revisions with `--take-theirs` and `--take-mine`; a new item never taking a row's key; refusals that write nothing;
backup and restore). The full ctest: 113 of 113 (pg_client skipped as before), the database suites run; with
RATW_PERF_TESTS, `talk_bench` and `mantle_bench`.

**The card** (`artifacts/talk/runs/p9`, every profile in both modes over P0's fixture, reading DEV's rows after 0066:
no mantle in them yet). Every metric is the base's (`ridgemere2` 46b6ad88's own card, `s5`) to the digit but the
timings, and the transcripts match line for line where compared (frozen L6, fixtures L7): with no mantle worn, the
buckets, the candidate gathering and the layer order change nothing that is said. `talk_card` exits 1 on the soft rows
§37 left flagged against the committed baselines (C8, O1, D1L, D1B, U2, K2, C9, X1: the hybrid's, by design or open)
and the card's timings under its own load (B1 frozen 0.218 ms against `s5`'s 0.197; B5 62 against 80); no hard gate is
broken and nothing was refrozen. Alone, `talk_bench` on the talk town: a beat 0.062 ms mean, p99 0.133 (the base's
build: 0.061, 0.132), a look p99 1.71 ms.

**For Phase 10.**
- **The bindings** are `live.npc_voice.mantle_id`, `bound_day`, `bound_how`, `released_day`; one living wearer a mantle
  (`npc_voice_mantle_worn`); the game's writes there show in `live.game_changes` (the game has UPDATE and INSERT on it).
  `mantle_store.bind` writes an authored one; death's release (`released_day`), births, arrivals and `mantle.assign`/
  `release`/`retire` are Phase 10's, through the same module. `Game::mantleCards_` holds the ready mantles nobody
  living wears (id, dialect, accents, fits, honesty): the pools' cards. A mantle's `fits` is parsed and checked as
  criteria but never tested against a wearer yet (§17.3.3, at binding).
- **Worlds without a database** bind by `written_for` under `--mantles-from`; the checkpoint's mantle ledger (§17.8)
  should take its place.
- **The Mantles panel** has its writes in `mantle_store.save` (checks, stale revisions, history settings, prune,
  retire-while-worn) and its reload in `dm.queue_talk_sync`; "Hear it" can render through `talk_check --mantle` plus a
  concept (not built). `tools/world_sync.py` and `tools/world_package.py` know neither the tables nor `live.npc_voice`'s
  four new columns: a sync to a world before 0066 would carry columns it lacks.
- **Left as they were**: the ten luna voice rows keep their `grammar`, `rules` and `persona` (their `p_` layer sits above
  the mantle's words: Phase 11 empties them once the pilot's mantles land); §11's new slots (`#other.called#`,
  `#self.workplace#`, `#self.trade_words#`, `#self.away#`...) are not facts yet; the persona template is stored and
  checked but only the window's Mind will read it (Phase 13); G3 (`talk_check --coverage --mantle`) and G4 are not built;
  `talk_scorecard_quick`'s town doesn't wear the fixture (it would move the quick card for no measure yet); the
  `Library::entries()` that `talk_check --validate` counts are the shared library's only.
- **The live world** (7788, ridgemere3) needs no restart: 0066 is additive and its server reads past it. A server built
  from this branch reads DEV's mantle rows at start (none yet) and answers `mantle.sync`.

## 39. Phase 10, passing mantles on, as built (2026-11-17)

Branch `r3-talk-p10`, on `r3-talk-p9` (8474c3de). A dead wolf's mantle now rests and passes to a newborn of its dialect;
a newborn is born with its mantle's voice, honesty and temperament with it (the user's Q4); one of the Country's own
takes a mantle up as it comes; the pools' health is logged and forecast; the DM can give and release a mantle by hand.
Births stay realm-wide, one for one with deaths, and no NPC is ever made but a child born in place of the dead (Q5;
`people_flow_tests`, unchanged, still prove it). The society decides (`Core/RatwHandoff.cpp`); the game writes what it
decided (`Core/RatwGameHandoff.cpp`).

**The ledger** (`CareerState::mantles`, `MantleBinding`: by wearer, living or dead): the mantle, how it was bound
(authored, spare, reused, echo, arrival, dm), the day bound and released, who knew the wearer well at release, whose kin
it echoes, the pool it came from, re-drawn marks (and a bank still to draw them from), and a newborn's voice (its birth
town, accent, seven traits, quirk). Saved in the checkpoint's `careers.mantles` only while it has anything (an older
save reads as none). A world in the database writes the bindings to `live.npc_voice` (§17.7) and reads them back as the
truth: `Game::syncMantleLedger`, after the checkpoint is read and on every `voice.sync`, takes each row's mantle, how,
bound and released over the ledger's and keeps the rest; a scratch server writes no row and keeps what it made itself.
A world from files binds each ready mantle to the resident it was written for (`authored`) unless the ledger says it
passed on. Every ready mantle's card (worn or not: §9.4's `mantleCards_` are gone) goes to the society for its pools.

**Death** (`Society::died`): the wearer's binding is released that day with `knownBy` (`CareerWorld::knownBy`: the
living residents with familiarity 40 or more toward the dead), a `mantle_release` career note, and a change the game
writes (`released_day`; `RATW_MANTLE cw017 rests: tt_co_keeper died on day 0 (known well by N)`). Memories, bonds and
beliefs stay the dead's. Revived before anyone took it up, they wear it again.

**Birth** (`Society::tendBirths`, as §12.3): the couple is chosen as before (realm-wide); the child's pool is its mother's
home's (`CareerWorld::dialectOfCell`, `World::dialectPoolOf`: the town's grammar, plain for Ridgemere, Fenshore's
mirebridge, nothing for a place none of the realm's). Candidates are the ready mantles of the pool not worn by a living
wolf and not asked for by a request still on its way (so a birth the host can't make leaves its mantle as it was: it was
never bound), whose `fits` holds for the child: `Society::fitsMantle` (and `mantle_store.fits`, the same) tests a stage
or an age as the wearer is or will grow to be (a newborn fits `stage=adult|old`), sex, role, job and class as they are (a
newborn is a civilian with no job, of its mother's class, so a `role=merchant` fit never goes to one), and a fact it
doesn't know never holds. In order: 1. an echo (rested, its last wearer kin of a parent: spouse, parent, child, sibling
or grandparent, by the society's families); 2. a rested one whose last wearer nobody living in the birth town knew well,
marks kept (`reused`); 3. any rested one, marks drawn again (`reused`); 2 and 3 never in the household of the last
wearer's kin; 4. a spare by a lot; 5. a neighbouring pool, through `settings.json`'s new `mantle_fallback`
(Frosthollow to Coldwater, Oakhaven to Larkfield, Saltwell to Southmeadow, Mirebridge to Oakhaven, followed on while each
has nothing), its marks drawn from the child's own dialect; 6. nothing: born as before, `RATW_MANTLE none for <dialect>
(birth)`. Rested is `mantle_rest_days` (new in `settings.json`, 365) since the release, longest rested first.

**The child** (`ResidentRequest::mantle`, `mantleHow`, `echoOf`, `dialect`, `redrawMarks`, `markDialect`, `voice`,
`personality`): its traits are the card's, all seven, the middle of an axis where the card says nothing (honesty plain:
`Society::wholeTraits`), so a `sly` card's pup can turn thief at 16 (the voice row's honesty feeds `Society::setHonesty`);
its quirk the card's; its accent drawn from the card's accents by the birth town's mix (the first, as written, where none
of them is heard there); its origin the birth town for life (its voice row's `home_region`, which `talkOrigin` takes over
the worked-out one: no flip at 18); its personality the card's in a few words (`Society::cardPersonality`: "Calm, warm
and content; swims in the lake even in autumn.", no pronouns), its greeting empty, its surname its mother's. In a world
in the database `Game::makeResidents` writes the child's `live.npcs` row with that personality, its parents into
`live.npc_relations` (every birth, mantle or not), and its `live.npc_voice` row (origin, accent, its mother's quarter,
traits, quirk, the binding); `World::makeRequestedResidents` mirrors it for worlds from files and scratch servers, where
the ledger is the voice (`Game::wearBinding`).

**Names** (`Data/Talk/names/<dialect>.names`, §7.10's format; eight pools of 68 to 72 female and 64 to 69 male names,
drafts for the user to settle by ear): read by `readNamePool` (refused with its reasons: one word, a capital and small
letters, none twice, none both female and male, male or female only) and checked by `mantle_pool.py names`. A child's is
its dialect's (plain's for one without a pool; the 28 placeholders where there is none, and in a society given no cards
and no pools, as every test before Phase 10 is): no two living residents (or children asked for) share a full name, no
living parent or sibling shares its first name, and one born in place of a dead grandparent of its own sex takes their
first name `named_for` of the time (.25).

**Marks and echoes** (§12.4): marks drawn again are the wearer's own words, never the mantle's (`p_<npc>`; in a database
world appended to the voice row's `grammar`, never over a symbol they already have): `Library::redrawHabit` takes a value
from the dialect's `mark.<symbol>` bank (none exist yet: Phase 15 writes them), else another of the mantle's own
alternatives than the one its seed gives (a mark with one alternative is kept). An echo's wearer has `echoOf`, and
`pair.echo=yes` with `pair.echo_of` (the dead's name) holds between them and anyone who knew the dead well.

**§11's slots as facts** (`facts.json`; `talkSpeaker`, the new `talkSlots` once a game day, `talkQuery`): `surname`;
`called` (what the one speaking calls this one: the first name between residents who know each other's names, a Master
or Mistress and the surname across a class gap in Ridgemere unless kin or fond; for strangers none, and
`#other.called#` then says the speaker's `#address#`); `spouse_word` (husband or wife); `late` (the kin last mourned,
dead); `friend` and `rival` (affinity 40 or more, -25 or less, living); `boss` and `hand` (a shop's keeper and the help
at its premises), each with its `_sex`; `workplace`, `trade_words`, `street`, `from_town`, `away`, `dialect`; and
`pair.echo`, `pair.echo_of`. (`boss` was a pair fact only: a bare `boss=` in a criterion would now read the person's; no
rule wrote it bare.)

**Arrivals** (§12.6): an "arrival" or a "farm" request (`arrivalFor`, `staffFarms`) takes a mantle from the pool of where
the newcomer grew up (their voice row's origin: the Country's 310 seed rows; else the town they come to), fitted to their
sex, age, role, job and class, bound `arrival` as they are welcomed; they keep their own voice row (§9.5).

**Pool health** (§12.1): `Society::mantlePools` (spare: never bound; resting; ready: rested; the births the dialect
expects in 180 days from the Gompertz deaths of the living and the births owed, shared as the fertile couples' mothers
are; the honesty mix of the pool against its living residents'). The game logs `RATW_MANTLE pool coldwater spare=N
resting=M ready=K` once a game day, a warning with `LOW` when spare plus ready is under 1.5 x the 180 days' births plus 2;
nothing while a world has no ready mantle (DEV today). `tools/mantle_pool.py forecast <world> [--years 5] [--json]`
reads a world read-only (one read-only transaction; the checkpoint's marriages and births with the rows' families) and
projects the same by year.

**The DM** (§17.10): the game's acts `mantle.assign <npc>` (payload `{"mantle": id}`; refused, writing nothing, unless the
mantle is ready, nobody else living wears it and the resident fits it; `bound_how 'dm'`) and `mantle.release <npc>`
(resting from today); `tools/dungeon_master.py` sends both (checked: the resident, a ready mantle) and retires a mantle
(`POST /api/talk/mantle/retire`: `mantle_store.retire`, refused while worn, then `mantle.sync`). `mantle_store.bind`
(`mantle_load --bind`) now checks `fits` and refuses a retired mantle.

**Atlas's column**: the NPC tab's list (Atlas and the DM) shows each resident's mantle beside their name (in brackets once
released), and a Mantle panel under the NPC says how it came to them and since when; the DM's gives one by hand or
releases it.

**Sync and packages** (§17.9): `live.mantles` and `live.mantle_lines` join `world_sync.SYNCED` before the voices that
bind them, compared item by item (an edit to one line in dev and another in live don't conflict; the same line on both
is a conflict), a mantle's `created_at` volatile, `live.mantle_history` each world's own (a sync's changes are recorded
as `sync`). `live.npc_voice`'s four binding columns are optional columns (a world before 0066, as PROD is, reads an
unset binding as equal, and a binding it can't hold is refused with why, and named in the import's plan); the bindings
are the live game's fields (`world_store.GAME_COLUMNS`). `world_package`: both tables are seed tables, add-only (an item
of a mantle the world hasn't is left out; a voice bound to one it hasn't comes unbound), `written_by` a model's, a
batch's, `dm`, `atlas`, `seed` or `package`, never a person's.

**Not built here**: the Mantles panel and its save endpoints (card, items, problems beside the item, history and revert,
"this mantle or this wolf only", Hear it) and `test_dungeon_master`'s mantle save; the fallback pool's words filtered of
the other dialect's (§12.3.5); the kits and mark banks (Phase 15); G3 and G4; `talk_scorecard_quick`'s town wearing the
fixture. The ten luna voice rows are as Phase 9 left them.

**The handoff, in a harness world** (`mantle_tests`' `handoffInTheTalkTown`: the talk town wearing the fixture, a scratch
game, nothing written): Myrtle Stillwater, Coldwater's keeper, who wears `cw017` (written for her), dies, and so do the
Saltwell rakers, leaving the Marshes of Coldwater the only couple of an age. `RATW_MANTLE cw017 rests: tt_co_keeper died
on day 0`. The calendar moved a year (nothing else run: no economy, no ticking days), the child owed is born: "Arthek
Marsh (born_40), born day 370 to Ysolde and Corwin Marsh, wears cw017 (reused), last worn by Myrtle Stillwater; temper
calm, honesty plain, from coldwater (light); 'Calm, warm and content; swims in the lake even in autumn.'" The game hears
the pup wear it (its facts: `mantle=cw017`, the card's temper, honesty and quirk, `from=coldwater`, `stage=young`), the
society's honesty map has the card's, and the ledger is in the save. **In a world database** (`test_mantle_load`'s
`PassingOnInADatabase`: a throwaway Greyfen world, its places made Coldwater's, `cw017` and `cw090` in its rows, a game
that writes, driven by `mantle_tests --db-life`): the DM gives Wren `cw017`, releases it, gives it to Sloe and back; Wren
dies and her row's `released_day` is written; nine days on, the child owed is born to Sorrel and Linden ("Wenneth",
`cw090 spare`: `cw017` rests), with its voice row (Coldwater, all seven traits, honesty plain), its parents in
`live.npc_relations`, the card's personality and no greeting, and the game's writes in `live.game_changes`.

**The forecast** (`python3 tools/mantle_pool.py forecast ridgemere3`, read only, DEV at day 34.1: 773 living residents,
69 couples of an age for pups once the checkpoint's marriages are counted). Births in the next 180 days and over five
years (births one for one with deaths, about 21 a year, 125 in five, shared where the mothers are):

| Pool | plain | coldwater | larkfield | frosthollow | oakhaven | southmeadow | saltwell | mirebridge |
|---|---|---|---|---|---|---|---|---|
| 180 days | 6.2 | 2.1 | 1.4 | 0.24 | 0.24 | 0.24 | 0.24 | 0 |
| 5 years | 67 | 27 | 17 | 3.4 | 3.4 | 3.4 | 3.4 | 0 |
| Living sly + guarded (%) | 21.0 | 22.4 | 19.5 | 33.3 | 50.0 | 15.8 | 23.1 | 22.7 |

Every pool is empty today (no mantle is in DEV's rows until Phase 11's pilot), so every pool reads LOW, and every birth
until then is born as before and says `RATW_MANTLE none for <dialect>`. Against §12.1's spares (20, 26, 10, 8, 6, 6, 6,
6): Coldwater's 26 and Larkfield's 10 about cover their five years with the mantles their own dead return after their year;
Ridgemere's 20 is thin against 67 births and leans on its own dead's mantles coming back (about 13 a year, §12.1), and
Mirebridge's 6 (Fenshore's) has no couple of an age today. Phase 15 sizes the spares from this, not from §12.1's guess.

**Tests.** `mantle_tests` (335 checks; new: `passingOn`, a small Society with synthetic deaths: `fits` as it is and will
be, one living wearer, the release with who knew her, a spare at birth with the card's seven traits, a Coldwater name with
no parent's first name, the grandmother's mantle as an echo a year on with her honesty, a Frosthollow birth lent
Coldwater's mantle with its marks to draw from Frosthollow's bank and named for its grandfather, `none` said, the pools,
the ledger saved and read back, the revived wearing hers again; `arrivalsBindOne`: the fisher from the Country comes with
a Coldwater mantle and keeps his own voice; `handoffInTheTalkTown`, above), `mantle_load_pg` (`test_binding_fits_and_
retire`: a pup's mantle refused to a grown wolf, a retire refused while worn and done after, a retired mantle never bound;
`PassingOnInADatabase`, above; the fixture's pup now aged as a pup), the new `mantle_pool_rules` (`test_mantle_pool`: the
forecast's arithmetic, the pools from bindings, `fits` as the game decides, the committed name pools clean and a bad one
refused with each reason), and `test_world_sync`'s `MantleItemsTests` (one line edited each side: no conflict; the same
line both sides: a conflict; the tables' order; the bindings the game's) and two Pull tests (a world without 0066 takes
voices without their bindings and refuses a binding, named in the plan; a mantle, its item and its binding pulled from
live, the history saying `sync`, level after). The full ctest: 116 of 116 (pg_client skipped as before). The Python
suites touched: `test_world_sync` and `test_dungeon_master` pass; `test_world_package` passes but for two cases that fail
the same on `r3-talk-p9` (its export of DEV now finds 12 planted accounts to close where it expected 8, and a hostile
package's import reaches the registry: DEV's data, not this branch). Editor: `tsc --noEmit` clean.

**The card** (`artifacts/talk/runs/p10`, every profile in both modes over P0's fixture, reading DEV's rows, alongside the
Python suites): every metric is Phase 9's (`p9`) to the digit but the timings (and frozen L5's R6 naming another of two
sentences tied at 38); `talk_card` exits 1 on the soft rows §37 and §38 left flagged (C8, O1, D1L, D1B, U2, K2, C9, X1)
and on timings under its neighbours' load (B1 0.221 ms); no hard gate is broken, nothing was refrozen. With no mantle in
the rows and no birth in a frozen hour, Phase 10 changes nothing that is said.

**For Phase 11.**
- Load the pilot with `mantle_load --bind`: a binding now checks `fits` (a pup's mantle needs a pup) and refuses a
  retired mantle. Once mantles are in DEV's rows the game gives every ready card to the pools and logs their health daily;
  the dead release theirs and the born take them from that moment, on the live server too once it runs a build of this
  branch.
- The pilot's mantles should use the new slots (`#other.called#` falls back to the speaker's `#address#`; `#self.friend#`,
  `#self.late#`, `#self.workplace#`, `#self.trade_words#`, `#self.street#`, `#self.from_town#`, `#self.spouse_word#`) and
  name 2-4 `marks` with more than one alternative each, so a reused mantle's marks can be drawn again before Phase 15's
  banks exist. `mantle_check` doesn't yet know which slots each truth pattern needs for the new ones.
- `talk_scorecard_quick`'s town still doesn't wear the fixture; the Mantles panel is still to build (its writes are
  `mantle_store.save` and `retire`, its reload `dm.queue_talk_sync`).
- The ten luna voice rows still hold their old grammar, rules and persona above the mantle's words.

**The live world** (7788, ridgemere3) needs no restart and nothing was written to DEV: there is no migration (0066's
columns hold everything), the forecast reads DEV in a read-only transaction, and every database test ran on throwaway
worlds. A server built from this branch reads the bindings and the checkpoint's ledger and passes mantles on from its
next death and birth.
