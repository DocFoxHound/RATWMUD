# Large lines 5-8: notes beside the programs (wave E)

`lq-blight-and-boar.json`, `lq-runnel-fever.json`, `lq-wreckers-light.json` and `lq-long-run.json`, from
`artifacts/quests/design/quests-large.md` §1, §7-§10 under `decisions.md`. These are the today-only versions: they use only
what `Data/Storylines/trigger-catalogue.json` has today. Check them with
`python3 tools/nh_check.py Data/Storylines/ridgemere3/quests-large/lq-*.json`.

## Shape (all four)
- Version 2, open audience (cap 5000), `quietAfterHours: 0`, since these lines are slow on purpose.
- Chapter 1 has a giver and a board. Chapter 2 and the roads come as journal offers (`quest.offer` from the chapter before).
- The shared bar grows with the helpers (`goal` starts at its base, and each new helper adds k). When the bar is met,
  every helper is offered the roads and the reckoning opens.
- The reckoning comes 6 game days after the bar is met, or 3 game days after the first road is taken. The most weight
  wins: 1 for each wolf, and 1 more for a wolf in a party of 2 or more when the road is taken. A tie, or nobody choosing,
  goes to the line's default.
- **A second road is refused.** The wolf's own road is kept in the player text `road`, and a second road fails at once
  with a journal note. *Later:* L5 `quest.withdraw`, or C6 `requires`.
- **Coin** follows decisions §3.2: an If on `coin_<town>_ok` and on `coin_<town>_day` against the day cap, a grant with
  `ifRefused: skip`, an Add to the day counter, and the line's 35 a wolf (`lq_coin`). When the purse is thin the wolf gets
  2 bread, a plain journal note and `lq_thin`. No chapter pays more than 12.
- **Campaign triggers cannot read `quest is` or `objective is`**, because the campaign scope has no quest index. Player
  variables stand in for them: `road`, `step`, `on_trial`, `out_ok`, `owed`, and a `*_paid` switch for each first-time
  reward.
- **Window choices** (`npc.offer`) are laid by a `day begins` trigger every day, and by the first `quest started` of a
  visit. They cannot be entry effects. A road's own choice is laid only while that road is under way, which a story
  counter tracks (`curing`, `soaking`, `drafting`, `facing`, `tiding`, `answering`). The choices are untargeted, so every
  wolf of the story sees them. *Later:* J1 per-wolf offers.
