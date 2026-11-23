# nh-pack.small.south: notes beside the program (wave C)

`nh-pack.small.south.json` is the small pack for Larkfield, Southmeadow, Fenshore and Saltwell: quests 32-45 of
`artifacts/quests/design/quests-small.md` (§2.6-2.9), with the festival phase (§1.6). Doc 86 §4, §9;
`decisions.md` §1, §3, §8. Check it with `python3 tools/nh_check.py Data/Storylines/ridgemere3/packs/nh-pack.small.south.json`.

## What it is today
- Version 2, open audience (cap 5000), `quietAfterHours: 0`. It has two phases: `open`, and `festival`, which runs on days
  40-46 of every season. The campaign trigger `sq-festival-days` moves between them on `day begins`. Only
  `sq-festival-garlands` (38) is limited to `festival`, with `lifetime: phase`. Every other quest is offered in both phases.
- Ids carry the `sq` prefix by convention, because today's validator refuses a top-level `prefix`.
- **Purses** (decisions §3): six pay triggers, one for each purse and amount: `sq-pay-larkfield-3`/`-2`,
  `sq-pay-southmeadow-3`, `sq-pay-fenshore-3` and `sq-pay-saltwell-3`/`-2`.
  - Each pays only while `coin_<town>_ok` is on and `coin_<town>_day + amount` stays within the day cap (24, 23, 21, 22).
  - A payment is a `character.grant` with `ifRefused: "skip"`, plus an Add to the counter.
  - Otherwise the wolf gets `bread` ×2 and a plain journal note. No coin is ever a quest reward.
- **Feed** (P1): `sq-feed-<town>` adds +3 (Larkfield) or +4 (the rest) to `nh_realm` and `nh_voice_<town>`, and +1 to the
  wolf's `sq_done` tracker ("Errands for the south").
  - The full garland rope adds +4/+4 once per festival.
  - The hives' bounty lowers `nh_threat` by 1, clamped at 0-100.
  - Fallow's names add +1 to `nh_names`, only while `nh_stage` is 4-6.
- **Deeds** (P2): one `once: member` trigger for each quest, and one for each bonus deed (the wager, the crossroads, the
  Shortcut Gift and the Dowse Gift). The phrases are lower case, as the game speaks them.
- **The garland rope** (38), changed from the design so it can be filled:
  - The design's goal (6 + 2 × helpers, with one bunch counted per completion) could never be reached.
  - Now every bunch of herbs or flowers given to the giver by a wolf with the quest under way goes on the rope
    (`sq_garlands`), and the rope's length is 6 + 1 for each helper (`sq_garland_goal`).
  - A crowd fills it at about six helpers, and a few wolves fill it by bringing more.
  - When it is full: a signed `board.post` on Southmeadow's board, a `story.tell` with no `from` to the wolves on the square
    (region `sq_southmeadow_square`), and +4/+4.
  - The `ribbon_collar` bonus is a `once: member` trigger (flowers and a contest both done), not a `bonus` reward.
  - One trigger, `sq-garland-gift`, hears every bunch: a trigger on `gift` also fires on `gift again` (the catalogue's
    note). A second trigger on `gift again`, as P4 has it, would hear each later bunch twice, so there is none.
- **The wagons** (45): wolves who take the quest go on the list `sq_wagon_waiters`.
  - On `caravan arrives` or `cart arrives` at Saltwell, each listed wolf with the quest active who stands in
    `sq_saltwell_well` has `welcome` completed. A wolf whose copy is no longer active leaves the list there.
  - Changed from the design: the cart counts too. On Ridgemere 3 a caravan reached Saltwell 10 times in 67 game days
    (`game.events`), up to 23 days apart, so a 2-day copy that waits for one nearly always failed. The farm cart from the
    Redsand warren comes in most mornings, about 06:00-07:30; Moss's lines say so.
  - `bread` is a story signal, done by `sq-caravan-meal` (`shared meal` in `cn_saltwell_inn`) or `sq-caravan-bowl`
    (`trade done`, bought, in `cn_saltwell_inn`), each only after `welcome`. Changed from the design's `shared meal` alone:
    a shared meal needs someone already eating within two tiles (RatwOccasions.cpp; a resident eats only when hungry and
    carrying food), and of two wolves only the second to eat is its actor, so a lone wolf could wait at the inn for hours.
    No resident is named: the inn's only trader is its innkeeper, who stays a place, not a target.
  - These triggers are in both phases, because campaign triggers cannot read quest conditions (see below).
