# The Ninth Howl: notes beside the programs (wave A)

Doc 86 §9, `artifacts/quests/design/decisions.md` §3, `campaign.md` §2.11, §4, §8.6. Check with
`python3 tools/nh_check.py <files>` (or `--all`); the world variables with `python3 tools/nh_world_vars.py --world ridgemere3`.

## world-vars.json
- Campaign §4's world variables and §4.5's purse counters and switches: 107 rows, each with its type, first value and a note.
- The DM declares them before approving `nh-purses` or Act I: `python3 tools/nh_world_vars.py --apply --world ridgemere3
  --who <account> --reason "The Ninth Howl, doc 86 10.1"`, or by hand from `--steps` in Campaigns > World variables.
  Declaring again keeps a value already held under the same type.
- The packs' own world variables (`lq_<line>`, `mq_<q>_posted`, `spawned_<region>` …) are added to this file by their
  waves; `nh_check.py` refuses a program whose world variable is missing here or has another type.

## nh-purses.json (always on; approve and publish first, leave running across all acts)
- Today: `day begins` sets every `coin_<purse>_day` to 0; `purse crossed` down past 60 turns `coin_<purse>_ok` off, up
  past 80 turns it on (the game writes both directions, so no daily fallback is needed).
- The Greatly Bank till (`till:job:rm_eleanor_saltcot`) is watched and counted already; no story pays from it until SQ2.
- At publication the game's first look makes no event: a purse already below 60 would keep its switch on. Every purse
  held 212 or more at the 2026-11-23 checkpoint.
- Later (C8, Phase 1b): on each town's down-crossing write the town talk key `nh_coin=thin` (`talk.remember`, 28 days)
  and `talk.forget` it on the up-crossing. Until then the DM seeds and clears `nh_coin=thin` by hand.
- Later (M2, Phase 2): the purse operand can replace the floor switches with a direct balance test.

## ninth-howl.cleanup.json (approve, do not publish: the undo if an act is stopped early)
- Today: `npc.unprotect` (ifRefused skip) on the 40 residents of campaign §8.6, in four groups a game minute apart
  (a phase holds 12 entry effects at most). Desmond Hollis is not on it. Imogen Nailridge is not protected by §8.6.
- Ch 1's setup (wave B) must protect exactly this list; checked 2026-11-23: all 40 placed, alive, reachable, and none
  carries the authored `live.npcs.protected` flag, so unprotecting takes nothing the world had before.
- Later (C11): add `npc.unspare` for the same 40.
- Not a program's job: the authored `protected` flag the DM sets on the seven oldest (§8.6) is a DM data edit, cleared
  by hand at the end. Holds, strangers, notices and the weather are undone by each run's own end.

## tools/dialect_allow.json (T9)
- "places": the place names of campaign §10.2, cast-and-talk §5.6 and the three pack files' allow-list lines.
- "names": the titles and feasts that are not places (the Moot and its names, the Reckoning, the Lantern Writ, the
  Concord, the Wardens, the Grey Captain, the Holding Howl, the Howl Line, the Last Sheaf, the Book of Names).

