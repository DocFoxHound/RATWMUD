# Writer's brief: doc 84 P14, Ridgemere's own voices, first half (lead: the r3-mantles-city-a session)

You are one of about ten mantle writers working side by side. You write mantles **as files only**, one per resident
your prompt assigns you. The lead edits every mantle, runs the batch gates, loads, commits and pushes. Not you.

These are **Ridgemere-born residents of Ridgemere** who speak **plain** speech: the city's own voice. The city is the
most-heard place in the game, so these are the mantles players and listeners hear most. Quality matters most here.

## Where

- Worktree (your only place): `/home/martinb/Documents/GitHub/MUDProject/.claude/worktrees/r3-mantles-city-a`.
  Begin every shell command with `cd /home/martinb/Documents/GitHub/MUDProject/.claude/worktrees/r3-mantles-city-a &&`
  (the cwd resets between calls) and run every `python3 tools/...` from there.
- `export RATW_TALK_CHECK=$PWD/build-core/talk_check` (built from this branch; `mantle_batch` finds it by itself).
- Facts: `Saved/Mantles/residents.jsonl` (pass `--facts Saved/Mantles/residents.jsonl`).
- Your batch folder `Saved/Mantles/<batch>/` (your prompt names it) holds `batch.tsv`, `briefs/<id>.md` (each
  resident's brief: read it whole) and `start/<id>.mantle` (a starting file with the header filled; `TODO` where you
  decide). For each resident: `cp Saved/Mantles/<batch>/start/<id>.mantle Data/Talk/mantles-export/plain/<id>.mantle`
  and write the mantle there. Remove every `TODO`. A new draft has **no** `revision` and no `written_by` line.

## Read first, fully

1. `Docs/Mantle-writing-handbook.md` §1 to §9 (appendices as you need them). **It is the law: follow it exactly**,
   including its corrections of the pilot (the ✗ marks in §4.6) and fix 8's rules: window lines on fact nodes gated
   on their case (§4.5.8), tagged scene lines checked like any line, personas with **no pronoun**, a story's `ask :`
   is **the teller's** question to the listener (§4.5.6).
2. Model mantles, **for shape and breadth only, never a phrase**: `Data/Talk/mantles-export/saltwell/sw101.mantle`
   (full, merchant), `plain/rm101.mantle` (plain, full), `plain/rm105.mantle`, `plain/rm109.mantle` (plain, newer),
   `coldwater/cw193.mantle` (a city servant), `saltwell/sw124.mantle` (a House household). For a pup or adolescent also
   `plain/rm102.mantle` (a plain pup: young, adolescent and grown sections) and `Data/Talk/kits/saltwell.young.talk`.
3. The plain grammar: `Data/Talk/grammar/plain.grammar` (its symbols: `#greet#`, `#farewell#`, `#address#`, `#sir#`,
   `#toffs#`, `#rabble#`, `#skint#`, `#posh#`, `#lump#`, `#watch#`, `#gaol#`, the backchannels `#bc_...#`; read what
   each says before you use it: `#pet#` and `#dear#` already start with "my" in some values).
4. The town's index before **each** mantle: `python3 tools/mantle_index.py ridgemere --world ridgemere3` (long: grep
   it for oaths, sayings, quirks, titles). The other writers are writing Ridgemere right now; their files show as they
   land. Pick an oath, a saying, a title and a quirk-angle nobody in Ridgemere has, **in any dialect**.

## The user's rules (verbatim in the handbook §1.3; non-negotiable)

- Unique dialogue per wolf, in the dialect of where they grew up (all of yours: `plain`, accents as the brief says).
  Nothing copied between mantles; no 5-word run shared with any other mantle (G2 checks plain; the lead checks every
  dialect); no oath or saying shared in the town.
- Name-free: no name of anyone in the world; every person, kin, trade and place through a slot (§4.4.9). Name-words
  (Ash, Moss, Reed, Frost, Rowan, Sage, Stone, Dawn, Hazel...) never capitalised mid-sentence or before a comma.
- Reusable by a newborn of the dialect: `fits` is stage only; every claim about the wearer gated by a fact
  (job, role, married, parent, age, likes, wealth, class, house, quarter); persona with no pronoun; sex-neutral words
  about the wearer ("an old stick", never "crone", "lad", "lass", "old man"; no `^m`/`^f`).
