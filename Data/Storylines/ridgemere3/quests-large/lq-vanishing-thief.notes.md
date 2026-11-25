# lq-vanishing-thief: The Thief Who Wasn't There (notes beside the program)

Design: `artifacts/quests/design/quests-large.md` §12 (and §1, §13.3), `decisions.md` §1.2.2 (Ivy Stonehollow a gift
target only), §3. Today-only version. Check: `python3 tools/nh_check.py
Data/Storylines/ridgemere3/quests-large/lq-vanishing-thief.json` (PASS, 5 rehearsals, nothing uncovered, references pass).

## What runs today
- **Phases**: `open` → `rumour` (at the second wolf under the street: a board notice and Petra's worried line) →
  `reckoning` → `after_forgiven`, `after_named` or `after_blinker`; `after_blinker_wardens` follows `after_blinker` on the
  first `day begins` with `nh_stage` ≥ 5, or when the DM sends the direction `wardens`. Forgiven and named are lasting;
  in a world that believed the lie, `season begins` sends the line back to `open` (the pup is still at it).
- **Chapter 1 `locked`** (Petra Vance, Ridgemere board; once; cap 40): the strongroom, Roald's and Silas's choices, a
  night on the Vance street (entering it between 23 and 4, or standing there at one). **Chapter 2 `under`** (journal
  offer; weekly; cap 12): the Sunken Street, the bricked arch (`old_drain`), a pledge from the cache, Hester's first
  choice (`lq_th_street`), and Palle's (hidden). **Chapter 3**: `thief_forgive` (the three pledges to Ivy, Piet and Runa,
  then the rates put plainly), `thief_name` (a pledge to Petra, then the name), `thief_quickened` (the lie). Each starts
  with Hester's `lq_th_face`.