## Talk library, part 1: the cast, the Keepers and the campaign's quest givers (wave G; cast-and-talk §1, §3, §5)
- Where: `Data/Talk/dialogue/resident.tree` (the cast's greetings in `hub`; the one new `news` choice "What's all this
  talk of the Moot?"; the nodes `nh_story`, `nh_help`, `nh_who`, `nh_ask` and every `nh_*` talking path),
  `watch.tree` `hub` (Dagfinn, Cressida, Bettany, Corra Slagmore, Desmond Hollis), `church.tree` `greet` (Solveig) and
  `inn.tree` `greet` (the innkeepers' own lines, §3.23; no offers). Part 2 (gossip, helpful knowledge) adds its lines
  inside `nh_story`, `nh_help`, `nh_who`, `nh_ask`, `news` and `gossip` between its own `>>>`/`<<<` markers.
- Today: every line is gated on talk memory (`world.mem.nh_*`), so it is silent until the DM seeds the realm and town
  keys by hand (`nh_stage` first; the branch keys as each chapter decides them). `self.mem.*` lines (the debtors'
  `nh_debt`, `nh_keeper` for the pledge and chorus choices, `nh_kin` for the wharf names, `nh_spy` on the kitchen
  servant, `nh_lorn` on the shepherd) and `pair.mem.*` lines (§5.3's twelve pair keys) stay silent until C8.
- Story-heard nodes (the chapter's `dialogue node` trigger matches the node id as `detail` and the resident as
  `target`): `nh_df_up`, `nh_df_letter`, `nh_dq_stand`, `nh_dq_step`, `nh_br_cry`, `nh_ot_loan_yes`, `nh_ot_loan_no`,
  `nh_od_house`, `nh_od_greatly`, `nh_od_contest`, `nh_od_accept`, `nh_ro_shown`, `nh_ro_kept`, `nh_ro_ash`,
  `nh_list_bettany`, `nh_list_press`, `nh_list_captain`, `nh_list_sold`, `nh_ul_list`, `nh_gm_told`, `nh_gm_lie`,
  `nh_gm_q_honest`, `nh_ta_still`, `nh_ta_night`, `nh_ta_teach`, `nh_ta_give`, `nh_ta_hide`, `nh_ke_rope`,
  `nh_os_confess`, `nh_os_expose`, `nh_os_run`, `nh_fw_honest`, `nh_fw_crooked`, `nh_fw_charter_give`,
  `nh_fw_charter_deny`, `nh_fw_come`, `nh_ma_bargain`, `nh_ig_truth`, `nh_ig_kind`, `nh_tb`, `nh_el_in`, `nh_cc_buy`,
  `nh_as_run`, `nh_rw_word`, `nh_hd_said`, `nh_scrap`, `nh_pledge`, `nh_back`, `nh_chorus`, `nh_wn`. The node does
  nothing by itself: each chapter checks that the wolf earned the choice (quest done, list held, three testimonies).
- Changes from cast-and-talk, as built: one cast-slot choice per resident a stage (Oda, Corra Slagmore, Fenwick and
  Eleanor have one a chapter); Kell's tally and rope sit under his own node `nh_ke`; the chorus is its own slot (not a
  per-Keeper slot), and The Scattered's scraps are one shared node `nh_scrap`. `nh_story` can show nine choices at
  most (help, who, ask, the cast slot, pledge and backing, chorus, wharf names, the two ways out): no room for more. Rook's Word (`nh_rw_word`) is gated on the stage only:
  the today-only fallback; with C8 add `pair.mem.nh_rook=carrying`. Hilde's `nh_hd_said` keeps its pair gate (its
  fallback is an `npc.offer` in the Ch 8 program). Speakers with accent `none` (the Queen, Celestine, Desmond Hollis,
  Tansy Reedmere, Cairn Oakhaven, Osric Timberland, Dun Harvestley) speak plain words: their regional sayings were
  taken out. A line in a regional saying carries `self.from` as well as `self.id`, so dialect_check can see whose it is.
  Every line that dated the Moot says "the Moot" or "the Moot night"; "Midwinter" only under `nh_moot=midwinter`.
- New keys these lines read (beside campaign §10.1 and cast-and-talk §5.3): `nh_gudrun` (realm: match, shown),
  `nh_spy` (npc on the kitchen servant: told_king, told_queen, paid), `nh_lorn` (npc on the shepherd: shamed, turned),
  `nh_oda` (realm: contest, accept), `nh_tansy`, `nh_dagfinn`, `nh_desmond` as §5.3. Bramble Bogditch's "if she dies
  first" line has no key yet and was left out.
- Review fixes (as built, over cast-and-talk): asking Rune Gullwell the Counting House's price (`nh_list_price`) no
  longer sells the list; only "Done. Here's the list." reaches the story-heard `nh_list_sold`. Fenwick's crooked choice
  says the wolf will take the rider's letters (it was a question that committed). Ottilie's and Edric's backing starts
  at `reckoning` (Ch 2; campaign §10.3, none in Ch 1); Dagfinn's Lamp backing stays from `moot_called`. Gorm's "Why did
  you write to the Wardens?" waits for `nh_wardens` (set at Ch 5's reveal), so it spoils nothing. Greetings no longer
  ask the wolf a question or answer one it never asked (Brenna, Ulric, Oskar, Aldric, Maren, Gudrun, Cressida, Bettany,
  Corra Slagmore, Desmond); Myrtle names "our carter" rather than a bare "she"; Aspen no longer calls a grown wolf
  "lambkin"; the Larkfield smith's and Thorn Marshditch's scraps are their own (not Skye's catchphrase; no unmet
  "buy me the second cup"); the signals recap's toll reads plainly. Part 2's inn recap (between its markers) still
  has "taking a mark a wagon": part 2's generator should say "taking his toll off every wagon".
- Later: C8 (Phase 1b) writes every key; N2 (`player.quest.*`, `player.story.*`) lets nodes hide choices a wolf already
  made; C5 (Phase 3) `npc.greet` for the one-off thanks (Cressida, Ingrid Kendrick, Aspen's runner, Desmond at the yard,
  Aldric at the gate); T5 puts `npc.line` through the speaker's voice. The quest offers, "during" lines, scenes and
  notices of §3 (Q, O, L, S, B) are story texts: they belong to the chapter programs (waves B, H, I), not here.

## Wave J (as built, 2026-11-24, branch `r3-nh-live`)
The engine of doc 86 (Phases 1a to 9) is merged, so this version uses what the sections above call "later". Every
program here passes `python3 tools/nh_check.py --all` (now with the wave J checks: the prefix, the purse guard, the talk keys).

- **nh-purses**: `"prefix": "coin"`. **C8**: each town's `thin-<town>` trigger also has the town remember
  `nh_coin=thin` (28 days) and `mended-<town>` forgets it, so the thin-purse lines play without the DM. **SQ2**: two
  more tills are watched and counted, `till:job:rm_edric_millton` (the cooper, medium Q6, day cap 19) and
  `till:job:rm_rupert_fogton` (Hollis Physic, the Runnel Fever's draught money, day cap 9): their `coin_till_*_day`
  are set to 0 at midnight and their `coin_till_*_ok` switches follow `purse crossed` at 60 and 80 (four new triggers,
  rehearsed with the other purses). Both are declared in `world-vars.json` (group "purse counters and switches").
- **M2** is used by the payers, not here: every grant's If now also reads the purse itself (`{"purse": ...}` at least
  60 and the amount), beside these switches, which stay for their 60/80 hysteresis.
- **ninth-howl.cleanup**: `"prefix": "nh"`, otherwise unchanged. Since C11 a story's `npc.unprotect` and `npc.unspare`
  let go only of that story's own (or its campaign's) holds, so for the chapters' campaign-long holds the undo is the
  Campaigns console's **Let go of the campaign's holds…**; this program stays the fallback for protections with the
  default `ever` mark. No `npc.unspare` was added: it would undo nothing of another story's.