- NPCs never answer players' open chat: everything said to a player is a `dlg` line or a `graft`; scenes, lines and
  stories are NPC-to-NPC; barks are said to no one.
- Deities: only **the Bright One** (benevolent) and **the Dark One** (malevolent). Never god, gods, goddess, godsend,
  godly, godforsaken, "Gods!".
- Wolves, never "dog/dogs/puppy/doggy" ("pups" for young; bone-tired, not dog-tired; no "dogsbody").
- "private messages", never "tells" (for player messages).
- Male or female only; pronouns follow sex: never "they/them/their" for one wolf named by a slot, no he/she in a
  sentence with a person slot (repeat the slot or rephrase); `#topic.they#` etc. for the topic's person.
- The RICHER size: full ≥ 92 items (aim at 105-120: 30-34 scenes, 5 stories, 9-11 opinions, 35-42 lines, 6 barks,
  15-22 dlg, 2 grafts, 18-26 words), standard ≥ 55 (aim at 65-80: 18-21 scenes, 3 stories, 6-7 opinions, 28-33
  lines, 4 barks, 6-12 dlg, 1 graft, 14-20 words). Pups/adolescents: `fits stage=young|adolescent|adult|old` and
  `young`, `adolescent` **and** `grown` sections (most items in their stage now, a solid `grown`: its edge lines and a
  dozen scenes).
- **No paid model calls** (never `tools/voice_tables.py`, `tools/live_npc_smoke.py`, the NPC Mind, any AI API). You
  write every line yourself.

## Ridgemere's own voice: plain speech, by quarter

