# 84. Living conversations: a voice of its own for every wolf, talk that answers itself, and players only in the window

Draft, 2026-11-15, for Ridgemere 3 (`ridgemere3`, database `ratw_ridgemere3`; doc 82). **Nothing here is built.** It
extends doc 62 (the talk library: facts, rules, grammar) and doc 83 (dialogue trees and the window) and replaces
neither: every one of today's 8,525 library entries and 10 trees keeps loading and speaking while it is converted. It
was written from five read-only surveys of the code, the data and the live world at `ridgemere2` 1034c1fc (the engine,
the trees, the lifecycle, what residents know, and prior art) and three competing designs; §4 says how they were
judged and what was taken from each. Decisions are marked **decided** (mine, for the user to overturn) or **open
(Qn)**, answered in §21.

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
| 1 | unique dialogue trees that follow the dialect of their region | Every placed resident wears a **mantle**: their own lines, opinions, stories and window branches, written without their name and natively in the dialect of where they grew up. The role trees keep their shape; the mantle fills their nodes in that wolf's voice and adds branches of its own. | §9, §10 | U1, U2, D1-D3, W1 |
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
  measured the quadratic load (defect 16). But about 0.7 million words (more than twice today's whole library) is not a
  realistic review load, a third voice was optional, and measurement came late.
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
neighbouring dialect, and logging the words.

**Contradictions, resolved:**

| Question | Engine-first | Content-first | Robustness-first | Decided |
|---|---|---|---|---|
| Name of the per-NPC unit | identity | mantle | identity / `.voice` | **mantle**: `Core/RatwIdentity.h` already means a player's central identity, and "voice", "character" and "persona" are taken |
| Where the text lives | a DB table (`live.voice_identities`) | files, DB override text | files; DB bindings and per-NPC overrides | **files**, DB bindings and per-NPC overrides (open, Q2, because it revises doc 62 decision 3) |
| How it is written | composed from ~900 shared packs, ~150 by hand | ~700-1,200 words each by hand | ~15-30 lines each by hand | **by hand, by tier**: about 40 lines for the ~220 public roles, 24 for the rest (size open, Q3) |
| Rest before reuse | 365 game days | 365 | 120 | **365 game days** (a setting), with re-drawn marks for wolves still remembered |
| Spares | 71 | 70 | 88 | **88**, at least 6 per pool |
| Speakers at once | up to 4 | 2 (3 optional) | up to 3 | **up to 3** in this build; the cap is a setting |
| Exchanges at once | 6 | 3 | 3 until measured | **3**, raised only when budget B2 shows room |
| Replies | tiers, then fallbacks | tiers | best fit, then tiers | **fit first** (must answer what is owed), **then tiers** within the fit |
| The move keyword | `move` | `act` | `does` | **`does`**, with `owes`/`invites` for obligations |
| Tags on alternatives | after the words | after | before | **before** the words: `{kind=look} Have you ...` |
| `react.customer` | keep, de-duplicated | an emote | an emote + `here.came_in` | **emote + `here.came_in`**, so the window greets |
| Free text in the window | classifier, Mind optional | router v2, Mind off | understanding layer, Mind off | **authored understanding, Mind off by default** (open, Q1) |

## 5. Decisions (summary)

All **decided**, for the user to overturn; each is explained where it is built.

1. **Measure first.** Phase 0 builds the offline conversation simulator, the scorer and a committed baseline scorecard
   before any engine or content change. Every later phase reports its scorecard diff and may not break a hard gate (§15).
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
    tier. Its lines speak in NPC-to-NPC talk and in the window, where it also adds branches (§9).
11. **Names, kin, trades and places are slots only** (§11).
12. **Mantles pass on**: 88 spares in eight dialect pools; a dead wolf's mantle rests a game year and passes to a
    newborn of the same dialect; no NPC is ever invented and births stay one for one with deaths (§12).
13. **A newborn gets a voice row at birth**: its mantle, a name from its dialect's pool, its birth town as its origin
    for life, its parents in `live.npc_relations`, a personality from its mantle instead of its mother's (§12).
14. **Residents never answer players' chat.** Everything a resident says *to* a player is in the window; unaddressed
    barks stay. Residents still notice what players say and may talk *about* it to each other (§13).
15. **The window speaks with the mantle's voice**: choices are moves; Ask about...; introductions; choices unlocked by
    what the player has heard; authored understanding of typed words (§13).
16. **Performance is budgeted and benched** (§16): batch loading and per-owner buckets before 1,000 mantles load.
17. **The ambient Mind path goes** once the final listen-through passes its hard gates (§13.6).
18. **The lean cadence**: one agent per phase in sequence, one build, targeted tests; the full suite, an end-to-end run
    and one review-and-fix at each slice's end (§18).

## 6. Words used here

- **Mantle**: one wolf's way of talking, written without their name. Worn by one living wolf at a time; it rests when
  they die and later passes to a newborn of the same dialect. The **wearer** is the wolf wearing it.
- **Move**: what a line does (greet, tell news, decline, preclose...). **Owes**: the answer a move makes due (a question
  owes an answer; a farewell owes a farewell).
- **Beat**: one line in a conversation. **Frame**: the kind of meeting (strangers, kin, at work...).
- **Ledger**: the talk ledger, what pairs talked about and how it went.
- **Pool**: the mantles of one dialect not worn by anyone (spares, and resting mantles of the dead).
- **Kit**: a dialect's shared lines for pups and adolescents.

## 7. The data model

Everything new is a file under `Data/Talk/`, checked when the library loads (an unknown move, tone, topic, frame, issue
or claim kind is refused, as an unknown fact is today) and by `talk_check --validate`. The database keeps only who
wears which mantle and the per-NPC overrides (§17).

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

### 7.9 The mantle file: `Data/Talk/mantles/<dialect>/<id>.mantle` (decided)

The example is written for a real resident: Myrtle Stillford (`tn_cw_myrtle_stillford`), 38, shopkeeper at Coldwater
Stores, from Coldwater, broad accent; calm, plain talk, warm, no humour, plain honesty, content, comfortable; swims in
the lake even in autumn. Her father is Orrin Stillford (`live.npc_relations`).

```
# cw017, the lake swimmer (doc 84). Written for tn_cw_myrtle_stillford; name-free; worn by one living wolf at a time.
mantle cw017
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
line request [other.job=carter world.bandits=near] : {request=carry} Will you take a box of my candles up the road with you?
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

### 7.12 Where everything lives (decided; storage open, Q2)

| What | Where | Why |
|---|---|---|
| The shared library, moves, topics, frames, temperaments, opinions, claims, truth, kits, names | `Data/Talk/` files | reviewed, versioned, gated in ctest |
| **Mantles**, spares included | `Data/Talk/mantles/<dialect>/<id>.mantle` | the same; they belong to no one world, so they ship to PROD with the build, no database push |
| Who wears which mantle, since when, how bound, when released | `live.npc_voice`, new columns (migration 0066, §17) | live data Atlas and the DM see; the game writes it at birth, death and arrival |
| Per-NPC overrides by the DM or Atlas, and re-drawn marks | `live.npc_voice.grammar/rules/persona` (existing) | doc 62 decision 3 stays true for one person: an override is that wolf's alone and is never passed on |
| Dialects | `Data/Talk/grammar/*` and `live.dialects` (existing) | unchanged |
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
| retired | taken out of use by the DM, or stale |

Status is worked out from the binding columns and the files: a mantle with no living wearer is spare (never bound) or
resting (bound before). There is no second store to drift.

### 9.3 Tiers and sizes (decided; the size is open, Q3)

| Tier | Who (placed) | Card | Words | NPC-to-NPC lines | Window | Stages | Lines, about |
|---|---|---|---|---|---|---|---|
| **full** | about 220 public roles: every merchant (127), guard (14), priest (7), innkeeper (15), master (60, overlapping), House family (15) | yes | 10-14 symbols | 4 openers, 10 move lines, 2 stories of 3 beats, 3 barks | hub, you, job, business, news, gossip, bye, the role tree's nodes; 2 graft nodes | old section if 55 or older | 40 |
| **standard** | about 553: everyone else placed, 97 pups and adolescents included | yes | 8-10 symbols | 3 openers, 8 move lines, 1 story, 2 barks | hub, you, news, bye, 1 graft node | young section for pups | 24 |
| **spare** | 88 (§12.1), sex-neutral | yes | 8-10 symbols | as standard | as standard | young and adolescent sections | 28 |

About **24,500 lines, roughly 300,000 words**, for the 773 placed residents and the spares. For scale, today's whole
library is 58,802 alternatives.

### 9.4 Loading (decided)

- **Batch, once.** `Library::setMantles(std::vector<MantleText>)` parses every mantle in one pass and reindexes once.
  Measured this session on the existing build: 20,000 rules in one `Library::parse` load in 0.85 s, against 22.4 s
  through 800 `setCharacter` calls (defect 16).
- **Per-owner buckets.** Mantle rules go into `byOwner_[mantle]`, never into the shared `byConcept_`, so a pick scans
  the shared bucket plus one speaker's own (about 25-40 rules): about today's 0.2 ms instead of 0.49 ms.
- **The layer order**, nearest first: `p_<npc>` (the wearer's own overrides and re-drawn marks: `live.npc_voice.grammar`)
  → `m_<mantle>` → the dialect → `plain`.
- **Only living wearers load.** `Game::loadVoices` (`RatwGameTalk.cpp:848`) reads the mantle files, the bindings from
  the database (or the checkpoint in a world without one), then the overrides. A new `Game::loadVoice(npc)` and the DM
  command `voice.sync <npc>` reload one wolf (today only the whole set reloads).
- **Mantle traits feed the society** as today's voice traits do (`setHonesty`, `setQuarters`), and its favourite topics
  feed `World::setTalkLeanings`.

### 9.5 Traits: kept, refined (decided)

A placed resident's mantle keeps their seeded `honesty` and `quarter` exactly, because those drive who turns thief and
the Market Board's district factor (doc 79 10.1; `RatwGameTalk.cpp:906-925`). The writer may refine the middle values
of the six talk-only axes (501 residents are "reserved", 592 have no humour) and may change a quirk where three or more
in a town share it (Coldwater has several lake swimmers). Nothing else in the simulation reads them.

### 9.6 The truth rules for mantle text (decided; enforced by `tools/mantle_check.py`)

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
"Aster Coldfell" become slots, the punctuation that breaks the window's templates goes, and the rows' `grammar`,
`rules` and `persona` columns are emptied once the mantle holds them.

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
- **`mantle_check` refuses**: any first name or surname of any NPC in `live.npcs` (living or dead) outside a slot;
  capitalised words outside symbols that aren't sentence-initial or on the allowlist (the realm's places, Glasswater,
  the Houses, the gods such as the Bright One, weekdays, ships); `he`/`she`/`him`/`her` about the listener; a habit value
  with its own end punctuation (defect 14); the word "dog"; "tells" for private messages; another dialect's words.
- **Rebinding is free.** Mantle rules key on the mantle, never the wolf: `self.mantle=<id>`, `byOwner_[mantle]`,
  layer `m_<id>`. Habits defined in the mantle are seeded by the mantle id, so a mantle's oath survives into its next
  wearer unless its marks are drawn again (§12.4).
- **Text that still names people stays per wolf and is never reused**: `live.npcs.backstory` (all 1,083 name the NPC)
  and the old persona. `#topic.greeting#` (`rules/player.talk:13-31`, the 169 broken greetings) is retired in favour of
  the mantle's window lines. If the Mind stays in the window (Q1), mantles carry a `persona` template with
  `#self.first#` slots.

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
- **Pool health**: the server logs `RATW_MANTLE pool coldwater spare=N resting=M ready=K` once a game day, and warns
  when spare plus ready falls below 1.5 × the births expected in the next 180 game days, plus 2. The warning is the
  signal for another authoring batch. `tools/mantle_pool.py forecast ridgemere3` (read-only) projects births by
  dialect from couples and ages. The DM's NPC tab shows the same.

### 12.2 Death: the mantle rests (decided)

`Society::died` (`Core/RatwCareers.cpp:873-897`) emits a `CareerNote{"mantle_release", resident}`. The host sets
`released_day` on the binding (database) or in the checkpoint's mantle ledger (`CareerState`, saved through
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
mantle to its earlier status. **The couple is still chosen realm-wide**; steering births toward the dead's region would
keep pools balanced (Coldwater runs about 2.7 a year short) but changes the economy sims (open, Q5).

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
  mantle's allowed accents by the town's mix, traits and quirk from the card (honesty open, Q4), `mantle_id`,
  `bound_how` (echo, reused or spare) and `bound_day`.
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
- **In the window's free talk: open, Q1.** Recommended: authored understanding only (§13.5), with the Mind behind a
  `--window-mind` option, off by default. Doc 83's answer 1 allowed the Mind to answer a player's own words; goal 6
  ("all preprogrammed") argues the other way. If it stays, it is given the mantle's persona template and the window
  brief, and `heed` ignores any promise it reports.
- **Summaries, recaps, books and letter polish** are untouched by this design.

## 14. Authoring plan

### 14.1 How much (decided; the mantles' size is open, Q3)

| Content | Phase | Size |
|---|---|---|
| Converting the library: moves and tones tagged on all 8,525 entries, ~121 answer concepts aliased, 2,652 short markers turned into tags | P7 | no new lines; every family checked by hand |
| Backchannel and discourse symbols | P7 | about 76 symbols × 8 grammars, about 1,500 alternatives |
| Shared lines the coverage matrix asks for: closings and pre-closings, known/new/doubt for every claim kind, details per topic, preferred and dispreferred pairs, pivots for the ~120 topic edges, repair, joining, breaks and returns, callbacks | P8 | about 1,500 rules |
| Thin topics, gated to the facts | P8 | gossip 70→250, event 49→150, news 43→150, newcomer 52→120, harvest and ships 11→60 each, deed 21→80, accent 26→60, times 21→60: about 600 rules |
| The data files (moves, topics, frames, temperaments, opinions, claims, truth) | P2-P6 | 85 moves; 45 topics, 120 edges; 8-10 frames, 6 goals, 4 moods; 21 trait values; 20 issues, 130 items; 25 claim kinds × up to 5 points of view |
| Mantles, full tier | P11, P14 | about 220 × 40 = 8,800 lines |
| Mantles, standard tier | P11, P14 | about 553 × 24 = 13,300 lines |
| Spares | P15 | 88 × 28 = 2,500 lines |
| Stage kits and mark banks | P15 | 16 kits × 60 = 960 lines; 4 banks × 8 dialects, about 1,100 alternatives |
| Name pools | P10 | about 1,100 names, drafts for the user |
| The window: new nodes, unclear lines, companion concepts, the understanding corpus | P13 | about 12 nodes, 80 say lines, 40 unclear lines, 60 companion lines; 500 corpus lines |

### 14.2 Who writes, and how (decided; authors open, Q3)

- **Claude agents in the user's sessions**, writing files, one town or one Ridgemere quarter per session, behind
  automated gates. No paid model calls (`tools/voice_tables.py` uses a paid story model; bulk runs need the user's go).
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

1. **Pilot** (P11): Saltwell 13, Fenshore 23 and the 8 placed luna residents, about 44 mantles. **The user listens; the
   format is frozen.**
2. Coldwater 66 (2 sessions).
3. Larkfield 36 (1).
4. Southmeadow 19, Frosthollow 9, Oakhaven 8 (1).
5. Ridgemere by quarter (13): commons 211 (5), the Sump 179 (4), the Estate 51 and the Heights 42 (2), the Works 32,
   the Office 32 and the Wharf 12 (2), with the 40 who have no quarter shared among them.
6. Spares, kits and mark banks (3).

### 14.4 Quality gates for every batch (decided)

1. **G1, the game's loader**: `talk_check --mantle` (syntax, facts, symbols, 160 characters, depth 6, graft
   reachability, `needs does=`).
2. **G2, `tools/mantle_check.py`**: names and the allowlist; truth and claims; stage, job and role gates; punctuation in
   habit values; no "dog", no "tells"; pronouns by symbol; the dialect lexicon by accent; dialect density 60%+;
   uniqueness (no 5-word run shared within a dialect, no oath or saying repeated within a town, a quirk at most twice
   in a town); habit saturation (`oath!`, `saying!`) in 25% of alternatives at most; the tier's minimum coverage.
3. **G3, `talk_check --coverage --mantle`**: the batch answers and is answered by sampled real listeners of its town.
4. **G4, the frozen scorecard** on ridgemere3 with the batch bound: no hard gate fails, no soft metric regresses by more
   than 10%, and U1, U2 and D1 for that dialect rise.
5. **G5, listening**: a 60-conversation packet (about 20 minutes). The user listens to the pilot and to the first batch of
   each dialect; for the rest, the slice's review agent reads a stratified 20% sample, and the user's packet is there
   if they want it. "Wrong" marks are fixed and listened to again before the batch merges.

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
  snapshots recorded once from a ticking run; needs a small `Options::checkpointFile`); `--tick` (the world really runs:
  a simulation, which needs the user's go-ahead, Q6).
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
   `artifacts/talk/marks/`, and the scorer reports the "marked wrong" rate per file family. `--packet` samples by
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
| Every commit (ctest) | the loader; `talk_lint`; `mantle_check`; coverage; golden transcripts (60 seeded conversations over a frozen copy of the library, `Tests/fixtures/talk_lib/`, regenerated with `--bless`); `talk_scorecard_quick`: a synthetic town export (`Tests/fixtures/talk_town/`: 3 cells, about 40 residents, 3 dialects), 2,000 frozen exchanges in under 60 s, no database, enforcing the hard gates and failing a soft metric that regresses by more than 10% |
| End of each phase | ridgemere3, frozen, every profile; the scorecard diff goes in the phase's report |
| End of each slice | a 100-hour listening run (frozen, or ticking with the user's go-ahead), a listen packet, and at Slices 2 and 5 the blind ratings |

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
| B3 | mantles add 2 s or less to start and to `voice.sync` | batch `setMantles`, one reindex |
| B4 | RSS grows 80 MB or less | interned strings for mantle entries (today about 2.3 KB an entry) |
| B5 | the simulator runs 150+ exchanges a second per core | ten profiles as separate processes take about 2 minutes frozen |

**Mechanisms:** batch loading and per-owner buckets (§9.4); only the speaker's own mantle is consulted; candidates are
rendered lazily in the chosen order; person facts are cached per conversation, because `peopleOf` and `neighboursOf`
scan positions; concurrency stays at `AtOnce = 3`, only where a player listens, until B2's numbers show room.

## 17. Migrating `ratw_ridgemere3` (decided)

**Migration `Database/migrations/0066_mantles.sql`** (the latest is 0065), with a rollback in `Database/rollback/`.
The server is PostgreSQL 18.6.

```sql
-- Who wears which mantle (Docs/Design/84 §17). The mantles themselves are files (Data/Talk/mantles).
ALTER TABLE live.npc_voice
    ADD COLUMN mantle_id    text NOT NULL DEFAULT '' CHECK (mantle_id ~ '^([a-z]{2}[0-9]{3,4})?$'),
    ADD COLUMN bound_day    double precision,
    ADD COLUMN bound_how    text NOT NULL DEFAULT '' CHECK (bound_how IN ('', 'authored', 'spare', 'reused', 'echo', 'arrival', 'dm')),
    ADD COLUMN released_day double precision;
COMMENT ON COLUMN live.npc_voice.mantle_id IS 'The mantle this resident wears or wore (doc 84); released_day set when they died.';
-- A mantle is worn by one living wolf at a time (goal 1).
CREATE UNIQUE INDEX npc_voice_mantle_worn ON live.npc_voice (world_id, mantle_id)
    WHERE mantle_id <> '' AND released_day IS NULL;
-- Rows the game writes (births, deaths, arrivals) show in Atlas's conflict list, as live.npcs's do (migration 0059).
CREATE TRIGGER game_changes AFTER INSERT OR UPDATE OR DELETE ON live.npc_voice FOR EACH ROW
    WHEN (current_user = 'ratw_game') EXECUTE FUNCTION live.log_game_change('npc_id');
```

A mantle's history is its binding rows ordered by `bound_day`; "worn", "resting since" and "spare" are worked out from
them and the files, so there is one truth.

**Backfill** (`tools/mantle_seed.py bind ridgemere3`; DEV writes are allowed, PROD is the user's push):
1. Each authoring batch binds its own mantles to their `written_for` residents (`bound_how='authored'`, `bound_day` =
   the world's day). Residents whose batch hasn't landed keep speaking from the library, as today.
2. The 10 luna rows have their `grammar`, `rules` and `persona` emptied once their mantles land (Phase 11).
3. Nothing else changes: `home_region`, accent, quarter, traits and quirk stay (a mantle's refinements of talk-only
   traits are in its card, not written back). The Country's 310 rows are untouched until they arrive.
4. `tools/mantle_seed.py check ridgemere3` reports placed residents with no mantle, mantles bound twice, and
   `written_for` ids that no longer exist; the scorecard and `voice_check` flag the same.

**The checkpoint** (`Core/RatwWire.cpp`, beside careers at :1011-1031) gains `talkLedger`, `talkCooldowns`, `news`,
`townNews`, residents' heard sets, `playerClaims`, and `mantles` (the society's ledger, for worlds without a database;
a database world reads its bindings from `live.npc_voice` instead, as it does for NPC rows). Older checkpoints load with
these empty.

**PROD** is on the old map at migration 54, so 0066 waits behind 0055-0065 there, and the mantles, written for Ridgemere
3's residents, bind only when PROD moves to Ridgemere 3. That push is the user's.

**Atlas and the DM**: the People panel shows each resident's mantle, its status and its pool; per-NPC overrides are
edited as today and checked by `talk_check --validate` before saving. New DM commands: `mantle.assign <npc> <mantle>`,
`mantle.release <npc>`, `mantle.retire <mantle>`, `voice.sync <npc>`.

## 18. Phases

The cadence is the lean one: one agent per phase, in this order, reusing one build, with targeted tests; the full suite,
an end-to-end run and one review-and-fix at each slice's end. Every phase's report carries its scorecard diff (§15.8).
Hours are rough, for one agent.

| # | Phase | Needs | Delivers | Tests and gate | h |
|---|---|---|---|---|---|
| | **Slice 1: hear it, then make it true** | | | | |
| P0 | The instrument | – | `ambientStart`/`ambientSpeak` split; `Tests/talk_sim.cpp` (frozen and fixture modes, virtual listeners, the ten profiles, `--ablate`, `--library-from`); `tools/talk_score.py`, `talk_listen.py`, `talk_rate.py`; the synthetic `talk_town` fixture and `talk_scorecard_quick`; `--log-talk-words`; **the committed baseline scorecard and a listen sheet for the user**; the most-heard residents list (it orders the authoring) | golden transcripts unchanged by the split | 12 |
| P1 | Truth fixes | P0 | defects 1-12, 14 and 15 (`heed` ignores promises from window free talk): structured claims (`Belief` kind, object, place; `claims.json`; `placeWords`), no raw ids, teller ≠ victim, `housemates`, `repeat` per concept, the cooldown store, every beat heard, `noteNews` for births and newcomers, one `claimKind`, `pair.named` between residents, the district priest, `exchangeContext`, news saved; `here.outdoors`; `truth.json` with `talk_lint` and the runtime check; the punctuation join; Coldwater's trade lines gated | T1 = 0, T3 = 0; T2 counted and frozen as the baseline | 10 |
| | Slice end: full suite, all profiles, review and fix | | | | 3 |
| | **Slice 2: the conversation engine** | | | | |
| P2 | Moves and tags | P1 | `moves.json`; `does`/`owes`/`invites`/`tone`/`asserts`/`denies`/`gist`/`binds`/`{k=v}` parsed; `Ns::Conv`, `Ns::X`; `byMove_`; aliases for the answer concepts; the dead-end proof; `SaidLine` metadata into transcripts; moves tagged on the 40 most-spoken families | loader refusals tested; transcripts carry moves | 10 |
| P3 | The turn engine | P2 | `Conversation`; `begin`/`next`/`answer`; fit then tiers; `frames.json`; budgets; closings; shrugs become `ack`; tone rules; `converse` rebuilt on `begin`/`next` | golden for unconverted content; C3, C4, C6 measured | 12 |
| P4 | Beats when spoken; knowledge; a third voice | P3 | per-beat queries in `Game::ambient` with cached person facts; breaks and returns; `joiners` (3 at most); per-beat effects; asserting, overhearing, corroboration, denial, `other.knows`; `talk_bench` | B1 within budget; T5, C8 measured | 10 |
| P5 | Topics, agendas, goals, the ledger | P4 | `topics.json` (today's numbers first, proved identical, then tuned); novelty, saturation, the everyday floor; the topic graph and pivots; `agendaFor`; goals; `RatwTalkLedger` in the checkpoint; person facts; unheard notes from `World::gossip`; invitations and talk-only promises | K1 35%+, K2 25% or less, K3 = 0 | 12 |
| P6 | Opinions, temperament, moods, accommodation | P5 | `opinions.json` with drift; `self.op.*`, `pair.agree.*`; likes; `temperaments.json`; `self.mood`; `world.mood`; `conv.accent` | T4 = 0; D1 measured | 6 |
| P7 | Converting the library; coverage | P6 | `tools/talk_conv.py` (2,652 markers to tags), moves and tones on all 8,525 entries, `talk_migrate.py` (backchannel symbols, plain identical by diff), the discourse symbols drafted in 8 grammars; `talk_check --coverage` and `profiles.json` | equivalence test (the same possible lines, old against new); coverage holes listed | 12 |
| P8 | The shared lines (three sessions) | P7 | about 1,500 coverage rules and 600 thin-topic rules (§14.1), in dialect symbols | 0 holes on the core moves; R7 3× baseline; C1 92%+ | 18 |
| | Slice end: full suite, all profiles, a listen packet, **the first blind ratings**, review and fix | | | H1 measured against the baseline | 6 |
| | **Slice 3: mantles and the lifecycle** | | | | |
| P9 | The mantle engine | P8 | the `.mantle` parser (card, words, opinions, stages, lines, stories, `dlg`, grafts); batch `setMantles`; `byOwner_`; `setCharacter` as an override; the layer order; habits by mantle; `loadVoices` rewrite, `loadVoice`, `voice.sync <npc>`; `mantle_check.py`; `talk_check --mantle`; migration 0066 and its rollback; the 10 luna tables ported as test content | B3 and B4 budgets; `mantle_tests` parse | 12 |
| P10 | Passing mantles on | P9 | the mantle ledger in `CareerState`; release in `died`; choosing in `tendBirths`; `ResidentRequest::mantle`; the voice row, relations and personality at birth; names and their checks (draft pools); re-drawn marks and echoes; the `makeRequestedResidents` mirror; arrival binding; pool health and `mantle_pool.py forecast`; DM commands; Atlas's column | `mantle_tests` with synthetic deaths in a small `Society` (no economy run); a harness world shows a handoff | 10 |
| P11 | The writer's kit and the pilot | P10 | `mantle_brief.py`, `mantle_index.py`, the writer's guide; **about 44 mantles: Saltwell, Fenshore, the 8 placed luna residents**; Saltwell and Mirebridge kits; bindings; a listen packet | G1-G4 on the pilot; U2 and W1 measured | 8 |
| | Slice end: **the user listens; the format is frozen** | | | | 4 |
| | **Slice 4: the window only** | | | | |
| P12 | Window-only plumbing | P11 | `residentsAnswerChat`; `overheard`; the emote and the hint; talk opens the window; `react.customer`, `welcomeAtInn` and `companionSays` converted; the companion tree's "What do you think?"; the storyline event from the window only; legacy lines removed; the client changes (§13.2) | W3 = 0; `dialogue_tests`; `game_tests` (introductions still learned) | 8 |
| P13 | The window's talk and typed words | P12 | `[does <move>]` choices and `answers` nodes; the hub rebuilt; Ask about...; `introduce`, `ask_name`; greetings by meeting and disposition; bye and thanks; day, hours, wanted, where-is; docs 52/54/55 nodes; grafts shown; the player claim store and knowledge choices; the ledger with players; `understand.json` and the 500-line corpus; unclear lines; `--window-mind` (Q1); line rotation; `#way.*#`/`#refuse.*#` symbols | W2 gates; the hub never shows more than 9 | 12 |
| | Slice end: end to end in the client on a scratch server (`tools/scratch.sh`) | | | | 4 |
| | **Slice 5: writing every wolf** | | | | |
| P14 | Mantle batches (17 sessions, §14.3) | P11's freeze, P13 | about 729 mantles, bound as each batch lands | G1-G5 per batch | 68 |
| P15 | Spares, kits and mark banks (3 sessions) | P10 | 88 spares, 16 stage kits, the mark banks | pool health green for 5 forecast years | 12 |
| P16 | The final listen-through | all | a 100-hour listening run; the second blind ratings; review and fix; the ambient Mind path deleted | every hard gate; H1 3.8+ | 8 |
| | **Total** | | | | **257** |

About **159 hours of engine, tools, pilot and checks** and **98 of writing** (P8, P14, P15). The user's own time: the
pilot's packet, the first packet of each dialect, an hour at each slice's end, the two blind ratings, and tuning the
discourse symbols and name pools by ear.

## 19. Risks

1. **The writing is large** (about 24,500 mantle lines and 2,100 shared rules) and could drift in quality or sameness.
   *Against it:* the pilot freezes the format with the user; the town index and uniqueness gates stop neighbours
   echoing each other; U2 measures distinctness; every batch passes G1-G5; the size is the user's choice (Q3).
2. **Rebuilding `converse` and the director breaks what works.** *Against it:* Phase 0's split and Phase 5's topic move
   are behaviour-neutral and proved by golden transcripts before anything changes; old content keeps loading.
3. **Load and pick time with 1,000 mantles** (measured: 22.4 s and 0.49 ms with today's `setCharacter`). *Against it:*
   batch loading and per-owner buckets in Phase 9, benched against B1-B5 before any batch lands.
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
11. **Sims change through voice traits** (honesty chooses thieves; quarter feeds the Market Board). *Against it:*
    placed residents' honesty and quarter never change; newborns' honesty is the user's call (Q4); births stay
    realm-wide unless the user says otherwise (Q5).
12. **The checkpoint grows** (about +2 MB on 1.3 MB). *Against it:* caps, least-recently-used eviction,
    `--no-talk-ledger` for fast economy sims.
13. **Measurement runs that tick are simulations.** *Against it:* frozen runs by default; ticking only with the user's
    go-ahead (Q6).

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

## 21. Open questions for the user

Only the ones that change what gets built.

1. **The window's "Say something else...".** Answered by authored understanding only (§13.5; my recommendation, for goal
   6's "all preprogrammed"), or kept on the NPC Mind, as doc 83's answer 1 allowed, behind `--window-mind`? Either way
   the Mind never performs actions and can no longer make promises from the window.
2. **Where mantles live.** Files in the repository, with the database holding who wears which and the per-NPC overrides
   (my recommendation: reviewed, tested, shipped with the build). Or rows in the world database, as doc 62's decision 3
   put character tables, editable live in Atlas and the DM (the DM would then edit a mantle's text, not only one wolf's
   override).
3. **How much per wolf, and who writes it.** About 40 lines for the ~220 public roles and 24 for the rest (my
   recommendation: about 98 hours of writing), or the richer size (about 0.7 million words, roughly 2.3 times the
   writing), or a leaner one (about 14 lines each, about 55 hours)? Written by Claude agents in sessions (my
   recommendation, no paid calls), or drafted by the paid story model through `tools/voice_tables.py` and reviewed?
4. **Newborns' honesty.** Should a newborn take its mantle's honesty (some become possible thieves at 16, more than 2
   real years away; my recommendation), or always start "plain"? It changes long simulations, not year-long ones.
5. **Births toward the dead's region.** Keep births realm-wide and let the pools absorb the imbalance (my
   recommendation), or prefer a couple in the dead wolf's dialect region? Steering keeps pools balanced but changes the
   economy sims.
6. **Listening runs that tick the world.** May agents record fixtures from one ticking 25-game-day run in Phase 0 and run
   ticking 100-hour listens at slice ends (a world simulation)? Or only frozen runs, which tick no economy?

## 22. What I could not verify

- **Real server speech.** `Saved/Logs/npc-voices.jsonl` has no words, so every "today" figure rests on the survey's
  harness (random pairs in a cell rather than the director's pick, coarse ticks, no 32-entry cap), and the targets are
  set against it. Phase 0 re-measures with the real director.
- **The cost of building facts per beat** (`talkSpeaker`, `peopleOf`): not profiled. The load and pick timings in §16
  were re-run this session on synthetic rules; real mantle rules have more criteria.
- **The agents' writing rate** (about 320 gated lines an hour) is an estimate drawn from the three designs' own
  estimates, not a measurement.
- **Ridgemere 3's births and deaths.** It has never been simulated and has had none; the pools use the Gompertz
  expectation on today's ages, with a margin for violent deaths, which are unmeasured.
- **Fixture mode**: whether a checkpoint file can be loaded into a scratch world needs a small option that doesn't exist.
- **The examples' world events** (the camp beliefs and their sources, the ledger note, Fern's warrant, Thistle's till,
  the soap works' load, trust values) are assumed. The residents' facts, households and the Ridge-Fern marriage were
  read.
- **The client line numbers** (`state.ts`) come from the trees survey; I didn't read the client.
- **The router's full intent list**, and how many of the 500 corpus lines today's router already understands.
- **Doc 20's push semantics** for the new columns, and Atlas's UI work for the mantle column.
- **About 44 Coldwater fishing lines**: a word search finds 46 lines in `scenes/regions/coldwater.talk` matching fish,
  net, catch or boat (a few may be false matches, and some may be fine with a `self.job` gate); Phase 1 reviews each.
