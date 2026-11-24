# The medium packs: notes beside the four programs

`quests-medium.md` §0–§4 and §5 (Q1–Q23), under `decisions.md` §1, §2, §3, §4, §8 and P2-22. The five stage entries
(§4b, S1–S5) belong to `nh-pack.stage`, which the small reviser writes; they are not here. Check with
`python3 tools/nh_check.py Data/Storylines/ridgemere3/packs/nh-pack.medium.*.json`.

| Program | Quests | Triggers | Variables | Rehearsals |
|---|---|---|---|---|
| `nh-pack.medium.city` | Q1–Q7 (Ridgemere) | 47 | 32 | 15 |
| `nh-pack.medium.greenholt` | Q8–Q12 (Coldwater, Larkfield) | 34 | 31 | 11 |
| `nh-pack.medium.south` | Q13–Q19 (Southmeadow, Fenshore, Saltwell, Mirebridge) | 49 | 45 | 14 |
| `nh-pack.medium.north` | Q20–Q23 (Oakhaven, Frosthollow) | 25 | 28 | 8 |

## What every pack does (today-only)
- Version 2, open audience (cap 5000), one phase `open` with no exits. Every quest: per wolf, `repeat: weekly`,
  `lifetime: campaign`, `abandon: true`, its giver's offer in their own dialect (`#symbols#`), deadline 2 game days
  (3 for Q6, Q10, Q19, Q21, Q22; 1 for the five that hold a resident).
- **Setup** runs on the story's own `quest started` (a refused start, the cap full, gives nothing away and moves nobody).
- **P-A** (night): a trigger with `hour between` completes a story-signal objective with `objective.complete`.
- **P-B** (fights): the objective is `bandit falls` by **cell**; the spawn trigger (once per copy, a player switch)
  calls `story.area.spawn` with 2, 4 or 6 by `party size` and turns on the **story** switch `spawned_<region>`
  (decisions §4.1 says story switch); `camp cleared` in that cell with the switch on turns it off. Nothing matches
  `camp_w_*`.
- **P-C** (choices): two `npc.offer` at the quest's own giver (`player: $actor`, `once`), laid when the step before is
  done; the `dialogue choice` trigger checks the quest is active, sets the wolf's `<q>_pick` and completes `choose`.
  Branch objectives are hidden and optional, gated with `after`, and a required last objective is completed by a
  trigger that checks the pick. Q16 chooses in a scene; Q18, Q19 and Q23 choose by what the wolf does.
- **P-D** (endings): on `quest completed`, an If on the pick gives the coin (P-F), the deed, a journal note (narration,
  no quotes) and **the giver's close as a follow-up choice in their own window** (`npc.offer <x>_since`, "How has it gone,
  since?"; the reply is the design's close). Then `nh_realm` +4 and `nh_voice_<town>` by the
  distance weight, and for a fight `nh_threat` −2, clamped 0–100.
- **P-E**: a story number per ending; at 3, 10 and 30 the town's board carries one notice in the town's voice, signed
  by no resident (7 days).
- **P-F** (coin, decisions §3.2): every `character.grant` sits inside an If on `coin_<town>_ok` and
  `coin_<town>_day` ≤ cap − amount, beside an Add to the counter, `ifRefused: skip`; else two loaves and a journal
  note ("…'s store is thin today"). No coin is a quest reward or an entry effect. No till is named (SQ2); the reason
  keeps the shop or House paying. Caps as decisions §3.1 (the balances have drifted a few pennies since; the caps are
  kept as written).
