# The Ninth Howl and the quest packs: the coordinator's decisions

This file settles every question that crosses files, so the per-file revisers of `campaign.md`, `quests-small.md`, `quests-medium.md`, `quests-large.md`, `cast-and-talk.md` and `engine-spec.md` agree. It answers `critic.md` item by item (P0-1 to P2-24). Where this file and another design file disagree, **this file wins**, and the other file is revised to match.

**Revised 2026-11-22 for the user's answer (a)** (binding): §6 is now **pacing by readiness** (participation plus a minimum time, or an admin's push; no 40-day calendar and no dates), replacing the launch calendar. §5.2, §7.3, §9 and §10 follow it. Answers (b) and (c) are applied too: (b) finished story quest lines count toward the Quickening's `closedStories`, capped (only a large line's last chapter, `closure: "line"`, or a campaign side story, `"side"`; at most 4 a program, the game refusing a fifth per wolf per program; J3, Phase 4), and the medium packs' four `closure` marks are dropped (doc 86 §11.1); (c) foe strengths are tuned by combat simulations, several passes if needed, at most 250 players (engine-spec C10t, Phase 7). Economy simulations are not covered and still need the user's go-ahead.

The giver registry is `givers.tsv`, next to this file. It is the single list of who carries what.

**Revised 2026-11-22 for doors locked to wolves** (doc 82, `World::playerDoorLocked`): 45 givers, choice holders and
targets who worked behind such doors were re-cast in the programs, and `givers.tsv` follows them; where a re-cast below
names someone else, `givers.tsv` and doc 86's "Givers behind locked doors" (the content section) win. Among them §1.3's
Edda Gullford (Q1), Gwen Cinderdale (#7), Brock Marshholm (#25), Fallow Reedwell (#40), Willa Wheatcombe (#38), and the
estate errands' givers (#46, #47).

**Checked against the world read only** (Ridgemere 3, `ratw_ridgemere3`, world id `upper_accord`), through `tools/world_db.connect('ridgemere3', 'owner')` inside `SET TRANSACTION READ ONLY` with a rollback. Nothing in the repo or the world was changed. The checkpoint read is the latest, saved **2026-11-22 22:34 UTC**, `calendarDays` **66.32**.

The queries:
```sql
-- placed, alive, reachable (used for every giver below)
with reach as (
  select a_area as area from world.links where world_id='upper_accord'
  union select b_area from world.links where world_id='upper_accord'
  union select id from world.cells where world_id='upper_accord')
select n.id, n.name, n.role, n.work_label, n.age, n.home_area, n.work_area, s.alive,
       n.home_area in (select area from reach) as home_ok,
       n.work_area in (select area from reach) as work_ok
from live.npcs n left join live.npc_state s on s.world_id=n.world_id and s.npc_id=n.id
where n.world_id='upper_accord';
-- dialect: live.npc_voice (home_region, accent) for every new giver
-- purses and calendar: game.checkpoints.payload, latest saved_at: society.accounts, calendarDays, people.projects
```
- 777 residents are placed: 774 are alive with a reachable home.
- "Town" and "quarter" come from the survey dump `/tmp/castsurvey/res2.tsv`, joined to the live flags above.
- Every new giver named here is placed and alive, with a reachable home and (where there is one) a reachable work place. None of them was named anywhere in the six design files before this file.

---

## 1. One giver registry (`givers.tsv`)

### 1.1 The columns
`resident_id`, `name`, `town`, `quarter`, `role_in_content`, `chapter_or_pack`.

- There is one row per resident and role. 244 residents have 247 rows. Three residents have two rows: a `campaign target` row plus one pack row (Aster Cornley, Linden Harvestton, Palle Plankmore).
- `role_in_content` is one of:
  - `campaign cast`: holds a campaign offer, chain, pledge, backing, story-heard node or scene part, is a post keeper, or is a named hero, villain or turnable;
  - `keeper`: a Keeper, a Keeper's fallback, or Imogen Nailridge (who takes Mirebridge pledges until Ch 6);
  - `innkeeper`: all 11 taproom and inn keepers in the world;
  - `campaign target`: named by the campaign only as a gift, list or conversation target, or as the source of an item. Such a resident holds no campaign hub item;
  - `look-alike`: the `like:` resident of a stranger (§1.6);
  - `small:<quest>`, `medium:<quest>`, `large:<quest>`, `stage:<quest>`.
- For campaign rows, `chapter_or_pack` lists the chapters of `campaign.md` that name the resident in full or by id. It is a finder, not the budget: the per-chapter offer count stays in campaign §10.3. Pack rows give the program id, and say "npc.offer in the same quest" for a secondary offer holder.
- Quest ids ending `-NEW` are the new quests of §7. Their reviser gives the real id and renames the row.

### 1.2 The rules (every file follows them)
1. **At most one pack quest per resident, across all packs.**
   - A "pack quest" is being a quest's `giver`, or holding any `npc.offer` choice of a pack quest.
   - Several `npc.offer` steps of the same quest, shown one after another, count as that one quest.
   - A quest's choices sit **on its own giver** wherever they can.
   - A resident who is only a `conversation`, `gift`, `trade done` or delivery target holds no hub item and is **not** a giver. Such targets are free, but no resident is a target in more than two pack quests, and never in a premise that contradicts their other role.
2. **Campaign cast and keepers carry no pack quests.**
   - They may be a pack's conversation or gift target only where the campaign's facts agree. Bettany Nailley and Solveig Tarwell in `lq-wreckers-light`, Jorunn Ropewick and Piet Lumley in `mq-sump-lamps`, and Ivy Stonehollow in `lq-vanishing-thief` are allowed.
   - A `campaign target` may carry **one** pack quest if its premise agrees with the campaign fact. Palle Plankmore's `mq-cinder-gate-tally` is the campaign's own Ch 3 paw-mark, so it stays.
3. **Campaign offers: at most 2 per resident per chapter.** Pledges, backings, chains and Q offers all count. Journal offers cost nothing. §1.4 has the fixes.
4. **Innkeepers give helpful knowledge and recap lines only.**
   - They give no quests, no `npc.offer` and no campaign offers, in any file.
   - They may be a delivery or conversation target in at most one quest across all files. Rowan Downridge (Rook's word) and Nell Plummeadow (Nell's letter scene) already use theirs.
   - This replaces campaign §7's "at most one pack quest per innkeeper". **No innkeeper is a Keeper or a Keeper's fallback** (§1.4).
5. **Names.** A new giver does not share a first name with campaign cast where the town has another free resident. Where it cannot be helped, no library line names either resident.
   - Oda Cinderdale is Oakhaven's only free resident. Iris Fernhaven and Marl Craghearth are Frosthollow's only two. Fallow Reedwell and Myrtle Larkcombe and Willa Wheatcombe are in towns where the same-named cast member does not live.
6. **Small towns.** Oakhaven has 4 non-cast residents, Frosthollow 5 and Saltwell 9. Where every free resident already has a pack quest, a quest is **board-only**:
   - it has no `giver`, only a `board` (which costs no hub slot);
   - a `quest.offer` goes to the wolf on `region entered` of the town square;
   - it ends on a gift to an existing resident target, closed by plain narration (no `from`, no quoted resident words);
   - from C5 on, the target's `npc.greet` thanks the wolf.

   The board-only quests are `sq-rope-for-the-drag`, `sq-lamp-oil-for-the-pit` (new premise, §7.1) and `st-pull-stake`.
7. **Validation.** Before approval, each reviser re-runs the query above for their file's residents, and the giver check:
   ```
   python3 - <<'EOF'  # every row with a pack role: one per resident, none on campaign cast/keeper/innkeeper/look-alike
   import csv,collections; r=collections.defaultdict(list)
   for x in csv.DictReader(open('artifacts/quests/design/givers.tsv'),delimiter='\t'): r[x['resident_id']].append(x['role_in_content'])
   for k,v in r.items():
       p=[y for y in v if y.split(':')[0] in ('small','medium','large','stage')]
       assert len(set(p))<=1 and not (p and any(y.startswith(('campaign cast','keeper','innkeeper','look-alike')) for y in v)), (k,v)
   EOF
   ```
   It passes on the file as written.

### 1.3 The re-casts
Dialect is `live.npc_voice` home region, with accent "none" meaning plain. Fenshore-raised speak mirebridge speech, and Ridgemere-raised speak plain.

**Small pack (`quests-small.md`)**

| # | Quest | Was | Now (id; sex, age; work; home cell; dialect) | Why |
|---|---|---|---|---|
| 7 | `sq-heights-round` | Grete Dimmock (giver of `lq-long-run`) | Gwen Cinderdale `rm_gwen_cinderdale`; f 63; clerking at the City Office; `1_1`; plain (light) | All three couriers are in `lq-long-run`. The letters are the City Office's, carried up to the Vances; the recipient is unchanged. |
| 23 | `sq-apple-nose` | Pell Harvestley (`mq-meresend-blight`) | Oriel Blossomby `cty_oriel_blossomby`; f 50; picks apples at Meresend; `7_6`; coldwater (light) | the same orchard |
| 25 | `sq-strayed-ewe` | Jessamy Greyford (`lq-old-brown`) | Brock Marshholm `cty_brock_marshholm`; m 36; shepherd at Larkdowns; `7_7`; **mirebridge speech** (broad) | another Coldwater shepherd |
| 26 | `sq-rope-for-the-drag` | Tristram Rainridge (`mq-windthrown-cedar`) | **board-only** (Oakhaven board); the rope is given to Tristram as a target | Oakhaven has no free resident left (§1.2.6) |
| 29 | `sq-lamp-oil-for-the-pit` | Ebba Coalgate (`mq-low-drift-props`) | **board-only** (Frosthollow board), with a **new premise** that is not lamp oil (§7.1) | Its premise duplicated `mq-lamp-oil-share`, and Frosthollow has no free resident left. |
| 30 | `sq-clamp-watch` | Stone Acorndell (`mq-lamp-oil-share`) | Iris Fernhaven `cty_iris_fernhaven`; f 26; collier at the Upper hearths; `9_3`; **plain** (larkfield, accent none) | a collier, at the clamp |
| 36 | `sq-smoke-for-the-hives` | Disa Fogford (`mq-bee-garth-bear`) | Hob Meadowfield `tn_sm_hob_meadowfield`; m 25; farmhand at Larkholm; `8_11`; southmeadow (light) | The hives become Larkholm's. |
| 38 | `sq-festival-garlands` | Sloe Harvestby (`lq-blight-and-boar`) | Willa Wheatcombe `cty_willa_wheatcombe`; f 39; farmhand at Larkholm; `8_13`; southmeadow (light) | |
| 39 | `sq-twine-for-the-nets` | Ivo Sootridge (`lq-eel-famine`) | Leif Rainley `cty_leif_rainley`; m 58; keeps rabbits at Dune; `3_14`; **plain** (fenshore, accent none) | Old-age risk at 58: add the `fail` on `death` rule (small §4.1). |
| 40 | `sq-fenside-pans` | Iris Pikeford (`mq-withy-lights`) | Fallow Reedwell `cty_fallow_reedwell`; f 44; works the salt at Fenside; `3_12`; **plain** (fenshore, accent none) | the same boiling house |
| 41 | `sq-mirebridge-clay` | Bram Harvestgarth (`mq-potters-place`) | Lichen Reedford `cty_lichen_reedford`; f 39; digs clay at the Mirebridge pits; `3_13`; plain (ridgemere, light) | a clay digger at those very pits |
| 43 | `sq-salt-for-the-road` | Fern Merewick (`mq-lost-raker`) | Winifred Fogridge `cty_winifred_fogridge`; f 45; warrener at Redsand; `12_11`; saltwell (light) | Saltwell's last free resident |
| 46 | new: Fellhall errand | — | Ansel Tarley `rm_ansel_tarley`; m 50; serving in the hall; `2_1`; plain | estate errands (§7.1) |
| 47 | new: Grayrock Hall errand | — | Erland Cinderridge `rm_erland_cinderridge`; m 21; serving in the hall; `0_2`; plain | §7.1 |
| 48 | new: Timberland Court errand | — | Lotte Greyley `rm_lotte_greyley`; f 21; helping at Greywool; `1_2`; plain | §7.1 |

Givers who stay and lose their second slot elsewhere:
- Rosamund Coalford (#4) and Corwin Bilgestack (#14) become **conversation targets** in `lq-salt-road` and `lq-wreckers-light`.
- Frost Colddrift (#27) becomes a target in `lq-mast-cedars`.
- Kari Bilgehand (#5) and Lichen Hedgemead (#21) lose their stage quests (§7.3).

**Medium pack (`quests-medium.md`)**

| Q | Quest | Was | Now | Why |
|---|---|---|---|---|
| Q1 | `mq-sump-lamps` | Ilse Coalley | Edda Gullford `rm_edda_gullford`; f 41; lighting the lamps at dusk for the Lamp Store; home Tar Corner, `0_1`; plain (broad) | Ilse is the villain of `lq-wreckers-light` (§2.2). Edda is one of the Lamp Store's four lamplighters (Ilse, Corwin Bilgestack, Rupert Cindergate and Edda). **Piet Lumley is campaign cast**, so the three `npc.offer` choices move from Piet to Edda: she is told the truth, or the wolf covers. Piet becomes a `conversation` target: his confession is his tree line `say[self.id=rmx_piet_lumley …]` gated on the quest (N2 later). |
| Q5 | `mq-net-court-cough` | Theodora Wickley | **withdrawn**. The slot becomes the Northgate medium: Sten Ropeford `rm_sten_ropeford`; m 33; helping at Rainford's Store; `1_0`; plain (light) | §2.1, §7.2 |
| Q7 | `mq-false-light` | Septimus Plankmarsh | **withdrawn** into `lq-wreckers-light`. The slot becomes the Timberland Court medium: Vidar Ashhand `rm_vidar_ashhand`; m 42; serving in the hall at Timberland Court; `1_2`; plain (broad) | §2.2, §7.2 |
| Q11 | `mq-short-weighed` | Pike Chalkley | **withdrawn** into `lq-clipped-silver` as its chapter 1. The slot becomes a new Larkfield medium: Heather Reedwater `tn_mb_heather_reedwater`; f 44; loom hand at Larkfield Looms; `6_9`; larkfield (light) | §2.3, §7.2 |
| Q14 | `mq-vineyard-cellar` | Sloe Harvestby | Harl Oakbough `tn_oh_harl_oakbough`; m 32; vine hand at Southmeadow, Press House; `8_13`; southmeadow (light) | Sloe keeps `lq-blight-and-boar`; Harl becomes only a target there. |
| Q18 | `mq-waste-road-salt` | Orm Sootgate | Tor Chalkfield `cty_tor_chalkfield`; m 24; carter at Southmeadow; `8_13`; southmeadow (light) | Orm keeps `lq-salt-road`. The wagon is now Southmeadow's own salt wagon coming home across the Waste (§2.4). |
| Q23 | new: Oakhaven medium | — | Oda Cinderdale `cty_oda_cinderdale`; f 29; logger at the Oakhaven cut; `4_4`; oakhaven (light) | §7.2 |

Givers who keep their medium and lose the duplicate:
- Disa Fogford Q13, Iris Pikeford Q15, Bram Harvestgarth Q16, Fern Merewick Q17 (Ridge Eelbank keeps his offers in Q17), Kite Bogditch Q19, Tristram Q20, Ebba Q21, Stone Acorndell Q22, Imogen Rustford Q12 and Pell Harvestley Q9.
- **Fenwick Mossdale is no longer in the medium pack**: his offer goes with Q11 into the large line (§2.3).

**Large pack (`quests-large.md`)**

| Line | Change |
|---|---|
| `lq-salt-road` | Orm giver; offers on Osric Ropeworth (two, in sequence) and Fenwick Mossdale. **Kite Bogditch, Fern Merewick and Rosamund Coalford become conversation targets** (each holds another pack quest). |
| `lq-mast-cedars` | Frost Colddrift becomes a conversation target. |
| `lq-old-brown` | Stone Acorndell becomes a conversation target. |
| `lq-clipped-silver` | Pike Chalkley holds the line, including the absorbed chapter-1 beam choice (§2.3). Imogen Rustford and Fenwick Mossdale are targets only. |
| `lq-blight-and-boar` | Harl Oakbough becomes a target (he gives `mq-vineyard-cellar`). Aster Cornley keeps one offer: she is a campaign target, which allows one. |
| `lq-runnel-fever` | Theodora Wickley gives only this line. Liv Slagstack (the Honey Crock's syrup, from the withdrawn Q5) may appear as a gift target. |
| `lq-wreckers-light` | Septimus keeps his offer. Ilse keeps her two. Corwin Bilgestack becomes a target. |
| `lq-long-run` | unchanged. Grete, Florence, Hanne, Orm Carlisle and Philippa: all couriers and carters stay here. |
| new: the Frosthollow line | Marl Craghearth `cty_marl_craghearth`; m 38; collier at the Upper hearths; `9_3`; **plain** (frosthollow, accent none). Every other role is a conversation or gift target (§7.2). |

**Campaign (`campaign.md`) and talk (`cast-and-talk.md`)**
- **Fenshore's fallback Keeper** becomes Jory Corncroft `cty_jory_corncroft` (m 41, carter at Fenshore, mirebridge speech, broad), replacing innkeeper Nell Plummeadow.
- **Ridgemere's fallback Keeper** becomes Corra Coldridge `rmx_corra_coldridge` (f 21, usher in the Antechamber, the herald's apprentice; plain), replacing Gerrit Mudhand, who now carries the Moot backing (§1.4).
- Add both to the protection list (campaign §8.6).
- cast-and-talk §3.25 is replaced by a pointer to `nh-pack.stage` (§7.3). Its givers who are cast or innkeepers lose those rows: Mab Carlisle, Bramble Reedford, Moss Blossomwell, Millicent Rustwell, Aster Coldfell, Harrow Reedford, Bramble Reedmere, Rusk Plummeadow and Piet Lumley.

### 1.4 The campaign's own per-chapter cap (critic P1-13)
- **Brenna Abbot:** the **Moot backing moves to Speaker Gerrit Mudhand** `rm_gerrit_mudhand`. As keeper of the Moot's roll, the Moot is his to back. That gives:
  - Brenna: Ch 1 Summons + pledge; Ch 2–8 pledge (+ the Ch 4 howl quest); Ch 9 pledge + the Moot rule;
  - Ridgemere's Ch 9 post quest is Cressida's waystation, not Brenna's;
  - never more than 2 a chapter;
  - Gerrit: Moot backing from Ch 2 to Ch 9, plus his story-heard nodes (no offers), at most 2.
- **Dagfinn:** the Question runs from **Ch 2 to Ch 8**, so Ch 1 is the round chain + Lamp backing, and Ch 9 is the backing only.
- §2.8's "every chapter" becomes "Ch 2–8".
- §2.6's patrons become "Dagfinn, Ottilie, Speaker Gerrit for the Moot, Steward Edric Kelpham for Greatly".
- **Every other Keeper** carries the pledge + the Ch 4 howl quest, and the pledge + the Ch 9 post quest. That is 2 each, so it is already inside the cap.

### 1.5 campaign §10.3 and §7
- campaign §10.3 gains one line: "The cross-pack giver table is `givers.tsv`. No campaign resident carries a pack quest."
- §7's "Givers" bullet is replaced by the rules in §1.2.

### 1.6 Look-alikes (critic P1-11)
A `like:` resident must not be cast, must not give any pack quest, and must live where the stranger never walks.

| Stranger | `like:` | Why |
|---|---|---|
| Warden-Envoy Ingeborg Rimewater | Brisk Plumby `tn_sm_brisk_plumby` (f 52, arctic, pot hand at the Vine and Sickle, Southmeadow) | An older arctic female. The Envoy's road (Pass House, Coldwater, the city, the Bay Light, Market Square) never reaches Southmeadow. |
| Hakon | Palle Bilgemarsh `rm_palle_bilgemarsh` (m 35, arctic, preserver on Market Square) | Hakon walks only the Greenholt ridges and the Pass and never comes to the city. He is not identical to Ulric. |
| The Grey Captain | Stone Sunton `cty_stone_sunton` (m 44, arctic, cowherd at Southgate) | kept as both files have it. Campaign line 2027 ("`like`-less") is corrected. |

`cty_ulla_rusthand` and `rm_cressida_carlisle` are no longer look-alikes anywhere.

---

## 2. Duplicate premises (critic P0-5)

### 2.1 The cough: medium Q5 and large `lq-runnel-fever`
- **`mq-net-court-cough` is withdrawn.** The Sump cough belongs to `lq-runnel-fever` alone, with Theodora Wickley as giver.
- The large reviser takes over two things from Q5:
  - the Ch 4 foreshadowing (with `nh_stage` ≥ 4, the close mentions the Residence's cough, campaign `nh_b_cough`);
  - optionally, Liv Slagstack's thyme syrup at the Honey Crock as a gift step.
- The medium Q5 slot becomes the Northgate medium (§7.2).
- **Small:**
  - `sq-mending-herbs` (#2) keeps its herbs but names no cough;
  - `sq-heights-thyme` (#17) changes its ailment and title. It is not a cough: for example, stiff joints and willow bark. That leaves one cough line in the city.

### 2.2 The false light: medium Q7 and large `lq-wreckers-light`; Ilse Coalley
- **`mq-false-light` is withdrawn into `lq-wreckers-light`.** Its usable parts go into the line's chapter 1, "A Lamp Where None Should Be": the `weather.set fog` on accept, the true bearing from the Bay Light's lamp room `r2_the_bay_light_up`, and the douse-or-wait choice on the rocks.
- Septimus Plankmarsh keeps one `npc.offer` in the line. The medium Q7 slot becomes the Timberland Court medium (§7.2).
- **Ilse Coalley's relation.** She is **only** the wronged lamplighter of `lq-wreckers-light`.
  - `mq-sump-lamps` is now Edda Gullford's (§1.3). In it the oil-drainer is Piet Lumley, as before.
  - No line of Q1 names or describes Ilse. Edda may speak of "the other lamplighters" in general.
  - So the two oil stories never meet: Piet takes Sump flasks for his debt (Q1, Ch 1–2), and Ilse takes Lamp Store oil for the headland lamp (the large line, Ch 5–6 flavour).
- `mq-sump-lamps` pays from the treasury (§3), not from the Lamp Store till `till:job:rm_ilse_coalley`, so no payment goes through the villain's name.

### 2.3 The Larkfield fraud: medium Q11 and large `lq-clipped-silver`
- **`mq-short-weighed` becomes chapter 1 of `lq-clipped-silver`.**
  - The false beam at the Larkfield market is the first thread a wolf pulls. It leads to the clipped silver and the coiner Vigdis Rainmore in debt.
  - The line keeps at most 3 chapters plus the reckoning: the beam replaces or merges into the current chapter 1.
  - Pike Chalkley is its giver. The beam's choice ("the Market Board should see it" or "I'll deal with the Counting House") is offered at Pike.
  - Fenwick Mossdale (warden of the Market Hall) is a gift or conversation target for the Market Board step.
- Campaign §7's "the Short-Weighed Sheaves" seed now points to this chapter, with stage 1–2 words in its talk (the factors' new moon).
- The medium Q11 slot becomes a new Larkfield medium (§7.2).

### 2.4 Also found: salt on the Waste (medium Q18 and large `lq-salt-road`)
- Both were Orm Sootgate's salt wagons with raiders on the Waste. **Orm keeps `lq-salt-road`.**
- `mq-waste-road-salt` becomes **Tor Chalkfield's** Southmeadow wagon bringing Saltwell salt home south across the dry wash.
- The toll-takers are the same grey-paw band as `lq-salt-road`'s raiders. The medium is offered only while the world text `lq_salt` is not that line's "broken" result. When it is, the giver's line says the wash is quiet now.
- This is a read-only cross-link. The medium writes nothing to `lq_salt`.

---

## 3. One purse rule for every program (critic P0-1, P0-8)

### 3.1 Balances (use these figures everywhere)
Checkpoint 2026-11-22 22:34 UTC, `society.accounts`:

| Purse | Account | Pennies | Day cap (≈10%) |
|---|---|---|---|
| `town:ridgemere` | `treasury` | 1,355 | 135 |
| `town:coldwater` | `stores:coldwater` | 212 | 21 |
| `town:larkfield` | `stores:larkfield` | 248 | 24 |
| `town:southmeadow` | `stores:southmeadow` | 236 | 23 |
| `town:fenshore` | `stores:fenshore` | 213 | 21 |
| `town:saltwell` | `stores:saltwell` | 224 | 22 |
| `town:oakhaven` | `stores:oakhaven` | 247 | 24 |
| `town:frosthollow` | `stores:frosthollow` | 247 | 24 |
| `till:job:rm_eleanor_saltcot` (Greatly Bank) | same | 161 | 16 |
| `till:job:rm_osric_ropeworth` (Vane & Daughters) | same | 125 | 12 |
| `till:job:rm_brenna_slaggate` (Grayrock Shipwrights) | same | 156 | 15 |
| `till:job:rm_vigdis_rainmore` (Silver & Rain) | same | 112 | 11 |
| `till:job:rm_rupert_fogton` (Hollis Physic) | same | 90 | 9 |
| `till:job:rm_corra_coalcot` (the Fish Hall) | same | 141 | 14 |
| `till:job:rm_ilse_coalley` (the Lamp Store) | same | 126 | not used (§2.2) |
| `till:job:rm_edric_millton` (the cooper's) | same | 197 | 19 |

- Correct "coldwater 200" to 212 and "treasury 1,340" to 1,355 everywhere: large §1.5, campaign §12, small §2.3.
- Mirebridge has no purse.
- `town:<id>:works|watch|church` accounts do not exist. No program names them.

### 3.2 The rule
1. **A per-purse, per-day world counter.** Each purse has a world number `coin_<purse>_day`: `coin_ridgemere_day`, `coin_coldwater_day`, …, `coin_till_eleanor_saltcot_day`. It is declared once with `world-var declare`, and every paying program declares it with the same name and type.
   - A grant happens only if `coin_<purse>_day + amount ≤ cap` (the table above) **and** the floor switch `coin_<purse>_ok` is on.
   - Then the program does `character.grant` with `ifRefused: "skip"` and `add coin_<purse>_day amount`.
   - Otherwise the wolf is paid in kind: `bread` ×2, or the quest's own item. The giver's thanks has a "thin purse" variant, and the deed, practice and campaign feed land as usual.
2. **A floor of 60.** A purse below 60 pays no story coin until it is back at 80 or more.
3. **One owner for the counters:** a tiny always-on program `nh-purses` (file `Data/Storylines/ridgemere3/ninth-howl/nh-purses.json`, one phase, written by the **campaign reviser** in a new campaign §2.11). It holds:
   - `day begins` → set every `coin_*_day` to 0;
   - `purse crossed` (`target` the purse, `item: "down"`, `detail: "60"`) → `coin_<purse>_ok` off;
   - `purse crossed` (`item: "up"`, `detail: "80"`) → on. If `purse crossed` has no "up" direction, a daily `schedule` turns the switch back on when the last grant of the day went through.
   - Every other program only reads and adds.
   - The small pack's P3 day caps and the medium pack's `coins_<town>` guard are **replaced** by this rule.
4. **Amounts per wolf:**
   - small: 2–4;
   - medium: 6–15;
   - large: at most 12 a chapter and 35 a line;
   - stage pack: as its tier;
   - campaign: at most 10 from a town store and at most 20 from the treasury, per deed;
   - Purse-path errands: at most 10.
   - Restitution from a villain's till: at most 10 a wolf and 60 in all, per the large pack's story counter, inside that till's day cap.
5. **Tills.** `till:job:<id>` purses are written **only after SQ2 ships** (engine Phase 1, §5).
   - Until then, every till payment names the treasury (city) or the town store, with the same counter rule.
   - The text of the reason stays as the shop paying ("Hollis Physic pays it back").
   - When SQ2 lands, a new version of the program swaps the purse.
   - This covers the campaign's Purse path (campaign §2.7: "Greatly Bank till after SQ2; the treasury until then") and large §1.5. Large fact 5 ("Tills are real purses") is reworded to "tills hold money, but a story can't name one until SQ2".

---

## 4. Camp objectives and the watchposts (critic P0-3)
1. **Matching.** A story's own wave is matched by **`cell` plus a story switch `spawned_<region>`**:
   - the switch is set when the chapter calls `story.area.spawn` there, and cleared when it scores;
   - never by `camp_w_ridgemere_<cell>`. Story spawns are `camp_story_<n>` (`Core/RatwRoads.cpp:1622`).
   - This applies to campaign Ch 1 (Alder Mere `5_4`), Ch 3 (the ridges, Glasswater `6_8`), Ch 5 (the bounders), Ch 6 (`6_12`, `7_12`, the cudgel bands at `2_3` / `3_3`), Ch 7 and Ch 9, and to every pack fight.
2. **No camp objective.** `camp cleared` is not a mission event. Every "break the camp" is a **trigger** on `camp cleared` (match `cell`, switch on) that does `objective.complete` for each participant `wolf in region`, or adds to the chapter's shared bar. `bandit falls` stays a plain objective.
3. **From L2 / L3 (engine Phase 2):**
   - spawns name their camp (`camp: "nh_alder_mere"` → `camp_story_nh_alder_mere`, rule in §5.2);
   - `camp cleared` becomes a mission event;
   - programs move to exact `target` matches in a new version.
4. **Credit table (campaign §5.3)** matches by `cell`, so a natural camp and a story camp in the same cell both credit the town. That is right: either way the road is clear.
   - The `6_3` row moves from Coldwater to **Frosthollow**: the lime quarry is Frosthollow's work. This aligns with `sq-scree-goats` (critic P2-21).
   - Ibex can be hunted there: `6_3` has 1,304 rock and 3,984 snow tiles, and the ibex habitats are rock 3, snow 1.
5. **Watchpost wording.** proj-1 (`8_6`, Frosthollow), proj-2 (`5_4`, Oakhaven) and proj-3 (`1_1`, the waystation) are **already posted** (state open, worked 0, needs stone 4 and timber 10 or 8).
   - Campaign §5.2's "a camp must be cleared before its post can be posted" becomes: "the chapters do not post posts 2, 10 and 12, which stand open in the world. A Line post counts as raised when its project is built and no story camp stands in its cell (its `spawned_` switch is off). Wolves raise it by clearing the camp and gifting the stone and timber."
   - The other Line projects (posts 7, 8, 9) are posted by their chapters as written.

---

## 5. Engine names and the items engine-spec must add (critic P0-2, P2-18, P2-23)

### 5.1 One name for each thing (every file uses these)
| Say | Not |
|---|---|
| C10 `foe.outlaw` (a resident downed, never killed) | "C10 `npc` mode" (campaign §3.1, §11.1) |
| C12a `story.notify` / event `story notified` | "C12" when the meaning is packs → chapter |
| C12b `match` / `field` with `starts` / `in` | "C12" when the meaning is deed matching |
| C13a events `variable changed`, `month begins`, `succession`; C13b `resident has post` | "C13" |
| L1 `hunt.stage` | M1 `wild.spawn` / `wild.leave` (**dropped**) |
| L2 `camp` on `story.area.spawn` / `bandits.call` | M4 (merged into L2) |
| SQ2 till purses | M6 (the same fix) |
| N2 window facts, now including `player.story.<var>` | T2 (merged into N2) |
| V2 story words check, now including the expander and the giver's dialect | T4 (merged into V2) |
| N1 choices overflow into "Ask about…" | T8 (the same item) |
| C8 `talk.remember` with `scope: pair` | T1 (merged into C8) |

### 5.2 The items engine-spec adds (catalogue entry, or a "deferred" line with its fallback)

| Id | What | Phase | Fallback until then |
|---|---|---|---|
| **SQ2** | `check_purse` accepts `till:job:<id>` (three parts, the second `job`). `Game::storyPurseAccount` maps `till:<id>` to `till:job:<id>`. `references()` refuses a purse whose account is not in the checkpoint's `society.accounts`, and refuses `town:<id>:works/watch/church`. | **1** (own entry, beside V1) | treasury or town store (§3.2.5) |
| V1 + | approval #4 also checks `live.npc_state.alive` (3 placed residents are dead); #2 is rewritten as SQ2 | 1 | by hand (§1.2.7) |
| **C8 pair scope** (T1) | `talk.remember` / `talk.forget` `{scope:"pair", npc, player:"$actor", key, value, days}` → `pair.mem.<k>` | 1 | C5 `npc.greet` or `deed.award`. The 17 `pair.mem` gates in cast-and-talk stay inert. |
| **T7** | `belief.plant` claim kind `story.cleared`, beside `story.rumour`, in `Data/Talk/claims.json` | 1 | the cleared resident's own line; J8 |
| **N2 +** (T2) | window fact `player.story.<var>` for player variables marked `"talk": true` (`p_howl`, `p_backs`, `p_advice`) | 1 | the reply stays generic |
| **SQ1** | objective `where: {cells:[…]} \| {region}` | 2 | a trigger + `objective.progress` |
| **SQ5** | objective `credit: "target"` (`stall sale`, `vouch`, `told the wardens`) | 2 | `trade done` with the stall keeper |
| **M2** | operand `{"purse": "<id>"}` (with C1) | 2 | fixed amounts and the §3 rule |
| **M5** | `cell` on `trade done` | 2 | a `wolf in region` check |
| **L2** | `camp: <name>` on `story.area.spawn` and `bandits.call`: `[a-z0-9_]{3,30}`, beginning with the program's own prefix (`nh_`, `sq_`, `mq_`, `lq_`, `st_`); the camp id is `camp_story_<name>`. While that camp stands, a second spawn with the same name adds to it; once it is cleared, the name may be reused. The validator refuses a name without the program's prefix. | 2 | `cell` + `spawned_` switch (§4) |
| **L3** | `camp cleared`, `caravan arrives`, `ship lost`, `ship docks`, `project built`, `festival` become mission events | 2 | trigger + `objective.complete` |
| **N4** (new) | quest `join.again: <days>`: game days before the same wolf may start that quest again, counted from any end (done, failed or abandoned). `repeat` counts only from "done" (`story_quests.again`). | 2 | §8 today-rules |
| **T5** | `npc.line` goes through `inVoice` | 3 | the dialect written out |
| **L5** | `quest.withdraw {quest, player}` | 4 (optional; C6 `requires` makes it moot) | `quest.fail` "another road taken" |
| **L1** | `hunt.stage {region, species, count 1–3, minutes}`: the next hunt in the region has that animal | **5** (renamed "Howls, crowds and the wild") | the animal objective is optional |
| **T3** | a scene cast entry `{id, visitor:"<visitor id>"}` with the `like` face, in the dialogue window. **`story.tell from_visitor` is dropped** (§9, P2-22). | 7 | Desmond and Gorm speak, and the strangers appear in narration |
| **L4** | `npc.sentence {npc, place, days 1–28}` (outlives the run) | 8 | the hold is re-applied at each visit |
| **SQ3** | lend a paw to a resident at work (`together` with a resident) | 9, deferred | `gift` / `trade done` / `conversation` objectives |
| **SQ4** | `rewardLimit` | 9, deferred: the §3 counter rule is the standard | §3 |
| **M3** | `house:<faction>` purses | 9, deferred | the treasury, or a House business's till after SQ2 |
| **T6** | a `story` talk topic for the director | 9, deferred | ride `news` / `gossip` / `day` |
| T9 | allow-list words (`tools/dialect_allow.json`) | a data edit, no phase | — |

- **Phase sizes grow** by about 4 h (1), 9 h (2), 1 h (3), 1 h (4), 4 h (5), 3 h (7), 3 h (8) and 10 h (9). The total is about 191 agent-hours. Engine-spec §1, §7 and §9 carry the new rows.
- **Engine-spec §7's pack mapping is corrected** (P2-18):
  - small packs need Phase 1 only (V1, SQ2, J1, C8), plus C2 for the garland rope (#38);
  - medium packs need Phase 1, plus Phase 4 for C12a / C6 / N1;
  - large packs need Phase 1, plus J3 (Phase 4), L1 (Phase 5) and C10 (Phase 7);
  - the stage pack needs nothing beyond what its tiers need.
- **Readiness (user (a), §6).** Engine-spec's R1 (participation), R2 (`ready` blocks, the command `advance`) and R3 (the console's bars and Advance), with C2's `activeDays`, are Phase 1a. This file rules on two of their details in §6.2: **R1's floor never asks for more wolves than are active**, and **R2 gains `slowNeedsYou`** (the soft maximum's "ready to push" alert as needs-you). It also settles D10 and D12 for this campaign (§6.2).
- **D1 is answered (b), 2026-11-22: yes, capped** (J3 closure). The large lines carry `closure: "line"` (`true` reads as `"line"`) on their last chapter's road quests (§9, P1-15); campaign side stories carry `"side"`, at most 4 a chapter; the medium packs carry none.

---

## 6. Pacing by readiness (the user's answer (a), 2026-11-22; replaces the launch calendar)

Binding for every file. It replaces this section's old launch calendar (critic P0-7: the dated plan, the anchors from the publication day P, the late-launch rules and the Act II and Act III deadlines) in full. The user's words: *"don't limit it to only 40 days. I think progression should be made by participation (percentage of active users accomplishing specific tasks, for example), plus time, and progression would be pushed forward by an admin in DM/Story Maker."* Campaign §3 has the detail; the engine is engine-spec's R1, R2, R3 and C2 (Phase 1a).

### 6.1 The rule
**Ready = (participation met AND the minimum time served) OR an admin push.**
1. **Participation** (R1 `participation`): a stated share of the **active** wolves are in the chapter's list `ch<n>_did`, which holds every wolf who did at least one of the chapter's **named tasks** (campaign §3.2.3), once per chapter.
   - **Active** = seen (signed in, or the actor of any event) within the chapter's `activeDays`: **18 game days, 3 real days** (C2). The audience stays `{"kind":"everyone","days":30,"cap":5000}`.
   - **A small floor**, 2 or 3, that never asks for more wolves than are active (§6.2), so a world of 1 to 5 wolves can always advance; and `cap: 150`.
2. **The minimum time** (R2 `minDays`, `since: "chapter"`, game days; held time does not count).
3. **An admin push**, always possible, even before the minimum time: the console's **Advance…**, or **Advance anyway…** with the typed title when not ready (R2's command `advance`, R3). A reason is required; the command row and `dm.audit` keep who, when, why and the readiness snapshot.
4. **Every chapter's `ready` block has `auto: true`**: a ready chapter moves on by itself. This replaces engine-spec §8 D10's suggested `auto: false` for this campaign: the admin's push is the accelerator and the safety, not the only way on.
5. **The soft maximum** (R2 `slowAfterDays`): a chapter not ready by then does **not** move by itself. It raises the alert **"ready to push"** (R2's `slow`, as a needs-you alert, §6.2) and keeps counting. **Unless the chapter is set to auto**: the story switch `ch<n>_auto` (off by default; the admin may flip it at any time) makes the soft maximum count as participation met, so the chapter moves on there by itself.
6. **The bar is not a gate.** A chapter's shared goal decides its good or worse road at the close, not the timing. The switch `ch<n>_wait_bar` (off by default) makes readiness also wait for the bar.
7. **Before Phase 1a** (if Act I is published first): a `dm signal` direction exit per working phase, the `ch<n>_did` count on the Variables tab, and the DM's eye against the same numbers; no latest-end `clock` exit. The run moves onto the `ready` version by `migrate` when Phase 1a lands (campaign §3.2.5; engine-spec §7.1).

**The numbers** (campaign §3.2.3; shares of the active wolves, floors, and game days):

| Ch | Tasks, in short | % | floor | min | soft max | the big beat |
|---|---|---|---|---|---|---|
| 1 | a Summons carried; fought at Alder Mere; the King found; a post gift | 30 | 3 | 10 | 30 | the chalk night (game day 3, or a new moon on days 2–5); the close is the Herald's Word |
| 2 | a debtor freed; a side story or the door quest | 25 | 2 | 12 | 36 | the Reckoning: a new moon within 3 days, else dusk the next day |
| 3 | a lamp doused; a Glasswater wave; a list piece; a proj-1 gift | 30 | 3 | 12 | 36 | the Road Feast eve |
| 4 | a chorus; a part of a howl; a Mirebridge wolf found; the silver; Bramble's Restday | 30 | 3 | 12 | 42 | the Howl Relay, called on readiness |
| 5 | a bound; a stake; a letter; a bounder camp; a side story | 30 | 3 | 18 | 48 | Black Cloaks on the Pass Road |
| 6 | a name; a false light; the path; a southern band; truce tokens; a post gift | 30 | 3 | 18 | 48 | the Rite at the Book's 30 names |
| 7 | mustered for the ford | 25 | 2 | 6 | 30 | the siege, called on readiness; the close 6 days after it |
| 8 | a testimony; a search; a bearing; the haul; a lesson; the List; the Count | 30 | 3 | 24 | 60 | the haul; the close never cuts it |
| 9 | a post kept; lanterns; the bell's road; a silent voice raised; the Envoy answered | 30 | 2 | 12 | 36 | the Moot night, dusk the next game day after the call |

Typical: about **24 real days** at 200 wolves and **30** at 5. These are not limits.

### 6.2 Two rulings on the engine, and two questions settled (engine-spec follows)
1. **R1 rule 4, the floor.** need = max(min(floor, |D|), ⌈percent × |D| / 100⌉), then min(need, cap) when a cap is given, and never below 1; with |D| = 0 it is false. So a world with fewer active wolves than the floor needs all of them. This replaces "when |D| < floor, it is false" and its message "only 3 active wolves; 5 needed". (The user: a small floor "so 1–5 wolves can still advance".)
2. **R2 rule 7, `slowNeedsYou`.** A new optional block field, default false. When true, the `slow` alert is "needs you" (it counts on the Campaigns tab and goes to the Dev Console), and R3's `alertText` words it **"Ready to push: <the furthest part>. Advance anyway, or wait."** Every chapter of the Ninth Howl sets it.
3. **D10 (the defaults)** is settled for this campaign by §6.1's table: 25–30%, floors 2–3, `cap: 150`, `activeDays: 18`, `auto: true`, `slowAfterDays` = the soft maximum, `slowNeedsYou: true`.
4. **D12 (Midwinter)** is settled: no wait for the next Midwinter and no `then: dm signal`. Ch 9's run-up is a working phase with a `ready` block (Advance is the admin's way to choose the night); the call announces the **Moot night at dusk the next game day**. Its name follows the season that night: "the Midwinter Moot" in winter, otherwise "the Moot of the Nine Howls" (the world text `nh_moot` and the realm talk key of the same name, `midwinter` or `nine_howls`, both set at the call). No text says "the longest night" of the Moot.

### 6.3 Calendar moments are flavour and optional timing
- A set piece waits for its calendar moment (a new moon, a full moon, a town festival) **only when it falls within 3 game days** of the closing phase's entry; otherwise it plays at the next dusk at least 3 game hours away (the finale: dusk the next game day).
- **Ch 4 opens at a feast:** the season's town festival if within 3 game days, else "the Moot Feast" that Ch 4 calls in every town for the next day.
- **Ch 6** forces fog in any season (as before). **Ch 7**'s siege is named "the Last Sheaf" only when it falls on Larkfield's festival. **Ch 8** forces an early freeze out of winter (`weather.set snow` on Coldwater and the ice regions) and, real or early, turns on the world switch **`nh_mere_frozen`**.
- **The words.** The lamp-book line and the writ hold the lamp "until the realm howls a Moot" (the writ's "on the longest night of the year" is gone). The Envoy asks for one howl "on the night your Moot sits". The Council's threat to name a ruler has no date. Act III is "The Moot Night". "Midwinter" names the Moot only under `nh_moot=midwinter`.
- **The grace key is optional.** If a new moon passes before Ch 1's chalk night (or before Act I is published), the DM **may** seed `nh_debts=grace` by hand; the debt gossip then plays its grace line, and the chalk notice begins "THE GRACE IS OVER." Nothing waits on it.

### 6.4 What each file does about it
- **Every pack and the stage pack** gate on `nh_stage` and on branch values only, never on a game day. The stage pack's `s9` quests run from Ch 9's opening until the call of the Moot night (withdrawn when the world text `nh_moot` is set, a game day before the night). Freeze gates read `nh_mere_frozen` (medium's `st_ice_frozen` becomes a copy of it, or is dropped). Words that promise "Midwinter" become "the Moot" (or are gated on `nh_moot=midwinter`).
- **engine-spec:** R1 rule 4 and R2's `slowNeedsYou` (§6.2); §7.1's "target publication day 80" and Ch 1's chalk night follow campaign §3.2.4 (chapter game day 3, or a new moon on days 2–5); §7.4's "the reference pacing … is now the fastest the campaign can go" becomes campaign §3.2.3's typical table; D10 and D12 as settled in §6.2; the stand-in as campaign §3.2.5.
- **cast-and-talk:** the grace lines stay, as optional; every line that dates the Moot ("A Moot at Midwinter", "before Midwinter", "on the longest night", "Next Midwinter") says "the Moot" or "the Moot night", with a winter variant gated on `nh_moot=midwinter` where the line is better for it; Tobias's "after day 185" becomes "12 game days into Ch 5"; Tansy's last chain step asks about "the Moot night".

### 6.5 The clock (for estimates only)
- Game day 66.32 at 2026-11-22 22:34 UTC. 6 game days pass per real day: a game day is 4 real hours.
- Real time of game day *d* is 2026-11-22 22:34 UTC + (*d* − 66.32) / 6 real days.
- No rule reads a game day: each chapter counts from its own opening, and each set piece from its own closing phase.

---

## 7. Coverage holes and the stage pack (critic P0-6, P1-16)

### 7.1 New small quests (`quests-small.md`, program `nh-pack.small.city` unless said)
- **#46 Fellhall** (Ansel Tarley), **#47 Grayrock Hall** (Erland Cinderridge) and **#48 Timberland Court** (Lotte Greyley).
  - Below-stairs errands inside each estate: kitchens, chambers, stables, gardens, carried to and from the city.
  - No House head, steward or campaign cast as giver. Stewards and family may only be named in talk as "the steward" or "the family".
  - Each reads `nh_b_quarrel` / `nh_stage` only to colour a line.
- **#29, a new Frosthollow board-only premise** (program `nh-pack.small.north`): not lamp oil, not props, not the clamp, and not Deepvein. For example, firewood or meal up to the Pass House for the snowed-in carters, handed to a Frosthollow target who holds no more than one other target role.
- **#26** becomes board-only (Oakhaven), as in §1.3.

### 7.2 New medium quests and the new large line
Each brief gives the giver and the walls the premise must not cross. Its reviser writes the full entry in that file's format.

| File | Slot | Giver | Town, quarter | Must not overlap |
|---|---|---|---|---|
| medium | Q5 (city) | Sten Ropeford `rm_sten_ropeford` | Ridgemere, **Northgate** `1_0` (Rainford's Store) | coughs, lamps, fraud, the Watch lists, Ambrose's notes. Suggested: "Smoke over Northgate": cheap wet coal sold door to door sets chimneys alight; trace the seller to a yard; report him to the Watch, or make him take it back and sell dry. |
| medium | Q7 (city) | Vidar Ashhand `rm_vidar_ashhand` | Ridgemere, **Timberland Court** `1_2` | the Houses' quarrel, the Gudrun/Osric match, Timberland's iron trade. Suggested: "The Court's Winter Larder": the servants' hall larder runs short; the cook blames rats; it is a hungry Sump family coming through the old coal chute; report them or set out a weekly basket. |
| medium | Q11 (greenholt) | Heather Reedwater `tn_mb_heather_reedwater` | Larkfield, the Looms `6_9` | fraud (now in `lq-clipped-silver`), the ford siege, the Print House's Council work. Suggested: "A Bolt Gone Missing": a bolt of Wharf-bound linen vanishes between the Looms and the carters; follow the scent; return it, or let the carter who sold it for bread pay it back. |
| medium | Q23 (north) | Oda Cinderdale `cty_oda_cinderdale` | **Oakhaven**, the cut `4_4` | windthrown cedar (Q20), the mast cedars (large), the wronged logger and the common right (campaign Ch 1, Ch 5). Suggested: "Logs Adrift on the Sound": a raft breaks loose on Cedar Sound in fog; recover the logs along the shore; Kell Reedwade `cty_kell_reedwade` (skipper at Soundside, `2_5`, unused) is the target; return all to the cut, or sell the strays at Soundside's price. |
| large | new line (program id by the reviser, e.g. `lq-black-hearths`) | Marl Craghearth `cty_marl_craghearth` | **Frosthollow**, the Upper hearths `9_3`, the Greenholt hearths (Larkfield), the city's Timberland Charcoal Store | Deepvein and Fenwick's confession (Ch 8), Silver Under the Ice (Ch 4), the Frosthollow Count (Ch 8), lamp oil (Q22), props (Q21), rustlers and the bear (`lq-old-brown`), and the clamp-watch objective (#30). Suggested: "The Black Hearths": the Upper hearths' charcoal is undersold in the city while their clamps are damped by night. The roads: Frosthollow sells its own at the Lamp and Pick; a fair contract with the city store; or the Greenholt hearths buy them out. Targets only: Ebba, Stone, Leif, Iris and the charcoal store's Arne Ashton `rm_arne_ashton`, who is unused. `closure: true` on the last chapter. |

### 7.3 The stage pack `nh-pack.stage`
**The program** is written once, by the **small reviser**, as a new section of `quests-small.md`:
- one open-audience program (cap 5000, ≤ 32 quests), file `Data/Storylines/ridgemere3/quests-stage/nh-pack.stage.json`;
- phases `quiet`, `s12`, `s3`, `s4`, `s56`, `s78` and `s9`, entered by an `hour` trigger that reads `nh_stage` (unset or 0 → `quiet`);
- each quest lists its `phases`;
- the purse rule of §3, and feeds as for any pack (small 2 / medium 4 / large 8, times the distance weight);
- `compare nh_b_*` read where a quest needs a branch: for example, `st-feed-larkfield` only if `nh_b_ford = burned`.

**The entries** are written in full (objectives, rewards, purse, gates, phases, dialogue in the giver's dialect, talk) by the reviser of each tier, in their own file:
- small: 12 entries, `quests-small.md` §2.10;
- medium: 5 entries, `quests-medium.md` §4b;
- large: 5 entries, `quests-large.md` §12b. These are single large quests of 3–5 objectives, `repeat: weekly`, with no reckoning.

| Quest | Tier | Phase | Giver (dialect) | Premise |
|---|---|---|---|---|
| `st-wake-bread` | small | s12 | Winifred Cinderworth `rm_winifred_cinderworth`, baker on Market Square (plain) | bread for the King's wake at the Soup Kitchen |
| `st-summons` | small | s12 | Olaf Pitchmore `rm_olaf_pitchmore`, City Office clerk (plain: coldwater, accent none) | carry a Moot Summons to a Rainward farmstead |
| `st-post-stone` | small | s3 | Frida Netherby `rmx_frida_netherby`, runs the Masons' Yard (plain) | stone for a Line post (proj-1/2/3 need 4) |
| `st-post-timber` | small | s3 | Odo Icewell `cty_odo_icewell`, woodward of the Rainwood coppice (plain) | timber for a Line post (they need 8–10) |
| `st-chorus-wrong` | small | s4 | Alder Blossomcroft `cty_alder_blossomcroft`, Larkholm (southmeadow) | the comic burying-howl-at-a-wedding (`mead`, `honey`, a `table game`) |
| `st-bounds-water` | small | s56 | Juniper Larkham `cty_juniper_larkham`, Rainward orchard `2_3` (frosthollow) | water and bread to the bounds-walkers in the Rainward Hills |
| `st-pull-stake` | small | s56 | **board-only** (Oakhaven) | pull a green stake from the cut; give it to Ingram Ashleaf (target) |
| `st-fog-lanterns` | small | s56 | Stone Marshmore `cty_stone_marshmore`, deckhand at Fen Shore (plain: fenshore, none) | lamp oil for the Fenshore boardwalk lamps in the fog (benign lamps: not the false lights) |
| `st-ford-blankets` | small | s78 | Quill Oakdell `tn_oh_quill_oakdell`, Larkfield Looms (larkfield) | blankets to the wolves at the ford |
| `st-feed-larkfield` | small | s78, if `nh_b_ford = burned` | Brand Sunham `cty_brand_sunham`, Longmead (larkfield) | Longmead grain and bread to the burned-out houses |
| `st-haulers-food` | small | s78 | Lichen Harvestton `tn_sm_lichen_harvestton`, Coldwater Bakehouse (coldwater) | bread and `smoked_fish` to the bell haulers at the Upper Mere boat shed |
| `st-moot-benches` | small | s9 | Runa Gullley `rm_runa_gullley`, carpenter in the Market Hall (mirebridge speech: fenshore, light) | planks and benches for the Moot on Market Square |
| `st-greenholt-wagon` | medium | s3 | Aud Saltridge `rmx_aud_saltridge`, the Carters' Yard (plain) | walk a wagon to Coldwater past the lit ridges (`caravan arrives` via trigger) |
| `st-black-cloak-road` | medium | s56 | Wick Marshwade `cty_wick_marshwade`, carter at Coldwater (coldwater) | scout and clear the Pass Road stage before the black cloak walks it. The Envoy (a campaign visitor) is never referenced by id. |
| `st-relief-convoy` | medium | s78 | Myrtle Larkcombe `cty_myrtle_larkcombe`, Larkholm (southmeadow) | a relief wagon from Larkholm to Larkfield in the siege nights |
| `st-ice-watch` | medium | s78 | Garth Barleyton `cty_garth_barleyton`, shepherd (coldwater) | sit a night on the Upper Mere ice and listen for the bell |
| `st-bell-road` | medium | s9 | Jory Rimepeak `tn_fh_jory_rimepeak`, Coldwater Looms (coldwater) | escort one stage of the bell's road to the city |
| `st-patrol-round` | large | s12, s3 | Bertil Saltcot `rmx_bertil_saltcot`, Watch guard (plain) | walk the Sump round the patrolman left when he took the lamp: night waves, help on the way |
| `st-camp-bounty` | large | s3 to s9 | Frederick Rainham `rm_frederick_rainham`, Watch guard (plain) | clear a re-formed camp, any of the nine (§4 matching; −1 `nh_threat`) |
| `st-midsummer-road` | large | s4 | Cedric Mudley `rm_cedric_mudley`, holding the north gate (plain) | keep the road safe for fair-goers in the feast days that open Ch 4 (the season's festival or the Moot Feast, §6.3); the title loses "Midsummer" unless the feast is Midsummer |
| `st-twelve-bounds` | large | s56 | Ada Coalstack `cty_ada_coalstack`, bees at Heatherside `3_1` (southmeadow) | walk all twelve Rainward bounds in a day. The pack defines its own copies of the regions. |
| `st-post-night` | large | s9 (from Ch 9's opening to the call of the Moot night, §6.4) | Beatha Foggate `rm_beatha_foggate`, Watch guard (plain) | stand one Line post a whole night before the Moot night, against probing raids |

**Dropped from the old seed lists**, because each duplicates existing content:

| Seed | Already covered by |
|---|---|
| ring the dawn bell | small #14 |
| the lamplighter's round with Piet | medium Q1 |
| the Short-Weighed Sheaves | `lq-clipped-silver` ch 1 |
| douse a lamp | campaign Ch 3 |
| the Cinder Gate's paw-mark | medium Q2 |
| fish for the post-builders | replaced by `st-post-timber` |
| a late howl | the Keepers' Ch 4 howl quest stays on offer in Ch 5–9 for a town whose howl is still lost: **campaign** |
| Silver Under the Ice weekly | the campaign Ch 4 side story's own weekly repeat: **campaign** |
| carry one letter | campaign Ch 5's forty letters |
| bread and salt to a steward | campaign Ch 6's truce tokens |
| return a cache item | campaign Ch 6 |
| the Frosthollow Count | campaign Ch 8 |
| oil for a post; a horn for a keeper | campaign Ch 9's keeper stocking |

Campaign §7's seed table and cast-and-talk §3.25 are replaced by a pointer to this table.

---

## 8. Scaling (critic P1-17)

### 8.1 Quests with `join.cap: 1`
These are small #3, #23, #25, #30 and medium Q4, Q17, Q18. Today, `repeat` counts only from "done", so a wolf who fails or abandons may retake at once. So each of them:
1. is `repeat: weekly`, never daily;
2. carries a deadline `fail: {afterDays: 1}` (one game day, 4 real hours), so the copy comes back quickly;
3. has an "it's taken" helpful line on the giver's town that points to another quest there;
4. where the world object allows, gets a second copy: for example, a second strayed-ewe trail region with `join.cap: 2`. The reviser says which.

With **N4** (§5.2), each gets `join.again: 7`.

### 8.2 Large lines
- **Chapter 1** (talk, deliveries, per wolf; no held resident, placed item or spawn) has `join.cap` **40**.
- **Fight chapters** keep `join.cap` 12, but spread their waves over **two** spawn regions, used in turn, so two groups of six fight at once.
- **Road quests** (journal offers) have no cap.
- Goals stay helper-grown (`base + k × helpers`). With C2 the goal becomes `max(base, k × active)`.
- `join.cap` is a literal, so no operand is proposed for it.

---

## 9. Every other critic item, and which file fixes it

| Critic | Decision | File |
|---|---|---|
| P0-1 tills | §3.2.5; SQ2 in Phase 1 | engine-spec, campaign §2.7 and §12, large §1.5 and §2 fact 5, medium §0.4, small §1.5 |
| P0-2 engine items | §5 | engine-spec §1, §4, §5, §7, §9 |
| P0-3 camps | §4 | campaign Ch 1, 3, 5, 6, 7, 9, §5.2, §5.3; every pack fight |
| P0-4 givers | §1, `givers.tsv` | small §4.1, medium §6, large §14.1, cast-and-talk §3.25, campaign §7 and §10.3 |
| P0-5 duplicates | §2 | medium, large, small #2 and #17 |
| P0-6 stage quests | §7.3 | small (program + 12), medium (5), large (5), campaign §7, cast-and-talk §3.25 |
| P0-7 calendar | §6, **now pacing by readiness** (user (a), 2026-11-22): no dates, no deadlines; participation plus a minimum time, or Advance | campaign §3 (rewritten), every chapter's Ends on, §4.6, §10.1, §11, §12; engine-spec R1, R2, §7.1, §7.4, §8 D10 and D12; cast-and-talk §4.3–4.4 (the grace lines, optional) and every line that dates the Moot; the packs' stage gates (§6.4) |
| P0-8 purses | §3 | every file |
| P1-9 approval gaps | `alive` check; accounts must exist (§5.2) | engine-spec V1 |
| P1-10 pair memory | C8 `scope: pair` in Phase 1 (§5.2) | engine-spec C8; cast-and-talk §5.4 (T1 → "in C8") |
| P1-11 cast corrections | Apply cast-and-talk §2 corrections 1–8 in campaign §8: Gorm at the Sump Infirmary; Cressida cold and frank; Fell Miremore plain; the Counting House clerks at Ashdown House; the Greatly Bank flat is Eleanor's; Corra Coalcot's own list hook; the matching notched ears. Look-alikes as §1.6. §2.10 says **three** strangers (the Envoy, the Captain, Hakon). Line 2027 is corrected. | campaign; cast-and-talk §2 (look-alike rows) |
| P1-12 the beaten Captain | **Beaten:** `visitor.add` id `captain` (`like: cty_stone_sunton`) at the Watch cells `r2_the_keep_down` for one game day after the Turn, a board post on `board:ridgemere` ("The Grey Captain is held at the Keep, to be judged at the Moot's end"), and talk lines; the epilogue's judgement is board and talk only. No resident body, no `npc.hold`. | campaign §6 Ch 9, §8.2, §9.8; cast-and-talk §2.2, §3.24 |
| P1-12 `npc.speak` (Ch 2, Desmond at the Carters' Yard) | removed. Desmond's words go into his own window: `say[self.id=rmx_desmond_hollis world.mem.nh_stage=reckoning]` in `watch.tree` (today, with a DM-seeded key), and C5 `npc.greet` to the wolf who came near the yard (`near resident`), once C5 exists. | campaign Ch 2 (line 631) and §11 (line 722); cast-and-talk §3.16 |
| P1-13 Brenna and Dagfinn | §1.4 | campaign §2.6, §2.8, §5.1, §10.3; cast-and-talk §3.1, §3.3, §3.8, §3.20 |
| P1-14 innkeepers | §1.2.4 (none, anywhere) | campaign §5.1 (Nell's fallback), §7; cast-and-talk §3.25; large §14.1 already right |
| P1-15 closure | `closure: true` (read as `"line"`) on each large line's last-chapter road quests, the new line's too. That is at most 3 a program (a wolf usually takes one road), inside answer (b)'s cap of 4. It is inert until J3 (Phase 4). | large |
| P1-16 coverage | §7.1, §7.2 | small, medium, large |
| P1-17 scaling | §8 | small, medium, large |
| P2-18 small packs' phases | §5.2 last bullets | engine-spec §7 |
| P2-19 the literal "…" | Spell out every value: `world.mem.nh_stage=moot_called\|reckoning\|signals\|howls\|letters\|lights\|harvest\|bell\|moot` (or whatever subset the line means). `\|` alternation is accepted: `Core/RatwTalk.cpp:1037` splits gate values on `\|`, and the library already has `say[pair.disposition=hostile\|wary]`. | cast-and-talk line 617 and any other "…" in a gate |
| P2-20 Corwin's voice | "plain" (larkfield, accent none) | large `lq-wreckers-light` cast table |
| P2-21 scree goats | Credit Frosthollow; the campaign's `6_3` camp row moves to Frosthollow (§4.4); ibex are confirmed. | small #31, campaign §5.3 |
| P2-22 face banners | **Decided: a `story.tell` banner with a face is not the dialogue window.** No `story.tell` carries `from` with a resident's words. A resident's words to a wolf go only in: the offer's `say` / `thanks`; `npc.offer` and chain replies; `npc.line`; C5 `npc.greet`; C6 `offer.during`; scenes (each speaker in their own panel); or their own tree lines (`say[self.id=…]`). Before C5, a resident's one-off thanks is a gated tree line or a `deed.award`. **Proclamations** (Brenna, the Keepers' Ember steps) become `board.post` notices, signed in the text, plus each speaker's own window line. Plain `story.tell` narration with no `from` and no quoted speech stays. | campaign §2.1, every Blaze/Ember and every `story.tell from`; medium (every "Close a/b (story.tell from …)", Q19 Kite); large §13.1 C5 fallback, Q10; small (any); cast-and-talk §0.2 vehicle codes, §3.22 (Hilde's whisper; the families' replies), §3.24 |
| P2-23 names | §5.1 | campaign §3.1, §11.1, §11.3; every pack's "C12" |
| P2-24 checks | Before approval, every reviser lists and runs: `tools/dialect_check.py --world ridgemere3`; `build-core/talk_check --data "$PWD/Data/Talk" --validate …` and `--render-trees`; `python3 tools/talk_lint.py`; `story_program.review(p)` / `problems(p) == []` / `story_runner.references(conn, 'upper_accord', p)`; the §1.2.7 giver check; the §1 query for every named resident. | every file's checks section |

---

## 10. Each reviser's list

- **campaign.md**
  - §1.4 Brenna, Gerrit and Dagfinn;
  - §1.3 the two new fallbacks (and §8.6 protection);
  - §1.6 look-alikes;
  - §3 Purse path, the new §2.11 `nh-purses` and the counters;
  - §4 camps and watchposts;
  - §5.1 names;
  - §6 pacing by readiness (**done 2026-11-22**: §3 rewritten, every chapter's **Ends on**, §4.6, the Moot's name by season, §10.1 `nh_debts=grace` optional and `nh_moot`, §11.3–11.5, §12);
  - §7 seeds → stage pack pointer; a late howl and the Silver weekly kept as the campaign's own;
  - §9 rows P1-11, P1-12 and P2-22;
  - §10.3 → `givers.tsv`.
- **quests-small.md**
  - §1.3 re-casts;
  - #2 and #17 no cough;
  - #26 and #29 board-only (#29 with a new premise);
  - #31 credit;
  - #46–48 estates;
  - the `nh-pack.stage` program and its 12 small entries;
  - §3 purse rule (P3 replaced);
  - §8 caps;
  - SQ ids as in §5;
  - §6.4: `st-moot-benches` and the deed "built benches for the Midwinter Moot" say "the Moot" (or are gated on `nh_moot`); the `s9` table row's "Midwinter, and the Moot." likewise.
- **quests-medium.md**
  - Q1 to Edda, with Piet a target;
  - Q5, Q7 and Q11 replaced (§7.2);
  - Q14 to Harl;
  - Q18 to Tor (§2.4);
  - new Q23;
  - 5 stage entries;
  - §3 purse rule (the `coins_<town>` guard replaced);
  - §8 caps;
  - drop M1, M4 and M6 as their own items and point to L1, L2 and SQ2;
  - every close moved off `story.tell from`;
  - §6.4: `st-bell-road` runs in `s9` until the call (not "days 302–319") and its lines say "the Moot night"; the freeze gate reads `nh_mere_frozen`; the cooper's quest (Edric Millton, "deadline: the next new moon") keeps J4 as optional timing with a fixed 3-game-day fallback.
- **quests-large.md**
  - absorb medium Q7 into `lq-wreckers-light` ch 1 and medium Q11 into `lq-clipped-silver` ch 1;
  - the Q5 foreshadowing into `lq-runnel-fever`;
  - targets per §1.3;
  - the new Frosthollow line;
  - 5 stage entries;
  - `closure: true` on every line;
  - caps (§8.2);
  - purses (§3: no till before SQ2; balances);
  - fact 5 reworded;
  - Corwin plain;
  - C5 fallback off `story.tell from`;
  - §6.4: `st-post-night` from Ch 9's opening to the call (no game days, no season-day table); `st-midsummer-road` in the feast days that open Ch 4; Grete's and Beatha's "Midwinter" lines say "the Moot".
- **cast-and-talk.md**
  - §2.4 and §3.24: look-alikes (§1.6) and the beaten Captain;
  - §3.3 and §3.8: the Moot backing moves to Gerrit;
  - §3.1: Dagfinn's Question from Ch 2;
  - §3.16: Desmond's Ch 2 line in the window;
  - §3.25 → stage-pack pointer;
  - §4.3–4.4: the `nh_debts=grace` lines;
  - line 617: no "…";
  - §5.4: T1 → C8, T2 → N2, T4 → V2, T8 → N1;
  - every `story.tell from` that carries a resident's words (P2-22);
  - Fenshore and Ridgemere fallbacks;
  - §6.4: the grace lines kept as optional; every line that dates the Moot reworded ("A Moot at Midwinter", "before Midwinter", "on the longest night", "Next Midwinter"), with winter variants under `nh_moot=midwinter`; Tobias after "12 game days into Ch 5".
- **engine-spec.md**
  - §5.2's rows: catalogue entries or "deferred" lines with fallbacks;
  - V1 `alive` and accounts;
  - C8 pair scope;
  - N2 `player.story`;
  - V2 including T4;
  - N1 = T8;
  - L2's naming rule;
  - new N4;
  - Phase 5 renamed;
  - §7 pack mapping corrected;
  - §7 "Act I needs no phase";
  - the sizes in §1 and §9;
  - §6.2: R1 rule 4 (the floor never above the active count), R2 `slowNeedsYou` and R3's "Ready to push" text; D10 and D12 marked settled; §7.1's target day and chalk night, §7.4's "fastest" line, and the stand-in as campaign §3.2.5.
