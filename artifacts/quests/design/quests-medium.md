# Medium quests for Ridgemere 3: twenty-three errands that end two ways, and five for the campaign's stages

Quest designer's pack for the medium tier, written beside **The Ninth Howl** (`campaign.md`) and revised to the coordinator's binding decisions (`decisions.md`; the giver registry is `givers.tsv`). Each quest stands alone, takes 30 to 120 real minutes, has 2 to 5 steps, and ends with a choice that changes how it ends. Most also feed one of the campaign's world variables or foreshadow one of its chapters. Five more (§4b) are offered only while one stage of the campaign runs.

World: Ridgemere 3 (world key `ridgemere3`, db `ratw_ridgemere3`, world id `upper_accord`). Checked read-only on 2026-11-22 (game day 66.32, spring, Year 1) through `tools/world_db.connect('ridgemere3', 'owner')` inside `SET TRANSACTION READ ONLY` with a rollback, against `live.npcs`, `live.npc_state`, `live.npc_voice`, `world.links`, `world.interiors`, `world.terrain_chunks` and the checkpoint saved 22:34 UTC (its `roads.caravans`, `roads.contracts` and `society.accounts`). Nothing in the repo or the world was changed.

**What was verified, for every quest:**
- Every giver is the one `givers.tsv` names for that quest: **placed, alive** (`live.npc_state.alive = true`), with a reachable home and work place (decisions §1, the reach query). Their dialect comes from `live.npc_voice.home_region` and their accent from `live.npc_voice.accent`.
- **One pack quest per resident** (decisions §1.2.1). No giver is campaign cast, a Keeper, a Keeper's fallback, an innkeeper or a look-alike. A quest's `npc.offer` choices sit on its own giver, except Q4 (the pup Desmond) and Q17 (the lost raker Ridge), which `givers.tsv` lists as "npc.offer in the same quest".
- Every resident named in an objective is placed and alive. A resident who is only a `conversation` or `gift` target holds no hub item. No target is a target in more than two pack quests. Campaign cast and Keepers appear as targets only where the campaign's own facts agree (named at each quest), and each innkeeper is a target in at most one quest across all files (Q10's two, Q22's one).
- Every interior is on the reachable list. Every region sits on real tiles of its cell (the glyphs were counted box by box from `world.terrain_chunks`; roads are `d`, `_`, `/`).
- Every item id exists in `Data/Items/items.json`.
- Every purse holds money at the checkpoint (§0.4), and no amount is more than its purse's day cap.

