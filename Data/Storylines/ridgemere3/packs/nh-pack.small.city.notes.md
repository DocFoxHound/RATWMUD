# nh-pack.small.city: notes beside the program (wave C)

`quests-small.md` §1, §2.1, §2.2, §2.9a (quests 1–19 and 46–48), under `decisions.md` §1, §3, §8 and P2-22. Check with
`python3 tools/nh_check.py Data/Storylines/ridgemere3/packs/nh-pack.small.city.json`.

## What it is (today-only)
- Version 2, open audience (cap 5000), one phase `open`, 22 quests, 47 triggers, 8 rehearsals, 17 variables.
- Every quest is per wolf, given in its giver's dialogue window and (all but 3) posted on `board:ridgemere`.
- Coins: two triggers (`pay-city-2`, `pay-city-3`) pay 2 or 3 pennies from `town:ridgemere` under the one purse rule
  (`coin_ridgemere_ok` on and `coin_ridgemere_day` + amount at most 135, then Add; `ifRefused: skip`), else two loaves
  and a journal note. The counter and switch are `nh-purses`' (wave A); this pack only reads and adds.
- Feed: `feed-ridgemere` (+2 `nh_realm`, +2 `nh_voice_ridgemere`), `feed-oakhaven` for the crate (16); +1 `sq_done`
  (the wolf's tracker "Errands for Ridgemere"). First deed only: one `once: member` trigger a quest.
- Reads `nh_stage` (the Records page in Ch 4–6, the estates' coloured lines), `nh_b_quarrel` and `nh_b_peace`; writes `nh_names`
  only in Ch 4–6; writes no branch, stage, surplus, post or Keeper.

## Where it differs from the design, and why
- Props (bread for 1 and 9, the letter, the crate, the bolt) and the pup's walk to the pier run on the story's own
  `quest started`, not `quest accepted`, so a refused start (the copy taken, the cap full) gives nothing away.
- Props go to a wolf **once a game day a quest** (player timers `t_prop_bread`, `t_prop_alms`, `t_prop_letter`,
  `t_prop_crate`, `t_prop_bolt`). `repeat` counts only from "done", so without this a wolf could drop an errand and
  take it up again for endless loaves, letters, crates and wool cloth to sell. A second start the same day gets a
  journal note instead ("You were handed ... once already today. Carry it, or bring your own."). Rehearsal 8.
- "Under way" in a condition is `active` or `ready`: a quest whose required objectives are done is ready, and the
  optional ones (early bread, the Marketday stall) still count until it is handed in.
- P4 is one trigger each (a trigger on `gift` also hears `gift again`): two tallow candles or two cups of willow tea in
  one gift finish the objective, one adds one.
- The pup waits at (161,97) on `w_ridgemere_0_1`, the pier deck under the Grayrock office (the design's (161,101) is
  water). The region `piers` is the deck, (148,94)–(171,99).
- 3 also fails if the pup dies. The Records page is one trigger (complete it and add `nh_names` together).
- 15 (the mooring lines): ships come to Ridgemere two or three times a week (the timetable at the checkpoint had
  gaps of up to 5 days), so "be on the pier the moment she docks" with a 2-day deadline would mostly fail. A story
  switch `ship_in` is set on `ship docks` (target `ridgemere`) and cleared on `ship sails`; a wolf on the piers at
  the docking, or one who walks onto the pier deck (`region entered` `piers`) while she is in, takes her lines. The
  deadline is 7 game days. Wystan no longer names ships (names are rolled; the ones in the design were not on the
  timetable): he points to the notice by the office. Rehearsal 5 covers the late wolf and one after she sailed.
- 4: three armfuls of firewood, not five. A bundle weighs 15, and a wolf is overloaded (cannot move) past about
  twice 12 + strength/4, so five bundles could never be carried at once.
- 13: straw is in one city till (Sump Sundries), not "the provisioners"; Philippa says so. The deed reads "fed and
  bedded the Greatly horses" (nobody mucks out in this errand).
- 19: the Echo objective names Slate Tarn's cell (Echo works in any cell, so it counted anywhere), and the label names
  the Sound Gift (Echo is one of its abilities). Yrsa's thanks no longer says "go to Highmere next time" when Highmere
  is this errand's own optional objective.
- 16 and 18: Petra's and Corwin's thanks no longer repeat the "letter for the Grayrock lord" line (their shared
  generated backstory); Thyra (10) keeps it, so the pack says it once.
- 46–48: the two quarrel lines also need `nh_b_peace` off; with the peace kept (Ch 6), the quiet line plays.
- The purse reason is one line for every city errand ("The city pays for an errand well done"); a per-shop reason
  would need a pay trigger per quest.
- Rehearsals cannot set a world variable, so the Records page and the estates' lines rehearse only their quiet path
  (`nh_stage` 0: nothing shown); their branches are checked by reading. The day cap is rehearsed (case 7: the 46th
  carrier is paid in bread).

## Not here
- The talk lines of each quest (`resident.tree` `news` and `gossip`, the "it's taken" line for 3, the thin-purse lines of
  §1.8) belong to the talk wave (doc 86 §9.2 wave G); they are inert until C8.

## Later (a new version as each lands)
- **C8** (Phase 1b): `talk.remember` the town keys `sq_on=yes` (renewed each season) and `sq_last=<short>` (2 days)
  in the feed triggers. Until then the DM seeds `sq_on=yes` for Ridgemere and Oakhaven by hand.
- **J1** (Phase 1b): `corra-where` and the estates' coloured lines go to their wolf only; today a second wolf's line
  replaces the first's.
- **SQ2** (Phase 1b): pay the city errands from their givers' tills, each with its own `coin_till_<id>_day` counter.
- **C12a** (Phase 4): the feed also sends `story.notify` `pack_deed` to the running chapter. **J2**: the pledged
  wolf's quarter share.
- **C6**: the givers' lines while under way ("Still on the stairs?", "Any sign of her?"). **C5**: thanks on a later visit.
- **SQ1**: `where: {cells: [...]}` on the pheasant objective (drop `rainward-pheasant`). **SQ5**: a stall sale credits
  the stall's wolf (10). **SQ3**: lend a paw at the Ropewalk, the stables.
- **J9**: tagged letter, crate and bolt. **J10**: the Echo objective (19) shown only to an Echo wolf.
- **N4**: `join.again: 7` on 3.

## DM steps
1. Wave A first: the world variables declared (`tools/nh_world_vars.py --apply`), `nh-purses` approved and published.
2. Approve this program on Ridgemere 3 and publish it; it runs with or without the campaign.
3. Seed the talk key `sq_on=yes` for Ridgemere and Oakhaven by hand until C8.

## Givers behind locked doors (2026-11-22, second pass; doc 86 "Givers behind locked doors")
A wolf can't go through a door into a private interior (doc 82, `World::playerDoorLocked`): only inns, alehouses and
taverns, and a shop while its merchant is in, are open, and no story effect lifts that (`door.unlock` sets the door's
own lock, which the player rule never reads). So every giver and target here now works where a wolf can walk in
during their hours, and the lines are rewritten in their speech (all plain):
- 1 the bread goes to the infirmary's porter at the foot of the Sump Stairs, Ottilie Nailmarsh (the nurse never leaves
  the Sump Infirmary); 2 the apothecary on Market Square, Rupert Pitchford, mixes the House of Mending's draughts;
  7 a Sump Stairs porter, Kjell Coalham, carries the City Office's Heights letters, and the Vance who runs the Rag
  Mill, Eleanor Vance, takes the letter (the doorkeeper never comes down); 8 Wickham's chandler, Sten Kelpwell, and the
  service is kept at the cathedral door (region `cathedral_door`, Restday 9-11); 9 the abbey's brewer, Arne Wickdale,
  hands out the almoner's bread; 13 a Greatly Yard hand, Hjalte Sootby, feeds the stables; 14 a Sump Stairs porter,
  Mathis Plankham, wants the bell, rung from the rope at the Clock Tower door (region `clock_door`); 18 the Mortar and
  Pestle's hand, Mogens Ashley, carries the Heights' physic; 6 the Council Press's printer, Runa Ashridge: archive work
  is given nowhere on Ridgemere 3 (no records post) and the Records Office is locked, so the errand is now "Paper for
  the Rolls" (two of paper and an ink), and the hidden page is asked of the printer (`records-roll` on a conversation,
  Ch 4-6, still adding a name to `nh_names`).
