# 62. Talk: one dialogue system, chosen by what's true, spoken in each person's own words

Draft, 2026-10-13. It replaces doc 30 phase 6's scene engine (`Core/RatwScenes.*`) and doc 28's exchange library
outright, and later the router's written answers. All 2,302 scenes are converted to the new format. **Phase 1 is built
(2026-10-13):** see "Phase 1 as built" below for what it is and where it differs from this draft. The rest is the
plan.

## Decisions (the user, 2026-10-13)

1. **Convert every scene** to the new format. `.scene` files, `Core/RatwScenes.*` and `Data/Voice/library.json`
   go once the conversion is shown to say the same things (phase 1).
2. **The most specific rule usually wins, with some chance:** see "Choosing, with some chance" in §2.
3. **Character tables live in the world database**, edited in Atlas (DEV) and the Dungeon Master (live): §5.
4. **Traits from the personality's adjectives, and the model for the rest** (2026-10-13): see "Character, as settled".
5. **Quirks become shared lines:** a `quirk` fact and a few lines for each of the realm's quirks.
6. **A character table for every resident**, drafted by the small model and reviewed.
7. **The dialects move to the database** too (open question 2), so the DM can tune a town's speech live.
8. **Residents know their people** (2026-10-13): family recorded and fed to the society, neighbours, those they deal
   with, and important people not in the game yet. See "Relations, as settled".
