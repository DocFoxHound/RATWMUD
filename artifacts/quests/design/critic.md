COMPLETENESS CRITIC: The Ninth Howl design set (campaign.md, quests-small/medium/large.md, cast-and-talk.md, engine-spec.md)

How I checked: I read ratw_ridgemere3 read-only through my own READ ONLY transaction helper at /tmp/critic/q.py. I changed nothing in the repo or the world.
- **Ids.** I pulled every id from the six files (423 tokens) and checked each against live.npcs, live.npc_state, world.cells, world.interiors and world.links.
  - All resident, place and interior ids exist.
  - Dead: `rm_gunnar_harrowgate` (intended) and `tn_cw_iris_greywater` (named only as the dead potter, fine).
  - Unplaced: `cty_eskar_rimedrift` (intended).
  - Unreachable (0 links): only `rm_the_rain_hall` (kept offstage on purpose).
  - `rm_grayrock_journey` is a real patrol route. Every faction id is real; `warden_order` is DM prep.
- **Items and species.** Every item and species I spot-checked is in items.json and animals.json.
- **Forbidden words.** None in any file (dog(s), puppy, god(s), goddess, deity, tells as a noun). Only verb uses of "tells".
- **Dialects.** I scanned about 1,300 quoted lines against `dialect_check.py --lexicon`. Every hit was correct for its speaker or tag. I spot-checked 40 speakers' home_region/accent against live.npc_voice, and all but one match (fix P2-12).
- **Game day.** The checkpoint has calendarDays 66.25 at 2026-11-22 22:17 UTC. Only 5 wolves exist (game.characters).

## P0: blocks shipping or makes content silently not work

1. **Shop tills cannot pay.**
   - **Files:** engine-spec.md §6 V1 and §7 Phase 1; quests-large.md §1.5 and §2 fact 5; campaign.md §2.7, Ch 2 (line 670), §12 (line 2025).
   - **Problem:** `tools/story_quests.py:79-86` `check_purse` only accepts `till:<id>` with two parts, so `till:job:<id>` is refused at validation. `Game::storyPurseAccount` passes `till:<id>` through unchanged, so that form names no account at all.
     - The campaign's Purse path (`till:job:rm_eleanor_saltcot`) is refused.
     - So are all of quests-large's restitution and contract payments (osric_ropeworth, brenna_slaggate, vigdis_rainmore, rupert_fogton, corra_coalcot).
     - quests-large §2.5 claims "Tills are real purses", which is wrong today.
     - engine-spec V1 checks `till:job:<resident>` "needs the resident", but never fixes the validator.
   - **Fix:**
     - Add SQ2/M6 to engine-spec Phase 1 as its own entry: `check_purse` accepts `till:job:<id>`, and `references()` checks that the account exists in the checkpoint's `society.accounts`. The balances exist: eleanor_saltcot 161, osric 125, brenna_slaggate 156, vigdis 112, rupert 90, corra_coalcot 141, ilse_coalley 126, edric_millton 197.
     - In quests-large §1.5 and campaign §2.7, state the treasury fallback until then.