- **Dialogue**: every offer line is written in the giver's dialect. The voices were checked in `live.npc_voice`:
  - larkfield for 32-35;
  - southmeadow for 36-38 (37 broad, so `#greet#` gives "Good 'arvest");
  - plain for 39-41 (Fenshore-raised with accent none, or Ridgemere-raised);
  - mirebridge for 42;
  - saltwell for 43-45.

  Lines improved over the design:
  - 34's and 44's thanks no longer assume the stage of the campaign, or that the wolf comes from the city;
  - 36's thanks no longer says "Bless you" twice;
  - 37's thanks fits whether or not the wager was won;
  - 38's thanks says "a stranger's paw";
  - 42's ask and Yrsa's line are reworded;
  - 32: a badger's temper is `cornered` (animals.json: it runs from a wolf, and bites when it has nowhere to go), so
    Fallow no longer says badgers don't run. She points the wolf's nose at its trail, because badgers are rare on
    Plumtree's grass (about one hunt in ten finds one; a trail helps);
  - 34: Fern's "I resent every mile" is now said as a carter would say it;
  - 37: the wager's prize is the bonus flour, so Emrys bets two of flour, not a penny;
  - 38: the contest objective says "on festival day" (contests run on day 46 only, RatwFestivals.cpp);
  - 42's bonus is the **Shortcut** Gift ("blinks across a wall, a gap or a doorway"), not Mend: Mend tends an injury
    (RatwMagic.cpp) and cannot mend a board. Deed: "blinked across the gap in the Fenshore boardwalk with a Gift".

  The only resident words outside the offers are Fallow's names, an `npc.offer` reply in her window.
