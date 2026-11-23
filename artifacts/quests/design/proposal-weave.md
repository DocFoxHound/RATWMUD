# The Ninth Howl: the first grand campaign of Ridgemere 3 (proposal "weave")

Designer: weave. The angle is a braid driven by the players. Every town has its own thread, and the threads come together in one crisis for the whole realm: the Moot of the Long Night, the first in living memory, at which the nine voices of the realm must choose who reigns after King Gunnar.

Every id below was checked against the surveys and, where noted, against `ratw_ridgemere3` (read only): the residents in `live.npcs` with a home (placed), and the interiors in the reachable list. Nothing in the repo or the world was changed.

---

## 0. The pitch in one paragraph

King Gunnar Harrowgate, born in the Sump, died of old age on day 41. Five days later the Great Palace passed by Gunnar's own "Lantern Law" to the wolf who held its lamp at dawn. That was Dagfinn Thornham, a fen-born Watch patrolman from the Sump who knows which streets flood first. Gunnar wrote the law to stop the Houses seizing an empty throne. It says the lamp-holder reigns only "until the Moot of the realm names the next". No Moot has sat for two generations: the Houses sealed the Rain Hall, and the Council of Houses has since spoken for the realm. A Moot needs **nine howls answered**, one from each community that once sent a voice. One of the nine, Mirebridge, was drowned and erased, and its people are scattered. So the Houses believe no Moot can ever sit. They are racing to make the Council name the ruler first. Greatly calls in its debts at the new moon to buy voices. Fell grabs the Rainward Hills. Grayrock hides on the road from a letter. The Watch is paid to look away from the bandit camps that choke the roads to the towns. The players are wolves of every town. They carry the Summons, open the roads, wake each town's howl, find the scattered of Mirebridge, raise its drowned bell from under the Upper Mere ice, and stand in the rain at Midwinter while the realm chooses. Every town's thread decides how its voice will howl. Every wolf's deeds are written on the Moot Roll.

---

## 1. Premise: why now, rooted in the seeds

| Seed (from the surveys) | What it becomes in the campaign |
|---|---|
| King Gunnar died on day 41; Sump guard Dagfinn Thornham (`rmx_dagfinn_thornham`) "took over the work of reigning" on day 46 by the vacancy rule | **The Lantern Law.** Gunnar, Sump-born, wrote that the throne may not stand empty one night: whoever keeps the Palace lamp at dawn reigns until a Moot names the next. On the night he died, the Palace guard was short and the Sump round was sent to keep the lamp. That was Dagfinn. The engine accident becomes the plot's first fact. His fen-born voice (mantle mb131, a Fenshore upbringing) ties him to the Mirebridge thread. |
| Queen Oda (`rm_oda_harrowgate`) and Princess Ottilie (`rm_ottilie_harrowgate`) passed over | Oda, sly and coin-counting, will not accept a Sump patrolman. She pays a spy in the household and bargains with Greatly. Ottilie is gentle, frail and loyal, and carries the Crown's petitions. She is the rightful heir if the Moot wants a Harrowgate. |
| "The Houses will fall out with each other before the next winter" (29 to 38 residents) | The quarrel the campaign builds to. Which Houses fall out, and how badly, depends on the players' branches (chapter 6). |
| "Owes a House factor money, to find by the new moon" (34 to 41, including Captain Corra Slagmore and Steward Edric Kelpham) | **The New-Moon Reckoning** (chapter 2). Ambrose Greatly calls in every small debt at once. A debtor who cannot pay must "lend his howl" to Greatly at the Moot. |
| "Keeps a secret list of which Houses pay the Watch to look away" (26 to 35, including Treasurer Karin Stanmore, Abbot Lionel Ropestack and cell-keeper Desmond Dripcot) | **The Lists.** The Watch is paid to look away from the bandit camps that cut the towns off from the Summons. Put the lists together and you know who paid for the closed roads (chapter 3). |
| "Carries a letter meant for Lord Grayrock, who is never home" (28 to 40, including Dagfinn himself, Sergeant Dagny Ropeley, priest Solveig Tarwell and innkeeper Mab Carlisle) | **The Grayrock Letters.** They are the realm's petitions. One of them is Gunnar's last letter, asking Grayrock to keep the Wharf and the roads open for a Moot. Lord Aldric avoids the Hall because reading it would bind his House (chapter 5). |
| Corwin Fell wants the Rainward Hills; Gudrun Fell writes secretly to Timberland | **The Hills** (chapter 5): Fell's bounders on the Oakhaven road and in the Rainwood, and Gudrun's letters as the hinge between Fell and Timberland. |
| "Saw the Chancellor weep in the Rain Hall" (38 to 46); the Rain Hall `rm_the_rain_hall` is unreachable | Twenty-two years ago a young clerk, now Chancellor Oskar Gullworth (`rm_oskar_gullworth`), was made to lock the Rain Hall when the Council took the Moot's powers, and he wept as he turned the key. The Hall stays sealed in the world, which is true to the data. His confession (chapter 7) breaks the Council's claim. |
| "Saw something big move under the ice" (7, all Coldwater: skipper Kell Greyton `tn_cw_kell_greyton`, carter Willa Coldwick, Myrtle Stillford and others) | **The Mirebridge Bell.** When the Long River drowned Mirebridge, its gathering bell was hauled upriver for safety. The barge went through the Upper Mere ice, and in winter the current rolls the bell along the bottom. That is the "something big" that moves. |
| "The marsh lights are the drowned" (7, Fenshore and the diaspora) | The lights are a Quickened wolf, **Wisp** (a stranger, chapter 6), a survivor of the drowning as a pup. She lights the old safe paths in the fog. |
| Erased Mirebridge: the crossroads at `w_ridgemere_5_11` (188, 52), dialect kept by Fenshore, lore `mb-fens-1` ("a path with no note at all") | **The Ninth Howl.** The Moot needs Mirebridge's voice. The `tn_mb_*` residents scattered across the realm (Aspen Marshholm, Tansy Eelbridge, Skye and Tamsin Eelbank, Oriel Marshditch, Bramble Bogditch, Roan Pikewater, Thorn Marshditch and others) are written as having Mirebridge blood. Their names, howls and memories are the strand that restores it. |
| Bandit camps keep forming at `6_8` (Glasswater roads), `5_4` (Oakhaven road), `8_6` (Frosthollow road), `7_12`, `6_12`, `1_3`, `2_3`, `3_3`, `6_3` | The roads the Summons must travel. The open watch-post projects proj-1 (`8_6`), proj-2 (`5_4`) and proj-3 (a Ridgemere waystation) become the first lasting visible changes. |
| Lore `any-howl-1`: "every town has a howl for gathering, a howl for warning and a howl for the dead, and no two towns agree which is which" | The Nine Howls themselves. Each town must find its own gathering howl again. The joke in the lore becomes a quest: "the town that sang the dead-howl at a wedding". |
| No Warden faction; Quickened and Gifted wolves raise Warden attention; Quickening needs `closedStories: 2` | Wisp's lights and the players' own Trances draw a Warden envoy in chapter 6. Every personal side story is a closed storyline, so the campaign is also the realm's road to the Quickening. |

**Why now:** the Lantern Law gives the lamp-holder until "the turning of the year", Midwinter. If no Moot has sat by then, the Council of Houses names the ruler. The deadline is in the law and in the calendar: day 46 of winter, the Long Night, Midwinter or the Hearth Feast by each town's festival name.

---

## 2. Design principles (the braid rules)

### 2.1 Three rungs: Spark, Ember, Blaze (players always make a difference)
Every deed climbs three rungs, so feedback is never more than a short wait away.
1. **Spark (at once, personal).** A personal tracker ticks ("Your voice: 7"). The giver says a thank-you line (quest `offer.thanks`; later `npc.greet`, C5). A `deed.award` goes into the beliefs, so residents greet the wolf with "You #topic.deed#, didn't you?".
2. **Ember (within a game day, the town).** Each town's strand (a world number variable) has **step thresholds**, about every 20% of its goal. Each step fires something you can see in that town:
   - a `board.post` on `board:<town>` (pre-written text for each step);
   - a `story.tell` to the town's region, `from` its Howl-keeper with their face;
   - a `talk.remember` key for that town (C8), so the town's gossip changes;
   - at some steps, a `festival.call`, an `event.add` or a project.