**What this revision changed** (decisions §10, "quests-medium.md"):
- **Re-cast givers:** Q1 to Edda Gullford (Piet Lumley a target only), Q14 to Harl Oakbough, Q18 to Tor Chalkfield.
- **Duplicates resolved:** Q5 `mq-net-court-cough` is withdrawn (the Sump cough is `lq-runnel-fever`'s alone); Q7 `mq-false-light` is withdrawn into `lq-wreckers-light` chapter 1; Q11 `mq-short-weighed` is withdrawn into `lq-clipped-silver` chapter 1. Q18's salt wagon is Southmeadow's own, so it no longer repeats `lq-salt-road` (decisions §2).
- **New quests in the freed slots:** Q5 Northgate (`mq-northgate-smoke`), Q7 Timberland Court (`mq-court-larder`), Q11 Larkfield (`mq-linen-bolt`), and a new **Q23 for Oakhaven** (`mq-logs-adrift`) (decisions §7.2).
- **Five stage quests** in full, for the program `nh-pack.stage` (§4b).
- **One purse rule** (P-F, decisions §3) replaces the old `coins_<town>` guard; **closes moved into the dialogue window** (P-D, decisions §9 P2-22); **camps matched by `cell` plus a `spawned_<region>` switch** (P-B, decisions §4); **cooldowns** for the `join.cap: 1` quests (§0.3, decisions §8.1).
- **Engine names** follow decisions §5.1: M1, M4 and M6 are gone (L1, L2 and SQ2); "C12" is C12a.

**Contents**
- §0 How the pack works: programs, patterns, purses, scaling, talk keys
- §1 Ridgemere (Q1–Q7)
- §2 Coldwater and Larkfield (Q8–Q12)
- §3 Southmeadow, Fenshore, Saltwell and the Mirebridge crossroads (Q13–Q19)
- §4 Oakhaven and Frosthollow (Q20–Q23)
- §4b The stage quests (five medium entries for `nh-pack.stage`)
- §5 Engine needs beyond today (catalogue ids, decisions §5)
- §6 Index, and the giver check
- Appendix A: Q1 as a program sketch

---

## 0. How the pack works

### 0.1 Programs
The campaign's rules for packs apply (`campaign.md` §7, as decisions §1.2 restates them): open audience, at most 32 quests per program, and only the world variables a pack touches are declared. The 23 quests fit in four programs, so that none goes over **64 triggers per phase** or **64 offers and choices**. The five stage quests (§4b) live in the stage program, which the small pack's reviser writes (decisions §7.3).

| Program id | Quests | Givers' towns | Triggers (estimate) | Offers and choices |
|---|---|---|---|---|
| `nh-pack.medium.city` | Q1–Q7 | Ridgemere | about 56 | 7 offers + 14 choices |
| `nh-pack.medium.greenholt` | Q8–Q12 | Coldwater, Larkfield | about 42 | 5 + 10 |
| `nh-pack.medium.south` | Q13–Q19 | Southmeadow, Fenshore, Saltwell | about 52 | 7 + 9 |
| `nh-pack.medium.north` | Q20–Q23 | Oakhaven, Frosthollow | about 30 | 4 + 7 |
| `nh-pack.stage` (small reviser's) | the five §4b entries, beside the small and large stage entries | Ridgemere, Coldwater, Southmeadow | about 30 for these five | 5 + 8 |

- **Where the files go:** `Data/Storylines/ridgemere3/packs/` (never `programs/`, which three tests count). The stage program is `Data/Storylines/ridgemere3/quests-stage/nh-pack.stage.json`.
- **Audience:** `{"kind":"open","cap":5000}`. One phase, `open`, with no exits (the south program keeps Q15's `phases` gate, §3). A wolf joins by taking a quest.
- **Repeat:** every quest is `repeat: "weekly"` (7 game days, about 28 real hours), `lifetime: "campaign"`, `abandon: true`. Each has a deadline `fail: [{"afterDays": 2}]` (8 real hours), except where a quest says otherwise.
- **Hand-in:** with the giver (`handIn: true`), except Q19 and Q23, which end where the goods are delivered.
- **Closure (J3): none.** Under the user's answer (b) (2026-11-22), only a large line's last chapter or a campaign side story counts toward `closedStories`, and J3 needs `repeat: "once"`, so the medium quests (weekly) carry no `closure` (doc 86 §11.1, decided). Q3, Q10, Q16 and Q22, the four with the heaviest choices, still end with a journal note inviting the wolf to record it as a social Story of two scenes. The claim is never made in game text.
- **Variables every program declares,** with the same name and type as their owners: `nh_stage`, `nh_realm`, `nh_voice_<town>` and `nh_threat` (campaign §4.1); the purse counters `coin_<purse>_day` (number, world) and floor switches `coin_<purse>_ok` (switch, world) of every purse it pays from, as `nh-purses` declares them (campaign §2.11); one switch `spawned_<region>` (world) per fight region (P-B); and the quests' own player texts (`<q>_pick`) and switches (`<q>_spawned`, `<q>_placed`).

### 0.2 Six patterns used by every quest
Each quest below names its patterns by letter, instead of repeating them.

- **P-A, the night gate.** An objective that must happen after dark is `on: "story signal"` with its own signal. A trigger raises that signal:
  ```json
  {"id":"q1-net-night","on":"region entered","audience":"participant","match":{"target":"sl_netcourt"},
   "if":{"test":"all","conditions":[{"test":"hour between","from":19,"to":5},
        {"test":"objective is","quest":"mq-sump-lamps","objective":"round_a","of":"$actor","status":"to do"}]},
   "actions":[{"do":"emit","signal":"sl_a"}]}
  ```
  `emit` passes the event on, so `$actor` is kept (`story_triggers.py:1270`). The same form gates an `enter place` by the hour (Q7, Q21).
- **P-B, the ambush** (decisions §4).
  - **The objective** is a plain `bandit falls` matched by **`cell`**, never by camp id. Verified in code: `story.area.spawn` names its camp `camp_story_<n>` (`Core/RatwRoads.cpp:1622`) and `bandits.call` names it `camp_dm_<n>`, so no program can know a story camp's id until L2. `camp cleared` is not a mission event, so no quest here has it as an objective.
  - **The spawn.** When the wolf reaches the place, a trigger guarded by the player switch `<q>_spawned` calls `story.area.spawn` in the quest's region. The count scales with the party: 2 for a lone wolf, 4 for two, 6 for three or more (`party size`, at most 6). The same trigger sets the world switch **`spawned_<region>`** on (for example `spawned_cg_camp`).
  - **The clearing.** A trigger on `camp cleared` with `match.cell` = the region's cell and `switch spawned_<region> is on` turns the switch off. It scores nothing: each finisher's threat credit is in P-D. A natural camp cleared in the same cell turns it off too, which is right (decisions §4.4: either way the road is clear).
  - `<q>_spawned` is turned off on `quest completed`, `quest failed` and `quest abandoned`. Any bandit falling in that cell counts, including one from a natural camp, which is fine.
  - **With L2** (Phase 2): each spawn names its camp, `camp: "mq_<region>"` (the program's own prefix), so it is `camp_story_mq_<region>`; a second wolf's spawn adds to the standing camp; a new version matches `bandit falls` by exact `target`. **With L3**, `camp cleared` may become an objective.
- **P-C, the choice.** The choices sit on **the quest's own giver** wherever they can (decisions §1.2.1). When the step before the choice is done (`objective completed` with `item` = that objective and `detail` = the quest), two `npc.offer`s go to that wolf (`player: "$actor"`, `once: true`). On `dialogue choice` with `detail` = the offer id, and if the wolf's copy is `active`, the trigger sets the player text `<q>_pick` and emits `<q>_chose`. That signal completes the required objective `choose`.
  - Until **J1** exists, `player` on `npc.offer` overwrites: the last wolf to reach the choice is the one who sees it. With few wolves this does no harm. The `quest is active` guard stops a wolf from picking for a quest they are not on.
  - Where the choice is made by what the wolf does (Q16's scene, Q18, Q19, Q23 and the stage quest S3), there is no `npc.offer`.
- **P-D, the ending.** On `quest completed` with `detail` = the quest, an `if`/`elif` on `<q>_pick` gives that ending's deed, coins (through P-F) and close, then feeds the campaign.
  - **The close is the giver's words in the dialogue window** (decisions §9, P2-22). No `story.tell` carries a resident's words or a resident's face. Each quest's "Close" lines are delivered by the best vehicle the program's version has:
    1. **Today-only version (Act I, no engine change):** no close is spoken. A plain `journal.note` to the wolf, narration with no quotes and no `from`, says what came of it (each quest gives its notes). The deed lands.
    2. **With C8 pair scope (Phase 1):** `talk.remember {scope: "pair", npc: <giver>, player: "$actor", key: "mq_<q>", value: "<ending>", days: 7}` (the key is the quest id with underscores, for example `mq_sump_lamps`), and the close is the giver's own tree line `say[self.id=<giver> pair.mem.mq_<q>=<ending> priority=3]` in `resident.hub` (`Data/Talk/rules/story_mq.talk`). The giver says it the next time that wolf opens the window.
    3. **With C5 (Phase 3):** `npc.greet {npc: <giver>, player: "$actor", text: <close>, days: 7}` in the ending's branch, and the pair line is dropped in that version. Every close is one line of at most 200 characters, raw text in the giver's dialect.
  - Then: `add nh_realm`, then `add nh_voice_<town>` by the distance weight. A medium quest is worth 4: Ridgemere and Oakhaven 4, Coldwater, Larkfield and Frosthollow 6, Southmeadow, Fenshore, Saltwell and Mirebridge 8;
  - for a fight, `subtract nh_threat 2`, then `clamp nh_threat 0 100`;
  - add 1 to the story number `<q>_<ending>` (P-E);
  - with **C8**, `talk.remember` the ending's town key (§0.5).
  - The quest's own `rewards` hold only what every ending shares: `story.practice` and, where there is one, a common item.
- **P-E, the town answers.** Each ending keeps a story number. When it reaches 3, then 10, then 30, the town answers: a `board.post` on the town's board (a notice in the town's voice, signed by no resident), and with C8 a stronger talk key (for example `mq_lamps=lit`: "the Sump round is lit end to end"). This is how a crowd changes a town more than one wolf can.
- **P-F, the purse** (decisions §3). Every coin reward in this pack goes through the same branch, with the amount and purse the quest names:
  ```json
  {"do":"if","if":{"test":"all","conditions":[
      {"test":"switch","var":"coin_ridgemere_ok","is":true},
      {"test":"compare","left":{"var":"coin_ridgemere_day"},"op":"<=","right":125}]},
   "then":[{"do":"effect","effect":{"kind":"character.grant","player":"$actor","coins":10,"purse":"town:ridgemere",
            "reason":"the city pays for a walked lamp round","ifRefused":"skip"}},
           {"do":"add","var":"coin_ridgemere_day","value":10}],
   "else":[{"do":"effect","effect":{"kind":"item.give","player":"$actor","item":"bread","quantity":2}},
           {"do":"effect","effect":{"kind":"journal.note","player":"$actor","text":"The purse is thin today. You were paid in bread."}}]}
  ```
  - The `compare` right side is the purse's **day cap minus the amount**, written as a literal (125 = 135 − 10). The caps are in §0.4.
  - The floor switch is `nh-purses`'s: off when the purse falls below 60, on again at 80. `nh-purses` also sets every `coin_*_day` back to 0 on `day begins`. **This pack only reads the switches and adds to the counters.**
  - The thin-purse variant of the thanks is that journal note. No offer `thanks` and no close names coin, so each fits either way.
  - Amounts are 6 to 15 pennies a wolf. Bread is the payment in kind unless a quest names its own item.
  - **No till is named before SQ2** (engine Phase 1): a shop's payment comes from the treasury or the town store, and its `reason` keeps the shop paying. When SQ2 lands, a new version swaps the purse and its counter (Q6 only: `till:job:rm_edric_millton`, counter `coin_till_edric_millton_day`, cap 19).

### 0.3 Scaling from 5 wolves to hundreds
- **Copies at once.** `join.cap` is the number of wolves on a quest at the same time: 1 where a resident is held or walked for the quest (Q4, Q12, Q17, Q18, Q20), 3 where a spawn or a placed item is per wolf, 6 elsewhere.
- **The `join.cap: 1` quests** (decisions §8.1; the decisions name Q4, Q17 and Q18, and the same rule is applied here to Q12 and Q20, which hold a resident too):
  1. `repeat: "weekly"`, never daily;
  2. a deadline of **one game day** (`fail: [{"afterDays": 1}]`, 4 real hours), so the copy comes back quickly;
  3. an **"it's taken" helpful line** in the giver's town, pointing to another quest there. It is gated on the key `mq_<q>_busy=yes`, which C8 writes (town scope, 1 day) on `quest accepted` and forgets on `quest completed`, `quest failed` and `quest abandoned`. Until C8, the same words go on the town's board for one game day (`board.post`, guarded by the world switch `mq_<q>_posted` so it posts once per copy, and turned off at the end);
  4. **no second copy**: each of the five holds one unique resident (the two pups, the stuck carter, the lost raker, the salt carter, the carter behind the cedar). A second copy would need a second resident in the same plight, and none is placed for it;
  5. with **N4** (Phase 2), each gets `join.again: 7`, counted from any end (done, failed or abandoned).
- **Fights grow with the party** (P-B). A party of three sees six bandits. With **C10** (`foe.add`, Phase 7) a named leader can be added to the wave. Without it, the wave stays anonymous.
- **Towns answer crowds** (P-E): 3, 10 and 30 finishes each bring a board notice and a stronger rumour. With **C2** the steps become `max(3, active/20)`, and so on.
- **Coins never empty a town** (P-F). Each town store pays about one medium quest a day; the wolves after that are paid in bread and still get the deed, the practice and the campaign feed.
- **Deeds carry fame, not coin.** Every ending awards a lower-case deed phrase, so residents greet the wolf with "You … didn't you?" and the criers can call it at the festivals. This costs nothing and scales without limit.

### 0.4 Purses used (checkpoint 2026-11-22 22:34 UTC, `society.accounts`)

| Purse | Account | Holds | Day cap | Pays for |
|---|---|---|---|---|
| `town:ridgemere` (the treasury) | `treasury` | 1,355 | 135 | Q1–Q7, Q23's sold ending |
| `town:coldwater` | `stores:coldwater` | 212 | 21 | Q8, Q9, Q10; S1, S2, S4, S5 |
| `town:larkfield` | `stores:larkfield` | 248 | 24 | Q11, Q12; S3 |
| `town:southmeadow` | `stores:southmeadow` | 236 | 23 | Q13, Q14, Q18 |
| `town:fenshore` | `stores:fenshore` | 213 | 21 | Q15, Q16 |
| `town:saltwell` | `stores:saltwell` | 224 | 22 | Q17, Q19 |
| `town:oakhaven` | `stores:oakhaven` | 247 | 24 | Q20, Q23 |
| `town:frosthollow` | `stores:frosthollow` | 247 | 24 | Q21, Q22 |

- The counters are `coin_ridgemere_day`, `coin_coldwater_day`, `coin_larkfield_day`, `coin_southmeadow_day`, `coin_fenshore_day`, `coin_saltwell_day`, `coin_oakhaven_day` and `coin_frosthollow_day`, with their `coin_<purse>_ok` switches.
- Town shop tills (`till:job:tn_*`) all hold 0, so town quests pay from the town's store. `town:<id>:works|watch|church` accounts do not exist, and no program here names them. Mirebridge has no purse.
- **No shop till is named before SQ2** (decisions §3.2.5). `story_quests.check_purse` accepts only two-part `till:<id>` today, so `till:job:<npc>` is refused at validation. City quests pay from the treasury; Q6's cooper is the one shop that moves to its own till (`till:job:rm_edric_millton`, 197) when SQ2 lands.
- No House has a purse a story can name (M3, deferred to Phase 9), so Q3's "Grayrock pays" ending and Q10's "Fell advance" are paid by the treasury and the Coldwater store.

### 0.5 Talk: memory keys, files and the writing rules
- **Files:** `Data/Talk/rules/story_mq.talk` (barks, smalltalk, and the givers' pair-gated closes and targets' own lines; the five stage quests' lines go in the stage pack's talk file, which its reviser names), `Data/Talk/scenes/common/story_mq.talk` (resident-to-resident scenes), and `say[...]` lines added to `resident.tree` (`gossip`, `news`) and `inn.tree` (`greet`).
- **Gates:** every line is gated on a `world.mem.mq_*` key (the stage quests: `world.mem.st_*`) plus `self.dialect` (and sometimes `self.id` or `self.quarter`), so the test town has none of them and `talk_scorecard_quick` and `talk_golden` do not move. A close is gated on `pair.mem.mq_<q>` (P-D).
- **Who writes the keys:** **C8** `talk.remember` (town scope, 1 to 90 days; pair scope for the closes). It is written at pack start (each quest's "open" value, as a phase entry effect, at most 12 per program) and at each ending (P-D). Until C8 exists, the DM seeds the "open" keys by hand and the ending keys stay unwritten. Deeds still reach gossip without C8.
- **A target's own line.** Where a quest needs a target to say something (Piet in Q1, the crier in Q5, the pup in Q7, the baker in Q11, the skipper in Q23), it is that resident's own tree line in `resident.hub`, gated with **N2** on the wolf's quest (`player.quest.<program>/<quest>=active`) and, until N2, on the quest's "open" key. Such a resident holds no hub item.
- **Rules kept in every line below:**
  - 160 characters at most for talk lines; 200 for a close;
  - no personal names and no ids in talk lines: roles only ("the lamplighter", "the Cinder Gate guard");
  - the speaker's own dialect only;
  - weather words only under a `here.weather` gate, which is written into the tag;
  - never the forbidden word for wolves; only the Bright One and the Dark One.
- **(H)** marks helpful knowledge, a hint toward a step. These go in `resident.news` and `inn.greet` at `priority=2`. Innkeepers give only these and recap lines (decisions §1.2.4).
- **Allow-list additions** for `tools/dialect_allow.json` "places", beyond those in `campaign.md` §10.2: Cinder Gate, Net Court, Knucklebones, Lamp Store, Hollis Physic, Ropewalk, Grey Gull, Heron, Lamp and Pick, Felled Cedar, Meresend, Upper hearths, Waste Road, Sallow pits, Fenside, Dune, Grey Goose, Rag Market, Timberland Court, Linen Shed, Soundside, Cedar Sound, Larkholm, Larkdowns, Carters' Yard, Pass House, Boat Shed.
- **Dialogue in the window:**
  - `say`, `thanks`, `during` and `npc.offer` replies are the giver's own words and go through `#symbols#` (`#greet#`, `#thanks#`, `#address#`, `#oath#`, `#very#`, `#bad#`, `#good#`, `#child#`, `#farewell#`, `#bc_it_is#`). Each symbol fills in the speaker's dialect. `#bc_it_is#` already ends with a full stop, so it stands alone.
  - `ask`, `accept`, `decline`, `handIn` and the `npc.offer` words are the wolf's own, in plain speech.
  - The closes (P-D) and scene lines are raw text, written out in the dialect.
- **Limits:** `say`, `thanks` and `during` 300 characters; `ask`, `accept`, `decline`, `handIn` and `duringAsk` 120; `npc.offer` words 120 and reply 300; deed 160 (lower-case, no full stop); `board.post` 300; `journal.note` and `npc.greet` 200.

### 0.6 What each quest entry gives
For each quest, in this order:
1. id, title, tier, program;
2. places (ids, doors, regions with their tiles);
3. giver and why them;
4. premise;
5. steps, mapped to events;
6. the choice and its endings;
7. repeat and limits;
8. rewards (coin through P-F);
9. what it feeds in the campaign;
10. dialogue (closes through P-D, with their journal notes);
11. gossip and helpful knowledge;
12. engine needs.

Steps run in order unless they are marked *any order* (by `after`).

---
## 1. Ridgemere (`nh-pack.medium.city`)

### Q1. `mq-sump-lamps`: The Dark Round
- **Tier:** medium. **Program:** city. **Town:** Ridgemere. **Quarter:** sump.
- **Places:**
  - the Lamp Store `r2_the_lamp_store` (door `w_ridgemere_1_1` (21, 15));
  - region `sl_netcourt` on `w_ridgemere_1_1`, tiles 20–33 × 56–61 (the lanes outside Net Court, `r2_net_court_house`);
  - region `sl_pawn` on `w_ridgemere_1_1`, tiles 18–28 × 81–84 (the corner by the Pawn and Ledger, `rm_the_pawn_and_ledger`, door (22, 78));
  - region `sl_steps` on `w_ridgemere_0_1`, tiles 198–209 × 64–71 (the cobbles below the Knucklebones, `r2_the_knucklebones`, door (210, 67)).
- **Giver:** Edda Gullford `rm_edda_gullford`. She is female, 41, and lights the lamps at dusk for the Lamp Store. She lives at Tar Corner `r2_tar_corner` (the ground floor, on `w_ridgemere_0_1`). She was raised in Ridgemere and speaks **plain** with a **broad** accent. She is patient and suspicious, calm, cold and guarded, swears by the forge and the sea, and is saving for a stall on the Commons Market. She "lost the old post to someone better placed, and took the work the city had going."
  - **Why her** (decisions §1.3, §2.2): she is one of the Lamp Store's four lamplighters, and a wolf who trusts nobody's word is the right one to be told, or not told, the truth. The old round was **Piet Lumley**'s `rmx_piet_lumley` (60), whose post the world no longer holds: in the data he has no position at all.
  - **Not Ilse Coalley.** Ilse is only the wronged lamplighter of `lq-wreckers-light`. No line of this quest names or describes her; Edda speaks of "the other lamplighters" only in general. The two oil stories never meet: Piet takes Sump flasks for his debt here, and the large line's oil is Lamp Store oil for the headland lamp.
- **Premise:** Since the old lamplighter lost his round, Edda has walked the Sump end on top of her own, and she takes nobody's word for anything. Now the Sump lamps are dry before midnight, and someone is draining them. Walk the round after dusk, find where the oil goes, and decide what she is told.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `round_a` | Light the lamp in the Net Court lanes after dusk. | `story signal` `sl_a` | P-A: `region entered` `sl_netcourt`, hours 19–5 |
| 1 | `round_b` | Light the lamp at the Pawn and Ledger corner after dusk. | `story signal` `sl_b` | P-A on `sl_pawn` |
| 1 | `round_c` | Light the lamp below the Knucklebones after dusk. | `story signal` `sl_c` | P-A on `sl_steps`. Steps 1a to 1c come in any order. |
| — | `pawn` (optional) | Ask at the Pawn and Ledger who has been selling lamp oil cheap. | `conversation` `target: rm_jorunn_ropewick` | a hint; `bonus` gives `story.practice watch`. Jorunn is campaign cast (Ch 2's factors' notes) and allowed here (decisions §1.2.2): a pawnbroker who knows who sells cheap is the campaign's own fact. |
| 2 | `oil` (hidden; after 1a–1c) | Find who has been draining the Sump lamps. | `conversation` `target: rmx_piet_lumley` | `objective.reveal` once 1a–1c are done. Piet's home is `r2_saltgate_row` (door `w_ridgemere_1_1` (68, 59)). His confession is his own tree line (below). |
| 3 | `choose` | Decide what the lamplighter should hear. | `story signal` `sl_chose` | P-C **at Edda**, offered when `oil` is done |

- **Setup:** on `quest accepted`, `item.give lamp_oil 2` to the wolf. These are the flasks for the round.
- **Piet's words, in his own window** (decisions §1.3). Piet is campaign cast (a Ch 2 debtor) and holds no hub item here. When the wolf talks to him, a tree line in `resident.hub` plays: `say[self.id=rmx_piet_lumley player.quest.nh-pack.medium.city/mq-sump-lamps=active priority=3]` with N2 (Phase 1), and `say[self.id=rmx_piet_lumley world.mem.mq_lamps=dark priority=3]` until then, so every wolf who talks to him while the round is dark hears it:
  - "#oath#. Forty years I lit that round, and the Council gave it to nobody. The factor wants his coin by the new moon. A flask here, a flask there. They'd have gone dark anyway, wouldn't they?"
- **The choice** (at Edda, P-C):
  - **a, the truth.** Edda hears all of it and takes it to the Watch Office herself, quietly.
  - **b, cover and share.** The wolf names nobody, pays the oil back, and finds her "an old hand" to walk half the round. The optional `repay` objective (hidden, revealed by choice b) is `gift` `target: rm_edda_gullford` `item: lamp_oil`, goal 2, measure quantity. Its `bonus` gives the deed below.
- **Repeat and limits:** weekly. `join.cap` 6. Deadline 2 game days.
- **Rewards:**
  - shared: `story.practice service`;
  - a: 10 from `town:ridgemere` (P-F), "the city pays for a walked lamp round". It never names the Lamp Store's till (decisions §2.2). Deed "caught the oil-drainer on the Sump lamp round";
  - b: 6 from `town:ridgemere`; deed "kept the Sump lamps lit and an old lamplighter's name clean" (the bonus, if the oil was paid back), otherwise "walked the Sump lamp round in the dark".
- **Campaign:**
  - feeds `nh_realm` +4 and `nh_voice_ridgemere` +4;
  - Piet is one of the campaign's chalked debtors (Ch 2, "owes a House factor money by the new moon"). Ending b leaves him a debtor who still has his pride, so Ch 2's debtor lines play warmer;
  - the Palace lamp (a Sump patrolman holds it since Ch 1) is in Edda's `during` line, so the quest stands in the campaign's world without reading `nh_stage`.
- **Dialogue** (Edda, plain, broad; patient and guarded):
  - ask: "You look worn down to the wick."
  - say: "#greet#. Since the old lamplighter lost his round, I walk the Sump end on top of my own. Somebody's draining my lamps dry by midnight. I take nobody's word, so walk it yourself after dusk: Net Court, the Pawn corner, the Knucklebones steps. Two flasks. Keep your nose open."
  - accept: "I'll walk your round after dark."
  - decline: "Not tonight."
  - handIn: "The round's walked. I know where the oil went."
  - thanks: "#thanks#. A lamp's a small thing until it's out. I'll count them lit tonight, and I'll count you with the honest ones. It's not a long list."
  - during (C6), duringAsk "About the dark round…": "A Sump patrolman holds the Palace lamp now, and the Sump goes dark by midnight. Walk it after dusk. I'll know if you haven't; a cold wick smells."
  - **npc.offer** `sl_tell` at Edda (when `oil` is done). Words: "It's the old lamplighter. He's been taking the oil." Reply: "Him. #oath#, I'd have bet on anyone else. Forty years on that round. I'll take it to the Watch Office myself, quiet, so there's no crowd at his door."
  - **npc.offer** `sl_cover` at Edda. Words: "I've stopped it. Don't ask who. I'll pay the oil back, and an old hand will walk half your round." Reply: "Don't ask. That's a lot to ask of me. Fine. Two flasks back in my store, and if your old hand knows the Sump lamps by their smoke, I'll have him. Half the round. Not a lamp more."
  - **Close a** (P-D, in the window): "The old lamplighter. I took it to the Watch Office myself. They were gentle with him. I wasn't, and I'm sorry for it now."
  - **Close b** (P-D, in the window): "Two of us on the Sump end again, and he knows every lamp by its smoke. I slept a whole night. Don't tell him I said so."
  - **Journal notes** (today-only): a "The lamplighter took it to the Watch Office herself." b "An old hand walks half the Sump lamp round again."
- **Gossip and helpful knowledge** (all Ridgemere, `self.dialect=plain`):
  - `[resident.gossip · world.mem.mq_lamps=dark]` "The Sump lamps go out by midnight now. Somebody's thirsty for oil, and it isn't the lamps."
  - (H) `[resident.news · self.quarter=sump · world.mem.mq_lamps=dark]` "Want the lamp-oil thief? Ask at the Pawn and Ledger who's been selling flasks cheap."
  - `[resident.gossip · world.mem.mq_lamps=reported]` "They found who was draining the Sump lamps. An old lamplighter, they say. Forty years of lighting, and that's how it ends."
  - `[resident.gossip · world.mem.mq_lamps=shared]` "Two lamplighters on the Sump round again. Lit right down to the Knucklebones steps, and about time."
- **Engine needs:** J1 (per-wolf offers), N2 (Piet's line on the quest), C6 (`during`), C8 (talk keys; pair scope for the closes), C5 (closes as greetings). C14 `npc.post` would let ending b give Piet a real half-post; without it, it lives in talk.

### Q2. `mq-cinder-gate-tally`: The Grey Paw at the Cinder Gate
- **Tier:** medium. **Program:** city. **Town:** Ridgemere. **Quarters:** sump and works (the Cinders).
- **Places:**
  - region `cg_gate` on `w_ridgemere_0_1`, tiles 236–244 × 192–203 (the Cinder Gate passage; the cobbles run through x 239–241);
  - region `cg_camp` on `w_ridgemere_1_3`, tiles 130–146 × 195–203 (open grass by the south road below Southgate, where camps keep forming; road tile (138, 195));
  - the Customs House `r2_the_customs_house` (door `w_ridgemere_0_1` (192, 132)).
- **Giver:** Palle Plankmore `rm_palle_plankmore`. He is male, 31, a Watch guard holding the Cinder Gate. He lives at `r2_oakum_stair_house_top`. He speaks plain with a light accent, is quiet, dry and frank, hums sea songs, and lost a brother on the wharf, so he does not forgive House Grayrock.
  - **Why him:** the campaign's seed table names "the Cinder Gate's paw-mark (Palle Plankmore)" for packs. A gate guard can't leave his gate, so he needs a wolf.
- **Premise:** Night wagons go out of the Cinder Gate with a grey paw-mark on their tally boards and no customs seal. They come back light. Palle has his gate to keep, and his grudge to keep quiet. Watch with him after dark, follow tonight's wagon, and bring back the tally.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `watch` | Keep the Cinder Gate with the guard after dark. | `story signal` `cg_w` | P-A on `cg_gate`, hours 20–4 |
| 2 | `follow` | Follow the wagon's ruts out to the south road. | `story signal` `cg_f` | `region entered` `cg_camp` raises it. P-B spawns the wave in `cg_camp`, and `item.place paper 1` at (138, 199). |
| 3 | `fight` | Drive off the wolves waiting at the wagon. | `bandit falls` `cell: w_ridgemere_1_3`, goal 2 | |
| 4 | `tally` | Pick up the tally board the drivers dropped. | `item picked up` `item: paper` | |
| 5 | `choose` | Decide where the tally goes. | `story signal` `cg_chose` | P-C at Palle |
| 6 | `deliver` | Put the tally where you said it would go. | `story signal` `cg_d` | a: `gift` `paper` to `rm_jasper_bilgeford`; b: `gift` `paper` to `rm_palle_plankmore`. Each trigger checks `cg_pick`. |

- **The choice:**
  - **a, the Crown's law.** The tally goes to the Crown's customs collector, Jasper Bilgeford `rm_jasper_bilgeford`, at the Customs House. It is slow, but it fines the House.
  - **b, the Sump's way.** Palle nails the tally to the Cinder Gate for every carter to read. It is quick, it fills his grudge, and Grayrock's name is in the mud.
- **Repeat and limits:** weekly. `join.cap` 3 (the tally is a placed item per wolf). Deadline 2 game days.
- **Rewards:**
  - shared: `story.practice watch`;
  - a: 12 from `town:ridgemere`, "the Crown's finder's fee"; deed "carried a smuggler's tally to the Crown's customs";
  - b: 8 from `town:ridgemere`; deed "nailed a Grayrock tally to the Cinder Gate".
- **Campaign:**
  - feeds `nh_realm` +4, `nh_voice_ridgemere` +4 and `nh_threat` −2;
  - **foreshadows Ch 5**, the toll receipts signed with a grey paw-mark. Grayrock pays the Captain so its freight passes;
  - **reads** `nh_b_toll`: if it is `cried`, Palle's choice reply begins "Same paw as the receipts they read out on Market Square."
- **Dialogue** (Palle, plain, dry):
  - ask: "Quiet gate tonight?"
  - say: "#greet#. Quiet's the word for it. Wagons go out of my gate after dark with a grey paw on the tally and no customs seal, and they come back light. I can't leave the gate. You could. Keep it with me after dusk, then follow the ruts."
  - accept: "I'll keep the gate with you, then follow them."
  - decline: "Not my business."
  - handIn: "The tally's where I said it would go."
  - thanks: "#thanks#. Grey paw on a board. You'd think they'd be ashamed to sign it. Mind the south road."
  - **npc.offer** `cg_crown` at Palle. Words: "This goes to the Crown's customs." Reply: "The Customs House. Slow as a tide, but it bites in the end. Take it to the collector. He hums the same songs I do."
  - **npc.offer** `cg_gate` at Palle. Words: "Nail it to your gate. Let every carter read it." Reply: "Ha. #oath#. Give it here, then. Let the Cinders read whose paw it is. My brother'd have liked that."
  - **Close a** (P-D, in the window): "The collector's got it. There'll be a fine, and the Wharf Office will pretend it's weather."
  - **Close b** (P-D, in the window): "It's up. Half the Cinders came to read it before the Watch changed. Nobody's touched it yet."
  - **Journal notes** (today-only): a "The tally is with the Crown's customs collector." b "The tally is nailed to the Cinder Gate."
- **Gossip and helpful knowledge** (Ridgemere, plain):
  - `[resident.gossip · self.quarter=works|sump · world.mem.mq_tally=open]` "Wagons out through the Cinder Gate after dark, and not a seal on them. Grey paw on the tally, they say."
  - (H) `[resident.news · world.mem.mq_tally=open]` "The night wagons go out by the south road. You'll see the ruts below Southgate if you've a nose for axle grease."
  - `[resident.gossip · world.mem.mq_tally=nailed]` "Somebody nailed a tally board to the Cinder Gate. Grey paw on it. The Grayrock clerks walk round the long way now."
  - `[resident.gossip · self.quarter=office · world.mem.mq_tally=customs]` "The customs collector's had a tally brought in. A fine on a House, if the Council lets it stand."
- **Engine needs:** J1, C8, C5, and J9 (a tagged "tally" item, instead of generic `paper` that any wolf could drop in). C10 `foe.add` (Phase 7) is optional: a named wagon master in the wave. L2 names the camp.

### Q3. `mq-wharf-brother`: What the Wharf Owes
- **Tier:** medium. **Program:** city. **Town:** Ridgemere. **Quarters:** wharf and office. **Closure (J3):** none (answer (b)); a journal note invites a social Story.
- **Places:**
  - the Grey Gull Inn `rm_the_grey_gull_inn` (door `w_ridgemere_1_1` (154, 41));
  - the Records Office `r2_the_records_office` (door `w_ridgemere_1_1` (74, 108));
  - the Grayrock Wharf Office `rm_the_harbourmaster_s_office` (door `w_ridgemere_0_1` (195, 94));
  - the Ropewalk `rm_the_ropewalk` (door `w_ridgemere_0_1` (192, 160));
  - Grayrock Hall `rm_grayrock_hall` (door `w_ridgemere_0_2` (182, 205));
  - the Chapel of the Tide `rm_the_chapel_of_the_tide` (door `w_ridgemere_0_1` (210, 11)).
- **Giver:** Leif Fogcot `rm_leif_fogcot`. He is male, 59, and helps at the Grey Gull Inn; he is not its innkeeper. He lives at `rm_the_barracks`. He speaks plain with a light accent, is chatty, warm and loyal, and "remembers every debt, owed or owing". He lost a brother on the wharf, so he does not forgive House Grayrock.
  - **Why him:** he is the warmest of the 56 who carry the wharf-brother hook, he cooks for strangers, and he is old enough to want it written down.
- **Premise:** Twenty winters ago a Grayrock crane let go of a load over the north pier, and Leif's brother went into the water under it. The Wharf Office wrote "the river took him." Leif wants the truth on paper before he is too old to care. Then he has to choose what the truth is for.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `rolls` | Search the old wharf rolls at the Records Office. | `archive work`, `detail: ridgemere`, goal 2 | any order with 2 and 3 |
| 2 | `office` | Ask the Grayrock Wharf Office about the north pier crane. | `conversation` `target: rmx_grete_dunn` | Grete Dunn, a clerk at the Wharf Office, "honest to a fault" |
| 3 | `rope` | Ask the ropemaker who sold Grayrock the crane rope that year. | `conversation` `target: rm_kari_bilgehand` | Kari Bilgehand, ropemaker at the Ropewalk |
| 4 | `choose` | Decide what Leif's truth is for. | `story signal` `wb_chose` | P-C at Leif, after 1–3 |
| 5 | `end` | See it done. | `story signal` `wb_done` | a: `enter place` `rm_grayrock_hall`, then `conversation` `rm_lavinia_plankwick`. b: `conversation` `rm_solveig_tarwell`, then `gift` `candle_tallow` to her. |

- **What the wolf learns:** the roll says "rope parted". The clerk finds the order: Grayrock bought cheap tarred rope that year, against the Ropewalk's advice. The ropemaker still has the refusal note in her book.
- **The choice:**
  - **a, the debt.** Take it to the Grayrock steward, Lavinia Plankwick `rm_lavinia_plankwick`, at Grayrock Hall, and ask for a widow's due.
  - **b, the name.** Take the brother's name to Solveig Tarwell `rm_solveig_tarwell` at the Chapel of the Tide, so it is read with a candle on Restday.
- **Repeat and limits:** weekly, though each wolf finds the same story. The weekly repeat lets late wolves play it. `join.cap` 6.
- **Rewards:**
  - shared: `story.practice service`;
  - a: 15 from `town:ridgemere`, "a widow's due, paid at last" (M3: there is no House purse); deed "made the Grayrock steward pay a wharf hand's widow's due";
  - b: 8 from `town:ridgemere`; deed "had a drowned wharf hand's name read in the Chapel of the Tide".
- **Scene, ending b** (`scene.play`, individual, in `rm_the_chapel_of_the_tide`; cast `priest` = Solveig): line "Every name the tide took is owed a candle. This one waited twenty winters. I'll read it on Restday, between the bells, so he can hear it."
- **Campaign:**
  - feeds `nh_realm` +4 and `nh_voice_ridgemere` +4;
  - **foreshadows Ch 5**, where the wharf names are read to the absent lord (`nh_b_wharf`), and Ch 6's Rite of the Tide. Ending b's key `mq_wharf=named` gives Ch 5's Solveig lines a "she has read such names before" variant.
- **Dialogue** (Leif, plain, warm):
  - ask: "You remember every debt, they say."
  - say: "#greet#, #address#. Twenty winters ago a Grayrock crane let go over the north pier, and my brother went under the load. The Wharf Office wrote 'the river took him.' The river never tied a knot in its life. Find me the truth on paper, would you, before I'm too old to care?"
  - accept: "I'll find it on paper."
  - decline: "Some things are better left in the water."
  - handIn: "It's done, the way you wanted."
  - thanks: "#thanks#. Twenty winters I've carried that. It's lighter in somebody else's paws for a day. Sit, there's soup on."
  - **npc.offer** `wb_coin` at Leif. Words: "Grayrock owes your family. I'll make the steward pay." Reply: "Pay? #oath#. They'll pay with a face like vinegar. Do it, then. My sister-in-law's roof leaks, and pride never mended a roof."
  - **npc.offer** `wb_name` at Leif. Words: "Let his name be read at the Chapel of the Tide." Reply: "The Chapel. Yes. Coin's spent by spring, but a name read out stays read. Take a candle; the priest likes to light it herself."
  - **Close a** (P-D, in the window): "The steward paid? In coin? Then the Wharf Office can write that down too: 'Grayrock owed him.'"
  - **Close b** (P-D, in the window): "Restday, between the bells. I'll be there in my good collar. Thank you for him."
  - **Journal notes** (today-only): a "The Grayrock steward paid a widow's due." b "A drowned wharf hand's name will be read on Restday."
- **Gossip and helpful knowledge** (Ridgemere, plain):
  - (H) `[resident.news · self.quarter=wharf|office · world.mem.mq_wharf=open]` "If you want to know why a crane rope parted, ask at the Ropewalk. They remember every order they refused."
  - `[resident.gossip · self.quarter=wharf · world.mem.mq_wharf=paid]` "The Grayrock steward paid a widow's due. First time in living memory. Somebody walked in with the rope order."
  - `[resident.gossip · world.mem.mq_wharf=named]` "There's a new name at the Chapel of the Tide, twenty winters late. The priest read it between the bells."
  - `[inn.greet · self.id=rm_rosamund_tarridge · world.mem.mq_wharf=named]` "You've heard? Our cook's brother is on the Chapel's roll at last. He's been singing over the pots all week."
- **Engine needs:** J1, C8, C5; M3 (deferred: a House purse, so that Grayrock really pays).

### Q4. `mq-sunken-street`: Down the Sunken Street
- **Tier:** medium. **Program:** city. **Town:** Ridgemere. **Quarters:** commons and sump.
- **Places:**
  - the Hook and Cleaver `rm_the_hook_and_cleaver` (door `w_ridgemere_1_0` (93, 204));
  - the Sunken Street `r2_the_sunken_street` (54 × 7, the old buried street level; doors `w_ridgemere_1_1` (17, 44) and (62, 67)), with region `ss_end` inside it, tiles 1–8 × 1–5 at the far west end;
  - the Morning Oven `rm_the_morning_oven` (door `w_ridgemere_1_0` (93, 236));
  - the Customs House `r2_the_customs_house`;
  - the Cathedral `r2_the_cathedral_of_ridgemere`.
- **Giver:** Theodora Bilgeridge `rm_theodora_bilgeridge`. She is female, 50, and the butcher at the Hook and Cleaver (where she also lives). She speaks plain with no accent, is quiet and nervy, "counts the bells from the Chapel of the Tide", and lost a brother on the wharf.
- **The pups** (real residents, held and led, never harmed):
  - her son Desmond Bilgeridge `rm_desmond_bilgeridge`, 11;
  - his friend Silas Brinegate `rm_silas_brinegate`, 9, son of Tove Brinegate `rm_tove_brinegate`, the baker at the Morning Oven.
- **Premise:** After lessons the two pups went down the Sunken Street steps "to find the old street's ghosts". The Chapel bells have rung twice, and they haven't come up. Down in the dark they found something better than ghosts: a smugglers' cache.
- **Setup:** on `quest accepted`: `npc.walk` both pups to `r2_the_sunken_street` (4, 3), then `npc.hold` both; `item.place brandy 2` and `item.place silver_beads 1` at (6, 2). `join.cap` is 1, because the pups can only be lost once at a time.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `down` | Go down into the Sunken Street. | `enter place` `target: r2_the_sunken_street` | |
| 2 | `find` | Find the two pups in the dark of the old street. | `story signal` `ss_f` | `near resident` `target: rm_desmond_bilgeridge` raises it. Then `npc.follow` both pups to the wolf, and P-C at Desmond. |
| 3 | `choose` | The boy begs you to keep the cache secret. | `story signal` `ss_chose` | |
| 4 | `home` | Lead both pups home to the Hook and Cleaver. | `story signal` `ss_h` | `enter place` `rm_the_hook_and_cleaver` with `resident in place {npc: rm_desmond_bilgeridge}` raises it. Then `npc.unfollow` and `npc.release` both. |
| — | `silas` (optional) | Walk the baker's boy to his own door at the Morning Oven. | `enter place` `rm_the_morning_oven` | `bonus`: `deed.award` "walked a lost pup to his mother's oven" |
| 5 | `cache` | Do as you promised with the cache. | `story signal` `ss_c` | a: `conversation` `rm_jasper_bilgeford`. b: `gift` `brandy` (2) and `silver_beads` to `rm_agnes_wetley`, priest at the Cathedral, for the poor box. |

- **The choice:**
  - **a, tell.** The Customs House hears of the cache. It is honest, and the pups are scolded.
  - **b, keep the secret.** The cache goes to the Cathedral's poor box, and the pups keep their adventure.
- **Repeat and limits** (decisions §8.1): weekly. `join.cap` 1. Deadline 1 game day. If the deadline passes, a `quest failed` trigger walks the pups home (`npc.release`), and the journal note says the Watch found them. With N4, `join.again: 7`. No second copy: there is one pair of lost pups.
  - **It's taken** (H): `[Ridgemere · plain · resident.news · world.mem.mq_sunken_busy=yes]` "Somebody's already gone down after the butcher's pups. The cooper on the Commons wants help with a House's book, if you're looking." It points to Q6.
- **Rewards:**
  - shared: `story.practice watch`; deed "brought two lost pups up out of the Sunken Street";
  - a: 12 from `town:ridgemere`, "the Crown's finder's fee";
  - b: 6 from `town:ridgemere`, and a second deed, "fed the Cathedral's poor box from a smugglers' cache".
- **Campaign:** feeds `nh_realm` +4 and `nh_voice_ridgemere` +4. It stands alone, with a nod to the campaign: the Sunken Street is where Ch 8's search for the hidden writ goes, so pups' talk of "a box down there that nobody's opened" is a harmless red herring for later.
- **Dialogue** (Theodora, plain, anxious):
  - ask: "Is something wrong?"
  - say: "#greet#. My boy and the baker's little one went down the Sunken Street steps after lessons, hunting ghosts. The Chapel bells have rung twice since, and they're not up. I can't leave the block with meat out. Please. Bring them up."
  - accept: "I'll bring them up."
  - decline: "The Watch would be quicker."
  - handIn: "They're home, both of them."
  - thanks: "#thanks#. Oh, #thanks#. Look at the state of them. I counted every bell, you know. Every one."
  - **npc.offer** `ss_tell` at Desmond (the pup). Words: "Your mother needs to know, and the Customs House too." Reply: "Aw. Then they'll take it all and we'll get a hiding. Fine. But I found it first, so put that in."
  - **npc.offer** `ss_keep` at Desmond. Words: "It stays our secret. The poor box can have it." Reply: "Swear? On the Bright One? Then it's ours, and nobody's. That's a proper secret."
  - **Close a** (P-D, in the window): "Smugglers, under our very feet. The Customs House sent a clerk. My boy's to scrub the block for a month."
  - **Close b** (P-D, in the window): "They won't say what they found. Grinning like fools, both of them. Whatever it was, thank you for bringing them back."
  - **Journal notes** (today-only): a "The pups are home, and the Customs House has the cache." b "The pups are home, and the Cathedral's poor box is fuller." On a fail: "The Watch found the pups and walked them home."
- **Gossip and helpful knowledge** (Ridgemere, plain):
  - (H) `[resident.news · world.mem.mq_sunken=open]` "Pups go down the Sunken Street steps for dares. The west end's where they hide; it's darkest there."
  - `[resident.gossip · world.mem.mq_sunken=customs]` "The Customs House pulled a smugglers' cache out of the Sunken Street. Two pups found it hunting ghosts."
  - `[resident.gossip · self.quarter=sump · world.mem.mq_sunken=poorbox]` "The Cathedral's poor box had silver beads in it on Restday. Nobody knows where from, and the priest isn't saying."
- **Engine needs:** C6 (Theodora is silent mid-quest), C8, C5, J1 (an offer at Desmond for one wolf only), N4.

### Q5. `mq-northgate-smoke`: Smoke over Northgate
*Replaces `mq-net-court-cough`, withdrawn: the Sump cough belongs to `lq-runnel-fever` alone (decisions §2.1).*
- **Tier:** medium. **Program:** city. **Town:** Ridgemere. **Quarters:** commons (Northgate, `w_ridgemere_1_0`) and the Sump.
- **Places:**
  - Rainford's Store `rm_rainford_s_store` (door `w_ridgemere_1_0` (107, 204)), with the flat above `rm_rainford_s_store_flat`;
  - the Grey Goose `rm_the_grey_goose` (door `w_ridgemere_1_0` (82, 196)), with its flat above `rm_the_grey_goose_flat`, whose chimney caught;
  - the Rag Market, on `w_ridgemere_0_1`, where the coal crier works by day; he lives at Drip Row House, the ground floor `r2_drip_row_house` (door `w_ridgemere_0_1` (232, 15));
  - region `sn_quay` on `w_ridgemere_0_1`, tiles 172–187 × 120–132: the quay cobbles at the foot of a pier (glyphs `f` and `_`), a few steps from the Customs House door (192, 132). The wet coal comes ashore here.
- **Giver:** Sten Ropeford `rm_sten_ropeford`. He is male, 33, and helps at Rainford's Store, living in the flat above it. He was raised in Ridgemere and speaks **plain** with a light accent. He is meticulous and loyal, even-tempered and warm, swears by the forge and the sea, and is "saving for a stall on the Commons Market, coin by coin".
  - **Why him:** a store hand who sells fuel by the honest sack loses trade to a farthing sack, and a meticulous wolf notices that every chimney that caught this month burned the same coal. Northgate (93 civilians and 35 merchants) had no medium quest (critic P1-16).
  - **Walls kept** (decisions §7.2): no cough, no lamps, no fraud, no Watch lists, nothing of Ambrose's notes. The wrongdoer is a hawker who sold a bad load, not a House.
- **The others** (real residents, conversation and gift targets only):
  - Magda Rustham `rm_magda_rustham`, female, 68, minds the shop door at the Grey Goose and lives in its flat; plain, broad, unhurried and restless;
  - Rupert Greyley `rmx_rupert_greyley`, male, 26, cries wares in the Rag Market; plain, no accent; shrewd and stubborn, coughs in the damp and pretends not to, and "grew up at a Timberland charcoal camp in the hills".
- **Premise:** Somebody is crying coal door to door in Northgate at a farthing a sack. It is wet slack off a lighter's bilge, and it smoulders in the flue until the soot catches. The flat over the Grey Goose went up last night. Sten wants the seller found, and he wants to decide, with you, whether the Watch hears of it.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `goose` | Ask the old wolf over the Grey Goose where her coal came from. | `conversation` `target: rm_magda_rustham` | On completion, `item.give coal 1` ("what was left of her sack"). |
| 2 | `crier` | Find the crier who sells coal at a farthing a sack in the Rag Market. | `conversation` `target: rmx_rupert_greyley` | after 1. His own window line says where the coal comes from (below). |
| 3 | `quay` (hidden; after 2) | Find where the wet coal comes ashore. | `story signal` `sn_q` | `region entered` `sn_quay` raises it, at any hour. On it (guarded by `sn_placed`), `item.place coal 2` at (178, 126) for that wolf. |
| 4 | `heap` | Take a sack off the wet heap, for proof. | `item picked up` `item: coal` | |
| 5 | `choose` | Decide whether the Watch hears of it. | `story signal` `sn_chose` | P-C at Sten |
| 6 | `end` | See it done. | `story signal` `sn_d` | a: `reported` `target: rmx_rupert_greyley` (at the Watch). b: `gift` `coal` (goal 2, quantity) to `rmx_rupert_greyley`, then `gift` `charcoal` to `rm_magda_rustham` (dry fuel, bought anywhere). Each trigger checks `sn_pick`. |

- **The crier's words, in his own window:** `say[self.id=rmx_rupert_greyley player.quest.nh-pack.medium.city/mq-northgate-smoke=active priority=3]` with N2, and `say[self.id=rmx_rupert_greyley world.mem.mq_coal=open priority=3]` until then:
  - "A farthing a sack, and folk still haggle. A lighter brings it off the pier foot, wet from the bilge. I grew up by the clamps. I know wet coal. I sold it anyway."
- **The choice:**
  - **a, the Watch.** The crier is reported and fined (talk), and the lighter at the pier foot loses its buyer.
  - **b, take it back.** The crier takes his wet sacks back and sells dry at an honest price, and the old wolf over the Grey Goose gets a sack of dry fuel. No name goes to the Watch.
- **Repeat and limits:** weekly. `join.cap` 3 (the heap is a placed item per wolf). Deadline 2 game days.
- **Rewards:**
  - shared: `story.practice watch`;
  - a: 10 from `town:ridgemere` (P-F), "the Watch's finder's fee"; deed "named the wet-coal crier who set Northgate's chimneys alight";
  - b: 8 from `town:ridgemere`; deed "made a coal crier take back his wet sacks and sell dry".
- **Campaign:** feeds `nh_realm` +4 and `nh_voice_ridgemere` +4. It stands alone. The crier is one of the wolves who "took this work when the city gave its markets over to food" (his backstory), so the quest sits in the realm's hungry year without touching any chapter's thread.
- **Dialogue** (Sten, plain, light; meticulous):
  - ask: "Smells of smoke round here."
  - say: "#greet#. It does, and it shouldn't. Somebody's crying coal door to door at a farthing a sack. Wet slack. It smoulders in the flue till the soot catches. The flat over the Grey Goose went up last night. Find me the seller? Then we'll decide who hears of it."
  - accept: "I'll find your seller."
  - decline: "Coal's coal."
  - handIn: "The coal crier's seen to."
  - thanks: "#thanks#. I'll write it in the book: who, where, and what it cost. I write everything down. It's the only way a thing stays true."
  - during (C6), duringAsk "About the farthing coal…": "The old wolf over the Grey Goose bought a sack of it. Start with her. Then the Rag Market."
  - **npc.offer** `sn_watch` at Sten. Words: "The Watch should hear of it. A roof burned." Reply: "They should. #oath#, a farthing sack that burns a roof isn't cheap. Report him at the Watch, and I'll write the day and the hour in my book."
  - **npc.offer** `sn_back` at Sten. Words: "Make him take it back and sell dry. No Watch." Reply: "No Watch. Fair. He's a crier with a barrow, not a thief. Give him back his wet sacks, and take the old wolf over the Goose something dry to burn. I'll know if he sells wet again."
  - **Close a** (P-D, in the window): "The Watch fined him, and the lighter at the pier foot has lost its buyer. Northgate's flues draw clean. I've written it down."
  - **Close b** (P-D, in the window): "He cries dry coal now, at a fair price. Lost his farthing, kept his barrow. The old wolf over the Goose has a fire that stays in the grate."
  - **Journal notes** (today-only): a "The Watch has the coal crier's name." b "The coal crier took his wet sacks back."
- **Gossip and helpful knowledge** (Ridgemere, plain):
  - `[resident.gossip · self.quarter=commons · world.mem.mq_coal=open]` "Coal at a farthing a sack, cried up and down the street. Wet as a herring. Two chimneys caught this month."
  - (H) `[resident.news · world.mem.mq_coal=open]` "The farthing coal comes off a lighter at the pier foot, by the Customs House. The crier's in the Rag Market by day."
  - `[resident.gossip · world.mem.mq_coal=reported]` "The Watch fined a coal crier for wet slack. Northgate chimneys are drawing clean again."
  - `[resident.gossip · self.quarter=sump · world.mem.mq_coal=dry]` "The coal crier in the Rag Market sells dry now. Dearer, mind. Somebody made him take his wet sacks back."
- **Engine needs:** J1, N2 (the crier's line on the quest), C6, C8, C5.

### Q6. `mq-coopers-note`: The Cooper's Note
- **Tier:** medium. **Program:** city. **Town:** Ridgemere. **Quarters:** commons and office.
- **Places:**
  - Staves & Hoops `rm_staves_hoops` (door `w_ridgemere_1_1` (50, 46));
  - the Greatly Counting House `rm_the_greatly_counting_house` (door `w_ridgemere_1_0` (190, 199));
  - the Records Office `r2_the_records_office` with its upper rooms `r2_the_records_office_up`;
  - the Knucklebones `r2_the_knucklebones`.
- **Giver:** Edric Millton `rm_edric_millton`. He is male, 25, cooper at Staves & Hoops, where he lives. He speaks plain with a light accent, is calm, warm and frank, honest to a fault, and never goes out without an oilcloth hood. He "owes a House factor money and has been told to find it by the new moon."
- **Premise:** Edric's father borrowed from the Counting House. Edric swears it was paid; his father said so at the end. The factor's book says otherwise, and the note falls due by the new moon. Pay it, or prove it.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `choose` | Decide: raise the coin, or find the proof. | `story signal` `cn_chose` | P-C at Edric, offered on `quest accepted` |
| — | `pot` (optional) | Win a pot at the Knucklebones. | `table game` `cell: r2_the_knucklebones` (the winner is the actor) | `bonus`: deed "won a cooper's debt back at the Knucklebones table" |
| 2a | `pay` (hidden; revealed by a) | Pay twenty pennies on the note at the Counting House. | `gift` `target: rm_rune_gullwell`, goal 20, measure coins | Rune Gullwell, Counting House clerk. Only coin gifts count. |
| 2b | `rolls` (hidden; revealed by b) | Search the Records Office for the paid entry. | `archive work`, `detail: ridgemere`, goal 2 | |
| 3b | `clerk` (hidden; after 2b) | Ask the Records clerk to find the line. | `conversation` `target: rmx_harriet_pennock` | Harriet Pennock, Sump-born, chatty. On completion, `item.give paper 1` (the copy of the paid entry). |
| 4b | `proof` (hidden; after 3b) | Lay the paid entry before the Counting House clerk. | `gift` `target: rm_rune_gullwell` `item: paper` | |
| 5 | `settled` | The note is settled. | `story signal` `cn_s` | `objective completed` of `pay` or of `proof` raises it. |

- **The choice:**
  - **a, pay.** The debt is paid. It is quick, it costs the wolf, and Edric pays back what he can.
  - **b, prove.** It is longer, it shames the Counting House's book, and it is worth more.
- **Repeat and limits:** weekly. `join.cap` 6. Deadline: the next new moon, at most 3 game days. With J4, a `quest failed` on `calendar is moon new`; until then `afterDays: 3`.
- **Rewards:**
  - shared: `story.practice commerce`;
  - a: 6 from `town:ridgemere`, "the Council's debt-relief share", and `item.give barrel 1`, "the cooper pays you back in his own trade"; deed "paid off a cooper's note at the Counting House";
  - b: 10 from `town:ridgemere`, and `item.give barrel 1`; deed "proved a dead wolf's debt was paid, in the Counting House's own book";
  - once SQ2 lands (Phase 1), a new version pays from Staves & Hoops' own till, `till:job:rm_edric_millton` (197), with the counter `coin_till_edric_millton_day` (cap 19). Until then the treasury pays, and the reason still names the cooper.
- **Campaign:**
  - feeds `nh_realm` +4 and `nh_voice_ridgemere` +4;
  - this is a small rehearsal of **Ch 2, the Reckoning**, whose debtors are "freed by labour, goods or proof" (`campaign.md` §0.3). Edric is one of the 41 with the debt hook. Ending b's key `mq_note=proved` gives Ch 2's Counting House lines "they've been caught out in their own book before".
- **Dialogue** (Edric, plain, frank):
  - ask: "You look like a wolf counting days."
  - say: "#greet#. My father borrowed from the Counting House, and paid it, every penny. He told me so at the end, and he never lied to me. Their book says otherwise, and the note's due by the new moon. I can't call a House a liar on my own. Will you help me settle it, one way or the other?"
  - accept: "One way or the other, it'll be settled."
  - decline: "Houses don't lose arguments."
  - handIn: "The note's settled."
  - thanks: "#thanks#. Here, a cask, a good one, with your mark burned in the lid. It's what I can spare, and it'll outlast us both."
  - **npc.offer** `cn_pay` at Edric. Words: "I'll raise the coin and pay it." Reply: "That's a kindness, and it'll end it quick. I'll pay you back what the shop can bear. Twenty on the note, to the clerk with the cold eyes."
  - **npc.offer** `cn_prove` at Edric. Words: "Your father paid. I'll find the proof." Reply: "Then the Records Office. Everything paid in this city was written somewhere twice. If they kept one copy, someone kept the other."
  - **Close a** (P-D, in the window): "Paid and done. I'll sleep. Father would be cross I let it go, but he'd be glad of the roof."
  - **Close b** (P-D, in the window): "The clerk went white as lime. Their own book, a line they'd crossed out. Father was right. He's always been right, the old stave."
  - **Journal notes** (today-only): a "The cooper's note is paid." b "The cooper's note is proved paid, in the Counting House's own book."
- **Gossip and helpful knowledge** (Ridgemere, plain):
  - (H) `[resident.news · self.quarter=office|commons · world.mem.mq_note=open]` "Anything paid in this city is written twice. If the Counting House lost a line, the Records Office kept it."
  - `[resident.gossip · world.mem.mq_note=proved]` "A cooper's note, proved paid out of the Counting House's own crossed-out line. They say the clerk had to sign for it."
  - `[resident.gossip · self.quarter=sump · world.mem.mq_note=paid]` "Somebody paid a cooper's note at the Counting House for him. Twenty pennies. There's still kindness about, then."
- **Engine needs:** C8, C5; J4 (the new moon as the deadline); J1; SQ2 (the cooper's own till).

### Q7. `mq-court-larder`: The Short Larder
*Replaces `mq-false-light`, withdrawn into `lq-wreckers-light` chapter 1 (decisions §2.2).*
- **Tier:** medium. **Program:** city. **Town:** Ridgemere. **Quarters:** estate (Timberland Court, `w_ridgemere_1_2`) and the Sump.
- **Places:**
  - Timberland Court `rm_mossgrave_court` (door `w_ridgemere_1_2` (102, 72)), with its kitchens and servants' hall `rm_mossgrave_court_down` (the stairs inside the Court, (14, 2));
  - region `tc_chute` on `w_ridgemere_1_2`, tiles 114–121 × 78–83: the grass behind the Court's back wall (the house stands at x 102–113, y 64–80), where the old coal chute opens;
  - Drip Row House, the top floor `r2_drip_row_house_top` (street door `w_ridgemere_0_1` (232, 15), then the stairs).
- **Giver:** Vidar Ashhand `rm_vidar_ashhand`. He is male, 42, and serves in the hall at Timberland Court, living in its kitchens and servants' hall. He was born in the Sump, raised in Ridgemere, and speaks **plain** with a **broad** accent. He is anxious and meticulous, nervy, keeps a pocketful of coal for luck, and "means to die somewhere better, the Heights for preference".
  - **Why him:** he counts the hall larder, and a Sump-born servant who climbed out is the right wolf to be torn when the Sump climbs in after him. Timberland Court had no quest in any pack (critic P1-16).
  - **Walls kept** (decisions §7.2): nothing of the Houses' quarrel, the Gudrun and Osric match, or Timberland's iron trade. The family and the steward are named only as "the family" and "the steward".
- **The others** (real residents, conversation and gift targets only; never held, walked or harmed):
  - Hilde Bilgegate `rm_hilde_bilgegate`, female, 31, the other hall servant; plain, light; anxious and guarded. She says it is rats;
  - Arne Rainworth `rmx_arne_rainworth`, male, 8, at lessons at the Abbey School; plain, light; ambitious and restless; "keeps a pocketful of coal for luck";
  - Mab Fogham `rmx_mab_fogham`, female, 54, works at Grayrock Shipwrights; plain, light; restless and suspicious; she "went back to waged work when the old place went to someone better placed". Both live at Drip Row House, the top floor.
- **Premise:** The servants' hall larder at Timberland Court comes up short every morning: a loaf, a heel of cheese, never more. The other hall servant says rats. Vidar counts, and rats don't take exactly one loaf. Sit up with the larder after the house is dark, find the way in, and follow it home. Then decide what the Court is told.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `hilde` | Ask the other hall servant about the rats. | `conversation` `target: rm_hilde_bilgegate` | any order with 2 |
| 2 | `larder` | Sit up in the servants' hall after the house is dark. | `story signal` `tc_l` | `enter place` `rm_mossgrave_court_down` with `hour between` 23–4 raises it (P-A form) |
| 3 | `chute` | Find where the old coal chute comes out behind the Court. | `story signal` `tc_c` | `region entered` `tc_chute`, hours 23–5. On it (guarded by `tc_placed`), `item.place bread 1` at (116, 80): a dropped loaf. |
| 4 | `loaf` | Pick up what was dropped by the chute. | `item picked up` `item: bread` | |
| 5 | `trail` | Follow the coal dust and crumbs home. | `enter place` `target: r2_drip_row_house_top` | after 4 |
| 6 | `pup` (hidden; after 5) | Talk to the pup with coal in his pocket. | `conversation` `target: rmx_arne_rainworth` | He is at the Abbey School by day and at home by night. His own window line (below). |
| 7 | `choose` | Decide what the Court is told. | `story signal` `tc_chose` | P-C at Vidar |
| 8 | `end` | See it done. | `story signal` `tc_d` | a: `gift` `bread` to `rm_vidar_ashhand` (the loaf back). b: `gift` `bread`, goal 2, quantity, to `rmx_mab_fogham` (the first basket). Each trigger checks `tc_pick`. |

- **The pup's words, in his own window:** `say[self.id=rmx_arne_rainworth player.quest.nh-pack.medium.city/mq-court-larder=active priority=3]` with N2, and `say[self.id=rmx_arne_rainworth world.mem.mq_larder=open priority=3]` until then:
  - "It's only one loaf. Gran's on short days at the yard. The chute's just my size. And I leave a coal on the step for it, so it's a trade, isn't it?"
- **The choice:**
  - **a, bar the chute.** Vidar bars it and tells the steward "it was the chute", not whose paws. No Watch; he will not send a pup to the cells.
  - **b, a basket.** Vidar leaves the hall's scraps by the chute every Restday and writes them in the book as rats. The wolf carries the first basket to Drip Row.
- **Repeat and limits:** weekly. `join.cap` 3 (the loaf is a placed item per wolf). Deadline 2 game days.
- **Rewards:**
  - shared: `story.practice service`;
  - a: 10 from `town:ridgemere` (P-F), "a hall servant's thanks"; deed "found the way into the Timberland Court larder and had it barred";
  - b: 6 from `town:ridgemere`; deed "set out a Restday basket for a Sump family by the Timberland Court chute".
- **Campaign:** feeds `nh_realm` +4 and `nh_voice_ridgemere` +4. It stands alone, but it is the campaign's whole question in small, a Sump wolf in the Palace and a Sump pup in a House's larder, and Vidar's `during` line says so.
- **Dialogue** (Vidar, plain, broad; anxious and meticulous):
  - ask: "You look like a wolf who's counted something twice."
  - say: "#greet#. Twice, and short both times. One loaf gone from the hall larder every morning, and a heel of cheese. The other servant says rats. Rats don't take exactly one loaf. Sit up with the larder after the house is dark? Find the way in, before the steward does."
  - accept: "I'll sit up with your larder."
  - decline: "Rats are rats."
  - handIn: "I know where your loaves go."
  - thanks: "#thanks#. I keep coal in my pocket for luck, you know. Born in the Sump. Funny what you can't put down."
  - during (C6), duringAsk "About the larder…": "A Sump wolf holds the Palace lamp, and something's in my larder. Sit up after the last bell. Quiet, mind. The family sleeps light."
  - **npc.offer** `tc_bar` at Vidar. Words: "It's a Sump pup, through the old coal chute. Bar it." Reply: "A pup. #oath#. I'll bar it tonight and tell the steward it was the chute, not who. No Watch. I came out of the Sump through a kitchen door myself. I'll not send a pup back through the cells."
  - **npc.offer** `tc_basket` at Vidar. Words: "It's a hungry Sump family. Leave a basket by the chute on Restday instead." Reply: "A basket, and write it in the book as rats? I could. The hall's scraps go to the pigs anyway. Take them this first one yourself, and tell the old one it's from a wolf who knows the Sump."
  - **Close a** (P-D, in the window): "The chute's barred and the larder counts true. I told the steward it was the chute. I didn't say whose paws. That's my business."
  - **Close b** (P-D, in the window): "The Restday basket's gone by morning, every week. The book says rats. The rats leave a lump of coal on the step. I keep it."
  - **Journal notes** (today-only): a "The old coal chute behind Timberland Court is barred." b "A Restday basket waits by the Timberland Court chute."
- **Gossip and helpful knowledge** (Ridgemere, plain):
  - `[resident.gossip · self.quarter=estate · world.mem.mq_larder=open]` "The Court's hall larder comes up a loaf short every morning. Rats, they say. Tidy rats."
  - (H) `[resident.news · self.quarter=estate|sump · world.mem.mq_larder=open]` "There's an old coal chute behind Timberland Court, round the back wall. Small enough for a pup, they say."
  - `[resident.gossip · self.quarter=estate · world.mem.mq_larder=barred]` "They barred the old coal chute at the Court. The larder counts true again, and nobody's saying why."
  - `[resident.gossip · self.quarter=sump · world.mem.mq_larder=basket]` "A basket by the Timberland Court chute, Restdays. Somebody up there remembers the Sump."
- **Engine needs:** J1, N2 (the pup's line on the quest), C6, C8, C5.

---

## 2. Coldwater and Larkfield (`nh-pack.medium.greenholt`)

### Q8. `mq-glasswater-nets`: Cut Nets at Glasswater
- **Tier:** medium. **Program:** greenholt. **Town:** Coldwater, at the Glasswater landing.
- **Places:**
  - the Glasswater fishing shed `fs_fishing_glasswater_shed` (door `w_ridgemere_7_7` (46, 198)), with region `gn_landing` on `w_ridgemere_7_7`, tiles 34–58 × 192–204, around the shed;
  - region `gn_road` on `w_ridgemere_6_8`, tiles 133–147 × 122–134. This is the Glasswater road (road tiles x 139–140), where camps form most often (7 times);
  - The Glasswater Inn `cn_coldwater_inn` (door `w_ridgemere_7_6` (163, 240)).
- **Giver:** Grist Reedmere `tn_cw_grist_reedmere`. He is male, 40, a boatman at Glasswater who lives at the Glasswater fishing home. He was raised in Ridgemere and speaks **plain** with a light accent. He is suspicious and shrewd, swims in the lake even in autumn, and "carts goods to Ridgemere and resents every mile".
  - **Why him:** a city-raised wolf on the lake, who blames the Larkfield boats before he looks at the road. That gives the quest its turn. He is kin to the Reedmeres of the campaign's ice thread, but he is not in its cast.
- **Premise:** Three nights running the Glasswater nets have been slashed and the catch lifted. Grist blames the Larkfield boats. The tracks go up the bank, not across the water: to the camp on the Glasswater road.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `nets` | Look over the cut nets at the Glasswater landing. | `region entered` `target: gn_landing` | |
| — | `mend` (optional) | Bring a fishing net to replace the worst of them. | `gift` `target: tn_cw_grist_reedmere` `item: fishing_net` | `bonus`: `story.practice craft` |
| 2 | `tracks` | Follow the tracks up to the Glasswater road after dark. | `story signal` `gn_t` | P-A on `gn_road`, hours 20–4. P-B spawns the wave; `item.place smoked_fish 4` at (140, 128). |
| 3 | `fight` | Drive the net-cutters off the road. | `bandit falls` `cell: w_ridgemere_6_8`, goal 2 | |
| 4 | `catch` | Recover the stolen catch. | `item picked up` `item: smoked_fish`, goal 3, measure quantity | |
| 5 | `choose` | Decide where the catch goes. | `story signal` `gn_chose` | P-C at Grist |
| 6 | `deliver` | Hand the catch over. | `story signal` `gn_d` | a: `gift` `smoked_fish` (3) to Grist. b: `gift` `smoked_fish` (3) to `tn_cw_harrow_reedford`, the cook at The Glasswater Inn. |

- **The choice:**
  - **a, back to the families.** The catch goes back to the Glasswater households.
  - **b, sold to the inn.** The catch is sold to the inn's cook. It pays better, and the families who lost it go without.
- **Repeat and limits:** weekly. `join.cap` 3. Deadline 2 game days.
- **Rewards:**
  - shared: `story.practice watch`;
  - a: 8 from `town:coldwater`; deed "won back the Glasswater catch from the road wolves";
  - b: 14 from `town:coldwater`; deed "sold the Glasswater families' own catch to the inn".
- **Campaign:**
  - feeds `nh_realm` +4, `nh_voice_coldwater` +6 and `nh_threat` −2. Ending b does not feed Coldwater's voice;
  - **foreshadows Ch 3**, "open the Glasswater road", and the Glasswater relay post on this same camp site (§5.2 post 9). From `nh_stage` ≥ 3, the close for both endings adds "they had a signal lamp up there, unlit".
- **Dialogue** (Grist, plain, sour):
  - ask: "Bad night on the water?"
  - say: "#greet#. Three nights my nets have been cut, and the catch gone with them. Larkfield boats, I'd say, but I'd like it proved before I say it louder. Look at the landing. Then follow whatever you find."
  - accept: "I'll look, and follow."
  - decline: "Nets aren't my trade."
  - handIn: "The catch is back, one way or another."
  - thanks: "#thanks#. Not Larkfield, then. I'll have to buy a boatman a drink I owe him. That's worse than the nets."
  - **npc.offer** `gn_back` at Grist. Words: "It goes back to the landing families." Reply: "Back to them. Good. Half that fish was the widow at the end shed's. She'll not say thank you; she'll send you a pie. That's how she says it."
  - **npc.offer** `gn_sell` at Grist. Words: "The inn's cook will pay for it." Reply: "Sell it. Course you will. It's fish, it doesn't remember whose it was. The families will, mind."
  - **Close a** (P-D, in the window): "The end shed's widow is baking. Don't make that face; eat the pie."
  - **Close b** (P-D, in the window): "The inn's serving our catch at the Glasswater price. Folk on the landing know whose it was."
  - **Journal notes** (today-only): a "The Glasswater catch went back to the landing families." b "The Glasswater catch was sold to the inn's cook."
- **Gossip and helpful knowledge** (Coldwater, `self.dialect=coldwater`, unless marked):
  - `[resident.gossip · world.mem.mq_nets=open]` "Nets cut three nights at Glasswater, eh? Somebody's fishing with a knife, and not from a boat."
  - (H) `[resident.news · world.mem.mq_nets=open]` "Tracks off the landing go up the bank, not out on the water. Up toward the Glasswater road, where the rough lot camp."
  - `[resident.gossip · world.mem.mq_nets=returned]` "The Glasswater catch came home off the road. Fair catch, that wolf. Net and line, I'd buy them a cup."
  - `[resident.gossip · self.dialect=plain · world.mem.mq_nets=sold]` "The Glasswater Inn's serving fish the landing families lost. Somebody won it back and sold it on, they say."
- **Engine needs:** C8, J1, C5. C10 `foe.add` is optional (a named net-cutter). L2 names the camp.

### Q9. `mq-meresend-blight`: Black Leaf at Meresend
- **Tier:** medium. **Program:** greenholt. **Town:** Coldwater, at the Meresend orchard.
- **Places:**
  - the Meresend orchard store `fs_orchard_coldwater_store` (door `w_ridgemere_8_6` (106, 177)), with region `mb_rows` on `w_ridgemere_8_6`, tiles 90–120 × 183–195 (the rows south of the store). The same cell has the Frosthollow-road camp site;
  - Hollis Physic `rm_hollis_physic` in the city.
- **Giver:** Pell Harvestley `tn_sm_pell_harvestley`. She is female, 35, and tends the Meresend orchard. She lives at the Meresend orchard home. She speaks **coldwater** with a broad accent, is loud and tired, and distrusts anything from the city. "Once walked all the way to Ridgemere and came back."
- **Premise:** Black leaf has come into three rows at Meresend. Burn them now and lose a season's apples for sure. Or fetch a wash from the city apothecary, and maybe save them, or maybe lose the whole orchard.
- **Setup:** the first acceptance of each week runs `event.add` `market: coldwater` `event: orchard_blight` `days: 0` (once per week, by `schedule`). The town then talks of the blight already.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `rows` | Walk the blighted rows at Meresend. | `region entered` `target: mb_rows` | |
| 2 | `choose` | Decide: burn, or fetch a wash from the city. | `story signal` `mb_chose` | P-C at Pell |
| 3a | `fire` (hidden; a) | Bring pitch and charcoal for the fire. | `gift` `target: tn_sm_pell_harvestley` `item: charcoal`, goal 2, quantity; then `item: pitch`, goal 1 | two objectives, `fire1` and `fire2` |
| 3b | `apothecary` (hidden; b) | Ask the apothecary at Hollis Physic for a wash against black leaf. | `conversation` `target: rm_rupert_fogton` | On completion, `item.give potash 2` (the wash). |
| 4b | `wash` (hidden; after 3b) | Bring the wash back to Meresend. | `gift` `target: tn_sm_pell_harvestley` `item: potash`, goal 2, quantity | |
| 5 | `done` | The rows are seen to. | `story signal` `mb_d` | raised by the completion of 3a or 4b |

- **The endings** (P-D, with a seeded `random` for b):
  - **a, burned.** The loss is certain and small. `harvest.override` `area: fruit` `word: poor` `days: 3` `words: "three rows burned at Meresend"`.
  - **b1, saved** (weight 60). Nothing changes at the market. Pell grudgingly admits the city was right.
  - **b2, spread** (weight 40). `harvest.override fruit poor 7` `"the black leaf spread at Meresend"`, and `event.add coldwater orchard_blight` again.
- **Repeat and limits:** weekly. `join.cap` 6. Deadline 2 game days. Pell holds back the burning until then.
- **Rewards:**
  - shared: `story.practice labour`;
  - a: 8 from `town:coldwater`; deed "burned out the black leaf at Meresend";
  - b1: 12; deed "saved the Meresend rows with a city wash";
  - b2: 4; deed "tried a city wash on the Meresend blight".
- **Campaign:** feeds `nh_realm` +4 and `nh_voice_coldwater` +6 (all endings: the wolf served the town either way). It stands alone. The talk key `mq_blight=spread` makes Coldwater's Ch 2–3 lines about the city's help more bitter.
- **Dialogue** (Pell, coldwater, broad):
  - ask: "Are your trees all right?"
  - say: "#greet#. No, they're not. Black leaf in three rows, and it'll be in thirty by the full moon. My gran would burn them tonight. The apothecary down in the city sells a wash he swears by, but I've never trusted a thing in a city bottle, eh? Walk the rows, then tell me which."
  - accept: "I'll walk the rows."
  - decline: "Trees aren't my business."
  - handIn: "The rows are seen to."
  - thanks: "#thanks#. Whatever comes of it, it was done with a will. Net and line, I'd rather lose apples than sit and watch."
  - **npc.offer** `mb_burn` at Pell. Words: "Burn the three rows tonight, before it spreads." Reply: "Gran's way. Bright One's tide, it'll hurt. Bring me charcoal and pitch, and we'll do it at dusk with the wind behind us."
  - **npc.offer** `mb_wash` at Pell. Words: "Let me fetch the city wash. It might save them." Reply: "Might. Might's a thin rope, eh? Go on, then. If it works I'll say so out loud, and you'll hear it from the far end of Glasswater."
  - **Close a** (P-D, in the window): "Three rows of ash, and the rest still green. My gran would've done the same. That's no comfort, but it's true."
  - **Close b1** (P-D, in the window): "It took. The black's drying off the leaf. Don't you tell the city I said so. Fine. Tell them."
  - **Close b2** (P-D, in the window): "It's in ten rows now. City bottle. I knew it, and I let you go anyway. That one's on both of us."
  - **Journal notes** (today-only): a "Three rows burned at Meresend." b1 "The city wash took at Meresend." b2 "The black leaf spread at Meresend."
- **Gossip and helpful knowledge** (Coldwater, coldwater):
  - (H) `[resident.news · world.mem.mq_blight=open]` "Black leaf at Meresend. Old folk say burn it at dusk with the wind behind you. City folk sell a wash. Pick your fool."
  - `[resident.gossip · world.mem.mq_blight=burned]` "Three rows burned at Meresend. A hard choice, and the right one, eh? The rest of the orchard's clean."
  - `[resident.gossip · world.mem.mq_blight=saved]` "A city wash saved the Meresend rows. I'd not have believed it. Bright One's tide, there's a first time."
  - `[resident.gossip · world.mem.mq_blight=spread]` "The black leaf's in ten rows at Meresend now. Somebody trusted a city bottle. Muddied water, that."
- **Engine needs:** C8 (three outcome keys), J1, C5. No beast is spawned. The `random` choice must be rehearsed with `rolls`.

### Q10. `mq-coldwater-still`: A Cask for the Fell Factor
- **Tier:** medium. **Program:** greenholt. **Town:** Coldwater, and the city. **Closure (J3):** none (answer (b)); a journal note invites a social Story.
- **Places:**
  - Coldwater Still `cn_coldwater_still` (door `w_ridgemere_7_6` (119, 253));
  - the Fell Distillery `r2_the_fell_distillery` (door `w_ridgemere_2_1` (92, 58));
  - Fellhall `rm_fellhall` (door `w_ridgemere_2_1` (150, 70));
  - The Sheaf `cn_larkfield_alehouse`;
  - The Glasswater Inn `cn_coldwater_inn`.
- **Giver:** Linden Harvestton `tn_sm_linden_harvestton`. She is female, 57, stillmaster at Coldwater Still, and lives over it. She speaks **coldwater** with a light accent, is quiet, calm, warm and guarded, gentle, never sits with her back to the door, and "wants to plant the biggest orchard in the south".
- **Premise:** House Fell's factor has written: "send a sample cask to the Distillery and we will name a price for your still." Linden knows what a Fell price means: the still stays in Coldwater and the brandy leaves it with a green seal. She wants someone to carry the cask, and to choose: put it on Fell's table, or find out whether Coldwater can sell its own.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `cask` | Collect the sample brandy from the Still. | `enter place` `target: cn_coldwater_still` | On completion, `item.give brandy 2`, then P-C at Linden. |
| 2 | `choose` | Decide where the brandy goes. | `story signal` `cs_chose` | |
| 3a | `distillery` (hidden; a) | Give a cask to the distiller at the Fell Distillery. | `gift` `target: rm_ilse_saltley` `item: brandy` | Ilse Saltley, distiller, Fenshore-raised |
| 4a | `steward` (hidden; after 3a) | Hear House Fell's price from the steward at Fellhall. | `conversation` `target: rm_millicent_rustwell` | |
| 3b | `sheaf` (hidden; b) | Pour a cask for the keeper of The Sheaf in Larkfield. | `gift` `target: tn_cw_bramble_reedford` `item: brandy` | the Larkfield innkeeper, as a buyer only |
| 4b | `glasswater` (hidden; b) | Pour the other for the keeper of The Glasswater Inn. | `gift` `target: tn_fh_loam_pinefell` `item: brandy` | the Coldwater innkeeper, as a buyer only |
| 5 | `word` | Bring Linden the answer. | `story signal` `cs_w` | raised by 4a, or by 3b and 4b both |

- **The choice:**
  - **a, Fell's table.** Fell pays well, and the still is Fell's in all but name.
  - **b, our own buyers.** Two taprooms take the brandy at a fair price, and the still stays Coldwater's.
- **Repeat and limits:** weekly. `join.cap` 6. Deadline 3 game days, for the travel.
- **Rewards:**
  - shared: `story.practice commerce`;
  - a: 15 from `town:coldwater`, "the Fell advance, paid through the store"; deed "carried Coldwater's still to House Fell's table"; `times.nudge` `market: coldwater` +10, 7 days, `words: "Fell money at the Coldwater Still"`;
  - b: 10 from `town:coldwater`; deed "found Coldwater's brandy its own buyers".
- **Campaign:**
  - feeds `nh_realm` +4, and `nh_voice_coldwater` +6 (ending b only);
  - **foreshadows Ch 5–6**: Fell's reach for the Rainward Hills, and the Houses falling out. **Reads** `nh_b_hills`: if it is `fell`, the steward's reply in 4a is "House Fell has the Hills now. A still is a small thing beside them";
  - it ends with a journal note inviting a social Story (no `closure`: answer (b)).
- **Dialogue** (Linden, coldwater, quiet):
  - ask: "You look like a wolf with a letter you don't like."
  - say: "#greet#, #address#. House Fell's factor writes: send a sample cask to the Distillery, and we'll name a price. I know a Fell price. The still stays here and the brandy goes out with a green seal on it. Carry the cask for me? And choose for me, eh? I've been choosing alone too long."
  - accept: "I'll carry it, and choose."
  - decline: "That's a House matter."
  - handIn: "You have your answer."
  - thanks: "#thanks#. Still water, then, whichever way it runs. I'll sit with my back to the wall and think it over."
  - **npc.offer** `cs_fell` at Linden. Words: "Put it on Fell's table. Let them pay for it." Reply: "Their table. It's a long one, eh? Take the cask to the distiller, then hear the steward's price at Fellhall. Don't sign anything with your paw."
  - **npc.offer** `cs_ours` at Linden. Words: "Coldwater can sell its own. Let me find the buyers." Reply: "Our own. That's my gran talking in your mouth. One cask to The Sheaf in Larkfield, one to The Glasswater Inn. If they pour it, we've a trade."
  - **Close a** (P-D, in the window): "Fell's paid, and paid well. A green seal on my brandy. I'll get used to it. Maybe."
  - **Close b** (P-D, in the window): "Two taprooms pouring Coldwater brandy at a fair price. No seal, no factor. Calm water, at last."
  - **Journal notes** (today-only): a "The Coldwater Still's cask is on House Fell's table." b "Two taprooms pour Coldwater brandy at a fair price."
- **Gossip and helpful knowledge:**
  - `[Coldwater · coldwater · resident.gossip · world.mem.mq_still=open]` "House Fell wants the Coldwater Still. Sent a factor's letter, eh? Like a pike writing to a minnow."
  - `[Coldwater · coldwater · resident.gossip · world.mem.mq_still=fell]` "The Still's brandy goes out with a green seal now. Fell money. Fair catch for somebody."
  - `[Larkfield · larkfield · resident.gossip · world.mem.mq_still=ours]` "The Sheaf's pouring Coldwater brandy, full weight, fair price. No Fell seal on it. Scales and silver, good stuff."
  - `[Ridgemere · plain · self.quarter=estate · world.mem.mq_still=ours]` "A lake still turned down a Fell price, they say. The steward at Fellhall had to tell the Lady herself."
- **Engine needs:** C8, C5; J1.

### Q11. `mq-linen-bolt`: A Bolt Gone Missing
*Replaces `mq-short-weighed`, withdrawn into `lq-clipped-silver` as its chapter 1, where Pike Chalkley offers the beam's choice and Fenwick Mossdale is a target (decisions §2.3).*
- **Tier:** medium. **Program:** greenholt. **Town:** Larkfield.
- **Places:**
  - the Larkfield Linen Shed `cn_larkfield_weavers` (Larkfield Looms; door `w_ridgemere_6_9` (165, 76));
  - region `bl_door` on `w_ridgemere_6_9`, tiles 159–164 × 70–82: the road and grass outside the Linen Shed's door, where the carts load (road tiles at x 162–164);
  - the Larkfield Bakehouse `cn_larkfield_bakehouse` (door `w_ridgemere_6_9` (133, 56));
  - the Southwood pig yard `fs_pigs_south_sties` (door `w_ridgemere_5_8` (97, 11)), where the lad works. He lives at Larkfield Cottage 2 `cn_larkfield_cottage_2`.
- **Giver:** Heather Reedwater `tn_mb_heather_reedwater`. She is female, 44, a loom hand at Larkfield Looms, living at Larkfield Cottage 3 `cn_larkfield_cottage_3`. She speaks **larkfield** with a light accent. She is generous and gentle, calm and warm, counts things under her breath, and is "the best eel trapper on the river, and says so".
  - **Why her:** a loom hand who counts every bolt off the loom knows one is gone before the master does, and a gentle one makes the second ending a real choice.
  - **Walls kept** (decisions §7.2): no fraud (that is `lq-clipped-silver`), nothing of the ford siege, nothing of the Print House's Council work. The Wharf-bound bolt is honest trade; only one bolt went astray.
- **The others** (conversation targets only):
  - Eskar Cornley `tn_sm_eskar_cornley`, male, 52, baker at the Larkfield Bakehouse; larkfield, broad; patient and unhurried; "never misses a harvest dance";
  - Ragnar Wetby `cty_ragnar_wetby`, male, 17, swineherd at Southwood; larkfield, broad; blunt and unhurried; he "went to Ridgemere to make a fortune once and came back with nothing".
- **Premise:** Twelve bolts of Wharf-bound linen went out of the Linen Shed door for the carter, and eleven reached the cart. Heather counted twice. The bolt went somewhere between the door and the wagon, and new linen smells of size and flax. Follow the scent, find the bolt, and decide who pays for it.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `door` | Smell round the Linen Shed door where the carts load. | `region entered` `target: bl_door` | |
| 2 | `bake` | Follow the smell of new flax to the Bakehouse. | `enter place` `target: cn_larkfield_bakehouse` | after 1 |
| 3 | `baker` | Ask the baker what he took in trade this week. | `conversation` `target: tn_sm_eskar_cornley` | His own window line (below). |
| 4 | `lad` (hidden; after 3) | Find the Southwood lad who traded a bolt of linen for bread. | `conversation` `target: cty_ragnar_wetby` | |
| 5 | `choose` | Decide who pays for the bolt. | `story signal` `bl_chose` | P-C at Heather |
| 6a | `fetch` (hidden; a) | Fetch the bolt back from the baker. | `conversation` `target: tn_sm_eskar_cornley` | On completion, `item.give linen_cloth 1` ("the bolt, folded in a flour sack"). |
| 7a | `return` (hidden; after 6a) | Bring the bolt home to the Linen Shed. | `gift` `target: tn_mb_heather_reedwater` `item: linen_cloth` | |
| 6b | `bread` (hidden; b) | Take the lad two loaves to start his work on. | `gift` `target: cty_ragnar_wetby` `item: bread`, goal 2, quantity | |
| 8 | `square` | The count is square. | `story signal` `bl_d` | raised by the completion of 7a or 6b |

- **The baker's words, in his own window:** `say[self.id=tn_sm_eskar_cornley player.quest.nh-pack.medium.greenholt/mq-linen-bolt=active priority=3]` with N2, and `say[self.id=tn_sm_eskar_cornley world.mem.mq_bolt=open priority=3]` until then:
  - "Linen? Aye, a bolt. A Southwood lad, all elbows and hungry, asked six loaves for it. I gave him the loaves. Scales and silver, I'd give it back if I was asked."
- **The choice:**
  - **a, the bolt home.** The wolf fetches it back from the baker and brings it to the Looms. The loom master hears who took it, gently (talk).
  - **b, the lad works it off.** The bolt stays with the baker as six loaves paid. The lad minds the Linen Shed's loading door until the bolt is paid for, and the wolf takes him two loaves to start on.
- **Repeat and limits:** weekly. `join.cap` 6 (nothing is placed; the bolt is given by the story). Deadline 2 game days.
- **Rewards:**
  - shared: `story.practice commerce`;
  - a: 12 from `town:larkfield` (P-F), "the Looms' finder's share"; deed "brought a lost bolt of Larkfield linen home to the Looms";
  - b: 8 from `town:larkfield`; deed "let a hungry Southwood lad work off a bolt of linen".
- **Campaign:**
  - feeds `nh_realm` +4 and `nh_voice_larkfield` +6 (both endings: the wolf served the town either way);
  - **reads** `nh_stage`: from stage 7 (the siege on the Last Sheaf), ending b's close is the variant below.
- **Dialogue** (Heather, larkfield, light; generous and gentle):
  - ask: "Counting again?"
  - say: "#greet#. Always. Twelve bolts of linen out of our door for the Wharf, and eleven on the cart. I counted twice, and I'm never wrong twice. Somebody lifted one between the door and the wagon. New linen smells of size and flax. Follow your nose for me, #address#?"
  - accept: "I'll follow the flax."
  - decline: "Linen's not my trade."
  - handIn: "I know where your bolt went."
  - thanks: "#thanks#. Twelve, then, one way or another. I'll stop counting in my sleep. Mostly."
  - during (C6), duringAsk "About the bolt…": "Size and new flax. Start at our door, where the carts load, and follow it."
  - **npc.offer** `bl_back` at Heather. Words: "A Southwood lad traded it to the baker for bread. I'll bring it back." Reply: "For bread? #oath#. Bring it home, then. The loom master will want his name, and I'll give it gently. Hungry isn't wicked, but a bolt's a bolt."
  - **npc.offer** `bl_work` at Heather. Words: "Let the lad work it off at your loading door. I'll take him bread to start on." Reply: "Work it off. Aye, there's a loading door wants minding, early and late. Six loaves for a bolt's a short-weighed trade, and he's learned it. Take him his bread. I'll square the count."
  - **Close a** (P-D, in the window): "Twelve bolts on the Wharf cart, and the count's square. The loom master had a word with the lad. A soft word. I made sure."
  - **Close b** (P-D, in the window): "The lad minds our loading door now, early and late. Not a bolt's gone walking since. Scales and silver, he counts better than I do."
  - **Close b from stage 7:** "The lad minds our door, and he stood at the ford on the Last Sheaf with the rest. Worked his bolt off twice over, I'd say."
  - **Journal notes** (today-only): a "The lost bolt is back at the Linen Shed." b "A Southwood lad minds the Linen Shed's loading door."
- **Gossip and helpful knowledge** (Larkfield, larkfield):
  - `[resident.gossip · world.mem.mq_bolt=open]` "A bolt of Wharf linen gone between the Looms and the cart, eh? Somebody's thumb on the count."
  - (H) `[resident.news · world.mem.mq_bolt=open]` "New linen smells of size. If a bolt's gone walking, follow your nose from the Linen Shed door."
  - `[resident.gossip · world.mem.mq_bolt=back]` "The lost bolt's home at the Looms. Full count on the Wharf cart. By the market cross, that's how it should be."
  - `[resident.gossip · world.mem.mq_bolt=worked]` "That Southwood lad's minding the Linen Shed door. Working off a bolt, they say. Fair price, I call it."
- **Engine needs:** J1, N2 (the baker's line on the quest), C6, C8, C5.

### Q12. `mq-mud-to-the-axles`: Mud to the Axles
- **Tier:** medium. **Program:** greenholt. **Town:** Larkfield.
- **Places:**
  - region `mw_wagon` on `w_ridgemere_6_9`, tiles 216–230 × 234–244. This is the footpath below Longmead Mill (path tiles from (219, 237) down to (230, 243)), by the mill door at (213, 237);
  - region `lf_square` on `w_ridgemere_6_9`, tiles 131–149 × 70–82 (Larkfield's market square);
  - the Larkfield smithy `cn_larkfield_smithy`.
- **Giver:** Imogen Rustford `cty_imogen_rustford`. She is female, 35, a carter at Larkfield, living at `cn_larkfield_cottage_5`. She speaks **larkfield** with a light accent, is loud, dreamy and chatty, "quotes old sayings for everything", and "is feuding with a neighbour over a well that is not really anyone's".
- **Premise:** Imogen's wagon is in the mud to the axles on the track below Longmead Mill. It is loaded with House Fell's casks and Larkfield's flour. Dusk is coming, and the Glasswater-road wolves know a stuck wagon when they smell one.
- **Setup:** on `quest accepted`, `npc.move` Imogen to `w_ridgemere_6_9` (222, 239), then `npc.hold`. `join.cap` 1.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `axle` | Bring the carter a new axle. | `gift` `target: cty_imogen_rustford` `item: axle` | The Larkfield smithy (`tn_mb_tansy_eelbridge`) sells them. Any order with 2. |
| 2 | `planks` | Bring two planks to lay under the wheels. | `gift` `target: cty_imogen_rustford` `item: planks`, goal 2, quantity | |
| 3 | `hold` | Hold the wagon while she frees it. They're coming up the track. | `bandit falls` `cell: w_ridgemere_6_9`, goal 2 | P-B: when 1 and 2 are done, spawn in `mw_wagon`. |
| 4 | `choose` | The wagon's still too heavy. Decide what goes in the mud. | `story signal` `mw_chose` | P-C at Imogen |
| 5 | `escort` | Walk the wagon into Larkfield. | `region entered` `target: lf_square` | On choice: `npc.release` and `npc.walk` Imogen to (140, 76), cue `mw_home`. |

- **The choice:**
  - **a, cut the Fell casks loose.** The flour reaches the bakehouse, and House Fell grumbles.
  - **b, leave the flour.** House Fell's casks are saved, and Larkfield goes short.
- **Repeat and limits** (decisions §8.1): weekly. `join.cap` 1. Deadline 1 game day. A `quest failed` trigger releases Imogen. With N4, `join.again: 7`. No second copy: one carter, one stuck wagon.
  - **It's taken** (H): `[Larkfield · larkfield · resident.news · world.mem.mq_axles_busy=yes]` "The carter's got help in the mud already. A loom hand at the Linen Shed is a bolt short, if you've a nose, eh?" It points to Q11.
- **Rewards:**
  - shared: `story.practice labour`;
  - a: 8 from `town:larkfield`; deed "freed a Larkfield wagon at the cost of House Fell's casks";
  - b: 12 from `town:larkfield`, "the carriage paid in full"; deed "saved House Fell's casks from the Longmead mud"; `times.nudge` `market: larkfield` −10, 5 days, "flour short in Larkfield".
- **Campaign:**
  - feeds `nh_realm` +4, `nh_voice_larkfield` +6 and `nh_threat` −2;
  - **foreshadows Ch 7**, Harvest Under Arms: the siege on the Larkfield ford, and feeding Larkfield. From `nh_stage` ≥ 7, ending a's close adds "flour's worth more than wine this autumn".
- **Dialogue** (Imogen, larkfield, sayings):
  - ask: "Stuck?"
  - say: "#greet#! Stuck, and deep. 'A wagon in the mud is a gift to every thief.' My gran's saying, and she's right. Fell's casks and our flour, and the axle cracked. I need an axle and two planks, and someone with teeth when the road lot come up the track at dusk."
  - accept: "An axle, two planks, and teeth."
  - decline: "Sorry, not today."
  - handIn: "The wagon's in Larkfield."
  - thanks: "#thanks#! 'Shared mud, shared bread.' My gran again, and right again."
  - **npc.offer** `mw_casks` at Imogen. Words: "Cut the Fell casks loose. The flour's for Larkfield." Reply: "#oath#! Fell will howl. Let them; casks float, flour doesn't. Over they go."
  - **npc.offer** `mw_flour` at Imogen. Words: "Leave the flour. Fell's casks pay the carriage." Reply: "Fell's casks it is. 'Pay the piper, starve the cook.' Larkfield will feel it in the bakehouse, mind."
  - **Close a** (P-D, in the window): "Flour's at the bakehouse, the casks are in the Longmead ditch, and a Fell factor's on his way to shout at me. Worth it."
  - **Close b** (P-D, in the window): "Fell paid the carriage, full weight. The bakehouse is short. My neighbour says I chose wine over bread. She would."
  - **Journal notes** (today-only): a "The flour reached Larkfield. House Fell's casks are in the ditch." b "House Fell's casks reached Larkfield. The flour is in the mud."
- **Gossip and helpful knowledge** (Larkfield, larkfield):
  - (H) `[resident.news · world.mem.mq_axles=open]` "A carter's in the mud below Longmead Mill. Needs an axle, two planks, and someone to stand by her at dusk."
  - `[resident.gossip · world.mem.mq_axles=casks_lost]` "Fell casks in the Longmead ditch and our flour in the bakehouse. Fair price, I call it. By the market cross, fair."
  - `[resident.gossip · world.mem.mq_axles=flour_lost]` "The flour went in the mud to save Fell's casks. Short loaves this week, short-weighed in every way."
- **Engine needs:** C8, J1, C5, N4; C10 `foe.add` (optional). L2 names the camp.

---

## 3. Southmeadow, Fenshore, Saltwell and the Mirebridge crossroads (`nh-pack.medium.south`)

### Q13. `mq-bee-garth-bear`: The Bear in the Bee Garth
- **Tier:** medium. **Program:** south. **Town:** Southmeadow, at the Sunfields Bee Garth.
- **Places:**
  - the Sunfields hives `fs_bees_south_hives` (door `w_ridgemere_7_12` (187, 31)), with region `bg_garth` on `w_ridgemere_7_12`, tiles 175–199 × 24–37 (the garth yard and its grass);
  - region `bg_wood` on `w_ridgemere_7_12`, tiles 150–175 × 40–60, where the bear lies up;
  - region `bg_far` on `w_ridgemere_6_13`, tiles 120–140 × 120–135. This is the woods and mud beyond the Sallow clay pits, far from the hives.
- **Giver:** Disa Fogford `cty_disa_fogford`. She is female, 45, the hive hand at the Sunfields Bee Garth, and lives at `cn_southmeadow_cottage_2`. She speaks **southmeadow** with a light accent, is quiet and dry, and "hopes a child of the family will be taken into service with one of the great Houses". The beekeeper, Aster Cornley, has no accent and speaks plain, so the hive hand carries the town's voice.
- **Premise:** A bear has torn open four hives in two nights. It will be back for the rest. Kill it, or lead it off with honey to the far woods past the clay pits, where there is nothing worth tearing open.
- **Setup:** with **L1** (Phase 5), on `quest accepted`, `hunt.stage {region: bg_wood, species: bear, count: 1, minutes: 240}`: the next hunt in the wood has the bear in it. Until L1, the kill path is offered only if `hunted` `bear` happens anywhere in `w_ridgemere_7_12`, and the honey path is always offered (see Engine needs). M1 `wild.spawn` is dropped (decisions §5.1).
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `hives` | Look at the torn hives in the garth. | `region entered` `target: bg_garth` | |
| 2 | `choose` | Decide: kill the bear, or lead it off. | `story signal` `bg_chose` | P-C at Disa |
| 3a | `kill` (hidden; a) | Hunt down the bear. | `hunted` `target: bear` `cell: w_ridgemere_7_12` | Bears are fierce: go as a party. |
| 3b | `comb` (hidden; b) | Carry two combs of honey to the far woods past the clay pits and leave them. | `item dropped` `item: honey` `cell: w_ridgemere_6_13`, goal 2, quantity | `item.give honey 2` on choice b |
| 4b | `quiet` (hidden; after 3b) | Watch the garth through one night to be sure. | `story signal` `bg_q` | P-A on `bg_garth`, hours 22–3, then a `wait` of 0.1 game days, rechecking `wolf in region`. |
| 5 | `done` | The hives are safe. | `story signal` `bg_d` | raised by 3a, or by 4b |

- **Repeat and limits:** weekly. `join.cap` 3. Deadline 2 game days.
- **Rewards:**
  - shared: `story.practice travel`;
  - a: 10 from `town:southmeadow`; deed "killed the bear that raided the Sunfields hives"; `item.give honey 1`;
  - b: 8 from `town:southmeadow`; deed "led a bear off from the Sunfields hives without a drop of blood".
- **Campaign:**
  - feeds `nh_realm` +4 and `nh_voice_southmeadow` +8;
  - **links to Ch 4**: "The Chorus That Sang Wrong" (a small pack seed) needs mead and honey to soothe the offended families. A wolf who has saved the hives is thanked with a jar for it (talk key `mq_bear=*`).
- **Dialogue** (Disa, southmeadow, dry):
  - ask: "Trouble with the bees?"
  - say: "#greet#. Not with the bees. With what wants their honey. A bear's torn four hives open in two nights, and it'll be back for the rest, full sure. Look at the garth. Then tell me: do we kill it, or lead it off where there's nothing worth tearing?"
  - accept: "I'll look at the garth."
  - decline: "Bears are beyond me."
  - handIn: "The bear won't trouble the hives."
  - thanks: "#thanks#. The bees will settle in a day or two. They hold a grudge longer than the beekeeper does, eh."
  - **npc.offer** `bg_kill` at Disa. Words: "Kill it. It'll only come back." Reply: "#oath#. It's in the wood west of the garth, by the smell. Take friends. It won't lie down for one."
  - **npc.offer** `bg_lead` at Disa. Words: "Lead it off with honey. Somewhere far." Reply: "Ripe thinking, that. Two combs, past the clay pits where the mud starts. Leave them in the trees, then sit a night in the garth and see if it learned."
  - **Close a** (P-D, in the window): "Big as a cart, it was. The beekeeper wants the hide for the garth door. I'd rather the hives back."
  - **Close b** (P-D, in the window): "Quiet night, not a hive touched. Somewhere past the pits a bear's eating our honey and thinking himself clever. Let him."
  - **Journal notes** (today-only): a "The bee garth bear is dead." b "The bee garth bear was led off with honey."
- **Gossip and helpful knowledge** (Southmeadow, southmeadow):
  - (H) `[resident.news · world.mem.mq_bear=open]` "A bear in the bee garth. It lies up in the wood west of the hives. Go in daylight, and go in numbers."
  - `[resident.gossip · world.mem.mq_bear=killed]` "They killed the bee garth bear. Sheaf and sickle, it was a size. The hives are humming again."
  - `[resident.gossip · world.mem.mq_bear=led_off]` "Somebody led the bee garth bear off with honey, past the clay pits. Not a drop of blood. Ripe days, that."
- **Engine needs:** **L1 `hunt.stage`** (Phase 5; the bear staged into the next hunt in `bg_wood`). C8, J1, C5.

### Q14. `mq-vineyard-cellar`: Casks Gone Walking
- **Tier:** medium. **Program:** south. **Town:** Southmeadow, at the vineyard.
- **Places:**
  - the Southmeadow Vineyard press `fs_vineyard_south_press` (door `w_ridgemere_9_12` (98, 105)), with its cellar `fs_vineyard_south_press_cellar` (down the press stairs);
  - region `vc_camp` on `w_ridgemere_7_12`, tiles 112–130 × 102–114. This is the cart road (footpath x 110–124, y 107–108), a camp site the campaign gives to Southmeadow.
- **Giver:** Harl Oakbough `tn_oh_harl_oakbough`. He is male, 32, a vine hand at the Southmeadow Press House, living at Southmeadow Cottage 1 `cn_southmeadow_cottage_1`. He speaks **southmeadow** with a light accent. He is patient and calm, honest to a fault and frank, collects smooth stones from the riverbanks, and "once got lost in the deep wood for three days".
  - **Why him** (decisions §1.3): the vine-keeper Sloe Harvestby keeps her large line (`lq-blight-and-boar`), where Harl is only a target. Harl works the same press and cellar, and a hand who will not say what he cannot prove gives the choice its weight.
- **Suspect:** Briar Cornby `cty_briar_cornby`, the cellar hand: dry, sly, and a believer in the standing stones. She is campaign cast (the Ch 4 giver of "Stones Remember"); here she is only someone to talk to, and that agrees with the campaign's facts (decisions §1.2.2): a sly cellar hand who sold spoiled casks, nothing worse, and ending a stays gentle in talk.
- **Premise:** Casks walk out of the press cellar at night. The press house mutters that it's the sly cellar hand. Harl won't say a thing he can't prove, so he wants it proved. The truth is half of the muttering: Briar has been selling the spoiled casks to the road wolves for a few pennies, and they came back for the good ones.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `cellar` | Keep watch in the vineyard cellar after dark. | `story signal` `vc_c` | `enter place` `fs_vineyard_south_press_cellar` with `hour between 20–4` raises it. |
| 2 | `briar` | Ask the cellar hand what she knows. | `conversation` `target: cty_briar_cornby` | after 1 |
| 3 | `track` | Follow the cask-cart ruts to the road camp. | `story signal` `vc_t` | `region entered` `vc_camp`. P-B spawns the wave (switch `spawned_vc_camp`); `item.place wine 3` at (118, 107). |
| 4 | `fight` | Drive off the cask thieves. | `bandit falls` `cell: w_ridgemere_7_12`, goal 2 | |
| 5 | `casks` | Bring back the casks. | `item picked up` `item: wine`, goal 2, quantity | |
| 6 | `choose` | Decide what to tell the vine hand about the cellar hand. | `story signal` `vc_chose` | P-C at Harl |

- **The choice:**
  - **a, the whole truth.** Briar opened the door to them, if only for spoiled casks.
  - **b, the road wolves.** It was the road wolves, start to finish. Briar is spared, and owes the wolf.
- **Repeat and limits:** weekly. `join.cap` 3. Deadline 2 game days.
- **Rewards:**
  - shared: 10 from `town:southmeadow` (P-F); `story.practice watch`;
  - a: deed "found out who let the road wolves into the Southmeadow cellar";
  - b: deed "won back the Southmeadow casks from the road wolves". Briar's window line (`say[self.id=cty_briar_cornby world.mem.mq_casks=spared priority=3]`, `resident.hub`) is "You. I owe you one, I do. The stones will remember it too, eh." With C8 pair scope, ending b also does `talk.remember {scope: "pair", npc: "cty_briar_cornby", player: "$actor", key: "mq_casks", value: "spared", days: 28}`, and her line is gated `pair.mem.mq_casks=spared`, so only that wolf hears it.
- **Campaign:**
  - feeds `nh_realm` +4, `nh_voice_southmeadow` +8 and `nh_threat` −2;
  - ending a is gentle in talk, so that Briar can still give the campaign's Ch 4 "Stones Remember" quest. Her Ch 4 line gains a variant: "You're the one who told on me. Well. The stones don't hold grudges, so I won't."
- **Dialogue** (Harl, southmeadow, light; patient, honest to a fault):
  - ask: "Something missing?"
  - say: "#greet#. Casks. Three this week, walked out of the cellar by night. The press house says it's the cellar hand, the sly one. Maybe. I'll not say a thing I can't prove, full sure. Watch the cellar after dark. Find where they go, and bring them back. Then we'll know."
  - accept: "I'll watch the cellar."
  - decline: "Not my vines."
  - handIn: "The casks are back."
  - thanks: "#thanks#. Two good casks home. I'll set a smooth stone on each lid, so I know them. I've a pocketful. The river keeps giving."
  - **npc.offer** `vc_truth` at Harl. Words: "The cellar hand sold them the spoiled casks. They came back for the good ones." Reply: "Spoiled casks, out the back door. #oath#. Half a theft, and half a kindness to the road. I'll tell the vine-keeper the whole of it, and I'll tell her it's no worse than it is."
  - **npc.offer** `vc_spare` at Harl. Words: "It was the road wolves, start to finish." Reply: "The road lot, start to finish. I'll take your word, since I've no better. Ripe enough. Only, I'll be sitting up in that cellar myself a night or two."
  - **Close a** (P-D, in the window): "She's scrubbing every cask in the cellar for a month, and she's not said a word to me. Fair. I'd not say one to me either."
  - **Close b** (P-D, in the window): "I gave the cellar hand a smooth stone, for the press house being wrong about her. She looked at me very oddly, eh."
  - **Journal notes** (today-only): a "The vine hand knows who opened the cellar door." b "The Southmeadow casks are home from the road camp."
- **Gossip and helpful knowledge** (Southmeadow, southmeadow):
  - (H) `[resident.news · world.mem.mq_casks=open]` "Casks out of the vineyard cellar at night. The cart ruts go west toward the road, where that rough camp sits."
  - `[resident.gossip · world.mem.mq_casks=told]` "The cellar hand sold spoiled casks to the road lot, they say. Wormy business. She's scrubbing barrels till harvest."
  - `[resident.gossip · world.mem.mq_casks=spared]` "The vineyard casks came home from the road camp. Ripe work. The vine hand's handing out pebbles by way of thanks."
- **Engine needs:** C8 (pair scope for the closes and Briar's line), J1, C5.

### Q15. `mq-withy-lights`: Lights over Withy Creek
- **Tier:** medium. **Program:** south. **Town:** Fenshore, at Withy Creek.
- **Places:**
  - Fenside Salterns `fs_salt_fencoast_boiling` (door `w_ridgemere_3_12` (131, 224)), where the giver works;
  - the Withy Creek eel house `fs_marsh_eels_eelhouse` (door `w_ridgemere_4_13` (35, 107));
  - region `wl_reeds` on `w_ridgemere_4_13`, tiles 43–60 × 100–116 (the reeds `E` east of the eel house; standable).
- **Giver:** Iris Pikeford `tn_mb_iris_pikeford`. She is female, 42, a salt raker at Fenside, living at `cn_fenshore_cottage_2`. Raised in Fenshore, she speaks **mirebridge** with a broad accent. She is quiet, dry and warm, and "swears the marsh lights are the drowned".
- **Premise:** The lights are walking over Withy Creek again in the fog. Iris wants someone to go out and say the old words to them, so the drowned lie quiet. The fowler on the creek swears one of them smelled of eel oil.
- **Setup:** on `quest accepted`, `weather.set` `place: w_ridgemere_4_13` `fog` 8 hours.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `fowler` | Ask the Withy Creek fowler what he saw. | `conversation` `target: tn_mb_nab_bogditch` | Nab Bogditch, fowler at Withy Creek, Ridgemere-raised |
| 2 | `lights` | Go out into the Withy Creek reeds after dark and find a light. | `story signal` `wl_l` | P-A on `wl_reeds`, hours 20–4. On it, `item.place oil_lamp 1` at (46, 108), and `story.ping` there (30 minutes, label "a light in the reeds"). |
| 3 | `lamp` | Take what's floating there. | `item picked up` `item: oil_lamp` | |
| 4 | `choose` | Decide what Iris should hear. | `story signal` `wl_chose` | P-C at Iris |

- **The choice:**
  - **a, the truth.** They are lamps: somebody's float lamps, set in the reeds. The fen folk are uneasy: then who is carrying them?
  - **b, the legend.** "I said the words. The drowned are quiet." Iris lights a candle for them, and the lamp stays in the wolf's pack as a secret.
- **Repeat and limits:** weekly. `join.cap` 3. Deadline 2 game days.
- **Rewards:**
  - shared: 8 from `town:fenshore`; `story.practice travel`;
  - a: deed "brought a fen float lamp out of Withy Creek";
  - b: deed "said the old words to the lights on Withy Creek"; `item.give candle_tallow 1`.
- **Campaign:**
  - feeds `nh_realm` +4 and `nh_voice_fenshore` +8;
  - **foreshadows Ch 6**, the Lights in the Fen: the Captain's southern company moves by float lamps, and the chapter lays or turns them (`nh_b_lights`);
  - **reads** `nh_stage` and `nh_b_lights`: from stage 6 with no result yet, ending a's close adds "Same as the ones the city wolves talk of. Somebody's whole company, no?" Once `nh_b_lights = laid`, the quest is not offered (`phases` gate, by an hourly poll); its board notice says the creek is dark now.
- **Dialogue** (Iris, mirebridge, broad):
  - ask: "You look like you've seen something in the fog."
  - say: "#greet#. Seen it every night this week. Lights, walking over Withy Creek, low on the reeds. The drowned, no? My gran said you go out and say the old words, and they lie quiet. The fowler on the creek swears one stank of eel oil. Ask him, then go and see."
  - accept: "I'll ask the fowler, then go out."
  - decline: "I'll not go near the drowned."
  - handIn: "I've been out to the lights."
  - thanks: "#thanks#. Mud and eels, you went out in that. My gran'd have given you her good boots. I've no boots, but here."
  - **npc.offer** `wl_lamps` at Iris. Words: "They're lamps. Somebody set float lamps in the reeds." Reply: "Lamps? #oath#. Then who's carrying them, and why on our creek? That's worse, no? The drowned at least stay put."
  - **npc.offer** `wl_words` at Iris. Words: "I said the words. They're quiet now." Reply: "Fen-sure? Then I'll light a candle for them tonight, and sleep. You're a kind liar or a brave one. I'll not ask which."
  - **Close a** (P-D, in the window): "Lamps on our creek. Nobody at the Heron's laughing about the drowned now. They're counting boats."
  - **Close b** (P-D, in the window): "The creek's dark tonight. I lit my candle. Dry feet to you, and to them."
  - **Journal notes** (today-only): a "The lights on Withy Creek were float lamps." b "The old words were said over Withy Creek."
- **Gossip and helpful knowledge** (Fenshore, mirebridge):
  - `[resident.gossip · here.weather=fog · world.mem.mq_lights=open]` "Lights on Withy Creek in this mizzle. The drowned walking, no? Mud and eels, I'll not go out."
  - (H) `[resident.news · world.mem.mq_lights=open]` "The fowler on Withy Creek sees the lights closest. Ask him first. He says one stank of eel oil."
  - `[resident.gossip · world.mem.mq_lights=lamps]` "Float lamps on Withy Creek. Somebody's lamps, not the drowned. That's a right boggy thought, that is."
  - `[resident.gossip · world.mem.mq_lights=quieted]` "Somebody went out and said the old words over Withy Creek. Dark as a sack since. Dry feet to them."
- **Engine needs:** C8, J1, C5; J9 (a tagged float lamp, which Ch 6 could recognise).

### Q16. `mq-potters-place`: The Potter's Place
- **Tier:** medium. **Program:** south. **Town:** Fenshore. **Closure (J3):** none (answer (b)); a journal note invites a social Story.
- **Places:**
  - Fenshore Kilns `cn_fenshore_pottery` (door `w_ridgemere_3_13` (50, 145));
  - The Heron `cn_fenshore_alehouse` (door `w_ridgemere_3_13` (82, 148)).
- **Giver:** Bram Harvestgarth `tn_sm_bram_harvestgarth`. He is male, 37, kiln hand at Fenshore Kilns, living at `cn_fenshore_cottage_3`. Raised in Fenshore, he speaks **mirebridge** with a broad accent. He is wry and suspicious, dry, cold and guarded, never misses a harvest dance, and "wants to plant the biggest orchard in the south".
- **The other side:** Ludo Beechshaw `tn_oh_ludo_beechshaw`, 46, Oakhaven-raised (dialect `oakhaven`, light accent), chatty and sly. He holds the dead potter's post (`job:tn_cw_iris_greywater`, holder `tn_oh_ludo_beechshaw`; the potter Iris Greywater died on day 27). This is real data.
- **Premise:** When the Fenshore potter died this spring, her wheel went to an Oakhaven wolf nobody here had heard of. Bram fired her kiln nine years and was never asked. He wants to know how it happened, and whether it was fair. It was the same old rule that put a Sump patrolman in the Palace: when a post falls empty, the work goes to whoever is free to take it.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `heron` | Ask the pot hand at The Heron how the new potter came by the wheel. | `conversation` `target: tn_oh_ludo_ashwood` | Ludo Ashwood, 55, pot hand at The Heron (`cn_fenshore_alehouse`); Fenshore-raised, mirebridge speech, light accent; loud and honest to a fault. The innkeeper Nell Plummeadow is not a target (decisions §1.2.4); her helpful line below stays. Any order with 2. |
| 2 | `ludo` | Talk with the new potter. | `conversation` `target: tn_oh_ludo_beechshaw` | |
| 3 | `clay` | Bring four lumps of clay for the next firing. | `gift` `target: tn_sm_bram_harvestgarth` `item: clay`, goal 4, quantity | The clay pits are at `w_ridgemere_4_12`. |
| 4 | `yard` | Stand in the kiln yard while the two of them have it out. | `story signal` `pp_y` | `enter place` `cn_fenshore_pottery` after 1–3 plays the scene `pp_kiln` (individual). |
| 5 | `choose` | Say your piece. | `story signal` `pp_chose` | `scene choice` with `detail` `pp_kiln.share` or `pp_kiln.rule` |

- **Scene `pp_kiln`** (`ratw-scene` v1; `visibility: individual`; cast `bram` = Bram, `ludo` = Ludo). Lines are raw text, in each speaker's dialect:
  1. `bram` line: "Nine years I fed that kiln. Nine. And the wheel goes to a wolf off a timber road who'd never seen a fen. Dark One's mire, tell me that's fair."
  2. `ludo` line: "I didn't ask for it, old root. The post sat empty and the rule said whoever's free. I was free. I'm still learning which clay sulks, mm?"
  3. choice (`mode: first`, 30 seconds, default `rule`). Text: "They both look at you." Options:
     - `share`, label "Bram fired this kiln nine years. Share the wheel.", next `s1`;
     - `rule`, label "The rule gave it fairly. Bram, teach him the fen clay.", next `r1`.
  4. `s1`, `ludo` line: "Two pairs of paws on one wheel. Proper. I'll take the mornings; you take the firing."
  5. `r1`, `bram` line: "Teach him. Fen-sure, I'll teach him. He'll learn the clay or the clay'll learn him, no?"
- **Repeat and limits:** weekly. `join.cap` 6. Deadline 2 game days.
- **Rewards:**
  - shared: 8 from `town:fenshore`; `story.practice craft`;
  - share: deed "talked the Fenshore kiln into two pairs of paws";
  - rule: deed "stood up for the new Fenshore potter".
- **Campaign:**
  - feeds `nh_realm` +4 and `nh_voice_fenshore` +8;
  - **echoes Ch 1's theme** (a post that fell by the vacancy rule: should the one who took it keep it?) and **Ch 9's** choice of who reigns. From `nh_stage` ≥ 1, Nell's helpful line points it out;
  - it ends with a journal note inviting a social Story (no `closure`: answer (b)).
- **Dialogue** (Bram, mirebridge, broad):
  - ask: "You look like you've a stone in your boot."
  - say: "#greet#. A stone, no. A wolf. The potter went up in the spring, and her wheel went to an Oakhaven wolf nobody'd heard of. I fed that kiln nine years. Find out how he came by it, would you, and bring me clay while you're at it. Four lumps from the pits."
  - accept: "I'll find out how, and bring clay."
  - decline: "Posts are the Council's business."
  - handIn: "You've had it out, the two of you."
  - thanks: "#thanks#. Boggy business. I'll fire the next batch and see if my temper comes out of the kiln harder or softer."
  - (No `npc.offer` here: the choice is in the scene.)
  - **Close share** (P-D, in the window): "He takes the mornings, I take the firing. His pots lean. Mine don't. We'll see who teaches who."
  - **Close rule** (P-D, in the window): "He's learning. Slowly. Put his paw in the slip like a tadpole in mud. I may even like him by the harvest dance."
  - **Journal notes** (today-only): share "The Fenshore wheel is shared." rule "The new potter keeps the wheel, and the kiln hand teaches him."
- **Gossip and helpful knowledge:**
  - (H) `[Fenshore · mirebridge · inn.greet · self.id=cty_nell_plummeadow · world.mem.mq_kiln=open]` "The wheel? The post sat empty and the rule gave it to whoever was free. Same rule as put a patrolman in the Palace, no?"
  - `[Fenshore · mirebridge · resident.gossip · world.mem.mq_kiln=shared]` "Two potters on the one wheel at the kilns now. One leans his pots, one doesn't. Fen-sure we'll hear about it."
  - `[Fenshore · mirebridge · resident.gossip · world.mem.mq_kiln=ludo]` "The kiln hand's teaching the new potter fen clay. Grumbling all the while, no? But teaching."
  - `[Oakhaven · oakhaven · resident.gossip · world.mem.mq_kiln=ludo]` "One of ours keeps the kilns down at Fenshore now. Proper, that. Learning the bog clay, mm?"
- **Engine needs:** C8; C5 (Bram's thanks by name); a scene, which works today.

### Q17. `mq-lost-raker`: Lost in the Salt Wind
- **Tier:** medium. **Program:** south. **Town:** Saltwell, out at the Saltpan.
- **Places:**
  - the Saltpan workings `fs_salt_saltpan_store` (door `w_ridgemere_13_12` (96, 243)), with region `lr_store` on `w_ridgemere_13_12`, tiles 84–108 × 236–249 (the store and home doors);
  - the far pans at `w_ridgemere_13_12` (60, 200), on salt flat `)`.
- **Giver:** Fern Merewick `cty_fern_merewick`. She is female, 38, a salt raker at the Saltpan, living at `cn_saltwell_cottage_1`. She speaks **saltwell** with a light accent, is content and short-tempered.
- **The lost one:** Ridge Eelbank `tn_mb_ridge_eelbank`, 32, a salt raker there too. He is Mirebridge-born, Saltwell-raised (saltwell, light accent), cold and guarded, and "swears the marsh lights are the drowned".
- **Premise:** A sandstorm came off the Red Waste at noon. Fern's workmate went out to the far pans to fetch the rakes and has not come back. Find him before dark, give him water, and bring him in. Then hear what he was really doing out there.
- **Setup:** on `quest accepted`:
  - `weather.set` `place: w_ridgemere_13_12` `sandstorm` 4 hours;
  - `npc.move` Ridge to `w_ridgemere_13_12` (60, 200), then `npc.hold`;
  - `item.give waterskin 1` to the wolf.
  - `join.cap` 1.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `find` | Find the lost raker out on the far pans. | `story signal` `lr_f` | `near resident` `target: tn_mb_ridge_eelbank` raises it. Then `npc.follow` Ridge to the wolf, and the P-C offer at Ridge. |
| 2 | `water` | Give him water. | `gift` `target: tn_mb_ridge_eelbank` `item: waterskin` | Plain `water` also counts (a second objective, either one). |
| 3 | `back` | Bring him in to the Saltpan store. | `story signal` `lr_b` | `region entered` `lr_store` with `resident in region {npc: tn_mb_ridge_eelbank, region: lr_store}`. Then `npc.unfollow` and `npc.release`. |
| 4 | `choose` | He tells you what he was really doing. Decide who hears it. | `story signal` `lr_chose` | |

- **What he admits** (in the `lr_why` reply): he wasn't fetching rakes. He was carrying a sack of Saltpan salt to sell on the side to a Waste Road wagon. Salt is short, and he has a debt.
- **The choice:**
  - **a, he tells.** "Tell Fern yourself, or I will."
  - **b, the secret is kept.** "It stays between us. Don't do it again."
- **Repeat and limits** (decisions §8.1): weekly. `join.cap` 1. Deadline 1 game day. On a fail, Ridge is released and walks home himself, and the journal note says the storm let him go. With N4, `join.again: 7`. No second copy: one lost raker.
  - **It's taken** (H): `[Saltwell · saltwell · resident.news · world.mem.mq_raker_busy=yes]` "Someone is already out on the pans after the raker, my friend. The eel trapper at the pans has names that want carrying." It points to Q19.
- **Rewards:**
  - shared: 10 from `town:saltwell`; `story.practice travel`; deed "brought a lost salt raker in out of the sandstorm";
  - b: Ridge's window line (`say[self.id=tn_mb_ridge_eelbank world.mem.mq_raker=kept priority=3]` in `resident.hub`) is "You. The well remembers who drew from it, #address#. So do I."
- **Campaign:** feeds `nh_realm` +4 and `nh_voice_saltwell` +8. With Q18 it sets up the Waste Road salt trade the Captain's company lives off. From `nh_stage` ≥ 3, Ridge's reply names "wolves in grey on the Waste Road".
- **Dialogue** (Fern, saltwell):
  - ask: "Is everyone in from the pans?"
  - say: "#greet#. Not everyone. The wind came off the Red Waste at noon, and my workmate went out to the far pans for the rakes. He's not come back. Take water. Find him before the dark, and bring him in. By salt and water, I'd go myself, but someone must keep the fire."
  - accept: "I'll find him and bring him in."
  - decline: "Not into that wind."
  - handIn: "He's in, and he's had water."
  - thanks: "#thanks#. The pans would be poorer without his grumbling. Sit by the fire; the salt is out of your coat already."
  - **npc.offer** `lr_why` at Ridge (once found). Words: "There are no rakes out here. What were you doing?" Reply: "Mirage-talk, all of it. No. A sack of our salt, for a Waste Road wagon. They pay double now salt is short, and I owe more than I earn. Truly, I am ashamed."
  - **npc.offer** `lr_tell` at Ridge. Words: "Tell Fern yourself, or I will." Reply: "Then I'll tell her. Better from my mouth than the wind's. She'll be brackish with me for a month."
  - **npc.offer** `lr_keep` at Ridge. Words: "It stays between us. Don't do it again." Reply: "Between us. May your well never fail. I'll not carry another sack, on bread and salt."
  - **Close a** (P-D, in the window): "He told me. A sack for a wagon. I could spit. But he told me, and that's more than most would."
  - **Close b** (P-D, in the window): "He's quiet since. Rakes harder than he did. Whatever the wind said to him out there, it took."
  - **Journal notes** (today-only): a "The raker told his workmate himself." b "The raker's secret is kept." On a fail: "The storm let the raker go, and he walked home."
- **Gossip and helpful knowledge** (Saltwell, saltwell):
  - (H) `[resident.news · world.mem.mq_raker=open]` "In a salt wind, walk with the sun on your left and the pans will bring you home. That is what the old ones say, my friend."
  - `[resident.gossip · world.mem.mq_raker=told]` "A raker was selling Saltpan salt to the Waste Road wagons, and he confessed it. Brackish, but honest at the end."
  - `[resident.gossip · world.mem.mq_raker=kept]` "A raker came in out of the salt wind with a stranger's paw on his shoulder. He has not said a word since. Truly."
- **Engine needs:** C8, J1, C5, N4.

### Q18. `mq-waste-road-salt`: Salt on the Waste Road
- **Tier:** medium. **Program:** south. **Towns:** Saltwell's road home to Southmeadow.
- **Places:**
  - region `ws_wash` on `w_ridgemere_11_12`, tiles 156–170 × 156–170. This is the dry wash where the Waste Road (road tiles at (163, 163)) crosses the Red Waste;
  - region `sm_square` on `w_ridgemere_8_13`, tiles 93–108 × 14–26 (Southmeadow's square, by its board).
- **Giver:** Tor Chalkfield `cty_tor_chalkfield`. He is male, 24, a carter at Southmeadow, living at Southmeadow Cottage 2 `cn_southmeadow_cottage_2`. He speaks **southmeadow** with a light accent. He is warm and patient, calm, "always a little late", and hopes a child of the family will be taken into service with one of the great Houses.
  - **Why him** (decisions §2.4): Orm Sootgate keeps `lq-salt-road` and its Saltwell wagons. This is **Southmeadow's own salt wagon**, bringing Saltwell salt home south across the dry wash, so the two never share a wagon or a giver. The toll-takers are the same grey-paw band as the large line's raiders.
- **The cross-link, read only.** The quest reads the large line's world text `lq_salt` and never writes it.
  - `lq-salt-road` ends `board`, `bought` or `cart`. Decisions §2.4's "broken" is read here as **`board` or `cart`**: the provisioner who bought the robbed sacks is exposed or undersold, so nobody buys what the toll-takers take. After `bought`, he still buys, and the toll stands. (If the large reviser names the result otherwise, this gate follows that name.)
  - **With C6** (Phase 4): `requires: {"test":"not","condition":{"test":"compare","left":{"var":"lq_salt"},"op":"in","right":["board","cart"]}}` (C12b's `in`), so the quest is not offered once the wash is quiet.
  - **Until then**, the quest stays on offer, and on `quest accepted` a trigger checks `lq_salt`: if it is `board` or `cart`, it sets `ws_pick = quiet`, spawns nothing, and `deal` completes when Tor and the wolf reach the wash. That is a full ending of its own, below.
  - **Tor's window line** once the wash is quiet: `say[self.id=cty_tor_chalkfield world.mem.lq_salt=board|cart priority=2]` (the large line writes those keys): "The dry wash is quiet now. Nobody buys a robbed sack, so nobody robs one. I drive it singing, full sure."
- **Premise:** Southmeadow's salt wagon is on its way home from Saltwell, and last time the toll-takers stood up out of the dry wash and said pay next time. Next time is tomorrow, and Tor is late already. Ride with him, and when they stand up out of the wash: pay, or don't.
- **Setup:** on `quest accepted`: `npc.move` Tor to `w_ridgemere_11_12` (163, 160), then `npc.hold`. He waits with the wagon. `join.cap` 1.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `meet` | Meet the carter at the dry wash on the Waste Road. | `story signal` `ws_m` | `region entered` `ws_wash` with `resident in region` Tor. P-B spawns the wave in `ws_wash` (switch `spawned_ws_wash`), unless `ws_pick = quiet`. |
| 2 | `deal` | Pay the toll, or break it. | `story signal` `ws_d` | `paid off bandits` with `cell: w_ridgemere_11_12` (sets `ws_pick = paid`), or the hidden objective `fight` (`bandit falls` `cell: w_ridgemere_11_12`, goal 2) (sets `ws_pick = fought`). When quiet, raised on `meet`. |
| 3 | `deliver` | See the salt home into Southmeadow. | `region entered` `target: sm_square` | On `deal`: `npc.release`, then `npc.walk` Tor to `w_ridgemere_8_13` (100, 20), cue `ws_in`. The hand-in is wherever Tor is. |

- **The choice is made by what the wolf does,** and every ending is a full ending.
- **Repeat and limits** (decisions §8.1): weekly. `join.cap` 1. Deadline 1 game day; on a fail, Tor is released and drives home alone. With N4, `join.again: 7`. No second copy: Southmeadow has one salt carter and one wagon on this road.
  - **It's taken** (H): `[Southmeadow · southmeadow · resident.news · world.mem.mq_salt_busy=yes]` "The salt carter's away to the wash with a wolf already. The bee garth wants help, mind, and the press cellar." It points to Q13 and Q14.
- **Rewards:**
  - shared: `story.practice travel`;
  - paid: 6 from `town:southmeadow` (P-F), "part of the toll back"; deed "paid the road wolves' toll to get Southmeadow's salt home". No change to the threat;
  - fought: 12 from `town:southmeadow`; deed "broke a road toll on the Waste Road"; `nh_threat` −2;
  - quiet: 6 from `town:southmeadow`; deed "rode Southmeadow's salt home through a quiet wash".
- **Campaign:**
  - feeds `nh_realm` +4 and `nh_voice_southmeadow` +8;
  - **foreshadows Ch 2 and Ch 7**: the Captain is fed by tolls and wagon days. From `nh_stage` ≥ 3, the toll-takers are "in Deepvein grey" (talk). With C12a, the paid ending notifies the running chapter as a coin into the Captain's purse.
- **Dialogue** (Tor, southmeadow, light; warm, patient, late):
  - ask: "Heading home with the salt?"
  - say: "#greet#. Late, as ever. The salt's loaded at Saltwell, and I bring it home tomorrow. Last time they stood up out of the dry wash, five of them, and said pay next time. Ride with me? Pay them or don't; I'll not argue. Only get me home before the sun's too high."
  - accept: "I'll meet you at the wash."
  - decline: "Pay them. It's cheaper."
  - handIn: "The salt's in Southmeadow."
  - thanks: "#thanks#. Home, and only a little late. My sister's sprout wants to go into service with a great House one day. I'll tell her about you instead."
  - (No `npc.offer`: the choice is made at the wash.)
  - **Close paid** (P-D, in the window): "They took the coin and laughed. We got through. Next time it'll be more. It's always more, eh."
  - **Close fought** (P-D, in the window): "They ran for the dunes. Sheaf and sickle, I've never seen the like. I'll drive that wash singing next time."
  - **Close quiet** (P-D, in the window): "Not a soul in the wash. Somebody broke whoever was buying what they stole. I sang the whole way. Badly."
  - **Journal notes** (today-only): paid "Southmeadow's salt is home. The toll was paid." fought "Southmeadow's salt is home. The toll-takers ran." quiet "Southmeadow's salt came home through a quiet wash."
- **Gossip and helpful knowledge:**
  - `[Saltwell · saltwell · resident.gossip · world.mem.mq_salt=open]` "Toll-takers in the dry wash on the Waste Road. Five of them, they say. Mirage-talk, I hope. I fear not."
  - (H) `[Southmeadow · southmeadow · resident.news · world.mem.mq_salt=open]` "The salt wagon comes home across the Waste tomorrow. If you've teeth, the dry wash is where the carter will want them."
  - `[Southmeadow · southmeadow · resident.gossip · world.mem.mq_salt=fought]` "Salt came home across the Waste clean, no toll paid. Ripe news. Somebody saw the toll-takers off into the dunes."
  - `[Southmeadow · southmeadow · resident.gossip · world.mem.mq_salt=paid]` "Salt came in, but somebody paid the road lot to let it. Blight take them, it'll be dearer by harvest."
- **Engine needs:** C8; C6 and C12b (the offer gate); C10 `foe.add` (optional, a named toll-master); C12a (optional); N4.

### Q19. `mq-crossroads-names`: Names for the Crossroads
- **Tier:** medium. **Program:** south. **Towns:** Saltwell, the Mirebridge crossroads, and then the city or Fenshore.
- **Places:**
  - region `mx_cross` on `w_ridgemere_5_11`, tiles 183–195 × 45–56: the erased village's crossroads, around road tile (188, 52);
  - Soundings `rm_soundings` (door `w_ridgemere_1_1` (69, 74));
  - the Withy Creek eel home `fs_marsh_eels_home`.
- **Giver:** Kite Bogditch `tn_mb_kite_bogditch`. He is male, 37, and works the salt at the Saltpan, living at `fs_salt_saltpan_home` with old Bramble Bogditch. He is Mirebridge-born, Saltwell-raised (saltwell, light accent), loud and ambitious, "remembers every name ever heard", and is "the best eel trapper on the river, and says so", the Mirebridge mark. He is not one of the campaign's Keepers.
- **Premise:** Kite remembers the names of the Mirebridge drowned that nobody says aloud any more. The pans will not let him go. Carry his list to the crossroads where the village stood, say the names there at dusk, and then put the list in paws that will keep it.
- **Setup:** on `quest accepted`, `item.give paper 1` ("Kite's list").
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `cross` | Stand at the Mirebridge crossroads at dusk and say the names. | `story signal` `mx_c` | P-A on `mx_cross`, hours 18–21 |
| — | `token` (optional) | Leave something at the crossroads for them. | `item dropped`, `cell: w_ridgemere_5_11` | `bonus`: deed "left a token for the drowned at the Mirebridge crossroads" |
| 2 | `keep` | Put the list in paws that will keep it. | `story signal` `mx_k` | a: `gift` `paper` to `rm_imogen_nailridge` (the cartographer at Soundings, who takes Mirebridge's pledges in the city until Ch 6). b: `gift` `paper` to `tn_mb_tamsin_eelbank` (70, the old Mirebridge wolf at Withy Creek). The gift sets `mx_pick`. |

- **The choice is made by the delivery:**
  - **a, the city.** The names go onto the realm's map.
  - **b, Mirebridge folk.** The names stay with those who knew them.
- **Hand-in:** `handIn: false`. The quest ends where the list is given. Kite's thanks is his close, in his own window (P-D): no `story.tell` carries it.
- **Repeat and limits:** weekly. `join.cap` 6. Deadline 3 game days, for the travel. Before Tamsin Eelbank is named, the trigger checks `resident is tn_mb_tamsin_eelbank alive`. If she is dead, ending b is not offered, and the journal line names only the city.
- **Rewards:**
  - shared: 8 from `town:saltwell`; `story.practice travel`;
  - a: deed "carried the Mirebridge names to the city's mapmaker";
  - b: deed "gave the Mirebridge names back to Mirebridge folk".
- **Campaign:**
  - feeds `nh_realm` +4 and **`nh_voice_mirebridge` +8**, the hardest voice to raise (no square, no board, no purse);
  - adds **`nh_names` +1 while `nh_stage` is 4 to 6**, which the campaign allows packs to write (`campaign.md` §7). This is the Book of Names.
- **Dialogue** (Kite, saltwell):
  - ask: "They say you never forget a name."
  - say: "#greet#, #address#. I never do. I have forty Mirebridge names that nobody says any more, and the pans will not let me go to say them. Carry them for me? To the crossroads where the village stood, at dusk. Then give them to someone who will keep them. The well remembers who drew from it."
  - accept: "I'll carry them, and say them."
  - decline: "The dead should rest."
  - handIn: (none; `handIn: false`)
  - thanks: (none shown; the close is P-D)
  - **Close a** (P-D, in the window): "The mapmaker has them? Then they are on the realm's map, if only in ink. May your well never fail, my friend."
  - **Close b** (P-D, in the window): "The old one at Withy Creek has them? Good. Mirebridge keeps its own. By salt and water, I can sleep."
  - **Journal notes** (today-only): a "The Mirebridge names are with the city's mapmaker." b "The Mirebridge names are with the old wolf at Withy Creek."
- **Gossip and helpful knowledge:**
  - `[Saltwell · saltwell · resident.gossip · world.mem.mq_names=open]` "The eel trapper at the pans knows every Mirebridge name. He is looking for someone to carry them home, they say."
  - `[Fenshore · mirebridge · resident.gossip · world.mem.mq_names=fen]` "Somebody said the Mirebridge names at the crossroads at dusk, then gave them to the old one on Withy Creek. Dry feet to them."
  - `[Ridgemere · plain · resident.gossip · world.mem.mq_names=city]` "The mapmaker at Soundings has a list of Mirebridge names, brought up from the crossroads. A village on the map again, in ink."
  - (H) `[Larkfield · larkfield · resident.news · world.mem.mq_names=open]` "If you're going to the Mirebridge crossroads, go at dusk. That's when the old village said its names. By the market cross, it is."
- **Engine needs:** C8, C5; C11 (Tamsin Eelbank is 70: an old-age hold); J1 is not needed (no offers).

---

## 4. Oakhaven and Frosthollow (`nh-pack.medium.north`)

### Q20. `mq-windthrown-cedar`: Windthrown on the Sound Road
- **Tier:** medium. **Program:** north. **Town:** Oakhaven.
- **Places:**
  - region `wc_tree` on `w_ridgemere_4_4`, tiles 202–220 × 178–192. This is the Sound Road's footpath below Oakhaven (path tiles run from (217, 179) down to (205, 191)), in the Oakhaven cut's own cell;
  - region `oh_square` on `w_ridgemere_3_4`, tiles 235–253 × 190–201 (Oakhaven's square);
  - the Oakhaven cut, `w_ridgemere_4_4`, where the loggers work.
- **Giver:** Tristram Rainridge `cty_tristram_rainridge`. He is male, 37, the carter at Oakhaven, living at `cn_oakhaven_cottage_1`. He speaks **oakhaven** with a broad accent, is short-tempered and reserved, "proud and warm; he has opinions about everyone's grandmother", and "owes a debt to someone in Ridgemere and has not said to whom".
- **Premise:** Last night's wind threw a cedar across the Sound Road below Oakhaven. Tristram's wagon is on the wrong side of it, with a Ridgemere creditor's order of timber and Oakhaven's winter salt. The wolves who camp toward Alder Mere will smell a stuck wagon by dusk. And Tristram has a question he'd rather not ask out loud.
- **Setup:** on `quest accepted`: `npc.move` Tristram to `w_ridgemere_4_4` (210, 186), then `npc.hold`. `join.cap` 1.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `loggers` | Fetch help from the loggers at the Oakhaven cut. | `conversation` `target: cty_frost_colddrift` | Frost Colddrift, who fells timber and "keeps a tally of favours owed". Any order with 2. |
| 2 | `rope` | Bring two lengths of rope to haul the trunk. | `gift` `target: cty_tristram_rainridge` `item: rope`, goal 2, quantity | |
| 3 | `hold` | Hold the wagon while the trunk comes off the road. | `bandit falls` `cell: w_ridgemere_4_4`, goal 2 | P-B: when 1 and 2 are done, spawn in `wc_tree`. |
| 4 | `choose` | He asks you to help him keep a load back. Decide. | `story signal` `wc_chose` | P-C at Tristram |
| 5 | `road` | Walk the wagon up into Oakhaven. | `region entered` `target: oh_square` | On choice: `npc.release`, then `npc.walk` Tristram to (244, 196). |

- **The choice:**
  - **a, every stick.** Every stick goes to the creditor, and Tristram stays honest and in debt.
  - **b, the lie.** "Tell him the road wolves took a load." Tristram keeps a load to pay his debt, and the wolf helps him lie.
- **Repeat and limits** (decisions §8.1): weekly. `join.cap` 1. Deadline 1 game day; on a fail, Tristram is released and the journal note says the loggers cleared the road at dawn. With N4, `join.again: 7`. No second copy: one carter, one cedar.
  - **It's taken** (H): `[Oakhaven · oakhaven · resident.news · world.mem.mq_cedar_busy=yes]` "The carter's got help on the Sound Road already, mm? The logger at the cut wants hands for her runaway logs." It points to Q23.
- **Rewards:**
  - shared: 10 from `town:oakhaven`; `story.practice labour`;
  - a: deed "cleared a windthrown cedar off the Sound Road";
  - b: deed "helped an Oakhaven carter short his Ridgemere creditor". This is meant to be talked about.
- **Campaign:**
  - feeds `nh_realm` +4, `nh_voice_oakhaven` +4 and `nh_threat` −2 (the Alder Mere road; `campaign.md` §5.3 credits camp `5_4` to Oakhaven);
  - **foreshadows Ch 1**, the Oakhaven road post on the Alder Mere camp (post 10), and **Ch 2**: Tristram is a debtor of the city's money. With ending b, Ch 2's debtor lines about Oakhaven gain "and some of them found their own way to pay".
- **Dialogue** (Tristram, oakhaven, broad):
  - ask: "Road trouble?"
  - say: "#greet#. Road trouble, proper. Wind threw a cedar clean across the Sound Road below the cut, and my wagon's the wrong side of it, timber for the city and our winter salt. Fetch the loggers, bring me two ropes, and stand by me at dusk, mm? The lot toward Alder Mere can smell a stuck wagon a mile off."
  - accept: "Loggers, two ropes, and a long dusk."
  - decline: "Wait for the loggers."
  - handIn: "The wagon's in Oakhaven."
  - thanks: "#thanks#. Good bark, you are. I'll say nothing against your grandmother, and that's a gift from me."
  - **npc.offer** `wc_ask` at Tristram (after `hold`). Words: "You've something to ask. Ask it." Reply: "I owe a wolf in the city, more than I'll say. If the road lot had taken one load of his timber, nobody'd blink, mm? One load. It'd clear me."
  - **npc.offer** `wc_honest` at Tristram. Words: "Every stick goes to him. You'll clear it honestly." Reply: "Honestly. Bark and branch, you sound like my mother. Fine. Every stick, and I'll owe till the Long Night."
  - **npc.offer** `wc_lie` at Tristram. Words: "The road wolves took a load. That's what I saw." Reply: "That's what you saw. Proper. I'll not forget it, old root, and I'll not say it twice."
  - **Close a** (P-D, in the window): "Every stick delivered. The city wolf counted it twice, the miser. I'm poorer and I sleep. Root well."
  - **Close b** (P-D, in the window): "One load lighter, one debt lighter. Don't look at me like that. Shade keep you."
  - **Journal notes** (today-only): a "Every stick reached the city creditor." b "One load of timber stayed in Oakhaven." On a fail: "The loggers cleared the road at dawn."
- **Gossip and helpful knowledge** (Oakhaven, oakhaven):
  - (H) `[resident.news · world.mem.mq_cedar=open]` "A cedar windthrown across the Sound Road below the cut, mm? The loggers have the ropes and the know-how; ask them."
  - `[resident.gossip · world.mem.mq_cedar=honest]` "The carter got through to the city, every stick delivered. Proper honest, that one. Poorer for it, mind."
  - `[resident.gossip · world.mem.mq_cedar=kept]` "The road lot took a load off the carter, they say. Odd. Never seen him look so easy about a robbery."
  - `[Ridgemere · plain · self.quarter=commons · world.mem.mq_cedar=kept]` "An Oakhaven carter came in a load short and swore the road wolves had it. His creditor's not best pleased."
- **Engine needs:** C8, J1, C5, N4.

### Q21. `mq-low-drift-props`: Rotten Props in the Low Drift
- **Tier:** medium. **Program:** north. **Town:** Frosthollow, at the iron mine.
- **Places:**
  - region `ld_mouth` on `w_ridgemere_9_2`, tiles 200–216 × 207–216. This is the mine mouth, where the footpath (x 199–211, y 211–212) meets the rock and rubble at the iron mine's spot (212, 213);
  - Frosthollow Stores `cn_frosthollow_store` (door `w_ridgemere_8_3` (250, 24));
  - The Lamp and Pick `cn_frosthollow_alehouse` (door `w_ridgemere_9_3` (14, 28)).
- **Giver:** Ebba Coalgate `cty_ebba_coalgate`. She is female, 36, the carter at Frosthollow, living at `cn_frosthollow_cottage_1`. She speaks **frosthollow** with a light accent, is quiet, dry and warm, "loyal and quietly funny; she never sits with a back to the door".
  - **Why her:** a carter sees what goes up and down the Pass Road, and she is no miner, so the mine captain's displeasure costs her less.
- **Premise:** Last month Ebba hauled good cedar props up the Pass Road for the low drift. This week she watched rotten ones go in. Somebody sold the good ones down the valley. "A drift with rotten props is a grave with the lid open."
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `drift` | Look at the props at the mine mouth. | `region entered` `target: ld_mouth` | |
| 2 | `store` | Ask the shopkeeper who bought cedar props this month. | `conversation` `target: tn_fh_aster_coldfell` | Aster Coldfell, calm and sly, who "means to go down to Ridgemere". She lets slip that the mine house sold props to a Ridgemere yard. |
| 3 | `beams` | Bring four sound beams for the low drift. | `gift` `target: cty_ebba_coalgate` `item: beam`, goal 4, quantity | `beam` is sold in the city and in Oakhaven. Any order with 4. |
| 4 | `choose` | Decide: say it aloud, or shore it up quietly. | `story signal` `ld_chose` | P-C at Ebba, after 2 |
| 5a | `taproom` (hidden; a) | Say it in the Lamp and Pick, where the miners drink. | `story signal` `ld_t` | `enter place` `cn_frosthollow_alehouse` with `hour between` 18–23 raises it (P-A form). On it, once a game day (world switch `ld_posted`), `board.post {town: frosthollow, days: 3}`: "Rotten props in the low drift. The good cedar went down the valley. Ask at the mine house who sold it. Signed, the colliers at the Lamp and Pick." No resident speaks, and the innkeeper is not a target here (decisions §1.2.4). |
| 6 | `shored` | The low drift is shored. | `story signal` `ld_s` | raised by 3 (and 5a, for ending a) |

- **The choice:**
  - **a, aloud.** Everyone in the Lamp and Pick knows by nightfall. The mine captain, Fenwick Plankworth `cty_fenwick_plankworth`, is talked about (talk only; he is not a target here).
  - **b, quietly.** The beams go in, and the captain keeps his name. He also keeps whatever he was paid.
- **Repeat and limits:** weekly. `join.cap` 6. Deadline 3 game days, for the beams.
- **Rewards:**
  - shared: 10 from `town:frosthollow`; `story.practice labour`;
  - a: deed "named the rotten props in the Frosthollow mine aloud";
  - b: deed "shored up the Frosthollow low drift with sound beams".
- **Campaign:**
  - feeds `nh_realm` +4 and `nh_voice_frosthollow` +6;
  - **foreshadows Ch 2** (Fenwick's seam note), **Ch 4** (Highmere's silver) and **Ch 8** (his confession about the barred door). Ending a's key `mq_props=named` gives Ch 8's Frosthollow lines "he's been caught out before"; ending b's `mq_props=quiet` gives "and some still cover for him";
  - **reads** `nh_b_seam`: if it is `crooked`, Aster's reply in step 2 adds "same as the seam note, stone".
- **Dialogue** (Ebba, frosthollow, light):
  - ask: "Bad road up the pass?"
  - say: "#greet#. Road's fine. It's what came off it. Last month I hauled good cedar props up for the low drift. This week rotten ones went in. Somebody sold the good ones down the valley, and a drift with rotten props is a grave with the lid open, eh. Look at the mouth. Then ask the shop who bought cedar."
  - accept: "I'll look, and ask."
  - decline: "Mines aren't my business."
  - handIn: "The low drift's shored."
  - thanks: "Lamp's thanks. Sound beams, deep in. The miners'll never know whose paws they owe their roof to. That's how it should be."
  - **npc.offer** `ld_loud` at Ebba. Words: "Say it in the Lamp and Pick. The miners should know." Reply: "Aloud, then. Cold iron, the captain won't love you for it. The taproom keeper's a fair wolf; tell him, and the whole valley hears it by the last lamp."
  - **npc.offer** `ld_quiet` at Ebba. Words: "Shore it up quietly. Let the captain keep his name." Reply: "Quiet. Aye, quiet's safer for the ones who work under him. Four beams, and we'll set them before the shift."
  - **Close a** (P-D, in the window): "Half the miners were at the mine house door by morning. The captain says the props came cheap from a city yard. Funny, that."
  - **Close b** (P-D, in the window): "Four sound beams in the low drift, and nobody's the wiser. The captain thanked me for 'the extra'. Black seam, the nerve."
  - **Journal notes** (today-only): a "The rotten props were named aloud in Frosthollow." b "The low drift is shored, quietly."
- **Gossip and helpful knowledge** (Frosthollow, frosthollow):
  - (H) `[resident.news · world.mem.mq_props=open]` "Rotten props went into the low drift this week. The shop knows who bought the good cedar, if you ask it right."
  - `[resident.gossip · world.mem.mq_props=named]` "Somebody named the rotten props aloud in the Lamp and Pick. The mine house had visitors at first light, eh."
  - `[resident.gossip · world.mem.mq_props=quiet]` "Sound beams in the low drift, set before the shift. Nobody's saying whose. Deep thanks, whoever."
  - `[Ridgemere · plain · self.quarter=works · world.mem.mq_props=named]` "Frosthollow mine props turning up in a Cinders yard. Bought cheap, they say. Somebody up the pass is fuming."
- **Engine needs:** C8, J1, C5. The taproom's word is a signed board notice, so nothing here needs a region over the taproom.

### Q22. `mq-lamp-oil-share`: Oil for the Long Dark
- **Tier:** medium. **Program:** north. **Town:** Frosthollow. **Closure (J3):** none (answer (b)); a journal note invites a social Story.
- **Places:**
  - the Upper hearths, `w_ridgemere_10_3` (the giver's work);
  - Frosthollow Stores `cn_frosthollow_store`;
  - The Lamp and Pick `cn_frosthollow_alehouse`;
  - the high valley and the Whitefangs for game.
- **Giver:** Stone Acorndell `cty_stone_acorndell`. He is male, 46, and burns charcoal at the Upper hearths, living at `cn_frosthollow_collier_house`. He speaks **frosthollow** with a broad accent, is chatty, short-tempered and reserved, "easily offended and warm; he keeps a pocketful of coal for luck", and has a cousin in the Fell vineyards.
- **Premise:** In the deep winters Frosthollow keeps a list of who shared their lamp oil. It is the list that, in the records, has no Deepvein name on it. The colliers render the autumn kill's tallow for the long dark. Stone needs hunters. Then comes the old question: put the oil on the common shelf at the Lamp and Pick, where the list is kept, or sell it in the city, where it fetches three times as much.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| — | `hunt` (optional) | Bring down an ibex in the high valley. | `hunted` `target: ibex` | `bonus`: deed "hunted an ibex for Frosthollow's winter tallow" |
| 1 | `tallow` | Bring four tallow to the Frosthollow store for rendering. | `gift` `target: tn_fh_aster_coldfell` `item: tallow`, goal 4, quantity | On completion, `item.give lamp_oil 3` and P-C at Stone. |
| 2 | `choose` | Decide where the oil goes. | `story signal` `lo_chose` | |
| 3a | `shelf` (hidden; a) | Set the oil on the common shelf at the Lamp and Pick. | `gift` `target: tn_oh_thorn_ashwood` `item: lamp_oil`, goal 2, quantity | the innkeeper, as keeper of the shelf: his one target use in any file (decisions §1.2.4) |
| 3b | `sell` (hidden; b) | Sell the oil, wherever it fetches most. | `trade done` `item: lamp_oil` `detail: sold`, goal 2, quantity | |
| 4 | `done` | The oil is where you put it. | `story signal` `lo_d` | |

- **The choice:**
  - **a, the shelf.** The wolf's name goes on this winter's list: the world `wolves` list `mq_oil_list`, which this pack owns and declares.
  - **b, sell.** The wolf takes the city price.
- **Repeat and limits:** weekly. `join.cap` 6. Deadline 3 game days. In winter (`calendar is season winter`), Stone's close is warmer and ending a gives 4 more pennies.
- **Rewards:**
  - shared: `story.practice labour`;
  - a: 6 from `town:frosthollow` (10 in winter); deed "put lamp oil on Frosthollow's common shelf for the long dark"; `list add mq_oil_list $actor`;
  - b: no coin from the town, since the trade paid; deed "sold Frosthollow's winter oil down in the city".
- **Campaign:**
  - feeds `nh_realm` +4, and `nh_voice_frosthollow` +6 (ending a only);
  - **foreshadows Ch 8 and the Grey Captain's grievance**: the lamp-oil list that has no Deepvein name, the barred door, Ulric's brother Hakon. Ch 8 can read `mq_oil_list` (`in list`) to tell a wolf "your name's on this winter's list", and Ch 9's Turn at the Pass can use it as a small weight toward turning the Captain. "They put names on it now that aren't even miners";
  - it ends with a journal note inviting a social Story (no `closure`: answer (b)).
- **Dialogue** (Stone, frosthollow, broad):
  - ask: "Burning for the winter already?"
  - say: "#greet#. Always, stone. In the deep winters we keep a list of who shared their lamp oil, names on it from before my grandda. Autumn tallow makes the oil. Bring four tallow to the shop to render, and we'll see where the oil goes, eh. Some sell it in the city for three times the price. Some don't."
  - accept: "Four tallow, to the shop."
  - decline: "Winter's a long way off."
  - handIn: "The oil's where I put it."
  - thanks: "Lamp's thanks. Here's a lump of coal for luck. Don't laugh. It's kept me alive twice."
  - **npc.offer** `lo_shelf` at Stone. Words: "On the common shelf at the Lamp and Pick. Put my name on the list." Reply: "On the list. #oath#. You're not from here and you'd go on the list? Then you're from here now, a bit. The taproom keeper keeps the shelf. Mind he writes you down proper."
  - **npc.offer** `lo_sell` at Stone. Words: "It'll fetch three times as much in the city." Reply: "It will. Dead rock, I'll not stop you. It's yours to sell. Only, the list's not got room for coin. Never has."
  - **Close a** (P-D, in the window): "Your name's on the list. Under the last hard winter's. I read it out at the Lamp and Pick, and nobody laughed."
  - **Close b** (P-D, in the window): "Sold it, did you. Well. City lamps burn as bright. They just don't remember who lit them."
  - **Journal notes** (today-only): a "Your name is on Frosthollow's lamp-oil list." b "Frosthollow's winter oil was sold."
- **Gossip and helpful knowledge** (Frosthollow, frosthollow):
  - (H) `[resident.news · world.mem.mq_oil=open]` "Want to be on the lamp-oil list? Tallow from the kill, rendered at the shop. Ibex in the high valley give the best."
  - `[resident.gossip · world.mem.mq_oil=shared]` "Another name on the lamp-oil list this year, and not a miner. Deep strange. Deep good, mind."
  - `[resident.gossip · world.mem.mq_oil=sold]` "Somebody rendered the autumn tallow and sold the oil in the city. Cold iron. Some folk don't know what a list is for."
  - `[Ridgemere · plain · self.quarter=works · world.mem.mq_oil=sold]` "Frosthollow lamp oil for sale on the Commons, three times the price. Burns clean, I'll say that."
- **Engine needs:** C8, C5; J2 (a realm-player flag instead of a world list); M5 (`trade done` has no `cell`, so "sold in the city" cannot be checked; any sale counts).

### Q23. `mq-logs-adrift`: Logs Adrift on the Sound
*New: Oakhaven's second medium quest (decisions §7.2, critic P1-16).*
- **Tier:** medium. **Program:** north. **Town:** Oakhaven (the cut), and the south shore of Cedar Sound.
- **Places:**
  - the Oakhaven cut, `w_ridgemere_4_4`, where the giver fells; she lives at Oakhaven Cottage 1 `cn_oakhaven_cottage_1` (door `w_ridgemere_4_4` (13, 201));
  - region `la_shore` on `w_ridgemere_2_5` (Cedar Sound, South), tiles 132–172 × 153–157: the stony south shore of the Sound (shingle `0` and `o`, rock `r`, shallows `~`);
  - region `la_landing` on `w_ridgemere_2_5`, tiles 84–100 × 66–73: the shingle landing below Soundside, by the Soundside Boat Shed `fs_fishing_sound_shed` (door (115, 69)) and Soundside Cottage `fs_fishing_sound_home` (door (108, 63)).
- **Giver:** Oda Cinderdale `cty_oda_cinderdale`. She is female, 29, a logger at the Oakhaven cut. She speaks **oakhaven** with a light accent. She is quietly funny and unhurried, content, dry and calm, and argues with the weather.
  - **Why her:** she is Oakhaven's only free resident (decisions §1.2.5). The campaign has another Oda, so no talk line names either one; the lines below use roles only.
  - Her backstory names a debt in the city. This quest does not use it: Oakhaven's debt story is Q20's.
  - **Walls kept** (decisions §7.2): no windthrown cedar (Q20), no mast cedars (the large line), nothing of the wronged logger or the common right (campaign Ch 1 and Ch 5). These are the cut's ordinary logs, rafted for the city's yards.
- **The skipper** (a gift target only; no hub item): Kell Reedwade `cty_kell_reedwade`, male, 56, skipper at Soundside, living at Soundside Cottage. Ridgemere-raised, **plain**, light accent; wry and tired, hums old marching songs, and is "saving coin toward something no one else knows about". He appears in no other file.
- **Premise:** The cut sends its logs down the creek to the Sound, roped into a raft for the city yards. Last night the fog came down and the raft's rope parted. The logs are fetching up along the south shore of Cedar Sound, and the skipper at Soundside buys driftwood and doesn't ask whose. Oda would like her logs back. She'd understand if they were sold. Mostly.
- **Setup:** on `quest accepted`, `weather.set` `place: w_ridgemere_2_5` `weather: fog` `hours: 6` (`ifRefused: skip`).
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `shore` | Walk the south shore of Cedar Sound in the fog and find the logs. | `story signal` `la_s` | `region entered` `la_shore` raises it. On it (guarded by `la_placed`): `item.place timber 2` at (144, 154) and `item.place timber 2` at (160, 153), and `story.ping` at (152, 154), 30 minutes, label "logs in the shallows". |
| 2 | `logs` | Haul four logs out of the shallows. | `item picked up` `item: timber`, goal 4, quantity | |
| — | `landing` (optional) | See what has fetched up at the Soundside landing. | `region entered` `target: la_landing` | On it, `item.place timber 1` at (90, 70): a fifth log. `bonus`: `story.practice travel`. |
| 3a | `home` (hidden) | Bring the logs home to the cut. | `gift` `target: cty_oda_cinderdale` `item: timber`, goal 4, quantity | revealed with 3b when 2 is done |
| 3b | `sold` (hidden) | Or sell them to the Soundside skipper. | `gift` `target: cty_kell_reedwade` `item: timber`, goal 4, quantity | |
| 4 | `deliver` | The logs are where you took them. | `story signal` `la_d` | The first of 3a or 3b to complete sets `la_pick` (`home` or `sold`), fails the other (`objective.fail`), and emits `la_d`. |

- **The skipper's words, in his own window:** `say[self.id=cty_kell_reedwade player.quest.nh-pack.medium.north/mq-logs-adrift=active priority=3]` with N2, and `say[self.id=cty_kell_reedwade world.mem.mq_logs=open priority=3]` until then:
  - "Cedar off the Sound, is it? I pay for driftwood, a fair price, and I don't ask whose axe it knew. Bring it up the landing and I'll stack it."
- **The choice is made by what the wolf does:**
  - **a, home to the cut.** Oakhaven's logs go back to Oakhaven: a long, wet haul uphill.
  - **b, sold at Soundside.** The skipper pays Soundside's price for driftwood, the wolf is paid better, and the cut is four logs short.
- **Hand-in:** `handIn: false`. The quest ends where the logs are given.
- **Repeat and limits:** weekly. `join.cap` 3 (the logs are placed items per wolf). Deadline 2 game days.
- **Rewards:**
  - shared: `story.practice labour`;
  - home: 10 from `town:oakhaven` (P-F), "the cut's carriage"; deed "brought the Oakhaven cut's runaway logs home from Cedar Sound";
  - sold: 14 from `town:ridgemere` (P-F; Soundside trades with the city), "Soundside's price for driftwood"; deed "sold the Oakhaven cut's runaway logs at Soundside".
- **Campaign:** feeds `nh_realm` +4; ending a also feeds `nh_voice_oakhaven` +4. Ending b serves no town, so it feeds no voice. It stands alone, and gives Oakhaven a second medium quest beside Q20.
- **Dialogue** (Oda, oakhaven, light; dry, unhurried):
  - ask: "Weather against you?"
  - say: "#greet#. Told the fog to keep off the Sound, and it came anyway, mm? The raft's rope parted in the night, and my logs are fetching up all along the south shore. Haul me four out of the shallows. Bring them home if you will. The Soundside skipper buys driftwood. I'll not hold it against you. Much."
  - accept: "Four logs out of the shallows."
  - decline: "Not in this fog."
  - handIn: (none; `handIn: false`)
  - thanks: (none shown; the close is P-D)
  - during (C6), duringAsk "About the logs…": "South shore of the Sound, west of the Soundside landing. Shingle and shallows. Mind your paws; cedar's slippery when it's sulking."
  - **Close home** (P-D, in the window): "Four logs home, wet as otters. And the fog's lifted. I told it to, mind. Root well."
  - **Close sold** (P-D, in the window): "Sold to the skipper, were they? Proper price, I hope. The cut's four logs short and the fog's sorry. One of you should be."
  - **Journal notes** (today-only): home "Four logs are back at the Oakhaven cut." sold "The Soundside skipper paid for four Oakhaven logs."
- **Gossip and helpful knowledge:**
  - `[Oakhaven · oakhaven · resident.gossip · world.mem.mq_logs=open]` "The cut's raft broke up in the fog, mm? Logs all down the Sound shore. Somebody'll be hauling wet cedar."
  - (H) `[Oakhaven · oakhaven · resident.news · world.mem.mq_logs=open]` "Logs off a broken raft fetch up on the south shore of the Sound. Shingle and shallows, west of the Soundside landing."
  - `[Oakhaven · oakhaven · resident.gossip · world.mem.mq_logs=home]` "The runaway logs came home to the cut. Somebody hauled them uphill wet. Proper, that."
  - `[Ridgemere · plain · self.quarter=wharf · world.mem.mq_logs=sold]` "Oakhaven cedar stacked at Soundside, bought as driftwood. The cut's not best pleased, they say."
- **Engine needs:** C8, N2 (the skipper's line on the quest), C6, C5. No J1: there are no offers.

---

## 4b. The stage quests (`nh-pack.stage`, medium tier)

These are the five medium entries of the stage pack (decisions §7.3). **The program is written once, by the small pack's reviser**, in `quests-small.md` (one open-audience program, cap 5000, at most 32 quests, file `Data/Storylines/ridgemere3/quests-stage/nh-pack.stage.json`; phases `quiet`, `s12`, `s3`, `s4`, `s56`, `s78` and `s9`, entered by an `hour` trigger that reads `nh_stage`, with unset or 0 meaning `quiet`). Each entry below lists its `phases`, so it is offered only while that stage runs.

**What the program carries for these five** (its reviser adds them to the program's declarations):
- **Purses** (P-F): `town:coldwater` (S1, S2, S4, S5) and `town:larkfield` (S3), with `coin_coldwater_day`, `coin_coldwater_ok`, `coin_larkfield_day` and `coin_larkfield_ok`.
- **Fights** (P-B): the world switches `spawned_gw_road`, `spawned_bc_road`, `spawned_rc_road` and `spawned_br_stage`.
- **Reads:** `nh_stage`, `nh_threat` (S5), `nh_b_bell` (S5); **writes** `nh_realm`, `nh_voice_<town>` and `nh_threat` only (packs never write a branch, a post or `nh_stage`).
- **Its own:** the world switch `st_ice_frozen` (S4); the player texts `gw_pick`, `bc_pick`, `rc_pick`, `iw_pick`, `br_pick`; the player switches `<q>_spawned` and a player timer `iw_t`.
- **Feeds:** a medium is worth 4 times the distance weight (decisions §7.3), as in P-D.
- **Talk keys** use the prefix `st_` (the "open" key of each quest is written at the phase's entry with C8, or seeded by the DM until then).
- **Patterns:** P-A to P-F of §0.2 apply unchanged. Every coin reward goes through P-F; every close is P-D.

| # | Quest | Phases | Giver (dialect) | Town |
|---|---|---|---|---|
| S1 | `st-greenholt-wagon` | `s3` | Aud Saltridge `rmx_aud_saltridge` (plain) | Ridgemere to Coldwater |
| S2 | `st-black-cloak-road` | `s56` | Wick Marshwade `cty_wick_marshwade` (coldwater, broad) | Coldwater; the road below Frosthollow |
| S3 | `st-relief-convoy` | `s78` | Myrtle Larkcombe `cty_myrtle_larkcombe` (southmeadow, broad) | Larkholm to Larkfield |
| S4 | `st-ice-watch` | `s78`, once the mere is frozen | Garth Barleyton `cty_garth_barleyton` (coldwater) | the Upper Mere |
| S5 | `st-bell-road` | `s9`, if `nh_b_bell` is `rung` or `cracked` | Jory Rimepeak `tn_fh_jory_rimepeak` (coldwater) | Coldwater, the road west |

### S1. `st-greenholt-wagon`: The Coldwater Wagon
- **Tier:** medium. **Program:** `nh-pack.stage`. **Phases:** `["s3"]` (Ch 3, "Two Long, One Short": the signal lamps on the ridges and the notices "One mark a wagon, paid at the ridge").
- **Places:**
  - the Carters' Yard `r2_the_carters_yard` (door `w_ridgemere_1_1` (177, 34));
  - region `gw_road` on `w_ridgemere_4_5` (The Rainwood, North-east), tiles 40–72 × 64–96: the wagon road under the Alder Mere knoll (road tiles run from (40, 64) to (72, 96));
  - the Coldwater square, `w_ridgemere_7_7`, where the wagon stops (about (166, 6)).
- **The real road** (checkpoint `roads.caravans`): the wagons between Coldwater and Ridgemere run `7_7` → `7_6` → `6_6` → `5_6` → `4_6` → `4_5` → `3_5` → `3_4` (Oakhaven square) → `3_3` → `2_3` → `1_3` → `1_2` → `1_1` (route of `cv1535`), so a wagon out of the city takes it the other way and passes under Alder Mere in `4_5`.
  - Each wagon posts the game's own escort contract at the board of the market it leaves from, "guarding the next wagon from Ridgemere to Coldwater" (paid by `stores:coldwater`, `Core/RatwRoads.cpp:1076`), and the wagon "will wait a little" for its escort (`Core/RatwTradeShow.cpp:887`). A wolf may take that contract too; the game pays it, not this quest.
  - `caravan arrives` carries `target` = the market reached and `detail` = `"from <market>"`, with `", guarded"` when guarded (`Core/RatwRoads.cpp:1181`). `caravan departs` carries `target` = the market it goes to and the start cell.
- **Giver:** Aud Saltridge `rmx_aud_saltridge`. She is female, 56, drives for the carters at the Carters' Yard, and lives at Gutter Rents `r2_gutter_rents` (`w_ridgemere_0_0`). She was raised in Ridgemere and speaks **plain** with a light accent. She is restless and anxious, nervy, keeps a pocketful of coal for luck, and "grew up at a Timberland charcoal camp in the hills and still misses the quiet".
- **Premise:** Since the paw-mark notices went up, no driver at the Carters' Yard wants the Coldwater road. Aud will drive it if somebody rides. Go with the wagon, or go ahead of it and make the road under the Alder Mere lamp safe. Either way, be on the Coldwater square when it comes in.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `choose` | Ride with the wagon, or ride ahead of it. | `story signal` `gw_chose` | P-C at Aud, offered on `quest accepted` |
| 2a | `ahead` (hidden; a) | Get ahead of the wagon to the road under Alder Mere, after dark. | `story signal` `gw_a` | P-A on `gw_road`, hours 20–4; P-B spawns the wave there |
| 3a | `fight` (hidden; after 2a) | Clear the road under the lamp. | `bandit falls` `cell: w_ridgemere_4_5`, goal 2 | |
| 4 | `arrive` | Be on the Coldwater square when the wagon comes in. | `story signal` `gw_in` | See the trigger below. Path a needs `fight` done first. |

- **The arrival trigger** (L3 later makes `caravan arrives` a mission event):
  - `on: "caravan arrives"`, `match: {"target": "coldwater", "detail": "from ridgemere"}`, and a twin with `"detail": "from ridgemere, guarded"` (until C12b's `starts` merges them);
  - `each over participants`: if `quest is st-greenholt-wagon active`, `wolf in town coldwater`, and (for `gw_pick = ahead`) `objective is fight done`, then `objective.complete arrive` for that wolf.
  - On `caravan departs` with `target: coldwater` and `cell: w_ridgemere_1_1`, a `journal.note` to every participant: "The Coldwater wagon is leaving the city now."
- **The choice** (at Aud):
  - **a, ahead.** Aud drives with her one guard; the wolf clears the road under the lamp first.
  - **b, with the wagon.** The wolf rides with it (and may take the board's escort contract), so the guard has one more pair of teeth.
- **Repeat and limits:** weekly. `join.cap` 6 (a spawn per wolf only on path a). Deadline 4 game days, because the wagons keep the board's days. On a fail, the journal note reads "No wagon went out for Coldwater in time. The carter waits for the next."
- **Rewards:**
  - shared: `story.practice travel`;
  - a: 12 from `town:coldwater` (P-F), "Coldwater's thanks for a clear road"; deed "cleared the road under the Alder Mere lamp ahead of the Coldwater wagon"; `nh_threat` −2;
  - b: 8 from `town:coldwater`; deed "rode the Coldwater wagon past the lit ridges".
- **Campaign:** feeds `nh_realm` +4 and `nh_voice_coldwater` +6. It never scores Ch 3's shared goal itself, but a wolf on path a stands under the knoll (`5_4`) where Ch 3's lamp burns, and the chapter counts what it counts there.
- **Dialogue** (Aud, plain, light; restless, anxious):
  - ask: "Nobody wants the Coldwater road?"
  - say: "#greet#. Not since the paw-mark notices. 'One mark a wagon, paid at the ridge.' I'll drive it. I grew up by the clamps; a lamp on a hill doesn't frighten me. Much. Ride with me, or go ahead and see the road's clear under Alder Mere. Be on the Coldwater square when I come in."
  - accept: "I'll see you to Coldwater."
  - decline: "Pay the mark."
  - handIn: (none; `handIn: false`: it ends on the Coldwater square)
  - during (C6), duringAsk "About the wagon…": "We go when the board says. If you're going ahead, go after dark. The road under Alder Mere is where they wait."
  - **npc.offer** `gw_ahead` at Aud. Words: "I'll go ahead and clear the road under Alder Mere." Reply: "Ahead, then. After dark the lamp's lit on the knoll, and the road under it is where they wait. #oath#, mind yourself. I'll come through after you, slow."
  - **npc.offer** `gw_with` at Aud. Words: "I'll ride with the wagon." Reply: "With me. Good. Take the escort off the board if you like; the stores pay it, and they can spare it better than I can. We go when the board says."
  - **Close a** (P-D, in the window): "Not a paw on the road under Alder Mere. I drove it with my eyes shut. Nearly. I kept the coal in my fist the whole way."
  - **Close b** (P-D, in the window): "We came in whole, lamp on the knoll and all. Nobody asked us for a mark. I think they saw your teeth."
  - **Journal notes** (today-only): a "The road under Alder Mere was clear when the Coldwater wagon passed." b "You rode the Coldwater wagon in past the lit ridges."
- **Gossip and helpful knowledge:**
  - `[Ridgemere · plain · self.quarter=sump|commons · world.mem.st_wagon=open]` "The carters won't take the Coldwater road. A mark a wagon, paid at the ridge, the notices said."
  - (H) `[Ridgemere · plain · resident.news · world.mem.st_wagon=open]` "The wagon road to Coldwater runs under the Alder Mere knoll. That's where the lamp is, and where they wait."
  - `[Coldwater · coldwater · resident.gossip · world.mem.st_wagon=ahead]` "The city wagon came in clean. Somebody cleared the road under Alder Mere ahead of it, eh? Fair catch."
  - `[Coldwater · coldwater · resident.gossip · world.mem.st_wagon=with]` "The city wagon came in with a wolf riding on the tail. No mark paid. Calm water, that."
- **Engine needs:** J1, C6, C8, C5; L3 (arrival as an objective) and C12b (`starts`) to simplify; L2.

### S2. `st-black-cloak-road`: Before the Black Cloak
- **Tier:** medium. **Program:** `nh-pack.stage`. **Phases:** `["s56"]` (the black cloaks at the Pass House from Ch 5's end; "Walk the Envoy Down" runs into Ch 6).
- **The visitor is never referenced.** The campaign's Concord visitor is named here only as "the black cloak". No visitor id appears in this entry (decisions §7.3).
- **Places:**
  - region `bc_road` on `w_ridgemere_8_4` (The Larkdowns, North-west), tiles 165–185 × 150–165: the road down from Frosthollow toward the Upper Mere (road tiles from (175, 150) to (164, 165)), the first stage below the Pass House;
  - the Pass House `cn_frosthollow_guest_inn` (door `w_ridgemere_8_3` (241, 45)).
- **Giver:** Wick Marshwade `cty_wick_marshwade`. She is female, 36, a carter at Coldwater, living at Coldwater Cottage 7 `cn_coldwater_cottage_7`. At the checkpoint she was driving the Coldwater wagon `cv1535` to the city. She speaks **coldwater** with a **broad** accent. She is playful and anxious, chatty, merry and nervy, hums old marching songs, and hopes a child of the family will be taken into service with one of the great Houses.
- **Premise:** Black cloaks came down the Pass and stopped at the Pass House with their boots still on, and they will want walking down to the city. Wick carts the road below Frosthollow, and she knows where the Captain's men sit by it. Scout the first stage below the Pass before the black cloak walks it, and clear it. Then tell Wick what you found.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `scout` | Scout the road below Frosthollow, the first stage down from the Pass. | `region entered` `target: bc_road` | P-B spawns the wave there |
| 2 | `fight` | Clear the stage. | `bandit falls` `cell: w_ridgemere_8_4`, goal 2 | |
| — | `pass` (optional) | Look in at the Pass House on your way. | `enter place` `target: cn_frosthollow_guest_inn` | `bonus`: `story.practice travel` |
| 3 | `choose` | Tell the carter what you found. | `story signal` `bc_chose` | P-C at Wick |

- **The choice:**
  - **a, drive it.** "It's clear." Wick takes her wagon up the road at first light, ahead of the black cloak.
  - **b, wait.** "Wait a day. They'll be back." Wick waits a day: safer for her, slower for the road.
- **Repeat and limits:** weekly. `join.cap` 3 (a spawn per wolf). Deadline 2 game days.
- **Rewards:**
  - shared: `story.practice watch`; `nh_threat` −2 (both endings fought);
  - a: 12 from `town:coldwater` (P-F); deed "cleared the road below the Pass before the black cloak walked it";
  - b: 10 from `town:coldwater`; deed "cleared the road below the Pass, and kept a Coldwater carter off it a day".
- **Campaign:** feeds `nh_realm` +4 and `nh_voice_frosthollow` +6 (the deed is done on Frosthollow's road). It writes nothing of the Envoy's walk.
- **Dialogue** (Wick, coldwater, broad; playful, anxious, chatty):
  - ask: "Black cloaks on the Pass, eh?"
  - say: "#greet#. Three of them at the Pass House, boots still on. They'll want walking down, and the first stage below Frosthollow is where the Captain's lot sit by the road. Go and look, would you? Clear it if you can, then come and tell me, eh? I've a wagon to drive up there, and I'm fond of my ears."
  - accept: "I'll scout the road below the Pass."
  - decline: "Black cloaks aren't my business."
  - handIn: "I've been up the road."
  - thanks: "#thanks#. Net and line, I'll hum the whole way up now. Badly. You'll hear me from the mere."
  - **npc.offer** `bc_go` at Wick. Words: "It's clear. Drive it." Reply: "Clear! Then I'm up it at first light, singing, ahead of the black cloaks and all. Deep and dark, if they've come back, I'll know who to blame."
  - **npc.offer** `bc_wait` at Wick. Words: "Wait a day. They'll be back." Reply: "A day, then. Muddied water, waiting. But I'd rather wait with my ears than drive without them. I'll go the day after, and you'll come and look again, eh?"
  - **Close a** (P-D, in the window): "Drove it at first light. Not a paw on the road, and a black cloak walking down behind me like a heron. Fair catch, that road."
  - **Close b** (P-D, in the window): "Waited the day, like you said. Glad I did; there was smoke by the road that night. I went the day after, and it was gone."
  - **Journal notes** (today-only): a "The Coldwater carter drove the road below the Pass at first light." b "The Coldwater carter waited a day before the road below the Pass."
- **Gossip and helpful knowledge:**
  - `[Coldwater · coldwater · resident.gossip · world.mem.st_cloak=open]` "Black cloaks at the Pass House, eh? The road below Frosthollow wants looking at before they walk it."
  - (H) `[Frosthollow · frosthollow · resident.news · world.mem.st_cloak=open]` "The first stage below the Pass is where the Captain's men sit by the road, stone. Go in a pair."
  - `[Frosthollow · frosthollow · resident.gossip · world.mem.st_cloak=cleared]` "Somebody cleared the road below the Pass. The black cloak walked down it like it was Restday, eh."
- **Engine needs:** J1, C8, C5; L2.

### S3. `st-relief-convoy`: Flour for Larkfield
- **Tier:** medium. **Program:** `nh-pack.stage`. **Phases:** `["s78"]` (Ch 7's siege on the Last Sheaf, and Ch 8 while Larkfield is still short).
- **Places:**
  - the Larkholm Granary `fs_grain_lark_granary` (door `w_ridgemere_8_11` (108, 45)), where the giver works;
  - region `rc_road` on `w_ridgemere_7_10` (The Greenholt, South), tiles 0–40 × 160–200: the road north to Larkfield (road tiles from (0, 168) to (24, 192));
  - region `rc_downs` on `w_ridgemere_8_10` (The Larkdowns, South-west), tiles 100–140 × 100–140: the open downs, off any road;
  - the Larkfield Bakehouse `cn_larkfield_bakehouse` (door `w_ridgemere_6_9` (133, 56)).
  - The Downs Road in `7_11` is one of Ch 7's three siege regions, so this quest neither spawns nor counts anything there.
- **Giver:** Myrtle Larkcombe `cty_myrtle_larkcombe`. She is female, 54, a farmhand at Larkholm (the Larkholm Granary), living at Southmeadow Cottage 1 `cn_southmeadow_cottage_1`. She speaks **southmeadow** with a **broad** accent. She is blunt and tired, quiet and frank, counts things under her breath, and is "saving coin toward something no one else knows about". The cast's other Myrtle lives in Coldwater, and no line names either (decisions §1.2.5).
- **The baker's shop hand** (a gift target only): Rowan Chalkley `tn_lf_rowan_chalkley`, female, 53, shop hand at the Larkfield Bakehouse; plain (larkfield, no accent).
- **Premise:** Larkfield's ovens are short. The siege has the Downs Road and the fields. Larkholm has flour in the granary and nobody to carry it. Myrtle counts out two measures and two loaves and wants them at the Larkfield Bakehouse: up the road, quick and watched, or the long way over the downs.
- **Setup:** on `quest accepted`, `item.give flour 2` and `item.give bread 2`.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `way` | Take the road north, or go round over the downs. | `story signal` `rc_w` | The first region the wolf enters decides: `rc_road` sets `rc_pick = road` and P-B spawns the wave (any hour); `rc_downs` sets `rc_pick = downs`. |
| 2r | `fight` (hidden; road) | Get the sacks through the wolves on the road. | `bandit falls` `cell: w_ridgemere_7_10`, goal 2 | |
| 3 | `ovens` | Give the flour to the Larkfield Bakehouse. | `gift` `target: tn_lf_rowan_chalkley` `item: flour`, goal 2, quantity | after 2r on the road |
| — | `loaves` (optional) | Give her the loaves too. | `gift again` `target: tn_lf_rowan_chalkley` `item: bread`, goal 2, quantity | `bonus`: deed "brought Larkholm's last loaves to Larkfield's ovens". (A second gift on the same day to the same resident is a `gift again`.) |

- **The choice is made by the way the wolf goes:** the road (faster, a fight) or the downs (longer, unwatched).
- **Hand-in:** `handIn: false`; the quest ends at the Bakehouse.
- **Repeat and limits:** weekly. `join.cap` 3 (a spawn per wolf on the road). Deadline 1 game day.
- **Rewards:**
  - shared: `story.practice labour`;
  - road: 12 from `town:larkfield` (P-F), "Larkfield's ovens pay what they can"; deed "fought a relief load up the road to Larkfield's ovens"; `nh_threat` −2;
  - downs: 8 from `town:larkfield`; deed "carried Larkholm flour to Larkfield the long way over the downs".
- **Campaign:** feeds `nh_realm` +4 and `nh_voice_larkfield` +6. It does not touch Ch 7's relief rules, which only the chapter writes.
- **Dialogue** (Myrtle, southmeadow, broad; blunt, tired):
  - ask: "Larkfield short?"
  - say: "#greet#. Short as a sprout. The siege has the Downs Road and their fields both. Two measures of flour and two loaves, counted twice. Get them to the Larkfield Bakehouse. Up the road's quick and watched. Round over the downs is long, and nobody's there. Your choice, full sure. Only get there."
  - accept: "Two measures and two loaves, to Larkfield."
  - decline: "Not through a siege."
  - handIn: (none; `handIn: false`)
  - during (C6), duringAsk "About the flour…": "The Bakehouse in Larkfield, the shop hand at the counter. Road or downs, I don't mind which. I mind if it's late."
  - **Close road** (P-D, in the window): "Up the road, with teeth? Sheaf and sickle. Larkfield's ovens are lit. I counted the loaves back in my head all night."
  - **Close downs** (P-D, in the window): "The long way, and every measure there. Ripe thinking. The downs don't ask you for a toll, eh."
  - **Journal notes** (today-only): road "Larkholm's flour reached Larkfield up the road." downs "Larkholm's flour reached Larkfield over the downs."
- **Gossip and helpful knowledge:**
  - `[Southmeadow · southmeadow · resident.gossip · world.mem.st_relief=open]` "Larkholm's got flour and Larkfield's got none. Somebody's got to carry it, siege or no siege, eh."
  - (H) `[Southmeadow · southmeadow · resident.news · world.mem.st_relief=open]` "If the road north's watched, go round over the downs. Long, but nobody sits up there."
  - `[Larkfield · larkfield · resident.gossip · world.mem.st_relief=road|downs]` "Larkholm flour at the Bakehouse, carried in by paw. Full weight, every measure. By the market cross, it's welcome."
- **Engine needs:** C8, C6, C5; L2.

### S4. `st-ice-watch`: A Night on the Ice
- **Tier:** medium. **Program:** `nh-pack.stage`. **Phases:** `["s78"]`, and only once the Upper Mere is frozen (Ch 8: "the mere freezes when winter begins").
- **The freeze gate.** The program's world switch `st_ice_frozen` is set by a trigger on `season begins` (winter) while `nh_stage = 8`, or by the DM when the campaign forces snow on Coldwater (`weather.set snow`, campaign §3.2). With C6 the offer has `requires: {"test":"switch","var":"st_ice_frozen","is":true}`. Until then, the quest is on offer through `s78`, and its first objective waits for the freeze (below), with no deadline before it.
- **Not Ch 8's bearings.** The pack writes no bearing (`nh_bearings` is the chapter's). A wolf who listens here may also take a bearing for the chapter, which the chapter counts by itself.
- **Places:**
  - the Upper Mere Boat Shed `fs_fishing_uppermere_shed` (door `w_ridgemere_9_6` (99, 39));
  - region `iw_shore` on `w_ridgemere_9_6` (The Greenholt, the Upper Mere), tiles 100–113 × 46–58: the shingle between the Boat Shed and the ice, at the foot of the jetty;
  - the Larkdowns Shearing Barn `fs_sheep_downs_barn` (door `w_ridgemere_9_8` (223, 105)), where the giver works.
- **Giver:** Garth Barleyton `cty_garth_barleyton`. He is male, 25, a shepherd at Larkdowns, living at Coldwater Cottage 3 `cn_coldwater_cottage_3`. He speaks **coldwater** with a light accent. He is tired and wry, quiet and dry, hums old marching songs, and "came along the road as a young wolf with nothing, and is quietly proud of what was built since".
- **Premise:** Since the mere froze, the Larkdowns ewes stand up at night and stare at the ice, and Garth swears it rings under them, low, like a bell. He can't leave the flock. Sit a night on the shore of the Upper Mere by the Boat Shed, listen, and come and tell him what it was. Then choose what he believes.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `frozen` | Wait for the Upper Mere to freeze. | `story signal` `iw_f` | An hourly trigger emits it to every participant once `st_ice_frozen` is on; on `quest accepted`, it completes at once if the switch is already on. |
| 2 | `sit` | Sit on the shore by the Boat Shed, after dark. | `story signal` `iw_s` | P-A on `iw_shore`, hours 22–4; it starts the player timer `iw_t`. |
| 3 | `night` | Stay till the small hours. | `story signal` `iw_n` | an hourly trigger: `each over participants` with `wolf in region iw_shore` and `timer iw_t` at least 0.1 game days (about 2½ game hours) emits it |
| 4 | `choose` | Tell the shepherd what you heard. | `story signal` `iw_chose` | P-C at Garth |

- **The choice:**
  - **a, the bell.** "It rang. There's a bell down there." Garth believes it, and takes his rope down to the Boat Shed (talk only; he is not one of Ch 8's seven haulers).
  - **b, the ice.** "It's only the ice settling. Sleep." Kind, or true; Garth sleeps, and the ewes settle.
- **Repeat and limits:** weekly. `join.cap` 6. No deadline: nothing can be done before the freeze, and the steps after it take one night. `abandon: true`.
- **Rewards:**
  - shared: `story.practice watch`;
  - a: 8 from `town:coldwater` (P-F); deed "sat a night on the Upper Mere shore and heard the bell";
  - b: 8 from `town:coldwater`; deed "sat a night on the Upper Mere shore for a shepherd who couldn't".
- **Campaign:** feeds `nh_realm` +4 and `nh_voice_coldwater` +6. **Reads** `nh_b_bell`: once it is `rung` or `cracked`, close a is the variant below.
- **Dialogue** (Garth, coldwater, light; wry, tired):
  - ask: "Ewes restless?"
  - say: "#greet#. Since the mere froze. They stand up at night and stare at the ice, eh, and I'd swear it rings under them, low, like a bell. I can't leave the flock. Sit a night on the shore by the Boat Shed and listen for me? Then tell me what it was. Truly, or kindly. I'll know which."
  - accept: "I'll sit a night by the Boat Shed."
  - decline: "Too cold for me."
  - handIn: "I sat the night."
  - thanks: "#thanks#. Waterlogged, you look. Here's a seat by the fold. They've settled since you came, the daft things."
  - during (C6), duringAsk "About the ice…": "Wait for the freeze. Then the shore by the Boat Shed, late, and keep still. Take something warm."
  - **npc.offer** `iw_bell` at Garth. Words: "It rang. There's a bell down there." Reply: "Deep and dark. I knew it. I'll take my rope down to the Boat Shed, then. If they're hauling, a shepherd can pull as well as anyone, eh?"
  - **npc.offer** `iw_ice` at Garth. Words: "It's only the ice settling. Sleep." Reply: "The ice. Aye. Well. I'll sleep, and so will they. I'll not ask if you're being kind. Calm water."
  - **Close a** (P-D, in the window): "Took my rope down to the Boat Shed. They let me pull on the end, eh. A shepherd on a bell rope. My ewes won't believe it."
  - **Close a, once the bell is raised:** "They raised it, you know. It rang for my ewes first. Fair catch, that."
  - **Close b** (P-D, in the window): "Slept like a stone, and the ewes too. If it was a kind lie, it was a fair one."
  - **Journal notes** (today-only): a "The shepherd took his rope down to the Boat Shed." b "The shepherd and his ewes slept the night."
- **Gossip and helpful knowledge** (Coldwater, coldwater):
  - `[resident.gossip · world.mem.st_ice=open]` "The Larkdowns ewes stare at the mere all night, eh? Something rings under that ice."
  - (H) `[resident.news · world.mem.st_ice=open]` "If you sit the ice, sit it by the Boat Shed, late, and keep still. It rings in the small hours."
  - `[resident.gossip · world.mem.st_ice=bell]` "A shepherd took his rope to the Boat Shed. Says a wolf sat the ice and heard the bell. Net and line."
- **Engine needs:** C6 (the offer gate), J1, C8, C5.

### S5. `st-bell-road`: The Bell's Felt
- **Tier:** medium. **Program:** `nh-pack.stage`. **Phases:** `["s9"]` (until the Moot night is called: withdrawn when `nh_moot` is set, decisions §6.4), offered only while `nh_b_bell` is `rung` or `cracked` (C6 `requires`; until then a trigger on `quest accepted` fails the copy at once with the journal note "The bell is still under the ice. There is no road for it.").
- **Not the campaign's escort.** Ch 9's "bell's road" is a story-scope escort in stages, which the campaign runs. This quest keeps **one stage** of it the night before the cart stands there, and writes nothing of the escort.
- **Places:**
  - the Coldwater Loom Shed `cn_coldwater_weavers` (door `w_ridgemere_7_6` (205, 226)), where the giver works;
  - region `br_stage` on `w_ridgemere_6_6` (The Greenholt, Middle), tiles 200–240 × 40–72: the road west from Coldwater (road tiles from (208, 40) to (248, 72)), the stage after the Coldwater square on the road to the city. If the campaign's escort names a different second stage, this region moves to match it (campaign reviser's list).
- **Giver:** Jory Rimepeak `tn_fh_jory_rimepeak`. He is male, 54, a loom hand at Coldwater Looms (the Coldwater Loom Shed), living at Coldwater Cottage 6 `cn_coldwater_cottage_6`. He speaks **coldwater** with a light accent. He is honest to a fault and quietly funny, quiet, dry and frank, "remembers every name ever heard", and "means to go down to Ridgemere and never come back up". The campaign's Fenshore fallback Keeper is another Jory, and no line names either (decisions §1.2.5).
- **Premise:** The bell goes down to the city for the Moot, wrapped in Coldwater felt, and Jory wove it. He wants the last of the felt laid on the stage west of Coldwater where the bell's cart will stand the night, and that stage kept till dawn. And he has a question he has meant to ask somebody for thirty years.
- **Setup:** on `quest accepted`, `item.give felt 2`.
- **Steps:**

| # | id | Journal line | Event and match | Notes |
|---|---|---|---|---|
| 1 | `stage` | Go to the stage west of Coldwater on the bell's road, after dark. | `story signal` `br_s` | P-A on `br_stage`, hours 20–4. If `nh_threat` > 50, P-B spawns a wave there and reveals `fight`. |
| — | `fight` (hidden) | Drive off whoever wants the stage. | `bandit falls` `cell: w_ridgemere_6_6`, goal 2 | revealed only with a spawn |
| 2 | `felt` | Lay the felt where the cart will stand. | `item dropped` `item: felt` `cell: w_ridgemere_6_6`, goal 2, quantity | |
| 3 | `keep` | Keep the stage till dawn. | `story signal` `br_k` | on `dawn`, `each over participants` with `wolf in region br_stage` and `fight` done or hidden emits it |
| 4 | `choose` | He asks whether he should go with it. | `story signal` `br_chose` | P-C at Jory |

- **The choice:**
  - **a, go.** "Go with it. The city has room for a Coldwater wolf." Jory means to walk beside the cart on the Moot night (talk only; the pack moves nobody).
  - **b, stay.** "Coldwater needs you on the night." Jory stays and howls from the Coldwater square.
- **Repeat and limits:** weekly (so two or three times in `s9`). `join.cap` 3 (a spawn per wolf). Deadline 2 game days.
- **Rewards:**
  - shared: `story.practice watch`; 10 from `town:coldwater` (P-F); `nh_threat` −2 if there was a fight;
  - a: deed "kept the bell's stage west of Coldwater, and sent its weaver to the city with it";
  - b: deed "kept the bell's stage west of Coldwater, and kept its weaver home for the Moot night".
- **Campaign:** feeds `nh_realm` +4 and `nh_voice_coldwater` +6.
- **Dialogue** (Jory, coldwater, light; honest, quietly funny):
  - ask: "Is that the bell's felt?"
  - say: "#greet#. Every ell of it, and I know the name of every ewe it came off. The bell's cart stands a night on the road west of here. Lay this last of the felt where it'll stand, and keep the stage till dawn? There's those who'd like it never to reach the city. And I've a question for after."
  - accept: "I'll lay the felt and keep the stage."
  - decline: "Bells aren't my trade."
  - handIn: "The stage is kept. Ask your question."
  - thanks: "#thanks#. A full net, that. I'll tell the felt it was well laid. It won't answer. Felt never does."
  - **npc.offer** `br_go` at Jory. Words: "Go with it. The city has room for a Coldwater wolf." Reply: "Thirty years I've meant to go down to Ridgemere and never come back. #oath#. I'll walk beside the cart, then, and if they ask who I am, I'll say I wove its coat."
  - **npc.offer** `br_stay` at Jory. Words: "Coldwater needs you on the night." Reply: "Needs me? Well, I'll be netted. Then I'll howl from our own square, and the bell can go without me. It knows the way. I wrapped it."
  - **Close a** (P-D, in the window): "I'm walking down with it on the night. Packed one bag. Took it out again. Packed it. It's by the door."
  - **Close b** (P-D, in the window): "Staying. Somebody has to remember the names here. I'll howl from our square, and loud."
  - **Journal notes** (today-only): a "The old weaver means to walk down to the city with the bell." b "The old weaver will howl from the Coldwater square."
- **Gossip and helpful knowledge** (Coldwater, coldwater):
  - `[resident.gossip · world.mem.st_bell=open]` "The bell's going down to the city in our felt, eh? Every ell woven here."
  - (H) `[resident.news · world.mem.st_bell=open]` "The bell's cart stands a night west of here on the road. Somebody should keep that stage till dawn."
  - `[resident.gossip · world.mem.st_bell=go]` "The old weaver's walking down to the city with the bell. Thirty years he's said it. Fair catch."
  - `[resident.gossip · world.mem.st_bell=stay]` "The old weaver's staying for the Moot night. Says somebody has to remember the names."
- **Engine needs:** C6 (the offer gate), J1, C8, C5; L2.

---

## 5. Engine needs beyond today

Ids and phases are the catalogue's (`engine-spec.md` §1), with the names of decisions §5.1. The medium packs need **Phase 1, plus Phase 4 for C12a, C6 and N1** (decisions §5.2); everything else below improves a quest that already runs without it.

### 5.1 The items this pack uses

| Id | What | Phase | Quests | Without it |
|---|---|---|---|---|
| **J1** | `npc.offer` keeps a `players` list | 1 | every P-C choice | The last wolf to reach a choice sees it; the `quest is active` guard prevents wrong picks. |
| **N2** | window fact `player.quest.<program>/<quest>` | 1 | the targets' own lines: Q1 Piet, Q5 the crier, Q7 the pup, Q11 the baker, Q23 the skipper | The line is gated on the quest's "open" talk key, so every wolf may hear it while the key is set. |
| **C8** | `talk.remember` / `talk.forget` (town scope, and **pair scope** for the closes) | 1 | all 28: every gossip and helpful line; every close (P-D rung 2); the "it's taken" keys | The DM seeds the `open` keys by hand. Ending keys are lost, but deeds still reach gossip; closes are journal notes. |
| **SQ2** | till purses `till:job:<id>` (was M6) | 1 | Q6 only (the cooper's own till) | the treasury pays, with the shop named in the reason |
| **C11** | `npc.spare` (old-age hold) | 1 | Q19 (Tamsin Eelbank, 70); Q1 (Piet Lumley, 60) | `resident is alive` guards with fallbacks (written in) |
| **L2** | `camp: <name>` on `story.area.spawn` / `bandits.call` (was M4) | 2 | every P-B fight | `cell` plus the `spawned_<region>` switch (P-B) |
| **L3** | `camp cleared`, `caravan arrives` … as mission events | 2 | S1's arrival; any "break the camp" | a trigger plus `objective.complete` |
| **C12b** | `match` / `field` with `starts` / `in` | 2 | S1's two arrival triggers; Q18's `lq_salt` gate | twin triggers; a compare per value |
| **N4** | quest `join.again: <days>` | 2 | Q4, Q12, Q17, Q18, Q20 (`join.again: 7`) | §0.3's weekly repeat, one-day deadline and "it's taken" line |
| **M2** | operand `{"purse": "<id>"}` | 2 | P-F could scale a reward to the purse | fixed amounts and the §3 counter rule |
| **M5** | `cell` on `trade done` | 2 | Q22 ("sold in the city") | any sale counts |
| **C2** | operand `audience`; arithmetic | 2 | P-E steps (3, 10, 30) | fixed steps |
| **J4** | `calendar is moon` | 2 | Q6's new-moon deadline | `afterDays: 3` |
| **C5** | `npc.greet` | 3 | every close (P-D rung 3) | the pair-gated tree line (rung 2), or the journal note (rung 1) |
| **C6** | quest `requires`; `offer.during` | 4 | every `during` line; Q18's and S4's and S5's offer gates | the journal line carries the hint; the gates fall back as each quest says |
| **C12a** | `story.notify` to the running chapter | 4 | Q18's paid toll (optional); every quest's `p_voice` credit | packs credit towns and the realm through world variables, not the wolf's own tracker |
| **N1** | choices overflow into "Ask about…" | 4 | none needed: each giver holds one quest | — |
| J3 | `closure` | 4 | none (answer (b): medium quests are weekly and not story lines) | Q3, Q10, Q16 and Q22 end with a journal note inviting a social Story |
| **L1** | `hunt.stage {region, species, count, minutes}` (replaces M1) | 5 | Q13 (the bear in the garth) | the kill path is offered only after a bear is hunted in the cell; the honey path always |
| **J9** | tagged story items | 6 | Q2 (the tally), Q15 (the float lamp), Q19 (Kite's list) | generic `paper` and `oil_lamp`, which any wolf could hand in |
| **C10** | `foe.add` (a named leader in a wave) | 7 | optional: Q2, Q8, Q12, Q18, Q20 | anonymous bandits only |
| **J2** | realm-player variables | 8 | Q22's list; `p_voice` credit | world `wolves` lists (≤ 500) |
| **C14** | `npc.post` | 8 | Q1 (half the round back to Piet), Q16 (a shared wheel) | talk only |
| **M3** | `house:<faction>` purses | 9, deferred | Q3's "Grayrock pays", Q10's "Fell advance" | the treasury and the Coldwater store pay |

- **Dropped as this pack's own items:** M1 (`wild.spawn`; decisions §5.1: L1 `hunt.stage` replaces it), M4 (merged into L2, with L2's naming rule `[a-z0-9_]{3,30}` and the program's prefix, here `mq_`), M6 (the same fix as SQ2).

### 5.2 What works today, with no engine change
Every quest above runs on today's engine (the today-only version of decisions §6.2), with these losses:
- talk keys must be seeded by the DM (C8), and closes are journal notes (P-D rung 1);
- the per-wolf choice offers overwrite each other (J1);
- targets' own lines are gated on the "open" key, not on the wolf's quest (N2);
- the bear quest (Q13) has the honey path only, unless a bear is hunted there anyway (L1);
- fights are matched by `cell` and the `spawned_<region>` switch (L2);
- House-paid coin comes from the towns (M3), and the cooper's coin from the treasury (SQ2);
- Q18, S4 and S5 run their fallback gates instead of `requires` (C6).

Every objective event, condition, effect, resident, place, region tile, item and purse named above exists on Ridgemere 3 now.

---

## 6. Index

| # | id | Title | Giver | Town |
|---|---|---|---|---|
| Q1 | `mq-sump-lamps` | The Dark Round | Edda Gullford `rm_edda_gullford` | Ridgemere (Sump) |
| Q2 | `mq-cinder-gate-tally` | The Grey Paw at the Cinder Gate | Palle Plankmore `rm_palle_plankmore` | Ridgemere (Cinders) |
| Q3 | `mq-wharf-brother` | What the Wharf Owes | Leif Fogcot `rm_leif_fogcot` | Ridgemere (Wharf) |
| Q4 | `mq-sunken-street` | Down the Sunken Street | Theodora Bilgeridge `rm_theodora_bilgeridge` | Ridgemere (Commons, Sump) |
| Q5 | `mq-northgate-smoke` | Smoke over Northgate | Sten Ropeford `rm_sten_ropeford` | Ridgemere (Northgate) |
| Q6 | `mq-coopers-note` | The Cooper's Note | Edric Millton `rm_edric_millton` | Ridgemere (Commons) |
| Q7 | `mq-court-larder` | The Short Larder | Vidar Ashhand `rm_vidar_ashhand` | Ridgemere (Timberland Court) |
| Q8 | `mq-glasswater-nets` | Cut Nets at Glasswater | Grist Reedmere `tn_cw_grist_reedmere` | Coldwater (Glasswater) |
| Q9 | `mq-meresend-blight` | Black Leaf at Meresend | Pell Harvestley `tn_sm_pell_harvestley` | Coldwater (Meresend) |
| Q10 | `mq-coldwater-still` | A Cask for the Fell Factor | Linden Harvestton `tn_sm_linden_harvestton` | Coldwater, then Ridgemere or Larkfield |
| Q11 | `mq-linen-bolt` | A Bolt Gone Missing | Heather Reedwater `tn_mb_heather_reedwater` | Larkfield (the Looms) |
| Q12 | `mq-mud-to-the-axles` | Mud to the Axles | Imogen Rustford `cty_imogen_rustford` | Larkfield (Longmead Mill) |
| Q13 | `mq-bee-garth-bear` | The Bear in the Bee Garth | Disa Fogford `cty_disa_fogford` | Southmeadow (Sunfields) |
| Q14 | `mq-vineyard-cellar` | Casks Gone Walking | Harl Oakbough `tn_oh_harl_oakbough` | Southmeadow (vineyard) |
| Q15 | `mq-withy-lights` | Lights over Withy Creek | Iris Pikeford `tn_mb_iris_pikeford` | Fenshore (Withy Creek) |
| Q16 | `mq-potters-place` | The Potter's Place | Bram Harvestgarth `tn_sm_bram_harvestgarth` | Fenshore |
| Q17 | `mq-lost-raker` | Lost in the Salt Wind | Fern Merewick `cty_fern_merewick` (offers at Ridge Eelbank `tn_mb_ridge_eelbank`) | Saltwell (Saltpan) |
| Q18 | `mq-waste-road-salt` | Salt on the Waste Road | Tor Chalkfield `cty_tor_chalkfield` | Saltwell's road to Southmeadow |
| Q19 | `mq-crossroads-names` | Names for the Crossroads | Kite Bogditch `tn_mb_kite_bogditch` | Saltwell, then the Mirebridge crossroads |
| Q20 | `mq-windthrown-cedar` | Windthrown on the Sound Road | Tristram Rainridge `cty_tristram_rainridge` | Oakhaven |
| Q21 | `mq-low-drift-props` | Rotten Props in the Low Drift | Ebba Coalgate `cty_ebba_coalgate` | Frosthollow (iron mine) |
| Q22 | `mq-lamp-oil-share` | Oil for the Long Dark | Stone Acorndell `cty_stone_acorndell` | Frosthollow (Upper hearths) |
| Q23 | `mq-logs-adrift` | Logs Adrift on the Sound | Oda Cinderdale `cty_oda_cinderdale` | Oakhaven (the cut; Cedar Sound) |
| S1 | `st-greenholt-wagon` | The Coldwater Wagon | Aud Saltridge `rmx_aud_saltridge` | Ridgemere to Coldwater (`s3`) |
| S2 | `st-black-cloak-road` | Before the Black Cloak | Wick Marshwade `cty_wick_marshwade` | Coldwater; below Frosthollow (`s56`) |
| S3 | `st-relief-convoy` | Flour for Larkfield | Myrtle Larkcombe `cty_myrtle_larkcombe` | Larkholm to Larkfield (`s78`) |
| S4 | `st-ice-watch` | A Night on the Ice | Garth Barleyton `cty_garth_barleyton` | the Upper Mere (`s78`, frozen) |
| S5 | `st-bell-road` | The Bell's Felt | Jory Rimepeak `tn_fh_jory_rimepeak` | Coldwater, the road west (`s9`) |

**Registry ids.** `givers.tsv` now carries the real ids of decisions §7.2's placeholders: `mq-northgate-smoke`, `mq-court-larder`, `mq-linen-bolt` and `mq-logs-adrift` (renamed in the re-check, 2026-11-22).

**Givers.** Every giver above is the one `givers.tsv` names for that quest, placed and alive, with a reachable home and work place, and none holds a second pack quest (decisions §1.2.1). Re-run on 2026-11-22 against the latest checkpoint: the reach query of decisions §1 for all 28 givers and every target below, and the §1.2.7 giver check on `givers.tsv` (it passes).

**Targets and others named** (all placed and alive on 2026-11-22; a target holds no hub item):
- **Ridgemere:** Piet Lumley and Jorunn Ropewick (Q1; campaign cast, allowed, decisions §1.2.2), Magda Rustham and Rupert Greyley (Q5), Hilde Bilgegate, Arne Rainworth and Mab Fogham (Q7), Jasper Bilgeford (Q2, Q4; the Crown's customs collector, as in campaign Ch 2), Grete Dunn, Kari Bilgehand, Lavinia Plankwick and Solveig Tarwell (Q3; the Grayrock steward and the Chapel priest, as in campaign Ch 5–6), Desmond Bilgeridge (Q4; offers), Silas Brinegate, Tove Brinegate, Agnes Wetley (Q4), Rune Gullwell and Harriet Pennock (Q6; Rune is the Counting House clerk, as in campaign Ch 1–3), Rupert Fogton (Q9), Ilse Saltley and Millicent Rustwell (Q10; Millicent is the Fell steward, as in campaign Ch 6), Imogen Nailridge (Q19; she keeps Mirebridge's pledges in the city, as in campaign §5.1), Kell Reedwade (Q23), Rosamund Tarridge (an inn line only).
- **The towns:** Harrow Reedford (Q8; the Glasswater Inn's cook, as in campaign §5.2), Loam Pinefell and Bramble Reedford (Q10; innkeepers as buyers, their one quest each), Eskar Cornley and Ragnar Wetby (Q11), Tansy Eelbridge (Q12, sells axles only), Briar Cornby (Q14; campaign cast, allowed as above), Nab Bogditch (Q15), Ludo Ashwood and Ludo Beechshaw (Q16; Ludo Beechshaw holds the dead potter's post, as in campaign Ch 6), Ridge Eelbank (Q17; offers), Tamsin Eelbank (Q19), Frost Colddrift (Q20), Aster Coldfell (Q21, Q22; the Frosthollow shopkeeper, a Keeper's fallback, as a target only), Thorn Ashwood (Q22; the innkeeper as keeper of the shelf, his one quest), Rowan Chalkley (S3). Fenwick Plankworth is in talk only.
- **Innkeepers:** Nell Plummeadow is no longer a target (her one use is campaign's letter scene); Q16 asks the Heron's pot hand instead and keeps her helpful line. Thorn Ashwood is a target in Q22 only (Q21 no longer talks to him).
- **Names shared:** Theodora Bilgeridge (Q4) and Theodora Wickley (`lq-runnel-fever`), the two Ludos of Q16, Oda Cinderdale and the campaign's other Oda, Jory Rimepeak and Jory Corncroft, Myrtle Larkcombe and Myrtle Stillford: no library line names any of them.

---
## Appendix A. Q1 as a program sketch (the shape every quest follows)
This is trimmed to one quest, in its **today-only** form (decisions §6.2): closes as journal notes (P-D rung 1), coin through P-F, choices at the giver. The P-E counters and two of the three lamps' triggers are left out. It follows `Docs/story-programs.md` v2 and the catalogue; `problems()` and `review()` are re-run on the full program before approval (decisions §9, P2-24).

```json
{"format":"ratw-story-program","version":2,"id":"nh-pack.medium.city","title":"Medium errands of Ridgemere",
 "audience":{"kind":"open","cap":5000},"start":"open",
 "variables":[
  {"id":"nh_stage","type":"number","scope":"world"},
  {"id":"nh_realm","type":"number","scope":"world"},
  {"id":"nh_voice_ridgemere","type":"number","scope":"world"},
  {"id":"coin_ridgemere_day","type":"number","scope":"world"},
  {"id":"coin_ridgemere_ok","type":"switch","scope":"world"},
  {"id":"sl_pick","type":"text","scope":"player"}],
 "regions":[
  {"id":"sl_netcourt","cell":"w_ridgemere_1_1","x1":20,"y1":56,"x2":33,"y2":61},
  {"id":"sl_pawn","cell":"w_ridgemere_1_1","x1":18,"y1":81,"x2":28,"y2":84},
  {"id":"sl_steps","cell":"w_ridgemere_0_1","x1":198,"y1":64,"x2":209,"y2":71}],
 "phases":[{"id":"open","journal":"Errands about the city.","effects":[],"exits":[],
  "triggers":[
   {"id":"q1-oil","on":"quest accepted","audience":"participant","match":{"detail":"mq-sump-lamps"},
    "actions":[{"do":"effect","effect":{"kind":"item.give","player":"$actor","item":"lamp_oil","quantity":2}}]},
   {"id":"q1-a","on":"region entered","audience":"participant","match":{"target":"sl_netcourt"},
    "if":{"test":"all","conditions":[{"test":"hour between","from":19,"to":5},
      {"test":"objective is","quest":"mq-sump-lamps","objective":"round_a","of":"$actor","status":"to do"}]},
    "actions":[{"do":"emit","signal":"sl_a"}]},
   {"id":"q1-reveal","on":"objective completed","audience":"participant","match":{"detail":"mq-sump-lamps","item":"round_c"},
    "if":{"test":"all","conditions":[
      {"test":"objective is","quest":"mq-sump-lamps","objective":"round_a","of":"$actor","status":"done"},
      {"test":"objective is","quest":"mq-sump-lamps","objective":"round_b","of":"$actor","status":"done"}]},
    "actions":[{"do":"effect","effect":{"kind":"objective.reveal","quest":"mq-sump-lamps","objective":"oil","player":"$actor"}}]},
   {"id":"q1-found","on":"objective completed","audience":"participant","match":{"detail":"mq-sump-lamps","item":"oil"},
    "actions":[
     {"do":"effect","effect":{"kind":"npc.offer","npc":"rm_edda_gullford","id":"sl_tell","player":"$actor","once":true,
      "words":"It's the old lamplighter. He's been taking the oil.",
      "reply":"Him. #oath#, I'd have bet on anyone else. Forty years on that round. I'll take it to the Watch Office myself, quiet, so there's no crowd at his door."}},
     {"do":"effect","effect":{"kind":"npc.offer","npc":"rm_edda_gullford","id":"sl_cover","player":"$actor","once":true,
      "words":"I've stopped it. Don't ask who. I'll pay the oil back, and an old hand will walk half your round.",
      "reply":"Don't ask. That's a lot to ask of me. Fine. Two flasks back in my store, and if your old hand knows the Sump lamps by their smoke, I'll have him. Half the round. Not a lamp more."}}]},
   {"id":"q1-tell","on":"dialogue choice","audience":"participant","match":{"detail":"sl_tell"},
    "if":{"test":"quest is","quest":"mq-sump-lamps","of":"$actor","status":"active"},
    "actions":[{"do":"set","var":"sl_pick","value":"a","of":"$actor"},{"do":"emit","signal":"sl_chose"}]},
   {"id":"q1-cover","on":"dialogue choice","audience":"participant","match":{"detail":"sl_cover"},
    "if":{"test":"quest is","quest":"mq-sump-lamps","of":"$actor","status":"active"},
    "actions":[{"do":"set","var":"sl_pick","value":"b","of":"$actor"},
     {"do":"effect","effect":{"kind":"objective.reveal","quest":"mq-sump-lamps","objective":"repay","player":"$actor"}},
     {"do":"emit","signal":"sl_chose"}]},
   {"id":"q1-end","on":"quest completed","audience":"participant","match":{"detail":"mq-sump-lamps"},
    "actions":[
     {"do":"if","if":{"test":"compare","left":{"var":"sl_pick","of":"$actor"},"op":"=","right":"a"},
      "then":[
       {"do":"if","if":{"test":"all","conditions":[{"test":"switch","var":"coin_ridgemere_ok","is":true},
           {"test":"compare","left":{"var":"coin_ridgemere_day"},"op":"<=","right":125}]},
        "then":[{"do":"effect","effect":{"kind":"character.grant","player":"$actor","coins":10,"purse":"town:ridgemere","reason":"the city pays for a walked lamp round","ifRefused":"skip"}},
                {"do":"add","var":"coin_ridgemere_day","value":10}],
        "else":[{"do":"effect","effect":{"kind":"item.give","player":"$actor","item":"bread","quantity":2}},
                {"do":"effect","effect":{"kind":"journal.note","player":"$actor","text":"The purse is thin today. You were paid in bread."}}]},
       {"do":"effect","effect":{"kind":"deed.award","player":"$actor","detail":"caught the oil-drainer on the Sump lamp round"}},
       {"do":"effect","effect":{"kind":"journal.note","player":"$actor","text":"The lamplighter took it to the Watch Office herself."}}],
      "else":[
       {"do":"if","if":{"test":"all","conditions":[{"test":"switch","var":"coin_ridgemere_ok","is":true},
           {"test":"compare","left":{"var":"coin_ridgemere_day"},"op":"<=","right":129}]},
        "then":[{"do":"effect","effect":{"kind":"character.grant","player":"$actor","coins":6,"purse":"town:ridgemere","reason":"the city pays for a walked lamp round","ifRefused":"skip"}},
                {"do":"add","var":"coin_ridgemere_day","value":6}],
        "else":[{"do":"effect","effect":{"kind":"item.give","player":"$actor","item":"bread","quantity":2}},
                {"do":"effect","effect":{"kind":"journal.note","player":"$actor","text":"The purse is thin today. You were paid in bread."}}]},
       {"do":"effect","effect":{"kind":"journal.note","player":"$actor","text":"An old hand walks half the Sump lamp round again."}}]},
     {"do":"add","var":"nh_realm","value":4},
     {"do":"add","var":"nh_voice_ridgemere","value":4}]}]}],
 "quests":[{"id":"mq-sump-lamps","title":"The Dark Round","summary":"The Sump lamps run dry by midnight. Walk the old round after dusk and find where the oil goes.",
  "giver":"rm_edda_gullford","repeat":"weekly","join":{"cap":6},"fail":[{"afterDays":2}],
  "offer":{"ask":"You look worn down to the wick.",
   "say":"#greet#. Since the old lamplighter lost his round, I walk the Sump end on top of my own. Somebody's draining my lamps dry by midnight. I take nobody's word, so walk it yourself after dusk: Net Court, the Pawn corner, the Knucklebones steps. Two flasks. Keep your nose open.",
   "accept":"I'll walk your round after dark.","decline":"Not tonight.","handIn":"The round's walked. I know where the oil went.",
   "thanks":"#thanks#. A lamp's a small thing until it's out. I'll count them lit tonight, and I'll count you with the honest ones. It's not a long list."},
  "objectives":[
   {"id":"round_a","label":"Light the lamp in the Net Court lanes after dusk.","on":"story signal","signal":"sl_a"},
   {"id":"round_b","label":"Light the lamp at the Pawn and Ledger corner after dusk.","on":"story signal","signal":"sl_b"},
   {"id":"round_c","label":"Light the lamp below the Knucklebones after dusk.","on":"story signal","signal":"sl_c"},
   {"id":"pawn","label":"Ask at the Pawn and Ledger who has been selling lamp oil cheap.","on":"conversation","match":{"target":"rm_jorunn_ropewick"},"optional":true},
   {"id":"oil","label":"Find who has been draining the Sump lamps.","on":"conversation","match":{"target":"rmx_piet_lumley"},"after":["round_a","round_b","round_c"],"hidden":true},
   {"id":"choose","label":"Decide what the lamplighter should hear.","on":"story signal","signal":"sl_chose","after":["oil"]},
   {"id":"repay","label":"Pay the lamplighter back two flasks of oil.","on":"gift","match":{"target":"rm_edda_gullford","item":"lamp_oil"},"goal":2,"measure":"quantity","after":["choose"],"optional":true,"hidden":true}],
  "rewards":[{"kind":"story.practice","player":"$actor","skill":"service"}],
  "bonus":[{"kind":"deed.award","player":"$actor","detail":"kept the Sump lamps lit and an old lamplighter's name clean"}]}],
 "tests":[{"name":"a wolf walks the round and tells the truth","cohort":[],"events":[],"expect":["open"]}]}
```

- **Notes on the sketch:**
  - Ending b's main deed ("walked the Sump lamp round in the dark") is given in the `else` branch when the `repay` objective is not done. The check is left out above for space. The `bonus` is given only when `repay` is done.
  - The full program adds the `sl_b` and `sl_c` night triggers, the P-E counters (`mq_lamps_a`, `mq_lamps_b`) with their board posts, the P-B camp-cleared triggers of the city's fights, and one rehearsal case per ending with real `region entered`, `hour` facts and `dialogue choice` events.
  - **The Phase 1 version** adds, in each ending's branch, `talk.remember {scope: "pair", npc: "rm_edda_gullford", player: "$actor", key: "mq_sump_lamps", value: "a"|"b", days: 7}`, and the town key `mq_lamps=reported|shared`. Edda's closes go in `Data/Talk/rules/story_mq.talk` as `say[self.id=rm_edda_gullford pair.mem.mq_sump_lamps=a priority=3]` and `=b`. **The Phase 3 version** replaces them with `npc.greet` in the same branches.
  - Before approval, the reviser runs every check of decisions §9 (P2-24): `tools/dialect_check.py --world ridgemere3`; `build-core/talk_check --data "$PWD/Data/Talk" --validate` and `--render-trees`; `python3 tools/talk_lint.py`; `story_program.review(p)`, `problems(p) == []` and `story_runner.references(conn, 'upper_accord', p)`; the §1.2.7 giver check; and the reach query for every named resident.
