# nh-pack.small.north: notes beside the program (wave C)

The small pack for Coldwater, Oakhaven and Frosthollow: `quests-small.md` quests 20-31, revised to `decisions.md` and
`givers.tsv`. Check it with `python3 tools/nh_check.py Data/Storylines/ridgemere3/packs/nh-pack.small.north.json`.

## What the program is
- Version 2, open audience (cap 5000), one phase `open` that never ends, `quietAfterHours: 0` (slow on purpose).
- 12 quests, each a per-wolf copy that lasts the campaign. 42 triggers, 19 variables and 6 regions. It has 7 rehearsals,
  and every trigger fires in at least one of them. The native and Python twins give the same review.
- **Purses** (decisions 3): `town:coldwater` (day cap 21), `town:oakhaven` (24) and `town:frosthollow` (24).
  - Coins are paid only from the six `pay-<town>-<amount>` triggers. Each pays only while `coin_<town>_ok` is on and
    `coin_<town>_day` plus the amount stays within the cap, adds to the counter, and uses `ifRefused: "skip"`.
  - Otherwise the wolf gets 2 bread and a journal line.
  - The program never writes a switch and never zeroes a counter: `nh-purses` owns both.
  - At the 2026-11-23 checkpoint Frosthollow held 235 (the design says 247). The design's caps are kept.
- **Feed**: Coldwater +3, Oakhaven +2 and Frosthollow +3 go to `nh_realm` and to the town's `nh_voice_<town>`, plus 1 to
  the wolf's own `sq_done`.
  - Each bounty bandit (quests 23, 26 and 31) takes 1 off `nh_threat`, clamped to 0-100.
  - Deeds come on the first completion only (`once: member`).
- **Givers** (all registered in `givers.tsv` for this program): Ingram Hazelshaw, Lichen Hedgemead, Sage Wheatridge,
  Oriel Blossomby (and her `pup-where` choice), Rusk Downfield, Brock Marshholm (mirebridge speech), Frost Colddrift,
  Ingram Ashleaf, Iris Fernhaven (plain) and Leif Driphand.
- **Board-only** (decisions 1.2.6): 26 (Oakhaven) and 29 (Frosthollow).
  - They have no giver, so they are posted on the board.
  - A wolf entering the square gets a journal offer, at most once a game day: the timer starts on the offer, on a "no"
    and on completion.
  - Each ends on a gift to a target (Tristram Rainridge for 26, Ebba Coalgate for 29), closed by plain narration.
- **Unique-object quests** (decisions 8.1): 23 (`join.cap` 1), and 25 and 30 (`join.cap` 2). Each repeats weekly and
  fails after one game day.
  - Quest 25 has two wool trails, `trail_n` and `trail_s`.
  - Its busy switches are worked out afresh from every copy still under way each time a copy ends. A copy that fails on
    its deadline names no wolf, so the ending wolf's own switch cannot be read.
- **Tiles**, checked standable on Ridgemere 3, read only, 2026-11-23:
  - the pup at `w_ridgemere_8_6` (242,170), on the grass just above the Upper Mere reeds;
  - the north tufts at `w_ridgemere_9_8` (238,96), (240,80) and (244,66);
  - the south tufts at (214,118), (202,130) and (190,142).
  - The design's first north tuft (230,94) was inside a building, and its pup tile (60,120) was nowhere near the water.

## Where it differs from the design text (deliberately)
- **No resident's name in any text.** Two givers are called Ingram, and the pup's name, Edric, is also a campaign
  name (decisions 1.2.5). Roles are used instead: "the orchard pup", "the apple picker", "the Oakhaven carter".