- **A choice that is the ask is the objective.** Where a target holds this line's own choice for the very thing the
  journal asks (the beekeeper's rows, the swineherd's fence, the washhouse water, the honest apothecary's test, the clerk's
  ledger, the neighbour on the stair, the time-sheet and the Council's clerk), the objective is that `dialogue choice`,
  not a `conversation`: picking the choice ticks it. The choices of weekly chapters are not `once`, so a repeat (or a
  wolf who picked one before its turn) can pick them again. Targets with no choice of their own (Harl, the tanner,
  Corwin, Bettany, Solveig) are still `conversation` objectives: the wolf says something to them.
- **Words for the conversation targets.** Their window lines become phase `npc.line`s where the target is not campaign
  cast: Harl (blight), Liv (runnel), Corwin and Ilse (wreckers), Hanne and Orm (long run). Bettany and Solveig are
  campaign cast, so they get no `npc.line`. Their words are in the window lines below and in the scene.
- **No top-level `prefix` and no `closure`.** Today's validator refuses both. *Later (J3, Phase 4):* a new version adds
  `"closure": "line"` to `vines_burn` and `vines_cure`, `runnel_cover` and `runnel_soak` (not `runnel_draught`),
  `wreck_tide`, `wreck_watch` and `wreck_wharf`, and `answer` (not `answer_again`).
- **Two phase changes are delayed by a timer** so that the quest completions of the same moment are counted first. The
  Blight bar check comes 0.01 day after the night watch, and the cover's result 0.01 day after `project built`. A phase
  change in the same round would drop the queued facts. For the same reason, the Long Run's finish comes 0.002 day after
  the Saltwell leg.

## Blight and Boar (`lq-blight-and-boar`, `town:southmeadow`, day cap 23)
- Phases: `open`, `reckoning`, then `after_burned` (the default), `after_cured` or `after_lost`. On `season begins` the
  after phase goes back to `open` with a fresh bar. After burning, it also sets `harvest.override fruit good 60`.
- Bar: `rows_kept` (+1 a wolf standing in the rows at 1:00, +2 a boar chapter done) against `goal`, 8 + 2 a helper.
- **`hunt` (two boar) is optional today.** No effect can stage a boar, and `hunted boar` waits on the wild (L1,
  Phase 5). *Later:* `hunt.stage` boar in `rows` at dusk, and the objective becomes required.
- The design's `ash2` objective is dropped, because a `gift` objective already counts `gift again`.
- The cure is a gamble: 60 in 100, and 10 better for every 5 wolves who *finished* it (`cure_hands`, up to 90). A cure
  wolf who finished before the reckoning waits on `curers` and is paid wine or grapes once the result is known.
- `harvest.override` names the area `fruit`, which is the realm's one fruit area (orchards and vines). That matches the
  design.
- A cure wolf who finishes is paid by the phase the line is in (`after_cured` wine, `after_lost` or `after_burned`
  grapes and the "lost" deed), never by last season's `lq_vines`; one who finishes before the reckoning waits on
  `curers`, and burning pays them too, so the list is always emptied.
- A wolf who first keeps the rows during the reckoning is offered both roads at once.
- From Ch 7 (`nh_stage >= 7`), the Southmeadow board carries a one-day picking notice, posted again each day.

## The Runnel Fever (`lq-runnel-fever`, `town:ridgemere`, day cap 135)
- Phases: `open`, `open_cough`, `reckoning`, `building`, `after_covered` (the default) and `after_drained`.
  - The after phases are lasting: the cover stands, and the culvert is dug. Chapters 1 and 2 stay on offer there.
- **`open_cough`** is the Residence variant of the nurse's line (campaign `nh_b_cough`). A `day begins` trigger moves
  `open` there while `nh_stage >= 4` and `nh_b_cough` is off, and moves it back otherwise. A DM can move it by hand with
  the signal `residence`, which is also how the rehearsal reaches it. A chapter's close adds a journal note about the
  Residence too.
- Bar: `cured` (+1 a remedy to a coughing household the wolf has not helped yet, from the 12 of `lq_sick`; +2 a Bad
  Water chapter done) against 10 + 2 a helper.
- The cover:
  - `project.post` (kind `cover`, Cistern Court (14, 22), title "Cistern Court cover", `ifRefused: skip`) runs when the
    first road taken is the cover, or at the reckoning if the cover wins.
  - `cover_posted` turns on only when its `project posted` event arrives. Until then a `day begins` trigger tries again.
  - The city's open-project cap is two, and proj-3 is open, so the cover never pushes a campaign project out.
  - The quest's `carry` objective matches the project's title, because its id is not known until it is posted.
- Bad Water's draught must be *bought* at Hollis Physic (`trade done`, detail `bought`); selling him something does not
  count.
- The cover road and the draught money can still be taken in `building`, so late wolves can carry for the cover.
- If the soakaway wins after a cover wolf asked for the cover, the city is still asked for it each day (and builds it
  when it can), so that wolf's road can be finished. The result stays `drained`.
- The draught money is 4 pennies a wolf, from the treasury ("Hollis Physic pays the coughing houses back"), with 40 in
  all (`restitution`). *Later (SQ2):* the purse becomes `till:job:rm_rupert_fogton`.
- *Later (C8):* the npc key `lq_rupert=paid` turns his window line sullen (line below).

## The Wreckers' Light (`lq-wreckers-light`, `town:ridgemere`)
- Phases: `open`, `reckoning`, then `after_named` (the default), `after_watch` or `after_wharf`.
  - `after_named` and `after_wharf` are lasting: the false lamp is gone for good. Each keeps a small `season begins`
    trigger so the run stays open and late road wolves are still paid.
  - `after_watch` goes back to `open` on the next season.
- Bar: `lamps_taken` (+1 a lamp brought to Linden, +2 a dusk round followed twice) against 5 + 1 a helper. A lone wolf
  needs the oil chapter's weekly repeat. Three wolves who each do chapters 1 and 2 meet it.
- Chapter 1 (the old medium `mq-false-light`):
  - Fog (`weather.set`, 6 hours, `ifRefused: skip`; a hosted world needs a Dungeon Master) comes at most once a game day.
  - After the bearing (`step` is `bearing`), entering `rocks` or `headland` after dark places the lamp at that region's
    spot. It is placed only if no lamp lies there, so two parties can each find one. The wolf gets a map marker, and the
    `wait` timer starts.
  - A wolf who found the lamp but saw another wolf take it finds a fresh one on coming back to that spot after dark.
  - Picking the lamp up before any gang came is the **doused** road.
  - If the wolf waits a game hour beside it, the `hour` trigger sends `bandits.call` (2, 4 or 6, by party size) and
    reveals `wreckers` (`lq_lamp1` = `waited`). Bringing two down completes `end` and makes it `caught`, which gives the
    "caught wreckers" deed. A wolf who takes the lamp before the gang is down still ends the night (no wolf is stuck),
    with the "took a wrecker's lamp" deed.
  - The design's "every hour in storm or rain" is kept only for the wreck night. A placed lamp stays until it is taken.
- The wreck night comes once a season, at the third lamp: `ship.storm` on one of s21-s25 (by `random`, `ifRefused: skip`),
  a 12-hour storm on the city, gangs in both coves, and the lamp burning every hour for half a day. *Later (C1):* storm
  the next posted ship.
- The cove gangs come on `ship stormbound`, to the shed and the north beach in turn (`wave_turn`). They are matched by
  cell plus `spawned_cove` or `spawned_cove_b`, never by a camp id. A gang comes only where none of the line's gangs stands yet (the switch is off), so storms
  never pile camps up in the cove. The design's "when the DM signals `wreck`" becomes
  **Fire a trigger → `stormbound`** in the Campaigns console, because a trigger cannot listen for `dm signal`.
- The `mole` region is dropped. The dusk round is `near resident` on Ilse between 17:00 and 22:00, once a game day.
- `wreck_tide`: her choice `lq_wr_tide` makes her follow the wolf (`npc.follow`). When wolf and lamplighter are both in
  the Chapel of the Tide, `near resident` (or, if she is already inside, `enter place` at the chapel door) completes the road and plays the public scene once: `tide_naming`, or
  `tide_naming_wharf` when `nh_b_wharf` is on.
- `after_watch`: she is moved to the Keep's cells, `r2_the_keep_down` (11, 10), and held, then released by a 7-day timer.
  *Later (L4):* `npc.sentence`, which outlives the run.
- `wreck_wharf` gives `standing.adjust house_grayrock +3` (`ifRefused: skip`), which needs a builder or the owner to
  approve the story. *Later (C14):* `npc.post none` really turns her out of the Lamp Store.
- No payment goes through the Lamp Store's till.

## The Long Run (`lq-long-run`, `town:ridgemere`)
- Phases: `open`, `open_howl` (from Ch 4: Grete's line about the howl's road), `race`, `after_couriers` and
  `after_wagons`. Each `season begins` goes back to `open` (or to `open_howl`) for a fresh run. A DM can move `open` to
  `open_howl` with the signal `howl`.
- When the run is posted: when `count runners >= 3`, or when there is 1 runner and 14 game days have passed since the
  season's list opened (`list_t`), a `day begins` trigger posts the run on all eight boards.
  - Mirebridge has no board.
  - The 14 days are counted from the season's first runner, not from the phase's opening. `nh_check.py` reads a
    condition's `from` key as a resident (a false positive), so `days since` could not be used.
- The start is the 6:00 `hour` on Marketday (`calendar is`). A game minute later, every runner on the list standing in the
  Oakhaven square gets a `document_tube` and joins the run.
- The runners on the list are told by journal note when the run is set, and where to be at six.
- A leg is run when a wolf of the story carrying a tube enters the next square in order (`leg_at`), **having stood in the
  last square after the tube reached it** (player number `at_sq`: set at the start for the runners in the Oakhaven
  square, for the carrier and every wolf of the story standing in a square as a leg is run, and for a wolf who comes into
  the square after the tube). So the tube really goes paw to paw: a wolf waiting ahead with their own trial tube cannot
  score a leg the tube never reached them for. *Later (J9):* the tagged tube makes the item itself count.
  - Each leg is a plain `story.tell` to the world and adds the town's voice: 2, 3 or 4 by distance; the Oakhaven start
    adds 2.
  - On the way, bandits may come: if `nh_threat > 40` while the campaign runs, or by a 30 in 100 chance before it starts.
  - A wolf must be in the story (have taken a trial leg) to score a leg. The tube can be handed on by a gift.
  - *Later:* J9 a tagged tube (today any `document_tube` counts); C2 `contributors` from the active wolves (today 1).
- The finish:
  - Saltwell inside a game day: the Couriers win, in three bands. Inside half a day is `half`, inside 0.9 is `day`, and
    up to 1.0 is `last` ("by the last grain of the day's sand"). The best band is kept in `lq_post_time`.
  - Saltwell later than that, or nobody by 2 game days: the wagons win.
  - The wagon's narration always comes at 1.5 game days.
  - A wolf who ran all eight legs inside 2 game days gets the Lone Run deed, whichever side won.
- Paid at the finish, whoever won: each wolf with a leg this season gets 6 pennies, the deed, paw boots in their first
  season, and `nh_realm` +8.
  - The answer is then offered: `answer` (12 pennies, once), or `answer_again` (the practice only) for a wolf who has
    answered before. Both are two window choices: the timekeeper's "I've come for the time-sheet." and the clerk's
    "Here is the Long Run's time-sheet.", laid while an answer is under way.
- A wolf who takes the trial leg inside the Couriers' Office has the tube there and then, and the sand-glass turns; one
  who takes it from the board collects it on coming into the Office.
- *Later:* C1 the run's own time in the board text; C3/C4 "the Long Runners" title.

## World variables
`lq_vines`, `lq_runnel`, `lq_wreck`, `lq_post` and `lq_post_time` are added to `ninth-howl/world-vars.json` (group
"large lines 5-8"), so the DM declares them with the rest. These programs also read or add, with the same names and
types: `coin_southmeadow_*`, `coin_ridgemere_*`, `nh_realm`, `nh_voice_*`, `nh_stage`, `nh_threat` (wreckers −1 a gang
met or a cove camp cleared), `nh_b_cough` and `nh_b_wharf`. They never write a branch, `nh_stage` or a post.

## Rehearsals (19)
| Line | Cases |
|---|---|
| Blight | burned, played through with the season's turn; cured (roll pinned); lost, with a late cure wolf; nobody chose; a tie and a second road refused; a party of four on the store's day cap (the fourth paid in bread) |
| Runnel | covered, with the city building it; drained, with the draught money and a second road refused; nobody chose (the cover asked for every day); the DM's Residence move and its move back |
| Wreckers | three wolves, doused, to the Tide and the scene; the gang waited for, the wreck night, the Watch wins, the cells, release, the season; the wharf, with ships stormbound in both coves; nobody chose |
| Long Run | a lone runner, half a day, the answer and the next season; three runners stopping at Coldwater, the wagon and the wagons; the DM's howl, a slow trial retaken, a lone run too slow; the "inside a day" band; the "last grain" band with a second season's `answer_again` |

Every phase and every trigger is reached. The line's 35 cannot be reached in a rehearsal: only `nh-purses` sets the day
counters back to 0, so the day cap turns a wolf away first, which the Blight party case shows.

## Talk: what the DM seeds, and the window lines for wave G
The gossip and the inn recaps of these lines are already in `resident.tree` and `inn.tree`. Until C8 `talk.remember`, the
DM seeds the realm talk key when a line moves:
- `lq_vines=blight` at open, `reckoning` at the reckoning, then `burned`, `cured` or `lost`;
- `lq_runnel=fever`, then `covered` or `drained`;
- `lq_wreck=lamp`, then `named`, `watch` or `wharf`;
- `lq_post=running` on run day, then `couriers` or `wagons`.

The targets' own window lines are not in the trees yet. They are written out here for `resident.tree` `hub` (Bettany's
for `watch.tree` `hub`), as quests-large.md gives them. Today the phase `npc.line`s carry the same words where allowed.
```
say[self.id=tn_oh_harl_oakbough world.mem.lq_vines=blight priority=4] Down from the Southwood, along the stream, under the old fence. I was lost three days in that wood once. I know its paths. Good harvest if you find the gap.
say[self.id=tn_oh_harl_oakbough world.mem.lq_vines=reckoning priority=4] Where I was raised they paint a sick root with oak bark and lime, and pray to the Bright One it takes. Three of bark and two of lime, and I'll do the painting.
say[self.id=rm_liv_slagstack world.mem.lq_runnel=fever priority=4] Thyme syrup? Bring me herbs and I'll brew it the old way, with honey, not sugar. A spoon at night and one at dawn. I cough in the damp myself.
say[self.id=rm_theodora_wickley world.mem.nh_stage=howls|letters|lights|harvest|bell|moot priority=4] Boil the Runnel water. And they say the cough's up at the Residence too. Damp doesn't care whose lungs it gets.
say[self.id=rm_rupert_fogton world.mem.lq_rupert=paid priority=4] Come to count my bottles again? Count away. Every penny went back to the courts, and I'll thank you not to mention it.
say[self.id=rm_corwin_bilgestack world.mem.lq_wreck=lamp priority=4] Full measure for each of us, and one draws a jar more on blowy days. Says the wind snuffs her wicks. I've said nothing, and I'd thank you not to say I did.
say[self.id=rm_bettany_nailley world.mem.lq_wreck=watch priority=4] A wrecker of Grayrock ships, and I'm the one who locks her up. The Bright One has a sense of humour. Thank you. I mean it.
```
Harl's first line and Corwin's are trimmed here to the library's 160 characters; the phase `npc.line`s keep the longer
words (200 allowed). The Theodora line needs `nh_b_cough` off, which a tree gate cannot read today: wave G
pairs it with a key the campaign writes, or leaves it to the phase line.
Built 2026-11-22 (`artifacts/quests/build/talk2/build_packs.py`, `NOTES`): these lines are in the trees now, spoken
by the re-cast residents below. The Theodora line is Halvard Ashley's, gated `world.mem.nh_cough!=tended`, and the
Corwin line is Dorcas Rainford's.

