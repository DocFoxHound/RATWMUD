# The Story Maker: a guide for admins

How to make quests and whole campaigns for a world of Run Against the World, and how to run them while people play. The
same guide is in the Dungeon Master suite, under **Guide**. Its reference part (every event, condition, action and effect,
with an example) is made from the trigger catalogue, so it always says what the tools accept. The design behind it is
`Docs/Design/85-story-triggers-and-campaign-control.md`; the format in detail is `Docs/story-programs.md`.

**Contents:** [1. What it is](#1-what-it-is) · [2. A small quest in five minutes](#2-a-small-quest-in-five-minutes) ·
[3. How a story is made](#3-how-a-story-is-made) · [4. Building a campaign](#4-building-a-campaign) ·
[5. Flow: if, wait, for each, random](#5-flow-if-wait-for-each-random) · [6. Variables and scopes](#6-variables-and-scopes) ·
[7. Quests and rewards](#7-quests-and-rewards) · [8. The world's facts and watched events](#8-the-worlds-facts-and-watched-events) ·
[9. Rehearsing](#9-rehearsing) · [10. Approving and publishing](#10-approving-and-publishing) ·
[11. The Campaigns console](#11-the-campaigns-console) · [12. Moving a run onto a new version](#12-moving-a-run-onto-a-new-version) ·
[13. Who may do what](#13-who-may-do-what) · [14. What the game undoes when a run ends](#14-what-the-game-undoes-when-a-run-ends) ·
[15. Troubleshooting: Explain, Why not?, What if](#15-troubleshooting-explain-why-not-what-if) · [16. Limits](#16-limits) ·
[Reference](#reference-every-event-condition-action-and-effect)

## 1. What it is

A **world story** is a small program of rules, like a map editor's triggers: *when* something happens in the world (an
**event**), *if* some things are true (**conditions**), *do* these things (**actions**). It is written in **Story Creator**,
rehearsed there against made-up events, approved by a Dungeon Master for one exact version, and then published: the story
runner (`tools/story_runner.py`, a process beside the game) reads the world's event log and plays it, and the game applies
what it asks for (a resident speaks, a door locks, a wolf is given a coil of rope), each with a receipt. **Campaigns** is
the console where a running story is watched and steered.

Three things are always true:

- **A story can do nothing a Dungeon Master could not do by hand.** Every world effect is an order the game checks and may
  refuse, under the same rules as the DM's own actions (the community world's guards, "may act on players' wolves", the
  economy's rules). No effect makes coins from nothing: a coin reward is paid from a named purse.
- **Nothing kills or moves a player's wolf, and nothing changes the calendar or the hour.**
- **It is rehearsed before it runs.** Approval needs every phase reached by a rehearsal that passes, and checks every id it
  names against the world.

## 2. A small quest in five minutes

A small quest is one quest any wolf may pick up from a resident, do and hand back, for a reward. No phases to think about.

1. **Story Creator → New… → A small quest.**
2. **Start from** one of the 100 library events (each one's steps and who gives it) or one of the 24 personal-quest
   templates, or start blank.
3. **Who gives it:** pick the resident (by name; nobody types an id). The first step "Speak with them" becomes a talk with
   that resident.
4. **What to do:** one to six steps, each an event from the picker ("gives rope to Ivo the carter", "enters the mill"), with
   how many times. The journal shows each step's words.
5. **The reward:** a deed told of them, coins **from a purse** (a town's treasury, a business's till, `till:job:<resident>`, a
   Chapter's purse: refused when the purse is short, never minted), an item, or standing with a faction. Approval checks that
   the purse exists in the world as last saved.
6. **The offer:** what the resident says in their dialogue window, and the wolf's words to ask, accept and hand it in.
7. **Make the small quest.** It makes a version 2 story with an open audience (anyone who asks may take it), one phase that
   stays open, the quest, and a rehearsal case (a wolf takes it, does each step, hands it in).
8. **Rehearse all paths.** The case passes; the Problems list is empty, or says exactly what is wrong and **Go** takes you
   there.
9. **Save** the draft, then **Approve** (a Dungeon Master) and **Publish**. **Open in Campaigns** shows it running: who has
   taken it, who has done it, and every order the game answered.

A wolf now sees "Ask about: An axle for the carter" in Ivo's window, takes it on, finds it in the journal with its steps
ticked as they go, and hands it in to Ivo for the reward. The **Quest editor** does the rest when you want it: optional and
hidden steps, steps in any order, fail rules and deadlines, how often it may be taken again, a cap on copies under way, a
notice board offer.

## 3. How a story is made

A **program** has an id and a title, an **audience** (who takes part), **phases**, the phase it opens in, optional
**campaign triggers** (heard in every phase), **regions** (rectangles of tiles in one place), embedded **scenes**, version 2
**variables** and **quests**, and **rehearsal cases**.

- A **phase** is what a wolf reads in the journal now. It has what it sends as it opens (its entry effects), its
  **triggers**, its **exits** (the older, simple trigger: "on this event, go to that phase"), up to 16 **missions** (the
  older quest that lasts one phase), and guest voices. A phase with nothing to wait for is an **ending**.
- A **trigger** is an event, conditions and actions:

```json
{"id": "count-rope", "on": "gift", "match": {"target": "rm111", "item": "rope"}, "audience": "participant", "once": "each",
 "if": {"test": "compare", "left": {"var": "ropes"}, "op": "<", "right": 6},
 "actions": [{"do": "add", "var": "ropes", "value": 1},
             {"do": "effect", "effect": {"kind": "story.tell", "to": "wolf", "player": "$actor", "text": "Ivo coils the rope by the cart."}}]}
```

- `match` filters the event's fields by exact text (`actor`, `target`, `cell`, `item`, `detail`). A gift's item matches its
  base item: a marked or crafted rope counts as rope.
- `audience`: `world` (anyone) or `participant` (only a wolf of the story).
- `once`: `each` (every time), `visit` (once this phase visit), `player` (once per wolf this visit), `campaign` (once in the
  run), `member` (once per wolf in the run).
- Triggers are tried in order, campaign triggers first; a variable one changes is seen by the next. A trigger that enters a
  phase ends the handling of that event.
- **Signals**: `{"do": "emit", "signal": "gate-opens"}` sends a named signal that triggers `"on": "story signal"` hear,
  after the triggers of the event that sent it. Timers (`schedule`, `start timer`) send a signal when due, on the world's
  clock or the real one.

**The workbench** (Story Creator) shows all of it as a tree on the left (campaign triggers, then each phase with its triggers
in folders, its exits, missions and what it sends as it opens; regions, scenes, quests, rehearsal cases, settings), the
selected trigger as sentences in the middle ("When **a wolf of the story** gives **rope** to **Ivo the carter**"), every
slot a picker (residents and wolves by name, a place then a tile on the real map, items by category, the story's own
signals, variables and phases), and on the right the Problems list, search, and the story's signals, variables and regions
with every use and **rename everywhere**. Undo and redo cover the whole draft. Drag a trigger to reorder it; right-click to
duplicate, copy, paste, save as a template. **Source** shows the JSON (also for a draft the forms cannot show).

**Version 1 and version 2.** A version 1 story has counters (whole numbers, made by being used) and the seven first actions.
Version 2 declares its variables and adds flow, typed values, quests, the world's facts and the story's own effects.
**Make it version 2…** turns a version 1 story into one that does the same. New stories are version 2.

## 4. Building a campaign

A campaign is a long story. There are two ways to build one, and they go together.

**Within one story: phases and campaign triggers.** Phases follow each other (`enter`, exits); campaign triggers keep
listening across all of them (a weekly watch that counts every phase); `"lifetime": "campaign"` timers and quests outlive a
phase. Ninety days on the winter ferries is one story of four phases.

**Across stories: chapters.** A campaign of **chapters** is several stories approved together, in order. Each chapter is an
ordinary story; one starts the next with the effect **Start the next chapter** (`story.start`):

```json
{"do": "effect", "effect": {"kind": "story.start", "story": "the-crossing", "audience": "same", "carry": true, "when": "ended"}}
```

- `when`: `ended` (the default) waits until this story has **ended in the game** (finish, or an ending phase), so what it
  held there (locked doors, held residents, strangers, the weather) is let go before the next chapter begins. `now` starts
  it beside this one.
- `audience`: `same` (this story's wolves as they are now), `own` (the next chapter's own audience), or `list` with `list`
  naming a declared list of wolves (`"list": "mustered"`).
- `carry` (yes by default): the story and per-wolf variables the next chapter declares **with the same name, scope and
  type** start with this story's values. World variables are shared anyway.
- Only a story approved as a chapter of a campaign may start another, only a chapter of the same campaign, and never one
  that is already running (the story's timeline says so if it was not started, and why).

Make the campaign: write each chapter as its own draft (rehearse each: a case that reaches the `story.start` can say
`"expectChapters": ["the-crossing"]`), then **Campaigns → New campaign…**: an id, a title, the drafts in order, **Approve
together** (each is rehearsed and checked as its own approval would be, and every `story.start` must name a chapter of this
campaign), then **Publish chapter 1**. The Campaigns list shows each campaign as its chapters (✔ done, ● running,
○ waiting to be started); a chapter's own page shows the strip above its phases and what it will start. Approving the
campaign again under the same id replaces the chapters that have not started; those that have keep their runs (move a
running chapter onto a new version as any run: section 12).

**Hearing other stories.** Every live story of the world that listens is told when another starts or ends:
`"on": "story started"` and `"on": "story ended"`, with the other story's **id** as the target (not its run, so one
chapter can wait for another), its run as the item, and for an ending how it ended (`complete`, `success`, `failure`,
`abandoned`, `stopped`) as the detail. A story ended by an admin's Stop starts no chapter.

**Carrying state.** Three ways, from the lightest: `carry` (the next chapter's own variables start where the last left
off), **world variables** (section 6: kept after a story ends, read by every story that declares them), and the audience
(`same` or a list).

**Campaign-long holds.** A chapter may hold a resident for the **whole campaign**, not just its own run: **Spare a resident
from old age** (`npc.spare`) and **Protect a resident** (`npc.protect`) take `"lasts": "campaign"`. The hold outlives the
chapter that set it and is let go when the campaign ends (the last chapter done, none running). Only a story approved as a
chapter may hold for the campaign; approving it alone is refused. An act stopped early keeps its holds until an admin
presses **Let go of the campaign's holds…** on the campaign's card in the Campaigns list. With `"lasts": "run"` (Spare's
default) the hold goes when the run ends; Protect's default, `ever`, is the mark it always was (until Unprotect). A
story's **Unprotect** of a resident it protected for its run or campaign lets go of that protection only: a House head,
one Atlas protects or one a Dungeon Master protected stays protected.

```json
{"kind": "npc.spare",   "npc": "rm_ingrid_kendrick", "lasts": "campaign"}
{"kind": "npc.protect", "npc": "rm_ingrid_kendrick", "lasts": "campaign"}
```

### Chapters that move on when the wolves have done enough

A chapter need not move on by a date. Give its working phase **Readiness** (`ready`): what must be true before it may move
on, and its one **ready exit**. The runner measures it (at least once a game hour) and shows it in the console as bars.

```json
{"id": "ch1_main", "journal": "…", "effects": [],
 "ready": {
   "all": [
     {"participation": {"list": "ch1_did"}, "of": "active", "percent": 30, "floor": 3, "cap": 150, "label": "Wolves who did the chapter's work"},
     {"minDays": 10, "since": "chapter", "label": "Days in the chapter"}
   ],
   "auto": true, "slowAfterDays": 30, "slowNeedsYou": true
 },
 "exits": [{"on": "ready", "to": "ch1_end"}]}
```

- **The parts** (1 to 8, all needed; `{"any": [2 to 4 parts]}` is one of them): `participation` (a share of the active wolves
  who are in a list, or who meet a test: section 6), `minDays` (game days since the phase or the chapter began; held time
  does not count), `value` with `atLeast` (two numbers of the story or the world), `test` (a condition over the story's own
  state), `missions` (the phase's missions done), `quest` (a story quest done). Each may have a `label` for its bar.
- **Ready stays ready.** Once every part is met, the phase is ready for the visit: a crowd arriving later and lowering the
  share does not take it back.
- **`auto`**: true moves on by itself at once (or at the `then` moment: `{"on": "day begins"}`, `hour`, `season begins`,
  `festival`, or a `dm signal`). False (the default) waits for an admin's **Advance** (section 11), and with
  `autoAfterDays` goes by itself that many days after it became ready, for a world with no admin about.
- **`slowAfterDays`**: not ready after that long, the console says **slow** (with `slowNeedsYou`, as a "needs you" alert:
  "Ready to push").
- The ready exit has no acting wolf: a phase that rewards `$actor` cannot be entered by it. The phase's other exits keep
  working beside it.
- A rehearsal case may check it (`{"check": "ready", "met": true}` among its events) and press Advance (`{"advance": "now"}`,
  `"anyway"` or `"then"`); a case may also give the audience (`"audience": {"active": 120}`) or the active wolves
  (`"activeWolves": [...]`). Rehearsing names a Readiness no case made ready (`uncoveredReady`).

**Recommended for a campaign's chapters:** `activeDays` 18 (three real days), 25 to 35% of the active wolves who did one of
the chapter's tasks (a story list `ch<n>_did`, one `list add` per task), a floor of 2 or 3, `cap` 150, `minDays` the
chapter's shortest sensible length, `slowAfterDays` its soft maximum, and `auto` true with an admin's Advance for the slow
ones (or `auto` false where an admin is always about).

## 5. Flow: if, wait, for each, random

Version 2 actions may hold actions of their own, three deep, 24 in a trigger counting nested ones.

| Action | What it does |
|---|---|
| `if` / `elif` / `else` | Runs the actions under the first condition that holds, or those under Else. Up to four Else ifs. |
| `wait` | The rest of the actions run later (a minute of the world's clock at least, or 5 real seconds), for the same wolf and with the same event. It ends with the phase visit unless it is a campaign trigger's. Never holds up another trigger. |
| `each` | For each wolf of the story, of the acting wolf's party, or each value of a list (500 at most): `$each` is the looped wolf, in effects and in variables. Not nested. |
| `random` | One of 2 to 8 weighted choices, rolled from the run's seed (a rehearsal may pin the rolls). |
| `run` | Run another trigger's actions now, checking its conditions or not. Cycles are refused. |
| `enable` / `disable` | Turn a trigger on or off (a trigger may start off: `"enabled": false`). The console's switch is the same. |
| `stop` | Skip the rest of the list. |
| `note` | A line for the DM in the timeline, nowhere in the world. |
| `finish` | End the run: `success`, `failure` or `abandoned`, with a line. For `$actor` or `$each`: that wolf's own ending. |

```json
{"do": "if", "if": {"test": "count in region", "region": "ford", "op": ">=", "value": 3},
 "then": [{"do": "effect", "effect": {"kind": "door.unlock", "door": "link_ferry_gate_a"}},
          {"do": "wait", "days": 0.25},
          {"do": "finish", "outcome": "success", "text": "The ferry crossed with three wolves aboard."}],
 "else": [{"do": "note", "text": "Not enough wolves at the ford yet."}]}
```

## 6. Variables and scopes

A version 2 story declares every variable it uses: a misspelt name is an error, not a new counter.

```json
"variables": [
  {"id": "ropes",    "type": "number", "scope": "story",  "initial": 0, "show": "Ropes delivered"},
  {"id": "helped",   "type": "switch", "scope": "player"},
  {"id": "mustered", "type": "wolves", "scope": "story"},
  {"id": "finder",   "type": "wolf",   "scope": "story"},
  {"id": "best",     "type": "number", "scope": "world"}
]
```

- **Types:** `number`, `switch` (on or off), `text` (200 characters), references (`wolf`, `resident`, `place`, `region`,
  `item`, `faction`), lists (`numbers`, `texts`, `wolves`, `residents`: 500 values), and `timer` (a moment: "days since").
- **Scopes:** `story` (one value), `player` (one per wolf), `party` (one per party taking part), and `world`.
- **Values** in a Set or a Compare: a constant, `{"var": "ropes"}`, an event field `{"field": "quantity"}`, `{"count":
  "mustered"}`, `{"since": "began"}`, `{"each": true}`, and a little arithmetic: `{"plus": [a, b]}`, `minus`, `min`, `max`,
  `times`, and `div` (floored toward zero; dividing by 0 gives no value, so nothing fires on it).
- **Shown:** a number or switch with `show` appears in each wolf's journal under the story as a tracker; the effect **Show or
  hide a tracker** hides and shows it.
- **Told to residents** (`"talk": true`, a wolf's own number, switch or text; at most 8 a story): the residents that wolf
  talks to in the dialogue window know it as `player.story.<name>` (a number's digits, `yes` or `no`, the text as it is), so
  a line can say "You pledged to Coldwater, didn't you?" with `say[player.story.p_howl=coldwater]`. A text a line could not
  test (capitals, spaces) is kept but not told, with a note in the timeline. Two live stories telling the same name: the
  newest run's value is the one they know (approval warns).
- **World variables** are kept after a story ends and shared by every story that declares one of that name and type (a map
  editor's game cache). **Campaigns → World variables…** lists them, with the stories that read each. There an admin
  **declares** one for the world (a type, a first value, a note), **sets** its value and **removes** it, each with a reason,
  audited (`story.world-var`). A story that declares a world variable the world has declared as another type is refused
  at approval. A live story reads a new value at its next turn.

### Goals that scale

`{"audience": "all"}` is how many wolves the story has, `{"audience": "active"}` how many of them are **active**, and
`{"audience": "online"}` how many are signed in now (unknown until the world says who is online: the story then watches the
world's wolves). Active means seen, signed in or the actor of any event, in the last `activeDays` game days: a program
field, 1 to 120, **18** when not given (three real days at the normal speed, so a wolf who plays every other evening still
counts). The runner keeps who it has seen, and a chapter started by another fills it at once from the world's log, so it
does not open with no active wolves.

A goal that grows with the crowd: `{"max": [12, {"times": [{"audience": "active"}, 2]}]}` is twelve, or two for every
active wolf when there are more than six.

### Counting who took part

`{"percent": {"list": "ch1_did"}, "of": "active"}` is the share (0 to 100) of the active wolves who are in the list
`ch1_did`; `{"among": …}` is how many. Instead of a list, a **test for each wolf**: `{"percent": {"where": {"test":
"compare", "left": {"var": "p_hauls", "of": "$each"}, "op": ">=", "right": 3}}, "of": "active"}`. `of` may also be `all`
(the story's wolves) or `online`.

The condition **Enough wolves took part** (`participation`) asks for a share, a floor and a cap:

```json
{"test": "participation", "list": "ch1_did", "of": "active", "percent": 30, "floor": 3, "cap": 150}
```

- It needs the larger of the share (rounded up) and the floor; **the floor never asks for more wolves than are active**, so a
  world of 1 to 5 wolves can always get there; a `cap` is always enough (a crowd of 3,000 does not need 900); never fewer
  than 1. With no active wolves it is false; with `online` not known it is unknown.
- **Why active is the fair count:** a wolf who did it and then went quiet leaves both counts, so the share stays honest when
  the crowd changes.
- **A list or a test:** a list (`list add` the acting wolf when they do a task) is cheap and remembers; a test reads each
  wolf's own variables (`of: "$each"`), switches, lists, days and quests, but never the world's facts or the event (it is
  counted outside one), 3 deep and 8 conditions at most.

## 7. Quests and rewards

A quest is a version 2 story's quest object, shown in each wolf's journal:

```json
{"id": "axle", "title": "An axle for the carter", "summary": "The carter at the ford is stuck.",
 "scope": "player", "lifetime": "campaign", "repeat": "daily", "giver": "rm111", "handIn": true,
 "offer": {"say": "The axle split at the ford. Could you find me another?", "ask": "Is something wrong with the cart?"},
 "objectives": [
   {"id": "ask",   "label": "Ask the carter which part gave way.", "on": "conversation", "match": {"target": "rm111"}},
   {"id": "bring", "label": "Bring a replacement axle.", "on": "gift", "match": {"target": "rm111", "item": "axle"}, "after": ["ask"]},
   {"id": "wheel", "label": "A spare wheel would not go amiss.", "on": "gift", "match": {"target": "rm111", "item": "cart_wheel"}, "optional": true},
   {"id": "thief", "label": "Find who cut the axle.", "on": "story signal", "signal": "thief-named", "hidden": true}],
 "fail": [{"on": "death", "match": {"actor": "rm111"}}, {"afterDays": 3}],
 "rewards": [{"kind": "character.grant", "player": "$actor", "coins": 12, "purse": "town:greyfen", "reason": "the carter thanks you"}],
 "bonus": [{"kind": "deed.award", "player": "$actor", "detail": "Brought the carter a spare wheel too."}]}
```

The examples here use the test town's ids (Greyfen, `rm111` the carter, `town:greyfen`): on your world pick its own
residents, places and purses from the pickers. A `death` filters on who died, its `actor` (it has no `target`): the fail
rule above ends the quest when the carter dies.

- **Scope:** `player` (each wolf their own copy), `party` (the party the wolf was in when it started; one who leaves keeps
  the credit earned), `story` (one copy for everyone, `contributors` different wolves).
- **Objectives** in order with `after`, in any order without; `optional`; `hidden` until revealed or done; counted with
  `goal` and `measure` (times, quantity, coins).
- **Offer and accept:** a `giver` offers it in their dialogue window, a `board` on a town's notice board; `quest.offer`,
  `quest.start` from a trigger. `handIn`: ready, then handed back to the giver. A wolf may abandon it from the journal.
- **Rewards** are effects given to each wolf with credit (`bonus` when every optional objective was done too). **Coins come
  from a purse** and are refused when it is short. A refused reward waits for its wolf by default (`ifRefused: "when here"`):
  it is sent again when they are next in the world.
- **A choice for one wolf.** **Offer a choice in a resident's window** (`npc.offer`) with `"player": "$actor"` lays the
  choice over that resident's window for the acting wolf. Offered again with the same name to another wolf, it is added for
  them: each wolf sees it until they pick it (with `once`, each once) or the visit ends. The words and the reply are the
  latest offer's. Offered with no `player`, it is every wolf's of the story, as before.
- **Its state in the window.** Each quest a wolf has is known to the residents they talk to as
  `player.quest.<story id>/<quest id>` (`offered`, `active`, `ready`, `done`, `failed`, `abandoned`, `expired`):
  `say[player.quest.ninth-howl.1/where_king=active priority=3] Still looking for the King, then?`.
- **Purses that pay:** a town's treasury (`town:<town>`; the city's is the realm's treasury), a business's till
  (`till:job:<founder>`, or `till:<founder>`), a Chapter's (`chapter:<id>`). Approval refuses a purse the world (as last
  saved) has no account for; no world has a town's works, watch or church purse, so `town:<town>:works` is refused too.
- **Who may take part:** audience `players` (a list), `active` (active in the last N days, frozen at publication), `open`
  (nobody at first: a wolf comes in by taking a quest, or by `participant.add`), `everyone` (the active wolves, and each who
  signs in after). Open and everyone take a `cap`, up to 5,000.

## 8. The world's facts and watched events

A condition marked **reads the world's facts** asks about the world as of the event: where a wolf is (region, place, town,
indoors), how many of the story's wolves stand in a region, what a wolf carries and their coins, their standing with a
faction and name in a town, whether they are downed, hurt, wanted or held, their party; a resident alive, where, their job,
mood and mantle; the hour, weekday, season, time of day; the weather; one faction's stance toward another; a quest's and an
objective's state. The game writes these facts with the events, for what the live stories ask, so a story that asks nothing
costs nothing. **A missing fact is unknown, and unknown never fires** (not even under Not): a wolf who is away has no place.

Some events are **watched**: the game writes them only while a live story listens for them (doors opened, closed and tried
locked; buildings entered and left; a region occupied and emptied; the weather changed; the hour, a new day, dawn,
nightfall, a new season; items picked up and dropped; a trade; dialogue windows; a crime seen; a wolf downed and revived;
parties formed and ended; a wolf near a resident; a purse or a wolf's standing past a number; a faction's stance changed; a
mantle taken or laid down; a skill grown a step; an injury healed). Three of them need the trigger to say what to look at,
in its filters:

| Event | Filters it needs | Example |
|---|---|---|
| `near resident` | `target`: the resident | `"match": {"target": "rm111"}`: a wolf comes within three tiles of Ivo, wherever he is |
| `purse crossed` | `target`: the purse; `detail`: the amount | `"match": {"target": "town:greyfen", "detail": "500"}` (`item` is `up` or `down`) |
| `standing crossed` | `target`: the faction; `detail`: the number | `"match": {"target": "watch", "detail": "-20"}` |

### Talk and rumour from a story

A story changes what residents say without writing their words: it gives them **memories** their lines in `Data/Talk` are
gated on, starts **rumours**, and names the realm's **figures**.

**Memories** (`talk.remember`, `talk.forget`): a key and a value (small letters, digits and `_`, up to 40) for 1 to 365
days, remembered by

| `scope` | who | the line's gate |
|---|---|---|
| `realm` | every resident | `world.mem.<key>` |
| `town` (with `town`) | the residents in that town (the whole city is one town) | `world.mem.<key>`, over the realm's |
| `npc` (with `npc`) | that resident | `self.mem.<key>` |
| `pair` (with `npc` and `player`) | that resident, when that wolf is the one they talk to | `pair.mem.<key>` |

```json
{"kind": "talk.remember", "scope": "realm", "key": "nh_stage", "value": "reckoning", "days": 30}
{"kind": "talk.remember", "scope": "pair", "npc": "rm_willa_hale", "player": "$actor", "key": "nh_stone", "value": "brought", "days": 120, "keep": true}
```

A line in a resident's tree: `say[world.mem.nh_stage=reckoning] They say the reckoning is close.` and
`say[pair.mem.nh_stone=brought priority=5] You're the one who brought my mother's stone.` The order a resident recalls them
in: the realm's, then the town's (over it), then talk's own memory of the town (over both); a resident's own; and, talking
with a wolf, the story's pair memory, then talk's own of the two. **Pair memory** is for a wolf of the story: the acting wolf
of a participant trigger (`$actor`) or the looped wolf (`$each`), or a quest's reward; never named, never at a phase's
opening. Memories are taken back when the run ends unless `keep` (then they last their days). A town or a resident keeps 64
of a story's memories, a pair 16; past that the soonest to be forgotten goes first (the console's Problems says so once a
game day). **Campaigns → the run → Variables & timers → What residents remember** lists them, as the game last saved it.

Writing the lines: no resident's name in a library line (they are said by everyone), the speaker's dialect as any line,
and a key table of the campaign's (`nh_stage`, `nh_debts`…) so chapters agree on the words.

**Rumours** (`belief.plant`): a number of residents of a town (living, placed, never the one it is about; the same ones each
time, by the run's seed) come to believe a phrase of a resident, as if they had heard it: gossip tells it ("Ingrid Kendrick
was seen at the Customs House at midnight") and it spreads on friendships and caravans like any belief, fading by the
world's rules. Write the phrase as what follows a name, in small letters at first, with no ids or names: no word with `_`, none
that starts `wolf-`, and none that runs letters into digits (the game takes "rm111", but also "2am" or "3rd-rate", for an
id: write "at two in the morning", "third-rate"). `"claimKind":
"story.cleared"` plants the **clearing**, written as the thing **not** done ("took the Customs House silver"): it is told
"Ingrid Kendrick never took the Customs House silver". A clearing does not lower the old belief; putting a rumour to rest
comes later (`belief.forget`).

**Figures** (`figure.set`, `figure.clear`; a builder's on a hosted world): who residents mean by the ruler, the lord, the
captain, the high priest, the hero or the villain (`#world.figure_villain#` in their lines), known in the realm, in a town,
or among a House. A resident, a wolf (by their name at that moment) or a name only. A figure stays after the story; the
run's timeline keeps the row it replaced, and **Undo this figure…** there puts it back.

## 9. Rehearsing

A rehearsal case is a made-up audience and a list of events with game days, and what should come of it: the phases in
order (`expect`), and optionally the effects, mission credits, triggers fired, variables (`expectVariables`), the outcome,
the quests (`expectQuests`), the chapters started (`expectChapters`). Version 2 cases may pin rolls, give parties, and
state the world's facts at the start and as they change. **Rehearse all paths** runs every case; approval needs every
phase reached by one that passes. **Step through** plays one case event by event: the phase, the journal, the variables
(changes marked), timers, what fired, what would be sent, and "Waiting for" judged as the console judges a running story.

## 10. Approving and publishing

A Dungeon Master approves the exact version rehearsed (a digest covers the whole document). Approval checks the ids it
names against the world (residents, wolves, places and tiles, factions, items, doors, mantles, towns, projects,
templates), and refuses what the world does not have. A publication ticket comes back, good for a day; **Publish** starts
the run: the audience is frozen (unless it is open or everyone), the first phase opens, and the game is told. A campaign of
chapters is approved from **Campaigns → New campaign…** (section 4).

Try a new story on a throwaway copy of the world before the live one: the live world's players are not test subjects.

## 11. The Campaigns console

**The list** answers, for every story, without pressing anything: is it alive, where is it, who is in it, what happened last,
and what is it waiting for (a wolf, a timer, the game's answer to an order, or you). It refreshes itself every 2 seconds.
The bar shows the story runner's heartbeat. Above the table: campaigns as their chapters. Alerts: **paused** (the game
refused an order), **the game is not answering**, **falling behind**, **quiet**, **cannot move on** (nothing left can fire),
**a timer overdue**, **the audience nearly full**, **the runner is not running** (also on the DM tab bar, on LIVE, and in a
Dungeon Master's Dev Console in the game). **Alert rules…** (the owner's) changes the thresholds for the world.

**One story** has its header (version, approver, the chapter strip for a campaign), the steering row, the phase strip, and
tabs: **Overview** (Waiting for, with each trigger's conditions judged ✔ ✘ ?, "would fire for" the wolves here now; Next by
itself; What if…), **Timeline** (phase visits, firings with the values before and after, orders and their receipts, admin
commands, versions), **Triggers** (each on or off, how often it fired, Fire…, Why not?), **Variables & timers**,
**Missions**, **Quests** (a grid: holders down, objectives across), **Wolves & map**, **Orders**, **Audit**.

**Steering.** Every control is a command to the story runner's inbox, taken between two world events, recorded with who,
when, why, and the values before and after:

| Control | What it does |
|---|---|
| Hold / Resume | A held story reads no events, sends nothing, fires no timers. Resume counts what happened meanwhile, or "from now" skips it. |
| Hold all… | Every running story at once (the owner's on a hosted world). |
| Stop… | Tells the game; stopped when it acknowledges. What it gave the world stays; what it held is undone (section 14). |
| Send signal | A story signal now, as a wolf or none. |
| Fire… | A trigger's actions as if its event had happened, for a wolf, checking its conditions or not. |
| On / off | A trigger turned on or off (stays off on later visits of its phase). |
| Go to phase… | A new visit of a phase, with or without its entry effects. Going back is not a rollback. |
| Set / timers | A variable set to a value of its type; a timer fired now, moved, cancelled or started. |
| Quests | Give, take, complete, fail; complete, reveal, add to or reset an objective. Completing by hand gives the rewards as play would ("without rewards" holds them back). |
| Add or remove a wolf | Who takes part. |
| Orders | Retry a refused order (and those after it), retry it when its wolf is next here, or skip it and carry on. |
| Tell the wolves… | A line to the story's wolves, one wolf, a wolf's party, the wolves in a region, or everyone. |
| Move onto a new version… | Section 12. |

### Moving a chapter on

A phase with Readiness (section 4) shows it on the run's **Overview**, under the phase strip:

- **The header**: "Ready to advance to ch1_end (since day 93)", "Not ready: 2 of 4 met", "Waiting for the next day (ready
  since day 93)", or "Goes on by itself when ready".
- **The bars**, one per part. Who took part: "42 of 118 active wolves (36%); 35% and at least 5 needed", the tick where the
  need is; above them "active 118 · in the story 260 · online now 31" and the window (seen in the last 18 game days, 3 real
  days). Time: "11.2 of 10 days in the chapter (1.5 held days not counted)".
- **Advance…** (when ready) takes the ready exit now, with the next phase's entry effects. **Advance anyway…** (when not
  ready) does the same and asks for the story's title as well as a reason. **Advance at the next …** (a phase with `then`)
  counts as ready and waits for that moment. **⏸ Hold / ▶ Resume** sit beside them: **holding stops the chapter's clock**
  (held days never count toward its days, its slow alert or its `autoAfterDays`).
- **The alerts**: **ready to advance** (it waits for you: it needs you, is listed first, and counts on the Campaigns tab),
  and **slow** (not ready after its `slowAfterDays`: "Slow: 21% of the 35% needed after 31 days"; with `slowNeedsYou`, "Ready
  to push: …", and it needs you). A phase whose way on is Readiness is never "cannot move on".
- **The list** has a **Ready** column (the furthest part, or a green "ready"); each campaign's running chapter shows its line
  ("Ch 1 · 36% of 35% · day 11 of 10 · ready") with **Advance…**, which opens it.
- **Who may press it**: whoever may steer the campaign (the owner, or "may steer campaigns"); a chapter whose next phase
  could release an effect needing more (acting on wolves, a builder's) needs that too.
- **What is kept**: the command's row holds the readiness as you saw it (every part's progress) and your reason; the audit
  says it in words; the Timeline shows "ready", "slow", "advanced by …" and "moved on by itself".

*Worked example.* Chapter 1 is at 36% of the 35% needed, 11 of 10 days: the header is green, "Ready to advance to ch1_end
(since day 93)", and the alert says it waits for you. Press **Advance…**, write "Chapter one has done enough, on to the
Herald's Word", read **This will do** (the ending phase's entry effects and the snapshot), and **Send it**.

**Safety rules:** every command needs a reason; every control shows **This will do** (a dry run of the command, nothing
saved or sent) and waits for a second click; stopping, going back, skipping an order and removing a wolf also ask for the
story's title; a command made for an earlier visit or another version is refused unless you say "even if it has moved on";
sending one twice does it once. Commands can only release what the program contains: a different program is a new
approval.

## 12. Moving a run onto a new version

A ninety-day campaign that needs a fix on day 40 keeps its wolves. Edit the story, rehearse it, and approve it **as a new
version of a running campaign** (Story Creator's approval). In Campaigns, **⇪ Move onto a new version…** shows the report
for the run as it stands: what is **kept** (the phase and every phase by id, variables by name, scope and type, timers whose
signal still has a listener, missions and quests whose objectives are unchanged, once-marks, the audience), what is **reset**
(progress of a mission or quest whose objectives changed), **back to its first value** (a variable retyped), **dropped**
(what the new version no longer has), and **new**. If the run's phase is gone, choose where to land. The move is a command:
nothing is delivered by it, the game is told the new words for this visit, and if the game refuses, the run goes back to
the version it was on. Rewards and world changes already given stay.

## 13. Who may do what

Ranks are viewer, dm, builder, owner. In your own studio a Dungeon Master does everything below. On a hosted world:

| What | Needs |
|---|---|
| Draft and rehearse | any account (a viewer's drafts stay in the browser) |
| See the Campaigns console | a Dungeon Master (it names wolves and where they are) |
| Approve and publish a story, a campaign | a Dungeon Master, and the marks below for what it contains |
| A story that **acts on wolves** (`character.injury`, `story.area.damage`, `item.take`, `warrant.issue`) | the owner, or "may act on players' wolves" |
| A story that **needs a builder** (`faction.relation`, `standing.adjust`, since Phase 10 the economy's `prices.reset`, `project.complete`, `project.cancel`, and the realm's figures, `figure.set`, `figure.clear`) | a builder or the owner |
| Undo a figure a story set | a builder or the owner |
| Let go of a campaign's holds | the owner, or "may steer campaigns" |
| Steer a running story (every control) | the owner, or the owner's "may steer campaigns" for the account (My world, Editor accounts) |
| A control that could release an effect marked above | also what approving it would need |
| Declare, set or remove a world variable | the owner, or "may steer campaigns" |
| Hold all; change the alert rules; grant "may steer campaigns" | the owner |

The weather, notices, market events and the other effects need what the same act needs by hand on LIVE: a Dungeon Master.
Underneath it all the game's own guard stays: on a community world with cheats off, a reward to the host's own wolf is
refused whatever the console allowed.

## 14. What the game undoes when a run ends

When a run ends **in the game** (the story's `finish`, an ending phase, or Stop once the game acknowledges it), the game
takes back what the story held, as it was before the story first touched it. What it gave stays.

| Effect | When the run ends |
|---|---|
| `door.lock`, `door.unlock` | the door's lock goes back to what it was before the story touched it (a restart gives the map's) |
| `npc.hold` | the resident is released to their day |
| `npc.follow` | the resident leaves the party and goes home |
| `visitor.add` | the stranger goes |
| `mantle.assign` | the mantle is laid down |
| `board.post` | the notice comes down |
| `weather.set` | the weather goes back to the season's (it also runs out by itself after its hours) |
| `story.ping` | the map markers go (they also run out by themselves) |
| `npc.spare`, `npc.protect` with `lasts: run` | the resident is no longer spared, or protected, by this run (another story's hold stays) |
| `npc.spare`, `npc.protect` with `lasts: campaign` | **stay** until the campaign ends (or an admin lets go of its holds) |
| `talk.remember` | the residents forget it, unless `keep` (then it lasts its days) |
| `belief.plant`, `figure.set` | **stay**: a rumour fades by the world's rules; a figure until cleared (or undone from the timeline) |
| `journal.note` | **stays**: the wolf's journal keeps it |
| items given or placed, coins paid, deeds, standing, warrants, injuries, relations, market events, prices, projects, residents killed | **stay**: not a rollback |

So a chapter that hands the world on should end before the next begins (`story.start` with `"when": "ended"`, the default).

## 15. Troubleshooting: Explain, Why not?, What if

| What you see | Where to look | What it tells you |
|---|---|---|
| "It is not moving" | Overview → **Waiting for** | Each trigger and exit of the phase, its conditions ✔ ✘ ? with the values behind them ("the world says 0 for Fern"), whether it is off or used up, and which wolves here now it could fire for. |
| A trigger did not fire | Triggers → **Why not?** | Its last 10 events of that kind and where each stopped: the filter that did not match (what it held, what it wanted), a wolf not in the story, turned off, already fired for that wolf, a condition false, the world as it was at that event. |
| "Would this work?" | Overview → **What if…** | A made-up event or signal through a copy of the run: what fires, the values after, the effects it would send, the phase it would end in. Nothing is saved. |
| Paused | Orders | The refused order and the game's words ("that wolf is not in the world just now"): Retry, Retry when they are next here, or Skip it and carry on. Give the effect an `ifRefused` next time. |
| "The game is not answering" | the server | An order waits over a minute: is the game server running? (A scratch server never applies orders.) |
| "The runner is not running" | the host | Start `tools/story_runner.py --world KEY` beside the game. |
| A chapter did not start | the earlier chapter's Timeline | Its note says why: not a chapter of the campaign, already running, the audience empty. |
| A watched event never comes | the trigger's filters | `near resident`, `purse crossed` and `standing crossed` need their names in the filters; the others need a live story listening (the watch list goes with each phase). |
| A world variable reads its first value | Campaigns → World variables | A story declaring it as another type reads nothing of it; check the type. |
| It says ready but did not move | Overview → **Readiness** | `auto` is off: it waits for an admin. Press **Advance…** (or give it `autoAfterDays`). With `then` it waits for that moment ("Waiting for the next day"). A held run's clock is stopped. |
| The share reads 0% | Readiness → the bar | No active wolves yet (none seen in the last `activeDays` game days), or the list is empty. Who is "online" is unknown until the world says (the story watches its wolves). |
| A resident's memory line never plays | Variables & timers → **What residents remember** | Is the key there, with days left? A town's is that town's residents only; a pair's only for that wolf. The line's gate must match the key and value exactly. |
| Approval refuses a purse | the purse | `till:job:<founder>` (or `till:<founder>`) of a till the world has; no town has works, watch or church purses. |
| Problems in the draft | Story Creator → **Problems** | Every problem at once with **Go**, and the world's warnings (a signal nothing hears, a counter nothing sets, an id the world lacks). |

## 16. Limits

| What | Limit |
|---|---|
| Phases | 100 |
| Triggers per phase (with the campaign's) | 64 |
| Actions per trigger | 12 (version 1); 24 counting nested ones, 3 deep (version 2) |
| Conditions | 8 per group, 5 levels |
| Entry effects, exits, missions per phase | 12, 16, 16 |
| Variables; values in a list; characters of a text | 64; 500; 200 |
| Quests; objectives; rewards | 32; 12; 8 |
| Regions; embedded scenes | 128; 32 |
| Audience | 500 (players, active); 5,000 (open, everyone) |
| Chapters in a campaign | 12 |
| Rehearsal cases; facts per case | 32; 256 |
| Readiness | 8 parts; an `any` of 2 to 4, one deep; labels 80 characters; `minDays` and the slow and auto days up to 3,650 |
| Counting who took part | 5,000 wolves; a test for each wolf 3 deep and 8 conditions; not inside For each; `activeDays` 1 to 120 |
| Talk memory | keys and values 40 small letters, digits and `_`; 1 to 365 days; 64 a town or a resident, 16 a pair; 8,192 and 20,000 in all |
| Values told to residents | 8 a story |
| A rumour | a phrase of 120 characters; 1 to 50 residents; 10% to 90% sure |

<!-- reference:begin (made by tools/story_guide.py from Data/Storylines/trigger-catalogue.json: do not edit by hand) -->

## Reference: every event, condition, action and effect

Made from the trigger catalogue (`Data/Storylines/trigger-catalogue.json`), the same one the workbench draws its sentences and pickers from and the checks read their bounds from. **v2** marks what needs a version 2 story; **watched** an event the game writes only while a live story listens for it; **the story's own** what the story keeps itself rather than sending to the game; **acts on wolves** what needs the owner or "may act on players' wolves" on a hosted world; **needs a builder** what needs a builder or the owner there; **undone at the end** what the game takes back when the run ends.

### Events

208 events. A trigger names one (`"on"`) and may filter its fields by exact text (`"match"`: actor, target, cell, item, detail). Fields not listed are empty for that event.

#### Story engine

<a id="event-story-phase"></a>**`story phase`**: A story enters a phase

A world story run (a campaign) is put into a phase by the story runner; recorded by the game.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the story run (run) | any story |
| `target` | the phase entered (phase) | any phase |
| `detail` | the story's title (text) | any |

Triggers cannot listen for it (the runner refuses it as a trigger event); exits and the runner use it.

```json
{"id": "when-story-phase", "on": "story phase", "actions": [{"do": "note", "text": "A story enters a phase"}]}
```

<a id="event-clock"></a>**`clock`**: Time passes in the phase

The story runner's clock: fires once the phase has been open the trigger's afterDays game days.

No fields. The delay is the trigger's afterDays (from 1/1440, a minute, to 36,500 days), counted from when the phase opened.

```json
{"id": "when-clock", "on": "clock", "actions": [{"do": "note", "text": "Time passes in the phase"}]}
```

<a id="event-dm-signal"></a>**`dm signal`**: The Dungeon Master gives a direction

The Dungeon Master sends a named direction from the Campaigns tab; it can only move a phase through an exit (never a trigger).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf it is sent as, if any (wolf) | any wolf |
| `detail` | the direction's name (text) | any |

Exits only. Made by the story runner, never in game.events. Tried as a story signal of the same name first.

```json
{"id": "when-dm-signal", "on": "dm signal", "actions": [{"do": "note", "text": "The Dungeon Master gives a direction"}]}
```

<a id="event-story-signal"></a>**`story signal`**: A story signal arrives

A named signal sent by another trigger (emit), by a timer (schedule) or by the Dungeon Master.

| Field | Holds | Left out means |
|---|---|---|
| `signal` | the signal's name (text) | any |
| `actor` | the wolf it concerns, if any (wolf) | any wolf |

Not in game.events. An emitted signal keeps the source event's actor, target, cell, item, detail, quantity, coins and tile; a timer's carries only the timer's owner as actor. Each name is delivered at most once per world event, after that event's triggers. The catalogue has a 'signal' type for the name.

```json
{"id": "when-story-signal", "on": "story signal", "signal": "gate-opens", "actions": [{"do": "note", "text": "A story signal arrives"}]}
```

<a id="event-missions-completed"></a>**`missions completed`**: The missions are done

An exit that waits until every mission it names has reached its completion threshold, for a wolf in the phase's cohort.

Exits only (not a trigger). Its parameter is the list 'missions' (ids of this phase's missions), not an event field.

```json
{"id": "when-missions-completed", "on": "missions completed", "actions": [{"do": "note", "text": "The missions are done"}]}
```

<a id="event-ready"></a>**`ready`**: The phase is ready

The way on when the phase is ready: every part of its Readiness met (enough of the active wolves took part, enough days in the phase or the chapter, values, missions, a quest). With auto it is taken by itself (at once, or at the moment `then` names); otherwise the Campaigns console says it is ready and an admin presses Advance.

Exits only, and only in a phase with "ready" (exactly one). Taken by the story runner, never by a world event. Its only fields are on and to.

```json
{"id": "when-ready", "on": "ready", "actions": [{"do": "note", "text": "The phase is ready"}]}
```

<a id="event-story-started"></a>**`story started`**: Another story starts (the story's own)

Another story of this world was published, or started by a chapter before it (story.start): its id, not its run, so one chapter can wait for another.

| Field | Holds | Left out means |
|---|---|---|
| `target` | the story (story) | any story |
| `item` | its run (run) | any run |
| `detail` | the campaign it is a chapter of (text) | any campaign |

Sent by the story runner to every other live story of the world that listens for it; never in the world's log.

```json
{"id": "when-story-started", "on": "story started", "match": {"target": "the-crossing"}, "actions": [{"do": "note", "text": "Another story starts"}]}
```

<a id="event-story-ended"></a>**`story ended`**: Another story ends (the story's own)

Another story of this world ended: complete, success, failure, abandoned or stopped (by an admin).

| Field | Holds | Left out means |
|---|---|---|
| `target` | the story (story) | any story |
| `item` | its run (run) | any run |
| `detail` | how it ended: complete, success, failure, abandoned, stopped (text) | any ending |

Sent by the story runner to every other live story of the world that listens for it; never in the world's log.

```json
{"id": "when-story-ended", "on": "story ended", "match": {"target": "the-crossing"}, "actions": [{"do": "note", "text": "Another story ends"}]}
```

#### Scenes

<a id="event-scene-arrived"></a>**`scene arrived`**: A directed resident arrives (has a tile)

A story's npc.walk order finished: the resident stands on the tile it was sent to.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the resident the scene directs (resident) | any resident |
| `target` | the story run the scene belongs to (run) | any story |
| `cell` | where the resident is (place) | anywhere |
| `detail` | the author's cue on the order (cue) | any cue |
| `quantity` | the visit (the run's visit number) (number) |  |

tile = the resident's tile on arrival.

```json
{"id": "when-scene-arrived", "on": "scene arrived", "actions": [{"do": "note", "text": "A directed resident arrives"}]}
```

<a id="event-scene-spoken"></a>**`scene spoken`**: A directed resident speaks (has a tile)

A story's npc.speak order was carried out: the resident said its line aloud.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the resident the scene directs (resident) | any resident |
| `target` | the story run the scene belongs to (run) | any story |
| `cell` | where the resident is (place) | anywhere |
| `detail` | the author's cue on the order (cue) | any cue |
| `quantity` | the visit (the run's visit number) (number) |  |

```json
{"id": "when-scene-spoken", "on": "scene spoken", "actions": [{"do": "note", "text": "A directed resident speaks"}]}
```

<a id="event-scene-posed"></a>**`scene posed`**: A directed resident takes a pose (has a tile)

A story's npc.pose order finished: the resident is now in the posture asked for.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the resident the scene directs (resident) | any resident |
| `target` | the story run the scene belongs to (run) | any story |
| `cell` | where the resident is (place) | anywhere |
| `detail` | the author's cue on the order (cue) | any cue |
| `quantity` | the visit (the run's visit number) (number) |  |

```json
{"id": "when-scene-posed", "on": "scene posed", "actions": [{"do": "note", "text": "A directed resident takes a pose"}]}
```

<a id="event-scene-blocked"></a>**`scene blocked`**: A directed resident can't do it (has a tile)

A story's walk, speak or pose order could not be carried out: the resident is gone, dead, downed, fighting, following someone, has no way there, or made no progress for 3 minutes.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the resident the scene directs (resident) | any resident |
| `target` | the story run the scene belongs to (run) | any story |
| `cell` | where the resident is (place) | anywhere |
| `detail` | the author's cue on the order (cue) | any cue |
| `quantity` | the visit (the run's visit number) (number) |  |

The story engine sets no tile when blocked (doc 85), but recordEvent then fills the resident's own tile if it is on stage in that cell; cell is empty when the resident no longer exists.

```json
{"id": "when-scene-blocked", "on": "scene blocked", "actions": [{"do": "note", "text": "A directed resident can't do it"}]}
```

<a id="event-scene-finished"></a>**`scene finished`**: A scene finishes (has a tile)

An embedded scene (cinematic) reached its last beat.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf who started the scene, if any (wolf) | any wolf |
| `target` | the story run the scene belongs to (run) | any story |
| `cell` | where that wolf is (place) | anywhere |
| `detail` | the scene's cue (cue) | any cue |
| `quantity` | the visit (the run's visit number) (number) |  |

actor empty when no wolf started it; cell and tile are that wolf's, filled by recordEvent when it is in the world.

```json
{"id": "when-scene-finished", "on": "scene finished", "actions": [{"do": "note", "text": "A scene finishes"}]}
```

<a id="event-scene-choice"></a>**`scene choice`**: A scene's choice is made (has a tile)

A choice beat of an embedded scene closed: the votes (or the default, on a tie or a timeout) picked an option.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf who started the scene, if any (wolf) | any wolf |
| `target` | the story run the scene belongs to (run) | any story |
| `cell` | where that wolf is (place) | anywhere |
| `detail` | cue.option: the scene's cue and the option chosen (cue) | any cue |
| `quantity` | the visit (the run's visit number) (number) |  |

detail is '<cue>.<option id>'.

```json
{"id": "when-scene-choice", "on": "scene choice", "actions": [{"do": "note", "text": "A scene's choice is made"}]}
```

<a id="event-scene-response"></a>**`scene response`**: Players answer in a scene (has a tile)

A response beat of an embedded scene closed, answered by the players or timed out.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf who started the scene, if any (wolf) | any wolf |
| `target` | the story run the scene belongs to (run) | any story |
| `cell` | where that wolf is (place) | anywhere |
| `detail` | cue.beat.submitted or cue.beat.timeout (cue) | any cue |
| `quantity` | the visit (the run's visit number) (number) |  |

The typed words are never logged.

```json
{"id": "when-scene-response", "on": "scene response", "actions": [{"do": "note", "text": "Players answer in a scene"}]}
```

<a id="event-scene-failed"></a>**`scene failed`**: A scene fails (has a tile)

An embedded scene could not go on (marked failed) and ended.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf who started the scene, if any (wolf) | any wolf |
| `target` | the story run the scene belongs to (run) | any story |
| `cell` | where that wolf is (place) | anywhere |
| `detail` | the scene's cue (cue) | any cue |
| `quantity` | the visit (the run's visit number) (number) |  |

```json
{"id": "when-scene-failed", "on": "scene failed", "actions": [{"do": "note", "text": "A scene fails"}]}
```

#### Regions

<a id="event-region-entered"></a>**`region entered`**: A wolf enters a story region (mission evidence, has a tile)

A wolf's move took it inside a region of a live story that watches regions.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the region's id (region) | any region |
| `cell` | the cell of the first tile inside (place) | anywhere |
| `detail` | the story run the region belongs to (run) | any story |

tile = the first tile inside.

```json
{"id": "when-region-entered", "on": "region entered", "match": {"target": "square"}, "actions": [{"do": "note", "text": "A wolf enters a story region"}]}
```

<a id="event-region-left"></a>**`region left`**: A wolf leaves a story region (mission evidence, has a tile)

A wolf's move took it outside a region of a live story that watches regions.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the region's id (region) | any region |
| `cell` | the cell of the last tile inside (place) | anywhere |
| `detail` | the story run the region belongs to (run) | any story |

tile = the last tile inside.

```json
{"id": "when-region-left", "on": "region left", "match": {"target": "square"}, "actions": [{"do": "note", "text": "A wolf leaves a story region"}]}
```

<a id="event-region-occupied"></a>**`region occupied`**: A region is no longer empty (watched, has a tile)

The count of the story's wolves standing in one of its regions went from none to one (quantity: the count). Signing in inside it counts. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the region (region) | any region |
| `detail` | the story run (run) | this story |
| `quantity` | how many are inside (number) | any |

```json
{"id": "when-region-occupied", "on": "region occupied", "match": {"target": "square"}, "actions": [{"do": "note", "text": "A region is no longer empty"}]}
```

<a id="event-region-emptied"></a>**`region emptied`**: A region is empty again (watched, has a tile)

The last of the story's wolves standing in one of its regions left it (or signed out inside it). Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the region (region) | any region |
| `detail` | the story run (run) | this story |
| `quantity` | how many are inside (0) (number) | any |

```json
{"id": "when-region-emptied", "on": "region emptied", "match": {"target": "square"}, "actions": [{"do": "note", "text": "A region is empty again"}]}
```

#### Coming and going

<a id="event-arrival"></a>**`arrival`**: A wolf comes into the world (has a tile)

A player enters the world with a character (logs in).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `cell` | where it appears (place) | anywhere |

```json
{"id": "when-arrival", "on": "arrival", "actions": [{"do": "note", "text": "A wolf comes into the world"}]}
```

<a id="event-enter-place"></a>**`enter place`**: A wolf enters a place (mission evidence, has a tile)

A wolf crosses a door or a cell edge into another cell or interior (residents' crossings are not logged).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the place entered (place) | anywhere |
| `cell` | the place entered (same as target) (place) | anywhere |
| `detail` | the place left (place) | anywhere |

detail is a cell or interior id, so a place picker fits it too.

```json
{"id": "when-enter-place", "on": "enter place", "match": {"target": "greyfen-square"}, "actions": [{"do": "note", "text": "A wolf enters a place"}]}
```

<a id="event-departure"></a>**`departure`**: A wolf leaves the world (has a tile)

A player's character leaves the world (logs out or disconnects).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `cell` | where it was (place) | anywhere |

```json
{"id": "when-departure", "on": "departure", "actions": [{"do": "note", "text": "A wolf leaves the world"}]}
```

<a id="event-relocation"></a>**`relocation`**: A resident is moved to a new home (has a tile)

A resident is given a new home elsewhere: by the Dungeon Master's director (npc_relocate), or moved to a House's estate site; it walks there before the home changes.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the resident (resident) | any resident |
| `cell` | where it is now (place) | anywhere |
| `detail` | 'new home in <cell id>' (text) | any |

```json
{"id": "when-relocation", "on": "relocation", "actions": [{"do": "note", "text": "A resident is moved to a new home"}]}
```

<a id="event-character-created"></a>**`character created`**: A new wolf is made

A player makes a new character.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the new wolf (wolf) | any wolf |

The wolf is not in the world yet, so no cell or tile.

```json
{"id": "when-character-created", "on": "character created", "actions": [{"do": "note", "text": "A new wolf is made"}]}
```

<a id="event-spawn"></a>**`spawn`**: A resident is spawned (has a tile)

A Dungeon Master's spawn rule puts a new resident into its area.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the new resident (resident) | any resident |
| `cell` | where it appears (place) | anywhere |
| `detail` | 'spawn rule <rule id>' (text) | any |

```json
{"id": "when-spawn", "on": "spawn", "actions": [{"do": "note", "text": "A resident is spawned"}]}
```

<a id="event-welcome"></a>**`welcome`**: An innkeeper welcomes a newcomer (has a tile)

An innkeeper welcomes a newcomer wolf and points out others who could help them.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the innkeeper (resident) | any resident |
| `target` | the newcomer (wolf) | any wolf |
| `cell` | where the innkeeper is (place) | anywhere |
| `detail` | '<n> pointed out' (text) | any |

```json
{"id": "when-welcome", "on": "welcome", "actions": [{"do": "note", "text": "An innkeeper welcomes a newcomer"}]}
```

<a id="event-welcomed-back"></a>**`welcomed back`**: A wolf comes back after a while (has a tile)

A wolf comes back into the world after days away and is told what happened meanwhile.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `cell` | where it is (place) | anywhere |
| `detail` | '<n> days away' (text) | any |

```json
{"id": "when-welcomed-back", "on": "welcomed back", "actions": [{"do": "note", "text": "A wolf comes back after a while"}]}
```

<a id="event-near-resident"></a>**`near resident`**: A wolf comes near a resident (watched, has a tile)

A wolf comes within three tiles of a resident the trigger names (the moving location: wherever the resident is now), in the same place; again only after they have been more than five tiles apart. Name the resident. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the resident (resident) | the resident named |
| `cell` | the place (place) | anywhere |
| `quantity` | how many tiles apart (number) | any |

A trigger on it must name `target` in its filters.

Looked at once a second while a story listens: the residents named in the filters of its triggers and exits.

```json
{"id": "when-near-resident", "on": "near resident", "match": {"target": "rm111"}, "actions": [{"do": "note", "text": "A wolf comes near a resident"}]}
```

#### Talk, ties and company

<a id="event-conversation"></a>**`conversation`**: Someone talks to someone (mission evidence, has a tile)

A wolf speaks to a resident (chat or the dialogue window), a resident answers a wolf, or two residents finish an ambient talk.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who spoke (character) | anyone |
| `target` | who was spoken to (character) | anyone |
| `cell` | where the speaker is (place) | anywhere |
| `detail` | for residents' own talk only: the topic kind (news, deed, friends, day...) (text) | any |

Never what was said. Wolf to resident: actor = the wolf. Resident answering: actor = the resident, target = the wolf. actor (or target) is empty for an unrecognised voice. An ambient quarrel is logged as 'quarrel', not this.

```json
{"id": "when-conversation", "on": "conversation", "actions": [{"do": "note", "text": "Someone talks to someone"}]}
```

<a id="event-marriage"></a>**`marriage`**: Two residents marry (has a tile)

Once a game week, two unmarried adult residents who like and know each other well enough may marry; one moves in with the other.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | one spouse (resident) | any resident |
| `target` | the other spouse (resident) | any resident |
| `cell` | where the first spouse is (place) | anywhere |
| `detail` | '<name> moves in' (text) | any |

```json
{"id": "when-marriage", "on": "marriage", "match": {"target": "rm111"}, "actions": [{"do": "note", "text": "Two residents marry"}]}
```

<a id="event-groomed"></a>**`groomed`**: Someone is groomed (mission evidence, has a tile)

A wolf grooms another character, or itself.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the groomer (character) | anyone |
| `target` | who was groomed (empty when self) (character) | anyone |
| `cell` | where the groomed one is (place) | anywhere |
| `detail` | 'self' or 'other' (text) | any |

```json
{"id": "when-groomed", "on": "groomed", "actions": [{"do": "note", "text": "Someone is groomed"}]}
```

<a id="event-introduced"></a>**`introduced`**: Someone gives their name (mission evidence, has a tile)

A character says one of its registered names to another, who learns it (hidden names on).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who gave the name (character) | anyone |
| `target` | who learned it (character) | anyone |
| `cell` | where the speaker is (place) | anywhere |
| `detail` | the name given (text) | any |

```json
{"id": "when-introduced", "on": "introduced", "actions": [{"do": "note", "text": "Someone gives their name"}]}
```

<a id="event-performance"></a>**`performance`**: Someone performs (mission evidence, has a tile)

A wolf begins to sing, tell a tale or play an instrument it carries, in a tavern or anywhere.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the performer (wolf) | any wolf |
| `cell` | where (place) | anywhere |
| `item` | 'sing', 'tale', or the instrument's item id (text) | any |

```json
{"id": "when-performance", "on": "performance", "actions": [{"do": "note", "text": "Someone performs"}]}
```

<a id="event-shared-meal"></a>**`shared meal`**: A meal is shared (mission evidence, has a tile)

A wolf eats beside another who is eating too (within two tiles); once a game day for a pair.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who ate (character) | anyone |
| `target` | who was eating beside them (character) | anyone |
| `cell` | where (place) | anywhere |

```json
{"id": "when-shared-meal", "on": "shared meal", "actions": [{"do": "note", "text": "A meal is shared"}]}
```

<a id="event-story-told"></a>**`story told`**: A story is finished as a book (mission evidence, has a tile)

A chronicle story is finished and bound as a book; one event for each wolf in it.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | a wolf in the story (wolf) | any wolf |
| `cell` | where the wolf is (place) | anywhere |
| `detail` | the book's title (text) | any |

```json
{"id": "when-story-told", "on": "story told", "actions": [{"do": "note", "text": "A story is finished as a book"}]}
```

<a id="event-table-game"></a>**`table game`**: A table game is won (mission evidence, has a tile)

A tavern table game ends with a winner, who takes the pot less the house's penny.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the winner (character) | anyone |
| `cell` | the tavern (place) | anywhere |
| `item` | the game: knucklebones, wolves or liars (text) | any |
| `detail` | the game's name (text) | any |
| `quantity` | players at the table (number) |  |
| `coins` | the pot (number) |  |

```json
{"id": "when-table-game", "on": "table game", "actions": [{"do": "note", "text": "A table game is won"}]}
```

<a id="event-tended"></a>**`tended`**: Someone is helped up (mission evidence, has a tile)

A character tends a downed one beside it and stands them up.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who tended (character) | anyone |
| `target` | who was helped up (character) | anyone |
| `cell` | where (place) | anywhere |

```json
{"id": "when-tended", "on": "tended", "actions": [{"do": "note", "text": "Someone is helped up"}]}
```

<a id="event-together"></a>**`together`**: Two worked together (mission evidence, has a tile)

When a joint work (foraging, farm work, bench work) ends, one event for each pair who worked enough beats side by side.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | one of the pair (character) | anyone |
| `target` | the other (character) | anyone |
| `cell` | where the work was (place) | anywhere |
| `item` | the activity's id (text) | any |
| `detail` | the joint work's id (text) | any |
| `quantity` | beats they worked together (number) |  |

```json
{"id": "when-together", "on": "together", "actions": [{"do": "note", "text": "Two worked together"}]}
```

<a id="event-trouble-heard"></a>**`trouble heard`**: A wolf hears of a trouble (mission evidence, has a tile)

A resident tells a wolf of a trouble of theirs in conversation.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the resident with the trouble (resident) | any resident |
| `cell` | where the wolf is (place) | anywhere |
| `detail` | the trouble's kind: short, feud, work, child (text) | any |

```json
{"id": "when-trouble-heard", "on": "trouble heard", "match": {"target": "rm111"}, "actions": [{"do": "note", "text": "A wolf hears of a trouble"}]}
```

<a id="event-trouble-solved"></a>**`trouble solved`**: A trouble is solved (mission evidence, has a tile)

A wolf solves a resident's trouble.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the resident it helped (resident) | any resident |
| `cell` | where the wolf is (place) | anywhere |
| `item` | the trouble's kind: short, feud, work, child (text) | any |
| `coins` | coins it brought (number) |  |

```json
{"id": "when-trouble-solved", "on": "trouble solved", "match": {"target": "rm111"}, "actions": [{"do": "note", "text": "A trouble is solved"}]}
```

<a id="event-vouch"></a>**`vouch`**: A wolf vouches for another (mission evidence, has a tile)

A wolf tells a resident it knows that a newcomer is with them; the resident and its household warm to the newcomer.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who vouched (wolf) | any wolf |
| `target` | who was vouched for (wolf) | any wolf |
| `cell` | where the voucher is (place) | anywhere |
| `detail` | the resident told (resident) | any resident |

detail holds a resident id, so a resident picker fits it.

```json
{"id": "when-vouch", "on": "vouch", "actions": [{"do": "note", "text": "A wolf vouches for another"}]}
```

<a id="event-witnessed"></a>**`witnessed`**: A wolf stands witness (mission evidence, has a tile)

An invited wolf comes within ten tiles of a wedding or a funeral while it is on.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the witness (wolf) | any wolf |
| `target` | the first host (resident) | any resident |
| `cell` | where it is held (place) | anywhere |
| `item` | 'wedding' or 'funeral' (text) | any |
| `detail` | the occasion's id ('<kind>:<subject>') (text) | any |

```json
{"id": "when-witnessed", "on": "witnessed", "match": {"target": "rm111"}, "actions": [{"do": "note", "text": "A wolf stands witness"}]}
```

<a id="event-matchmaking"></a>**`matchmaking`**: A resident points out a companion (has a tile)

A resident talking to a newcomer wolf suggests another wolf they might meet (doc 52).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the matchmaker (resident) | any resident |
| `target` | the newcomer told (wolf) | any wolf |
| `cell` | where the resident is (place) | anywhere |
| `detail` | '<reason> <the suggested wolf's id>' (text) | any |

```json
{"id": "when-matchmaking", "on": "matchmaking", "actions": [{"do": "note", "text": "A resident points out a companion"}]}
```

<a id="event-nickname"></a>**`nickname`**: A wolf is given a nickname (has a tile)

After a notable deed (or a third small one), a resident who likes the wolf coins a nickname for it.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf named (wolf) | any wolf |
| `target` | who coined it (resident) | any resident |
| `cell` | where the deed was done (place) | anywhere |
| `detail` | '<nickname> (<nickname id>)' (text) | any |

tile only when the wolf is in the deed's cell.

```json
{"id": "when-nickname", "on": "nickname", "match": {"target": "rm111"}, "actions": [{"do": "note", "text": "A wolf is given a nickname"}]}
```

<a id="event-nickname-dropped"></a>**`nickname dropped`**: A nickname is dropped (has a tile)

A wolf drops one of its nicknames; it is never coined again from that deed.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `cell` | where the wolf is (place) | anywhere |
| `detail` | '<nickname> (<nickname id>)' (text) | any |

```json
{"id": "when-nickname-dropped", "on": "nickname dropped", "actions": [{"do": "note", "text": "A nickname is dropped"}]}
```

<a id="event-promise"></a>**`promise`**: A promise is made (has a tile)

In conversation (not the dialogue window) the NPC Mind reports a promise: by the resident to the wolf, or by the wolf to the resident.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who promised (character) | anyone |
| `target` | to whom (character) | anyone |
| `cell` | where the promiser is (place) | anywhere |
| `detail` | what was promised, in words (text) | any |

Kept if the promiser trades with, pays, gives to or helps the other within 3 days ('promise kept', not listed); else 'promise broken'.

```json
{"id": "when-promise", "on": "promise", "actions": [{"do": "note", "text": "A promise is made"}]}
```

<a id="event-promise-broken"></a>**`promise broken`**: A promise is broken (has a tile)

A promise falls due without being kept.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who promised (character) | anyone |
| `target` | to whom (character) | anyone |
| `cell` | where the promiser is (place) | anywhere |
| `detail` | what was promised, in words (text) | any |

```json
{"id": "when-promise-broken", "on": "promise broken", "actions": [{"do": "note", "text": "A promise is broken"}]}
```

<a id="event-sermon"></a>**`sermon`**: A sermon is preached (has a tile)

A priest reaches the pulpit and begins the week's sermon.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the priest (resident) | any resident |
| `target` | the town (town) | any town |
| `cell` | the church (place) | anywhere |
| `detail` | the sermon's title (text) | any |

```json
{"id": "when-sermon", "on": "sermon", "actions": [{"do": "note", "text": "A sermon is preached"}]}
```

<a id="event-table-game-begun"></a>**`table game begun`**: A table game begins (has a tile)

A wolf starts a tavern table game with the players seated.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who started it (wolf) | any wolf |
| `cell` | the tavern (place) | anywhere |
| `item` | the game: knucklebones, wolves or liars (text) | any |
| `detail` | the table's id (text) | any |
| `quantity` | players seated (number) |  |
| `coins` | each player's stake (number) |  |

```json
{"id": "when-table-game-begun", "on": "table game begun", "actions": [{"do": "note", "text": "A table game begins"}]}
```

<a id="event-tie-ended"></a>**`tie ended`**: A newcomer tie ends (has a tile)

A newcomer's tie with a mentor is ended by the host or the Dungeon Master.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf whose tie ended (wolf) | any wolf |
| `cell` | where it is (place) | anywhere |
| `detail` | 'by <who>' (text) | any |

A player ending their own tie from the client is not logged.

```json
{"id": "when-tie-ended", "on": "tie ended", "actions": [{"do": "note", "text": "A newcomer tie ends"}]}
```

<a id="event-vouch-broken"></a>**`vouch broken`**: A vouch goes bad (has a tile)

A newcomer someone vouched for wrongs the resident they were vouched to.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who vouched (wolf) | any wolf |
| `target` | who was vouched for (wolf) | any wolf |
| `cell` | where the voucher is (place) | anywhere |
| `detail` | the resident wronged (resident) | any resident |

```json
{"id": "when-vouch-broken", "on": "vouch broken", "actions": [{"do": "note", "text": "A vouch goes bad"}]}
```

<a id="event-mantle-bound"></a>**`mantle bound`**: A resident takes a mantle (watched)

A resident now wears a mantle (doc 84: a dialogue of their own), by a DM's hand, a story's, or a mantle loaded again. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the resident (resident) | any resident |
| `item` | the mantle (mantle) | any mantle |

```json
{"id": "when-mantle-bound", "on": "mantle bound", "actions": [{"do": "note", "text": "A resident takes a mantle"}]}
```

<a id="event-mantle-released"></a>**`mantle released`**: A resident lays a mantle down (watched)

A resident no longer wears the mantle they wore. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the resident (resident) | any resident |
| `item` | the mantle (mantle) | any mantle |

```json
{"id": "when-mantle-released", "on": "mantle released", "actions": [{"do": "note", "text": "A resident lays a mantle down"}]}
```

#### Gifts, items and loans

<a id="event-gift"></a>**`gift`**: A gift is given (mission evidence, has a tile)

A character gives goods or coins to another, the first gift of the game day between that pair (the one that warms the bond).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the giver (character) | anyone |
| `target` | who receives it (character) | anyone |
| `cell` | where the giver is (place) | anywhere |
| `item` | what was given (empty for coins only) (item) | anything |
| `quantity` | how many (number) |  |
| `coins` | coins given (number) |  |

A trigger on 'gift' also fires on 'gift again'. A gift's item filter matches the base item, so a marked or crafted rope counts as rope (unless the filter names marks with ~ or @).

```json
{"id": "when-gift", "on": "gift", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "A gift is given"}]}
```

<a id="event-gift-again"></a>**`gift again`**: Another gift the same day (mission evidence, has a tile)

A character gives to the same receiver again on a game day it has already given to them (no more warmth).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the giver (character) | anyone |
| `target` | who receives it (character) | anyone |
| `cell` | where the giver is (place) | anywhere |
| `item` | what was given (empty for coins only) (item) | anything |
| `quantity` | how many (number) |  |
| `coins` | coins given (number) |  |

Item filter matches the base item, as for gift.

```json
{"id": "when-gift-again", "on": "gift again", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "Another gift the same day"}]}
```

<a id="event-gift-used"></a>**`gift used`**: A Gift is used outside a fight (mission evidence, has a tile)

A wolf uses a work Gift outside a fight (Mend, Shortcut, Lighten Load, Dowse, Echo, Carry, or lending it to a workshop).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `cell` | where it is (place) | anywhere |
| `item` | the Gift's family (text) | any |
| `detail` | the ability's id (text) | any |

item is a Gift family, not an item id. Gifts used in fights are not logged.

```json
{"id": "when-gift-used", "on": "gift used", "actions": [{"do": "note", "text": "A Gift is used outside a fight"}]}
```

<a id="event-forage"></a>**`forage`**: Someone forages (mission evidence, has a tile)

A wolf gathers from wild ground (alone, or the whole find of a joint foraging).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the forager (wolf) | any wolf |
| `cell` | where (place) | anywhere |
| `item` | what was gathered (item) | anything |
| `detail` | the foraging ground's kind (text) | any |
| `quantity` | how many (a joint's whole find) (number) |  |

```json
{"id": "when-forage", "on": "forage", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "Someone forages"}]}
```

<a id="event-lent"></a>**`lent`**: Something is lent (mission evidence, has a tile)

A wolf accepts a loan another offered: the goods move to the borrower until due.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the lender (character) | anyone |
| `target` | the borrower (wolf) | any wolf |
| `cell` | where the lender is (place) | anywhere |
| `item` | what was lent (item) | anything |
| `detail` | the loan's id (text) | any |
| `quantity` | how many (number) |  |

item may be a marked id (base~marks).

```json
{"id": "when-lent", "on": "lent", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "Something is lent"}]}
```

<a id="event-loan-returned"></a>**`loan returned`**: A loan is given back (mission evidence, has a tile)

A borrower gives a loan back, or it falls due and the courier takes back what they still have (the whole loan).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the borrower (character) | anyone |
| `target` | the lender (character) | anyone |
| `cell` | where the borrower is (place) | anywhere |
| `item` | what was lent (item) | anything |
| `detail` | the loan's id (text) | any |
| `quantity` | how many came back (number) |  |

cell and tile only when the borrower is in the world (a loan falling due may come back while they are away). item may be a marked id.

```json
{"id": "when-loan-returned", "on": "loan returned", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "A loan is given back"}]}
```

<a id="event-loan-unreturned"></a>**`loan unreturned`**: A loan isn't given back (has a tile)

A loan falls due and the borrower no longer has all of it: the rest is owed to the lender.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the borrower (character) | anyone |
| `target` | the lender (character) | anyone |
| `cell` | where the borrower is (place) | anywhere |
| `item` | what was lent (item) | anything |
| `detail` | the loan's id (text) | any |
| `quantity` | how many are missing (number) |  |
| `coins` | what is now owed (number) |  |

cell and tile only when the borrower is in the world.

```json
{"id": "when-loan-unreturned", "on": "loan unreturned", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "A loan isn't given back"}]}
```

<a id="event-item-picked-up"></a>**`item picked up`**: A wolf picks up an item (watched, mission evidence, has a tile)

A wolf picked up something lying on the ground (what was knocked loose in a fight). Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `item` | the item (item) | anything |
| `cell` | where (place) | anywhere |
| `quantity` | how many (number) | any |

```json
{"id": "when-item-picked-up", "on": "item picked up", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "A wolf picks up an item"}]}
```

<a id="event-item-dropped"></a>**`item dropped`**: A wolf drops an item (watched, mission evidence, has a tile)

A wolf's held item fell to the ground (knocked loose in a fight). Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `item` | the item (item) | anything |
| `cell` | where (place) | anywhere |
| `detail` | how (text) | any |

```json
{"id": "when-item-dropped", "on": "item dropped", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "A wolf drops an item"}]}
```

#### Fights, hunting, death

<a id="event-hunted"></a>**`hunted`**: An animal is killed in a hunt (mission evidence, has a tile)

An animal falls in a hunt; its parts go to the hunters.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who made the kill (character) | anyone |
| `target` | the species (text) | any |
| `cell` | where the hunt was (place) | anywhere |
| `detail` | the kill's grade: 'a clean kill', 'a good kill', 'a rough kill' or 'a ragged kill' (text) | any |
| `quantity` | kinds of goods taken from it (number) |  |

tile = where the animal fell.

```json
{"id": "when-hunted", "on": "hunted", "actions": [{"do": "note", "text": "An animal is killed in a hunt"}]}
```

<a id="event-fight-ends"></a>**`fight ends`**: A fight ends

A battle (a fight or a hunt) is over.

| Field | Holds | Left out means |
|---|---|---|
| `cell` | where the fight was (place) | anywhere |
| `detail` | the battle's id (text) | any |

No actor or target.

```json
{"id": "when-fight-ends", "on": "fight ends", "actions": [{"do": "note", "text": "A fight ends"}]}
```

<a id="event-death"></a>**`death`**: Someone dies (has a tile)

A character dies: killed in a fight, of old age, or by the Dungeon Master.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who died (character) | anyone |
| `cell` | where they died (place) | anywhere |

Old age also records 'old age' straight after.

```json
{"id": "when-death", "on": "death", "actions": [{"do": "note", "text": "Someone dies"}]}
```

<a id="event-revival"></a>**`revival`**: Someone is brought back (has a tile)

A dead character is raised again (a Dungeon Master or developer action).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who is revived (character) | anyone |
| `cell` | where they lie (place) | anywhere |

```json
{"id": "when-revival", "on": "revival", "actions": [{"do": "note", "text": "Someone is brought back"}]}
```

<a id="event-hunt"></a>**`hunt`**: A hunt begins (mission evidence, has a tile)

A wolf starts a hunt: animals are lined up in a hunting battle.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the hunter who started it (wolf) | any wolf |
| `cell` | where (place) | anywhere |
| `detail` | the battle's id (text) | any |
| `quantity` | animals in the hunt (number) |  |

```json
{"id": "when-hunt", "on": "hunt", "actions": [{"do": "note", "text": "A hunt begins"}]}
```

<a id="event-huntrole"></a>**`huntRole`**: A part played in a hunt (mission evidence, has a tile)

When a hunt ends, one event for each part each wolf played; also when a wolf is given another's share to carry.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who played it (character) | anyone |
| `target` | the role: tracker, driver, ambusher, carrier... (text) | any |
| `cell` | where the hunt was (place) | anywhere |
| `detail` | the hunt's battle id (text) | any |

```json
{"id": "when-hunt-ole", "on": "huntRole", "actions": [{"do": "note", "text": "A part played in a hunt"}]}
```

<a id="event-assault"></a>**`assault`**: A resident is attacked (has a tile)

A wolf starts a fight (not a spar) with a resident: a crime the watch may hear of.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the attacker (wolf) | any wolf |
| `target` | the one attacked (resident) | any resident |
| `cell` | where (place) | anywhere |
| `detail` | the incident's id (text) | any |

'fight' is recorded too.

```json
{"id": "when-assault", "on": "assault", "match": {"target": "rm111"}, "actions": [{"do": "note", "text": "A resident is attacked"}]}
```

<a id="event-downed"></a>**`downed`**: Someone goes down in a fight (mission evidence, has a tile)

A fighter (or an animal in a hunt) is knocked down in a battle.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who downed them (character) | anyone |
| `target` | who went down (character) | anyone |
| `cell` | where the fight is (place) | anywhere |
| `detail` | the battle's id (text) | any |

tile = where the fallen one lies in the fight, not the actor's. A bandit going down also records 'bandit falls'.

```json
{"id": "when-downed", "on": "downed", "actions": [{"do": "note", "text": "Someone goes down in a fight"}]}
```

<a id="event-fight"></a>**`fight`**: A fight starts (has a tile)

A fight begins: a character attacks another, or bandits rush or attack a wolf on the road.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the attacker (or the bandit camp's id when bandits start it) (character) | anyone |
| `target` | the one attacked (character) | anyone |
| `cell` | where (place) | anywhere |
| `detail` | the battle's id, or 'bandits rush' / 'bandits attack' (text) | any |

tile only when a character attacks (none for bandits).

```json
{"id": "when-fight", "on": "fight", "actions": [{"do": "note", "text": "A fight starts"}]}
```

<a id="event-mourning"></a>**`mourning`**: A resident mourns (has a tile)

Someone dies and each resident close to them (family, or a good friend) begins to mourn.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the mourner (resident) | any resident |
| `target` | who died (character) | anyone |
| `cell` | where the mourner is (place) | anywhere |
| `detail` | 'family' or 'friend' (text) | any |

```json
{"id": "when-mourning", "on": "mourning", "actions": [{"do": "note", "text": "A resident mourns"}]}
```

<a id="event-old-age"></a>**`old age`**: A resident dies of age (has a tile)

A resident dies by the daily chance of its age (after 'death').

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who died (resident) | any resident |
| `cell` | where (place) | anywhere |
| `detail` | 'died of old age' (60 or older) or 'died' (text) | any |

```json
{"id": "when-old-age", "on": "old age", "actions": [{"do": "note", "text": "A resident dies of age"}]}
```

<a id="event-wolf-downed"></a>**`wolf downed`**: A wolf goes down (watched, has a tile)

One of the world's wolves was knocked down (in a fight, or beaten by bandits). Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | who downed them (character) | anyone |
| `cell` | where (place) | anywhere |
| `detail` | the battle's id (text) | any |

```json
{"id": "when-wolf-downed", "on": "wolf downed", "actions": [{"do": "note", "text": "A wolf goes down"}]}
```

<a id="event-wolf-revived"></a>**`wolf revived`**: A wolf is back on their feet (watched, has a tile)

A downed wolf got up, or was tended back to their feet (target: who tended them, if anyone). Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | who tended them (character) | nobody, or anyone |

```json
{"id": "when-wolf-revived", "on": "wolf revived", "actions": [{"do": "note", "text": "A wolf is back on their feet"}]}
```

<a id="event-injury-healed"></a>**`injury healed`**: A wolf's injury heals (watched, has a tile)

An injury of a wolf healed with rest: the injury in words. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `detail` | the injury, in words (text) | any injury |

```json
{"id": "when-injury-healed", "on": "injury healed", "actions": [{"do": "note", "text": "A wolf's injury heals"}]}
```

#### Bandits and the roads

<a id="event-raid"></a>**`raid`**: Bandits rob a wagon

Bandits camped on a road waylay a passing wagon and take its cargo.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the bandit camp (its id, e.g. camp_<cell>) (text) | any |
| `target` | the market the wagon was going to (market) | any market |
| `cell` | where the raid happened (place) | anywhere |
| `item` | the wagon's departure id (text) | any |
| `detail` | 'the wagon to <market> was robbed' (text) | any |
| `quantity` | pieces of cargo taken (number) |  |

```json
{"id": "when-raid", "on": "raid", "actions": [{"do": "note", "text": "Bandits rob a wagon"}]}
```

<a id="event-bandits-gather"></a>**`bandits gather`**: Bandits make camp on a road

Once a game week a new bandit camp may form in wild country along a road (never near a watch post).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the new camp (its id, e.g. camp_<cell>) (text) | any |
| `target` | the market at the road's far end (market) | any market |
| `cell` | where they camp (place) | anywhere |
| `detail` | 'on the road to <market>' (text) | any |

```json
{"id": "when-bandits-gather", "on": "bandits gather", "actions": [{"do": "note", "text": "Bandits make camp on a road"}]}
```

<a id="event-camp-cleared"></a>**`camp cleared`**: A bandit camp is broken (has a tile)

A bandit camp is broken: by whoever beat its last bandits, or by a town's watch on patrol.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who cleared it (a character; the town's id when the watch did it) (character) | anyone |
| `target` | the bandit camp (its id, e.g. camp_<cell>) (text) | any |
| `cell` | the camp's cell (place) | anywhere |
| `detail` | 'by <name>' or 'by the watch' (text) | any |

tile only when a character cleared it and stands in that cell.

```json
{"id": "when-camp-cleared", "on": "camp cleared", "actions": [{"do": "note", "text": "A bandit camp is broken"}]}
```

<a id="event-bandits-scattered"></a>**`bandits scattered`**: Bandits starve and scatter

A bandit camp grown too hungry and weak breaks up on its own.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the bandit camp (its id, e.g. camp_<cell>) (text) | any |
| `cell` | the camp's cell (place) | anywhere |
| `detail` | 'starved out' (text) | any |

```json
{"id": "when-bandits-scattered", "on": "bandits scattered", "actions": [{"do": "note", "text": "Bandits starve and scatter"}]}
```

<a id="event-patrol"></a>**`patrol`**: The watch goes after bandits

A town with a bounty out on a camp sends its watch after the bandits (each game day, by chance).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the town whose watch went out (town) | any town |
| `target` | the bandit camp (its id, e.g. camp_<cell>) (text) | any |
| `cell` | the camp's cell (place) | anywhere |
| `detail` | 'the watch went out after the bandits' (text) | any |

```json
{"id": "when-patrol", "on": "patrol", "actions": [{"do": "note", "text": "The watch goes after bandits"}]}
```

<a id="event-carter-robbed"></a>**`carter robbed`**: A carter loses the purse

A wagon is raided and its carter, not hurt, is robbed of the purse.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the carter (resident) | any resident |
| `target` | the bandit camp (its id, e.g. camp_<cell>) (text) | any |
| `cell` | where the raid happened (place) | anywhere |
| `detail` | 'robbed of the purse in the raid' (text) | any |

Logged with a raid, as the other half of 'carter hurt'.

```json
{"id": "when-carter-robbed", "on": "carter robbed", "actions": [{"do": "note", "text": "A carter loses the purse"}]}
```

<a id="event-paid-off-bandits"></a>**`paid off bandits`**: Someone pays off bandits (mission evidence, has a tile)

A wolf stopped by bandits on the road pays what they demand (half the purse, if already fighting).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the one who paid (wolf) | any wolf |
| `target` | the bandit camp (its id, e.g. camp_<cell>) (text) | any |
| `cell` | where (place) | anywhere |
| `detail` | 'on the road' (text) | any |
| `coins` | what was paid (number) |  |

```json
{"id": "when-paid-off-bandits", "on": "paid off bandits", "actions": [{"do": "note", "text": "Someone pays off bandits"}]}
```

<a id="event-bandit-falls"></a>**`bandit falls`**: A bandit falls (mission evidence, has a tile)

A bandit is downed in a fight.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who landed the last blow (character) | anyone |
| `target` | the bandit's camp (its id, e.g. camp_<cell>) (text) | any |
| `cell` | where the fight is (place) | anywhere |
| `detail` | the bandit's name (text) | any |

tile = where the bandit fell. Doc 85's table leaves target blank, but the code fills it with the camp's id.

```json
{"id": "when-bandit-falls", "on": "bandit falls", "actions": [{"do": "note", "text": "A bandit falls"}]}
```

<a id="event-bandits-called"></a>**`bandits called`**: Bandits are called up (has a tile)

The Dungeon Master sets a bandit camp near a character, or a story effect sets one in a story area.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | always 'dungeon master' (text) | any |
| `target` | the character it gathers near (DM), or the new camp's id (a story's) (text) | any |
| `cell` | where the camp is (place) | anywhere |
| `detail` | the new camp's id (text) | any |
| `quantity` | how many bandits (number) |  |

tile = the camp's spot.

```json
{"id": "when-bandits-called", "on": "bandits called", "actions": [{"do": "note", "text": "Bandits are called up"}]}
```

<a id="event-bandits-creep"></a>**`bandits creep`**: Bandits creep up on someone

Bandits near a wolf with coins sneak toward it unseen (nobody notices them).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the bandit camp (its id, e.g. camp_<cell>) (text) | any |
| `target` | the wolf they stalk (wolf) | any wolf |
| `cell` | the camp's cell (place) | anywhere |
| `detail` | 'on the road' (text) | any |

```json
{"id": "when-bandits-creep", "on": "bandits creep", "actions": [{"do": "note", "text": "Bandits creep up on someone"}]}
```

<a id="event-bandits-demand"></a>**`bandits demand`**: Bandits demand a purse

Bandits step out around a wolf on the road and demand coins (pay, fight or get clear).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the bandit camp (its id, e.g. camp_<cell>) (text) | any |
| `target` | the wolf stopped (wolf) | any wolf |
| `cell` | the camp's cell (place) | anywhere |
| `detail` | 'on the road' (text) | any |
| `coins` | what they demand (number) |  |

```json
{"id": "when-bandits-demand", "on": "bandits demand", "actions": [{"do": "note", "text": "Bandits demand a purse"}]}
```

<a id="event-carter-hurt"></a>**`carter hurt`**: A carter is hurt in a raid

A wagon is raided and its carter is bruised (half the time; else 'carter robbed').

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the carter (resident) | any resident |
| `target` | the bandit camp (its id, e.g. camp_<cell>) (text) | any |
| `cell` | where the raid happened (place) | anywhere |
| `detail` | 'bruised in the raid' (text) | any |

```json
{"id": "when-carter-hurt", "on": "carter hurt", "actions": [{"do": "note", "text": "A carter is hurt in a raid"}]}
```

<a id="event-cleared"></a>**`cleared`**: A fallen spawned resident is cleared

A spawn rule clears away one of its spawned residents who has fallen, before spawning another.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the fallen resident (resident) | any resident |
| `detail` | 'spawn rule <rule id>' (text) | any |

Nothing to do with bandits, despite doc 85's grouping: it comes from DM spawn rules (RatwGame.cpp:2344).

```json
{"id": "when-cleared", "on": "cleared", "actions": [{"do": "note", "text": "A fallen spawned resident is cleared"}]}
```

<a id="event-robbed"></a>**`robbed`**: Bandits beat and rob a wolf

Bandits beat a wolf to the ground in a road fight and take half its purse.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the bandit camp (its id, e.g. camp_<cell>) (text) | any |
| `target` | the wolf robbed (wolf) | any wolf |
| `cell` | the camp's cell (place) | anywhere |
| `detail` | 'beaten and robbed on the road' (text) | any |
| `coins` | coins taken (number) |  |

```json
{"id": "when-robbed", "on": "robbed", "actions": [{"do": "note", "text": "Bandits beat and rob a wolf"}]}
```

#### Caravans, wagons, carts

<a id="event-caravan-arrives"></a>**`caravan arrives`**: A wagon arrives at a market

A wagon between markets reaches the far market and unloads.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wagon's id (cv<n>) (text) | any |
| `target` | the market it reached (market) | any market |
| `cell` | where it unloads (place) | anywhere |
| `item` | its departure id (text) | any |
| `detail` | 'from <market id>' and ', guarded' when escorted (text) | any |
| `quantity` | kinds of goods it carried (number) |  |

```json
{"id": "when-caravan-arrives", "on": "caravan arrives", "actions": [{"do": "note", "text": "A wagon arrives at a market"}]}
```

<a id="event-wagon-overdue"></a>**`wagon overdue`**: A wagon is overdue

A wagon on the road is more than a day past due at its far market.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wagon's id (cv<n>) (text) | any |
| `target` | the market it is going to (market) | any market |
| `cell` | where the wagon is (place) | anywhere |
| `item` | its departure id (text) | any |
| `detail` | 'Overdue: the wagon from <market>' and its cause, or 'No word of it yet.' (text) | any |

```json
{"id": "when-wagon-overdue", "on": "wagon overdue", "actions": [{"do": "note", "text": "A wagon is overdue"}]}
```

<a id="event-wagon-held-up"></a>**`wagon held up`**: A wagon is held up

A wagon meets a seeded mishap on the road and stops for a day or two.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wagon's id (cv<n>) (text) | any |
| `target` | the market it is going to (market) | any market |
| `cell` | where it is held up (place) | anywhere |
| `item` | its departure id (text) | any |
| `detail` | the cause: 'a broken axle', 'mud to the axles', 'a wheel off in a rut', 'a washed-out ford' (text) | any |

```json
{"id": "when-wagon-held-up", "on": "wagon held up", "actions": [{"do": "note", "text": "A wagon is held up"}]}
```

<a id="event-caravan-lost"></a>**`caravan lost`**: A wagon can go no further

A wagon finds no road on from where it is and stops for good.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wagon's id (cv<n>) (text) | any |
| `target` | the market it was going to (market) | any market |
| `cell` | where it stopped (place) | anywhere |
| `item` | its departure id (text) | any |
| `detail` | 'no road on from here' (text) | any |

```json
{"id": "when-caravan-lost", "on": "caravan lost", "actions": [{"do": "note", "text": "A wagon can go no further"}]}
```

<a id="event-caravan-departs"></a>**`caravan departs`**: A wagon sets out

A committed wagon departure loads and makes ready to leave its market (it waits a little for its escorts).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wagon's id (cv<n>) (text) | any |
| `target` | the market it is going to (market) | any market |
| `cell` | where it loads (place) | anywhere |
| `item` | its departure id (text) | any |
| `detail` | '<n> guards', and ', <carter> driving' (text) | any |
| `quantity` | kinds of goods it carries (number) |  |

```json
{"id": "when-caravan-departs", "on": "caravan departs", "actions": [{"do": "note", "text": "A wagon sets out"}]}
```

<a id="event-caravan-dropped"></a>**`caravan dropped`**: A wagon is dropped

A wagon's market no longer exists (a host's edit), so the wagon is taken off the road.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wagon's id (cv<n>) (text) | any |
| `target` | the market it was going to (market) | any market |
| `cell` | where it was (place) | anywhere |
| `item` | its departure id (text) | any |
| `detail` | 'its market is gone' (text) | any |

```json
{"id": "when-caravan-dropped", "on": "caravan dropped", "actions": [{"do": "note", "text": "A wagon is dropped"}]}
```

<a id="event-caravan-home"></a>**`caravan home`**: A wagon comes home

A wagon gets back to the market it set out from; its carter returns to his post.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wagon's id (cv<n>) (text) | any |
| `target` | its home market (market) | any market |
| `cell` | where it stops (place) | anywhere |
| `item` | its departure id (text) | any |
| `detail` | 'back from <market id>' (text) | any |

```json
{"id": "when-caravan-home", "on": "caravan home", "actions": [{"do": "note", "text": "A wagon comes home"}]}
```

<a id="event-caravan-passes"></a>**`caravan passes`**: A wagon gets past bandits

A wagon passes a bandit camp and its guards keep the bandits off.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wagon's id (cv<n>) (text) | any |
| `target` | the bandit camp (its id, e.g. camp_<cell>) (text) | any |
| `cell` | where (place) | anywhere |
| `detail` | 'the guards kept the bandits off' (text) | any |

```json
{"id": "when-caravan-passes", "on": "caravan passes", "actions": [{"do": "note", "text": "A wagon gets past bandits"}]}
```

<a id="event-cart-arrives"></a>**`cart arrives`**: A farm cart arrives

A farm's cart reaches its market; its goods go on the shelves.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the cart's id (cv<n>) (text) | any |
| `target` | the market (market) | any market |
| `cell` | where it unloads (place) | anywhere |
| `item` | the farm's id (text) | any |
| `detail` | 'from <farm id>' (text) | any |
| `quantity` | kinds of goods (number) |  |

```json
{"id": "when-cart-arrives", "on": "cart arrives", "actions": [{"do": "note", "text": "A farm cart arrives"}]}
```

<a id="event-cart-departs"></a>**`cart departs`**: A farm cart sets out

A farm's morning cart leaves the farm for its market.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the cart's id (cv<n>) (text) | any |
| `target` | the market (market) | any market |
| `cell` | the farm, where it loads (place) | anywhere |
| `item` | the farm's id (text) | any |
| `detail` | the farm's name (text) | any |
| `quantity` | kinds of goods (number) |  |

```json
{"id": "when-cart-departs", "on": "cart departs", "actions": [{"do": "note", "text": "A farm cart sets out"}]}
```

<a id="event-cart-missed"></a>**`cart missed`**: A farm cart doesn't come

A farm's cart was due but nobody at the farm could drive it, or there was no way through.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the cart departure's id (text) | any |
| `target` | the market (market) | any market |
| `cell` | the farm (place) | anywhere |
| `item` | the farm's id (text) | any |
| `detail` | why, in words (text) | any |
| `quantity` | kinds of goods it would have brought (number) |  |

```json
{"id": "when-cart-missed", "on": "cart missed", "actions": [{"do": "note", "text": "A farm cart doesn't come"}]}
```

<a id="event-cart-stocks-the-stalls"></a>**`cart stocks the stalls`**: A cart stocks the Marketday stalls

A farm's cart arrives on Marketday and fills the market's stalls.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the cart's id (cv<n>) (text) | any |
| `target` | the market (market) | any market |
| `cell` | where it unloads (place) | anywhere |
| `item` | the farm's id (text) | any |
| `detail` | 'Marketday' (text) | any |
| `quantity` | kinds of goods (number) |  |

```json
{"id": "when-cart-stocks-the-stalls", "on": "cart stocks the stalls", "actions": [{"do": "note", "text": "A cart stocks the Marketday stalls"}]}
```

<a id="event-wagon-cancelled"></a>**`wagon cancelled`**: A wagon departure is cancelled

A committed wagon (or cart) departure can't go: its market is gone, the road is closed, or no carter for too long.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the departure's id (w<n>) (text) | any |
| `target` | the market it is for (market) | any market |
| `cell` | the setting-out market's place (empty when that market is gone) (place) | anywhere |
| `item` | where it was to set out from (a market, or a farm's id for a cart) (text) | any |
| `detail` | why, in words (text) | any |

```json
{"id": "when-wagon-cancelled", "on": "wagon cancelled", "actions": [{"do": "note", "text": "A wagon departure is cancelled"}]}
```

<a id="event-wagon-delayed"></a>**`wagon delayed`**: A wagon departure is put off

A committed wagon has no carter or no room on the roads, and goes a day later.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the departure's id (w<n>) (text) | any |
| `target` | the market it is for (market) | any market |
| `cell` | the setting-out market's place (place) | anywhere |
| `item` | the market it sets out from (market) | any market |
| `detail` | the new notices (text) | any |

```json
{"id": "when-wagon-delayed", "on": "wagon delayed", "actions": [{"do": "note", "text": "A wagon departure is put off"}]}
```

<a id="event-wagon-posted"></a>**`wagon posted`**: A wagon departure is posted

The staged economy commits a wagon between two markets and posts its notices.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the departure's id (w<n>) (text) | any |
| `target` | the market it is for (market) | any market |
| `cell` | the setting-out market's place (place) | anywhere |
| `item` | the market it sets out from (market) | any market |
| `detail` | the notices (text) | any |
| `quantity` | kinds of goods it will carry (number) |  |

```json
{"id": "when-wagon-posted", "on": "wagon posted", "actions": [{"do": "note", "text": "A wagon departure is posted"}]}
```

#### Ships

<a id="event-ship-docks"></a>**`ship docks`**: A ship comes in

A ship on the timetable comes in to its port with its cargo.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the ship (ship) | any ship |
| `target` | its port's market (market) | any market |
| `cell` | the port (place) | anywhere |
| `item` | the ship's name (text) | any |
| `detail` | the words posted about it (text) | any |
| `quantity` | kinds of goods it brings (number) |  |

```json
{"id": "when-ship-docks", "on": "ship docks", "actions": [{"do": "note", "text": "A ship comes in"}]}
```

<a id="event-ship-lost"></a>**`ship lost`**: A ship is lost at sea

A ship due at a port is lost with all its cargo (by chance or by the Dungeon Master).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the ship (ship) | any ship |
| `target` | its port's market (market) | any market |
| `cell` | the port (place) | anywhere |
| `item` | the ship's name (text) | any |
| `detail` | the words posted about it (text) | any |
| `quantity` | kinds of goods it was bringing (number) |  |

```json
{"id": "when-ship-lost", "on": "ship lost", "actions": [{"do": "note", "text": "A ship is lost at sea"}]}
```

<a id="event-ship-sails"></a>**`ship sails`**: A ship sails

A docked ship leaves its port, taking goods with it.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the ship (ship) | any ship |
| `target` | its port's market (market) | any market |
| `cell` | the port (place) | anywhere |
| `item` | the ship's name (text) | any |
| `detail` | the words posted about it (text) | any |
| `quantity` | pieces of goods it took on (number) |  |

```json
{"id": "when-ship-sails", "on": "ship sails", "actions": [{"do": "note", "text": "A ship sails"}]}
```

<a id="event-ship-stormbound"></a>**`ship stormbound`**: A storm keeps a ship out

A storm keeps a ship out of its port (by chance or by the Dungeon Master); what it brings grows dearer.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the ship (ship) | any ship |
| `target` | its port's market (market) | any market |
| `cell` | the port (place) | anywhere |
| `item` | the ship's name (text) | any |
| `detail` | the words posted about it (text) | any |
| `quantity` | kinds of goods it was bringing (number) |  |

```json
{"id": "when-ship-stormbound", "on": "ship stormbound", "actions": [{"do": "note", "text": "A storm keeps a ship out"}]}
```

<a id="event-ship-posted"></a>**`ship posted`**: A ship is put on the timetable

The wharf's timetable gains a ship due at a port.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the ship (ship) | any ship |
| `target` | its port's market (market) | any market |
| `cell` | the port (place) | anywhere |
| `item` | the ship's name (text) | any |
| `detail` | the words posted about it (text) | any |
| `quantity` | kinds of goods it will bring (number) |  |

detail is '<Name>, due <weekday>.'.

```json
{"id": "when-ship-posted", "on": "ship posted", "actions": [{"do": "note", "text": "A ship is put on the timetable"}]}
```

<a id="event-ships-cleared"></a>**`ships cleared`**: A port's ship notice comes down

A port's ships notice comes down: no ships due any more, or its port is gone.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the port's market (market) | any market |
| `target` | the same market (market) | any market |
| `cell` | the port (empty when it is gone) (place) | anywhere |
| `detail` | empty, or 'its port is gone' (text) | any |

Its twin 'ships posted' (the notice put up) is not listed.

```json
{"id": "when-ships-cleared", "on": "ships cleared", "actions": [{"do": "note", "text": "A port's ship notice comes down"}]}
```

#### Money, work and markets

<a id="event-economy"></a>**`economy`**: Money or goods change hands (has a tile)

Every move between accounts in the economy's ledger: sales, wages, rent, fees, gifts, prizes, fines, upkeep. Very frequent.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the account paying or handing over (a character, or an account such as treasury, stores:<town>, till:<post>, contract:<id>) (character) | anyone |
| `target` | the account receiving (character) | anyone |
| `cell` | where the payer is, when it is a character (place) | anywhere |
| `item` | the goods moved (empty for coins only) (item) | anything |
| `detail` | the ledger's reason ('a gift', 'rent', 'stall sale', 'contract reward', 'fine'...) (text) | any |
| `quantity` | how many goods (number) |  |
| `coins` | coins moved (number) |  |

Logged from the society's journal (World::absorbJournal). Accounts are not all characters; tile only when the payer is a character in the world. item may be a marked id (base~marks).

```json
{"id": "when-economy", "on": "economy", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "Money or goods change hands"}]}
```

<a id="event-contract-done"></a>**`contract done`**: A contract is fulfilled (mission evidence, has a tile)

A contract (bounty, escort, courier, supply...) is settled as done: its reward paid to the one who did it, or returned to the poster when nobody took it (the watch cleared the camp, the carters carried it).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who did it and was paid (or the poster, when the reward went back) (character) | anyone |
| `target` | what it was for: a bounty's camp, a courier's recipient, an escort's or supply's town (text) | any |
| `cell` | where the doer is (place) | anywhere |
| `detail` | '<contract kind>: <its description>', e.g. 'bounty: ...' (text) | any |
| `coins` | what was paid (or the whole reward returned) (number) |  |

Written as 'contract ' + status; 'contract expired' is the other status and is not listed. Filter detail by its 'courier:', 'escort:' or 'bounty:' prefix only with exact text.

```json
{"id": "when-contract-done", "on": "contract done", "actions": [{"do": "note", "text": "A contract is fulfilled"}]}
```

<a id="event-archive-work"></a>**`archive work`**: Someone does archive work (mission evidence, has a tile)

A wolf does a task in a town's archive (copying, sorting records...) and is paid by the town.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the worker (wolf) | any wolf |
| `cell` | the archive (place) | anywhere |
| `item` | the task: copy or sort (text) | any |
| `detail` | the town whose archive it is (town) | any town |
| `coins` | what it was paid (number) |  |

```json
{"id": "when-archive-work", "on": "archive work", "actions": [{"do": "note", "text": "Someone does archive work"}]}
```

<a id="event-contract-taken"></a>**`contract taken`**: Someone takes on a contract (mission evidence, has a tile)

A wolf takes on a posted contract (at a board, by letter), or a resident or town guard is given one (a guard taking an escort).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who took it (character) | anyone |
| `target` | who posted it (a character or an account such as stores:<town>) (character) | anyone |
| `cell` | where the taker is (place) | anywhere |
| `detail` | '<contract kind>: <its description>' (text) | any |
| `coins` | the reward on offer (number) |  |

```json
{"id": "when-contract-taken", "on": "contract taken", "actions": [{"do": "note", "text": "Someone takes on a contract"}]}
```

<a id="event-contract-posted"></a>**`contract posted`**: A contract is posted

Someone posts a contract with its reward held in trust: a town's escort for a wagon, a bounty on a camp, a courier's letter, a project's work.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who posted it (a character or a town's store/treasury account) (character) | anyone |
| `target` | what it is for: a bounty's camp, a courier's recipient, an escort's or supply's town (text) | any |
| `cell` | where the poster is, when it is a character (place) | anywhere |
| `detail` | '<contract kind>: <its description>' (text) | any |
| `coins` | the reward set aside (number) |  |

cell and tile only when the poster is a character in the world.

```json
{"id": "when-contract-posted", "on": "contract posted", "actions": [{"do": "note", "text": "A contract is posted"}]}
```

<a id="event-lease"></a>**`lease`**: Someone rents lodging (mission evidence, has a tile)

A wolf rents a bed, a room upstairs at an inn, a lodger's place or a home's place.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the renter (wolf) | any wolf |
| `target` | the landlord (a resident, or 'treasury') (character) | anyone |
| `cell` | the lodging's place (place) | anywhere |
| `item` | the kind: bed, night, lodger, place (text) | any |
| `detail` | the lodging's id (text) | any |
| `coins` | the rent paid (number) |  |

```json
{"id": "when-lease", "on": "lease", "actions": [{"do": "note", "text": "Someone rents lodging"}]}
```

<a id="event-lease-ended"></a>**`lease ended`**: A lodging ends (mission evidence, has a tile)

A lodging runs out, is given up, can't be paid for, or ends because the place is held for a story.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the renter (wolf) | any wolf |
| `target` | the landlord (a resident, or 'treasury') (character) | anyone |
| `cell` | the lodging's place (place) | anywhere |
| `item` | the kind: bed, night, lodger, place (text) | any |
| `detail` | the lodging's id (text) | any |
| `coins` | rent returned (number) |  |

tile only when the renter is in that place.

```json
{"id": "when-lease-ended", "on": "lease ended", "actions": [{"do": "note", "text": "A lodging ends"}]}
```

<a id="event-stall-rented"></a>**`stall rented`**: A market stall is rented (mission evidence, has a tile)

A wolf rents a stall at a market for the day.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the renter (wolf) | any wolf |
| `cell` | the market (place) | anywhere |
| `detail` | the stall's id (text) | any |
| `coins` | the fee (number) |  |

```json
{"id": "when-stall-rented", "on": "stall rented", "actions": [{"do": "note", "text": "A market stall is rented"}]}
```

<a id="event-stall-sale"></a>**`stall sale`**: Something is bought at a stall (mission evidence, has a tile)

Someone buys goods from a wolf's market stall.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the buyer (character) | anyone |
| `target` | the stall's keeper (wolf) | any wolf |
| `cell` | the market (place) | anywhere |
| `item` | what was bought (item) | anything |
| `detail` | the stall's id (text) | any |
| `quantity` | how many (number) |  |
| `coins` | what was paid (number) |  |

item may be a marked id.

```json
{"id": "when-stall-sale", "on": "stall sale", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "Something is bought at a stall"}]}
```

<a id="event-harvest-over"></a>**`harvest over`**: A harvest override ends

The Dungeon Master clears a harvest override on an area (a poor, fair or good season called earlier).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the harvest area (area) | any area |

```json
{"id": "when-harvest-over", "on": "harvest over", "actions": [{"do": "note", "text": "A harvest override ends"}]}
```

<a id="event-contract-given-back"></a>**`contract given back`**: A contract is given back (has a tile)

A wolf gives up a contract it had taken; it is open again.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who gave it back (wolf) | any wolf |
| `target` | who posted it (character) | anyone |
| `cell` | where the wolf is (place) | anywhere |
| `detail` | '<contract kind>: <its description>' (text) | any |
| `coins` | the reward on offer (number) |  |

```json
{"id": "when-contract-given-back", "on": "contract given back", "actions": [{"do": "note", "text": "A contract is given back"}]}
```

<a id="event-harvest-called"></a>**`harvest called`**: A season's harvest is called

The Dungeon Master calls a poor, fair or good season for a harvest area; posted on the boards of its markets (one event per market).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the harvest area (area) | any area |
| `target` | a market it is posted at (market) | any market |
| `cell` | that market's place (place) | anywhere |
| `detail` | the notice, e.g. 'A good season for wheat and oats.' (text) | any |
| `quantity` | the season: 0 poor, 1 fair, 2 good (number) |  |

```json
{"id": "when-harvest-called", "on": "harvest called", "actions": [{"do": "note", "text": "A season's harvest is called"}]}
```

<a id="event-nudge-over"></a>**`nudge over`**: A Times nudge lapses

A Dungeon Master's nudge to a market's Times runs out or is cleared.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the market (market) | any market |
| `target` | the market (same) (market) | any market |
| `cell` | the market's place (place) | anywhere |

```json
{"id": "when-nudge-over", "on": "nudge over", "actions": [{"do": "note", "text": "A Times nudge lapses"}]}
```

<a id="event-prices-reset"></a>**`prices reset`**: A market's prices are reset

The Dungeon Master resets the prices of one market or all (one event per market).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the market (market) | any market |
| `target` | the market (same) (market) | any market |
| `cell` | the market's place (place) | anywhere |
| `detail` | the words given, default 'fresh stock everywhere' (text) | any |

```json
{"id": "when-prices-reset", "on": "prices reset", "actions": [{"do": "note", "text": "A market's prices are reset"}]}
```

<a id="event-stall-closed"></a>**`stall closed`**: A market stall closes (has a tile)

A rented stall closes at the day's end (or is given up); its goods go back to the keeper.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the keeper (wolf) | any wolf |
| `cell` | the market (place) | anywhere |
| `detail` | the stall's id (text) | any |
| `coins` | the day's takings (number) |  |

tile only when the keeper is still in that place.

```json
{"id": "when-stall-closed", "on": "stall closed", "actions": [{"do": "note", "text": "A market stall closes"}]}
```

<a id="event-times-cause"></a>**`times cause`**: The Times' cause is told

The economy's slider moves and its cause is told once in every market (a notice and a crier's line).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the market (market) | any market |
| `target` | the same market (market) | any market |
| `cell` | the market's place (place) | anywhere |
| `item` | 'up' or 'down' (text) | any |
| `detail` | the notice (text) | any |
| `quantity` | the cause's strength (number) |  |

```json
{"id": "when-times-cause", "on": "times cause", "actions": [{"do": "note", "text": "The Times' cause is told"}]}
```

<a id="event-times-nudged"></a>**`times nudged`**: Trade at a market is nudged

The Dungeon Master nudges a market's Times up or down for some days; posted once.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the market (market) | any market |
| `target` | the same market (market) | any market |
| `cell` | the market's place (place) | anywhere |
| `detail` | the notice ('Trade is brisk in ...' or the cause) (text) | any |
| `quantity` | the nudge, -100 to 100 (number) |  |

```json
{"id": "when-times-nudged", "on": "times nudged", "actions": [{"do": "note", "text": "Trade at a market is nudged"}]}
```

<a id="event-trade-done"></a>**`trade done`**: A wolf buys or sells (watched, mission evidence, has a tile)

A wolf bought from or sold to a trader (a shop, a stall's keeper): detail is bought or sold, coins what changed hands. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the trader (resident) | any trader |
| `item` | the item (item) | anything |
| `detail` | bought or sold (text) | any |
| `quantity` | how many (number) | any |
| `coins` | the pennies paid or got (number) | any |

```json
{"id": "when-trade-done", "on": "trade done", "match": {"target": "rm111"}, "actions": [{"do": "note", "text": "A wolf buys or sells"}]}
```

<a id="event-purse-crossed"></a>**`purse crossed`**: A purse crosses an amount (watched)

A town's treasury, a business's till or a Chapter's purse (as a reward names one: town:<town>, till:job:<resident>, chapter:<id>) goes up or down past an amount the trigger names in its detail (quantity: what it holds now). Name the purse and the amount. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `target` | the purse (purse) | the purse named |
| `detail` | the amount crossed, in pennies (text) | the amount named |
| `item` | which way: up or down (text) | either way |
| `quantity` | what the purse holds now (number) | any |

A trigger on it must name `target` and `detail` in its filters.

Looked at every two seconds while a story listens; the first look makes no event.

```json
{"id": "when-purse-crossed", "on": "purse crossed", "match": {"target": "town:greyfen", "detail": "500"}, "actions": [{"do": "note", "text": "A purse crosses an amount"}]}
```

#### Letters and notices

<a id="event-letter-sent"></a>**`letter sent`**: A letter is sent (mission evidence, has a tile)

A wolf hands a letter to the courier (paying the fee, perhaps with coins or goods enclosed).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the writer (wolf) | any wolf |
| `target` | the recipient (empty when it goes by a courier contract) (character) | anyone |
| `cell` | where the writer is (place) | anywhere |
| `detail` | the letter's id (letter) | any letter |
| `coins` | the courier's fee (number) |  |

An enclosure is not in the event; see 'letter returned'.

```json
{"id": "when-letter-sent", "on": "letter sent", "actions": [{"do": "note", "text": "A letter is sent"}]}
```

<a id="event-letter-delivered"></a>**`letter delivered`**: A letter is delivered (mission evidence, has a tile)

A letter reaches its reader, in their post town or at one of its inns.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the reader (wolf) | any wolf |
| `cell` | where the reader is (empty when it waits at an inn) (place) | anywhere |
| `detail` | the letter's id (letter) | any letter |

cell and tile only when the reader is in the world.

```json
{"id": "when-letter-delivered", "on": "letter delivered", "actions": [{"do": "note", "text": "A letter is delivered"}]}
```

<a id="event-notice-answered"></a>**`notice answered`**: A resident answers a notice (mission evidence, has a tile)

A resident answers a wolf's notice when it is posted: a seller or buyer for an item, a master for an apprenticeship.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the resident who answered (resident) | any resident |
| `target` | the notice's author (wolf) | any wolf |
| `cell` | where the resident is (place) | anywhere |
| `item` | what the notice is about: 'item:<item id>', 'apprenticeship:<trade>', 'room', 'partner' (text) | any |
| `detail` | the notice's id (notice) | any notice |

```json
{"id": "when-notice-answered", "on": "notice answered", "actions": [{"do": "note", "text": "A resident answers a notice"}]}
```

<a id="event-notice-posted"></a>**`notice posted`**: A notice is posted (mission evidence, has a tile)

A wolf pins a notice on a board (seeking, offering, an event, lost, other) and pays the fee.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the author (wolf) | any wolf |
| `cell` | where the author is (place) | anywhere |
| `item` | what it is about: 'item:<item id>', 'apprenticeship:<trade>', 'room', 'partner', or empty (text) | any |
| `detail` | the notice's id (notice) | any notice |
| `coins` | the fee (number) |  |

```json
{"id": "when-notice-posted", "on": "notice posted", "actions": [{"do": "note", "text": "A notice is posted"}]}
```

<a id="event-crier"></a>**`crier`**: The town crier cries (has a tile)

A town's crier (its official, an innkeeper or the priest) cries a news line, or the season's deeds and nicknames, at the square.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the crier (resident) | any resident |
| `cell` | the square (place) | anywhere |
| `detail` | the line cried (news), or the festival's name (deeds) (text) | any |
| `quantity` | lines of news (1), or deeds called (number) |  |

tile only when the crier already stands in the square's cell.

```json
{"id": "when-crier", "on": "crier", "actions": [{"do": "note", "text": "The town crier cries"}]}
```

<a id="event-letter-returned"></a>**`letter returned`**: A letter's enclosure comes back (has a tile)

A letter holding coins or goods goes back to its writer with them: unread for 56 game days, its reader gone, or burnt unopened.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the writer (wolf) | any wolf |
| `target` | the one it was for (character) | anyone |
| `cell` | where the writer is (place) | anywhere |
| `item` | the goods it held (item) | anything |
| `detail` | the letter's id (letter) | any letter |
| `quantity` | how many goods (number) |  |
| `coins` | coins it held (number) |  |

Recorded only when there was an enclosure to return. cell and tile when the writer is in the world.

```json
{"id": "when-letter-returned", "on": "letter returned", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "A letter's enclosure comes back"}]}
```

<a id="event-notice-removed"></a>**`notice removed`**: A notice is taken down (has a tile)

The Dungeon Master takes a notice off a board.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the notice's author (wolf) | any wolf |
| `cell` | where the author is (place) | anywhere |
| `detail` | the notice's id (notice) | any notice |

cell and tile only when the author is in the world.

```json
{"id": "when-notice-removed", "on": "notice removed", "actions": [{"do": "note", "text": "A notice is taken down"}]}
```

<a id="event-resident-letter"></a>**`resident letter`**: A resident writes to a wolf (has a tile)

A resident writes to a wolf (thanks for help, a wedding or funeral invitation, work offered...), perhaps with coins.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the writer (resident) | any resident |
| `target` | the wolf (wolf) | any wolf |
| `cell` | where the resident is (place) | anywhere |
| `item` | the letter's kind (text) | any |
| `detail` | the letter's id (letter) | any letter |
| `coins` | coins enclosed (number) |  |

```json
{"id": "when-resident-letter", "on": "resident letter", "actions": [{"do": "note", "text": "A resident writes to a wolf"}]}
```

#### Law, crime and pacts

<a id="event-fine-paid"></a>**`fine paid`**: A fine is paid to the watch (mission evidence, has a tile)

Someone with a warrant pays what is owed and the fine to a guard who stopped them.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who paid (character) | anyone |
| `target` | the guard (resident) | any resident |
| `cell` | where the payer is (place) | anywhere |
| `detail` | 'to the watch of <town>' (text) | any |
| `coins` | restitution and fine together (number) |  |

```json
{"id": "when-fine-paid", "on": "fine paid", "match": {"target": "rm111"}, "actions": [{"do": "note", "text": "A fine is paid to the watch"}]}
```

<a id="event-pact-sealed"></a>**`pact sealed`**: A pact is sealed (mission evidence, has a tile)

Both parties to a pact (a written agreement sent by letter) have sealed it; recorded when the last of them seals.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the pact's author (wolf) | any wolf |
| `target` | the other party (character) | anyone |
| `cell` | where the author is (place) | anywhere |
| `detail` | the pact's id (text) | any |

```json
{"id": "when-pact-sealed", "on": "pact sealed", "actions": [{"do": "note", "text": "A pact is sealed"}]}
```

<a id="event-reported"></a>**`reported`**: A crime is reported to the watch (mission evidence, has a tile)

A witness to a crime tells a guard of it.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the witness (character) | anyone |
| `target` | the guard told (resident) | any resident |
| `cell` | where the crime happened (place) | anywhere |
| `detail` | '<charge> (<incident id>)' (text) | any |

tile only when the witness is in the crime's cell.

```json
{"id": "when-reported", "on": "reported", "match": {"target": "rm111"}, "actions": [{"do": "note", "text": "A crime is reported to the watch"}]}
```

<a id="event-told-the-wardens"></a>**`told the wardens`**: A wolf is reported to the Wardens (mission evidence, has a tile)

A wolf who saw another use a Gift tells the Wardens of it (the told-of wolf's Warden attention rises).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the one who told (wolf) | any wolf |
| `target` | the one told of (wolf) | any wolf |
| `cell` | where the teller is (place) | anywhere |

```json
{"id": "when-told-the-wardens", "on": "told the wardens", "actions": [{"do": "note", "text": "A wolf is reported to the Wardens"}]}
```

<a id="event-vouched-to-the-wardens"></a>**`vouched to the wardens`**: A wolf vouches to the Wardens (mission evidence, has a tile)

A wolf who saw another's Gift vouches for them to the Wardens (once a month).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the one who vouched (wolf) | any wolf |
| `target` | the one vouched for (wolf) | any wolf |
| `cell` | where the voucher is (place) | anywhere |

```json
{"id": "when-vouched-to-the-wardens", "on": "vouched to the wardens", "actions": [{"do": "note", "text": "A wolf vouches to the Wardens"}]}
```

<a id="event-warrant"></a>**`warrant`**: A warrant is out (has a tile)

Word of a crime reaches a town's watch: the offender is wanted (a new warrant or one more charge).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the offender (character) | anyone |
| `target` | the victim (character) | anyone |
| `cell` | where the crime happened (place) | anywhere |
| `item` | what was stolen, if a theft (item) | anything |
| `detail` | 'wanted by the watch of <town> for <charge> (<incident id>)' (text) | any |
| `quantity` | how many were stolen (number) |  |
| `coins` | coins stolen (number) |  |

tile only when the offender is in the crime's cell.

```json
{"id": "when-warrant", "on": "warrant", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "A warrant is out"}]}
```

<a id="event-arrest"></a>**`arrest`**: Someone is arrested

A guard takes a wanted character into custody (they wouldn't pay, or were downed) and puts them in the gaol.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the guard (resident) | any resident |
| `target` | who is held (character) | anyone |
| `cell` | the gaol (place) | anywhere |
| `detail` | 'held by the watch of <town>' (text) | any |

tile only if the guard stands in the gaol's cell.

```json
{"id": "when-arrest", "on": "arrest", "actions": [{"do": "note", "text": "Someone is arrested"}]}
```

<a id="event-released"></a>**`released`**: Someone is let out of the gaol (has a tile)

Someone held by the watch has served their time.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who is let go (character) | anyone |
| `cell` | the gaol (place) | anywhere |
| `detail` | 'by the watch of <town>' (text) | any |

```json
{"id": "when-released", "on": "released", "actions": [{"do": "note", "text": "Someone is let out of the gaol"}]}
```

<a id="event-stolen-goods-smelt"></a>**`stolen goods smelt`**: Stolen goods are smelt (has a tile)

A resident's nose catches its own maker's scent on goods a thief carries: it now knows of the theft.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who smelt it (resident) | any resident |
| `target` | the thief (character) | anyone |
| `cell` | where the thief is (place) | anywhere |
| `item` | the stolen goods (item) | anything |
| `detail` | the theft's incident id (text) | any |
| `quantity` | always 1 (number) |  |

```json
{"id": "when-stolen-goods-smelt", "on": "stolen goods smelt", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "Stolen goods are smelt"}]}
```

<a id="event-stopped-by-the-watch"></a>**`stopped by the watch`**: The watch stops someone (has a tile)

A guard confronts someone with a warrant: pay, or come along.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the guard (resident) | any resident |
| `target` | who is wanted (character) | anyone |
| `cell` | where they are (place) | anywhere |
| `detail` | the charges, 'theft and assault'... (text) | any |

```json
{"id": "when-stopped-by-the-watch", "on": "stopped by the watch", "actions": [{"do": "note", "text": "The watch stops someone"}]}
```

<a id="event-crime-seen"></a>**`crime seen`**: A crime is seen (watched, mission evidence, has a tile)

Someone noticed a crime as it happened (a theft, an attempted theft, a blow): the witness (a wolf or a resident), the offender, the victim, the kind. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the witness (character) | anyone |
| `target` | the offender (empty when the witness could not tell who) (character) | anyone |
| `item` | the victim (character) | anyone |
| `detail` | theft, attempted theft or assault (text) | any |

```json
{"id": "when-crime-seen", "on": "crime seen", "actions": [{"do": "note", "text": "A crime is seen"}]}
```

<a id="event-standing-crossed"></a>**`standing crossed`**: A wolf's standing crosses a number (watched)

A wolf in the world: their standing with a faction (their Chapter's, as the faction counts it) goes up or down past a number the trigger names in its detail (quantity: the standing now). Name the faction and the number. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the faction (faction) | the faction named |
| `detail` | the number crossed (text) | the number named |
| `item` | which way: up or down (text) | either way |
| `quantity` | the standing now (number) | any |

A trigger on it must name `target` and `detail` in its filters.

Looked at every two seconds for the wolves in the world while a story listens; a wolf first looked at makes no event.

```json
{"id": "when-standing-crossed", "on": "standing crossed", "match": {"target": "watch", "detail": "500"}, "actions": [{"do": "note", "text": "A wolf's standing crosses a number"}]}
```

<a id="event-relation-changed"></a>**`relation changed`**: A faction's stance changes (watched)

One faction's stance toward another changed (a DM's change, a story's, or the world's own): the new stance (quantity: the disposition). Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `target` | the faction (faction) | any faction |
| `item` | toward this faction (faction) | any faction |
| `detail` | the new stance (text) | any stance |
| `quantity` | the disposition (-100 to 100) (number) | any |

```json
{"id": "when-relation-changed", "on": "relation changed", "match": {"target": "watch"}, "actions": [{"do": "note", "text": "A faction's stance changes"}]}
```

#### Town life

<a id="event-festival"></a>**`festival`**: A festival begins

A town's festival day reaches noon and the festival starts at the square.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the town (town) | any town |
| `cell` | the town square's cell (place) | anywhere |
| `detail` | the festival's name (text) | any |

```json
{"id": "when-festival", "on": "festival", "actions": [{"do": "note", "text": "A festival begins"}]}
```

<a id="event-project-built"></a>**`project built`**: A town project is finished

A town project has all its materials and work and stands built.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the town (written 'town:<town id>') (town) | any town |
| `target` | the project's id (text) | any |
| `cell` | where it stands (place) | anywhere |
| `item` | the kind of project (project) | any kind |
| `detail` | the project's title (text) | any |
| `coins` | its whole worth (number) |  |

actor carries the 'town:' prefix. The project's x,y are passed in the event's time slots and overwritten, so no tile is logged.

```json
{"id": "when-project-built", "on": "project built", "actions": [{"do": "note", "text": "A town project is finished"}]}
```

<a id="event-project-gift"></a>**`project gift`**: Someone gives to a town project (mission evidence, has a tile)

A wolf gives coins or hands in materials to a town project (or its upkeep).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the giver (wolf) | any wolf |
| `target` | the project's id (text) | any |
| `cell` | where the giver is (place) | anywhere |
| `item` | the base item handed in, or 'coin' (item) | anything |
| `detail` | the project's title (text) | any |
| `quantity` | how many pieces (number) |  |
| `coins` | coins given (number) |  |

item is 'coin' for money.

```json
{"id": "when-project-gift", "on": "project gift", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "Someone gives to a town project"}]}
```

<a id="event-festival-entered"></a>**`festival entered`**: Someone enters a festival contest (mission evidence, has a tile)

A wolf pays to enter a festival contest at the square, a board or an inn.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the entrant (wolf) | any wolf |
| `cell` | where it signed up (place) | anywhere |
| `item` | the contest kind (text) | any |
| `detail` | the festival's name (text) | any |
| `coins` | the entry fee (number) |  |

```json
{"id": "when-festival-entered", "on": "festival entered", "actions": [{"do": "note", "text": "Someone enters a festival contest"}]}
```

<a id="event-festival-won"></a>**`festival won`**: A festival contest is won (mission evidence, has a tile)

A festival contest ends with a winner and the pot is shared out.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the winner (the first of a winning team) (character) | anyone |
| `cell` | the town square (place) | anywhere |
| `item` | the contest kind (text) | any |
| `detail` | the festival's name (text) | any |
| `quantity` | how many entered (number) |  |
| `coins` | the winner's share of the pot (number) |  |

```json
{"id": "when-festival-won", "on": "festival won", "actions": [{"do": "note", "text": "A festival contest is won"}]}
```

<a id="event-project-ruined"></a>**`project ruined`**: A town project falls to ruin

A built town project wears down to nothing for want of upkeep.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the town (written 'town:<town id>') (town) | any town |
| `target` | the project's id (text) | any |
| `cell` | where it stands (place) | anywhere |
| `item` | the kind of project (project) | any kind |
| `detail` | the project's title (text) | any |

actor carries the 'town:' prefix.

```json
{"id": "when-project-ruined", "on": "project ruined", "actions": [{"do": "note", "text": "A town project falls to ruin"}]}
```

<a id="event-festival-called"></a>**`festival called`**: A festival is called

The Dungeon Master (or a story) calls a festival for a town, today or some days ahead.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the town (town) | any town |
| `detail` | the festival's name (text) | any |

```json
{"id": "when-festival-called", "on": "festival called", "actions": [{"do": "note", "text": "A festival is called"}]}
```

<a id="event-event-added"></a>**`event added`**: The Dungeon Master adds a market event

The Dungeon Master adds an event to a market's calendar (a fair, a shortage, a glut...).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the market event's id (e<n>) (text) | any |
| `target` | the market (market) | any market |
| `item` | the event's kind (text) | any |
| `detail` | the event in words (text) | any |
| `quantity` | goods it bears on (number) |  |

No cell. Its notice, if due already, follows as 'event posted'.

```json
{"id": "when-event-added", "on": "event added", "actions": [{"do": "note", "text": "The Dungeon Master adds a market event"}]}
```

<a id="event-event-begins"></a>**`event begins`**: A market event begins

A market event's day arrives and it takes effect on prices and stock.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the market event's id (e<n>) (text) | any |
| `target` | the market (market) | any market |
| `cell` | the market's place (place) | anywhere |
| `item` | the event's kind (text) | any |
| `detail` | its notice, or its words when it has none (text) | any |
| `quantity` | 1 if it was not announced before, else 0 (number) |  |

```json
{"id": "when-event-begins", "on": "event begins", "actions": [{"do": "note", "text": "A market event begins"}]}
```

<a id="event-event-notice"></a>**`event notice`**: A market event's notice changes

A market event tied to a ship gets its after-notice once that ship has sailed (or is gone).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the market event's id (e<n>) (text) | any |
| `target` | the market (market) | any market |
| `cell` | the market's place (place) | anywhere |
| `item` | the event's kind (text) | any |
| `detail` | the new notice (text) | any |

```json
{"id": "when-event-notice", "on": "event notice", "actions": [{"do": "note", "text": "A market event's notice changes"}]}
```

<a id="event-event-posted"></a>**`event posted`**: A market event is announced

A coming market event's notice goes up on the board, some days before it begins.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the market event's id (e<n>) (text) | any |
| `target` | the market (market) | any market |
| `cell` | the market's place (place) | anywhere |
| `item` | the event's kind (text) | any |
| `detail` | the notice (text) | any |

```json
{"id": "when-event-posted", "on": "event posted", "actions": [{"do": "note", "text": "A market event is announced"}]}
```

<a id="event-event-rolled"></a>**`event rolled`**: A market event is rolled

The staged economy rolls a new event onto a market's calendar.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the market event's id (e<n>) (text) | any |
| `target` | the market (market) | any market |
| `cell` | the market's place (place) | anywhere |
| `item` | the event's kind (text) | any |
| `detail` | the event in words (text) | any |
| `quantity` | goods it bears on (number) |  |

```json
{"id": "when-event-rolled", "on": "event rolled", "actions": [{"do": "note", "text": "A market event is rolled"}]}
```

<a id="event-project-cancelled"></a>**`project cancelled`**: A town project is cancelled

A town project is called off (by the Dungeon Master) and its gifts refunded.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the town (written 'town:<town id>') (town) | any town |
| `target` | the project's id (text) | any |
| `item` | the kind of project (project) | any kind |
| `cell` | where it was to stand (place) | anywhere |
| `detail` | why, e.g. 'cancelled by the Dungeon Master' (text) | any |

```json
{"id": "when-project-cancelled", "on": "project cancelled", "actions": [{"do": "note", "text": "A town project is cancelled"}]}
```

<a id="event-project-posted"></a>**`project posted`**: A town project is posted

A town project is put up for gifts and work: by the town's own needs, the Dungeon Master or a developer.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who posted it: 'town', 'dm' or 'dev' (text) | any |
| `target` | the project's id (text) | any |
| `item` | the kind of project (project) | any kind |
| `cell` | where it will stand (place) | anywhere |
| `detail` | the project's title (text) | any |

The project's x,y are passed in the event's time slots and overwritten, so no tile is logged. A town's own proposal also records 'project proposed'.

```json
{"id": "when-project-posted", "on": "project posted", "actions": [{"do": "note", "text": "A town project is posted"}]}
```

<a id="event-project-proposed"></a>**`project proposed`**: A town proposes a project

A town decides it needs something (cover, mending...) and posts a project of its own.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the town (written 'town:<town id>') (town) | any town |
| `target` | the project's id (text) | any |
| `item` | the kind of project (project) | any kind |
| `cell` | where it will stand (may be empty) (place) | anywhere |
| `detail` | the town's reason (text) | any |

Follows a 'project posted' with actor 'town'.

```json
{"id": "when-project-proposed", "on": "project proposed", "actions": [{"do": "note", "text": "A town proposes a project"}]}
```

<a id="event-project-removed"></a>**`project removed`**: A town project is removed

A town project (built or not) is removed and its gifts refunded.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the town (written 'town:<town id>') (town) | any town |
| `target` | the project's id (text) | any |
| `item` | the kind of project (project) | any kind |
| `cell` | where it stood (place) | anywhere |
| `detail` | the project's title (text) | any |

```json
{"id": "when-project-removed", "on": "project removed", "actions": [{"do": "note", "text": "A town project is removed"}]}
```

#### Fame, journal and tales

<a id="event-storyline-objective"></a>**`storyline objective`**: A storyline objective is met (has a tile)

Something a character did met an objective of a storyline they are in (a personal storyline or a tale).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who met it (character) | anyone |
| `target` | the storyline's id (a story run, a tale or a personal storyline) (run) | any storyline |
| `cell` | where they are (place) | anywhere |
| `detail` | the objective's line, with ' (by hand)' when marked done by hand (text) | any |

```json
{"id": "when-storyline-objective", "on": "storyline objective", "actions": [{"do": "note", "text": "A storyline objective is met"}]}
```

<a id="event-storyline-step"></a>**`storyline step`**: A storyline step is done (has a tile)

Every objective of a storyline's step is met and the storyline moves to its next step.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | whose act finished it (character) | anyone |
| `target` | the storyline's id (a story run, a tale or a personal storyline) (run) | any storyline |
| `cell` | where they are (place) | anywhere |
| `detail` | the step's title (text) | any |

```json
{"id": "when-storyline-step", "on": "storyline step", "actions": [{"do": "note", "text": "A storyline step is done"}]}
```

<a id="event-tale-ended"></a>**`tale ended`**: A tale ends (has a tile)

A storyteller's tale (a player-run storyline) is ended by its teller, or closed.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the storyteller (wolf) | any wolf |
| `target` | the storyline's id (a story run, a tale or a personal storyline) (run) | any storyline |
| `cell` | where the teller is (place) | anywhere |
| `detail` | '<title> (<how it ended>)' (text) | any |

```json
{"id": "when-tale-ended", "on": "tale ended", "actions": [{"do": "note", "text": "A tale ends"}]}
```

<a id="event-deed"></a>**`deed`**: A deed is told of (mission evidence, has a tile)

A wolf's notable act becomes a deed in the town's talk (a camp broken, a project built, a promise kept, a trouble solved, a milestone credited...).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the first of those who did it (wolf) | any wolf |
| `target` | who it was done for (a character, or 'town:<town id>') (character) | anyone |
| `cell` | where it was done (place) | anywhere |
| `item` | the deed's kind (broke_camp, tended, kept_promise, carried_letter, built_project, award...) (text) | any |
| `detail` | the deed in words, then its id in brackets (text) | any |
| `quantity` | its weight: 0 small, 1 notable, 2 great, 3 legendary (number) |  |

Only wolves are doers. target may be empty.

```json
{"id": "when-deed", "on": "deed", "actions": [{"do": "note", "text": "A deed is told of"}]}
```

<a id="event-deed-revoked"></a>**`deed revoked`**: A deed is revoked

The Dungeon Master strikes a deed from the record; the town forgets it.

| Field | Holds | Left out means |
|---|---|---|
| `detail` | the deed's id (text) | any |

Only detail is filled.

```json
{"id": "when-deed-revoked", "on": "deed revoked", "actions": [{"do": "note", "text": "A deed is revoked"}]}
```

<a id="event-held-for-a-story"></a>**`held for a story`**: A place is held for a story

The Dungeon Master or a story holds a place: no new lodgings, running ones given a week's notice.

| Field | Holds | Left out means |
|---|---|---|
| `cell` | the place held (place) | anywhere |
| `detail` | the reason given (text) | any |

```json
{"id": "when-held-for-a-story", "on": "held for a story", "actions": [{"do": "note", "text": "A place is held for a story"}]}
```

<a id="event-milestone"></a>**`milestone`**: A story milestone is credited

The Dungeon Master's host credits a world story's milestone to the wolves who took part (deeds follow for the main ones).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | always 'dm' (text) | any |
| `target` | the credits' id (text) | any |
| `detail` | '<story> · <milestone title>' (text) | any |

```json
{"id": "when-milestone", "on": "milestone", "actions": [{"do": "note", "text": "A story milestone is credited"}]}
```

<a id="event-story-prize"></a>**`story prize`**: A story's prize is given (has a tile)

A storyteller gives a prize from their purse (or an Officer from the Chapter's) for their tale.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the storyteller (wolf) | any wolf |
| `target` | who receives it (character) | anyone |
| `cell` | where the teller is (place) | anywhere |
| `item` | the goods given (empty for coins) (item) | anything |
| `detail` | the tale's id (run) | any story |
| `quantity` | how many goods (number) |  |
| `coins` | coins given (number) |  |

```json
{"id": "when-story-prize", "on": "story prize", "match": {"item": "rope"}, "actions": [{"do": "note", "text": "A story's prize is given"}]}
```

<a id="event-storyline-begun"></a>**`storyline begun`**: A storyline begins (has a tile)

A character starts a storyline (a personal quest, from a template or a source).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who began it (character) | anyone |
| `target` | the storyline's id (a story run, a tale or a personal storyline) (run) | any storyline |
| `cell` | where they are (place) | anywhere |
| `detail` | its title (text) | any |

```json
{"id": "when-storyline-begun", "on": "storyline begun", "actions": [{"do": "note", "text": "A storyline begins"}]}
```

<a id="event-storyline-done"></a>**`storyline done`**: A storyline is finished (has a tile)

Every step of a storyline is done.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | its owner (or its author) (character) | anyone |
| `target` | the storyline's id (a story run, a tale or a personal storyline) (run) | any storyline |
| `cell` | where they are (place) | anywhere |
| `detail` | its title (text) | any |

```json
{"id": "when-storyline-done", "on": "storyline done", "actions": [{"do": "note", "text": "A storyline is finished"}]}
```

<a id="event-tale-begun"></a>**`tale begun`**: A tale begins (has a tile)

A storyteller starts (or resumes) their tale.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the storyteller (wolf) | any wolf |
| `target` | the storyline's id (a story run, a tale or a personal storyline) (run) | any storyline |
| `cell` | where the teller is (place) | anywhere |
| `detail` | the tale's title (text) | any |

```json
{"id": "when-tale-begun", "on": "tale begun", "actions": [{"do": "note", "text": "A tale begins"}]}
```

<a id="event-tale-joined"></a>**`tale joined`**: A wolf joins a tale (has a tile)

An invited wolf accepts a part in a tale.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | who joined (wolf) | any wolf |
| `target` | the storyline's id (a story run, a tale or a personal storyline) (run) | any storyline |
| `cell` | where they are (place) | anywhere |
| `detail` | the tale's title (text) | any |

```json
{"id": "when-tale-joined", "on": "tale joined", "actions": [{"do": "note", "text": "A wolf joins a tale"}]}
```

<a id="event-skill-practised"></a>**`skill practised`**: A wolf's skill grows a step (watched, has a tile)

A wolf's skill passes a whole step by practice (quantity: the new whole value), as their growth lines say. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `item` | the skill (text) | any skill |
| `quantity` | the new whole value (number) | any |

```json
{"id": "when-skill-practised", "on": "skill practised", "actions": [{"do": "note", "text": "A wolf's skill grows a step"}]}
```

#### Quests and parties

<a id="event-quest-accepted"></a>**`quest accepted`**: A wolf takes on a quest

A wolf accepted a story's quest: in the giver's dialogue window, or from the journal for a quest offered to them. The story checks who may take it (its join rules) and starts their copy.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the story run (run) | any story |
| `item` | the resident it was taken from (empty: from the journal) (resident) | anyone, or the journal |
| `detail` | the quest (quest) | any quest |

```json
{"id": "when-quest-accepted", "on": "quest accepted", "match": {"item": "rm111"}, "actions": [{"do": "note", "text": "A wolf takes on a quest"}]}
```

<a id="event-quest-declined"></a>**`quest declined`**: A wolf declines a quest

A wolf said not now to a story's quest offered to them; it can be asked about again.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the story run (run) | any story |
| `item` | the resident it was taken from (empty: from the journal) (resident) | anyone, or the journal |
| `detail` | the quest (quest) | any quest |

```json
{"id": "when-quest-declined", "on": "quest declined", "match": {"item": "rm111"}, "actions": [{"do": "note", "text": "A wolf declines a quest"}]}
```

<a id="event-quest-handed-in"></a>**`quest handed in`**: A wolf hands in a quest (mission evidence)

A wolf whose quest was ready came back to its giver and handed it in (the dialogue window's hand-in choice): the quest is done and its rewards are given.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the story run (run) | any story |
| `item` | the giver (resident) | anyone |
| `detail` | the quest (quest) | any quest |

```json
{"id": "when-quest-handed-in", "on": "quest handed in", "match": {"item": "rm111"}, "actions": [{"do": "note", "text": "A wolf hands in a quest"}]}
```

<a id="event-quest-abandoned"></a>**`quest abandoned`**: A wolf abandons a quest

A wolf put a story's quest aside from the journal.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the story run (run) | any story |
| `detail` | the quest (quest) | any quest |

```json
{"id": "when-quest-abandoned", "on": "quest abandoned", "actions": [{"do": "note", "text": "A wolf abandons a quest"}]}
```

<a id="event-dialogue-choice"></a>**`dialogue choice`**: A wolf picks a story's choice in a dialogue window (mission evidence)

A wolf picked a story choice laid over a resident's dialogue window (npc.offer): the wolf's words said, the resident's reply given.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the story run (run) | any story |
| `item` | the resident (resident) | anyone |
| `detail` | the choice's name (offer) | any choice |

```json
{"id": "when-dialogue-choice", "on": "dialogue choice", "match": {"item": "rm111"}, "actions": [{"do": "note", "text": "A wolf picks a story's choice in a dialogue window"}]}
```

<a id="event-party-joined"></a>**`party joined`**: A wolf joins a party

A wolf joined a party (accepted an invitation). Stories keep each wolf's party from these: party quests and party variables.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the party (text) | any party |

```json
{"id": "when-party-joined", "on": "party joined", "actions": [{"do": "note", "text": "A wolf joins a party"}]}
```

<a id="event-party-left"></a>**`party left`**: A wolf leaves a party

A wolf left a party, or the party was disbanded (one event for each of its wolves).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the party (text) | any party |

```json
{"id": "when-party-left", "on": "party left", "actions": [{"do": "note", "text": "A wolf leaves a party"}]}
```

<a id="event-quest-started"></a>**`quest started`**: A story's quest starts (the story's own)

The story's own fact (never in the world's log): a copy of a quest began for a wolf, their party or the story.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf it started for (wolf) | any wolf |
| `target` | the story run (run) | any story |
| `detail` | the quest (quest) | any quest |

Sent by the story itself as it runs, after the fact that caused it; the game never records it.

```json
{"id": "when-quest-started", "on": "quest started", "actions": [{"do": "note", "text": "A story's quest starts"}]}
```

<a id="event-quest-completed"></a>**`quest completed`**: A story's quest is done (the story's own, mission evidence)

The story's own fact: a copy of a quest was completed (handed in, or its last required objective done); its rewards follow.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf who finished it (wolf) | any wolf |
| `target` | the story run (run) | any story |
| `detail` | the quest (quest) | any quest |

Sent by the story itself as it runs, after the fact that caused it; the game never records it.

```json
{"id": "when-quest-completed", "on": "quest completed", "actions": [{"do": "note", "text": "A story's quest is done"}]}
```

<a id="event-quest-failed"></a>**`quest failed`**: A story's quest fails (the story's own)

The story's own fact: a copy of a quest failed (a fail rule, its deadline, an objective failed, or by hand).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf it failed for (wolf) | any wolf |
| `target` | the story run (run) | any story |
| `detail` | the quest (quest) | any quest |

Sent by the story itself as it runs, after the fact that caused it; the game never records it.

```json
{"id": "when-quest-failed", "on": "quest failed", "actions": [{"do": "note", "text": "A story's quest fails"}]}
```

<a id="event-objective-completed"></a>**`objective completed`**: A quest's objective is done (the story's own, mission evidence)

The story's own fact: an objective of a quest copy was done.

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf who did it (wolf) | any wolf |
| `target` | the story run (run) | any story |
| `item` | the objective (objective) | any objective |
| `detail` | the quest (quest) | any quest |

Sent by the story itself as it runs, after the fact that caused it; the game never records it.

```json
{"id": "when-objective-completed", "on": "objective completed", "actions": [{"do": "note", "text": "A quest's objective is done"}]}
```

<a id="event-party-formed"></a>**`party formed`**: A party is formed (watched)

A new party came together (an invitation accepted): its leader. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the party (text) | any |

```json
{"id": "when-party-formed", "on": "party formed", "actions": [{"do": "note", "text": "A party is formed"}]}
```

<a id="event-party-ended"></a>**`party ended`**: A party breaks up (watched)

A party ended (disbanded, or down to one). Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the party (text) | any |

```json
{"id": "when-party-ended", "on": "party ended", "actions": [{"do": "note", "text": "A party breaks up"}]}
```

#### Doors and buildings

<a id="event-door-opened"></a>**`door opened`**: A wolf opens a door (watched, mission evidence, has a tile)

A wolf opened a door (or walked through a closed one, which opens it). Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the door (door) | any door |
| `cell` | where the door is (place) | anywhere |
| `detail` | the door's name (text) | any |

```json
{"id": "when-door-opened", "on": "door opened", "actions": [{"do": "note", "text": "A wolf opens a door"}]}
```

<a id="event-door-closed"></a>**`door closed`**: A wolf closes a door (watched, mission evidence, has a tile)

A wolf closed a door. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the door (door) | any door |
| `cell` | where the door is (place) | anywhere |
| `detail` | the door's name (text) | any |

```json
{"id": "when-door-closed", "on": "door closed", "actions": [{"do": "note", "text": "A wolf closes a door"}]}
```

<a id="event-door-locked"></a>**`door locked`**: A wolf tries a locked door (watched, mission evidence, has a tile)

A wolf tried to open a locked door. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the door (door) | any door |
| `cell` | where the door is (place) | anywhere |
| `detail` | the door's name (text) | any |

```json
{"id": "when-door-locked", "on": "door locked", "actions": [{"do": "note", "text": "A wolf tries a locked door"}]}
```

<a id="event-building-entered"></a>**`building entered`**: A wolf goes indoors (watched, mission evidence, has a tile)

A wolf went from out of doors (or another room) into an interior: a building, a room, a cellar. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the interior entered (place) | anywhere |
| `cell` | the interior entered (same as target) (place) | anywhere |
| `detail` | where they came from (place) | anywhere |

```json
{"id": "when-building-entered", "on": "building entered", "match": {"target": "greyfen-square"}, "actions": [{"do": "note", "text": "A wolf goes indoors"}]}
```

<a id="event-building-left"></a>**`building left`**: A wolf comes out of a building (watched, mission evidence, has a tile)

A wolf left an interior. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the interior left (place) | anywhere |
| `cell` | where they are now (place) | anywhere |
| `detail` | where they are now (place) | anywhere |

```json
{"id": "when-building-left", "on": "building left", "match": {"target": "greyfen-square"}, "actions": [{"do": "note", "text": "A wolf comes out of a building"}]}
```

#### Time, weather and calendar

<a id="event-weather-changed"></a>**`weather changed`**: The weather changes (watched)

The weather in a place the story watches changed (a place it names, or where one of the world's wolves stands). Checked every few seconds. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `target` | the place (place) | anywhere |
| `cell` | the place (same as target) (place) | anywhere |
| `detail` | the new weather (clear, overcast, rain, storm, fog, snow, sandstorm) (text) | any weather |
| `item` | the weather before (text) | any |

```json
{"id": "when-weather-changed", "on": "weather changed", "match": {"target": "greyfen-square"}, "actions": [{"do": "note", "text": "The weather changes"}]}
```

<a id="event-hour"></a>**`hour`**: An hour of the day begins (watched)

The world's clock reached a new hour (quantity: the hour, 0 to 23; detail: the same as text, "14"). Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `detail` | the hour, as text ("6", "14") (text) | any |
| `quantity` | the hour, 0 to 23 (number) | any |

```json
{"id": "when-hour", "on": "hour", "actions": [{"do": "note", "text": "An hour of the day begins"}]}
```

<a id="event-day-begins"></a>**`day begins`**: A new day begins (watched)

Midnight: a new day of the world's calendar (detail: the weekday, Marketday, Restday...; item: the season; quantity: the day of the season). Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `detail` | the weekday (Dawnday ... Restday) (text) | any |
| `item` | the season (text) | any |
| `quantity` | the day of the season (number) | any |

```json
{"id": "when-day-begins", "on": "day begins", "actions": [{"do": "note", "text": "A new day begins"}]}
```

<a id="event-dawn"></a>**`dawn`**: Dawn (watched)

The light comes up (5 o'clock). Written only while a live story listens for it (the story's watch list).

```json
{"id": "when-dawn", "on": "dawn", "actions": [{"do": "note", "text": "Dawn"}]}
```

<a id="event-night-falls"></a>**`night falls`**: Night falls (watched)

Full dark (19 o'clock). Written only while a live story listens for it (the story's watch list).

```json
{"id": "when-night-falls", "on": "night falls", "actions": [{"do": "note", "text": "Night falls"}]}
```

<a id="event-season-begins"></a>**`season begins`**: A season begins (watched)

The first day of a season (detail: spring, summer, autumn or winter). Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `detail` | the season (text) | any |

```json
{"id": "when-season-begins", "on": "season begins", "actions": [{"do": "note", "text": "A season begins"}]}
```

#### Dialogue windows

<a id="event-dialogue-opened"></a>**`dialogue opened`**: A wolf opens a resident's window (watched, mission evidence, has a tile)

A wolf opened a resident's dialogue window (doc 83): detail is what they opened it on (resident, trade, missions, introduce...). Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the resident (resident) | anyone |
| `detail` | what it was opened on (text) | any |

```json
{"id": "when-dialogue-opened", "on": "dialogue opened", "match": {"target": "rm111"}, "actions": [{"do": "note", "text": "A wolf opens a resident's window"}]}
```

<a id="event-dialogue-node"></a>**`dialogue node`**: A dialogue reaches a node (watched, mission evidence, has a tile)

A resident's dialogue window moved to a node of its tree (detail: the node's id, item: the tree's id); a story's own nodes are @story:<run>:<kind>:<id>. Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the resident (resident) | anyone |
| `detail` | the node (text) | any |
| `item` | the tree (text) | any |

```json
{"id": "when-dialogue-node", "on": "dialogue node", "match": {"target": "rm111"}, "actions": [{"do": "note", "text": "A dialogue reaches a node"}]}
```

<a id="event-dialogue-closed"></a>**`dialogue closed`**: A wolf leaves a resident's window (watched, has a tile)

A resident's dialogue window closed (detail: why: end, bye, walked away, replaced...). Written only while a live story listens for it (the story's watch list).

| Field | Holds | Left out means |
|---|---|---|
| `actor` | the wolf (wolf) | any wolf |
| `target` | the resident (resident) | anyone |
| `detail` | why it closed (text) | any |

```json
{"id": "when-dialogue-closed", "on": "dialogue closed", "match": {"target": "rm111"}, "actions": [{"do": "note", "text": "A wolf leaves a resident's window"}]}
```

### Conditions

A condition is a tree: All, Any and Not hold others (8 under each, 5 deep). A condition over the world's facts reads the world as of the event; a fact that is missing (a wolf away) is unknown, and unknown never fires, not even under Not.

<a id="condition-all"></a>**`all`**: All of these

True when every condition under it is true (1 to 8).

```json
{"test": "all", "conditions": [{"test": "switch", "var": "gate-open", "is": true}, {"test": "compare", "left": {"var": "ropes"}, "op": ">=", "right": 2}]}
```

<a id="condition-any"></a>**`any`**: Any of these

True when one condition under it is true.

```json
{"test": "any", "conditions": [{"test": "switch", "var": "gate-open", "is": true}, {"test": "compare", "left": {"var": "ropes"}, "op": ">=", "right": 2}]}
```

<a id="condition-not"></a>**`not`**: Not

True when the condition under it is false.

```json
{"test": "not", "condition": {"test": "switch", "var": "gate-open", "is": true}}
```

<a id="condition-field"></a>**`field`**: The event's field

Compares one field of the event that just happened. Text takes = and !=; quantity and coins take a number.

Reads: the event's **actor** **=** **value**

| Parameter | What | Kind |
|---|---|---|
| `field` | the field | choice; one of: `actor`, `target`, `cell`, `item`, `detail`, `quantity`, `coins` |
| `op` | the comparison | choice; one of: `=`, `!=`, `>`, `>=`, `<`, `<=` |
| `value` | the value | value |

```json
{"test": "field", "field": "actor", "op": "=", "value": "value"}
```

<a id="condition-counter"></a>**`counter`**: A counter

Compares one of the story's counters with a number. A player counter is the acting wolf's.

Reads: **the story's** counter **ropes** **=** **1**

| Parameter | What | Kind |
|---|---|---|
| `scope` | whose | choice; one of: `story`, `player` |
| `key` | the counter | counter |
| `op` | the comparison | choice; one of: `=`, `!=`, `>`, `>=`, `<`, `<=` |
| `value` | the number | int; -1000000000 to 1000000000 |

```json
{"test": "counter", "scope": "story", "key": "ropes", "op": "=", "value": 1}
```

<a id="condition-region"></a>**`region`**: Inside a region

The event's tile is inside the rectangle. An event with no tile is unknown, and unknown never fires, not even under "not".

Reads: the event happened inside **square**

| Parameter | What | Kind |
|---|---|---|
| `region` | the region | region |

```json
{"test": "region", "region": "square"}
```

<a id="condition-mission"></a>**`mission`**: A mission is ready

That mission of the current phase has reached its completion threshold. Phase triggers only.

Reads: the mission **gather-rope** is complete

| Parameter | What | Kind |
|---|---|---|
| `id` | the mission | mission |

```json
{"test": "mission", "id": "gather-rope"}
```

<a id="condition-compare"></a>**`compare`**: Compare two values (v2)

Compares two values of one kind: variables, the event's fields, numbers, texts, a list's count, a timer's days, a little arithmetic. Numbers take any comparison; switches and texts take = and !=. A value of a wolf not in the story makes it false.

Reads: **{"var": "ropes"}** **>=** **2**

| Parameter | What | Kind |
|---|---|---|
| `left` | a value | operand |
| `op` | the comparison | choice; one of: `=`, `!=`, `>`, `>=`, `<`, `<=` |
| `right` | another value | operand |

```json
{"test": "compare", "left": {"var": "ropes"}, "op": ">=", "right": 2}
```

<a id="condition-switch"></a>**`switch`**: A switch is on or off (v2)

A declared switch is on, or off.

Reads: the switch **ropes** is **on**

| Parameter | What | Kind |
|---|---|---|
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the switch | variable; a variable of type switch |
| `is` | on or off | bool; default true |

```json
{"test": "switch", "var": "ropes", "is": true}
```

<a id="condition-in-list"></a>**`in list`**: A value is in a list (v2)

A value (the acting wolf, the event's item, a text) is one of a list variable's.

Reads: **1** is in the list **ropes**

| Parameter | What | Kind |
|---|---|---|
| `value` | the value | operand |
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `list` | the list | variable; a variable of type numbers or texts or wolves or residents |

```json
{"test": "in list", "value": 1, "list": "ropes"}
```

<a id="condition-phase-is"></a>**`phase is`**: The story is in a phase (v2)

For campaign triggers: true in that phase only.

Reads: the story is in the phase **muster**

| Parameter | What | Kind |
|---|---|---|
| `phase` | the phase | phase |

```json
{"test": "phase is", "phase": "muster"}
```

<a id="condition-visit-is"></a>**`visit is`**: The phase's visit (v2)

The run's visit number (1 the first time the story is in a phase, then one more each time it moves).

Reads: this is visit **=** **1** of the phase

| Parameter | What | Kind |
|---|---|---|
| `op` | the comparison | choice; one of: `=`, `!=`, `>`, `>=`, `<`, `<=` |
| `value` | the visit | int; 1 to 1000000 |

```json
{"test": "visit is", "op": "=", "value": 1}
```

<a id="condition-timer"></a>**`timer`**: A timer is running (v2)

A named timer (Start a timer) is running; with a comparison, it has that many days left. A timer that has gone off is not running.

Reads: the timer **gate-opens** (**shared**) is running, with days left

| Parameter | What | Kind |
|---|---|---|
| `signal` | the timer | signal |
| `owner` | whose | choice; one of: `story`, `player`; default "story" |
| `op` | the comparison | choice; one of: `=`, `!=`, `>`, `>=`, `<`, `<=`; optional |
| `days` | days | days; optional |

```json
{"test": "timer", "signal": "gate-opens", "owner": "story"}
```

<a id="condition-fired"></a>**`fired`**: A trigger has fired (v2)

How often a trigger of this story fired, this phase visit or this run (Run counts; a trigger firing by hand counts).

Reads: the trigger **count-rope** has fired **=** **1** times this **visit**

| Parameter | What | Kind |
|---|---|---|
| `trigger` | the trigger | trigger |
| `op` | the comparison | choice; one of: `=`, `!=`, `>`, `>=`, `<`, `<=` |
| `value` | times | int; 0 to 1000000 |
| `in` | since | choice; one of: `visit`, `run`; default "visit" |

```json
{"test": "fired", "trigger": "count-rope", "op": "=", "value": 1, "in": "visit"}
```

<a id="condition-chance"></a>**`chance`**: A chance (v2)

True that many times in 100, rolled when it is asked: seeded by the run, so a replay gives the same answer; a rehearsal case may pin its rolls.

Reads: a **1** in 100 chance

| Parameter | What | Kind |
|---|---|---|
| `percent` | in 100 | int; 1 to 99 |

```json
{"test": "chance", "percent": 1}
```

<a id="condition-days-since"></a>**`days since`**: Days since (v2)

Game days since the phase opened or the run began, or days (on its own clock) since a timer variable started.

Reads: **=** **1** days have passed since **the phase opened**

| Parameter | What | Kind |
|---|---|---|
| `op` | the comparison | choice; one of: `=`, `!=`, `>`, `>=`, `<`, `<=` |
| `days` | days | days |
| `from` | since | choice; one of: `phase`, `run`, `var`; default "phase" |
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the timer | variable; a variable of type timer; optional |

```json
{"test": "days since", "op": "=", "days": 1, "from": "phase"}
```

<a id="condition-wolf-here"></a>**`wolf here`**: A wolf is in the world (v2, reads the world's facts)

The wolf is signed in now (as of the event). A wolf away is not here: this is never unknown while the world's facts are known.

Reads: **$actor** is in the world

| Parameter | What | Kind |
|---|---|---|
| `who` | the wolf | wolf; default "$actor" |

```json
{"test": "wolf here", "who": "$actor"}
```

<a id="condition-wolf-in-region"></a>**`wolf in region`**: A wolf stands in a region (v2, reads the world's facts)

Where the wolf stands now (as of the event), inside the story's region. Unknown for a wolf away.

Reads: **$actor** stands in **square**

| Parameter | What | Kind |
|---|---|---|
| `who` | the wolf | wolf; default "$actor" |
| `region` | the region | region |

```json
{"test": "wolf in region", "who": "$actor", "region": "square"}
```

<a id="condition-wolf-in-place"></a>**`wolf in place`**: A wolf is in a place (v2, reads the world's facts)

The wolf is in that cell or interior now. Unknown for a wolf away.

Reads: **$actor** is in **greyfen-square**

| Parameter | What | Kind |
|---|---|---|
| `who` | the wolf | wolf; default "$actor" |
| `place` | the place | place |

```json
{"test": "wolf in place", "who": "$actor", "place": "greyfen-square"}
```

<a id="condition-wolf-in-town"></a>**`wolf in town`**: A wolf is in a town (v2, reads the world's facts)

The wolf is somewhere in that town's cells (indoors too). Unknown for a wolf away.

Reads: **$actor** is in **greyfen**

| Parameter | What | Kind |
|---|---|---|
| `who` | the wolf | wolf; default "$actor" |
| `town` | the town | town |

```json
{"test": "wolf in town", "who": "$actor", "town": "greyfen"}
```

<a id="condition-wolf-indoors"></a>**`wolf indoors`**: A wolf is indoors (v2, reads the world's facts)

The wolf is under a roof (an interior), or out of doors. Unknown for a wolf away.

Reads: **$actor** is **indoors**

| Parameter | What | Kind |
|---|---|---|
| `who` | the wolf | wolf; default "$actor" |
| `is` | indoors or out | bool; default true |

```json
{"test": "wolf indoors", "who": "$actor", "is": true}
```

<a id="condition-count-in-region"></a>**`count in region`**: How many stand in a region (v2, reads the world's facts)

Counts the wolves of the story standing in the region now (StarCraft's Bring). Wolves away are not counted.

Reads: **at least** **1** of the story's wolves stand in **square**

| Parameter | What | Kind |
|---|---|---|
| `op` | the comparison | choice; one of: `=`, `!=`, `>`, `>=`, `<`, `<=`; default ">=" |
| `value` | wolves | int; 0 to 5000; default 1 |
| `region` | the region | region |

```json
{"test": "count in region", "op": ">=", "value": 1, "region": "square"}
```

<a id="condition-carries"></a>**`carries`**: A wolf carries an item (v2, reads the world's facts)

How many of an item the wolf has in their pack (marked or crafted ones count as their base item). Unknown for a wolf away.

Reads: **$actor** carries **at least** **1** **rope**

| Parameter | What | Kind |
|---|---|---|
| `who` | the wolf | wolf; default "$actor" |
| `op` | the comparison | choice; one of: `=`, `!=`, `>`, `>=`, `<`, `<=`; default ">=" |
| `value` | how many | int; 0 to 100000; default 1 |
| `item` | the item | item |

```json
{"test": "carries", "who": "$actor", "op": ">=", "value": 1, "item": "rope"}
```

<a id="condition-coins"></a>**`coins`**: A wolf's coins (v2, reads the world's facts)

The coins in the wolf's own purse. Unknown for a wolf away.

Reads: **$actor** has **at least** **10** pennies

| Parameter | What | Kind |
|---|---|---|
| `who` | the wolf | wolf; default "$actor" |
| `op` | the comparison | choice; one of: `=`, `!=`, `>`, `>=`, `<`, `<=`; default ">=" |
| `value` | pennies | int; 0 to 1000000000; default 10 |

```json
{"test": "coins", "who": "$actor", "op": ">=", "value": 10}
```

<a id="condition-standing"></a>**`standing`**: Standing with a faction (v2, reads the world's facts)

The standing of the wolf's Chapter with a faction or House (-100 to 100; only Chapters have standing). Unknown for a wolf with no Chapter, or away.

Reads: **$actor**'s standing with **watch** is **at least** **20**

| Parameter | What | Kind |
|---|---|---|
| `who` | the wolf | wolf; default "$actor" |
| `faction` | the faction or House | faction |
| `op` | the comparison | choice; one of: `=`, `!=`, `>`, `>=`, `<`, `<=`; default ">=" |
| `value` | standing | int; -100 to 100; default 20 |

```json
{"test": "standing", "who": "$actor", "faction": "watch", "op": ">=", "value": 20}
```

<a id="condition-reputation"></a>**`reputation`**: Reputation in a town (v2, reads the world's facts)

What the town's residents who know the wolf think of them: those who like them less those who dislike them. 0 where nobody knows them. Unknown for a wolf away.

Reads: **$actor**'s name in **greyfen** is **at least** **1**

| Parameter | What | Kind |
|---|---|---|
| `who` | the wolf | wolf; default "$actor" |
| `town` | the town | town |
| `op` | the comparison | choice; one of: `=`, `!=`, `>`, `>=`, `<`, `<=`; default ">=" |
| `value` | liked less disliked | int; -1000 to 1000; default 1 |

```json
{"test": "reputation", "who": "$actor", "town": "greyfen", "op": ">=", "value": 1}
```

<a id="condition-wolf-is"></a>**`wolf is`**: A wolf is downed, hurt, wanted or held (v2, reads the world's facts)

Downed (knocked down, not yet up), hurt (carrying wounds or an injury), wanted (a warrant of the watch), held (in a town's gaol). Unknown for a wolf away.

Reads: **$actor** **is** **wanted by the watch**

| Parameter | What | Kind |
|---|---|---|
| `who` | the wolf | wolf; default "$actor" |
| `state` | what | choice; one of: `downed`, `hurt`, `wanted`, `held`; default "wanted" |
| `is` | is or is not | bool; default true |

```json
{"test": "wolf is", "who": "$actor", "state": "wanted", "is": true}
```

<a id="condition-party-size"></a>**`party size`**: A wolf's party (v2, reads the world's facts)

How many wolves are in the wolf's party, the wolf included (1: alone). Unknown for a wolf away.

Reads: **$actor**'s party has **at least** **2** wolves

| Parameter | What | Kind |
|---|---|---|
| `who` | the wolf | wolf; default "$actor" |
| `op` | the comparison | choice; one of: `=`, `!=`, `>`, `>=`, `<`, `<=`; default ">=" |
| `value` | wolves | int; 1 to 12; default 2 |

```json
{"test": "party size", "who": "$actor", "op": ">=", "value": 2}
```

<a id="condition-in-party-with"></a>**`in party with`**: In a party with another wolf (v2, reads the world's facts)

Both wolves are in the world and in one party.

Reads: **$actor** is in a party with **$actor**

| Parameter | What | Kind |
|---|---|---|
| `who` | the wolf | wolf; default "$actor" |
| `other` | the other wolf | wolf |

```json
{"test": "in party with", "who": "$actor", "other": "$actor"}
```

<a id="condition-resident-is"></a>**`resident is`**: A resident is alive, dead or indoors (v2, reads the world's facts)

A named resident is alive, dead, or indoors now (as of the event).

Reads: **rm111** is **alive**

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `state` | what | choice; one of: `alive`, `dead`, `indoors`, `out of doors`; default "alive" |

```json
{"test": "resident is", "npc": "rm111", "state": "alive"}
```

<a id="condition-resident-in-place"></a>**`resident in place`**: A resident is in a place (v2, reads the world's facts)

A named resident is in that cell or interior now.

Reads: **rm111** is in **greyfen-square**

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `place` | the place | place |

```json
{"test": "resident in place", "npc": "rm111", "place": "greyfen-square"}
```

<a id="condition-resident-in-region"></a>**`resident in region`**: A resident stands in a region (v2, reads the world's facts)

A named resident stands inside the story's region now.

Reads: **rm111** stands in **square**

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `region` | the region | region |

```json
{"test": "resident in region", "npc": "rm111", "region": "square"}
```

<a id="condition-resident-has"></a>**`resident has`**: A resident's job, mood or mantle (v2, reads the world's facts)

A named resident's job (the kind of work, as residents' talk names it: guard, merchant, smith, farmer...), their mood now (even, bright, low or sore), the mantle they wear (doc 84), or the town they live in.

Reads: **rm111**'s **job** is **A few words.**

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `what` | what | choice; one of: `job`, `mood`, `mantle`, `town`; default "job" |
| `value` | is | text; up to 80 characters |

```json
{"test": "resident has", "npc": "rm111", "what": "job", "value": "A few words."}
```

<a id="condition-hour-between"></a>**`hour between`**: The hour (v2, reads the world's facts)

The world's hour (0 to 23), from one hour to another, both included; from 22 to 4 runs through midnight.

Reads: the hour is from **6** to **18**

| Parameter | What | Kind |
|---|---|---|
| `from` | from the hour | int; 0 to 23; default 6 |
| `to` | to the hour | int; 0 to 23; default 18 |

```json
{"test": "hour between", "from": 6, "to": 18}
```

<a id="condition-calendar-is"></a>**`calendar is`**: The day, the season, the time of day (v2, reads the world's facts)

The weekday (Marketday, Restday...), the season, or the time of day (dawn, day, dusk, night).

Reads: the **weekday** is **Restday**

| Parameter | What | Kind |
|---|---|---|
| `what` | what | choice; one of: `weekday`, `season`, `period`; default "weekday" |
| `value` | is | choice; one of: `Dawnday`, `Hearthday`, `Stoneday`, `Riverday`, `Emberday`, `Marketday`, `Restday`, `spring`, `summer`, `autumn`, `winter`, `dawn`, `day`, `dusk`, `night`; default "Restday" |

```json
{"test": "calendar is", "what": "weekday", "value": "Restday"}
```

<a id="condition-weather-is"></a>**`weather is`**: The weather (v2, reads the world's facts)

The weather in a place now: where the acting wolf is, or a named place. Indoors it is the weather outside.

Reads: the weather **where the acting wolf is** is **rain**

| Parameter | What | Kind |
|---|---|---|
| `where` | where | place; default "$actor" |
| `weather` | the weather | choice; one of: `clear`, `overcast`, `rain`, `storm`, `fog`, `snow`, `sandstorm`; default "rain" |

```json
{"test": "weather is", "where": "$actor", "weather": "rain"}
```

<a id="condition-relation-is"></a>**`relation is`**: How one faction stands with another (v2, reads the world's facts)

One faction's stance toward another, in the order war, hostile, tense, neutral, friendly, allied: ">=" is at least, "<=" at most.

Reads: **watch** stands **at least** **friendly** with **watch**

| Parameter | What | Kind |
|---|---|---|
| `faction` | the faction | faction |
| `op` | the comparison | choice; one of: `=`, `!=`, `>=`, `<=`; default ">=" |
| `stance` | the stance | choice; one of: `war`, `hostile`, `tense`, `neutral`, `friendly`, `allied`; default "friendly" |
| `other` | the other faction | faction |

```json
{"test": "relation is", "faction": "watch", "op": ">=", "stance": "friendly", "other": "watch"}
```

<a id="condition-quest-is"></a>**`quest is`**: A quest's state (v2)

Where a quest stands for the acting wolf (or the looped one; their party's copy for a party quest), or the story's one copy: none (never taken), offered, active, ready (to hand in), done, failed, abandoned, expired.

Reads: **axle** **for the acting wolf** is **done**

| Parameter | What | Kind |
|---|---|---|
| `quest` | the quest | quest |
| `of` | whose | choice; one of: `$actor`, `$each`, `story`; default "$actor" |
| `status` | the state | choice; one of: `none`, `offered`, `active`, `ready`, `done`, `failed`, `abandoned`, `expired`; default "done" |

```json
{"test": "quest is", "quest": "axle", "of": "$actor", "status": "done"}
```

<a id="condition-objective-is"></a>**`objective is`**: An objective's state (v2)

An objective of a quest, in the acting wolf's (or the looped one's, or the story's) copy: done, failed, to do (neither yet), or hidden (not yet shown). Without a copy it is to do, or hidden.

Reads: **ask** of **axle** **for the acting wolf** is **done**

| Parameter | What | Kind |
|---|---|---|
| `quest` | the quest | quest |
| `objective` | the objective | objective |
| `of` | whose | choice; one of: `$actor`, `$each`, `story`; default "$actor" |
| `status` | the state | choice; one of: `done`, `failed`, `to do`, `hidden`; default "done" |

```json
{"test": "objective is", "quest": "axle", "objective": "ask", "of": "$actor", "status": "done"}
```

<a id="condition-participation"></a>**`participation`**: Enough wolves took part (v2)

Counts the story's wolves who are active (seen in the last activeDays game days; or all of them, or those online now) and, of those, the ones in a list of wolves or the ones for whom a test is true. True when they are at least the share asked and at least the floor (the floor never asks for more wolves than are active; a cap, if given, is always enough, so a crowd of 3,000 does not need 1,000). With no active wolves it is false; with the online wolves not known it is unknown. A wolf who did it and then went quiet leaves both counts, so the share stays honest.

Reads: at least **1**% of the **active** wolves (at least; ) **are in the list**

| Parameter | What | Kind |
|---|---|---|
| `source` | who counts | choice; one of: `list`, `where`; default "list" |
| `list` | the list | variable; a variable of type wolves; optional |
| `where` | the test for each wolf | condition; optional |
| `of` | out of | choice; one of: `active`, `all`, `online`; default "active" |
| `percent` | the share (%) | int; 1 to 100 |
| `floor` | at least this many | int; 1 to 5000; optional; default 1 |
| `cap` | this many is always enough | int; 1 to 5000; optional |

```json
{"test": "participation", "source": "list", "of": "active", "percent": 1}
```

### Actions

A trigger runs its actions in order (12 in version 1; 24 counting nested ones in version 2). World effects are the action `effect`: they are listed under Effects below.

#### Variables

<a id="action-set"></a>**`set`**: Set a counter (version 1)

Sets a counter, clamped to plus or minus 1,000,000,000.

Reads: Set **the story's** counter **ropes** to **1**

| Parameter | What | Kind |
|---|---|---|
| `scope` | whose | choice; one of: `story`, `player` |
| `key` | the counter | counter |
| `value` | the value | int; -1000000000 to 1000000000 |

```json
{"do": "set", "scope": "story", "key": "ropes", "value": 1}
```

<a id="action-add"></a>**`add`**: Add to a counter (version 1)

Adds to a counter (a negative number takes away).

Reads: Add **1** to **the story's** counter **ropes**

| Parameter | What | Kind |
|---|---|---|
| `scope` | whose | choice; one of: `story`, `player` |
| `key` | the counter | counter |
| `value` | the amount | int; -1000000000 to 1000000000 |

```json
{"do": "add", "scope": "story", "key": "ropes", "value": 1}
```

<a id="action-set-var"></a>**`set`** (set var): Set a variable (v2)

Sets a declared variable to a value of its kind (a number is kept within plus or minus 1,000,000,000).

Reads: Set **ropes** to **1**

| Parameter | What | Kind |
|---|---|---|
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the variable | variable; a variable of type number or switch or text or wolf or resident or place or region or item or faction |
| `value` | the value | operand |

```json
{"do": "set", "var": "ropes", "value": 1}
```

<a id="action-add-var"></a>**`add`** (add var): Add to a number (v2)

Adds to a number variable (a negative number takes away).

Reads: Add **1** to **ropes**

| Parameter | What | Kind |
|---|---|---|
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the number | variable; a variable of type number |
| `value` | the number | operand |

```json
{"do": "add", "var": "ropes", "value": 1}
```

<a id="action-subtract"></a>**`subtract`**: Take from a number (v2)

Subtracts from a number variable.

Reads: Take **1** from **ropes**

| Parameter | What | Kind |
|---|---|---|
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the number | variable; a variable of type number |
| `value` | the number | operand |

```json
{"do": "subtract", "var": "ropes", "value": 1}
```

<a id="action-multiply"></a>**`multiply`**: Multiply a number (v2)

Multiplies a number variable.

Reads: Multiply **ropes** by **1**

| Parameter | What | Kind |
|---|---|---|
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the number | variable; a variable of type number |
| `value` | the number | operand |

```json
{"do": "multiply", "var": "ropes", "value": 1}
```

<a id="action-clamp"></a>**`clamp`**: Keep a number between two (v2)

Raises a number to the lower bound, or lowers it to the upper one.

Reads: Keep **ropes** between **1** and **1**

| Parameter | What | Kind |
|---|---|---|
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the number | variable; a variable of type number |
| `min` | at least | int; -1000000000 to 1000000000 |
| `max` | at most | int; -1000000000 to 1000000000 |

```json
{"do": "clamp", "var": "ropes", "min": 1, "max": 1}
```

<a id="action-turn-on"></a>**`turn on`**: Turn a switch on (v2)

A switch, on.

Reads: Turn on **ropes**

| Parameter | What | Kind |
|---|---|---|
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the switch | variable; a variable of type switch |

```json
{"do": "turn on", "var": "ropes"}
```

<a id="action-turn-off"></a>**`turn off`**: Turn a switch off (v2)

A switch, off.

Reads: Turn off **ropes**

| Parameter | What | Kind |
|---|---|---|
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the switch | variable; a variable of type switch |

```json
{"do": "turn off", "var": "ropes"}
```

<a id="action-toggle"></a>**`toggle`**: Flip a switch (v2)

A switch, the other way.

Reads: Flip **ropes**

| Parameter | What | Kind |
|---|---|---|
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the switch | variable; a variable of type switch |

```json
{"do": "toggle", "var": "ropes"}
```

<a id="action-remember"></a>**`remember`**: Remember who or what (v2)

Keeps an id from the event (the acting wolf, the target, the item, the place, the detail) or the loop's value in a variable: the wolf who found the pup.

Reads: Remember **the acting wolf** as **ropes**

| Parameter | What | Kind |
|---|---|---|
| `from` | what | choice; one of: `actor`, `target`, `item`, `cell`, `detail`, `each` |
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the variable | variable; a variable of type text or wolf or resident or place or region or item or faction |

```json
{"do": "remember", "from": "actor", "var": "ropes"}
```

<a id="action-list-add"></a>**`list add`**: Add to a list (v2)

Adds a value to a list variable, once (a list holds up to 500).

Reads: Add **1** to the list **ropes**

| Parameter | What | Kind |
|---|---|---|
| `value` | the value | operand |
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the list | variable; a variable of type numbers or texts or wolves or residents |

```json
{"do": "list add", "value": 1, "var": "ropes"}
```

<a id="action-list-remove"></a>**`list remove`**: Take out of a list (v2)

Removes a value from a list variable.

Reads: Take **1** out of the list **ropes**

| Parameter | What | Kind |
|---|---|---|
| `value` | the value | operand |
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the list | variable; a variable of type numbers or texts or wolves or residents |

```json
{"do": "list remove", "value": 1, "var": "ropes"}
```

<a id="action-list-clear"></a>**`list clear`**: Empty a list (v2)

Removes every value.

Reads: Empty the list **ropes**

| Parameter | What | Kind |
|---|---|---|
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the list | variable; a variable of type numbers or texts or wolves or residents |

```json
{"do": "list clear", "var": "ropes"}
```

<a id="action-pick"></a>**`pick`**: Pick one from a list (v2)

Puts one value of a list, chosen at random (seeded), into a variable of its kind. An empty list leaves it as it was.

Reads: Pick one of **ropes** at random as **ropes**

| Parameter | What | Kind |
|---|---|---|
| `list` | the list | variable; a variable of type numbers or texts or wolves or residents |
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the variable | variable; a variable of type number or text or wolf or resident |

```json
{"do": "pick", "list": "ropes", "var": "ropes"}
```

<a id="action-start-timer"></a>**`start timer`**: Start a timer variable (v2)

Sets a timer variable to now (its clock); Days since and "since" count from it.

Reads: Start **ropes** counting from now

| Parameter | What | Kind |
|---|---|---|
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the timer | variable; a variable of type timer |

```json
{"do": "start timer", "var": "ropes"}
```

<a id="action-stop-timer"></a>**`stop timer`**: Clear a timer variable (v2)

Unsets a timer variable: Days since it is false until it starts again.

Reads: Clear **ropes**

| Parameter | What | Kind |
|---|---|---|
| `of` | whose | whose; one of: `$actor`, `$each`; optional; default "$actor" |
| `var` | the timer | variable; a variable of type timer |

```json
{"do": "stop timer", "var": "ropes"}
```

#### Signals and timers

<a id="action-emit"></a>**`emit`**: Send a signal

A named signal, delivered after this event's triggers; it keeps the event's actor, item, target, place, amounts and detail.

Reads: Send the signal **gate-opens**

| Parameter | What | Kind |
|---|---|---|
| `signal` | the signal | signal |

```json
{"do": "emit", "signal": "gate-opens"}
```

<a id="action-schedule"></a>**`schedule`**: Start a timer

A named timer that sends the signal when due. The same name for the same owner replaces the deadline. The shortest delay is a minute (1/1440 of a day).

Reads: Start the timer **gate-opens** to go off in **1** days on the **world's** clock, **shared by the story**, for **this phase visit****false**

| Parameter | What | Kind |
|---|---|---|
| `signal` | the signal | signal |
| `days` | days | days |
| `clock` | the clock | choice; one of: `game`, `real`; default "game" |
| `owner` | whose | choice; one of: `story`, `player`; default "story" |
| `scope` | its life | choice; one of: `visit`, `campaign`; default "visit" |
| `repeat` | repeats | bool; default false |

```json
{"do": "schedule", "signal": "gate-opens", "days": 1, "clock": "game", "owner": "story", "scope": "visit", "repeat": false}
```

<a id="action-cancel"></a>**`cancel`**: Cancel a timer

Stops that timer.

Reads: Cancel the timer **gate-opens** (**shared**)

| Parameter | What | Kind |
|---|---|---|
| `signal` | the timer | signal |
| `owner` | whose | choice; one of: `story`, `player`; default "story" |

```json
{"do": "cancel", "signal": "gate-opens", "owner": "story"}
```

<a id="action-shift"></a>**`shift`**: Move a timer (v2)

Moves a running timer's deadline later, or earlier with a negative number.

Reads: Move the timer **gate-opens** (**shared**) by **1** days

| Parameter | What | Kind |
|---|---|---|
| `signal` | the timer | signal |
| `owner` | whose | choice; one of: `story`, `player`; default "story" |
| `days` | days | days |

```json
{"do": "shift", "signal": "gate-opens", "owner": "story", "days": 1}
```

#### Flow

Version 2 only. Nested lists of actions: up to 24 actions in a trigger counting nested ones, three deep.

<a id="action-if"></a>**`if`**: If, else if, else (v2)

Runs the actions under the first condition that holds (If, then each Else if), or those under Else. Nests three deep with For each and Random.

```json
{"do": "if", "if": {"test": "compare", "left": {"var": "ropes"}, "op": ">=", "right": 2}, "then": [{"do": "note", "text": "Enough rope."}], "else": [{"do": "add", "var": "ropes", "value": 1}]}
```

<a id="action-wait"></a>**`wait`**: Wait (v2)

The rest of the actions run later, for the same wolf and with the same event. It ends with the phase visit, unless it is a campaign trigger's; it never holds up another trigger. At least a minute of the world's clock, or 5 seconds of the real one (coarse: tight timing belongs in a scene). Not inside For each.

Reads: Wait **1** days on the **world's** clock

| Parameter | What | Kind |
|---|---|---|
| `days` | days | days |
| `clock` | the clock | choice; one of: `game`, `real`; default "game" |

```json
{"do": "wait", "days": 1, "clock": "game"}
```

<a id="action-each"></a>**`each`**: For each (v2)

Runs the actions once for each wolf of the story, of the acting wolf's party, or each value of a list (500 at most). The value is $each: the looped wolf, in effects and in variables of each wolf. Not inside another For each.

Reads: For each of **the wolves of the story**

| Parameter | What | Kind |
|---|---|---|
| `over` | what | choice; one of: `participants`, `party`, `list`; default "participants" |
| `list` | the list | variable; a variable of type numbers or texts or wolves or residents; optional |

```json
{"do": "each", "over": "participants", "actions": [{"do": "effect", "effect": {"kind": "deed.award", "player": "$each", "detail": "Held the ford."}}]}
```

<a id="action-random"></a>**`random`**: Random (v2)

Runs one of 2 to 8 lists of actions, each as likely as its weight. Seeded: a replay or a rehearsal gives the same pick.

```json
{"do": "random", "choices": [{"weight": 3, "actions": [{"do": "emit", "signal": "fair-weather"}]}, {"weight": 1, "actions": [{"do": "emit", "signal": "storm"}]}]}
```

<a id="action-run"></a>**`run`**: Run another trigger (v2)

Runs another trigger's actions now, for the same wolf and event, checking whether it is on and its conditions, or not. A trigger may not run itself, or one that runs it back.

Reads: Run the trigger **count-rope**, **whatever its conditions**

| Parameter | What | Kind |
|---|---|---|
| `trigger` | the trigger | trigger |
| `check` | checked | bool; default false |

```json
{"do": "run", "trigger": "count-rope", "check": false}
```

<a id="action-enable"></a>**`enable`**: Turn a trigger on (v2)

Turns a trigger on (one that starts off, or one turned off). From the next event: the triggers of the event that turned it on have been chosen.

Reads: Turn on the trigger **count-rope**

| Parameter | What | Kind |
|---|---|---|
| `trigger` | the trigger | trigger |
| `phase` | of the phase | phase; optional |

```json
{"do": "enable", "trigger": "count-rope"}
```

<a id="action-disable"></a>**`disable`**: Turn a trigger off (v2)

Turns a trigger off until turned on again (it stays off on a later visit of its phase).

Reads: Turn off the trigger **count-rope**

| Parameter | What | Kind |
|---|---|---|
| `trigger` | the trigger | trigger |
| `phase` | of the phase | phase; optional |

```json
{"do": "disable", "trigger": "count-rope"}
```

<a id="action-stop"></a>**`stop`**: Skip the rest (v2)

Ends this trigger's actions here.

```json
{"do": "stop"}
```

<a id="action-note"></a>**`note`**: A note for the DM (v2)

A line in the campaign's timeline and the rehearsal's trace, and nowhere in the world: the debugging message.

Reads: Note for the DM: **The bells ring for the ford.**

| Parameter | What | Kind |
|---|---|---|
| `text` | the note | text; up to 300 characters |

```json
{"do": "note", "text": "The bells ring for the ford."}
```

<a id="action-finish"></a>**`finish`**: End the story (v2)

Ends the run with an outcome and a line for the chronicle; for one wolf, gives them their own ending and the story goes on.

Reads: End the story for **everyone**: **success**, **The bells ring for the ford.**

| Parameter | What | Kind |
|---|---|---|
| `who` | for whom | choice; one of: `everyone`, `$actor`, `$each`; default "everyone" |
| `outcome` | the outcome | choice; one of: `success`, `failure`, `abandoned` |
| `text` | the line | text; up to 300 characters |

```json
{"do": "finish", "who": "everyone", "outcome": "success", "text": "The bells ring for the ford."}
```

<a id="action-effect"></a>**`effect`**: Change the world

Asks the game to change the world: one of the world effects below. Each is a DM order the game checks again when it applies it.

Reads: **{"kind": "deed.award", "player": "$actor", "detail": "Kept the ford."}**

| Parameter | What | Kind |
|---|---|---|
| `effect` | the world effect | effect |

```json
{"do": "effect", "effect": {"kind": "deed.award", "player": "$actor", "detail": "Kept the ford."}}
```

<a id="action-enter"></a>**`enter`**: Go to a phase (must be last)

Moves the story to a phase. Must be the last action.

Reads: Go to the phase **muster**

| Parameter | What | Kind |
|---|---|---|
| `phase` | the phase | phase |

```json
{"do": "enter", "phase": "muster"}
```

### Effects

78 world effects, each one action `{"do": "effect", "effect": {...}}` (or, for a phase, one of what it sends as it opens). Each becomes an order the game applies and may refuse, with a receipt, except the story's own. On any effect the game applies, `ifRefused` says what the story does when it is refused: `pause` (the default), `skip`, or `when here` (an effect to one wolf, held until they are next in the world, `waitDays` 1 to 90).

#### Telling the wolves

<a id="effect-story-tell"></a>**`story.tell`**: Tell the wolves

A line to one wolf, a wolf's party, the story's wolves, the wolves standing in a region, or everyone in the world: a banner and a line in their log. A wolf of the story who is away is told when back; nobody away refuses it.

Reads: Tell **the story's wolves**: **The bells ring for the ford.**

| Parameter | What | Kind |
|---|---|---|
| `to` | who is told | choice; one of: `participants`, `wolf`, `party`, `region`, `world` |
| `player` | the wolf | wolf; optional |
| `region` | the region | region; optional |
| `text` | the words | text; up to 300 characters |
| `from` | said by (a resident) | resident; optional |

```json
{"do": "effect", "effect": {"kind": "story.tell", "to": "participants", "text": "The bells ring for the ford."}}
```

<a id="effect-journal-note"></a>**`journal.note`**: Add a note to the journal

A line under the story in the wolf's journal (or every wolf's of the story), kept with the story: the newest 12.

Reads: Note in the journal of: **The bells ring for the ford.**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf; optional |
| `text` | the note | text; up to 200 characters |

```json
{"do": "effect", "effect": {"kind": "journal.note", "text": "The bells ring for the ford."}}
```

<a id="effect-story-ping"></a>**`story.ping`**: Mark a place on the map (undone at the end)

A marker on the minimap and the World Map of the story's wolves (or one wolf's) for a while. Undone when the story ends.

Reads: Mark **greyfen-square, 1, 1** on the maps of as **The ford** for **1** minutes

| Parameter | What | Kind |
|---|---|---|
| `cell` | the place | place |
| `x` | tile x | int; 0 to 4095 |
| `y` | tile y | int; 0 to 4095 |
| `player` | the wolf | wolf; optional |
| `label` | the label | text; up to 40 characters |
| `minutes` | minutes | int; 1 to 240 |

```json
{"do": "effect", "effect": {"kind": "story.ping", "cell": "greyfen-square", "x": 1, "y": 1, "label": "The ford", "minutes": 1}}
```

<a id="effect-story-tracker"></a>**`story.tracker`**: Show or hide a tracker (v2, the story's own)

A shown variable (its "shown as" label) among the story's trackers in the wolves' journal: shown again, or hidden for now.

Reads: **Hide** the tracker **ropes**

| Parameter | What | Kind |
|---|---|---|
| `shown` | show or hide | bool |
| `var` | the variable | variable; a variable of type number or switch |

```json
{"do": "effect", "effect": {"kind": "story.tracker", "shown": false, "var": "ropes"}}
```

#### Residents

<a id="effect-npc-line"></a>**`npc.line`**: Give a resident a dialogue line (phase entry only)

Phase entry only: while the phase is open the resident has this line for the wolves of the story.

Reads: **rm111** has the line **The bells ring for the ford.** for the story's wolves

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `text` | the line | text; up to 200 characters |

```json
{"do": "effect", "effect": {"kind": "npc.line", "npc": "rm111", "text": "The bells ring for the ford."}}
```

<a id="effect-npc-walk"></a>**`npc.walk`**: A resident walks somewhere

The resident goes there; "scene arrived" follows (or "scene blocked" after 180 seconds without progress).

Reads: **rm111** walks to **greyfen-square, 1, 1** at a **walk**, cue **begin**

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `cell` | the place | place |
| `x` | tile x | int; 0 to 4095 |
| `y` | tile y | int; 0 to 4095 |
| `pace` | pace | choice; one of: `walk`, `run` |
| `cue` | the cue | cue |

```json
{"do": "effect", "effect": {"kind": "npc.walk", "npc": "rm111", "cell": "greyfen-square", "x": 1, "y": 1, "pace": "walk", "cue": "begin"}}
```

<a id="effect-npc-speak"></a>**`npc.speak`**: A resident speaks

The resident says it aloud; "scene spoken" follows.

Reads: **rm111** says **The bells ring for the ford.**, cue **begin**

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `text` | the words | text; up to 200 characters |
| `cue` | the cue | cue |

```json
{"do": "effect", "effect": {"kind": "npc.speak", "npc": "rm111", "text": "The bells ring for the ford.", "cue": "begin"}}
```

<a id="effect-npc-pose"></a>**`npc.pose`**: A resident takes a posture

The resident stands, sits or lies down; "scene posed" follows.

Reads: **rm111** is **standing**, cue **begin**

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `posture` | posture | choice; one of: `standing`, `sitting`, `lying` |
| `cue` | the cue | cue |

```json
{"do": "effect", "effect": {"kind": "npc.pose", "npc": "rm111", "posture": "standing", "cue": "begin"}}
```

<a id="effect-npc-move"></a>**`npc.move`**: Put a resident somewhere

The resident is there at once (no walk).

Reads: Put **rm111** at **greyfen-square, 1, 1**

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `cell` | the place | place |
| `x` | tile x | int; 0 to 4095 |
| `y` | tile y | int; 0 to 4095 |

```json
{"do": "effect", "effect": {"kind": "npc.move", "npc": "rm111", "cell": "greyfen-square", "x": 1, "y": 1}}
```

<a id="effect-npc-protect"></a>**`npc.protect`**: Protect a resident

Marks the resident as one who cannot be killed: for good (until Unprotect), while this story runs, or while its campaign runs (let go when the campaign ends).

Reads: **rm111** cannot be killed

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `lasts` | for | choice; one of: `ever`, `run`, `campaign`; optional; default "ever" |

```json
{"do": "effect", "effect": {"kind": "npc.protect", "npc": "rm111"}}
```

<a id="effect-npc-unprotect"></a>**`npc.unprotect`**: Remove a resident's protection

Takes the mark off. When this story protected them for its run or its campaign, lets go of that protection only: one protected in their own right, or by the Dungeon Master, stays protected.

Reads: **rm111** can be killed again

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |

```json
{"do": "effect", "effect": {"kind": "npc.unprotect", "npc": "rm111"}}
```

<a id="effect-npc-spare"></a>**`npc.spare`**: Spare a resident from old age

Old age passes the resident by (stories and fights still can't kill a protected one: Protect them too). For this story, or for its whole campaign (let go when the campaign ends).

Reads: **rm111** does not die of old age while

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `lasts` | for | choice; one of: `run`, `campaign`; optional; default "run" |

```json
{"do": "effect", "effect": {"kind": "npc.spare", "npc": "rm111"}}
```

<a id="effect-npc-unspare"></a>**`npc.unspare`**: Let old age take its course

Ends this story's (or its campaign's) sparing; another story may still spare them.

Reads: **rm111** is no longer spared by this story

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |

```json
{"do": "effect", "effect": {"kind": "npc.unspare", "npc": "rm111"}}
```

<a id="effect-npc-revive"></a>**`npc.revive`**: Revive a resident

Brings a dead resident back.

Reads: Bring **rm111** back to life

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |

```json
{"do": "effect", "effect": {"kind": "npc.revive", "npc": "rm111"}}
```

<a id="effect-npc-offer"></a>**`npc.offer`**: Offer a choice in a resident's window (v2, the story's own)

While this phase visit is open, the resident's dialogue window shows a story choice to the wolf (or every wolf of the story): the wolf's words, the resident's reply in their own voice. Picking it makes a 'dialogue choice' event. Offered again to another wolf with the same name, it is added for them; each wolf sees it until they pick it (once) or the visit ends.

Reads: **rm111** offers the choice **ask-about-the-ford** to: the wolf says **the ford is open again**, **rm111** replies **A few words.** (once: )

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `id` | the choice's name | offer |
| `words` | the wolf's words | text; up to 120 characters |
| `reply` | the resident's reply | text; up to 300 characters |
| `player` | who sees it (empty: every wolf of the story) | wolf; optional |
| `once` | once for each wolf | bool; optional |

```json
{"do": "effect", "effect": {"kind": "npc.offer", "npc": "rm111", "id": "ask-about-the-ford", "words": "the ford is open again", "reply": "A few words."}}
```

<a id="effect-visitor-add"></a>**`visitor.add`**: Bring a stranger (undone at the end)

A temporary resident appears at the tile for a while, then goes (the spawn). Their id is the story's own: Send away names it. Sent away when the story ends.

Reads: **The Pale Rider** (**pale-rider**) comes to **greyfen-square, 12, 9** for **60** minutes, looking like **rm111**

| Parameter | What | Kind |
|---|---|---|
| `visitor` | their id | visitor |
| `name` | their name | text; up to 60 characters |
| `cell` | the place | place |
| `x` | tile x | int; 0 to 4095 |
| `y` | tile y | int; 0 to 4095 |
| `minutes` | minutes | int; 1 to 1440 |
| `like` | looking like (a resident) | resident; optional |
| `description` | how they look | text; up to 200 characters; optional |

```json
{"do": "effect", "effect": {"kind": "visitor.add", "visitor": "pale-rider", "name": "The Pale Rider", "like": "rm111", "cell": "greyfen-square", "x": 12, "y": 9, "minutes": 60}}
```

<a id="effect-visitor-leave"></a>**`visitor.leave`**: Send a stranger away

A temporary resident the story brought goes.

Reads: Send **stranger-1** away

| Parameter | What | Kind |
|---|---|---|
| `visitor` | their id | visitor |

```json
{"do": "effect", "effect": {"kind": "visitor.leave", "visitor": "stranger-1"}}
```

<a id="effect-mantle-assign"></a>**`mantle.assign`**: Bind a mantle (undone at the end)

A mantle (doc 84: a part with its own lines in the dialogue window) bound to the resident. Released when the story ends.

Reads: **rm111** takes up the mantle **ferryman**

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `mantle` | the mantle | mantle |

```json
{"do": "effect", "effect": {"kind": "mantle.assign", "npc": "rm111", "mantle": "ferryman"}}
```

<a id="effect-mantle-release"></a>**`mantle.release`**: Release a mantle

The resident's mantle is released.

Reads: **rm111** lays down their mantle

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |

```json
{"do": "effect", "effect": {"kind": "mantle.release", "npc": "rm111"}}
```

<a id="effect-npc-kill"></a>**`npc.kill`**: A resident dies

The resident dies where they stand (Bring back undoes it). Never a protected resident.

Reads: **rm111** dies

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |

```json
{"do": "effect", "effect": {"kind": "npc.kill", "npc": "rm111"}}
```

<a id="effect-npc-hold"></a>**`npc.hold`**: Hold a resident (undone at the end)

Off their schedule: they stay where they stand (or where a walk takes them) for the story, and no other story may direct them. Released by Release, or when the story ends.

Reads: Hold **rm111** where they are

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |

```json
{"do": "effect", "effect": {"kind": "npc.hold", "npc": "rm111"}}
```

<a id="effect-npc-release"></a>**`npc.release`**: Release a resident

A resident the story held goes back to their day.

Reads: Release **rm111**

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |

```json
{"do": "effect", "effect": {"kind": "npc.release", "npc": "rm111"}}
```

<a id="effect-npc-follow"></a>**`npc.follow`**: A resident follows a wolf (undone at the end)

The resident joins the wolf's party (or makes one with them) for the story's sake and follows. Refused when the party is full or the resident cannot leave. They go home when the story ends.

Reads: **rm111** travels with **$actor**

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |
| `player` | the wolf | wolf |

```json
{"do": "effect", "effect": {"kind": "npc.follow", "npc": "rm111", "player": "$actor"}}
```

<a id="effect-npc-unfollow"></a>**`npc.unfollow`**: A resident stops following

A resident the story sent along goes home.

Reads: **rm111** stops travelling with the party

| Parameter | What | Kind |
|---|---|---|
| `npc` | the resident | resident |

```json
{"do": "effect", "effect": {"kind": "npc.unfollow", "npc": "rm111"}}
```

#### Talk, rumour and figures

<a id="effect-talk-remember"></a>**`talk.remember`**: Residents remember something

Talk memory residents' lines can test (world.mem.<key> for the realm or a town, self.mem.<key> for one resident, pair.mem.<key> for one resident about one wolf: Data/Talk lines gated on it play). Realm: every resident; a town: its residents (the whole city is one town); a resident: that one; a resident and a wolf: that resident, when that wolf is the one they talk to. Taken back when the story ends unless kept.

Reads: **the town of** **greyfen** remembers **ford_open** = **yes** for **30** days (after the story: )

| Parameter | What | Kind |
|---|---|---|
| `scope` | who | choice; one of: `realm`, `town`, `npc`, `pair`; default "town" |
| `town` | the town | town; optional |
| `npc` | the resident | resident; optional |
| `player` | the wolf | wolf; optional |
| `key` | what (a key) | talkkey |
| `value` | its value | talkvalue |
| `days` | days | int; 1 to 365 |
| `keep` | kept after the story | bool; optional |

```json
{"do": "effect", "effect": {"kind": "talk.remember", "scope": "town", "town": "greyfen", "key": "ford_open", "value": "yes", "days": 30}}
```

<a id="effect-talk-forget"></a>**`talk.forget`**: Residents forget something

Takes away a memory a story gave (any story's, by key).

Reads: **the town of** **greyfen** forgets **ford_open**

| Parameter | What | Kind |
|---|---|---|
| `scope` | who | choice; one of: `realm`, `town`, `npc`, `pair`; default "town" |
| `town` | the town | town; optional |
| `npc` | the resident | resident; optional |
| `player` | the wolf | wolf; optional |
| `key` | what (a key) | talkkey |

```json
{"do": "effect", "effect": {"kind": "talk.forget", "scope": "town", "town": "greyfen", "key": "ford_open"}}
```

<a id="effect-belief-plant"></a>**`belief.plant`**: Start a rumour

Residents of a town (chosen by the story's seed: placed, living, not the one it is about) believe something of a resident, as if they had heard it: it is told in gossip ('#x.name# <claim>') and spreads along friendships and caravans as any belief does. About residents only, never a wolf. A clearing (story.cleared) is told as '<Name> never <claim>': write the claim as the thing not done.

Reads: **6** residents of **greyfen** come to believe **rm111** **was seen at the ford at midnight** (**60**% sure)

| Parameter | What | Kind |
|---|---|---|
| `subject` | about | resident |
| `claim` | that they (a phrase) | text; up to 120 characters |
| `town` | in the town | town |
| `count` | how many | int; 1 to 50 |
| `confidence` | how sure (%) | int; 10 to 90 |
| `claimKind` | the kind of claim | choice; one of: `story.rumour`, `story.cleared`; optional; default "story.rumour" |

```json
{"do": "effect", "effect": {"kind": "belief.plant", "subject": "rm111", "claim": "was seen at the ford at midnight", "town": "greyfen", "count": 6, "confidence": 60}}
```

<a id="effect-figure-set"></a>**`figure.set`**: Name a figure of the realm (needs a builder)

Who residents mean by the ruler, the lord, the captain, the high priest, the hero or the villain (#world.figure_<role># in their lines), where they are known. A resident, a wolf, or a name only. It stays after the story; Clear the figure takes it away.

Reads: The figure **ford_villain**: **rm111** is the realm's **villain**, **the ford breaker**, known **in** **greyfen** ()

| Parameter | What | Kind |
|---|---|---|
| `figure` | its id | figure |
| `role` | the role | choice; one of: `ruler`, `lord`, `captain`, `high_priest`, `hero`, `villain` |
| `npc` | a resident | resident; optional |
| `player` | a wolf | wolf; optional |
| `name` | or a name | text; up to 120 characters; optional |
| `title` | their title | text; up to 120 characters; optional |
| `known` | known | choice; one of: `realm`, `region`, `house`; default "realm" |
| `region` | the town or land | town; optional |
| `house` | the House | faction; optional |
| `about` | about them | text; up to 600 characters; optional |

```json
{"do": "effect", "effect": {"kind": "figure.set", "figure": "ford_villain", "role": "villain", "npc": "rm111", "title": "the ford breaker", "known": "region", "region": "greyfen"}}
```

<a id="effect-figure-clear"></a>**`figure.clear`**: Clear a figure (needs a builder)

Takes a figure away (one a story or a Dungeon Master set).

Reads: Residents no longer speak of the figure **ford_villain**

| Parameter | What | Kind |
|---|---|---|
| `figure` | its id | figure |

```json
{"do": "effect", "effect": {"kind": "figure.clear", "figure": "ford_villain"}}
```

#### Players' wolves

<a id="effect-character-injury"></a>**`character.injury`**: Injure a wolf (acts on wolves)

Acts on players' wolves: needs "may act on players' wolves" on a hosted world.

Reads: Give **$actor** a **torn_flank** of severity **1**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |
| `add` | the injury | choice; one of: `torn_flank`, `bitten_foreleg`, `bitten_hindleg`, `torn_ear_acute`, `wrenched_neck`, `deep_gash`, `cut_foreleg`, `cut_muzzle`, `cut_shoulder`, `bruised_ribs`, `cracked_rib`, `sprained_foreleg`, `knocked_senseless`, `burned_paws`, `singed_coat`, `burned_muzzle`, `torn_ear`, `bent_tail`, `scarred_muzzle`, `scarred_flank`, `burn_scars`, `notched_nose`, `clouded_eye`, `permanent_limp`, `bad_back`, `stiff_shoulder` |
| `severity` | severity | int; 1 to 3 |

```json
{"do": "effect", "effect": {"kind": "character.injury", "player": "$actor", "add": "torn_flank", "severity": 1}}
```

<a id="effect-bandits-call"></a>**`bandits.call`**: Send bandits after a wolf

The wolf must be in the world.

Reads: **1** bandits come for **$actor**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |
| `count` | how many | int; 1 to 6 |

```json
{"do": "effect", "effect": {"kind": "bandits.call", "player": "$actor", "count": 1}}
```

<a id="effect-story-practice"></a>**`story.practice`**: Give a wolf a lesson

A lesson under the ordinary practice rules; the wolf must be in the world.

Reads: **$actor** practises **craft**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |
| `skill` | the skill | choice; one of: `craft`, `commerce`, `labour`, `service`, `travel`, `watch` |

```json
{"do": "effect", "effect": {"kind": "story.practice", "player": "$actor", "skill": "craft"}}
```

<a id="effect-deed-award"></a>**`deed.award`**: Award a deed

A deed on the wolf's fame; the wolf must be in the world.

Reads: Award **$actor** the deed **Kept the ford.**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |
| `detail` | the deed | text; up to 200 characters |

```json
{"do": "effect", "effect": {"kind": "deed.award", "player": "$actor", "detail": "Kept the ford."}}
```

<a id="effect-participant-add"></a>**`participant.add`**: Take a wolf into the story (v2, the story's own)

The wolf joins the story's audience (an open audience's way in), up to its cap: they see it in their journal.

Reads: Take **$actor** into the story

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |

```json
{"do": "effect", "effect": {"kind": "participant.add", "player": "$actor"}}
```

<a id="effect-participant-remove"></a>**`participant.remove`**: Take a wolf out of the story (v2, the story's own)

The wolf leaves the story's audience; quests they hold are abandoned.

Reads: Take **$actor** out of the story

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |

```json
{"do": "effect", "effect": {"kind": "participant.remove", "player": "$actor"}}
```

<a id="effect-character-grant"></a>**`character.grant`**: Pay a wolf from a purse

Coins from a named purse (a town's treasury, a business's till, a Chapter), never made from nothing: refused when the purse is short or over the wolf's day of grants. Approval checks the world has the purse.

Reads: Pay **$actor** **12** pennies from **town:greyfen**: **the carter thanks you**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |
| `coins` | pennies | int; 1 to 2500 |
| `purse` | who pays | purse |
| `reason` | the reason | text; up to 80 characters |

```json
{"do": "effect", "effect": {"kind": "character.grant", "player": "$actor", "coins": 12, "purse": "town:greyfen", "reason": "the carter thanks you"}}
```

<a id="effect-item-give"></a>**`item.give`**: Give a wolf an item

An item into one wolf's pack; a full pack is refused and the wolf told.

Reads: Give **$actor** **1** **rope**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |
| `item` | the item | item |
| `quantity` | how many | int; 1 to 20 |

```json
{"do": "effect", "effect": {"kind": "item.give", "player": "$actor", "item": "rope", "quantity": 1}}
```

<a id="effect-standing-adjust"></a>**`standing.adjust`**: Change a wolf's standing (needs a builder)

A few points of standing with a faction, for a reason the wolf is told. A builder or the owner must approve it on a hosted world.

Reads: Change **$actor**'s standing with **watch** by **1**: **for the ford**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |
| `faction` | the faction | faction |
| `points` | points | int; -10 to 10, not 0 |
| `reason` | the reason | text; up to 80 characters |

```json
{"do": "effect", "effect": {"kind": "standing.adjust", "player": "$actor", "faction": "watch", "points": 1, "reason": "for the ford"}}
```

<a id="effect-item-take"></a>**`item.take`**: Take an item from a wolf (acts on wolves)

Out of one wolf's pack (gone, not paid for). Refused when they carry fewer. Acts on players' wolves: needs "may act on players' wolves" on a hosted world.

Reads: Take **1** **rope** from **$actor**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |
| `item` | the item | item |
| `quantity` | how many | int; 1 to 20 |

```json
{"do": "effect", "effect": {"kind": "item.take", "player": "$actor", "item": "rope", "quantity": 1}}
```

<a id="effect-warrant-issue"></a>**`warrant.issue`**: Mark a wolf wanted (acts on wolves)

A warrant from a town's watch, as for a crime: its guards stop the wolf for the fine or the gaol. Acts on players' wolves: needs "may act on players' wolves" on a hosted world.

Reads: **$actor** is wanted by the watch of **greyfen**, fined **1**p: **for the ford**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |
| `town` | the town | town |
| `fine` | the fine (pennies) | int; 0 to 200 |
| `reason` | what for | text; up to 80 characters |

```json
{"do": "effect", "effect": {"kind": "warrant.issue", "player": "$actor", "town": "greyfen", "fine": 1, "reason": "for the ford"}}
```

<a id="effect-warrant-clear"></a>**`warrant.clear`**: Lift a warrant

The town's watch no longer wants the wolf (what was owed is forgiven). Refused when there is no warrant.

Reads: Lift the warrant on **$actor** in **greyfen**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |
| `town` | the town | town |

```json
{"do": "effect", "effect": {"kind": "warrant.clear", "player": "$actor", "town": "greyfen"}}
```

<a id="effect-deed-revoke"></a>**`deed.revoke`**: Revoke a deed

The wolf's newest deed awarded by a Dungeon Master or a story in these words is struck from their fame.

Reads: Revoke **$actor**'s deed **Kept the ford.**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |
| `detail` | the deed's words | text; up to 200 characters |

```json
{"do": "effect", "effect": {"kind": "deed.revoke", "player": "$actor", "detail": "Kept the ford."}}
```

#### Places and regions

<a id="effect-story-area-damage"></a>**`story.area.damage`**: Hurt the wolves in a region (acts on wolves)

Acts on players' wolves: needs "may act on players' wolves" on a hosted world.

Reads: Hurt the story's wolves inside **square** by **1** points

| Parameter | What | Kind |
|---|---|---|
| `region` | the region | region |
| `points` | points | int; 1 to 99 |

```json
{"do": "effect", "effect": {"kind": "story.area.damage", "region": "square", "points": 1}}
```

<a id="effect-story-area-item"></a>**`story.area.item`**: Give an item to the wolves in a region

A full pack is skipped and the wolf told.

Reads: Give each of the story's wolves inside **square** **1** **rope**

| Parameter | What | Kind |
|---|---|---|
| `region` | the region | region |
| `item` | the item | item |
| `quantity` | how many | int; 1 to 100 |

```json
{"do": "effect", "effect": {"kind": "story.area.item", "region": "square", "item": "rope", "quantity": 1}}
```

<a id="effect-story-area-buff"></a>**`story.area.buff`**: Feed the wolves in a region

The timed "fed" benefit.

Reads: The story's wolves inside **square** are **fed** for **1** hours

| Parameter | What | Kind |
|---|---|---|
| `region` | the region | region |
| `buff` | the benefit | choice; one of: `fed` |
| `hours` | hours | int; 1 to 168 |

```json
{"do": "effect", "effect": {"kind": "story.area.buff", "region": "square", "buff": "fed", "hours": 1}}
```

<a id="effect-story-area-spawn"></a>**`story.area.spawn`**: A bandit camp in a region

Bandits camp in the rectangle.

Reads: A camp of **1** bandits in **square**

| Parameter | What | Kind |
|---|---|---|
| `region` | the region | region |
| `count` | how many | int; 1 to 6 |

```json
{"do": "effect", "effect": {"kind": "story.area.spawn", "region": "square", "count": 1}}
```

<a id="effect-door-lock"></a>**`door.lock`**: Lock a door (undone at the end)

Both sides of the door are locked (it closes first). Unlocked again when the story ends.

Reads: Lock **link_inn_door_a**

| Parameter | What | Kind |
|---|---|---|
| `door` | the door | door |

```json
{"do": "effect", "effect": {"kind": "door.lock", "door": "link_inn_door_a"}}
```

<a id="effect-door-unlock"></a>**`door.unlock`**: Unlock a door (undone at the end)

Both sides of a locked door are unlocked. Locked again when the story ends.

Reads: Unlock **link_inn_door_a**

| Parameter | What | Kind |
|---|---|---|
| `door` | the door | door |

```json
{"do": "effect", "effect": {"kind": "door.unlock", "door": "link_inn_door_a"}}
```

<a id="effect-door-open"></a>**`door.open`**: Open a door

The door stands open (an unlocked one). It stays as a wolf would leave it.

Reads: Open **link_inn_door_a**

| Parameter | What | Kind |
|---|---|---|
| `door` | the door | door |

```json
{"do": "effect", "effect": {"kind": "door.open", "door": "link_inn_door_a"}}
```

<a id="effect-door-close"></a>**`door.close`**: Close a door

The door is shut. It stays as a wolf would leave it.

Reads: Close **link_inn_door_a**

| Parameter | What | Kind |
|---|---|---|
| `door` | the door | door |

```json
{"do": "effect", "effect": {"kind": "door.close", "door": "link_inn_door_a"}}
```

<a id="effect-item-place"></a>**`item.place`**: Leave an item on the ground

Things to be found and picked up, by anyone. They lie there until taken (or the server restarts).

Reads: Leave **1** **rope** on the ground at **greyfen-square, 1, 1**

| Parameter | What | Kind |
|---|---|---|
| `item` | the item | item |
| `quantity` | how many | int; 1 to 5 |
| `cell` | the place | place |
| `x` | tile x | int; 0 to 4095 |
| `y` | tile y | int; 0 to 4095 |

```json
{"do": "effect", "effect": {"kind": "item.place", "item": "rope", "quantity": 1, "cell": "greyfen-square", "x": 1, "y": 1}}
```

<a id="effect-board-post"></a>**`board.post`**: Pin a notice to a board (undone at the end)

A notice on the town's board, signed with the story's title. Taken down when the story ends.

Reads: Pin to the board of **greyfen** for **1** days: **The bells ring for the ford.**

| Parameter | What | Kind |
|---|---|---|
| `town` | the town | town |
| `text` | the notice | text; up to 300 characters |
| `days` | days | int; 1 to 28 |

```json
{"do": "effect", "effect": {"kind": "board.post", "town": "greyfen", "text": "The bells ring for the ford.", "days": 1}}
```

<a id="effect-weather-set"></a>**`weather.set`**: Set the weather (undone at the end)

The weather of a place out of doors, or of every street of a town, for some hours of the world's clock, then the season's again. Harvests and the cold follow it. A story never changes the day or the hour. On a hosted world it needs what the same act by hand on LIVE needs (a Dungeon Master). Undone when the story ends.

Reads: The weather in **greyfen** is **snow** for **6** hours

| Parameter | What | Kind |
|---|---|---|
| `place` | the place | place; optional |
| `town` | or a town's streets | town; optional |
| `weather` | the weather | choice; one of: `clear`, `overcast`, `rain`, `storm`, `fog`, `snow`, `sandstorm` |
| `hours` | hours | int; 1 to 72 |

```json
{"do": "effect", "effect": {"kind": "weather.set", "town": "greyfen", "weather": "snow", "hours": 6}}
```

#### Towns, markets and factions

<a id="effect-ship-storm"></a>**`ship.storm`**: Hold a ship in a storm

A ship due in port is held by a storm.

Reads: **the-heron** is stormbound

| Parameter | What | Kind |
|---|---|---|
| `ship` | the ship | ship |

```json
{"do": "effect", "effect": {"kind": "ship.storm", "ship": "the-heron"}}
```

<a id="effect-harvest-override"></a>**`harvest.override`**: Set a harvest

Sets a harvest area's yield for a while; "clear" gives it back to the season.

Reads: The harvest in **greyfen-fields** is **poor** for **1** days (**the ford is open again**)

| Parameter | What | Kind |
|---|---|---|
| `area` | the harvest area | area |
| `word` | the harvest | choice; one of: `poor`, `fair`, `good`, `clear` |
| `days` | days | int; 1 to 366 |
| `words` | what people say | text; up to 80 characters |

```json
{"do": "effect", "effect": {"kind": "harvest.override", "area": "greyfen-fields", "word": "poor", "days": 1, "words": "the ford is open again"}}
```

<a id="effect-band-override"></a>**`band.override`**: Set a household's prosperity

A household's prosperity band for a while.

Reads: **rm111**'s household is **destitute** for **1** days

| Parameter | What | Kind |
|---|---|---|
| `npc` | a resident of the household | resident |
| `band` | the band | choice; one of: `destitute`, `poor`, `comfortable`, `rich`, `clear` |
| `days` | days | int; 1 to 112 |

```json
{"do": "effect", "effect": {"kind": "band.override", "npc": "rm111", "band": "destitute", "days": 1}}
```

<a id="effect-festival-call"></a>**`festival.call`**: Call a festival

Schedules a festival.

Reads: **greyfen** holds the festival **The Pale Rider** in **1** days

| Parameter | What | Kind |
|---|---|---|
| `town` | the town | town |
| `name` | its name | text; up to 80 characters |
| `inDays` | days from now | int; 0 to 30 |

```json
{"do": "effect", "effect": {"kind": "festival.call", "town": "greyfen", "name": "The Pale Rider", "inDays": 1}}
```

<a id="effect-faction-relation"></a>**`faction.relation`**: Change faction relations (needs a builder)

One faction's stance toward another. A builder or the owner must approve it on a hosted world.

Reads: **watch** is **tense** toward **road-company** (**-30**): **the blockade**

| Parameter | What | Kind |
|---|---|---|
| `faction` | the faction | faction |
| `other` | the other faction | faction |
| `stance` | the stance | choice; one of: `allied`, `friendly`, `neutral`, `tense`, `hostile`, `war` |
| `disposition` | disposition | int; -100 to 100 |
| `reason` | the reason | text; up to 300 characters |

```json
{"do": "effect", "effect": {"kind": "faction.relation", "faction": "watch", "other": "road-company", "stance": "tense", "disposition": -30, "reason": "the blockade"}}
```

<a id="effect-times-nudge"></a>**`times.nudge`**: Change market conditions

Pushes a market's times up or down for a while.

Reads: Push the times in **greyfen** by **1** for **1** days (**the ford is open again**)

| Parameter | What | Kind |
|---|---|---|
| `market` | the market | market |
| `points` | points | int; -100 to 100, not 0 |
| `days` | days | int; 1 to 366 |
| `words` | what people say | text; up to 80 characters |

```json
{"do": "effect", "effect": {"kind": "times.nudge", "market": "greyfen", "points": 1, "days": 1, "words": "the ford is open again"}}
```

<a id="effect-project-post"></a>**`project.post`**: Post a town project

Opens a town project.

Reads: **greyfen** posts the project **granary** "**The ford is open**" at **greyfen-square, 1, 1**

| Parameter | What | Kind |
|---|---|---|
| `town` | the town | town |
| `project` | the kind of project | project |
| `cell` | the place | place |
| `x` | tile x | int; 0 to 4095 |
| `y` | tile y | int; 0 to 4095 |
| `title` | its title | text; up to 80 characters |

```json
{"do": "effect", "effect": {"kind": "project.post", "town": "greyfen", "project": "granary", "cell": "greyfen-square", "x": 1, "y": 1, "title": "The ford is open"}}
```

<a id="effect-event-add"></a>**`event.add`**: Stage a market event

One of the market's events (a wedding, a fire, a glut) staged for today or within the next week, under the rules the scheduler keeps.

Reads: **the House wedding** at **greyfen** in **1** days at:00

| Parameter | What | Kind |
|---|---|---|
| `market` | the market | market |
| `event` | the event | choice; one of: `house_wedding`, `works_fire`, `good_catch`, `orchard_blight`, `festival_buying`, `lame_ox`, `rich_cargo`, `lost_ship`, `mill_stopped`, `drovers_in`, `salt_short`, `brewers_glut` |
| `days` | days from today | int; 0 to 7 |
| `hour` | the hour | int; 0 to 23; optional |

```json
{"do": "effect", "effect": {"kind": "event.add", "market": "greyfen", "event": "house_wedding", "days": 1}}
```

<a id="effect-event-cancel"></a>**`event.cancel`**: Call off a market event

The next such event at the market still to come or on is called off.

Reads: Call off **the House wedding** at **greyfen**

| Parameter | What | Kind |
|---|---|---|
| `market` | the market | market |
| `event` | the event | choice; one of: `house_wedding`, `works_fire`, `good_catch`, `orchard_blight`, `festival_buying`, `lame_ox`, `rich_cargo`, `lost_ship`, `mill_stopped`, `drovers_in`, `salt_short`, `brewers_glut` |

```json
{"do": "effect", "effect": {"kind": "event.cancel", "market": "greyfen", "event": "house_wedding"}}
```

<a id="effect-prices-reset"></a>**`prices.reset`**: Ease prices back (needs a builder)

A market's prices (or every market's) go back to their usual over a day. It changes the economy: a builder or the owner must approve it on a hosted world.

Reads: Prices at **greyfen** ease back to their usual: **the ford is open again**

| Parameter | What | Kind |
|---|---|---|
| `market` | the market | market |
| `words` | why, in a few words | text; up to 80 characters |

```json
{"do": "effect", "effect": {"kind": "prices.reset", "market": "greyfen", "words": "the ford is open again"}}
```

<a id="effect-project-complete"></a>**`project.complete`**: Finish a town project (needs a builder)

A town project of that kind open in the town (one the story posted, or any) is finished by hand. It changes the economy: a builder or the owner must approve it on a hosted world.

Reads: The **granary** project in **greyfen** is finished

| Parameter | What | Kind |
|---|---|---|
| `town` | the town | town |
| `project` | the kind of project | project |

```json
{"do": "effect", "effect": {"kind": "project.complete", "town": "greyfen", "project": "granary"}}
```

<a id="effect-project-cancel"></a>**`project.cancel`**: Call off a town project (needs a builder)

A town project of that kind open in the town is called off, its gifts given back. It changes the economy: a builder or the owner must approve it on a hosted world.

Reads: The **granary** project in **greyfen** is called off

| Parameter | What | Kind |
|---|---|---|
| `town` | the town | town |
| `project` | the kind of project | project |

```json
{"do": "effect", "effect": {"kind": "project.cancel", "town": "greyfen", "project": "granary"}}
```

#### Quests

<a id="effect-storyline-give"></a>**`storyline.give`**: Give a personal quest

Starts a personal quest from a DM template; refused at three personal quests.

Reads: Give **$actor** the personal quest **the-debt** with **rm111** at **greyfen-square**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |
| `template` | the template | template |
| `resident` | the resident | resident |
| `place` | the place | place |

```json
{"do": "effect", "effect": {"kind": "storyline.give", "player": "$actor", "template": "the-debt", "resident": "rm111", "place": "greyfen-square"}}
```

<a id="effect-quest-offer"></a>**`quest.offer`**: Offer a quest (v2, the story's own)

The wolf is offered the quest: they accept or decline it in its giver's window, or in the journal when it has no giver. Takes the wolf into the story.

Reads: Offer **axle** to **$actor**

| Parameter | What | Kind |
|---|---|---|
| `quest` | the quest | quest |
| `player` | the wolf | wolf |

```json
{"do": "effect", "effect": {"kind": "quest.offer", "quest": "axle", "player": "$actor"}}
```

<a id="effect-quest-start"></a>**`quest.start`**: Start a quest (v2, the story's own)

Gives the quest without asking: the wolf's own copy, their party's, or the story's one copy. Takes the wolf into the story.

Reads: Start **axle** for **$actor**

| Parameter | What | Kind |
|---|---|---|
| `quest` | the quest | quest |
| `player` | the wolf | wolf |

```json
{"do": "effect", "effect": {"kind": "quest.start", "quest": "axle", "player": "$actor"}}
```

<a id="effect-quest-complete"></a>**`quest.complete`**: Complete a quest (v2, the story's own)

Completes the copy the wolf holds, as if its last objective were done: its rewards are given.

Reads: Complete **axle** for **$actor**

| Parameter | What | Kind |
|---|---|---|
| `quest` | the quest | quest |
| `player` | the wolf | wolf |

```json
{"do": "effect", "effect": {"kind": "quest.complete", "quest": "axle", "player": "$actor"}}
```

<a id="effect-quest-fail"></a>**`quest.fail`**: Fail a quest (v2, the story's own)

Fails the copy the wolf holds; the journal says so.

Reads: Fail **axle** for **$actor**

| Parameter | What | Kind |
|---|---|---|
| `quest` | the quest | quest |
| `player` | the wolf | wolf |

```json
{"do": "effect", "effect": {"kind": "quest.fail", "quest": "axle", "player": "$actor"}}
```

<a id="effect-objective-complete"></a>**`objective.complete`**: Complete an objective (v2, the story's own)

Marks one objective of the copy the wolf holds done (a hidden one is shown).

Reads: Complete **ask** of **axle** for **$actor**

| Parameter | What | Kind |
|---|---|---|
| `quest` | the quest | quest |
| `objective` | the objective | objective |
| `player` | the wolf | wolf |

```json
{"do": "effect", "effect": {"kind": "objective.complete", "quest": "axle", "objective": "ask", "player": "$actor"}}
```

<a id="effect-objective-fail"></a>**`objective.fail`**: Fail an objective (v2, the story's own)

An optional objective failed is struck through; a required one failed fails the quest.

Reads: Fail **ask** of **axle** for **$actor**

| Parameter | What | Kind |
|---|---|---|
| `quest` | the quest | quest |
| `objective` | the objective | objective |
| `player` | the wolf | wolf |

```json
{"do": "effect", "effect": {"kind": "objective.fail", "quest": "axle", "objective": "ask", "player": "$actor"}}
```

<a id="effect-objective-reveal"></a>**`objective.reveal`**: Reveal a hidden objective (v2, the story's own)

A hidden objective appears in the journal.

Reads: Show **ask** of **axle** to **$actor**

| Parameter | What | Kind |
|---|---|---|
| `quest` | the quest | quest |
| `objective` | the objective | objective |
| `player` | the wolf | wolf |

```json
{"do": "effect", "effect": {"kind": "objective.reveal", "quest": "axle", "objective": "ask", "player": "$actor"}}
```

<a id="effect-objective-progress"></a>**`objective.progress`**: Add to an objective (v2, the story's own)

Counts towards an objective's goal as if its event had happened that many times (or that quantity).

Reads: Add **1** to **ask** of **axle** for **$actor**

| Parameter | What | Kind |
|---|---|---|
| `quest` | the quest | quest |
| `objective` | the objective | objective |
| `player` | the wolf | wolf |
| `amount` | how much | int; 1 to 1000000 |

```json
{"do": "effect", "effect": {"kind": "objective.progress", "quest": "axle", "objective": "ask", "player": "$actor", "amount": 1}}
```

<a id="effect-storyline-tick"></a>**`storyline.tick`**: Tick a personal quest

An objective of the wolf's live personal quest from that DM template is done.

Reads: Tick objective **1** of step **1** in **$actor**'s **the-debt**

| Parameter | What | Kind |
|---|---|---|
| `player` | the wolf | wolf |
| `template` | the template | template |
| `step` | the step (1 is the first) | int; 1 to 10 |
| `objective` | the objective (1 is the first) | int; 1 to 8 |

```json
{"do": "effect", "effect": {"kind": "storyline.tick", "player": "$actor", "template": "the-debt", "step": 1, "objective": 1}}
```

#### Chapters

<a id="effect-story-start"></a>**`story.start`**: Start the next chapter (v2, the story's own)

Another story of the same campaign starts: the next chapter. With "when this one has ended" (the default) it waits until this story has ended in the game (finish, an ending phase), so what this one held (doors, residents, the weather) is let go first; "now" runs it beside this one. Its wolves: the same as this story's now, its own audience, or a list variable. Carrying: the story and per-wolf variables the next chapter declares with the same name and type start with this story's values; world variables are shared anyway. Only a story approved as part of a campaign may start one, and only a chapter of that campaign that is not running.

Reads: Start the chapter **the-crossing** for **this story's wolves**, carrying its variables: **yes**, **when this one has ended**

| Parameter | What | Kind |
|---|---|---|
| `story` | the chapter | story |
| `audience` | whose | choice; one of: `same`, `own`, `list` |
| `list` | a list of wolves | variable; a variable of type wolves; optional |
| `carry` | carry the variables | bool; optional |
| `when` | when | choice; one of: `ended`, `now`; optional |

```json
{"do": "effect", "effect": {"kind": "story.start", "story": "the-crossing", "audience": "same", "carry": true, "when": "ended"}}
```

#### Scenes

<a id="effect-scene-play"></a>**`scene.play`**: Play a scene

Plays one of the scenes embedded in this story; a public scene may play for the world.

Reads: Play the scene **the-oath** for **$actor**, cue **begin**

| Parameter | What | Kind |
|---|---|---|
| `scene` | the scene | scene |
| `player` | who sees it | wolf |
| `cue` | its cue | cue |

```json
{"do": "effect", "effect": {"kind": "scene.play", "scene": "the-oath", "player": "$actor", "cue": "begin"}}
```

<a id="effect-scene-stop"></a>**`scene.stop`**: Stop a scene

A scene of this story still playing ends now (scene failed follows, with its cue).

Reads: Stop the scene

| Parameter | What | Kind |
|---|---|---|
| `scene` | the scene | scene; optional |

```json
{"do": "effect", "effect": {"kind": "scene.stop"}}
```

<!-- reference:end -->
