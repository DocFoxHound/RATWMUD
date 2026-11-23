# Large quests for Ridgemere 3: eleven heavier quest lines and five stage quests (design)

World: Ridgemere 3 (world key `ridgemere3`, db `ratw_ridgemere3`, world id `upper_accord`). These eleven quest lines live beside **The Ninth Howl** (`campaign.md`). Each one stands alone, and each one feeds or foreshadows the campaign through its world variables, never through a campaign branch. Five more single quests, the large tier of the stage pack `nh-pack.stage`, follow the campaign's stages (§12b).

**Binding.** `decisions.md` (the coordinator's decisions) and `givers.tsv` (the one giver registry) win over this file wherever they differ. This revision applies them: the re-cast targets (decisions §1.3), the two absorbed medium quests (§2.2, §2.3), the cough's foreshadowing (§2.1), the one purse rule (§3), camp matching (§4), the engine names (§5.1), the Frosthollow line and the stage entries (§7.2, §7.3), the join caps (§8.2), `closure` (§9, P1-15), Corwin's voice (P2-20) and no resident speech in `story.tell` banners (P2-22).

**What was checked (2026-11-22, read only; nothing in the repo or the world was written):**
- Every resident id below was read from `live.npcs` / `live.npc_state` / `live.npc_voice` / `live.mantles` in `ratw_ridgemere3` through `tools/world_db.connect('ridgemere3', 'owner')` inside `SET TRANSACTION READ ONLY` and a rollback: **alive, placed** (home, and work where there is one), with home and work places on the reachable list (`world.links` ends plus `world.cells`). The query is decisions §1.1's.
- **Dialect.** A speaker's dialect is `live.npc_voice.home_region` with its accent. **Accent `none` means plain speech**, whatever the home region or the mantle's dialect: the talk code skips the dialect layer for accent `none` (`Core/RatwTalk.cpp:2478`). Fenshore-raised speakers speak mirebridge speech, Ridgemere-raised speakers speak plain. This revision found **twelve** speakers written in a dialect they don't speak (accent `none`): Corwin Bilgestack (critic P2-20), and also Orm Sootgate, Osric Ropeworth, Ivy Stonehollow, Brenna Slaggate, Vigdis Rainmore, Aster Cornley, Brisk Lakemere, Karin Wetridge, Jasper Greymore, Rowan Cragpeak and Philippa Pitchstack. All twelve now speak plain.
- No giver and no `npc.offer` holder is campaign cast, a keeper, an innkeeper or a look-alike, and nobody holds a second pack quest (`givers.tsv`, the giver check of decisions §1.2.7).
- Every region box was sampled tile by tile against `world.terrain_chunks` (128-tile chunks, 256-tile cells) and `Data/Terrain/terrain.json`: every box is open ground or road, with no deep water. The new boxes of this revision are listed with their open-tile counts.
- Door positions come from `world.links`. Purses come from the 2026-11-22 22:34 UTC checkpoint (decisions §3.1); the open projects (proj-1 to proj-3, each at local (8, 8)) from the 23:01 checkpoint.
- Event fields and the mission flag come from `Data/Storylines/trigger-catalogue.json`. Story camp ids come from `Core/RatwRoads.cpp` (`World::storyBandits`). Item ids come from `Data/Items/items.json`.

**Contents**
- §0 Index
- §1 Shared rules for all lines (shape, branches, scaling, fights, rewards, campaign feed, talk)
- §2 Facts found while designing that change how quests must be written
- §3–§12 The ten first lines
- §12a The Black Hearths (the Frosthollow line)
- §12b The stage pack's large quests
- §13 Engine needs: the items these lines cite, by the names of decisions §5.1
- §14 Giver budget and checks before approval

---

## 0. Index

| # | id | Title | Giver (id) | Town / region | Villain or rival | Campaign touch |
|---|---|---|---|---|---|---|
| 1 | `lq-salt-road` | The Salt Road | Orm Sootgate `cty_orm_sootgate` | Saltwell, the Saltpan, the Red Waste → Ridgemere | Osric Ropeworth `rm_osric_ropeworth`, a salt hoarder | grey paw-mark receipts (Ch 5); `nh_voice_saltwell`, `nh_threat` |
| 2 | `lq-mast-cedars` | The Mast Cedars | Ivy Stonehollow `tn_fh_ivy_stonehollow` | Oakhaven, the Rainwood, Cedar Sound → the Wharf | Brenna Slaggate `rm_brenna_slaggate`, a Grayrock shipwright | Oakhaven's common right (Ch 5); `nh_voice_oakhaven` |
| 3 | `lq-old-brown` | The Old Brown of the Greyteeth | Jessamy Greyford `cty_jessamy_greyford` | Coldwater's Greyteeth fold, Greytarn, the Upper hearths | grey-cloaked rustlers; the bear | the Captain's northern band (Ch 3); `nh_voice_coldwater`, `nh_threat` |
| 4 | `lq-clipped-silver` | Clipped Silver | Pike Chalkley `tn_lf_pike_chalkley` | Larkfield → the Market Hall → the Commons → the Kestrel workings | Vigdis Rainmore `rm_vigdis_rainmore`, a coiner in debt; a Counting House beam | the factors' new moon (Ch 2); `nh_voice_larkfield`. Chapter 1 is the old medium Q11 (the Short-Weighed Sheaves). |
| 5 | `lq-blight-and-boar` | Blight and Boar | Sloe Harvestby `cty_sloe_harvestby` | Southmeadow, the Sunfields, Southwood | the blight; the boar | the harvest (Ch 7); `nh_voice_southmeadow` |
| 6 | `lq-runnel-fever` | The Runnel Fever | Theodora Wickley `rm_theodora_wickley` | Ridgemere: the Sump, Cistern Court, the Wharf | Rupert Fogton `rm_rupert_fogton`, a draught-seller; a careless tanner | the Sump and the damp cough (Ch 1–4), the Residence's cough (`nh_b_cough`); `nh_voice_ridgemere`. The city's one cough line (the old medium Q5 is withdrawn into it). |
| 7 | `lq-wreckers-light` | The Wreckers' Light | Linden Sedgeditch `cty_linden_sedgeditch` | the North Headland, North Cove, the Wharf | Ilse Coalley `rm_ilse_coalley`, a lamplighter who lights a false lamp | the wharf dead (Ch 5), the false lights (Ch 6); `nh_voice_ridgemere`. Chapter 1 takes in the old medium Q7 (the Wrong Light on the Headland). |
| 8 | `lq-long-run` | The Long Run | Grete Dimmock `rmx_grete_dimmock` | every square, from Oakhaven on the Sound to Saltwell on the Waste | Orm Carlisle `rmx_orm_carlisle`, the Carters' Yard (a friendly rival) | a rehearsal of the Howl Line (Ch 4, Ch 9); `nh_realm`, every `nh_voice_*` |
| 9 | `lq-eel-famine` | The Eel Famine | Ivo Sootridge `cty_ivo_sootridge` | Fenshore, Withy Creek, Rivermouth, the river weir | the Fish Hall's weir and its guards | float lamps and the drowned (Ch 6); `nh_voice_fenshore`, `nh_names` |
| 10 | `lq-vanishing-thief` | The Thief Who Wasn't There | Petra Vance `rm_petra_vance` | Ridgemere: the Heights, the Sunken Street, the Sump | a lender's rates; a Sump pup who isn't Quickened at all | Warden attention (Ch 5–9), Sump debts (Ch 2); `nh_voice_ridgemere` |
| 11 | `lq-black-hearths` | The Black Hearths | Marl Craghearth `cty_marl_craghearth` | Frosthollow, the Upper hearths, Kestrel Tarn → the Timberland Charcoal Store; the Greenholt hearths | a band of charcoal thieves who damp the clamps by night | `nh_voice_frosthollow`, `nh_threat` (no Deepvein, no mine, no oil: §12a) |

| # | Stage quest (`nh-pack.stage`) | Giver | Phase |
|---|---|---|---|
| S1 | `st-patrol-round` The Patrolman's Round | Bertil Saltcot `rmx_bertil_saltcot` | `s12`, `s3` |
| S2 | `st-camp-bounty` A Camp Come Back | Frederick Rainham `rm_frederick_rainham` | `s3` to `s9` |
| S3 | `st-midsummer-road` The Midsummer Road | Cedric Mudley `rm_cedric_mudley` | `s4` |
| S4 | `st-twelve-bounds` Twelve Bounds in a Day | Ada Coalstack `cty_ada_coalstack` | `s56` |
| S5 | `st-post-night` A Night on the Line | Beatha Foggate `rm_beatha_foggate` | `s9` (until the Moot night is called) |

Every line has **three chapters and a reckoning**: two chapters a wolf or a party plays at their own pace, a third that splits into two or three roads, and a shared **reckoning** that turns the roads the wolves chose into one lasting state of the world.

---

## 1. Shared rules for all lines

### 1.1 Program shape
- **One program per line.** The id is the line's id (`lq-salt-road`, …), the file is `Data/Storylines/ridgemere3/quests-large/<id>.json` (never `programs/`, which three tests count), version 2, the top-level `prefix` is `lq` (engine-spec L2's naming rule), and the audience is `{"kind":"open","cap":5000}`. A wolf joins by taking the first chapter.
- **Phases are the world's state, not a wolf's progress:**
  1. `open`: chapters 1 and 2 are on offer;
  2. `reckoning`: entered when the line's shared goal is met; chapter 3's road quests are offered; the tally window runs;
  3. `after_<road>`: one phase per result, with that road's `npc.line`s, board posts and talk key. Chapters 1 and 2 stay on offer as weekly repeats ("the road needs keeping"), so the line never empties;
  4. on `season begins`, an `after_*` phase goes back to `open` with a new visit: raids, blight and thieves come back each season, at the cost of a fresh shared goal.
- **Chapter 1 has a giver and a board** (`giver` + `board`), so every wolf can find it.
- **Chapters 2 and 3 are journal offers** (`quest.offer {quest, player:"$actor"}` from a `quest completed` trigger with `audience: participant`), with no giver. That avoids the giver leak (gaps.md A3): a giver offer shows to every wolf with no quest view. The talk in those chapters runs through `conversation` objectives (which cost no hub room) and a few `npc.offer` steps.
- **The roads of chapter 3 are separate journal quests** (two or three per line). The wolf accepts one; a `quest accepted` trigger withdraws the others for that wolf (`quest.fail`, journal text "another road taken") and adds the wolf's weight to that road's tally. With C6 `requires` or L5 `quest.withdraw` the withdrawal is cleaner.
- **Closure (J3; decisions §9, P1-15).** Every road quest of chapter 3 carries `"closure": true`, with `repeat: "once"` and scope player or party, as J3 requires. J3 (answer (b), 2026-11-22: yes, capped) pays one receipt per closure quest a wolf finishes, at most 4 a wolf a program, and reads `true` as `"line"`. The roads exclude each other ("another road taken"), and an optional third quest carries **no** `closure`, so a wolf's line counts **once** toward the Quickening's `closedStories` whichever road it took. Each program has at most 3 closure quests, inside the cap of 4. It is inert until J3 (Phase 4).