- **25:** the wolf keeps the wool (as in the design's rewards). So Brock asks the wolf to pick up three tufts and come
  back, not to bring them. The hand-in is "I followed the wool to the end of it", because the two trails lead different
  ways.
- **28:** the storm's narration plays when the storm breaks, to each wolf in Oakhaven who has already brought the
  herbs (the quest ready to hand in). It is no longer the bonus. The bonus is now a honey cake.
  - The trigger watches Oakhaven's own cell `w_ridgemere_3_4` by name. A storm somewhere else never counts, and the
    change is seen with no wolf outdoors.
  - The wolf counts when on that cell or indoors in an Oakhaven interior. The live game's wolf `town` is the region of the
    wolf's cell, and the outdoor cell's region is `cedar_sound`, so `wolf in town oakhaven` alone is true only indoors.
- **29:** the optional item is `oats` ("two measures of oats for the carters' pot", 1 penny each, kept by every store),
  which fits the porridge bonus. The design's `meal` is a prepared tavern meal at 6 pennies each.
  - The quest has no hand-in, so it ends on the last firewood. The summary and the label say to give the oats first.
- **26:** one coil, paid 4 pennies (`pay-oakhaven-4`). A coil costs 4 in the city, so two coils for 3 pennies left a
  daily errand 5 pennies out of pocket. The bounty label says to fight before handing over the rope, for the same
  reason as 29.
- **27:** a second objective, `meat`: give the feller 2 raw meat after the kill (the deer yields 4). This is so "Venison
  for the fellers" really hands venison over.
- **21 and 25, no minting by putting the errand aside.** Today `repeat` counts only from "done", so a wolf could
  accept, abandon and accept again at once.
  - 21: the baker hands over the 3 loaves at most once a game day per wolf (player timer `t_landing_loaves`). A wolf
    who takes it again that day is told to carry the loaves they had, or three of their own.
  - 25: a trail (3 fleeces of `wool`, 2 pennies each) is laid at most once a week per wolf (`t_ewe_trail`). A wolf who
    takes it again within the week is told there is nothing astray, and the copy fails at once. This works like N4's
    `join.again: 7`.
- **30:** the label says the wolf sits "through the first hour after midnight, until one". The check is being by the
  clamps when hour 1 begins.
- **Units:** straw is in bales and firewood in bundles, as the items are named.
- **23:** apples ×2 instead of ×3 (a basket each).

## Today only, and what comes later
- **C5** (`npc.greet`): the targets thank the wolf in their own window. Add a trigger on 26's and 29's `quest completed`
  that sends the greeting, alongside the narration. The lines (design §2.4, §2.5):
  - Tristram (oakhaven): "Good bark, you. The drag's moving on your rope, mm? My grandmother would've called you sound
    wood."
  - Ebba (frosthollow): "Lamp's thanks, cousin. The Pass House fires kept, and the carters with them. Warm hearth to
    you."
- **C8** (`talk.remember`):
  - `feed-<town>` writes the town key `sq_last=<short>` for 2 days, with these values: fox, landing, kettle, apples, pens,
    ewe, drag, venison, sleep, passwood, clamp, goats.
  - A `season begins` trigger renews `sq_on=yes` for 90 days.
  - Until then the DM seeds `sq_on=yes` by hand in Coldwater, Oakhaven and Frosthollow. The thin-purse key `nh_coin=thin`
    is nh-purses' (see ninth-howl/notes.md).
- **N4**: give 23, 25 and 30 `join.again: 7`. In the same version, drop 25's `t_ewe_trail` gate and its fail-at-once
  branch.
- **SQ1** (objective `where`): 25's tufts and 30's watch could match the region directly instead of going through a
  trigger.
- **J9** (tagged placed items): today a tuft left on the downs stays there when a wolf gives up, which is a little litter.
- **L2 / L3** (story camps): the bounties match by `cell`, which counts natural and story camps alike (decisions 4.1).
  Nothing to change unless a camp objective is wanted.
- **C12a / J2**: a pack deed should also tick the wolf's own "Your voice" tracker and the pledged town's quarter-share
  (design §1.7).
- **Talk** (wave G): the news and gossip lines of `quests-small.md` §2.3-2.5 belong in `Data/Talk/dialogue/resident.tree`:
  - the "it's taken" lines for 23, 25 and 30 (decisions 8.1.3);
  - the targets' own `self.id` gossip lines for Tristram and Ebba.
  - Every line is gated on `sq_on` / `sq_last`, so all of them stay silent until C8.

## DM steps
1. Declare the world variables first (`tools/nh_world_vars.py --apply`, ninth-howl/notes.md). This program uses
   `nh_realm`, `nh_voice_coldwater`, `nh_voice_oakhaven`, `nh_voice_frosthollow`, `nh_threat` and the three towns'
   `coin_*_day` / `coin_*_ok`. They are all already in world-vars.json.
2. Approve and publish `nh-purses` before this pack, and leave it running.
3. Approve and publish this pack. It needs no chapter and runs on its own.

## Givers behind locked doors (2026-11-22, second pass; doc 86 "Givers behind locked doors")
The Oakhurst pig yard, the Meresend apple store, the Larkdowns shearing barn and the Glasswater boat shed are locked to
wolves (doc 82), so their workers can be met only on their walks. Each errand now comes from the Coldwater wolf who
trades with that farm and works where a wolf can walk in, in coldwater speech:
- `sq-fox-at-the-hen-run`: Flint Downcombe, the Soapworks (Oakhurst sends its pig fat for the soap).
- `sq-apple-nose`: Laurel Pinefell, pot hand at the Glasswater Inn (the orchard's cart calls there); the pup is brought
  to the inn (`pup-home` is near her), and her window has the hint (`pup-where`).
- `sq-straw-for-the-pens` and `sq-strayed-ewe`: Varg Acorndell and Marl Cragpeak at the Coldwater Looms, which buys the
  Larkdowns wool; the straw goes to the Looms and up with the empty sacks.
- `sq-bread-to-the-landing`: the loaves go to Fennel Cornley at the Coldwater Stores, who buys the boatman's catch
  (`mq-glasswater-nets`) and sends the crews' bread down with the catch cart.
- `givers.tsv` rows moved; talk lines follow (`build_packs.py`). Oakhaven and Frosthollow had nothing to change: their
  givers work out of doors (the cut, the clamps, the quarry) or are board-only.

## Wave J (as built, 2026-11-24, branch `r3-nh-live`)
The engine of doc 86 (Phases 1a to 9) is merged, so this version uses what the sections above call "later". Every
program here passes `python3 tools/nh_check.py --all` (now with the wave J checks: the prefix, the purse guard, the talk keys).

- `"prefix": "sq"`. **C8**: each errand's rewards say `sq_last=<its word>` (2 days) and renew `sq_on=yes` in its town;
  the phase opens with `sq_on=yes` in Coldwater, Oakhaven and Frosthollow.
- **M2**: the six pay triggers read the purse itself too.
- **N4**: `join.again: 7` on 23 (`sq-apple-nose`), 25 (`sq-strayed-ewe`) and 30 (`sq-clamp-watch`); 25's own
  once-a-week gate (`t_ewe_trail`, the fail-at-once branch and the variable) is dropped. The rehearsal "Taken again"
  no longer takes the ewe errand a second time inside the week (the game would refuse it).
- **C5**: Tristram (26) and Ebba (29) thank the wolf by name in their own window (rewards' `npc.greet`, 7 days), with
  the design's lines.
- **Left**: J9 for the tufts (they are placed with `item.place`, which takes no story mark), L2/L3 (the bounties are on
  natural camps by cell, as meant), C12a/J2, SQ1.