10. **A written personality for every resident** (2026-10-13), by the model, from where they're from, their age,
   family, standing (a rich House, a farmer, Ridgemere's Heights or the Sump) and the rest; used for their own words
   (their character table) and for the model whenever it voices them. See "Personalities, as settled".
11. **The world stays about the size it was made** (2026-10-13): the old die and children are born to take their place,
    and children grow up. See "Phase 6 as built".
9. **Residents know players too** (2026-10-13): introductions, names, and their disposition toward each player, as
   facts any line may test; disposition left open-ended for a later analysis of conversations (perhaps a model), with
   the framework for it now. See "Players, as settled".

## The ask

> Redesign dialect and dialogue. [...] Left 4 Dead through Hades and Emily Short [...] adopt those models for
> dialogue, and then adapt dialect to it. Tracery is a great one. And I want to be able to expand the uniqueness per
> character [...] we only have a finite amount of NPCs. I want to develop dialogue tables for each, and then for each
> individual NPC, but we'll get there in a second. Let's redesign our dialogue system first with all of that in mind.

Earlier the same day: NPCs talking to each other, less reliance on a model, seven regions each with a slightly
different dialect (Ridgemere the most ordinary), and lines that depend on many factors: rich or poor, day or night,
injuries, combat, excited, young or old, working or off work, Restday, Marketday, holidays, having just spoken with a
player.

## The models we're adopting

| From | The idea | What we take |
|---|---|---|
| **Valve**, Left 4 Dead 2 (Elan Ruskin, GDC 2012, *"AI-driven Dynamic Dialog through Fuzzy Pattern Matching"*) | The game asks a **query**: a *concept* ("I'm greeting someone") plus every **fact** about the speaker, the listener and the world. Every line is a **rule** with criteria. **The rule matching the most criteria wins.** Rules can **remember** facts and **hand a follow-up concept** to someone else, which is how conversations chain. | The whole core: facts, rules, scoring, memory, follow-ups. |
| **Supergiant**, Hades | Thousands of lines, each with requirements and a **priority**; eligible lines are played **once**, or once in a while; the big pool stays fresh because the game remembers what's been said. | Priorities, play-once and cooldowns, and "not heard yet" (we already have that for scenes). |
| **Emily Short**, *Beyond Branching* (2016); Failbetter's **storylets** | **Salience:** the most fitting content comes up by itself. **Storylets:** a small authored piece with prerequisites ("qualities") and effects on those qualities. | Our written scenes become storylets: whole, coherent conversations gated by facts that compete in the same pool as single lines. |
| **Kate Compton**, Tracery | A line is a template: `#greeting#, #name#!` expands through a **grammar** of symbols, each with alternatives, recursively. | All wording goes through grammars, and **dialect and personal voice are layers of grammar**. |

What we avoid is **Oblivion's Radiant chatter**: loose fragments mixed and matched with no shared topic. Every
conversation here stays on one topic, chained through concepts that belong to it.

## The shape: three layers

    WHAT IS TRUE          WHAT TO SAY                   HOW IT'S WORDED
    facts (the query) ->  rules (best match wins)   ->  grammar (layers: the person -> their dialect -> plain)
                              |
                              +-> effects: remember a fact; hand the next concept to the other speaker

1. **Facts** answer "what's true right now" about the speaker, the one spoken to, the two of them, the place, the
   world, and what has been remembered.
2. **Rules** answer "what should be said" for a **concept** (greet, complain about prices, answer a greeting, a stall
   cry). The most specific rule that holds wins. A rule written for one named resident is the most specific of all,
   which is how each character becomes unique.
3. **Grammar** answers "in what words". A rule's text is a template, and its symbols are filled from the speaker's
   stack of grammars: their own habits, then their quarter, then their home region's dialect, then plain speech.

These three layers are independent. One writer can add a rule ("the poor answer a price complaint bitterly")
without touching dialect. Another can write all of Mirebridge's speech without touching a single rule.

## 1. Facts

A query is one flat set of facts, filled when someone is about to speak. Speaking is rare in machine terms (an
exchange every 60–180 s a place, a bark every 25 s), so computing the facts fresh each time costs nothing. Nothing new
needs storing except `from`, traits and memory.

Keys are namespaced by whose they are. In a conversation, **`self` is whoever is speaking this line** and `other` is
whoever they're speaking to. This flips each turn, so one rule serves either speaker.

| Namespace | What | Examples |
|---|---|---|
| `self.` / `other.` | the speaker, the one spoken to | `id`, `sex`, `stage`, `age`, `job`, `role`, `from`, `accent`, `class`, `wealth`, `injured`, `tired`, `drunk`, `mourning`, `working`, `shift`, `fought`, `robbed`, `saw_player`, `trait.temper`... |
| `pair.` | the two of them | `band` (strangers... family), `kin`, `boss`, `richer`, `same_from`, `last_talk` |
| `here.` | the place and moment | `region`, `place` (market, tavern...), `time`, `season`, `weather`, `day` (plain, marketday, restday, festival), `crowd`, `player_near` |
| `world.` | what's been happening | the existing news topics as facts: `prices.herbs=up`, `bandits=near`, `crime=recent`, `festival=greening` |
| `topic.` | the subject in hand | today's topic tags: `item=herbs`, `dir=up`, `kind=raided`, `known=no` |
| `mem.` | what's remembered (§4) | `self.mem.complained_rain`, `pair.mem.talked_money` |
| `conv.` | this conversation so far | `beat` (1, 2...), marks set by earlier lines (`conv.worried`), the alternative chosen |

**The list of factors** (yesterday's table, in these names) is in the appendix, with which ones the world already
tracks. Every key and its allowed values are in a schema, `Data/Talk/facts.json`, so a misspelt fact in a rule fails
the load, as doc 30's checks do now. Facts are strings or numbers. Numbers can be compared (`self.age>=60`,
`self.wealth_days<7`), and that's how Valve wrote "health below a third".

## 2. Rules

A rule is a concept, criteria, what to say, and what follows. Rules are written in plain text files
(`Data/Talk/**/*.talk`), like the scene files:

    rule prices.complain.poor
    when concept=prices.complain self.wealth=poor|destitute
    say #oath#. I'll not eat this week at #topic.price#. | #topic.price#? For #topic.item#? #robbery#.
    then other: prices.answer_complaint

    rule prices.answer.merchant
    when concept=prices.answer_complaint self.role=merchant
    say Roads are bad, #other.address#. I pay more, you pay more.
    remember self: defended_prices 1d

    rule prices.answer
    when concept=prices.answer_complaint
    say #agree#. #grumble#. | It's the caravans, they say.

- **Scoring (Valve):** a rule's score is the number of criteria it tests, plus its `priority` (default 0). A
  general rule is always there as a fallback, and a specific rule takes over wherever it fits. Writers add specific
  rules anywhere, in any file, without reordering or rewiring anything.
- **A named person is the most specific of all:** `self.id=...` counts as 5 criteria, so a line written for one
  resident beats everything general about people like them.
- **Priority tiers (Hades):** `priority 10` for what must be said if it can be (a death in the family, today's
  festival); negative for filler that only speaks when nothing else does.
- **Once and cooldowns (Hades):** `once` (ever, per speaker), `once per day`, `once per other` (per pair),
  `cooldown 2h`. A rule that has spoken is passed over until it's allowed again, and the next best speaks.
- **Not heard yet:** doc 30's preference stays. Among the best rules, those the listening players haven't heard
  speak first.
- **Several texts per rule** (`|`): never the same one twice running from the same speaker (Valve's "norepeat").

### Choosing, with some chance (decision 2)

Strict Valve always says the most specific line, so a resident with one line written for being poor would say it
every time they're poor. Instead, the rules that hold are grouped into **tiers by score**, and a **tier** is chosen
first, then a rule within it:

1. Of the rules that hold (and aren't on cooldown or used up), group them by score, best first.
2. Choose the tier: **the best tier 70%, the next 20%, any lower one 10%** (split evenly among them). If only one
   tier exists it takes everything; if two, 80/20. The tier is chosen first, so twenty general lines can't drown out
   one specific one by numbers alone.
3. Within the tier: prefer rules the listening players haven't heard (doc 30's rule), then choose by each rule's
   `weight` (default 1).
4. **Exceptions, strict as Valve:** a rule with `priority 10` or more (what must be said: a death, today's festival)
   always wins when it holds; and **a concept in the middle of a scene** (beats 2 onwards) takes its best tier, so a
   scene's branches stay coherent.

The three numbers live in `Data/Talk/settings.json`. `talk_check` shows which tier was taken and why.

### Conversations: chains and storylets

There are two ways to write a conversation, and both end up as rules:

- **Chains (Valve):** a rule's `then other: <concept>` hands the next beat to the listener, who answers with *their*
  best rule for that concept. Every reply is chosen by the replier's own facts. That's the "a poor man answers one
  way, a rich man another" from the ask, at every step. A chain ends when a rule has no `then`, or after 8 beats.
  Chains are good for **families of exchanges**: greet → answer the greeting → small talk; complain → answer the
  complaint.
- **Storylets (Short):** a whole scene written in one place: a `scene` block in a `.talk` file, laid out as
  doc 30's scenes were (turns of `a` and `b`, tagged variants, `?` optional turns), but in the new names: criteria are
  facts (`b[wealth=poor]`, and the old bare words `b[old]`, `a[friends]` still work), and words are grammar
  (`#b.name#`, `#topic.price#`, `#greet#`). In a scene, `a.` and `b.` name the two speakers whoever is speaking, and
  `self.`/`other.` work too. A scene competes with rules for its concept (`talk.prices`) as one candidate, scored by
  its `when`, and is checked whole before it's chosen (every turn that must be spoken can be). Inside it, a turn
  speaks its first line that holds, in the order written, and parallel `|` alternatives answer each other, exactly
  as doc 30's did. (As built: the draft had each later turn become a concept of its own; keeping a scene whole was
  simpler and is what made the conversion provably word for word.)
- **The conversion (decision 1):** `tools/scenes_to_talk.py` rewrote every `.scene` file and the 23-entry exchange
  library once into `Data/Talk/` (`{a}` → `#a.name#`, `{price}` → `#topic.price#`, `when region=` →
  `here.region=`, tags into criteria, `groups.scene` into `Data/Talk/groups.talk`). Before the old engine went,
  a test rendered every old scene through both engines for a spread of situations with the same seeds, and required
  **the same lines, word for word**. Done 2026-10-13: see "Phase 1 as built".
- **Marks** (branches inside a scene): a variant can set a mark when it's spoken (`#worried`), and later lines can
  require it (`a@worried :`). That turns a scene into a small tree written in one place:

      scene work_how_is_it
      when topic=work band=acquaintances|friends
      a : How's the work, #other.name#?
      b[business=failing] #worried : Slow. Too slow. I don't know how long we'll last.
      b[wealth=rich] #proud : Couldn't be better. We're turning folk away.
      b : Steady. | Same as ever.
      a@worried[friends] : If it comes to it, you come to me first. You hear?
      a@proud[poor] : Must be nice.
      a : Good. That's good.

### Where rules are used

One engine, asked by every part of the game that speaks a written line:

| Speaker | Concept(s) | Today |
|---|---|---|
| Two residents talking | `talk.<topic>`, then the chain or scene | the scene library (doc 30) |
| A resident calling out | `bark.<occasion>`: stall cry, the watch, weather, children | the bark scenes |
| **A resident reacting** (new) | `react.<event>`: sees a fight, is struck, rain starts, the bell, passes a friend, sees a theft, a player walks into their shop | nothing: new, cheap, and the thing that makes a place feel alive |
| A resident answering a player (canned) | `player.<intent>`: greet, thanks, price, hours, where is... | the router's lines (doc 28); the router keeps recognising what the player said, and rules choose the words (phase 5) |

The live model (player conversations that aren't canned) keeps working as now. It also gets a short **voice card**:
the speaker's dialect words and personal habits from the grammar (§3), so model-written lines sound like the same
person.

## 3. Grammar: the words, and dialect

Every text a rule says is a **Tracery template**. `#symbol#` is filled from the speaker's grammar stack, recursively
(at most 6 deep, and at most 160 characters spoken, checked at load against the longest expansion).

Grammar files (`Data/Talk/grammar/*.grammar`):

    grammar plain
    greet         = Good day | Hello | Morning
    greet[here.time=dusk|night] = Evening | Good evening
    agree         = Aye | True enough | That it is
    very          = very | right | terribly
    robbery       = Robbery, plain and simple | Daylight robbery | A crime, that is
    oath!         = Gods | Heavens | Blood and bread | Well, I never
    address       = friend | neighbour
    address[other.stage=young] = little one | lad^m | lass^f

    grammar frosthollow
    greet         = Warm hearth | Hearth to you
    greet.broad   = 'Arth
    very          = deep
    agree         = Sound as rock | Aye, sound
    worthless     = dead rock
    oath!         = Black seam | Cold iron | Lamp and pick
    tag           = , eh

- **Layers.** A speaker's stack is: **their own grammar** (a named person, §5) → **their quarter or household**
  (Ridgemere's Heights, Wharf, slums; later) → **their home region's dialect** → **plain**. Each symbol is taken from
  the first layer that defines it. A layer can **add** to the layer below (`greet += ...`) instead of replacing it.
- **Every symbol must exist in `plain`.** The load refuses a symbol a rule uses that plain doesn't define, so any
  dialect can leave anything out and fall back to ordinary speech.
- **Conditional entries** (`greet[here.time=dusk|night]`) use the same facts as rules. The most specific entry that
  holds wins, exactly as rules do.
- **Accent strength** (`self.accent`: none, light, broad): with `none` the dialect layer is skipped, with `light`
  each symbol takes the dialect's word half the time, and with `broad` it always does and may use `.broad`
  entries.
- **Habits (`!`): a symbol marked `!` is chosen once per person, for good**, seeded by their id. `oath!` gives every
  Frosthollow miner *their own* oath from the region's pool, and they always use it. `address!` per pair gives each
  person a fixed name they call each friend ("duck", "pet"). That makes 1,075 residents distinct from each other
  before anyone writes a line for one of them.
- **Facts as symbols:** `#other.name#`, `#topic.price#`, `#here.town#`, `#here.festival#`. (Doc 30's `{a}`, `{b}`,
  `{price}`... were converted to these once; the engine doesn't read braces.)
- **Modifiers (Tracery's):** `.capitalize`, `.a` (a/an), `.s` (plural), and pronouns of the person spoken of
  (`#other.they#`, `.them`, `.their`) from their sex. `^m` / `^f` mark an alternative for one sex only. Doc 30's
  sentence-opening capitals and doubled "the" are handled here too.
- **Binding (Tracery's `[x:#y#]`):** a symbol chosen once and kept for the rest of the conversation, so "the cheese"
  stays the cheese for every beat.

### The seven dialects (placeholders to change)

Ridgemere is plain. About 40 symbols each to start: greetings, farewells, agree, refuse, surprise, annoyance, very, a
little, good, bad, worthless, luck, child, friend, money, cold, hot, rain, tired, drunk, nonsense, a tag question,
oaths.

| Region | Character | Some words |
|---|---|---|
| **Ridgemere** (plain) | the realm's common speech | Good day, Farewell, very, fine, nonsense |
| **Frosthollow** | mining and snow; short, dry, dark humour | Warm hearth, Keep your lamp lit, deep (very), dead rock (worthless), a good seam (luck) |
| **Coldwater** | the lake's fishers; calm | Still water (hello), Fair catch (good), it'll float (it'll do), muddied (confused), ", eh?" |
| **Oakhaven** | the Warmwood; slow, rooted, warm | Shade on you (hello), sapling (child), rooted (stubborn), windthrown (ruined), good bark (honest) |
| **Mirebridge** | the fens; wry, damp, suspicious | Dry feet (hello and goodbye), boggy (dubious), fen-sure (certain), mizzle, ", no?" |
| **Larkfield** | the downs' market town; quick, a trader's | Fair price to you (hello), Sold (agreed), a lark's worth (a little), short-weighed (cheated) |
| **Southmeadow** | grain and orchards; cheerful | Good harvest (hello), ripe (good), chaff (nonsense), a windfall (luck), my apple (my dear) |

**A resident speaks the dialect of where they grew up** (`self.from`), wherever they stand. Worldgen gives about 15%
of each place's people another home (Ridgemere's wharf and estates more), so the city sounds mixed. A topic,
`talk.accent`, lets strangers notice each other's speech ("Fen folk, aren't you? I can hear it."), so the dialects
are noticed rather than just being texture.

## 4. Memory

Rules write short-lived facts (`remember self: complained_rain 1d`, `remember pair: talked_money 7d`, `remember
town: heard_wedding 3d`), and later rules read them (`self.mem.complained_rain`). Memory is used for:

- not repeating a complaint from the same mouth, even to different people (Valve's main use);
- follow-ups: "Did the business pick up?" needs `pair.mem.talked_money`, and b's `business` changed since;
- once-ever lines and cooldowns (§2), which are the same store;
- qualities in Short's sense: what a person has come to feel about something, written by one exchange and read by
  the next.

Memory is kept in the save beside `scenesHeard`, capped per resident (32 entries, oldest dropped) and per pair, and
expired on read.

## 5. Making each character unique (the next step, which this is built for)

Uniqueness comes in three tiers, each cheaper than the one below it:

1. **Kinds of people** (rules and grammar keyed on facts): region, class, job, age, wealth. That's most of the
   library.
2. **Traits:** a few structured axes per resident, held as facts (`self.trait.temper=short`, `talk=chatty|quiet`,
   `humour`, `piety`, `honesty`, `warmth`), worked out **once** from today's free-text `personality` and stored with
   the resident. Rules keyed on traits give a bad-tempered smith and a cheerful one different lines without anyone
   writing for either of them. Habits (`!` symbols) add their own oath, pet names and stock phrases.
3. **The person themselves:** a **character table** per resident, holding their own grammar layer (catchphrases,
   their name for their spouse, the word they always misuse) and their own rules (`self.id=...`, which outrank
   everything general). It can be as small as one line.

### Where it's kept: the world database (decision 3)

The **shared library** (generic rules, scenes, the plain grammar and the seven dialects) stays in files under
`Data/Talk/`, reviewed and versioned like the rest of the game's data. **Everything about one person lives with the
person**, in the world database:

- **`live.npc_voice`** (migration 0050), one row per NPC (`world_id`, `npc_id` → `live.npcs`):
  - `home_region` (`from`), `accent` (none, light, broad);
  - `traits` (jsonb: the §5 axes);
  - `grammar` and `rules`: text in the same `.grammar` and `.talk` syntax as the files, so there's one parser and one
    set of checks. Rules in a character table get `self.id=<this npc>` added automatically, so they can't speak for
    anyone else.
- **It's live data, so it follows the live-data rules (doc 20):**
  - **Atlas** edits it in DEV, in the People workspace, as a keyed edit (`voice:<npc id>`) like `person:`.
  - **Push to live** only adds rows for NPCs that are new to PROD. Once an NPC exists in PROD, PROD owns its voice
    too.
  - **The Dungeon Master** edits it on the live world. The DM overrides Atlas for NPCs, as it does now.
  - **Copy NPC state from live** brings PROD's voices back to DEV.
- **Checked before it's saved:** Atlas and the DM send the text to the game's checker (`talk_check --validate`, the
  same loader the server uses). A table that doesn't load (an unknown fact or symbol, a line too long) is refused
  with the reasons, so a bad edit never reaches the server. The editor shows the problems beside the text.
- **Live reload:** the game server reads every voice at start, and reloads one NPC's when its row changes (a
  `NOTIFY ratw_voice` on save). A DM's edit is heard on the next line that NPC speaks.
- **Worldgen** fills `home_region` and `accent` for every resident (phase 3), and the traits from personality
  (phase 6). Those that already exist are kept identical when the world is regenerated.

How we write the tier-2 and tier-3 tables for a finite cast is the next conversation. This design only makes sure
there's a place for them and that they win when they should.

## 6. Tools

- **`talk_check`** (a build target, like `voice_check`): name two residents and a moment, and it prints the facts, every
  rule that held with its score, which one won and why, and the grammar expansion step by step. Valve's writers
  lived in a tool like this. It's the only way to debug "why did she say that".
- **`tools/talk_report.py`** (replacing `scene_report.py`): rules by concept, coverage of each fact (how many rules
  test `wealth=poor`, `injured`...), symbols per dialect against plain, characters with tables, and the thin spots to
  write next.
- **The draft writer** (doc 28's `tools/ambient_library.py`, retargeted): the small model drafts rules and grammar
  entries in bulk, each checked by the loader and read over before keeping. It's paid, so it only runs when asked.
- **The Dungeon Master** (later): a resident's facts, traits, dialect and memory, with "what would they say now"
  through `talk_check`.

## 7. Speed

Rules are bucketed by concept, so a query reads only the rules for its concept and stops at a rule's first failing
criterion. Facts are worked out only for the two speakers. **Measured (phase 1):** a pick for the busiest concept
(`talk.smalltalk`, 511 scenes, each that fits rendered in full) takes about 0.18 ms; talk happens only where players
are, an exchange every 5 s at most and a bark look every 6 s, so it's far inside a 50 ms tick. Interning fact keys
wasn't needed. (`world_check` runs the World alone, without the game's talk, so it isn't the gate for this.)

## Phase 1 as built (2026-10-13)

**The engine:** `Core/RatwTalk.{h,cpp}` (`ratw::talk`). **The library:** `Data/Talk/`:

| File | What |
|---|---|
| `facts.json` | Every fact, and the values it may take (a list), or `"text"` (words to say; testable with any value) or `"number"` (comparable). A misspelt fact or value refuses to load. |
| `settings.json` | The tier odds (0.7 / 0.2 / 0.1), the strict priority (10), what `self.id=` counts (5), the concepts there are, which may repeat (`talk.weather`, `bark`), and which have one speaker (`bark`). |
| `grammar/plain.grammar` | The plain layer: seven symbols to start (greet, farewell, agree, very, robbery, oath!, address), placeholders until phases 3 and 7. |
| `groups.talk` | The region groups (`north`, `fens`, `countryside`...). |
| `scenes/**/*.talk` | The 2,302 converted scenes, in the same files and order as before, IDs kept (what players have heard is remembered by ID). |
| `library.talk` | Doc 28's 23 exchanges as scenes `lib_<kind>_<n>`, priority -1. |

**The format, as built** (the header of `Core/RatwTalk.h` has it in full):

- `rule <id>` with `when concept=...` and `say a | b`; `scene <id>` with `when topic=X` (the concept `talk.X`) or
  `when concept=bark`, then `a : ...` / `b[tags] : ...` lines. Both take `weight` (0 to 20) and `priority` (-10 to
  100).
- Criteria: `key=a|b`, `key!=a|b`, and for numbers `<`, `<=`, `>`, `>=`. A key without its namespace is the speaker's
  if a person has it (`stage=young`: in a scene's `when`, the opener; in a line's tags, that line's speaker), else
  the one namespace that has it (`place` is `here.place`). Bare words as doc 30 had them (`b[old]`, `a[friends]`).
- **Priority below 0 is filler:** it speaks only when nothing of priority 0 or more holds. (That is how the library's
  exchanges stay a fallback, as they were.)
- Grammar: `^m`/`^f` are for one sex **of the one spoken to** (of the speaker, speaking to no one). Habits (`!`) are
  per person; per pair (a name for each friend) waits for phase 3. A binding (`[x:#y#]`) must name a symbol plain has.
  The 160-character limit counts the longest expansion of the words and symbols, not the facts filled in.
- Facts named in phase 1 (doc 30's tags and blanks, renamed): `here.region, place, time, season, day (work, market,
  rest, festival), weather, town, place_name, weekday, festival`; `pair.band`; `topic.item, dir, kind, when, known,
  has_nickname` and the words `topic.subject, claim, news, what` (was `{item}`), `price, where` (was `{other}`),
  `victim, deed, nickname, who, remark` (the library's `{day}`); a person's `id, name, sex, stage, age, job, role,
  trade` (their job, unless none: was `{a_job}`). The world still tells the moment in doc 30's words
  (`World::sceneMoment`, the topics' tags and blanks); `Game::talkQuery` reads them as facts. Phase 2 renames them at
  the source.

**The game:** the ambient director and barks ask `Game::talkPick` (one place for "not heard yet", what was said here
lately, and remembering what players heard). When the model is asked and fails, the talk library speaks (before, only
the 23 library exchanges did). The server takes `--talk-data DIR` (default: `Talk` beside `--voice-data`;
`tools/server.sh` always passes it, so `RATW_VOICE=off` no longer silences residents' talk). The ledger's routes are
unchanged (`scene`, `library`, `written`, `model`).

**The conversion gate:** 3,683,200 renderings of the 2,302 old scenes (400 moments each across every region, place,
hour, band, season, day, weather and kind of speaker, four seeds; every scene spoken at least once), and the 23
library exchanges for every band. All the same, word for word, except five, as intended:

- `ev_muse_purpose` (`when stage=adult|old`) and `tr_any_old_hurts` (`b[stage=young]`): a person's fact without
  `a.`/`b.` was read as the moment's and was never true, so the scene, and the line, never played. Now they do.
- `lib_news_2`, `_3`, `_4`: the library didn't capitalise news that opens a sentence ("my sister wed."); the scenes
  always did, and now these do too.

The gate test and the converter went with the old engine (they need it); both are in this phase's history.

**Tools:** `talk_check` (a build target): a moment from facts given as `key=value`, or two residents of DEV, PROD or an
export read as a scratch server reads them (`--world dev A_ID B_ID talk.prices topic.item=herbs`), with every rule that
held, its score and tier, why the choice, the grammar's steps and the lines; `--many N` counts N picks (the odds, by
ear); `--validate FILE...` checks `.talk` and `.grammar` files as Atlas and the DM will. `tools/talk_report.py`
replaces `scene_report.py`. `tools/ambient_library.py` (paid) now adds its exchanges to `library.talk`;
`tools/ai_review.py` reads them there; `tools/talk_files.py` reads the library for the tools.

**Found, for phase 2: residents' job categories are often wrong.** `jobCategory` (moved unchanged from the scenes)
looks for word fragments in the job title **and the resident's description**, so "Lark*field*" makes a shopkeeper a
farmer, "m*ale*" a brewer, "st*ore*" a miner. Across DEV's 1,083 residents it finds 100 miners and 52 brewers where
their work labels suggest about 19 and 7. The scenes have always been chosen with these. Fixing it (the job post and
work label only, matched as words) changes which scenes fit, so it is phase 2's first step rather than slipped into
the conversion.

**Not in phase 1, as planned:** `then` chains, `remember` and memory, once and cooldowns, marks, `react.*` (phase 4);
`from`, accents and the dialects (phase 3); `player.*` (phase 5); `live.npc_voice` (phase 6). `talk_check`'s world
mode takes the topic's facts from the command line, as the world's topic picker is private to `World` (phase 2).

## Phase 2 as built (2026-10-13)

**Job categories, fixed.** `talk::jobCategory` reads only the post's title and the work label (a title *is* its
founder's work label), never the description, and as whole words: the work first ("digs ore", "logging"), and only if
that says nothing ("works at", "helping at") the place's name, from its last word ("The Wharf Tannery" is a tannery, not
a wharf). Keeping house is no trade. On DEV: 25 miners (were 100), 6 brewers (52), 119 with no trade (204), mostly
"keeps the house", "sits by the well" and the Heights' gentry. The same categories serve the newcomers (doc 50) and the
fame criers (doc 56), which used the old reading too.

**The facts at their source.** The world tells the moment in talk's names now: `World::talkMoment` (was
`sceneMoment`) writes `here.*`, and a topic's `AmbientTopic::talk` (was `tags` and `blanks`) holds `pair.band` and
`topic.*`; the translation table is gone. The world's topic picker (`World::ambientTopic`) is public, so
`talk_check --world dev A B talk` asks for the pair's topic as the ambient director does.

**The facts added** (`Core/RatwGameTalk.cpp` for people and pairs; `World::talkTown` and `World::pairBand` in
`RatwAmbient.cpp` for the town and the band; all in `facts.json`):

| Fact | Values | From |
|---|---|---|
| `wealth`, `wealth_days` | destitute (<3 days) / poor (<7, the society's line) / comfortable / rich (≥42: placeholder, about the top 6% on DEV) | the household's money and food in days of plain food for all of them (`troubles::foodDays`, as the troubles read it); the household is everyone at the same home |
| `on_dole` | yes/no | the dole's own test (household cash under 7 days' food); who was actually paid isn't kept |
| `owes` | yes/no | a debt in a bond (`owed` < 0), or a rescue loan on their till |
| `business` | thriving / steady / failing / none | their own till against its float: a rescue loan, three days propped up of the last 28, or under half the float is failing; twice the float thriving |
| `hungry`, `tired` | yes/no | hunger ≥55 (when they go for food), fatigue ≥60 |
| `homeless` | yes/no | no home, or a beggar (no one is otherwise without a home) |
| `injured` | none / light / bad | `hurt` (bad ≥50, or down); NPCs have no lasting injuries, and hurt heals within hours |
| `mourning` | no / kin / friend | the society's mourning (5 days for kin, 2 for friends: not 14) |
| `fighting`, `fought`, `robbed` | yes/no | in a battle now; an assault they were in (2 days); a theft from them (3 days) |
| `working`, `shift` | yes/no; before / during / after / day_off / none | their task now; their post's hours (day off: Restday and festivals, but for guards) |
| `on_watch`, `travelling`, `errand` | yes/no; yes/no; none / letter / caravan / goods / order / wedding / funeral | `guardOnDuty`; on the road for their work, moving house, a companion; what their activity says |
| `married`, `widowed`, `parent`, `apprentice`, `master`, `house` | yes/no...; the House's name | spouses, parents, positions; a House's head, its head's kin, or working at its business |
| `tone` | plain / warm / gruff / formal / sly / shy | the router's tones, from their personality |
| `pair.kin`, `pair.elder` | spouses / parent / siblings / family / household / no; a or b (the parent) | spouses and parents |
| `pair.boss`, `pair.richer` | a / b / no; a / b / same | who pays the other's wages (a till's keeper, a household, a House); wealth_days half again more |
| `here.crowd`, `here.player_near` | quiet / busy (3+) / crowd (6+); yes/no | residents in the place; a traveller within 12 tiles |
| `world.herbs_price`, `world.meal_price`, `world.bandits`, `world.crime`, `world.festival`, `world.death`, `world.wedding`, `world.birth` | up/down; near/no; recent/no; today/soon/no; recent/no | the same town news the topics are made of |

**Not tracked by the world, so not facts yet:** `drunk` (drinks leave no state), `faith`, `newcomer` (no arrival
day is kept), `class` (Ridgemere's quarters exist only in worldgen's `buildings.json`), `ill` and `mood`. `mourning`
runs the society's 5 and 2 days, not 14. Each needs a record of its own first; phase 4's memory is the natural home
for the arrival day and the death day.

**Checked:** a game test that every fact the game gives is one `facts.json` knows, with a value it allows (and badly
hurt is `injured=bad`); `talk_check --world dev --run 14 --residents` tallies DEV's 1,083 residents after 14 game
hours (777 at work, 15 guards on watch, tones in all six, 70 rich, 61 businesses failing). Married, parents and
apprentices are all "no" on DEV today: its careers have recorded no marriages or apprenticeships yet, not a fault of
the reading. Building one speaker's facts costs about 0.2 ms.

## Phase 3 as built (2026-10-13)

**Where they're from, and how they sound** (`World::talkOrigin`, in `RatwAmbient.cpp`). Worked out from who each
resident is, not stored, so a resident always has the same (and keeps it when the world is regenerated, as the plan
asked): children grew up where they live; of the grown, about one in seven came from another of the seven regions
(one in four in Ridgemere). Accent: the old who never moved speak broad, movers' fade to light; other locals 20% broad,
65% light, 15% none; movers 10% broad, 50% light, 40% none. All placeholders. On DEV: from Ridgemere 547, Larkfield
107, Southmeadow 99, Frosthollow 91, Mirebridge 82, Oakhaven 80, Coldwater 77; accents light 666, broad 213, none
204. Phase 6's `live.npc_voice` will hold these per resident (so Atlas and the DM can set them); until then they're
derived. Players have no `from` yet (open question 6).

**The engine:** a speaker's dialect is the grammar layer named for their `from`, used as their `accent` says: none
skips it, light takes its word half the time (per symbol), broad always, plus its `name.broad` entries. A speaker with
a layer but no accent fact (tests, a character table) uses it fully. Habits: `name!` once per person, and now
`name!!` once per person **for each one they speak to** (the fond name for each friend). After filling in, a full stop
straight after `?` or `!` is dropped (a tag that asks, ", no?", before the line's stop).

**The words** (`Data/Talk/grammar/`): plain's 27 symbols (greet, farewell, thanks, agree, refuse, surprise,
annoyed, very, little, good, bad, worthless, luck, child, address, pet!!, money, cold, hot, rain, tired, drunk,
nonsense, tag, oath!, robbery, home), and the six dialects, 17 to 20 of them each from the table in section 3 (Ridgemere
is plain). A rule learned in writing them: **each symbol is one kind of word** (cold, tired: words like "cold"; rain,
luck: things; greet, oath: whole sayings), or "it's #very# #cold#" comes out "it's deep nothing new". Plain's header
says which is which. All drafts, for phase 7 with you.

**Heard now:** the converted scenes still say what they always did (no symbols in them), so to hear the dialects:

- `rules/barks.talk`: 11 bark rules on phase 2's facts, in symbols: the cold, the rain, the heat, tired, the poor, a
  failing business, a stall's cry, a greeting for a passing traveller, and hurt, robbed and mourning (priority 2 to 3,
  so they come first when they hold). A broad Frosthollow miner in the snow: "Lamp and pick, it's deep cold."
- `talk.accent`, a new topic: two strangers or acquaintances, one not sounding like the other (`topic.accent_of`,
  `topic.accent_town`), and 4 scenes for it where b answers in their own dialect ("Sinking gods. I said Dry feet.").
  It competes with the other topics at about the weight of everyday talk.
- `pair.same_from` (yes/no).

**Checked:** tests for none/light/broad (light about half), `.broad` only for the broad, `!!` the same for the same
friend and different for others, the tidied tag; every grammar and the new files load; all 67 test suites pass.

## Phase 4 as built (2026-10-13)

**The format** (rules; `remember` and the limits also on scenes):

    rule prices_complain_poor
    when concept=talk.prices self.wealth=poor|destitute topic.dir=up
    say #oath#. I'll not eat this week at #topic.price#.
    then other: prices.answer_complaint      # or then self: (the same speaker goes on)
    remember self: complained_prices 1d      # self, other, pair or town; key[=value]; 30m, 6h, 1d or forever
    once per day                             # or once, once per other, cooldown 2h

- **Chains** (`Library::converse`): after the first pick, each rule's `then` asks the other speaker (or the same) for
  their best rule for that concept, the best tier (so answers stay coherent), up to 8 beats. Each beat sees what the
  earlier ones remembered. Only rules are later beats: a scene is a whole talk. The ambient director, barks and
  reactions all speak through `converse`.
- **Memory:** what's remembered is read back as facts any rule may test: `self.mem.complained_prices` (or
  `!self.mem...`, or `self.mem.x=value`), `other.mem.x`, `pair.mem.talked_money`, `world.mem.x` (the town's). Kept by the
  game (`Game::talkMemory_`) per resident, per pair and per town, at most 32 each (the soonest to go dropped first),
  expired when read, and saved in the checkpoint beside what players have heard (`talkMemory`). A pair's memory holds
  while one of them is away (a player logged out).
- **Once and cooldowns** are the same store: `once` (ever), `once per day`, `cooldown` (the speaker's), `once per
  other` (the pair's).
- **Marks:** a scene line `b[business=failing] #worried : ...` marks the talk; a later `a@worried[friends] : ...`
  needs the mark. The draft's `conv.*` facts weren't needed for this.

**Reactions** (`Game::reactions`, every 2 s where players are; `react.*` concepts; `Game::react` is public, for the
DM's tools and tests):

| Concept | When | Who speaks |
|---|---|---|
| `react.fight` | a fight in the place (every 45 s while it lasts) | the nearest resident not in it |
| `react.struck` | a resident in a fight is hurt more (at most every 20 s) | they do |
| `react.theft` | a theft just now | the nearest resident, not the thief |
| `react.rain` | rain or a storm begins outdoors | a resident near a player |
| `react.customer` | a player walks into the room where someone keeps shop (a merchant's work place, or a post's) | the keeper, to the player |
| `react.friend` | two friends within 3 tiles (once in 30 minutes a pair) | a greeting, then the other's answer (a chain) |

The bell isn't wired: the world has no bell to hear yet.

**The writing** (drafts for phase 7): `rules/chains.talk`, the doc's price complaint (the poor complain; merchants,
the poor, the rich and anyone answer each as they are; a second complaint the next day reads `self.mem`) and a
friend's greeting with answers for the tired, the mourning and those at work; `rules/reactions.talk`, two to four
rules a reaction, with guards, children and outdoor workers answering their own way and a returning customer greeted
as one (`pair.mem.served`).

**Checked:** tests for chains (who answers, as whom, then self, eight beats at most), memory (what's remembered and
read back, by whom), once, cooldown, once per other, marks, and the refusals; a game test that a keeper greets a
customer, the two remember it, and it survives a restart. On DEV: a poor wolf's complaint answered by a merchant as a
merchant, both memories recorded (the rule comes up about 7 times in 300 price talks there: one of 15 in the second
tier). All 67 test suites pass.

## Phase 5 as built (2026-10-13)

**The router's lines are rules.** `Data/Voice/router.json` keeps its patterns (what was asked) and tones (from a
personality); its 160 lines and the openers are now `Data/Talk/rules/player.talk`, 58 rules, concept `player.<intent>`
(greet, thanks, bye, trade, hours, job, where, wanted, day, repeat, trouble). Each list became a rule: how well they
know the player as `topic.meeting` (stranger, known, returning, famous, famous_look; one rule a meeting, priority 1) and
the tone as `tone` (from phase 2). These concepts always take their best tier (settings.json `best_only`), which keeps
the router's order exactly: a meeting's own lines before any, a tone's own before plain. The game's facts go in as
`topic.answer` (the price, the hours, the way...), `topic.greeting` (their own), `topic.player` (the name they were
given: never `other.name`), `topic.since`, `topic.deed`, `topic.nickname`. `voice::Rules::line` is gone.

- **Openers** are a plain grammar symbol by tone (`opener[self.tone=warm] = Of course! | ...`); `~` in a grammar is
  an alternative that says nothing (the router's empty openers). The game lowers the answer's first letter (not "I"),
  so it runs on after "Well," and takes a capital where it opens a sentence: exactly the router's rule. Spaces a
  symbol that said nothing leaves are tidied.
- **In dialect:** each greeting and farewell list gained a line in the grammar (`#greet#, traveller.`, `#farewell#!
  Come back soon.`, `#greet#, #topic.player#.`): a broad Frosthollow resident greets a stranger "Warm hearth. A new
  face." The rest of the router's words are as they were; phase 7 writes more of them in symbols.
- **Never the same line twice running** to the same player (the router's rule), now by asking again with another seed.

**The voice card** (`talk::Library::voiceCard`; `talk_check` shows it): where they grew up and how strongly it shows,
up to ten of their dialect's words (the most telling first, none that plain says too, `.broad` ones for the broad), and
their habits as chosen ("Their own oath, always: 'Lamp and pick'"), at most 480 characters. It's added to the
personality the Mind is given, for talk with a player and for overheard exchanges (each speaker's), so the model sounds
like the library: no change to the Mind's protocol.

**Checked:** the router's line tests, moved to `talk_tests` (a known wolf greeted by name, a resident's own greeting
for a stranger, the answer opened in the tone, no answer no line, "I" kept, a formal farewell to a known wolf through
the fallbacks), Frosthollow's farewell in dialect, the voice card; all 67 suites and the Mind's own tests pass.

## Character, as settled (2026-10-13)

**What the world has to build on.** On DEV every one of the 1,083 residents has a personality, a backstory (about 70
characters) and a greeting. The personalities come from worldgen's small pools: two adjectives of **26** and one quirk
of **41** ("Gentle and blunt; he hums sea songs under the breath."), 15 outside that pattern. So each quirk is shared by
about 26 residents, and traits can be read exactly from the adjectives.

**Tier 2, traits** (decision 4). Seven axes, each with a middle value for those the adjectives say nothing about:

| Axis | Values |
|---|---|
| `temper` | calm / even / nervy / short |
| `talk` | quiet / plain / chatty |
| `warmth` | cold / reserved / warm |
| `humour` | none / dry / merry |
| `honesty` | frank / plain / guarded / sly |
| `piety` | devout / ordinary |
| `drive` | content / steady / restless / ambitious |

The adjectives, mapped once (**a draft for you to correct**):

| Adjective | Traits | | Adjective | Traits |
|---|---|---|---|---|
| playful | humour merry, talk chatty | | blunt | honesty frank |
| suspicious | honesty guarded, warmth cold | | guarded | honesty guarded, talk quiet |
| tired | talk quiet | | loud | talk chatty |
| devout | piety devout | | proud | temper short, warmth reserved |
| dreamy | talk quiet, drive content | | patient | temper calm |
| wry | humour dry | | easily offended | temper short |
| loyal | warmth warm | | curious | talk chatty |
| anxious | temper nervy | | shrewd | honesty sly |
| unhurried | temper calm, drive content | | warm | warmth warm |
| ambitious | drive ambitious | | generous | warmth warm |
| restless | drive restless | | quietly funny | humour dry, talk quiet |
| meticulous | temper even, talk plain | | honest to a fault | honesty frank |
| gentle | temper calm, warmth warm | | stubborn | temper short |

Where two adjectives set the same axis, the first wins. The 15 outside the pattern, and personalities the DM writes by
hand later, are read by the small model (paid, a handful). Traits are stored in each resident's row, so Atlas and the
DM can change them.

**Between tiers, quirks** (decision 5). A `quirk` fact (worldgen's 41, as short names: `coughs_in_damp`,
`back_to_wall`, `rival_houses`...), read from the personality, and 2 to 4 rules written for each: a bark, an aside in
talk, a reaction where it fits ("*coughs* It's nothing. The damp."). About 150 lines, drafted then reviewed with you,
in `Data/Talk/rules/quirks.talk`. The 26 who share a quirk share its lines, varied by their dialect, tone and traits.

**Tier 3, everyone's own table** (decision 6). Each resident's row holds a small grammar layer and a few rules, drafted
by the small model from their personality, backstory, job, household and dialect, then checked by the loader:

- **their words:** a saying or two of their own (`saying!`), what they call their spouse, their children and their
  friends (overriding `pet!!`), their own oath if it isn't the dialect's;
- **their lines:** at most four rules (`self.id=` added on load): a remark at their work, a greeting for a player they
  know, and one or two about what their backstory says matters to them;
- nothing that repeats their quirk (the quirk lines do that), and nothing longer than 160 characters a line.

The draft is paid: ten residents first (one of each kind: a shopkeeper, a guard, a child, a House head...) for you to
read, then all 1,083 once you say so. Review is by sample and by the loader's checks; any table can be changed in
Atlas or by the DM afterwards.

**The dialects in the database** (decision 7). A `live.dialects` table (one row a grammar: name, text), beside
`live.npc_voice`, under the same live-data rules: Atlas edits DEV's, a push to live adds only dialects PROD doesn't
have, the DM edits PROD's, copying from live brings them back. The files in `Data/Talk/grammar/` become the seed (a
tool loads them into DEV once). **Plain stays in a file:** every rule's symbols must exist in it, so it's the game's own,
reviewed with the rules; Ridgemere, plain today, can have a layer of its own in the table.

**Phase 6, then** (with "Relations, as settled" below): migration 0050 (`live.npc_voice`, `live.dialects`,
`live.npc_relations`, `live.figures`); the server reads them at start and on `NOTIFY ratw_voice`; worldgen and a seeding
tool fill `from`, `accent` (as phase 3 works them out), traits (the table above), `quirk`, families (and the society
from them, measured with a 30 to 60 day run); neighbours and dealings worked out live; the quirk lines; the Atlas and DM
editors with `talk_check --validate`; the players' facts and `Game::regard` with its ledger ("Players, as settled");
then the paid draft of everyone's table, ten first.

## Relations, as settled (2026-10-13)

Residents should know their family (spouse, children, parents), their neighbours, and the people they would deal with:
the shopkeepers they buy from, their priest, their leaders, and important people who aren't in the game yet.

**Family is recorded** (the world doesn't record it today: worldgen makes each household from one house and one
surname, but never says who is whose spouse or child, so the society's spouses and parents fill only as marriages and
births happen in play, and "married" reads "no" for everyone on DEV).

- A `live.npc_relations` table (`npc`, `other`, `kind`: spouse or parent), seeded once by rule from each household:
  the eldest man and woman of age within fifteen years of each other are spouses; those at least sixteen years younger
  than both are their children; one at least eighteen years older than either is that one's parent. Communal homes
  (bunkhouses) are left out. Siblings follow from shared parents. Atlas and the DM correct it.
- **It feeds the society:** its spouses and parents are filled from the table at load, so founding families count
  everywhere: married, a widow's mourning, inheritance, and couples having children. That changes the simulation
  (births, estates, households), so it's measured with a 30 to 60 day run before it's kept.

**Worked out live from the world** (never stored, so it stays true as people move and shops change hands):

- **neighbours:** everyone in the same building (the flats over a shop), then the households whose doors are nearest on
  the same street, up to five households;
- **those they deal with:** for each trade they buy from, the nearest keeper in their town (their baker, their smith);
  their town's priest; their leaders (the town's head, their House's head, the captain of the watch); their employer
  and those they work beside.

**Figures, not in the game yet:** a `live.figures` table: name, title, where they're known (the realm, a region, a
House's people), a line or two about them, and the NPC they become (empty until they arrive). Edited in Atlas and by the
DM; when a figure arrives, residents speak of the real one. I'll draft the first list from the lore docs for you to
correct.

**In talk:** facts and words for all of it: `#self.spouse#`, `#self.child#`, `#self.neighbour#`, `#self.priest#`,
`#self.leader#`, a figure by role where they're known (the realm's ruler, a House's head); pair facts that are now true
(`pair.kin`, from the records) and new ones (`pair.neighbours`, `pair.customer`: one buys from the other, `pair.flock`:
one is the other's priest). The Mind's card gains their circle: family, neighbours, the people they deal with, the
figures they've heard of. Everyone's character table (decision 6) is drafted knowing them, so a resident's own lines
can name their spouse, their baker, their lord.

## Players, as settled (2026-10-13)

**What the game already keeps** (none of it is the talk library's yet):

- **Introductions and names** (doc 32): each resident's list of whom they know by name, which name, and how they
  learned it (introduced, familiar, a tie, founding), with when; whether they'll give their own (`willName`); a name
  owed after an introduction. Saved.
- **Disposition** (the bonds): a resident's affinity, trust, respect (−100 to 100), familiarity and fear (0 to 100)
  toward each player, and theirs back; money owed; the day they last spoke. Moved by what happens (trading, talk, help,
  harm, promises kept and broken, crimes witnessed, fights, gifts, favours, deeds heard of...) and by the model's nudges
  in a conversation (±3 a reply, at most 6 an hour a pair). Familiarity and liking fade when apart, never while a
  player is away. Saved.
- **Memories:** each pair's conversation, consolidated into summaries; the model's notes and promises. Saved.
- **What's said of them:** deeds and nicknames (doc 56), rumours ("is violent", "breaks promises"), a warrant.

**The framework:**

1. **Facts** (phase 6), for any pair, a resident toward the other (player or resident): `pair.named` (knows their
   name) and `pair.named_how`; `pair.known_by` (the other knows theirs); `pair.affinity`, `pair.trust`,
   `pair.respect`, `pair.familiarity`, `pair.fear` (numbers, comparable); `pair.disposition`, one word from them
   (hostile, wary, cool, neutral, warm, fond: the lines between are placeholders); `pair.days_since` (last spoke);
   `pair.owed` (who owes whom); `pair.promise` (an open one, whose); `pair.rumour` (the worst said of the other they
   believe); `pair.tie`; `pair.famous` (a deed of theirs they've heard). A greeting can then be warm to a friend, cold
   to one who broke a promise, wary of one said to be violent, and by name only to one introduced.
2. **One way disposition changes** (phase 6): every change goes through `Game::regard(holder, other, change, source,
   reason)`: events, the model's nudges, talk rules (a rule may `regard other: affinity +1`, small and budgeted), and
   later the analysis. Each source has its clamp and budget, and a short ledger keeps the last changes for each pair
   with their reasons, for the DM to read ("−4 trust: broke a promise to deliver herbs, day 31").
3. **A place for analysis, left open** (later, your call): when a conversation with a player ends (it's consolidated
   today), an optional analyst reads it and proposes changes with reasons: a model, rules, or both. It goes through
   `regard`, so its budget, clamps and ledger are the same as everything else's; until there is one, nothing changes.

## Personalities, as settled (2026-10-13)

Every resident gets a personality written for them by the model, from everything the world knows of them: where they
grew up and their accent, their age and sex, their family (from the relations, decision 8) and household, their
standing (household wealth; their House and their place in it; their trade; in Ridgemere, their quarter: the Heights,
the Wharf, the Commons, the Sump..., from worldgen's `artifacts/realm/buildings.json`), their traits and quirk, their
job, and worldgen's short personality and backstory as the seed. It is used twice:

1. **Their own words.** The same call drafts their character table (decision 6) from the personality: their sayings,
   their names for their kin, their own oath, their few lines. The words follow the person, and the dialect lists of
   each character are built from who they are.
2. **The model's voice.** When the model voices them (a player conversation, an overheard exchange), it's given this
   personality instead of the short one, with the voice card.

It's kept in their `live.npc_voice` row beside worldgen's short personality (which stays, as the seed), editable in
Atlas and by the DM like the rest of the row. The run is paid: ten residents of different kinds first, for you to read,
then all of them when you say. The generator is `tools/voice_tables.py`; its prompt and its inputs are printed with
`--dry-run`, free.

## Phase 6 as built (2026-10-13)

**The tables** (migration 0050, applied to DEV; PROD's is yours to apply: `python3 tools/world_db.py migrate --db prod`.
Until then PROD's server simply finds none and works as before): `live.npc_voice`, `live.dialects`,
`live.npc_relations`, `live.figures`. Migration 0051 adds families to the people manifest (`kin "a" "b"
spouse|parent`), and the editor's exporter writes the same, so DEV's builds and exports, `world_check` and the live
server all have them.

**Seeded in DEV** (`tools/talk_seed.py`, once; rows already there are kept): the six dialects; every resident's
voice row (1,083) with origin and accent as the game works them out, Ridgemere quarter from worldgen's
`buildings.json` (621: Heights 117, Sump 116, Commons 114, Works 70...), traits from the adjective table, quirk (41
kinds); families by the household rule (329 ties, 55 couples, and one-parent households: with no couple, the eldest
is the parent of those sixteen or more years younger). Ridgemere's flats and manors mostly hold unrelated people
(servants, lodgers): they aren't families, and get none.

**The game** reads them at start and on the DM's `voice.sync` (`Game::loadVoices`): a dialect from the database
replaces the file's; a resident's row overrides what's worked out of them (origin, accent, quarter, traits, quirk);
their own words are a grammar layer above their dialect (`p_<id>`), their own lines rules only they can say
(`self.id=` added, their ID before each rule's); the personality written for them is what the model is given.
Families go to the society, which takes them in at each day's turn (married, a child's parents, the founding
children counted toward a couple's three, the last born when the youngest was).

**New facts:** a resident's `quarter`, `quirk`, traits (`temper`, `talk`, `warmth`, `humour`, `honesty`, `piety`,
`drive`: the middle value where none is set), their people by name (`spouse`, `child`, `mother`, `father`,
`neighbour`, `shopkeeper`, `priest`, `leader`); the pair's `neighbours`, `customer`, `flock`, and toward the other
(player or resident) `named`, `named_how`, `known_by`, the five feelings as numbers, `disposition`, `days_since`,
`owed`, `promise`, `rumour`, `famous`; figures by role where they're known (`world.figure_ruler`, `_lord`,
`_captain`, `_high_priest`, `_hero`, `_villain`). Neighbours: the same building, then the nearest doors on the same
street, five households at most (worked out live, the doors' way out cached).

**Regard** (decision 9): `Game::regard`, bounded by source ("model": the old nudge budget, now here; "talk": a rule's
`regard self|other: affinity +1`, 1 a change and 3 an hour; "analysis": 5 and 10 a day; "dm"), with a ledger of each
pair's last twelve changes and their reasons, saved with the checkpoint. `Game::conversationAnalyst` is the empty place
for an analysis of conversations (called as each closes); none runs yet.

**The population** (decision 11, the user 2026-10-13): from seventy a resident may die of old age (a year's chance
4% at seventy, 8% at seventy-five, 15% at eighty, 30% at eighty-five, half from ninety: placeholders), through the
same death as any (mourning, the estate, the post falls vacant), the DM's protected excepted. Births come only while
a town has fewer living residents than it had the first day this ran (its target, saved), no more a day than it's
short, about one a month for each couple who can; newcomers sent for count. Children grow up as before (at sixteen,
work, an apprenticeship or day labour).

**Quirks** (decision 5): `rules/quirks.talk`, 61 rules for the 41 quirks (barks with cooldowns, asides, reactions),
drafts. **Dialect words** kept in personal tables never repeat their dialect's (a personal layer is always used: a
dialect's word there would make a light accent broad).

**Personalities** (decision 10): `tools/voice_tables.py` (story model): ten written as a sample, all ten tables
accepted by the game's check; the drafts are in `artifacts/voices/` for you to read. The full run (1,073 more, paid)
waits for you.

**Editing:** the Dungeon Master (`tools/dungeon_master.py`; NPCs tab): a resident's voice and family, the dialects and
the figures, on PROD or DEV, each checked by the game's own loader (`talk_check --character` / `--dialect`) before it
is saved, and read again by a running server. Push to live adds only new rows (voices and families only for NPCs PROD
will have); copying from live brings PROD's back. **Not yet:** Atlas's own panel (the DM's edits to DEV serve until
then), and the figures themselves: the lore names no ruler or lord beyond the four Houses' heads (in the game) and the
Wardens (off the map), so the table is empty for you to fill.

## Phase 7 as begun (2026-10-13)

**Rules for the main factors,** written in Claude's hand (no paid drafting), 139 rules and about 430 lines, every one
checked by the game's loader and sampled with `talk_check` against DEV's residents:

- `rules/keeping.talk`: **how they're keeping.** Acquaintances and friends ask after each other (`talk.day`,
  `talk.friends`); the other answers as they are today (`keeping.answer`: grieving, hurt, robbed, roofless, hungry,
  destitute or rich, on the dole, in debt, a business thriving or failing, tired, at work or a day off, their spouse
  and child by name, apprentice or master, old, on the road, and by trait and Ridgemere quarter); the first answers
  that in kind (`keeping.sorry`, `keeping.glad`, `keeping.nod`, by warmth, humour, temper, piety, wealth). Or one tells
  the other, unasked, what's on their mind (`talk.smalltalk`, `talk.day`, `talk.work`, `talk.family`). Everyday
  remarks have a two-day cooldown, serious ones half a day, asking after someone a day: without them a married wolf
  spoke of their spouse in most small talk (a rule with one fact outranks the many scenes with none).
- `rules/days.talk`: **the day as each lives it,** barks by shift (before, after, a day off, worked hard), the day's
  kind as it falls on them (working on Restday, at a festival, on market day; the devout on Restday; the poor at a
  feast), the hour (the old at dawn, the street at dusk and night, the nervy in the dark, the tavern), errands (each
  kind), the road and the watch; and how they are (rich, the Heights and the Sump, hungry, roofless, on the dole, in
  debt, lightly hurt, mourning a friend, widowed, fought, fighting, apprentice and master, the ambitious, the
  chatty, the dry).
- `rules/regard.talk`: **how a resident feels about a player,** in greetings, thanks, goodbyes and answers: hostility
  always shows (priority 3; by name when known); wariness, coolness, warmth and fondness join the tone's own lines at
  the same score, so they show often but not always; a long absence (`pair.days_since>=28`); the resident's own grief,
  hurt, or a robbery.

**New words** in the plain grammar: `hungry`, `hurt`, `work`, `bed`. No dialect has them yet, so they fall back to plain.

**The engine:** a rule may test that a speaker has words to say (`self.spouse`, `!self.child`), as it could a memory,
so it names only those who exist.

**Still to do with you:** the dialects' words (the four new ones, and those each dialect lacks: `talk_report.py` lists
them; the database's dialects are what's used, edited in the Dungeon Master), the quirk lines (drafts), and a listen
through these for tone. Found while sampling: the wealth line (rich from 42 days' food) makes the head of House
Greatly only "comfortable", since household food days don't measure a House's fortune.

## Phases

1. **The engine and the conversion.** *(Built 2026-10-13.)* Facts and the schema, rules, tiered choice with chance, grammar with layers
   and modifiers, the `.talk` and `.grammar` formats, strict load checks, and `talk_check`. `scenes_to_talk.py`
   converts every scene and the exchange library. The ambient director and barks switch to the engine. Tests: both
   engines give the same possible lines for every old scene; then `Core/RatwScenes.*`, `Data/Voice/scenes` and
   `Data/Voice/library.json` are removed, and doc 30's scene tests move to the new engine. Tiers choose as set,
   priority 10 is strict, a typo refuses to load.
2. **The facts.** *(Built 2026-10-13.)* Everything in the appendix that the world already has, under its new names.
   First, the job categories (see "Phase 1 as built", found by `talk_check`), and the world's topic picker opened to
   `talk_check`. See "Phase 2 as built".
3. **Dialect.** *(Built 2026-10-13: see "Phase 3 as built".)* `from` and `accent` on residents (worldgen, with a default from the home town for today's world),
   the plain grammar, the six dialects, habits (`!`), and the accent topic.
4. **Chains, memory and reactions.** *(Built 2026-10-13: see "Phase 4 as built".)* `then`, `remember`, once and cooldowns, marks, and the `react.*` concepts wired
   to the events that cause them.
5. *(Built 2026-10-13: see "Phase 5 as built".)* **The router's lines** become `player.*` rules, so canned answers come in dialect and character. The live model
   gets the voice card.
6. *(Built 2026-10-13, but for Atlas's own panel and the full run of personalities: see "Phase 6 as built".)* **Character.** `live.npc_voice` and its migration, the Atlas and DM editors with validation, live reload,
   traits from personality. What goes in the tables was settled 2026-10-13: see "Character, as settled".
7. *(Begun 2026-10-13: the main factors' rules, see "Phase 7 as begun"; the dialects' words and a listen with you remain.)* **Writing.** Grammar by hand (with you). Rules drafted in bulk for the main factors: wealth, working,
   injured, mourning, fought, saw a player, the day's kind, the time (paid; ask first).

## Open questions

(Settled 2026-10-13: convert every scene; the most specific with some chance; character tables in the world
database. See Decisions.)

1. **The tier odds** (70/20/10) are a first guess, to tune by ear.
2. **Dialect grammars in files or the database?** *(Settled 2026-10-13: the database. See "Character, as settled".)*
3. **Where dialect comes from:** recommended birthplace (`from`), with the accent fading for people long settled
   elsewhere. *(Built so in phase 3; the shares are placeholders.)*
4. **How thick should accents be?** Recommended: broad only for the old and the never-travelled. You'll want to hear
   it and tune. *(Phase 3: broad for the old who never moved and a fifth of other locals; see "Phase 3 as built".)*
5. **Ridgemere's quarters as their own layers** (Heights, Wharf, slums): now, or after the seven? Recommended: after.
6. **Player characters' `from`:** if a player picks a home town at creation, NPCs can notice it. That's cheap after
   phase 3.

## Appendix: the factors

"Have" means the world tracks it and only needs reading; "new" means it has to be worked out or added. Several "have"
entries are from a reading of the headers and need confirming in phase 2.

**Who they are:** `sex`, `stage`, `age`, `job`, `role` (have); `from`, `accent`, `away` (new); `class` (the Ridgemere
quarter they live in, or household wealth elsewhere); `tone` (have: the router's six tones); `trait.*` (new, §5);
`faith`, `house` (have where set); `married`, `parent`, `widowed`, `apprentice`, `master` (have: households,
positions); `newcomer` (have: arrivals in the last 28 days).

**Means:** `wealth` destitute/poor/comfortable/rich and `wealth_days` (have: household purse in days of food);
`on_dole`, `owes`, `business` thriving/steady/failing (have: the dole, loans, rent, till takings); `hungry`, `homeless`
(have).

**Body and mood:** `injured` none/light/bad, `tired`, `drunk` (have); `ill` (new: the ailments plan); `mourning` (have:
a death in the household or among friends in the last 14 days); `fought`, `fighting`, `robbed` (have: battle and crime
logs); `mood` glad/low/angry/afraid/excited (new, worked out from the rest and recent events, never stored).

**Doing:** `working`, `shift` before/during/after/day_off, `on_watch`, `travelling`, `errand` (have: hours, schedules,
routes); `saw_player` and `player_kind` friendly/rude/paid/fought (new: a timestamp per resident from the dialogue
log); `player_near` (have).

**The two of them:** `band`, `kin`, `boss` (have); `same_from`, `richer`, `last_talk` (new).

**The moment:** `region`, `place`, `time`, `season`, `weather`, `day` plain/marketday/restday/festival, `festival`,
`crowd` (have, from `sceneMoment` and the director); the town's news as `world.*` facts (have).

**Later:** quests, storylines, the Houses' feuds, wars. When they come they're just more facts, and nothing in the
engine changes.
