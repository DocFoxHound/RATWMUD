# Small quests for Ridgemere 3: 48 errands that stand alone, 12 that follow the campaign's stage, and all of them feed The Ninth Howl

Design only. Nothing in the repo or the world was changed. World: Ridgemere 3 (key `ridgemere3`, db `ratw_ridgemere3`, world id `upper_accord`).

**Revised to `decisions.md` and `givers.tsv`** (the coordinator's binding decisions). Where this file and `decisions.md` disagree, `decisions.md` wins. This revision:
- re-casts the givers of quests 7, 23, 25, 30, 36, 38, 39, 40, 41 and 43 per `givers.tsv`, and makes 26 and 29 board-only (29 with a new premise);
- takes the cough out of 17 (it belongs to `lq-runnel-fever` alone);
- replaces the old day caps (P3) with the one purse rule (decisions §3);
- applies the `join.cap: 1` rules (decisions §8.1) to 3, 23, 25 and 30;
- aligns 31's credit with the campaign (decisions §4.4);
- adds the estate errands 46–48 (§2.9a) and the stage pack `nh-pack.stage` with its 12 small entries (§2.10);
- moves every resident's words off `story.tell from` and into the dialogue window (decisions §9, P2-22).

**What was checked, read only, on 2026-11-22** (`tools/world_db.connect('ridgemere3','owner')` inside `SET TRANSACTION READ ONLY`, rolled back):
- Every giver, recipient and pup was checked against `live.npcs`, `live.npc_voice`, `live.npc_state` and the checkpoint's `society.careers.positions`. Each is **alive and placed**, holds the post named (or none, where it says so), and has a **reachable** home and workplace: the home and work area is a cell or an end of a `world.links` row (decisions §1 query).
- Every giver new in this revision (the re-casts, 46–48 and the stage givers) and every new target was checked the same way on the day; the facts quoted for them (sex, age, post, home, work, dialect, personality) are the rows read.
- No giver is in the campaign cast, and no innkeeper gives or holds anything here. Campaign residents appear only as **gift recipients**, and only where the campaign's facts agree: Cairn Oakhaven (Oakhaven's storekeeper, 16) and Thistle Stonepick (Saltwell's storekeeper, 43). A gift takes no hub slot.
- The giver check of decisions §1.2.7 passes on `givers.tsv` (244 residents).
- Every item id is in `Data/Items/items.json`, and every species id is in `Data/Wild/animals.json`. Where a quest needs a good bought, the checkpoint's holdings are quoted; where nobody holds it (timber, honey, wool cloth, salt, waterskins), the quest hands it to the wolf on accept.
- Forage goods and their seasons are from `Data/Wild/forage.json`. Every event, with its fields and its `mission` flag, is from `Data/Storylines/trigger-catalogue.json`.
- The purses and their day caps are decisions §3.1 (checkpoint 2026-11-22 22:34 UTC):
  - `town:ridgemere` is the treasury, 1,355 pennies, day cap 135 (`Core/RatwRoads.cpp:236`: the first town's store is the treasury);
  - `town:coldwater` 212 (cap 21), `town:fenshore` 213 (21), `town:frosthollow` 247 (24), `town:larkfield` 248 (24), `town:oakhaven` 247 (24), `town:saltwell` 224 (22), `town:southmeadow` 236 (23).
- Dialect is the giver's **upbringing** (`npc_voice.home_region`), not where they live. Fenshore-raised wolves speak mirebridge, unless their accent is `none`. A giver with accent `none` speaks plain words, so their lines are written plain.

**Contents:**
- §0 Index
- §1 How the packs are built: programs, shared triggers, the purse rule, scaling, rewards, campaign feed, talk keys
- §2 The quests: Ridgemere city (1–17), the wild near the city (18–19), Coldwater and the Greyteeth (20–25), Oakhaven (26–28), Frosthollow (29–31), Larkfield (32–35), Southmeadow (36–38), Fenshore (39–42), Saltwell (43–45), the estates (46–48), and the stage pack `nh-pack.stage` (§2.10, S1–S12)
- §3 Engine needs: what works today, the `gaps.md` items used, and the new items SQ1–SQ5 this pack found
- §4 Giver budget and checks before approval

---

## 0. Index

| # | id | Title | Giver (id) | Town / place |
|---|---|---|---|---|
| 1 | `sq-morning-bread` | Morning Bread for the Sump | Tove Brinegate `rm_tove_brinegate` | Ridgemere: Northgate → the Sump |
| 2 | `sq-mending-herbs` | Herbs for the House of Mending | Frederick Nailgate `rm_frederick_nailgate` | Ridgemere: the abbey's House of Mending |
| 3 | `sq-gone-to-see-the-ships` | Gone to See the Ships (lost pup) | Soren Ropewick `rm_soren_ropewick` | Ridgemere: the Sump → the piers |
| 4 | `sq-smokehouse-wood` | Oak Smoke for the Herring | Rosamund Coalford `rm_rosamund_coalford` | Ridgemere: North Wharf |
| 5 | `sq-ropewalk-resin` | Resin for the Tar Pot | Kari Bilgehand `rm_kari_bilgehand` | Ridgemere: the Ropewalk (works) |
| 6 | `sq-records-copying` | A Page and a Drawer | Harriet Platt `rmx_harriet_platt` | Ridgemere: Records Office (office) |
| 7 | `sq-heights-round` | The Heights Round (letter) | Gwen Cinderdale `rm_gwen_cinderdale` | Ridgemere: City Office → the Heights |
| 8 | `sq-restday-candles` | Candles for Restday | Berit Coalworth `rm_berit_coalworth` | Ridgemere: the Cathedral |
| 9 | `sq-almoners-round` | The Almoner's Round | Disa Raincot `rm_disa_raincot` | Ridgemere: Chapter House → the Sump |
| 10 | `sq-marketday-stock` | Marketday Stock (stall) | Thyra Plankridge `rm_thyra_plankridge` | Ridgemere: Market Hall, Market Square (commons) |
| 11 | `sq-rag-market-mushrooms` | Greens for the Rag Market (stall) | Agnes Millford `rm_agnes_millford` | Ridgemere: the Rag Market (sump) |
| 12 | `sq-nailers-charcoal` | Charcoal for the Nail-Hearths | Wynn Sootworth `rm_wynn_sootworth` | Ridgemere: Nailers' Forge, the Cinders (works) |
| 13 | `sq-greatly-stables` | Oats for the Factors' Horses | Philippa Rustton `rm_philippa_rustton` | Ridgemere: Greatly stables (estate) |
| 14 | `sq-dawn-bell` | The Dawn Bell | Corwin Bilgestack `rm_corwin_bilgestack` | Ridgemere: Lamp Store → Clock Tower |
| 15 | `sq-mooring-lines` | Take Her Lines (wharf chore) | Wystan Coalmarsh `rm_wystan_coalmarsh` | Ridgemere: the piers (wharf) |
| 16 | `sq-oakhaven-load` | A Crate Light (carrying for a caravan) | Petra Plankhand `rm_petra_plankhand` | Ridgemere: Carters' Yard → Oakhaven |
| 17 | `sq-heights-thyme` | Willow Bark for the Heights' Joints | Egil Grantham `rm_egil_grantham` | Ridgemere: the Heights |
| 18 | `sq-rainward-pheasants` | Pheasants for the Cinder Butchers | Corwin Sootstack `rm_corwin_sootstack` | wild: Rainward Woods (for Ridgemere) |
| 19 | `sq-sounding-the-tarns` | Sounding the Tarns | Yrsa Nailridge `rm_yrsa_nailridge` | wild: Rainmere, Slate Tarn (for Ridgemere) |
| 20 | `sq-fox-at-the-hen-run` | The Fox at the Hen-Run | Ingram Hazelshaw `cty_ingram_hazelshaw` | Coldwater: Oakhurst pig yard |
| 21 | `sq-bread-to-the-landing` | Bread to the Landing | Lichen Hedgemead `tn_lf_lichen_hedgemead` | Coldwater: Bakehouse → Glasswater landing |
| 22 | `sq-kettle-wood` | Wood for the Kettles | Sage Wheatridge `tn_lf_sage_wheatridge` | Coldwater: the Boiling House |
| 23 | `sq-apple-nose` | The Pup Who Follows His Nose (lost pup, scent) | Oriel Blossomby `cty_oriel_blossomby` | Coldwater: Meresend orchard, Upper Mere shore |
| 24 | `sq-straw-for-the-pens` | Straw for the Lambing Pens | Rusk Downfield `tn_lf_rusk_downfield` | Coldwater: Larkdowns fold |
| 25 | `sq-strayed-ewe` | Wool on the Thorns (scent-tracking) | Brock Marshholm `cty_brock_marshholm` | Coldwater: the Larkdowns fold and the downs above it |
| 26 | `sq-rope-for-the-drag` | Rope for the Drag | **board-only** (Oakhaven board); rope to Tristram Rainridge `cty_tristram_rainridge` (target) | Oakhaven: the square, the Sound Road |
| 27 | `sq-venison-for-the-cut` | Venison for the Cut | Frost Colddrift `cty_frost_colddrift` | Oakhaven: the cut in the Rainwood |
| 28 | `sq-sleepless-logger` | A Quiet Head Before the Storm | Ingram Ashleaf `cty_ingram_ashleaf` | Oakhaven: the cut |
| 29 | `sq-lamp-oil-for-the-pit` | Wood for the Pass House (new premise; id kept for the registry) | **board-only** (Frosthollow board); wood to Ebba Coalgate `cty_ebba_coalgate` (target) | Frosthollow: square → the Pass House |
| 30 | `sq-clamp-watch` | Sit the Clamp | Iris Fernhaven `cty_iris_fernhaven` | Frosthollow: the Upper hearths |
| 31 | `sq-scree-goats` | Goats on the Scree | Leif Driphand `cty_leif_driphand` | Frosthollow: the high lime quarry |
| 32 | `sq-plumtree-badger` | The Badger Under the Apple Store | Fallow Plumfield `cty_fallow_plumfield` | Larkfield: Plumtree orchard |
| 33 | `sq-swill-for-southwood` | Swill for Southwood | Njal Rustwick `cty_njal_rustwick` | Larkfield: Southwood pig yard |
| 34 | `sq-linen-for-the-press` | Old Linen for the Paper Vats | Fern Mistshore `tn_cw_fern_mistshore` | Larkfield: the Print House |
| 35 | `sq-mill-race` | Reeds in the Mill Race | Gorse Hedgewell `cty_gorse_hedgewell` | Larkfield: Longmead Mill |
| 36 | `sq-smoke-for-the-hives` | Smoke for the Hives | Hob Meadowfield `tn_sm_hob_meadowfield` | Southmeadow: the Larkholm skeps |
| 37 | `sq-hare-in-the-seed-rows` | The Miller's Wager | Emrys Stillshore `cty_emrys_stillshore` | Southmeadow: Larkholm Mill |
| 38 | `sq-festival-garlands` | Garlands for the Square (festival chore) | Willa Wheatcombe `cty_willa_wheatcombe` | Southmeadow: the square |
| 39 | `sq-twine-for-the-nets` | Twine for the Purse-Nets (mending) | Leif Rainley `cty_leif_rainley` | Fenshore: the Dune warren |
| 40 | `sq-fenside-pans` | Fire Under the Pans | Fallow Reedwell `cty_fallow_reedwell` | Fenshore: Fenside salterns |
| 41 | `sq-mirebridge-clay` | Grey Clay from the Mirebridge Pits | Lichen Reedford `cty_lichen_reedford` | Fenshore: the Mirebridge pits |
| 42 | `sq-boardwalk-plank` | A Plank Gone by the Heron (mending) | Yrsa Plankgate `cty_yrsa_plankgate` | Fenshore: the boardwalks |
| 43 | `sq-salt-for-the-road` | Salt for the Stores | Winifred Fogridge `cty_winifred_fogridge` | Saltwell: Saltpan → Saltwell Stores |
| 44 | `sq-ochre-rabbits` | Rabbits in the Vines | Senna Applegarth `cty_senna_applegarth` | Saltwell: the Ochre vineyard |
| 45 | `sq-caravan-welcome` | Water and a Word (meet the caravan) | Moss Downridge `tn_lf_moss_downridge` | Saltwell: the square, the Oasis Inn |
| 46 | `sq-fellhall-errand` | Eggs for the Fellhall Kitchen | Ansel Tarley `rm_ansel_tarley` | Ridgemere: the Commons → Fellhall's kitchens (estate) |
| 47 | `sq-grayrock-hall-errand` | Clean Linen for Grayrock Hall | Erland Cinderridge `rm_erland_cinderridge` | Ridgemere: the stores → Grayrock Hall (estate) |
| 48 | `sq-timberland-court-errand` | A Bolt for the Court | Lotte Greyley `rm_lotte_greyley` | Ridgemere: Greywool (Northgate) → Timberland Court (estate) |

The stage pack's 12 small entries (S1–S12, program `nh-pack.stage`) have their own index in §2.10.

Every quest has its own giver, or none (26, 29 and S7 are board-only), so no resident hosts more than one pack quest of any size (decisions §1.2.1).

**The spread:**
- **Ridgemere city (20):** commons 3, sump 3, wharf 3, works 2, office 2, heights 2, estate 4 (13 and 46–48), the abbey and cathedral 3. Some quests cross quarters.
- **The wild (3):** the Rainward Woods, Rainmere and Slate Tarn, and the Greyteeth fold. Coldwater's lost pup (23) is also out on the Upper Mere shore.
- **The towns (25):** Coldwater 5, Larkfield 4, Southmeadow 3, Fenshore 4, Saltwell 3, Oakhaven 3 (26 board-only), Frosthollow 3 (29 board-only).

**By kind:**
- hunting 7;
- foraging 7;
- errands and carrying 13 (46–48 are below-stairs errands);
- letters 1;
- lost pups 2;
- scent-tracking 2;
- wharf chores 2;
- helping at a stall 2;
- herbs and remedies for healers 3 (none of them for a cough);
- mending 2;
- a fox at the coops 1;
- carrying for a caravan 2;
- Restday 2;
- a festival chore 1;
- night watch 1;
- exploration 1.

---

## 1. How the packs are built

### 1.1 Three programs, and the stage pack
As `campaign.md` §7 says, the packs are open-audience programs, never chapters. They live in `Data/Storylines/ridgemere3/packs/`, never in `programs/`, because three tests count the files there.

| Program id | Quests | Towns credited | Board |
|---|---|---|---|
| `nh-pack.small.city` | 1–19, 46–48 | Ridgemere; Oakhaven (16 only) | `board:ridgemere` |
| `nh-pack.small.north` | 20–31 | Coldwater, Oakhaven, Frosthollow | `board:coldwater`, `board:oakhaven`, `board:frosthollow` |
| `nh-pack.small.south` | 32–45 | Larkfield, Southmeadow, Fenshore, Saltwell | `board:larkfield`, `board:southmeadow`, `board:fenshore`, `board:saltwell` |
| `nh-pack.stage` (§2.10) | S1–S12 here, plus 5 medium (`quests-medium.md` §4b) and 5 large (`quests-large.md` §12b) | each quest's own town | the giver's town |

The stage pack is one program for all three tiers. It is written once, here (§2.10), and lives in `Data/Storylines/ridgemere3/quests-stage/nh-pack.stage.json`.

**Each program has:**
- `audience: {"kind":"open","cap":5000}`. A wolf joins by accepting a quest.
- `start: "open"`. The south pack also has a phase `festival` (§1.6).
- Every quest is `scope: "player"`, so each wolf has their own copy (except the shared garland rope in 38).
- `lifetime: "campaign"` (except 38, which is `phase`), a `giver` (except the board-only quests below), and `board: <town>`. The board line means the quest is also posted under QUESTS; the wolf hands it in to the giver either way. The two lost-pup quests (3, 23) and the trail (25) are **not** on boards, because each needs the giver's hint.
- **Board-only quests** (26 and 29 here, S7 in the stage pack; decisions §1.2.6) have no `giver` and no `offer`. They sit on the town's board, and a trigger on `region entered` of the town square sends the wolf a journal `quest.offer` (§1.3 P9). They end on a gift to a resident **target**, with no hand-in, and are closed by plain narration: a `story.tell` to the wolf with no `from` and no quoted words. From C5 on, the target's `npc.greet` thanks the wolf in the window.

**Budgets** (limits from `story-format.md` §2 and §6):
- **Quests:** 22, 12 and 14 against a cap of 32.
- **Offers and choices:** a giver offer and a board offer count as two; a journal `quest.offer` and each `npc.offer` count as one. The city pack has 22 × 2 − 1 (3 is not on the board) + 1 (`corra-where`) + 9 (the estates' coloured lines, three per estate, one shown) = 53 against a cap of 64. The north pack has about 26 and the south about 30.
- **Triggers per phase,** campaign triggers included: about 52 in the city pack, 40 in the north and 42 in the south, against a cap of 64. The pay triggers (P3) are one per purse and amount, not one per quest.
- **Variables:** at most 26 per pack (the campaign's world numbers, the purse counters and switches of §1.3 P3, the player tracker and a few timers), against a cap of 64.
- **Trackers:** one per pack (`sq_done`, "Errands for Ridgemere", "…for the north", "…for the south"), plus the stage pack's `st_done` ("Errands of the season"), so a wolf sees at most 4 from the packs plus the campaign's 4. The cap is 16.

### 1.2 Variables every pack declares
- **World variables,** with the same names and types as `campaign.md` §4.1:
  - `nh_stage` (number, read only, for gates);
  - `nh_realm` (number);
  - the `nh_voice_<town>` strands it credits (number);
  - `nh_threat` (number; packs only lower it);
  - `nh_names` (number; written only while `nh_stage` is 4 to 6);
  - `nh_b_quarrel` (text, read only), city pack only, to colour the estates' lines (46–48).
- **The purse rule's world variables** (decisions §3.2), with the same names and types in every program that pays:
  - `coin_<purse>_day` (number): what that purse has paid in story coin today, for each purse the pack pays from: `coin_ridgemere_day`, `coin_coldwater_day`, `coin_oakhaven_day`, `coin_frosthollow_day`, `coin_larkfield_day`, `coin_southmeadow_day`, `coin_fenshore_day`, `coin_saltwell_day`;
  - `coin_<purse>_ok` (switch): the purse's floor switch.
  - Both are declared once with `world-var declare`. They are **owned** by the campaign's always-on program `nh-purses` (`campaign.md` §2.11), which zeroes the counters at `day begins` and turns a switch off when its purse falls past 60 and on again at 80. The packs only read them and add to the counters.
- **Story variables:** `garlands`, `garland_goal` and `sq_helpers` (wolves), south pack only, for the garland rope; the trail switches `ewe_n_busy` and `ewe_s_busy` of 25 (north).
- **Player variables:** `sq_done` (number, shown as the tracker); the board-only offer timers `t_drag_offer` (north) and `t_passwood_offer` (north); the trail switch `ewe_on_north` (north).

The old story counters `paid_<purse>` are gone: the purse rule replaces them.

Packs **never** write branch variables, `nh_stage`, `nh_surplus`, posts or Keepers (`campaign.md` §7), and never the floor switches.

### 1.3 Shared trigger patterns (campaign triggers, used by every listening phase)
They are numbered P1–P10, and each quest names the patterns it uses.

- **P1 Feed.** One trigger per credited town. It runs `on: "quest completed"` with `audience: participant` and `if: any [field detail = <quest id> …]`, at most 8 conditions per `any`, nested where needed. Its actions:
  - `add nh_realm <points>` and `add nh_voice_<town> <points>`;
  - `add sq_done 1 of $actor`.

  Points follow the campaign's distance weight for a small deed (`campaign.md` §2.3, §7):
  - **2** in Ridgemere and Oakhaven;
  - **3** in Coldwater, Larkfield and Frosthollow;
  - **4** in Southmeadow, Fenshore, Saltwell and Mirebridge.

  A bounty bonus (a `bandit falls` optional objective) adds `subtract nh_threat 1` on `objective completed` with `item: bounty`.
- **P2 First deed.** One trigger per quest: `on: "quest completed"`, `match: {detail: <quest id>}`, `once: "member"`, then `effect deed.award {player: $actor, detail: <phrase>}`.
  - The deed comes only on the **first** completion, so a daily chore never floods the gossip.
  - The phrase is written as the game speaks it: lower case, a verb phrase, no full stop (`talk.md` §3).
- **P3 Pay while the purse lasts (the one purse rule, decisions §3).** Coins are paid from a trigger, not from `rewards`, and every program in the realm follows the same rule:
  1. A grant happens only if the purse's floor switch `coin_<purse>_ok` is on **and** `coin_<purse>_day + amount ≤ cap`, the purse's day cap from decisions §3.1 (about 10% of what it held at the checkpoint).
  2. Then the trigger does `character.grant` with `ifRefused: "skip"`, and `add coin_<purse>_day <amount>`.
  3. Otherwise the wolf is paid **in kind**: `bread` ×2. The deed, the practice and the campaign feed land as usual, and the journal says plainly why (a `journal.note` with no speaker).
  4. Below 60 pennies a purse pays no story coin until it is back at 80 or more. That is `nh-purses`' switch; the packs only read it.

  One trigger per purse and per amount, listing the quests that pay it:
  ```json
  {"id":"pay_coldwater_3","on":"quest completed","audience":"participant",
   "if":{"test":"any","conditions":[{"test":"field","field":"detail","op":"=","value":"sq-fox-at-the-hen-run"},
                                     {"test":"field","field":"detail","op":"=","value":"sq-kettle-wood"}]},
   "actions":[{"do":"if",
     "if":{"test":"all","conditions":[
       {"test":"switch","var":"coin_coldwater_ok","is":true},
       {"test":"compare","left":{"var":"coin_coldwater_day"},"op":"<=","right":18}]},
     "then":[{"do":"effect","effect":{"kind":"character.grant","player":"$actor","coins":3,"purse":"town:coldwater","reason":"a Coldwater errand","ifRefused":"skip"}},
             {"do":"add","var":"coin_coldwater_day","value":3}],
     "else":[{"do":"effect","effect":{"kind":"item.give","player":"$actor","item":"bread","quantity":2}},
             {"do":"effect","effect":{"kind":"journal.note","player":"$actor","text":"Coldwater's purse is thin today, so the errand is paid in bread."}}]}]}
  ```
  The `right` operand is the cap less the amount: for 3 pennies, `town:ridgemere` 132, `town:coldwater` 18, `town:fenshore` 18, `town:saltwell` 19, `town:southmeadow` 20, `town:larkfield`, `town:oakhaven` and `town:frosthollow` 21. For 2 pennies add one, and for 4 take one away.

  - **The counters are shared.** The campaign, the medium and large packs and the stage pack add to the same `coin_<purse>_day`, so the cap holds for the whole realm, not per program. World variables are loaded at the start of each run's tick and saved at its end, one run after another, so no addition is lost.
  - **The thin-purse words.** Thanks text can't vary at run time (C1 isn't built), so the giver's own "thin purse" line is a window line in `resident.tree` `news`, one per dialect, gated on the town key `world.mem.nh_coin=thin` that `nh-purses` writes with C8 (the DM seeds it by hand before C8). They are in §1.8.
  - **Tills.** Before SQ2 ships (engine Phase 1) no story may name a `till:job:<id>` purse. Every small quest pays from its town store or the treasury. When SQ2 lands, a new version of the city pack may move the city errands to their givers' own tills (§3.3, SQ2), with their own `coin_till_<id>_day` counters, the reason still reading as the shop paying.
- **P4 Any of these.** For seasonal or interchangeable goods, a trigger runs `on: "gift"` and also `on: "gift again"`, as two triggers. Its filters:
  - `match: {target: <giver>}`;
  - `if: all [quest is {quest, of:$actor, status:active}, any [field item = a | field item = b …]]`.

  It then runs `objective.complete` or `objective.progress {amount: 1}` for `$actor`.
  - Used where one item would lock the quest out of a season. Chamomile, for example, is foraged in spring and summer only; valerian can be bought all year.
- **P5 Calendar or hour gate.** Here an ordinary objective event is not enough, because its time matters.
  - The trigger runs on the event (`enter place`, `stall rented`, `region entered`, `festival entered`) with `if: all [quest is active, calendar is … | hour between …]`.
  - It then runs `objective.complete` on a `story signal` objective.
  - The objective itself is `{"on":"story signal","signal":"<id>"}`, so it can't be done any other way.
- **P6 World event, every wolf present.** Events with no wolf actor (`ship docks`, `caravan arrives`, `hour`) are handled like this:
  ```json
  {"id":"lines_ship","on":"ship docks","match":{"target":"ridgemere"},
   "actions":[{"do":"each","over":"participants","actions":[
     {"do":"if","if":{"test":"all","conditions":[
        {"test":"quest is","quest":"sq-mooring-lines","of":"$each","status":"active"},
        {"test":"wolf in region","who":"$each","region":"piers"}]},
      "then":[{"do":"effect","effect":{"kind":"objective.complete","quest":"sq-mooring-lines","objective":"lines","player":"$each"}}]}]}]}
  ```
  The `each` cap of 500 is ample, because only participants with the quest active pass the condition.
- **P7 A resident who comes along** (the lost pups, 3 and 23):
  1. On `quest accepted` with `match.detail` = the quest, the trigger runs `npc.move` (the pup to a set tile) and then `npc.hold`.
  2. On `near resident` with `match.target` = the pup, guarded by `quest is active` and `objective is find: to do`, it runs `npc.release`, then `npc.follow {npc, player:$actor}`, then `objective.complete find`.
  3. On `near resident` with `match.target` = the giver, guarded by `objective is find: done`, it runs `npc.unfollow`, then `objective.complete home`.
  4. On `quest failed` and `quest abandoned` with `match.detail`, it runs `npc.unfollow` and `npc.release`, so the pup goes back to her day.

  Settings (decisions §8.1): `join.cap: 1` (one wolf at a time), `repeat: weekly`, never daily, and `fail: [{afterDays: 1}]`, one game day (4 real hours), so the pup goes back to her day and the copy comes free quickly. A wolf who fails or abandons can retake at once today (`repeat` counts only from "done"); with **N4** each quest gets `join.again: 7`. Holds and follows are undone when the run ends anyway.
- **P8 Campaign foreshadow.** Some quests have a **hidden, optional** objective that matters only while the campaign is in a given stage.
  - On `quest accepted`, `if compare nh_stage` is in the stage's range (two `compare`s under `all`), the trigger runs `objective.reveal`.
  - Its `objective completed` trigger feeds the campaign variable it names (only ever `nh_names` or `nh_threat`).
  - Outside the stage the objective stays hidden and the quest is just an errand.
  - Any words a resident says because of the stage go in the window: an `npc.offer` on the giver for `$actor` (the estates' coloured lines, Fallow's names offer in 40), never a `story.tell` with `from`.
- **P9 A board-only offer** (26, 29, S7). The town square is a story region (`oak_square`, `frost_square`). On `region entered` of it, with `audience: world`:
  ```json
  {"id":"drag_offer","on":"region entered","match":{"target":"oak_square"},
   "if":{"test":"all","conditions":[
     {"test":"not","condition":{"test":"any","conditions":[
       {"test":"quest is","quest":"sq-rope-for-the-drag","of":"$actor","status":"active"},
       {"test":"quest is","quest":"sq-rope-for-the-drag","of":"$actor","status":"offered"}]}},
     {"test":"any","conditions":[
       {"test":"quest is","quest":"sq-rope-for-the-drag","of":"$actor","status":"none"},
       {"test":"days since","from":"var","var":"t_drag_offer","of":"$actor","op":">=","days":1}]}]},
   "actions":[{"do":"start timer","var":"t_drag_offer","of":"$actor"},
              {"do":"effect","effect":{"kind":"quest.offer","quest":"sq-rope-for-the-drag","player":"$actor"}}]}
  ```
  The timer keeps the journal offer to once a game day a wolf. `quest.offer` may name `$actor` from a world trigger.
- **P10 A gated offer held by a resident** (S10, S11 in the stage pack). A giver's offer can't be gated on a branch today (C6 `requires` isn't built, and a giver offer shows to every wolf in the phase). So the quest has **no `giver`** and no board; a trigger on `near resident` of the holder, gated on the branch (`compare nh_b_ford = "burned"`, `compare nh_stage = 8`), starts a once-a-day timer and sends a journal `quest.offer` plus the holder's `npc.offer` "ask" for `$actor`. On `quest completed` a second `npc.offer` on the holder carries the thanks. Both are the holder's own words, in the window.

### 1.4 Scaling: 5 wolves today, hundreds later
- **Every quest is per wolf,** so 5 wolves or 500 each get their own copy. Nothing waits for a crowd.
- **Crowding one giver.**
  - Errands that hand out a prop (bread, a letter, a crate) have `join.cap` 3 to 6 copies at a time.
  - When the cap is full, the board shows the quest as full, and that town's helpful lines point to a neighbouring giver. For example, "the other bakehouse wants carriers too".
  - World-object quests (the two pups) have `join.cap: 1`; the wool trail (25) and the clamp watch (30) have `join.cap: 2`, because each has a second copy in the world (decisions §8.1).
- **The `join.cap` 1 and 2 quests** (3, 23, 25, 30; decisions §8.1). Each:
  1. is `repeat: weekly`, never daily;
  2. carries `fail: [{afterDays: 1}]`, one game day (4 real hours), so a stalled copy comes back quickly;
  3. has an "it's taken" helpful line in its town that points to another quest there (in each quest's Talk table);
  4. gets a second copy where the world allows: two trails for 25 and two clamps for 30, hence `join.cap: 2`. The pups (3, 23) are one pup each, so they stay at 1.
  - With **N4**, each gets `join.again: 7`, counted from any end (done, failed or abandoned).
- **Purses.** The one rule of §1.3 P3: per-purse day caps of about 10%, shared with every program, the floor of 60, and the game's per-wolf grant cap.
  - At 5 wolves nobody meets a cap.
  - At 500 wolves the whole realm's story coin from the treasury is at most 135 pennies a game day, and from each town store 21–24. Everyone past the cap is paid in bread and in practice.
  - That keeps the 212–248-penny town stores alive, and a store below 60 pays nothing until it recovers to 80. Coins are never minted.
- **The one shared bar** is the garland rope in 38. Its goal grows with the wolves who help: `garland_goal = 6 + 2 × count(sq_helpers)`, using the campaign's `ch_helpers` trick (`campaign.md` §2.2), which works today. With C2 it becomes `max(6, 2 × active)`.
- **The campaign scales itself.** Its chapter goals grow with the helpers who feed them (`campaign.md` §2.2). A crowd doing small quests reaches a chapter's goal sooner and then pours into `nh_surplus`, exactly as the campaign wants.
- **Prop items minted by `item.give`** (bread, letters, crates, salt) are consumed by the delivery gift. At most 4 go out per accept, and a wolf who abandons the quest keeps them; that is the cost of a few loaves.

### 1.5 Rewards
- **Coins:** 2 to 4 pennies from the giver's town store or the treasury (P3, decisions §3.2.4). They come from the town's purse, never minted, and only while the purse's day cap and floor allow; otherwise the wolf is paid `bread` ×2.
  - No merchant's till is named before SQ2 ships (decisions §3.2.5): `till:job:<id>` fails the purse check today, and `till:<id>` names no account. See SQ2 in §3.
- **Items in `rewards`:** 1 to 3 pieces that fit the giver, such as cake from the baker, nails from the nailer or eggs from the hen-run. Minted by `item.give`, so kept small.
- **Practice:** `story.practice` in the fitting skill: craft, commerce, labour, service, travel or watch.
- **Deeds:** first completion only (P2).
- **Bonus effects** for optional objectives, as listed per quest.
- **Recognition** is the deed: residents greet the wolf with "You … didn't you?", and the crier calls the season's deeds at the festival.

### 1.6 The festival phase (south pack)
- A campaign trigger runs on `day begins`[w], whose `quantity` is the day of the season:
  - `if field quantity >= 40 and <= 46`, it does `enter festival`;
  - on day 47 the phase's own trigger does `enter open`.
- Only quest 38 lists `phases: ["festival"]`. Every other quest has no `phases` field, so it is offered in both phases.
- Festival day is day 46 of every season in every town (`world-lore.md` §3). Southmeadow's festivals are the Blossom Fair, Midsummer, Harvest Home and Midwinter (`campaign.md` §3.3). The texts say only "the festival", so they are right in every season.

### 1.7 Campaign feed and gates, in one place
- **Today (no engine change):** world variables, P1. The campaign polls them hourly (`campaign.md` §4).
- **With C12a** `story.notify`: P1 also sends `{"kind":"story.notify","story":"<running chapter>","signal":"pack_deed","player":"$actor"}`, so the wolf's own "Your voice" tracker ticks. With J2 (realm variables), P1 adds to `p_voice` directly instead.
- **A pledged wolf's quarter-share** (`campaign.md` §5.3) needs the wolf's `p_howl`, which belongs to the chapter program. With J2 it is readable as a realm variable, and P1 adds `max(1, points/4)` to `nh_voice_<p_howl>` through an if-ladder of 9. Without J2, packs credit only the town where the deed was done.
- **Stage gates** (`compare nh_stage`, `compare nh_b_*`) are used by P8 hidden objectives, by the estates' coloured lines, and by the stage pack (§2.10), whose phases follow `nh_stage`.

  Thanks text can't vary at run time (C1 isn't built). So any line that depends on the campaign is an `npc.offer` on the giver for `$actor`, added by a trigger, or a gated tree line in `resident.tree`. **No `story.tell` carries `from` with a resident's words** (decisions §9, P2-22): a banner with a face is not the dialogue window. Plain `story.tell` narration, with no `from` and no quoted speech, stays.

### 1.8 Talk: where the gossip lines live and what gates them
- **Window lines** go in `Data/Talk/dialogue/resident.tree`:
  - helpful knowledge in node `news`;
  - "someone did it" lines in node `gossip`.

  Each is `say[here.region=<town> self.dialect=<d> world.mem.<key>=<v> priority=2] line | alt`. A shared `.talk` rule cannot target `dlg.*` concepts, so the lines go in the tree (`talk.md` §1).
- **Two town memory keys per town,** so the 32-entry town memory isn't crowded (the city is one owner and shares it with the campaign's `nh_*` keys):
  - `world.mem.sq_on=yes`: the pack is live. Written at publish by C8 `talk.remember {scope: town, key: sq_on, value: yes, days: 90}`, and renewed on `season begins`. **Before C8, the DM seeds it by hand per town.**
  - `world.mem.sq_last=<short>`: the last small quest done in that town, 2 days. Written by P1 with C8. With no C8, the "after" lines simply don't play, and the deed gossip (P2) still does.
- **Every line follows the rules:**
  - at most 160 characters per alternative, and no ids;
  - **no personal names**: roles only, such as "the baker", "the Fenside salt raker";
  - dialect by `self.dialect`;
  - no weather or time-of-day words without a gate;
  - wolves and pups, the Bright One and the Dark One only.
- **Helpful knowledge is true to the data:**
  - forage grounds and seasons from `forage.json`;
  - animal habitats and tempers from `animals.json`;
  - places that exist.
- **Allow-list additions** for `tools/dialect_allow.json` "places", beyond `campaign.md` §10.2: Morning Oven, House of Mending, Ropewalk, Records Office, City Office, Carters' Yard, Clock Tower, Lamp Store, Nailers' Forge, Cinders, Boiling House, Oakhurst, Meresend, Plumtree, Southwood, Print House, Heron, Fen Shore, Rainmere, Slate Tarn, Rainward Woods, Upper hearths, Greyteeth, Pass House, Dune, Fenside, Redsand, Fellhall, Grayrock Hall, Timberland Court, Greywool, Masons' Yard, Hillside, Soup Kitchen, Vine and Sickle, Looms.
- **The thin-purse lines** (§1.3 P3), node `news`, each gated `here.region=<town> self.dialect=<d> world.mem.nh_coin=thin priority=3`:

  | Dialect | Line |
  |---|---|
  | plain | The town purse is thin this week. Errands are still paid, in bread if not in coin. |
  | coldwater | Purse is thin as a spring net, eh? The errands still pay, in bread if not coin. |
  | oakhaven | The purse is down to the bark, mm? Work's still paid, in bread if not in coin. |
  | frosthollow | Purse is thin as a worked seam, stone. You'll be paid in bread till it fills. |
  | larkfield | The purse is short-weighed this week, eh? Errands pay in bread till it's full weight. |
  | southmeadow | The purse is blighted thin this week, bless it. You'll get bread, not coin. |
  | mirebridge | Purse is boggy-low, no? Errands still pay, in bread if not in coin. |
  | saltwell | The purse is dry this week, my friend. The work is paid in bread until it fills. |

### 1.9 Two reference implementations (validate with `story_program.review`)
**Quest 1, an errand with a prop:**
```json
{"id":"sq-morning-bread","title":"Morning Bread for the Sump","summary":"Carry the Morning Oven's first loaves to the nurse at the Sump Infirmary.",
 "giver":"rm_tove_brinegate","board":"ridgemere","repeat":"daily","join":{"cap":3},
 "offer":{"ask":"You've flour to your elbows. Something wanted?",
          "say":"#greet#. The first loaves go down to the Sump Infirmary every day, and my carrier's off his feet. Four loaves to the nurse there. Take the Sump Stairs, not the lanes; the lanes flood first. Will you?",
          "accept":"I'll take them down.","decline":"Not today.","handIn":"The nurse has her bread.",
          "thanks":"#thanks#. Warm bread for the sick, and you never dropped one. Take a cake for the walk. Same again tomorrow, if your paws are willing."},
 "objectives":[{"id":"deliver","label":"Give the four loaves to the nurse at the Sump Infirmary.","on":"gift","match":{"target":"rm_theodora_wickley","item":"bread"},"goal":4,"measure":"quantity"},
               {"id":"early","label":"Get there before the tenth hour.","on":"story signal","signal":"bread-early","optional":true,"hidden":true}],
 "rewards":[{"kind":"item.give","player":"$actor","item":"honey_cake","quantity":1},{"kind":"story.practice","player":"$actor","skill":"service"}],
 "bonus":[{"kind":"item.give","player":"$actor","item":"bread","quantity":1}]}
```
Its triggers:
```json
{"id":"bread_give","on":"quest accepted","audience":"participant","match":{"detail":"sq-morning-bread"},
 "actions":[{"do":"effect","effect":{"kind":"item.give","player":"$actor","item":"bread","quantity":4}}]},
{"id":"bread_early","on":"gift","audience":"participant","match":{"target":"rm_theodora_wickley","item":"bread"},
 "if":{"test":"all","conditions":[{"test":"quest is","quest":"sq-morning-bread","of":"$actor","status":"active"},{"test":"hour between","from":4,"to":9}]},
 "actions":[{"do":"effect","effect":{"kind":"objective.complete","quest":"sq-morning-bread","objective":"early","player":"$actor"}}]}
```
**Quest 3, a lost pup (P7):**
```json
{"id":"pup_lost","on":"quest accepted","audience":"participant","match":{"detail":"sq-gone-to-see-the-ships"},
 "actions":[{"do":"effect","effect":{"kind":"npc.move","npc":"rm_corra_ropewick","cell":"w_ridgemere_0_1","x":161,"y":101}},
            {"do":"effect","effect":{"kind":"npc.hold","npc":"rm_corra_ropewick"}},
            {"do":"effect","effect":{"kind":"npc.offer","npc":"rm_soren_ropewick","id":"corra-where","words":"Where does she usually go?",
              "reply":"Wherever the masts are. The piers under the Grayrock office, mostly. Follow your nose, eh? She smells of schoolroom chalk and the coal I keep in my pocket.","player":"$actor","once":true}}]},
{"id":"pup_found","on":"near resident","audience":"participant","match":{"target":"rm_corra_ropewick"},
 "if":{"test":"all","conditions":[{"test":"quest is","quest":"sq-gone-to-see-the-ships","of":"$actor","status":"active"},
                                   {"test":"objective is","quest":"sq-gone-to-see-the-ships","objective":"find","of":"$actor","status":"to do"}]},
 "actions":[{"do":"effect","effect":{"kind":"npc.release","npc":"rm_corra_ropewick"}},
            {"do":"effect","effect":{"kind":"npc.follow","npc":"rm_corra_ropewick","player":"$actor"}},
            {"do":"effect","effect":{"kind":"objective.complete","quest":"sq-gone-to-see-the-ships","objective":"find","player":"$actor"}}]},
{"id":"pup_home","on":"near resident","audience":"participant","match":{"target":"rm_soren_ropewick"},
 "if":{"test":"objective is","quest":"sq-gone-to-see-the-ships","objective":"find","of":"$actor","status":"done"},
 "actions":[{"do":"effect","effect":{"kind":"npc.unfollow","npc":"rm_corra_ropewick"}},
            {"do":"effect","effect":{"kind":"objective.complete","quest":"sq-gone-to-see-the-ships","objective":"home","player":"$actor"}}]},
{"id":"pup_let_go","on":"quest failed","match":{"detail":"sq-gone-to-see-the-ships"},
 "actions":[{"do":"effect","effect":{"kind":"npc.unfollow","npc":"rm_corra_ropewick"}},{"do":"effect","effect":{"kind":"npc.release","npc":"rm_corra_ropewick"}}]}
```
Before approval, pick the exact standable tile: the pier tile must be walkable, and approval checks walk tiles.

---

## 2. The quests

**Notation used in every quest:**
- **Objectives** are written `id`: label (event; `match`; goal/measure).
- **Talk lines** are written `node · gate`:
  - `news` means `resident.tree` node `news`; `gossip` means node `gossip`;
  - `on` means the gate `world.mem.sq_on=yes`, and `last=x` means the gate `world.mem.sq_last=x`.
- **Dialogue** uses `#symbols#` in the giver's lines (`say`, `thanks`, `reply`), so their own dialect fills them. The wolf's lines (`ask`, `accept`, `decline`, `handIn`, `words`) are plain.

### 2.1 Ridgemere, the city (program `nh-pack.small.city`)

Every city giver speaks plain unless it says otherwise. The city's purse is `town:ridgemere` (the treasury, 1,355), day cap 135 shared with every program (P3). Feed (P1): +2 `nh_realm` and +2 `nh_voice_ridgemere`, except 16, which credits Oakhaven.

---

#### 1. `sq-morning-bread`: Morning Bread for the Sump
- **Tier:** small, about 10 minutes. Repeat `daily`, `join.cap` 3. Board `ridgemere`.
- **Where:** from Northgate down to the Sump.
  - Giver at The Morning Oven `rm_the_morning_oven` (door `w_ridgemere_1_0` (93,236)).
  - Delivery to the Sump Infirmary `rm_the_sump_infirmary` (door `w_ridgemere_0_0` (249,250)).
- **Giver:** Tove Brinegate `rm_tove_brinegate`, f 58, baker at The Morning Oven, plain.
  - Home `rm_the_morning_oven_flat`. Alive, placed, reachable.
  - Why her: she is loud and blunt and "knows which streets flood first". The Oven feeds the Sump.
- **Recipient:** Theodora Wickley `rm_theodora_wickley`, nursing for the abbey at the Sump Infirmary. Alive and placed.
- **Premise:** The Morning Oven's first loaves go to the Sump Infirmary each day, and Tove's carrier is laid up. Carry four loaves down the Sump Stairs before the lanes fill.
- **Objectives:**
  - `deliver`: Give the four loaves to the nurse at the Sump Infirmary (`gift`; `{target: rm_theodora_wickley, item: bread}`; goal 4, quantity).
  - `early` (optional, hidden): Get there before the tenth hour (`story signal` `bread-early`, P5 with `hour between 4 9`).
- **Triggers:** `bread_give` (`item.give bread 4` on accept), `bread_early` (§1.9), P1, P2, P3.
- **Rewards:**
  - P3: 2 pennies from `town:ridgemere`.
  - `rewards`: `honey_cake` ×1 and practice in service.
  - `bonus`: `bread` ×1.
  - Deed: "carried the Morning Oven's bread down to the Sump Infirmary".
- **Feeds:** `nh_realm` +2, `nh_voice_ridgemere` +2.
- **Needs:** none (C6 `offer.during` would let Tove say "Still on the stairs?").

**Dialogue** (Tove, plain):
- **ask:** "You've flour to your elbows. Something wanted?"
- **say:** "#greet#. The first loaves go down to the Sump Infirmary every day, and my carrier's off his feet. Four loaves to the nurse there. Take the Sump Stairs, not the lanes; the lanes flood first. Will you?"
- **accept:** "I'll take them down." **decline:** "Not today." **handIn:** "The nurse has her bread."
- **thanks:** "#thanks#. Warm bread for the sick, and you never dropped one. Take a cake for the walk. Same again tomorrow, if your paws are willing."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain` · on | The Morning Oven sends bread to the Sump Infirmary every day. The baker's always a carrier short, and she pays in cake. \| Want an honest errand? The baker on the north side needs her loaves carried down to the Sump. |
| news · `here.region=ridgemere self.dialect=plain self.quarter=sump` · on | Going down to the infirmary? Take the Stairs. The lanes behind the Fish Hall are the first to flood. |
| gossip · `here.region=ridgemere self.dialect=plain` · last=bread | The infirmary had its bread warm today. Even the nurse smiled, and she never smiles. |

---

#### 2. `sq-mending-herbs`: Herbs for the House of Mending
- **Tier:** small, 10–20 minutes. Repeat `daily`, `join.cap` 4. Board `ridgemere`.
- **Where:** the abbey's House of Mending `rm_the_house_of_mending` (door `w_ridgemere_1_0` (77,242)). Herbs are foraged outside the walls.
- **Giver:** Frederick Nailgate `rm_frederick_nailgate`, m 30, healer of the abbey. Plain (accent none).
  - Home `rm_mossgrave_cottages`. Alive, placed, reachable.
  - Why him: he is the abbey's healer but not the infirmarian. Gorm Plankgate, the infirmarian, is campaign cast and is kept free. He is blunt and guarded.
- **Premise:** The House of Mending's draughts run through herbs faster than the abbey garden grows them. Five bunches will do the week; yarrow is better still. It names no cough or other ailment: the Sump cough belongs to `lq-runnel-fever` alone (decisions §2.1).
- **Objectives:**
  - `herbs`: Bring the healer five bunches of herbs (`gift`; `{target: rm_frederick_nailgate, item: herbs}`; goal 5, quantity).
  - `yarrow` (optional): Find yarrow for the poultices (`gift`; `{target: rm_frederick_nailgate, item: yarrow}`; goal 2, quantity).
- **Seasons:** herbs and yarrow are foraged from spring to autumn. In winter, `herbs` can still be bought: the treasury stocks 102, and most tills hold some.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: practice in service.
  - `bonus`: `yarrow_poultice` ×1.
  - Deed: "filled the House of Mending's herb shelf".
- **Feeds:** +2 / +2.
- **Needs:** none.

**Dialogue** (Frederick, plain):
- **ask:** "Is the abbey short of something?"
- **say:** "#greet#. The House of Mending goes through herbs faster than the abbey garden grows them. Five bunches, from the long grass outside the walls or under the willows. Yarrow too, if you find it. I'll not ask where you picked them."
- **accept:** "Five bunches. I'll find them." **decline:** "Not today." **handIn:** "Your herbs, and yarrow if I found it."
- **thanks:** "#thanks#. That's the week's draughts. If you come in hurt one day, I'll remember whose herbs these were. That's more than I promise most."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain` · on | The abbey's healer pays for herbs. Long grass and willows give them, any season but winter. \| Herbs for the House of Mending? Any green bunch from the grass outside the walls, he says. |
| news · `here.region=ridgemere self.dialect=plain` · on | Yarrow's the one the healer wants most. It grows on open grass, spring to autumn. |
| gossip · `here.region=ridgemere self.dialect=plain` · last=herbs | The House of Mending had a full shelf today. Someone brought the healer an armful. |

---

#### 3. `sq-gone-to-see-the-ships`: Gone to See the Ships (lost pup)
- **Tier:** small, 10–20 minutes. Repeat `weekly`, `join.cap` 1, `fail: [{afterDays: 1}, {on: "death", match: {actor: "rm_soren_ropewick"}}]`; with N4, `join.again: 7` (decisions §8.1). Not on the board: the hint is the giver's. There is one pup, so there is no second copy.
- **Where:** the Sump, then the piers.
  - Home: The Pawn and Ledger, the flat above `rm_the_pawn_and_ledger_flat` (shop door `w_ridgemere_1_1` (22,78)).
  - The pup is found on the piers, `w_ridgemere_0_1` about (161,101).
- **Giver:** Soren Ropewick `rm_soren_ropewick`, m 62, no post. **Coldwater-raised** (light), living in the Sump.
  - Home `rm_the_pawn_and_ledger_flat`. Alive, placed, reachable.
  - Why him: he is honest to a fault and playful, "keeps a pocketful of coal for luck", and is the father of the pup (`live.npc_relations`, parent).
- **Pup:** Corra Ropewick `rm_corra_ropewick`, f 6, at lessons at the Commons school. Her backstory: she "dreams of a berth on a Grayrock ship bound somewhere dry". Alive and placed.
- **Premise:** Corra slipped her lessons to count masts on the piers again, and Soren's knees won't take the Sump Stairs twice in a day. Find her by her scent and walk her home.
- **Objectives:**
  - `find`: Find Corra on the Wharf (`story signal` `corra-found`; P7 `near resident` `rm_corra_ropewick`).
  - `home`: Walk her back to her father (`story signal`; P7 `near resident` `rm_soren_ropewick`, after `find`).
- **Triggers:** `pup_lost`, `pup_found`, `pup_home`, `pup_let_go` (§1.9), P1, P2, P3.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: practice in watch.
  - Deed: "walked a lost pup home from the Wharf".
- **Feeds:** +2 / +2.
- **Needs:**
  - **J1**, so the hint `npc.offer` is per wolf. Without it, the offer shows to every pack participant (`once: true`), which is harmless.
  - C6, for "Any sign of her?".

**Dialogue** (Soren, coldwater):
- **ask:** "You look like you've lost something."
- **say:** "#greet#, #address#. My littlest slipped her lessons again. She'll be at the water counting masts; she always is. My knees won't take the Sump Stairs twice in a day. Would you find my minnow and bring her home, eh?"
- **accept:** "I'll find her." **decline:** "I can't just now." **handIn:** "Here she is, safe."
- **npc.offer** `corra-where` (sent on accept):
  - **words:** "Where does she usually go?"
  - **reply:** "Wherever the masts are. The piers under the Grayrock office, mostly. Follow your nose, eh? She smells of schoolroom chalk and the coal I keep in my pocket."
- **thanks:** "There's my minnow. #thanks#, #address#. The patrolman on the Sump round used to walk her home, before he went up the Hundred Steps to keep the Palace lamp. Now it's you. Calm water to you."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain self.quarter=sump` · on | The old wolf above the pawnshop loses his littlest twice a week. She's always on the piers, counting masts. |
| news · `here.region=ridgemere self.dialect=plain self.quarter=wharf` · on | Hunting for a pup on the quay? Try the piers under the Grayrock office. That's where the small ones go to look at the ships. |
| gossip · `here.region=ridgemere self.dialect=plain` · last=ships | Somebody walked the pawnbroker's little sister home from the piers. She talked about ships the whole way, I'd wager. |
| news · `here.region=ridgemere self.dialect=coldwater` · on | That old lake wolf over the pawnshop has a pup who'd sail to the Bright One's door if a ship went there, eh? |
| news · `here.region=ridgemere self.dialect=plain self.quarter=sump` · on (it's taken: points to 1) | If somebody's already gone after the pawnbroker's pup, the Morning Oven always wants a carrier down the Stairs. |

---

#### 4. `sq-smokehouse-wood`: Oak Smoke for the Herring
- **Tier:** small, 15–20 minutes. Repeat `daily`, `join.cap` 4. Board `ridgemere`.
- **Where:** the North Wharf Smokehouse `rm_the_north_wharf_smokehouse` (door `w_ridgemere_0_0` (187,200)). Wood comes from the Rainward Woods (`w_ridgemere_2_0`, `2_2`) or any broadleaf trees.
- **Giver:** Rosamund Coalford `rm_rosamund_coalford`, f 55, runs the smokehouse. Plain (broad).
  - Home `r2_salt_row_house_up`. Alive, placed, reachable.
  - Why her: she is gentle and proud and believes "the Houses will fall out before the next winter".
- **Premise:** The herring is in and the smokehouse fires are low. Five armfuls of firewood, and oak bark for the flavour, never pine.
- **Objectives:**
  - `wood`: Bring five armfuls of firewood (`gift`; `{target: rm_rosamund_coalford, item: firewood}`; goal 5, quantity).
  - `bark`: Bring two strips of oak bark (`gift`; `{target: rm_rosamund_coalford, item: oak_bark}`; goal 2, quantity).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `smoked_fish` ×1 and practice in labour.
  - Deed: "kept the North Wharf smokehouse fires going".
- **Feeds:** +2 / +2.
- **Needs:** none.

**Dialogue** (Rosamund, plain):
- **ask:** "Your fires look low."
- **say:** "#greet#. The herring's in and my fires are low. Five armfuls of firewood, and two strips of oak bark. Oak, mind, not pine; pine smoke makes the fish taste of a ship's hold. The broad-leaved trees past the walls have both."
- **accept:** "Oak, not pine. I'll bring it." **decline:** "Another day." **handIn:** "Wood and oak bark, as you asked."
- **thanks:** "That'll smoke sweet. #thanks#. Take a fish; it's this week's. Eat it while the Houses are still speaking to each other. I don't think they will be by winter."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain` · on | The smokehouse on the north wharf pays for oak bark. Never pine, she says, and you can smell why. |
| news · `here.region=ridgemere self.dialect=plain` · on | Oak bark peels off the broad-leaved trees in the woods past the walls. Pines give you resin, not bark. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=wharf` · last=smoke | The north wharf smokehouse was going full today. You could smell it from the Fish Hall. |

---

#### 5. `sq-ropewalk-resin`: Resin for the Tar Pot
- **Tier:** small, 15–25 minutes. Repeat `daily`, `join.cap` 4. Board `ridgemere`.
- **Where:** the Ropewalk `rm_the_ropewalk` (door `w_ridgemere_0_1` (192,160)), in the works by the Cinders. Resin comes from the pines in the Rainward Hills.
- **Giver:** Kari Bilgehand `rm_kari_bilgehand`, f 48, ropemaker. Plain (accent none).
  - Home `rm_rook_house`. Alive, placed, reachable.
  - Why her: she is meticulous and playful, Sump-born, and "answers to the steward for every load" of the House that owns the walk.
- **Premise:** An untarred rope rots on its first wet voyage. Kari's tar pot wants pine resin, and the Grayrock ships take every fathom she makes.
- **Objectives:**
  - `resin`: Bring three lumps of pine resin (`gift`; `{target: rm_kari_bilgehand, item: resin}`; goal 3, quantity).
  - `wax` (optional): A lump of beeswax for whipping the rope ends (`gift`; `{target: rm_kari_bilgehand, item: beeswax}`). The chandlers sell it.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `rope` ×1 and practice in craft.
  - `bonus`: `cord` ×2.
  - Deed: "brought pine resin for the Ropewalk tar pots".
- **Feeds:** +2 / +2.
- **Needs:** none.
  - Kari can't be helped *at* the walk: lend a paw works only between wolves (SQ3).

**Dialogue** (Kari, plain):
- **ask:** "What's that smell? Tar?"
- **say:** "#greet#. Tar, and not enough of it. An untarred rope rots on its first wet voyage. Three lumps of pine resin for the pot, from the pines up in the hills. The Grayrock ships take every fathom I make, so the pot's never full for long."
- **accept:** "Three lumps of resin. Done." **decline:** "Not now." **handIn:** "Resin for your pot."
- **thanks:** "#thanks#. Into the pot it goes, and I'll write it in the steward's book to the lump. Take a coil; tarred, it'll outlast us both."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain` · on | The Ropewalk buys pine resin for the tar. The hills east of the city are thick with pines. |
| news · `here.region=ridgemere self.dialect=plain self.quarter=works` · on | Every Grayrock hull leaves port in Ropewalk rope. The ropemaker pays for resin, and counts every lump. |
| gossip · `here.region=ridgemere self.dialect=plain` · last=resin | The tar pot at the Ropewalk was full today. The whole Cinders smelled like a pine wood. |

---

#### 6. `sq-records-copying`: A Page and a Drawer
- **Tier:** small, 10–15 minutes. Repeat `daily`, `join.cap` 6. Board `ridgemere`.
- **Where:** the Records Office `r2_the_records_office`, with the copying room `r2_the_records_office_up` (door `w_ridgemere_1_1` (74,108)), in the office quarter.
- **Giver:** Harriet Platt `rmx_harriet_platt`, f 33, clerking at the Records Office, plain.
  - Home `rm_platt_house`. Alive, placed, reachable.
  - Why her: "she writes a careful, square hand", is ambitious and devout, and dreams of a Grayrock berth.
- **Premise:** The copyists are a paw short. Copy one page and sort one drawer. The Office pays by the page, and its lore pages come with the work.
- **Objectives:**
  - `copy`: Copy a page at the Records Office (`archive work`; `{item: copy, detail: ridgemere}`).
  - `sort`: Sort a drawer of old rolls (`archive work`; `{item: sort, detail: ridgemere}`).
  - `roll` (optional, hidden; P8): Find the drowned village's page in the old count.
    - It is revealed on accept while `nh_stage` is 4 to 6.
    - It completes on a second `sort` while revealed, through a trigger on `archive work` `{item: sort, detail: ridgemere}` with `objective is sort: done`.
- **Rewards:**
  - P3: 2 pennies. The archive also pays its own rate.
  - `rewards`: practice in commerce.
  - `bonus`: a `story.tell {to: wolf, player: $actor}` with no `from` (narration of the page, not Harriet's words): "Under the drowned village's name, someone has written: struck from the rolls, the spring after the flood. Nothing else."
  - Deed: "copied the rolls at the Records Office".
- **Feeds:**
  - +2 / +2.
  - `roll` adds **`nh_names` +1** (Ch 4–6 only; the Book of Names).
- **Needs:** none.

**Dialogue** (Harriet, plain):
- **ask:** "Do they want hands up there?"
- **say:** "#greet#. The copyists are a paw short and the rolls don't wait. Copy me one page and sort me one drawer. The Office pays by the page, and you'll read things up there nobody downstairs remembers."
- **accept:** "One page, one drawer." **decline:** "My paws are full." **handIn:** "The page is copied and the drawer sorted."
- **thanks:** "#thanks#. Square letters, too; I can tell a careless paw a room away. You'd be surprised what's in those drawers. A whole village struck off in one line, the spring after a flood. Come again."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain self.quarter=office` · on | Upstairs at the Records Office they pay for copying. You read the old count while you do it. |
| news · `here.region=ridgemere self.dialect=plain` · on | If you want to know how the city was counted, sort a drawer at the Records Office. They'll even pay you for it. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=office` · last=records | There was a new paw at the copying desk today. Neat letters, they say. |

---

#### 7. `sq-heights-round`: The Heights Round (a letter)
- **Tier:** small, about 10 minutes. Repeat `daily`, `join.cap` 3, `fail: [{on: "death", match: {actor: "rm_gwen_cinderdale"}}]` (she is 63). Board `ridgemere`.
- **Where:** from the City Office `rm_the_city_office` (door `w_ridgemere_1_1` (16,114)) to the Vance house `rm_vance_house` (door `w_ridgemere_1_1` (197,134)) in the Heights.
- **Giver:** Gwen Cinderdale `rm_gwen_cinderdale`, f 63, clerking at the City Office. Plain (Ridgemere-raised, light).
  - Home `rm_quarles_house` (door `w_ridgemere_1_1` (6,124)); work `rm_the_city_office`. Alive, placed, reachable.
  - Why her: she is tired and anxious and "smells faintly of smoke whatever the weather". All three of the city's couriers carry `lq-long-run`, so the letters are now the City Office's own, carried up to the Vances (decisions §1.3). Her backstory (a brother lost on the wharf, and no forgiveness for House Grayrock) is never spoken here.
- **Recipient:** Roald Wickstack `rm_roald_wickstack`, in service to the Vances. He is saving for a stall on the Commons. Alive and placed. He gives `lq-vanishing-thief`; this is his only target role.
- **Premise:** The City Office's letters for the Heights pile up on the clerk's desk, because nobody likes the climb or the looks at the door. Carry one up.
- **Objectives:**
  - `letter`: Give the letter to the Vances' doorkeeper (`gift`; `{target: rm_roald_wickstack, item: letter}`).
- **Triggers:**
  - `letter_give`: `item.give letter 1` on accept.
  - P1, P2, P3.
- **Rewards:**
  - P3: 2 pennies from `town:ridgemere`.
  - `rewards`: practice in travel.
  - Deed: "carried a City Office letter up to the Heights".
- **Feeds:** +2 / +2.
- **Needs:** **J9**, a tagged letter. Without it, any `letter` counts, which is acceptable for an errand.

**Dialogue** (Gwen, plain):
- **ask:** "Is the City Office sending something up the hill?"
- **say:** "#greet#. The City Office's letters for the Heights pile up on my desk, because nobody likes the climb or the looks at the door. This one's for the Vance house, to the wolf who keeps their door. My knees did that hill for forty years. They've retired, even if I haven't."
- **accept:** "I'll take it up." **decline:** "Find another paw today." **handIn:** "Your letter's delivered."
- **thanks:** "#thanks#. One less on the pile, and one less climb for me. Half the Heights owes a House factor, and they'd sooner not open anything with a seal on it. You got one opened. That's a day's work, that is."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain` · on | The City Office never has enough paws for the Heights round. Those houses tip in thanks, not pennies. |
| news · `here.region=ridgemere self.dialect=plain self.quarter=heights` · on | A letter for this street? Bring it to the door, not the gate. The doorkeepers here don't come down for anyone. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=heights` · last=letter | A City Office letter came to one of our doors today. The servant took it like it might bite. |

---

#### 8. `sq-restday-candles`: Candles for Restday (a Restday task)
- **Tier:** small, 15 minutes, plus the service. Repeat `weekly`, `join.cap` 10. Board `ridgemere`.
- **Where:** the Cathedral of Ridgemere `r2_the_cathedral_of_ridgemere` (door `w_ridgemere_0_0` (231,175)), North Headland.
- **Giver:** Berit Coalworth `rm_berit_coalworth`, f 41, sexton of the cathedral, plain.
  - Home `rm_the_mossgrave_gatehouse`. Alive, placed, reachable.
  - Why her: she is warm and restless, Sump-born, and "counts the bells from the Chapel of the Tide". She isn't the abbot (Lionel Ropestack is campaign cast).
- **Premise:** On Restday the cathedral burns more candles than the abbey can pour. Bring two, and be in the nave for the service.
- **Objectives:**
  - `candles`: Bring two candles, wax or tallow (`gift`; `{target: rm_berit_coalworth, item: candle_wax}`; goal 2, quantity). P4 completes it from tallow: a trigger on `gift`/`gift again` to Berit with item `candle_tallow` and `field quantity >= 2` runs `objective.complete candles`.
  - `service`: Be in the cathedral for the Restday service (`story signal`; P5 on `enter place` `{target: r2_the_cathedral_of_ridgemere}` with `calendar is weekday Restday` and `hour between 9 11`).
- **Rewards:**
  - P3: 2 pennies.
  - `rewards`: practice in service.
  - Deed: "brought candles for the Restday service".
- **Feeds:** +2 / +2.
- **Needs:** none.

**Dialogue** (Berit, plain):
- **ask:** "Is there work for a pair of paws here?"
- **say:** "#greet#. On Restday this cathedral burns more candles than the abbey can pour. Bring me two, wax or tallow, and come to the service from the ninth hour to the eleventh. The Bright One sees who fills the lamps, and so do I."
- **accept:** "Two candles, and I'll be there." **decline:** "Not this week." **handIn:** "Candles, and I came to the service."
- **thanks:** "#thanks#. The old King's candle is still lit by the altar. We've not let it go out since he went up. Yours will stand beside it, and I'll count them with the bells."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain` · on | The cathedral's sexton takes candles any day, but she wants you at the service on Restday too. |
| news · `here.region=ridgemere self.dialect=plain` · on | Wax or tallow, the cathedral burns it all. The chandlers on the north side sell both. |
| gossip · `here.region=ridgemere self.dialect=plain` · last=candles | There were new candles beside the old King's on Restday. Somebody's kind. |

---

#### 9. `sq-almoners-round`: The Almoner's Round (a Restday-week task)
- **Tier:** small, 15–20 minutes. Repeat `weekly`, `join.cap` 6. Board `ridgemere`.
- **Where:** from the Chapter House `r2_the_chapter_house` (door `w_ridgemere_0_0` (217,180)) to the Wharf and Sump cell `w_ridgemere_0_1`.
- **Giver:** Disa Raincot `rm_disa_raincot`, f 22, almoner of the abbey. **Coldwater-raised** (light).
  - Home `rm_the_grayrock_gatehouse`. Alive, placed, reachable.
  - Why her: she is stubborn and blunt and "never goes out without an oilcloth hood". She dreams of a Grayrock berth.
- **Premise:** The abbey's alms bread has to reach the Sump, and the almoner can't carry it all. Three loaves to three wolves who need it, and no questions asked.
- **Objectives:**
  - `alms`: Give a loaf each to three wolves in the Wharf and Sump (`gift`; `{item: bread, cell: w_ridgemere_0_1}`; goal 3, count).
  - `meal` (optional): Share a meal with someone in the Sump (`shared meal`; `{cell: w_ridgemere_0_1}`).
- **Triggers:**
  - `alms_give`: `item.give bread 3` on accept.
  - P1, P2, P3.
- **Rewards:**
  - P3: 2 pennies.
  - `rewards`: practice in service.
  - `bonus`: `porridge` ×1.
  - Deed: "carried the abbey's alms bread through the Sump".
- **Feeds:** +2 / +2.
- **Needs:** none.
  - `gift` counts the first gift to each resident in a day, and `gift again` also counts. A wolf giving two loaves to one hungry wolf still finishes, which suits alms.

**Dialogue** (Disa, coldwater):
- **ask:** "You're carrying a lot of bread."
- **say:** "#greet#. The abbey's alms go down to the Sump and I can't carry them all. Three loaves, one each, to three wolves on the Wharf side who look like they need it. Don't ask them why. A full net's no shame to share, eh?"
- **accept:** "Three loaves, three wolves." **decline:** "Not this week." **handIn:** "All three loaves are given."
- **thanks:** "#thanks#. Three fed. Back home the lake gives to whoever puts a net in. The Sump never had a lake, so it has us. Fair winds."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain self.quarter=sump` · on | The young almoner from the lake country sends bread down here. Some wolf carries it, and doesn't ask questions. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=sump` · last=alms | Somebody gave me bread today and didn't ask a thing. Bright One keep them. |
| news · `here.region=ridgemere self.dialect=coldwater` · on | The abbey's almoner is lake-born, eh? She'll find you work if you've a strong back and a closed mouth. |

---

#### 10. `sq-marketday-stock`: Marketday Stock (helping at a stall)
- **Tier:** small, 15–30 minutes. Repeat `daily`, `join.cap` 6. Board `ridgemere`.
- **Where:** the Market Hall `r2_the_market_hall` (doors `w_ridgemere_1_1` (84,16) and (123,16)) and Market Square (`w_ridgemere_1_1`), in the commons.
- **Giver:** Thyra Plankridge `rm_thyra_plankridge`, f 34, provisioner in the Market Hall, plain.
  - Home `rm_thorncote_house`. Alive, placed, reachable.
  - Why her: she is warm and honest to a fault. She "carries a letter meant for Lord Grayrock" and means to keep the counter in the family.
- **Premise:** Marketday empties the provisioner's shelves by noon. Sell her three of anything you've gathered. If you've the nerve, run your own stall on the Square on Marketday.
- **Objectives:**
  - `stock`: Sell the provisioner three goods you've gathered (`trade done`; `{target: rm_thyra_plankridge, detail: sold}`; goal 3, quantity).
  - `stall` (optional): Rent a stall on Market Square on Marketday (`story signal`; P5 on `stall rented` `{cell: w_ridgemere_1_1}` with `calendar is weekday Marketday`).
- **Rewards:**
  - P3: 2 pennies, on top of the sale.
  - `rewards`: practice in commerce.
  - `bonus`: practice in commerce again.
  - Deed: "stocked the Market Hall provisioner's shelves".
- **Feeds:** +2 / +2.
- **Needs:**
  - SQ5: `stall sale` credits the buyer, not the stall's wolf, so "sell three things at your stall" can't be an objective. `trade done` with the provisioner is used instead.
  - `trade done` is a watched event: the pack must list it in its watch, which the runner does from its objectives.

**Dialogue** (Thyra, plain):
- **ask:** "Your shelves look bare."
- **say:** "#greet#. Marketday empties my shelves by noon. Sell me three of anything good you've gathered: mushrooms, berries, herbs. I'll pay an honest price and a little over. And if you've the nerve, rent a stall on the Square on Marketday and see how the other half lives."
- **accept:** "Three goods. I'll be back." **decline:** "I've nothing to sell today." **handIn:** "That's three sold to you."
- **thanks:** "#thanks#. Fair dealing. Half the Square owes the Counting House this month, so it's nice to trade with someone who doesn't flinch at a coin. I've a letter for a lord who's never home, if you ever see him."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain self.quarter=commons` · on | The provisioner in the Market Hall buys whatever you've gathered, and pays honest. On Marketday she's desperate. |
| news · `here.region=ridgemere self.dialect=plain` · on | Stalls on the Square go for a few pennies on Marketday. Sell from one and you'll learn every trick on the Commons. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=commons` · last=stock | The provisioner's shelves were full by Marketday. Somebody's been gathering for her. |

---

#### 11. `sq-rag-market-mushrooms`: Greens for the Rag Market (helping at a stall)
- **Tier:** small, 15–25 minutes. Repeat `daily`, `join.cap` 4. Board `ridgemere`.
- **Where:** the greens stall in the Rag Market (`w_ridgemere_0_1`, about (236,51)). Mushrooms come from the Rainward Woods.
- **Giver:** Agnes Millford `rm_agnes_millford`, f 28, greens stall in the Rag Market. **Oakhaven-raised** (light).
  - Home `rm_saltwick_house`. Alive, placed, reachable.
  - Why her: she is dreamy and generous, a Rainwood wolf in the Sump. She "owes a House factor money… by the new moon".
- **Premise:** City wolves buy any green thing, mushrooms most of all, and her stall is a basket short.
- **Objectives:**
  - `mushrooms`: Bring four mushrooms (`gift`; `{target: rm_agnes_millford, item: mushrooms}`; goal 4, quantity).
  - `berries` (optional): A few berries for colour (`gift`; `{target: rm_agnes_millford, item: berries}`; goal 2, quantity). Berries grow summer and autumn.
- **Seasons:** mushrooms grow spring to autumn, in deadwood, moss and ferns, and under broadleaf trees in summer and autumn. In winter the quest still takes bought mushrooms.
- **Rewards:**
  - P3: 2 pennies.
  - `rewards`: `mushroom_stew` ×1 and practice in commerce.
  - Deed: "brought mushrooms for the Rag Market greens stall".
- **Feeds:** +2 / +2.
- **Needs:** none.

**Dialogue** (Agnes, oakhaven):
- **ask:** "What sells best on this stall?"
- **say:** "#greet#, #address#. City wolves'll buy any green thing, mm? Mushrooms most of all. Four, from the deadwood and the ferns in the woods past the walls. Back home in the Rainwood I'd have a basket by breakfast. Here I'm a basket short."
- **accept:** "Four mushrooms, coming." **decline:** "Not today." **handIn:** "Mushrooms for your stall."
- **thanks:** "Good bark, these. #thanks#. Have yesterday's stew, mm? It's better the second day, like most things. And every penny the stall makes goes to the factor by the new moon."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain self.quarter=sump` · on | The greens wolf from the Rainwood pays for mushrooms. She says the Sump's too damp for anything but mould. |
| news · `here.region=ridgemere self.dialect=plain` · on | Mushrooms come up out of rotten logs and fern banks, spring to autumn. The woods past the walls are full of both. |
| news · `here.region=ridgemere self.dialect=oakhaven` · on | Mushrooms want deadwood, mm? Rotten logs and wet ferns. You'll fill a basket slow and sure. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=sump` · last=greens | The greens stall had mushrooms today, fat ones. Someone knows the woods. |

---

#### 12. `sq-nailers-charcoal`: Charcoal for the Nail-Hearths
- **Tier:** small, about 10 minutes. Repeat `daily`, `join.cap` 4. Board `ridgemere`.
- **Where:** Timberland's Nailers' Forge `rm_nailers_forge` (door `w_ridgemere_0_1` (228,146)), the Cinders. Charcoal comes from the Timberland Charcoal Store `rm_the_charcoal_store` (door `w_ridgemere_1_1` (0,150)), on the Sump side of the Commons.
- **Giver:** Wynn Sootworth `rm_wynn_sootworth`, f 33, smith at the Nailers' Forge, plain.
  - Home `rm_nailers_forge_flat`. Alive, placed, reachable.
  - Why her: she is curious and devout and "trusts no one from a rival House". She saw the Chancellor weep and has told no one, and her lines never say it.
- **Premise:** The nail-hearths eat charcoal like the Houses eat coin. Three sacks from the charcoal store will keep them in through Emberday.
- **Objectives:**
  - `charcoal`: Bring three sacks of charcoal (`gift`; `{target: rm_wynn_sootworth, item: charcoal}`; goal 3, quantity).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `nails` ×4 and practice in labour.
  - Deed: "carried charcoal to the Nailers' Forge".
- **Feeds:** +2 / +2.
- **Needs:** none.

**Dialogue** (Wynn, plain):
- **ask:** "Those hearths look hungry."
- **say:** "#greet#. The nail-hearths eat charcoal like the Houses eat coin. Three sacks from the Timberland charcoal store, on the Sump side of the Commons, and I'll keep the fires in through Emberday. Timberland pays, eventually. Timberland always pays eventually."
- **accept:** "Three sacks. I'll bring them." **decline:** "Not today." **handIn:** "Three sacks of charcoal."
- **thanks:** "#thanks#. Take a paw of nails; we make more than we sell. And if anyone from another House asks what you saw down here, you saw a forge. That's all there is to see."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain self.quarter=works` · on | The Nailers' Forge never has charcoal enough. The charcoal store on the Sump side of the Commons sells it by the sack. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=works` · last=charcoal | The nail-hearths were roaring today. Someone fed them. |
| news · `here.region=ridgemere self.dialect=plain` · on | Want honest sweat in the Cinders? Carry charcoal for the nailers. They pay in nails, and nails sell. |

---

#### 13. `sq-greatly-stables`: Oats for the Factors' Horses
- **Tier:** small, about 15 minutes. Repeat `daily`, `join.cap` 4. Board `ridgemere`.
- **Where:** the Greatly stables `rm_the_greatly_stables` (door `w_ridgemere_1_0` (203,170)), in the Greatly estate at Northgate.
- **Giver:** Philippa Rustton `rm_philippa_rustton`, f 54, keeping the Greatly stables. Plain (accent none).
  - Home `r2_gutter_rents_top`. Alive, placed, reachable.
  - Why her: she is loyal and loud, grew up at a Timberland charcoal camp, and "went back to waged work… and does not complain where anyone can hear". She works for the campaign's villain House without being one of them.
- **Premise:** Greatly horses eat better than half the Sump. They have to be fresh, because the factors will soon be riding the debt rounds.
- **Objectives:**
  - `oats`: Bring three of oats (`gift`; `{target: rm_philippa_rustton, item: oats}`; goal 3, quantity). Oats are in 24 tills; hay is in only one, so hay isn't asked for.
  - `straw`: Bring two bundles of straw for bedding (`gift`; `{target: rm_philippa_rustton, item: straw}`; goal 2, quantity).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: practice in labour.
  - Deed: "mucked out and fed the Greatly stables".
- **Feeds:** +2 / +2.
- **Note:** this is not the campaign's Purse path. It credits the realm like any errand, never `nh_bought`.
- **Needs:** none.

**Dialogue** (Philippa, plain):
- **ask:** "Do the Greatly horses want feeding?"
- **say:** "#greet#. Greatly horses eat better than half the Sump, and I'm the one feeds them. Three of oats and two bundles of straw for bedding; the provisioners sell both. The factors want the horses fresh. They'll be out riding the debt rounds soon, chalk in their pouches."
- **accept:** "Oats and straw. I'll fetch it." **decline:** "Not for Greatly." **handIn:** "Oats and straw, as asked."
- **thanks:** "#thanks#. Don't look so sour. Whoever owns the horse, it's the horse that's hungry. I grew up on a charcoal camp; I know what it is to work for a House and keep your own counsel."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain` · on | The Greatly stables pay for oats and straw. Don't expect the steward to thank you; the stable wolf will. |
| gossip · `here.region=ridgemere self.dialect=plain` · last=stables | The Greatly horses were fed early today. That means the factors are riding. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=sump` · last=stables | Somebody's fattening the factors' horses. I'd sooner they went hungry, and so would every chalked door. |

---

#### 14. `sq-dawn-bell`: The Dawn Bell
- **Tier:** small, about 10 minutes. Repeat `daily`, no cap, `fail: [{on: "death", match: {actor: "rm_corwin_bilgestack"}}]` (he is 60). Board `ridgemere`.
- **Where:** from the Lamp Store `r2_the_lamp_store` (door `w_ridgemere_1_1` (21,15)) to the Clock Tower top `r2_the_clock_tower_up` (door `w_ridgemere_1_0` (113,181)). Both are reachable.
- **Giver:** Corwin Bilgestack `rm_corwin_bilgestack`, m 60, lighting the lamps at dusk. Larkfield-raised with accent none, so his lines are plain.
  - Home `r2_cockle_court_house_top`. Alive, placed, reachable.
  - Why him: he is loyal and guarded and "lost the old post to someone better placed". He saw the Chancellor weep and has told no one.
- **Premise:** Since the old King went up, nobody rings the morning bell in the Clock Tower. Climb it at first light, ring it, and tell the lamplighter.
- **Objectives:**
  - `ring`: Climb the Clock Tower and ring the bell at first light (`story signal`; P5 on `enter place` `{target: r2_the_clock_tower_up}` with `hour between 5 7`). The trigger also sends `story.tell {to: wolf, player: $actor}`: "You haul on the rope. The bell rolls out over the roofs, and somewhere below a door bangs open."
  - `tell`: Tell the lamplighter it's rung (`conversation`; `{target: rm_corwin_bilgestack}`; `after: [ring]`).
- **Rewards:**
  - P3: 2 pennies.
  - `rewards`: practice in watch.
  - Deed: "rang the dawn bell from the Clock Tower".
- **Feeds:** +2 / +2. This is the campaign's stage 1–2 seed "Ring the bell at dawn" (`campaign.md` §7).
- **Needs:** none. There is no "bell rung" event; being at the top inside the hour is the ring.

**Dialogue** (Corwin, plain):
- **ask:** "Who rings the morning bell these days?"
- **say:** "#greet#. I light the lamps at dusk and put them out at first light, and since the old King went up nobody rings the morning bell. The rope's at the top of the Clock Tower stair. Ring it between the fifth hour and the seventh, and come and tell me."
- **accept:** "I'll ring it." **decline:** "I sleep late." **handIn:** "The bell's rung."
- **thanks:** "#thanks#. I heard it from here. There's wolves in the Sump who set their day by that bell, and they've been late ever since he went up. I had a post once that mattered. This is a small thing, but it's mine to see done."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain` · on | The lamplighter wants somebody to ring the morning bell. You climb the Clock Tower at first light; he can't, at his age. |
| gossip · `here.region=ridgemere self.dialect=plain` · last=bell | Somebody rang the Clock Tower bell at first light. First time since the old King went up. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=sump` · last=bell | Heard the bell? I was at the Stairs before the fish carts for once. |

---

#### 15. `sq-mooring-lines`: Take Her Lines (a wharf chore)
- **Tier:** small, 5–30 minutes, depending on when a ship docks. Repeat `daily`, `join.cap` 8, `fail: [{afterDays: 2}]`. Board `ridgemere`.
- **Where:** the piers, `w_ridgemere_0_1`, region `piers` = (148,88)–(170,108). The tiles are snapped to the pier at authoring.
- **Giver:** Wystan Coalmarsh `rm_wystan_coalmarsh`, m 50, unloading ships at the piers, plain.
  - Home `rm_kiln_row`. Alive, placed, reachable.
  - Why him: he is proud and devout, Sump-born and "means to die… in the Heights". He "takes the long stairs two at a time".
- **Premise:** When a ship comes in, somebody has to take her lines, and the Grayrock office never sends enough paws. Be on the piers when the next one docks, and bring a fresh coil.
- **Objectives:**
  - `lines`: Be on the piers when a ship docks (`story signal`; P6 on `ship docks` `{target: ridgemere}`, each participant with the quest active and `wolf in region piers`).
  - `rope`: Bring a coil of rope for the bollards (`gift`; `{target: rm_wystan_coalmarsh, item: rope}`).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: practice in labour.
  - Deed: "took a ship's lines at the Ridgemere piers".
- **Feeds:** +2 / +2.
- **Needs:** none. The ships are s21 to s25: the Fair Wind, the Grey Tern, the Cormorant and the Long Tide.

**Dialogue** (Wystan, plain):
- **ask:** "You look like you're waiting for something."
- **say:** "#greet#, #address#. When a ship comes in, somebody has to be on the pier to take her lines, and the Grayrock office never sends enough paws. Be on the piers when the next one docks, and bring me a coil of rope for the bollards. The Fair Wind's due, or the Grey Tern."
- **accept:** "I'll be on the pier." **decline:** "I can't wait about today." **handIn:** "She's tied up, and here's your rope."
- **thanks:** "#thanks#. Snug as a pup. I'll be up in the Heights one day, you know, a house with a door that sticks. And I'll still come down to take a ship's lines."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain self.quarter=wharf` · on | Want work on the piers? Be there when a ship comes in. They always want paws on the lines. |
| news · `here.region=ridgemere self.dialect=plain` · on | The ships come in on a timetable, and the notice board tells you which is due. Bring rope and you're welcome. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=wharf` · last=lines | A wolf off the street took a ship's lines today, neat as a sailor. |

---

#### 16. `sq-oakhaven-load`: A Crate Light (carrying for a caravan)
- **Tier:** small, 15–25 minutes, about 1,500 tiles of the Sound Road. Repeat `daily`, `join.cap` 3. Board `ridgemere`.
- **Where:** from the Carters' Yard `r2_the_carters_yard` (door `w_ridgemere_1_1` (177,34)) to Oakhaven Stores `cn_oakhaven_store` (door `w_ridgemere_3_4` (244,188)).
- **Giver:** Petra Plankhand `rm_petra_plankhand`, f 53, driving for the carters, plain.
  - Home `r2_tar_corner_top`. Alive, placed, reachable.
  - Why her: she is ambitious and quietly funny, "smells faintly of smoke", and "carries a letter meant for Lord Grayrock".
- **Recipient:** Cairn Oakhaven `tn_oh_cairn_oakhaven`, the Oakhaven storekeeper, who "distrusts anything from the city".
  - He is campaign cast as a Keeper fallback. Here he is only a gift recipient, so he gets no hub slot.
- **Premise:** The Oakhaven wagon went out a crate light, and the storekeeper counts everything twice. Carry it along the Sound Road.
- **Objectives:**
  - `crate`: Carry the crate to the Oakhaven storekeeper (`gift`; `{target: tn_oh_cairn_oakhaven, item: crate}`).
- **Triggers:**
  - `crate_give`: `item.give crate 1` on accept.
  - P1, crediting **Oakhaven**, P2, P3.
- **Rewards:**
  - P3: 3 pennies, `town:ridgemere`.
  - `rewards`: practice in travel.
  - Deed: "carried a Carters' Yard crate along the Sound Road to Oakhaven".
- **Feeds:** `nh_realm` +2, **`nh_voice_oakhaven` +2**. A deed is credited to the town it serves (`campaign.md` §5.3).
- **Needs:** J9, a tagged crate, is optional.

**Dialogue** (Petra, plain):
- **ask:** "Is that crate going somewhere?"
- **say:** "#greet#. The Oakhaven wagon went out a crate light, and the storekeeper there counts everything twice. Carry it along the Sound Road to Oakhaven Stores, would you? It's not heavy. It's just far, and the road's not as safe as it was."
- **accept:** "I'll carry it to Oakhaven." **decline:** "Too far for me today." **handIn:** "The crate's in Oakhaven."
- **thanks:** "#thanks#. He'll have counted it and found it right, for once. I've a letter for the Grayrock lord in my harness; been there a month. Roads are like that lord: never where you want them."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain` · on | The Carters' Yard pays for a crate carried when a wagon goes short. The Oakhaven road's the usual one. |
| gossip · `here.region=oakhaven self.dialect=oakhaven` · last=crate | A city wolf walked a crate to the stores, mm? The storekeeper counted it three times and found it whole. |
| news · `here.region=oakhaven self.dialect=oakhaven` · on | The Ridgemere carters send short loads, mm? Some kind wolf usually walks the rest along the Sound Road. |

---

#### 17. `sq-heights-thyme`: Willow Bark for the Heights' Joints (remedies for a healer)
- **Tier:** small, 15–20 minutes. Repeat `daily`, `join.cap` 4. Board `ridgemere`. The id is kept for `givers.tsv`; the ailment and title are new (decisions §2.1).
- **Where:** Grantham House `rm_grantham_house` (door `w_ridgemere_1_1` (173,163)), the Heights.
- **Giver:** Egil Grantham `rm_egil_grantham`, m 39, visiting the sick of the Heights. Larkfield-raised with accent none, so his lines are plain.
  - Home `rm_grantham_house_up`. Alive, placed, reachable.
  - Why him: he is unhurried and dreamy, has "a cousin in the Fell vineyards", and is the only resident whose post is tending the sick of the Heights.
- **Premise:** The damp has got into every old joint in the Heights. The good houses send for a physician; Egil trusts willow bark. **No cough:** the city's one cough line is `lq-runnel-fever`'s.
- **Objectives:**
  - `bark`: Bring three strips of willow bark, or willow tea (`gift`; `{target: rm_egil_grantham, item: willow_bark}`; goal 3, quantity). P4 completes it from tea: a trigger on `gift`/`gift again` to Egil with item `willow_tea` and `field quantity >= 2` runs `objective.complete bark`.
- **Where to get it:** willow bark isn't foraged; it is sold. At the checkpoint 74 purses hold 154 strips (every physic in the city and most town stores), and 59 hold willow tea. So it works in every season.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: practice in service.
  - Deed: "brought willow bark for the stiff joints of the Heights".
- **Feeds:** +2 / +2. It no longer foreshadows `nh_b_cough`: that belongs to `lq-runnel-fever` (decisions §2.1).
- **Needs:** none.

**Dialogue** (Egil, plain):
- **ask:** "Who's ailing up here?"
- **say:** "#greet#. The damp's got into every old joint in the Heights, and the good houses would sooner send for a physician than for bark. I've more faith in willow. Three strips of willow bark from any physic on the Commons, or willow tea if they've some made, and I'll steep it myself."
- **accept:** "Three strips of willow bark." **decline:** "Not today." **handIn:** "Willow bark, as you asked."
- **thanks:** "#thanks#, #address#. Steeped tonight, and three old wolves on this street will take their stairs tomorrow without swearing at them. My cousin in the Fell vineyards swears by it for the pickers' backs."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain self.quarter=heights` · on | The wolf who visits our sick swears by willow bark for stiff joints. Says it beats any physician on the Commons. |
| news · `here.region=ridgemere self.dialect=plain` · on | Every physic in the city sells willow bark. Steep it long and it eases an old joint, they say. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=heights` · last=willow | Half the Heights smells of willow tea. Somebody brought the healer bark enough for every stiff knee on the street. |

---

### 2.2 The wild near the city (program `nh-pack.small.city`)

#### 18. `sq-rainward-pheasants`: Pheasants for the Cinder Butchers (hunting)
- **Tier:** small, 15–30 minutes. Repeat `daily`, `join.cap` 6. Board `ridgemere`.
- **Where:** the Rainward Woods, `w_ridgemere_2_0` and `w_ridgemere_2_2`, either side of Fellhall. Hand-in at Cinder Butchers `r2_cinder_butchers` (door `w_ridgemere_1_1` (25,146)).
- **Giver:** Corwin Sootstack `rm_corwin_sootstack`, m 43, butcher at Cinder Butchers. **Coldwater-raised** (light).
  - Home `r2_hook_row_house`. Alive, placed, reachable.
  - Why him: he is unhurried and generous, "can name every ship in the Sound", and "carries a letter meant for Lord Grayrock".
- **Premise:** The Heights want pheasant pie and his hooks are bare. Two birds out of the Rainward Woods, and the feathers too if you keep them.
- **Objectives:**
  - `birds`: Take two pheasants in the Rainward Woods (`story signal` `pheasant`, goal 2). Set by a trigger on `hunted` `{target: pheasant}` with `if any [field cell = w_ridgemere_2_0, field cell = w_ridgemere_2_2]`, using `objective.progress amount 1`. This is SQ1 until an objective can name two cells.
  - `feathers` (optional): Bring the feathers (`gift`; `{target: rm_corwin_sootstack, item: feathers}`; goal 3, quantity). A pheasant gives 3.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `meat_pie` ×1 and practice in watch.
  - `bonus`: practice in commerce, for selling the feathers on.
  - Deed: "brought pheasants from the Rainward Woods to the Cinder butchers".
- **Feeds:** +2 / +2.
- **Needs:** SQ1, which is convenience only.

**Dialogue** (Corwin, coldwater):
- **ask:** "Hooks are looking bare."
- **say:** "#greet#, #address#. The Heights want pheasant pie and my hooks are bare. Two birds out of the Rainward Woods, past Fellhall, eh? They'd sooner run than fly; head them off. And the feathers, if you keep them. I sell those too."
- **accept:** "Two pheasants." **decline:** "Not today." **handIn:** "Two birds, and the feathers."
- **thanks:** "#thanks#. Fair catch, both. Here's a pie from yesterday's batch. I've a letter for the Grayrock lord in my apron; if you see a ship he's on, you tell me which. I know them all."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain` · on | The Cinder butcher pays for pheasants out of the Rainward Woods. They run more than they fly; head them off. |
| news · `here.region=ridgemere self.dialect=plain` · on | Pheasants feed in the grass and scrub at the wood's edge. Two wolves, one to drive and one to wait, take them easy. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=sump` · last=pheasant | There's pheasant pie at the Cinder butchers. Somebody hunted the Rainward Woods. |

---

#### 19. `sq-sounding-the-tarns`: Sounding the Tarns (exploration)
- **Tier:** small, 20–30 minutes. Repeat `weekly`, no cap. Board `ridgemere`.
- **Where:** Rainmere (`w_ridgemere_3_2`, "The Rainward Hills, Rainmere") and Slate Tarn (`w_ridgemere_4_2`, "The Rainward Hills, Slate Tarn"). Optionally Highmere (`w_ridgemere_6_2`).
- **Giver:** Yrsa Nailridge `rm_yrsa_nailridge`, f 52, helping at Soundings `rm_soundings` (door `w_ridgemere_1_1` (69,74)), plain.
  - Home `rm_soundings_flat`. Alive, placed, reachable.
  - Why her: she is dreamy and blunt and "hums sea songs". She is the cartographer's helper and **not** the cartographer: Imogen Nailridge is campaign cast and takes the Mirebridge pledges.
- **Premise:** The cartographer is redrawing the hills. Walk out to Rainmere and Slate Tarn, and come back and say they're still where the chart says.
- **Objectives:**
  - `rainmere`: Walk out to Rainmere (`enter place`; `{target: w_ridgemere_3_2}`).
  - `slate`: Walk on to Slate Tarn (`enter place`; `{target: w_ridgemere_4_2}`).
  - `report`: Tell Yrsa what you saw (`conversation`; `{target: rm_yrsa_nailridge}`; `after: [rainmere, slate]`).
  - `highmere` (optional): Go as far as Highmere (`enter place`; `{target: w_ridgemere_6_2}`).
  - `echo` (optional, hidden until accept): If you carry the Echo Gift, sound a tarn's depth with it (`gift used`; `{detail: echo}`).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: practice in travel.
  - `bonus`: `map` ×1.
  - Deed: "walked the tarns for the Soundings chart".
- **Feeds:** +2 / +2. It foreshadows the Howl Line (`campaign.md` §5.2): Yrsa's thanks speaks of "posts a howl could carry from".
- **Needs:** J10 (`wolf has`), so `echo` is offered only to an Echo wolf. Until then the label says "if you carry".

**Dialogue** (Yrsa, plain):
- **ask:** "What are all those charts?"
- **say:** "#greet#. The cartographer's redrawing the hills and wants to know the tarns are where the old chart says. Walk out to Rainmere and on to Slate Tarn, then come back and tell me they're still there. They will be. It's the roads between that keep moving."
- **accept:** "Rainmere and Slate Tarn." **decline:** "My paws need a rest." **handIn:** "Both tarns are where the chart says."
- **thanks:** "#thanks#. She'll be pleased. She's marking every hill a howl could carry from, though she won't say why. Go to Highmere next time; that's the one the chart's least sure of."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain self.quarter=commons` · on | Soundings pays wolves to walk out to the tarns and back. Easy pennies, if your paws are good. |
| news · `here.region=ridgemere self.dialect=plain` · on | Rainmere's up in the hills east of here, and Slate Tarn past it. Keep the city behind you and climb. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=commons` · last=tarns | Soundings has fresh word of the tarns. The new chart's half done. |

### 2.3 Coldwater and the Greyteeth (program `nh-pack.small.north`)

Coldwater givers speak **coldwater**: "Still water", "Fair winds", minnow, ", eh?", "Net and line", "fair catch". The purse is `town:coldwater` (212 pennies), day cap 21 (P3). Feed (P1): +3 `nh_realm` and +3 `nh_voice_coldwater`.

---

#### 20. `sq-fox-at-the-hen-run`: The Fox at the Hen-Run (chasing off a fox from the coops)
- **Tier:** small, 15–30 minutes. Repeat `weekly`, `join.cap` 3. Board `coldwater`.
- **Where:** Oakhurst pig yard `fs_pigs_greenholt_sties` (door `w_ridgemere_5_6` (145,99)), in the Greenholt west of Coldwater. The hunt is in cell `w_ridgemere_5_6`.
- **Giver:** Ingram Hazelshaw `cty_ingram_hazelshaw`, m 52, keeps pigs at Oakhurst. Coldwater (broad).
  - Home `fs_pigs_greenholt_home`. Alive, placed, reachable.
  - Why him: he is shrewd and playful, "has opinions about everyone's grandmother", and "swore an oath at a wedding and takes it seriously".
- **Premise:** A fox has taken three of the hens that scratch round the pig yard, and the pigs just watch. Find it in the scrub and see it off.
- **Objectives:**
  - `fox`: Bring down the fox near the Oakhurst yard (`hunted`; `{target: fox, cell: w_ridgemere_5_6}`).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `eggs` ×2 and practice in watch.
  - Deed: "drove the fox off the Oakhurst hen-run".
- **Feeds:** +3 / +3.
- **Needs:** none.
  - Foxes live in forest, scrub, grass and crop ground, and fight when cornered (`animals.json`), so the ground round a farm yard fits.

**Dialogue** (Ingram, coldwater):
- **ask:** "Trouble at the yard?"
- **say:** "#greet#, #address#. There's a fox at my hens, and the pigs just watch it, the great lumps. Three birds this week. Find it in the scrub round the yard and see it off for good, eh? Net and line, I'd do it myself, but it runs rings round me."
- **accept:** "I'll find your fox." **decline:** "Not this week." **handIn:** "Your fox won't be back."
- **thanks:** "#thanks#. A full net, that. Here's eggs from the hens that are left; they'll lay better now they can sleep. My grandmother would've had that fox for a scarf, eh? Yours too, I'd say."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=coldwater self.dialect=coldwater` · on | The Oakhurst pig wolf has a fox at his hens, eh? Foxes keep to the scrub and woods. Corner one and it fights. |
| news · `here.region=coldwater self.dialect=coldwater` · on | Hunting a fox? Go quiet and downwind. They hear you coming a field away, eh? |
| gossip · `here.region=coldwater self.dialect=coldwater` · last=fox | Somebody saw off the Oakhurst fox. Fair catch, that. The hens are laying again. |

---

#### 21. `sq-bread-to-the-landing`: Bread to the Landing
- **Tier:** small, 5–10 minutes. Repeat `daily`, `join.cap` 3. Board `coldwater`.
- **Where:** from the Coldwater Bakehouse `cn_coldwater_bakehouse` (door `w_ridgemere_7_7` (154,3)) to the Glasswater boat shed `fs_fishing_glasswater_shed` (door `w_ridgemere_7_7` (46,198)).
- **Giver:** Lichen Hedgemead `tn_lf_lichen_hedgemead`, f 56, baker at Coldwater Bakehouse. Coldwater (light).
  - Home `cn_coldwater_bakehouse_flat`. Alive, placed, reachable.
  - Why her: she is blunt and unhurried, "collects smooth stones from the riverbanks", and "has a sweetheart in Southmeadow".
- **Recipient:** Grist Reedmere `tn_cw_grist_reedmere`, boatman at Glasswater. He was raised in Ridgemere; he isn't campaign cast. Alive and placed.
- **Premise:** The Glasswater boat crews go out before the oven is warm and come back starving. Three loaves down to the landing.
- **Objectives:**
  - `loaves`: Give the three loaves to the Glasswater boatman (`gift`; `{target: tn_cw_grist_reedmere, item: bread}`; goal 3, quantity).
- **Triggers:**
  - `landing_give`: `item.give bread 3` on accept.
  - P1, P2, P3.
- **Rewards:**
  - P3: 2 pennies.
  - `rewards`: practice in service.
  - Deed: "carried the bakehouse loaves down to the Glasswater landing".
- **Feeds:** +3 / +3.
- **Needs:** none.

**Dialogue** (Lichen, coldwater):
- **ask:** "Anything need carrying?"
- **say:** "#greet#. The boat crews on the Glasswater landing go out before my oven's warm, and they come back hungry. Three loaves down to the boat shed, to the boatman there. It's a short walk, and the view's the best in the Greenholt, eh?"
- **accept:** "Three loaves to the landing." **decline:** "Not today." **handIn:** "The boatman has his bread."
- **thanks:** "#thanks#. Full bellies on the water. If you're ever going Southmeadow way, I might have a letter for you. Might. Calm water."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=coldwater self.dialect=coldwater` · on | The baker sends bread down to the landing most days, eh? Carry it and she'll see you right. |
| gossip · `here.region=coldwater self.dialect=coldwater` · last=landing | The Glasswater crew had warm bread today, eh? Somebody carried it down. |
| news · `here.region=coldwater self.dialect=plain` · on | If you want a small job here, ask the baker. Her loaves go down to the boat crews every day. |

---

#### 22. `sq-kettle-wood`: Wood for the Kettles
- **Tier:** small, 10–20 minutes. Repeat `daily`, `join.cap` 4. Board `coldwater`.
- **Where:** the Coldwater Boiling House `cn_coldwater_soapworks` (door `w_ridgemere_7_7` (216,2)).
- **Giver:** Sage Wheatridge `tn_lf_sage_wheatridge`, f 38, soap boiler. Coldwater (broad).
  - Home `cn_coldwater_soapworks_flat`. Alive, placed, reachable.
  - Why her: she is anxious and restless and "whistles back at the larks".
- **Premise:** The soap kettles want firing all day and her woodpile is down to bark. Fat scraps from a hunt would be welcome too.
- **Objectives:**
  - `wood`: Bring five armfuls of firewood (`gift`; `{target: tn_lf_sage_wheatridge, item: firewood}`; goal 5, quantity).
  - `fat` (optional): Two fat scraps for the kettle (`gift`; `{target: tn_lf_sage_wheatridge, item: fat_scraps}`; goal 2, quantity). A badger gives two.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `soap` ×1 and practice in labour.
  - `bonus`: `fine_soap` ×1.
  - Deed: "kept the Coldwater Boiling House kettles fired".
- **Feeds:** +3 / +3.
- **Needs:** none.

**Dialogue** (Sage, coldwater):
- **ask:** "What are you boiling?"
- **say:** "#greet#, #address#. Soap, and the kettles want firing all day. My woodpile's down to bark. Five armfuls of firewood, eh? And if you've fat scraps from a hunt, I'll take two and thank you twice. Deep and dark, don't let the fire go out on me."
- **accept:** "Five armfuls, and fat if I find it." **decline:** "Not today." **handIn:** "Wood for your kettles."
- **thanks:** "#thanks#. Fair catch. Here's a cake of soap. Every wolf in Ridgemere could use one, but they won't buy soap from the lake, eh? Their loss."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=coldwater self.dialect=coldwater` · on | The Boiling House always wants wood, eh? Any wood. The kettle doesn't care if it's pretty. |
| news · `here.region=coldwater self.dialect=coldwater` · on | Fat scraps? A badger gives two. They're in the woods and scrub, and they don't run, eh? They turn round. |
| gossip · `here.region=coldwater self.dialect=coldwater` · last=kettle | The Boiling House smoked all day. Somebody fed those kettles, fair winds to them. |

---

#### 23. `sq-apple-nose`: The Pup Who Follows His Nose (a lost pup and scent-tracking)
- **Tier:** small, 15–30 minutes. Repeat `weekly`, `join.cap` 1, `fail: [{afterDays: 1}]`; with N4, `join.again: 7` (decisions §8.1). Not on the board. One pup, so no second copy.
- **Where:** the Meresend Apple Store `fs_orchard_coldwater_store` (door `w_ridgemere_8_6` (106,177)), on the Upper Mere shore. The pup is moved to a shore tile in `w_ridgemere_8_6`, about (60,120), snapped at authoring.
- **Giver:** Oriel Blossomby `cty_oriel_blossomby`, f 50, picks apples at Meresend. Coldwater (light).
  - Home `cn_coldwater_cottage_1`; work `fs_orchard_coldwater_store`. Alive, placed, reachable.
  - Why her: she is devout and curious and "quotes old sayings for everything". She "came along the road as a young wolf with nothing". She works the same orchard and minds its pup while his mother is out in the far rows. The mother is not part of this quest and is never named.
- **Pup:** Edric Harvestley `tn_sm_edric_harvestley`, m 10, of Meresend Orchard House; he "can tell an apple by its smell". Alive and placed (a pup at play has no work place).
- **Premise:** Edric follows his nose wherever it takes him. He's down the shore after the wild apples, too near the camp that keeps re-forming at the Upper Mere.
- **Objectives:**
  - `find`: Find Edric along the mere shore (`story signal`; P7 `near resident` `tn_sm_edric_harvestley`).
  - `home`: Bring him back to the orchard (`story signal`; P7 `near resident` `cty_oriel_blossomby`, after `find`).
  - `bounty` (optional): Bring down a bandit while you're there (`bandit falls`; `{cell: w_ridgemere_8_6}`): the natural camp or a story camp alike (decisions §4.1).
- **Triggers:** P7 (`edric_lost`, `edric_found`, `edric_home`, `edric_let_go`), P1, P2, P3, and the bounty feed.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `apples` ×3 and practice in watch.
  - `bonus`: the deed "brought down a bandit on the Upper Mere shore".
  - Main deed: "tracked a lost pup along the Upper Mere shore".
- **Feeds:**
  - +3 / +3.
  - `bounty` adds **`nh_threat` −1**. The `8_6` camp sits under the campaign's Upper Mere post (`campaign.md` §5.2).
- **Needs:** J1, for the per-wolf hint.

**Dialogue** (Oriel, coldwater):
- **ask:** "Are you looking for someone?"
- **say:** "#greet#. The orchard's pup has slipped me again, and his mother's out in the far rows. He can smell an apple a mile off and follows his nose wherever it goes. Down the shore by the wild trees, I'd wager. A pup that's missed is a pup that's loved, they say. Fetch him back, eh?"
- **accept:** "I'll find him." **decline:** "I can't today." **handIn:** "Here's your pup, nose and all."
- **npc.offer** `edric-where` (sent on accept, P7):
  - **words:** "Which way does his nose take him?"
  - **reply:** "Downwind of anything sweet. The wild trees on the mere shore drop their fruit early; follow the fallers. And mind the camp up the shore, eh? There's wolves up there with no orchard of their own."
- **thanks:** "There he is, nose first. #thanks#, #address#. Here's apples, the good ones off the bough. Still water finds its way back to the lake, the old ones say, and a pup finds his way back to the orchard. Mostly with help."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=coldwater self.dialect=coldwater` · on | The Meresend pup wanders after apples. Wherever the wind smells sweet, that's where he'll be, eh? |
| news · `here.region=coldwater self.dialect=coldwater` · on | Mind the mere shore past the orchards. There's a camp up there that won't stay broken. |
| news · `here.region=coldwater self.dialect=coldwater` · on (it's taken: points to 21) | If the Meresend pup's already being looked for, the bakehouse wants loaves carried to the landing, eh? |
| gossip · `here.region=coldwater self.dialect=coldwater` · last=apples | The orchard pup was found down the mere shore again. They ought to tie a bell on him, eh? |

---

#### 24. `sq-straw-for-the-pens`: Straw for the Lambing Pens
- **Tier:** small, 15–20 minutes. Repeat `daily`, `join.cap` 4. Board `coldwater`.
- **Where:** the Larkdowns shearing barn `fs_sheep_downs_barn` (door `w_ridgemere_9_8` (223,105)). Region `fold` = `w_ridgemere_9_8` (213,95)–(233,115).
- **Giver:** Rusk Downfield `tn_lf_rusk_downfield`, m 57, raises sheep at Larkdowns. Coldwater (light).
  - Home `fs_sheep_downs_home`. Alive, placed, reachable.
  - Why him: he is warm and gentle, "hums old marching songs", and "dreams of a farm of their own".
- **Premise:** Ewes lamb better on clean straw, and the pens are down to dust. Bring three bundles, and walk the fold with him at dusk so the flock learns your scent.
- **Objectives:**
  - `straw`: Bring three bundles of straw (`gift`; `{target: tn_lf_rusk_downfield, item: straw}`; goal 3, quantity). Every town's stores keep straw.
  - `dusk` (optional): Walk the fold at dusk (`story signal`; P5 on `region entered` `{target: fold}` with `calendar is period dusk`).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: practice in labour.
  - `bonus`: `wool` ×1.
  - Deed: "brought clean straw for the Larkdowns lambing pens".
- **Feeds:** +3 / +3.
- **Needs:** none.
  - Salt was the first idea, but no store or till in Ridgemere 3 holds salt at the checkpoint, so it was dropped. The only salt in play is what quest 43 carries.

**Dialogue** (Rusk, coldwater):
- **ask:** "Your flock looks restless."
- **say:** "#greet#, #address#. Ewes lamb better on clean straw, and my pens are down to dust. Three bundles from the stores, eh? And if you're about at dusk, walk the fold with me. A wolf the flock knows is a wolf they don't run from."
- **accept:** "Three bundles of straw." **decline:** "Another day." **handIn:** "Straw for your pens."
- **thanks:** "#thanks#. That's a full net for the flock. One day I'll have a fold of my own, and you'll be the first I ask to walk it. Calm water."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=coldwater self.dialect=coldwater` · on | The Larkdowns shepherd pays for straw for his pens. Calm sort, eh? He hums to the ewes. |
| gossip · `here.region=coldwater self.dialect=coldwater` · last=pens | The Larkdowns pens have fresh straw. The ewes are settled, eh? Somebody walked the fold with him. |

---

#### 25. `sq-strayed-ewe`: Wool on the Thorns (scent-tracking on the downs)
- **Tier:** small, 15–25 minutes. Repeat `weekly`, `join.cap` 2 (two trails), `fail: [{afterDays: 1}]`; with N4, `join.again: 7` (decisions §8.1). Not on the board.
- **Where:** the Larkdowns Shearing Barn `fs_sheep_downs_barn` (door `w_ridgemere_9_8` (223,105)), and the downs above it in the same cell. The fold region `fold` of quest 24 is beside it.
  - **Two trails, one per copy:** region `trail_n` = `w_ridgemere_9_8` (226,60)–(250,100) and region `trail_s` = (180,112)–(220,150).
  - On accept, the trigger `ewe_trail` checks the story switch `ewe_n_busy`. If it is off, it turns it on, turns on the player switch `ewe_on_north` of `$actor`, and places three wool tufts with `item.place {item: wool, quantity: 1}` at about (230,94), (238,80) and (244,66). Otherwise it turns on `ewe_s_busy` and places them at about (214,118), (202,130) and (190,142). Every tile is snapped to a standable one at authoring.
  - On `quest completed`, `quest failed` and `quest abandoned` (`match.detail`), the matching busy switch is turned off again, and the player switch with it.
- **Giver:** Brock Marshholm `cty_brock_marshholm`, m 36, shepherd at Larkdowns. **Fenshore-raised** with a broad accent, so he speaks **mirebridge** (decisions §1.3).
  - Home `cn_coldwater_cottage_3`; work `fs_sheep_downs_barn`. Alive, placed, reachable.
  - Why him: he is devout and curious, "keeps a tally of favours owed", and "came along the road as a young wolf with nothing, and is quietly proud of what was built since". The old giver holds `lq-old-brown`; Brock is another Coldwater shepherd.
- **Premise:** A ewe has gone up onto the downs and the flock won't settle. She'll have left wool on every thorn: follow the tufts and say where they lead.
- **Objectives:**
  - `tufts`: Follow the wool up the downs: three tufts (`story signal` `tuft`, goal 3). A trigger on `item picked up` `{item: wool, cell: w_ridgemere_9_8}`, with the quest active and `wolf in region` of the wolf's own trail (`trail_n` if `ewe_on_north` is on, else `trail_s`), runs `objective.progress amount 1`. So one wolf's tufts never tick the other's copy.
  - At the hand-in, the wolf says where the trail ends.
- **Triggers:**
  - `ewe_trail` (on accept), `ewe_tuft`, `ewe_free` (the busy switches).
  - P1 (credits **Coldwater**), P2, P3.
- **Rewards:**
  - P3: 3 pennies, `town:coldwater`.
  - `rewards`: practice in travel. The wolf keeps the wool.
  - Deed: "tracked a strayed ewe up the Larkdowns".
- **Feeds:** `nh_realm` +3, `nh_voice_coldwater` +3. The fold is a Coldwater farm, as in 24.
- **Needs:**
  - **J9**: a tagged tuft, so stray wool doesn't count and placed tufts are taken back when the run ends. Today `item.place` is not undone, and one wool per tuft stays on the downs if the wolf gives up. That is small litter.
  - SQ1 would let the objective match the region directly.

**Dialogue** (Brock, mirebridge):
- **ask:** "Lost one of your flock?"
- **say:** "#greet#, #address#. One of my ewes is off up the downs, and the flock won't settle without her, no? She'll have left wool on every thorn she passed. Follow the tufts, bring me three, and tell me where they lead. I keep a tally, and you'll be on the good side of it."
- **accept:** "I'll follow the wool." **decline:** "Not today." **handIn:** "The tufts lead up over the downs."
- **thanks:** "Up over the downs, is it? I'll have her home before dark. #thanks#. That's a mark in my tally, fen-sure, and I keep it honest. I came up this road with nothing, and I've counted every ewe in that fold twice. Dry feet."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=coldwater self.dialect=coldwater` · on | The fen-born shepherd at Larkdowns loses ewes up the downs, eh? They leave wool on the thorns; a good nose follows it. |
| news · `here.region=coldwater self.dialect=coldwater` · on (it's taken: points to 24) | If both his strays are being tracked, the other Larkdowns shepherd wants straw for the pens, eh? |
| gossip · `here.region=coldwater self.dialect=coldwater` · last=ewe | The Larkdowns ewe's home. Somebody followed her wool all the way up, eh? |

---

### 2.4 Oakhaven (program `nh-pack.small.north`)

Oakhaven residents speak **oakhaven**: "Shade on you", "Root well", sapling, ", mm?", "good bark", "Bark and branch". The purse is `town:oakhaven` (247 pennies), day cap 24 (P3). Feed: +2 `nh_realm` and +2 `nh_voice_oakhaven`. Oakhaven has only 8 residents and 4 who aren't campaign cast; every one of those already carries a pack quest, so 26 is **board-only** (decisions §1.2.6). None is the innkeeper.

---

#### 26. `sq-rope-for-the-drag`: Rope for the Drag (board-only)
- **Tier:** small, 20–30 minutes (rope is bought in the city). Repeat `daily`, `join.cap` 4. **Board-only** on `board:oakhaven` (decisions §1.2.6): no `giver`, no `offer`, no hand-in.
- **Where:** the Oakhaven square, region `oak_square` = `w_ridgemere_3_4` (236,188)–(252,204), and the Sound Road. The bounty is at Alder Mere (`w_ridgemere_5_4`).
- **The offer:** P9 `drag_offer` (§1.3) sends a journal `quest.offer` to a wolf who enters `oak_square`, at most once a game day.
- **Board text** (the quest's `summary`, no resident's words): "The Oakhaven carter's drag rope has gone to threads. Two coils from the Ridgemere ropewalk, carried along the Sound Road. Rope is sold only in the city."
- **Target:** Tristram Rainridge `cty_tristram_rainridge`, m 37, carter at Oakhaven, works the square (`w_ridgemere_3_4` (245,197)); home `cn_oakhaven_cottage_1`. Alive, placed, reachable. He gives `mq-windthrown-cedar`, so he holds no hub item here; this is his only target role (decisions §1.3).
- **Premise:** The carter's drag rope has gone to threads, and cedar won't drag itself to the Sound. Rope is sold only in the city (the Ropewalk, the chandleries), so this is a walk to Ridgemere and back. If the road wolves at Alder Mere get a bloody nose on the way, so much the better.
- **Objectives:**
  - `rope`: Give two coils of rope to the Oakhaven carter (`gift`; `{target: cty_tristram_rainridge, item: rope}`; goal 2, quantity).
  - `bounty` (optional): Bring down a bandit at Alder Mere (`bandit falls`; `{cell: w_ridgemere_5_4}`), the natural camp or a story camp there alike (decisions §4.1).
- **Close:** on `quest completed` (`match.detail`), `story.tell {to: wolf, player: $actor}` with no `from`: "The carter hitches the new rope to the drag, and the cedar starts down to the Sound again."
- **From C5:** Tristram's `npc.greet` to that wolf (oakhaven): "Good bark, you. The drag's moving on your rope, mm? My grandmother would've called you sound wood."
- **Rewards:**
  - P3: 3 pennies, `town:oakhaven`.
  - `rewards`: practice in labour.
  - `bonus`: the deed "brought down a bandit on the Oakhaven road".
  - Main deed: "brought rope for the Oakhaven timber drag".
- **Feeds:**
  - +2 / +2.
  - `bounty` adds **`nh_threat` −1**. The `5_4` camp sits under the campaign's Oakhaven road post, proj-2.
- **Needs:** none. C5 for the greeting.

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=oakhaven self.dialect=oakhaven` · on | The carter's drag rope's given out, mm? Rope comes from the city ropewalk. The notice is on the board by the square. |
| news · `here.region=oakhaven self.dialect=oakhaven` · on | The road wolves sit at Alder Mere, on the way to the city. Go in a pair if you're carrying. |
| gossip · `here.region=oakhaven self.dialect=oakhaven` · last=drag | The cedar's moving again, mm? The carter got his rope. |
| gossip · `self.id=cty_tristram_rainridge` · last=drag (his own line in his window, with C8) | Rope's good, mm? Somebody walked it from the city. Root well, whoever they were. |

---

#### 27. `sq-venison-for-the-cut`: Venison for the Cut (hunting)
- **Tier:** small, 15–30 minutes. Repeat `daily`, `join.cap` 4. Board `oakhaven`.
- **Where:** the Oakhaven cut in the Rainwood, `w_ridgemere_4_4` (work spot (28,37)).
- **Giver:** Frost Colddrift `cty_frost_colddrift`, f 50, fells timber at the Oakhaven cut. Oakhaven (light).
  - Home `cn_oakhaven_cottage_1`. Alive, placed, reachable.
  - Why her: she is guarded and devout, "keeps a tally of favours owed", and "is saving coin toward something no one else knows about".
- **Premise:** The fellers live on bread, and a wolf can't fell cedar on bread. A roe deer out of the Rainwood would feed the cut for days.
- **Objectives:**
  - `deer`: Bring down a roe deer in the Rainwood by the cut (`hunted`; `{target: roe_deer, cell: w_ridgemere_4_4}`).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `jerky` ×2 and practice in watch.
  - Deed: "brought venison to the loggers at the Oakhaven cut".
- **Feeds:** +2 / +2.
- **Needs:** none. Roe deer live in forest and scrub and flee.

**Dialogue** (Frost, oakhaven):
- **ask:** "The fellers look tired."
- **say:** "#greet#. The loggers eat bread and more bread, and a wolf can't fell cedar on bread, mm? A roe deer out of the Rainwood would feed the cut for days. They're shy. Come at them downwind, slow through the trees, and pray the Bright One keeps the wind steady."
- **accept:** "A roe deer for the cut." **decline:** "Not today." **handIn:** "Venison for the fellers."
- **thanks:** "#thanks#. The Bright One sent you, I'd say, though you'd say you walked. That's a favour I owe, and I keep count. Shade keep you."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=oakhaven self.dialect=oakhaven` · on | The fellers at the cut want venison. Roe deer keep to the deep trees, mm? Downwind, and slow. |
| gossip · `here.region=oakhaven self.dialect=oakhaven` · last=venison | The cut ate venison today, mm? Proper food for proper work. |

---

#### 28. `sq-sleepless-logger`: A Quiet Head Before the Storm
- **Tier:** small, 10–20 minutes. Repeat `weekly`, `join.cap` 6. Board `oakhaven`.
- **Where:** the cut `w_ridgemere_4_4` (29,35), or Oakhaven cottage 1.
- **Giver:** Ingram Ashleaf `cty_ingram_ashleaf`, m 40, logger at the Oakhaven cut. Oakhaven (light).
  - Home `cn_oakhaven_cottage_1`. Alive, placed, reachable.
  - Why him: he is warm and anxious and **"sleeps badly before storms"**.
- **Premise:** He can feel a storm coming in his teeth and won't sleep a wink. Chamomile or valerian: anything to quiet the head.
- **Objectives:**
  - `calm`: Bring two sprigs of chamomile, or valerian (`gift`; `{target: cty_ingram_ashleaf, item: chamomile}`; goal 2, quantity).
    - P4 completes it from `valerian` or `valerian_draught`, by `gift`/`gift again` to Ingram with `any [field item = …]`.
    - Chamomile is a wildflower of spring and summer. Oakhaven Stores keeps chamomile and valerian (4 each at the checkpoint), and the physics in the city keep valerian draught.
  - `storm` (optional): Be with him when the storm breaks (`story signal`). A trigger on `weather changed`[w] `{detail: storm}`, through `each over participants` with the quest active and `wolf in town {town: oakhaven}`, runs `objective.complete`.
- **Rewards:**
  - P3: 2 pennies.
  - `rewards`: practice in service.
  - `bonus`: a `story.tell {to: wolf, player: $actor}` with no `from` (narration): "He is asleep before the first thunder, snoring like a sawpit."
  - Deed: "brought a calming draught to a sleepless logger".
- **Feeds:** +2 / +2.
- **Needs:** none.

**Dialogue** (Ingram, oakhaven):
- **ask:** "You look like you haven't slept."
- **say:** "#greet#. There's a storm coming, I can feel it in my teeth, and I won't sleep a wink before it. Chamomile, if the meadows have any, or valerian from the stores. Anything to quiet the head, mm?"
- **accept:** "I'll find you something." **decline:** "I can't help just now." **handIn:** "Something to help you sleep."
- **thanks:** "#thanks#. Root and rain, I'll sleep now, even if the sky falls in. You've a good root in you."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=oakhaven self.dialect=oakhaven` · on | The young logger can't sleep before a storm. Chamomile helps, mm? It grows with the wildflowers in spring and summer. |
| gossip · `here.region=oakhaven self.dialect=oakhaven` · last=sleep | The logger slept through the last storm, he says. First time in years, mm? |

---

### 2.5 Frosthollow (program `nh-pack.small.north`)

Frosthollow-raised residents speak **frosthollow** (the town's talk is in it; quest 30's giver is Larkfield-raised and speaks plain): "Warm hearth", "Keep your lamp lit", deep, stone or cousin, whelp, ", eh", "Black seam", "Cold iron", "Lamp's thanks". The purse is `town:frosthollow` (247 pennies), day cap 24 (P3). Feed: +3 `nh_realm` and +3 `nh_voice_frosthollow`. Frosthollow has 5 residents who aren't campaign cast, and every one already carries a pack quest, so 29 is **board-only** (decisions §1.2.6).

---

#### 29. `sq-lamp-oil-for-the-pit`: Wood for the Pass House (board-only)
- **Tier:** small, 15–20 minutes. Repeat `daily`, `join.cap` 4. **Board-only** on `board:frosthollow` (decisions §1.2.6). The id is kept for the registry and `decisions.md`; the premise is new (decisions §7.1): **not** lamp oil (`mq-lamp-oil-share`), not props (`mq-low-drift-props`), not the clamp (30), and not Deepvein.
- **Where:** the Frosthollow square, region `frost_square` = `w_ridgemere_9_3` (0,20)–(12,36), and the Pass House `cn_frosthollow_guest_inn` (door `w_ridgemere_8_3` (241,45)), where carters held below the Pass wait for the road.
- **The offer:** P9 `passwood_offer` on `region entered` `frost_square`, with the player timer `t_passwood_offer`.
- **Board text** (the `summary`): "The Pass House burns through its woodpile while the carters wait on the Pass Road. Four armfuls of firewood for the carters' stack; meal for their pot, if you have it."
- **Target:** Ebba Coalgate `cty_ebba_coalgate`, f 36, carter at Frosthollow, who works on the square (`w_ridgemere_9_3` (5,29)); home `cn_frosthollow_cottage_1`. Alive, placed, reachable. She gives `mq-low-drift-props` and is a target in the Frosthollow large line, so this is her second and last target role (decisions §7.1: no more than one other).
- **Premise:** Carters stopped below the Pass wait at the Pass House for the road to open, and its stack goes faster than the town can cut. The Frosthollow carter keeps the carters' stack.
- **Objectives:**
  - `look`: Go into the Pass House and see how low the pile is (`enter place`; `{target: cn_frosthollow_guest_inn}`).
  - `wood`: Give four armfuls of firewood to the Frosthollow carter (`gift`; `{target: cty_ebba_coalgate, item: firewood}`; goal 4, quantity; `after: [look]`). Firewood is foraged in any wood, and 59 purses hold 165 armfuls.
  - `meal` (optional): Two of meal for the carters' pot (`gift`; `{target: cty_ebba_coalgate, item: meal}`; goal 2, quantity). Meal is in 382 purses.
- **Close:** on `quest completed`, `story.tell {to: wolf, player: $actor}`, no `from`: "The carters' stack by the Pass House door is high again, and somebody inside starts a song."
- **From C5:** Ebba's `npc.greet` (frosthollow): "Lamp's thanks, cousin. The Pass House fires kept, and the carters with them. Warm hearth to you."
- **Rewards:**
  - P3: 3 pennies, `town:frosthollow`.
  - `rewards`: practice in labour.
  - `bonus`: `porridge` ×1 (from the carters' pot).
  - Deed: "carried firewood up to the Pass House".
- **Feeds:** +3 / +3.
- **Needs:** none. C5 for the greeting.

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=frosthollow self.dialect=frosthollow` · on | The Pass House burns through wood while the carters wait on the road. The carter on the square keeps their stack, stone. |
| news · `here.region=frosthollow self.dialect=frosthollow` · on | Wood for the Pass House? The notice is on the board, cousin. Any deadwood burns, and the slopes are full of it. |
| gossip · `here.region=frosthollow self.dialect=frosthollow` · last=passwood | The Pass House stack's high again. Somebody carried wood up, cousin. |
| gossip · `self.id=cty_ebba_coalgate` · last=passwood (her own line in her window, with C8) | The stack's high and the carters are singing. Deep thanks to whoever carried it, eh. |

---

#### 30. `sq-clamp-watch`: Sit the Clamp (a night watch)
- **Tier:** small, about 20 minutes around the hour. Repeat **`weekly`** (was daily), `join.cap` 2 (two clamps), `fail: [{afterDays: 1}]`; with N4, `join.again: 7` (decisions §8.1). Board `frosthollow`.
- **Where:** the Upper hearths, `w_ridgemere_10_3`. Region `clamp` = (76,165)–(90,179) holds both clamps: Iris's beside her work spot (82,171), and the next one at (83,173).
- **Giver:** Iris Fernhaven `cty_iris_fernhaven`, f 26, collier at the Upper hearths. **Plain**: Larkfield-raised with accent none (decisions §1.3).
  - Home `cn_frosthollow_cottage_1` (door `w_ridgemere_9_3` (11,8)); work `w_ridgemere_10_3` (82,171). Alive, placed, reachable.
  - Why her: she is tired and generous and "is always a little late". She "is the third generation to hold this trade and feels every one of them watching". The old giver holds `mq-lamp-oil-share`.
- **Premise:** A charcoal clamp wants watching all night, or it breaks out and burns to ash. Sit the first hour after midnight so the collier can sleep.
- **Objectives:**
  - `watch`: Sit the clamp through the first hour after midnight (`story signal`). P6 on `hour`[w] `{detail: "1"}`: each participant with the quest active and `wolf in region clamp` gets `objective.complete`. Two wolves can sit the two clamps on the same night.
  - `wood` (optional): Bring two armfuls of firewood for the clamp's bank (`gift`; `{target: cty_iris_fernhaven, item: firewood}`; goal 2, quantity).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `charcoal` ×2 and practice in watch.
  - Deed: "sat the night watch on the Upper hearths clamp".
- **Feeds:** +3 / +3.
- **Needs:** none.
  - `hour` is watched, and the pack lists it. Holding the wolf for the whole hour would need a `region left` fail check; the design accepts presence at the hour's start.

**Dialogue** (Iris, plain):
- **ask:** "Does that clamp need watching?"
- **say:** "#greet#. A charcoal clamp wants watching all night, or it breaks out and burns to ash. My mother kept these hearths, and hers before her, and neither ever lost a clamp. Sit the first hour after midnight at the clamps on the Upper hearths, and I'll sleep."
- **accept:** "I'll sit your clamp." **decline:** "Not tonight." **handIn:** "Your clamp held."
- **thanks:** "#thanks#. Not a crack in her. I'm late for everything else, but never for a clamp. Take two sacks of the good stuff. My grandmother would have said you'd make a collier, and she said that of nobody."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=frosthollow self.dialect=frosthollow` · on | The young collier on the Upper hearths wants her clamp sat at night. Deep cold up there; take a fur. |
| news · `here.region=frosthollow self.dialect=frosthollow` · on (it's taken: points to 31) | If both clamps are spoken for, the limeburner wants the goats off his scree, stone. |
| gossip · `here.region=frosthollow self.dialect=frosthollow` · last=clamp | The Upper hearths clamp came through whole. Somebody sat it, stone. |

---

#### 31. `sq-scree-goats`: Goats on the Scree (hunting)
- **Tier:** small, 15–30 minutes. Repeat `weekly`, `join.cap` 4. Board `frosthollow`.
- **Where:** the high lime quarry, `w_ridgemere_6_3` ("The Greenholt, Lime quarry"), work spot (53,20). The bounty is the camp that keeps forming there (`camp_w_ridgemere_6_3`).
- **Giver:** Leif Driphand `cty_leif_driphand`, m 31, burns lime at the high lime quarry. Frosthollow (light).
  - Home `cn_frosthollow_cottage_1`. Alive, placed, reachable.
  - Why him: he is ambitious and unhurried, "distrusts anything from the city", and "fears the deep woods".
- **Premise:** Mountain goats on the quarry scree are kicking stones down on the kiln path. One brought down and the rest will keep off.
- **Objectives:**
  - `goat`: Bring down a mountain goat on the quarry scree (`hunted`; `{target: ibex, cell: w_ridgemere_6_3}`).
  - `bounty` (optional): Bring down a bandit of the quarry camp (`bandit falls`; `{cell: w_ridgemere_6_3}`). Matching the cell, not the camp id, counts a natural camp and a story camp there alike (decisions §4.1).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: practice in watch.
  - `bonus`: the deed "brought down a bandit below the lime quarry".
  - Main deed: "drove the goats off the lime quarry scree".
- **Feeds:**
  - +3 `nh_realm` / +3 `nh_voice_frosthollow`.
  - `bounty` adds **`nh_threat` −1**. The lime quarry is Frosthollow's work, and the campaign's `6_3` camp row now credits **Frosthollow** too (decisions §4.4, critic P2-21), so the pack and the campaign agree.
- **Needs:** none.
  - Ibex are confirmed for the quarry: `6_3` has 1,304 rock and 3,984 snow tiles, and the ibex habitats are rock 3 and snow 1 (`animals.json`). Ibex fight when cornered and yield 2 horn, which the wolf keeps.

**Dialogue** (Leif, frosthollow):
- **ask:** "Stones on the path again?"
- **say:** "#greet#. Mountain goats on the quarry scree, kicking stones down on my kiln path. One of them brought down and the rest'll keep off, stone. And mind the road wolves that camp below the quarry. They're no friends of Frosthollow."
- **accept:** "One goat off your scree." **decline:** "Not this week." **handIn:** "Your scree's clear."
- **thanks:** "Lamp's thanks, cousin. Solid work. The kiln path's clear, and the goats know it. You're not as soft as most from down the valley."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=frosthollow self.dialect=frosthollow` · on | The limeburner wants the goats off his scree. They fight when they're cornered, stone. Go in a pair. |
| gossip · `here.region=frosthollow self.dialect=frosthollow` · last=goats | The quarry path's clear of goats. The limeburner's almost smiling, eh. |

### 2.6 Larkfield (program `nh-pack.small.south`)

Larkfield givers speak **larkfield**: "Fair price to you", "Good trading", lambkin, short-weighed, full weight, top-weight, "Scales and silver", ", eh?". The purse is `town:larkfield` (248 pennies), day cap 24 (P3). Feed: +3 `nh_realm` and +3 `nh_voice_larkfield`. None of the campaign's Larkfield cast gives here: Aspen, Dun, Rook and the smith are left free.

---

#### 32. `sq-plumtree-badger`: The Badger Under the Apple Store (hunting)
- **Tier:** small, 15–30 minutes. Repeat `weekly`, `join.cap` 3. Board `larkfield`.
- **Where:** Plumtree orchard, the apple store `fs_orchard_larkfield_store` (door `w_ridgemere_7_10` (81,181)). The hunt is in cell `w_ridgemere_7_10`.
- **Giver:** Fallow Plumfield `cty_fallow_plumfield`, f 52, tends the orchard at Plumtree. Larkfield (light).
  - Home `fs_orchard_larkfield_home`. Alive, placed, reachable.
  - Why her: she is easily offended and suspicious, "has opinions about everyone's grandmother", and "fears the deep woods", so she won't go after it herself.
- **Premise:** A badger has dug in under her apple store and is eating her short every night. Badgers don't run; they turn and fight.
- **Objectives:**
  - `badger`: Bring down the badger by the Plumtree store (`hunted`; `{target: badger, cell: w_ridgemere_7_10}`).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `dried_apples` ×2 and practice in watch.
  - Deed: "dug the badger out from under the Plumtree apple store".
- **Feeds:** +3 / +3.
- **Needs:** none.
  - A badger gives a pelt, 2 fat scraps and meat. The fat answers Coldwater's kettle quest (22).

**Dialogue** (Fallow, larkfield):
- **ask:** "Something's been at your apples."
- **say:** "#greet#. There's a badger under my apple store, and it's eating me short-weighed every night. Badgers don't run, mind; they turn and fight. Bring it down and I'll pay full weight. Scales and silver, I'd go myself, but I'll not go near those trees."
- **accept:** "One badger, gone." **decline:** "Not this week." **handIn:** "Your badger's dealt with."
- **thanks:** "#thanks#. Full weight, as promised, and dried apples on top. That's a bargain you won't get in the city, eh? My grandmother would've done it with a broom. Yours, I couldn't say."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=larkfield self.dialect=larkfield` · on | The Plumtree orchard has a badger under the store. Fair warning: they're top-weight and they don't back off. |
| news · `here.region=larkfield self.dialect=larkfield` · on | A badger's fat is worth the bite. The soap wolves at the lake pay for it, eh? |
| gossip · `here.region=larkfield self.dialect=larkfield` · last=badger | Somebody shifted the Plumtree badger. Full weight to them, eh? |

---

#### 33. `sq-swill-for-southwood`: Swill for Southwood (an errand)
- **Tier:** small, 10–20 minutes. Repeat `daily`, `join.cap` 4. Board `larkfield`.
- **Where:** from Larkfield Stores `cn_larkfield_store` to the Southwood pig yard `fs_pigs_south_sties` (door `w_ridgemere_5_8` (97,11)).
- **Giver:** Njal Rustwick `cty_njal_rustwick`, m 41, swineherd at Southwood. Larkfield (light).
  - Home `cn_larkfield_cottage_2`. Alive, placed, reachable.
  - Why him: he is loyal and tired, **"keeps a tally of favours owed"**, and "owes a debt to someone in Ridgemere".
- **Premise:** Southwood's pigs eat more than Larkfield does, and the granary is short-weighing the yard this month. Carry oats and meal out from the stores.
- **Objectives:**
  - `meal`: Bring two of meal (`gift`; `{target: cty_njal_rustwick, item: meal}`; goal 2, quantity). Larkfield Stores keeps meal and oats; it has no vegetables.
  - `oats`: Bring two of oats (`gift`; `{target: cty_njal_rustwick, item: oats}`; goal 2, quantity).
- **Rewards:**
  - P3: 2 pennies.
  - `rewards`: practice in labour.
  - Deed: "carried swill out to the Southwood pig yard".
- **Feeds:** +3 / +3.
- **Needs:** none.

**Dialogue** (Njal, larkfield):
- **ask:** "Your pigs sound hungry."
- **say:** "#greet#. Southwood's pigs eat more than the whole of Larkfield, and the granary's short-weighing us this month. Two of meal and two of oats from the stores, carried out to the yard, eh? I keep a tally. You'll be on the good side of it."
- **accept:** "Meal and oats, coming." **decline:** "Not today." **handIn:** "Swill for the yard."
- **thanks:** "#thanks#. That's one in the tally, friend, and I don't forget a mark. I owe a wolf in Ridgemere more than I'll say. Better somebody owes me something back. Good trading."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=larkfield self.dialect=larkfield` · on | The Southwood swineherd keeps a tally of who's helped him. Better to be on it than off it, eh? |
| gossip · `here.region=larkfield self.dialect=larkfield` · last=swill | Southwood's pigs ate well today. The swineherd marked somebody down for it. |

---

#### 34. `sq-linen-for-the-press`: Old Linen for the Paper Vats
- **Tier:** small, 10–20 minutes. Repeat `daily`, `join.cap` 4. Board `larkfield`.
- **Where:** the Larkfield Print House `cn_larkfield_press` (door `w_ridgemere_6_9` (138,26)). Linen cloth is kept by every town's stores; Larkfield Stores holds 4 at the checkpoint.
- **Giver:** Fern Mistshore `tn_cw_fern_mistshore`, f 31, paper vat hand at the Print House. Larkfield (light).
  - Home `cn_larkfield_cottage_4`. Alive, placed, reachable.
  - Why her: she is stubborn and generous, and "carts goods to Ridgemere and resents every mile". She isn't the printer, Dun Harvestley, who is a Keeper fallback.
- **Premise:** Good paper is made from old linen beaten to pulp, and the vats are low. Two lengths of linen cloth.
- **Objectives:**
  - `linen`: Bring two lengths of linen cloth (`gift`; `{target: tn_cw_fern_mistshore, item: linen_cloth}`; goal 2, quantity).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `paper` ×2 and practice in craft.
  - Deed: "brought linen for the Larkfield paper vats".
- **Feeds:** +3 / +3.
- **Needs:** none.
  - `rags` was the first idea, but no store, till or household holds rags at the checkpoint, so it was dropped.

**Dialogue** (Fern, larkfield):
- **ask:** "What goes in those vats?"
- **say:** "#greet#. Old linen, beaten to pulp. Good paper's made from linen, and the vats are low. Two lengths of linen cloth, eh? I'll not cart to Ridgemere for it again this week. The press pays full weight for good linen."
- **accept:** "Two lengths of linen." **decline:** "Not today." **handIn:** "Linen for your vats."
- **thanks:** "#thanks#. Into the vat. The printer's got sheets to run, all about who sits in the Palace now, and every one of them starts in a linen bag. Fair price to you."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=larkfield self.dialect=larkfield` · on | The press buys old linen. You'd not think a rag of it was worth anything till you see them weigh it, eh? |
| news · `here.region=larkfield self.dialect=larkfield` · on | Linen for the vats? The stores keep a bolt or two. Buy fair, sell to the press, and everybody's full weight. |
| gossip · `here.region=larkfield self.dialect=larkfield` · last=linen | The vats were full today. The printer's running sheets again, eh? |

---

#### 35. `sq-mill-race`: Reeds in the Mill Race (foraging)
- **Tier:** small, 10–20 minutes. Repeat `daily`, `join.cap` 4. Board `larkfield`.
- **Where:** Longmead Mill `fs_grain_long_mill` (door `w_ridgemere_6_9` (213,237)), on the Long River bank. The reeds are foraged in cell `w_ridgemere_6_9`.
- **Giver:** Gorse Hedgewell `cty_gorse_hedgewell`, m 53, grinds the corn at Longmead. Larkfield (light).
  - Home `cn_larkfield_cottage_1`. Alive, placed, reachable.
  - Why him: he is warm and dreamy and **"hums old marching songs"**. He is not Rook Wheatfield, who farms Longmead and is campaign cast.
- **Premise:** The mill race is choking on reeds and the wheel turns light. Pull four bundles out by the bank.
- **Objectives:**
  - `reeds`: Pull four bundles of reeds by the Longmead mill (`forage`; `{item: reeds, cell: w_ridgemere_6_9}`; goal 4, quantity).
  - Then hand them in.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `flour` ×2 and practice in labour.
  - Deed: "cleared the reeds from the Longmead mill race".
- **Feeds:** +3 / +3. Longmead is where the campaign's Ch 7 ford siege is fought, so this is a quiet first visit.
- **Needs:** none. Reed beds (tile `E`) give reeds all year.

**Dialogue** (Gorse, larkfield):
- **ask:** "Your wheel sounds slow."
- **say:** "#greet#. The mill race is choking on reeds again and the wheel's turning light. Pull four bundles out by the bank near the mill, eh? I'd do it myself, but I start humming and forget why I'm standing in the water."
- **accept:** "Four bundles of reeds." **decline:** "Not today." **handIn:** "Your race is clear."
- **thanks:** "#thanks#. Hear that? Full weight on the wheel again. Take some flour; it's ground fine, top-weight. I'll hum you a march as you go."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=larkfield self.dialect=larkfield` · on | The Longmead miller wants the reeds pulled from his race. Wet work, but he pays in flour, eh? |
| gossip · `here.region=larkfield self.dialect=larkfield` · last=race | The Longmead wheel's turning sweet again. Somebody waded in for the miller. |

---

### 2.7 Southmeadow (program `nh-pack.small.south`)

Southmeadow givers speak **southmeadow**: "Good harvest", "Ripe days", sprout, ripe, blighted, a windfall, "Sheaf and sickle", "Bless you". The purse is `town:southmeadow` (236 pennies), day cap 23 (P3). Feed: +4 `nh_realm` and +4 `nh_voice_southmeadow` (×2 distance).

---

#### 36. `sq-smoke-for-the-hives`: Smoke for the Hives
- **Tier:** small, 10–20 minutes. Repeat `daily`, `join.cap` 4. Board `southmeadow`.
- **Where:** Larkholm, `w_ridgemere_8_11`: the farm keeps a row of skeps along the wall of the Larkholm Granary `fs_grain_lark_granary` (door (108,45)). The hives are now **Larkholm's** (decisions §1.3), not the Sunfields garth's, which belongs to Aster Cornley and to `mq-bee-garth-bear`. A bandit camp keeps forming over the boundary in the Sunfields (`w_ridgemere_7_12`).
- **Giver:** Hob Meadowfield `tn_sm_hob_meadowfield`, m 25, farmhand at Larkholm. Southmeadow (light).
  - Home Larkholm Farmhouse `fs_grain_lark_home` (door `w_ridgemere_8_11` (114,38)); work `fs_grain_lark_granary`. Alive, placed, reachable.
  - Why him: he is meticulous and proud and "hums old marching songs", and he "once walked all the way to Ridgemere and came back". He is not the farm's keeper (Iris Meadowfield, a Keeper, is left free).
- **Premise:** Bees go gentle in smoke, and the smoker is empty. Herbs to burn in it, wormwood if the season gives any. Keep your eyes open: there are wolves camped up the Sunfields road who aren't there for the honey.
- **Objectives:**
  - `herbs`: Bring three bunches of herbs for the smoker (`gift`; `{target: tn_sm_hob_meadowfield, item: herbs}`; goal 3, quantity).
  - `wormwood` (optional): Wormwood burns best (`gift`; `{target: tn_sm_hob_meadowfield, item: wormwood}`; goal 2, quantity). It grows in grass in summer and autumn.
  - `bounty` (optional): Bring down a bandit of the Sunfields camp (`bandit falls`; `{cell: w_ridgemere_7_12}`), natural or story camp alike (decisions §4.1).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `honey` ×1 and practice in craft. No purse holds honey at the checkpoint, so it is minted by `item.give`, one at a time.
  - `bonus`: `honey_cake` ×1.
  - Deed: "brought smoker herbs for the Larkholm skeps".
- **Feeds:**
  - +4 / +4.
  - `bounty` adds **`nh_threat` −1**. The `7_12` camp is credited to Southmeadow in the campaign.
- **Needs:** none.

**Dialogue** (Hob, southmeadow):
- **ask:** "Are those skeps yours?"
- **say:** "#greet#. Larkholm keeps a row of skeps along the granary wall, and I'm the one minds them. Bees go gentle in smoke, and my smoker's empty. Three bunches of herbs to burn; wormwood's best, if the season's given any. And mind the Sunfields road. There's wolves camped there who aren't after honey."
- **accept:** "Herbs for your smoker." **decline:** "Not today." **handIn:** "Herbs to burn, as asked."
- **thanks:** "#thanks#. Bless you. Here's honey off the Larkholm skeps, ripe as it gets. I walked to Ridgemere once and back on a comb like this. Mind the bees on the way out; they don't know you yet."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=southmeadow self.dialect=southmeadow` · on | The Larkholm farmhand keeps skeps by the granary. He wants herbs for his smoker; wormwood if there's any, summer and autumn. |
| news · `here.region=southmeadow self.dialect=southmeadow` · on | Mind the Sunfields road past Larkholm. There's a camp up there, blight take it, and it keeps coming back. |
| gossip · `here.region=southmeadow self.dialect=southmeadow` · last=hives | The Larkholm bees were gentle as anything today. Somebody filled the smoker, bless them. |

---

#### 37. `sq-hare-in-the-seed-rows`: The Miller's Wager (hunting)
- **Tier:** small, 15–30 minutes. Repeat `daily`, `join.cap` 4. Board `southmeadow`.
- **Where:** Larkholm Mill `fs_grain_lark_mill` (door `w_ridgemere_9_11` (109,50)). The hunt is in cell `w_ridgemere_9_11`.
- **Giver:** Emrys Stillshore `cty_emrys_stillshore`, m 50, grinds the corn at Larkholm. Southmeadow (broad).
  - Home `cn_southmeadow_cottage_1`. Alive, placed, reachable.
  - Why him: he is suspicious and blunt, **"cannot resist a wager"**, and "knows a story about one of the great Houses that would be unwise to repeat".
- **Premise:** Hares are eating the young wheat in his seed rows to stubble. One hare earns pay; two win his wager.
- **Objectives:**
  - `hare`: Bring down a hare in the Larkholm fields (`hunted`; `{target: hare, cell: w_ridgemere_9_11}`).
  - `wager` (optional): Take a second hare and win the miller's wager. A trigger on `hunted` `{target: hare, cell: w_ridgemere_9_11}`, with `objective is hare: done`, runs `objective.complete wager`.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: practice in watch.
  - `bonus`: the wager, `flour` ×2, and the deed "won the Larkholm miller's wager over the hares".
  - Main deed: "kept the hares off the Larkholm seed rows".
- **Feeds:** +4 / +4.
- **Needs:** none.
  - Hares live in grass, heather and crop ground. They flee fast, so two wolves drive and wait (`huntRole` driver and ambusher).

**Dialogue** (Emrys, southmeadow):
- **ask:** "Something's eaten your rows."
- **say:** "Good 'arvest. Hares in my seed rows, eating the young wheat to stubble. One hare, and I'll pay. Two, and you've won a wager, and I'll bet you a penny you can't. They go like a windfall rolling downhill."
- **accept:** "You're on." **decline:** "I'll not take your bet today." **handIn:** "Your rows are clear."
- **thanks:** "#thanks#. Sheaf and sickle, there's my penny gone. Don't tell the granary; they think I'm careful. I know a tale about a great House that'd curl your ears, but a wager's one thing and a tale's another."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=southmeadow self.dialect=southmeadow` · on | The Larkholm miller'll wager on anything, bless him. He's got one going on the hares in his seed rows. |
| news · `here.region=southmeadow self.dialect=southmeadow` · on | Hares sit tight and then go like a shot. Two of you: one to drive, one to wait in the furrow. |
| gossip · `here.region=southmeadow self.dialect=southmeadow` · last=hare | The miller lost his hare wager. He's telling everyone he let them win. |

---

#### 38. `sq-festival-garlands`: Garlands for the Square (a festival chore, with a shared rope)
- **Tier:** small, 10–20 minutes. `phases: ["festival"]` (days 40–46 of every season; §1.6), `lifetime: phase`, repeat `weekly` (so once per festival), no cap. Board `southmeadow`.
- **Where:** the Southmeadow square (`w_ridgemere_8_13` (100,20)). The giver works the Larkholm Granary (`w_ridgemere_8_11` (108,45)) and lives on the square.
- **Giver:** Willa Wheatcombe `cty_willa_wheatcombe`, f 39, farmhand at Larkholm. Southmeadow (light).
  - Home `cn_southmeadow_cottage_1`; work `fs_grain_lark_granary`. Alive, placed, reachable.
  - Why her: she is restless and tired and "distrusts anything from the city", and she "is the third generation to hold this trade and feels every one of them watching". The old giver keeps `lq-blight-and-boar`. She shares a first name with the campaign's Willa Coldwick, who lives in Coldwater, not here (decisions §1.2.5); no line names either.
- **Premise:** The festival is coming and the square is as bare as stubble. Every wolf's bunch goes on one rope, and when the rope reaches round the square, Southmeadow is ready.
- **Objectives:**
  - `green`: Bring two bunches of herbs for the garland (`gift`; `{target: cty_willa_wheatcombe, item: herbs}`; goal 2, quantity).
  - `flowers` (optional): Any flowers the season has left. P4 on `gift` to Willa with item `lavender`, `chamomile` or `rose_petals` runs `objective.complete flowers`.
  - `enter` (optional): Enter a contest at the Southmeadow festival. P5 on `festival entered` with `wolf in town {town: southmeadow}`.
- **The shared rope** (scales with the helpers):
  - A trigger on `quest completed` `{detail: sq-festival-garlands}` runs:
    - `add garlands 1`;
    - if `not in list sq_helpers $actor`: `list add sq_helpers $actor` and `add garland_goal 2`.
  - `garland_goal` is a story number, set to 6 on `story phase` `{target: festival}`, so the goal is always 6 + 2 × helpers, with no new operand.
  - A second trigger on `quest completed` `{detail: sq-festival-garlands}`, `once: visit`, `if compare garlands >= {var: garland_goal}`, runs:
    - `effect board.post {town: southmeadow, text: "The garland rope reaches right round the square. Southmeadow is ready for the festival. From the Larkholm farmhands.", days: 7}`;
    - `effect story.tell {to: participants}` with **no `from`** (narration, decisions §9 P2-22): "The garland rope reaches right round the Southmeadow square.";
    - `add nh_voice_southmeadow 4` and `add nh_realm 4`.
  - Willa's own words about the full rope are her window line `say[self.id=cty_willa_wheatcombe world.mem.sq_last=garland]` (with C8), and from C5 her `npc.greet` to each helper.
  - On `story phase` `{target: festival}`: `set garlands 0`, `set garland_goal 6` and `list clear sq_helpers`.
  - Tracker `garlands`: "Garlands on the rope", shown in this phase only.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: practice in craft.
  - `bonus`: `ribbon_collar` ×1 (once a year is enough: `once: member` on the bonus trigger).
  - Deed: "hung garlands on the Southmeadow square for the festival".
- **Feeds:** +4 / +4 each, and +4 / +4 once when the rope is full. The summer festival opens the campaign's Ch 4 (`campaign.md` §0.1).
- **Needs:** none today. C2 (`audience` operand) would let the goal follow the wolves signed in, not only the helpers.

**Dialogue** (Willa, southmeadow):
- **ask:** "Is the square being dressed?"
- **say:** "#greet#. The festival's on us and the square's as bare as a stubble field. Bring me green things for the garlands: herbs, and any flowers the season's left. Every wolf's bunch goes on one rope. When it reaches round the square, Southmeadow's ready."
- **accept:** "Green for the rope." **decline:** "Not this festival." **handIn:** "Green things for your garland."
- **thanks:** "#thanks#. Ripe, that, and on the rope already. My mother strung this rope, and hers before her. I'd not have taken green from a city paw once. I'll take yours."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=southmeadow self.dialect=southmeadow` · on | The Larkholm farmhand's stringing garlands round the square for the festival. Every bunch helps the rope reach, bless it. |
| gossip · `here.region=southmeadow self.dialect=southmeadow` · last=garland | The square's garlanded right round. Good harvest to whoever brought the green. |
| gossip · `self.id=cty_willa_wheatcombe` · last=garland (her own line, with C8) | The rope's round the square. Ripe, every bunch of it, and not one of them city-bought. |
| news · `here.region=larkfield self.dialect=larkfield` · on | Southmeadow's dressing its square for the festival. They'll pay in thanks, not silver, but it's a fair trade, eh? |

---

### 2.8 Fenshore (program `nh-pack.small.south`)

Fenshore-raised residents speak **mirebridge** (`settings.json` `dialect_of`): "Dry feet", fen-sure, boggy, tadpole, ", no?", "Mud and eels", "Dark One's mire". A Fenshore-raised wolf with accent `none` speaks plain. All three Fenshore givers re-cast in this revision speak plain (39 and 40 are Fenshore-raised with accent `none`; 41 is Ridgemere-raised), so their own lines are plain and the town's talk about them stays mirebridge. No line uses "mizzle" or another weather word without a gate. The purse is `town:fenshore` (213 pennies), day cap 21 (P3). Feed: +4 `nh_realm` and +4 `nh_voice_fenshore` (×2 distance).

---

#### 39. `sq-twine-for-the-nets`: Twine for the Purse-Nets (mending)
- **Tier:** small, 10–20 minutes. Repeat `daily`, `join.cap` 4, `fail: [{on: "death", match: {actor: "cty_leif_rainley"}}]` (he is 58; decisions §1.3). Board `fenshore`.
- **Where:** the Dune Rabbitry `fs_warren_dunes_rabbitry` (door `w_ridgemere_3_14` (169,48)), on the dunes south-west of Fenshore.
- **Giver:** Leif Rainley `cty_leif_rainley`, m 58, keeps rabbits at Dune. **Plain**: Fenshore-raised with accent none (decisions §1.3).
  - Home Dune Warren House `fs_warren_dunes_home` (door (162,40)); work `fs_warren_dunes_rabbitry`. Alive, placed, reachable.
  - Why him: he is tired and patient and "quotes old sayings for everything". His backstory (a fortune sought in Ridgemere, and nothing brought home) he "does not speak of", so his lines don't. The old giver holds `lq-eel-famine`.
- **Premise:** A warrener's purse-nets wear through on the burrow mouths. Two lengths of cord and he'll knit new ones; straw for the hutches if you have it.
- **Objectives:**
  - `cord`: Bring two lengths of cord (`gift`; `{target: cty_leif_rainley, item: cord}`; goal 2, quantity). 27 purses hold 87 lengths.
  - `straw` (optional): Straw for the hutches (`gift`; `{target: cty_leif_rainley, item: straw}`; goal 2, quantity). Every town's stores keep straw.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `smoked_rabbit` ×1 and practice in craft.
  - Deed: "brought twine to mend the Dune warren's purse-nets".
- **Feeds:** +4 / +4.
- **Needs:** none.

**Dialogue** (Leif, plain):
- **ask:** "What are those little nets for?"
- **say:** "#greet#. Purse-nets, for the burrow mouths. A rabbit bolts into one and the net draws shut. Mine are worn through, and a worn net is a rabbit's best friend, as my father said. Two lengths of cord and I'll knit new ones. Straw for the hutches too, if you've any."
- **accept:** "Two lengths of cord." **decline:** "Not today." **handIn:** "Cord for your nets."
- **thanks:** "#thanks#. A mended net catches what a new one only promises, they say. Take a smoked one for the road. And mind the soft ground by the burrows; it's riddled."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=fenshore self.dialect=mirebridge` · on | The Dune warrener's purse-nets are worn through, no? Bring him cord. He pays in smoked rabbit and old sayings. |
| gossip · `here.region=fenshore self.dialect=mirebridge` · last=nets | The Dune warren has new nets. Somebody brought the warrener cord, fen-sure. |

---

#### 40. `sq-fenside-pans`: Fire Under the Pans
- **Tier:** small, 15–20 minutes. Repeat `daily`, `join.cap` 4. Board `fenshore`.
- **Where:** the Fenside Boiling House `fs_salt_fencoast_boiling` (door `w_ridgemere_3_12` (131,224)), the salterns.
- **Giver:** Fallow Reedwell `cty_fallow_reedwell`, f 44, works the salt at Fenside. **Plain**: Fenshore-raised with accent none (decisions §1.3).
  - Home Fenside Cottage `fs_salt_fencoast_home` (door (124,218)); work `fs_salt_fencoast_boiling`. Alive, placed, reachable.
  - Why her: she works the same boiling house as the old giver (who keeps `mq-withy-lights`). She is dreamy and quietly funny, "has opinions about everyone's grandmother", and "swore an oath at a wedding and takes it seriously". Her grandmother, Mirebridge-born, is how the old names reach her.
  - She shares a first name with a campaign resident who lives elsewhere, and with Fallow Plumfield (32, Larkfield); decisions §1.2.5 allows it, and no line names any of them.
- **Premise:** The salt pans boil on firewood, and the fen has none worth burning. Four armfuls from the dry ground, and the salt comes white.
- **Objectives:**
  - `wood`: Bring four armfuls of firewood (`gift`; `{target: cty_fallow_reedwell, item: firewood}`; goal 4, quantity).
  - `peat` (optional): Bring peat if you've the back for it (`gift`; `{target: cty_fallow_reedwell, item: peat}`; goal 2, quantity).
- **P8 (Ch 4–6):** a trigger on `quest completed` `{detail: sq-fenside-pans}`, with `compare nh_stage >= 4` and `<= 6`, adds an `npc.offer` `fallow-names` on Fallow for `$actor`, `once: true`. Her words are in her window, not a banner.
  - **words:** "Whose names did the water take?"
  - **reply:** "My grandmother was Mirebridge-born. Every Restday she said them over the pans: the ferry wolf, the trapper by the causeway, the pup who rang the bell. Carry them to whoever keeps the Book of Names. Somebody ought to."
  - On `dialogue choice` `{detail: fallow-names}` (`once: member`): **`nh_names` +1**.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `salt` ×1 and practice in labour.
  - Deed: "fired the Fenside salt pans".
- **Feeds:** +4 / +4. `nh_names` +1 in Ch 4–6.
- **Needs:** J1, so the names offer is shown only to the wolf who did the work (today the `player` field already scopes it).

**Dialogue** (Fallow, plain):
- **ask:** "Are the pans boiling?"
- **say:** "#greet#. The pans boil on firewood, and the fen's got none worth burning. Four armfuls from the dry ground and the salt comes up white. Peat too, if you've a back for it. My grandmother carried peat on her head and called it a hat."
- **accept:** "Four armfuls of wood." **decline:** "Not today." **handIn:** "Wood for your pans."
- **thanks:** "#thanks#. White as a wedding cloth, that'll come up. Take a measure of salt. If you see lights out on the fen, don't follow them. My grandmother said they were only the marsh. She said a lot of things."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=fenshore self.dialect=mirebridge` · on | Firewood's scarce in the fen, no? The dry ground north of the boardwalks has scrub and willow, and that burns. |
| news · `here.region=fenshore self.dialect=mirebridge` · on | The Fenside salt worker has a grandmother story for everything, no? Ask her about Mirebridge, if you've the stomach for it. |
| gossip · `here.region=fenshore self.dialect=mirebridge` · last=pans | The Fenside pans came up white today. Somebody hauled her wood, fen-sure. |

---

#### 41. `sq-mirebridge-clay`: Grey Clay from the Mirebridge Pits (foraging)
- **Tier:** small, 15–30 minutes. Repeat `daily`, `join.cap` 4. Board `fenshore`.
- **Where:**
  - The Mirebridge clay pits, `w_ridgemere_4_12` ("The Sallow Fens, North"), the giver's dig at (156,205).
  - The Mirebridge crossroads, `w_ridgemere_5_11` (188,52). Region `crossroads` = (183,47)–(193,57).
- **Giver:** Lichen Reedford `cty_lichen_reedford`, f 39, digs clay at the Mirebridge pits. Plain (Ridgemere-raised, light).
  - Home `cn_fenshore_cottage_2`; work `w_ridgemere_4_12` (156,205). Alive, placed, reachable.
  - Why her: a clay digger at those very pits (decisions §1.3). She is easily offended and restless and "counts things under the breath". Her backstory (a petition to the Council of Houses, lost and not forgiven) stays unspoken. The old giver keeps `mq-potters-place`, so the kilns are named only as a place.
- **Premise:** The Fenshore Kilns want twice the clay the old potter took, and she is one wolf with one spade. Dig four lumps beside her, and she'll cart them in with hers. Don't stand too long at the crossroads where the village was.
- **Objectives:**
  - `dig`: Dig four lumps of clay at the Mirebridge pits (`forage`; `{item: clay, cell: w_ridgemere_4_12}`; goal 4, quantity).
  - `bring`: Give the clay to the clay digger (`gift`; `{target: cty_lichen_reedford, item: clay}`; goal 4, quantity; `after: [dig]`).
  - `crossroads` (optional): Stand a moment at the Mirebridge crossroads (`region entered`; `{target: crossroads}`).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `clay_pot` ×1 and practice in labour.
  - `bonus`: the deed "stood at the Mirebridge crossroads where the village was".
  - Main deed: "dug grey clay at the Mirebridge pits for the Fenshore kilns".
- **Feeds:** +4 / +4. It is a first, quiet visit to the campaign's post 7 (the Mirebridge crossroads, Ch 6).
- **Needs:** none. Mud tiles (`D`) give clay all year.

**Dialogue** (Lichen, plain):
- **ask:** "Is that all yours to dig?"
- **say:** "#greet#. The kilns want twice the clay the old potter did, and I'm one wolf with one spade. Dig me four lumps here at the pits, and I'll cart them in with mine. Four. I'll count them. Don't stand long at the old crossroads on the way. That's where the village was."
- **accept:** "Four lumps of clay." **decline:** "Not today." **handIn:** "Four lumps, counted."
- **thanks:** "#thanks#. Four, and good grey ones. I counted. Most wolves bring three and call it four. Take a pot from the kilns' last firing; it leans, but it holds."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=fenshore self.dialect=mirebridge` · on | The clay digger at the Mirebridge pits wants a second spade, no? She counts every lump, so bring four. |
| news · `here.region=fenshore self.dialect=mirebridge` · on | Nobody digs at the crossroads itself, no? That's where the village stood, before the river came over. |
| gossip · `here.region=fenshore self.dialect=mirebridge` · last=clay | Fresh clay at the kilns today, the grey Mirebridge kind. You can tell it by the smell. |

---

#### 42. `sq-boardwalk-plank`: A Plank Gone by the Heron (mending)
- **Tier:** small, 10–20 minutes. Repeat `daily`, `join.cap` 3. Board `fenshore`.
- **Where:** the boardwalks by The Heron `cn_fenshore_alehouse`, Fenshore square `w_ridgemere_3_13` (92,148).
- **Giver:** Yrsa Plankgate `cty_yrsa_plankgate`, f 44, deckhand at Fen Shore. Mirebridge speech (light).
  - Home `cn_fenshore_cottage_1`. Alive, placed, reachable.
  - Why her: she is unhurried and guarded, "never sits with a back to the door", and "knows a story about one of the great Houses that would be unwise to repeat". She isn't the Heron's innkeeper, Nell Plummeadow.
- **Premise:** A plank has gone on the boardwalk by the Heron. Bring planks and nails, and she'll do the hammering.
- **Objectives:**
  - `planks`: Bring two planks (`gift`; `{target: cty_yrsa_plankgate, item: planks}`; goal 2, quantity).
  - `nails`: Bring a paw of nails (`gift`; `{target: cty_yrsa_plankgate, item: nails}`; goal 4, quantity).
  - `mend` (optional): If you carry the Mend Gift, use it on the boardwalk. This is `gift used` `{detail: mend}`, checked in Fenshore with `wolf in town`, so a trigger is used.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: practice in craft.
  - `bonus`: the deed "mended a Fenshore boardwalk with a Gift".
  - Main deed: "mended the Fenshore boardwalk by the Heron".
- **Feeds:** +4 / +4.
- **Needs:** J10 (`wolf has`) to show `mend` only to Mend wolves. Until then the label says "if you carry".
  - Using a work Gift openly is Gifted utility, which never draws Warden attention.

**Dialogue** (Yrsa, mirebridge):
- **ask:** "Watch your step there?"
- **say:** "#greet#. There's a plank gone on the boardwalk by the Heron, and the next wolf through will be wet to the knee, no? Two planks and a paw of nails, and I'll do the hammering. I'm unhurried, but it'll be done."
- **accept:** "Planks and nails." **decline:** "Not today." **handIn:** "Planks and nails for the boardwalk."
- **thanks:** "#thanks#. Dry feet for the next one through. Fen-sure, that's the best thanks the fen gives. I know a tale about a great House that'd sink a boardwalk, but I'll keep it. Some planks hold better nailed down."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=fenshore self.dialect=mirebridge` · on | There's a plank gone by the Heron. The deckhand who mends the boardwalks takes planks and nails, no? |
| gossip · `here.region=fenshore self.dialect=mirebridge` · last=plank | The boardwalk by the Heron's whole again. Dry feet, for a change. |

---

### 2.9 Saltwell (program `nh-pack.small.south`)

Saltwell givers speak **saltwell**: a formal, hospitable voice ("Peace on you", "Go in peace", "May your well never fail", my brother or my sister, "By salt and water", proverbs). The only borrowed words are "wadi" and "Baba". The purse is `town:saltwell` (224 pennies), day cap 22 (P3). Feed: +4 `nh_realm` and +4 `nh_voice_saltwell` (×2 distance).

---

#### 43. `sq-salt-for-the-road`: Salt for the Stores (carrying)
- **Tier:** small, 10–15 minutes. Repeat `daily`, `join.cap` 3. Board `saltwell`.
- **Where:** from the Saltpan salt store `fs_salt_saltpan_store` (door `w_ridgemere_13_12` (96,243)) to Saltwell Stores `cn_saltwell_store` (door `w_ridgemere_12_11` (220,108)). The giver's warren is the Redsand Rabbitry `fs_warren_waste_rabbitry` (door `w_ridgemere_11_10` (161,44)).
- **Giver:** Winifred Fogridge `cty_winifred_fogridge`, f 45, warrener at Redsand. Saltwell (light).
  - Home `cn_saltwell_cottage_1`; work `fs_warren_waste_rabbitry`. Alive, placed, reachable.
  - Why her: Saltwell's last free resident (decisions §1.3). She is anxious and dreamy, "keeps a tally of favours owed", and "is the third generation to hold this trade". A warren's pelts can't be cured without salt, and the stores have none.
- **Recipient:** Thistle Stonepick `tn_fh_thistle_stonepick`, the Saltwell storekeeper. She is the campaign's Saltwell Keeper; here she is a gift recipient only, with no hub slot, and receiving stock agrees with her campaign role as storekeeper (decisions §1.2.2).
- **Premise:** The salt is raked and the stores are empty, and no carter goes out into the white while the sun is high. Carry two measures. No purse holds salt at the checkpoint, so the rakers' salt is handed to the wolf on accept.
- **Objectives:**
  - `salt`: Carry two measures of salt to the Saltwell storekeeper (`gift`; `{target: tn_fh_thistle_stonepick, item: salt}`; goal 2, quantity).
  - `lighten` (optional): If you carry the Lighten Load or Carry Gift, use it on the road. A trigger on `gift used` checks `any [field detail = lighten_load, field detail = carry]`.
- **Triggers:**
  - `salt_give`: `item.give salt 2` on accept.
  - P1, P2, P3.
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: practice in travel.
  - `bonus`: `waterskin` ×1.
  - Deed: "carried Saltpan salt across the white to the Saltwell stores".
- **Feeds:** +4 / +4.
- **Needs:** J10, for the Gift option.

**Dialogue** (Winifred, saltwell):
- **ask:** "Is the Saltwell store out of salt?"
- **say:** "#greet#. The salt is raked at the pan, but the stores are empty, and no carter will go into the white while the sun is high. Without salt I cannot cure a single pelt. Carry two measures to the storekeeper for me. A favour carried is a favour owed, and I keep count."
- **accept:** "Two measures, to the stores." **decline:** "Not in this heat." **handIn:** "Your salt is at the stores."
- **thanks:** "#thanks#. You walked it in the heat and did not complain. My mother kept this warren, and hers before her, and none of them had a wolf like you to call on. That is one in my tally, #address#. The well remembers who drew from it."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=saltwell self.dialect=saltwell` · on | The salt rakers at the pan need carriers. Go when the sun is low, my friend; the white burns when it is high. |
| news · `here.region=saltwell self.dialect=saltwell` · on | The Redsand warrener cannot cure her pelts without salt. Carry some to the stores and she will remember it. |
| gossip · `here.region=saltwell self.dialect=saltwell` · last=salt | Salt came in to the stores today, carried on foot. Blessings on the wolf who walked it. |

---

#### 44. `sq-ochre-rabbits`: Rabbits in the Vines (hunting)
- **Tier:** small, 15–30 minutes. Repeat `daily`, `join.cap` 4. Board `saltwell`.
- **Where:** the Ochre vineyard press house `fs_vineyard_ochre_press` (door `w_ridgemere_9_10` (225,91)). The hunt is in cell `w_ridgemere_9_10`.
- **Giver:** Senna Applegarth `cty_senna_applegarth`, f 50, tends the vines at Ochre. Saltwell (light).
  - Home `fs_vineyard_ochre_home`. Alive, placed, reachable.
  - Why her: she is tired and restless, "distrusts anything from the city", and "hopes a child of the family will be taken into service with one of the great Houses".
- **Premise:** Rabbits come in from the scrub and strip the young vines to the stalk. Two of them, and the rest will learn.
- **Objectives:**
  - `rabbits`: Bring down two rabbits in the Ochre vines (`hunted`; `{target: rabbit, cell: w_ridgemere_9_10}`; goal 2, count).
- **Rewards:**
  - P3: 3 pennies.
  - `rewards`: `grapes` ×2 and practice in watch.
  - Deed: "kept the rabbits off the Ochre vines".
- **Feeds:** +4 / +4.
- **Needs:** none. Rabbits live in grass, scrub and crop, flee, and hear better than they see.

**Dialogue** (Senna, saltwell):
- **ask:** "Your vines look bitten."
- **say:** "#greet#. The rabbits come in from the scrub and strip my young vines to the stalk. Bring down two of them, and the rest will learn. Do this, and you are welcome at my table, by bread and salt."
- **accept:** "Two rabbits." **decline:** "Not today." **handIn:** "Your vines are safe."
- **thanks:** "#thanks#. Sit, take grapes; they are sweet this year. A wolf from the city who hunts like a wolf of the wadi. I did not think I would see it. Perhaps the city is not all brackish."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=saltwell self.dialect=saltwell` · on | The Ochre vines are stripped by rabbits. The vine-tender pays well, and feeds better. |
| news · `here.region=saltwell self.dialect=saltwell` · on | Rabbits hear you before they see you. Come at them slowly, downwind, and the vines will thank you. |
| gossip · `here.region=saltwell self.dialect=saltwell` · last=rabbits | The Ochre vines are safe a while. The vine-tender is smiling, and she does not smile at the city. |

---

#### 45. `sq-caravan-welcome`: Water and a Word (meeting the caravan)
- **Tier:** small, 5–30 minutes, depending on the wagons. Repeat `daily`, `join.cap` 6, `fail: [{afterDays: 2}]`. Board `saltwell`.
- **Where:** the Saltwell square, `w_ridgemere_12_11`, region `well` = (210,106)–(230,126) round the pool and the caravan arc. Also the Oasis Inn `cn_saltwell_inn` (door (230,115)).
- **Giver:** Moss Downridge `tn_lf_moss_downridge`, f 36, pot hand at the Oasis Inn. Saltwell (light).
  - Home `cn_saltwell_cottage_2`. Alive, placed, reachable.
  - Why her: she is blunt and restless and "whistles back at the larks". She is **not** the innkeeper, Oriel Marshditch, who carries helpful lines only.
- **Premise:** When a caravan comes in off the sand, someone must meet it at the well with water and a word, and the innkeeper can't leave the fire. Wait for the wagons, then break bread with them inside.
- **Objectives:**
  - `welcome`: Be at the well when a caravan comes in (`story signal`; P6 on `caravan arrives` `{target: saltwell}`, each participant with the quest active and `wolf in region well`).
  - `bread`: Share a meal at the Oasis Inn (`shared meal`; `{cell: cn_saltwell_inn}`; `after: [welcome]`).
  - `dowse` (optional): If you carry the Dowse Gift, find where the old wells ran (`gift used` `{detail: dowse}`, with `wolf in town {town: saltwell}`, through a trigger).
- **Rewards:**
  - P3: 2 pennies.
  - `rewards`: practice in service.
  - `bonus`: the deed "dowsed the old wells of Saltwell".
  - Main deed: "welcomed a caravan in at the Saltwell well".
- **Feeds:** +4 / +4.
- **Needs:** J10, for the Dowse option.

**Dialogue** (Moss, saltwell):
- **ask:** "Who meets the wagons when they come in?"
- **say:** "#greet#. When a caravan comes in off the sand, someone must meet it at the well with water and a word. The innkeeper cannot leave the fire. Wait for the next wagons at the square, then share bread with them inside. That is how Saltwell says welcome."
- **accept:** "I'll meet the wagons." **decline:** "I can't wait today." **handIn:** "They were met, and fed."
- **thanks:** "#thanks#. They will tell it on the road: at Saltwell, someone met us. Every caravan comes home at last, and this one came home to you. I dream of a farm, you know, somewhere green. But I would miss this."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=saltwell self.dialect=saltwell` · on | The wagons come in by the square. Wait in the shade by the pool, and you will hear them before you see them. |
| gossip · `here.region=saltwell self.dialect=saltwell` · last=caravan | Someone met the wagons at the well and broke bread with them. That is the old way, and it is good to see. |
| news · `here.region=ridgemere self.dialect=saltwell` · on | In Saltwell, #address#, a caravan is met at the well. Even here, so far from it, I look for someone to meet. |

---

### 2.9a The estates of Ridgemere (program `nh-pack.small.city`; quests 46–48)

Three below-stairs errands, one inside each House estate the packs had left empty (decisions §7.1, critic P1-16): kitchens, chambers and hangings, carried to and from the city. The rules for all three:
- **No House head, steward or campaign cast gives, holds or receives anything.** The givers serve in the hall; the one other resident named (48) keeps the chambers and is no one's cast. Stewards and family are named in talk only as "the steward" or "the family".
- **The campaign only colours a line.** Each giver has up to three `npc.offer` choices for `$actor`, added on `quest completed` by one trigger with an `if`/`elif`, and only one of them is ever sent: a line when `nh_b_quarrel` names that House (Ch 6 on), a line for any other quarrel result, and a quiet line otherwise (Ch 1–5, or Ch 6 before the quarrel has a result). They are the giver's own words, in the window (decisions §9, P2-22). They read `nh_stage` and `nh_b_quarrel` and write nothing.
- **Purse:** `town:ridgemere` (the treasury), 2 pennies (P3). The Houses' own purses are M3, deferred (decisions §5.2).
- **Feed:** +2 `nh_realm`, +2 `nh_voice_ridgemere` (P1); first-time deed (P2).
- **Dialect:** all three givers are Ridgemere-raised with a light accent, so they speak plain.

The colour trigger, for 46 (47 and 48 swap the giver, the ids and the House's value):
```json
{"id":"fell_colour","on":"quest completed","audience":"participant","match":{"detail":"sq-fellhall-errand"},
 "actions":[{"do":"if",
   "if":{"test":"all","conditions":[{"test":"compare","left":{"var":"nh_stage"},"op":">=","right":6},
                                     {"test":"compare","left":{"var":"nh_b_quarrel"},"op":"=","right":"fell_timber"}]},
   "then":[{"do":"effect","effect":{"kind":"npc.offer","npc":"rm_ansel_tarley","id":"fell-upstairs","player":"$actor","once":true,
            "words":"How is it upstairs?","reply":"…"}}],
   "elif":[{"if":{"test":"all","conditions":[{"test":"compare","left":{"var":"nh_stage"},"op":">=","right":6},
                                              {"test":"compare","left":{"var":"nh_b_quarrel"},"op":"!=","right":""}]},
            "then":[{"do":"effect","effect":{"kind":"npc.offer","npc":"rm_ansel_tarley","id":"fell-houses","player":"$actor","once":true,"words":"How is it upstairs?","reply":"…"}}]},
           {"if":{"test":"compare","left":{"var":"nh_stage"},"op":">=","right":1},
            "then":[{"do":"effect","effect":{"kind":"npc.offer","npc":"rm_ansel_tarley","id":"fell-quiet","player":"$actor","once":true,"words":"How is it upstairs?","reply":"…"}}]}]}]}
```
With `nh_stage` unset or 0 (no campaign running), no line is sent and the errand is just an errand.

---

#### 46. `sq-fellhall-errand`: Eggs for the Fellhall Kitchen
- **Tier:** small, about 15 minutes. Repeat `daily`, `join.cap` 4. Board `ridgemere`.
- **Where:** Fellhall `rm_fellhall` (door `w_ridgemere_2_1` (150,70)), and through the hall its kitchens and servants' hall `rm_fellhall_down`. The goods come from the Commons market: eggs are in 65 purses (117 at the checkpoint) and cheese in 143 (665).
- **Giver:** Ansel Tarley `rm_ansel_tarley`, m 50, serving in the hall at Fellhall. Plain.
  - Home `rm_fellhall_down`; work `rm_fellhall`. Alive, placed, reachable.
  - Why him: he is patient and restless and "knows which streets flood first". He "was born in the Sump and means to die somewhere better, the Heights for preference". He is not the steward (Millicent Rustwell, campaign cast) and none of the family.
- **Premise:** The Fellhall kitchen is short for the family's supper, and the cook won't send a hall servant to market twice in a day. Four eggs and two cheeses from the Commons, brought round to the kitchens.
- **Objectives:**
  - `eggs`: Bring four eggs (`gift`; `{target: rm_ansel_tarley, item: eggs}`; goal 4, quantity).
  - `cheese`: Bring two cheeses (`gift`; `{target: rm_ansel_tarley, item: cheese}`; goal 2, quantity).
  - `door` (optional): Come in by the kitchens, not the front (`enter place`; `{target: rm_fellhall_down}`).
- **Triggers:** `fell_colour` (above), P1, P2, P3.
- **Rewards:**
  - P3: 2 pennies, `town:ridgemere`.
  - `rewards`: practice in service.
  - `bonus` (`door`): `meat_pie` ×1, the cook's own.
  - Deed: "carried the market eggs to the Fellhall kitchen".
- **Feeds:** +2 / +2.
- **Needs:** none.

**Dialogue** (Ansel, plain):
- **ask:** "Does the kitchen want something fetched?"
- **say:** "#greet#. The cook's short for the family's supper and won't send me to market twice. Four eggs and two cheeses from the Commons, brought round to the kitchens. Keep to the high side of the street; the low side floods first. I'd go myself, but the hall doesn't sweep itself."
- **accept:** "Eggs and cheese, round the back." **decline:** "Not today." **handIn:** "Eggs whole, and the cheese."
- **thanks:** "#thanks#. Eggs whole and the cheese not sat on. The cook will think I went myself. I was born in the Sump, you know. One day I'll have a kitchen in the Heights and a hall servant of my own to send."
- **Coloured lines** (`npc.offer`, words "How is it upstairs?"):
  - `fell-quiet` (Ch 1–5, or no quarrel result yet): "Upstairs is the steward's business, and the family's. Down here we know the price of eggs and which lanes flood first. That's enough knowing for anyone."
  - `fell-upstairs` (`nh_b_quarrel = fell_timber`): "The family and Timberland aren't speaking, so the kitchen cooks for one House and feeds the gossip of two. We don't take sides down here. We take plates."
  - `fell-houses` (any other quarrel): "The Houses have fallen out, and the family's at the windows all day watching who calls on whom. The kitchen just counts the plates coming back."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain self.quarter=estate` · on | The Fellhall kitchen is always a basket short. The hall servant pays for eggs and cheese brought round the back. |
| news · `here.region=ridgemere self.dialect=plain` · on | Fellhall's kitchen buys eggs off the Commons most days. Go in by the kitchens; the front door's for the family. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=estate` · last=fellhall | The Fellhall kitchen had its eggs in time today. The cook was nearly civil about it. |

---

#### 47. `sq-grayrock-hall-errand`: Clean Linen for Grayrock Hall
- **Tier:** small, about 15 minutes. Repeat `daily`, `join.cap` 4. Board `ridgemere`.
- **Where:** Grayrock Hall `rm_grayrock_hall` (door `w_ridgemere_0_2` (182,205)). Linen cloth is in 30 purses (81 lengths), Greywool's and every town's stores among them; lavender in 26 (83), and it is foraged in summer.
- **Giver:** Erland Cinderridge `rm_erland_cinderridge`, m 21, serving in the hall at Grayrock Hall. Plain.
  - Home `rm_grayrock_hall_down`; work `rm_grayrock_hall`. Alive, placed, reachable.
  - Why him: he is loud and generous and "watches the rain as if it might stop". His backstory (a list he keeps) belongs to the campaign's Watch lists and is never spoken here. He is not the steward (Lavinia Plankwick, campaign cast) and none of the family.
- **Premise:** The family's linen came back from the washers grey with sea damp, and the chamber keeper wants fresh on every bed before anyone upstairs notices. Two lengths of linen cloth from the stores, and lavender for the presses if the season has any.
- **Objectives:**
  - `linen`: Bring two lengths of linen cloth (`gift`; `{target: rm_erland_cinderridge, item: linen_cloth}`; goal 2, quantity).
  - `lavender` (optional): Lavender for the linen presses (`gift`; `{target: rm_erland_cinderridge, item: lavender}`; goal 2, quantity).
- **Triggers:** `grayrock_colour` (as 46, with `nh_b_quarrel = greatly_grayrock`), P1, P2, P3.
- **Rewards:**
  - P3: 2 pennies, `town:ridgemere`.
  - `rewards`: practice in service.
  - `bonus` (`lavender`): `candle_wax` ×1, from the chambers' box.
  - Deed: "brought fresh linen for the Grayrock Hall chambers".
- **Feeds:** +2 / +2.
- **Needs:** none.

**Dialogue** (Erland, plain):
- **ask:** "Is the Hall short of something?"
- **say:** "#greet#! The family's linen came back from the washers grey with sea damp, and the chamber keeper wants fresh on every bed before anyone upstairs notices. Two lengths of linen cloth from the stores, and lavender for the presses if the season's left any."
- **accept:** "Two lengths of linen." **decline:** "Not today." **handIn:** "Linen for the beds."
- **thanks:** "#thanks#! Every bed made before the bell. Take a candle; the chambers burn more than they need. I'll watch the rain stop one day, you'll see. Then I'll have nothing to do but fold."
- **Coloured lines** (`npc.offer`, words "How is it upstairs?"):
  - `grayrock-quiet` (Ch 1–5, or no quarrel result yet): "Upstairs is upstairs. The family's away more than it's home, and the steward keeps the keys. Down here we keep the rain out, or try."
  - `grayrock-upstairs` (`nh_b_quarrel = greatly_grayrock`): "The Hall and Greatly are at each other's throats. Every Greatly cart that passes the gate gets a look from the steward that'd curdle milk. I just fold the sheets."
  - `grayrock-houses` (any other quarrel): "The Houses have fallen out, and the Hall's doors are shut to callers half the day. More sheets to fold, and fewer feet to dirty them."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain self.quarter=estate` · on | Grayrock Hall buys linen when the sea damp gets into the sheets. The young hall servant pays, and talks loud enough for two. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=estate` · last=grayrock | Every bed in Grayrock Hall had fresh linen today. The chamber keeper hummed, they say. |

---

#### 48. `sq-timberland-court-errand`: A Bolt for the Court
- **Tier:** small, 15–20 minutes. Repeat `daily`, `join.cap` 4, `fail: [{on: "death", match: {actor: "rm_arne_wetworth"}}]` (the recipient is 61). Board `ridgemere`.
- **Where:** from Greywool `rm_greywool` (door `w_ridgemere_1_0` (124,253)), the weaver's on the Northgate side, to Timberland Court `rm_mossgrave_court` (door `w_ridgemere_1_2` (102,72)). The chamber keeper takes his evenings in the kitchens and servants' hall `rm_mossgrave_court_down` (through the hall, (14,2)).
- **Giver:** Lotte Greyley `rm_lotte_greyley`, f 21, helping at Greywool. Plain. She lives below stairs at the Court.
  - Home `rm_mossgrave_court_down`; work `rm_greywool`. Alive, placed, reachable.
  - Why her: she is quietly funny and curious, and hates the damp of the servants' hall. Her backstory (a brother lost on the wharf, and no forgiveness for House Grayrock) is never spoken here. She is not the steward (Hollis Nailwick, campaign cast) and none of the family.
- **Recipient:** Arne Wetworth `rm_arne_wetworth`, m 61, keeping the chambers at Timberland Court (home `rm_mossgrave_court_down`, evenings there; work the family's chambers `rm_mossgrave_court_up`). Alive, placed, reachable. No other role in any pack.
- **Not the larder:** the Court's servants'-hall larder is `quests-medium.md` Q7 (Vidar Ashhand). This errand is the hangings, and names neither the larder nor Vidar.
- **Premise:** Greywool has finished a bolt of wool cloth for the Court's winter hangings, and Lotte can't carry it and keep her place at the loom. Carry it to the chamber keeper.
- **Objectives:**
  - `bolt`: Give the bolt to the Court's chamber keeper (`gift`; `{target: rm_arne_wetworth, item: wool_cloth}`). The bolt is `item.give wool_cloth 1` on accept; no purse holds wool cloth at the checkpoint, so only the prop can count.
  - `wood` (optional): Bring two armfuls of firewood for the servants' hall fire, against the damp (`gift`; `{target: rm_lotte_greyley, item: firewood}`; goal 2, quantity).
- **Triggers:** `bolt_give` (on accept), `court_colour` (as 46, with `nh_b_quarrel = fell_timber`, the Court's quarrel with Fellhall), P1, P2, P3.
- **Rewards:**
  - P3: 2 pennies, `town:ridgemere`.
  - `rewards`: practice in travel.
  - `bonus` (`wood`): practice in service.
  - Deed: "carried a Greywool bolt up to Timberland Court".
- **Feeds:** +2 / +2.
- **Needs:** J9 (a tagged bolt) is optional, since no other wool cloth exists.

**Dialogue** (Lotte, plain):
- **ask:** "Is that bolt going somewhere?"
- **say:** "#greet#. Greywool's finished the Court's winter hangings, and I can't carry the bolt and keep my place at the loom. Take it to the chamber keeper at Timberland Court. He takes his supper in the servants' hall, and he hums, so you'll find him."
- **accept:** "I'll carry it up." **decline:** "Not today." **handIn:** "The bolt's with the chamber keeper."
- **thanks:** "#thanks#. He'll have hummed at it and counted the ells twice. And if you brought wood for the hall fire, thank you for that too. The damp in that hall could drown a fish."
- **Coloured lines** (`npc.offer`, words "How is it up at the Court?"):
  - `court-quiet` (Ch 1–5, or no quarrel result yet): "Upstairs is the steward's worry. Down here we worry about the damp, and the damp doesn't take sides."
  - `court-upstairs` (`nh_b_quarrel = fell_timber`): "The Court and Fellhall have fallen out over the timber, and the steward has us counting every log in the woodstore. I count. Nobody notices."
  - `court-houses` (any other quarrel): "The Houses have all fallen out, and the Court's doors are barred to half the city. The bolts still need carrying, mind."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain` · on | Greywool sends its finished bolts up to the big houses. The young loom hand pays a carrier, and makes a joke of it. |
| news · `here.region=ridgemere self.dialect=plain self.quarter=estate` · on | Looking for the Court's chamber keeper? He takes his supper below stairs, and you'll hear him humming before you see him. |
| gossip · `here.region=ridgemere self.dialect=plain self.quarter=estate` · last=bolt | Timberland Court has its new hangings. Greywool's bolt went up the hill on somebody's back. |

---

### 2.10 The stage pack `nh-pack.stage`: errands that follow the campaign (decisions §7.3)

The packs above are stage-agnostic: they run the same in every chapter, and with no campaign at all. The stage pack is the other half (critic P0-6): quests offered only while the campaign is at a given stage, so a wolf in Act II or III always has small, medium and large work that matches what the realm is living through. This section is the **program** (written once, here) and its **12 small entries**. The 5 medium entries are `quests-medium.md` §4b and the 5 large are `quests-large.md` §12b; they are listed in §2.10.1 so the program is whole.

#### 2.10.1 The program

**File and shape:**
- `Data/Storylines/ridgemere3/quests-stage/nh-pack.stage.json`, never `programs/`.
- `format: "ratw-story-program"`, `version: 2`, `id: "nh-pack.stage"`, `title: "The season's errands"`.
- `audience: {"kind":"open","cap":5000}`. A wolf joins by accepting any of its quests.
- `start: "quiet"`.
- At most 32 quests: it has 22.

**Phases.** Seven, one per stage band. None has exits; the campaign triggers below move between them. A quest lists the phases it is offered in (`phases`), and a quest that lives in one phase is `lifetime: "phase"`, so its copies expire when the stage moves on.

| Phase | `nh_stage` | Journal (narration) | Talk key while it runs (`world.mem.nh_stage=`) |
|---|---|---|---|
| `quiet` | unset, 0, or past 9 | "The roads are quiet. Nothing asks more of you than the day's work." | (none) |
| `s12` | 1–2 | "The old King is dead, and the Moot is called." | `king\|moot_called\|reckoning` |
| `s3` | 3 | "The Line is going up, post by post." | `signals` |
| `s4` | 4 | "The towns are finding their howls." | `howls` |
| `s56` | 5–6 | "Bounds on the hills, letters for an absent lord, and lights on the fen." | `letters\|lights` |
| `s78` | 7–8 | "The harvest, the ford, and the ice on the Upper Mere." | `harvest\|bell` |
| `s9` | 9 | "The Moot night." | `moot` |

`quiet` carries one phase trigger of its own (`quiet_day`, a `note` on `day begins`), so a phase with no quests is not read as an ending.

**Moving between phases.** Two campaign triggers on `hour`[w] read `nh_stage` (decisions §7.3). Each branch also checks `not phase is` its target, so the hour never re-enters the phase it is in (re-entering would end every `lifetime: phase` copy):
```json
{"id":"stage_low","on":"hour","actions":[{"do":"if",
  "if":{"test":"all","conditions":[
    {"test":"any","conditions":[{"test":"compare","left":{"var":"nh_stage"},"op":"<","right":1},
                                {"test":"compare","left":{"var":"nh_stage"},"op":">","right":9}]},
    {"test":"not","condition":{"test":"phase is","phase":"quiet"}}]},
  "then":[{"do":"enter","phase":"quiet"}],
  "elif":[
   {"if":{"test":"all","conditions":[{"test":"compare","left":{"var":"nh_stage"},"op":">=","right":1},
                                     {"test":"compare","left":{"var":"nh_stage"},"op":"<=","right":2},
                                     {"test":"not","condition":{"test":"phase is","phase":"s12"}}]},
    "then":[{"do":"enter","phase":"s12"}]},
   {"if":{"test":"all","conditions":[{"test":"compare","left":{"var":"nh_stage"},"op":"=","right":3},
                                     {"test":"not","condition":{"test":"phase is","phase":"s3"}}]},
    "then":[{"do":"enter","phase":"s3"}]},
   {"if":{"test":"all","conditions":[{"test":"compare","left":{"var":"nh_stage"},"op":"=","right":4},
                                     {"test":"not","condition":{"test":"phase is","phase":"s4"}}]},
    "then":[{"do":"enter","phase":"s4"}]}]}]}
```
`stage_high` is the same shape for `s56` (5–6), `s78` (7–8) and `s9` (9). Each `then` holds only its `enter`, which is the last action of its list and outside any `each`.

- **A second, prompt route** (and the rehearsal's way in): `stage_start_<n>` on `story started` with `match.target` = `ninth-howl.<n>` (the chapter programs, `campaign.md` §0) enters that chapter's phase at once, and `stage_end` on `story ended` with `match.target` = `ninth-howl.9` enters `quiet`. The hourly read of `nh_stage` stays the authority: if a chapter is moved by hand, the next hour corrects the phase.
- **Rehearsal.** A rehearsal case can't seed a world variable, so each of the seven cases reaches its phase through a `story started` fact (`target` the chapter's program id). Before approval, check that `review` accepts that fact kind; if it does not, ask the engine reviser for a case-level world seed, because approval needs every phase reached.

**The 22 quests** (decisions §7.3):

| Quest | Tier | Phases | Giver or holder | Written in |
|---|---|---|---|---|
| `st-wake-bread` | small | s12 | Winifred Cinderworth `rm_winifred_cinderworth` | S1 below |
| `st-summons` | small | s12 | Olaf Pitchmore `rm_olaf_pitchmore` | S2 |
| `st-post-stone` | small | s3 | Frida Netherby `rmx_frida_netherby` | S3 |
| `st-post-timber` | small | s3 | Odo Icewell `cty_odo_icewell` | S4 |
| `st-chorus-wrong` | small | s4 | Alder Blossomcroft `cty_alder_blossomcroft` | S5 |
| `st-bounds-water` | small | s56 | Juniper Larkham `cty_juniper_larkham` | S6 |
| `st-pull-stake` | small | s56 | **board-only** (Oakhaven); target Ingram Ashleaf | S7 |
| `st-fog-lanterns` | small | s56 | Stone Marshmore `cty_stone_marshmore` | S8 |
| `st-ford-blankets` | small | s78 | Quill Oakdell `tn_oh_quill_oakdell` | S9 |
| `st-feed-larkfield` | small | s78, only if `nh_b_ford = burned` | Brand Sunham `cty_brand_sunham` (holder, P10) | S10 |
| `st-haulers-food` | small | s78, only while `nh_stage = 8` | Lichen Harvestton `tn_sm_lichen_harvestton` (holder, P10) | S11 |
| `st-moot-benches` | small | s9 | Runa Gullley `rm_runa_gullley` | S12 |
| `st-greenholt-wagon` | medium | s3 | Aud Saltridge `rmx_aud_saltridge` | `quests-medium.md` §4b |
| `st-black-cloak-road` | medium | s56 | Wick Marshwade `cty_wick_marshwade` | `quests-medium.md` §4b |
| `st-relief-convoy` | medium | s78 | Myrtle Larkcombe `cty_myrtle_larkcombe` | `quests-medium.md` §4b |
| `st-ice-watch` | medium | s78 | Garth Barleyton `cty_garth_barleyton` | `quests-medium.md` §4b |
| `st-bell-road` | medium | s9 | Jory Rimepeak `tn_fh_jory_rimepeak` | `quests-medium.md` §4b |
| `st-patrol-round` | large | s12, s3 | Bertil Saltcot `rmx_bertil_saltcot` | `quests-large.md` §12b |
| `st-camp-bounty` | large | s3 to s9 | Frederick Rainham `rm_frederick_rainham` | `quests-large.md` §12b |
| `st-midsummer-road` | large | s4 | Cedric Mudley `rm_cedric_mudley` | `quests-large.md` §12b |
| `st-twelve-bounds` | large | s56 | Ada Coalstack `cty_ada_coalstack` | `quests-large.md` §12b |
| `st-post-night` | large | s9 (until the Moot night is called) | Beatha Foggate `rm_beatha_foggate` | `quests-large.md` §12b |

A quest offered in more than one phase (`st-patrol-round`, `st-camp-bounty`) is `lifetime: "campaign"`, so a copy under way survives the move.

**Variables** (the program declares only what it touches):
- **World**, same names and types as `campaign.md` §4: `nh_stage` (number, read only), `nh_realm`, the nine `nh_voice_<town>` strands, `nh_threat` (packs only lower it), `nh_b_ford` (text, read only); and the purse rule's `coin_<purse>_day` (number) and `coin_<purse>_ok` (switch) for `ridgemere`, `coldwater`, `larkfield`, `southmeadow`, `fenshore`, `oakhaven` and `frosthollow`. That is 27.
- **Player:** `st_done` (number, the tracker "Errands of the season"); the offer timers `t_stake_offer`, `t_feed_offer`, `t_haul_offer`. The small tier uses 4; the medium and large entries add their own, well inside 64.
- The program writes no branch, never `nh_stage`, `nh_surplus`, posts or Keepers, and never a floor switch.

**Purse** (decisions §3, §1.3 P3 here): the same per-purse counters and floor as every program, with each tier's amounts: small 2–4 a wolf; medium 6–15; large at most 12 a chapter. One pay trigger per purse and amount, campaign-wide.

**Feeds** (P1, as any pack): on `quest completed`, by tier and the town the deed serves, `add nh_realm` and `add nh_voice_<town>` by **small 2 / medium 4 / large 8, times the distance weight** (×1 Ridgemere and Oakhaven, ×1.5 Coldwater, Larkfield and Frosthollow, ×2 Southmeadow, Fenshore, Saltwell and Mirebridge), and `add st_done 1 of $actor`. With C12a, also `story.notify` to the running chapter. First-time deeds (P2) are phase triggers, one per quest, in the phases where the quest lives.

**Gates:** the phases themselves; branch gates read with `compare nh_b_*` through P10 (S10 reads `nh_b_ford = burned`). Every quest in the program also works with `nh_stage` unset: it is then simply not offered, since the program sits in `quiet`.

**Talk:** each entry's helpful lines are gated on the campaign's own talk key `world.mem.nh_stage=<keys of its phase>` (the table above; `|` alternation is accepted, decisions §9 P2-19), so they play **today**, with no C8. Its "someone did it" lines are gated on `world.mem.st_last=<short>`, written by P1 with C8 (2 days); without C8 they don't play and the deed gossip still does.

**Budgets:**
- **Offers and choices**, 64 for the whole program: the 12 small entries use 25 (9 giver offers and 9 board offers, S7's board, S10's and S11's journal offers and their 4 `npc.offer` choices). That leaves 39 for the 10 medium and large entries.
- **Triggers per phase**, campaign triggers included, 64: the campaign triggers are about 24 (2 stage, 9 chapter routes, about 8 pay, 7 feeds, the 3 post credits of S3–S4); each phase adds its own (deeds, objectives' helpers), at most 12 in the busiest (`s78`).
- **Regions:** `oak_square`, `ford`, `lamp_1`–`lamp_3`, `moot_square` from the small tier, plus the large tier's own bounds copies (`st-twelve-bounds`), well inside 128.

---

#### 2.10.2 The small entries

**Common to all twelve:** `scope: "player"`; `lifetime: "phase"`; the purse and feed as above; a first-time deed. The town of each giver's dialect is noted; `#symbols#` fill the giver's lines in that dialect, and the wolf's lines are plain.

---

#### S1. `st-wake-bread`: Bread for the King's Wake
- **Tier:** small, 10–15 minutes. Phase `s12`. Repeat `daily`, `join.cap` 6. Board `ridgemere`.
- **Where:** the giver's counter on Market Square (`w_ridgemere_1_1` (86,34)), and the Soup Kitchen `rm_the_soup_kitchen` (door `w_ridgemere_0_1` (253,69)), where 21 residents take their evenings or meals.
- **Giver:** Winifred Cinderworth `rm_winifred_cinderworth`, f 40, baker on Market Square. Plain (Ridgemere-raised, light).
  - Home `rm_thorncote_house`; works on Market Square. Alive, placed, reachable.
  - Why her: she is stubborn and warm and "watches the rain as if it might stop". She "took over the counter here when it came free, and means to keep it in the family". Not the Soup Kitchen's keeper, and not campaign cast.
- **Premise:** Half the city is at the Soup Kitchen for the old King's wake, and the Sump is there besides. The Square's baker bakes double: three loaves, one each to three mourners at the tables. This is the bakers' share; the campaign's own wake errand (Ch 1, at the Residence) is separate.
- **Objectives:**
  - `wake`: Give a loaf each to three wolves at the Soup Kitchen (`gift`; `{item: bread, cell: rm_the_soup_kitchen}`; goal 3, count).
  - `meal` (optional): Sit and share a meal at the tables (`shared meal`; `{cell: rm_the_soup_kitchen}`).
- **Triggers:** `wake_give` (`item.give bread 3` on accept), P1, P2, P3.
- **Rewards:**
  - P3: 2 pennies, `town:ridgemere`.
  - `rewards`: practice in service.
  - `bonus` (`meal`): `honey_cake` ×1.
  - Deed: "carried wake bread to the Soup Kitchen".
- **Feeds:** +2 `nh_realm`, +2 `nh_voice_ridgemere`.

**Dialogue** (Winifred, plain):
- **ask:** "Baking for the wake?"
- **say:** "#greet#. Half the city's at the Soup Kitchen for the old King's wake, and the Sump's there besides. I've baked double. Three loaves, one each to three wolves at the kitchen tables. Don't let anyone pay you for them. That's the point of a wake."
- **accept:** "Three loaves for the wake." **decline:** "Not today." **handIn:** "The loaves went to the tables."
- **thanks:** "#thanks#. He was a good King, or good enough, and he'd have hated a hungry wake. This counter's mine now, and it'll stay in the family. So will the habit of feeding the Sump."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain world.mem.nh_stage=king\|moot_called\|reckoning` | The Square's baker is baking double for the wake. She wants carriers to the Soup Kitchen, and no payment taken at the tables. |
| news · `here.region=ridgemere self.dialect=plain self.quarter=sump world.mem.nh_stage=king\|moot_called\|reckoning` | The Soup Kitchen's feeding the wake and the Sump both. Bread carried down from the Square goes a long way. |
| gossip · `here.region=ridgemere self.dialect=plain` · `world.mem.st_last=wake` | The Soup Kitchen had fresh bread for every mourner today. The Square's baker sent it, and somebody carried it. |

---

#### S2. `st-summons`: A Summons to Hillside
- **Tier:** small, 20–30 minutes (out past Rainmere and back). Phase `s12`. Repeat `daily`, `join.cap` 4. Board `ridgemere`.
- **Where:** from the City Office `rm_the_city_office` (door `w_ridgemere_1_1` (16,114)) to Hillside Farmhouse `fs_dairy_rainward_home` (door `w_ridgemere_3_2` (150,42)), "The Rainward Hills, Rainmere"; the byre is at (144,49).
- **Giver:** Olaf Pitchmore `rm_olaf_pitchmore`, m 39, clerking at the City Office. **Plain**: Coldwater-raised with accent none.
  - Home `rm_quarles_house`; work `rm_the_city_office`. Alive, placed, reachable.
  - Why him: he is blunt and anxious and "takes the long stairs two at a time". A City Office clerk sends out the Moot's written summonses. He is not Gwen Cinderdale (7), who carries the Heights round in the same office.
- **Recipient:** Isolde Gullwick `cty_isolde_gullwick`, f 54, herds the cattle at Hillside (home Hillside Farmhouse; work Hillside Byre `fs_dairy_rainward_byre`). Alive, placed, reachable. No other role in any pack. She "is always a little late".
- **Premise:** The Moot is called, and every farmstead on the old roll gets its summons in writing. This one is for Hillside.
- **Objectives:**
  - `summons`: Give the summons to the Hillside cattle herder (`gift`; `{target: cty_isolde_gullwick, item: letter}`).
- **Triggers:** `summons_give` (`item.give letter 1` on accept), P1, P2, P3.
- **Rewards:**
  - P3: 3 pennies, `town:ridgemere`.
  - `rewards`: practice in travel.
  - Deed: "carried a Moot summons out to Hillside".
- **Feeds:** +2 / +2 Ridgemere (the Rainward farms are the city's).
- **Needs:** J9 (a tagged summons) is optional.

**Dialogue** (Olaf, plain):
- **ask:** "Is the City Office sending out summonses?"
- **say:** "#greet#. The Moot's called, and every farmstead on the old roll gets its summons in writing. That's the law, and the law has two paws, both mine. This one's for Hillside, up past Rainmere, to the wolf who herds the cattle. She'll be late to the Moot. They always are."
- **accept:** "To Hillside, then." **decline:** "Not today." **handIn:** "Hillside has its summons."
- **thanks:** "#thanks#. One summons served. The law's satisfied, and so am I, nearly. Forty more on my desk. If your paws are good tomorrow, the office stairs are the long ones. I take them two at a time."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain world.mem.nh_stage=moot_called\|reckoning` | The City Office is sending the Moot's summons to every farmstead on the roll. The clerk pays carriers who don't mind hills. |
| gossip · `here.region=ridgemere self.dialect=plain` · `world.mem.st_last=summons` | Hillside has its summons for the Moot. The herder read it twice and was late to the milking. |

---

#### S3. `st-post-stone`: Stone for the Line
- **Tier:** small, 20–40 minutes, by the post chosen. Phase `s3`. Repeat `daily`, `join.cap` 6. Board `ridgemere`.
- **Where:** the Greatly Masons' Yard `rm_the_greatly_masons_yard` (door `w_ridgemere_1_0` (173,170)), and the three Line posts that stand open in the world (decisions §4.5): proj-1, the Frosthollow watch post at the Upper Mere (`w_ridgemere_8_6`); proj-2, the Oakhaven road post at Alder Mere (`w_ridgemere_5_4`); proj-3, the Ridgemere waystation (`w_ridgemere_1_1`). Each needs 4 stone.
- **Giver:** Frida Netherby `rmx_frida_netherby`, f 47, runs the Greatly Masons' Yard. Plain (Ridgemere-raised, light).
  - Home `rm_netherby_house_2`; work `rm_the_greatly_masons_yard`. Alive, placed, reachable.
  - Why her: she is dreamy and warm and "keeps a pocketful of coal for luck". She "answers to the steward for every load" of the House that owns the yard, and is "saving for a stall on the Commons, coin by coin". She is not of House Greatly's cast.
- **Premise:** The Line posts want four stone apiece, and only one purse in the realm holds any (4 blocks). The yard can spare two dressed blocks a day. Carry them to whichever post you choose and give them in.
- **Objectives:**
  - `post`: Give the two stones to a Line post (`project gift`; `{item: stone}`; goal 2, quantity).
- **Triggers:**
  - `stone_give`: `item.give stone 2` on accept. Minted two at a time, like any prop (decisions §3 counts coin, not goods).
  - `post_credit_1`, `_2`, `_3`: on `project gift` with `match.target` = `proj-1`, `proj-2` or `proj-3`, if `any [quest is st-post-stone active, ready; quest is st-post-timber active, ready]` of `$actor`, add the post's town strand: +3 `nh_voice_frosthollow`, +2 `nh_voice_oakhaven` or +2 `nh_voice_ridgemere`. (The quest goes `ready` on the gift and is handed in to Frida, so either status may hold when the trigger runs.)
  - P1 adds only `nh_realm` +2 and `st_done`; P2; P3.
- **Rewards:**
  - P3: 3 pennies, `town:ridgemere`.
  - `rewards`: practice in labour.
  - Deed: "carried stone to a Howl Line post".
- **Feeds:** +2 `nh_realm`, and the post's town as above. The campaign scores its own +1 for each project gift (`campaign.md` §5.2); the two counts are separate and both stand.

**Dialogue** (Frida, plain):
- **ask:** "Is the yard selling stone for the posts?"
- **say:** "#greet#. The Line posts out on the roads want four stone apiece, and this yard can spare two dressed blocks a day. Take them to whichever post you like: the Upper Mere, Alder Mere, or our own waystation. I'll write it in the steward's book as wastage."
- **accept:** "Two blocks, to a post." **decline:** "My back says no." **handIn:** "The stone's at a post."
- **thanks:** "#thanks#. Two blocks, set where they're wanted. I keep a coal in my pocket for luck, and I've a feeling we'll need it on those roads. Same again tomorrow, if your back holds."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain world.mem.nh_stage=signals` | The masons' yard by Northgate gives a carrier two blocks a day for the Line posts. The posts want four apiece. |
| news · `here.region=oakhaven self.dialect=oakhaven world.mem.nh_stage=signals` | The post at Alder Mere wants stone, mm? The city masons send it out, two blocks at a time. |
| gossip · `here.region=ridgemere self.dialect=plain` · `world.mem.st_last=stone` | There's new stone at one of the Line posts. The masons' yard sent it, and the steward hasn't noticed. |

---

#### S4. `st-post-timber`: Timber for the Line
- **Tier:** small, 20–40 minutes. Phase `s3`. Repeat `daily`, `join.cap` 6. Board `ridgemere`. It replaces the old seed "fish for the post-builders" (decisions §7.3).
- **Where:** the giver's coppice in the Rainward Hills (`w_ridgemere_2_3` (107,253)), and the same three posts: proj-1 and proj-2 need 10 timber, proj-3 needs 8.
- **Giver:** Odo Icewell `cty_odo_icewell`, m 49, woodward of the Rainwood coppice. Plain (Ridgemere-raised, accent none).
  - Home `r2_kelp_stair_house_top`; work `w_ridgemere_2_3` (107,253). Alive, placed, reachable.
  - Why him: he is quietly funny and blunt and "has opinions about everyone's grandmother", and he "is the third generation to hold this trade".
- **Premise:** No purse in the realm holds timber at the checkpoint, and timber isn't foraged. The woodward cuts two straight poles a day for the Line. Carry them to a post.
- **Objectives:**
  - `post`: Give the two poles to a Line post (`project gift`; `{item: timber}`; goal 2, quantity).
- **Triggers:** `timber_give` (`item.give timber 2` on accept); the shared `post_credit_*` triggers of S3; P1 (`nh_realm` +2), P2, P3.
- **Rewards:**
  - P3: 3 pennies, `town:ridgemere`.
  - `rewards`: practice in labour.
  - Deed: "carried coppice timber to a Howl Line post".
- **Feeds:** as S3.

**Dialogue** (Odo, plain):
- **ask:** "Are those poles for the posts?"
- **say:** "#greet#. Two poles a day out of the coppice, cut straight, for the Line posts. They want eight or ten apiece, and nobody in this city sells timber. Carry these to whichever post you like. My grandmother cut this coppice, and she'd not have trusted you with an axe."
- **accept:** "Two poles, to a post." **decline:** "Not today." **handIn:** "The poles are at a post."
- **thanks:** "#thanks#. Two poles, standing where they're needed. My grandmother would say a post is only as good as the wolf who carried it. She said that about most things."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain world.mem.nh_stage=signals` | The coppice woodward cuts two poles a day for the Line posts. He wants carriers, and says so bluntly. |
| news · `here.region=frosthollow self.dialect=frosthollow world.mem.nh_stage=signals` | The Upper Mere post wants timber, stone. Poles come out from the city coppice, two at a time. |
| gossip · `here.region=ridgemere self.dialect=plain` · `world.mem.st_last=timber` | Fresh poles at one of the Line posts. The coppice woodward's, by the cut of them. |

---

#### S5. `st-chorus-wrong`: The Chorus That Sang Wrong
- **Tier:** small, 20–30 minutes. Phase `s4`. Repeat `weekly`, `join.cap` 6. Board `southmeadow`.
- **Where:** Southmeadow (`w_ridgemere_8_13`): the two households at Southmeadow Cottage 2 `cn_southmeadow_cottage_2`, and the Vine and Sickle `cn_southmeadow_alehouse` (door (97,28)).
- **Giver:** Alder Blossomcroft `cty_alder_blossomcroft`, m 21, farmhand at Larkholm. Southmeadow (broad).
  - Home `cn_southmeadow_cottage_1`; work `fs_grain_lark_granary`. Alive, placed, reachable.
  - Why him: he is loud and unhurried and "tells the same three stories", and this is now one of them. His backstory (a sibling lost to a hard winter) stays unspoken.
- **Targets** (no hub item; each holds no more than one other target role):
  - Briar Sedgebridge `cty_briar_sedgebridge`, f 51, digs clay at the Sallow pits, home `cn_southmeadow_cottage_2`. Alive, placed, reachable. No other role. Playful and warm.
  - Tor Chalkfield `cty_tor_chalkfield`, m 24, carter at Southmeadow, home `cn_southmeadow_cottage_2`, works the square. Alive, placed, reachable. He gives `mq-waste-road-salt`; this is his only target role.
  - The innkeeper of the Vine and Sickle is not a target (decisions §1.2.4): the table is a place.
- **Premise:** At the howl practice on the square, half the chorus sang the burying howl at a wedding. Two households aren't speaking. Carry the apology: mead to the clay digger's, honey to the carter's, then sit at the Vine and Sickle's table where the elders play. (The Keepers' own Ch 4 howl quest is the campaign's; this is the comedy beside it.)
- **Objectives:**
  - `mead`: Give the jug of mead to the clay digger's household (`gift`; `{target: cty_briar_sedgebridge, item: mead}`).
  - `honey`: Give the crock of honey to the carter's household (`gift`; `{target: cty_tor_chalkfield, item: honey}`).
  - `table` (optional): Win a round at the Vine and Sickle's table, and let the elders laugh about it (`table game`; `{cell: cn_southmeadow_alehouse}`; the event's `actor` is the winner).
- **Triggers:** `chorus_give` (`item.give mead 1` and `item.give honey 1` on accept: no purse holds honey, and only six hold mead), P1, P2, P3.
- **Rewards:**
  - P3: 3 pennies, `town:southmeadow`.
  - `rewards`: practice in service.
  - `bonus` (`table`): `honey_cake` ×1.
  - Deed: "mended the wedding the chorus sang wrong at Southmeadow".
- **Feeds:** +4 `nh_realm`, +4 `nh_voice_southmeadow`.

**Dialogue** (Alder, southmeadow):
- **ask:** "What happened at the wedding?"
- **say:** "Good 'arvest. We practised the gathering howl on the square, and half the chorus sang the burying one instead. At a wedding. Two households aren't speaking. Take mead to the clay digger's and honey to the carter's, from me, and say sorry ripe and proper."
- **accept:** "Mead and honey, and sorry." **decline:** "That's your mess, not mine." **handIn:** "They're speaking again."
- **thanks:** "#thanks#. Bless you. They're speaking again, and laughing, which is worse. I've three stories, and that's the best of them now. Sheaf and sickle, never again."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=southmeadow self.dialect=southmeadow world.mem.nh_stage=howls` | The Larkholm lad's sending sorry-gifts round for the howl that went wrong at the wedding. Carry one and you'll hear the story, bless you. |
| news · `here.region=southmeadow self.dialect=southmeadow world.mem.nh_stage=howls` | Practising the howl? Mind which one. We've one for the dead and one for a wedding, and the square mixed them, ripe and proper. |
| gossip · `here.region=southmeadow self.dialect=southmeadow` · `world.mem.st_last=chorus` | The two households are speaking again since the howl. Somebody carried mead and honey and said the right thing. |

---

#### S6. `st-bounds-water`: Bread for the Bounds
- **Tier:** small, 20–30 minutes. Phase `s56`. Repeat `daily`, `join.cap` 8. Board `ridgemere`.
- **Where:** the Rainward Orchard House `fs_orchard_rainward_home` (door `w_ridgemere_2_3` (130,112)) and its Drying House (125,119); the bounds are the Rainward Hills cells `w_ridgemere_0_3`, `1_3`, `2_3`, `3_0`, `3_1`, `3_2`, `3_3`, `4_2`, `4_3`, `5_2`, `5_3`, and the cut `w_ridgemere_4_4`.
- **Giver:** Juniper Larkham `cty_juniper_larkham`, f 54, tends the orchard at Rainward. Frosthollow (light).
  - Home `fs_orchard_rainward_home`; work `fs_orchard_rainward_drying`. Alive, placed, reachable.
  - Why her: she is unhurried and generous and "hums old marching songs", and she "swore an oath at a wedding and takes it seriously".
- **Premise:** Wolves are walking the old bounds of the cut through the Rainward Hills (`campaign.md` Ch 5), long days out on the hills. Juniper sends bread up: a loaf each to three wolves you meet out on the bounds.
- **Objectives:**
  - `bread`: Give a loaf each to three wolves out on the Rainward Hills (`story signal` `bounds-bread`, goal 3). The phase trigger `bounds_bread` on `gift` `{item: bread}` with the quest active and `any [field cell = …]` over the twelve cells (two nested `any`s of 6) runs `objective.progress amount 1`. That is SQ1 until an objective can name cells. Any wolf counts, resident or not.
- **Triggers:** `bounds_give` (`item.give bread 3` on accept), `bounds_bread`, P1, P2, P3.
- **Rewards:**
  - P3: 2 pennies, `town:ridgemere`.
  - `rewards`: `waterskin` ×1 (no purse holds one, so it is the giver's) and practice in travel.
  - Deed: "carried bread out to the wolves on the bounds".
- **Feeds:** +2 `nh_realm`, **+2 `nh_voice_oakhaven`**: the bounds are Oakhaven's cause, and a deed is credited to the town it serves (`campaign.md` §5.3).

**Dialogue** (Juniper, frosthollow):
- **ask:** "Who's all that bread for?"
- **say:** "#greet#, #address#. There's wolves out walking the old bounds of the cut, long days on the hills, and nobody feeds a wolf on a boundary. Three loaves, one each to whoever you meet up on the Rainward Hills. Keep your lamp lit on the high paths, eh."
- **accept:** "Three loaves for the bounds." **decline:** "Not today." **handIn:** "The bounds-walkers are fed."
- **thanks:** "Lamp's thanks. I swore at a wedding once to feed whoever came to my door. The bounds are a long way from my door, but an oath runs deep, stone. Take a waterskin for your own walking."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain world.mem.nh_stage=letters\|lights` | The Rainward orchard keeper sends bread up to the wolves walking the bounds. Carry some and she'll fill your waterskin. |
| news · `here.region=oakhaven self.dialect=oakhaven world.mem.nh_stage=letters\|lights` | Bread comes up from the Rainward orchard for the bounds-walkers, mm? Good bark, that keeper. |
| gossip · `here.region=ridgemere self.dialect=plain` · `world.mem.st_last=bounds` | The wolves on the bounds were fed today. The orchard keeper's bread, carried up into the hills. |

---

#### S7. `st-pull-stake`: A Green Stake (board-only)
- **Tier:** small, 15–20 minutes. Phase `s56`. Repeat `daily`, `join.cap` 6. **Board-only** on `board:oakhaven` (decisions §1.2.6): no `giver`, no `offer`, no hand-in.
- **Where:** the Oakhaven square, region `oak_square` (`w_ridgemere_3_4` (236,188)–(252,204), the program's own copy), and the Oakhaven cut `w_ridgemere_4_4`.
- **The offer:** P9 `stake_offer` on `region entered` `oak_square`, with the player timer `t_stake_offer`.
- **Board text** (the `summary`): "Bounders in green have driven stakes into the Oakhaven cut to claim it. Pull one, and take it to the logger at the cut."
- **Target:** Ingram Ashleaf `cty_ingram_ashleaf`, m 40, logger at the Oakhaven cut (work `w_ridgemere_4_4` (29,35)); home `cn_oakhaven_cottage_1`. Alive, placed, reachable. He gives `sq-sleepless-logger` (28); this is his only target role.
- **Premise:** Bounders have staked the cut (`campaign.md` Ch 5, "Walk the Old Bounds"). The pack places its own stake for each wolf, so the quest never waits on the campaign's.
- **Objectives:**
  - `stake`: Pull a green stake in the cut (`item picked up`; `{item: timber, cell: w_ridgemere_4_4}`).
  - `give`: Give it to the logger at the cut (`gift`; `{target: cty_ingram_ashleaf, item: timber}`; `after: [stake]`).
- **Triggers:**
  - `stake_place`: on `quest accepted` (`match.detail`), `random` over three tiles in the cut, about (40,48), (52,30) and (22,60), snapped to standable tiles, each running `item.place {item: timber, quantity: 1}`. `item.place` is not undone (J9 would tag it); an untaken stake is one pole of litter.
  - The campaign's Ch 5 scores its own +4 for a stake pulled in the cut (`item picked up timber`), so this pull counts for the bounds too.
  - P1, P2, P3; the close below.
- **Close:** on `quest completed`, `story.tell {to: wolf, player: $actor}` with no `from`: "The logger snaps the stake across his knee and throws it on the fire."
- **From C5:** Ingram's `npc.greet` (oakhaven): "Root well. That stake burned sweet, mm? The cut's ours yet, and you pulled for it."
- **Rewards:**
  - P3: 2 pennies, `town:oakhaven`.
  - `rewards`: practice in labour.
  - Deed: "pulled a bounder's stake from the Oakhaven cut".
- **Feeds:** +2 `nh_realm`, +2 `nh_voice_oakhaven`.

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=oakhaven self.dialect=oakhaven world.mem.nh_stage=letters\|lights` | Green stakes in the cut, mm? Pull one and take it to the logger. The notice is up on the square. |
| gossip · `here.region=oakhaven self.dialect=oakhaven` · `world.mem.st_last=stake` | Another green stake on the logger's fire. Good bark, whoever pulled it. |
| gossip · `self.id=cty_ingram_ashleaf` · `world.mem.st_last=stake` (his own line, with C8) | That stake burned sweet, mm? Somebody pulled it for the cut. Root well, whoever they were. |

---

#### S8. `st-fog-lanterns`: Lamps on the Boardwalk
- **Tier:** small, 15–20 minutes around dusk. Phase `s56`. Repeat `daily`, `join.cap` 4. Board `fenshore`.
- **Where:** the Fen Shore Boat Shed `fs_fishing_fencoast_shed` (door `w_ridgemere_2_14` (168,190)), and three boardwalk lamps by the Fenshore square, regions `lamp_1` = `w_ridgemere_3_13` (84,144)–(88,148), `lamp_2` = (96,150)–(100,154) and `lamp_3` = (108,146)–(112,150), snapped to the boardwalk at authoring.
- **Giver:** Stone Marshmore `cty_stone_marshmore`, m 41, deckhand at Fen Shore. **Plain**: Fenshore-raised with accent none.
  - Home `cn_fenshore_cottage_2`; work `fs_fishing_fencoast_shed`. Alive, placed, reachable.
  - Why him: he is suspicious and curious and "tells the same three stories", and he "is the third generation to hold this trade".
- **Premise:** With lights walking out on the fen (`campaign.md` Ch 6), Fenshore wants its own lamps lit along the boardwalk at dusk, so a wolf knows a true light from a false one. **Benign lamps only**: nothing here touches the false lights, `nh_b_lights` or the Lamp Store.
- **Objectives:**
  - `lamp1`, `lamp2`, `lamp3`: Light the boardwalk lamps at dusk (`story signal` each). P5: on `region entered` `lamp_<n>`, with the quest active, `calendar is period dusk` and `carries {item: lamp_oil, op: ">=", value: 1}`, run `objective.complete lamp<n>`.
- **Triggers:** `lamps_give` (`item.give lamp_oil 2` on accept: lamp oil is in only five purses, all in the city), `lamp_1`–`lamp_3`, P1, P2, P3.
- **Rewards:**
  - P3: 3 pennies, `town:fenshore`.
  - `rewards`: practice in watch.
  - Deed: "lit the Fenshore boardwalk lamps".
- **Feeds:** +4 `nh_realm`, +4 `nh_voice_fenshore`.

**Dialogue** (Stone, plain):
- **ask:** "Who lights the boardwalk lamps?"
- **say:** "#greet#. There's lights walking out on the fen of a night, and nobody wants to follow the wrong one. Light the three boardwalk lamps by the square at dusk, so a wolf knows which lights are ours. Here's oil from the shed. My father did this, and his father before him."
- **accept:** "Three lamps, at dusk." **decline:** "Not tonight." **handIn:** "All three are lit."
- **thanks:** "#thanks#. Three lamps lit, and every one of them ours. I'll tell you a story about the marsh lights one day. I've only three stories, mind, and you'll hear all of them twice."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=fenshore self.dialect=mirebridge world.mem.nh_stage=lights` | The Fen Shore deckhand wants the boardwalk lamps lit at dusk, no? So a wolf can tell our lights from the ones out on the fen. |
| gossip · `here.region=fenshore self.dialect=mirebridge` · `world.mem.st_last=lamps` | The boardwalk lamps were lit in good time. Dry feet and true lights, fen-sure. |

---

#### S9. `st-ford-blankets`: Blankets for the Ford
- **Tier:** small, 15–25 minutes. Phase `s78`. Repeat `daily`, `join.cap` 6. Board `larkfield`.
- **Where:** Larkfield Looms `cn_larkfield_weavers` (door `w_ridgemere_6_9` (165,76)), and the Larkfield ford at (140,76) in the same cell (`campaign.md` §5.2, post 8), region `ford` = (132,68)–(148,84).
- **Giver:** Quill Oakdell `tn_oh_quill_oakdell`, f 52, loom hand at Larkfield Looms. Larkfield (light).
  - Home `cn_larkfield_cottage_4`; work `cn_larkfield_weavers`. Alive, placed, reachable.
  - Why her: she is warm and suspicious, "distrusts anything from the city", and "carves birds and gives them away". She is not Heather Reedwater, who gives the Looms' medium quest (Q11).
- **Premise:** The wolves holding the Larkfield ford (`campaign.md` Ch 7) sleep out by the water on the siege nights. The Looms have three blankets fit for it.
- **Objectives:**
  - `blankets`: Give a blanket each to three wolves at the ford (`gift`; `{item: blanket, cell: w_ridgemere_6_9}`; goal 3, count).
  - `night` (optional): Stay by the ford through the first hour after midnight (`story signal`; P6 on `hour`[w] `{detail: "1"}`, each participant with the quest active and `wolf in region ford`).
- **Triggers:** `blankets_give` (`item.give blanket 3` on accept: only 14 blankets are in five purses, three of them in the city), P1, P2, P3.
- **Rewards:**
  - P3: 3 pennies, `town:larkfield`.
  - `rewards`: practice in service.
  - `bonus` (`night`): practice in watch.
  - Deed: "carried blankets to the wolves at the Larkfield ford".
- **Feeds:** +3 `nh_realm`, +3 `nh_voice_larkfield`.

**Dialogue** (Quill, larkfield):
- **ask:** "Who are those blankets for?"
- **say:** "#greet#. The wolves holding the ford sleep out by the water these nights, and the Looms have three blankets fit for it. One each to three wolves at the ford, eh? Full weight, every one. I'd not trust a city carrier with them, but you'll do."
- **accept:** "Three blankets, to the ford." **decline:** "Not today." **handIn:** "The ford wolves have them."
- **thanks:** "#thanks#. Three warm wolves at the ford. I carve birds while I wait and give every one away; it's the carving I like, not the keeping. I'll have one for you next time. Fair price to you."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=larkfield self.dialect=larkfield world.mem.nh_stage=harvest\|bell` | The Looms send blankets down to the wolves at the ford, eh? The loom hand wants carriers, and she'll give you a hard look first. |
| gossip · `here.region=larkfield self.dialect=larkfield` · `world.mem.st_last=blankets` | The wolves at the ford have blankets now. From the Looms, carried down full weight. |

---

#### S10. `st-feed-larkfield`: Bread for the Burned Houses (gated on the ford)
- **Tier:** small, 15–25 minutes. Phase `s78`, and **only if `nh_b_ford = burned`**. Repeat `daily`, `join.cap` 6. **No `giver` and no board** (P10), so the branch gate can't leak.
- **Where:** the Longmead Granary `fs_grain_long_granary` (Longmead, the ford's farm), and Larkfield `w_ridgemere_6_9`.
- **Holder:** Brand Sunham `cty_brand_sunham`, m 35, farmhand at Longmead. Larkfield (light). He holds the quest's two `npc.offer` choices, which count as his one pack quest (decisions §1.2.1).
  - Home `cn_larkfield_cottage_1`; work `fs_grain_long_granary`. Alive, placed, reachable.
  - Why him: he is honest to a fault and warm and "never sits with a back to the door". His debt in Ridgemere stays unspoken. He is not Rook Wheatfield, who farms Longmead and is campaign cast.
- **The offer (P10):** `feed_offer` on `near resident` `{target: cty_brand_sunham}`, if `compare nh_b_ford = "burned"`, the quest is not active, offered or ready for `$actor`, and `any [quest is none, days since t_feed_offer >= 1]`: `start timer t_feed_offer`, `quest.offer`, and `npc.offer` `feed-ask` on Brand for `$actor` (`once: true`).
- **Premise:** The ford burned (`campaign.md` Ch 7), and the houses by it with it. Longmead's grain is what's left: bread baked from it and meal besides, for whoever lost a roof.
- **Objectives:**
  - `bread`: Give a loaf each to three wolves of Larkfield (`gift`; `{item: bread, cell: w_ridgemere_6_9}`; goal 3, count).
  - `meal`: Give the meal to a Larkfield household (`gift`; `{item: meal, cell: w_ridgemere_6_9}`; goal 2, quantity).
- **Triggers:** `feed_offer`, `feed_give` (`item.give bread 3` and `item.give meal 2` on accept), `feed_thanks` (on `quest completed`: `npc.offer` `feed-thanks` on Brand for `$actor`, `once: true`), P1, P2, P3.
- **Rewards:**
  - P3: 3 pennies, `town:larkfield`.
  - `rewards`: practice in service.
  - Deed: "fed the burned-out houses of Larkfield".
- **Feeds:** +3 `nh_realm`, +3 `nh_voice_larkfield`.

**Dialogue** (Brand, larkfield; all in his window):
- **`feed-ask`** words: "What happened at the ford?" reply: "The ford burned, and the houses by it with it. Longmead's grain is what's left, eh? There's bread baked from it, and meal besides. Carry it to whoever lost a roof. I'll not short-weigh a wolf who's lost everything."
- **`feed-thanks`** words: "The bread's given out." reply: "#thanks#. Full weight, every loaf. I'll be honest, since I can't help it: I'd sooner the ford had held. But it didn't, and you came. That's the trade."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=larkfield self.dialect=larkfield world.mem.nh_stage=harvest\|bell world.mem.nh_ford=burned` | The Longmead farmhand's giving out bread for the burned houses, eh? Find him at the granary and he'll load you up. |
| gossip · `here.region=larkfield self.dialect=larkfield` · `world.mem.st_last=feed` | The burned houses by the ford had bread today. Longmead grain, carried in. |

---

#### S11. `st-haulers-food`: Bread for the Haulers (gated on Ch 8)
- **Tier:** small, 20–30 minutes. Phase `s78`, and **only while `nh_stage = 8`** (the bell haul). Repeat `daily`, `join.cap` 6. **No `giver` and no board** (P10).
- **Where:** the Coldwater Bakehouse `cn_coldwater_bakehouse` (door `w_ridgemere_7_7` (154,3)), and the Upper Mere Boat Shed `fs_fishing_uppermere_shed` (door `w_ridgemere_9_6` (99,39)).
- **Holder:** Lichen Harvestton `tn_sm_lichen_harvestton`, f 55, shop hand at Coldwater Bakehouse. Coldwater (light).
  - Home `cn_coldwater_cottage_5`; work `cn_coldwater_bakehouse`. Alive, placed, reachable.
  - Why her: she is easily offended and guarded, "collects smooth stones from the riverbanks", and "wants to plant the biggest orchard in the south". She is not the bakehouse's baker, who gives 21; no line names either.
- **Recipient:** Thorn Mistmere `cty_thorn_mistmere`, m 19, boatman at Upper Mere (home `cn_coldwater_cottage_4`; work the boat shed). Alive, placed, reachable. No other role in any pack. Not Kell Greyton, the shed's skipper, who is campaign cast.
- **The offer (P10):** `haul_offer` on `near resident` `{target: tn_sm_lichen_harvestton}`, if `compare nh_stage = 8`, not active, offered or ready, and once a game day (`t_haul_offer`): `quest.offer` and `npc.offer` `haul-ask` on Lichen for `$actor`.
- **Premise:** The haulers at the Upper Mere are out on the ice with ropes all day (`campaign.md` Ch 8), and a cold wolf eats twice. Three loaves up to the boat shed.
- **Objectives:**
  - `bread`: Give the three loaves to the boatman at the Upper Mere boat shed (`gift`; `{target: cty_thorn_mistmere, item: bread}`; goal 3, quantity).
  - `fish` (optional): Smoked fish for the haulers (`gift`; `{target: cty_thorn_mistmere, item: smoked_fish}`; goal 2, quantity). 29 purses hold 81.
- **Triggers:** `haul_offer`, `haul_give` (`item.give bread 3` on accept), `haul_thanks` (`npc.offer` `haul-thanks` on Lichen for `$actor` on `quest completed`), P1, P2, P3.
- **Rewards:**
  - P3: 3 pennies, `town:coldwater`.
  - `rewards`: practice in service.
  - `bonus` (`fish`): practice in labour.
  - Deed: "carried bread to the haulers on the Upper Mere".
- **Feeds:** +3 `nh_realm`, +3 `nh_voice_coldwater`.

**Dialogue** (Lichen, coldwater; all in her window):
- **`haul-ask`** words: "Who's the bread for?" reply: "The haulers at the Upper Mere boat shed, eh? They're out on the ice all day with ropes, and a cold wolf eats twice. Three loaves to the young boatman there. Smoked fish too, if you can find any."
- **`haul-thanks`** words: "The haulers have their bread." reply: "#thanks#. Fair catch. I'd give you a smooth stone off my shelf, but I've counted them and I'd miss it. One day I'll plant an orchard so big the haulers can eat apples instead."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=coldwater self.dialect=coldwater world.mem.nh_stage=bell` | The bakehouse sends bread up to the haulers on the mere, eh? The shop hand wants carriers who don't mind the cold. |
| gossip · `here.region=coldwater self.dialect=coldwater` · `world.mem.st_last=haul` | The haulers at the boat shed had bread today, and fish besides. Somebody carried it up, eh? |

---

#### S12. `st-moot-benches`: Benches for the Moot
- **Tier:** small, 15–20 minutes. Phase `s9`. Repeat `daily`, `join.cap` 6. Board `ridgemere`.
- **Where:** the Market Hall `r2_the_market_hall` (doors `w_ridgemere_1_1` (84,16) and (123,16)), and Market Square, region `moot_square` = `w_ridgemere_1_1` (84,20)–(124,50).
- **Giver:** Runa Gullley `rm_runa_gullley`, f 41, carpenter in the Market Hall. **Mirebridge speech** (Fenshore-raised, light).
  - Home `rm_saltwick_house`; work `r2_the_market_hall`. Alive, placed, reachable.
  - Why her: she is dreamy and devout and "coughs in the damp and pretends not to". She "was born in the Sump" and "took over the counter here when it came free".
- **Premise:** The Moot sits on Market Square, and every community needs a bench to sit on. Bring planks and nails; she'll knock them together.
- **Objectives:**
  - `planks`: Bring four planks (`gift`; `{target: rm_runa_gullley, item: planks}`; goal 4, quantity). 149 purses hold 290.
  - `nails`: Bring a paw of nails (`gift`; `{target: rm_runa_gullley, item: nails}`; goal 4, quantity). 121 purses hold 275.
  - `square` (optional): Be on the Square when the benches go out, between the ninth hour and the tenth (`story signal`; P5 on `region entered` `moot_square` with `hour between 9 10`).
- **Triggers:** P1, P2, P3, the `square` signal.
- **Rewards:**
  - P3: 3 pennies, `town:ridgemere`.
  - `rewards`: practice in craft.
  - `bonus` (`square`): practice in service.
  - Deed: "built benches for the Moot".
- **Feeds:** +2 `nh_realm`, +2 `nh_voice_ridgemere`.

**Dialogue** (Runa, mirebridge):
- **ask:** "Building something for the Moot?"
- **say:** "#greet#. Benches for the Moot, no? Every community gets one to sit on, and the Square's got none. Four planks and a paw of nails, and I'll knock them together. Dry wood, mind. Wet wood makes me cough, and I don't cough."
- **accept:** "Planks and nails, coming." **decline:** "Not today." **handIn:** "Planks and nails for the benches."
- **thanks:** "#thanks#. Fen-sure, that's a bench a Moot can sit on. The Bright One sees who builds the seats, and so will everyone sitting on them. Dry feet to you."

**Talk:**

| Node · gate | Line |
|---|---|
| news · `here.region=ridgemere self.dialect=plain world.mem.nh_stage=moot` | The carpenter in the Market Hall is building benches for the Moot. She wants planks and nails, and dry ones. |
| news · `here.region=ridgemere self.dialect=mirebridge world.mem.nh_stage=moot` | The fen carpenter in the Market Hall wants planks for the Moot benches, no? Dry ones, or she'll cough at you. |
| gossip · `here.region=ridgemere self.dialect=plain` · `world.mem.st_last=benches` | There's a new bench on the Square for the Moot. The fen carpenter's work, they say. |

---

## 3. Engine needs

The names are decisions §5.1's: C12a `story.notify`, L2 for camps, SQ2 for tills (M6 is the same fix), N1 for "Ask about…", N4 for `join.again`.

**Which engine phases the small pack needs** (decisions §5.2, engine-spec §7): **Phase 1 only** (V1, SQ2, J1, C8), plus C2 for the garland rope (38). Every quest here also runs today, with the fallbacks below, so the packs ship with Act I's today-only version (decisions §6.2). The stage pack needs nothing beyond what its tiers need; its small tier needs nothing at all.

### 3.1 What runs today, with no engine change
All 48 quests and the 12 stage entries run on today's engine through the patterns in §1.3:
- quests with givers and boards; board-only quests (P9) and gated journal offers held by a resident (P10);
- gift, hunt, forage, archive, trade, enter-place, project-gift, region and Gift-use objectives;
- triggers on watched events (`near resident`, `item picked up`, `hour`, `weather changed`, `day begins`) and on world events (`ship docks`, `caravan arrives`, `story started`);
- `npc.move`, `npc.hold`, `npc.follow`, `item.place`, `npc.offer` and `quest.offer`;
- the one purse rule (P3), first-time deeds, world-variable feeds, the festival phase and the stage phases.

What is lost without the items below:
- campaign-aware gossip (C8), except deed gossip and the stage pack's helpful lines, which ride the campaign's own `nh_stage` key;
- per-wolf hint offers (J1): today the `player` field scopes them already;
- tagged props (J9);
- Gift-gated options (J10);
- the wolf's own campaign tracker crediting pack deeds (C12a or J2);
- the targets' thanks on a later visit (C5) in the board-only quests;
- `join.again` (N4) on the `join.cap` 1 and 2 quests.

### 3.2 Items from `gaps.md`, `campaign.md` and engine-spec this pack uses

| Item | Phase | What it gives these quests | Fallback today |
|---|---|---|---|
| **V1** (+ `alive`) and **SQ2** | 1 | honest approval of givers, targets and purses; tills as purses | the checks by hand (§4.2); town stores and the treasury only (decisions §3.2.5) |
| **C8** `talk.remember` | 1 | the `sq_on`, `sq_last`, `st_last` and `nh_coin` town keys that gate the helpful, "someone did it" and thin-purse lines (§1.8) | the DM seeds `sq_on=yes` per town and `nh_coin=thin` by hand; the `*_last` lines don't play; deed gossip (P2) still does |
| **J1** `npc.offer` keeps a `players` list | 1 | the lost-pup hints (3, 23), Fallow's names (40), the estates' coloured lines (46–48) and S10/S11's choices go only to the wolf they belong to | the `player` field, `once: true`; harmless |
| **C6** `offer.during` | 4 | the giver's line while the wolf is under way ("Any sign of him?", "Still on the stairs?") | the journal's summary only |
| **C5** `npc.greet` | 3 | a giver, or a board-only quest's target, thanking a wolf on their next visit | the deed greeting "You … didn't you?"; the target's own `say[self.id=…]` line with C8 |
| **C12a** `story.notify` (or **J2** realm variables) | 4 | pack deeds tick the wolf's "Your voice" tracker in the running chapter | world variables, polled hourly by the campaign |
| **J9** tagged story items | 6 | the letters (7, S2), the crate (16), the wool tufts (25), the stake (S7), taken back when the run ends | generic items; stray wool counts; placed tufts and stakes are not removed |
| **J10** `wolf has gift` | 4 | the Gift options in 19, 42, 43 and 45 are shown only to wolves with that Gift | offered to all, labelled "if you carry the … Gift" |
| **C2** `audience` operand | 2 | the garland rope (38) follows the wolves signed in, not only the helpers | the helper-grown goal (§2.7, 38) |
| **N4** `join.again` | 2 | a per-wolf wait of 7 days on 3, 23, 25 and 30, from any end | weekly repeat, a 1-day deadline, a second copy where the world allows (§1.4) |
| **L2** `camp` names | 2 | none needed: the pack's bounties match `bandit falls` by `cell` (decisions §4.1), which counts natural and story camps alike | — |

### 3.3 New items found while designing the small pack
These are written in the catalogue's style. Their phases are decisions §5.2's.

**SQ1. Where an objective counts: `where` on an objective (Phase 2, convenience).**
```json
{"id":"birds","label":"Take two pheasants in the Rainward Woods.","on":"hunted","match":{"target":"pheasant"},
 "where":{"cells":["w_ridgemere_2_0","w_ridgemere_2_2"]},"goal":2}
```
- `where` takes `{cells: [ids]}` or `{region: <story region>}`. The event counts only if its `cell` (or tile) is inside.
- `story_quests.track` tests it after `rules.matches`. The validator checks the cells and regions exist.
- Used by 18, 25 and S6. Until then they use a trigger plus `objective.progress` (P4/P6 style).

**SQ2. Tills that pay: purse names for a business's till (Phase 1, a bug fix the campaign needs too).**
- `story_quests.check_purse` accepts `till:<id>` only with two parts (`len(parts) == 2`).
- But the game's till accounts are named `till:job:<founder id>`. The checkpoint has, for example, `till:job:rm_eleanor_saltcot` holding 161 pennies.
- So `till:job:<id>` is refused at validation, and `till:<id>` names no account (`Game::storyPurseAccount` passes it through unchanged).
- **No merchant's till can pay a story reward today.** That includes the campaign's Purse path (`campaign.md` §2.7).
- **Fix** (engine-spec SQ2): accept `till:job:<id>` in `check_purse` (three parts, the second `job`), map `till:<id>` to `till:job:<id>` in `storyPurseAccount`, and refuse at approval a purse whose account is not in the checkpoint's `society.accounts`.
- **Until SQ2 ships, no small quest names a till** (decisions §3.2.5). After it, a new version of the city pack may move the city errands to their givers' own tills (cash at the 2026-11-22 checkpoint):
  - Tove 154, Rosamund 182, Kari 80, Thyra 199, Agnes 136, Wynn 103, Philippa 130;
  - Corwin Bilgestack 99, Wystan 113, Corwin Sootstack 114.

  Each till then gets its own counter `coin_till_<id>_day` with a day cap of about 10% of its cash, under the same floor rule, and the reason still reads as the shopkeeper paying.

**SQ3. Lend a paw to a resident at work (Phase 9, deferred; it emits the existing `together` event).**
- Today `World::mayLend` (`Core/RatwTogether.cpp:223`) refuses when either side is a resident (`h->npc || w->npc`). So "help at the stall", "work a shift at the ropewalk" or "rake a pan beside her" cannot be a `together` objective: that event only ever pairs two wolves.
- **Proposal:** allow a wolf to lend a paw to a resident who is `atWork`, within the same `lendTiles`. The joint's beats are the resident's work activity, and the end of the joint writes `together {actor: wolf, target: resident, item: activity, quantity: beats}`.
- Then 5, 10, 13, 30 and 40 gain a real "work beside them" objective, and the campaign's labour credits get a natural event.
- Rules: once a game day per pair, as `shared meal` is, and no wages from the resident's purse.
- Fallback: `gift`, `trade done` and `conversation` objectives, as written.

**SQ4. A day cap on quest rewards (Phase 9, deferred).**
- It would have been `"rewardLimit":{"perDay":4,"scope":"purse"}` on a quest's coin rewards.
- **Deferred**: the purse rule of §1.3 P3 (decisions §3) is the standard, and it caps coin realm-wide, which a per-program limit could not.

**SQ5. Credit the stall's keeper for a stall sale (Phase 2, a small fix).**
- `stall sale` has `actor` = the buyer (a resident) and `target` = the stall's wolf.
- Quest progress credits only `actor` (`story_quests.track`, line 543), so "sell three things at your stall" can never tick.
- **Proposal:** an objective field `"credit":"target"` for events whose wolf is the target: `stall sale`, `vouch` (target) and `told the wardens` (target).
- Today 10 uses `trade done` with the provisioner instead.

### 3.4 Not needed by the small pack
- C1 (text variables): every text is fixed. The thin-purse words are tree lines (§1.8), not a thanks variant.
- C3 and C4 (ranks, titles): deeds are enough at this tier.
- C7 (casting): the givers are fixed.
- C9 (figures), C10 `foe.outlaw` (foes), C11 (sparing), C13a/C13b (events), C14 (fates), C15 (mantles), L1 `hunt.stage`.

No small quest uses `mantle.assign`, `npc.kill`, a stranger or a foe. Every resident a quest moves (two pups) is released at the end of the copy, and again when the run ends.

---

## 4. Giver budget and checks before approval

### 4.1 Hub slots (the shared 9-choice room)
The registry is `givers.tsv` (decisions §1). Its rules, as this file applies them:
- **At most one pack quest per resident, across all packs** (decisions §1.2.1). Each giver here carries exactly one offer (and its hand-in when ready), or, for S10 and S11, the `npc.offer` choices of one quest. The board copy of a quest costs no hub slot, and a board-only quest (26, 29, S7) has no giver at all.
- **No giver or holder is campaign cast, a Keeper or fallback, an innkeeper or a look-alike** (decisions §1.2.2–1.2.4, §1.6). So the campaign's budget (`campaign.md` §10.3) is untouched.
- **Targets** hold no hub item. No resident is a target in more than two pack quests, and never in a premise that contradicts their other role.

| Program | Givers (one slot each) |
|---|---|
| `nh-pack.small.city` | `rm_tove_brinegate`, `rm_frederick_nailgate`, `rm_soren_ropewick`, `rm_rosamund_coalford`, `rm_kari_bilgehand`, `rmx_harriet_platt`, `rm_gwen_cinderdale`, `rm_berit_coalworth`, `rm_disa_raincot`, `rm_thyra_plankridge`, `rm_agnes_millford`, `rm_wynn_sootworth`, `rm_philippa_rustton`, `rm_corwin_bilgestack`, `rm_wystan_coalmarsh`, `rm_petra_plankhand`, `rm_egil_grantham`, `rm_corwin_sootstack`, `rm_yrsa_nailridge`, `rm_ansel_tarley`, `rm_erland_cinderridge`, `rm_lotte_greyley` |
| `nh-pack.small.north` | `cty_ingram_hazelshaw`, `tn_lf_lichen_hedgemead`, `tn_lf_sage_wheatridge`, `cty_oriel_blossomby`, `tn_lf_rusk_downfield`, `cty_brock_marshholm`, `cty_frost_colddrift`, `cty_ingram_ashleaf`, `cty_iris_fernhaven`, `cty_leif_driphand` (26 and 29 are board-only) |
| `nh-pack.small.south` | `cty_fallow_plumfield`, `cty_njal_rustwick`, `tn_cw_fern_mistshore`, `cty_gorse_hedgewell`, `tn_sm_hob_meadowfield`, `cty_emrys_stillshore`, `cty_willa_wheatcombe`, `cty_leif_rainley`, `cty_fallow_reedwell`, `cty_lichen_reedford`, `cty_yrsa_plankgate`, `cty_winifred_fogridge`, `cty_senna_applegarth`, `tn_lf_moss_downridge` |
| `nh-pack.stage` (small tier) | `rm_winifred_cinderworth`, `rm_olaf_pitchmore`, `rmx_frida_netherby`, `cty_odo_icewell`, `cty_alder_blossomcroft`, `cty_juniper_larkham`, `cty_stone_marshmore`, `tn_oh_quill_oakdell`, `cty_brand_sunham` (holder), `tn_sm_lichen_harvestton` (holder), `rm_runa_gullley` (S7 is board-only) |

**Who left this pack** (decisions §1.3), each keeping their other pack quest: Grete Dimmock (`lq-long-run`), Pell Harvestley (`mq-meresend-blight`), Jessamy Greyford (`lq-old-brown`), Tristram Rainridge (`mq-windthrown-cedar`; now a target in 26), Ebba Coalgate (`mq-low-drift-props`; now a target in 29), Stone Acorndell (`mq-lamp-oil-share`), Disa Fogford (`mq-bee-garth-bear`), Sloe Harvestby (`lq-blight-and-boar`), Ivo Sootridge (`lq-eel-famine`), Iris Pikeford (`mq-withy-lights`), Bram Harvestgarth (`mq-potters-place`) and Fern Merewick (`mq-lost-raker`). Rosamund Coalford (4), Corwin Bilgestack (14) and Frost Colddrift (27) keep their small quests and are conversation targets in the large lines. Kari Bilgehand (5) and Lichen Hedgemead (21) keep theirs and carry no stage quest.

**Residents named only as targets or recipients** (no slot), with their other roles:

| Resident | Here | Other pack role |
|---|---|---|
| `rm_theodora_wickley` | 1 (bread) | gives `lq-runnel-fever` |
| `rm_roald_wickstack` | 7 (letter) | gives `lq-vanishing-thief` |
| `tn_cw_grist_reedmere` | 21 (bread) | gives `mq-glasswater-nets` |
| `cty_tristram_rainridge` | 26 (rope) | gives `mq-windthrown-cedar` |
| `cty_ebba_coalgate` | 29 (wood) | gives `mq-low-drift-props`; target in the Frosthollow large line |
| `rm_arne_wetworth` | 48 (bolt) | none |
| `cty_isolde_gullwick` | S2 (summons) | none |
| `cty_briar_sedgebridge`, `cty_tor_chalkfield` | S5 (mead, honey) | none; Tor gives `mq-waste-road-salt` |
| `cty_ingram_ashleaf` | S7 (stake) | gives 28 |
| `cty_thorn_mistmere` | S11 (bread) | none |
| `tn_oh_cairn_oakhaven`, `tn_fh_thistle_stonepick` | 16 (crate), 43 (salt) | Keepers (fallback, Saltwell); a gift to the storekeeper agrees with the campaign (decisions §1.2.2) |

Ulric Stonevein is no longer a recipient: 29's lamp oil is gone.

The two pups are `rm_corra_ropewick` and `tn_sm_edric_harvestley`.

**Old-age risk.** Soren Ropewick (62), Gwen Cinderdale (63), Corwin Bilgestack (60), Leif Rainley (58) and the recipient Arne Wetworth (61) have a yearly death chance of a few per cent (`cast.md` §4).
- Each of those quests carries a `fail` rule `{on: "death", match: {actor: <resident>}}` (3, 7, 14, 39, 48). The death event names only `actor`, so this is right; the guide's `target` example is wrong.
- If the giver dies, copies under way fail by that rule. Whether the open offer leaves the window with them is unverified (`RatwGameStorylines.cpp:1101` checks `society().resident(npc)`), so the DM should withdraw the quest in a new version. With V1's `alive` check, approval refuses a dead giver.
- Nothing else depends on any small-quest giver.

### 4.2 Checks before approval (approval misses some of these; `story-format.md` §9; decisions §9 P2-24)
Every reviser lists and runs these; this file's list:
1. `story_program.review(p)` passes, with nothing in `uncovered`, for each pack. That means a rehearsal case per phase: `open` for all three, `festival` for the south pack, and the seven phases of `nh-pack.stage` (§2.10.1, reached through `story started` facts).
2. `story_program.problems(p) == []`, and `story_runner.references(conn, 'upper_accord', p)` passes on Ridgemere 3.
3. **The giver check** of decisions §1.2.7 on `givers.tsv`, and **the §1 query** (placed, alive, reachable home and work) for every resident this file names: givers, holders, targets and pups. Both passed on 2026-11-22 for the residents new in this revision.
4. **By hand** (approval does not check these):
   - every objective `match.target` is a placed, alive resident;
   - every `board` is one of the eight towns;
   - every purse is `town:<town>` for a town that holds money, and no `till:` and no `town:<id>:works|watch|church` purse appears before SQ2;
   - every `coin_<purse>_day` and `coin_<purse>_ok` is declared with the same name and type as `nh-purses`;
   - every `cell` in a match is a real cell id;
   - every item id is in `items.json`;
   - every species is in `animals.json`.
5. **Walkable tiles:** `npc.move` (3: (161,101) on `0_1`; 23: (60,120) on `8_6`), `item.place` (25: six tiles on `9_8`; S7: three tiles on `4_4`), and the regions `oak_square`, `frost_square`, `trail_n`, `trail_s`, `clamp`, `ford`, `lamp_1`–`lamp_3` and `moot_square`. Pick them from the tile map before approval.
6. **Every giver's `say`, `thanks` and `reply`, by eye:**
   - symbols exist in the giver's dialect;
   - no `[`;
   - at most 300 characters, and at most 120 for the wolf's words;
   - no other dialect's set phrases (a plain speaker says no "eh?" or "no?").

   No tool scans story programs for these (`talk.md` §4).
7. **Every new `say[...]` line in `resident.tree`:**
   - `tools/dialect_check.py --world ridgemere3` (the places in §1.8 added to `tools/dialect_allow.json` first);
   - `build-core/talk_check --data "$PWD/Data/Talk" --validate Data/Talk/dialogue/resident.tree`;
   - `--render-trees` (at most 160 characters, symbols filled);
   - `python3 tools/talk_lint.py` (0 new findings).

   The `talk_scorecard_quick` and `talk_golden` baselines don't move, because every line is gated on `world.mem.sq_*`, `st_last`, `nh_coin` or `nh_stage`, which the test town never has.
8. **No resident's words in a banner:** no `story.tell` in any pack carries `from` (decisions §9, P2-22). Grep the three pack files and the stage pack for `"from"` before approval.
9. **Words:** wolves and pups, never the forbidden word for them; the Bright One and the Dark One only; male or female only, with no pronoun guessed for the listening wolf; no ids in any spoken line; "private messages", never the other word.
10. **Watched events:** each pack's watch list must include every watched event it uses. The runner builds it from triggers and objectives.
    - city: `near resident`, `hour`, `trade done`, `day begins`;
    - north: `near resident`, `item picked up`, `hour`, `weather changed`, `day begins`;
    - south: `day begins`, `trade done`;
    - stage: `hour`, `near resident`, `item picked up`, `day begins`.