- 46 Erland Fogley (the Lamp and Wick, lives in the Fell Cottages) and 47 Agna Dripridge (Bridge Stores, lives in the
  Grayrock Cottages) carry the estates' errands up the hill; 46's optional "come in by the kitchens" (a locked interior)
  is an optional pat of butter, so its bonus pie still has an optional objective. 48's bolt goes to Wenna Timberland,
  in the yard by the Court with the iron carts all day (the chamber keeper never comes out).
- The estates' coloured lines (`fell-*`, `grayrock-*`) are now asked "How is it up at Fellhall?" / "up at the Hall?",
  and answered from the cottages.
- `givers.tsv` rows moved to the new givers. The talk library's lines for these errands follow
  (`artifacts/quests/build/talk2/build_packs.py` FIX map; trees regenerated). Rehearsals follow (the region events,
  the paper and ink, the new residents' cells).

## Wave J (as built, 2026-11-24, branch `r3-nh-live`)
The engine of doc 86 (Phases 1a to 9) is merged, so this version uses what the sections above call "later". Every
program here passes `python3 tools/nh_check.py --all` (now with the wave J checks: the prefix, the purse guard, the talk keys).

- `"prefix": "sq"`. **C8**: each errand's rewards have its towns remember `sq_last=<its word>` (2 days) and renew
  `sq_on=yes` (365); the phase's opening says `sq_on=yes` in Ridgemere and Oakhaven. The DM no longer seeds them.
- **M2**: both pay triggers also read the treasury itself (at least 60 and the amount).
- **N4**: `sq-gone-to-see-the-ships` has `join.again: 7`.
- **C6**: words while under way from four givers (the pup's father, the Stairs porter, the bell's porter, the mooring
  hand), in plain speech with `#address#`.
- **J9**: the City Office letter (`letter^city_letter`), the Carters' Yard crate (`crate^yard_crate`) and Greywool's
  bolt (`wool_cloth^greywool_bolt`) are the story's own; only they count, and the second-start notes no longer say
  "or bring your own". The rehearsals' gifts name them.
- **J1**: every choice here already went to its wolf.
- **Left**: SQ2 for the errands (fourteen givers have tills; each would need its own counter and switch in nh-purses,
  and the economy has not been asked about shop tills paying out: the treasury still pays, worded as before); C12a/J2
  (the pack does not yet tell the running chapter), SQ1, SQ5, SQ3 and J10.
