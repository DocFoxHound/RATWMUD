# The Holding Howl: the first grand campaign (designer "wild")

The realm under pressure from outside and below: the roads, a bandit captain uniting the camps, the drowned lights
of the fens, something under the ice, and the Wardens coming down the Pass. The main storyline asks one thing of
every wolf in every town: **hold together**. By Midwinter night one howl must run from the Bay Light to the Pass and
out to the Waste, every town answering the one before it.

Built for Ridgemere 3 (world key `ridgemere3`, world id `upper_accord`). All ids below are real, placed residents,
reachable interiors and real cells, checked read-only against `ratw_ridgemere3` and the survey files. Nothing was
written to the world.

---

## 0. At a glance

| Ch | Title | Game days (real days) | Season | Where it centres | The shared goal |
|---|---|---|---|---|---|
| 1 | Carters Who Don't Come Home | 70-100 (5) | late spring | Ridgemere, Oakhaven road, Coldwater | Light the near roads; raise the first two Howl posts |
| 2 | Two Long, One Short | 100-130 (5) | summer | Glasswater roads, Upper Mere, Greyteeth, Grayrock Hall | Break the Captain's lamp signals; take back the Glasswater camp |
| 3 | Black Cloaks on the Pass Road | 130-160 (5) | summer, the Long Light | Frosthollow, the Pass House, Ridgemere | Answer the Wardens' Writ: walk the envoy down, learn the eight howls |
| 4 | The Drowned Lights | 160-190 (5) | late summer | Sallow Fens, Fenshore, Mirebridge crossroads | Lay or turn the lights; raise the Mirebridge post |
| 5 | Who Speaks for the Realm | 190-220 (5) | early autumn | Ridgemere (Antechamber, Watch), every town | Eight towns send one word; the Watch marches out as the Road Watch |
| 6 | Harvest Under Arms | 220-250 (5) | autumn, Harvest Home | Larkfield ford, Longmead, Southmeadow | Hold the ford for three nights |
| 7 | The Mere Will Not Freeze | 250-290 (7) | first frost to winter | Glasswater Landing, Upper Mere, Coldwater, Frosthollow | Still water: the Quickened pup, the ice, the last posts |
| 8 | The Holding Howl | 290-321 (5) | Midwinter | All 13 posts; the Pass | Man the Howl Line and run the Howl on Midwinter night |

A game day is 4 real hours, so the whole campaign is about 42 real days and ends on the winter festival (day 321,
day 46 of winter). Today is about day 66: Chapter 1 can open within a day of approval.

Each chapter is one v2 program in a `campaign.approve` campaign (8 of 12 chapters), `audience: everyone, days 30`.
Each ends on its own goal or its own deadline (`clock` exit, `afterDays` 30) and hands on with
`story.start {audience: same, carry: true, when: ended}`. **No chapter waits on players.** Deadlines move the realm
on to a harder version of the next chapter; nothing ever stalls.

---

## 1. Premise: why now

Every seed in the data points at the same season.

- **King Gunnar died on day 41.** On day 46 the vacancy rule gave "reigning at the Great Palace" to Dagfinn
  Thornham, a Sump patrolman. In this campaign the accident is written in on purpose: the law of the Palace watch
  ("the crown is kept by whoever keeps the Palace door the night the King goes up to the Bright One") is old and
  forgotten, and Dagfinn was on that door. The Queen and the King's daughter were passed over. The towns have
  never heard of Dagfinn. **A realm with nobody it agrees on at its head is a realm that looks unkept from outside.**
- **The Greatly debts fall due at the new moon (day 85).** Carters who can't pay sell their oxen. A captain on the
  roads pays in silver, up front, for wolves who know the wagon days.
- **Lord Grayrock is never home**, and forty residents carry letters for him. One kind of letter that nobody has
  opened is a carter's receipt: the Grayrock wagons pay a toll to someone on the High Road, and so they pass.
- **The Houses will fall out before winter**: Corwin Fell wants the Rainward Hills, Gudrun Fell writes to
  Timberland in secret, and the Watch captain owes a Greatly factor. None of them will send wolves out of the walls.
- **Bandit camps keep forming** at Glasswater, on the Oakhaven road and on the Frosthollow road. This year they
  don't starve out. Someone is feeding them, and someone is signalling between them at night.
- **"The marsh lights are the drowned."** Mirebridge went under, and nothing of it is left above the fen but a
  crossroads. The lights are walking again, and the fen-folk won't go out.
- **"Something big moved under the ice."** Seven Coldwater and Upper Mere wolves saw it last winter, among them the
  Reedmere twins, 14. What they saw was the water itself, rising to carry Tansy Reedmere out from under the ice after
  she fell through. She has told no one that the water came when she called.
- **Warden attention.** Quickened magic that others see raises `wardenAttention`. A Quickening as strong as Tansy's,
  plus the Quickened wolves among the players, has been felt in Upper Accord. The Wardens are coming down the Pass,
  and there is no Warden in the realm to meet them.

**The Writ of Keeping.** The Concord's envoy brings the campaign's central test (Chapter 3): *a realm with no crowned
head, roads it cannot walk and Quickened it does not keep is a realm the Concord must keep. Show me it is one
realm. By Midwinter night let one howl run from the Pass to the Waste, every town answering. Then I will write that
you keep yourselves.*

The lore already holds the key (`any-howl-1`): every town has a howl for gathering, one for warning and one for the
dead, **and no two towns agree which is which**. The campaign is the realm learning to agree.

---

## 2. The spine: the Hold, the Howl Line and the eight voices

### 2.1 The Hold (shared progress)
- World number `hh_hold` (shown: "The Hold"). Every campaign deed adds to it: a camp cleared 5, a bandit falling 1,
  an escort done 2, a project gift 1, a letter carried 1, a wound tended 1, a side quest finished 3, a set-piece
  objective 5.
- Each chapter's goal is a story number `hh_goal`. It starts at the chapter's base and **grows by k for each new
  wolf who contributes** (a `wolves` list `hh_helpers`: on a wolf's first contribution, `in list` is false, so
  `list add` and `add hh_goal k`). The goal therefore tracks the wolves who actually play, not idle sign-ins.
  - Base/k by chapter: 20/4, 30/5, 30/5, 30/5, 40/6, 40/6, 40/6, (Ch 8 has no Hold goal).
  - With 5 wolves the goal is about 40; with 200 it is about 1,000, and 200 wolves at 5 deeds each reach it in
    about a real day. With C2 (the `audience` operand) this becomes a single `compare` instead.
