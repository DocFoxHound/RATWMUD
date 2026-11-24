# Large lines 1-4: notes beside the programs (wave E)

`lq-salt-road.json`, `lq-mast-cedars.json`, `lq-old-brown.json` and `lq-clipped-silver.json`, from
`artifacts/quests/design/quests-large.md` §1-6 under `decisions.md`. Today-only versions: only what is in
`Data/Storylines/trigger-catalogue.json` today. Check: `python3 tools/nh_check.py Data/Storylines/ridgemere3/quests-large/lq-*.json`.

## Shape (all four)
- Version 2, open audience (cap 5000). Phases `open` (chapters 1-2), `reckoning` (chapter 3's roads), one `after_<road>`
  per result; `season begins` in an `after_*` phase goes back to `open` (fresh goal, tallies, helpers).
- Chapter 1 has a giver and a board; chapter 2 and the roads are journal offers. Chapter 2 is offered again: Salt's
  ride at each wagon (repeat `daily`, so a rider can ride every wagon: `weekly` counts 7 days from the hand-in and ran out
  a few game hours after the next wagon had loaded); the others weekly, offered each `day begins` to wolves who have done
  it once.
- The shared goal grows with helpers. Reaching it opens the reckoning, with a 7-day fallback tally. The first road taken
  sets the tally to 3 game days. The most weight wins (1 for each wolf, 2 from a pack). A tie or nobody goes to the
  default: Salt `board`, Cedars `contract` (nobody: `taken`), Brown `spared` (`slain` only if a bear was really killed),
  Silver `board`.
- Every coin follows decisions §3.2 (`coin_<town>_ok`, `coin_<town>_day`, `ifRefused: skip`) and the line's 35 a wolf
  (`lq_coin`). A wolf turned away is paid 2 bread, gets a journal note, and `lq_thin` is set for them. Payments a shop
  makes ("Vane & Daughters pays it back", "Grayrock Shipwrights pays...", "Silver & Rain pays it back", the Counting House
  chit, the silver shares) come from `town:ridgemere` until SQ2.
- Story waves are matched by cell plus a `spawned_<region>` story switch, never `camp_w_*`. Two regions take turns
  (`in_turn`), and a wave is 2-6 bandits depending on how many wolves are present.
- A wave comes only for a wolf on the chapter, never for a passer-by, so nobody farms bands or `nh_threat`: Salt, a wolf
  of Brackish Wells not yet credited with a broken band (`raiders_done`); Cedars and Brown, a sitting party's wolf in that
  stand or fold at the hour (`sitters`); Silver, each trailing wolf once a trail (`called`).
- A lone wolf's wave is 2 bandits, so every "bring down" objective has goal 2 (Cedars `crew`, Brown `rustlers`, Silver
  `guards`). The bandit falls event credits only the wolf who landed the last blow; party quests share it.
- Window choices (`npc.offer`) cannot be entry effects. A new visit lays them through a `lay` timer scheduled just before
  each `enter` (campaign scope), and the quest-start triggers lay them too.

## Things to know
- **No top-level `prefix` and no `closure`.** Today's validator refuses both: the quest fields allowed are summary ... bonus.
  **Later (J3, Phase 4):** a new version adds `"closure": "line"` to the roads: Salt `salt_board`/`salt_bought`/`salt_cart`,
  Cedars `cedar_contract`/`cedar_return` (not `cedar_debt`), Brown `brown_hunt`/`brown_spare`, Silver
  `silver_board`/`silver_mercy`/`silver_cut`.
- **Campaign triggers cannot read `quest is` or `objective is`.** The campaign scope has no quest index, so validate
  refuses them there. Player switches stand in for them: `on_brackish`, `riding`, `drove_off`, `ringbarking`, `felling`,
  `sitting`, `trailing`, `on_light`, `saw_beam`, `asked_where`, `keeping`. Party quests set their switch for each member
  at the start, with `each` over the party.
- A second road is refused at once with `quest.fail` and a journal note. The other roads stay in the journal as offers.
  **Later:** L5 `quest.withdraw`, or C6 `requires`.
- Road choices are untargeted, so every wolf of the story sees them. Their triggers check the wolf's own switches and
  road. **Later:** J1 per-wolf offers.