- **Held residents** (Q4 the pups, Q12 Imogen, Q17 Ridge, Q18 Tor, Q20 Tristram; `join.cap` 1): a per-wolf timer at
  0.95 game days fails the quest, releases the resident and tells the wolf (a deadline's own `quest failed` names no
  wolf); `quest abandoned` (the wolf's own copy) releases too. The "it's taken" words go on the town's board for a day,
  once per copy (story switch `mq_<q>_posted`, off at every end).
- **Targets' own words** (Piet in Q1, the coal crier in Q5, the pup in Q7, the baker in Q11, the skipper in Q23) are
  one-line individual **scenes** (`scene.play` for `$actor`), played at the step where the wolf meets them: only that
  wolf hears them, once, in the speaker's own panel (decisions P2-22). They were phase-entry `npc.line`s, which every
  wolf of the pack heard appended to every greeting, forever, before ever taking the quest. No entry `npc.line` is
  left. No `npc.speak`, no `story.tell` with a resident's words.
- **Doors locked to wolves** (doc 82: on Ridgemere 3 a private interior is locked to players; a merchant's workplace
  opens in the merchant's hours; inns, alehouses and taverns stay open; `World::playerDoorLocked`; since the third pass
  a door the story unlocks is open to wolves, used only for workplaces, see the end of this file). No objective asks a
  wolf to enter a locked interior any more, and no step needs a resident who never leaves one: those steps are an
  outdoor region at the door (the resident answers in a one-line scene: Piet at Saltgate Row, the Grayrock steward, the
  Fellhall steward, the priest of the Chapel of the Tide, Tamsin Eelbank, the pup at Drip Row), or the Watch, the
  Customs House and the press house are told or watched at their doors.
- **Pings** (`story.ping`, the wolf's own map, an hour) mark every region a step sends the wolf to that is not a
  named place (the ruts, the steps, the pier foot, the drips, the far pans, the wash, the cedar, the drift mouth...),
  with a journal note saying what the wolf found that points there.
- **Goods a quest hands out** cannot be farmed by taking a quest, grabbing the goods and putting it aside: the brandy
  of Q10 and the waterskin of Q17 come once a week a wolf (a per-wolf 7-day timer, `weekly_stock`); the fish of Q8 and
  the wine of Q14 are laid down only once the thieves are driven off, and Q14's casks go home to the vine hand. Q4's
  cache (110 pennies of brandy and beads) is no longer laid down at all.
- Effects to a wolf are `ifRefused: when here`; world effects (moves, holds, spawns, notices, weather, items placed)
  are `skip`, so one refusal never pauses a pack.

## Where it differs from the design, and why
- `spawned_<region>` and `mq_<q>_posted` are story switches (only this program spawns there or posts that notice), so
  they need no world declaration. The world variables declared are only the campaign's, the purse counters, and
  `mq_oil_list` (added to `world-vars.json`, Ch 8 may read it) and `lq_salt` (added to `world-vars.json` for Q18's read;
  its owner is `lq-salt-road`).
- **Q1**: Piet never leaves his house (no work post, home locked), so "find who drains the lamps" is the oil drips
  followed to his door on Saltgate Row (region `sl_door`), where he says his line in a scene. The optional pawnbroker
  step now gives a clue (a journal note pointing to Saltgate Row).
- **Q2, Q7, Q8**: one "deliver" objective, so the journal shows one line. Q2 a is handed in at the Customs House door
  (region `cu_door`, carrying the tally): the collector works and lives behind locked doors. Q7 b and Q8 need the right
  number in one gift.
- **Q3**: the Records Office is locked to wolves (no archive work there), so the rolls are a Records clerk at the Grey
  Gull after work (`rmx_lothar_abbot`, a target only). What the wolf learns at each step (the roll, the order, the
  ropemaker's refusal) is a journal note. Ending a is Grayrock Hall's door, where the steward answers in a scene
  (`wb_hall`); ending b is the Chapel of the Tide's door, carrying a candle (`wb_chapel`, Solveig's line; the wolf keeps
  the candle to light on Restday).
- **Q4**: the Sunken Street is locked to wolves: the pups sit at the foot of its steps (walked and held there,
  region `ss_steps`), and are led to the Hook and Cleaver's and the Morning Oven's doors (regions, with the pup in
  them). The cache is told: a, at the Customs House door; b, the pups take it to the poor box themselves (no goods are
  laid down; the priest and the collector are behind locked doors).
- **Q5**: ending a is reported at the Watch Office door (region `sn_keep`): the Watch Office is locked to wolves, and
  `reported` names the guard told, not the one reported.
- **Q6**: the clerk of the Counting House works and lives behind locked doors, so a gives the twenty pennies to the
  cooper, and b brings him the copied entry (from the Records clerk at the Grey Gull); the cooper lays it before the
  House. Archive work is dropped (the Records Office is locked).
- **Q7**: Vidar and Hilde never leave Timberland Court, which is locked to wolves. The pack holds Vidar at the Court's
  back gate by the coal chute from its opening (entry `npc.move` + `npc.hold`), so the quest can be taken at all; the
  watch is kept by the chute, the trail ends at Drip Row's door (region `tc_home`), where the pup speaks in a scene.
  Hilde's step is dropped. The basket goes to the pup's gran (Mab Fogham) at the Grayrock shipwrights.
- **Q9**: the wash takes 60 / spreads 40 (`random`, rehearsed with pinned rolls); the harvest words and the second
  `orchard_blight` are applied when the fire is lit or the wash tried. The first taking each week stages the blight
  (a 7-day story timer).
- **Q12**: Imogen walks to the square at the choice and is released at the end. The flour-short nudge is applied
  when the flour goes in the mud.
- **Q10**: both sample bottles go to the distiller on Fell's road (none is left in the wolf's pack); the steward
  answers at Fellhall's door in a scene, and says Fell has the Hills when `nh_b_hills` is `fell` (the design's read,
  done today).