- **The salt** (43): the warrener hands over two measures on accept only once a game day a wolf (player timer
  `sq_salt_given`, as the north pack's `t_landing_loaves`), and the timer is cleared when the salt reaches the stores, so
  the next day's errand comes with salt again. Without it, putting the errand aside and taking it again minted two
  measures each time, and no purse in the world holds salt.
- **Fails and caps**:
  - 39 fails on the warrener's death (he is 58);
  - 45 fails 2 game days after it is taken;
  - each quest's `join.cap` and repeat are as the design gives them;
  - this pack has no `join.cap: 1` quests (decisions §8.1).
- **Rehearsals** (6 cases; both engines agree):
  - Larkfield: wrong cells and wrong targets are ignored, and a weekly quest is refused again;
  - Southmeadow: the bounty, and the wager won and not won;
  - the Southmeadow day cap: the eighth errand is paid in bread;
  - the festival week: the rope fills, a contest is entered, the collar is given, an unfinished copy expires when the
    week ends, and the festival phase's wagon triggers (a caravan and a bowl, a cart and a shared meal);
  - Fenshore: the crossroads, the Shortcut Gift, the names choice heard outside Ch 4-6 (no effect), and the warrener's
    death failing a copy;
  - Saltwell: the Lighten Load bonus, rabbits, a cart met and a bowl bought (a sale and a meal elsewhere ignored), a wolf
    away from the well running out of time, an abandoned copy, a caravan met with a shared meal, and the list pruned.

## Engine findings (today's validator and runner)
- **Quest conditions are a phase's.** `quest is` and `objective is` are refused in top-level (campaign) triggers ("Unknown
  quest"), because the campaign scope has no quest index. So the garland triggers live in `festival`, and
  `sq-wagons-arrive`, `sq-cart-arrives`, `sq-caravan-meal` and `sq-caravan-bowl` are written once in each phase.
- **A deadline's `quest failed` names no wolf** (the actor is empty), so a list of waiting wolves cannot be cleaned on it.
  The waiting list is pruned on the next caravan instead.
- **The rope's tracker** `sq_garlands` is shown in festival week and hidden on the first midnight outside it, with
  `story.tracker` in `sq-festival-days`. Between publication and that first midnight it shows "Garlands on the rope: 0".
- **What a rehearsal cannot reach**: `nh_stage` cannot be seeded by a rehearsal, so Fallow's names offer (Ch 4-6) is
  covered only on its stage-0 path. Check it live, or with the Campaigns console's What if…, after the DM has set
  `nh_stage` to 4.

## Later (engine phases)
- **C8** (Phase 1b): write the town keys `sq_on=yes`, when the pack is published and at each `season begins`, and
  `sq_last=<short>` for 2 days in each feed trigger (`badger`, `swill`, `linen`, `race`, `hives`, `hare`, `garland`,
  `nets`, `pans`, `clay`, `plank`, `salt`, `rabbits`, `caravan`). Until then the DM seeds `sq_on=yes` by hand in
  Larkfield, Southmeadow, Fenshore and Saltwell. The thin-purse key `nh_coin=thin` belongs to nh-purses.
- **C5**: Willa's `npc.greet` thanks to each helper when the rope is full, which replaces the gated tree line.
- **C1**: a thanks that varies with the purse (today it is the journal note and the tree's thin-purse lines).
- **C2**: the garland goal becomes `max(6, 2 × active)`.
- **J10** (`wolf has`): show the Gift objectives (42 Shortcut, 43 Lighten Load, 45 Dowse) only to wolves who carry the Gift.
  Until then their labels say "If you carry…". The design also offered Carry for 43, but Carry is a voice Gift, so only
  Lighten Load counts.
- **J1**: the names offer is already scoped to the wolf (`player: $actor`).
- **C12a** `story.notify` (`pack_deed`) and **J2** (`p_voice`, the quarter-share): add to each feed trigger in a new
  version.
- **L2**: once story camps are named, 36's bounty may also match the named camp; the cell match stays right for natural
  camps.

## Talk (wave G, not in this program)
The lines in each quest's Talk table in `quests-small.md` §2.6-2.9 go in `Data/Talk/dialogue/resident.tree`: `news`
(helpful knowledge) and `gossip`, gated `here.region=<town> self.dialect=<d> world.mem.sq_on=yes` or
`world.mem.sq_last=<short>`. They also take Willa's own line `say[self.id=cty_willa_wheatcombe world.mem.sq_last=garland]`
and the thin-purse lines of §1.8. They are not written here: `resident.tree` is shared by every pack and the campaign, and
wave G writes it. They are inert until C8, or until the DM seeds the keys.

For wave G, two Talk lines should follow this program rather than the design: 45's helpful line should say the farm cart
comes in early, most mornings (not only "the wagons"), and 32's "they don't back off" should say a cornered badger bites.
Done at the commit (2026-11-22): both lines, and Coldwater's "they don't run" badger line, now say so
(`artifacts/quests/build/talk2/build_packs.py`).

## Givers behind locked doors (2026-11-22, second pass; doc 86 "Givers behind locked doors")
Farm sheds, mills, presses, boat sheds and boiling houses are locked to wolves (doc 82), so their workers can be met
only on their walks. Where the town had a free resident a wolf can reach at work, the errand moved to them, in their
own speech:
- Larkfield: `sq-plumtree-badger` Vale Miremore (the Looms, larkfield); `sq-mill-race` Eskar Cornley, the baker, whose
  flour comes short (larkfield); `sq-swill-for-southwood` Rowan Chalkley at the Bakehouse, whose swill cart goes out to
  Southwood at dusk (plain).
- Fenshore: `sq-fenside-pans` Briar Greenshaw, the peat cutter at the Fenshore cutting (plain), who now has the
  drowned village's names for the Book (`sq-fallow-names`, said over the pans by the salt workers); `sq-boardwalk-plank`
  Ludo Ashwood, pot hand at the Heron (mirebridge).
- Southmeadow: `sq-festival-garlands` Briar Sedgebridge, who digs clay at the Sallow pits (plain; the garland gossip
  line is hers now, without "ripe").
- Left open by this pass, closed by the third (below; no free resident left in the town): `sq-hare-in-the-seed-rows`
  (Emrys Stillshore), `sq-smoke-for-the-hives` (Hob Meadowfield), `sq-twine-for-the-nets` (Leif Rainley), and Saltwell's
  `sq-ochre-rabbits` (Senna Applegarth) and `sq-salt-for-the-road` (Winifred Fogridge). Doc 86 lists what would close
  them (an engine or catalogue change, or board-only).
- Briar Greenshaw, Ludo Ashwood and Briar Sedgebridge share a first name with campaign cast: they are their towns' only
  free residents (decisions §1.2.5), and no line names them.

## Doors a story unlocks (2026-11-22, third pass; doc 86 "Doors a story unlocks")
A door a live story has unlocked (`door.unlock`) is open to wolves while the story holds it (DECISION of 2026-11-22,
engine on branch `r3-nh-doors`; doc 86 "Doors a story unlocks"). A story never unlocks a home, and `tools/nh_check.py`
refuses a door into one, or into a room whose stairs reach one.

- Phase `open`, entry: the Larkholm Granary (`link_fs_grain_lark_granary_door_a`, Hob Meadowfield), the Larkholm Mill
  (`link_fs_grain_lark_mill_door_a`, Emrys Stillshore), the Dune Rabbitry (`link_fs_warren_dunes_rabbitry_door_a`, Leif
  Rainley), the Redsand Rabbitry (`link_fs_warren_waste_rabbitry_door_a`, Winifred Fogridge) and the Ochre Press House
  (`link_fs_vineyard_ochre_press_door_a`, Senna Applegarth). Each is a workplace with no home behind it. The quests'
  words are unchanged: the giver is at work where the wolf can now walk in.
- No `door.lock` closes the window: the map locks no door, and `Door::locked` stops residents too, so locking a shed
  when its phase ends would shut its own workers out until the run ends. The doors stay unlocked from the phase that
  offers the quest until the run ends, when the game puts back what each had.