Plain has **no density rule**: the register *is* the voice, and every wolf must sound like their quarter, their
station and themselves. The city (doc 67): the rain city on terraced hills above Ridgemere Sound, climbing east from
the sea wall in three tiers. Low by the water: the wharf, the piers, the Customs Pier, the Sump and the Cinders; in the
middle: the abbey and cathedral close, Northgate, the Commons with Market Square and the Market Hall, the City Office
quarter (Records Square), the Watch House, the Rain Hall where the Council of Houses sits, the Great Palace and the Old
Keep; at the top: the Rise (the Minor Houses' mansions, "the Heights") and the Greatly compound. Outside the walls: the
north and south headlands of the bay (the docks, smokehouses, coves), Grayrock Hall on the south headland, Timberland
Court, Fellhall out on the East Road. Four great Houses: Greatly (brick, tile, lime, stone; the bank), Fell (wines,
liquors, fruit), Grayrock (the wharf, ships, warehouses), Timberland (forges, timber). It rains. A great deal.

Below, each area's register. **The examples show the register's colour only: never write these words** (every
writer gets this brief, and a copied 5-word run collides). Find this wolf's own way of saying it.

| Area (the brief's quarter + street) | Who | Register: rhythm, attitude, what they notice |
|---|---|---|
| **The Commons** (street "the Commons and the Rise") | shopkeepers, artisans, clerks' and keepers' households, pupils | Ordinary city speech, brisk and neighbourly: errands, prices, the square, the stalls, rent day, the lamplighter, chapel, who's courting whom. Respectable and a touch proud of it ("we keep a decent house"); judges by manners and paid debts. Looks *up* the stairs at the Rise with curiosity or envy, *down* at the Sump with worry or pity. Middle register: no slang beyond a mild word, no airs. |
| **Northgate** (street "Northgate") | middle class by the north gate, the North Road, the Market Hall and the abbey close | The city's front door: carts and drovers, the gate's hours, the inns, travellers' news, tolls, what came in on the North Road, the cathedral bells and the close next door. Worldly, quick to pass news on (only the topic's facts), a little self-important about being first to hear. Busier, faster talk than the Commons; comparisons with the towns the carts come from. |
| **The Sump** (tenements "X House, the top floor", "Pitch Corner", "Rope End", "Brine Buildings"...) | the poor, porters, stevedores, still hands, the out of work | Rough, quick, wary: clipped sentences, gallows humour, slang from the grammar (`#skint#`, `#toffs#`, `#rabble#`, `#lump#`, `#watch#`, `#gaol#`) used where it fits, never every line. Stairs, thin walls, damp, a roof that leaks, the landlord's man, the Watch as a nuisance, "up the hill" for everyone better off. Loyal to their own, suspicious of questions. **Never cruel caricature, never dropped letters**: hard lives, sharp wits. |
| **The Sump by the water** (street "the Wharf and the Sump") | dockside poor, net-menders, crane hands, carriers | The Sump's roughness with the water in it: the tide in the cellar, the piers, the cranes, the smell of the fish hall, flood streets in a storm. |
| **The North Headland** (street "Ridgemere Bay, North Headland") | outside the walls on the bay's north headland | Wind, gulls, the cliff path, the coves, the smokehouses and drying racks, a long walk to the gate. Plainer and slower than the Sump, weathered, a little apart from the city ("in town" means inside the walls). |
| **The Office** (Records Square, the City Office, the Hall of Records, the courts) | clerks, copyists, officials | Clerkly, precise, dry: forms, stamps, ledgers, petitions, the Council's sittings, "in order", the exact figure. Dry humour that undercuts. |
| **The Works** (ironworks, foundry, kilns, ropewalk, dyeworks, yards) | furnace and yard hands | Blunt, tired, shift-bound: the bell, the heat, burns, the foreman, pay day, sleep. Short sentences; pride in the work under the grumbling. |
| **The Estate** (Timberland Court, Grayrock Hall, Fellhall) | House households and their servants | Genteel and comfortable, or a servant's discretion about the family: the House's honour, gardens, dinners, the season's calls, what is and isn't said. |
| **The Heights / the Rise** | the Minor Houses, the rich | Formal and measured, complete sentences, understatement, civic duty, manners; distance from the lower city without contempt said aloud. Never slang (`#skint#`, `#lump#`) of themselves. |
| **The Wharf** | ships' folk, the harbour | Salt: ropes, cranes, tides, the harbourmaster, customs, weather from the sea; ships only by the topic's facts. |

**Place claims are gated, register is not.** A later wearer of this mantle is a plain-speaking newborn who may live
in another quarter. Write the register (tone, rhythm, slang, attitude) freely; gate the claims: "down here", "our
stairs", "the tenement" `self.quarter=sump`; "the gate" as home ground `self.quarter=commons` or the work; the Office
`self.quarter=office` or `self.job=official|scholar`; House matters `self.house` (the fact is the House's name: gate
on its presence, `[self.house]`); wealth `self.wealth=poor|destitute` / `rich`; class `self.class=upper`; city talk
`here.region=ridgemere`. Quarters are `heights commons sump office works wharf docks harbour_hill suburbs estate`.

**Every wolf unique by role, station, House, family and temperament.** From the brief: the **job/post** (gate it:
`self.job=`, `self.role=merchant`, `self.working=yes`); the **class and wealth** (the register within the quarter: a
rich innkeeper of Northgate is not a poor carrier of Northgate); the **House** (pride, duty, discretion, rivalry with
the other Houses, gated `[self.house]`); the **family** (spouse, children, parents in the household: feelings and
gated scenes, `self.married=yes`, `self.parent=yes`, `self.mother`...); and the **seven traits** (talk quiet → short
turns; chatty → runs on; merry → teases; dry → undercuts; nervy → worries twice; short → bristles; devout → the Bright
One in passing; ambitious → plans and money). Two of your own mantles must never sound alike either.

## Ridgemere's data traps (handbook §8, and what the last batches met)

- **Quirks**: Ridgemere's briefs give the same few quirks to many residents (sea songs, rain-watching, the damp
  cough, remembering debts, coal for luck, the long stairs, the bells, whetting a knife, naming ships, rival Houses).
  G2 refuses a third in the town: grep the index and **turn yours early** into something only this wolf does, drawn
  from their brief.
