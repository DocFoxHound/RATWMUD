# Writing a mantle: the writer's guide (doc 84, Phase 11)

A **mantle** is one wolf's way of talking, written without their name, in the dialect of where they grew up, and passed
on to a newborn of the same dialect when its wearer dies (Docs/Design/84-living-conversations.md §6, §9, §11, §12). This
guide is for whoever writes one: a Claude agent in a session, or the DM in the Mantles panel. It says what a mantle must
hold, how the game hears it, the rules every line keeps, and the steps from a brief to a checked file. The format is
frozen at Slice 3's end (doc 84 §41).

The pilot's writers (Phase 11) found most of the traps below the hard way; each is now a line of this guide or a check.

## 1. The steps

1. **Brief**: `python3 tools/mantle_brief.py ridgemere3 <npc_id>` (add `--facts FILE` with a saved
   `talk_check --world ridgemere3 --residents --json` dump to skip the minute it takes). Read all of it: who they are,
   what slots are filled for them, their stances and why, their household, the town's lore, the dialect's words.
2. **Index**: `python3 tools/mantle_index.py <town> --world ridgemere3` lists what the town's mantles (rows and files)
   have taken: quirks, oaths, sayings, opinions, stories, scenes, and the 4-word runs more than one says. Run it again
   before each new mantle: neighbours must come out distinct.
3. **Write** the file at its export path, `Data/Talk/mantles-export/<dialect>/<id>.mantle`.
4. **G1, the game's loader**: `build-core/talk_check --mantle <file>`. Syntax, facts, symbols, 160 characters an
   alternative, graft reachability, known moves and tones.
5. **G2, the rules**: `python3 tools/mantle_check.py Data/Talk/mantles-export/<dialect> --world ridgemere3 --strict`
   (the whole dialect, so a 5-word run shared with a neighbour shows). Fix every refusal of your own mantles; `--strict`
   also refuses the tier's size and a dialect density under 60%.
6. Only then is it ready for `tools/mantle_load.py` (the batch's owner loads, binds and exports; a writer never writes
   rows by hand).

## 2. How the game hears a mantle (why the recipe is what it is)

Since §37's hybrid, **talk between residents is mostly whole written scenes**: the engine picks the topic, the pair and
the moment, then plays a scene that fits as written; single lines are said only at the edges (a greeting in the
opener's breath, the goodbye pair, an answer to a direct question, a joiner's word).

- **A mantle's scene is its wearer's talk.** When the wearer opens a talk on a topic their mantle has a scene for, it
  outranks the shared library's (+5), and the listener hears this wolf, not the library. So **scenes are where a mantle
  is heard**: most of a mantle's weight goes into them, on as many everyday topics as its wearer would really talk about.
- A scene is heard once per listener per thread: write **two or three parallel threads** in each (the `|`
  alternatives line up turn by turn), so the same scene can come round several times as different talks.
- **Lines** (`line <move> ...`) are the wearer's at the edges: greetings and their returns, goodbyes and theirs,
  reactions to news, a short answer, a decline. They are short and in voice.
- **Stories** are scenes too: told once to each listener, on `talk.memories` unless their criteria name a topic.
- **Window lines** (`dlg <tree>.<node>`) are what the wearer says to a player in the dialogue window; **grafts** add the
  wearer's own choices there.
- The wearer's **card** (traits, likes, topics) steers what they bring up; **opinions** are their stances with reasons.

## 3. The file

```
# sw101, the stone-keeper (doc 84 Phase 11): written for tn_fh_thistle_stonepick; name-free.
mantle sw101
state ready
dialect saltwell
accent light broad            # the accents a wearer may have; the first as written. A wearer with none: `accent none`
tier full                     # full: merchants, innkeepers, guards, priests, masters, House family; else standard
written_for tn_fh_thistle_stonepick
title the stone-keeper        # for writers and the DM; never spoken
fits stage=adult|old          # hard: who may ever wear it (a pup's mantle: stage=young|adolescent|adult|old with sections)
prefers role=merchant         # soft
traits temper=calm talk=plain warmth=reserved humour=dry honesty=plain piety=ordinary drive=steady
quirk collects_smooth_stones  # lower_case_words
likes barley_soup old_tales summer_evenings
dislikes early_bells rats
fears a_dry_spring
hopes a_shelf_that_never_empties
topics lore +.4 weather +.2 prices +.3 gossip -.4
styles tease=dry refuse=soft comfort=practical gossip=reluctant
marks oath saying             # 2-4 of its own habits (name! in words), each with 2+ alternatives
persona #self.first# keeps ... (one line, for the window's Mind; names the wearer only as #self.first#)

words
oath! = By the pebble and the pool | Stone and shade      # a habit: `!` (said at most ~25% of lines)
saying! = A smooth stone was a hard one once | The well keeps no secrets, only water
stone = a river-stone | a smooth one from the wadi       # its own words: what makes it sound like nobody else

opinion road against 2 : Every cart from the Saltpan road comes in a day late, and it's my shelves that pay.
...
grown
line greet.return : #greet#. Mind the step; it's worn smooth.
scene ... (below)
story ... (below)
dlg shop.greet : #greet#. Salt, oil, rope and dates, and every weight honest.
graft resident.hub
...
```