## The givers (givers.tsv)
- Every giver and every `npc.offer` holder has its `large:<line>` row: Sloe, Aster and Brisk; Theodora, Ivo Fogstack,
  Dunstan, Rupert and Halvard; Linden, Ilse, Ivo Cinderham and Septimus; Grete, Florence (two choices: the time and the time-sheet), Hanne, Orm Carlisle and Philippa.
- Targets only: Harl, Liv and the twelve households (`lq_sick`); Corwin (he gives `sq-dawn-bell`, so here he is a
  conversation target only), Bettany and Solveig (campaign cast, as decisions §1.2.2 allows).
- `nh_check.py` printed every speaker's `live.npc_voice`, and they match the design. Accent `none` means plain speech: Aster,
  Brisk, Theodora, Corwin, Solveig, Florence, Orm and Philippa speak plain.

## Givers behind locked doors (2026-11-22, second pass; doc 86 "Givers behind locked doors")
A door into a private interior is locked to wolves (doc 82, `World::playerDoorLocked`), and no story effect lifts it.
This pass re-cast the residents a wolf could meet only on their walks, and moved every step into a locked interior to
its door. `givers.tsv` rows moved; the window lines in the talk library follow (`build_packs.py`). The list above, of
givers and targets as first written, is superseded by this one.
- **The Runnel Fever.** The nurse (Sump Infirmary, locked) no longer gives it: Halvard Ashley, the honest apothecary at
  the Mortar and Pestle (7-19, plain), who held the line's "what's in it" choice already, gives `cough`, takes the
  willow bark for the infirmary and says the nurse's phase lines. The board notices are still the infirmary's. The
  Washhouse keeper's choice (`lq_run_wash`, locked) is now Dirk Langstead's, a rag-crier in the Rag Market (out of doors
  7-18, plain), who washes her own rags in the Runnel.