2. **The engine spec leaves out every engine item the packs and the talk plan found.**
   - **Files:** engine-spec.md §1, §4, §5, §7, §9.
   - **Missing:**
     - small: SQ1 objective `where`, SQ2 till fix, SQ3 lend a paw to a working resident, SQ4 `rewardLimit`, SQ5 objective `credit: target`;
     - medium: M1 `wild.spawn` / `wild.leave`, M2 purse operand, M3 `house:<faction>` purses, M4 named story camps, M5 `cell` on `trade done`, M6;
     - large: L1 `hunt.stage`, L2 `camp` on `story.area.spawn`, L3 `camp cleared` / `caravan arrives` / `ship docks` / `project built` / `festival` as mission events, L4 `npc.sentence`, L5 `quest.withdraw`;
     - talk: cast-and-talk §5.4 T1 (pair scope), T2's `player.story.<var>`, T3 strangers in scenes / `story.tell from_visitor`, T5 `npc.line` through inVoice, T6 a story topic, T7 the `story.cleared` claim kind (only `story.rumour` is in C8).
   - **Fix:** add catalogue entries, or an explicit "deferred" line with a fallback, and phases for each. Merge the duplicates:
     - M1 and L1 both solve the bear/boar spawn: pick one (L1 `hunt.stage` fits the hunt arena).
     - M4 and L2 are the same `camp` parameter with different rules (`[a-z0-9_]{3,40}` vs `{3,30}`; "unique per run" vs not): give one rule.
     - M6 and SQ2 are the same fix.
     - T2 overlaps N2, T4 overlaps V2, T8 overlaps N1: cross-reference them.

3. **Campaign camp objectives can never fire.**
   - **Files:** campaign.md Ch 1 (lines 482-488), Ch 3 (lines 743-747), Ch 5 (lines 1000-1004), Ch 6 (`6_12`, `7_12`, the cudgel bands at `2_3`/`3_3`), Ch 7, §5.3 credit table.
   - **Problem:** each chapter spawns its own wave with `story.area.spawn`, then matches `camp cleared camp_w_ridgemere_<cell>`.
     - Story spawns are named `camp_story_<n>` (`Core/RatwRoads.cpp:1622`), so the match never fires.
     - The natural camps are mostly inactive in the checkpoint: 5_4 strength 0.2, 6_8 0.3, 6_12 0.2, 7_12 0.3, 2_3 0.2. Only 6_7, 8_6 and 4_12 are active.
     - So "break the Alder Mere camp", the Glasswater relay and the bounders all go unscored.
   - **Fix:** match `cell` plus a `spawned_<region>` switch, as quests-large §1.4 does (the event carries `cell`), or depend on L2.
     - Also: `camp cleared` is not a mission event, so it can't be a quest objective. Use a trigger with `objective.complete`, or L3.
     - Note that proj-1 (8_6) and proj-2 (5_4) watchposts are already posted while campaign §5.2 says "a camp must be cleared before its post can be posted". Reword.

4. **Givers carry more pack offers than the packs' own one-per-resident rule allows.**
   - **Files:** quests-small §4.1, quests-medium §6, quests-large §14.1, cast-and-talk §3.25.
   - **Problem:** a hub has room for only 3-5 story choices, and the rest are silently dropped until N1 (Phase 4).
     - In small and medium: `cty_disa_fogford`, `cty_ebba_coalgate`, `cty_fern_merewick`, `cty_sloe_harvestby`, `cty_stone_acorndell`, `cty_tristram_rainridge`, `tn_mb_iris_pikeford`, `tn_sm_bram_harvestgarth`, `tn_sm_pell_harvestley`.
     - In small and large: `cty_fern_merewick`, `cty_frost_colddrift`, `cty_ivo_sootridge`, `cty_jessamy_greyford`, `cty_sloe_harvestby`, `cty_stone_acorndell`, `rm_corwin_bilgestack`, `rm_rosamund_coalford`, `rmx_grete_dimmock`.
     - In medium and large: `cty_fern_merewick`, `cty_imogen_rustford`, `cty_orm_sootgate`, `cty_sloe_harvestby`, `cty_stone_acorndell`, `rm_ilse_coalley` (medium giver plus two large offers), `rm_septimus_plankmarsh`, `rm_theodora_wickley`, `tn_lf_pike_chalkley`, `tn_mb_kite_bogditch`.
     - In all three: **Fern Merewick, Sloe Harvestby and Stone Acorndell.**
     - cast-and-talk §3.25 adds more: Theodora Wickley (now 3), Pike Chalkley (3), Kari Bilgehand and Lichen Hedgemead (small givers), Orm Carlisle (large Q8). It also uses innkeepers Mab Carlisle and Bramble Reedford, against campaign §10.3 "innkeepers carry none". And it uses campaign cast: Aster Coldfell (Frosthollow Keeper fallback), Moss Blossomwell (post 10 keeper), Millicent Rustwell (steward), Harrow Reedford (post 9 fallback).
   - **Fix:** re-cast the medium and large givers and per-wolf offers to unused placed residents in the same town (there are 777 placed). Keep one pack offer per resident across all packs, and add a cross-pack giver table to campaign §10.3.