- **Ids**: two letters of the dialect and a number (`sw`, `mb`, `cw`, `lf`, `sm`, `oh`, `fh`, `rm` for plain). The
  pilot uses 101 and up (the test fixture has the low numbers).
- **Traits**: copy the brief's seven, but **never change honesty** (it chooses thieves) and keep quarter as the brief
  says; you may refine the middle of the talk-only axes (a "reserved" wolf who is warm with kin). Every axis is
  needed: a newborn who inherits the mantle takes all seven from the card.
- **Sections**: `words` (symbols), `any` (the default: items before any stage section), `grown` (adults and the old),
  `young` (pups, and adolescents unless there is an `adolescent` section), `adolescent`, `old` (the old's own, on top
  of `grown`, which the old speak too: frozen so at Slice 3's end). A stage section is
  criteria: a `grown` line never comes from a pup who inherits the mantle. Write the items in `grown` (or `old` for the
  old), since the mantle will be worn by grown wolves; a pup's mantle has `young` and `adolescent` too.
- Keys: `line @name move ...` names an item (`grown.request.carry`); leave names out and the loader numbers them. Give
  scenes and stories names (`scene noon_shutters`).

## 4. The sizes (doc 84 §9.3, the richer size; `mantle_check --strict` counts items that aren't symbols)

| | full (92 items or more) | standard (55 or more) |
|---|---|---|
| words | 16-20 symbols: oath, saying (marks), a laugh or a pet name, and their own words | 12-14 |
| scenes | about 30 (2-3 threads each), across the topics below | about 18 |
| stories | 5, of 3-4 beats with the listener's answers | 3 |
| opinions | 8 (one or two lines each), with reasons | 5 |
| lines | about 30: 8 openers (greetings by frame: strangers, kin, at work, again today, dawn, dusk), greet.return by band and time, farewell and farewell.return, react.news (new, known, doubt, sorrow, glad), answer.how, sympathise, decline (hedged), accept_decline, thank and deflect_thanks, preclose, a tease and tease.return if they tease, ack | about 18 |
| barks | 6 `rule` blocks on `concept=bark` | 4 |
| window | 12-16 `dlg` lines: resident.hub (by meeting and disposition), job, business, news, gossip, bye, and the role tree's nodes (shop.greet/prices/bye, inn.greet/rooms/roomed/rest/bye); 2 grafts of 2 nodes | 4-6 `dlg` (hub, job, news, bye); 1 graft of 2 nodes |

Topics a scene can be on (`when topic=...`): **everyday** weather, day, smalltalk, work, food, family, home, friends,
pastimes, animals, health, memories, dreams, faith, travel, land, season, lore, accent; **news** gossip, crime,
bandits, caravan, prices, ships, event, times, harvest, newcomer, festival, life, deed. Favour the everyday ones the
wearer would really talk about; give each favourite topic (card `topics`) two or three scenes; keep news scenes to what
the facts give (below).

## 5. Scenes

```
scene noon_stones
when topic=weather|day here.time=day here.weather=clear
does tell.self / react.self / assess
a : Noon, and I've a stone in my paw to keep it cool. Old trick of #grandad#'s. | The shutters are half down and the shelves are half asleep. Noon. | #hot.capitalize#. Even the dates are sweating in their sacks.
b[from=saltwell] #sw : A stone in the paw. My mother did the same. | We all sleep at noon. Only the flies work. | They'll keep. Dates have crossed worse.
b : Does it work? | Lucky shelves. | I'd sweat too.
a@sw : She knew a thing or two. | And they never tire. | #bc_it_is#
a : Try it. A wadi stone, flat and grey. | They've earned it. | So would I, in a sack.
```

- `when` holds the scene's criteria (the speaker `a` is the wearer: `self.*` or `a.*`; the listener `b.*`); `does`
  names each turn's move, in order (`a / b / a`); `tone` (optional) each turn's tone.
- `a : x | y | z` are **parallel threads**: the n-th alternative of every line belongs together. Write every line with
  the same number of alternatives (or one, which serves every thread). Each thread must read as one natural talk.
- `b[criteria] #tag : ...` is the listener's line for a kind of listener (`b[old]`, `b[job=farmer|herder]`,
  `b[from=saltwell]`, `b[friends]`, `b[wealth=poor|destitute]`, `b[stage=young]`); `a@tag :` is the wearer's answer to
  that kind. A bare `b :` (and `a :`) after them is for everyone else. Always give a bare `b :`: a scene must fit any
  listener.