- **Q11**: the lad's pig yard is pinged; "the Last Sheaf" is not named in the stage-7 close (decisions §6.3).
- **Q13**: the kill road is offered only after a bear has been hunted near the garth (story switch `mq_bear_about`);
  the honey road always. A wolf can set nothing down outside a fight (`item dropped` is only "knocked loose in a
  fight"), so the honey road is a comb rubbed in the wolf's ruff, walked out to the far woods (region `bg_far`); no honey
  is given. The night in the garth is a wait of 0.1 game days, then the wolf must still be in it.
- **Q14**: the press house and its cellar are locked to wolves: the watch is kept on the press house door after dark
  (region `vc_door`). The spoiled casks by the thieves' fire tell the wolf what the cellar hand did (a journal note),
  and the two casks go home to the vine hand (a gift objective) before the choice.
- **Q15**: once `nh_b_lights` is `laid`, taking the quest fails it at once with a journal note (the "creek is dark"
  words); no second phase. The float lamp is pinged on the wolf's map for 30 minutes.
- **Q16**: the scene's choice is spoken by the kiln hand ("Well? You heard us both. Say it."); the option labels name
  no one.
- **Q17**: water is a story-signal objective done by a gift of `waterskin` or `water` to Ridge. Ridge's second
  choice (tell / keep) is laid when the wolf asks why.
- **Q18**: the quiet ending (the Salt Road line ended `board` or `cart`) cannot be rehearsed, since a rehearsal cannot
  set a world variable; it is checked by reading. The threat falls when the toll-takers break, not at the hand-in.
- **Q19**: the notice for ending a goes on the city's board, for b on Fenshore's (Mirebridge has no board). Tamsin
  Eelbank never leaves her house: ending b is her door (region `mx_tamsin`, carrying the list), where she answers in a
  scene; a dead Tamsin is told in a journal note at the start, and her door then does nothing. The optional "leave
  something at the crossroads" is dropped (a wolf cannot set an item down).
- **Q20**: "till the Long Night" became "till my whiskers go grey" (decisions §6.3: nothing that reads as the Moot's
  night).
- **Q21**: the colliers' notice goes up at most once a game day (a 1-day story timer).
- **Q22**: in winter ending a pays 10, else 6. The lump of coal from Stone's thanks is a shared item reward.
- **Q23**: the first of "home" or "sold" to be done sets the pick and fails the other.
- **Q6**: the cask from Edric's thanks is a shared item reward (`barrel`).
- Q3, Q10, Q16 and Q22 end with a journal note inviting a Story of two scenes; no `closure` anywhere (answer (b)).

## Not here (the talk wave, doc 86 §9.2)
- Every gossip and helpful line of quests-medium.md, the C8 "it's taken" lines, Nell's line in Q16, Tor's quiet line in
  Q18, Briar's and Ridge's ending lines (Q14, Q17), the givers' `during` lines (C6) and the pair-gated closes go to
  `Data/Talk/rules/story_mq.talk`. They are inert until C8; until then the DM seeds the `mq_*=open` keys by hand.

## Later (a new version as each lands)
- **J1**: the choices go to their wolf only; today a second wolf reaching a choice replaces the first's.
- **N2**: the targets' lines may move from their one-line scenes into their own windows, gated on
  `player.quest.<program>/<quest>=active`.
- **C8**: `talk.remember` the town keys (`mq_lamps=reported|shared` …) in each ending, the pair-scope closes, the "it's
  taken" key; **C5**: the closes as `npc.greet` instead of the follow-up choice.
- **C6**: `during` lines; `requires` for Q15 (`nh_b_lights` not `laid`) and Q18 (`lq_salt` not in board, cart, with
  **C12b** `in`).
- **SQ2**: Q6 pays from `till:job:rm_edric_millton` with `coin_till_edric_millton_day` (cap 19).
- **L1**: Q13 stages the bear (`hunt.stage bg_wood`), and the kill road is always offered. **L2**: spawns name their
  camp `mq_<region>` and `bandit falls` matches it; **L3**: `camp cleared` as an objective.
- **N4**: `join.again: 7` on Q4, Q12, Q17, Q18, Q20. **J4**: Q6 fails at the new moon (today 3 days).
- **M5**: Q22's sale must be in the city. **J9**: tagged tally (Q2), float lamp (Q15) and list (Q19). **C10**: named
  leaders in the waves of Q2, Q8, Q12, Q18, Q20. **C11**: spare Tamsin Eelbank (70) and Piet Lumley (60). **C14**: half
  the round back to Piet, a shared wheel (Q16). **M3**: Grayrock (Q3) and Fell (Q10) pay from House purses. **J2**: Q22's
  list as a realm-player flag. **C12a**: Q18's paid toll notifies the running chapter.
- Reads that need a resident's line (N2/C8): Q21's shopkeeper on `nh_b_seam=crooked` (Q10's steward is now a scene).
- **The world's door rule** (not this pack's to change): see "Givers behind locked doors" below for what this pass
  changed and what is still open. Opening a producer's workplace to wolves in its hours, or a story lifting a door for
  its wolves, would close the rest; then Vidar's hold can go. (The third pass did the second, for workplaces only;
  Timberland Court is Vidar's and Hilde's home, so his hold stays. See the end of this file.)

