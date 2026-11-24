# ninth-howl.3, "Two Long, One Short" (Act I, Chapter 3): notes

Written to `campaign.md` Chapter 3 and decisions §6, on the merged engine (`r3-nh-live`). Prefix `nh`. Started by
`ninth-howl.2` (`story.start`, carry); it starts no chapter itself. It is the last chapter of Act I, so it ends in an
interlude that waits for Act II.

**Phases:**
- `ch3_main`: the work. Its `ready` block: 30% of the active wolves in `ch3_did`, floor 3, cap 150, or `ch3_auto` once
  36 game days have passed; 12 game days since the chapter; `ch3_wait_bar` with the bar; `auto`, `slowAfterDays` 36,
  `slowNeedsYou`.
- `ch3_closing`: the herald's close at the next dusk at least three game hours away. On the season's days 43 and 44 it
  waits for day 45, the eve of the festival. Every branch is resolved there, with its default.
- `interlude`: `campaignRules: false`. It closes the seven doors the chapter opened (`door.close`, never `door.lock`, so
  their workers still go in). It ends on `story started` (`ninth-howl.4`), or on the DM's direction "End the act".
- `end`.

**The tasks** (`ch3_did`): a lamp doused (each lamp hour, 21 to 4, on one of the four ridges); a Glasswater wave fought;
a Watch-list piece taken; a gift to the Upper Mere post (proj-1).

**Rehearsals:** sixteen. They cover 1, 5 and 200 active wolves; Advance anyway on day 3 (`nh_pushed`); the soft maximum
with auto off and on; the bar waited for; and every value of every branch:
- lists: bettany, press, captain, sold and lost;
- Desmond: held and fled;
- Lorn: shamed, turned and none.

They also cover both scars, the Blaze and no Blaze, the surplus, the Ridge-walkers and `expectChapters: []`; the goal at
200 wolves (only counted lamp wolves grow it), the nightly cap on a wolf's lamp voice, a starved wave raised again, a
shepherd's choice picked after he is settled, a list failed at the close, and the treasurer's favour and a pick-up in
the closing. Both twins give identical results.

## Where it differs from the design, and why

- **Quests in campaign triggers.** A campaign trigger cannot read a quest's state (`campaign_scope` has no quest index).
  The seven triggers that must read one live in `ch3_main`, and all seven are copied into `ch3_closing` (`_c`), so a list,
  a favour's tally or door, the harness and the copied board can still be finished before the close. Every other trigger acts without reading quests:
  `objective.complete` and `objective.progress` do nothing for a wolf with no copy, and `quest.offer` says nothing to a
  wolf who has the quest already.
- **Variable names.** `ch3_points`, `ch3_goal`, `ch3_helpers` and `p_ch3` replace `ch_points`, `ch_goal` and `p_ch`.
  With the general names, carry would bring in Chapter 2's bar.
- **Archive work is given nowhere on Ridgemere 3.**
  - Karin's favour is now Jasper's customs tally (the tree node `nh_jasper_tally`, at the Customs House, which the story
    unlocks), brought to her own door after work. Stanmore House is a home, and her Administrative Wing is behind the
    Residence's door, so the story opens neither.
  - Lamp-Black's record is asked of the keeper of the rolls, Eleanor Mudworth, at the Records Office (unlocked).
- **The favours, by their places.**
  - Dripcot's night watch: a wolf in the cells at the second hour (the Keep is unlocked). Nobody works there at night,
    so there is no `together`.
  - Lionel's service: a wolf in the cathedral or its crypt at the tenth hour of Restday (unlocked), then the crypt, then
    the hand-in.
  - Farrow's note: the tagged note brought to the Weavers' Terrace door (a home) between 19 and 4.
  - Karin's favour is a journal offer when a wolf starts the lists, because she is never anywhere a wolf can reach her.
- **The Most Suspected Wolf.** `stolen goods smelt` has no wolf as its actor, and `reported` needs a crime seen. The steps
  are now:
  1. open the Pawn and Ledger keeper's window;
  2. enter the Fish Hall;
  3. reach `nh_cc_smell`;
  4. hand in to Dagny.

  The rewards: the clearing (`belief.plant` `story.cleared`), `belief.forget` (theft, 60%), a rumour on Corra Coalcot,
  and Ingrid Kendrick's `npc.greet` for 7 days.
- **The Glasswater waves** rise when a wolf comes to the camp, not at the opening. Each is
  `min(6, 1 + wolves there)` (the opening has no wolves to count). They are matched by the camp's name
  (`camp_story_nh_glasswater`) and `spawned_glasswater`. Every wolf of the story standing at the camp when a wave breaks
  has "fought in a Glasswater wave" (the task), not only those who landed a last blow. A story camp hungers like any
  other: a wave left standing until it starves (`bandits scattered`) turns `spawned_glasswater` off, so the next wolf to
  come raises the wave again and the road is never stuck.
- **The shepherd.** The first wolf to choose settles Lorn: one resident cannot be both shamed and turned. A later wolf's
  three nights still finish the story, and so does a choice picked after he is settled (a journal note says so). Exposing him takes effect at the choice, with no separate walk to the square:
  Coldwater's talk turns on him, a rumour is planted in Coldwater and the Coldwater board posts it.