- `b? :` is an optional line.
- **3 to 5 turns**, each line short (a breath: one or two sentences, 160 characters at most). End on a line that
  closes (an assessment, a laugh, a "so it is"), never on a question nobody answers.
- **The listener's lines are anyone's**: no name, no claim about them but what the criteria say (`b[job=fisher]` may
  say "my nets"); a bare `b :` must suit a stranger as well as a friend.
- **Gate the moment**: weather words need `here.weather=` (or `here.outdoors=` indoors), the hour's words `here.time=`,
  the season's `here.season=`; work talk `self.working=yes` or the job (`self.job=`); talk of where you stand
  `here.region=waste` (Saltwell) or `here.region=fens` (Fenshore); talk of home `self.from=` or `self.away=yes`.
- **News scenes** only use the facts the topic gives (`#topic.good#`, `#topic.subject#`, `#world.event_words#`...) and
  never invent a price, a crew, a culprit or a cargo; write them only where the wearer has something of their own to say.

Each scene is one talk in the wearer's voice: their oath now and then, their words, their opinions, their way of
answering (a dry wolf undercuts, a warm one asks after you, a nervy one worries a thing twice).

## 6. Lines and their etiquette

`line <move>[.variant] [criteria] : alt | alt | alt` (2-3 alternatives a line). The moves (Data/Talk/moves.json):
greet, greet.return, kin.open, again.open, hail, tell.self, tell.feeling, tell.worry, tell.complaint, tell.boast,
react.news (.new .known .doubt .sorrow .glad .glee), react.complaint (.agree .disagree), react.story, assess,
sympathise, reassure, congratulate, laugh, ask.how, answer.how (.well .poorly .busy .grieving .deflect), ask.after,
answer.after, give.opinion, agree, disagree, concede, insist, tease, tease.return, tease.laugh, take_offence,
apologise, accept_apology, thank, deflect_thanks, praise, deflect_praise, confide, warn, request, grant, decline,
accept_decline, offer, accept, refuse, invite, accept_invite, give.advice, preclose, accept_preclose, farewell,
farewell.return, leave, ack, backchannel.

- **Dispreferred answers** (decline, refuse, disagree, react.news.doubt) hedge and give a reason, or are six words or
  more: "No." is never a decline. A cold or short-tempered wolf may be blunt, but it costs regard.
- **Closings come in pairs**: a wolf who says `farewell` also has a `farewell.return`; greet and greet.return the same.
- **Openers by frame**: `line greet [pair.band=strangers] : ...`, `line kin.open [pair.kin!=no] : ...`,
  `line again.open : ...` (seen today), `line greet [here.time=dawn] : ...`, `line greet [self.working=yes] : ...`.
- **Scene turns**: `does` names one move for every turn, an optional `b? :` line included (`a / b / a / b`).
- **Reactions that pity or congratulate** ("I'm sorry to hear it", "Good for you") need the news they answer:
  `line react.news.sorrow [topic.kind=death|loss|theft|assault|arrest|crime] : ...`, `[topic.kind=marriage|birth|work...]`.
- **Topics on the card** are the director's topics (§4's list): `player` for travellers, never "strangers".
- The tier's minimum (refused under it): a line answering greet.return, farewell.return and react.news.

## 7. Words: the dialect and the wearer's own

