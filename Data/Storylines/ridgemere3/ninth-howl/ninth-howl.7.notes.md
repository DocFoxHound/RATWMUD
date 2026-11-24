# The Ninth Howl, Chapter 7: Harvest Under Arms (`ninth-howl.7`, wave I, as built 2026-11-24)

Act III "The Moot Night", its first chapter (campaign `ninth-howl.act3`). Design: `artifacts/quests/design/campaign.md`
Chapter 7, with §2 (rungs, scaling, pledges, backings, purses), §3.2 (readiness), §4 (state), §5 (voices), §8.6 (holds)
and §10 (talk); `decisions.md` §6 (pacing by readiness). Made by a script kept out of the tree (`/tmp/nh7/gen.py`), as the
content waves' were; the review's fixes are in its copy `/tmp/nh7rv/gen.py`, which writes this file exactly (see "Review"
below). Nothing was approved, published or written to a world.

## Shape
- **Phases:** `ch7_muster` (working phase, the `ready` block) → `ch7_siege` (the three nights; no `ready`, so the console
  offers Go to phase…, not Advance) → `ch7_aftermath` (`ready`: only `minDays` 6 `since: "phase"`, `auto`) → `ch7_closing`
  (the herald's word at the next dusk at least 3 game hours on; `story.start ninth-howl.8 {audience: same, carry: true,
  when: ended}`; `finish`).
- **Readiness** (campaign 3.2.3): 25% of the active wolves, floor 2, cap 150, on `ch7_did`; 6 game days since the
  chapter; soft maximum 30 (`slowAfterDays`, `slowNeedsYou`), with `ch7_auto` counting it as participation met;
  `ch7_wait_bar` makes the call wait for `ch7_points >= ch7_goal`; `auto: true`. `activeDays: 18`.
- **Muster tasks** (each does the `ch7_did` step once a wolf): a story wolf in a siege region after dark (`hour` 21–4,
  `wolf in region`), a gift to the ford mend, bread/flour/oats to Rook, tending in Larkfield. Breaking a wave counts too.
  The points: a mend gift scores only for a piece of its stone, timber, limestone or cord (not coin), grain to Rook only
  through Bring the Harvest In (once a game day), tending once a wolf a phase, so nothing is farmed a loaf or a penny at a
  time.
- **Act III opens:** the setup trigger sets `nh_stage` 7 (the stage pack goes to `s78`), the realm key
  `nh_stage=harvest`, `self.mem.nh_keeper=on` on the nine Keepers (Oakhaven's by `nh_b_logger`), and re-applies the
  lasting holds on Desmond Hollis (`held`: the Keep's cells; `fled`: the Downs Road). Four clock triggers protect and spare
  exactly `ninth-howl.cleanup`'s 40 (`lasts: campaign`: Act III is its own campaign, so its first chapter holds them again;
  Desmond never).

## The siege
- Called on readiness (or Advance during the muster). The nights start at dusk the next game day (`siege_arm` on
  `day begins`, midnight); if that day would be autumn day 41–44 the siege waits (`wait_sheaf`, posted once) so its nights
  are the Last Sheaf's eve (45), the festival (46) and the night after; armed on 45 or 46 the texts name the Last Sheaf
  (`ch7_sheaf`).
- Waves at 21, 0 and 3 in the ford (`6_9`), the Longmead granary yard (`7_9`) and the Downs Road (`7_11`), each
  `min(6, 1 + wolves there)` by a `count in region` ladder, named camps `nh_ford`, `nh_granary`, `nh_downs` (L2, exact
  matches). A region is **breached** for the night if a wave still stands at the next wave hour (or at dawn), or nobody is
  there at a wave hour (`ch7_breached`, a list of regions). **A night holds** with at most one region breached, or two
  with relief that night (`ch7_relief`, also -1 between nights). Two nights held or more: `nh_b_ford=held`.
- **Relief, one wave each:** Grayrock's ships (`nh_b_toll=bargained` or `nh_b_grayrock=crown`: the granary's 3:00),
  Sloe's boats (`turned`: the ford's 21:00 on night 2), Bettany's Watch (`nh_b_lists=bettany`: the ford's 0:00), the
  smith's flash (a wolf on "Two Long and One Short" at the reed end at a wave hour: that ford wave), the Runner (the
  Downs Road's 21:00 on nights 2 and 3), Desmond's mercy (the granary's 21:00 on night 3), a broken truce (the Downs
  Road's 0:00 on night 3). `nh_b_lights=turned` halves the ford's waves; the mended bridge (`mend_built` fired) takes one.
  Post 8 (`larkfield` in `nh_posts`) is raised when the bridge is built with no wave standing at the ford, or else at the
  third dawn (campaign 5.2).
- **Foes (C10):** the Captain's Lieutenant (`nh-lieutenant`, fierce, band `min(6, active / 3)`) on the Downs Road on night
  3 unless the truce was broken; Desmond Hollis, if he fled, `foe.outlaw` on nights 1 and 3 (not the parley night),
  downed and never killed.
- **The parley** (night 2, 22:00): the Captain a mute stranger (`visitor.add captain`, `like: cty_stone_sunton`, Eskar's
  description) on the far bank, a truce-lamp ping, and the first wolf in the `truce` region hears him in a scene (mode
  first): through Desmond if he fled, else Rook reading his letter; four scenes (speaker × a key: `nh_ulric` at least 1,
  `nh_b_ledger` shown or kept, or `nh_b_seam=honest`). Offer terms (only with a key; without one the option is `bluff`, which he laughs at,
  counted `refused`), Refuse, or Strike under the truce (`broken`: `deed.revoke` of that wolf's "broke a wave at the
  Larkfield ford", the scar `truce_broken`, `nh_truce=broken`). Default `none`. The villain figure is set to Eskar of
  Deepvein for the Greenholt (`nh_road_villain`).
- **Desmond's hearing** (if `held`): announced at the call on the Ridgemere board and to the wolves; at the first 14:00
  at least six game hours (one real hour) after the call, always before the first night, a public scene outside the
  Magistrate's Court (Bettany and Desmond moved and held there), majority, default mercy. Mercy: he walks and stands in the granary yard on
  night 3 (A Granary Guard; done → `redeemed`). The pillory: Pillory Square for one game day, then back to the cells.
- **Branches at the third dawn:** held: `harvest.override grain good` 30, Larkfield +10, `nh_threat` −20,
  `festival.call larkfield "the Held Ford"` tomorrow, `nh_ford=held`. Burned: `grain poor` 30, `times.nudge larkfield
  −30` 14, the scar `ford_burned`, `nh_ford=burned`. At the close, held with the bar short: `grain fair` (the harvest
  came in thin).

## Quests (10 of 32; closure "side" on 4)
`nh7_hold` Three Nights at the Ford (story scope, one objective a night, marked for the wolves who mustered; a night lost
leaves its objective open, so the third dawn settles it: done if the ford held, failed if it burned);
`nh7_door` A Night at the Ford (the door quest: a journal offer on arrival and at the call; `camp cleared`, credit
present); `nh7_runner` The Night Runner (side; Aspen's `nh_as_run` node on night 1, a howl on the Southmeadow square then
the Saltwell square, each leg within a game hour by player timers); `nh7_bogpath` The Smith's Bog Path (side; giver the
Larkfield smith; three reed beds and the path's end); `nh7_flash` Two Long and One Short (the smith's `npc.offer`, laid
only with `nh_code_known`, the lamp code from Ch 3); `nh7_word` Rook's Word (side; giver Rook; the Downs Road, then Rowan's
`nh_rw_word` node); `nh7_harvest` Bring the Harvest In (giver Rook, daily); `nh7_granary` The Granary Door (fails on
`region emptied`); `nh7_guard` A Granary Guard (side; giver Desmond, `requires` mercy and night 3, one wolf); `nh7_price`
Siege Price (the Purse path, `requires p_backs=purse`: `p_marks`, `nh_bought`, `nh_back_purse` +1, up to 6 from the
Greatly Bank till under the counters, bread when thin). Givers: Rook 2, the smith 2 (quest and choice), Desmond 1;
Aspen's run and pledge are tree nodes.

## Scoring, voices, recognition
- One scoring routine (`score`, `tmp` the points): the wolf's realm `p_voice` and `p_ch7`, `ch7_points`, `nh_realm`, the
  town by the deed's cell, surplus past the goal, a new helper +6 to the goal; `share` adds the pledged quarter-share.
  Points: a mend piece 1, tending 2 (once a wolf a phase), Bring the Harvest In 3 (once a game day), a wave broken 15, a
  side story or the door quest 6, a foe 10; the bridge built 10. Goal 40, +6 a helper, at least 6 an active wolf.
- Relief convoys: an escorted wagon (`caravan arrives` at Larkfield, detail "from <market>, guarded") spends 5 surplus.
- Nine hourly Ember triggers (marks scaled by `(40 + active) / 40`), board posts signed by each Keeper, `nh_voice=raised`
  kept. Pledges (`nh_pledge` at any Keeper or fallback), backings (`nh_back`: a change moves the voice and costs 2) and
  Dagfinn's Question (`nh_dq_stand`/`nh_dq_step`: a change moves the voice), each once a chapter (`once: member`).
- The close: the herald's word on all eight boards (held / held on the Last Sheaf / burned), the ford's deed for every
  wolf who mustered or helped (the helpers joined to `ch7_did` first, so a wolf who stood the nights without a last blow
  has it), the roll on the Larkfield board with the count (C1: "{ch7_did_n} wolves mustered for the ford", "One wolf..."
  for one), then the hero figure for the best wolf
  (`nh_ford_hero`, the Greenholt) and the title "Ford-holder" (Larkfield, 120 days) for the best three by `p_ch7`, ranked
  into `ch7_helpers` once it has done its work (the program holds 64 variables, the most it may).
  The Runner: Southmeadow and Saltwell +10, `nh_runner=came` in both towns, Aspen's greeting by name.
- Talk keys written (each read by the library): `nh_stage=harvest`, `nh_ford=siege|held|burned`, `nh_captain=parley`,
  `nh_truce=broken`, `nh_runner=came`, `nh_desmond=mercy|pillory|redeemed`, `nh_voice=raised`, `nh_keeper=on`.
- `nh_pushed` gets `ch7` when the call came before readiness (the muster's own test at the call) or the close before the
  aftermath's six days (`ch7_served`).

## Rehearsals (10)
1, 5 and 200 active wolves; Advance anyway in the muster and again in the aftermath (both pushes noted; its effects
pinned); the soft maximum with `ch7_auto` off (not ready) and on (it moves; effects pinned); `ch7_wait_bar`; the held
road (five wolves, three nights, the parley refused, every side story); the burned road (one wolf, the truce broken);
mercy and the guard redeemed, terms offered, the Last Sheaf, Desmond downed; the pillory, a runner too slow, a relief
wagon, the wait for the Sheaf; a lone runner holding two nights with the relief. `expectChapters` `ninth-howl.8` where the
chapter closes. Rehearsals can't seed world variables, so the paths that read earlier chapters' branches (the relief
switches, the parley's key, Desmond held or fled) are reached by their events (a scene choice, a foe defeated) and
checked by reading.

## Checks
- `world-vars.json` gains `nh_code_known` (switch, off), as Ch 3 declares it; every other world variable was there.
- After the review: `nh_check` PASS (2 of 2, no warning) on a fresh throwaway copy migrated through 0081, and also on
  `ratw_ridgemere3` itself with the current `tools/nh_check.py` (read only); review 10 of 10, nothing uncovered, about
  1.9 s; `story_words` 0 problems offline and with the world's voices (as the dm role, read only).
- `python3 tools/nh_check.py Data/Storylines/ridgemere3/ninth-howl/ninth-howl.7.json` passes with no warning **on a
  throwaway copy of `ratw_ridgemere3` migrated through 0081** (`RATW_RIDGEMERE3_DBNAME=<copy>`): on `ratw_ridgemere3`
  itself `references()` refuses any program with realm variables until migration 0081 is applied there (going live,
  doc 86 Phase 8). Every chapter of Acts II and III that uses J2 needs the same.
- `story_words` (offline and with the world's voices, read only): 0 problems.

## Engine findings (not fixed here)
- `story_runner.references` reads every scene cast entry that is not a resident as a `selector` (`KeyError: 'selector'`
  for a stranger's entry, T3), so the parley scenes leave the mute Captain out of the cast.
- A campaign trigger cannot use `quest is` / `objective is` (the campaign scope has no `_questIndex`): those tests sit in
  phase triggers.
- `reference_problems` reads `objective completed`'s `item` (the objective) as an item id; the trigger tests it in `if`.
- `dm signal` is an exit event only, so the two admin switches are set from the Variables tab, or by a `story notified`
  of `nh7_auto` / `nh7_wait_bar`.
- `story_words.py --world` connects as the editor role, which cannot read `dm.story_residents`; the dm role can.

## Left, and why
- `contributors` of Three Nights is 1, not `min(3, helpers)` (a whole number only).
- No `p_sides` (nothing in this chapter reads it), no `nh_roll` or tier lists (J2's realm `p_voice` carries them).
- Rook's pair key `nh_rook=carrying` is not written: the library's `nh_rw_word` choice is gated on the stage only.
- The Envoy is not re-added in this chapter (her words come through Gorm's `npc.line` and his window line).
- A scar touching Larkfield does not raise its mark here.

## Review (2026-11-24)
Fixed in the program (generator copy `/tmp/nh7rv/gen.py`; it reproduces the reviewed file byte for byte before the fixes):
- **Farming.** Every gift to Rook scored 3 (a trigger on `gift` also fires on `gift again`), so a loaf at a time farmed
  `p_voice` (the Moot ballot), the town strands and the Ford-holder ranking; a penny at a time did the same through the
  mend (`project gift` takes coin); tending scored without limit. Now: Rook's points come with Bring the Harvest In (once
  a game day), the mend scores only for its four materials, tending once a wolf a phase. Rehearsal 7 shows 17 loaves
  scoring nothing.
- **The Last Sheaf.** The siege waited only while arming on 41–43, so its nights fell 44–46 (the festival the third night);
  the design's nights are the eve, the festival and the night after: it now waits while arming on 41–44 and names the
  Sheaf when armed on 45–46. The wait notice is posted once, not every day.
- **The hearing** played at the very 14:00 its board post went up ("this afternoon"), so no wolf could reach the court:
  it is announced at the call and held at the first 14:00 at least six game hours on (still before the first night).
- **Three Nights at the Ford** stayed active forever when a night was lost: the third dawn settles it (done if held,
  failed if burned); rehearsals 10 and 2 check it.
- **Recognition:** the ford's deed went only to wolves who scored (a 200-wolf crowd standing the nights mostly never lands
  a last blow); it now goes to everyone mustered or helping. The roll used bands with "hundreds" at 40: it now posts the
  count (a fill may name `ch7_did_n`; the "no `_` in a fill" note was wrong).
- **Post 8** is raised only with no wave standing at the ford (campaign 5.2), or at the third dawn; the bridge's wave
  relief reads `mend_built` instead.
- **Relief convoys** count only escorted wagons ("from <market>, guarded"), as the design says.
- **Words:** the letter's "It says nothing more. It says: With what?" (contradictory) and Desmond's "He says: He saves...",
  the Captain's terms naming Greatly again (design), the smith's reply ("Let them think it's one of their own"), Desmond's
  greeting (it read as the wolf speaking), Bettany's charge (the Carters' Yard and Greatly pennies, as Ch 2-3 have it),
  Rook's oath in his own dialect, the siege journal (the Sheaf wait).

Found outside this file (not changed here):
- **Ch 8** gates "A Visit to the Cells" and Desmond's memory on `nh_b_desmond = held`, but this chapter's hearing always
  turns `held` into `mercy` or `pillory` (then `redeemed`), so the Ch 8 quest can never open. After the pillory he is back
  in the cells: Ch 8 should read `pillory` (and `held`) as "in the cells", and re-apply that hold at its setup.
- **The library**, `resident.tree` line 508: "The dismissed guard stood the granary door all the third night. Mercy paid,
  for once." is gated on `nh_desmond=mercy`, which is set at the hearing, before the nights and whether or not anyone stood
  with him; it belongs under `nh_desmond=redeemed`.
- Ch 9 should read `nh_b_ford=burned` for the Captain's band +2 (no Ch 9 condition reads it today).

## Campaign check (2026-11-24)
- The Grey Captain's name at the parley is set on `nh_villain_coldwater`, `nh_villain_larkfield` and
  `nh_villain_frosthollow` (Ch 3's figures, which sort after Ch 2's `nh_villain` and are known where the residents are
  from or stand), not on `nh_road_villain` in `greenholt`, a region no resident is from or stands in by name.
- The ford's hero is `nh_hero_larkfield`, known in Larkfield (was `nh_ford_hero` in `greenholt`).
- New `desmond_again` (an hour into the muster): Act I lets Desmond Hollis go only when this act starts, so the setup's
  hold may come before it; the hold is laid again.