- **The pledge quarter-share** is paid on deeds a wolf does in person. A lamp hour credits each wolf inside a For each,
  and `run` keeps the event's wolf, so a douse feeds only the ridge's own town.
- **The lamps' points** are 2 a wolf with the distance weight (3 on the hill, the Upper Mere and the downs, 2 on the
  knoll), three wolves counted a ridge an hour. Only a counted wolf joins `ch3_helpers` (a fourth wolf on a crowded ridge
  adds nothing to the bar, so he does not grow the goal either). The bar takes every lamp hour, but a wolf's own voice and
  merit take at most three lamp hours a night (`p_lamp` against `night_no`, counted at each dawn), so standing on a
  ridge all night every night does not farm `p_voice`.
- **The dark-night boards** say how many ridges went dark ("No lamps on the downs last night" only when all four did).
- **The Blaze's villain** is set per Greenholt town as `nh_villain_coldwater`, `_larkfield` and `_frosthollow`. The game
  reads `live.figures` in order of their ids and the last that is known wins, so these must sort after Chapter 2's realm
  figure `nh_villain` (Ambrose); `nh_captain_<town>` sorted before it and was never heard. Bettany as the Watch's leader
  is `nh_captain`, the id Chapter 9 sets.
- **The journal offers at the opening.** The wolves Chapter 2 brought are in the world already, so they have no
  `arrival`: the setup offers them the Watch lists and the dismissed guard.
- **Stranded copies.** When the lists close, a list still being gathered or carried fails. When Desmond Hollis is held,
  everyone else on his trail fails, and "The Dismissed Guard" is offered only while `nh_b_desmond` is empty.
- **Lamps still lit** (the scar `signals_burning`) means fewer than four dark nights in the chapter (`dawn_dark` fired
  fewer than four times).
- **The harness** is laid at the 2/3 step, and again on each night's first douse of the Upper Mere ridge. Ropes nobody
  picks up stay on the ground until the server restarts.
- **The admin's switches** can also be sent as the news `nh3_auto` and `nh3_wait_bar` (`story notified`, as Chapter 5
  does). The Variables tab's Set does the same, and the news is how a rehearsal reaches them.
- **`nh_voice`** is written only as `raised`, the one value the library reads. Mirebridge has no town owner, so it has no
  key.

## Left (not done)

- The design's "camps stop re-forming for 20 days" after the press is only `nh_threat` −20.
- The `protected` and spared holds are Chapter 1's (`lasts: campaign`). This chapter holds only Desmond Hollis. The next
  act's first chapter re-applies his hold.
- Approval on Ridgemere 3 needs migration 0081 (realm variables).
- The harness (Upper Mere, the first douse of each night) and the copied wagon board (the Carters' Yard at 21) are laid
  every night whether or not anyone needs them, so unclaimed ones pile up until a restart (one rope or letter a night).
- Not this chapter's to fix, found in review: Chapter 4 does not re-hold Desmond Hollis (the design asks every later
  chapter's setup to), and Chapter 7's `nh_road_villain` (`region: greenholt`) matches no `here.region` (town ids) and
  sorts before `nh_villain`; to rename the Grey Captain it should set `nh_villain_coldwater`, `_larkfield`, `_frosthollow`.

## The checks

- `python3 tools/nh_check.py Data/Storylines/ridgemere3/ninth-howl/ninth-howl.3.json`: PASS.
  - The program passes `validate`; `problems` is empty.
  - `review`: 16 rehearsals pass, with no phase, trigger or Readiness uncovered.
  - References pass on Ridgemere 3, read only.
  - 62 residents, all placed, alive and reachable.
  - Seven doors, all workplaces with no home behind them.
  - Thirty memories written, each one read by the library.
  - One warning: migration 0081.
- `python3 tools/story_words.py` reports no problems, offline and with the givers' voices.
- The generator is outside the tree, in `/tmp/nh3rev/gen3.py` and `/tmp/nh3rev/tests3.py` (the reviewed copy of
  `/tmp/nh3`).

## Campaign check (2026-11-24)
- The act's last phase `end` no longer runs the campaign triggers (`campaignRules: false`): before, every lamp night,
  Glasswater wave and deed rule of this chapter kept running beside Act II.
- The run is not finished when Act II starts, so that what Act I keeps for the whole campaign stays: the cast spared from
  old age (`lasts: campaign`), the talk not kept, and Desmond Hollis held in the cells (or on the Greyteeth road) through
  Act II: Ch 4 and Ch 6 never hold him, and Ch 5's own hold is refused while this one stands (its move still
  puts him on its spot, and he stays held there).
- New `act3_free` (`story started` ninth-howl.7): Desmond is let go, so the Moot Night can hold, try and outlaw him
  (Ch 7 lays its hold again an hour after its start, `desmond_again`).
- New `act_over` (`story ended` ninth-howl.9, any ending): the run finishes, letting go of all it held. To close the act
  early, the DM stops the run (and publishes `ninth-howl.cleanup` if the campaign is being abandoned).
- The rehearsal "Desmond Hollis held..." carries both events and ends in success.
- Its other resident effects without an `ifRefused` (11) now skip when refused, as the later chapters' do.