3. **Blaze (the chapter, the realm).** When the realm goal is met, the chapter turns. A world `story.tell` comes from Herald Brenna Abbot. A lasting change follows (a watch post, a resident's fate, a faction relation, a figure), and every town's talk moves to the new stage.

### 2.2 Goals with floors and surplus (from 5 wolves to 500)
- **Floor:** every goal is `max(floor, rate × active audience)`. This needs operand C2 `audience`, and goals read from an operand (N5). Today it is a fixed goal sized for 5 to 20 wolves, with `contributors: 2`.
- **Surplus is never wasted.** Contributions past the goal fill a **surplus** variable, which buys extra world change: another watch post, a second festival, a better ending clause at the Moot. A crowd of 200 therefore gets *more world*, not a goal that is over in an hour.
- **Distance weight.** A deed in a far town counts for more: Saltwell, Southmeadow and Larkfield count double, and Frosthollow and Coldwater one and a half times (walks of 5,648 and 5,532 tiles versus Oakhaven's 1,546). This pulls crowds away from the spawn on Market Square.

### 2.3 Taking a howl (how a crowd spreads itself out)
In chapter 1, each wolf **takes a howl**: they pledge to stand for one of the nine voices, held in the player text variable `p_howl`. A wolf can change it once, at each chapter's start.
- A pledged wolf's deeds anywhere add a quarter-share to their town's strand, and their name goes on that town's roll.
- A lone wolf pledges one town and *is* that town's champion.
- At 200 wolves, every town has twenty or so. The **contribution rank** (C3) inside each town names that town's Howl-friends.
- Wolves who pledge to Mirebridge start the hardest strand, which has no town and no board. They get the richest personal story.

### 2.4 Many hands, one weave (how personal choices become branches)
The big branches are not one wolf's choice, and they are not one vote either. Each is the **tally of many personal choices**. Each wolf who meets Gudrun Fell decides what to do with *their* letter. Each wolf who finds a page of the bribe lists decides where *that page* goes. The realm's branch is whichever choice holds the most weight (deeds × distance) at the chapter's end.
- A lone wolf decides alone.
- With 200 wolves, the outcome reflects the realm, and the wolves on the losing side still get their own personal ending.
- This works today with `each over participants` (at most 500 turns) and player variables. A `tally` operand (N6) handles more than 500.

### 2.5 Failure never blocks
- No chapter fails. A missed deadline moves to the chapter's **worse road** (`clock` exit to a phase variant). The worse road costs the realm something real: Greatly buys howls, a bridge stays held, a resident is hurt. The campaign always goes on.
- Every loss can be **won back later** at a price. For example, "buy back the howls" in chapter 4 undoes the Reckoning.
- Personal quests fail per wolf (`fail` rules, `finish who:$actor`) and are offered again `weekly`.
- The Moot always sits. The worst ending ("the Bought Crown") still has a living sequel hook.

---

## 3. The braid map (towns × chapters)

The strands, their keepers and antagonists. The **Howl-keeper** is the resident who howls for the town at the Moot.

| Strand | Howl-keeper (id) | Other threads | Pressure | Legend |
|---|---|---|---|---|
| Ridgemere | Herald Brenna Abbot `rmx_brenna_abbot` | Dagfinn, Ottilie, Oda, the Houses, the Watch, Oskar | throne, debts, lists | the Rain Hall tears |
| Coldwater | Fallow Lakewick `tn_cw_fallow_lakewick` (loom master, devout, distrusts the city) | Kell Greyton (Upper Mere skipper), Willa Coldwick (carter), Loam Pinefell (Glasswater Inn), Lorn Hedgeton `cty_lorn_hedgeton` (informer) | Glasswater road camps `6_8` | the bell under the ice |
| Larkfield | Aspen Marshholm `tn_mb_aspen_marshholm` (linen master, Mirebridge blood) | Dun Harvestley `tn_sm_dun_harvestley` (printer), Tansy Eelbridge `tn_mb_tansy_eelbridge` (smith, "a path across any bog"), Pike Chalkley, Bramble Reedford (The Sheaf) | Greatly factors short-weighing grain debts | the Free Press |
| Southmeadow | Iris Meadowfield `tn_sm_iris_meadowfield` (Larkholm, frank) | Briar Cornby `cty_briar_cornby` (sly, standing-stones lore), Aster Cornley (bees), Rowan Downridge (Vine and Sickle), Orrin Appleley (potter) | Fell buying the vintage forward; camps `7_12`, `6_12` | the stones remember |
| Fenshore | Nell Plummeadow `cty_nell_plummeadow` (The Heron) | Skye Eelbank (guide), Tamsin Eelbank (70, "the lights are the drowned"), Sloe Mistwick (smuggler), Ludo Beechshaw (took the dead potter's post), Roan Pikewater | fog, smugglers, the Wardens | the marsh lights |
| Saltwell | Bramble Bogditch `tn_mb_bramble_bogditch` (78, devout; must be spared) | Oriel Marshditch (Oasis Inn), Thistle Stonepick (mine-collapse grief), Lavinia Mudworth (Redsand) | the Crown's salt customs (Jasper Bilgeford), caravans | the well remembers |
| Oakhaven | Tamsin Icefell `tn_fh_tamsin_icefell` (Felled Cedar, wants to leave) | Ulla Rusthand `cty_ulla_rusthand` (logger, wrongly believed a thief by 222), Moss Blossomwell (printer), Cairn Oakhaven (sly trader), Nan Ashhand `cty_nan_ashhand` (Rainward quarry) | Fell's bounders, camps `5_4`, `3_3` | the old right of the cut |
| Frosthollow | Thorn Ashwood `tn_oh_thorn_ashwood` (Lamp and Pick) | Mine captain Fenwick Plankworth `cty_fenwick_plankworth` (sly), Aster Coldfell (sly, wants to leave) | Greatly holds the mine's note; camp `8_6`; proj-1 | "Count twice" |
| Mirebridge (erased) | chosen in chapter 6: Wisp (stranger), Tamsin Eelbank, or Dagfinn himself | the scattered `tn_mb_*` wolves, Imogen Nailridge (maps), Solveig Tarwell (the drowned) | it has no town, no board and no purse | the bell, the lights, the Book of Names |

Which strands lead in each chapter (●) and which are touched (○):

| Ch | Ridgemere | Coldwater | Larkfield | Southmeadow | Fenshore | Saltwell | Oakhaven | Frosthollow | Mirebridge |
|---|---|---|---|---|---|---|---|---|---|
| 1 Keys at Dawn | ● | ○ | ○ | ○ | ○ | ○ | ○ | ○ | ○ (Dagfinn's accent) |
| 2 New-Moon Reckoning | ● | ○ | ● | ○ | | | | ● | |
| 3 Open Roads | ○ | ● | ○ | ● | | | ● | ● | |
| 4 Nine Howls | ○ | ○ | ○ | ● | ○ | ● | ○ | ○ | ● |
| 5 Hills and Letters | ● | | | ○ | | | ● | ○ | |
| 6 Lights in the Fen | ○ | | ○ | | ● | ○ | | | ● |
| 7 Bell under the Ice | ● | ● | | | ○ | | | ○ | ● |
| 8 Moot of the Long Night | ● | ● | ● | ● | ● | ● | ● | ● | ● |

### 3.1 The campaign's state (shared world variables)

| Variable | Type | Shown as | Meaning |
|---|---|---|---|
| `realm_voice` | number, world | "The realm's voice" | the main bar, always rising |
| `strand_<town>`, 9 of them | number, world | "Coldwater's howl" and so on | the strands (9 trackers + 1 fits the 16-tracker cap) |
| `keepers_named` | number, world | "Howls answered" | 0–9 |
| `greatly_howls` | number, world | hidden | voices Greatly has bought; can fall again |
| `b_oath`, `b_ledger`, `b_lists`, `b_hills`, `b_gudrun`, `b_grayrock`, `b_wisp`, `b_bell`, `b_oskar` | text, world | hidden | the branches |
| `p_voice` | number, player, carried | "Your voice" | the wolf's personal contribution |
| `p_howl` | text, player, carried | (journal note) | the wolf's pledged strand |
| `p_ballot` | text, player | | the wolf's Moot choice |

Each chapter is one program of the campaign (`story.start` with `carry: true`, audience `everyone`, `days: 30`). Quest packs (small, medium, large) are separate programs with an open audience. They `add` to the same world variables, and the chapter polls them on `hour` (today) or hears `variable changed` (C13).

---

## 4. The chapters

### Chapter 1: The Keys at Dawn
**When:** from publication (spring, about day 70), for about 14 game days.
**Where:**
- Ridgemere: the King's Residence `r2_the_king_s_residence`, the Antechamber `r2_the_antechamber`, Market Square (spawn, `w_ridgemere_1_1` (104, 56)), the Watch Office `r2_the_keep`, the Sump;
- then the roads to all eight town boards.

**Opening (entry effects):**
- Herald Brenna Abbot's `story.tell` to the world, with her face: "By the Lantern Law of King Gunnar, the lamp is kept. A Moot is called for the turning of the year. Let every hearth hear it."
- `board.post` on all eight boards: *"THE LAMP IS KEPT. A Moot of the realm is summoned for Midwinter. Wolves who will carry the Summons, ask at the Great Palace for the Herald."*
- `figure.set` (C9) with role ruler = Dagfinn Thornham, title "keeper of the lamp" (fallback: a DM figure row). Every existing `#world.figure_ruler#` line then speaks of him.
- `npc.protect` and `npc.spare` (C11) on the key cast: Dagfinn, Oda, Ottilie, Brenna, Bramble Bogditch, Halvard, Ambrose and Gerrit.

**Shared goal: "Let every hearth hear it."** Wolves carry sealed Summons. Brenna gives a `letter` with `item.give`, and the wolf gives it to any resident in a town (objective `gift` item `letter`). A trigger checks `wolf in town` and adds to that strand.
- Goal: `max(16, 2 × active)` deliveries, at least one in each of the eight towns. That is two per town at five wolves; at 200 wolves the farms and cottages count too.
- Each delivery adds 1 to `realm_voice` (2 in the far towns).

**Mid-chapter steps (Ember):**
- First delivery in a town: that town's board changes to "The Summons was read here by a wolf of the road", and its innkeeper's `npc.line` changes.
- Half of the goal: Dagfinn walks out of the Palace (`npc.walk` to Market Square, cue `lamp`) and stands by the square's board at dusk. Scene `lamp_at_dusk` plays: "I kept a lamp. I didn't ask for a crown."
- Goal met (Blaze): the **Oath at the Lamp** scene.

**Branch B1, the Oath at the Lamp** (public scene in Market Square, `choice` mode `majority` among the wolves present; a wolf who is away sends a vote through `npc.offer` at Brenna, tallied):
- **"Swear to keep the lamp until the Moot, and no longer"** (`b_oath = steward`). Dagfinn cannot stand at the Moot. The Houses relax a little, so chapters 2 and 3 are easier. The possible endings become Ottilie, a Regency or the Bought Crown.
- **"Swear to keep the lamp"** (`b_oath = open`). Dagfinn may stand. Oda and Greatly harden, so the Reckoning goal rises by 25%. This opens the "Lamp-King" ending.

**Side storylines** (one per wolf; offered by where a wolf is and by a `random` pick among the eligible):
1. **The Spy in the Kitchen** (Ridgemere, the Residence kitchens `r2_the_king_s_residence_down`). Servant Mab Coalridge `rm_mab_coalridge` carries £218, far too much for her wage.
   - The wolf scents the Greatly Bank's lamp-oil smell on her coin (a `conversation` with her, then with moneychanger Eleanor Saltcot `rm_eleanor_saltcot` at `rm_the_greatly_bank`).
   - The choice: tell Dagfinn (Mab is dismissed and Oda's spy is gone), tell Oda you know (Oda owes the wolf, which pays off in chapter 8), or keep quiet and take Mab's bribe (a coin reward and a hidden mark against the wolf).
2. **Ottilie's Petitions** (the City Office `rm_the_city_office`).
   - Ottilie asks the wolf to find the 25 to 38 petitions "lost in the City Office": conversations with the residents who carry that backstory, each named by a chain of `npc.offer` hints.
   - Every petition found adds to `strand_ridgemere` and to the wolf's standing with Ottilie. The wolf learns that Ottilie coughs blood in the damp (sets up a healer side story in chapter 4).
3. **The Patrolman's Round** (the Sump).
   - Dagfinn asks the wolf to walk his old Sump round "so the streets know someone still walks it": `region entered` on the six `r2_sump_patrol` posts in order.
   - At the end, the fen-born King tells the wolf in his own voice where he grew up. He gives a Mirebridge cue: "the long way round".
4. **(Outside the city) The Summons at the Lamp and Pick** (Frosthollow). Innkeeper Thorn Ashwood will not post the Summons until a wolf has gone down the mine and "counted twice": a `region entered` at the pithead `w_ridgemere_9_2` (212, 213), then a talk with mine captain Fenwick.

**Lone wolf and crowd:**
- A lone wolf can carry all 16 Summons in about two real days, and every town greets them by deed.
- With 200 wolves the Summons is done in hours. The surplus is "readings": every further delivery in a town adds to the crowd at the Oath scene, and a surplus over 2 × the goal calls an early **Lantern Night** `festival.call` in Ridgemere.

**Quest types:** courier (Summons), scent-trace (Mab), errand chain (petitions), patrol (the round), descent (the mine).

**Talk (gossip and helpful knowledge; key `world.mem.ninth=summons`):**
- *plain, Ridgemere:* "A Sump patrolman keeping the Palace lamp. Blood and bread, the Heights are choking on it." / "Want to be useful? The Herald's giving out the Summons at the Palace door."
- *coldwater:* "A Moot, eh? My grandsire spoke of one. Said it was the only time the city asked the lake anything."
- *larkfield:* "Summons came through on the morning cart. Scales and silver, they'll want our grain before they want our voice."
- *southmeadow:* "A Moot at Midwinter? Ripe days for talkers, then."
- *mirebridge, Fenshore:* "That new lamp-keeper's got fen in his mouth, I hear. Talks like my uncle, no?"
- *saltwell:* "The Summons reached the well. The well remembers who drew from it: we'll see if the city does."
- *oakhaven:* "Slow news, the city. A king dead a fortnight before the Sound Road hears it, mm?"
- *frosthollow:* "Patrolman on the throne. Deep strange. Keep your lamp lit, they say. Well, someone is."
- *helpful, any innkeeper:* "If you're carrying the Summons, the far towns want it most. Saltwell's not heard a city word since the last caravan."

**Triggers needed:** existing: `item.give`, a `gift` objective, `wolf in town`, `add`, `board.post`, `story.tell from`, scene majority, `npc.walk`. New: C2/N5 (goal by audience), C9 (`figure.set`), C11 (`npc.spare`), C5 (thanks by name).

---

### Chapter 2: The New-Moon Reckoning
**When:** until the next new moon, the end of the 28-day month (C13 `month begins`; today a `clock` exit with `afterDays` counted from entry).
**Where:**
- Ridgemere: the Greatly Counting House `rm_the_greatly_counting_house`, the Bank, Greatly House `rm_greatly_house`, the Pawn and Ledger `rm_the_pawn_and_ledger`;
- Larkfield: the granary at Longmead `fs_grain_long_granary`, and the Press `cn_larkfield_press`;
- Frosthollow: the mine.

**Opening:**
- Strangers come: `visitor.add` "a Greatly factor" in grey-blue (`like: rm_eleanor_saltcot`) on each town square in daylight, re-added daily (N2 would let them talk).
- Board post: *"BY ORDER OF THE COUNTING HOUSE: all notes fall due at the new moon. A debtor who cannot pay may pledge his voice at the Moot to House Greatly in lieu."*
- `figure.set` with role villain = Ambrose Greatly, known in the realm.
- The 41 debtors' talk turns frightened (`talk.remember` scope npc on each debtor, `debt=pressed`).

**Shared goal: "Not one voice sold."** Wolves free debtors:
- earning or giving coin to them (objective `gift` measured in `coins` to a debtor in the `debtors` list; a trigger matches `in list`);
- doing their work for a day (`contract done` at their workplace);
- finding proof the debt is false (the ledger).

The bar is **debtors freed of the total pressed**, with a goal of `max(12, active × 1.5)` and a cap of 41.
- **Deadline at the new moon:** every unfreed debtor's voice goes to Greatly (`greatly_howls += unfreed / 4`, rounded).
- Captain Corra Slagmore `rm_corra_slagmore` and Steward Edric Kelpham `rm_edric_kelpham` count triple.

**Mid-chapter steps:**
- 25% freed: Larkfield's Press prints a handbill: "A debt is not a voice" (board, Larkfield and Ridgemere).
- 50%: debtors' gossip turns from fear to anger, and the Greatly factors stop coming to Coldwater and Oakhaven (`visitor.leave`).
- If Corra Slagmore is freed: she says so to the wolf by name (`npc.greet`) and opens chapter 3's Watch help.

**Branch B2, Rosalind's Ledger.** Rosalind Greatly `rm_rosalind_greatly` has "found a ledger that does not add up". The **personal side story** "The Ledger in the Garden" brings a wolf to her in the Greatly gardens. Proving it means scent-tracking the false entries:
1. the factor's note at the Pawn and Ledger (pawnbroker Jorunn Ropewick, fence of pledged goods);
2. the grain tallies at Longmead, where Rook Wheatfield shows short-weighed sacks: "short-weighed", in Larkfield's own word;
3. the mine's note at the Mine Captain's House `cn_frosthollow_mine_house`.

Each wolf who completes it holds a copy and chooses:
- **Show it to the Council** at the Antechamber (to Gerrit Mudhand `rm_gerrit_mudhand`) (`b_ledger = shown`):
  - every pledge Greatly took on a short-weighed note is void (`greatly_howls` halved);
  - Ambrose is shamed: `faction.relation` house_greatly–council_of_houses goes to tense;
  - Rosalind is turned out of Greatly House and lodges at the abbey (N4 `npc.stay` at `r2_abbey_quarters`);
  - she becomes Greatly's conscience and a possible voice at the Moot.
- **Bargain with Ambrose**, giving the copy to Steward Edric for Ambrose (`b_ledger = kept`):
  - Ambrose forgives every debt under 20 pennies (all the Sump debtors are freed at once), but keeps the big notes, Corra's among them;
  - Greatly stays friendly to the Crown, and Ambrose owes the realm one favour at the Moot;
  - Rosalind stays, and turns cold to wolves.
- **Burn it** (`b_ledger = ash`): nothing changes. Rosalind trusts *that* wolf above all others; it is a personal ending.

The tally is weighted by each holder's `p_voice`.

**Other side storylines:**
1. **The Captain's Note** (the Watch House `rm_the_watch_house`). Corra Slagmore, guarded, asks a wolf with standing to pay her note quietly, or to find who sold it to Greatly. It leads to Desmond Dripcot `rm_desmond_dripcot` in the cells and his list, which sets up chapter 3.
2. **The Seam Note** (Frosthollow). Greatly holds the Frosthollow mine's note.
   - Fenwick Plankworth, sly and restless, offers the wolf a share if they "lose" the note-bearer's letters on the Pass Road.
   - The honest road instead fills the mine's quota (objective `gift` `iron_ore` to Fenwick, goal 20; or `hunted`/`forage` for the miners' larder).
   - Fenwick's fate is set here: honest (he becomes Frosthollow's second voice) or crooked (he is arrested in chapter 3 by `warrant.issue`, and his post goes to the vacancy rule; N-post).
3. **The Short-Weighed Sheaves** (Larkfield, small and repeatable). Weigh sacks at the granary (`trade done` at the store) and carry the true weights to Dun Harvestley's press (gift `paper`, `printing_ink`).

**Lone wolf and crowd:**
- At 5 wolves the goal is 12 debtors, and every freed debtor thanks their freer by name.
- At 200 the goal is the full 41 plus the surplus "the Debt Fund". Surplus coin (`gift` coins to Brenna) is held by the story. At the Moot it buys back `greatly_howls` one for one (N: needs `character.grant` from a story purse, or a town purse `town:ridgemere`).

**Failure:** the new moon passes and Greatly holds howls. That is not a block. Chapter 4 has "Buy Back the Howls".

**Quest types:** coin relief, work-a-day contracts, scent-tracking, investigation chains, ore hauls, a printing errand.

**Talk (`world.mem.ninth=reckoning`, plus npc memories `debt=pressed|freed`):**
- *plain (a debtor, pressed):* "They want my howl for my note. A howl! As if it were a spoon to pawn."
- *plain (freed):* "#thanks#. A wolf paid my note and asked nothing. I'll howl how I please now."
- *larkfield:* "Short-weighed, the lot of it. Fair price to you, they say, and the scale's got a thumb on it."
- *frosthollow:* "Captain's quiet. Too quiet. Dead rock talk, that note."
- *helpful (Eleanor Saltcot's clerk, a bark at the Bank):* "If you want the notes, the Pawn and Ledger buys and sells them. Smell the ink: Counting House ink smells of lamp oil."
- *coldwater (after the factors leave):* "Factor's gone off the crossroads. It'll float, this Moot, eh?"

**Triggers needed:** existing: `in list`, a `gift` coins measure, `contract done`, `clock`, `faction.relation` (B), `warrant.issue`. New: C13 `month begins`, C8 `talk.remember` with npc scope, N4 `npc.stay`, N-post (`npc.post`, C14), N2 speaking strangers (the factors), C5.

---

### Chapter 3: The Open Roads
**When:** about 25 game days, into early summer (`season begins` summer is the soft gate to chapter 4).
**Where:**
- the camps at `6_8` (Glasswater roads, between Coldwater and Larkfield), `5_4` and `3_3` (the Oakhaven road), `8_6` (the Frosthollow road), and `7_12`, `6_12` (Sunfields);
- the watch-post projects proj-1 (`8_6`) and proj-2 (`5_4`), and the Ridgemere waystation proj-3;
- the Keep's cells `r2_the_keep_down`; Larkfield's Press.

**Opening:**
- `story.area.spawn` at each named camp (at most 6 each) and `bandits.call` near lone wolves on those roads.
- A foe (C10 `foe.add`) leads the Glasswater camp: **the Grey Captain**, `like: rmx_desmond_hollis`, the dismissed Palace guard who has no post, `strength: tough`, with a band of 4.
- The boards read "ESCORT WANTED" for the Summons carts.

**Shared goal: "Every road open."**
- Each camp cleared adds a large amount to the strand of the nearest town: 6_8 feeds Coldwater and Larkfield, 5_4 and 3_3 Oakhaven, 8_6 Frosthollow, 7_12 and 6_12 Southmeadow.
- Each watch post built (project gifts of `timber` and `stone`) adds to `realm_voice` and is the chapter's lasting change.
- The goal is all five camp sites cleared *and held*: a camp that re-forms within 3 days counts again. Also `max(2, active / 25)` watch posts built.

**Mid-chapter steps:**
- First camp down: the cart from Larkfield arrives on time, and the boards say so.
- Glasswater cleared: the **Grey Captain** is downed, or flees and comes back at 6_12 with C10 `foe fled`. If downed, he is held and talks: the hand that paid him is the hand on the lists.
- Each watch post completed: `project.complete` (B). Watch posts stay as structures in the world. The innkeepers' `npc.line` in that town changes to "the post's manned now".

**Branch B3, the Lists.** Three residents hold pages of the bribe list: Treasurer Karin Stanmore `rm_karin_stanmore`, Abbot Lionel Ropestack `rm_lionel_ropestack` and cell-keeper Desmond Dripcot `rm_desmond_dripcot`. Each gives one page to a wolf they trust (a medium personal quest each: "The Treasurer's Page", "The Abbot's Page", "The Gaoler's Page"). A wolf with a page chooses:
- **Print it** at Larkfield's Press with Dun Harvestley (`b_lists = printed`):
  - the Houses that paid for the closed roads are named on every board (pre-written by the `b_ledger` branch: Greatly if the ledger was kept; Fell and Greatly if it was shown);
  - Captain Corra is disgraced. If her note was freed in chapter 2, she **resigns in honour**; if not, she is **dismissed**. Either way Sergeant Bettany Nailley `rm_bettany_nailley` takes command (N-post);
  - the faction relations of the named Houses with ridgemere_watch go hostile;
  - the camps stop re-forming for 20 days.
- **Give it to the Watch quietly** (to Corra if she was freed, otherwise to Bettany) (`b_lists = quiet`):
  - the Watch is purged without a scandal and Corra keeps her post, redeemed;
  - the Houses do not know who knows, so at the Moot the wolves can use the lists once to sway one House's voice;
  - the camps stop re-forming for 10 days.
- **Sell it back** to the House it names (`b_lists = sold`): coin for that wolf (`character.grant` from `town:ridgemere`) and a hidden mark against them. The camps keep re-forming, and the next chapter's goals rise.

**Other side storylines:**
1. **The Wronged Logger** (Oakhaven). Ulla Rusthand `cty_ulla_rusthand` is "believed a thief by 222". The wolf follows the true scent from the robbed Oakhaven wagon to the `5_4` camp, and finds the stolen goods carry the bounders' camp-smoke, not Ulla's. Each gossiper the wolf corrects (a `conversation` with residents who hold the belief) lowers the gossip. Lasting result: Ulla speaks for Oakhaven's cut in chapter 5.
2. **The Coldwater Informer.** Sheep farmer Lorn Hedgeton (cold, ambitious, "watches the road") sells cart times to the Grey Captain for Greatly coin.
   - Expose him: `warrant.issue` by the Watch; he leaves for the Greyteeth fold and loses his say.
   - Or turn him: he feeds the Grey Captain false times, so the wolves ambush the band, and he is grudgingly redeemed.
3. **The Bee-Wagon** (Southmeadow, escort). Aster Cornley's honey wagon is to go to Ridgemere by way of the `7_12` and `6_12` camps. It is a repeatable escort; the wagon arriving fills the Southmeadow strand.
4. **Waystation Raising** (Ridgemere, large group). proj-3 needs hauling: timber from the Rainward Woods `2_0` and stone from Nan Ashhand's quarry `4_1`. Harnessed haulers (`labour` practice) are thanked by Nan: "I'm saving for something. Maybe it's this."

**Lone wolf and crowd:**
- A lone wolf with a Watch escort (`npc.follow` of Bettany for a day) can clear one camp, and every camp counts.
- A crowd clears all five in an evening. The **surplus** builds more watch posts (the Coast and Fen roads; `project.post` new sites) and calls **the Road Feast** in the town with the most cleared camps nearby.
- Bounty contributors are ranked per road (C3). The top wolf on each road earns the title "Roadwarden of the <road>" (C4; fallback: a `deed.award` that the "road" nickname family picks up as "the Roadwarden").

**Quest types:** camp-breaking (`camp cleared`, `bandit falls`), a foe fight, escort (wagon), building (project gifts), name-clearing (talk chains), page-carrying (secret courier), printing.

**Talk (`world.mem.ninth=roads`; per town `world.mem.road=open|held`):**
- *coldwater (road held):* "Glasswater road's bad. Willa won't take the cart past the ford without wolves on it, eh?" (helpful: "The camp's south of the mere road, where the alders thin.")
- *coldwater (road open):* "Calm water on the roads now. A wolf broke the Glasswater camp, they say. Fair catch."
- *oakhaven:* "Bounders in green, mm? Not robbers. Robbers don't drive stakes."
- *southmeadow:* "The bee-wagon got through! Sheaf and sickle, a windfall of a week."
- *larkfield (lists printed):* "Sold! Every board in the realm, by the market cross, and the Houses' names on it."
- *plain (lists quiet):* "Watch is changing faces and no one's saying why. Funny, that."
- *frosthollow:* "Post's up on the pass road. Warm hearth to the wolves that built it."

**Triggers needed:** existing: `camp cleared`, `bandit falls`, `story.area.spawn`, `project.post` and `project complete` (B), `warrant.issue`, `npc.follow`, `faction.relation`. New: C10 `foe.add` (the Grey Captain), C3 `rank`, C4 `title.give`, C14 `npc.post` (Bettany takes command), C8 (to correct beliefs: `belief.forget` on a subject, a new small effect, N-belief).

---

### Chapter 4: The Nine Howls
**When:** early summer up to each town's summer festival (day 46 of summer). It ends the morning after.
**Where:**
- every town square, with a howl relay between them;
- Southmeadow's old stone on the Sunfields;
- Saltwell's spring pool;
- the Mirebridge crossroads `w_ridgemere_5_11` (188, 52).

**Opening:**
- Brenna's `story.tell`: "A Moot needs nine howls. Who remembers how?"
- Each Howl-keeper's `npc.line`: "We've a howl for the dead and one for a wedding, and I'd not swear which is which."

**Shared goal: "Nine howls answered."** Each town must find **its gathering howl** (a medium quest for each town):
- collect the three halves of the town's howl-memory from the old residents (conversations with the oldest wolves in the town; `cast` (C7) picks them by age);
- then a **chorus** at the town square at dusk with `max(2, active / 20)` wolves howling together.

Needs N1, a `howl` event with `quantity` = the number of wolves. Today's fallback is `count in region` ≥ N during the `hour` at dusk, plus a `performance` event.

When a town's chorus is sung, its Howl-keeper is named (`keepers_named += 1`) and the strand jumps.

**The Howl Relay** (the chapter's showpiece): a timed event on the eve of the festival. Each town's chorus at dusk "carries" to the next town along the roads. The relay runs Frosthollow → Coldwater → Larkfield → Southmeadow → Saltwell → Fenshore → Oakhaven → Ridgemere.
- Wolves at each square must answer within one game hour of the previous howl (an N1 chorus closing, or a `story.tell` region cue).
- A link the players miss isn't a failure: the relay "goes round by the long road" (Dagfinn's own saying) and costs a half-step.
- When the relay reaches Ridgemere, `festival.call` makes the summer festival in every town "the Howling" for one night, and the eight boards read: *"Eight howls answered. The ninth is silent."*

**Branch B4, Buy Back the Howls** (only if `greatly_howls > 0`). The debtors who pledged to Greatly can be won back:
- each by a personal deed for that debtor (a small quest chain cast at run time, C7 plus a variable giver C1);
- or out of the Debt Fund surplus.

Each one won back lowers `greatly_howls`. Every buy-back is visible: the debtor sings in their town's chorus.

**Side storylines:**
1. **The Stones Remember** (Southmeadow; a large personal story). Briar Cornby `cty_briar_cornby`, sly, a cellar hand, says the old standing stone on the Sunfields "remembers the Moot". It is half a trick. She sends the wolf on false errands, three of them, each a tease that teaches the dialect: "chaff", "a windfall", "my apple".
   - On the fourth night, under a clear sky, the wolf finds the real thing: names of nine communities scratched in the stone. Mirebridge's name has been chiselled out.
   - The place is a story region on open ground by Sunfields Cottage `fs_bees_south_home`, at `w_ridgemere_7_12`. If no stone exists there, Atlas may add a prop in DEV; otherwise the stone stays in the telling.
   - Lasting result: `world.mem.stone=read` in Southmeadow, and Briar turns honest towards *that* wolf (N4-lite: `npc.greet`).
2. **The Well's Roll** (Saltwell). Bramble Bogditch, 78 and devout, keeps "the well's memory": the Guest Roll of every caravan watered at the pool.
   - She is too old to howl the gathering howl. A wolf must sit with her through three Restdays (`calendar is Restday`, `together` events with her), learn it, and howl it *for* her at the Saltwell chorus.
   - If Bramble dies before then (possible without C11), Oriel Marshditch becomes Keeper, and the howl becomes Saltwell's howl for the dead, sung at the Moot.
3. **The Chorus That Sang Wrong** (comic, any town; small and repeatable). The town sang the dead-howl at a wedding (from lore `any-howl-1`). Smooth it over: gifts to the offended families (gift `mead`, `honey`), plus a `table game` with the elders.
4. **The Princess's Cough** (Ridgemere; a healer story). Ottilie's cough worsens in the summer rain. Bring `yarrow_poultice` and `herbs` to infirmarian Gorm Plankgate `rm_gorm_plankgate` at the Abbey Infirmary, then sit with her (`tended`). This sets up the Warden thread: Gorm asks oddly specific questions about marsh lights.
5. **The Scattered** (Mirebridge pledges; begins). Cartographer Imogen Nailridge `rm_imogen_nailridge` at Soundings `rm_soundings` has an old survey with Mirebridge on it. The wolf seeks out the Mirebridge-blooded in every town: Aspen in Larkfield, Tansy Eelbridge, Skye and Tamsin Eelbank in Fenshore, Oriel and Bramble in Saltwell, Thorn Marshditch at The Sheaf, Roan Pikewater, Fell Miremore in Coldwater. Each found adds a line to the **Book of Names** (`strand_mirebridge`).

**Lone wolf and crowd:**
- A lone wolf cannot sing a chorus of two. At five wolves or fewer, the floor is 2, and the town's **residents join** a lone howler: the howl system already turns residents toward a howl. With N1, residents near a howling wolf within the chorus window count as a second voice.
- At 200 wolves, every square at dusk is a real chorus. The surplus is "the Long Howl": the relay's carry, which adds weather (`weather.set clear` over the towns for a night) and a festival purse.

**Quest types:** oral history (talk chains), choruses (howl events), a timed relay, a lore hunt (the stone), sitting with the old (`together`), healing (`tended`), diaspora finding (talk across towns).

**Talk (`world.mem.ninth=howls`; per town `world.mem.howl=lost|found|sung`):**
- *southmeadow (lost):* "Our gathering howl? Blight take it, I've only ever known the harvest one."
- *southmeadow (sung):* "Good harvest of a night! Half the vale came out to howl."
- *saltwell:* "Water and shade to you. Baba Bramble taught it to a stranger wolf, and he howled it like he was born at the pool."
- *coldwater:* "Still water tonight. Listen. That's Frosthollow's howl coming down the valley, eh?"
- *mirebridge, Fenshore:* "Ninth howl's gone under, tadpole. Mirebridge got drowned, and the drowned don't howl. Or do they, no?"
- *oakhaven:* "Root and rain, the whole Sound sang. The old ones were crying, mm?"
- *frosthollow:* "First howl in the relay was ours. Deep proud."
- *plain:* "Eight towns howling at dusk and one silent. My grandmother said the ninth was the loudest."
- *helpful:* "Want the old howl? Ask the oldest wolf in town, then ask her again. She'll give you half the first time."

**Triggers needed:** new N1 (`howl` event), C7 (`cast`), C1 (variable giver), C11 (spare Bramble). Existing: `festival.call`, `weather.set`, `calendar is`, `together`, `tended`, `count in region`.

---

### Chapter 5: The Hills and the Letters
**When:** late summer into early autumn, about 30 game days.
**Where:**
- the Rainward Hills (quarry `w_ridgemere_4_1`, the Rainwood, the Oakhaven cut `w_ridgemere_4_4`);
- Fellhall `rm_fellhall`; Timberland Court `rm_mossgrave_court`; Grayrock Hall `rm_grayrock_hall`;
- the Grayrock road (patrol route `rm_grayrock_journey`).

**Opening:**
- Corwin Fell `rm_corwin_fell_` "says so in every tavern": the Council should grant Fell all the Rainward Hills before the Moot.
- Bounders appear at `3_3` and `5_4` (`story.area.spawn`, as "wolves in Fell green") and drive stakes at the Oakhaven cut: `item.place` of `timber` stakes as props.
- The Oakhaven board reads: *"The cut is ours by old right. Who will walk the bounds with us?"*

**Shared goal, part one: "Walk the Old Bounds."** Oakhaven's right rests on wolves walking the old bounds and marking them. That means scent-marking, which the wolf world already understands: 12 regions round the hills, each a `region entered` of a scent post.
- The bar is posts walked × wolves (goal `max(24, active × 2)`).
- Bounders' stakes pulled (`item picked up` timber at the cut) count double.

**Shared goal, part two: "Deliver the Grayrock Letters."** 28 to 40 residents carry letters for Lord Aldric Grayrock `rm_aldric_grayrock_`.
- A wolf collects a letter from a carrier (`npc.offer` "I'll take it to him", then `item.give` `letter`), finds Aldric on his road (`story.ping` from the Grayrock Wharf Office; his route `rm_grayrock_journey`), and gives it to him (`gift` letter to `rm_aldric_grayrock_`).
- Goal: `max(10, active)` letters. Gunnar's letter, carried by **Dagfinn himself**, is the last. Dagfinn asks a trusted wolf to take it: a personal quest for the top `p_voice` wolf, with C3 `rank` top 1. Without C3, the first wolf to finish three letters gets it.

**Branch B5a, Gudrun's Letters** (personal, tallied). Gudrun Fell `rm_gudrun_fell_` writes to Timberland "more than Isolde knows". She asks wolves to carry her letters to Signe Timberland `rm_signe_mossgrave`. Each wolf decides:
- **Carry it** (`b_gudrun = timber`): Fell and Timberland draw close in secret. In chapter 6 the quarrel becomes **Fell and Timberland against Greatly**.
- **Show it to Isolde** `rm_isolde_fell_` (`b_gudrun = fell`): Isolde shuts Gudrun in the Fellhall accounts room (`npc.hold` for 7 days; the hold is undone). Fell and Timberland are set at each other: the chapter 6 quarrel is **Fell against Timberland** over charcoal and the hills.
- **Burn it, and tell Gudrun** (`b_gudrun = ash`): Gudrun owes that wolf. A personal ending: she gives the wolf Fell's plans for the Moot.

**Branch B5b, the Hills.** It is set at the chapter's end from the bounds walked and the Gudrun tally:
- **Oakhaven's right upheld** (bounds over 1.5 × goal): the Council refuses Fell, and Corwin is humiliated. `faction.relation` house_fell–council_of_houses goes tense. Ulla Rusthand and Nan Ashhand speak for the cut. Oakhaven's strand jumps.
- **Shared** (bounds met, and `b_gudrun = timber`): the hills are split, with Timberland's charcoal right and Fell's vines. A peace that holds. Oakhaven is half-satisfied.
- **Fell's grant** (bounds missed): Fell takes the hills. Oakhaven's board grows bitter; Tamsin Icefell, the Keeper, says she will leave for Ridgemere after the Moot. Oakhaven's voice at the Moot is **against** whichever candidate Fell backs. The loss turns into leverage.

**Branch B5c, Lord Grayrock comes home** (when Gunnar's letter is delivered):
- Scene `grayrock_reads`: Aldric reads forty letters on the Wharf Road, then walks home (`npc.walk` to Grayrock Hall; N4 `npc.stay` keeps him there, lasting).
- **Lady Maren Grayrock** `rm_maren_grayrock_`, who "never leaves the Hall", walks out to meet him at the Wharf Road gate. It is the first time in years, and every Sump resident who "lost a brother on the wharf" has a line about it.
- **Ingrid Grayrock** `rm_ingrid_grayrock_` (16) gets her reply at last. That is her side story: a wolf has carried her weekly letters all chapter.
- Aldric's choice depends on the letters read (tallied from the petitions the wolves chose to bring, of three kinds):
  - **for the Crown** (`b_grayrock = crown`): Grayrock keeps the Wharf open for the Moot and guarantees the realm's ships;
  - **for the Wharf** (`b_grayrock = wharf`): neutral, and takes the Wharf trade's side;
  - if Gunnar's letter was never delivered (`b_grayrock = absent`): Grayrock's voice is missing at the Moot.

**Other side storylines:**
1. **Ingrid's Letters** (Grayrock Hall): carry the weekly letter, and read the one she never sent.
2. **Nan's Secret** (Rainward quarry): quarrymaster Nan Ashhand has been saving to buy the quarry folk's common right in the Rainward Hills. Help her raise the last 60 pennies (coins gift). If the Hills go to Fell, she spends it instead on a cart to Oakhaven to live.
3. **Corwin's Wager** (a tavern duel): Corwin Fell bets any wolf they can't out-drink him at the Fell Distillery. It is comic. Winning (`table game` won) shames him; the Gudrun tally weighs heavier for whoever wins.

**Lone wolf and crowd:**
- A lone wolf walks the bounds over two real days and delivers ten letters. Lord Grayrock thanks *that wolf* by name at the Wharf gate, in front of Maren.
- With 200 wolves the bounds are a living ring of scent round the hills by nightfall. The surplus is "the Long Mark", which makes Oakhaven's right permanent in the talk (`talk.remember keep: true`).

**Quest types:** scent-marking (a region chain), stake-pulling, letter collection and delivery to a moving resident, secret courier, a tavern contest, coin saving.

**Talk (`world.mem.ninth=hills`; `world.mem.hills=*`; `world.mem.grayrock=home|road`):**
- *oakhaven:* "Shade on you. Walk the bounds with us? Mark every ninth cedar and the old boundary stone, mm?"
- *oakhaven (upheld):* "Good bark, those wolves. The cut's ours yet. Root well."
- *plain (Grayrock home):* "Lady Grayrock stood at the Wharf gate in the rain. Didn't even put her hood up."
- *plain (a Sump wolf who lost a brother):* "He's home, then. Let him look at the wharf and count what it cost."
- *larkfield:* "Fell wants the hills. Fell wants everything with a price on it, lambkin."
- *helpful:* "Lord Grayrock? Ask at the Wharf Office which road he took. He's always on one."

**Triggers needed:** existing: `region entered`, `item picked up` (W), `gift` letter to a resident, `story.ping`, `npc.walk`, `npc.hold`, `faction.relation`, scenes. New: N4 (`npc.stay`, lasting), C3, C8 with `keep`, C12 (a deed event filter so packs can tell which letters).

---

### Chapter 6: The Lights in the Fen
**When:** autumn, fog season, about 30 game days. **The Houses fall out** in this chapter: "before winter", as the realm has said all along.
**Where:**
- the Sallow Fens; Fenshore `w_ridgemere_3_13`; Withy Creek `fs_marsh_eels_eelhouse` at `4_13`; Rivermouth `3_11`;
- the Mirebridge crossroads `5_11` (188, 52);
- the Chapel of the Tide `rm_the_chapel_of_the_tide`;
- the city, where the House quarrel plays out.

**Opening:**
- `weather.set fog` over Fenshore by night (72 hours at a time, re-set).
- Marsh lights appear: `story.ping` labelled "a light in the reeds" at night, moving each hour.
- Tamsin Eelbank `tn_mb_tamsin_eelbank` (70, never sits with her back to a door): "The drowned are walking. They want something said."

**Shared goal: "Say the Names."** The Book of Names, the Mirebridge dead and scattered, begun by the Scattered in chapter 4, must be finished and read aloud at the Mirebridge crossroads. Lines come from:
- the diaspora conversations;
- the Records Office rolls `r2_the_records_office_top` (archive work, the `archive work` event);
- the Chapel of the Tide, where priest Solveig Tarwell `rm_solveig_tarwell` keeps the sea-drowned lists;
- **following the lights** at night along the old safe paths (lore `mb-fens-1`). The paths are a region chain in the fog, and stepping off them costs a wound (`character.injury`, W, severity 1, via `ifRefused: skip`).

The bar is names gathered, with goal `max(30, active × 2)`.

**The stranger, Wisp** (needs N2, a speaking stranger; today she is a mute visitor seen in scenes through a stand-in, below):
- Wisp is a female wolf about 24, red-coated, fen-born. She was a pup in the drowning and was pulled from the water by a Fenshore eel-trapper. She is **Quickened**: fire, a cold light she cannot always put out.
- She makes the marsh lights to keep wolves off the deadly paths. "The drowned" is her joke and her grief.
- She appears at Withy Creek after the first 10 names (`visitor.add`, `like: tn_mb_skye_eelbank`, re-added each night).
- Today's fallback for her speech: scenes cast with Skye Eelbank `tn_mb_skye_eelbank`, who speaks Wisp's lines in Skye's own panel as "what the light-girl told me".

**The Warden's Envoy:** Warden Hesper Vale, a stranger in a black cloak with silver trim. She is drawn by Wisp's lights and by any Quickened player's Trance in the fen (`wardenAttention`). Her local correspondent is infirmarian **Gorm Plankgate** `rm_gorm_plankgate` (cold, guarded). This needs a `warden_order` faction created by the DM before the chapter, with Gorm as a ranked member, so that the `told the wardens` and `vouched to the wardens` events work.

**Branch B6, Wisp's fate** (tallied by personal choices; every wolf who meets Wisp chooses):
- **Shelter her** (`b_wisp = sheltered`). She stays in the fen.
  - At the Moot she carries the Ninth Howl, lit with her light.
  - The Wardens are angered: `faction.relation` warden_order–council goes tense. Quickened players gain warden attention, which opens a later Warden storyline. Her lights keep the fen paths safe for good.
- **Hand her to the Wardens** (`told the wardens` on her; `b_wisp = wardens`).
  - The lights go out, and the fen paths grow deadly again (a lasting `character.injury` risk in the fog regions, from a pack).
  - The Warden order is friendly; Hesper vouches for the realm's Quickened players.
  - Tamsin Eelbank carries the Ninth Howl at the Moot, old and shaking.
- **Persuade her to go to Upper Accord of her own will** (`b_wisp = accord`). This needs the Book of Names finished first, so she can leave knowing her dead are said.
  - She leaves after the Moot. Her lights are relit once a year at the Moot by her own wish (a `story.ping` each Midwinter).
  - The Wardens are friendly, and Wisp writes to the wolf who persuaded her.

**The House quarrel** (the "falling out", set by the branches):

| Earlier branches | The quarrel | What it looks like |
|---|---|---|
| `b_gudrun = fell` | Fell against Timberland | Fire at the Timberland Ironworks (`event.add works_fire`, ridgemere market); charcoal wagons held on the Cinder Road; `times.nudge` down on iron |
| `b_gudrun = timber` and `b_ledger = shown` | Fell and Timberland against Greatly | A run on the Greatly Bank (`times.nudge`); Greatly masons walk off; stone short in the realm |
| `b_ledger = kept` and `b_lists = printed` | Greatly against Grayrock | Grayrock closes the Wharf to Greatly stone; `ship.storm` news; Wharf workers idle |
| otherwise | All against the Crown | Council petitions to unseat Dagfinn before the Moot; the Palace guard doubled |

The shared side goal **"Keep the peace"** runs beside it:
- carry **truce tokens** (bread and salt, Saltwell's custom, given as `gift` `bread` plus `salt` to House stewards);
- guard the Houses' wagons (escort);
- fight the bounders the quarrel lets loose (`bandits.call`).

Peace kept (goal met) means no House is broken: the quarrel ends in a `house_wedding` event staged by Celestine Greatly's salon. Peace failed means one House's head is shamed (`faction.relation` hostile, lasting) and that House's voice at the Moot is *against* the others. It is never a block.

**Other side storylines:**
1. **The Potter's Post** (Fenshore). Ludo Beechshaw `tn_oh_ludo_beechshaw` took the dead potter Iris Greywater's post. Did he help her die? (He didn't: she died of the fen-cough.) A scent-and-records chain clears him or shames him. The truth is innocent, and the wolf's reward is a friend in Fenshore.
2. **Sloe's Cargo** (Rivermouth). Skipper Sloe Mistwick, sly, is smuggling House goods out of the quarrel. Report her (warrant), take her cut, or have her ferry the Book of Names' copy to Ridgemere safely. That last choice adds names.
3. **The Quickened Wolf's Question** (only for Quickened or Gifted players; `wolf is` with a Gift fact). Warden Hesper asks a Quickened player to vouch for Wisp or to tell on her, and either way the player's warden attention moves. This is where the campaign speaks to the Gift system directly.

**Lone wolf and crowd:**
- A lone wolf following the lights through the fog alone is the chapter's purest moment: one wolf, one light, the path with no note.
- A crowd makes a lantern-line of wolves across the fen: `count in region` along the path makes the lights brighter, a ping every 10 minutes instead of every hour. The surplus of names is "the Long Remembrance", read by Solveig Tarwell at the Chapel of the Tide on the next Restday (`npc.walk`, scene).

**Quest types:** night tracking in fog, records work, diaspora talk, a stranger meeting, Warden choices, truce-carrying, wagon guard, a smuggler intrigue, an innocence chain.

**Talk (`world.mem.ninth=lights`; `world.mem.wisp=*`; `world.mem.quarrel=*`):**
- *mirebridge, Fenshore:* "Dry feet. Mizzle's up. If you see a light, follow it, but only the cold ones, no?"
- *mirebridge (helpful):* "The path with no note, tadpole: start at Withy Creek, keep the reeds on your left, and never step where it shines warm."
- *mirebridge (sheltered):* "Fen-sure, she's one of ours. Mud and eels, let the black-cloaks try."
- *plain (quarrel):* "The Timberland works burned in the night. Every smith in the Cinders swears it wasn't them."
- *saltwell:* "In the old way, my sister, you bring bread and salt to a House at war. Even a House needs to eat."
- *larkfield:* "A Warden on the Fen Road, in black and silver. Price of a room at the Long River just doubled."
- *coldwater:* "Fog to the south and Houses at each other in the city. Good season to mend nets, eh?"

**Triggers needed:** N2 (speaking strangers), the `warden_order` faction (DM prep), existing `told the wardens` and `vouched to the wardens`, `archive work`, `weather.set`, `story.ping`, `character.injury` (W), `event.add`, `times.nudge`. New: C12 (`story.notify` between the quarrel pack and the chapter), N8 (`reputation.adjust`, so peace-keepers without a Chapter gain town regard).

---

### Chapter 7: The Bell under the Ice
**When:** the start of winter (`season begins` winter, with `weather is snow` near the Upper Mere; the DM can force it with a `dm signal` if the season lags).
**Where:**
- the Upper Mere, the shore at the Upper Mere Boat Shed `fs_fishing_uppermere_shed` at `9_6`;
- Coldwater; the Glasswater Inn `cn_coldwater_inn`;
- in Ridgemere, Oskar Gullworth at the City Office.

**Opening:**
- Kell Greyton, who "saw something big move under the ice", comes to the Glasswater Inn: "It's moving again. I heard it ring."
- `weather.set snow` at Coldwater.
- The board: *"THE MERE HAS FROZEN. Wolves with ropes and strong backs, to the Upper Mere."*

**Shared goal: "Raise the Bell."** This is the campaign's great group feat, in three stages:
1. **Find it.** Wolves lie on the ice and listen (`listening` practice). Each listen at a different ice region adds a bearing. Six bearings place it. Hearing is a wolf attribute, so a wolf with good ears finds it faster.
2. **Cut and frame.** The bell frame is a project: `project.post` `cover`, titled "the Upper Mere bell frame", or the proposed `belfry` kind (N10). Wolves give `timber`, `tug_rope` and `rope`. Willa Coldwick's cart brings the frame timber from Coldwater, and the cart needs an escort.
3. **The Haul.** At dusk, `count in region` at the haul region by the Boat Shed must reach `max(3, active / 8)` wolves at once, each carrying `tug_rope` (`carries`). The haul runs three times over three dusks; each success raises the bell a stage.
   - With fewer wolves than needed, Coldwater's residents join the ropes: Kell, Willa, Loam Pinefell, Fallow Lakewick, Harrow Reedford and others walk to the haul region by `npc.walk`, each counting as one hauler (N: `count in region` that counts residents in a list, or a resident-hauler signal).
   - So a lone wolf with the town behind them can do it.

**Rung changes:**
- First bearing: the Coldwater board updates.
- Frame built: a lasting structure on the mere's shore.
- The bell raised: scene `the_bell_rises`. The bell, green with weed, is set on the frame and rung **once**. Every town hears it: `story.tell` to the world, "From the north-east, over the snow, a bell no living wolf has heard."

**Branch B7a, the Bell** (it depends on the haul):
- **Rung** (three hauls): `b_bell = rung`. The bell is carried to Ridgemere for the Moot by an escort of wolves along the High and East Roads. A great escort quest (wagon), with `bandits.call` en route if `b_lists = sold`.
- **Cracked** (a haul failed twice): it rings flat. It can still be carried, but the Ninth Howl at the Moot is a weaker voice (half weight).
- **Lost** (the deadline passed with no haul): the bell stays under the ice. Mirebridge howls without it, and the Moot sits anyway. A sequel hook: "the bell still moves".

**Branch B7b, the Chancellor's Tears** (personal, big). Oskar Gullworth `rm_oskar_gullworth`, cold and guarded, Sump-born and climbing, never goes out without an oilcloth hood.
- Forty-six residents saw "the Chancellor weep in the Rain Hall". The wolf gathers enough of them (C7 `cast` picks witnesses who are alive; Gerrit Mudhand, Desmond Hollis and Hilde Dripby are among them) to confront him.
- Hilde Dripby `rm_hilde_dripby`, a thief, will **blackmail** him unless a wolf gets there first.
- Oskar can be led to confess: he locked the Rain Hall at the Council's order, and Gunnar's charter for the Moot was kept from the realm.
  - **Confessed publicly** at the Antechamber (`b_oskar = confessed`): the Council's claim to name the ruler collapses; `faction.relation` council_of_houses with every House goes tense. Oskar resigns and becomes Ridgemere's second voice at the Moot, a Sump wolf standing for the Sump. If Atlas links `rm_the_rain_hall` in DEV, Oskar unlocks it and the Moot sits there.
  - **Kept secret** (the wolf took Oskar's key and his trust; `b_oskar = silent`): Oskar owes the wolf, and at the Moot he swings the Council's clerks to the wolf's ballot.
  - **Hilde got there first** (deadline): Oskar pays her and stays Chancellor, and the Council's claim stands as a threat over the Moot.

**Other side storylines:**
1. **Kell's Winter** (Coldwater). Kell distrusts anything from the city. Win him over (fish with him, `hunted` duck at the mere, a gift of `fur_oil` against the cold), and he leads the bearings.
2. **The Frosthollow Count** (Frosthollow). Frosthollow sends iron for the bell's new yoke. The mine must run a winter shift, and "nobody goes down alone, and nobody comes up without counting the others". It is a party quest: a party of `max(2, …)` down the mine, counted at the top (`party size` and region).
3. **The Pups on the Ice** (Coldwater; small and repeatable). Gorse and Tansy Reedmere (`tn_cw_gorse_reedmere` and `tn_cw_tansy_reedmere`, 14), who also "saw something under the ice", keep sneaking out onto thin ice. Bring them back (`near resident`, `npc.follow` home).

**Lone wolf and crowd:**
- The haul is designed for both: residents fill the ropes for the few, and wolves fill them for the many.
- At 200 wolves, the haul is a sight. The surplus raises the bell *and* the Mirebridge ferry-raft beneath it (an item hunt: a Mirebridge keepsake `item.place` for each surplus step), and those keepsakes become gifts for the diaspora.

**Quest types:** listening and bearings, project building, escort, a timed group haul, a confrontation chain, a blackmail race, a party descent, rescues.

**Talk (`world.mem.ninth=ice`; `world.mem.bell=*`):**
- *coldwater:* "Still water. Hear that? Under your paws. Bright One's tide, it's ringing."
- *coldwater (rung):* "Net and line, they raised it! Green as a pike and loud as a gale, eh?"
- *frosthollow:* "Our iron's in the bell's yoke. Cold iron for a cold bell. Lamp and pick."
- *plain:* "They say the Chancellor stood in the Antechamber and wept again. In front of everyone this time."
- *larkfield:* "Bell came down the High Road on a cart with forty wolves round it. Couldn't buy a seat to see it."
- *helpful:* "If you're going to the mere, take a tug rope and oil your coat. And lie flat on the ice; it carries the sound."

**Triggers needed:** existing: `count in region`, `carries`, `project gift`, `party size`, `season begins` (W), `weather is`. New: N10 (`belfry` project kind), a resident-hauler count (an extension of `count in region` with `residents: true`), C7, C14 or N4 (Oskar resigns: `npc.post none`), an optional Atlas link for `rm_the_rain_hall`.

---

### Chapter 8: The Moot of the Long Night
**When:** the winter festival, day 46 of winter (Midwinter, the Long Night or the Hearth Feast, by each town's own name), plus the 3 days before it.
**Where:**
- Market Square, Ridgemere, in the rain and under open sky, round its notice board;
- or the Rain Hall if Atlas has linked it and Oskar confessed;
- the eight towns, where the voices set out;
- the High, East, South, Coast, Fen and Waste roads.

**Before the Moot, "the Walk of Voices"** (3 days). Each Howl-keeper walks to Ridgemere (`npc.walk` legs between waystations, cue by cue, from Frosthollow, Coldwater, Larkfield, Southmeadow, Saltwell, Fenshore and Oakhaven). Each needs an **escort**, a medium quest in each town.
- An unescorted Keeper still arrives, but late, and their howl comes last and weaker. No Keeper is killed: they are protected and spared.
- Bramble Bogditch at 78 rides Aster Cornley's bee-wagon if a wolf arranges it.
- The bell comes on its own cart if raised.

**Ballots.** Every participant may set `p_ballot` (by `npc.offer` at their pledged town's Keeper). The four candidates:
- **Ottilie Harrowgate**, the heir;
- **Dagfinn Thornham**, the Lamp-King (only if `b_oath = open`);
- **a Regency of the Moot**: no ruler; the Moot sits every Midwinter, and the lamp passes to a steward;
- **House Greatly's choice**: Ambrose's heir as ruler. Ambrose would put Rosalind, or Julian Greatly `rm_julian_greatly` (who wants to go to sea) if Rosalind was turned out.

**The Moot scene** (`moot`, public, at most 16 cast; the voices are cast from the Keepers and the residents chosen by the branches). Each voice howls in turn: a `line` beat with that Keeper's face, giving the town's verdict. Each voice's weight and verdict come from:
1. its strand (the ratio of strand to goal);
2. its thread's branch outcome (for example, Oakhaven *against* Fell's candidate if `b_hills = fell`);
3. the ballot tally of the wolves pledged to it;
4. bought voices (`greatly_howls`) that were never won back, which howl for Greatly.

The tally runs in a trigger (`each over participants` with `if compare`). The candidate with the most weighted voices wins. A tie goes to the side with more wolves on the Moot Roll.

**The endings:**

| Outcome | Conditions (dominant) | The world after |
|---|---|---|
| **Queen Ottilie, with the Lamp beside her** | Ottilie wins; Grayrock for the Crown; the Princess's Cough tended | Ottilie takes "reigning at the Great Palace" (C14 `npc.post`). Dagfinn becomes **Lord Lamplighter**: he keeps the Palace lamp and walks the Sump on Restdays. Oda reconciles with her daughter if a wolf told her of the spy (chapter 1); otherwise she retires to Coldwater, where she grew up (N4 `npc.stay`). Figures: ruler = Ottilie, hero = the top `p_voice` wolf. |
| **The Lamp-King** | Dagfinn wins (`b_oath = open`); Mirebridge's voice rung loud; lists printed | The fen-born patrolman keeps the Palace. Mirebridge's name is cut back into the Sunfields stone. Every Sump wolf's talk lifts (opinion `sump`). The Houses grumble; Greatly's relation with the Crown goes tense. Dagfinn's first act: a Mirebridge waystation at the crossroads (`project.post waystation` at `5_11`), lasting. |
| **The Regency of the Moot** | Regency wins, or a near tie with `b_oskar = confessed` | There is no ruler. The Moot sits each Midwinter, and the next campaign is the next Moot. Dagfinn hands the lamp to Herald Brenna as steward. The Council of Houses is cut to an advisory voice: faction relations with the Crown neutral and lasting. |
| **The Bought Crown** (the "failure", which is still a story) | Greatly's voices win | Rosalind (or Julian) is crowned unwillingly. Ambrose rules in all but name; debts become law. But the Moot *sat*. The final scene shows the new ruler alone at the lamp, telling the wolf who did the most, by name: "Next Midwinter, howl louder." That is the sequel hook: the Moot is now law, and it can unmake this Crown. |

**Recognition at the Moot** (every wolf who contributed is recognised):
- **The Moot Roll:** every wolf with `p_voice > 0` gets a `journal.note` with their pledged town's line, and the deed "stood at the Moot of the Long Night".
- **Tiers** (C3 `rank` per town and realm-wide; fallback: fixed `compare` tiers):
  - **Howl-friend of <town>:** the top `max(1, pledged / 10)` per town; title C4, known in that town;
  - **Voice of the Realm:** the top `max(1, active / 25)` realm-wide; title C4, known in the realm;
  - **The Lantern:** the single top wolf. It becomes `world.figure_hero` (C9 for a wolf, N3), so every existing hero line names them across the realm.
- **Thanks by name:** each Keeper sends an `npc.greet` (C5) to the top wolf of their town: "#thanks#, #address#. My town howled because you walked it."
- **Coin:** `character.grant` from each town's purse (check the purses hold the coin) to that town's Howl-friends.
- **Quickening:** each personal side story the wolf finished is a closed storyline. A wolf who did three has passed `closedStories: 2`.

**Talk (`world.mem.ninth=moot`, then `world.mem.crown=ottilie|lamp|regency|greatly`, kept 90 days, renewable):**
- *plain (Ottilie):* "Bright One keep the Queen. And the Lamplighter, bless his flat feet."
- *plain (Lamp-King):* "A Sump wolf at the lamp, by the realm's own howl. Blood and bread, I howled for him."
- *mirebridge, Fenshore (any outcome with the bell rung):* "Ninth howl was ours. Dry feet to every wolf who carried it, no?"
- *saltwell:* "Peace on you. The well remembers who drew from it, and now the realm does too."
- *oakhaven (regency):* "No king and no queen, mm? Old Oak's done well enough without one."
- *frosthollow (bought crown):* "Bought, then. Black seam. Next Midwinter, we howl deeper."
- *southmeadow:* "Ripe days, my apple. Ripe days."
- *coldwater:* "The bell rang the Moot in. Calm water to the lot of you."
- *larkfield:* "Every name on the Moot Roll got printed. Sold out by noon."

**Triggers needed:** C14 or N-post (crowning), N4, C3, C4, C5, C9 with N3, N6 (a tally over 500), scene casts from variables (C1 in scenes, N11).

---

## 5. The cast

### 5.1 Heroes

| Name (id) | Sex, voice | Motive | Arc | Fate by branch |
|---|---|---|---|---|
| Dagfinn Thornham `rmx_dagfinn_thornham` | m, 31; fen speech (mb131), calm, quick to feel a slight; walks once round the block before answering | Keep the lamp he was handed; deliver his own Grayrock letter | Bewildered patrolman → steward or candidate → the realm's lamp | Lamp-King / Lord Lamplighter / steward handing the lamp to Brenna / the man the Bought Crown keeps at the door |
| Ottilie Harrowgate `rm_ottilie_harrowgate` | f, 39; plain, gentle, coughs and pretends not to | See the petitions answered; be worthy of her father's law | Frail heir → healed → queen or petition-keeper | Queen / the Moot's first Speaker under a Regency / the Lamp-King's councillor |
| Herald Brenna Abbot `rmx_brenna_abbot` | f, 52; plain, warm, content; owes a factor | Proclaim the law truly | The campaign's voice (every `story.tell`) | Ridgemere's Keeper; steward under a Regency |
| Sergeant Bettany Nailley `rm_bettany_nailley` | f, 26; plain, ambitious, gentle; hates Grayrock | An honest Watch | Sergeant → Captain if the lists are printed | Captain / Corra's right paw |
| Rosalind Greatly `rm_rosalind_greatly` | f, 22; plain, bookish, wry | The ledger that doesn't add up | Whistleblower → exile at the abbey, or a silenced heir | Greatly's conscience / the unwilling Bought Queen |
| Gerrit Mudhand `rm_gerrit_mudhand` | m, 55; plain, chatty, warm | Mourns the King; saw the tears | The Council's honest chancellor; receives the ledger | Speaker of the Council under any outcome |
| The Keepers: Fallow Lakewick, Aspen Marshholm, Iris Meadowfield, Nell Plummeadow, Bramble Bogditch, Tamsin Icefell, Thorn Ashwood | own dialects | their town's say | named in chapter 4, walk in chapter 8 | each howls at the Moot; Tamsin leaves Oakhaven if the Hills go to Fell |
| Ulla Rusthand `cty_ulla_rusthand`, Nan Ashhand `cty_nan_ashhand`, Kell Greyton `tn_cw_kell_greyton` | oakhaven / plain / coldwater | name cleared / a common right / be believed | small heroes of their strands | they speak at the Walk of Voices |

### 5.2 Villains and rivals

| Name (id) | Sex, voice | Motive | Arc | Fate by branch |
|---|---|---|---|---|
| Ambrose Greatly `rm_ambrose_greatly` | m, 63; courteous, never raises his voice, never forgets | "His heir to inherit a city" | Calls in debts → buys voices → the Bought Crown or shame | Shamed (ledger shown) / owed a favour (kept) / power behind the throne (Bought Crown). Spared from old age for the campaign (C11). |
| Queen Oda Harrowgate `rm_oda_harrowgate` | f, 60; Coldwater-raised (cw210), sly, counts coin | Won't bow to a patrolman; wants the purse | Spy in the kitchen → bargains with Greatly → reconciles or retires | Reconciled with Ottilie / retires to Coldwater / Greatly's ally in the Bought Crown |
| Corwin Fell `rm_corwin_fell_` | m, 31; loud, proud | The Rainward Hills | Bounders → tavern wager → humiliated or granted | Humiliated / split hills / granted, and Oakhaven's lasting enemy |
| The Grey Captain (foe, `like: rmx_desmond_hollis`; with C10 `npc:` it is Desmond Hollis himself, downed not killed) | m; a dismissed Palace guard, proud and shrewd | Paid to keep the roads shut | Glasswater camp → downed or fled | Held and talking (names the payer) / returns at `6_12` with a smaller band |
| Lorn Hedgeton `cty_lorn_hedgeton` | m, 58; Coldwater, cold, ambitious | Greatly coin for cart times | Informer → exposed or turned | Warrant / grudging redemption |
| Fenwick Plankworth `cty_fenwick_plankworth` | m, 54; Frosthollow mine captain, sly, restless | Save the mine from the note, any way he can | The seam note | Honest second voice / arrested, and his post to the vacancy rule |
| Hilde Dripby `rm_hilde_dripby` | f, 61; short, sly thief | Blackmail the Chancellor | The race for Oskar | Paid off / caught (warrant) |

### 5.3 Turnable and key NPCs

| Name (id) | Role in the weave |
|---|---|
| Captain Corra Slagmore `rm_corra_slagmore` | compromised by her note; freed → redeemed; printed lists → resigns in honour or is dismissed |
| Oskar Gullworth `rm_oskar_gullworth` | the weeping Chancellor; confesses, keeps the secret, or is blackmailed |
| Gudrun Fell `rm_gudrun_fell_` | the hinge between Fell and Timberland; her letters tallied |
| Lord Aldric `rm_aldric_grayrock_`, Lady Maren `rm_maren_grayrock_`, Ingrid `rm_ingrid_grayrock_` | the Grayrock letters; homecoming |
| Isolde Fell `rm_isolde_fell_`, Signe Timberland `rm_signe_mossgrave`, Celestine Greatly `rm_celestine_greatly` | the House heads (protected); their relations are the quarrel's dials; Celestine's salon hosts the peace wedding |
| Karin Stanmore `rm_karin_stanmore`, Abbot Lionel Ropestack `rm_lionel_ropestack`, Desmond Dripcot `rm_desmond_dripcot` | the three list pages |
| Mab Coalridge `rm_mab_coalridge` | Oda's spy in the kitchen, paid in Greatly coin |
| Edric Kelpham `rm_edric_kelpham`, Eleanor Saltcot `rm_eleanor_saltcot`, Jorunn Ropewick `rm_jorunn_ropewick` | the money trail |
| Gorm Plankgate `rm_gorm_plankgate` | infirmarian; the Wardens' secret correspondent (the `warden_order` member) |
| Solveig Tarwell `rm_solveig_tarwell`, Imogen Nailridge `rm_imogen_nailridge`, Tamsin Eelbank `tn_mb_tamsin_eelbank`, Skye Eelbank `tn_mb_skye_eelbank` | the Mirebridge strand's keepers of names, maps and paths |
| Briar Cornby `cty_briar_cornby`, Dun Harvestley `tn_sm_dun_harvestley`, Willa Coldwick `tn_cw_willa_coldwick`, Ludo Beechshaw `tn_oh_ludo_beechshaw`, Sloe Mistwick `cty_sloe_mistwick` | the side stories' guides and suspects |

### 5.4 Strangers and foes (they come and go)
- **Greatly factors** (chapter 2): `visitor.add` daily on the town squares (`like: rm_eleanor_saltcot`); they leave as the debts are freed.
- **Fell's bounders** (chapter 5): anonymous bandit spawns at `3_3` and `5_4`, described as wolves in green.
- **The Grey Captain** (chapter 3): a foe (C10).
- **Wisp** (chapter 6): female, red-coated, about 24, fen-born and Quickened (fire: a cold light). Her voice is wry and damp, the old Mirebridge speech, with long silences. A speaking stranger (N2). She leaves for the fen, for the Wardens, or for Upper Accord.
- **Warden Hesper Vale** (chapter 6): female, the Concord's black cloak with silver trim, formal and exact. Present only in the fen, and at the Moot if `b_wisp` is not `sheltered`.
- **The Mirebridge dead:** never shown, only spoken of. Marsh lights (pings) stand in for them.

**Protect and spare before chapter 1** (C11 plus `npc.protect`, undone at the campaign's end):
- the key cast: Dagfinn, Ottilie, Oda, Brenna, Gerrit, Bettany, Rosalind, Oskar and Ambrose;
- all eight Keepers;
- Kell, Ulla, Nan and Gorm.

Without C11, old age can take Bramble Bogditch (78) or Halvard (66). Every branch has a `resident is alive` fallback: Bramble falls to Oriel Marshditch, and Ambrose to Celestine.

---

## 6. Talk: givers, gossip, and helpful knowledge

### 6.1 Where the words live
1. **Quest givers** speak through `quests[].offer` (`say`, `thanks`) and through `npc.offer` chains. Always write the frame in symbols (`#greet#`, `#thanks#`, `#address#`, `#oath#`) so the giver's own dialect fills it. Write a dialect's set phrase out ("Dry feet", "Peace on you") only when the giver grew up in that dialect.
   Example, Nell Plummeadow (mirebridge, Fenshore), "Say the Names": *say:* "#greet#, #address#. Tamsin's sat up three nights watching the reeds. If the drowned want their names said, someone ought to go and listen, no?" *thanks:* "#thanks#. That's three more names I can say over the water."
2. **Giver mid-quest lines** need C6 `offer.during` (today: nothing shows). For example, Kell Greyton: *during:* "Lie flat. The ice carries it, eh? You'll hear it before you see it."
3. **Campaign-stage gossip** goes in a new `Data/Talk/scenes/common/story_ninth_howl.talk` and `Data/Talk/rules/story_ninth_howl.talk`. Every rule and scene is gated on `world.mem.ninth=<stage>`, plus `here.region` or `self.dialect`. The keys are written by `talk.remember` (C8). The gating side already works today; only the writer is missing.
4. **Window lines** in the resident tree's `news` and `gossip` nodes: `say[world.mem.ninth=roads here.region=coldwater priority=2] …`.
5. **Names in talk:** use `#world.figure_ruler#`, `#world.figure_villain#`, `#world.figure_captain#` and `#world.figure_hero#` (C9 keeps them current: Dagfinn or the new ruler; Ambrose; Corra or Bettany; the Lantern). Use only allow-listed places. Add "Upper Mere", "Rainward Hills", "Rain Hall", "Sunfields" and "Withy Creek" to `tools/dialect_allow.json` places if they are missing. Withy Creek is already allowed.
6. **Deeds as gossip:** every reward's `deed.award` is a lower-case verb phrase with no full stop:
   - "carried the Summons to Saltwell";
   - "broke the Glasswater camp";
   - "walked the old bounds of the cut";
   - "said the drowned names at Mirebridge";
   - "hauled the bell from under the Upper Mere ice";
   - "stood at the Moot of the Long Night".

   Residents then say "You carried the Summons to Saltwell, didn't you?" without any new code.

### 6.2 New dialogue paths for the cast (a mantle-free method)
`mantle.assign` overwrites a resident's own mantle and doesn't restore it (C15). So:
- Each key cast member gets **story grafts** through C15, or **tree lines keyed to `self.id`** (`say[self.id=rmx_dagfinn_thornham world.mem.ninth=summons priority=3] …`), which works today with no binding.
- Each main giver gets a short **story conversation** of 2 or 3 steps: `npc.offer` chains on `dialogue choice`, within the 64-offer budget. For example:
  - **Dagfinn, "Why you?"** → "I held the lamp. Someone had to. Walk it round before you judge it." → "And the letter?" → "Lord Grayrock's. I've carried it two years. Seems I'll carry it till I'm a king or a ghost."
  - **Ambrose, "What do you want?"** → "A city that pays what it owes. You'd be surprised how few wolves want that." → "And the howls?" → "A voice is only a debt with a tune to it."
  - **Bramble Bogditch, "Teach me the howl"** (saltwell) → "#greet#, my child. Sit. The well remembers who drew from it, and the howl remembers who sat still." (In three steps over three Restdays.)
- **Spread givers apart.** No resident gives more than two of the campaign's offers at once (the 9-slot hub). Innkeepers give only helpful knowledge, never chain quests.

---

## 7. Trigger features required

| Need | Exists today | Gap addition | New here |
|---|---|---|---|
| Shared bars, strands, chapters | world variables, story quests, `story.start carry`, trackers | C13 `variable changed` (instead of polling) | — |
| Goals that scale | — | C2 `audience`, `multiply` | **N5:** a quest `goal` and phase thresholds read from an operand, worked out when the chapter opens |
| Per-wolf contribution, ranks, titles | player variables, `each`, `deed.award` | C3 `rank`, C4 `title.give` | — |
| Thanks by name | quest `thanks`, deeds into beliefs | C5 `npc.greet` | — |
| Gated gossip by stage and branch | talk gates on `world.mem.*` | C8 `talk.remember` (with `keep`), `belief.plant` | **N-belief:** `belief.forget {subject, kind, town}`, so clearing Ulla's name lowers the false gossip |
| Figures | `live.figures` via the DM | C9 `figure.set` | **N3:** `figure.set` with `player` (a wolf as hero) |
| Run-time casting (debtors, witnesses, elders) | lists plus `each` (effects only) | C7 `cast`, C1 a variable `giver` | — |
| Giver speaks mid-quest; prerequisites | — | C6 `offer.during`, `requires` | — |
| Fightable foes | bandits (anonymous) | C10 `foe.add` | — |
| Speaking strangers (Wisp, Hesper, factors) | mute `visitor.add` ≤ 24 h | gap #15 | **N2:** `visitor.voice {visitor, dialect, mantle?}`, letting a stranger be scene cast, `npc.offer` and quest giver, and stay up to 7 days |
| Howls as story events | `performance`; `count in region` | — | **N1:** event `howl` (`actor` the first howler, `quantity` the wolves, `cell`, `detail` the town), written when a chorus closes in `Game::tendHowls` (`Core/RatwGameHowl.cpp`); plus `residents near` counted with a flag |
| Group haul with residents | `count in region` (wolves) | — | **N9:** `count in region` with `residents: [list]`, or `npc.walk` arrivals counted as a signal |
| Key cast survive | `npc.protect` | C11 `npc.spare` | — |
| Lasting fates (posts, homes) | `npc.hold` (undone) | C14 `npc.post`, `npc.stance` | **N4:** `npc.stay {npc, place, days or lasting}` overrides the schedule (Aldric home, Rosalind at the abbey, Oda to Coldwater) |
| New moon deadline | `clock` `afterDays` | C13 `month begins` | — |
| Packs feeding the chapter | world variables (polled) | C12 `story.notify`, `field starts` | — |
| Town regard without a Chapter | `reputation` condition only | — | **N8:** `reputation.adjust {player, town, points}` |
| Large tallies | `each` ≤ 500 | — | **N6:** operand `{"tally": {"var": "p_ballot", "value": "ottilie"}}` over the cohort |
| Text with numbers and names | — | C1 text fill | — |
| Crier announcements | deeds called at festivals | gap #2 | **N7:** `crier.call {town, text, from?}`: the town's herald or innkeeper calls it aloud in the square at the next hour |
| Bell frame | projects `watchpost`, `cover`, `waystation` | — | **N10:** a `belfry` project kind in `Data/Town/projects.json` with its structure (fallback: `cover` titled "the bell frame") |
| Scenes cast from branch variables | static `cast` | — | **N11:** a scene cast `npc: {"var": …}` (C1 applied to scenes), so the Moot's 9 voices are whoever the branches chose |
| Warden faction | the `told the wardens` events exist | — | DM prep: create `warden_order` with Gorm Plankgate as a ranked member; optional `the_crown` faction |

**Build order (it matches the chapter order, so chapter 1 ships first):**
1. C11, C2 with N5, C9, C5, C8. With these, chapter 1 plays fully, and so do chapter 2's deadline and gossip.
2. C13, C14, N4, C7 with C1, N2 for chapters 2 and 3.
3. C10, C3, C4 for chapter 3.
4. N1 for chapter 4. Chapter 4 can run on the `count in region` fallback first.
5. N2 hardened, plus the faction prep, for chapter 6.
6. N9, N10 for chapter 7.
7. N6, N11 for chapter 8.

Everything has a fallback (noted in each chapter), so the campaign can open on today's engine. It plays thinner without the additions, but it is never broken.

---

## 8. Quest-pack spin-offs
Each chapter's "Quest types" line becomes repeatable open-audience packs (small: daily; medium: weekly; large: once per wolf). Each pack feeds `realm_voice` and a strand. Every large personal story is a closed storyline that counts toward the Quickening. Givers are spread so that no resident's hub carries more than two campaign offers.

---

## 9. Risks and checks
- **World rules kept throughout:** wolves and pups; the Bright One and the Dark One only; "private messages"; male or female only; all talk pre-written, shown in the dialogue window or scenes; dialect by upbringing; no new residents (strangers and foes come and go); reachable interiors only.
- **Dagfinn's post** is an engine accident. This design writes it in on purpose. The DM must not "fix" it before chapter 1, and `live.npcs.work_label` should be updated to read "keeping the Palace lamp".
- **Old age:** without C11 the 78-year-old Keeper may die mid-campaign. Fallbacks are written, but C11 should come first.
- **Approval misses** objective targets, purses and boards. Check every `town:<id>` purse holds the reward coin, and that every `board:` is one of the eight towns.
- **Unplaced residents** pass approval but fail at run time. Every id here is a placed resident.
- **Offer crowding:** at most 9 choices per hub. Count each giver's offers across all live packs.
- **The Rain Hall** stays unreachable unless Atlas links it in DEV, which is optional. The Moot plays in Market Square by default, in the rain, as Ridgemere's weather already gives.
- **Words:** check story text by eye (no tool scans story programs for dialect). Wolves, pups, the Bright One and the Dark One, "private messages", no ids in speech.