- `each` over participants covers 500 wolves at most (the engine's limit). Beyond that, the hour, wagon and Marketday
  loops miss some wolves.
- **The world variables** `lq_salt`, `lq_salt_sacks`, `lq_cedars`, `lq_brown`, `lq_rustlers` and `lq_silver` are added to
  `ninth-howl/world-vars.json` (group "large lines 1-4"), so the DM declares them with the rest. The `lq_*` text goes
  back to its open-state value (`raided`, `dying`, `rustled`, `clipping`) each new season. That matches medium Q18,
  which offers its wagon only while `lq_salt` is '' or `raided`.
- `lq_beam` is a player text (each wolf's own word on the beam), not a world variable.
- **The rehearsals** (13 in all) cover every phase, road and trigger: a tie, nobody choosing (the fallback tally), a second
  road refused, a thin purse, a new season, a pack's weight, Dirk's death, and the Marketday shares.
  - The line's 35 cannot be reached in a rehearsal of one program. Only `nh-purses` sets the day counters back to 0, so
    the day cap turns a wolf away first.
- **False warnings in the Problems list** (`reference_problems`): "No item `raiders`/`face`/`tn_lf_pike_chalkley`...".
  Those filters are the `item` of `objective completed` (the objective's id) and of `dialogue choice` (the resident),
  which is what the game writes (`Game::storyQuestEvent`). They do match.

## Talk (wave G) and the DM's hand until C8
Until C8 `talk.remember`, the DM seeds the realm talk key when the line moves: `lq_salt=raided|board|bought|cart`,
`lq_cedars=dying|reckoning|contract|returned|taken`, `lq_brown=rustled|slain|spared` and `lq_rustlers=grey`,
`lq_silver=clipping|board|mercy|cut` and `lq_beam=board`. The gossip and the inn recaps are in the trees already. The
targets' window lines (`say[self.id=...]` in `resident.tree` `hub`) are written out in quests-large.md:
- Salt §3: Kite Bogditch, Fern Merewick and Rosamund Coalford (two lines);
- Cedars §4: Frost Colddrift (two lines);
- Brown §5: Stone Acorndell;
- Silver §6: Imogen Rustford, Fenwick Mossdale (two lines) and Vigdis Rainmore (`self.mem.lq_vigdis=freed`).

The conversation objectives complete without those lines; the lines carry the clue.

## Line by line: today, and what comes later
**The Salt Road**
- Brackish Wells' raiders are driven off when their band is broken (`camp cleared`, cell plus switch): every wolf of the
  chapter there (in either flats region, or the one who broke it) is credited, given the raiders' tally (`paper`) and
  told of the grey paw-mark. A wolf not yet credited calls a new band on any later night. (Counting a wolf's own last
  blows could not be done alone: a lone wolf's band is 2, the objective wanted 3.)
- The wagon loads 3 sacks and one more for each rider at the yard after the first, so the goal (6, and 1 a rider) stays
  in reach of a crowd. A lone rider needs 3 good wagons, two or three riders together 2.
- The day before wagon day the Saltwell board says so (`wagon-eve`); a wolf who finishes Brackish Wells is told the
  wagon's hour and week in the journal.
- The wagon:
  - runs at the eighth hour, a game day after the first raiders broke, then every 7 game days; the DM can send `wagon`;
  - Orm is protected for each run and the Marketday walk, and unprotected at the end or when his walk is blocked;
  - **if the DM stops the run mid-wagon, unprotect `cty_orm_sootgate` by hand.**
- `prices.reset` needs a builder, so the board road uses `times.nudge ridgemere -20, 14 days` plus
  `event.cancel salt_short`. A builder may approve a version with `prices.reset`.
- The board copy of Ride With the Salt is left out: the ride is offered to each wolf who drove the raiders off, at every
  wagon.
- Later:
  - C10 a named "Brine Captain";
  - L2 camps `lq_salt_flats`/`lq_salt_flats_b`;
  - L3 `camp cleared` objectives;
  - SQ2 Osric's till for the restitution and the bribe;
  - N2 Orm's thin-purse thanks;
  - C12a credit in the running chapter.

**The Mast Cedars**
- Ivy's "both" `npc.line` when `nh_b_hills = fell` is left out. An entry line cannot depend on a variable today, and a
  second open phase could not be rehearsed (a case cannot set a world variable). **Later:** T5 or C1.
- `cedar_debt` frees Senna in talk. **Later:** the npc key `lq_senna=free` (C8).
- Senna's Debt is offered to a wolf who heard her when their Night Fellers ends in the reckoning too (before, only the
  `lay` at the reckoning's start offered it). Its design "+1 weight to whichever road wins" is left out: it changes no
  result.
- Ivy's first words say five cedars are gone, not seven, so nobody reads the Seven Sisters as already felled; and her
  `open` line says "the best cedars" (plain), not oakhaven's "good bark".
- Later: C5, Ivy thanking a wolf by name; SQ2 for the shipwrights' till.

**The Old Brown**
- The bear objective waits on the wild: bears are rare. `slain` needs a real bear killed by a wolf of the story.
  **Later:** L1 `hunt.stage` at `greytarn`.
- Wool on the Rocks' hidden step "see where the forty were driven" is the old camp site on the Frosthollow road
  (`pass_camp`, 8_6), up the Pass Road. It was the Upper hearths, where the collier already stands, so it never fired
  until the wolf left and came back.
- The rustlers come at the second hour to a fold that is kept, once any party has kept a first night quietly
  (`fold_ready`), and only when a wolf of a sitting party is in that part of the fold. The design's "the second night of each party" is simplified to this.
- From Ch 3 (`nh_stage >= 3`) the cloak objective is "the Captain's colour" (two hidden objectives, one revealed).
- Lorn Hedgeton is never named or given anything: the campaign owns his story.
- Later: C10, the rustlers' drover.

**Clipped Silver**
- Chapter 1 is the old medium Q11.
  - The beam choice sits on Pike.
  - Hushed: the 6-penny chit comes from the treasury (the Greatly Bank till after SQ2), with a deed. No `nh_bought`
    (C12a later).
  - Pike's thanks is the Board one for everyone. **Later:** N2, the hushed variant.
- If Dirk dies, each trailing wolf's `dirk` objective is completed with a journal note. **Later:** C11 spares him.
- The design's weekly re-made dies (Stoneday turning both `spawned_kestrel` switches off) is left out: the switches are
  off once a band is broken anyway, and turning them off under a band still standing left that band unmatched, so no
  shears fell when it was broken. The guards come back for each new trail instead.
- On a wolf's first trail, each of the party besides the one who picked up the shears gets a pair (`trail-shears`), so
  every wolf of a pack can take the Board or the mercy road. Any shears match the gift (the item filter is the base item).
- The Board road plays the embedded public scene `lq_silver_die` on the Larkfield square: Pike and Harrow, each in their
  own panel.
- Later:
  - C5, Vigdis thanking a mercy wolf by name;
  - C14, Vigdis's fate;
  - SQ2, Silver & Rain's till for the restitution and the shares.

## Givers behind locked doors (2026-11-22, second pass; doc 86 "Givers behind locked doors")
- **The Old Brown: left open by this pass, closed by the third (below).** Jessamy Greyford (giver), Vale Pikebridge and
  Juniper Larkcombe (choices and targets) all work in the Greyteeth shearing barn, locked to wolves (doc 82). Coldwater
  has two free residents left that a wolf can reach at work (Wren Coldpeak, Stone Pinefell), not enough for the line's
  three, and half a line re-cast is worse than none. Doc 86 lists what would close it.
- The Salt Road's Fern Merewick and Kite Bogditch (conversation targets) work in the Saltpan salt store, also locked;
  Saltwell has no free resident. Its optional "ledger" steps ask for archive work, which Ridgemere 3 gives nowhere
  (no records post), as does the Clipped Silver's optional chalk-list.

## Doors a story unlocks (2026-11-22, third pass; doc 86 "Doors a story unlocks")
A door a live story has unlocked (`door.unlock`) is open to wolves while the story holds it (DECISION of 2026-11-22,
engine on branch `r3-nh-doors`; doc 86 "Doors a story unlocks"). A story never unlocks a home, and `tools/nh_check.py`
refuses a door into one, or into a room whose stairs reach one.

- **The Old Brown**, phase `open`, entry: the Greyteeth Shearing Barn (`link_fs_sheep_foothills_barn_door_a`). Jessamy
  Greyford, Vale Pikebridge and Juniper Larkcombe are found at work.
- **The Salt Road**, phase `open`, entry: the Saltpan Salt Store (`link_fs_salt_saltpan_store_door_a`) for Fern Merewick
  and Kite Bogditch (`brackish`). Phase `reckoning`, entry: the Records Office (`link_r2_the_records_office_door_a`, no
  home up its stairs). The optional, hidden `ledger` step of the three road quests is now a conversation with the
  Records Office copyist Hollis Shaw (`rmx_hollis_shaw`, 7-18 up the stairs, or the Grey Goose after work until its cook
  locks up at 19), who copies the provisioner's buying out of the rolls. Trigger `ledger-read` adds a journal note in
  the copyist's plain Ridgemere speech: three times last year's Saltpan salt at Vane and Daughters, with duty paid on a
  third of it.
- **The Clipped Silver**, phase `open`, entry: the Records Office. The optional `chalk` step of `trail` is now a
  conversation with the Records clerk who keeps the old ledgers, Harriet Pennock (`rmx_harriet_pennock`, up the stairs
  by day, the Grey Gull every evening). Trigger `chalk-read` notes, in her plain speech, that Silver and Rain is
  chalked, owing more than the shop takes in a year, due at the new moon (the design's "shows she owes").
- The rehearsals that did archive work now talk to the clerk after taking the quest. Harriet is a target in two quests
  (this and `mq-coopers-note`), Hollis in this line only; neither gives or holds a choice.
- There is no window to close before the run ends. A quest without `phases` is open in every phase of its story
  (`story_quests.open_in`), and each door here serves one, so each stays unlocked until the run ends, when the game puts
  back what each had. (`door.lock` would lock the workers out too. `door.close` closes a door to wolves and leaves its
  lock alone: that is how the stage pack ends its windows.)

## Wave J (as built, 2026-11-24, branch `r3-nh-live`)
The engine of doc 86 (Phases 1a to 9) is merged, so this version uses what the sections above call "later". Every
program here passes `python3 tools/nh_check.py --all` (now with the wave J checks: the prefix, the purse guard, the talk keys).

- `"prefix": "lq"` on the four. **J3**: the roads count as a finished Story (`"closure": "line"`): Salt
  `salt_board`/`salt_bought`/`salt_cart`, Cedars `cedar_contract`/`cedar_return`, Brown `brown_hunt`/`brown_spare`,
  Silver `silver_board`/`silver_mercy`/`silver_cut`; **C6**: each road requires the line's first or second chapter done,
  so a wolf who only stood a watch is not offered one.
- **C8**: each phase says its line's key to the realm as it opens (`lq_salt`, `lq_cedars`, `lq_brown`, `lq_silver`,
  the values the DM seeded by hand; `after_taken` forgets `lq_cedars`); a fold kept from Ch 3 tells Frosthollow
  `lq_rustlers=grey`, and the beam put before the Board tells the city `lq_beam=board`.
- **L2**: the waves name their camps (`lq_brine_flats`, `lq_old_stand`, `lq_fold`, `lq_kestrel`, ...) and the camp
  triggers match them. **C10**: The Brine Captain leads the Brackish Wells raiders, Sleet the Drover the rustlers
  (where the trigger had room under 24 actions).
- **L1**: the Old Brown hunt road stages the bear at the tarn (`hunt.stage greytarn`, 4 hours), the kill counted in the
  tarn's cell. **J1**: the choices laid for one wolf's start or step go to that wolf. **M2** and **C1** on every grant
  (`min(amount, purse / 20)`, the counters adding what was paid). **SQ2**: the Counting House chit comes from the
  Greatly Bank's till. **C5**: Ivy thanks each wolf who brought the cedars home, Vigdis each of the mercy road.
- **Left**: the clock lays stay every wolf's choices (no acting wolf to give them to); L3 (the cell-plus-switch
  triggers credit every wolf there); SQ2 for Osric, the shipwrights and Silver and Rain (no till of theirs in the world
  as last saved); C14, C11, N2, C12a.