- **Saltwell** (the oasis of the Red Waste): unhurried, courteous, hospitable; peace and welcome first, the guest
  honoured, proverbs; the well and the spring, salt and the Saltpan, the Ochre vines, the Redsand warren, carts and
  caravans, dunes and wadis, noon heat and cold starry nights, dates and palms, trade by measure and oath. Broad is the
  more formal: one courtesy more. Its grammar: `Data/Talk/grammar/saltwell.grammar`.
- **Mirebridge** (Fenshore, a stilt hamlet of the Sallow Fens): wry, damp, suspicious, plain; mud, eels, reeds, the
  boardwalks, mizzle and murk, the Heron's hundred-year fire, the drowned town of Mirebridge under the fen, the marsh
  lights; dry feet are luck. Its grammar: `Data/Talk/grammar/mirebridge.grammar`.
- **Plain** (Ridgemere): the city's ordinary speech, coloured by the wearer's quarter (the Heights formal, the Sump
  rough, the Wharf salt); no density rule.
- **Density (`--strict`): 60% of the wearer's own alternatives carry a dialect marker** (every `line`, `dlg`, bark,
  opinion and the `a` lines of scenes and stories; a listener's `b :` lines count neither way): a dialect symbol
  (`#greet#`, `#farewell#`, `#oath#`, `#saying#`, `#address#`, `#very#`, `#good#`, `#bad#`, `#hot#`, `#cold#`,
  `#tired#`, `#child#`, `#mum#`, `#bc_it_is#`..., the grammar's list is in the brief), one of the mantle's own words,
  or (only where the accents don't include `none`) a dialect word written out ("the well", "dry feet", "mizzle").
  Weave them in where a wolf of that place would say them; the mantle's own words are the best way: `stone = a
  river-stone | a smooth one from the wadi` gives the wolf a word nobody else uses and counts as a marker.
- **Accent none**: the wearer speaks plain. The mantle's accents are `none`, and **no dialect word may be written out**
  (mantle_check refuses it): the content stays local (the fen, the eel traps), the words ordinary, and dialect only
  through symbols (which say plain words for this wearer). The mantle's own words are spoken whatever the accent, so
  keep them free of dialect words too.
- **The listener's lines** (`b :`) are said by whoever the wearer talks to, in *their* own speech: a bare `b :` never
  writes a dialect's words out (a city wolf doesn't say "peace on you"); it carries the dialect through symbols
  (`#greet#`, `#bc_it_is#`, `#hot#`), which each listener says in their own grammar. Only `b[from=saltwell]` (or
  `b[dialect=mirebridge]`) may write the town's words out. mantle_check refuses a bare `b :` that does.
- **Another dialect's words** are refused (the lexicon in `tools/dialect_check.py`): a Saltwell wolf never says "dry
  feet"; a Fenshore one never says "peace on you".
- **Marks** (`marks oath saying`): the 2-4 habits a neighbour would remember. Each needs two or more alternatives, so
  a reused mantle's mark can be drawn again. Habits (`!`) in at most 25% of the mantle's alternatives (§14.4); aim at
  10-15%.
- **A word's value never ends its own sentence** (no `.`, `;`, `!`, `?` at its end): words are spliced into lines.
- Use `.capitalize` at a sentence's start: `#hot.capitalize#`.
- **A broad form is never called by name**: `#greet#` says the dialect's `greet.broad` to a broad wearer by itself;
  `#greet.broad#` is refused. Write the plain symbol.
- **A symbol picks its alternative alone**, so it can't follow a scene's threads: where a thread's later line must
  answer exactly what an earlier one said (a riddle and its answer), write the words out in each thread.
- **Don't pad the density.** `#address#` on every line, or a tic word, reads as a tic: the mantle's own words, woven
  where the wolf would say them, carry the density and the voice together.
- **Write the brief's words in your own.** Its likes and stance reasons are handed to every writer of the town; a
  paraphrase close to them collides with a neighbour's (a shared 5-word run).

## 8. Slots: names, kin, trades and places (doc 84 §11)

Never a name. Every person, kin, trade and place comes through a slot, filled from the wearer's facts; a line whose
slot is empty for its wearer doesn't fit them (and so a mantle passed to a bachelor never names his wife).

| Slot | Is |
|---|---|
| `#self.first#`, `#self.surname#`, `#other.called#` (what this wolf calls the listener: a first name, a Master or Mistress and surname across a class gap, else the dialect's address), `#other.first#` (only with `pair.named=yes`) | names |
| `#self.spouse#`, `#self.spouse_word#` (husband or wife), `#self.child#`, `#self.mother#`, `#self.father#`, `#self.late#` (the kin last mourned) | kin |
| `#self.neighbour#`, `#self.friend#`, `#self.rival#`, `#self.boss#`, `#self.hand#`, `#self.priest#`, `#self.shopkeeper#` | people |
| `#self.workplace#` ("Saltwell Stores"), `#self.trade_words#` ("shopkeeper at Saltwell Stores"), `#self.street#`, `#self.from_town#`, `#here.town#`, `#here.place_name#` | places and trades |

- **Gate a scene or story on the slot's fact** (`when ... self.married=yes`, `self.parent=yes`, `self.friend`,
  `self.boss`): a scene must render whole. The brief lists the slots filled for the wearer; a slot they don't have makes
  a line that is never heard from them (it may be for a later wearer: keep those few).
- **Pronouns**: never he, she, him, her, his or hers in a sentence that names someone by a slot (`#self.friend#`,
  `#other.called#`...): they come from the person's sex, which a mantle doesn't know. Say the name again, or "that
  one", or build the line so it needs none.
- **The listener** is `#other.called#` or `#address#`; never "he" or "she" about them.

## 9. Truth (doc 84 §9.6; `Data/Talk/truth.json` and mantle_check)

1. **Opinions and feelings, freely.** "I can't abide the noon heat."
2. **Claims about the wearer need a fact behind them**: "my shop" `self.role=merchant`; "my husband" `self.married=yes`
   (better: `my #self.spouse_word#`); "back home" `self.away=yes`; "my nets" `self.job=fisher`; "my children"
   `self.parent=yes`. Trade talk carries `self.job=` or `self.role=` gates, so the mantle stays true in a later wearer
   of another trade (gate `rule` barks and work scenes on the job).
3. **The invented past only about people not in the world** ("my grandmother", unnamed), with age gates ("when I was
   twenty" needs `self.age>=25`).
4. **Named residents only through slots**, and only for feelings, facts the game holds, or what the line itself records.
5. Weather, hour and season words need their gates (above). "Good morning" needs `here.time=dawn`.
6. Nobody saw what the game didn't show them: "I heard", "they say".

## 10. The project's rules for world text

- Never the word "dog" or "dogs": wolves; **pups** for the young.
- Players' private messages are **private messages**, never "tells".
- Characters are **male or female only**; pronouns follow sex.
- The deities are **the Bright One** (kindly) and **the Dark One** (malevolent), never "gods" or "the gods".
- No literal names of anyone in the world, no capitalised words mid-sentence but the realm's places, Houses, weekdays,
  the Watch and the deities (`tools/mantle_rules.json` allow).

## 11. Window lines and grafts

`dlg resident.hub [topic.meeting=known] : #greet#, #topic.player#. ...` (`#topic.player#` is the player's name, which
the game knows). Nodes: resident (hub, town, job, business, news, gossip, trouble, helped, way, place, refused, bye), shop
(greet, prices, bye), inn (greet, rooms, roomed, rest, refused, bye), watch, church, master, work (see
`Data/Talk/dialogue/*.tree`). Give the hub greetings by meeting (`topic.meeting=stranger|known|returning`) and by
disposition (`pair.disposition=hostile|wary|warm`), the hour (`here.time=night`), and the job's own words (`job`), so
W1 (window lines that are the mantle's) rises.

```
graft resident.hub
choice Do you really keep every stone you find? -> stones
node stones
say Not every one. Only the smooth ones. A smooth stone was rough once, and the river did the rest.
mood pleased
free
choice What do you do with them? -> stones_what
choice Goodbye. -> bye
node stones_what
say Line the cistern with them. The water tastes of nothing but water. Come and see, if you like.
choice I might. -> hub
choice Goodbye. -> bye
```

Every graft node says something, every choice leads to one of its nodes, the tree's node, `bye` or `end`, and every
node has a way back. A node's `mood` is one of the trees' moods: calm, formal, keen, pleased, stern, warm.

## 12. A good mantle reads like one wolf

Before you finish, read it whole, aloud in your head: does it sound like nobody else in the town? Would a neighbour
know this wolf by their oath, their way of answering, the things they always come back to? Is every scene one natural
talk, every thread of it? Is it true to their trade, caste, House, kin and town? Then run G1 and G2.