5. **Duplicate or contradictory quest premises.**
   - **medium Q5 `mq-net-court-cough` and large Q6 `lq-runnel-fever`:** same giver (Theodora Wickley), same Sump cough, same apothecary (Rupert Fogton). The small pack adds two more herb-for-cough errands (`sq-mending-herbs`, `sq-heights-thyme`).
   - **medium Q7 `mq-false-light` and large Q7 `lq-wreckers-light`:** the same false lamp on the North Headland rocks, with Septimus Plankmarsh in both. In the large line the culprit is Ilse Coalley, who is the sympathetic giver of medium Q1 "The Dark Round".
   - **medium Q11 `mq-short-weighed` and large Q4 `lq-clipped-silver`:** both Larkfield fraud lines from Pike Chalkley.
   - **Fix:** either merge each pair, making the medium the large line's chapter 1 with `requires`, or change the premise and giver of the medium. State in large Q7 how medium Q1's Ilse relates.

6. **The stage-gated pack quests are only one-liners.**
   - **Files:** cast-and-talk §3.25; campaign.md §7 seed table.
   - **Problem:** 18 stage quests are given only as say/thanks lines; no pack file has them. They are: Soup Kitchen bread, a Summons to a farmstead, the lamplighter's round (Piet Lumley, which clashes with medium Q1's giver Ilse Coalley), stone for a post, the Greenholt wagon, The Chorus That Sang Wrong, a late howl, carry a letter, pull a stake, bread and salt, Escort the Envoy, blankets to the ford, hot food for the haulers, feed Larkfield, an ice watch, oil for a post, the bell's road. Campaign §7's large seeds (the Patrolman's Round, Silver Under the Ice weekly, walk all twelve bounds, the Frosthollow Count, stand a post all night) are also unwritten.
   - **Effect:** the packs are almost entirely stage-agnostic, so Acts II-III have no stage-matched small or medium content. That weakens user point 4 ("always feel like they're making a difference").
   - **Fix:** add a `nh-pack.stage` program to quests-small and quests-medium with full entries (objectives, rewards, purse, `compare nh_stage` gates, phases, givers outside the casts), or drop the table from §7 and §3.25.

7. **The launch calendar is nearly out of time.**
   - **Files:** campaign.md §3.2 and §11.3; engine-spec §7.
   - **Timeline:** game day is 66.25 at 2026-11-22 22:17 UTC, and 6 game days pass per real day.
     - The reference launch, day 80, is about 2026-11-25.
     - The first new moon, 88.6, is about 2026-11-26.
     - The late-launch limit, day 110, is about 2026-11-29; day 130 is about 2026-12-03.
     - Phases 1-3 (67 agent-hours) are listed as "before Act I". Act II needs Phases 4-6 by about day 137 (about 2026-12-04).
   - **Seed problem:** the residents' gossip "owes a factor by the new moon" will pass at 88.6 with nothing happening if Ch 1 isn't live.
   - **Fix:**
     - Make Act I ship on the today-only version (§11.4) with a dated plan.
     - Compute every anchor from the publication day (J4 fallback).
     - Add a talk key and line "the factors gave a moon's grace" for the case where 88.6 passes before Ch 1.
     - State what happens if Act II's engine misses day 137.

8. **Shared purses drain at scale.**
   - **Files:** all programs.
   - **Problem:** town stores hold 212-248 pennies (coldwater 212, fenshore 213, frosthollow 247, larkfield 248, oakhaven 247, saltwell 224, southmeadow 236; treasury 1,355).
     - Every program draws on them: the campaign at ≤20 a wolf, the small pack with a per-program day cap, the medium pack with a guard at 60, and the large pack at 25-35 a wolf with no guard.
     - At 200 wolves the stores empty within the hour. Those stores also fund the town economy (doc 42).
   - **Fix:**
     - One shared rule in every program: a world number per town per day (e.g. `coin_<town>_day`, cap about 10% of the balance) plus a `purse crossed` floor of 60.
     - Add the guard to the large pack and the campaign.
     - Make the snapshot figures consistent (coldwater 200 vs 212; treasury 1,340 vs 1,355).

## P1: contradictions and missing pieces

9. **Approval gaps.**
   - **File:** engine-spec V1.
   - **Problem:** #4 checks placed (`home_area` / `work_area`) but not alive. 3 placed residents are dead, including `rm_gunnar_harrowgate` and `tn_cw_iris_greywater`, and they would pass. #2 accepts `town:<id>:works|watch|church`, but those accounts don't exist (medium §0.4).
   - **Fix:** check `live.npc_state.alive`, and check every purse against the checkpoint's `society.accounts`.

10. **Pair memory is used but never written.**
    - **Files:** engine-spec C8; cast-and-talk §5.4 T1.
    - **Problem:** 17 cast-and-talk gates use `pair.mem.*` (per-wolf greetings, "already pledged", Rosalind's trust), but C8 offers only realm/town/npc scopes.
    - **Fix:** add `scope: pair` (npc plus player) to `talk.remember` and `talk.forget`, or rewrite those lines on N2 / C5.

11. **campaign.md doesn't carry cast-and-talk's corrections.**
    - **Files:** campaign.md §8, §12 (line 2027), §2.10.
    - **Problem:**
      - The cast-and-talk §2 corrections (Gorm at the Sump Infirmary, Cressida cold and frank, Fell Miremore plain, the Envoy's look-alike) are not applied in campaign.md.
      - The Envoy's look-alike differs: campaign lines 1075/1776 use `rm_cressida_carlisle` (timber), cast-and-talk uses `cty_ulla_rusthand`.
      - Campaign line 2027 says Eskar is "like-less", but both files use `like: cty_stone_sunton`.
      - §2.10 says "two strangers", but there are three (Envoy, Captain, Hakon).
    - **Fix:** apply the corrections in campaign.md. Then re-pick two look-alikes:
      - The Envoy as Ulla gives her the face of Oakhaven's Keeper.
      - Hakon as `cty_ulric_stonevein` stands beside an identical Ulric in the Ch 9 Turn.
      - Choose placed arctic residents outside the cast.

12. **Two campaign mechanics break the engine or the talk rule.**
    - **Grey Captain "beaten: led to the city, judged in the epilogue"** (campaign line 1566, cast-and-talk line 322): impossible. C10 removes downed foes after the fight, and Eskar has no body.
      - Fix: a `visitor.add` "captain" at `r2_the_keep_down` for a day plus board and talk lines, or reword.
    - **`npc.speak` in Ch 2** (campaign line 631, Desmond at the Carters' Yard) speaks aloud in the world, against "NPCs talk only in the dialogue window".
      - Fix: `story.tell {to: wolf, from: rmx_desmond_hollis}` on `near resident`, or a scene.

13. **Brenna Abbot and Dagfinn break the campaign's 2-per-chapter cap.**
    - **File:** campaign.md §10.3 and §5.1.
    - **Problem:** Brenna is both herald and Ridgemere's Keeper. In Ch 4 she has pledge + backing + howl quest (3). In Ch 9 she has backing + Moot rule + pledge + post quest (4). Dagfinn in Ch 1 has round chain + Lamp backing + the Question (§2.8 says "every chapter").
    - **Fix:** make Gerrit Mudhand Ridgemere's Keeper, or make the howl and post quests journal offers and say so.

14. **The innkeeper rule disagrees between files.**
    - campaign §7 allows one pack quest per innkeeper.
    - campaign §10.3 and quests-large §14.1 allow none.
    - cast-and-talk §3.25 gives quests to Mab Carlisle and Bramble Reedford.
    - **Fix:** pick one rule.

15. **The large lines count nothing toward the Quickening.**
    - **File:** quests-large.md.
    - **Problem:** no line uses `closure: true` (J3), yet campaign §9.7 (line 1655) counts closure receipts from large side stories toward `closedStories: 2`.
    - **Fix:** put `closure` on each line's last chapter (1 per program, inside D1's cap of 4).

16. **Town and quarter coverage has holes.**
    - No large line is set in Frosthollow; Q3 only touches the Upper hearths.
    - Oakhaven has only 1 medium quest.
    - Timberland Court (`w_ridgemere_1_2`) has no quest in any pack.
    - Fellhall (2_1) and Grayrock Hall (0_2), and the estate quarter's 51 residents, are almost unused.
    - Northgate (1_0: 93 civilians and 35 merchants) has a few small and medium mentions and no large line.
    - **Fix:** add estate small errands (outside the House cast), a Northgate medium, and a Frosthollow large line that doesn't overlap Ch 8's Deepvein confession.

17. **Scaling at 200 wolves is thin in places.**
    - Large `join.cap` 12 with chapter 1 "once": a crowd queues for weeks. Use C2 `audience` to raise the cap, or run several instances.
    - The `join.cap: 1` quests (small: 3, 23, 25, 30; medium: Q4, Q17, Q18) have no `join.limit`, so one wolf can hold the only copy every week. Add a per-wolf cooldown.

## P2: smaller fixes

18. **engine-spec §7:** "Small packs need Phase 2 (C1a, C2)" contradicts quests-small §3.4 (C1 not needed; C2 only for quest 38). Correct it.

19. **cast-and-talk line 617:** the gate `world.mem.nh_stage=moot_called|…|moot` has a literal "…". Spell out every value, and confirm `talk_check --validate` accepts `|` alternation in gates.

20. **quests-large Q7 cast table:** lists Corwin Bilgestack's voice as "larkfield". The DB has larkfield with accent none, so his lines are plain, as quests-small says. Change it to plain.

21. **quests-small #31 `sq-scree-goats`:** sits at the Greenholt lime quarry (6_3) and credits Frosthollow, while campaign §5.3 credits the 6_3 camp to Coldwater. Align them. Also confirm ibex spawn in 6_3's habitat.

22. **`story.tell` with `from` banners as resident speech:** used for Keeper Ember lines (campaign §2.1) and Kite's close (medium Q19). Ask the user whether a face banner counts as "dialogue window only". If not, move them to C5 `npc.greet` / C6 `offer.during`.

23. **Naming:** campaign §3.1 calls C10 "npc mode", but engine-spec names it `foe.outlaw`. The packs cite "C12", which engine-spec splits into C12a and C12b. Unify.

24. **Before approval:** run `tools/dialect_check.py --world ridgemere3`, `talk_check --validate` / `--render-trees` and `talk_lint` on the written files. My lexicon scan found no real mismatches, but the trees aren't written yet.

User points 1-8 are all covered in substance. The world seeds are all used: King Gunnar's death and Dagfinn's accidental reign (I confirmed `job:rm_gunnar_harrowgate` holder = `rmx_dagfinn_thornham`), the Houses' falling-out, the debts by the new moon, the Watch bribe lists, Lord Grayrock's letter, the Rainward Hills, Gudrun's letters, the Chancellor weeping, the ice, the marsh lights, Mirebridge, the camps, Wardens and Quickened wolves. The gaps are items 4-8 and 16 above.
