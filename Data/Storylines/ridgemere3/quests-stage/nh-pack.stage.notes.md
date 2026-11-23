# nh-pack.stage: notes beside the program (wave F)

`quests-small.md` §2.10 (the program and S1–S12), `quests-medium.md` §4b (5 entries), `quests-large.md` §12b (5 entries),
under `decisions.md` §1, §3, §4, §6.4, §7.3, §8 and P2-22. Check with
`python3 tools/nh_check.py Data/Storylines/ridgemere3/quests-stage/nh-pack.stage.json`.

## What it is (today-only)
- Version 2, open audience (cap 5000), 22 quests, 7 phases (`quiet`, `s12`, `s3`, `s4`, `s56`, `s78`, `s9`), 35 regions,
  63 variables (29 world, all already in `ninth-howl/world-vars.json`), 13 campaign triggers, 7 rehearsals.
- It sits in `quiet` until a chapter of the Ninth Howl runs. Two routes move it:
  - `stage_start`: a `story started` of `ninth-howl.<n>` enters that chapter's phase at once (and is the rehearsals' way in);
  - `stage_hour`: every game hour, the authority. It reads `nh_stage` and enters the matching phase, never the one it is in.
  - `stage_end`: `ninth-howl.9` ending, or a chapter stopped or abandoned, sends it to `quiet` and keeps it there (`st_over`)
    until a chapter starts again. A chapter ending only to start the next clears `st_chapter`.
  - The hourly read goes to `quiet` on an unset, 0 or past-9 `nh_stage` only while no chapter is heard running
    (`st_chapter` empty). That is what lets a rehearsal, which cannot seed a world variable, stay in its phase.
- Coins: every grant is inside an If on `coin_<purse>_ok` and `coin_<purse>_day` (cap less the amount), beside an Add to the
  counter, `ifRefused: skip`; else two loaves and a journal note. Small 2–3, medium 8–12, large 10–12.
  Large quests pay coin once a phase a wolf (`st_paid` tokens), then bread; the party quests (bounty, feast road, post
  night) pay every wolf of the hand-in wolf's party, and feed the campaign once a phase for the wolf who hands it in.
- Feeds: small 2/3/4, medium 4 realm and 6 town, large 8 times the weight; `nh_threat` only lowered (medium fights −2,
  each camp broken −1, clamped 0–100). It writes no branch, never `nh_stage`, `nh_surplus`, a post, a Keeper or a floor switch.
- Props (bread, the summons, stone, timber, mead, honey, lamp oil, blankets, meal, flour, felt, the stake) are given when the
  copy starts (`quest started`), once a game day a quest a wolf (`t_props`, `st_props`), so dropping an errand and taking it
  again mints nothing. A second start the same day gets a journal note instead.
- First-time deeds of the small tier are tokens in the wolf's `st_deeds` (once a wolf, ever); medium endings and large
  quests award their deed every time.

## Where it differs from the design, and why
- **No top-level `prefix`**: today's validator refuses it. Every id of this program begins `st` by convention (quests
  `st-…`, variables `st_…`, `gw_`/`bc_`/`rc_`/`iw_`/`br_` picks).
- **Fewer triggers than the design's count**: one completion trigger per pay-and-feed group (S9 with S10), one choice
  trigger per medium quest, one accept and one props trigger per phase.
- **Objectives are completed by effects, not emitted signals** (`objective.complete` / `objective.progress`): today's
  Problems list reads a signal heard only by a quest objective as "sent but nothing listens", and approval needs none.
- **Campaign triggers cannot read a quest's state** (the validator has no quest index for them), so the two quests that
  outlive their phases keep switches: `st_pr_on` (the round, with `st_round_at` for the posts in order) and `st_cb_on`
  (the bounty, party scope), set at the start and cleared on done or abandoned.
- **The night round has no deadline**: a deadline's failure names no wolf (the fact's actor is empty), so its switch
  could not be cleared. It is weekly and stays under way until done or put aside.