## DM steps
1. Wave A first: the world variables declared (`tools/nh_world_vars.py --apply`, which now includes `mq_oil_list` and
   `lq_salt`), `nh-purses` approved and published.
2. Approve each of the four programs on Ridgemere 3 and publish it; each runs with or without the campaign.
3. Until C8, seed the `mq_*=open` talk keys by hand when the talk wave's lines are in.

## Givers behind locked doors (2026-11-22, second pass; doc 86 "Givers behind locked doors")
A door into a private interior is locked to wolves (doc 82, `World::playerDoorLocked`; only inns, alehouses, taverns and
a shop while its merchant is in are open), and no story effect lifts it. Re-cast where the town has a free resident a
wolf can reach at work, with the lines in their speech; `givers.tsv` rows moved; talk lines follow:
- **Q1 `mq-sump-lamps`**: Edda Gullford (the Lamp Store, 17-23) -> Dorcas Rainford at the Lamp and Wick (8-18, plain),
  who sells the Lamp Store its oil and hears the oil was never thin; she takes the truth to the Lamp Store, which
  takes it to the Watch. Her three choices keep their ids.
- **Q8 `mq-glasswater-nets`**: Grist Reedmere (the Glasswater boat shed) -> Fennel Cornley at the Coldwater Stores (7-19,
  coldwater), who buys the catch; the net and the families' fish go to him.
- **Q9 `mq-meresend-blight`**: Pell Harvestley (the Meresend apple store) -> Kindle Applemeadow at the Coldwater Still
  (7-19, coldwater), which buys every Meresend apple; the charcoal, pitch and wash go to her, to go up to the orchard.
- Kept: Vidar (held at the Court's back gate by this pack), the clerks of Q3 and Q6 (Grete Dunn, Harriet Pennock,
  Lothar Abbot: every evening at an inn), Kell Reedwade in Q23 and Ragnar Wetby in Q11 (met on the walk the quest
  tells), the pups of Q4 (walked and held by the story).
- Left open by this pass, closed by the third (below; no free resident left in the town): Q13 Disa Fogford, Q14 Harl
  Oakbough and the cellar hand Briar Cornby (Southmeadow), Q15 Iris Pikeford and the fowler Nab Bogditch (Fenshore), Q17
  Fern Merewick and Q19 Kite Bogditch (Saltwell). Ridge Eelbank in Q17 is found where the story has moved him.

## Doors a story unlocks (2026-11-22, third pass; doc 86 "Doors a story unlocks")
A door a live story has unlocked (`door.unlock`) is open to wolves while the story holds it (DECISION of 2026-11-22,
engine on branch `r3-nh-doors`; doc 86 "Doors a story unlocks"). A story never unlocks a home, and `tools/nh_check.py`
refuses a door into one, or into a room whose stairs reach one.

- `nh-pack.medium.south`, phase `open`, entry: the Sunfields Hive Shed (`link_fs_bees_south_hives_door_a`, Q13 Disa
  Fogford), the Southmeadow Press House (`link_fs_vineyard_south_press_door_a`, Q14 Harl Oakbough, and Briar Cornby in
  its cellar down the stairs), the Fenside Boiling House (`link_fs_salt_fencoast_boiling_door_a`, Q15 Iris Pikeford),
  the Withy Creek Eel House (`link_fs_marsh_eels_eelhouse_door_a`, Q15 Nab Bogditch) and the Saltpan Salt Store
  (`link_fs_salt_saltpan_store_door_a`, Q17 Fern Merewick and Q19 Kite Bogditch). No home is behind any of them.
- Q14's watch is still kept at the press house door after dark, and the old wolf of Withy Creek (Q15) still answers at
  her own door in a scene: that is her home, which no story opens.
- There is no window to close before the run ends. A quest without `phases` is open in every phase of its story
  (`story_quests.open_in`), and each door here serves one, so each stays unlocked until the run ends, when the game puts
  back what each had. (`door.lock` would lock the workers out too. `door.close` closes a door to wolves and leaves its
  lock alone: that is how the stage pack ends its windows.)