- **District slots** (§8.1): when `#self.workplace#` or `#self.street#` is a district ("Ridgemere, the Commons and the
  Rise", "Ridgemere, Northgate", "Ridgemere Bay, North Headland", "Ridgemere, the Wharf and the Sump") never put it
  where a building or a doorstep is meant; use own words. A street like "Pitch Corner, the top floor" reads badly too.
- **`#self.trade_words#`** is "keeping the inn", "runs the Rag Market stall", "smith at Eastgate Ironmongers" or
  "carrying for the Couriers' Office" by wearer: only ever **a fragment of its own** ("#self.trade_words.capitalize#.
  Long days.") or after a colon or comma. **Never** "I'm #self.trade_words#", "I'm a/the #self.trade_words#", "I keep a
  #self.trade_words#", "I work #self.trade_words#".
- **Friend slots that are kin** (§8.2): friend lines true if the friend is a spouse or child ("I'd trust #self.friend#
  with my keys"); never "not family, but...".
- **Repeated backstories** in the briefs: "grew up at a Timberland charcoal camp", "born in the Sump" (for a Northgate
  wolf), "saw the Chancellor weep in the Rain Hall", "a letter for Lord Grayrock", "a berth on a Grayrock ship
  somewhere dry", a House factor's debt (`self.owes` is usually no). Never a deed of a real person (the Chancellor,
  a lord); give each wolf their own angle, or leave it out; gate debt talk `self.owes=yes`.
- **`job none` for real work** (§8.3): gate on `self.working=yes` (+ `self.job=none`); "guards" with no post are not
  guards: write them as they are.
- **Capitalised places**: allowed mid-sentence: the realm's towns, the Houses, the quarters (Commons, Heights, Sump,
  Wharf, Estate, Works, Office), Northgate, Rise, Market Square, Market Hall, Rain Hall, City Office, Watch House, Rag
  Market, Sound, Ridgemere Sound, Customs House, Fish Hall, Ropewalk, Chapel of the Tide, the weekdays and their
  plurals (Restday, Restdays, Marketday...). Refused: inn and shop names ("the Grey Gull": use `#self.workplace#` /
  `#here.place_name#` when it is theirs, or "the inn by the gate"), "Midsummer", "Country", "Tuesday", any name a
  resident bears. G2 tells you; rephrase.
- G1 refuses `here.outdoors=yes` (its values are weather words) and `self.house` on a **graft choice** (gate a choice
  on `self.class=upper` instead); `#bc_it_did#`, `#bc_this_time#` don't exist.

## What the lead sends back (do it right the first time)

- **Openers never ask**: no alternative of a `greet`, `again.open`, `hail` or `kin.open` line ends with "?" (the
  engine drops it): "New face. Where from, then?" is wrong; "New face on the stair. Welcome to it." is right.
- **Never reuse a phrase of any other mantle**, the models above above all (the last batches copied sw101's "keep to
  the lit side of the lane", "thick enough to stand a spoon in", "I lift with whatever's nearest", its rats scene, its
  "came by and stayed an hour" / "singing through the wall"). Models are for shape, never words.
- **No oath or saying another mantle of any dialect has**: `python3 Saved/Mantles/xruns.py --habits-only <file>`.
- **No invented deeds of real residents** (slot people): only feelings, facts the game holds, or what the line
  records. "#self.neighbour# came round and fixed my door" is wrong; "I'd trust #self.neighbour# with my door key" is
  right. No invented losses of life, crews, culprits, prices or dates in news scenes.
- **No doubled determiners**: never "my #pet#", "my #dear#", "a #x#", "the #x#", "your #x#" when the symbol's values
  begin with a determiner ("a #penny#" = "a a penny"). Run `python3 Saved/Mantles/dets.py <your files>`: it must say
  "0 doubled determiners".
- **No famous riddles** (footsteps; "what has a neck and no head"): make up your own, or none.
- **Listener lines (`b :`) phrased your own way** too: G2 counts their 5-word runs ("What do you do with them?"
  collides).
- **Windows**: hub greetings by meeting (`topic.meeting=known` with `#topic.player#`), hour and disposition;
  `resident.job` (gated if trade-specific, plus one any wearer could say); `resident.bye`; every fact node (news,
  gossip, business, shop.greet, inn.greet/rest, watch.*, church.*, master.learn) **one line per case, gated like the
  tree's** (§4.5.8; read the tree file under `Data/Talk/dialogue/`); full tier with a role tree (shop, inn, watch,
  church, master) also that tree's nodes, gated where the tree's lines are. Write your own words, not the tree's.
  Grafts: one choice on a hub, plain English, gated if it assumes a trade, every node with a way back.
- **Edge lines**: core moves greet.return, farewell.return, react.news (+ .new .known .doubt, .sorrow/.glad gated on
  `topic.kind`), accept_decline, accept_preclose if you write preclose; hedged declines with a reason (6+ words);
  closings in pairs.