- **The Wreckers' Light.** Giver: Greta Gullton, who works the Wharf's cranes (6-18, plain), for the North Cove skipper
  (in his boat shed). The bearing is taken at the Bay Light's foot (`bay_foot`), and the oil is found at the Lamp
  Store's door (`ls_door`). "Who draws extra" is asked of Dorcas Rainford at the Lamp and Wick, who sells the Lamp Store
  its oil (she gives `mq-sump-lamps`; here she is a conversation target only), and her window line is hers now. The
  ledger choice and the Wharf road's lamp are Florence Millhand's, who unloads ships at the piers (6-18), for the
  Wharf Office clerk. The dusk round is watched at the Lamp Store door, 17-22, on two evenings (the lamplighters are
  inside it all evening). In the reckoning Ilse waits on her Salt Row stair (`npc.move` + `npc.hold` at the phase's
  entry, `npc.release` when it ends in `after_named` or `after_wharf`; `after_watch` takes her to the cells as before;
  the run's end lets her go). The Tide road ends at the Chapel of the Tide's door (`tide_door`): the priest answers in
  a scene (`wr_priest`), and the naming scenes play at the door, with Ilse beside the wolf. The Watch road ends at the
  Keep door (`keep_door`) carrying the lamp: the sergeant answers in a scene (`wr_sergeant`), and the wolf keeps the
  lamp, as Q2a keeps its tally (a story can't take goods today). The two door triggers live in the reckoning phase,
  where their quests are.
- **The Long Run.** The tube is collected and brought back at the Couriers' Office door (`co_door`); `trial-start`
  (for a wolf inside the Office) is gone. The Council's clerk (Administrative Wing, locked) is now the Council Press's
  printer, Wynn Fogworth (8-18, plain), who sets the Council's notices. The couriers stay. The timekeeper (Florence) and
  Hanne spend their evenings at the Grey Gull, an inn whose door never locks. Grete spends hers at the Grey Goose, which
  is open only in its cook's hours (7-19): she can be found there from the end of work until 19, and all day on her days
  off. The trial is taken from the board too, but it is handed in to her. The time-sheet labels no longer send the wolf
  into the Couriers' Office by day.
- **Blight and Boar: left open by this pass, closed by the third (below).** Sloe, Aster, Harl and Brisk all work in the
  Southmeadow press house, hive shed and Southwood pig yard, and Southmeadow has no free resident a wolf can reach. Doc
  86 lists what would close it.

## Purses
At the 2026-11-23 checkpoint, the treasury held 1,358 pennies and Southmeadow's store 236. The design's day caps (135 and
23) are kept as written.

## Doors a story unlocks (2026-11-22, third pass; doc 86 "Doors a story unlocks")
A door a live story has unlocked (`door.unlock`) is open to wolves while the story holds it (DECISION of 2026-11-22,
engine on branch `r3-nh-doors`; doc 86 "Doors a story unlocks"). A story never unlocks a home, and `tools/nh_check.py`
refuses a door into one, or into a room whose stairs reach one.

- **Blight and Boar**, phase `open`, entry: the Southmeadow Press House (`link_fs_vineyard_south_press_door_a`, Sloe
  Harvestby and Harl Oakbough), the Sunfields Hive Shed (`link_fs_bees_south_hives_door_a`, Aster Cornley) and the
  Southwood Pig Yard (`link_fs_pigs_south_sties_door_a`, Brisk Lakemere). Sloe's home, the Southmeadow Vineyard House,
  stays shut.
- No `door.lock` closes the window: the map locks no door, and `Door::locked` stops residents too, so locking a shed
  when its phase ends would shut its own workers out until the run ends. The doors stay unlocked from the phase that
  offers the quest until the run ends, when the game puts back what each had.