- **Gates a rehearsal cannot reach** are written so every trigger still fires:
  - S10 (`st-feed-larkfield`, only if `nh_b_ford = burned`): the offer triggers fire and test the branch inside; its
    completion shares `ford_done` with S9. Its own path cannot be rehearsed (no case can seed `nh_b_ford`).
  - S11 and S4 medium read Ch 8 as `nh_stage = 8` **or** `st_chapter = ninth-howl.8`.
  - S4 medium's freeze reads `nh_mere_frozen` **or** the pack's story switch `st_ice_frozen`, turned on when winter
    begins while Ch 8 runs (decisions §6.4: a copy of `nh_mere_frozen`).
  - S5 medium (the bell's felt) fails at the start when `nh_b_bell = lost`, or once `nh_stage` is 9 and the bell is
    neither rung nor cracked, or once `nh_moot` is set. S5 large fails at the start once `nh_moot` is set.
- **P10's ask** (S10, S11) is two triggers: a world-audience one sends the journal offer (it takes the wolf in), and a
  participant one lays the holder's `npc.offer` for that wolf once the quest stands offered to them.
- **S3 medium (`st-relief-convoy`)**: the flour counts at the Bakehouse whichever way was taken; the road ending needs
  the fight done, and a wolf who took the road but slipped past the wave gets a third ending (8 pennies, deed "slipped a
  relief load up the watched road to Larkfield's ovens", no threat credit). The loaves count only before the flour:
  the quest has no hand-in and ends at the flour, so the step's label says "before the flour".
- **S5 medium's felt is laid by standing on the cart's place, and brought back at dawn.** `item dropped` is written only
  when a fight knocks a held item loose (`Core/RatwBattle.cpp`, `World::dropItem`), so no wolf could lay felt with it, and
  no event takes goods out of a pack without `item.take` (which needs "may act on players' wolves"). So: on reaching the
  stage, a map marker shows the cart's place (region `br_cart`, `w_ridgemere_6_6` (226,52)–(236,60), on the road); entering
  it carrying the 2 felt completes `felt` (signal `br_f`, by `br_felt`); after the dawn keep, a new required objective
  `back` gives the 2 felt back to the weaver (gift, quantity), so the felt minted at the start goes home. The weaver's
  offer says so ("Lay this felt where it'll stand, keep the stage till dawn, then bring it back to me").
- **The checker's other fixes (wave F review):**
  - S5 large (the post night): an hour of the night counts **once a party** (party number `st_post_hr`, reset at the
    twentieth hour), and the raids at 23:00 and 2:00 send **one band a party**, sized to it, on the first standing wolf.
    Before, every standing wolf added an hour (a party of nine was done at 21:00) and each called a band scaled by the
    party (four wolves drew sixteen bandits).
  - S2 large (the bounty): `st_cb_on` and `st_bcell` are **player** scope, set for every wolf of the party that takes it
    (and `st_bcell` for every wolf of the party that breaks the camp). As party scope they were read from the wolf's
    party of the moment, so a wolf who took it alone and then joined a party stopped counting kills, and a camp broken by
    one wolf and handed in by another credited no town.
  - S6 small: the waterskin is the first completion's only (it was a free waterskin every day); a loaf handed back to the
    orchard keeper herself does not count as bread for the bounds.
  - The journal offers (S7, S10, S11) offer first and then start the once-a-day timer, so the timer starts on the very
    first offer (before, the wolf was not yet in the story when the timer was started).
  - Fights of the player-scope quests (the four medium waves, P-B, and the night round's cutpurses): **two bandits for
    each wolf's copy**, not a count scaled by the party for each copy. Each copy of a party spawned or called its own
    party-sized band, so three wolves who all took the road quest met eighteen bandits in one road region, and four on
    the round met sixteen at a post; now a party that all took it meets two each (six for three, as P-B means), and each
    wolf still needs its own two. The party-scope fights (the feast road, the post night) keep one party-sized band.
  - S4 large: a fresh copy cancels the wolf's old day-out timer (`bounds_late`), so a bound walk abandoned and taken
    again is not failed by the old copy's day.
  - Journal words: "this season" (the coin once a phase) is "in these days"; the feast road's turn-away no longer says the
    feast days are "over" when they have not begun; the single props (summons, lamp oil, felt, the stake) read right.
- **S3 large is titled "The Feast Road"** (decisions §7.3: the title loses "Midsummer"); the id stays `st-midsummer-road`.
  It is offered in `s4` and fails at the start outside the 3 feast days that Ridgemere's `festival` opens (`st_fair_on`).
- **S5 large's night** counts each hour from the 21st to the 5th once for a party with a wolf still at the post it took
  at the 20th (nine hours, so the whole night); raids at 23:00 and 2:00 by one `bandits.call` a party, on its first
  standing wolf.
- **S4 large's day** is a player timer signal (`bounds_late`, a game day from the first bound): the copy fails with a note
  and may be taken again at once, since an objective's count cannot be set back.
- **S1 medium's fail note** ("No wagon went out for Coldwater in time…") is not sent: the deadline's failure names no
  wolf. The journal's own "failed" stands.
- Ada's second `npc.line` (when `nh_b_hills` is set) is not written: entry lines cannot be conditional.

## Later (engine phases)
- **C8** (Phase 1): `talk.remember` the open keys `st_wagon`, `st_cloak`, `st_relief`, `st_ice`, `st_bell`, `st_round`,
  `st_bounty`, `st_fair`, `st_bounds`, `st_posts` at each phase entry, and `st_last=<short>` on each small completion;
  until then the DM seeds them. The closes below become pair-gated tree lines.
- **C5** (Phase 3): the closes as `npc.greet {npc, player:$actor, days:7}` in each ending's branch; Ingram's greet for S7:
  "Root well. That stake burned sweet, mm? The cut's ours yet, and you pulled for it."
- **C6** (Phase 4): `requires` on S4/S5 medium, S5 large, S3 large and S10/S11 in place of the start-time fails; the
  `during` lines below.
- **J1** (Phase 1): `npc.offer` per wolf (today the last wolf to reach a choice sees it).
- **SQ1** (Phase 2): S6's cells as an objective `where`. **L2/L3** (Phase 2): camps named `st_<region>`, `caravan arrives`
  and `camp cleared` as objectives. **J9** (Phase 6): the summons and the stake as tagged items. **C12a** (Phase 4):
  `story.notify` to the running chapter on each completion. **N4**: `join.again`.
- **T5** or a conditional line: Ada's `nh_b_hills` line: "Whatever the Council says now, the hills were walked in a day
  this year. I'll remember that, even if they don't."

## The closes and during lines (spoken in the window once C8/C5/C6 land; today the journal notes stand in)
- Aud (gw), during "About the wagon…": "We go when the board says. If you're going ahead, go after dark. The road under
  Alder Mere is where they wait." Close a: "Not a paw on the road under Alder Mere. I drove it with my eyes shut. Nearly.
  I kept the coal in my fist the whole way." Close b: "We came in whole, lamp on the knoll and all. Nobody asked us for a
  mark. I think they saw your teeth."
- Wick (bc), close a: "Drove it at first light. Not a paw on the road, and a black cloak walking down behind me like a
  heron. Fair catch, that road." Close b: "Waited the day, like you said. Glad I did; there was smoke by the road that
  night. I went the day after, and it was gone."
- Myrtle (rc), during "About the flour…": "The Bakehouse in Larkfield, the shop hand at the counter. Road or downs, I don't
  mind which. I mind if it's late." Close road: "Up the road, with teeth? Sheaf and sickle. Larkfield's ovens are lit. I
  counted the loaves back in my head all night." Close downs: "The long way, and every measure there. Ripe thinking. The
  downs don't ask you for a toll, eh."
- Garth (iw), during "About the ice…": "Wait for the freeze. Then the shore by the Boat Shed, late, and keep still. Take
  something warm." Close a: "Took my rope down to the Boat Shed. They let me pull on the end, eh. A shepherd on a bell
  rope. My ewes won't believe it." Close a once the bell is raised: "They raised it, you know. It rang for my ewes first.
  Fair catch, that." Close b: "Slept like a stone, and the ewes too. If it was a kind lie, it was a fair one."
- Jory (br), close a: "I'm walking down with it on the night. Packed one bag. Took it out again. Packed it. It's by the
  door." Close b: "Staying. Somebody has to remember the names here. I'll howl from our square, and loud."

## DM steps
1. The world variables of `ninth-howl/world-vars.json` are declared (wave A); this pack declares no new one.
2. Approve and publish after `nh-purses`. It waits in `quiet`, costing nothing, until a chapter starts.
3. Until C8, seed the talk keys above by hand when a phase opens (the stage lines in `resident.tree` read
   `world.mem.nh_stage=…` and `world.mem.st_*`).
4. If a chapter is moved by hand, the next game hour moves the pack (it reads `nh_stage`). To end it early, stop a chapter
   (the pack goes quiet) or stop this run.

## Givers behind locked doors (2026-11-22, second pass; doc 86 "Givers behind locked doors")
A door into a private interior is locked to wolves (doc 82), and no story effect lifts it. Re-cast where the town has a
free resident a wolf can reach at work, in their own speech; `givers.tsv` rows moved; talk lines follow:
- `st-summons`: the City Office clerk -> Delphine Brackwick, a carter at the Carters' Yard (8-18); the summons goes to
  Edda Ropeton at the Round and Rind (6-16), where Hillside's cart calls (the herder is in the Hillside byre).
- `st-bounds-water` and `st-twelve-bounds`: the Rainward orchard keeper and the Heatherside beekeeper -> Vesper
  Greenholt and Mogens Mudstack, who cut wood at the Rainwood coppice, out on the hills (6-17).
- `st-ice-watch`: the Larkdowns shepherd -> Wald Larkwell, a hand at the Coldwater Still (7-19), who asks for him.
- `st-haulers-food`: the loaves and fish go to Laurel Pinefell, pot hand at the Glasswater Inn, who sends the haulers'
  basket up (the boatman is in the Upper Mere boat shed).
- `st-feed-larkfield`: its offer and words are Clover Snowhollow's, a vat hand at the Larkfield Print House (7-19), who
  hands out Longmead's bread.
- Kept: the three patrol guards (`st-post-night`, `st-camp-bounty`, `st-patrol-round`) walk the street patrol.
- Still open (no free resident left in the town): `st-chorus-wrong` (Alder Blossomcroft) and `st-relief-convoy` (Myrtle
  Larkcombe) in Southmeadow, `st-fog-lanterns` (Stone Marshmore) in Fenshore.

