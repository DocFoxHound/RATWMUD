# 62. Talk: one dialogue system, chosen by what's true, spoken in each person's own words

Draft, 2026-10-13. It replaces doc 30 phase 6's scene engine (`Core/RatwScenes.*`) and doc 28's exchange library
outright, and later the router's written answers. All 2,302 scenes are converted to the new format. Nothing here is
built yet.

## Decisions (the user, 2026-10-13)

1. **Convert every scene** to the new format. `.scene` files, `Core/RatwScenes.*` and `Data/Voice/library.json`
   go once the conversion is shown to say the same things (phase 1).
2. **The most specific rule usually wins, with some chance:** see "Choosing, with some chance" in §2.
3. **Character tables live in the world database**, edited in Atlas (DEV) and the Dungeon Master (live): §5.

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
  `self.`/`other.` work too. The loader turns each scene into rules: its first turn becomes a rule for the topic
  concept (`talk.prices`) with the scene's `when` as criteria, and each later turn becomes a concept of its own
  (`scene:rm_market_herbs_dear#2`), with its tagged variants as rules. So scenes and chains compete for the same
  moment. The parallel `|` alternatives become a `conv.alt` fact.
- **The conversion (decision 1):** `tools/scenes_to_talk.py` rewrites every `.scene` file and the 23-entry exchange
  library once into `Data/Talk/` (`{a}` → `#a.name#`, `{price}` → `#topic.price#`, `when region=` →
  `here.region=`, tags into criteria, `groups.scene` into `Data/Talk/groups.talk`). Before the old engine goes,
  a test renders every old scene through both engines for a spread of situations (each region, place, time, band,
  speaker kind) and requires **the same set of possible lines**. Anything that differs is listed and fixed.
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
- **Facts as symbols:** `#other.name#`, `#topic.price#`, `#here.town#`, `#world.festival#`. Doc 30's `{a}`, `{b}`,
  `{price}`... are read as these, so old scenes keep working.
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

Rules are bucketed by concept. Fact keys are interned to integers when the library loads, and each rule's criteria
are sorted with the most selective first, so a query reads only the rules for its concept and stops at the first
failure. Tens to hundreds of rules per concept at a few criteria each is microseconds. Facts are worked out only for
the two speakers. Checked against the usual gate (`world_check --players 20`).

## Phases

1. **The engine and the conversion.** Facts and the schema, rules, tiered choice with chance, grammar with layers
   and modifiers, the `.talk` and `.grammar` formats, strict load checks, and `talk_check`. `scenes_to_talk.py`
   converts every scene and the exchange library. The ambient director and barks switch to the engine. Tests: both
   engines give the same possible lines for every old scene; then `Core/RatwScenes.*`, `Data/Voice/scenes` and
   `Data/Voice/library.json` are removed, and doc 30's scene tests move to the new engine. Tiers choose as set,
   priority 10 is strict, a typo refuses to load.
2. **The facts.** Everything in the appendix that the world already has, under its new names.
3. **Dialect.** `from` and `accent` on residents (worldgen, with a default from the home town for today's world),
   the plain grammar, the six dialects, habits (`!`), and the accent topic.
4. **Chains, memory and reactions.** `then`, `remember`, once and cooldowns, marks, and the `react.*` concepts wired
   to the events that cause them.
5. **The router's lines** become `player.*` rules, so canned answers come in dialect and character. The live model
   gets the voice card.
6. **Character.** `live.npc_voice` and its migration, the Atlas and DM editors with validation, live reload,
   traits from personality: the next conversation decides what goes in the tables.
7. **Writing.** Grammar by hand (with you). Rules drafted in bulk for the main factors: wealth, working,
   injured, mourning, fought, saw a player, the day's kind, the time (paid; ask first).

## Open questions

(Settled 2026-10-13: convert every scene; the most specific with some chance; character tables in the world
database. See Decisions.)

1. **The tier odds** (70/20/10) are a first guess, to tune by ear.
2. **Dialect grammars in files or the database?** The design keeps the seven dialects in files (shared, reviewed),
   and only each person's own layer in the database. If you want the DM to tune a whole town's speech live, they
   could move to a `live.dialects` table too.
3. **Where dialect comes from:** recommended birthplace (`from`), with the accent fading for people long settled
   elsewhere.
4. **How thick should accents be?** Recommended: broad only for the old and the never-travelled. You'll want to hear
   it and tune.
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
