# lq-black-hearths: The Black Hearths (notes beside the program)

Design: `artifacts/quests/design/quests-large.md` §12a (and §1, §13.3), `decisions.md` §7.2, §3, §4. Today-only
version. Check: `python3 tools/nh_check.py Data/Storylines/ridgemere3/quests-large/lq-black-hearths.json` (PASS, 6
rehearsals, nothing uncovered, references pass). It never goes to the mine, the Mine Captain's House or the Lamp and
Pick, never names the Grey Captain, and no line names the young collier (her first name is campaign cast's).

## What runs today
- **Phases**: `open` → `reckoning` → `after_contract`, `after_home` or `after_greenholt`; `season begins` in an after
  phase goes back to `open` with a fresh shared goal.
- **Chapter 1 `undersold`** (Marl Craghearth, Frosthollow board; once; cap 40): the carter, the east floor by daylight
  (6 to 18, with a journal line of what the wolf sees), the young collier, the Timberland Charcoal Store, then its hand;
  hand in to Marl. The four witnesses are **conversation targets only** (each already carries another pack quest, or
  none: Arne), so what they say is their window line below.
- **Chapter 2 `tarnfire`** (party; journal offer; weekly; cap 12): the counter (Stone), the lime burner (optional), the
  drying fire after dark (20 to 4) in `tarn_yard` or `tarn_yard_b`, three thieves, a marked sack. Entering a yard after
  dark raises a band there (2/3/4/6 by the wolves present), or in the other yard if a band already stands here
  (switches `spawned_tarn_yard`, `spawned_tarn_yard_b`); `camp cleared` in `10_2` leaves the sack by that fire (by the
  first fire, with both switches off, when the wolf who broke it stands in neither yard or the watch broke it) and takes
  1 off `nh_threat`, at most 3 a game day for this line (`threat_day`), so raising and breaking band after band cannot
  farm the campaign's threat. Before the reckoning both switches are reset each Emberday (the band comes back weekly);
  in any phase a band nobody fights starves out after some weeks (`bandits scattered`) and both switches go off.
- **Shared goal** `sacks_back` (shown "Sacks taken back"): 4, and one more for every three wolves who take a sack back.
  A game minute after the goal, the reckoning; on entering it the band is broken: `nh_threat` −3 (clamped).
- **Chapter 3** (the choices on Marl, decisions 1.2.1): `hearth_contract` (two charcoal to Arne, a word with Ebba),
  `hearth_home` (two charcoal to Marl, the Frosthollow square on a Marketday), `hearth_greenholt` (charcoal carried into
  the Greenholt hearths' yard).
- **The reckoning**: weight 1 a road (2 after a pack); first road starts the 3-day tally, a 7-day fallback. Home or
  Greenholt win only when heavier than both others; a tie or nobody: the price by the sack (`lq_hearths=contract`).
- **A season's return**: if nobody takes chapter 1 or 2 within 14 game days, the band is back (`lq_hearths=undersold`
  and a Frosthollow board notice). While `nh_threat` is over 40 at an `open`, the board adds "Carters go two together on
  the pass road this season."
- **Coin** (decisions 3.2, from triggers, all `town:frosthollow`, day cap 24): the fire's first time 6, a road 12. Most a
  wolf can get: 18. In kind otherwise.
- **Campaign feed**: a road done adds 12 to `nh_realm` and 12 to `nh_voice_frosthollow` (Greenholt: 3, and 3 to
  `nh_voice_larkfield`).

## Where the program differs from the design, and why
- Marl's three road choices are laid by a clock trigger a game minute into each later visit (`npc.offer` is a trigger's
  effect), untargeted and not `once`: a wolf who picks their road's choice before taking the road can pick it again
  (J1/C6 later: shown only to the wolf on that road).