- **Scenes**: 3-5 turns, 2-3 threads, every line 1 or N alternatives; read each thread alone top to bottom: one
  natural talk, each line answering the one before, no cryptic replies, ending closed (never on an unanswered
  question). Gate the moment (weather, hour, season, `here.region=ridgemere`), the wearer, the world (`world.times=`,
  `world.wagons=`, the topic's facts). Cover the everyday topics this wolf would talk about, 2-3 scenes per favourite,
  and a few news scenes the facts allow (gossip passed on through `#topic.subject# #topic.claim#` or withheld; the
  times; the wagons late/in/raided; newcomers; festivals). Scenes are where a mantle is heard: they are the body.

## Gates (run until they pass; this is your definition of done)

```
python3 tools/mantle_batch.py gates <your file> --facts Saved/Mantles/residents.jsonl --no-g3   # while drafting
python3 tools/mantle_batch.py gates <all your files> --facts Saved/Mantles/residents.jsonl      # at the end, with G3
python3 Saved/Mantles/dets.py <your files>            # 0 doubled determiners
python3 Saved/Mantles/tradefix.py <your files>        # read what it lists: no "I'm #self.trade_words#"
python3 Saved/Mantles/xruns.py <your files>           # 5-word runs shared with ANY mantle of any dialect: fix yours
python3 Saved/Mantles/review.py --flags-only <your files>   # sexed words, slot deeds, prices, openers that ask
```

Pass = "G1 pass, G2 pass, lint pass, G3 pass", 0 refusals, 0 doubled determiners, xruns 0 shared (or only runs so
common they can't be helped, listed in your report); read every lint warning and fix it unless it's truly fine (say
why). G2 runs over the whole plain folder, so another writer's file landing may bring a 5-word run: **change your own
file, never anyone else's**. If a failure is in someone else's file only, ignore it and say so.

Then the self-review checklist (§6.5) for every mantle, and read every scene thread aloud in your head.

## Hard limits

Write only your own mantle files (and nothing else, anywhere). No git commands at all. Never run `mantle_load`,
`mantle_export`, SQL, `psql`, `world_db.py` writes, or anything that writes the database. No simulations, no servers,
no `talk_sim --tick`. Don't touch the main checkout (`/home/martinb/Documents/GitHub/MUDProject` itself), the tools,
the grammars, the trees, or anyone else's mantle. Don't spawn sub-agents: write them yourself.

## Your final report (your last message; under 25 lines)

1. One TSV row per mantle: `mantle  writer  started  finished  items  alternatives  note` (times from
   `date -Iseconds`; the note: quarter register, job/gating choices, quirk turned (from -> to), collisions fixed).
2. The final `gates` summary over all your files (the summary lines) and dets/xruns counts.
3. Data problems met (handbook §8 numbers, and new ones); lint warnings left and why; anything still failing.

## Notes from the first writers (read these too)

- A claim of living in, or being born to, a quarter ("Northgate bore me", "down here on our stair", "born within
  sound of the gate bell") is gated `self.quarter=<q>`; Northgate's and the Rise-side streets' residents have quarter
  `commons`. Mentioning a place the wearer passes or looks at needs no gate.
- Allowed capitals now also: Records Square, North Road, Market Hall, Great Palace, Palace, Old Keep, Rainwood, Chapel of the Tide,
  weekday plurals. Still refused: "Midwinter", "Moonday", "School", card-game names, "Houses'" (plural possessive).
- dlg keys are not scoped by section: in a mantle with young/adolescent/grown sections, name later dlgs uniquely
  (`dlg @g1 resident.hub ...`).
- `topic=houses`, `strangers`, `sump` are opinion issues, not topics (G1 refuses them as topics).
- Every writer gets the same brief phrases ("we keep a decent house", "new faces bring new coin"): never use them.
- Files land beside yours all the time: run `xruns.py` and the gates once more as your very last step.
- **One wolf is never "they"**, in the listener's lines too: after "#topic.subject# #topic.claim#" a reply says
  "Good for #topic.them#", never "Good for them"; after "#self.child# makes me proud" never "You should tell them".
  Run `python3 Saved/Mantles/slotthey.py --deep <your files>` and fix every one that means the slot's wolf.
- **Reserved window node names** (doc 84 §45, the window's new nodes): a graft may **not** name its own nodes
  about, how, hours, quirk, opinions, opinion, kin_list, kin_said, told, vouch, letters_said, festival_said, records,
  people, whereis, day, wanted, match, introduced, name, thanks, unclear. Name yours more specifically
  (`how_remember`, `day_off`...). (kin, letter, festival stay fine.)