### 1.2 The reckoning: many wolves, one world
- Each road has a story number, `t_<road>`. Accepting a road adds 1, plus 1 more if the wolf did chapter 2 in a party (a party's choice weighs more because it took more wolves).
- The **first** acceptance of any road schedules the signal `reckon` 3 game days later (half a real day).
- At `reckon` an `if`/`elif` ladder sets the world text `lq_<line>` to the road with the most weight. Ties and "nobody chose" go to each line's stated default.
- Wolves on the losing road still finish their own quest and keep their own reward and deed. Only the world state follows the majority.
- This copies the campaign's §2.4 rule: totals updated at the moment of choice, no `each`, so the 500 cap never bites.

### 1.3 Scaling (5 wolves today, hundreds later; decisions §8.2)
- **Join caps.** `join.cap` is a literal, so no operand is proposed for it.
  - **Chapter 1** (talk, deliveries, per wolf; no held resident, no placed item, no spawn): `join.cap` **40**. Chapter 1s that used to place a clue item now score a story signal on a small region instead, so they stay at 40.
  - **Fight chapters** keep `join.cap` **12**, and spread their waves over **two spawn regions, used in turn** (a story counter `wave_turn` flips 0/1 on each spawn), so two groups of six fight at once. The same holds for a chapter that places a unique piece or holds a resident.
  - **Talk-only chapter 2s** (no spawn, no placed item): `join.cap` 40.
  - **Road quests** (journal offers) have no cap.
- **Shared goals grow with the wolves who play.** Each line has a story number goal `g` = base + k × helpers. A wolf's first contribution adds them to the story `wolves` list `helpers` (`in list` false → `list add`, `add g k`), exactly as the campaign's `ch_goal`. A quest goal cannot be read from a variable (story-format §6, "Not built"), so shared bars are variables shown as trackers, not story-scope quest goals. With C2 (`audience` operand) the goal becomes `max(base, k × active)`.
- **Fights scale with wolves present.** Every spawn is an `if`/`elif` ladder on `count in region`: 1 wolf → 2 bandits, 2 → 3, 3–4 → 4, 5+ → 6 (the cap). A crowd gets several waves through a `schedule` of 20 game minutes, up to a stated number, alternating between the chapter's two regions.
- **Repeats.** `repeat: "once"` holds for chapter 1 and for every road. Chapter 2 is `weekly`, so the next season's wolves can play it.
- **Unique pieces** (the pledges in Q10, the false lamp in Q7) are re-placed by a trigger while the phase lasts, so a crowd cannot use them up.

### 1.4 Fights and camps (decisions §4; checked against the code)
- The only fightable foes today are bandits: `story.area.spawn {region, count 1-6}` and `bandits.call {player, count 1-6}`.
- **A story spawn's camp id is `camp_story_<n>`, not `camp_<cell>`** (`World::storyBandits`, `Core/RatwRoads.cpp:1622`). So every story wave is matched by **`cell` plus the story switch `spawned_<region>`**: the switch is turned on when the chapter calls `story.area.spawn` in that region, and turned off when the wave scores. A line **never** matches `camp_w_ridgemere_<cell>`, not even for a natural camp: a natural camp in the same cell is matched by its `cell` too, and that is right, since either way the road is clear (decisions §4.4).
- **`camp cleared` is not a mission event**, so it cannot be a quest objective. Every "break the camp" is a **trigger** on `camp cleared` (match `cell`, switch on) that runs `objective.complete` for each participant in the region (`each over participants` + `wolf in region {who:$each}`), or adds to the line's shared bar. `bandit falls` **is** a mission event, so "bring down N of them" is a plain objective matched by `cell`.
- **From L2 / L3 (engine Phase 2)** a new version of each program names its camps (`"camp": "lq_salt_flats"` → `camp_story_lq_salt_flats`; the name begins with the program's prefix `lq_`, 3–30 of `[a-z0-9_]`; while the camp stands a second spawn with that name adds to it, and once it is cleared the name may be reused), and `camp cleared` becomes an objective matched by exact `target`.
- Residents are never attacked: attacking one is an assault, and a warrant follows. A resident villain is beaten by proof, talk and choices.

### 1.5 Rewards: the one purse rule (decisions §3)
**The purses** (checkpoint 2026-11-22 22:34 UTC, `society.accounts`):

| Purse | Pennies | Day cap | Used by |
|---|---|---|---|
| `town:saltwell` (`stores:saltwell`) | 224 | 22 | Q1 |
| `town:oakhaven` | 247 | 24 | Q2, S5 (Oakhaven road post) |
| `town:coldwater` | 212 | 21 | Q3 |
| `town:larkfield` | 248 | 24 | Q4 |
| `town:southmeadow` | 236 | 23 | Q5 |
| `town:ridgemere` (the treasury) | 1,355 | 135 | Q6, Q7, Q8, Q10, every till payment before SQ2, S1–S4, S5 (waystation) |
| `town:fenshore` | 213 | 21 | Q9 |
| `town:frosthollow` | 247 | 24 | Q11, S5 (Upper Mere and Pass posts) |

**Tills are not named before SQ2.** A till holds money, but a story can't name one until SQ2 ships (engine Phase 1): today `check_purse` refuses `till:job:<id>`. Until then every payment that is "the shop paying" names the treasury (a city shop) or the town store, under the same counter rule, and its reason text still names the shop ("Vane & Daughters pays it back"). When SQ2 lands, a new version of the program swaps the purse to the till:

| Shop's payment | Today | After SQ2 | Till balance |
|---|---|---|---|
| Vane & Daughters (Q1) | `town:ridgemere` | `till:job:rm_osric_ropeworth` | 125 (day cap 12) |
| Grayrock Shipwrights (Q2) | `town:ridgemere` | `till:job:rm_brenna_slaggate` | 156 (15) |
| Silver & Rain (Q4) | `town:ridgemere` | `till:job:rm_vigdis_rainmore` | 112 (11) |
| Hollis Physic (Q6) | `town:ridgemere` | `till:job:rm_rupert_fogton` | 90 (9) |
| the Fish Hall (Q9) | `town:ridgemere` | `till:job:rm_corra_coalcot` | 141 (14) |
| the Counting House chit (Q4 ch 1) | `town:ridgemere` | `till:job:rm_eleanor_saltcot` (the Greatly Bank) | 161 (16) |

The Lamp Store's till `till:job:rm_ilse_coalley` is never used: no payment goes through the villain's name (decisions §2.2).

**The counters.** Each purse has a world number `coin_<purse>_day` (`coin_ridgemere_day`, `coin_saltwell_day`, …, and after SQ2 `coin_till_osric_ropeworth_day` and the rest) and a world switch `coin_<purse>_ok`. They are declared once with `world-var declare` and owned by the campaign's small always-on program `nh-purses` (campaign §2.11): it zeroes the counters at `day begins`, turns `coin_<purse>_ok` off when the purse falls past 60 and on again at 80. These programs **only read and add**, declaring the same names and types. Every coin payment is this pattern:
```json
{"do":"if","if":{"all":[
   {"test":"switch","var":"coin_saltwell_ok","is":true},
   {"test":"compare","left":{"var":"coin_saltwell_day"},"op":"<=","right":{"value":10}},
   {"test":"compare","left":{"var":"lq_coin","of":"$actor"},"op":"<=","right":{"value":23}}]},
 "then":[{"do":"effect","effect":{"kind":"character.grant","player":"$actor","coins":12,"purse":"town:saltwell",
           "reason":"the Saltwell stores pay their thanks","ifRefused":"skip"}},
         {"do":"add","var":"coin_saltwell_day","value":12},
         {"do":"add","var":"lq_coin","of":"$actor","value":12}],
 "else":[{"do":"effect","effect":{"kind":"item.give","player":"$actor","item":"bread","quantity":2}},
         {"do":"set","var":"lq_thin","of":"$actor","value":true}]}
```
- The literal in the first `compare` is the day cap less the amount (22 − 12 = 10 here), and in the second the line's 35 less the amount. Written as literals, the pattern reads the same in every program and survives a change of amount by one edit.
- `lq_coin` is a **player number in each program**: what this line has paid that wolf in its life. It keeps every line inside **35 pennies a wolf** however often its weekly chapters are replayed, and every chapter pays **at most 12**.
- The `else` pays in kind (`bread` ×2, or the line's own item where named), and the giver's thanks plays its "thin purse" variant, gated on the player switch `lq_thin` (with N2, `player.story.lq_thin`; until then a window line on the giver reads the realm key `lq_<line>_thin`, which the trigger sets for the day). The deed, the practice, the item and the campaign feed land as usual.
- **Restitution** from a villain's till (the "honest" roads) is at most 10 a wolf and at most 60 in all, per the line's story counter `restitution`, inside the chapter's 12 and that purse's day cap.
- **Size.** A finished line pays at most 35 pennies a wolf, plus one good item, two practices and a deed. Rough figures: a carter earns 12 for a contract, and a steel sword sells for 90. "Big" here means the best deeds in the realm, real gear and a lasting mark on the world, because the purses are small and shared with the campaign.
- **Deeds** are written as the game speaks them: lower case, a verb phrase, no full stop (talk survey §3). Each line gives at most one deed per chapter and one per road.

### 1.6 What the lines feed in the campaign (world variables; campaign §7)
Each program declares the world variables it touches, with the same name and type as the campaign. It writes them only from v2 trigger actions, never from rewards.

| Variable | Written by these lines | Amount |
|---|---|---|
| `nh_realm` | the last chapter done, per wolf | 8 × distance weight: ×1 Ridgemere/Oakhaven (8), ×1.5 Coldwater/Larkfield/Frosthollow (12), ×2 Saltwell/Southmeadow/Fenshore/Mirebridge (16) |
| `nh_voice_<town>` | the same moment, to the town the line serves | the same amount; the "bad" roads give a quarter |
| `nh_threat` | each story camp cleared (−1); a line's bandit strand broken (−3) | never below 0 (`clamp`) |
| `nh_names` | Q9 only, while `nh_stage` is 4–6 | +1 per wolf, once |
| `coin_<purse>_day` | every coin payment (§1.5) | the amount paid |

- **Read-only.** The lines read `nh_stage`, `coin_<purse>_ok` and the campaign's branch results (`nh_b_*`) to change a line's words. They never write a branch, `nh_stage`, `nh_surplus`, a post or a Keeper.
- **Each line's own result** is a world text `lq_<line>`, declared once with `world-var declare`, so packs, the campaign and later stories can read it. These are lasting states.
- **Personal credit in the running chapter:** C12a `story.notify` to the current `ninth-howl.<n>` with `player: $actor` (or J2 `p_voice`). Until then, packs credit towns and the realm only.

### 1.7 Talk
- Every gossip and helpful line below goes into `Data/Talk/dialogue/resident.tree`, in the `gossip` or `news` node, as `say[world.mem.<key>=<value> here.region=<town> self.dialect=<d> priority=2]`. Recaps go into `inn.tree` `greet`.
- The keys are written by C8 `talk.remember` (`scope: realm` unless said) at each phase entry. Until C8 exists, the DM seeds a key by hand when a phase opens. The lines are valid today (talk survey §0), and they stay silent until a key is set, so `talk_scorecard_quick` and `talk_golden` don't move.
- Library lines name no one (roles only: "the salt carter", "an old lamplighter") and stay under 160 characters. Weather and time words are avoided. Where a role's resident shares a first name with campaign cast, no library line names either (decisions §1.2.5).
- **Where residents speak to a wolf** (decisions P2-22): only in the dialogue window. That is the giver's `say` / `thanks`; `npc.offer` replies; `npc.line`; C5 `npc.greet`; C6 `offer.during`; a scene (each speaker in their own panel); or the resident's own **window line**, `say[self.id=<id> world.mem.<key>=<value> priority=4]` in the `hub` node of `resident.tree` (or `watch.tree` for the Watch). **No `story.tell` carries `from` with a resident's words.** A `story.tell` here is plain narration: no `from`, no quoted speech. Notices that a resident signs go on a board (`board.post`), signed in the text.
- **Conversation targets** hold no hub item. What they say when the wolf comes to them is their window line, gated on the line's talk key for the phase (every wolf hears it while the line is open, which is harmless). With N2, it also reads the wolf's own step (`player.story.<var>`).
- Quest words (`say`, `thanks`, `npc.offer.reply`) are the giver's own and use `#greet#`, `#address#`, `#thanks#` and `#oath#`, so the giver's mantle fills them. A dialect's set phrase is written out only by a speaker raised in that dialect **with an accent** (light or broad). A speaker with accent `none` speaks plain (§0's note).
- `ask`, `accept`, `decline`, `handIn` and `npc.offer.words` are **the wolf's** words. The game does not expand them, so they are plain speech. The giver answers a decline with silence, and the window goes back to the hub.
- `npc.line` (phase entry, raw, ≤ 200 characters) and window lines are never expanded, so they are written out in the speaker's dialect.

---

## 2. Facts found while designing that change how quests must be written

1. **Story camps are `camp_story_<n>`** (§1.4). Story waves match `cell` plus a `spawned_` switch, never `camp_w_ridgemere_<cell>`.
2. **`camp cleared`, `caravan arrives`, `ship lost`, `project built` and `hour` are not mission events.** They work only through triggers and `objective.complete` (until L3).
3. **The ships are not the Fair Wind, Grey Tern, Cormorant and Long Tide.** The checkpoint has `s21` the Fair Wind, `s22` the Lark, `s23` the Salt Rose, `s24` the Wavecutter and `s25` the Petrel (all port `ridgemere`). Campaign text naming "the Grey Tern" should be checked against the ship table before it ships.
4. **Several farms stand far from the town they serve.** The Greyteeth sheep barn's door is at `w_ridgemere_11_5` (14, 249) in the Larkdowns. The Southmeadow vineyard press is at `9_12` (98, 105), Withy Creek's eelhouse at `4_13` (35, 107), North Cove's shed at `0_4` (157, 49) on Cedar Sound, the Saltpan store at `13_12` (96, 243), and the Greenholt hearths in the Rainwood at `4_8` (236, 77). Quests walk wolves to the doors, not to the names.
5. **Tills hold money, but a story can't name one until SQ2.** `till:job:<founder>` holds a workplace's cash, and the 2026-11-22 balances are in §1.5. Today's validator refuses the three-part purse, so every "the shop pays" road names the treasury or the town store until SQ2 ships, and then moves to the till in a new version.
6. **`npc.walk` takes local tile coordinates in the named cell** and can walk between cells (`canWalkBetween`, `Core/RatwGameStorylines.cpp`). A resident escorted across the Waste is possible today.
7. **The Kestrel lead and silver mine (`w_ridgemere_8_2`) has no workers.** It is an empty working, a good den.
8. **Hester Greyby (14) has no parents in `live.npc_relations`** and lives alone in `r2_slop_rooms`, with Ivy Stonehollow (Q2's giver) upstairs. Q10 uses that.
9. **Bears are rare** (`rarity 1`, forest or rock) **and no effect can stage an animal.** A quest that says "kill the bear" may wait for weeks. L1 (Phase 5) fixes it; until then the bear is an optional objective.
10. **Accent `none` is plain speech** (§0's note). Twelve speakers of the first draft were written in a home dialect they no longer speak.
11. **The Lamp Store has four lamplighters:** Ilse Coalley, Corwin Bilgestack, Rupert Cindergate and Edda Gullford. Edda gives medium Q1 (`mq-sump-lamps`), whose oil-drainer is Piet Lumley; no line of Q1 names or describes Ilse. The two oil stories never meet (§9).
12. **Three of the Greenholt hearths' workers share a first name with campaign cast** (Fell Cragdrift, Kell Greenshaw, Ambrose Gullton), and the Timberland Charcoal Store's keeper is an Ilse. Q11 names none of them: the Greenholt hearths appear as a place and a road, and the store speaks through Arne Ashton.

---
## 3. The Salt Road (`lq-salt-road`)

| | |
|---|---|
| Tier | large: 3 chapters + a reckoning; one party needs about 3–5 real days |
| Program | `lq-salt-road`, open, cap 5000, prefix `lq` |
| Towns and places | **Saltwell** (square `w_ridgemere_12_11` (220, 116); board `board:saltwell`); the **Saltpan workings** `fs_salt_saltpan_store` (door `13_12` (96, 243)); the Red Waste road (`12_12`, `11_12`); the Ochre cut (`11_11`); **Ridgemere**: Vane & Daughters `rm_vane_daughters` (door `1_0` (101, 250)), the North Wharf Smokehouse `rm_the_north_wharf_smokehouse`, the Market Hall `r2_the_market_hall`, the Records Office `r2_the_records_office` |
| Giver | **Orm Sootgate** `cty_orm_sootgate`: m 21, the Saltwell carter, works on the square by the board (221, 117); Saltwell-born but **plain speech** (home region saltwell, accent none; mantle sw108, "the sky-arguing carter"); short-tempered and warm, argues with the weather out loud. He carts the salt that gets robbed, he stands beside the board every day, and he "hopes a child of the family will be taken into service with one of the great Houses", which makes a House provisioner's greed personal. |
| Reward purse | `town:saltwell` (224, day cap 22); Vane & Daughters' payments from `town:ridgemere` until SQ2 (§1.5) |

**Premise.** Salt carts out of the Saltpan are robbed on the brine flats and in the red dunes, and only the salt is ever taken, never the oxen. In Ridgemere a provisioner at Vane & Daughters buys every sack that comes to his door with no carter, and the city's "salt short" price is his doing. Ride with the salt, break the raiders, and decide whether the city's salt goes before the Market Board, stays bought, or is sold off the cart.

**Cast**

| Role | Resident (id) | Voice | Hub item |
|---|---|---|---|
| hero, giver | Orm Sootgate `cty_orm_sootgate` | plain | quest `brackish`; `npc.offer` `lq_salt_wagon`; road choice `lq_salt_cart` |
| witness | Kite Bogditch `tn_mb_kite_bogditch` (m 37, works the salt at the Saltpan; gives medium `mq-crossroads-names`) | saltwell (light) | **conversation target only**: counts the sacks out and back; chatty, ambitious |
| witness | Fern Merewick `cty_fern_merewick` (f 38, salt raker; gives medium `mq-lost-raker`) | saltwell (light) | **conversation target only**: "went to Ridgemere to make a fortune and came back with nothing": she worked at Vane & Daughters |
| villain | Osric Ropeworth `rm_osric_ropeworth` (m 27, provisioner at Vane & Daughters) | plain (frosthollow, accent none) | two `npc.offer`s in sequence; short-tempered and sly; "keeps the stores counted to the last sack, and knows who is behind on paying" |
| city witness | Rosamund Coalford `rm_rosamund_coalford` (f 55, runs the North Wharf Smokehouse; gives small `sq-smokehouse-wood`) | plain (broad) | **conversation and gift target only**: her fish spoil for want of salt |
| arbiter | Fenwick Mossdale `rmx_fenwick_mossdale` (m 33, warden of the Market Hall) | coldwater (broad) | one `npc.offer` `lq_salt_board`: the Market Board |
| foes | the salt raiders: `story.area.spawn` bands | — | with C10 (`foe.add`), a leader foe "the Brine Captain" |

**Regions**

| id | cell | box (local) | what |
|---|---|---|---|
| `salt_yard` | `w_ridgemere_13_12` | (84, 226)–(108, 250) | the Saltpan yard by the store door |
| `brine_flats` | `w_ridgemere_12_12` | (180, 24)–(199, 43) | the road over the salt flats (99 road tiles) |
| `brine_flats_b` | `w_ridgemere_12_12` | (200, 44)–(219, 63) | further down the same road (399 open, 97 road): the second spawn region, used in turn |
| `saltwell_sq` | `w_ridgemere_12_11` | (208, 104)–(232, 126) | the square and board |
| `dune_road` | `w_ridgemere_11_12` | (192, 108)–(211, 127) | the road through the red dunes |
| `ochre_cut` | `w_ridgemere_11_11` | (96, 24)–(115, 43) | the road through the sandstone cut |

**Chapters and objectives**

*Chapter 1, `brackish`, "Brackish Wells".* Scope player; giver Orm, board `saltwell`; repeat once; `join.cap` 12 (a fight chapter: waves alternate between `brine_flats` and `brine_flats_b`).

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `kite` | Ask the sack-counter at the Saltpan what goes missing. | conversation | target `tn_mb_kite_bogditch` | 1 | his window line (below) |
| `fern` | Ask the raker who came home from Ridgemere with nothing. | conversation | target `cty_fern_merewick` | 1 | her window line (below) |
| `flats` | Walk the brine flats after dark. | story signal | `flats_dark` | 1 | trigger: `region entered` `brine_flats` or `brine_flats_b` with `hour between 20 4` → `objective.complete` |
| `raiders` | Drive off the raiders on the flats. | bandit falls | cell `w_ridgemere_12_12` | 3 | `after: [flats]`. The spawn ladder fires on the `flats_dark` trigger, in the region of this turn |
| `tally` | Pick up what the raiders dropped. | item picked up | item `paper`, cell `w_ridgemere_12_12` | 1 | `after: [raiders]`; `hidden`. Trigger on `camp cleared` (cell 12_12, switch `spawned_brine_flats` or `spawned_brine_flats_b` on): `item.place paper 1` at (190, 34) or (210, 54), the cleared region's centre |
| hand-in | to Orm | | | | `handIn: true` |

*Chapter 2, `ride`, "Ride With the Salt".* Scope player; a journal offer when `brackish` is done (a board copy for anyone after the first wagon has run); repeat weekly; `join.cap` 12.

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `loaded` | Be at the Saltpan yard when the wagon loads. | story signal | `wagon_loaded` | 1 | at the wagon's start: `each over participants`, `wolf in region salt_yard` |
| `fought` | Fight off a band on the wagon's road. | story signal | `wagon_fought` | 1 | trigger on `bandit falls` (participant) whose `cell` is any of 12_12, 11_12, 11_11 (`any` of three `field` tests) |
| `through` | See the wagon through the Ochre cut. | story signal | `wagon_through` | 1 | on `scene arrived` cue `ochre`: each participant in `ochre_cut` or within 3 tiles of Orm |

**The wagon itself** (a world event, not a quest; anyone may ride):
- **When it runs.** The first wagon starts 1 game day after the first `brackish` is done. After that, `schedule` signal `wagon` every 7 game days (game clock, repeat), always at the 8:00 `hour`. The DM can fire `wagon` from the console.
- **Stages.** `npc.protect cty_orm_sootgate`, then `npc.walk` Orm (pace walk) from the yard to the centre of each region in turn, with cues `flats`, `dunes`, `ochre` and `home` (the last back to the Saltwell square).
- **Each stage.** On `scene arrived` with a cue, run the spawn ladder and `start timer stage`. The two regions used in turn are the stage's own and the next one on the road (flats ↔ `brine_flats_b`, dunes ↔ flats, ochre ↔ dunes), so a crowd splits into two fights. The stage **holds** if `camp cleared` (that cell, switch on) comes within 2 game hours (`since stage` ≤ 0.084 days). If not, the band takes a sack: `subtract sacks 1`. Then Orm walks on.
- **After the last stage.** `add lq_salt_sacks {var: sacks}` (a world number); `sacks` resets to 3; `npc.unprotect`.
- **Shared goal.** `lq_salt_sacks` ≥ `g` (base 6, + 1 per helper) → `enter reckoning`. The tracker reads "Salt through the Waste".

*Chapter 3, "Who Buys Salt Dear".* Three road quests, each `closure: true`, repeat once, no cap. The `quest completed` of `ride` offers all three by journal. Their common first objectives are below, and each road then adds its own.

| id | label | on | match | goal |
|---|---|---|---|---|
| `door` | Find the shop the robbed salt goes to. | enter place | target `rm_vane_daughters` | 1 |
| `smoke` | Ask the smokehouse why the fish spoil. | conversation | target `rm_rosamund_coalford` | 1 |
| `string` | Show the provisioner the Saltpan string on his sacks. | dialogue choice | item `rm_osric_ropeworth`, detail `lq_salt_osric1` | 1 |
| `ledger` (optional, hidden) | Copy his buying out of the Records Office rolls. | archive work | detail `ridgemere` | 1 |

The three roads:

| Road quest | Its last objective | World result |
|---|---|---|
| `salt_board` "Before the Market Board" | `dialogue choice` item `rmx_fenwick_mossdale`, detail `lq_salt_board` | `lq_salt = board` (**the default**) |
| `salt_bought` "Salt Stays Dear" | `dialogue choice` item `rm_osric_ropeworth`, detail `lq_salt_bought` | `lq_salt = bought` |
| `salt_cart` "Off the Cart" | `dialogue choice` item `cty_orm_sootgate`, detail `lq_salt_cart` (the choice sits on the giver), then a `gift` of `salt` ≥ 2 to `rm_rosamund_coalford` | `lq_salt = cart` |

**The spawn ladder** (the pattern every line uses; shown once):
```json
{"id":"flats_dark","on":"region entered","match":{"target":"brine_flats"},"audience":"participant","once":"player",
 "if":{"test":"hour between","from":20,"to":4},
 "actions":[
  {"do":"effect","effect":{"kind":"objective.complete","quest":"brackish","objective":"flats","player":"$actor"}},
  {"do":"if","if":{"test":"switch","var":"spawned_brine_flats","is":false},"then":[
    {"do":"turn on","var":"spawned_brine_flats"},
    {"do":"if","if":{"test":"count in region","region":"brine_flats","op":">=","value":5},
     "then":[{"do":"effect","effect":{"kind":"story.area.spawn","region":"brine_flats","count":6}}],
     "elif":[{"if":{"test":"count in region","region":"brine_flats","op":">=","value":3},"then":[{"do":"effect","effect":{"kind":"story.area.spawn","region":"brine_flats","count":4}}]},
             {"if":{"test":"count in region","region":"brine_flats","op":">=","value":2},"then":[{"do":"effect","effect":{"kind":"story.area.spawn","region":"brine_flats","count":3}}]}],
     "else":[{"do":"effect","effect":{"kind":"story.area.spawn","region":"brine_flats","count":2}}]}]}]}
```
- The twin trigger `flats_dark_b` is the same on `brine_flats_b` with its own switch. When a region's switch is already on (a wave stands there), the trigger spawns in the other region instead, if its switch is off: that is "used in turn".
- A `camp cleared` trigger (match `cell` `w_ridgemere_12_12`, if either switch is on) turns that switch off, places the tally and runs `subtract nh_threat 1` then `clamp nh_threat 0 100`.
- `once: "player"` keeps one wolf from re-triggering on each step in and out.
- With L2 the spawns carry `"camp": "lq_salt_flats"` and `"lq_salt_flats_b"`.

**Branches and lasting consequences** (the reckoning, §1.2; default `board`):
- **board.** `prices.reset {market: ridgemere, words: "salt back at the old price"}` (B; without a builder, `times.nudge ridgemere -20 days 14`). Also `event.cancel {market: ridgemere, event: salt_short}`.
  - Restitution: each `salt_board` finisher gets 8 "Vane & Daughters pays it back" (today from `town:ridgemere`; after SQ2 from `till:job:rm_osric_ropeworth`), story counter `restitution` capped at 60.
  - Boards: Ridgemere *"THE MARKET BOARD: Saltpan salt sells at the old price from this Marketday. A Northgate provisioner will answer for robbed sacks."*; Saltwell *"The salt road holds. Wagons load on wagon day. Water and shade to every wolf who rode."*
  - Talk key `lq_salt=board`.
- **bought.** `event.add {market: ridgemere, event: salt_short, days: 7}`, re-added by a weekly `schedule` for the rest of the season.
  - Each `salt_bought` wolf gets 10 "the provisioner's coin" (today `town:ridgemere`; after SQ2 Osric's till) and the deed "took a provisioner's coin to keep the city's salt dear". Gossip about that deed follows the wolf.
  - Talk key `lq_salt=bought`. Saltwell's Keeper hears of it: `nh_voice_saltwell` gets only a quarter share.
- **cart.** `times.nudge {market: saltwell, points: 20, days: 28, words: "Saltwell salt sold off the cart"}`.
  - Orm's cart sells on Ridgemere's Marketday: `npc.walk` to the Rag Market (`w_ridgemere_0_1`) on each `day begins` Marketday, and home again.
  - Talk key `lq_salt=cart`.
- **Read by medium Q18** `mq-waste-road-salt` (Tor Chalkfield's Southmeadow wagon, decisions §2.4), read only: the raiders count as **broken** whenever `lq_salt` is `board`, `bought` or `cart` (the reckoning was reached), and the medium is offered only while `lq_salt` is unset or `raided`. That medium writes nothing to `lq_salt`.
- **Lasting:** the world text `lq_salt`; the price state; the deed gossip. **The raiders' tally carries a grey paw-mark.** If `nh_stage ≥ 5` at the reckoning, a `journal.note` to each finisher reads: "The paw-mark on the salt tally matches the one on the Grayrock toll receipts." That links to the campaign's Ch 5.

**Repeat and limits.** Chapter 1 once; chapter 2 weekly; chapter 3 once. The wagon runs weekly for ever. The shared goal resets on each season's new visit.

**Rewards** (§1.5: at most 12 a chapter, 35 a line, by `lq_coin`)

| When | Reward |
|---|---|
| `brackish` | deed "drove the salt raiders off the brine flats"; `story.practice watch`; `item.give salt 2` |
| `ride` | first time: 6 from `town:saltwell`, deed "rode the salt wagon through the red dunes", `story.practice travel`; each later ride: 3 from `town:saltwell` while `lq_coin` allows; `item.give salt 1` every time |
| a road done | `item.give waterskin 1`, `salt 4`; `story.practice commerce`. Coin: board 4 from `town:saltwell` + the 8 restitution; bought the 10 above; cart 12 from `town:saltwell`. The deed by road: "laid the salt tally before the Market Board" / (bought: as above) / "sold the Saltpan's salt off the cart at the old price" |

**Campaign feed.**
- On a road done: `add nh_realm 16`; `add nh_voice_saltwell 16` (4 if bought).
- Each wagon run that held at least 2 stages: `subtract nh_threat 3` and clamp.
- If `nh_stage ≥ 3`, every board text about the raiders reads "grey-cloaked raiders" instead of "raiders" (an if-branch on the post).

**Dialogue** (Orm, Osric and Rosamund speak plain; Kite and Fern saltwell, light; Fenwick coldwater, broad)
- Quest `brackish` (Orm):
  - **ask**: "Your cart's come back empty, carter."
  - **say**: "#greet#, #address#. Three carts left the Saltpan and three came home light. They take the salt and never the oxen. Ask the sack-counter what goes missing, ask the raker who worked in the city, then walk the brine flats after dark and see who waits there. #oath#."
  - **accept**: "I'll walk the brine flats." / **decline**: "Not this road." / **handIn**: "Here's what the raiders dropped."
  - **thanks**: "#thanks#. A tally of sacks, and a grey paw-mark at its foot. Somebody counts my salt before I've loaded it. When the next wagon loads, ride with me."
  - **thanks, thin purse** (`lq_thin`): "#thanks#. The stores can't spare a penny this week, so take bread and the tally's word. When the next wagon loads, ride with me."
- `npc.line` Orm (phase `open`): "The salt sits in the yard and the city goes without. And this wind. Look at it. Even the weather's in on it."
- `npc.line` Orm (phase `after_board`): "Wagon day again, and the Board's own price in Ridgemere. I'd shake every paw on that Board if they'd let me."
- `npc.line` Orm (phase `after_cart`): "Full sacks off the tail-board on Marketday. No middle wolf. The wind can blow all it likes."
- `npc.offer` Orm (`lq_salt_wagon`, once): **"When does the wagon load?"** → "At the eighth hour of wagon day, from the yard by the store. Bring your teeth and a waterskin, #address#. The dunes drink a wolf dry before the raiders do."
- `npc.offer` Orm (`lq_salt_cart`, while the wolf's `salt_cart` is active): **"Sell the salt off the cart, straight to the smokehouses."** → "No provisioner between us and the fish? I'll drive it to the Rag Market myself on Marketday. Take two sacks to the smokehouse now, so they know it's coming."
- **Window lines** (`resident.tree` `hub`, gated `world.mem.lq_salt=raided`):
  - `say[self.id=tn_mb_kite_bogditch world.mem.lq_salt=raided priority=4]` "Everyone in the yard knows the wagon days, and someone in the city besides. Forty sacks out, none home, and the raiders waiting each time. By salt and water, that is not luck."
  - `say[self.id=cty_fern_merewick world.mem.lq_salt=raided priority=4]` "Two winters, I worked for a provisioner in Northgate. Vane and Daughters. The young one there pays good coin for salt that comes with no carter. I came home with nothing, and I knew why."
  - `say[self.id=rm_rosamund_coalford world.mem.lq_salt=raided priority=4]` "My fish spoil in the racks for want of salt. Salt's never been this dear. Somebody's sitting on it."
  - `say[self.id=rm_rosamund_coalford world.mem.lq_salt=cart priority=4]` "Saltpan salt off the cart, at the old price. Blood and bread, the racks are full again. Tell the carter the smokehouses will take every sack he brings."
- Osric (plain):
  1. `lq_salt_osric1` **"Your sacks have Saltpan string on them."** → "String's string. I buy what's brought to my door, and I don't ask whose paws tied it. That's trade. If you want a sermon, the chapel's down the hill."
  2. `lq_salt_bought` (shown while the wolf's `salt_bought` is active) **"Pay me and the tally stays with me."** → "Now you're talking sense. Ten pennies and a sack's worth of quiet. Keep your nose pointed somewhere else."
- Fenwick (coldwater), `lq_salt_board` **"Lay this salt tally before the Market Board."** → "#oath#. A tally with a grey paw on it, eh? The Board sits on Marketday. I'll put it first, and his counter can answer for it. #thanks#, neighbour."
- Journal summaries (no giver): `ride` "Ride with the carter's wagon from the Saltpan yard through the red dunes to the Ochre cut." `salt_board` "Take the raiders' tally to the warden of the Market Hall." `salt_bought` "Take the provisioner's coin, and keep his secret." `salt_cart` "Tell the carter to sell to the smokehouses straight off the cart, and take the first two sacks there."

**Gossip and helpful knowledge** (`resident.tree`, priority 2)

| Town (gate) | Dialect | Key | Line |
|---|---|---|---|
| Saltwell `here.region=saltwell` | saltwell | `lq_salt=raided` (helpful) | "If you ride with the salt, carry water. The raiders wait where the flats turn to dune, and they run from a crowd, by salt and water." |
| Ridgemere `here.region=ridgemere` | plain | `lq_salt=board` | "Salt's back at the old price. A provisioner up Northgate was buying robbed sacks, and the Market Board heard every word of it." |
| Larkfield `here.region=larkfield` | larkfield | `lq_salt=bought` | "Salt's short-weighed and dear again. Somebody took coin to keep it so, and the market knows who." |
| Southmeadow `here.region=southmeadow` | southmeadow | `lq_salt=cart` | "The Saltwell cart sells off the tail-board now, full sacks at the old price. Ripe days for the pickling, my apple." |

Recap (`inn.greet`, `world.mem.lq_salt=raided`): "Salt's dear because the Saltpan carts get robbed on the Waste. The carter at Saltwell wants wolves to ride with him."

**Engine needs.**
- Runs today. The fallbacks: `prices.reset` needs a builder (else `times.nudge`); the talk keys need C8 (else the DM seeds them); Vane & Daughters pays from the treasury until SQ2.
- Nicer with: C10 (a named "Brine Captain" foe leading the dune band); C12a (credit in the running chapter); J1 (per-wolf `npc.offer`); C6 (chapter 2 with a giver); L2 and L3 (camp matching); SQ2 (Osric's own till pays).

---
## 4. The Mast Cedars (`lq-mast-cedars`)

| | |
|---|---|
| Tier | large: 3 chapters + a reckoning |
| Program | `lq-mast-cedars`, open, cap 5000, prefix `lq` |
| Towns and places | **Oakhaven** (square `3_4` (244, 196), board `board:oakhaven`); the Rainwood cut on Cedar Sound (`w_ridgemere_2_4`); the Oakhaven cut (`4_4`); the old cedar stands in the Rainwood (`2_6`, `3_7`); **the Wharf**: Grayrock Shipwrights `r2_grayrock_shipwrights` (door `0_1` (187, 164)) |
| Giver | **Ivy Stonehollow** `tn_fh_ivy_stonehollow`: f 56, fells timber at the Rainwood cut (works at (140, 69)); Oakhaven-born, **plain speech** (home region oakhaven, accent none); warm and frank; "lost a brother in a mine collapse". She lives in the Sump (`r2_slop_rooms_up`) and walks to the cut. She knows trees and knows what a lost trade does to a family. |
| Reward purse | `town:oakhaven` (247, day cap 24); Grayrock Shipwrights' payments from `town:ridgemere` until SQ2 (§1.5) |

**Premise.** Someone is ring-barking the oldest cedars of the Rainwood so they die standing, then felling them by night and dragging them to the Sound for a Grayrock lighter. A straight cedar a hundred years tall is a mast. The old stands are Oakhaven's by common right, and the "Seven Sisters" have stood since before the town. Find the stumps, sit up with the fellers, follow the tow-rope to the slipway, and decide what Oakhaven's trees are worth.

**Cast**

| Role | Resident (id) | Voice | Hub item |
|---|---|---|---|
| hero, giver | Ivy Stonehollow `tn_fh_ivy_stonehollow` | plain | quest `ringbark`; road choice `lq_ced_return` |
| witness | Flint Snowdrift `cty_flint_snowdrift` (m 28, logger at the Rainwood cut) | plain (broad) | one `npc.offer`: heard saws at night and was whistled off; nervy |
| old knowledge | Frost Colddrift `cty_frost_colddrift` (f 50, fells at the Oakhaven cut; gives small `sq-venison-for-the-cut`) | oakhaven (light) | **conversation target only**: devout, guarded; knows the stands' old names |
| inside wolf | Senna Beechleaf `cty_senna_beechleaf` (f 42, logger at the Rainwood cut) | plain (light) | one `npc.offer`; "owes a debt to someone in Ridgemere and has not said to whom": the shipwright |
| buyer (villain) | Brenna Slaggate `rm_brenna_slaggate` (f 43, shipwright at Grayrock Shipwrights) | plain (larkfield, accent none) | two `npc.offer`s in sequence; cold, guarded, short-tempered; "a petition lost in the City Office" |
| foes | the night crew: `story.area.spawn` bands in the stands | — | |

**Regions**

| id | cell | box | what |
|---|---|---|---|
| `cut_camp` | `w_ridgemere_2_4` | (128, 56)–(152, 80) | the Rainwood cut by the Sound |
| `old_stand` | `w_ridgemere_2_6` | (210, 20)–(229, 39) | the Seven Sisters (30 cedar tiles, 358 open) |
| `rope_spot` | `w_ridgemere_2_6` | (215, 26)–(223, 34) | inside the old stand: where the cut tow-rope lies |
| `ravine_stand` | `w_ridgemere_3_7` | (140, 130)–(159, 149) | the ravine stand |

**Chapters and objectives**

*Chapter 1, `ringbark`, "Ring-Barked".* Scope player; giver Ivy, board `oakhaven`; once; `join.cap` 40 (no placed item: the rope is a story signal).

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `flint` | Ask the young logger what he heard in the stands. | conversation | target `cty_flint_snowdrift` | 1 | |
| `old` | Walk the old stand. | region entered | target `old_stand` | 1 | |
| `ravine` | Walk the ravine stand. | region entered | target `ravine_stand` | 1 | |
| `names` | Ask the old feller at the Oakhaven cut what the stands are called. | conversation | target `cty_frost_colddrift` | 1 | her window line |
| `rope` | Find the cut tow-rope in the old stand. | story signal | `rope_found` | 1 | `after: [old]`. `region entered` `rope_spot` (participant, `once: player`) → `objective.complete` and `journal.note` "A cut tow-rope, tied off in a shipwright's knot." |

*Chapter 2, `nightfell`, "Night Fellers".* Scope **party** (members fixed at start; a lone wolf is a party of one); journal offer; weekly; `join.cap` 12. The two stands are the two spawn regions, used in turn.

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `sit` | Sit up in a stand between midnight and the fourth hour. | story signal | `stand_sat` | 1 | `hour` trigger 0–4: each participant in `old_stand` or `ravine_stand` |
| `crew` | Bring down the night crew. | story signal | `crew_down` | 3 | `objective.progress` from `bandit falls` triggers with cell 2_6 or 3_7. The spawn ladder runs at the 1:00 `hour` in the stand where wolves sit, or, if both are held, in the stand whose switch is off |
| `slip` | Follow the tow-rope's knot to the slipway. | enter place | target `r2_grayrock_shipwrights` | 1 | `after: [crew]` |
| `senna` (hidden) | Ask the logger who went white when you showed her the rope. | conversation | target `cty_senna_beechleaf` | 1 | revealed by `objective.reveal` after `slip` |

**Shared goal.** Story number `stumps_saved` (+1 per `crew` progress). At ≥ `g` (base 6, + 1 per helper) → `enter reckoning`. Tracker: "Cedars standing".

*Chapter 3, "What a Cedar Is Worth".* Road quests by journal, each `closure: true` (the optional third, `cedar_debt`, carries no `closure`, so the line counts once). Each begins with `face`: `dialogue choice` item `rm_brenna_slaggate`, detail `lq_ced_face`.

| Road quest | Its objectives after `face` | World result |
|---|---|---|
| `cedar_contract` "A Mast a Season" | `dialogue choice` item `rm_brenna_slaggate`, detail `lq_ced_contract` | `lq_cedars = contract` (**default**) |
| `cedar_return` "Float Them Home" | `dialogue choice` item `tn_fh_ivy_stonehollow`, detail `lq_ced_return` (the choice sits on the giver); `conversation` target `cty_frost_colddrift` ("ask the old feller where the Sisters stood"); then `region entered` `cut_camp` between 6 and 18 (trigger), with `together` beside another wolf there (optional) | `lq_cedars = returned` |
| `cedar_debt` (optional third, any wolf who did `senna`) "Senna's Debt" | a `gift` of ≥ 12 coins to `cty_senna_beechleaf` (`measure: coins`) | sets no world result, but adds +1 weight to whichever road wins and frees Senna in talk (`npc` key `lq_senna=free`) |

**Branches and lasting consequences** (default `contract`):
- **contract.** Each `cedar_contract` wolf gets 8 "Grayrock Shipwrights pays the first mast's price" (today `town:ridgemere`; after SQ2 `till:job:rm_brenna_slaggate`; story counter cap 60). `times.nudge {market: oakhaven, points: 15, days: 28, words: "Grayrock pays Oakhaven for a mast a season"}`. Oakhaven board: *"A MAST A SEASON: the Wharf pays Oakhaven's own axes for one cedar a season. The Sisters stand."* Key `lq_cedars=contract`.
- **returned.** `times.nudge {market: ridgemere, points: 10, days: 14, words: "masts short at the Wharf"}`. Oakhaven's board: *"The cedars came home up the Sound. Root and rain, the Wharf will think twice."* Key `lq_cedars=returned`.
- **Nobody chose** (the reckoning fires with no road): `lq_cedars=taken`. The night crews come back next season (the phase loops), and Ivy's `npc.line` turns bitter.
- **Lasting:** the world text. The campaign's Ch 5 (the Hills and the cut) can read `lq_cedars`. A `contract` world makes Oakhaven's fellers confident; a `taken` world makes them angry.

**Rewards** (§1.5)

| When | Reward |
|---|---|
| `ringbark` | deed "found the ring-barked cedars of the Rainwood"; `story.practice travel` |
| `nightfell` (first time) | deed "sat up with the night fellers in the old cedar stand"; `story.practice watch`; `item.give rain_cape 1`; 6 from `town:oakhaven` |
| a road done | `item.give timber 4`; `story.practice labour`. Coin: contract 4 from `town:oakhaven` + the shipwrights' 8; returned 12 from `town:oakhaven`. Deeds: "won Oakhaven a fair price for its mast cedars" / "floated stolen cedars back up the Sound to Oakhaven" |

**Campaign feed.**
- Road done: `nh_realm +8`, `nh_voice_oakhaven +8`.
- Each crew broken: `nh_threat −1`.
- If `nh_b_hills = fell` (Ch 5 has given Fell the hills), Ivy's `npc.line` in `open` is the "both" line below.

**Dialogue** (Ivy, Flint, Senna and Brenna speak plain; Frost oakhaven, light)
- Quest `ringbark` (Ivy):
  - **ask**: "Trouble at the cut?"
  - **say**: "#greet#, #address#. Somebody's ring-barking the old cedars. A band of bark cut right round, so the tree dies on its feet and nobody asks why it's felled after. Seven gone from the old stand since spring. Walk the stands and find me who."
  - **accept**: "I'll walk the stands." / **decline**: "Not today." / **handIn**: "I found a cut tow-rope."
  - **thanks**: "#thanks#. That's a shipwright's knot. Nobody ties a log like that to burn it. They're masts, then. They're stealing masts. Come back with a pack and we'll sit up in the stands."
- `npc.line` Ivy (`open`): "Mind the old stand. The best bark in the Rainwood is dying on its feet, and I can't stand guard on every tree."
- `npc.line` Ivy (`open`, if `nh_b_hills = fell`): "Fell's bounders on the hills and Grayrock's saws in the stands. Some years everybody wants a piece of the wood."
- `npc.line` Ivy (`after_taken`): "Three more Sisters down. Nobody stopped them. I've stopped counting the stumps."
- `npc.offer` Ivy (`lq_ced_return`, while the wolf's `cedar_return` is active): **"Float the marked logs back up the Sound."** → "Up the Sound to its head, and lay them where the Sisters stood? It'll take a crew and a long day. Ask the old feller at the Oakhaven cut where they stood. I'd like to see the Wharf's face."
- `npc.offer` Flint (`lq_ced_flint`, once): **"What did you hear in the stands?"** → "Saws, at night, where there's no cut. I went to look once and somebody whistled the way you whistle a crew off. I didn't go twice. Bright One keep me, I'm not proud of it."
- `npc.offer` Senna (`lq_ced_senna`, after `slip`): **"Who do you owe in Ridgemere?"** → "A shipwright on the Wharf. She lent when my roof went, and now she wants it paid in nights. Marked trees, a whistle, a rope on the landing. I hate every one of them."
- **Window lines** (`resident.tree` `hub`):
  - `say[self.id=cty_frost_colddrift world.mem.lq_cedars=dying priority=4]` "The Seven Sisters, mm? Seven cedars older than Oakhaven. We never fell a Sister. Root and rain, if they're gone, the Rainwood has lost its mothers."
  - `say[self.id=cty_frost_colddrift world.mem.lq_cedars=dying|reckoning priority=4]` (second alternative) "They stood in a ring on the rise past the ravine, mm? Lay a log where each one stood and the wood will know."
- Brenna (plain):
  1. `lq_ced_face` **"Those mast cedars are Oakhaven's."** → "#greet#. Are they? I bought them full weight off the landing, a felling mark on every butt. Grayrock needs masts and Oakhaven sells planks. I paid. Who did I pay? That's the question, isn't it?"
  2. `lq_ced_contract` **"Pay Oakhaven's own fellers, one mast a season."** → "One a season, felled by their axes and paid to their stores? That's a dearer mast. Fine. Sold. And the night crew can find other work."
- Journal summaries: `nightfell` "Sit up in the old stands with a pack, bring down the night crew, and follow the rope." `cedar_contract` "Make the shipwright pay Oakhaven's fellers for a mast a season." `cedar_return` "Float the stolen cedars back up the Sound to where the Sisters stood." `cedar_debt` "Pay the logger's debt, so the shipwright has no hold on her."

**Gossip and helpful knowledge**

| Town | Dialect | Key | Line |
|---|---|---|---|
| Oakhaven | oakhaven | `lq_cedars=dying` (helpful) | "Sit up in the old stand when the night's dark, mm? The night crew comes in by the ravine and goes out by the Sound." |
| Oakhaven | oakhaven | `lq_cedars=contract` | "Grayrock pays Oakhaven for a mast a season now, proper money, felled by our own axes. Good bark, that bargain." |
| Ridgemere `self.quarter=wharf` | plain | `lq_cedars=returned` | "Somebody floated a lighter's worth of mast cedars back up the Sound. The shipwrights are spitting nails." |
| Coldwater | coldwater | `lq_cedars=contract` | "They say the Oakhaven fellers stood up to the Wharf over their cedars, eh? Calm water to them." |

Recap (`inn.greet`, Oakhaven, `lq_cedars=dying`): "Somebody's killing the old cedars in the Rainwood for masts. The feller at the Sound cut is asking for wolves to sit up in the stands."

**Engine needs.** Runs today (L2 and L3 for camps). C8 for talk; J1 for per-wolf `npc.offer`; C5 (`npc.greet`) for Ivy thanking a wolf by name in a `contract` world; SQ2 for the shipwrights' own till.

---
## 5. The Old Brown of the Greyteeth (`lq-old-brown`)

| | |
|---|---|
| Tier | large: 3 chapters + a reckoning |
| Program | `lq-old-brown`, open, cap 5000, prefix `lq` |
| Towns and places | **Coldwater** (square `7_7` (164, 4), board `board:coldwater`); the Greyteeth sheep barn `fs_sheep_foothills_barn` (door `w_ridgemere_11_5` (14, 249), in the Larkdowns); Greytarn (`13_3`); the Upper charcoal hearths under the Whitefangs (`10_3`); the high fold (`12_4`); the camp on the Frosthollow road (`8_6`) |
| Giver | **Jessamy Greyford** `cty_jessamy_greyford`: f 36, shepherd at the Greyteeth fold, lives in Coldwater Cottage 3; coldwater speech, light (cw115); quiet and frank; "swore an oath at a wedding and takes it seriously". She keeps the flock as she swore to, and she will not lie to suit the owner. |
| Reward purse | `town:coldwater` (212, day cap 21) |

**Premise.** Something is taking sheep from the Greyteeth fold. The flock's owner says Frosthollow thieves and wants the heather burned to the rock. Jessamy says an old brown bear has come down early from Greytarn. Both are half right. The bear took two. Forty were driven off by wolves in grey cloaks, up the Pass Road, to feed a camp on the Frosthollow road. Read the signs, sit the fold through the night, break the rustlers, and then decide what to do about the Old Brown.

**Cast**

| Role | Resident (id) | Voice | Hub item |
|---|---|---|---|
| hero, giver | Jessamy Greyford `cty_jessamy_greyford` | coldwater (light) | quest `wool` |
| doubter | Juniper Larkcombe `tn_lf_juniper_larkcombe` (f 34, shepherd; shares Cottage 3 with Jessamy) | coldwater (light) | one `npc.offer`; dry, cold, guarded; counts the flock and notices the owner never counted the forty |
| witness | Stone Acorndell `cty_stone_acorndell` (m 46, burns charcoal at the Upper hearths; gives medium `mq-lamp-oil-share`) | frosthollow (broad) | **conversation target only**: "lost a sibling to a hard winter and still watches the road": he saw the bear and the grey cloaks |
| shepherd | Vale Pikebridge `cty_vale_pikebridge` (f 47, shepherd at Greyteeth) | southmeadow (light) | one `npc.offer`; short, guarded; keeps the high fold's key |
| offstage | Lorn Hedgeton (the flock's owner; the campaign's Ch 3 informer) | — | never given an offer here, never cleared or accused: the campaign owns his story |
| foes | the rustlers: `story.area.spawn` bands at the fold; any camp at `8_6` | — | the Captain's northern band, if the campaign runs |
| the beast | a brown bear (`hunted` target `bear`) | — | with L1, staged at Greytarn |

**Regions**

| id | cell | box | what |
|---|---|---|---|
| `fold` | `w_ridgemere_11_5` | (4, 236)–(28, 255) | the fold round the barn door |
| `fold_b` | `w_ridgemere_11_5` | (56, 212)–(75, 231) | the fold's upper pasture (400 open): the second spawn region, used in turn |
| `greytarn` | `w_ridgemere_13_3` | (200, 70)–(219, 89) | rock and scree by Greytarn (all open) |
| `hearths` | `w_ridgemere_10_3` | (72, 162)–(96, 186) | the Upper hearths, where Stone works at (83, 173) |
| `pass_camp` | `w_ridgemere_8_6` | (0, 0)–(19, 19) | the old camp site on the Frosthollow road, by proj-1 (400 open) |
| `high_fold` | `w_ridgemere_12_4` | (170, 190)–(189, 209) | grass on the Larkdowns' north-east, the summer fold |

**Chapters and objectives**

*Chapter 1, `wool`, "Wool on the Rocks".* Scope player; giver Jessamy, board `coldwater`; once; `join.cap` 40.

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `count` | Count the flock with the other shepherd. | conversation | target `tn_lf_juniper_larkcombe` | 1 | |
| `sign` | Find the bear's sign at Greytarn. | region entered | target `greytarn` | 1 | on completion, `journal.note` "Scat and a torn fleece on the scree: a bear, an old one." (nothing is placed) |
| `collier` | Ask the collier at the Upper hearths what he saw. | conversation | target `cty_stone_acorndell` | 1 | his window line |
| `pass` (hidden) | See where the forty were driven. | region entered | target `hearths` | 1 | `after: [collier]`; revealed when `collier` is done |

*Chapter 2, `foldnight`, "Nights at the Fold".* Scope party; journal; weekly; `join.cap` 12 (waves in `fold` and `fold_b`, in turn).

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `sit` | Keep the fold between the tenth hour of night and the fourth of morning, two nights. | story signal | `fold_night` | 2 | an `hour` trigger at 3:00 adds 1 for each participant in `fold` or `fold_b`, once per wolf per game day (player number `last_night`) |
| `rustlers` | Drive the rustlers off the fold. | bandit falls | cell `w_ridgemere_11_5` | 3 | the spawn ladder runs at 2:00 on the second night a party sits, in the region of the turn |
| `cloak` | Pick up the grey cloak a rustler dropped. | item picked up | item `wool_hood`, cell `w_ridgemere_11_5` | 1 | placed on `camp cleared` (cell 11_5, the region's switch on): at (24, 250) for `fold`, (65, 221) for `fold_b` |
| `camp` (optional) | Break the rustlers' camp on the Frosthollow road. | story signal | `camp86` | 1 | trigger on `camp cleared` with **cell** `w_ridgemere_8_6` (a natural camp or any story camp there: either way the road is clear), each participant `wolf in region pass_camp` |

**Shared goal.** Story number `rustlers_broken` (+1 per party that finishes `rustlers`; +2 for `camp`). At ≥ `g` (base 3, + 1 per 3 helpers) → `enter reckoning`. Tracker: "Rustlers driven off".

*Chapter 3, "The Old Brown".* Two road quests by journal, each `closure: true`.

| Road quest | Objectives | World result |
|---|---|---|
| `brown_hunt` "Bring Down the Old Brown" (scope party) | `hunted` target `bear` (any cell; with L1, staged at `greytarn` for a party that enters at dusk); then a `gift` of `bear_pelt` 1 to `cty_jessamy_greyford` | `lq_brown = slain` (only if a bear was really killed; else the road counts as `spared`) |
| `brown_spare` "Move the Flock High" (scope player) | `region entered` `high_fold`; keep the high fold at dusk (story signal from an `hour` 19–21 trigger, goal 2 game days); a `gift` of `salt` ≥ 2 to `cty_vale_pikebridge` for the high fold's lick | `lq_brown = spared` (**default**) |

**Branches and lasting consequences.**
- **slain.** Deed for the party ("brought down the Old Brown of the Greyteeth"). The Coldwater board reads *"The Old Brown is down. The Greyteeth fold sleeps."* Key `lq_brown=slain`.
- **spared.** Jessamy's oath is kept "without blood". The Coldwater board reads *"The flock is on the high fold, and the Old Brown has its rocks."* Key `lq_brown=spared`. A world with the bear spared makes Greytarn a place bears are known in talk, which suits later hunting quests.
- **Rustlers broken** (the shared goal met at all): `subtract nh_threat 3`.
- The talk key `lq_rustlers=grey` stays for the season. It is the campaign's thread: grey-cloaked wolves living on Coldwater mutton on the Frosthollow road.
- The owner's silence about the forty is planted (Juniper's line) and never resolved here. It is the campaign's Ch 3 to resolve.

**Rewards** (§1.5)

| When | Reward |
|---|---|
| `wool` | deed "read the signs on the Greyteeth fold"; `story.practice travel` |
| `foldnight` (first time) | deed "drove the rustlers off the Greyteeth fold"; `story.practice watch`; `item.give fleece_boots 1`; 6 from `town:coldwater` |
| a road done | 12 from `town:coldwater`; `story.practice labour`. Hunt: `item.give fur_hat 1`. Spare: `item.give sheepskin 1`. Deeds: "brought down the Old Brown of the Greyteeth" / "moved the Greyteeth flock out of the Old Brown's road" |

**Campaign feed.** Road done: `nh_realm +12`, `nh_voice_coldwater +12`. `camp` done: `nh_threat −3` (the large bounty rate).
- If `nh_stage ≥ 3` (the Grey Captain named), the `cloak` objective's label becomes "Pick up the grey cloak, the Captain's colour." That is a second objective with the same event, shown by `objective.reveal` while the other is hidden.

**Dialogue** (Jessamy and Juniper speak coldwater; Stone frosthollow, broad; Vale southmeadow)
- Quest `wool` (Jessamy):
  - **ask**: "You're a sheep short?"
  - **say**: "#greet#, #address#. Forty short, and two of those were the bear's, I'd swear it on the oath I keep. The owner says Frosthollow thieves and wants the heather burned to bare rock. Count with my cottage-mate, find the bear's sign at Greytarn, and ask the collier what he saw, eh?"
  - **accept**: "I'll read the signs." / **decline**: "Another day." / **handIn**: "The forty went up the Pass Road."
  - **thanks**: "#thanks#. Up the Pass Road, driven, not dragged. Net and line, it's wolves, then, and a bear besides. Sit the fold with me after dark. If they come again, they'll find teeth waiting."
- `npc.line` Jessamy (`open`): "Calm water to you. Mind the fold gate, neighbour. Something's counting our sheep before we do."
- `npc.line` Jessamy (`after_spared`): "Still water. The flock's high and the Old Brown's on its rocks, and nobody's oath was broken. Fair catch, eh?"
- `npc.offer` Juniper (`lq_brown_count`, once): **"How many are gone?"** → "Forty-two, if you count the lambs. Funny thing, eh? The owner never counted the forty. Only the two the bear had. Make of that what you like, neighbour."
- **Window line** `say[self.id=cty_stone_acorndell world.mem.lq_brown=rustled priority=4]` (`resident.tree` `hub`): "'Arth to you, cousin. A brown bear, deep old, nosing the scree by Greytarn. And a night after, wolves in grey cloaks driving sheep up the Pass Road at a trot. Black seam, they never once looked at my fire."
- `npc.offer` Vale (`lq_brown_high`, offered with `brown_spare`): **"Will the high fold take the flock?"** → "Grass enough, and a lick if somebody brings salt up. Bring me two of salt and I'll open the gate. Blight take the bear, I'd still not want it shot for being hungry."
- Journal summaries: `foldnight` "Sit the Greyteeth fold two nights with a pack, and drive the rustlers off." `brown_hunt` "The Old Brown has taken two of the flock and will take more. Bring it down, with a pack at your side." `brown_spare` "The Old Brown is old and hungry. Move the flock to the high fold and leave the bear its rocks."
- Board (Coldwater, while `open`): *"THE GREYTEETH FOLD: shepherds want sitters for the night. Rustlers about. Bring teeth and a friend."*

**Gossip and helpful knowledge**

| Town | Dialect | Key | Line |
|---|---|---|---|
| Coldwater | coldwater | `lq_brown=rustled` (helpful) | "Sit the Greyteeth fold in the dark hours, eh? The rustlers come in the middle of the night, and they run from more than three of us." |
| Frosthollow | frosthollow | `lq_rustlers=grey` | "Grey cloaks on the Pass Road with Coldwater sheep. Cold iron, the camp up there eats well, cousin." |
| Coldwater | coldwater | `lq_brown=slain` | "The Old Brown's down. A pack took it by Greytarn, they say. Bright One's tide, I'd not have gone near it." |
| Larkfield | larkfield | `lq_brown=spared` | "The Greyteeth flock's on the high fold and the bear kept its rocks. Fair price all round, that." |

Recap (`inn.greet`, Coldwater): "Sheep going off the Greyteeth fold. Some say a bear, some say wolves. The shepherd from Cottage 3 wants help."

**Engine needs.** **L1** `hunt.stage` (Phase 5; or the bear stays optional and slow). C8 for talk. L2 and L3 for the fold camp. C10 (`foe.add`) would let the rustlers' leader be a named foe.

---

## 6. Clipped Silver (`lq-clipped-silver`)

| | |
|---|---|
| Tier | large: 3 chapters + a reckoning. Chapter 1 is the old medium `mq-short-weighed` (decisions §2.3), which is withdrawn from the medium pack. |
| Program | `lq-clipped-silver`, open, cap 5000, prefix `lq` |
| Towns and places | **Larkfield** (square `6_9` (140, 76), board `board:larkfield`): Larkfield Stores `cn_larkfield_store` (door (130, 76)), the Smithy `cn_larkfield_smithy` (door (138, 64)); **Ridgemere**: the Market Hall `r2_the_market_hall` (doors `1_1` (84, 16) and (123, 16)), the Greatly Counting House `rm_the_greatly_counting_house`, Silver & Rain `rm_silver_rain` (door `1_1` (168, 58)), the Records Office; **the Kestrel lead and silver workings** `w_ridgemere_8_2` (no workers: a dead working) |
| Giver | **Pike Chalkley** `tn_lf_pike_chalkley`: m 26, keeps Larkfield Stores and lives over the shop; larkfield speech, broad (lf120); unhurried and tired, distrusts anything from the city; "dreams of a farm of his own". A quiet shopkeeper whose till is the realm's trust in miniature. He holds the line's quest and its chapter-1 choice. |
| Reward purse | `town:larkfield` (248, day cap 24); the Counting House chit and Silver & Rain's payments from `town:ridgemere` until SQ2 (§1.5) |

**Premise.** Larkfield sends full wagons of Longmead sacks to the city, and the Market Hall pays for two-thirds of them, in coin that is light besides: pennies with the rim shaved, and new pennies thin as leaves, struck from the shavings. First the beam: the Market Hall weighs Larkfield's grain on a Counting House beam with a weighted pan. Then the coin: weigh it on the anvil scales, follow it up the High Road to a jeweler's on the Commons, and out to the dead Kestrel workings where the dies are hidden and guarded. The coiner is an old jeweler with a factor's chalk on her door. The realm's markets run on trust, and so does her neighbour.

**Cast**

| Role | Resident (id) | Voice | Hub item |
|---|---|---|---|
| hero, giver | Pike Chalkley `tn_lf_pike_chalkley` | larkfield (broad) | quest `light`; the beam choice `lq_silver_beam` / `lq_silver_hush` (two `npc.offer`s of the same quest, shown together while it runs) |
| witness | Imogen Rustford `cty_imogen_rustford` (f 35, Larkfield carter; gives medium `mq-mud-to-the-axles`) | larkfield (light) | **conversation target only**: drives the grain down and took the light coin from Ridgemere buyers |
| the beam | Fenwick Mossdale `rmx_fenwick_mossdale` (m 33, warden of the Market Hall) | coldwater (broad) | **conversation and gift target only** here (his one offer is Q1's): shows the grain beam, and takes it, and later the dies, before the Market Board |
| expert | Harrow Larkwell `tn_lf_harrow_larkwell` (m 26, striker at Larkfield's anvil) | larkfield (light) | one `npc.offer`; weighs coin on the smith's scales; cold, guarded |
| villain (sympathetic) | Vigdis Rainmore `rm_vigdis_rainmore` (f 62, jeweler at Silver & Rain) | plain (larkfield, accent none) | one `npc.offer` (the cut); quiet, short; "owes a House factor money, to find by the new moon": the campaign's Reckoning |
| witness | Dirk Rainmore `rm_dirk_rainmore` (m 74, minds the door at Silver & Rain; lives in its flat with Vigdis) | plain (broad) | one `npc.offer`; warm, quiet; "a petition lost in the City Office" |
| foes | the coiners' hired guards at the Kestrel workings | — | |

**Regions**

| id | cell | box | what |
|---|---|---|---|
| `kestrel` | `w_ridgemere_8_2` | (0, 110)–(19, 129) | rubble and rock of the old workings (386 open) |
| `kestrel_b` | `w_ridgemere_8_2` | (12, 88)–(31, 107) | the spoil heaps above the workings (397 open): the second spawn region, used in turn |
| `larkfield_sq` | `w_ridgemere_6_9` | (130, 66)–(150, 86) | the square, for the final crier scene |

**Chapters and objectives**

*Chapter 1, `light`, "Short-Weighed".* Scope player; giver Pike, board `larkfield`; once; `join.cap` 40. The old medium's steps, with the coin added; the miller and the Counting House clerk are left out (the clerk is campaign cast).

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `carter` | Ask the carter who drives the grain down what the Hall paid. | conversation | target `cty_imogen_rustford` | 1 | her window line |
| `hall` | Watch the sacks weighed at the Market Hall. | enter place | target `r2_the_market_hall` | 1 | |
| `warden` | Ask the warden of the Market Hall to show you the grain beam. | conversation | target `rmx_fenwick_mossdale` | 1 | `after: [hall]`. His window line: the beam is a Counting House beam with a weighted pan |
| `weigh` | Have the striker weigh the Hall's coin on the anvil scales. | conversation | target `tn_lf_harrow_larkwell` | 1 | his `npc.offer` `lq_silver_weigh` |
| `beam` | Tell the shopkeeper what to do about the beam. | story signal | `beam_chosen` | 1 | `after: [warden]`. A `dialogue choice` trigger on Pike, detail `lq_silver_beam` or `lq_silver_hush`, completes it and sets the player text `lq_beam` (`board` / `hushed`) |
| `told` (hidden) | Tell the warden the Market Board should see the beam. | conversation | target `rmx_fenwick_mossdale` | 1 | revealed only when `lq_beam = board` |
| hand-in | to Pike | | | | `handIn: true` |

*Chapter 2, `trail`, "The Silver Trail".* Scope party; journal; weekly; `join.cap` 12 (waves in `kestrel` and `kestrel_b`, in turn).

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `shop` | Find the jeweler's the coin comes from. | enter place | target `rm_silver_rain` | 1 | |
| `dirk` | Ask the old wolf at the door where she goes. | conversation | target `rm_dirk_rainmore` | 1 | |
| `chalk` (optional) | Copy the factors' chalk-list at the Records Office. | archive work | detail `ridgemere` | 1 | shows she owes |
| `workings` | Go out to the dead Kestrel workings. | region entered | target `kestrel` or `kestrel_b` | 1 | the spawn ladder fires on entry (`once: visit` per party), in the region of the turn |
| `guards` | Bring down the guards on the workings. | bandit falls | cell `w_ridgemere_8_2` | 3 | |
| `dies` | Take the clipping shears and the die. | item picked up | item `shears`, cell `w_ridgemere_8_2` | 1 | placed on `camp cleared` (cell 8_2, the region's switch on) at (9, 119) or (21, 97) |

**Shared goal.** Story number `dies_taken` (+1 per party). At ≥ `g` (base 2, + 1 per 4 helpers) → `enter reckoning`. Before the reckoning the coiner re-makes her dies each week: on `day begins` Stoneday, both `spawned_kestrel` switches are turned off, so the workings are guarded again.

*Chapter 3, "Weights and Measures".* Three road quests by journal, each `closure: true`.

| Road quest | Objectives | World result |
|---|---|---|
| `silver_board` "Before the Market" | a `gift` of `shears` 1 to `rmx_fenwick_mossdale` | `lq_silver = board` (**default**) |
| `silver_mercy` "A Debt Paid Quietly" | a `gift` of ≥ 20 coins to `rm_vigdis_rainmore` (`measure: coins`); a `gift` of `shears` 1 to `tn_lf_harrow_larkwell`, to be broken on the anvil | `lq_silver = mercy` |
| `silver_cut` "A Share of the Silver" | `dialogue choice` item `rm_vigdis_rainmore`, detail `lq_silver_cut` | `lq_silver = cut` |

**Branches and lasting consequences.**
- **The beam (chapter 1, per wolf).**
  - `board`: the first wolf of each week whose `told` completes runs `times.nudge {market: larkfield, points: 10, days: 7, words: "full weight at the Market Hall again"}` (a story timer keeps it to one a week). Talk key `lq_beam=board` for the week.
  - `hushed`: the wolf takes "a Counting House chit, cashed" (6, today `town:ridgemere`; after SQ2 the Greatly Bank till `till:job:rm_eleanor_saltcot`, never more than 10 a time) and the deed "kept quiet about a crooked grain beam for the Counting House". It does not write `nh_bought`, which only chapters write; with C12a it notifies the running chapter as a small Purse-path deed. Pike's thanks is the cold variant.
- **board.** `times.nudge {market: larkfield, points: 10, days: 14, words: "light coin called in"}`.
  - Each `silver_board` wolf gets restitution of 8 "Silver & Rain pays it back" (today `town:ridgemere`; after SQ2 `till:job:rm_vigdis_rainmore`; story counter cap 50).
  - Public scene on the Larkfield square (cast Pike and Harrow, each in their own panel): the striker breaks the die on the anvil.
  - Key `lq_silver=board`; npc key `lq_vigdis=shamed`.
  - Vigdis is never held or killed. Her fate is talk, and C14 would let the post pass.
- **mercy.** `times.nudge larkfield +10` as above, but no restitution and no scene.
  - Vigdis thanks the wolf by name with C5 `npc.greet`. Until C5, her window line `say[self.id=rm_vigdis_rainmore self.mem.lq_vigdis=freed priority=4]` (below) thanks whoever comes, and each mercy wolf gets the deed, which carries the news.
  - Key `lq_silver=mercy`; npc key `lq_vigdis=freed`.
- **cut.** `times.nudge {market: larkfield, points: -10, days: 28, words: "light coin about"}`.
  - Each `silver_cut` wolf gets 3 "a share of the silver" on each `day begins` Marketday for four weeks (`schedule`, repeat 4, owner player; today `town:ridgemere`, after SQ2 Vigdis's till): 12 in all, the chapter's whole coin.
  - Deed "took a share of a coiner's silver". Its gossip follows them.
  - Key `lq_silver=cut`.
- **Campaign hook.** If `nh_stage = 2` (By the New Moon), `silver_mercy`'s summary adds: "She's one of the Counting House's debtors; the herald's lists will want her name." The pack does **not** add her to `nh_debtors_freed`, which is the chapter's to write.

**Rewards** (§1.5)

| When | Reward |
|---|---|
| `light` | `story.practice commerce`. Board: 6 from `town:larkfield`, deed "proved the Market Hall's grain beam short-weighed Larkfield". Hushed: the chit and deed above |
| `trail` (first time) | deed "broke the coiners' guard at the Kestrel workings"; `story.practice travel`; 6 from `town:larkfield` |
| a road done | Board: 4 from `town:larkfield` + the 8 restitution, `item.give scales 1`. Mercy: 12 from `town:larkfield`, `item.give silver_clip 1` ("from the jeweler's own tray"). Cut: the shares only. Deeds: "laid a coiner's dies before the Market Board" / "paid an old jeweler's debt and broke her dies" / (cut, as above) |

**Campaign feed.**
- Chapter 1, board only: `nh_realm +4`, `nh_voice_larkfield +6`.
- Road done: `nh_realm +12`; `nh_voice_larkfield +12` (3 for cut). Each Kestrel camp cleared: `nh_threat −1`.

**Dialogue** (Pike, Harrow and Imogen speak larkfield; Vigdis and Dirk plain; Fenwick coldwater)
- Quest `light` (Pike):
  - **ask**: "Trade slow?"
  - **say**: "#greet#, #address#. Slow and light. We send full wagons of Longmead sacks to the city, and the Market Hall pays for two-thirds, in coin with the rim shaved off besides. Short-weighed twice over. Ask the carter, watch the Hall's beam, and take their pennies to the striker at the anvil. I can't leave the counter."
  - **accept**: "Carter, beam and anvil. I'll find the light one." / **decline**: "Grain's grain." / **handIn**: "I know who's light."
  - **thanks** (the wolf chose the Board): "#thanks#. A Counting House beam and a jeweler's file, both. Scales and silver, a city wolf shaving the realm's coin. Follow it to the Commons, customer. I'll keep my till shut till you're back."
  - **thanks, hushed** (`lq_beam = hushed`): "Still light, then, and somebody's paid to keep it so. Weights and measures. Larkfield will find out who. We always do."
- `npc.offer` Pike (`lq_silver_beam`, after `warden`): **"That beam is false. The Market Board should see it."** → "#oath#. Then tell the warden so to his face, and I'll write it on the wall behind the counter so I never forget the look of it."
- `npc.offer` Pike (`lq_silver_hush`, after `warden`): **"Leave it with me. I'll deal with the Counting House."** → "You? With them? Well. Mind they don't weigh you on it."
- `npc.line` Pike (`open`): "Fair price to you, and full weight, if you've got it. Count your change twice this week."
- `npc.line` Pike (`after_board`): "Good trade. Full-weight pennies in the till again, and the die broke on our own anvil."
- `npc.offer` Harrow (`lq_silver_weigh`, once): **"Are these pennies true?"** → "Short-weighed, every third one. Rims shaved and stamped again. Somebody with a jeweler's file and a good die. That's not Larkfield work. We'd have heard the hammer."
- **Window lines** (`resident.tree` `hub`):
  - `say[self.id=cty_imogen_rustford world.mem.lq_silver=clipping priority=4]` "Full wagons down, two-thirds paid, friend. And paid by a jeweler's runner from the Commons at the High Road gate. Short weight and a top-weight smile. I've a sack of it."
  - `say[self.id=rmx_fenwick_mossdale world.mem.lq_silver=clipping priority=4]` "The grain beam? It's the Counting House's. They lent it us. The pan sits low, eh? I always thought it sat wrong. #oath#, I'd be shaking if I had to say so to the Board."
  - `say[self.id=rmx_fenwick_mossdale world.mem.lq_beam=board priority=4]` "I put the beam before the Board myself. My paws haven't stopped shaking since. Full weight for Larkfield from now on, neighbour."
- `npc.offer` Dirk (`lq_silver_dirk`, once): **"Where does she go on Stonedays?"** → "Out to the old Kestrel workings, with a basket and a lamp. Says she buys silver off the tailings. There's no silver in dead tailings. I've told her so. She cries at night about the factor's chalk on our door."
- Vigdis (plain), `lq_silver_cut` **"Keep clipping. Give me a share."** → "You're a cold one. A share, then, every Marketday, and you never saw my file. The chalk comes off my door by the new moon, and that's all I ever wanted."
- **Window line** `say[self.id=rm_vigdis_rainmore self.mem.lq_vigdis=freed priority=4]`: "You paid what I owed and broke what I made. I'll not forget either one. Neither will Dirk."
- Journal summaries: `trail` "Follow the light coin to the jeweler's on the Commons and out to the Kestrel workings." `silver_board` "Take the shears and die to the warden of the Market Hall." `silver_mercy` "Pay the jeweler's debt, and have the striker break the die." `silver_cut` "Let her clip, and take your share."

**Gossip and helpful knowledge**

| Town | Dialect | Key | Line |
|---|---|---|---|
| Larkfield | larkfield | `lq_silver=clipping` (helpful) | "Weigh your pennies on the anvil scales before you take them. Light coin's coming down the High Road, and top-weight liars with it." |
| Larkfield | larkfield | `lq_silver=clipping` and `world.mem.nh_stage=moot_called\|reckoning` | "Every wagon comes home short-weighed, and the factors want their money by the new moon. Weights and measures, somebody's thumb's on the scale." |
| Ridgemere `self.quarter=commons` | plain | `lq_beam=board` | "The Market Hall's grain beam was false, they say. A Counting House beam. The warden put it before the Board himself." |
| Ridgemere `self.quarter=commons` | plain | `lq_silver=board` | "The Market Board broke a coiner's dies. An old jeweler, they say, with a factor's chalk on her door. Sad business, all of it." |
| Larkfield | larkfield | `lq_silver=mercy` | "Somebody paid an old wolf's debt and broke her dies quiet. Light coin's gone and nobody's in the stocks. Fair price, that." |
| Southmeadow | southmeadow | `lq_silver=cut` | "There's still light coin about, my apple. Weigh it twice. Blight take whoever's shaving it." |

Recap (`inn.greet`, Larkfield, `lq_silver=clipping`): "Larkfield's grain comes home short-weighed and paid in light coin. The shopkeeper at the Stores wants somebody to find out who."

**Engine needs.** Runs today. C5 (named thanks), C8 (talk), C11 (keep Dirk, 74, from old age while the line runs, or guard his steps with `resident is alive` and send the wolf to the jeweler instead), C12a (the hushed beam to the running chapter), C14 (a lasting fate for Vigdis's post, optional), SQ2 (the tills), N2 (Pike's two thanks by the wolf's `lq_beam`; until then, the `board` thanks plays and the hushed wolf hears the cold `npc.line`).

---

## 7. Blight and Boar (`lq-blight-and-boar`)

| | |
|---|---|
| Tier | large: 3 chapters + a reckoning |
| Program | `lq-blight-and-boar`, open, cap 5000, prefix `lq` |
| Towns and places | **Southmeadow** (square `8_13` (100, 20), board `board:southmeadow`); the Southmeadow vineyard press `fs_vineyard_south_press` (door `w_ridgemere_9_12` (98, 105)); the Sunfields Bee Garth `fs_bees_south_hives` (door `7_12` (187, 31)); Southwood sties `fs_pigs_south_sties` (door `5_8` (97, 11)) |
| Giver | **Sloe Harvestby** `cty_sloe_harvestby`: f 26, tends the vines at Southmeadow, lives at the vineyard; southmeadow speech, light (sm120); short-tempered and frank; "saving coin toward something no one else knows about" (a row of vines of her own). She holds the quest and the cure road's choice. |
| Reward purse | `town:southmeadow` (236, day cap 23) |

**Premise.** Black leaves curl along the south rows of the Southmeadow vineyard, and by night wild boar root out what the blight has spared. The vines are Southmeadow's wine and half its winter coin. Find which rows the bees won't touch, hunt the boar out of the rows, find the broken fence that let them in, and then choose. Burn the sick rows and lose a vintage, or try a vine hand's bark-and-ash cure that may save it all, or may not.

**Cast**

| Role | Resident (id) | Voice | Hub item |
|---|---|---|---|
| hero, giver | Sloe Harvestby `cty_sloe_harvestby` | southmeadow (light) | quest `leaves`; road choice `lq_vine_cure` |
| tracker, healer | Harl Oakbough `tn_oh_harl_oakbough` (m 32, vine hand; gives medium `mq-vineyard-cellar`) | southmeadow (light) | **conversation and gift target only**: "once got lost in the deep wood for three days": knows the Southwood's paths; brings an Oakhaven bark cure |
| knowledge | Aster Cornley `cty_aster_cornley` (f 45, keeps bees at the Sunfields Bee Garth; a campaign target, which allows this one offer) | plain (southmeadow, accent none) | one `npc.offer`; restless, dry; the bees shun the sick rows |
| cause | Brisk Lakemere `cty_brisk_lakemere` (f 45, swineherd at Southwood) | plain (southmeadow, accent none) | one `npc.offer`; her sows broke out, and the wild boar came back with them |
| foes | boar (`hunted` target `boar`) | — | with L1, staged in the rows |

**Regions**

| id | cell | box | what |
|---|---|---|---|
| `rows` | `w_ridgemere_9_12` | (86, 93)–(110, 117) | the vine rows round the press |
| `hives` | `w_ridgemere_7_12` | (176, 20)–(198, 42) | the Bee Garth (only for a scene cue) |
| `southwood` | `w_ridgemere_5_8` | (84, 0)–(110, 24) | round the sties' door |

**Chapters and objectives**

*Chapter 1, `leaves`, "Black Leaves".* Scope player; giver Sloe, board `southmeadow`; once; `join.cap` 40.

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `walk` | Walk the vine rows. | region entered | target `rows` | 1 | |
| `bees` | Ask the beekeeper which rows her bees won't touch. | conversation | target `cty_aster_cornley` | 1 | |
| `ash` | Bring wood ash for the roots. | gift | target `cty_sloe_harvestby`, item `wood_ash` | 2 (quantity) | also counts `gift again` (a second objective `ash2`, optional, same match) |

*Chapter 2, `boar`, "The Boar in the Rows".* Scope party; journal; weekly; `join.cap` 40 (no spawn: the boar come from the wild).

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `hunt` | Hunt boar out of the vine country. | hunted | target `boar` | 2 | any cell. With L1, a party entering `rows` at dusk meets a staged boar |
| `track` | Ask the vine hand where they come from. | conversation | target `tn_oh_harl_oakbough` | 1 | his window line |
| `fence` | Find the broken fence at Southwood. | region entered | target `southwood` | 1 | `after: [track]` |
| `brisk` | Tell the swineherd her fence is broken outward. | conversation | target `cty_brisk_lakemere` | 1 | `after: [fence]` |
| `watch` | Keep the rows one night. | story signal | `rows_kept` | 1 | `hour` trigger 23–3: participants in `rows` |

**Shared goal.** Story number `rows_kept` (+1 per participant per night kept; +2 per `hunt` done). At ≥ `g` (base 8, + 2 per helper) → `enter reckoning`. Tracker: "Nights the rows were kept".

*Chapter 3, "Burn or Cure".* Two road quests by journal, each `closure: true`.

| Road quest | Objectives | World result |
|---|---|---|
| `vines_burn` "Burn the Sick Rows" | a `gift` of `firewood` 4 to `cty_sloe_harvestby`; be in `rows` at dusk (signal, `hour` 18–20) | `lq_vines = burned` (**default**) |
| `vines_cure` "Bark and Ash" | `dialogue choice` item `cty_sloe_harvestby`, detail `lq_vine_cure` (the choice sits on the giver); `forage` item `oak_bark` 3 (quantity); a `gift` of `oak_bark` 3 to `tn_oh_harl_oakbough`; a `gift` of `lime` 2 to `tn_oh_harl_oakbough` | `lq_vines = cured` or `lost` (a seeded `random` at the reckoning) |

**Branches and lasting consequences.**
- **burned.** `harvest.override {area: fruit, word: poor, days: 30, words: "the south rows burned for the blight"}` now.
  - Then, on the next `season begins`, `harvest.override fruit good 60 "the burned rows came back clean"`.
  - Key `lq_vines=burned`.
- **cure.** At the reckoning, `random` with weights 60 and 40. The weights shift by 10 toward "cured" for each 5 `vines_cure` finishers, up to 90 (an `if` ladder on `t_cure`).
  - **cured:** `harvest.override fruit good 30 "Southmeadow's vines saved"`; key `lq_vines=cured`.
  - **lost:** `harvest.override fruit poor 60 "the blight past the Southmeadow rows"`; key `lq_vines=lost`. Cure wolves keep their reward (they tried), and the Southmeadow board is kind about it.
- **Lasting:** the fruit harvest word, which every market's prices and the campaign's Ch 7 ("Harvest Under Arms") read through the economy. And the season's wine is plentiful or short.

**Rewards** (§1.5)

| When | Reward |
|---|---|
| `leaves` | deed "found the blight in the Southmeadow vines"; `story.practice labour` |
| `boar` (first time) | deed "hunted the boar out of the Southmeadow vines"; `story.practice watch`; `item.give boiled_leather 1`; 6 from `town:southmeadow` |
| a road done | 12 from `town:southmeadow`; `story.practice craft`. Burn: `item.give grapes 3`. Cure: `item.give wine 2` (cured) or `item.give grapes 2` (lost). Deeds: "burned the blighted Southmeadow rows to save the next vintage" / "saved the Southmeadow vines with bark and ash" / "fought the blight in the Southmeadow vines and lost" |

**Campaign feed.** Road done: `nh_realm +16`, `nh_voice_southmeadow +16` (8 for "lost").
- If `nh_stage ≥ 7` (Harvest Under Arms), the Southmeadow board adds *"Every grape picked is one the Captain's companies don't eat."*

**Dialogue** (Sloe and Harl speak southmeadow; Aster and Brisk plain)
- Quest `leaves` (Sloe):
  - **ask**: "Your vines look sick."
  - **say**: "#greet#, #address#. Not a good harvest, not this year. Black leaves on the south rows, curling like burnt paper, and boar in at night eating what's left. Blight take it. Walk the rows, ask the beekeeper which ones her bees won't touch, and bring me wood ash for the roots."
  - **accept**: "I'll walk the rows." / **decline**: "Not today." / **handIn**: "The bees shun the south rows."
  - **thanks**: "#thanks#. The south rows, then, and spreading north. Sheaf and sickle, I've coin saved for a row of my own and it's dying before I can buy it. The boar first. Hunt them out of my rows."
- `npc.line` Sloe (`open`): "Harvest to you. Mind where you tread in the south rows. The leaves are blighted and the ground's all boar-dug."
- `npc.line` Sloe (`after_cured`): "Ripe days, my apple, ripe days. Green leaves on every row, and a bark poultice on every root."
- `npc.offer` Sloe (`lq_vine_cure`, while the wolf's `vines_cure` is active): **"The vine hand knows a cure. Let's try it."** → "Oak bark and lime on a sick root, the Oakhaven way? It's a gamble, my apple. Ripe or rotten. Bring him three of bark and two of lime, and I'll hold the rows till he's done."
- `npc.offer` Aster (`lq_vine_bees`, once): **"Which rows do your bees avoid?"** → "The south rows, every one. Bees know a sick flower before the vine does. Three weeks they've flown round those rows like there's a ditch in the way."
- `npc.offer` Brisk (`lq_vine_fence`, once): **"Your fence is broken outward."** → "Outward, is it? My sows went courting and the wild boar came back with them. I've mended it twice. Every boar in the Sunfields knows the way to our sties now."
- **Window lines** (`resident.tree` `hub`):
  - `say[self.id=tn_oh_harl_oakbough world.mem.lq_vines=blight priority=4]` "Down from the Southwood, along the stream, under the old fence. I was lost three days in that wood once. I know its paths better than I'd like. Good harvest to you if you find the gap."
  - `say[self.id=tn_oh_harl_oakbough world.mem.lq_vines=reckoning priority=4]` "Where I was raised they paint a sick root with oak bark and lime, and pray to the Bright One it takes. Three of bark and two of lime, and I'll do the painting."
- Journal summaries: `boar` "Hunt the boar out of the Southmeadow rows, and find where they get in." `vines_burn` "Burn the sick south rows before the blight spreads. No wine from them this year." `vines_cure` "Gather oak bark and lime for the vine hand's old cure. It may save the rows, or not."

**Gossip and helpful knowledge**

| Town | Dialect | Key | Line |
|---|---|---|---|
| Southmeadow | southmeadow | `lq_vines=blight` (helpful) | "If you hunt the boar, go in by the Southwood stream. They come up the rows from there, full-bellied and slow." |
| Southmeadow | southmeadow | `lq_vines=burned` | "They burned the south rows for the blight. No wine from them this year, but clean ground. Ripe days next year, Bright One willing." |
| Larkfield | larkfield | `lq_vines=cured` | "Southmeadow saved its vines with bark and lime, they say. Fair-priced wine this autumn, if it's true." |
| Ridgemere | plain | `lq_vines=lost` | "Southmeadow wine'll be dear this year. The blight got past them, for all they tried." |

Recap (`inn.greet`, Southmeadow, `lq_vines=blight`): "Blight in the south vine rows and boar in at night. The vine-tender wants help before the harvest's gone."

**Engine needs.** **L1** `hunt.stage` (Phase 5; else `hunt` waits on the wild's own boar, rarity 3). C8 for talk.

---
## 8. The Runnel Fever (`lq-runnel-fever`)

| | |
|---|---|
| Tier | large: 3 chapters + a reckoning. **The city's one cough line**: the old medium `mq-net-court-cough` is withdrawn into it (decisions §2.1), and it takes over that quest's Ch 4 foreshadowing and the Honey Crock's thyme syrup. |
| Program | `lq-runnel-fever`, open, cap 5000, prefix `lq` |
| Towns and places | **Ridgemere**: the Sump Infirmary `rm_the_sump_infirmary` (door `0_0` (249, 250)); Cistern Court and the Runnel (world (270, 278) = `w_ridgemere_1_1` (14, 22)); the Washhouse `r2_the_washhouse` (door `0_1` (210, 79)); the Wharf Tannery `rm_the_wharf_tannery` (door `0_1` (192, 184)); Hollis Physic `rm_hollis_physic` (door `1_1` (167, 0)); the Mortar and Pestle `rm_the_harness_shop` (door `1_1` (208, 70)); the Honey Crock `rm_the_cough_jar` (its id is old; door `1_1` (46, 68)); board `board:ridgemere` |
| Giver | **Theodora Wickley** `rm_theodora_wickley`: f 55, nursing for the abbey at the Sump Infirmary; plain speech (ridgemere, accent none; rm213); quiet, tests a latch before closing a door; "grew up at a Timberland charcoal camp in the hills". The nurse with too many coughing pups and too few cots. She gives only this line. She is not the infirmarian: Gorm Plankgate (the campaign's Wardens' Eye) is named only in gossip, as "the infirmarian". |
| Reward purse | `town:ridgemere` (the treasury, 1,355, day cap 135); Hollis Physic's payments from the treasury until SQ2 |

**Premise.** A wet cough is going round the courts by Cistern Court, worst in the pups who drink from the Runnel. Theodora has more coughing pups than cots, and not a spoon of thyme syrup left. Carry remedies, walk the water back to where it goes bad, and find two culprits. One is careless: since the spring flood the Wharf Tannery's lime-pits drain into the Runnel. One is greedy: an apothecary sells a "Runnel draught" of chalk and mint. Then decide who makes the water clean.

**Cast**

| Role | Resident (id) | Voice | Hub item |
|---|---|---|---|
| heroine, giver | Theodora Wickley `rm_theodora_wickley` | plain | quest `cough` |
| witness | Ivo Fogstack `rm_ivo_fogstack` (m 20, keeps the Washhouse) | plain (broad) | one `npc.offer`; his washing comes out stiff with lime |
| careless | Dunstan Greywick `rm_dunstan_greywick` (m 44, tanner, runs the Wharf Tannery) | plain (broad) | one `npc.offer` (the soakaway); warm but guarded; "lost a brother on the wharf"; won't shut the tannery, will dig a soakaway |
| villain | Rupert Fogton `rm_rupert_fogton` (m 32, apothecary at Hollis Physic) | plain (broad) | two `npc.offer`s in sequence; cold, guarded; "saving for a stall": sells hope in a bottle |
| honest healer | Halvard Ashley `rm_halvard_ashley` (m 23, apothecary at the Mortar and Pestle) | plain (light) | one `npc.offer`; Sump-born, short-tempered; tests the draught |
| the syrup | Liv Slagstack `rm_liv_slagstack` (f 42, preserver at the Honey Crock; "learned preserving from a Fell cook and swears by honey"; coughs in the damp herself) | plain (broad) | **gift target only** (decisions §2.1): brews thyme syrup from herbs |
| the sick (`lq_sick`, a `residents` list) | Piet Wickridge `rm_piet_wickridge`, Tristram Rainley `rm_tristram_rainley`, Mathis Plankham `rm_mathis_plankham`, Rupert Cindergate `rm_rupert_cindergate`, Lotte Bilgeton `rm_lotte_bilgeton`, Mogens Greycot `rm_mogens_greycot`, Orm Fogmore `rm_orm_fogmore` (11), Josephine Wetmore `rm_josephine_wetmore` (9), Runa Ropemarsh `rm_runa_ropemarsh`, Magnus Greycot `rm_magnus_greycot`, Dunstan Slagley `rm_dunstan_slagley`, Humphrey Sootton `rmx_humphrey_sootton` (8) | — | gift targets only; all placed and alive, living in Oakum Stair, Rag End and Slop Rooms, whose doors open within 40 tiles of Cistern Court |

**Regions**

| id | cell | box | what |
|---|---|---|---|
| `cistern` | `w_ridgemere_1_1` | (4, 12)–(24, 32) | Cistern Court and the Runnel |
| `drain` | `w_ridgemere_0_1` | (182, 174)–(202, 194) | the yard by the tannery door |

**Chapters and objectives**

*Chapter 1, `cough`, "The Runnel Cough".* Scope player; giver Theodora, board `ridgemere`; once; `join.cap` 40.

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `bark` | Bring the infirmary willow bark. | gift | target `rm_theodora_wickley`, item `willow_bark` | 2 (quantity) | |
| `syrup` (optional) | Take herbs to the preserver at the Honey Crock for thyme syrup. | gift | target `rm_liv_slagstack`, item `herbs` | 2 (quantity) | on completion `item.give thyme_syrup 2`, which the `houses` step takes; her window line |
| `houses` | Carry a remedy to three coughing households. | story signal | `remedy` | 3 | trigger on `gift` (participant) with `in list lq_sick {field: target}` and the item any of `herbs`, `willow_bark`, `willow_tea`, `thyme_syrup`, `yarrow_poultice`, `bone_broth`, `herb_tea`. Once per wolf per resident (player `residents` list) |
| `wash` | Ask the washhouse keeper if his water smells wrong. | conversation | target `rm_ivo_fogstack` | 1 | |

*Chapter 2, `water`, "Bad Water".* Scope player; journal; weekly; `join.cap` 40.

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `court` | Walk Cistern Court and the Runnel. | region entered | target `cistern` | 1 | |
| `drain` | Find where the tannery drains. | region entered | target `drain` | 1 | |
| `tanner` | Ask the tanner about his lime-pits. | conversation | target `rm_dunstan_greywick` | 1 | `after: [drain]` |
| `draught` | Buy the apothecary's Runnel draught. | trade done | target `rm_rupert_fogton` | 1 | anything bought there counts as the draught |
| `test` | Have the honest apothecary test it. | conversation | target `rm_halvard_ashley` | 1 | `after: [draught]` |

**Shared goal.** Story number `cured` (+1 per `houses` step done; +2 per `water` done). At ≥ `g` (base 10, + 2 per helper) → `enter reckoning`. Tracker: "Coughs eased".

*Chapter 3, "Clean Water".* Two road quests, plus an optional third every wolf can do. The two roads are `closure: true`; the optional third is not, so the line counts once.

| Road quest | Objectives | World result |
|---|---|---|
| `runnel_cover` "A Cover for the Cisterns" | `project gift` to the Cistern Court cover, goal 3 (quantity) | `lq_runnel = covered` (**default**) when the project is built |
| `runnel_soak` "The Tanner's Soakaway" | `dialogue choice` item `rm_dunstan_greywick`, detail `lq_run_soak`; a `gift` of `bricks` ≥ 4 to `rm_dunstan_greywick` | `lq_runnel = drained` |
| `runnel_draught` (optional, any road) "The Draught Money" | `dialogue choice` item `rm_rupert_fogton`, detail `lq_run_payback` | npc key `lq_rupert=paid`; restitution, below |

**Branches and lasting consequences.**
- **covered.** At the reckoning (or at once, if the first road chosen is `runnel_cover`), run `project.post {town: ridgemere, project: cover, cell: w_ridgemere_1_1, x: 14, y: 22, title: "Cistern Court cover"}`.
  - **Guard:** a city holds at most two open projects, and proj-3 (the Ridgemere waystation, Line post 12) already stands open in the world (decisions §4.5), so the cover fits beside it. The post carries `ifRefused: "skip"`, and the story switch `cover_posted` is turned on only when the `project posted` event for the cover arrives; while it is off, a `day begins` trigger tries again. So the cover never pushes a campaign project out.
  - The `project built` trigger completes every `runnel_cover` copy, `npc.line`s change, and the key becomes `lq_runnel=covered`.
  - **Lasting:** the cover stands in the world for good.
- **drained.** Dunstan digs. The key becomes `lq_runnel=drained` at once, with no build wait. This is the fast road for a small world.
- **The draught money.** Each `runnel_draught` wolf gets 4 "Hollis Physic pays the coughing houses back" (today `town:ridgemere`; after SQ2 `till:job:rm_rupert_fogton`; story counter cap 40). A follow-on objective (story signal) completes when that wolf next gifts any item to anyone in `lq_sick`. Rupert's npc key is `lq_rupert=paid`, and his `npc.line` turns sullen.
- **Stopping the cough.** When `lq_runnel` is set, a plain `story.tell {to: region, region: cistern}` narrates "The Runnel runs clear past Cistern Court." (no `from`, no quoted words). The Ridgemere board posts *"THE SUMP INFIRMARY: the Runnel water is safe again. Boil it a week more all the same. Sister Theodora."*, and Theodora's own words go in her phase `npc.line`.

**Rewards** (§1.5)

| When | Reward |
|---|---|
| `cough` | deed "carried remedies to the coughing courts of the Sump"; `story.practice service`; `item.give bandages 2` |
| `water` (first time) | deed "found what was fouling the Runnel"; `story.practice travel`; 6 from `town:ridgemere` |
| a road done | 8 from `town:ridgemere` (+ the 4 draught money for a wolf who also took that road); `item.give theriac 1`; `story.practice labour`. Deeds: "helped raise a cover over Cistern Court" / "dug the tanner a soakaway and cleaned the Runnel" |

**Campaign feed.** Road done: `nh_realm +8`, `nh_voice_ridgemere +8`.
- **The Residence's cough** (from the old medium Q5; campaign `nh_b_cough`, Ch 4). While `nh_stage ≥ 4` and `nh_b_cough` is off (the Princess's cough not yet tended), the close of each chapter mentions it: Theodora's phase `npc.line` reads the `_residence` variant below, and her window line `say[self.id=rm_theodora_wickley world.mem.nh_stage=howls|letters|lights|harvest|bell|moot priority=4]` plays the same words in her hub. It points wolves at the campaign's Ch 4 side story without giving it.

**Dialogue** (all plain)
- Quest `cough` (Theodora):
  - **ask**: "You look worn through."
  - **say**: "#greet#, #address#. Every cot is full and half of them are pups. A wet cough, from the courts round the cisterns, worst in those who drink from the Runnel, and not a spoon of thyme syrup left. Bring me willow bark, carry what I can spare to the coughing houses, and ask the washhouse keeper if his water smells wrong."
  - **accept**: "I'll carry remedies." / **decline**: "I can't today." / **handIn**: "The washhouse water smells of lime."
  - **thanks**: "#thanks#. Lime. Then it isn't the damp alone, it's the water. Bright One keep the pups. Walk the Runnel back to where it goes bad, and find who's fouling it."
- `npc.line` Theodora (`open`): "Mind the Runnel water. Boil it, or don't drink it. We've pups coughing in every cot."
- `npc.line` Theodora (`open`, `_residence` variant, if `nh_stage ≥ 4` and `nh_b_cough` off): "Boil the Runnel water. And they say the cough's up at the Residence too. Damp doesn't care whose lungs it gets."
- `npc.line` Theodora (`after_covered`): "Half the cots are empty this week. Bright One bless whoever carried stone for that cover."
- `npc.line` Theodora (`after_drained`): "The Runnel runs clean. Boil it a week more all the same. I've learned not to trust water twice."
- `npc.offer` Ivo Fogstack (`lq_run_wash`, once): **"Does your water smell wrong?"** → "Sharp, like a lime-kiln, since the spring flood. The wash comes out stiff. I told the tannery. They said their pits don't leak. Pits always leak."
- `npc.offer` Dunstan (`lq_run_soak`): **"Your lime-pits drain into the Runnel."** → "Do they? The flood shifted the old culvert, I'll grant that. I can't shut the tannery, friend. Half the Sump wears my leather. Bring me four of bricks for a soakaway and I'll dig it myself."
- `npc.offer` Rupert (`lq_run_rupert`, once): **"Does your Runnel draught do anything?"** → "It does what the label says. It soothes. Folk want something in a bottle when their pups cough, and I sell bottles. Is that a crime now?"
- `npc.offer` Rupert (`lq_run_payback`, with `runnel_draught`): **"Pay back the Sump for every bottle."** → "Every bottle? Bright One, you'd beggar me. Here. Four pennies, and carry them to the courts yourself, since you're so fond of them."
- `npc.offer` Halvard (`lq_run_test`, once): **"What's in the Runnel draught?"** → "Chalk, mint and river water. Runnel water, likely, which is the joke of it. Willow bark and thyme syrup do more good. Tell the nurse I'll make them at cost."
- **Window line** `say[self.id=rm_liv_slagstack world.mem.lq_runnel=fever priority=4]`: "Thyme syrup? Bring me herbs and I'll brew it the old way, with honey, not sugar. A spoon at night and one at dawn. I cough in the damp myself."
- Journal summaries: `water` "Walk the Runnel back to where it goes bad, and find who's fouling it." `runnel_cover` "Carry stone, timber and lime for a cover over Cistern Court." `runnel_soak` "Bring the tanner bricks, and he'll dig a soakaway for his pits." `runnel_draught` "Make the draught-seller pay the coughing houses back."

**Gossip and helpful knowledge**

| Town | Dialect | Key | Line |
|---|---|---|---|
| Ridgemere `self.quarter=sump` | plain | `lq_runnel=fever` (helpful) | "Boil Runnel water before the pups drink it. The cough's from the water, not the damp. The honest apothecary sells thyme syrup at cost." |
| Ridgemere `self.quarter=sump` | plain | `lq_runnel=fever` (helpful) | "If the infirmary's out of syrup, the preserver at the Honey Crock can brew it. Bring her herbs; she's honey enough." |
| Ridgemere | plain | `lq_runnel=covered` | "There's a cover over Cistern Court now, stone and timber. Wolves from all over carried for it, and the coughing's eased." |
| Ridgemere `self.quarter=sump` | plain | `lq_runnel=drained` | "The tanner dug a soakaway and the Runnel runs clean again. It smells of the sea now, not the kiln." |
| Coldwater | coldwater | `lq_runnel=fever` | "They've a cough in the Ridgemere Sump, eh? From bad water. Calm water's a blessing, neighbour." |

Recap (`inn.greet`, Ridgemere): "There's a cough in the Sump courts by the cisterns. The nurse at the Sump Infirmary wants willow bark and legs."

**Engine needs.** Runs today, within the project cap (`project.post` cover; two open projects per city). C8 for talk, C12a for chapter credit, J1 for per-wolf offers, SQ2 for Hollis Physic's own till.

---

## 9. The Wreckers' Light (`lq-wreckers-light`)

| | |
|---|---|
| Tier | large: 3 chapters + a reckoning. Chapter 1 takes in the old medium `mq-false-light` (decisions §2.2), which is withdrawn from the medium pack: its fog on accept, the true bearing from the Bay Light's lamp room, and the douse-or-wait choice on the rocks. |
| Program | `lq-wreckers-light`, open, cap 5000, prefix `lq` |
| Towns and places | **Ridgemere**: the North Headland and the Mole (`w_ridgemere_0_0`), the Bay Light `r2_the_bay_light` (door (71, 179)) and its lamp room `r2_the_bay_light_up` (stairs inside), the Lamp Store `r2_the_lamp_store` (door `1_1` (21, 15)), the Grayrock Wharf Office `rm_the_harbourmaster_s_office`, the Watch Office `r2_the_keep` (cells `r2_the_keep_down`), the Chapel of the Tide `rm_the_chapel_of_the_tide`, Salt Row (`r2_salt_row_house`); **Cedar Sound**: North Cove's fishing shed `fs_fishing_northcove_shed` (door `w_ridgemere_0_4` (157, 49)) |
| Giver | **Linden Sedgeditch** `cty_linden_sedgeditch`: f 55, skipper at North Cove; plain speech (ridgemere, light; rm397); dry and calm; "knows a story about one of the great Houses that would be unwise to repeat". The story she won't repeat is the one this line uncovers. |
| Reward purse | `town:ridgemere` (treasury) |

**Premise.** In fog and in a blow, a lamp burns low on the North Headland rocks where no lamp should be. Ships standing in for the Bay Light take it for the Mole and turn onto the rocks, and two Grayrock lighters have gone that way since spring. Linden has pulled their cargo out of North Cove in her nets. Take the true bearing from the Bay Light, find the false lamp, trace its oil to the city's own Lamp Store, and follow an old lamplighter on her dusk round. She has been paying the Wharf back, one cargo at a time, for a brother it wrote down as "cargo lost".

**Ilse Coalley's place in the design** (decisions §2.2). Ilse is **only** the wronged lamplighter of this line. The Lamp Store's four lamplighters are Ilse, Corwin Bilgestack, Rupert Cindergate and Edda Gullford. Medium Q1 `mq-sump-lamps` is Edda's, and its oil-drainer is Piet Lumley, who takes Sump flasks for his debt (Ch 1–2). No line of Q1 names or describes Ilse; Edda speaks of "the other lamplighters" in general. Ilse takes Lamp Store oil for the headland lamp, and this line's oil chapter is her story alone (its flavour matches the campaign's Ch 5–6). The two oil stories never meet. No payment here goes through the Lamp Store's till `till:job:rm_ilse_coalley`.

**Cast**

| Role | Resident (id) | Voice | Hub item |
|---|---|---|---|
| heroine, giver | Linden Sedgeditch `cty_linden_sedgeditch` | plain (light) | quest `lamp` |
| villain (wronged) | Ilse Coalley `rm_ilse_coalley` (f 61, lights the lamps at dusk; lives in Salt Row) | plain (light) | two `npc.offer`s in sequence; curious and anxious, counts the bells from the Chapel of the Tide; "lost a brother on the wharf and does not forgive House Grayrock" |
| witness | Corwin Bilgestack `rm_corwin_bilgestack` (m 60, lamplighter at the Lamp Store; gives small `sq-dawn-bell`) | **plain** (larkfield, accent none; critic P2-20) | **conversation target only**: quiet, guarded, counts the oil |
| witness | Septimus Plankmarsh `rm_septimus_plankmarsh` (m 29, clerk at the Grayrock Wharf Office; the old medium Q7's giver) | coldwater (light) | one `npc.offer` (`lq_wr_ledger`); playful and wry, "can name every ship in the Sound": reads out "cargo lost". A gift target on the wharf road. |
| neighbour | Ivo Cinderham `rm_ivo_cinderham` (m 40, crane hand; lives upstairs in Salt Row) | plain (light) | one `npc.offer`; has seen her go out in every blow |
| the Watch | Bettany Nailley `rm_bettany_nailley` (campaign hero) | plain (light) | **a conversation and gift target only**, no offer: she hates Grayrock and must lock up its wrecker |
| the Tide | Solveig Tarwell `rm_solveig_tarwell` (campaign) | plain | **a conversation target only**, and a scene speaker |
| foes | the salvage gang that strips wrecks at North Cove (`story.area.spawn`) and comes to see what the lamp has caught (`bandits.call`) | — | |

**Regions**

| id | cell | box | what |
|---|---|---|---|
| `rocks` | `w_ridgemere_0_0` | (105, 84)–(112, 96) | the North Headland's rock shore, below the walls (the old medium's `fl_rocks`) |
| `headland` | `w_ridgemere_0_0` | (106, 96)–(130, 120) | headland rock outside the walls (600 open, 10 tiles of deep water at its sea edge, never a spawn tile) |
| `cove` | `w_ridgemere_0_4` | (145, 37)–(169, 61) | North Cove round the shed |
| `cove_b` | `w_ridgemere_0_4` | (144, 16)–(163, 35) | the cove's north beach (363 open, 48 path): the second spawn region, used in turn |
| `mole` | `w_ridgemere_0_0` | (60, 170)–(80, 190) | the Mole's road to the Bay Light (Ilse's dusk round passes here) |

**Chapters and objectives**

*Chapter 1, `lamp`, "A Lamp Where None Should Be".* Scope player; giver Linden, board `ridgemere`; once; `join.cap` 12 (it places a unique piece: the lamp burns at one of two spots in turn, (108, 90) in `rocks` and (118, 108) in `headland`, so two parties can each find one).
- **Setup.** On `quest accepted`: `weather.set {place: w_ridgemere_0_0, weather: fog, hours: 6}` with `ifRefused: "skip"` (on a hosted world it needs a Dungeon Master), at most once a game day (story timer `fogged`).

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `cove` | Walk North Cove and see what the nets brought up. | region entered | target `cove` | 1 | |
| `bearing` | Climb to the Bay Light's lamp room and take the true bearing. | enter place | target `r2_the_bay_light_up` | 1 | |
| `rocks` | Find the false light on the North Headland after dark. | story signal | `rocks_found` | 1 | `after: [bearing]`. `region entered` `rocks` or `headland`, `hour between 19 5`: `item.place oil_lamp 1` at that region's spot (if no lamp lies there), `story.ping` "a light on the headland" for 60 minutes, and `start timer wait` (player). With `weather is storm` or `rain`, the lamp burns every hour until 3:00 |
| `end` | Put out the false light, or wait for whoever comes to it. | story signal | `lamp_end` | 1 | `after: [rocks]`; the choice is made by what the wolf does (below) |
| `wreckers` (hidden) | Bring down the salvage gang at the lamp. | bandit falls | cell `w_ridgemere_0_0` | 2 | revealed by the wait road |
| `take` | Take the lamp off the rocks. | item picked up | item `oil_lamp`, cell `w_ridgemere_0_0` | 1 | `after: [rocks]` |
| hand-in | to Linden | | | | `handIn: true` |

- **Douse it.** The wolf picks up the lamp at once (`item picked up` `oil_lamp` in `rocks` or `headland`, with the `wait` timer under 0.04 days). That sets the player text `lq_lamp1 = doused` and completes `end` and `take`.
- **Wait for them.** The wolf leaves the lamp burning and stays in the region. On each `hour`, an `each over participants` finds the wolves still there with `lq_lamp1` empty and the `wait` timer past 0.04 game days (about a game hour). For each, `bandits.call` scaled by party size (1 → 2, 2–3 → 4, 4+ → 6), `lq_lamp1 = waited`, and `objective.reveal wreckers`. When `wreckers` completes it emits `lamp_end`; the wolf then takes the lamp. A waited wolf adds `nh_threat −1` (clamped).

*Chapter 2, `oil`, "The Lamplighter's Oil".* Scope player; journal; weekly; `join.cap` 12 (the salvage fight spawns in `cove` and `cove_b`, in turn).

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `store` | Find where that oil is kept. | enter place | target `r2_the_lamp_store` | 1 | |
| `corwin` | Ask the lamplighter who counts the oil who draws extra. | conversation | target `rm_corwin_bilgestack` | 1 | his window line |
| `ledger` | Ask at the Wharf Office about the lost lighters. | conversation | target `rm_septimus_plankmarsh` | 1 | |
| `shadow` | Follow the lamplighter on her dusk round, two evenings. | story signal | `shadowed` | 2 | `near resident` (watched; match target `rm_ilse_coalley`) with `hour between 17 22`; once per wolf per game day |
| `neighbour` (hidden) | Ask her neighbour on the stair. | conversation | target `rm_ivo_cinderham` | 1 | revealed after one `shadow` |
| `salvage` (optional) | Drive the salvage gang off North Cove. | bandit falls | cell `w_ridgemere_0_4` | 2 | the gang spawns in `cove` or `cove_b` on a `ship stormbound` event, or when the DM signals `wreck` |

**The wreck night** (a world event once a season):
- When `lamp` completions reach ≥ 3, a `random` over five branches runs `ship.storm` on one literal ship (`s21`…`s25`, the Fair Wind, the Lark, the Salt Rose, the Wavecutter, the Petrel), then `weather.set {town: ridgemere, weather: storm, hours: 12}` (`ifRefused: "skip"`).
- That night the false lamp burns every hour. The cove gang spawns twice, once in each cove region, scaled by `count in region`.
- With C1 the stormed ship could be the next one `ship posted` (`remember from: actor`).

**Shared goal.** Story number `lamps_taken` (+1 per `take`; +2 per `shadow` completed). At ≥ `g` (base 5, + 1 per helper) → `enter reckoning`.

*Chapter 3, "The Drowned Brother".* Three road quests by journal, each `closure: true`. Every road quest starts with `face`: `dialogue choice` item `rm_ilse_coalley`, detail `lq_wr_face`.

| Road quest | Objectives after `face` | World result |
|---|---|---|
| `wreck_tide` "Name Him at the Tide" | `dialogue choice` item `rm_ilse_coalley`, detail `lq_wr_tide` (she follows the wolf: `npc.follow`); `conversation` target `rm_solveig_tarwell`. A trigger completes the quest when `resident in place` `rm_ilse_coalley` is in `rm_the_chapel_of_the_tide` while the wolf is there (`wolf in place`) | `lq_wreck = named` (**default**) |
| `wreck_watch` "Hand Her to the Watch" | `conversation` target `rm_bettany_nailley`; a `gift` of `oil_lamp` 1 to `rm_bettany_nailley` | `lq_wreck = watch` |
| `wreck_wharf` "Tell the Wharf Office" | a `gift` of `oil_lamp` 1 to `rm_septimus_plankmarsh` | `lq_wreck = wharf` |

**Branches and lasting consequences.**
- **named.** A public scene `tide_naming` in the Chapel of the Tide (cast Solveig and Ilse, each in their own panel with their face; a line beat each; no choice). Ilse "puts the lamp out". From then on no false lamp is placed. Key `lq_wreck=named`.
  - This prefigures the campaign's Ch 5 "The Names on the Water". If the campaign has already set `nh_b_wharf` on, Solveig's beat names "the wharf's dead read in spring" as well.
- **watch.** `npc.move rm_ilse_coalley` to `r2_the_keep_down`, then `npc.hold`. A `wait 7` game days, then `npc.release`. Holds are undone when a run ends, so each new visit of `after_watch` re-applies the hold if the week has not passed (a story timer).
  - L4 `npc.sentence` (Phase 8) would make this lasting.
  - Bettany's words go in her own window: her line `say[self.id=rm_bettany_nailley world.mem.lq_wreck=watch priority=4]` in `watch.tree` `hub` (below), and with C5 an `npc.greet` to each `wreck_watch` wolf. Key `lq_wreck=watch`.
- **wharf.** `standing.adjust house_grayrock +3` for wolves with a Chapter (B; skipped otherwise). Talk says Ilse was turned out of the Lamp Store; with C14 `npc.post none` she really is. The key `lq_wreck=wharf` gives the Sump a cold line for those wolves (their deed's gossip).
- **Lasting:** the world text; the false lamp gone for good (named), for a season (watch) or for good (wharf).

**Rewards** (§1.5)

| When | Reward |
|---|---|
| `lamp` | deed "took a wrecker's lamp off the North Headland" (doused) or "caught wreckers at their false light on the North Headland" (waited); `story.practice watch`; `item.give smoked_fish 3` ("from Linden's nets") |
| `oil` (first time) | deed "traced the false lamp's oil to the Lamp Store"; `story.practice service`; 6 from `town:ridgemere` |
| a road done | 12 from `town:ridgemere` (the wharf road's reason: "Grayrock's thanks, through the treasury"); `story.practice travel`; `item.give oilcloth_hat 1`. Deeds: "brought a wrecker to the Chapel of the Tide to name her drowned brother" / "handed the headland wrecker to the Watch" / "told the Grayrock Wharf Office who lit the false lamp" |

**Campaign feed.**
- `lamp`: `nh_realm +4`, `nh_voice_ridgemere +4` (the old medium's feed).
- Road done: `nh_realm +8`, `nh_voice_ridgemere +8` (2 for wharf).
- Foreshadow: if `nh_stage ≥ 6` (The Lights in the Fen), the journal note at `take` reads "A false light to wreck ships. The oil smells of eel fat. In the fens, they say, the false lights float."

**Dialogue** (plain unless noted)
- Quest `lamp` (Linden):
  - **ask**: "Your nets came up heavy?"
  - **say**: "#greet#, #address#. Heavy with Grayrock cargo, two lighters' worth since spring. Both ran onto the North Headland in fog, steering for a light low on the rocks, as if it were the Mole. Take the true bearing from the Bay Light's lamp room, walk the cove, then find me that other light after dark."
  - **accept**: "The lamp room, then the rocks after dark." / **decline**: "Not tonight." / **handIn**: "Here's the lamp from the rocks."
  - **thanks**: "#thanks#. A lamp in a hood, set low to look like the Mole. City lamp oil, the Lamp Store's own; I'd know the stink anywhere. One of the lamplighters, then. I know a story about that House and the wharf that I won't repeat. Follow the oil, gently."
- `npc.line` Linden (`open`): "Mind the North Headland in fog. There's a light there that wants ships."
- `npc.line` Linden (`after_named`): "The headland's dark in a blow now, the way it should be. Somebody said a name out loud, I hear."
- **Window line** `say[self.id=rm_corwin_bilgestack world.mem.lq_wreck=lamp priority=4]` (`resident.tree` `hub`, plain): "Full measure for every one of us, and one draws a jar more on blowy days. Says the wind snuffs her wicks. I've not said a word, and I'd thank you not to say I did."
- `npc.offer` Septimus (coldwater; `lq_wr_ledger`, once): **"What became of the lighters' crews?"** → "Crews? Here it says cargo lost, eh, and a line under it. Same words as the one that went down in the bay years back. A crew of three, I think, and the ledger calls them cargo. Rough water, to be written down like that."
- `npc.offer` Ivo Cinderham (`lq_wr_stair`, once): **"Does your neighbour go out in a blow?"** → "Every blow, with a lamp and a jar, and back at dawn soaked through. She cries on the stair sometimes. Her brother was on a Grayrock lighter, years back. Nobody ever came to say sorry."
- Ilse:
  1. `lq_wr_face` **"You lit the lamp on the headland."** → "And they wrote my brother down as cargo. Cargo lost, a line under it. I light a lamp so the Wharf loses cargo too. Nobody drowns, mostly. I see to that. Mostly."
  2. `lq_wr_tide` (with `wreck_tide`) **"Come and name him at the Tide."** → "Out loud, in the Chapel of the Tide? All these years and nobody once asked his name. Walk with me, then. I'll put the lamp out on the way."
- **Window line** `say[self.id=rm_bettany_nailley world.mem.lq_wreck=watch priority=4]` (`watch.tree` `hub`): "A wrecker of Grayrock ships, and I'm the one who locks her up. The Bright One has a sense of humour. Thank you. I mean it."
- Scene `tide_naming`, beats (each speaker in their own panel): Solveig: "We name the drowned at the Tide, whoever wrote them down as cargo." Ilse: "He was my brother. He was a lighterman. He was not cargo." Solveig: "Then the tide has him by his name, and the Bright One with it."
- Journal summaries: `oil` "Find where the false lamp's oil comes from, and follow the lamplighter who draws extra." `wreck_tide` "Bring the lamplighter to the Chapel of the Tide to name her drowned brother." `wreck_watch` "Take the false lamp to the Watch sergeant who hates Grayrock most." `wreck_wharf` "Take the false lamp to the Grayrock Wharf Office."

**Gossip and helpful knowledge**

| Town | Dialect | Key | Line |
|---|---|---|---|
| Ridgemere `self.quarter=wharf` | plain | `lq_wreck=lamp` (helpful) | "Fog like this, watch the North Headland, not the Mole. The sailors swear there's a second light down on the rocks." |
| Ridgemere `self.quarter=wharf` | plain | `lq_wreck=lamp` (helpful) | "If you go out on the headland at night, go two together. Somebody lights a lamp on the rocks, and the salvage gangs come after." |
| Ridgemere | plain | `lq_wreck=named` | "The Chapel of the Tide named a lighter crew the Wharf wrote down as cargo. The lamp on the headland's out." |
| Ridgemere `self.quarter=sump` | plain | `lq_wreck=watch` | "They've the headland wrecker in the cells. An old lamplighter. Half the Sump is sorry for her." |
| Coldwater | coldwater | `lq_wreck=wharf` | "Grayrock turned out an old lamplighter over the wrecks, eh? Rough water for her." |

Recap (`inn.greet`, Ridgemere): "Ships are going onto the North Headland in fog. A skipper at North Cove says somebody lights a lamp there."

**Engine needs.** L4 (`npc.sentence`) or C14 for a lasting fate; C1 for storming the next posted ship; C5 for Bettany's thanks by name; C8 for talk; J1 for per-wolf offers; N2 for Linden's thanks by `lq_lamp1`. Today's version works with the re-applied hold and the five-branch `random`.
---

## 10. The Long Run (`lq-long-run`)

| | |
|---|---|
| Tier | large: 3 chapters; the second is a realm-wide relay once a season |
| Program | `lq-long-run`, open, cap 5000, prefix `lq` |
| Towns and places | every square and board, from **Oakhaven** on Cedar Sound to **Saltwell** on the Red Waste, with the Mirebridge crossroads (`w_ridgemere_5_11` (188, 52)) as the ninth stop; **Ridgemere**: the Couriers' Office `rm_the_hall_of_records` (door `1_1` (38, 114)), the Carters' Yard `r2_the_carters_yard`, the Administrative Wing `r2_the_administrative_wing` |
| Giver | **Grete Dimmock** `rmx_grete_dimmock`: f 31, carries for the Couriers' Office; plain speech, broad (rm442); all the realm's couriers and the Carters' Yard keeper are in this line, so no other pack uses them (decisions §1.3); content, calm and warm; "grew up at a Timberland charcoal camp in the hills". A courier who loves the roads and wants the Office to keep its charter. |
| Reward purse | `town:ridgemere` (treasury, 1,355, day cap 135) |

**Premise.** The Council means to give the realm's post to one hand, and the Carters' Yard swears its wagons beat paws. The Couriers' Office has bet its charter on the Long Run: one sealed courier's tube, carried paw to paw through every square from the Sound to the Waste in a single day. Run a trial leg, then run the relay. Hand the tube on at each square while the bandits watch the long roads, and carry the time-sheet to the Council. This is the realm's first rehearsal of one message passing from town to town, as the Moot night's howl must later do.

**Cast**

| Role | Resident (id) | Voice | Why |
|---|---|---|---|
| heroine, giver | Grete Dimmock `rmx_grete_dimmock` | plain | the courier |
| timekeeper | Florence Pennock `rmx_florence_pennock` (f 49, courier) | plain | quiet, devout; keeps the sand-glass and the board times |
| stake | Hanne Ash `rmx_hanne_ash` (f 33, courier) | plain, broad | nervy; "owes a House factor money": she'll be let go if the Office loses |
| rival | Orm Carlisle `rmx_orm_carlisle` (m 31, keeps the Carters' Yard) | plain | chatty, even; has a wager on the wagons and plays fair |
| clerk | Philippa Pitchstack `rmx_philippa_pitchstack` (f 24, clerk in the Administrative Wing) | plain (larkfield, accent none) | takes the time-sheet for the Council |
| foes | `bandits.call` on the tube's carrier on the long legs | — | |

**Regions** (all town squares; sampled; road-heavy)

| id | cell | box |
|---|---|---|
| `sq_oakhaven` | `w_ridgemere_3_4` | (234, 186)–(254, 206) |
| `sq_ridgemere` | `w_ridgemere_1_1` | (94, 46)–(114, 66) |
| `sq_frosthollow` | `w_ridgemere_9_3` | (0, 18)–(14, 38) |
| `sq_coldwater` | `w_ridgemere_7_7` | (154, 0)–(174, 14) |
| `sq_larkfield` | `w_ridgemere_6_9` | (130, 66)–(150, 86) |
| `sq_mirebridge` | `w_ridgemere_5_11` | (178, 42)–(198, 62) |
| `sq_fenshore` | `w_ridgemere_3_13` | (82, 138)–(102, 158) |
| `sq_southmeadow` | `w_ridgemere_8_13` | (90, 10)–(110, 30) |
| `sq_saltwell` | `w_ridgemere_12_11` | (210, 106)–(230, 126) |

The relay's order: Oakhaven → Ridgemere → Frosthollow → Coldwater → Larkfield → Mirebridge crossroads → Fenshore → Southmeadow → Saltwell (8 legs).

**Chapters and objectives**

*Chapter 1, `trial`, "A Trial Leg".* Scope player; giver Grete, board `ridgemere`; repeat weekly (a wolf may better their time); `join.cap` 40.

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `tube` | Collect the courier's tube at the Couriers' Office. | enter place | target `rm_the_hall_of_records` | 1 | on completion: `item.give document_tube 1`, `start timer leg {of: $actor}` (a player timer) |
| `out` | Run to the Oakhaven square inside three game hours. | story signal | `trial_out` | 1 | `region entered sq_oakhaven` with `compare {since: leg} <= 0.125` and `carries document_tube` |
| `back` | And back to the Couriers' Office inside six. | story signal | `trial_back` | 1 | `enter place rm_the_hall_of_records` with `since leg <= 0.25` |
| hand-in | to Grete | | | | the wolf joins the story list `runners` |

*Chapter 2, `run`, "The Long Run".* Scope **story**, `lifetime: phase`, in phase `race` (re-entered each season for a fresh copy); `contributors` = min(3, `count runners`); 8 objectives `leg1`…`leg8` (story signals), each `after` the one before.
- **Setting the day.** When `count runners` ≥ 3, or ≥ 1 after 14 game days so that a lone runner can try, Grete posts the run for the next Marketday at the 6:00 `hour`. The post goes on all eight boards: *"THE LONG RUN: the Couriers' tube comes through this square on Marketday. Stand by the board and cheer it on, or carry it to the next town."*
- **The start.** At 6:00 that day: `enter race`, `start timer run` (story), `item.give document_tube 1` to every runner standing in `sq_oakhaven`.
- **A leg is run** when a participant who `carries document_tube` enters the next square's region after the previous leg. That leg's runner gets `add legs 1 {of: $actor}`, and a plain `story.tell` to the world narrates it, pre-written per leg, with no `from` and no quoted words: "The Couriers' tube has left Oakhaven and reached Ridgemere." / "… has reached Frosthollow." / "… has reached Coldwater." / "… has reached Larkfield." / "… has reached the Mirebridge crossroads." / "… has reached Fenshore." / "… has reached Southmeadow." / "The Couriers' tube is in Saltwell, Sound to Waste." (each written out in full in the program: "The Couriers' tube has reached Frosthollow.", and so on) Handing the tube on is just a `gift` of `document_tube` between wolves, which needs no new code. Fresh legs win.
- **Bandits.** On each leg's start, if `nh_threat` > 40 (or with 30% `chance` when the campaign isn't running), `bandits.call {player: $actor, count: 2}` on the wolf who carried it in.
- **Finish.** All eight legs with `since run` ≤ 1.0 game day → `lq_post = couriers`. If the eighth leg comes later, or the run times out at 2 game days → `lq_post = wagons`.
  - The wagons' time is fixed: the Carters' Yard wagon "arrives" after 1.5 game days, by a plain narrated `story.tell` ("The Carters' Yard wagon has reached Saltwell.").
  - A lone wolf who runs every leg inside 2 game days earns the Lone Run deed whichever side wins.

*Chapter 3, `answer`, "The Council's Answer".* Scope player; journal, offered to every wolf with `legs` ≥ 1 after the run; repeat once, `closure: true`. In later seasons a wolf who has done `answer` is offered `answer_again` instead: the same two objectives and the same choice, with no closure and no coin, only the practice.

| id | label | on | match | goal |
|---|---|---|---|---|
| `sheet` | Take the time-sheet from the timekeeper. | conversation | target `rmx_florence_pennock` | 1 |
| `council` | Carry it to the Council's clerk. | conversation | target `rmx_philippa_pitchstack` | 1 |

**Branches and lasting consequences.**
- **couriers.**
  - Boards (all eight) post the time, in one of three pre-written bands: "inside half a day" / "inside a day" / "a day and a hair".
  - Hanne keeps her place (`npc.line`). Orm Carlisle buys the round: his phase `npc.line` in `after_couriers` reads "Fair run. The first round at the Grey Gull is mine, and I'll pay it smiling." (The Grey Gull's keeper is not named, given a line or made a target.)
  - Key `lq_post=couriers`.
- **wagons.** The Office keeps half its couriers (talk). Grete's `npc.line` reads "Next season, then." Key `lq_post=wagons`.
- **Lasting:**
  - The record time band is kept in the world text `lq_post_time`, and later seasons try to beat it.
  - With C3/C4, the top 3 by `legs` become "the Long Runners" (a title).
  - The campaign reads `lq_post` and `lq_post_time` as proof that a message can cross the realm in a day (the Howl Line, Ch 4 and Ch 9).

**Rewards**

| When | Reward |
|---|---|
| `trial` (first time) | deed "ran the Oakhaven leg for the Couriers' Office"; `item.give courier_harness 1`; `story.practice travel` |
| `run` (each runner with `legs` ≥ 1) | 6 from `town:ridgemere` each season, while `lq_coin` allows (§1.5); `item.give paw_boots 1` (first season only); deed "carried the Couriers' tube on the Long Run from the Sound to the Waste"; lone: "ran the Long Run alone, every leg of it" |
| `answer` | 12 from `town:ridgemere`; `story.practice service` (`answer_again`: the practice only) |

**Campaign feed.**
- Each leg run: `add nh_voice_<that square's town> 1 × weight` (2/3/4 points; Mirebridge counts ×2). The Ridgemere and Oakhaven legs add 2.
- The finish (any side): `nh_realm +8` per runner, once per season.
- If `nh_stage ≥ 4`, Grete's `npc.line` reads "Same road the howl will take on the Moot night, they say. Best learn it now."

**Dialogue** (all plain)
- Quest `trial` (Grete):
  - **ask**: "Is the Couriers' Office hiring legs?"
  - **say**: "#greet#, #address#. The Carters' Yard swears its wagons beat paws across the realm, and the Council's listening. So the Office has bet its charter on the Long Run: one tube, paw to paw, through every square from the Sound to the Waste in a day. Run me a trial leg to Oakhaven and back."
  - **accept**: "Time me." / **decline**: "My legs are tired." / **handIn**: "There and back. How did I do?"
  - **thanks**: "#thanks#. Not bad at all for a first leg. You're on my list. When we've runners enough we run it for real, on a Marketday at dawn, and the whole realm watches the squares."
- `npc.line` Grete (`open`): "Legs wanted! The Couriers' Office runs the Long Run as soon as we've runners. Ask me for a trial leg."
- `npc.offer` Florence (`lq_run_time`, once): **"Who keeps the time?"** → "I do, with the Office's sand-glass and the Bright One's patience. Every square has a board, and every board gets the hour the tube came through. No wagon ever had its hours written up in public."
- `npc.offer` Hanne (`lq_run_stake`, once): **"What happens if we lose?"** → "Then the wagons get the realm's post, and the Office lets half of us go. I owe a factor money by the new moon. I'd rather run till my pads bleed than lose this place."
- `npc.offer` Orm Carlisle (`lq_run_wager`, once): **"You think wagons beat paws?"** → "Over two days with a load, every time. Over one day with a tube? Ha. I've a wager on the wagon, friend, and a wager's a wager. Run fair and I'll buy the first round at the Grey Gull, win or lose."
- `npc.offer` Philippa (`lq_run_clerk`, with `answer` or `answer_again`): **"Here is the Long Run's time-sheet."** → "#greet#. The timekeeper's seal, and every square's hour on it. The Council will have to read this one, whatever it would rather. Much obliged."
- `npc.line` Orm Carlisle (`after_wagons`): "Wagons it is, this season. Run again next season and I'll lose my wager gladly."
- Journal summaries: `run` "Carry the Couriers' tube, paw to paw, through every square from the Sound to the Waste in a day." `answer` "Take the time-sheet from the timekeeper to the Council's clerk."

**Gossip and helpful knowledge**

| Town | Dialect | Key | Line |
|---|---|---|---|
| any (`self.dialect=plain`) | plain | `lq_post=running` (helpful) | "If you carry the Couriers' tube, hand it on at the square. Fresh legs beat proud ones, and bandits watch the long roads." |
| Ridgemere | plain | `lq_post=couriers` | "The Couriers beat the wagons. Sound to Waste in a day, paw to paw. The Carters' Yard keeper bought the round, fair play to him." |
| Saltwell | saltwell | `lq_post=couriers` | "The runners came over the Waste like water down a wadi. Peace on their pads, every one." |
| Frosthollow | frosthollow | `lq_post=wagons` | "Wagons won the post, cousin. Dead rock luck for the runners. Next season, they say." |

**Engine needs.** J9 (a tagged tube: today any `document_tube` counts, so a wolf could bring their own). C3/C4 (rank and title). C1 (the run's time in board text; today three bands). C2 (a run day only when enough wolves are active). `hour` as a trigger is fine today.

---
## 11. The Eel Famine (`lq-eel-famine`)

| | |
|---|---|
| Tier | large: 3 chapters + a reckoning |
| Program | `lq-eel-famine`, open, cap 5000, prefix `lq` |
| Towns and places | **Fenshore** (square `3_13` (92, 148), board `board:fenshore`); Fen Shore landing's shed `fs_fishing_fencoast_shed` (door `w_ridgemere_2_14` (168, 190)); Withy Creek eelhouse `fs_marsh_eels_eelhouse` (door `4_13` (35, 107)); Rivermouth's shed `fs_fishing_delta_shed` (door `3_11` (132, 209)); the weir in the reeds (`3_12`); the Fenshore peat cutting (`3_14`); **Ridgemere**: the Fish Hall `rm_the_fish_hall` (door `0_0` (205, 216)) |
| Giver | **Ivo Sootridge** `cty_ivo_sootridge`: m 44, skipper at Fen Shore landing; Fenshore-raised, so mirebridge speech, light (mb110); warm and frank; "has family far away who never write". He fishes the shore and eats what Fenshore eats. Not Skye Eelbank, the campaign's marsh guide. |
| Reward purse | `town:fenshore` (213, day cap 21); the Fish Hall's share from `town:ridgemere` until SQ2 (§1.5) |

**Premise.** The eels didn't run up Withy Creek this spring, not one. Fenshore eats eels the way the city eats bread, and the traps come up empty. Below Rivermouth someone has driven a weir of stakes right across the reeds. A Ridgemere boat with a Fish Hall mark on its oars lifts the nets at dawn, and two guards with a lamp on a float keep the weir by night. The fen-folk say the float lamp is the drowned walking. Pull the weir, break its guards, and decide whether Fenshore takes its river back or takes a share of the Fish Hall's eels.

**Cast**

| Role | Resident (id) | Voice | Why |
|---|---|---|---|
| hero, giver | Ivo Sootridge `cty_ivo_sootridge` | mirebridge | the skipper |
| witness | Nab Bogditch `tn_mb_nab_bogditch` (m 25, fowler at Withy Creek) | plain, broad (Mirebridge-born, Ridgemere-raised) | "swears the marsh lights are the drowned"; his gran's lore of eels and the drowned |
| witness | Rowan Cragpeak `tn_fh_rowan_cragpeak` (f 57, peat cutter at the Fenshore cutting) | plain (fenshore, accent none) | nervy; saw the stakes driven |
| witness | Jasper Greymore `cty_jasper_greymore` (m 24, deckhand at Rivermouth) | plain (fenshore, accent none) | chatty, merry, frank; saw the Fish Hall boat and the float lamp |
| the deal | Karin Wetridge `rm_karin_wetridge` (f 64, works at the Fish Hall) | plain (saltwell, accent none) | restless, sly; the Fish Hall's eel buyer. The Fish Hall's keeper, Corra Coalcot (campaign), stays offstage |
| foes | the weir guards: `story.area.spawn` in `weir` | — | with C10, a named "weir-boss" |

**Regions**

| id | cell | box | what |
|---|---|---|---|
| `withy` | `w_ridgemere_4_13` | (24, 96)–(48, 120) | Withy Creek by the eelhouse |
| `rivermouth` | `w_ridgemere_3_11` | (120, 198)–(144, 222) | Rivermouth by the shed |
| `weir` | `w_ridgemere_3_12` | (230, 90)–(249, 109) | reed marsh by the river channel (391 open) |
| `weir_b` | `w_ridgemere_3_12` | (236, 40)–(255, 59) | the reeds upstream of the weir (386 open): the second spawn region, used in turn |
| `landing` | `w_ridgemere_2_14` | (156, 178)–(180, 202) | Fen Shore landing |

**Chapters and objectives**

*Chapter 1, `traps`, "Empty Traps".* Scope player; giver Ivo, board `fenshore`; once; `join.cap` 40.

| id | label | on | match | goal |
|---|---|---|---|---|
| `creek` | Look in the eel traps at Withy Creek. | region entered | target `withy` | 1 |
| `fowler` | Ask the fowler at the creek. | conversation | target `tn_mb_nab_bogditch` | 1 |
| `peat` | Ask the peat cutter what she saw. | conversation | target `tn_fh_rowan_cragpeak` | 1 |
| `delta` | Ask the Rivermouth deckhand what lifts the nets. | conversation | target `cty_jasper_greymore` | 1 |

*Chapter 2, `weir`, "The Weir".* Scope party; journal; weekly; `join.cap` 12 (waves in `weir` and `weir_b`, in turn).

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `find` | Find the weir below Rivermouth. | region entered | target `weir` or `weir_b` | 1 | while the weir stands, each dusk (`hour` 19) places `staves` ×5 at fixed reed tiles in each of `weir` and `weir_b` |
| `guards` | Drive off the weir's night guards. | bandit falls | cell `w_ridgemere_3_12` | 3 | the spawn ladder runs at the 21:00 `hour` in each of `weir` and `weir_b` that holds wolves and has its switch off |
| `stakes` | Pull the weir's stakes. | item picked up | item `staves`, cell `w_ridgemere_3_12` | 6 (quantity) | |
| `float` (hidden) | Take the guards' float lamp. | item picked up | item `oil_lamp`, cell `w_ridgemere_3_12` | 1 | placed on `camp cleared` (cell 3_12, the region's switch on), at the cleared region's centre; revealed after `guards` |

**Shared goal.** Story number `weir_left` starts at `g` (base 40, + 5 per helper). Each `staves` picked up in `weir` takes 1 off. At 0 the weir is gone: stop placing stakes, `enter reckoning`. Tracker: "Stakes left in the weir".

*Chapter 3, "Mud and Eels".* Two road quests by journal, each `closure: true`.

| Road quest | Objectives | World result |
|---|---|---|
| `eels_run` "Let the Eels Run" | be at Withy Creek at dusk (story signal; `hour` 18–21, participant in `withy`), on a day after the weir is gone; then a `gift` of `fish` 2 to `cty_ivo_sootridge` ("the first of the run") | `lq_eels = run` (**default**) |
| `eels_share` "A Gap and a Share" | `enter place` `rm_the_fish_hall`; `dialogue choice` item `rm_karin_wetridge`, detail `lq_eel_share` | `lq_eels = shared` |

**Branches and lasting consequences.**
- **run.**
  - `harvest.override {area: fish, word: good, days: 60, words: "the eels run up Withy Creek again"}`.
  - `times.nudge {market: ridgemere, points: 10, days: 14, words: "eels dear at the Fish Hall"}`.
  - Fenshore board: *"THE EELS ARE UP WITHY CREEK. Mud and eels, and thanks to every wolf who pulled a stake."*
  - Key `lq_eels=run`.
- **shared.**
  - `harvest.override fish fair 60 "one gap left in the weir"`.
  - Each `eels_share` wolf brings Fenshore its first share: 10 "the Fish Hall's first eel-share" to the wolf (today `town:ridgemere`; after SQ2 `till:job:rm_corra_coalcot`; story counter cap 60), plus a follow-on objective to `gift` ≥ 10 coins to `cty_ivo_sootridge`. Then `deed.award` "carried the Fish Hall's first eel-share to Fenshore".
  - Key `lq_eels=shared`.
- **Weir rebuilt?** If the line returns to `open` next season and no wolf takes `traps` within 14 game days of the season's start, the weir comes back (`weir_left` reset) and the key goes back to `weir`. Fenshore must be kept.
- **The float lamp** is a planted clue for the campaign's Ch 6, where the "marsh lights" are the Grey Captain's float lamps. The pack never says so. Jasper's line only says "It isn't the drowned. It's a lamp on a float."

**Rewards**

| When | Reward |
|---|---|
| `traps` | deed "asked along the river why the eels never came"; `story.practice travel` |
| `weir` (first time) | deed "pulled the Fish Hall's weir out of the reeds below Rivermouth"; `story.practice labour`; `item.give marsh_wraps 1`; 6 from `town:fenshore` |
| a road done | Run: 12 from `town:fenshore`, `item.give smoked_fish 4`. Shared: 2 from `town:fenshore` beside the 10 share the wolf carries on, `item.give fishing_net 1`. Deeds: "let the eels run up Withy Creek again" / (shared, as above) |

**Campaign feed.**
- Road done: `nh_realm +16`; `nh_voice_fenshore +16`.
- Each guard camp cleared: `nh_threat −1`.
- **`nh_names`** (only while `nh_stage` is 4–6): at `run` or `share`, a hidden objective "Ask the fowler his gran's name", a `conversation` with `tn_mb_nab_bogditch`, adds 1 to `nh_names`, once per wolf. It is "a Mirebridge eel-wife's name for the Book".

**Dialogue** (Ivo speaks mirebridge, light; Nab, Rowan, Jasper and Karin plain)
- Quest `traps` (Ivo):
  - **ask**: "Poor catch this spring?"
  - **say**: "#greet#, #address#. No catch. The eels never came up Withy Creek, not one, and Fenshore eats eels the way the city eats bread, no? Look in the traps, ask the fowler and the peat cutter, and ask the Rivermouth lad what he saw. Mud and eels, somebody knows."
  - **accept**: "I'll ask along the river." / **decline**: "Not today." / **handIn**: "There's a weir below Rivermouth."
  - **thanks**: "#thanks#. A weir of stakes right across, and a Ridgemere boat lifting the nets at dawn. Dark One's mire. They're taking our eels before they reach us. Pull it, fen-friend. Every stake you pull is a supper here."
- `npc.line` Ivo (`open`): "Dry feet. Traps up empty again. Fen-sure, something's wrong with the river."
- `npc.line` Ivo (`after_run`): "Dry feet, and full traps! Thick as reeds up Withy Creek, no? Supper for everyone."
- `npc.offer` Nab (`lq_eel_nab`, once): **"Where have the eels gone?"** → "Not here, and the herons are leaving too. My gran said the eels come up the creek because the drowned send them. If the drowned have stopped, somebody's standing in the way."
- `npc.offer` Rowan (`lq_eel_rowan`, once): **"Did you see anyone on the river?"** → "Wolves with mallets in the drizzle, driving stakes below Rivermouth. I didn't ask whose. I've a cottage and no wish to lose it."
- `npc.offer` Jasper (`lq_eel_jasper`, once): **"What boat lifts the nets?"** → "A Ridgemere boat with a Fish Hall mark on the oars, every dawn, friend. Two guards on the weir by night, with a lamp on a float. Folk say that's the drowned walking. It isn't. It's a lamp on a float."
- `npc.offer` Karin (plain; `lq_eel_share`, with `eels_share`): **"One gap in the weir, and Fenshore gets a share of every eel."** → "#greet#. A gap and a share? The Hall sells more eels than Fenshore ever ate. A share is cheaper than a war on the river. Take the first of it now. The Hall will complain the whole while."
- `npc.offer` Nab (`lq_eel_name`, the `nh_names` step): **"What was your gran's name?"** → "Her name was a Mirebridge name, from before the water came over. I'll say it to you, and you say it to whoever's writing the names down. She'd have liked that."
- Journal summaries: `weir` "Find the weir below Rivermouth with a pack, drive off its guards and pull its stakes." `eels_run` "Be at Withy Creek at dusk when the first eels come up." `eels_share` "Go to the Fish Hall and bargain one gap in the weir for a share of the eels."

**Gossip and helpful knowledge**

| Town | Dialect | Key | Line |
|---|---|---|---|
| Fenshore | mirebridge | `lq_eels=weir` (helpful) | "Go to the weir at dusk with friends and a strong back, no? The guards run when they're outnumbered, and the stakes come out easy in the mud." |
| Fenshore | mirebridge | `lq_eels=run` | "The eels are up Withy Creek again, thick as reeds. Mud and eels, it's a fair-dry spring after all." |
| Ridgemere `self.quarter=wharf` | plain | `lq_eels=run` | "Eels are dear at the Fish Hall. Somebody pulled their weir out of the fens." |
| Larkfield | larkfield | `lq_eels=shared` | "Fenshore has a share of the Fish Hall's eels now. Fair price, if you ask me." |

Recap (`inn.greet`, Fenshore): "No eels this spring. The skipper at Fen Shore landing thinks somebody's stopped them on the river."

**Engine needs.** Runs today. C8 for talk; C10 (`foe.add`) for a named weir-boss; L2 and L3 for camps; J1 for per-wolf offers; SQ2 for the Fish Hall's own till.

---

## 12. The Thief Who Wasn't There (`lq-vanishing-thief`)

| | |
|---|---|
| Tier | large: 3 chapters + a reckoning |
| Program | `lq-vanishing-thief`, open, cap 5000, prefix `lq` |
| Towns and places | **Ridgemere**: Vance House `rm_vance_house` (door `1_1` (197, 134)) on the Rise; Thornbury House `rm_thornbury_house`; the Sunken Street `r2_the_sunken_street` (54 × 7, doors `1_1` (17, 44) and (62, 67)); the Slop Rooms `r2_slop_rooms` (door `1_0` (15, 250)); the abbey school `r2_the_abbey_school`; board `board:ridgemere` |
| Giver | **Petra Vance** `rm_petra_vance`: f 38, lending at Vance House; plain speech, broad (rm378); quiet, devout and guarded; "dreams of a berth on a Grayrock ship bound somewhere dry". A devout lender who keeps a ledger "like a fist" and who, given a plain answer, can change. |
| Reward purse | `town:ridgemere` (treasury) |

**Premise.** Pledges are vanishing from Vance House's locked strongroom: a ring, a harness clip, a silver bracelet. No lock is forced and no window is open. The Heights say a Quickened wolf walks through walls, and someone has said the word "Wardens". The truth is under the street. The old buried Sunken Street runs beneath the Rise, an old drain opens into the Vance cellar, and a fourteen-year-old Sump pup who lives alone is taking back what Vance House took from her neighbours at a penny a week on the mark. She has no Gift at all.

**Cast**

| Role | Resident (id) | Voice | Why |
|---|---|---|---|
| giver (victim) | Petra Vance `rm_petra_vance` | plain | the lender |
| suspect (innocent) | Roald Wickstack `rm_roald_wickstack` (m 23, in service to the Vances) | plain | devout, short-tempered; has heard scratching under the cellar floor |
| witness | Silas Brinemore `rm_silas_brinemore` (m 25, in service to the Thornburys) | plain | the Thornburys lost only a clip a Sump carter had pawned |
| the thief | Hester Greyby `rmx_hester_greyby` (f 14, at lessons at the abbey school; lives alone in the Slop Rooms; **no parents in the data**) | plain | ambitious, sly; "owes a House factor money": she pawned her mother's shawl to pay it |
| lookout | Palle Brinewell `rmx_palle_brinewell` (m 14, her schoolmate) | plain | calm, sly; whistles when someone comes |
| the owners (`lq_owners`) | Ivy Stonehollow `tn_fh_ivy_stonehollow` (Hester's neighbour upstairs; Q2's giver: a `gift` target only, no offer), Piet Wickridge `rm_piet_wickridge` (Oakum Stair), Runa Ropemarsh `rm_runa_ropemarsh` (Rag End) | — | the wolves whose pledges Hester took back |

**The pledges** (items placed in the Sunken Street while the line is open; each re-placed daily if missing): `brass_clip` (Ivy's harness clip), `silver_bracelet` (Piet's drowned brother's), `ear_ring` (Runa's).

**Regions**

| id | cell | box | what |
|---|---|---|---|
| `vance_street` | `w_ridgemere_1_1` | (187, 124)–(207, 144) | the street before Vance House |
| `old_drain` | `r2_the_sunken_street` | (20, 1)–(34, 5) | the middle of the buried street, where the bricked arch is |

**Chapters and objectives**

*Chapter 1, `locked`, "Locked Rooms".* Scope player; giver Petra, board `ridgemere`; once; `join.cap` 40.

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `room` | See the strongroom at Vance House. | enter place | target `rm_vance_house` | 1 | |
| `servant` | Question the Vances' servant. | conversation | target `rm_roald_wickstack` | 1 | |
| `thornbury` | Ask at Thornbury House what they lost. | conversation | target `rm_silas_brinemore` | 1 | |
| `watch` | Watch the Vance street one night. | story signal | `street_watched` | 1 | `hour` trigger 23–4: participants in `vance_street` |

*Chapter 2, `under`, "Under the Street".* Scope player; journal; weekly; `join.cap` 12 (it places the pledges, three unique pieces, re-placed daily).

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `down` | Follow the scent of old stone down to the Sunken Street. | enter place | target `r2_the_sunken_street` | 1 | |
| `arch` | Find the bricked arch and what's behind it. | region entered | target `old_drain` | 1 | |
| `cache` | Pick up a pledge from the cache. | story signal | `pledge_found` | 1 | `item picked up` of `brass_clip`, `silver_bracelet` or `ear_ring` in `r2_the_sunken_street` |
| `thief` | Find the one who knows the old street. | conversation | target `rmx_hester_greyby` | 1 | `after: [cache]` |
| `lookout` (hidden) | Ask her lookout. | conversation | target `rmx_palle_brinewell` | 1 | revealed after `thief` |

**Shared goal.** Story number `found` (+1 per `under` done). At ≥ `g` (base 3, + 1 per 3 helpers) → `enter reckoning`. Until then, rumour grows: at 1 and at 2, a signed `board.post` on `board:ridgemere` (at 1: *"VANCE HOUSE: a reward for plain word of the wall-walking thief. No tales of magic. Petra Vance."*; at 2: *"VANCE HOUSE: another pledge gone. The Heights ask their neighbours to lock their cellars. Petra Vance."*), Petra's phase `npc.line` changes to the matching line below, and with C8 `belief.plant` the claim "walks through walls" about "a Quickened thief" (a place-worded rumour; talk survey §6.2).

*Chapter 3, "What Is Owed".* Three road quests by journal, each `closure: true`. Each starts with `face`: `dialogue choice` item `rmx_hester_greyby`, detail `lq_th_face`.

| Road quest | Objectives after `face` | World result |
|---|---|---|
| `thief_forgive` "What Is Owed" | a `gift` of each pledge to its owner (`brass_clip` → `tn_fh_ivy_stonehollow`, `silver_bracelet` → `rm_piet_wickridge`, `ear_ring` → `rm_runa_ropemarsh`; three objectives, any order); `dialogue choice` item `rm_petra_vance`, detail `lq_th_forgive` | `lq_thief = forgiven` (**default**) |
| `thief_name` "Name the Thief" | a `gift` of any one pledge to `rm_petra_vance`; `conversation` target `rm_petra_vance` | `lq_thief = named` |
| `thief_quickened` "A Quickened Thief" | `dialogue choice` item `rm_petra_vance`, detail `lq_th_blinker` | `lq_thief = blinker` |

**Branches and lasting consequences.**
- **forgiven.** Petra lets the Sump's pledges go. Vance House's rates change in talk (npc key `lq_petra=eased`), and a Ridgemere board post reads *"VANCE HOUSE: pledges from the Sump courts may be had back for the loan alone, no weekly penny."* Hester is not punished; she keeps her mother's shawl. Key `lq_thief=forgiven`.
  - With J8 `belief.forget`, the "Quickened thief" rumour is lowered. Until then, a `deed.award` on each forgiving wolf ("showed the Heights there was no Quickened thief") spreads the truth as a deed.
- **named.** Petra calls in Hester's debt: talk, and a journal note to the wolf, "The pup has been sent to the abbey school's board for her keep". Hester's npc key becomes `lq_hester=named`, and her `npc.line` turns cold toward the naming wolves (C5 per wolf; today for all participants). There is no warrant, because residents cannot be warranted. Key `lq_thief=named`.
- **blinker.** The lie wins.
  - Petra "writes to whoever answers for such wolves" (talk). The rumour grows, and the Heights board posts *"Lock your strongrooms. The Heights ask the Council to send for the Wardens."*
  - Hester keeps going: the pledges are re-placed weekly, and the line stays open with chapter 2 offered again.
  - Each `thief_quickened` wolf gets the deed "swore a Quickened thief robbed the Heights" (notorious) and 10 "Vance House's gratitude" (`town:ridgemere`).
  - Key `lq_thief=blinker`.
- **Campaign tie.**
  - From Ch 5 on, the campaign's key `nh_wardens` runs "coming / question / watching". In a `blinker` world, Petra's `npc.line` while `nh_stage ≥ 5` reads: "Black cloaks on the Pass Road, and they say I sent for them. Bright One help me, perhaps I did."
  - The forgiven road prefigures Ch 2's "Not One Voice Sold".
- **Gifted wolves (J10).** A wolf with a Gift gets one more Hester step: **"I carry a Gift. You don't."** → "No. I've just got four paws and a good nose and an old street nobody remembers. You lot make everything into magic." The step adds +1 to `t_forgive`.

**Rewards**

| When | Reward |
|---|---|
| `locked` | deed "watched the Vance street for the wall-walking thief"; `story.practice watch` |
| `under` (first time) | deed "found the old way under the Heights that the vanishing thief used"; `story.practice travel`; `item.give torch 2`; 6 from `town:ridgemere` |
| a road done | 12 from `town:ridgemere` (forgive and name; blinker the 10 above); `story.practice service`. Forgive: `item.give scent_locket 1` ("Hester's thanks: her mother's"). Name: `item.give silver_clip 1` (Petra's thanks). Deeds: "gave the Sump its pledges back from Vance House" / "named the Heights thief to the lender" / (blinker, as above) |

**Campaign feed.** Road done: `nh_realm +8`; `nh_voice_ridgemere +8` (forgiven), +4 (named), +2 (blinker).

**Dialogue** (all plain)
- Quest `locked` (Petra):
  - **ask**: "Something missing from your strongroom?"
  - **say**: "#greet#, #address#. Pledges, gone from a locked room: a ring, a clip, a bracelet. No lock forced, no window open. The Heights say a Quickened wolf walks through walls, and someone has said the word Wardens. I'd rather a plain answer. See the room, ask my servant, and watch my street one night."
  - **accept**: "I'll watch your street." / **decline**: "Not my business." / **handIn**: "Nobody came by the street."
  - **thanks**: "#thanks#. Nobody by the street. Then by what, the open air? There's an old street under these houses, they say, from before the walls. Find where it opens, and find who knows it."
- `npc.line` Petra (`open`): "Lock your doors, they say. Mine were locked. Bright One keep us from wolves who walk through walls."
- `npc.line` Petra (`open`, after `found` reaches 2): "Another pledge gone, and the whole Heights whispering about Gifts. I want a plain answer, and nobody will give me one."
- `npc.line` Petra (`after_forgiven`): "A penny a week on a mark. Put plainly, it was robbery. I keep a gentler ledger now, Bright One forgive me the old one."
- `npc.offer` Roald (`lq_th_roald`, once): **"Did you take the pledges?"** → "I've never touched the strongroom key. But I've heard scratching under the cellar floor at night, like something in the drains, and the cellar smells of old stone and wet. Nobody believes a servant."
- `npc.offer` Silas (`lq_th_silas`, once): **"Was your house robbed too?"** → "A brass clip off the master's best harness. Only the one, that a Sump carter pawned to the Vances last winter. Funny sort of thief, that takes back only what was pawned."
- Hester:
  1. `lq_th_face` **"You took the pledges from Vance House."** → "Took them back. A carter's clip, a bracelet a lighterman left his brother, an old ear-ring. My mother's shawl. The Vances lend at a penny a week on the mark and keep the pledge if you're a day late. I'm not Quickened. I know the old street."
  2. `lq_th_way` (once, after `face`) **"Show me the way you came."** → "Down the Sunken Street, past the bricked arch, up the drain under the Vance cellar. Mind the low bit. And don't tell the school. They think I'm at lessons."
- `npc.offer` Palle (`lq_th_palle`, once): **"Do you keep watch for her?"** → "Somebody has to whistle. She's cleverer than the whole Heights lot. If you're going to name her, name me too. It's only fair."
- Petra, road choices:
  - `lq_th_forgive` **"Your rates are robbery. Let the Sump pledges go."** → "Robbery? A penny a week on a mark is... a great deal, put that way. Bright One forgive me, I keep a ledger like a fist. Let them go, then. And the girl stays out of my cellar."
  - `lq_th_blinker` **"It was a Quickened wolf. Nobody could have stopped it."** → "Then the Heights were right, and I was a fool to want a plain answer. I'll write to whoever answers for such wolves. Somebody must."
- Journal summaries: `thief_forgive` "Give the pledges back to their owners, and put the lender's rates to her plainly." `thief_name` "Take a pledge back to the lender and name the thief." `thief_quickened` "Tell the lender it was a Quickened wolf, and keep the pup's secret."

**Gossip and helpful knowledge**

| Town | Dialect | Key | Line |
|---|---|---|---|
| Ridgemere `self.quarter=heights` | plain | `lq_thief=walking` | "They say a Quickened wolf walks through the Heights' walls and takes pledges. Lock your strongroom, for all the good it does." |
| Ridgemere `self.quarter=sump` | plain | `lq_thief=walking` (helpful) | "There's an old street under the Heights, from before the walls. The Sunken Street. Folk say it runs further than anybody's walked." |
| Ridgemere | plain | `lq_thief=forgiven` | "Vance House let the Sump pledges go. A wolf put it to the lender plain, they say, and the lender listened." |
| Larkfield | larkfield | `lq_thief=named` | "Ridgemere caught its wall-walking thief. A pup, they say, no Gift at all. Short-weighed magic, that." |
| Ridgemere | plain | `lq_thief=blinker` | "The Heights swear it was a Quickened thief. Somebody's writing to the Wardens. Bright One keep us all." |

Recap (`inn.greet`, Ridgemere): "Pledges vanishing from a locked room on the Rise. The lender at Vance House wants a plain answer."

**Engine needs.** J10 (`wolf has gift`) for the Gifted step; C8 `belief.plant` for the rumour; J8 `belief.forget` for clearing it; C5 for Hester's coldness toward one wolf; C14 (optional) for a lasting change to Vance House.

---

## 12a. The Black Hearths (`lq-black-hearths`)

The Frosthollow line (decisions §7.2, critic P1-16). **It must not overlap**, and does not: Deepvein and the mine captain's confession (campaign Ch 8), Silver Under the Ice (Ch 4), the Frosthollow Count (Ch 8), the lamp-oil list and lamp oil (medium Q22 `mq-lamp-oil-share`), the props (Q21 `mq-low-drift-props`), the rustlers and the bear (`lq-old-brown`), and the clamp watch (small #30 `sq-clamp-watch`: no objective here sits a clamp). It never goes to the mine (`w_ridgemere_9_2`), the Mine Captain's House or the Lamp and Pick, and it never names the Grey Captain.

| | |
|---|---|
| Tier | large: 3 chapters + a reckoning |
| Program | `lq-black-hearths`, open, cap 5000, prefix `lq`; file `Data/Storylines/ridgemere3/quests-large/lq-black-hearths.json` |
| Towns and places | **Frosthollow** (square `w_ridgemere_9_3` (4, 28), board `board:frosthollow`); the **Upper hearths** under the Whitefangs (`w_ridgemere_10_3`; Marl works at (85, 171)); **below Kestrel Tarn** (`w_ridgemere_10_2`, the Whitefangs); **Ridgemere**: the Timberland Charcoal Store `rm_the_charcoal_store` (door `w_ridgemere_1_1` (0, 150), on the Sump side of the Commons); the **Greenholt hearths** (`w_ridgemere_4_8`, in the Rainwood, where Larkfield's colliers burn) |
| Giver | **Marl Craghearth** `cty_marl_craghearth`: m 38, collier at the Upper hearths; lives in Frosthollow Cottage 1 (`cn_frosthollow_cottage_1`), which he shares with the carter, the young collier and the lime burner below; Frosthollow-born, **plain speech** (home region frosthollow, accent none; mantle fh101 "the riddler by the wall"); guarded, warm and close-mouthed, never sits with his back to the door, keeps a riddle back for anyone who sits long enough; fears "a clamp breaking out in the night"; "feuding with a neighbour over a well that is not really anyone's". Alive, placed, home and work reachable (checked 2026-11-22). The young collier, Iris Fernhaven, shares a first name with campaign cast (Iris Meadowfield, Southmeadow's Keeper), so no line of this quest names her (decisions §1.2.5). |
| Reward purse | `town:frosthollow` (247, day cap 24) |

**Premise.** The Upper hearths' charcoal sells in the city at half its price. The Timberland Charcoal Store says Frosthollow's sacks come in wet and smoky, and pays for wet coal. The colliers swear their char leaves the hearths dry. By night somebody opens the clamps on the east floor from the side and throws water in, and at dawn the colliers rake out the spoiled char as waste. By noon the waste is gone. A band of charcoal thieves bags the good char from under the wet crust, dries it at a fire below Kestrel Tarn, and sells it at the store's yard door for half, so the store has cheap dry sacks and no need of Frosthollow's honest ones. Follow the char to the city, break the band at its fire, and decide how Frosthollow's charcoal is sold from now on.

**Cast**

| Role | Resident (id) | Voice | Hub item |
|---|---|---|---|
| hero, giver | Marl Craghearth `cty_marl_craghearth` | plain | quest `undersold`; the three road choices `lq_bh_contract`, `lq_bh_home`, `lq_bh_greenholt` (one quest's `npc.offer`s, shown one at a time to the wolf on that road) |
| the carter | Ebba Coalgate `cty_ebba_coalgate` (f 36, carter at Frosthollow, works on the square (5, 29); home Cottage 1; gives medium `mq-low-drift-props`; also the target of small #29) | frosthollow (light) | **conversation target only**: drives the Upper hearths' sacks down the pass road; the store paid half and called them wet |
| the young collier | Iris Fernhaven `cty_iris_fernhaven` (f 26, collier at the Upper hearths, the third generation at the trade; home Cottage 1; gives small `sq-clamp-watch`) | plain (larkfield, accent none) | **conversation target only**; never named in any line: found the east clamps opened and steaming at dawn |
| the counter | Stone Acorndell `cty_stone_acorndell` (m 46, burns charcoal at the Upper hearths; counts things under his breath; gives medium `mq-lamp-oil-share`; also a target in `lq-old-brown`, his second and last) | frosthollow (broad) | **conversation target only**: counted the waste, and the drag marks to the north |
| the lime burner | Leif Driphand `cty_leif_driphand` (m 31, burns lime at the high lime quarry; home Cottage 1; distrusts anything from the city; gives small `sq-scree-goats`) | frosthollow (light) | **conversation target only** (optional step): saw a lampless cart of black sacks on the quarry road by night |
| the store hand | Arne Ashton `rm_arne_ashton` (m 62, works at the Timberland Charcoal Store; home `r2_net_court_house_up`; ambitious and warm, keeps a pocketful of coal for luck, writes choruses on apple wrappers; in no other content) | plain (ridgemere, light) | **conversation and gift target only**: buys strangers' sacks at the yard door and didn't ask; the store's keeper stays offstage and unnamed |
| foes | the charcoal thieves: `story.area.spawn` bands at their drying fire | — | strangers of the roads, never residents |

**Regions** (all sampled 2026-11-22: open ground, no deep water)

| id | cell | box | what |
|---|---|---|---|
| `east_floor` | `w_ridgemere_10_3` | (156, 196)–(175, 215) | the hearths' east burning floor (400 open, 34 path), well away from the clamp small #30 watches ((78, 168)–(88, 178)) |
| `tarn_yard` | `w_ridgemere_10_2` | (88, 176)–(107, 195) | tundra below Kestrel Tarn's south shore, where the thieves dry the char (398 open) |
| `tarn_yard_b` | `w_ridgemere_10_2` | (132, 152)–(151, 171) | moss and pines east of the tarn (362 open): the second spawn region, used in turn |
| `fh_square` | `w_ridgemere_9_3` | (0, 18)–(14, 38) | the Frosthollow square and board (263 open, 132 road) |
| `greenholt_yard` | `w_ridgemere_4_8` | (228, 68)–(247, 87) | the Greenholt hearths' yard in the Rainwood (400 open) |

**Chapters and objectives**

*Chapter 1, `undersold`, "Wet Coal".* Scope player; giver Marl, board `frosthollow`; once; `join.cap` 40 (talk and walking: no spawn, no placed item).

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `carter` | Ask the carter what the city store paid. | conversation | target `cty_ebba_coalgate` | 1 | her window line |
| `floor` | Look over the east burning floor by daylight. | story signal | `floor_seen` | 1 | `region entered` `east_floor` with `hour between 6 18` → `objective.complete` and `journal.note` "Clamps opened from the side, the crust wet through, and the raked-out char gone." |
| `collier` | Ask the young collier what she found at dawn. | conversation | target `cty_iris_fernhaven` | 1 | her window line |
| `store` | Go down to the Timberland Charcoal Store in Ridgemere. | enter place | target `rm_the_charcoal_store` | 1 | |
| `hand` | Ask the store hand where his cheap sacks come from. | conversation | target `rm_arne_ashton` | 1 | `after: [store]`; his window line |
| hand-in | to Marl | | | | `handIn: true` |

*Chapter 2, `tarnfire`, "The Fire Below the Tarn".* Scope party; journal; weekly; `join.cap` 12 (waves in `tarn_yard` and `tarn_yard_b`, in turn).

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `count` | Ask the collier who counts everything how much waste went missing. | conversation | target `cty_stone_acorndell` | 1 | his window line: twelve sacks, and drag marks north |
| `quarry` (optional) | Ask the lime burner what passed on the quarry road. | conversation | target `cty_leif_driphand` | 1 | his window line |
| `fire` | Find the thieves' drying fire below Kestrel Tarn after dark. | story signal | `fire_found` | 1 | `region entered` `tarn_yard` or `tarn_yard_b` with `hour between 20 4` (participant, `once: visit` per party) → `objective.complete` and the spawn ladder in the region of the turn (§3's pattern; switches `spawned_tarn_yard`, `spawned_tarn_yard_b`) |
| `thieves` | Bring down the charcoal thieves. | bandit falls | cell `w_ridgemere_10_2` | 3 | `after: [fire]` |
| `sack` | Take back a sack with the Upper hearths' mark on it. | item picked up | item `sack`, cell `w_ridgemere_10_2` | 1 | placed on `camp cleared` (cell 10_2, the region's switch on): at (97, 185) for `tarn_yard`, (141, 161) for `tarn_yard_b` |

**Shared goal.** Story number `sacks_back` (+1 per party that finishes `sack`). At ≥ `g` (base 4, + 1 per 3 helpers) → `enter reckoning`. Tracker: "Sacks taken back". Before the reckoning the band comes back each week: on `day begins` Emberday both `spawned_tarn_yard` switches are turned off. Each tarn camp cleared: `nh_threat −1` (clamped).

*Chapter 3, "What Charcoal Is Worth".* Three road quests by journal, each `closure: true`, repeat once, no cap. Each road's choice sits on the giver (decisions §1.2.1).

| Road quest | Objectives | World result |
|---|---|---|
| `hearth_contract` "A Price by the Sack" | `dialogue choice` item `cty_marl_craghearth`, detail `lq_bh_contract`; a `gift` of `charcoal` 2 to `rm_arne_ashton` ("show the store what dry looks like"); `conversation` target `cty_ebba_coalgate` ("tell the carter she drives on Marketday") | `lq_hearths = contract` (**default**) |
| `hearth_home` "Frosthollow Burns Its Own" | `dialogue choice` item `cty_marl_craghearth`, detail `lq_bh_home`; a `gift` of `charcoal` 2 to `cty_marl_craghearth` (for the stall); be on the Frosthollow square on a Marketday (story signal: `region entered` `fh_square` with `calendar is weekday Marketday`) | `lq_hearths = home` |
| `hearth_greenholt` "The Greenholt Wagons" | `dialogue choice` item `cty_marl_craghearth`, detail `lq_bh_greenholt`; carry charcoal to the Greenholt hearths (story signal: `region entered` `greenholt_yard` with `carries charcoal`) | `lq_hearths = greenholt` |

**Branches and lasting consequences** (the reckoning, §1.2; default `contract`):
- **contract.** `times.nudge {market: frosthollow, points: 15, days: 28, words: "the city store pays the Upper hearths by the sack"}`. Frosthollow board: *"THE UPPER HEARTHS: a price by the sack from the Timberland Charcoal Store, paid when the cart unloads. The carter drives on Marketday."* Key `lq_hearths=contract`.
- **home.** `times.nudge {market: frosthollow, points: 10, days: 28, words: "Frosthollow charcoal sold at home"}` and `times.nudge {market: ridgemere, points: 10, days: 14, words: "charcoal dear in the city"}`. Frosthollow board: *"CHARCOAL on the square, Marketday, from the Upper hearths. Dry, and full sacks."* Key `lq_hearths=home`.
- **greenholt.** `times.nudge {market: larkfield, points: 10, days: 28, words: "Greenholt and Upper hearths charcoal sold together"}`. Frosthollow's voice takes a quarter share, and Larkfield a quarter (below). Key `lq_hearths=greenholt`.
- **The band broken** (the shared goal met at all): `subtract nh_threat 3`, clamped.
- **Next season.** If the line returns to `open` and no wolf takes `undersold` or `tarnfire` within 14 game days, the band is back (`sacks_back` reset), and the key goes back to `undersold`.
- **Lasting:** the world text `lq_hearths`; the price state; the deed gossip.

**Rewards** (§1.5: at most 12 a chapter, 35 a line)

| When | Reward |
|---|---|
| `undersold` | deed "followed the Upper hearths' charcoal down to the city"; `story.practice travel`; `item.give charcoal 2` |
| `tarnfire` (first time) | deed "broke the charcoal thieves at their fire below Kestrel Tarn"; `story.practice watch`; `item.give wool_scarf 1`; 6 from `town:frosthollow` |
| a road done | 12 from `town:frosthollow`; `story.practice commerce`; `item.give hobnail_boots 1`. Deeds: "won the Upper hearths a price by the sack in the city" / "sold Frosthollow's charcoal on its own square" / "carried the Upper hearths' charcoal to the Greenholt wagons" |

**Campaign feed.**
- Road done: `nh_realm +12`; `nh_voice_frosthollow +12` (greenholt: 3, and `nh_voice_larkfield +3`).
- Each tarn camp cleared: `nh_threat −1`; the band broken: −3.
- Read only: while `nh_threat` > 40, the Frosthollow board adds *"Carters go two together on the pass road this season."* Nothing else in the campaign is read or touched.

**Dialogue** (Marl, Iris and Arne speak plain; Ebba and Leif frosthollow, light; Stone frosthollow, broad)
- Quest `undersold` (Marl):
  - **ask**: "Your charcoal's not selling?"
  - **say**: "#greet#, #address#. It sells. At half. The city store says our sacks come in wet. They leave here dry, I'd stake the hearth on it. Ask our carter what they paid, look over the east floor by daylight, ask the young collier what she found at dawn, then go down and see the store for yourself."
  - **accept**: "I'll find out where the dry sacks go." / **decline**: "Not today." / **handIn**: "Your char is sold twice and dried once."
  - **thanks**: "#thanks#. Wetted at night, dried by somebody else, sold at the yard door for half. A riddle with a band of thieves at the bottom of it. Find their fire. It won't be far from water."
- `npc.line` Marl (`open`): "What burns twice and is paid for once? Our charcoal, this season. Mind the east floor if you're up there at night."
- `npc.line` Marl (`after_contract`): "A price by the sack, paid when the cart unloads. I'll still count it twice, with my back to the wall."
- `npc.line` Marl (`after_home`): "Frosthollow buys Frosthollow's charcoal now. The city can climb up and pay, like everybody else."
- `npc.line` Marl (`after_greenholt`): "Greenholt's wagons, Greenholt's name on the sacks. Steady money. I'd still rather the sacks said ours."
- Road choices (Marl; each shown only to the wolf whose road is active, with C6 `offer.during`; today untargeted with `once: true`):
  - `lq_bh_contract` **"Sell to the city store, at a price by the sack."** → "A contract, paid when the cart unloads. Take the store hand two sacks to show him what dry looks like, and tell our carter she drives on Marketday."
  - `lq_bh_home` **"Sell it here, on our own square."** → "No yard door and no middle wolf. Bring me two sacks for the stall, and stand the square with us on Marketday."
  - `lq_bh_greenholt` **"Let the Greenholt hearths sell it with theirs."** → "Under their name, on their wagons. Safer roads and smaller coin. Carry them the first sacks, so they know we mean it."
- **Window lines** (`resident.tree` `hub`, priority 4):
  - `say[self.id=cty_ebba_coalgate world.mem.lq_hearths=undersold]` "Half, cousin. Half, and a face at the store like I'd brought them mud. Those sacks were dry when I loaded them. Cold iron, I'd swear it on the top bend."
  - `say[self.id=cty_iris_fernhaven world.mem.lq_hearths=undersold]` "I came up late, the way I always do, and the east clamps were open on the side and steaming. Wet ash, all of it. We raked it out as waste. By noon the waste was gone."
  - `say[self.id=rm_arne_ashton world.mem.lq_hearths=undersold]` "Strangers bring sacks to the yard door at half the price, dry as a good song. I didn't ask where from. I should have. The hill carter gets what's left of our coin."
  - `say[self.id=cty_stone_acorndell world.mem.lq_hearths=undersold]` "'Arth, cousin. Twelve sacks of waste raked out, and twelve gone by noon. I counted. Drag marks to the north, deep, toward the tarn. Black seam, somebody's drying our char."
  - `say[self.id=cty_leif_driphand world.mem.lq_hearths=undersold]` "A cart on the quarry road at night, no lamp, black sacks to the tail. Going west, to the city. Nothing good goes that way in the dark, cousin."
  - `say[self.id=rm_arne_ashton world.mem.lq_hearths=contract]` "A price by the sack, and dry sacks from the hill. No more strangers at the yard door. I'd sing about it if you stood still long enough."
  - `say[self.id=cty_ebba_coalgate world.mem.lq_hearths=contract]` "Marketday, down the pass road with a full cart and a paper that says what they'll pay. Warm hearth, cousin. That's all a carter wants."
- Journal summaries: `tarnfire` "Find where the stolen char is dried, below Kestrel Tarn, break the thieves and take back a marked sack." `hearth_contract` "Win the Upper hearths a price by the sack from the city store." `hearth_home` "Help Frosthollow sell its own charcoal on its own square." `hearth_greenholt` "Carry the Upper hearths' charcoal to the Greenholt hearths, to sell with theirs."

**Gossip and helpful knowledge** (`resident.tree`, priority 2)

| Town (gate) | Dialect | Key | Line |
|---|---|---|---|
| Frosthollow `here.region=frosthollow` | frosthollow | `lq_hearths=undersold` (helpful) | "If you go after the char thieves, go at night and go below the tarn. They dry what they steal by a fire, cousin, and a fire shows." |
| Frosthollow `here.region=frosthollow` | frosthollow | `lq_hearths=undersold` | "Upper hearths charcoal at half price in the city. Dead rock luck, or somebody's hand in it. Deep, I'd say the hand." |
| Ridgemere `self.quarter=commons` | plain | `lq_hearths=contract` | "The charcoal store pays the hill colliers by the sack now. Dearer coal, and no strangers at the yard door." |
| Frosthollow `here.region=frosthollow` | frosthollow | `lq_hearths=home` | "Frosthollow charcoal on the Frosthollow square, Marketday. Deep and plain, the way it should be, stone." |
| Larkfield `here.region=larkfield` | larkfield | `lq_hearths=greenholt` | "Greenholt wagons carry the Upper hearths' charcoal too now. Fair price, if not full weight for the hill folk." |

Recap (`inn.greet`, Frosthollow, `world.mem.lq_hearths=undersold`): "The Upper hearths' charcoal sells at half in the city. The colliers want somebody to find out why."

**Engine needs.** Runs today: talk-target chapters, one fight with cell-matched camps, journal roads. C8 for the talk keys (else the DM seeds them); C6 `offer.during` and J1 for the per-wolf road choices; L2 (`"camp": "lq_bh_tarn"`, `"lq_bh_tarn_b"`) and L3 for camp matching; C10 (`foe.add`) for a named leader of the band, optional.

---

## 12b. The stage pack's large quests (`nh-pack.stage`)

The campaign's large seeds that were kept (decisions §7.3), written in full. **The program** `nh-pack.stage` (one open-audience program, cap 5000, at most 32 quests, file `Data/Storylines/ridgemere3/quests-stage/nh-pack.stage.json`, prefix `st`, phases `quiet`, `s12`, `s3`, `s4`, `s56`, `s78`, `s9` entered by an `hour` trigger that reads `nh_stage`) is written once, by the small reviser, in `quests-small.md`. These five entries go into it as written here.

Shared by all five:
- **Single large quests** of 3–5 objectives. `repeat: "weekly"`, scope player (or party where said), no reckoning, no chapters, no `closure`. Each lists its `phases`; outside them it is not offered.
- **Giver and board.** Each has a `giver` and `board: ridgemere` (or the town named), so a wolf finds it even while a Watch giver walks a patrol route.
- **Purse.** The §1.5 pattern (decisions §3) with the stage program's own per-wolf counter `st_coin`: **at most 12 a completion**, coin only on the wolf's first completion in each phase, and later weekly repeats pay in kind (`bread` ×2) with the same deed, practice and feed.
- **Feed.** Large tier: `nh_realm` and `nh_voice_<town>` + 8 × the distance weight (Ridgemere/Oakhaven 8, Coldwater/Larkfield/Frosthollow 12, the far towns 16), once per wolf per phase.
- **Camps** match by `cell` plus a `spawned_<region>` switch (§1.4); after L2, the camp names begin `st_`.
- **Fights** use two spawn regions in turn, or `bandits.call` on each wolf, so a crowd never queues on one band. `join.cap` 12 on a fight quest; none otherwise.
- **Givers** are in `givers.tsv` with a `stage:` row each: none is campaign cast, a keeper, an innkeeper or a look-alike, and none holds another pack quest. All five were checked alive, placed and reachable on 2026-11-22.

| # | Quest | Phases | Giver (dialect) | Purse | Fight |
|---|---|---|---|---|---|
| S1 | `st-patrol-round` | `s12`, `s3` | Bertil Saltcot `rmx_bertil_saltcot` (plain, light) | `town:ridgemere` | `bandits.call` |
| S2 | `st-camp-bounty` | `s3`, `s4`, `s56`, `s78`, `s9` | Frederick Rainham `rm_frederick_rainham` (plain, light) | `town:ridgemere` | the camps themselves |
| S3 | `st-midsummer-road` | `s4` (the festival days) | Cedric Mudley `rm_cedric_mudley` (plain, light) | `town:ridgemere` | two regions in turn |
| S4 | `st-twelve-bounds` | `s56` | Ada Coalstack `cty_ada_coalstack` (southmeadow, light) | `town:ridgemere` | none |
| S5 | `st-post-night` | `s9` (until the Moot night is called) | Beatha Foggate `rm_beatha_foggate` (plain, broad) | the post's town | `bandits.call` |

### S1. `st-patrol-round`: The Patrolman's Round
- **Phases:** `s12`, `s3`. **Scope:** player. **Repeat:** weekly. `join.cap` 12. **Board:** `board:ridgemere`.
- **Giver:** Bertil Saltcot `rmx_bertil_saltcot`: m 22, a Watch guard on the street patrol (`rm_street_patrol`; evenings at the Watch House `rm_the_watch_house`); home `r2_lantern_row`; plain speech, light; generous and quietly funny, "remembers every debt, owed or owing". Alive, placed, home reachable; he has no fixed work place, so the board copy carries the quest.
- **Premise.** The Sump patrolman who now keeps the Palace lamp still walks his round at dusk, "because the alleys don't stop needing it" (campaign §2). After dusk the round is nobody's, and the courts know it. Walk the night half of it: the four posts of the Sump round, in order, between the 22nd hour and the 4th, and lend a paw on the way.
- **Regions** (the Sump round's own posts, `live.patrol_posts` of `r2_sump_patrol`; street boxes, sampled):

| id | cell | box | the round's post |
|---|---|---|---|
| `round_1` | `w_ridgemere_1_1` | (38, 36)–(50, 48) | post 1 (44, 42) |
| `round_2` | `w_ridgemere_0_1` | (213, 32)–(225, 44) | post 4 (219, 38) |
| `round_3` | `w_ridgemere_0_0` | (243, 215)–(255, 227) | post 6 (249, 221) |
| `round_4` | `w_ridgemere_0_1` | (242, 78)–(254, 90) | post 8 (248, 84) |

- **Objectives:**

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `post1` | Walk to the round's first post after dark. | story signal | `round_1` | 1 | `region entered` `round_1`, `hour between 22 4`; `story.ping` the next post for the wolf (30 minutes) |
| `post2` | On to the second post. | story signal | `round_2` | 1 | `after: [post1]`; on entry, `bandits.call {player: $actor}` scaled by party (1 → 2, 2–3 → 3, 4+ → 4), once per wolf per night |
| `post3` | On to the third post. | story signal | `round_3` | 1 | `after: [post2]` |
| `post4` | And the last, by the wharf stair, before the fourth hour. | story signal | `round_4` | 1 | `after: [post3]`; on entry, a second `bandits.call` as at `post2` |
| `cutpurses` | Bring down the cutpurses who wait between the posts. | bandit falls | cell any of `w_ridgemere_1_1`, `w_ridgemere_0_1`, `w_ridgemere_0_0` | 2 | |
| hand-in | to Bertil | | | | `handIn: true` |

- **Lend a paw** (optional, not an objective): a `gift` of `bread` or `herb_tea` to any resident while the wolf is in a `round_*` region adds `st_round_help` (player) and the deed below; no hub item, no named resident.
- **Rewards:** 10 from `town:ridgemere` (the Watch's thanks, through the treasury); deed "walked the Sump's night round when nobody else would" (and "lent a paw on the Sump round at night" for a helper); `story.practice watch`; `item.give mouth_lantern 1` (first time only).
- **Feed:** `nh_realm +8`, `nh_voice_ridgemere +8`.
- **Gates:** phases only; it reads nothing else.
- **Dialogue** (Bertil, plain):
  - **ask:** "The Watch short of paws?"
  - **say:** "#greet#, #address#. The patrolman who walked the Sump round at night keeps the Palace lamp now. He still walks it at dusk, Bright One love him, but after that the round is nobody's, and the courts know it. Walk it for me tonight, all four posts, and lend a paw to anyone who needs one."
  - **accept:** "I'll walk the night round." **decline:** "Not tonight." **handIn:** "The round's walked."
  - **thanks:** "#thanks#. Four posts and nobody's purse lifted. That's a better night than the Sump's had since spring. Come back next week. The round doesn't stop needing it."
- **Talk** (`resident.tree`, `news`, priority 2, key `st_round=open` written by C8 at phase entry, or seeded):
  - Ridgemere `self.quarter=sump`, plain (helpful): "If you walk the Sump round at night, keep to the posts and carry a light. The cutpurses wait in the dark between."
  - Ridgemere `self.quarter=sump`, plain (gossip): "Somebody's walking the night round since the patrolman went up the Hundred Steps. Not the Watch. Just somebody."

### S2. `st-camp-bounty`: A Camp Come Back
- **Phases:** `s3`, `s4`, `s56`, `s78`, `s9`. **Scope:** party. **Repeat:** weekly. `join.cap` 12. **Board:** `board:ridgemere`.
- **Giver:** Frederick Rainham `rm_frederick_rainham`: m 34, a Watch guard on the street patrol (`rm_street_patrol`; evenings at the Watch House); home `rm_the_watch_house`; plain speech, light; patient and ambitious, keeps a pocketful of coal for luck. Alive, placed, home reachable.
- **Premise.** The camps the realm broke for the Line keep coming back, like weeds in a path. The Watch pays a bounty for any camp broken again on the Line's roads, any of the nine: Alder Mere `5_4`, Glasswater `6_8`, the Upper Mere on the Frosthollow road `8_6`, the Sallow Fens `6_12`, the Sunfields `7_12`, the Rainward Hills `1_3`, `2_3` and `3_3`, and the lime quarry `6_3` (campaign §5.3).
- **Objectives:**

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `fallen` | Bring down three bandits of a camp on the Line's roads. | bandit falls | `cell` any of the nine (`any` of nine `field` tests) | 3 | |
| `camp` | Break the camp. | story signal | `camp_broken` | 1 | trigger on `camp cleared` (audience participant) with `cell` any of the nine: `objective.complete` for the actor and for each wolf `in party with` the actor and `wolf in place` that cell. Matched by `cell` only (decisions §4): a natural camp or any story's camp there counts, since either way the road is clear |
| hand-in | to Frederick | | | | `handIn: true`; the journal names the cell broken (`journal.note` from the trigger) |

- **Rewards:** 10 from `town:ridgemere` ("the Watch's bounty"); deed "broke a camp that came back on the Line's roads"; `story.practice watch`.
- **Feed:** `subtract nh_threat 1`, clamped, per camp broken. The camp's town is credited as campaign §5.3's table says (5_4 Oakhaven; 6_8 Coldwater ⅔ and Larkfield ⅓; 8_6 and 6_3 Frosthollow; 6_12 Mirebridge; 7_12 Southmeadow; 1_3, 2_3, 3_3 Ridgemere): `nh_voice_<town>` and `nh_realm` + 8 × that town's weight.
- **Gates:** phases only. A Line post standing on a cleared cell is the campaign's business; this quest never posts, builds or reads a post.
- **Dialogue** (Frederick, plain):
  - **ask:** "Is the Watch paying for camps?"
  - **say:** "#greet#, #address#. The camps the realm broke for the Line keep coming back, like weeds in a path. The Watch pays for every one broken again. Any of the nine on the Line's roads will do. Bring down its bandits, break it, and come and tell me which."
  - **accept:** "I'll break one." **decline:** "Not today." **handIn:** "One camp the fewer."
  - **thanks:** "#thanks#. One fewer on the roads. I'd buy you a drink, but the Watch pays me in coal and promises. Take the bounty instead."
- **Talk** (key `st_bounty=open`):
  - any town, `self.dialect=plain` (helpful): "The broken camps come back where the roads cross the wild: Alder Mere, Glasswater, the Frosthollow road. The Watch pays for each one broken again."
  - Coldwater, coldwater (helpful): "Camp's back by Glasswater, eh? The Watch in the city pays for it broken. Net and line, go in a pack."

### S3. `st-midsummer-road`: The Midsummer Road
- **Phases:** `s4`, and only in the summer festival days: offered from Ridgemere's `festival` event (Midsummer, which opens Ch 4 at about day 137) for 3 game days (story timer `fair_days`). **Scope:** party. **Repeat:** weekly (in practice once a year). `join.cap` 12. **Board:** `board:ridgemere`.
- **Giver:** Cedric Mudley `rm_cedric_mudley`: m 50, holds the north gate (works `w_ridgemere_1_0` (104, 159)); home `rm_the_watch_house`; plain speech, light; loud and suspicious, can name every ship in the Sound. Alive, placed, home and work reachable.
- **Premise.** Midsummer, and half the north walks the north road to Ridgemere's fair, pups and grandmothers and all. Bandits know the fair days too. Walk the north road out past the bend to where the woods close in, and clear whatever is waiting.
- **Regions** (sampled):

| id | cell | box | what |
|---|---|---|---|
| `north_road_a` | `w_ridgemere_1_0` | (96, 100)–(115, 119) | the dirt road north of the gate (400 open, 60 road) |
| `north_road_b` | `w_ridgemere_1_0` | (96, 20)–(115, 39) | the far stretch, where the woods close in (400 open, 66 road) |

- **Objectives:**

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `near` | Walk the north road from the gate to the bend by day. | story signal | `road_near` | 1 | `region entered` `north_road_a`, `hour between 8 20` |
| `far` | Walk on to where the woods close in. | story signal | `road_far` | 1 | `after: [near]`; `region entered` `north_road_b`, by day |
| `band` | Clear the bandits waiting for fair-goers. | bandit falls | cell `w_ridgemere_1_0` | 2 | the spawn ladder (§3) runs when a party enters either region, in the region whose `spawned_` switch is off, so two parties fight at once |
| hand-in | to Cedric | | | | `handIn: true` |

- **Rewards:** 10 from `town:ridgemere`; deed "kept the north road safe for the Midsummer fair"; `story.practice watch`; `item.give cider 1`.
- **Feed:** `nh_realm +8`, `nh_voice_ridgemere +8`; each road camp cleared `nh_threat −1`.
- **Dialogue** (Cedric, plain):
  - **ask:** "Busy at the gate?"
  - **say:** "#greet#, #address#. Midsummer, and half the north comes down this road for the fair, pups and grandmothers and all. Bandits know it as well as I do. Walk the north road out past the bend to where the woods close in, and clear whatever's waiting. I can't leave the gate."
  - **accept:** "I'll walk the north road." **decline:** "Not today." **handIn:** "The road's clear."
  - **thanks:** "#thanks#. Clear road, full fair. I'll stand here and count them in, and none of them will know your name. That's the job."
- **Talk** (key `st_fair=open`, for the fair days): Ridgemere, plain (helpful): "Fair-goers on the north road this week. The gate guard wants it walked. Bandits know the fair days too."

### S4. `st-twelve-bounds`: Twelve Bounds in a Day
- **Phases:** `s56`. **Scope:** player. **Repeat:** weekly. No cap (no fight). **Board:** `board:oakhaven`.
- **Giver:** Ada Coalstack `cty_ada_coalstack`: f 48, keeps bees at Heatherside (works `fs_bees_rainward_hives`, home `fs_bees_rainward_home`, in the Rainward Hills, North); southmeadow speech, light (mantle sm101, "the beekeeper with river stones"); shrewd and wry, sly with the truth; "once petitioned the Council of Houses and lost, and has not forgiven it". Alive, placed, home and work reachable.
- **Premise.** In the old days the bounds of the Rainward Hills and the cut were walked in a single day, every one, so nobody could say the line had moved. Ch 5's bounders say it has. Walk all twelve in one game day. The pack defines its **own** twelve regions (decisions §7.3), so it never reads or scores the campaign's; a wolf who walks both scores both, which is right.
- **Regions** (14 × 14 boxes, each 191–196 of 196 tiles open, no water):

| id | cell | box | | id | cell | box |
|---|---|---|---|---|---|---|
| `bound_01` | `w_ridgemere_1_3` | (188, 188)–(201, 201) | | `bound_07` | `w_ridgemere_3_3` | (16, 168)–(29, 181) |
| `bound_02` | `w_ridgemere_2_3` | (168, 172)–(181, 185) | | `bound_08` | `w_ridgemere_3_3` | (228, 104)–(241, 117) |
| `bound_03` | `w_ridgemere_3_0` | (36, 240)–(49, 253) | | `bound_09` | `w_ridgemere_4_1` | (52, 220)–(65, 233) |
| `bound_04` | `w_ridgemere_3_1` | (152, 108)–(165, 121) | | `bound_10` | `w_ridgemere_4_2` | (224, 176)–(237, 189) |
| `bound_05` | `w_ridgemere_3_2` | (112, 68)–(125, 81) | | `bound_11` | `w_ridgemere_4_3` | (124, 232)–(137, 245) |
| `bound_06` | `w_ridgemere_3_2` | (160, 180)–(173, 193) | | `bound_12` | `w_ridgemere_4_4` | (16, 40)–(29, 53), at the Oakhaven cut |

- **Objectives:**

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `begin` | Begin at any bound of the hills. | story signal | `bounds_begun` | 1 | the first `region entered` of any `bound_*` starts the player timer `bounds` (1 game day) and the player list `st_bounds` |
| `walk` | Walk all twelve bounds before a day is out. | story signal | `bound_walked` | 12 | each `region entered` of a `bound_*` not yet in `st_bounds` while `bounds` runs: `list add`, `objective.progress`. When `bounds` goes off before 12, the list and the objective reset ("the day ran out") |
| `cut` | End at the boundary stone by the Oakhaven cut. | story signal | `bounds_cut` | 1 | `after: [walk]`; `region entered` `bound_12` once the list holds 12 (the cut may also be walked earlier; the wolf comes back to it) |
| hand-in | to Ada | | | | `handIn: true` |

- **Rewards:** 12 from `town:ridgemere`; deed "walked all twelve bounds of the Rainward Hills in a day"; `story.practice travel`; `item.give honey 1` (Heatherside's, first time only).
- **Feed:** the bounds are the cut's, so `nh_voice_oakhaven +8` and `nh_realm +8`.
- **Gates:** phases only. If `nh_b_hills` is set (Ch 5's result), Ada's `npc.line` for the phase reads the matching line below; nothing is written.
- **Dialogue** (Ada, southmeadow, light):
  - **ask:** "You walk these hills a lot?"
  - **say:** "#greet#, #address#. The old bounds of these hills were walked in a day, once, every one of them, so nobody could say the line had moved. I petitioned the Council about these hills and lost. Walk all twelve in a day and they'll know it's been done again. Begin wherever you like. Just don't stop."
  - **accept:** "Twelve bounds, one day." **decline:** "My paws say no." **handIn:** "Twelve bounds walked."
  - **thanks:** "#thanks#. Twelve in a day. Sheaf and sickle, the Council can keep its papers. The hills have your paw-prints now. Here, the honey's on me."
  - `npc.line` (phase `s56`): "Bounders in the hills, and green stakes where the old line ran. Walk the bounds, my apple, before somebody moves them."
  - `npc.line` (phase `s56`, if `nh_b_hills` is set): "Whatever the Council says now, the hills were walked in a day this year. I'll remember that, even if they don't."
- **Talk** (key `st_bounds=walked`, set by C8 at the first completion in the phase):
  - Oakhaven, oakhaven: "Somebody walked all twelve bounds of the hills in a day, mm? Old Oak, the old folk did that."
  - Ridgemere, plain: "Somebody walked every bound of the Rainward Hills in one day. Let the Council argue with that."

### S5. `st-post-night`: A Night on the Line
- **Phases:** `s9`, from Ch 9's opening until the call of the Moot night (decisions §6.4): withdrawn when the world text `nh_moot` is set, a game day before the night. No game day is read. **Scope:** party. **Repeat:** weekly, and once per game night per wolf. `join.cap` 12. **Board:** `board:ridgemere`.
- **Giver:** Beatha Foggate `rm_beatha_foggate`: f 48, a Watch guard on the street patrol (`rm_street_patrol`; evenings at the Watch House); home `rm_the_watch_house`; plain speech, broad; quietly funny and meticulous, "coughs in the damp and pretends not to"; grew up at a Timberland charcoal camp. Alive, placed, home reachable.
- **Premise.** The Moot night is close, and bands test the Line's posts every night to see which ones nobody stands. Pick a post, be there by the twentieth hour and stay till the fifth. If they come, they find a wolf.
- **Regions** (the posts that stand in the world: proj-1 to proj-3 at local (8, 8), and the Pass; sampled):

| id | cell | box | post | purse |
|---|---|---|---|---|
| `post_upper_mere` | `w_ridgemere_8_6` | (0, 0)–(19, 19) | 2, the Upper Mere post (proj-1) | `town:frosthollow` |
| `post_oakhaven_road` | `w_ridgemere_5_4` | (0, 0)–(19, 19) | 10, the Oakhaven road post (proj-2) | `town:oakhaven` |
| `post_waystation` | `w_ridgemere_1_1` | (0, 0)–(19, 19) | 12, the Ridgemere waystation (proj-3; 196 open, streets) | `town:ridgemere` |
| `post_pass` | `w_ridgemere_9_3` | (0, 18)–(14, 38) | 1, Frosthollow and the Pass | `town:frosthollow` |

- **Objectives:**

| id | label | on | match | goal | notes |
|---|---|---|---|---|---|
| `post` | Be at a post of the Line by the twentieth hour. | story signal | `post_taken` | 1 | `hour` 20: each participant in a `post_*` region; the player text `st_post` keeps which |
| `night` | Stand the post through the night. | story signal | `post_hour` | 9 | each `hour` from 21 to 5: +1 for each participant still in the region `st_post` names |
| `raids` | Drive off the raids. | bandit falls | the post's cell | 2 | at 23:00 and 2:00, `bandits.call {player: $each}` on each standing wolf, scaled by party (1 → 2, 2–3 → 3, 4+ → 4): each party gets its own band, so a crowd at one post never queues |
| hand-in | to Beatha | | | | `handIn: true` |

- **Rewards:** 12 from the post's purse (the table above; its own `coin_<purse>_day` counter); deed "stood a night on the Howl Line before the Moot"; `story.practice watch`; `item.give blanket 1`.
- **Feed:** the post's town: `nh_voice_frosthollow +12` (Upper Mere, the Pass), `nh_voice_oakhaven +8`, or `nh_voice_ridgemere +8`; `nh_realm` the same. It never raises, posts or stocks a post: that is campaign Ch 9's.
- **Dialogue** (Beatha, plain, broad):
  - **ask:** "The Watch wants the posts held?"
  - **say:** "#greet#, #address#. The Moot night's close, and there are bands testing the Line's posts every night, to see which ones nobody stands. Pick a post. Be there by the twentieth hour and stay till the fifth. If they come, they find you. I'd go myself, but somebody has to cough on the city walls."
  - **accept:** "I'll stand a post." **decline:** "Not tonight." **handIn:** "The post stood all night."
  - **thanks:** "#thanks#. A whole night, and the post still standing. Go and sleep. The Line needs you awake on the Moot night more than it needs you now."
- **Talk** (key `st_posts=probed`, at phase entry):
  - any town, `self.dialect=plain` (helpful): "Bands are testing the Line's posts at night. Stand one from dark till dawn, and take a friend."
  - Frosthollow, frosthollow: "Somebody stood the Pass post all night in the cold, cousin. Warm hearth to them. Deep cold, that."

**What these five replace.** Campaign §7's large seeds and cast-and-talk §3.25's large rows point here: the Patrolman's Round (S1), walk all twelve bounds (S4), stand a post all night (S5), plus the bounty (S2) and the Midsummer road (S3). Silver Under the Ice weekly and the Frosthollow Count stay the campaign's own (decisions §7.3, "Dropped").

---
## 13. Engine needs

Names follow decisions §5.1: C10 `foe.add` / `foe.outlaw`; C12a `story.notify`; C12b `match`/`field`; L1 `hunt.stage` (M1 dropped); L2 named camps (M4 merged); SQ2 tills (M6 the same fix); N2 window facts (T2 merged); C8 with `scope: pair` (T1 merged). The large packs need **engine Phase 1** (V1, SQ2, J1, C8), plus J3 (Phase 4), L1 (Phase 5) and C10 (Phase 7) (decisions §5.2). Every line also runs on today's engine (§13.3).

### 13.1 The items these lines cite

| Item | What it gives these lines | Lines | Without it |
|---|---|---|---|
| C1 variables in effect parameters and texts | the next posted ship stormed (Q7); a run's time in board text (Q8); restitution sized to the wrong done | Q1, Q7, Q8 | literal branches (`random` over ships; three time bands) |
| C2 `audience` operand | goals and run days sized by active wolves | all | helper-grown goals (§1.3) |
| C3 `rank` / C4 `title.give` | "the Long Runners" and other line titles | Q8, any line | deeds only |
| C5 `npc.greet` | Vigdis, Ivy, Bettany and Hester thanking or shunning one wolf by name | Q2, Q4, Q7, Q10 | the resident's gated window line (`say[self.id=… self.mem.<k>=<v>]` or `world.mem`) for everyone, plus a `deed.award` that carries the news; **never** a `story.tell` with `from` (decisions P2-22) |
| C6 `requires` and `offer.during` | chapters 2–3 with real givers; road choices shown only to the wolf on that road; "still on my errand?" lines | all | journal offers (§1.1); untargeted choices with `once: true` |
| C8 `talk.remember`, `belief.plant` | every gossip, helpful and window line below; the Heights rumour | all | the DM seeds keys by hand |
| C10 `foe.add` | named band leaders: the Brine Captain (Q1), the rustlers' drover (Q3), the weir-boss (Q9), the charcoal thieves' leader (Q11) | Q1, Q3, Q9, Q11 | anonymous bandits |
| C11 `npc.spare` | Dirk Rainmore (74) kept alive while Q4 runs | Q4 | `resident is alive` guards, with the jeweler's shop as the fallback clue |
| C12a `story.notify` | personal credit in the running campaign chapter; Q4's hushed beam as a Purse-path deed | all | towns and the realm only |
| C14 lasting resident fates | Vigdis's or Ilse's post; Vance House's rates | Q4, Q7, Q10 | talk keys and holds |
| J1 per-wolf `npc.offer` (campaign §11.2) | every chain step shown only to the wolf who reached it | all | untargeted offers with `once: true` |
| J3 quest `closure: true` | each line counts once toward the Quickening's `closedStories` (answer (b), capped at 4 a program) | all eleven (roads) | inert: the flag is carried and ignored |
| J8 `belief.forget` | clearing the "Quickened thief" rumour | Q10 | a deed that spreads the truth |
| J9 tagged items | the Couriers' own tube (Q8); the raiders' tally (Q1) | Q1, Q8 | the generic `document_tube` / `paper` |
| J10 `wolf has gift` | the Gifted step with Hester | Q10 | offered to all, worded "if you carry a Gift" |
| N2 window facts with `player.story.<var>` | the giver's thanks by the wolf's own road (Q4 `lq_beam`, Q7 `lq_lamp1`) and the thin-purse thanks (`lq_thin`); conversation targets' lines by the wolf's step | Q1–Q11 | one thanks for all; the thin purse in an `npc.line` variant |
| SQ2 till purses | the shop's own till pays its restitution, bribe, share or contract | Q1, Q2, Q4, Q6, Q9 | the treasury or the town store under the same counter rule, the reason text still naming the shop (§1.5) |
| N4 quest `join.again` | not used: no large quest has `join.cap: 1` | — | — |

### 13.2 The L items, as engine-spec catalogues them

**L1, effect `hunt.stage`** (Phase 5, "Howls, crowds and the wild"; fixes the bear and the boar; Q3, Q5).
```json
{"kind":"hunt.stage","region":"greytarn","species":"bear","count":1,"minutes":240}
```
- The next hunt begun inside the region (`hunt` event) has that animal in its arena, as well as whatever the ground gives. `species` is checked against `Data/Wild/animals.json` ids, `count` is 1–3, and the stage lapses after `minutes`.
- The kill raises the usual `hunted` with `target` the species, so quest objectives stay as written.
- Undone at the run's end. It needs `RatwHunt.cpp` to read a staged list when building an arena.
- `actsOnWolves: false`.

**L2, `camp` on `story.area.spawn` and `bandits.call`** (Phase 2; fixes §1.4; every line with fights, and the stage pack).
```json
{"kind":"story.area.spawn","region":"weir","count":4,"camp":"lq_eel_weir"}
```
- The camp's id becomes `camp_story_lq_eel_weir`, so `bandit falls` and `camp cleared` can match `target` exactly.
- The name is `[a-z0-9_]{3,30}` and begins with the program's `prefix` and `_` (`lq_` here, `st_` for the stage pack); the validator refuses any other. While the camp stands, a second spawn with the same name adds to it; once it is cleared, the name may be reused (decisions §5.2, one rule replacing M4's and this file's old "unique per run").
- Until then: `cell` plus the `spawned_<region>` switch (§1.4).

**L3, more mission events** (Phase 2; fixes "objective on a world event"; all lines).
- Mark `camp cleared`, `caravan arrives`, `ship lost`, `ship docks`, `project built` and `festival` as `mission` in the catalogue, with the fields they already have.
- `story_quests.py` already treats any mission event alike, so quest objectives may then name them directly instead of going through a trigger and `objective.complete`. Programs move to them in a new version.

**L4, effect `npc.sentence {npc, place, days 1-28}`** (Phase 8; a lasting hold; Q7).
- The resident is moved to `place` (it must be a reachable interior, such as `r2_the_keep_down`) and held there for `days`. Unlike `npc.hold`, the sentence **outlives the run** and ends by itself. It is kept in the game's own list (as `noteStoryHeld`, but not cleared at `endStoryWorld`).
- Refused on a protected resident.
- Events `sentence begun` and `sentence served` (`actor` the resident).
- A lighter, bounded cousin of C14: the resident keeps their post and home.
- Until then: the hold is re-applied at each visit.

**L5, effect `quest.withdraw {quest, player}`** (Phase 4, optional; branch roads; all lines).
- Takes back an offered (not accepted) journal quest from one wolf, with no "failed" mark. Used when a wolf accepts one road of a chapter.
- C6 `requires` makes it unneeded, since the other roads would never be offered. Until then: `quest.fail` with "another road taken".

### 13.3 Today-only version (no engine change)
Every line, and every stage quest, runs on today's engine with these losses. This is the version that ships with Act I (decisions §6.2):
- **Q3 and Q5:** the bear and boar objectives wait on the wild (L1).
- **All lines:**
  - camp matching by `cell` plus a switch (L2);
  - world-event objectives via triggers (L3);
  - road alternatives withdrawn with `quest.fail` (L5);
  - gossip and window lines that only play after the DM seeds a key (C8);
  - untargeted `npc.offer` and road choices (J1, C6);
  - **coin from the treasury and the town stores only**, never a till (SQ2), under the `coin_<purse>_day` counters and floors of `nh-purses` (§1.5);
  - `closure: true` carried but inert (J3).
- **Q7:** Ilse's week in the cells re-applied at each visit (L4).

Nothing in any line **needs** a new effect to finish.

---

## 14. Giver budget and checks before approval

### 14.1 Who holds which hub slot (decisions §1.2: at most one pack quest per resident, across all packs; campaign cast, keepers, innkeepers and look-alikes none)

| Resident | Line | Hub item |
|---|---|---|
| Orm Sootgate | Q1 | quest `brackish`; `npc.offer` `lq_salt_wagon`; road choice `lq_salt_cart` (one quest) |
| Osric Ropeworth | Q1 | two `npc.offer`s, in sequence, never at once |
| Fenwick Mossdale | Q1 | one `npc.offer` (`lq_salt_board`). In Q4 he is a conversation and gift target only |
| Ivy Stonehollow | Q2 | quest `ringbark`; road choice `lq_ced_return`. In Q10 she is only a gift target |
| Flint Snowdrift, Senna Beechleaf | Q2 | one `npc.offer` each |
| Brenna Slaggate | Q2 | two `npc.offer`s, in sequence |
| Jessamy Greyford | Q3 | quest `wool` |
| Juniper Larkcombe, Vale Pikebridge | Q3 | one `npc.offer` each |
| Pike Chalkley | Q4 | quest `light`; the beam choice (two `npc.offer`s of the same quest) |
| Harrow Larkwell, Dirk Rainmore, Vigdis Rainmore | Q4 | one `npc.offer` each |
| Sloe Harvestby | Q5 | quest `leaves`; road choice `lq_vine_cure` |
| Aster Cornley | Q5 | one `npc.offer` (a campaign target, which allows one pack quest: decisions §1.2.2) |
| Brisk Lakemere | Q5 | one `npc.offer` |
| Theodora Wickley | Q6 | quest `cough` (her only pack quest) |
| Ivo Fogstack, Halvard Ashley, Dunstan Greywick | Q6 | one `npc.offer` each |
| Rupert Fogton | Q6 | two `npc.offer`s, in sequence |
| Linden Sedgeditch | Q7 | quest `lamp` |
| Septimus Plankmarsh, Ivo Cinderham | Q7 | one `npc.offer` each |
| Ilse Coalley | Q7 | two `npc.offer`s, in sequence |
| Grete Dimmock | Q8 | quest `trial` |
| Florence Pennock, Hanne Ash, Orm Carlisle, Philippa Pitchstack | Q8 | one `npc.offer` each |
| Ivo Sootridge | Q9 | quest `traps` |
| Rowan Cragpeak, Jasper Greymore, Karin Wetridge | Q9 | one `npc.offer` each |
| Nab Bogditch | Q9 | two `npc.offer`s (the second only while `nh_stage` is 4–6) |
| Petra Vance | Q10 | quest `locked`; later two road choices |
| Roald Wickstack, Silas Brinemore, Palle Brinewell | Q10 | one `npc.offer` each |
| Hester Greyby | Q10 | two `npc.offer`s, in sequence |
| **Marl Craghearth** | **Q11** | quest `undersold`; three road choices (one quest) |
| Bertil Saltcot, Frederick Rainham, Cedric Mudley, Ada Coalstack, Beatha Foggate | S1–S5 (`nh-pack.stage`) | one stage quest each |

**Targets only** (no hub item; decisions §1.3; none is a target in more than two pack quests):

| Resident | Their own pack quest | Target in |
|---|---|---|
| Kite Bogditch | medium `mq-crossroads-names` | Q1 (conversation) |
| Fern Merewick | medium `mq-lost-raker` | Q1 (conversation) |
| Rosamund Coalford | small `sq-smokehouse-wood` | Q1 (conversation, gift) |
| Frost Colddrift | small `sq-venison-for-the-cut` | Q2 (conversation) |
| Stone Acorndell | medium `mq-lamp-oil-share` | Q3, Q11 (conversation) |
| Imogen Rustford | medium `mq-mud-to-the-axles` | Q4 (conversation) |
| Harl Oakbough | medium `mq-vineyard-cellar` | Q5 (conversation, gift) |
| Corwin Bilgestack | small `sq-dawn-bell` | Q7 (conversation) |
| Ebba Coalgate | medium `mq-low-drift-props` | Q11 (conversation); also small #29's target |
| Iris Fernhaven | small `sq-clamp-watch` | Q11 (conversation) |
| Leif Driphand | small `sq-scree-goats` | Q11 (conversation) |
| Arne Ashton | none | Q11 (conversation, gift) |
| Liv Slagstack | none | Q6 (gift) |
| the `lq_sick` twelve, the `lq_owners` (Piet Wickridge, Runa Ropemarsh) | none | Q6, Q10 (gift) |
| **Campaign cast:** Bettany Nailley, Solveig Tarwell (Q7), Ivy Stonehollow (Q10) | — | conversation or gift targets only, where the campaign's facts agree (decisions §1.2.2) |

- **Innkeepers** carry only the recap lines (`inn.greet`). No line here sends a wolf to an innkeeper (decisions §1.2.4).
- **Withdrawn from the medium pack into these lines:** Q5 `mq-net-court-cough` (into Q6), Q7 `mq-false-light` (into Q7's chapter 1), Q11 `mq-short-weighed` (into Q4's chapter 1). Their old givers keep one pack quest each: Theodora (Q6), Septimus (an offer in Q7), Pike (Q4).
- **Program budgets** (each line, worst case): at most 9 quests (cap 32); at most 40 triggers in a phase (cap 64); at most 14 offers and choices (cap 64); at most 30 variables (cap 64), the shared `coin_*` counters included; 1 tracker per wolf; 1 scene (Q4, Q7) or none; at most 10 regions (Q8 has 9; Q11 has 5). The stage pack's five large entries use 30 regions between them (S4's twelve bounds the most), inside the program's own budget, which the small reviser keeps.

### 14.2 Checks before approval (decisions §1.2.7 and P2-24; story-format survey §9)
1. `story_program.review(p)` passes with nothing in `uncovered`; `problems(p) == []`; `story_runner.references(conn, 'upper_accord', p)` passes on ridgemere3.
2. One rehearsal case per phase and per road. For the reckoning, also cases for "a tie" and "nobody chose", which must reach the default. A case for a thin purse (`coin_<purse>_ok` off) and one for a wolf at the line's 35.
3. **The giver check** (decisions §1.2.7), run on `givers.tsv` as written: one pack role per resident, none on campaign cast, keepers, innkeepers or look-alikes.
4. **The resident query** (decisions §1.1) for every resident named in this file, read only: alive (`live.npc_state.alive`), placed, home and work reachable. Run 2026-11-22: all pass, including the new names of this revision (Marl Craghearth, Arne Ashton, Leif Driphand, Iris Fernhaven, Ebba Coalgate, Liv Slagstack, and the five stage givers).
5. **By hand, because approval misses them** (until V1's additions):
   - every objective `match` id is a placed, living resident or a real region, place or item (all listed above);
   - every `character.grant` purse is one in §1.5, and no `till:job:` purse appears before SQ2;
   - every coin payment uses the §1.5 counter pattern;
   - every `board` is one of the eight towns.
6. **By eye:**
   - every `say`, `thanks`, `npc.offer.reply`, `npc.line` and window line for symbols, dialect and length, against the speaker's `live.npc_voice` (accent `none` is plain);
   - no `story.tell` with `from`, and no quoted resident words in any `story.tell`;
   - the forbidden words (the word for wolves' tame cousins, any word for the divine other than the Bright One and the Dark One, the word for private messages);
   - "Warden" in no title.
7. **The talk tools** on the new `Data/Talk` lines (the gossip tables and the window lines): `python3 tools/dialect_check.py --world ridgemere3`; `build-core/talk_check --data "$PWD/Data/Talk" --validate …` and `--render-trees`; `python3 tools/talk_lint.py`. Add "Saltpan" (already allowed), "Kestrel", "Kestrel Tarn", "Greytarn", "Cistern Court", "Runnel", "North Cove", "Sunken Street", "Couriers' Office", "Lamp Store", "Seven Sisters", "Upper hearths", "Greenholt hearths" and "Honey Crock" to `tools/dialect_allow.json` "places" if `dialect_check` objects.
8. **World variables** declared once with `world-var declare`, with notes: `lq_salt`, `lq_salt_sacks`, `lq_cedars`, `lq_brown`, `lq_rustlers`, `lq_silver`, `lq_beam`, `lq_vines`, `lq_runnel`, `lq_wreck`, `lq_post`, `lq_post_time`, `lq_eels`, `lq_thief` and `lq_hearths`; the stage keys `st_round`, `st_bounty`, `st_fair`, `st_bounds`, `st_posts`; plus the campaign's `nh_*` names and `nh-purses`' `coin_<purse>_day` / `coin_<purse>_ok`, which these programs touch with the same names and types.
9. **Protection.** Lines that walk a resident through bandit country (`npc.walk` Orm in Q1) protect them for the walk and **unprotect at its end**. `npc.protect` is permanent, so the cleanup trigger runs on the run's `finish` and on every `wagon` end.