- The floor step needs the wolf to be on Wet Coal (`on_ch1`), so the journal line goes only to wolves who asked.
- `quest is` is not readable by a campaign trigger today, so `road`, `paid_tarn` and `on_ch1` stand in.
- `closure: true` is not written (today's validator refuses it).

## DM steps
1. `lq_hearths` is declared in `ninth-howl/world-vars.json` (text, first value empty); the program sets `undersold` on
   its first `open`.
2. `nh-purses` approved and running first.
3. Talk keys (until C8): seed `lq_hearths=undersold` (realm) when the line opens (the four witnesses' window lines
   need it: without it chapter 1's conversations carry no clue); the result key when the reckoning settles.

## Later (engine phases)
- **J3**: `"closure": true` on the three roads.
- **L2/L3**: `"camp": "lq_bh_tarn"` and `"lq_bh_tarn_b"`; `camp cleared` as an objective by exact target.
- **C10**: a named leader of the band (`foe.add`), optional.
- **C6/J1**: each road choice shown only to the wolf on that road.
- **C8**: `talk.remember` the keys (and `lq_hearths_thin` for the day). **N2**: the witnesses' lines by the wolf's step.

## Talk lines for wave G (exact; window lines in `resident.tree` `hub`, priority 4; gossip priority 2)
```
say[self.id=cty_ebba_coalgate world.mem.lq_hearths=undersold priority=4] Half, cousin. Half, and a face at the store like I'd brought them mud. Those sacks were dry when I loaded them. Cold iron, I'd swear it on the top bend.
say[self.id=cty_iris_fernhaven world.mem.lq_hearths=undersold priority=4] I came up late, the way I always do, and the east clamps were open on the side and steaming. Wet ash, all of it. We raked it out as waste. By noon the waste was gone.
say[self.id=rm_arne_ashton world.mem.lq_hearths=undersold priority=4] Strangers bring sacks to the yard door at half the price, dry as a good song. I didn't ask where from. I should have. The hill carter gets what's left of our coin.
say[self.id=cty_stone_acorndell world.mem.lq_hearths=undersold priority=4] 'Arth, cousin. Twelve sacks of waste raked out, and twelve gone by noon. I counted. Drag marks to the north, deep, toward the tarn. Black seam, somebody's drying our char.
say[self.id=cty_leif_driphand world.mem.lq_hearths=undersold priority=4] A cart on the quarry road at night, no lamp, black sacks to the tail. Going west, to the city. Nothing good goes that way in the dark, cousin.
say[self.id=rm_arne_ashton world.mem.lq_hearths=contract priority=4] A price by the sack, and dry sacks from the hill. No more strangers at the yard door. I'd sing about it if you stood still long enough.
say[self.id=cty_ebba_coalgate world.mem.lq_hearths=contract priority=4] Marketday, down the pass road with a full cart and a paper that says what they'll pay. Warm hearth, cousin. That's all a carter wants.
say[self.id=cty_marl_craghearth world.mem.lq_hearths_thin=yes priority=4] The stores are scraped bare this week. Bread, and my thanks, and a riddle owed. Sit long enough and you'll get the riddle.
say[world.mem.lq_hearths=undersold here.region=frosthollow self.dialect=frosthollow priority=2] If you go after the char thieves, go at night and go below the tarn. They dry what they steal by a fire, cousin, and a fire shows.
say[world.mem.lq_hearths=undersold here.region=frosthollow self.dialect=frosthollow priority=2] Upper hearths charcoal at half price in the city. That's no cracked seam, cousin. That's somebody's hand in it.
say[world.mem.lq_hearths=contract self.quarter=commons priority=2] The charcoal store pays the hill colliers by the sack now. Dearer coal, and no strangers at the yard door.
say[world.mem.lq_hearths=home here.region=frosthollow self.dialect=frosthollow priority=2] Frosthollow charcoal on the Frosthollow square, Marketday. Sound sacks at our own price, the way it should be, stone.
say[world.mem.lq_hearths=greenholt here.region=larkfield self.dialect=larkfield priority=2] Greenholt wagons carry the Upper hearths' charcoal too now. Fair price, if not full weight for the hill folk.
# inn.tree greet, Frosthollow:
say[world.mem.lq_hearths=undersold here.region=frosthollow] The Upper hearths' charcoal sells at half in the city. The colliers want somebody to find out why.
```
- Allow-list (large §14.2 item 7): "Upper hearths", "Greenholt hearths", "Kestrel Tarn" if `dialect_check` objects.

## Wave J (as built, 2026-11-24, branch `r3-nh-live`)
The engine of doc 86 (Phases 1a to 9) is merged, so this version uses what the sections above call "later". Every
program here passes `python3 tools/nh_check.py --all` (now with the wave J checks: the prefix, the purse guard, the talk keys).

- `"prefix": "lq"`. **J3**: the three roads count as a finished Story (C6: after `undersold` or `tarnfire`).
- **C8**: `lq_hearths` by phase (the first witnesses' clue no longer waits for the DM); a wolf paid in bread tells the
  realm `lq_hearths_thin=yes` for a day. **L2** on the yard camps; **M2** and **C1** on every grant.
- **Left**: Marl's road choices are laid by a clock trigger, so they stay every wolf's (J1 needs an acting wolf).