- **The pledges** (`brass_clip`, `silver_bracelet`, `ear_ring`) are placed in the old drain at `open`. Each wolf who comes
  to the arch while the pup is still at it (open, rumour, reckoning, the lie's phases) finds one more in the cache, once
  each time they take Under the Street (player switch `laid`, cleared when they take it): items on the ground do not
  outlast a restart, and no pledge comes back on its own, so the cache is no jewellery farm. A wolf on the forgiving road
  who faces the pup is given the pledges they lack, to take home; a wolf on the naming road who carries none finds one
  more in the cache (both once a wolf, `face-pledges`).
- **Shared goal** `found` (shown as "Ways under the Heights found"): 3, and one more for every three wolves who go
  under. Board notices at 1 and 2 (signed Petra Vance).
- **The reckoning**: one road a wolf, weight 1 (chapter 2 is a wolf's own, so no pack weight); the first road starts a
  3-day tally, with a 7-day fallback. Named or the lie win only when heavier than both others; a tie or nobody:
  forgiven. Hester's Gift answer (`lq_th_gift`, offered to every wolf today, worded "if I carried a Gift") is flavour
  only: shown to every wolf, it must not weigh the tally (a wolf on the lie's road could otherwise add a vote for
  forgiving). With J10 it weighs 1 toward forgiving, as the design says.
- **Coin** (decisions 3.2, from triggers, all `town:ridgemere`, the treasury): under's first time 6; forgive 12, name
  12, the lie 10 ("Vance House's gratitude"). Most a wolf can get: 18. In kind otherwise (bread, `lq_thin`, a journal
  line).
- **Campaign feed**: a road done adds 8 to `nh_realm`, and 8 / 4 / 2 to `nh_voice_ridgemere` (forgiven / named / lie).

## Where the program differs from the design, and why
- Chapter 2's `thief` step is Hester's own choice `lq_th_street` (the design had a bare conversation): a choice carries
  her words, and it sets up the `face` choice of chapter 3.
- The name road's last step is Petra's choice `lq_th_name` ("It was a Sump pup, through an old drain. No Gift at all."),
  not a bare conversation, so the naming is something the wolf says.
- The servant and the Thornburys' man are choices too (`lq_th_roald`, `lq_th_silas`), so their clues are heard.
- No choice is `once`: a choice that is an objective stays in the window, so a wolf who picks the pup's choice before the
  cache, or the lender's naming choice before giving her the pledge, or the pup's `face` choice before taking a road, can
  pick it again when it counts. What a choice pays or gives is guarded by its trigger (`once: member`).
- The owners are named by their work: Ivy Stonehollow fells timber (the brass harness clip is hers), Piet Wickridge is a
  Sump Stairs porter on Oakum Stair (the bracelet was his drowned lighterman brother's).
- J8 fallback as designed: each forgiving wolf also gets the deed "showed the Heights there was no Quickened thief".
- Hester's coldness to the naming wolves is her `after_named` phase line for everybody (C5 later).
- `quest is` is not readable by a campaign trigger today, so the wolf's own `road` and `paid_under` stand in.
- `closure: true` is not written (today's validator refuses it).

## DM steps
1. `lq_thief` is declared in `ninth-howl/world-vars.json` (text, first value empty); the program sets `walking` on its
   first `open`.
2. `nh-purses` approved and running first.
3. Talk keys (until C8): seed `lq_thief=walking` (realm) when the line opens; the result key when the reckoning settles.
4. In a world that believed the lie, the Wardens line comes by itself with `nh_stage` 5; the DM can send the direction
   `wardens` to bring it sooner.

## Later (engine phases)
- **J3**: `"closure": true` on the three roads.
- **J10**: show `lq_th_gift` only to a Gifted wolf (`wolf has gift`), reword it "I carry a Gift. You don't.", and add
  its trigger back: `dialogue choice` on it, `once: member`, during the reckoning with a road taken, `add t_forgive 1`.
- **C8**: `belief.plant` the "walks through walls" claim about a Quickened thief at `found` 1 and 2; `talk.remember` the
  keys. **J8**: `belief.forget` it on forgiven (drop the extra deed then).
- **C5**: Hester's thanks or coldness to one wolf; Petra's thanks by name. **C14**: Vance House's rates as a lasting change.
- **J1/C6**: Hester's and Petra's choices shown only to the wolf on that road.

## Talk lines for wave G (exact)
```
say[world.mem.lq_thief=walking self.quarter=heights priority=2] They say a Quickened wolf walks through the Heights' walls and takes pledges. Lock your strongroom, for all the good it does.
say[world.mem.lq_thief=walking self.quarter=sump priority=2] There's an old street under the Heights, from before the walls. The Sunken Street. Folk say it runs further than anybody's walked.
say[world.mem.lq_thief=forgiven here.region=ridgemere priority=2] Vance House let the Sump pledges go. A wolf put it to the lender plain, they say, and the lender listened.
say[world.mem.lq_thief=named here.region=larkfield self.dialect=larkfield priority=2] Ridgemere caught its wall-walking thief. A pup, they say, no Gift at all. Short-weighed magic, that.
say[world.mem.lq_thief=blinker here.region=ridgemere priority=2] The Heights swear it was a Quickened thief. Somebody's writing to the Concord for the black cloaks. Bright One keep us all.
# hub, Petra's thin purse (C8 later; the DM may seed it):
say[self.id=rm_petra_vance world.mem.lq_thief_thin=yes priority=4] The treasury's short this week, so it's bread and my thanks. Bright One knows I've had my fill of counting coin.
# inn.tree greet, Ridgemere:
say[world.mem.lq_thief=walking here.region=ridgemere] Pledges vanishing from a locked room on the Rise. The lender at Vance House wants a plain answer.
```
- Allow-list: "Sunken Street" (already in large §14.2's list), "the Rise", "the Heights".

## Givers behind locked doors (2026-11-22, second pass; doc 86 "Givers behind locked doors")
Every resident of this line lived and worked behind doors locked to wolves (doc 82), and two of its places were locked
interiors: the strongroom, and the Sunken Street, where the cache lay (so it could never be picked up). Now:
- The Vance who runs the Rag Mill, Eleanor Vance (6-18, plain), gives `locked` and holds the three roads' choices
  (`lq_th_forgive`, `lq_th_name`, `lq_th_blinker`), speaking for the House; Petra stays in the words, as the lender, and
  signs the board notices ("Ask at the Vances' Rag Mill"). Her window line and the thin-purse line are Eleanor's.
- "Ask my servant" is the Harrowgates' man, Emmerich Greyridge, out on the Rise all day (6-21); "the Thornburys' man" is
  the Eastgate stable keeper, Ragnar Roperidge (6-18), whose peg the Thornburys' harness clip went from.
- The thief is Una Brineridge, 18, a Greywool hand (8-18), and her lookout Finn Dripwick, 18, at the Sailors' Rest
  (7-18). The words say "young Sump wolf" for the pup, and Greywool for the school.
- The strongroom is looked over from Vance House's door (`vance_door`); the old street is found at the Sunken Street's
  Commons stair (`ss_stair`); the bricked arch is by its far door, on the street (`old_drain` on `w_ridgemere_1_1`),
  and the cache's three pledges are laid there (15,41 / 16,41 / 17,42), so `pledge-found` listens on that cell.
- `givers.tsv` rows moved to the five new residents. Rehearsals follow (region events for the door and the stair).

## Wave J (as built, 2026-11-24, branch `r3-nh-live`)
The engine of doc 86 (Phases 1a to 9) is merged, so this version uses what the sections above call "later". Every
program here passes `python3 tools/nh_check.py --all` (now with the wave J checks: the prefix, the purse guard, the talk keys).

- `"prefix": "lq"`. **J3**: the three roads count as a finished Story (C6: after `locked` or `under`).
- **C8**: `lq_thief` by phase (`walking`, `forgiven`, `named`, `blinker`); a wolf paid in bread tells the realm
  `lq_thief_thin=yes` for a day (Petra's line). **M2** and **C1** on every grant.
- **Left**: the clock lays stay every wolf's choices; C5 for Hester (her cold line stays a phase line).