- Quest packs (the small, medium and large packs) feed the same world variable: their quest-completed triggers do
  `add hh_hold`. The campaign polls on `hour` (a world variable change doesn't wake another story, gap A6).

### 2.2 The eight voices (every town matters)
- World numbers `hh_voice_ridgemere` … `hh_voice_frosthollow`. A deed adds to the voice of the town it was done
  in or for. A wild deed (a camp) is credited to its town by the trigger's `match.target`: `camp_w_ridgemere_5_4`
  goes to Oakhaven, `camp_w_ridgemere_6_8` to Coldwater, `camp_w_ridgemere_8_6` to Frosthollow, and
  `camp_w_ridgemere_6_12`/`7_12` to Fenshore and Southmeadow.
- **A town's voice is "raised" at a threshold sized to that town**, plus 2 per distinct wolf who has worked for it:
  Ridgemere 12, Coldwater 6, Larkfield 5, Fenshore 4, Southmeadow 4, Saltwell 3, Oakhaven 3, Frosthollow 3.
- **A raised town answers the Holding Howl by itself**: its residents howl from the square even if no wolf stands
  there. A lone wolf can raise Oakhaven's voice in an evening and know that Oakhaven will answer at Midwinter
  because of them.

### 2.3 The Howl Line (the thing you can see grow)
There are 13 posts, from the Bay Light to Saltwell. Each is a project (`project.post`, kind `watchpost` or
`waystation`) built from gifts of stone and timber, or a town square:

| # | Post | Where | Comes in |
|---|---|---|---|
| 1 | The Bay Light | `r2_the_bay_light_up`, door `w_ridgemere_0_0` (71,179) | Ch 1 (stands already) |
| 2 | Ridgemere waystation | proj-3, `w_ridgemere_1_1` | Ch 1 |
| 3 | Oakhaven road post | proj-2, `w_ridgemere_5_4` (Alder Mere, an old camp site) | Ch 1 |
| 4 | Oakhaven square | `w_ridgemere_3_4` (244,196) | Ch 1 |
| 5 | Glasswater relay | new post on the camp site `w_ridgemere_6_8` | Ch 2 |
| 6 | Coldwater square | `w_ridgemere_7_7` (164,4) | Ch 2 |
| 7 | Upper Mere post | proj-1, `w_ridgemere_8_6` (an old camp site) | Ch 2 |
| 8 | Frosthollow and the Pass | `w_ridgemere_9_3` (4,28), the Pass House `cn_frosthollow_guest_inn` | Ch 3 |
| 9 | Mirebridge crossroads | new post at `w_ridgemere_5_11` (188,52) | Ch 4 |
| 10 | Fenshore boardwalk | `w_ridgemere_3_13` (92,148) | Ch 4 |
| 11 | Larkfield ford | `w_ridgemere_6_9` (140,76) | Ch 6 |
| 12 | Southmeadow | `w_ridgemere_8_13` (100,20) | Ch 6 |
| 13 | Saltwell | `w_ridgemere_12_11` (220,116) | Ch 5/6 |

**The bandit camps sit exactly where the Line must run.** A camp has to be cleared before its post can be raised.
That is the campaign's physical logic, and it explains why the Captain fights the Line.

### 2.4 Recognition (a lone wolf and a crowd of 200)
- **Personal tally** (`hh_mine`, player number, shown "Your part in the Hold"), with tiers checked on every deed:
  - 1: deed "stood with the realm when the roads went dark";
  - 5: Steadfast (title C4; the herald greets you by name, C5);
  - 15: Pillar of the Hold (title C4; a `deed.award` crier call);
  - 40: Holder of the Line.
  
  Tiers are absolute: everyone who does the work gets them, however many others do too.
- **Per town**: the top contributor to each town's voice in each chapter becomes "Voice of <town>" for that
  chapter (C3 `rank` with top 1 on `hh_mine_<town>`). The town's innkeeper greets them (C5), and their name goes on
  that town's board (C1).
- **Realm-wide**: the top 3 of each chapter are cried by the herald Brenna Abbot at the festivals, and the top one
  becomes `world.figure_hero` (C9) for the next chapter. Fifteen existing lines then gossip about them by name.
- **Residents remember you**: every side story ends with its NPC thanking that wolf by name until seen (C5).
  Fallback today: `deed.award` (fame, beliefs, nicknames from the road, healer, honest and watch families).
- **The crowd feels it as weight**: story-scope set pieces show "n / goal" in every journal, and chorus strength at
  the posts grows with the number of howlers. Board posts give the count: "212 wolves held the Larkfield ford."

### 2.5 Failure never blocks
- Every chapter has a deadline that moves the realm on. A missed goal becomes a **scar**, a world switch the later
  chapters read: harder fights, a town's higher threshold, a different ending line. It never becomes a locked door.
- Personal quests fail per wolf (`fail` rules, `finish who: $actor`), never for everyone.
- Every branch has a default, so a choice nobody makes still resolves at the deadline.

---

## 3. The cast

All residents are placed and alive. Every key resident gets `npc.protect` at campaign start. Those at old-age risk
also get `npc.spare` (C11): `tn_mb_tamsin_eelbank` (70), `rm_oda_harrowgate` (60), `cty_fenwick_plankworth` (54).
Talk is carried in story text (quest offers, `npc.offer`, scenes, `story.tell from`), never through
`mantle.assign` until C15 lands. The two exceptions are wolves who wear no mantle today and get a written one:
Eskar Rimedrift and Holt Pikewade.

### 3.1 Heroes

| Who | id | Voice | Motive | Arc | Fate by branch |
|---|---|---|---|---|---|
| **Dagfinn Thornham**, m 31, the Sump King | `rmx_dagfinn_thornham` | mirebridge (fen-born; mantle mb131), calm, "knows which streets flood first" | A Mirebridge orphan who never wanted a crown; carries an unopened letter for Lord Grayrock | Bewildered watchman → walks the fens himself (Ch 4) → the realm's answer to the Writ | **King**: the "Answered King" (`figure ruler`). **Heir**: steps aside and becomes Ottilie's Roadwarden. **Council**: back to Sump patrol (C14), the Sump's hero for life |
| **Bettany Nailley**, f 26, Watch sergeant | `rm_bettany_nailley` | plain, ambitious, warm; hates Grayrock | Wants the Watch out of the walls and on the roads | Leads the Road Watch from Ch 5 | If Corra is exposed: commands the Watch (C14). Otherwise first captain of the Road Watch |
| **Ulric Stonevein**, m 30, Frosthollow miner | `cty_ulric_stonevein` | frosthollow, short, warm; "lost a sibling to a hard winter and still watches the road" | His elder brother Hakon was turned away from the mine house in the deep winter and never seen again | Smells his brother on a bandit harness (Ch 2) → learns who barred the door → can turn the Captain's lieutenant (Ch 8) | Reunited with Hakon, or mourns him |
| **Tansy Reedmere**, f 14 (adolescent), Glasswater Landing | `tn_cw_tansy_reedmere` | coldwater (mantle cw127), calm, honest, hums old marching songs | Hides that she is Quickened (water); terrified of the Wardens | Hidden → found by the envoy (Ch 7) → taught, taken or hidden | **Taught**: the first realm-kept Quickened; she answers the Howl from Glasswater. **Taken**: goes north with the Wardens (C18 depart). **Hidden**: stays untaught, and the black ring stays |
| **Gorse Reedmere**, m 14, her twin | `tn_cw_gorse_reedmere` | coldwater, short, warm, loyal and easily offended | Protect his sister at any cost | Stands at the farm gate with his hackles up | If Tansy is taken: runs to the Captain and is a never-lethal foe in Ch 8 (C10 `npc`), and can be brought home |
| **Solveig Tarwell**, f 44, priest of the Chapel of the Tide | `rm_solveig_tarwell` | plain, quiet; counts the bells; carries a Grayrock letter | The drowned have no rite | Walks to Mirebridge crossroads to say the drowned's names (Ch 4) | Her Chapel's bell begins the Howl at the Bay Light |
| **Imogen Nailridge**, f 31, cartographer | `rm_imogen_nailridge` | plain, ambitious | The first true map of the roads | Plots the Howl Line and its posts (quest-giver throughout) | Her map hangs in the Antechamber after the Howl |
| **Ottilie Harrowgate**, f 39, the King's daughter | `rm_ottilie_harrowgate` | plain, calm, warm; coughs in the damp | Has carried the Crown's petitions for years: she knows what the roads need | A candidate to speak for the realm (Ch 5) | **Heir**: Queen, with Dagfinn at her side. Otherwise the realm's Petitioner |
| **Gerrit Mudhand**, m 55, chancellor of the Council | `rm_gerrit_mudhand` | plain, chatty, warm | Keep the Houses at one table | Drafts the Answer to the Writ | **Council**: the voice of the realm |
| **Brenna Abbot**, f 52, herald | `rmx_brenna_abbot` | plain, calm, warm; owes a factor | The campaign's crier | Cries every stage and every name | Always the herald |

### 3.2 Villains and foes

| Who | id / kind | Voice | Motive | Arc | Fate by branch |
|---|---|---|---|---|---|
| **The Grey Captain: Eskar Rimedrift**, m 44, tall piebald arctic wolf | `cty_eskar_rimedrift`, an **unplaced** resident (no body, so "away over the Pass"); brought in with C18 `npc.arrive`, until then a stranger `like: cty_eskar_rimedrift`; fought through C10 `foe.add` | frosthollow, broad, quiet, guarded; "never sits with a back to the door". A new mantle (he wears none): "Cold iron. You'll learn the road." | A Deepvein miner. When Deepvein closed in the deep winter, Frosthollow's mine house would not open to its miners, and the lamp-oil list in `fh-snow-1` has no Deepvein name on it. He has been "saving coin toward something no one knows about": the price of Deepvein, to reopen it for the miners who were turned out | Unites the camps (Ch 1-2), parleys at the ford (Ch 6), strikes at the Frosthollow post on Midwinter night (Ch 8) | **Beaten**: judged by the Speaker, mercy or the pillory. **Turned**: arrives as a resident, Keeper of the Pass Road post (C18, with a post). **Killed**: lethal only if wolves choose it at the end. **Fled**: over the Pass, the seed for the next campaign |
| **Hakon Stonevein**, m ~33, the Captain's northern lieutenant | stranger and foe, `like: cty_ulric_stonevein` (he looks and smells like his brother) | frosthollow gone hard | Left in the snow and taken in by the Deepvein miners; owes the Captain his life | Signal-master on the High Road (Ch 2); holds the Pass in Ch 8 | Turned by Ulric's plea; otherwise downed and gone |
| **Desmond Hollis**, m 33, the dismissed Palace guard | `rmx_desmond_hollis` | coldwater (light), short, proud, sly; saw the Chancellor weep | Lost his post when a Sump patrolman took the throne he guarded | Copies the wagon days at the Carters' Yard for the Captain (Ch 1-2) | **Caught**: held in the cells (`npc.move` to `r2_the_keep_down` + `npc.hold`); talks; judged in Ch 7. **Fled**: leads the river company at the ford (C10 `npc` outlaw, never lethal) |
| **Sloe Mistwick**, f 44, Rivermouth skipper | `cty_sloe_mistwick` | mirebridge, short, sly; feuding over a well | Coin; and her boats are the only way across the delta at night | Ferries plunder and floats the false lights (Ch 4) | **Turned** (her feud settled): her boats bring grain upriver in Ch 6. **Exposed**: boat shed locked, held |
| **Lord Aldric Grayrock**, m 52 | `rm_aldric_grayrock_` | plain, estate; "trusts contracts more than people" | Pays the Captain a mark a wagon so Grayrock freight passes | His toll receipts turn up among the Grayrock letters (Ch 1-2) | **Exposed**: shamed; Grayrock↔Council tense. **Bargained** (through Lady Maren): tribute stops quietly, and his ships carry relief in Ch 6 |
| **Warden-Envoy Ingeborg Rimewater**, f, arctic, silvered muzzle, black cloak with silver trim | stranger (C16 voice); with two Wardens, **Aric Hollowmere** (m) and **Sunniva Coldmantle** (f) | formal Concord plain, no town words; never contracts | Believes small realms break their Quickened pups, and has seen it happen | Brings the Writ (Ch 3); watches the Mere (Ch 7); judges the Howl (Ch 8) | Leaves satisfied, leaves a Warden House, or stations keepers (the endings) |
| **Fenwick Plankworth**, m 54, Frosthollow mine captain | `cty_fenwick_plankworth` | coldwater-born, restless, sly | He barred the mine house in the deep winter | His guilt is the key to turning the Captain | Confesses at the Lamp and Pick (Frosthollow's voice rises), or keeps the secret and is shamed when the Captain names him |

### 3.3 Key NPCs (quest-givers, informants)
- **Thorn Ashwood** (`tn_oh_thorn_ashwood`, the Lamp and Pick, frosthollow): the northern hub. He carves a bird for
  every wolf who stands a night at the Pass.
- **Lorn Hedgeton** (`cty_lorn_hedgeton`, Greyteeth farm, coldwater): the shepherd who watches the road; he cracks
  the lamp code.
- **Kell Greyton** (`tn_cw_kell_greyton`, Upper Mere skipper): saw it under the ice; distrusts the city.
- **Bramble Reedmere** (`tn_cw_bramble_reedmere`): the twins' mother, Glasswater skipper.
- **Myrtle Stillford** (`tn_cw_myrtle_stillford`, Coldwater Stores).
- **Willa Coldwick** (`tn_cw_willa_coldwick`): the Coldwater carter who doesn't come home.
- **Orm Sootgate** (`cty_orm_sootgate`): the Saltwell carter, 21, who doesn't come home either.
- **Ingrid Grayrock** (`rm_ingrid_grayrock_`, 16) and **Lady Maren Grayrock** (`rm_maren_grayrock_`): the letters.
- **Gorm Plankgate** (`rm_gorm_plankgate`, abbey infirmarian, Saltwell-born, "honest to a fault"): **the Wardens'
  Eye**, the one who wrote to Upper Accord. Revealed in Ch 3 and made the first member of a new faction
  `warden_order`, which makes `told the wardens` and `vouched to the wardens` work at him.
- **Corra Slagmore** (`rm_corra_slagmore`): the Watch captain who owes a Greatly factor.
- **Tamsin Eelbank** (`tn_mb_tamsin_eelbank`, 70, Withy Creek) and **Skye Eelbank** (`tn_mb_skye_eelbank`): the
  drowned and the fen paths.
- **Fallow Lakewick** (Coldwater), **Aspen Marshholm** (Larkfield), **Rowan Downridge** (Southmeadow), **Nell
  Plummeadow** (Fenshore), **Oriel Marshditch** (Saltwell), **Tamsin Icefell** (Oakhaven) and **Thorn Ashwood**
  (Frosthollow): each town's **howl-keeper** (Ch 3).
- **Tansy Eelbridge** (`tn_mb_tansy_eelbridge`, Larkfield smith, "a path across any bog"): the ford's flank path.
- **Town watchwolves**: unplaced residents brought in by C18 `npc.arrive` from Ch 5. They exist already in
  `live.npcs`, so this respects births-match-deaths:
  - Coldwater: `tn_cw_gorse_glasston`, `tn_cw_slate_glasswick`;
  - Frosthollow: `tn_fh_vesper_icevein`, `tn_fh_wick_coldwell`;
  - Larkfield: `tn_lf_birch_downfield`, `tn_lf_thorn_larkridge`;
  - Oakhaven: `tn_oh_jessamy_greenleaf`, `tn_oh_linden_fernleaf`;
  - Southmeadow: `tn_sm_sella_cornmeadow`;
  - Fenshore: `tn_mb_holt_pikewade` (Mirebridge-born; "swears the marsh lights are the drowned").

---

## 4. The chapters

Notation: **S/M/L** = small, medium, large quest. "→ Hold n" = adds n to `hh_hold`, to the wolf's `hh_mine` and to
the town's voice.

### Chapter 1: Carters Who Don't Come Home (days 70-100)

**What happens.** The new moon (day 85) calls in the Greatly debts. Wagons go missing on the Oakhaven road and the
Coldwater road, and the near camps (`5_4`, `1_3`, `2_3`, `3_3`) don't starve out. Willa Coldwick and Orm Sootgate
don't come home: both are held at the Alder Mere camp (`npc.move` to `w_ridgemere_5_4` + `npc.hold`). Brenna Abbot
cries the King's first proclamation: Dagfinn asks every wolf with four sound paws to walk the near roads.

**Shared goal: light the near roads** (`hh_goal` 20 + 4 per helper):
- clear or scatter the near camps (`camp cleared` / `bandits scattered` on `camp_w_ridgemere_5_4`, `_1_3`, `_2_3`,
  `_3_3`) → Hold 5 each;
- escort wagons (`contract done` with escort detail; `caravan arrives`) → Hold 2;
- raise the **Ridgemere waystation** (proj-3) and the **Oakhaven road post** (proj-2): `project gift` of stone and
  timber → Hold 1 per gift, 5 on `project built`.

**The world changes.**
- At 25%, every board: "The Oakhaven road is walked again by night. Wagons go out on Riverday with an escort."
- At 50%: `talk.remember hh_road=walked` (realm), `weather.set` clear on Ridgemere for a day ("the rain lifted the
  day the road opened" is good crier copy), and the herald names the top three.
- At 100%: both posts stand (`project.complete` if the gifts fell short but the Hold was met), and
  `figure.set villain` "the Grey Captain", title "who keeps the roads now". From here the villain lines play.
- The deadline without the goal: the posts carry over unbuilt, and `hh_road=dark` stays.

**Side storylines.**
1. **Willa's Wheel-Rut** (M, Coldwater; giver Myrtle Stillford). The objectives:
   - find the overturned wagon (a `cart_wheel` placed with `item.place` on the Glasswater road at `6_8`; `item
     picked up`);
   - follow the scent to the Alder Mere camp (`region entered`);
   - free Willa (`camp cleared` at `5_4`, or `bandits.call` 3 at the wolf for a smaller fight);
   - walk her home (`npc.follow`, then `region entered` Coldwater square).
   
   She thanks that wolf by name for the rest of the campaign (C5). Coldwater +3.
   - Optional: bring Orm too (a second `npc.follow`, home to Saltwell: a long walk with a real reward, Saltwell +3).
   - If no one comes by the deadline, both are released and walk home alone (`npc.release`, `npc.walk`), and
     Coldwater's threshold goes up by 2 ("nobody came for her").
2. **Letters for a Lord Who Is Never Home** (M, personal, scales with the 40 carriers; giver Ingrid Grayrock).
   - A wolf collects 5 letters: a `conversation` with any resident in the story list `hh_letter_carriers` (the 40
     ids from `grep "letter meant for Lord Grayrock"`), counted once per carrier through a per-wolf list.
   - Each letter is one of three pre-written kinds (`random`): a petition, a grief, or **a toll receipt** ("Paid one
     mark, for the Grayrock wagons to pass the High Road. Signed with a grey paw-mark.").
   - Handed in to Ingrid, the letters set the player switch `hh_saw_toll`. This opens the Grayrock branch in Ch 2.
     Dagfinn's own letter is among the 40, and it is a toll receipt too: he has been carrying the proof since spring.
3. **The Dismissed Guard** (S→M, hidden; Ridgemere).
   - Desmond Hollis drinks at the Knucklebones (`r2_the_knucklebones`).
   - A wolf who talks to him there gets an `npc.offer`: "You smell of lamp-black and road dust." A wolf who is in
     the Carters' Yard (`r2_the_carters_yard`) at `night falls` sees him copying the wagon board (`npc.walk` on a
     schedule).
   - Telling Sergeant Bettany Nailley sets `hh_desmond_seen`.
4. **The Healer's Questions** (S, only for wolves with a Gift, N5). Gorm Plankgate asks to "look at" Gifted and
   Quickened wolves at the Sump Infirmary. It's a quiet hint that he is the Wardens' Eye.

**Quests it spawns.**
- S, daily: Oil for the waystation lamp (gift `lamp_oil` to the waystation keeper). Scent the Oakhaven road (three
  `region entered` waypoints at dusk). Bread for the robbed (gift `bread` to households in the robbed list).
- M: Willa's Wheel-Rut; Letters for a Lord.
- L, story-scope: Break the Alder Mere camp (`camp cleared` `camp_w_ridgemere_5_4`, `contributors` 3, or 1 when
  fewer than 3 wolves have signed in this chapter: two variants of the quest, offered by a `count` check).

**Talk.**
- Brenna Abbot, Market Square (story.tell from, world, at chapter start): "Hear the Palace! Wagons are going
  missing between here and Oakhaven. The King asks every wolf with four sound paws to walk the near roads, and
  every wolf with a stone to spare to bring it to the waystation."
- Myrtle Stillford (offer `say`): "#greet#, #address#. Willa took the wagon to the city on Hearthday and it hasn't
  floated home since. She knows that road blind. Would you look?" (`accept`: "I'll find her.") Her `thanks`:
  "#thanks#. Net and line, I thought we'd lost her."
- Ingrid Grayrock (offer `say`): "Half the city carries a letter for my father. He never comes home to read them.
  Would you bring me some? I'll answer them myself, if he won't."

**Gossip and helpful knowledge** (`world.mem.hh_road=dark`, then `walked`):

| Dialect | Line |
|---|---|
| plain (Ridgemere) | "Third wagon this week that never reached the Oakhaven road's end. Blood and bread, who's feeding those camps?" / helpful: "The camp by Alder Mere lights no fire. Follow the smell of wet ash downwind of the mere." |
| coldwater | "Willa's wagon hasn't come home, eh? Net and line, she knows that road blind." / helpful: "If you're going looking, the ruts turn off the road where the reeds are flattened. A fair catch of tracks there." |
| oakhaven | "No wheels on the Sound Road since Hearthday, mm? Root and rain, I don't like it." / helpful: "Camp's on the Alder Mere side, under the old windthrown oaks. You'll smell them before you see them." |
| frosthollow | "Carters down from the Pass say there are lamps on the High Road that aren't ours. Cold iron." |
| larkfield | "Freight's short-weighed every way: half the wagons and twice the price. Scales and silver." |
| southmeadow | "No wagon for the press this week. Blight take whoever's on that road." |
| mirebridge (Fenshore) | "Wagons stop at the fen edge now, no? Nobody wants the causeway after dark. Boggy times." |
| saltwell | "The caravan from the north is late, my brother. The well remembers who drew from it. The road forgets." |

**Triggers.** Existing: everyone audience; world variables; `camp cleared`, `contract done`, `project gift`,
`conversation` with `in list`; `npc.move`/`hold`/`follow`; `item.place`; `board.post`; `story.tell from`;
`deed.award`; `weather.set`. Needed: C8 `talk.remember` (gossip), C9 `figure.set`, C5 `npc.greet`, C1 text fill
(names on boards), N5 `wolf has gift` (the Healer's Questions; fallback: offer it to everyone).

---

### Chapter 2: Two Long, One Short (days 100-130)

**What happens.** The camps are one company. At night lamps flash from ridge to ridge: two long, one short. A board
notice appears in every town in a grey paw-mark: "The roads have a keeper now. One mark a wagon." The Captain's
northern lieutenant, Hakon Stonevein (a stranger, re-added daily on the High Road ridges), runs the signals from
the Upper Mere camp (`8_6`). The Glasswater camp (`6_8`) grows to strength 12.

**Shared goal: break the signal chain** (30 + 5 per helper):
- **Douse the lamps**: four signal regions on ridges (one cell each: `6_8`, `8_6`, `7_7` hills and `5_4`). On each
  `hour` between 21 and 4, if `count in region` ≥ 1, the lamp is out that hour: Hold 1 per wolf present, capped. A
  lone wolf on one ridge matters; four wolves on four ridges blind the company.
- **Take back the Glasswater camp** (L, story-scope): three waves of `story.area.spawn` (6 each); `camp cleared`.
  Then `project.post` the **Glasswater relay post** on the cleared site.
- **Build the Upper Mere post** (proj-1, `8_6`).
- Voices: Coldwater, Frosthollow.

**The world changes.**
- Signal lamps are visible as gold map markers (`story.ping`) where they burned last night.
- Each night the lamps stay dark, every board in the Greenholt posts "No lamps on the downs last night."
- At 100%: `figure.set` gives the Captain's name ("Eskar Rimedrift, once of Deepvein", through the figure's
  `about`), and `talk.remember hh_captain=named`.

**Branches.**
- **Desmond Hollis.** If `hh_desmond_seen` and a wolf brings Bettany the copied wagon board (an `item.place` of a
  `letter` in the Carters' Yard at night; gift it to `rm_bettany_nailley`): Desmond is held in the cells, and his
  `npc.offer` chain names the Captain. Otherwise, at the deadline, `npc.walk` takes him out of the North Gate and
  C18 `npc.depart` (or `npc.hold` off-road with today's engine) sends him to the Captain. Switch `hh_desmond_fled`:
  he leads the river company in Ch 6.
- **Grayrock's toll.** A wolf with `hh_saw_toll` can take the receipts:
  - **to Brenna, to be cried** (`hh_grayrock_exposed`): `faction.relation house_grayrock`–`council_of_houses`
    tense; the Captain hits Grayrock wagons next week (`bandits.call` on Grayrock escorts); the Sump's talk turns
    warm toward the wolves (56 residents "do not forgive House Grayrock");
  - **to Lady Maren, quietly** (`hh_grayrock_bargain`): the toll stops; Maren pledges two ships for relief in Ch 6
    (`ship` events then carry grain); the relation stays; the Sump never learns, and Gorm Plankgate's trust (Ch 3)
    starts lower.
  - The default, if nobody chooses: Lord Grayrock keeps paying, and the Captain is richer. Ch 6 gets one extra wave.

**Side storylines.**
1. **Lamp-Black on the Snow** (M, Frosthollow; giver Ulric Stonevein through Thorn Ashwood's taproom).
   - A harness dropped at the Upper Mere camp (`item.place rope`) carries a scent Ulric knows. His `npc.offer`
     chain: "That's my brother's scent. Older and smokier, but his. He died in the deep winter. Didn't he?"
   - The objectives: read the Frosthollow winter records at the Records Office (`archive work`); then the hidden
     one, ask Fenwick Plankworth who was turned away (`conversation`, the `npc.offer` "You barred the door.").
   - Sets `hh_ulric_knows`, which is required for the Ch 8 turn.
2. **The Shepherd Who Watches the Road** (M, Greyteeth farm; Lorn Hedgeton). Count the lamps from his shearing
   barn (`fs_sheep_foothills_barn`) on three different nights (an `hour` trigger, `wolf in place`). He gives up the
   code: `hh_signal_known`. In Ch 6 a wolf can flash false signals.
3. **Ingrid's Answer** (S, personal). Carry Ingrid's weekly letter to her father, who is not down the coast at all:
   he is at the Grayrock Wharf Office, ten minutes from home (`gift letter` to `rm_aldric_grayrock_`). His reply
   (`npc.offer`) gives the wolf a choice of what to tell Ingrid: the truth or a kind lie. Her greeting differs ever
   after.
4. **The Black Ring** (S, teaser; Kell Greyton). "Water by the Glasswater landing won't lie still, even in summer."
   A wolf who visits `fs_fishing_glasswater_shed` at dawn sees it (`story.ping`). This seeds Ch 7.

**Quests it spawns.**
- S: Douse a lamp (daily). Fish for the post-builders (`smoked_fish` to Thorn Ashwood). Stone for the Upper Mere
  post.
- M: Lamp-Black on the Snow; The Shepherd; Bring Desmond in.
- L: Break the Glasswater Company.

**Talk** (`hh_road=signals`, `hh_captain=rumour|named`):

| Dialect | Line |
|---|---|
| coldwater | "Lamps on the downs every night, two long and one short. That's no fisher's light, eh?" / helpful: "Stand on the ridge where they burn and they won't light it. They don't like being seen." |
| frosthollow | "#world.figure_villain#, they're saying. A Deepvein wolf. Knows the dark like a good seam." / helpful: "Lamps burn on the Upper Mere ridge after the second bell. Go up with your own lamp out." |
| plain (Sump) | "Grey paw-mark on the board at the Rag Market. One mark a wagon, it says. Cheek of it." |
| plain (Heights) | "Grayrock wagons came through untouched again. Funny, that." |
| larkfield | "#world.figure_villain# charges a mark a wagon. That's a better toll than the Council's, and that's no compliment. Sold, to the wolf with teeth." |
| oakhaven | "The Alder Mere post stands, mm? First thing built on that road in my life." |

**Triggers.** Existing: the `hour` trigger with `count in region`; `story.ping`; `story.area.spawn`; `project.post`;
`archive work`; `npc.offer` chains on `dialogue choice`; `faction.relation`; `bandits.call`. Needed: C10 (Hakon as
a fightable foe; fallback: he is a mute stranger and his company are bandits), C16 (a stranger who speaks), C18
(Desmond departs), C9, C8.

---

### Chapter 3: Black Cloaks on the Pass Road (days 130-160)

**What happens.** Down the Pass Road come Warden-Envoy Ingeborg Rimewater and two Wardens (strangers, `visitor.add`
at the Pass House `cn_frosthollow_guest_inn`, then at the Grey Gull Inn `rm_the_grey_gull_inn`, re-added daily
under C16). Gorm Plankgate is revealed as their Eye: the DM creates faction `warden_order` and Gorm is its first
member. At the Long Light festival in Ridgemere (`festival.call`, day 138), the envoy reads the Writ of Keeping in a
public scene in the Antechamber `r2_the_antechamber`. Brenna reads it on her behalf until C16.

**Shared goal: answer the Writ** (30 + 5 per helper):
- **Walk the envoy down** (L, story-scope, party-friendly): she moves from the Pass House to Frosthollow square, the
  Upper Mere post, the Glasswater relay and the Ridgemere waystation. She moves one stage (`visitor.leave` +
  `visitor.add`) only when at least one wolf reaches the next stage (`region entered`). The Captain tries to show
  her the roads failing: `bandits.call` on the escorting wolves at stages 2 and 4. Hold 5 per stage.
- **Learn the eight howls** (M, personal, per wolf): talk to each town's howl-keeper (8 `conversation` objectives,
  any order). Each teaches one line of the Holding Howl (`journal.note`). Then howl together at any post with at
  least one other wolf, or alone at a town square with a resident howler (N2 `chorus closed`). Finishing gives the
  title **Howl-Carrier** (C4). A lone wolf crosses the whole map; a crowd teaches it to each other.
- **Raise the Frosthollow and Pass post**: gifts to Thorn Ashwood's post at the square.
- **The realm's Quickened come forward**: every `vouched to the wardens` event at Gorm adds Hold 2.

**The world changes.**
- `talk.remember hh_wardens=here`.
- Black cloaks are seen on the High Road (`story.ping` along their route).
- At the Writ, `story.tell world`: "The Wardens of the Concord have read their Writ in the Antechamber. By Midwinter
  night, one howl from the Pass to the Waste, or the Concord keeps us." Every board posts it for 28 days.
- After the festival the herald starts a running count: "Towns whose voice is raised: n of eight" (C1; fallback:
  eight pre-written board posts, one per count).

**Branches.**
- **The Quickened wolf's choice** (personal, N5): register (`vouched to the wardens` by a friend; deed "stood before
  the Wardens unafraid"; lowers attention) or hide (`masking_oil`, avoid the cloaks' regions; switch
  `hh_hidden_quickened`). Hidden Quickened wolves get a secret path to teach Tansy in Ch 7. Registered ones get the
  envoy's trust, so her watching in Ch 7 is gentler.
- **Who should speak for the realm?** Three story numbers start counting here and run to Ch 5:
  - `hh_for_king`: deeds in the Sump and at the Palace, Dagfinn's quests;
  - `hh_for_heir`: Ottilie's petitions at the City Office `rm_the_city_office`;
  - `hh_for_council`: Gerrit's errands to the four Houses.
- **Gorm's reasons** (M): why did he write? Years ago in Saltwell he saw an untaught Quickened pup drown a well and
  herself with it. A wolf who hears it (a 3-step `npc.offer` chain) can ask him to speak for the realm's Quickened
  in Ch 7 instead of against them (`hh_gorm_ally`).

**Side storylines.**
1. **The Envoy's Wagon** (above, M for a party).
2. **Howls of Eight Towns** (above). Each keeper is a different howl tradition. Thorn Ashwood: "In Frosthollow the
   warning howl goes low, so it carries down the shafts."
3. **The Petitioner's Road** (M; Ottilie). Carry three of her petitions that the City Office lost to the towns
   they concern (`letter delivered`), and bring back the towns' answers.
4. **The Pass House Guest** (S, Frosthollow). Bring the Wardens lamp oil and blankets. Unlocks the envoy's first
   private words (C16; fallback: Gorm passes them on).

**Talk** (`hh_wardens=coming|here|writ`):

| Dialect | Line |
|---|---|
| plain (commons) | "Black cloaks at the Grey Gull, silver on the hems. Wardens, the porter says. Bright One keep us, what do they want here?" |
| plain (office) | "The Writ says one howl, Pass to Waste, by Midwinter. The Council's still arguing about whose howl." |
| saltwell | "Wardens come for the touched, my sister. In my grandmother's time they took a well-finder from the oasis. The well remembers." / helpful: "If a friend of yours carries a Gift, speak for them to the healer at the abbey. The Wardens hear a friend's word better than your own." |
| frosthollow | "Three black cloaks came down the Pass on Stoneday. Didn't take their boots off at the Pass House. Deep strange." |
| coldwater | "One howl for the whole realm? We can't agree which howl's for supper, eh?" |
| southmeadow | "Our gathering howl's the one the rest use for a burying. Sheaf and sickle, there'll be some confusion." |
| oakhaven | "Learn our howl? It's slow and it climbs, like good bark. Come by the Felled Cedar at dusk, mm?" |
| mirebridge | "Fenshore's howl? Short, so it doesn't wake the drowned. Fen-sure you'll learn it quick." |
| larkfield | "Larkfield's howl goes up at the end like a price. Scales and silver, it's the best one." |

**Triggers.** Existing: `festival.call`; `scene.play`; `visitor.add`/`leave`; `vouched to the wardens`; `region
entered`; `bandits.call`; `journal.note`. Needed: C16 (the envoy speaks, gives quests and stands in a scene), N2
(`chorus closed` for the howl practice), N5, N6 (`warden_order` and a Warden-attention fact), C4, C1.

---

### Chapter 4: The Drowned Lights (days 160-190)

**What happens.** The Captain's southern company moves through the Sallow Fens by night, guided by lights. Sloe
Mistwick's boats out of Rivermouth carry plunder down the Long River. The fen-folk won't go out, because the drowned
are walking. **Dagfinn comes to the fens himself**: `npc.walk` from the Palace to Fenshore, at walking pace, over
two game days. His arrival at the Heron (`cn_fenshore_alehouse`) is a scene. He is Mirebridge-born; his mother's
house is under the fen.

**Shared goal: lay the lights** (30 + 5 per helper):
- **Find the false lights**: each night, 3-5 `oil_lamp` items are placed on floats in fen regions (`item.place`
  across `4_11`-`5_12`). Picking one up (`item picked up`) → Hold 1.
- **Clear the southern camps** `6_12` and `7_12`.
- **Raise the Mirebridge crossroads post** (`project.post` at `w_ridgemere_5_11`, 188,52) on drowned ground: gifts of
  `planks` and `stone`.
- **The Rite of the Tide**: Solveig Tarwell walks to the crossroads (`npc.walk` with a cue). At dusk, wolves in the
  region (`count in region` ≥ 1) and Solveig say the drowned's names (scene: her lines, a `response` beat for the
  wolves).
- Voice: Fenshore.

**Branches (lasting).**
- **Lay them or turn them.**
  - **Laid** (`hh_lights_laid`): after the rite the false lamps stop and the fog lifts (`weather.set` clear on the
    fens for 24 h). Tamsin Eelbank blesses the realm, and Fenshore's voice needs 2 less. Dagfinn's grief scene
    plays ("I knew every street of it, and which flooded first") and `hh_for_king` gets +10. Holt Pikewade arrives
    as Fenshore's first watchwolf (C18).
  - **Turned** (`hh_lights_used`): wolves keep the false lamps burning and move them onto the "do not" paths of
    `mb-fens-1`, luring the company's boats into the bog. The southern company is broken (the Ch 6 river wave is
    smaller), but the fens say the drowned were used. Fenshore's threshold goes up by 3, and Tamsin Eelbank's lines
    turn bitter for the rest of the campaign.
  - The default is laid, if any wolf attended the rite; otherwise the lights simply fade at the deadline and
    Fenshore stays wary.
- **Sloe Mistwick.**
  - **Turned**: settle her well feud (`trouble solved`, item feud, near Rivermouth). Her boats carry grain upriver
    in Ch 6.
  - **Exposed**: report her to Bettany. `door.lock` on the Rivermouth boat shed, `npc.hold`, `times.nudge
    fenshore` −20 for 14 days.

**Side storylines.**
1. **The Path With No Note** (M, risky; Skye Eelbank, then Tamsin Eelbank). Follow the unnoted path of `mb-fens-1`
   in fog (`weather.set fog`) through a chain of regions to the drowned causeway. There lies the Captain's cache:
   goods taken from the robbed wagons (`item.place`). Return each to its owner (`gift` to named residents); every
   owner thanks that wolf by name (C5).
2. **The King's Mother's Door-Stone** (M, personal; Dagfinn). Find the carved threshold stone at the crossroads (an
   `item.place stone` with a description in the journal) and carry it to Thornham House (`rm_thornham_house`).
   Afterwards: "#greet#. You're the one who brought my mother's stone up out of the mire." `hh_for_king` +3.
3. **Brackish Salt** (S, Saltwell; Thistle Stonepick). Salt from the robbed wagons is being sold at the Oasis Inn.
   `stolen goods smelt` → Hold 2, and Saltwell +2.
4. **The Chancellor's Tears** (M, hidden, unlocked by `hh_lights_laid` or `hh_lights_used`). Oskar Gullworth wept in
   the Rain Hall because he signed away the Mirebridge flood relief to pay a House debt. Forty-six residents saw
   it. A wolf who asks him at the City Office (with three witnesses' word: `conversation` with 3 from the "weep"
   list) hears his confession. He funds the Mirebridge post (`character.grant` 200 from `town:ridgemere` toward
   the wolves who built it). `hh_for_council` −5, `hh_for_king` +5.

**Talk** (`hh_lights=walking|false|laid|used`):

| Dialect | Line |
|---|---|
| mirebridge | "Lights out over Mirebridge again. Fen-sure they're the drowned, no? My nan says never follow one." / helpful: "Smell the water by a light. The drowned don't burn oil." / laid: "Dark marsh last night, and dry feet. First time since spring. The priest from the city said every name." / used: "They used the drowned for bait. Dark One's mire, we'll pay for that." |
| plain (wharf) | "The King's gone to the fens. Walked it. A King, walking." |
| saltwell | "A fen-born wolf at the oasis says the lights were lamps on floats. Mirage-talk, or the truth? The well knows." |
| southmeadow | "Fen boats on the Long River at night, no lamps lit. Blight take them, my cousin lost a cart of honey." |
| coldwater | "The Sump King walked the fens, eh? Fair catch for a King." |

**Triggers.** Existing: `item.place`; `item picked up`[w]; `npc.walk` with a cue → `scene arrived`; `scene.play`;
`count in region`; `weather.set`; `trouble solved`; `door.lock`; `times.nudge`; `stolen goods smelt`. Needed: C18
(Holt arrives), C5, C8, C11 (Tamsin Eelbank, 70).

---

### Chapter 5: Who Speaks for the Realm (days 190-220)

**What happens.** Autumn. The Houses fall out, as 38 residents always said they would:
- Corwin Fell takes the Captain's offer: "Fell wagons safe for Rainward" (Gudrun's letters).
- Ambrose Greatly forbids the Watch to leave the walls his House built, and holds Corra Slagmore's debt over her.

The envoy demands to know, by the Harvest festival, who will answer the Writ.

**Shared goal: one word** (40 + 6 per helper):
- **Eight words**: each town's howl-keeper gives a sealed word (`item.give letter`, quest-tracked). A wolf carries
  it to Gerrit Mudhand in the Antechamber (`gift letter`). Hold 5 each; the town's voice +3.
- **The Road Watch**: free the Watch to leave the walls (the Corra branch). Then escort each arriving town
  watchwolf from the Watch House `rm_the_watch_house` to their town (`npc.follow`; C18 `npc.arrive` with a post).
  A lone wolf can give Oakhaven its first watch.
- **The Saltwell road**: raise the Saltwell post; escort the Waste Road caravans.

**Branches (lasting).**
- **The Speaker.** On day 228 the largest of `hh_for_king`, `hh_for_heir` and `hh_for_council` wins (a tie goes to
  the Council). A public scene in the Antechamber follows.
  - **King** (`hh_speaker=king`): Dagfinn is the "Answered King" (`figure.set ruler`). The Sump's talk is proud;
    the Heights sneer; Oda is bitter (and her purse stays shut).
  - **Heir** (`hh_speaker=heir`): Ottilie is acclaimed Queen and names Dagfinn her Roadwarden. Oda opens the royal
    purse: `character.grant` from `treasury` to the Road Watch escorts.
  - **Council** (`hh_speaker=council`): Gerrit speaks; Dagfinn goes back to Sump patrol (C14 `npc.post`); the
    Houses pay for the Road Watch, so in Ch 8 House wagons get escorts first.
- **Corra's debt.**
  - **Paid**: wolves gift coins to Corra (`gift`, `measure: coins`, goal 60 + 5 per helper). She squares the
    factor and marches out with the Watch. Deed: "paid the Watch captain's debt so the Watch could march".
  - **Exposed**: take her debt to the advocate Cressida Carlisle (`rm_cressida_carlisle`). Corra stands down and
    Bettany commands the Watch (C14).
  - The default at the deadline: Ambrose relents for a price (`faction.relation house_greatly` ±), and the Watch
    marches late, so Ch 6 begins with one night less of warning.
- **Fell and the Captain** (Gudrun's letters).
  - Intercept a letter at Fellhall (`rm_fellhall`) and take it to Signe Timberland: Fell refuses the Captain.
  - Otherwise the Captain gets the Rainward camps back (`story.area.spawn` at `1_3`/`2_3`/`3_3`), and Ch 6 has a
    second front near the city.

**Side storylines.**
1. **The Secret Lists** (M; Desmond Dripcot keeps the cells, and 35 residents carry lists). The lists show a new
   payer: a grey paw-mark pays the gatewolf at the Cinder Gate. Find him (Palle Plankmore). `hh_cinder_gate_shut`
   means no ambush at the Cinder Gate in Ch 6.
2. **A Watchwolf for Your Town** (S, personal; one per wolf; see above).
3. **The Dismissed Guard Judged** (if Desmond is held): the Speaker hears his case at the Magistrate's Court
   (`rm_the_magistrate_s_court`) in a scene with a `choice` (`majority`) for the wolves present:
   - mercy: he joins the Road Watch and in Ch 6 guards the granary;
   - the pillory: the Sump cheers, and his kin turn cold.
4. **Ottilie's Cough** (S; Gorm or Rupert Fogton). Bring `herb_tea` and `blanket` so she can stand in the damp
   Antechamber. If she is the Speaker, this decides whether she can howl at the Bay Light in Ch 8.

**Talk** (`hh_speaker`, `hh_road=held`):

| Dialect | Line |
|---|---|
| plain (Sump) | "Our Dagfinn, answering for the realm. He knows which streets flood first. That's more than the Heights know." |
| plain (Heights) | "A patrolman on the Palace steps. Still, he walked the fens. Bright One keep him." |
| plain (office) | "Eight sealed words came in to the Antechamber. The Chancellor reads them in his sleep." |
| coldwater | "We've a watch of our own now, eh? Two of them. Fair winds to them." |
| larkfield | "The Road Watch rides through Marketday. That's worth a penny on every sack. Fair price to you." |
| oakhaven | "A watchwolf in Oakhaven, mm? First since my grandmother's day." |
| frosthollow | "Fell's dealing with #world.figure_villain#, they say. Black seam, that House." |

**Triggers.** Existing: `item.give`; gifts with `measure: coins`; `npc.follow`; `scene.play` with a `majority`
choice; `faction.relation`; `story.area.spawn`. Needed: C14 (`npc.post`), C18 (`npc.arrive` with a post), C9
(`figure.set ruler`), C13 (`month begins`: the new-moon dues).

---

### Chapter 6: Harvest Under Arms (days 220-250)

**What happens.** The Captain gathers every company to take the Larkfield ford at Harvest Home. Whoever holds the
ford holds the grain, and whoever holds the grain holds the winter. He parleys first.

**Shared goal: hold the ford for three nights** (L, story-scope; 40 + 6 per helper):
- three nights, three waves each, at three regions: **the ford** (`w_ridgemere_6_9`), **the Longmead granary**
  (`fs_grain_long_granary`, its yard) and **the Downs Road**;
- spawn count per region is `min(6, 1 + wolves in region)`, through `count in region` branches, so 5 wolves get a
  hard fight and 200 wolves get many fights, never a hopeless one;
- named foes (C10): the Captain's lieutenants, and Desmond Hollis if he fled (`npc`, never lethal);
- **mend the ford bridge** (`project.post` kind `mend`): planks and nails;
- **bring the harvest in**: gift grain or bread to the granary keeper;
- **tend the wounded** (`tended`);
- **the relief**: Grayrock ships at the Wharf (if bargained) and Sloe's boats (if turned) each cut a wave.

Voices: Larkfield, Southmeadow, Saltwell.

**The lone wolf's jobs.** **The Night Runner**: carry the warning howl from the ford to the Southmeadow and Saltwell
posts within an hour (a timer, and `chorus closed` or `region entered` at each post), so their watchwolves come.
One fast wolf brings two towns' help. **The Granary Door**: keep the granary region occupied through a wave
(`region emptied` fails it).

**Branches (lasting).**
- **Ford held** (`hh_ford_held`): `harvest.override grain good`, 30 days; Larkfield's voice +5; the Captain
  retreats north, his pride hurt.
- **Ford burned** (`hh_ford_burned`): `harvest.override grain poor`, 30 days; `times.nudge larkfield` −30. Ch 7 adds
  a goal: feed Larkfield (`gift` food to its residents). The Captain recruits, so Ch 8 has more waves.
- **The parley** (C16; the Captain's stranger voice). On night two a truce lamp burns. The first wolf to reach it
  hears him in a scene: Deepvein, the barred door, the coin he is saving. The choices:
  - **Offer terms** (needs `hh_ulric_knows`: "Frosthollow will say what it did"): `hh_parleyed`, which makes the
    turn possible in Ch 8;
  - **Refuse**;
  - **Strike under the truce**: the lieutenant falls at once, but the realm's gossip turns ("truce-breakers") and a
    `deed.revoke` takes back that wolf's last road deed.

**Side storylines.**
1. **The Night Runner** and **The Granary Door** (above).
2. **The Smith's Bog Path** (M; Tansy Eelbridge). She shows a flank path. With `hh_signal_known`, a wolf flashes two
   long and one short from it, and the river company walks into the shallows (one wave gone).
3. **Rook's Word** (S; Rook Wheatfield at Longmead). Carry his word to his sweetheart in Southmeadow across the
   besieged Downs Road.
4. **A Granary Guard** (if Desmond was shown mercy). He asks one wolf to stand with him through the third night. If
   they do, he is redeemed in the talk and keeps his post in the Road Watch.

**Talk** (`hh_ford=siege|held|burned`):

| Dialect | Line |
|---|---|
| larkfield | "Every sack in Longmead's gone to the ford walls. Scales and silver, I never thought I'd see grain for a rampart." / helpful: "They come up the shallows below the mill. The current's slack there after dark." / burned: "Short-weighed, all of it. The winter, the bread, the lot." |
| southmeadow | "The runner came at midnight, howling the new howl. We knew it! Sheaf and sickle, we went." |
| saltwell | "Salt and water for the wolves at the ford, my brother. The well gives to those who stand." |
| plain | "They held the Larkfield ford three nights. Blood and bread, I'd have liked to see it." |
| mirebridge | "Fen boats bringing grain upriver now, no? Not plunder. That's new." (if Sloe turned) |

**Triggers.** Existing: `story.area.spawn` by region with `count in region`; `harvest.override`; `times.nudge`;
`tended`; `region emptied`; timers; `deed.revoke`. Needed: C10 (named foes), C16 (the parley), N2 (the runner's
howl), C1 ("212 wolves held the ford"), C3 (rank: the ford's heroes).

---

### Chapter 7: The Mere Will Not Freeze (days 250-290)

**What happens.** First frosts, then winter (day 276). Glasswater and the Upper Mere freeze, all but **a black lane
that runs upriver like a wake**. Something big moves under the new ice again. The envoy comes to the Upper Mere
with her instruments (C16; she lodges at the Glasswater Inn `cn_coldwater_inn`). Tansy Reedmere hums marching songs
to keep the water still. Gorse stands at the farm gate with his hackles up. The Captain moves north to the Pass.

**Shared goal: still water** (40 + 6 per helper):
- **Safe crossings**: the ice crossings at `8_6` and `7_8`. Rescues: Kell Greyton stranded on the ice in a snow
  (`npc.move` + `npc.hold` + `weather.set snow`; bring `rope`). Board warnings.
- **Warm the north**: `blanket`, `peat` and `smoked_fish` to Coldwater and Frosthollow households. If the ford
  burned, feed Larkfield too.
- **The last posts**: whatever is unbuilt is offered again, with a larger Hold reward for each.
- **The Tansy question** (below).

Voices: Coldwater, Frosthollow.

**Branch: Tansy** (the realm decides by deeds; the default is "hidden").
- **Teach her** (`hh_tansy_taught`): **Stillwater lessons**, a story-scope quest with `contributors` 3 (1 below 3
  helpers). Each lesson is one of:
  - sit with her (`together`);
  - hum the old songs (`performance` at the Glasswater landing);
  - walk the ice with her (`npc.follow` to the black lane and back).
  
  Gifted and Quickened wolves' lessons count double (N5). Hidden Quickened wolves can teach her too, at the risk of
  being seen (a `chance` check raises N6 attention). Ten lessons and Tansy stills the lane. The envoy writes that
  the realm keeps its own: the Writ needs one fewer town, and Gorm (if `hh_gorm_ally`) is named the realm's own
  Warden.
- **Give her up** (`hh_tansy_taken`): wolves tell the envoy (or talk Bramble round, through an `npc.offer` chain).
  Tansy goes north at Midwinter (C18 `npc.depart`; her family's talk turns to grief). The Writ eases by one town.
  **Gorse runs to the Captain** (C10 `npc` outlaw, never lethal, in Ch 8). He can be brought home.
- **Hide her** (`hh_tansy_hidden`, the default): `masking_oil`, the Upper Mere boat shed, a lie to the envoy (an
  `npc.offer` "No pups here but ours."). Each hiding wolf rolls `chance` against the envoy's search. If she is
  found, she is taken and the Writ hardens by one town. If not, she stays untaught and the black lane stays.

**Side storylines.**
1. **Kell on the Ice** (S, rescue).
2. **The Lamp-Oil List** (M, needs `hh_ulric_knows`). Bring Fenwick Plankworth to the Lamp and Pick on a Restday
   evening (`npc.walk`) for a public confession scene. Frosthollow's voice +4. Ulric goes up the Pass to look for
   Hakon.
3. **Midwinter Bells** (S, all posts). Bring a `handbell` or a `horn` and `lamp_oil` to every post's keeper.
4. **Gorse at the Gate** (S; only if Tansy is hidden or taught). Win Gorse's trust (`gift`, `conversation` ×3) so
   that he stands the Glasswater post on Midwinter night.

**Talk** (`hh_mere=ring|moving|still|taken|hidden`):

| Dialect | Line |
|---|---|
| coldwater | "Glasswater's iced edge to edge, but there's a black lane in it like a boat went through. No boat did, eh?" / helpful: "The Reedmere lass hums when the water's restless. If you hear her stop, stand back from the edge." / still: "Still water. Truly still, the whole mere. I've never seen it so, eh? Calm water to her." / taken: "They took the Reedmere minnow north. The mere's frozen right over. Doesn't feel like a mercy." |
| frosthollow | "#world.figure_villain# is on the Pass Road with every wolf he has left. Keep your lamp lit." |
| plain | "A Warden at the Glasswater Inn with brass things that tick. Looking for the touched, they say." |
| larkfield | "Coldwater wants blankets, not grain. Sold, the lot, at cost. Fair price to you." |
| oakhaven | "The Sound's freezing at the head. Root well, it'll be a long one." |

**Triggers.** Existing: `together`; `performance`; `npc.follow`; `chance`; `weather.set snow`; `scene.play`; `gift`.
Needed: C16 (the envoy's voice and her search), N5/N6 (Gift and attention), C18 (`npc.depart`), C10 (Gorse), C11.

---

### Chapter 8: The Holding Howl (days 290-321, finale on the winter festival night)

**What happens.**
- **Before the night**, days 290-320: stock every post (`lamp_oil`, `blanket`, a `horn`). Each post has a keeper,
  a resident walked there by `npc.walk` once a wolf finishes the post's "Keeper" quest. Cast at run time (C7: the
  nearest willing resident of that town); fallback: one fixed keeper per post, chosen from the cast lists.
- **The Captain's last stroke**: he means to break the Line at the Frosthollow post on Midwinter night, so the
  Howl dies halfway.
- `festival.call` names the night "the Holding Howl" in every town.

**How the Howl runs** (N2 `chorus closed`, timers; mostly today's engine):
1. At `night falls` on day 321, the Bay Light (post 1) begins. A wolf howls in `r2_the_bay_light_up`, or Solveig's
   bell rings for the Speaker (a resident howl, N2 `npc.howl`).
2. Each post opens a **window of one game hour (10 real minutes)** when the post before it answers. The post
   answers if:
   - a chorus closes in its region (any number of wolves; strength = howlers), **or**
   - its keeper resident is present and the town's voice is raised (residents howl: N2 `npc.howl`), **or**
   - a wolf the keeper knows (who finished its Keeper quest) is anywhere in its town.
3. A `story.tell world` traces the run: "The howl has reached Coldwater." "Fenshore answers, and the fens with it."
   "Silence at the Upper Mere post..." (pre-written per post and per outcome; C1 adds the howlers' names).
4. If a post misses its window, the next post can still pick the howl up from **any answered post within two
   steps**, so one silent post never ends the night. It just costs that town its answer.
5. **The Frosthollow fight**: the Captain and his band attack post 8 at the window's opening (C10 `foe.add`,
   `bandits.call`). The post answers if it is held (a fight won, or `count in region` ≥ 1 at the window's close).
   - **Turn** (needs `hh_ulric_knows` and `hh_parleyed`): Ulric steps out of the dark with a scene `choice` for
     the wolves present: "Hakon. They said what they did." Hakon lowers his lamp; the Captain stands alone, and his
     scene choice is to surrender, fight or go. Fenwick's confession (Ch 7) makes "surrender" the default.
6. The night ends when the last post answers or the last window closes. `hh_answered` (the towns that answered)
   and `hh_strength` (howlers in all) are kept.

**The shared goal**: 8 towns answering, sized by nothing. A raised voice answers even with no wolf there, so a
small world that worked all autumn wins. Wolves at a post make it **strong**, and strength makes the epilogue lines.

**Endings** (decided by `hh_answered`, adjusted by Ch 7: taught +1, taken +1, found −1):

| Ending | Needs | What happens |
|---|---|---|
| **The Realm Answers** | 8 | The envoy tears the Writ at the Bay Light (scene). The Wardens leave by the Pass. The realm keeps its own: a Warden House in Ridgemere run by Gorm, or Tansy's teacher. Every wolf who manned a post gets the title "Howler of the Holding" (C4); the top howlers become `figure hero`. `hh_road=held` for good |
| **The Realm Holds** | 5-7 | The Writ is set aside. One Warden stays at the Grey Gull and watches; the towns that stayed silent get a Warden keeper (a recurring stranger) until next spring's campaign |
| **The Realm Bends** | 0-4 | Wardens are stationed in every silent town, and Quickened wolves must register. The next campaign is winning them back. Still a full ending: deeds, titles and thanks are given |

**The Captain's fate**:
- **Turned**: C18 `npc.arrive` places `cty_eskar_rimedrift` as keeper of the Frosthollow post, with a written
  mantle. He has a home at the Pass House, and Deepvein's coin goes to the Frosthollow purse. Hakon goes home
  with Ulric.
- **Beaten**: held in the cells; the Speaker's judgment (mercy or the pillory) is the epilogue scene.
- **Killed**: only through an explicit "no quarter" scene choice. Ulric's grief lines follow, and Frosthollow
  mourns.
- **Fled**: over the Pass with his coin. `figure villain` stays set, the seed for the next campaign.

**Talk on the night and after** (`hh_howl=night|ran|broke`):

| Dialect | Line |
|---|---|
| plain | "I heard it come in off the Bay Light and go up the hill like a wave. Bright One, I've never heard the like." |
| coldwater | "We answered, eh? Glasswater answered! Still water, and a howl over it." |
| frosthollow | "Held the Pass post. Cold iron and warm hearts, the both of it." |
| mirebridge | "Fenshore howled short, like always. But it was on time, no?" |
| saltwell | "The howl came out of the dark to the oasis, my sister, and we sent it on. The well remembers." |
| oakhaven | "Old Oak, it came through the cedars slow and climbing. Our howl, in the realm's mouth." |
| larkfield | "Larkfield answered on the dot. Sold, to the realm." |
| southmeadow | "Ripe days. We howled the burying howl by mistake and nobody minded." |

**Triggers.** Needed: N2 (`chorus closed`, `npc.howl`), C7 (cast keepers), C10, C16, C18, C4, C1, C9, C3. Existing:
timers, `schedule`, `story signal`, `count in region`, `festival.call`, scenes.

---

## 5. Talk plan

**Where it lives.**
- `Data/Talk/scenes/regions/holding_<town>.talk` (8 files) and `Data/Talk/scenes/common/holding_common.talk`:
  NPC-to-NPC scenes on the topics `day`, `smalltalk`, `news`, `bandits`, `lore` and `event`, gated on
  `world.mem.hh_*`.
- `resident.tree`: `news`/`gossip` `say[world.mem.hh_*]` lines.
- `say[self.id=<npc> world.mem.hh_*]` window lines for the key cast in `resident`, `official`, `inn`, `watch` and
  `church`.
- Everything is gated on facts the test town never has, so `talk_scorecard_quick` doesn't move.

**Memory keys** (C8 `talk.remember`, `scope: realm|town`):

| Key | Values |
|---|---|
| `hh_stage` | 1-8 |
| `hh_road` | dark, signals, walked, held, broken |
| `hh_captain` | rumour, named, parley, beaten, turned, fled, dead |
| `hh_wardens` | coming, here, writ, watching, satisfied, staying |
| `hh_lights` | walking, false, laid, used |
| `hh_speaker` | king, heir, council |
| `hh_ford` | siege, held, burned |
| `hh_mere` | ring, moving, still, taken, hidden |
| `hh_voice` (town scope) | quiet, rising, raised |
| `hh_howl` | coming, night, ran, broke |

**Kinds of lines, per key and per dialect** (about 8-12 alternatives each, ≤160 characters):
- **gossip**: what happened;
- **helpful knowledge**: where, how, and the scent tips quoted above;
- **opinion**: by `opinions.json` issue (watch, houses, road, strangers);
- **thanks**: `topic.meeting=famous` with `topic.deed`.

The helpful lines are the heart of item 7. Each chapter has 3-4 facts a wolf can learn from any resident instead of
the journal:
- where the camp is, and that it lights no fire;
- the lamp code;
- smelling oil on the water;
- the shallows below the mill;
- "she hums when the water's restless";
- "listen for the Bay Light bell, then count a hundred breaths".

**Key NPC paths** (multi-step `npc.offer` chains on `dialogue choice`, one step per pick, ≤64 per program):
- **Ulric** (5 steps): the scent → "He died. Didn't he?" → the records → "Who barred the door?" → "If he's alive,
  I'll stand on the Pass with a lamp lit."
- **Gorm** (4): "Why did you write to them?" → the Saltwell well → "Would you speak for ours?" → the answer, by
  `hh_tansy_*`.
- **Dagfinn** (4 per stage): "Do you want the crown?" → "I know which streets flood first. That's all a King
  needs, they tell me." Branches by `hh_speaker`.
- **Tansy** (4, only for wolves with `hh_lessons` ≥ 1): "Does the water talk?" → "It listens. That's worse." → the
  songs → "Will you stand at the landing on Midwinter night?"
- **Bramble Reedmere** (3, the give-up path): she is talked round, or not.
- **Maren Grayrock** (3): the receipts → "My husband trusts contracts. Let him see one he can't refuse." → the
  bargain.

**Voices of the strangers** (C16):
- **The envoy**: formal and unhurried. She never uses town words or oaths and never swears by the Bright One. "I
  have not come to take. I have come to see whether there is anyone here to give to."
- **The Captain**: Frosthollow broad, gone cold. "Warm hearth, they say up there. Not for Deepvein wolves."
- **Hakon**: few words. "Ulric always did count twice."

**Allow lists.** Add "Grey Captain", "Holding Howl", "Writ", "Wardens", "Concord", "Pass House", "Bay Light" and
"Deepvein" to `tools/dialect_allow.json`. Mantles stay name-free (slots only). Eskar Rimedrift's and Holt
Pikewade's new mantles (`fh9xx`, `mb9xx`) follow doc 84's gates.

---

## 6. Trigger features required

Existing features are named in each chapter. The table gives what's missing and what's new.

| Need | Covers | Status / mapping | Fallback today |
|---|---|---|---|
| Hold goal that scales with the wolves who play | §2.1 | **Existing workaround** (`hh_helpers` list + `add hh_goal k`); best with **C2** `audience` | Works now |
| Town voices credited by place | §2.2 | **Existing** (`wolf in town`, match on camp ids, 8 small triggers on one `emit`) | Works now |
| Progress words in boards, banners, names | every chapter | **C1** variable text | Pre-written posts per threshold |
| Top contributors per town and realm | §2.4 | **C3** `rank`, `credit: share` | `each` tiers only |
| Campaign titles | §2.4, Ch 3, Ch 8 | **C4** `title.give` | `deed.award` phrases |
| A resident thanks one wolf by name | all side stories | **C5** `npc.greet` | `deed.award` (fame gossip) |
| Chain quests per wolf; the giver speaks mid-quest | Ulric, Tansy | **C6** `requires`, `offer.during` | `quest.offer` journal chains |
| Cast keepers and robbed carters at run time | Ch 1, Ch 8 | **C7** `cast` | Fixed cast lists |
| Campaign gossip and helpful knowledge | §5 | **C8** `talk.remember`, `belief.plant`, `player.quest.*` fact | Only deeds and figures |
| Villain, ruler and hero in everyone's mouth | Ch 1, 5, 8 | **C9** `figure.set` | DM `save_figure` by hand at each stage |
| The Captain, Hakon, Desmond and Gorse as fightable foes | Ch 2, 6, 8 | **C10** `foe.add`, `foe defeated` | Anonymous bandits only |
| Keep the old alive | Tamsin Eelbank, Oda, Fenwick | **C11** `npc.spare` | Authored `protected` flag (DM) |
| Quest packs signal the campaign by name | §2.1 | **C12** `story.notify`, `field starts` | A world variable, polled hourly |
| The new moon and the succession | Ch 1, 5 | **C13** `month begins`, `succession`, `variable changed` | `schedule` on fixed days |
| Lasting fates: Dagfinn back on patrol, Bettany commands | Ch 5 | **C14** `npc.post` | Talk-only fates |
| Story mantles without clobbering | — | **C15** restore | Don't use `mantle.assign` |
| **N1 = C16. Strangers who speak**: `visitor.add` with `voice {dialect, accent}`, `face` from `like`; usable as `npc` in `npc.line`/`offer`/`speak`/`walk`, scene cast and quest `giver`; `days` up to 30 (re-added by the game, still undone at the end) | the envoy, the Captain's parley, Hakon | **New** | Gorm and Brenna speak for them; `story.tell` |
| **N2 = C17. The howl in stories**: events `howled` (actor, cell, x, y) and `chorus closed` (actor = first howler, quantity = howlers, cell, x, y, item = nearest town); effect `npc.howl {npc}` (a resident howls: hearers, chorus and town memory as for a wolf) | Ch 3 practice, Ch 6 runner, Ch 8 Howl | **New**; the game already has `Game::howl` and choruses (`RatwGameHowl.cpp`), but no story event | `region entered` + `count in region` at the window instead of a howl |
| **N3 = C18. Arrivals and departures**: `npc.arrive {npc (unplaced), home?, post?, cell, x, y}` and `npc.depart {npc}` (to the unplaced pool; posts fall vacant by the usual rules). Lasting, never undone. Respects births-match-deaths: only existing `live.npcs` rows | the Captain turned, town watchwolves, Tansy taken, Desmond fled | **New** (cast survey §8.4) | `npc.hold` far off-road; no new watchwolves |
| **N5. Gift facts**: condition `wolf has {what: gift\|family\|tier, value}` | the Quickened and Gifted side stories, double lessons | **New** | Offer to everyone, and say so in the text |
| **N6. Warden attention**: fact `warden attention {op, value}` and effect `warden.attention {player, points ±5}`; DM data: faction `warden_order` and member `rm_gorm_plankgate` | Ch 3, Ch 7 | **New** (the field exists: `Entity::wardenAttention`) | `vouched/told the wardens` events once the faction exists |
| **N7. A story's resident in a quest list**: `in list` with `{field: target}` for letter carriers and weep witnesses | Ch 1, Ch 4 | **Existing** (verify the `field` operand in `in list`) | — |

**Data the DM creates first**: faction `warden_order` (Gorm a member); `npc.protect` on the cast; the authored
`protected` flag for the old; the `hh_*` world variables (`world-var declare`); `project.post` for the new posts at
`6_8` and `5_11` and the ford mend; a decision to write Dagfinn's "reigning" post in on purpose (this campaign does).

---

## 7. Building it

1. **Chapters first**: Ch 1 and 2 need only C8 and C9 to be good, and run today with fallbacks. Approve them while
   C16 and N2 are built. Ch 3 is the first chapter that really wants C16; Ch 8 really needs N2.
2. **Quest packs alongside**: the S quests listed per chapter go in an open-audience pack per chapter (`repeat
   daily`, ≤32 quests). Givers are spread over different residents so the 9-slot hubs don't crowd: no innkeeper
   carries more than two campaign offers at once.
3. **Checks**:
   - `review(p)` must pass with no uncovered phases, `problems(p) == []` and `references()` must pass on R3.
   - Check by hand what approval misses: objective `match` ids, `character.grant` purses and `board` towns.
   - Run every offer `say`/`thanks` through `talk_check` by eye for symbols and dialect.
   - `dialect_check` and `talk_lint` on the new `.talk` files.
4. **Risks**:
   - Old age (C11); unplaced residents passing approval (cast only placed ones; Eskar and the watchwolves only
     through N3).
   - Several stories at one giver.
   - `npc.kill` is never used on a key resident without a `resident is alive` fallback.
   - No population NPC is ever generated: strangers come and go, and arrivals come only from the existing unplaced
     pool.
