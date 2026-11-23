# lq-eel-famine: The Eel Famine (notes beside the program)

Design: `artifacts/quests/design/quests-large.md` §11 (and §1, §13.3), `decisions.md` §3, §4. Today-only version: every
effect and event is in today's `Data/Storylines/trigger-catalogue.json`. Check: `python3 tools/nh_check.py
Data/Storylines/ridgemere3/quests-large/lq-eel-famine.json` (PASS, 4 rehearsals, nothing uncovered, references pass).

## What runs today
- **Phases** (the world's state): `open` (the weir stands) → `reckoning` (the weir is down; the roads) → `after_run` or
  `after_shared`; `season begins` in an after phase goes back to `open` with a fresh shared goal.
- **Chapter 1 `traps`** (Ivo Sootridge, Fenshore board; once; cap 40): Withy Creek, then the three witnesses' choices
  (Nab, Rowan, Jasper), hand in to Ivo. **Chapter 2 `weir`** (party; journal offer; weekly; cap 12): find the weir, three
  guards, six stakes, the hidden float lamp. **Chapter 3**: `eels_run` (dusk at the creek, two fish to Ivo) or
  `eels_share` (the Fish Hall, Karin's choice, the first share of ten pennies to Ivo).
- **Shared goal** `weir_left`: 40 stakes, +5 for each wolf's first stake (shown as "Stakes left in the weir"). Dusk lays
  ten stakes at three reed tiles in each region, only when fewer than six lie there; while the weir stands, a stake pulled is driven
  again elsewhere, so a crowd never runs out. Stakes on the ground do not outlast a server restart, so a wolf who finds the
  weir when no stake has been pulled or driven for a game day (timer `stake_seen`) finds five freshly driven where they
  stand. At 0, a game minute later, the reckoning.
- **Waves**: every night hour (21 to 3), a band of guards in `weir` and in `weir_b` where story wolves stand and no band
  stands (switches `spawned_weir`, `spawned_weir_b`), 2/3/4/6 by the wolves present. `camp cleared` in `3_12` with a
  switch on turns that region's switch off (both, when the wolf who broke it stands in neither region, or the watch broke
  it), leaves the float lamp at that region's centre (one lamp a game day, `lamp_today`) and takes 1 off `nh_threat` (at
  most 3 a game day for this line, `threat_day`, so raising and breaking band after band cannot farm the campaign's
  threat). A band nobody fights starves out after some weeks (`bandits scattered`): both switches go off.
- **The reckoning**: each road taken weighs 1 (2 after a pack of two or more). The first road starts a 3-day tally;
  the reckoning phase also sets a 7-day fallback, so if nobody chooses the default still comes. The share wins only if
  heavier; a tie or nobody: the eels run (`lq_eels=run`).
- **A season's return**: the shared goal is fresh; if nobody takes chapter 1 or 2 within 14 game days, the weir is back
  (`lq_eels=weir` and a Fenshore board notice).
- **Coin** (decisions 3.2, all from triggers): the weir's first time 6 (`town:fenshore`); the run road 12
  (`town:fenshore`); the share road 2 (`town:fenshore`) plus the Hall's share 10 (`town:ridgemere` until SQ2, at most 60
  in all, `share_paid`). Every grant is inside the `coin_<purse>_ok` / `coin_<purse>_day` If, adds to the counter, and
  is `ifRefused: skip`; a wolf's `lq_coin` for this line stays at or under 35 (the most a wolf can get here is 18).
  Paid in kind otherwise (bread; for the share, four fish and the carry step done for the wolf), with the player switch
  `lq_thin` and a journal line.
- **Campaign feed**: a road done adds 16 to `nh_realm` and `nh_voice_fenshore`; each guard camp −1 `nh_threat`
  (clamped); `nh_names` +1 for Nab's gran's name, once a wolf with a road, only while `nh_stage` is 4 to 6.

## Where the program differs from the design, and why
- The witnesses' objectives are their **choices** (`dialogue choice` on `lq_eel_nab`, `lq_eel_rowan`, `lq_eel_jasper`),
  not bare `conversation`: the wolf then hears the clue the reply carries. The choices are laid by a clock trigger a game
  minute into each visit, because `npc.offer` is a trigger's effect, never a phase's entry effect.
- `quest is` cannot be read by a campaign trigger in today's validator (its scope has no quest index), so the program
  keeps the wolf's own facts instead: `road` (the road taken), `paid_weir` (did the weir), `pack2`.
- Roads already offered cannot be withdrawn today (L5): taking a second road fails it at once ("Another road taken").
- The reckoning waits a game minute after the last stake, so the pulling wolf's own chapter settles in `open` first.
- No choice is `once`: a choice that is an objective stays in the window, so a wolf who picks Karin's bargain before
  entering the Fish Hall (the step that comes first) can pick it again there. What a choice pays or adds is guarded by its
  trigger (`once: member`).
- The hidden "gran" objective is shown to a wolf taking a road while `nh_stage` is 4 to 6.
- The `closure: true` mark on the road quests is **not written**: today's validator refuses an unknown quest field.

## DM steps
1. `lq_eels` is declared in `ninth-howl/world-vars.json` (text, first value empty); the program sets `weir` on its first
   `open`. Declare with `tools/nh_world_vars.py --apply` (or Campaigns > World variables) before approval.
2. Approve and publish `nh-purses` first (it owns the `coin_*` counters this line reads and adds to).
3. Talk keys (until C8): seed `lq_eels=weir` (realm) when the line opens; `lq_eels=run` or `lq_eels=shared` when the
   reckoning settles (the phase journal shows which). The lines below are silent until a key is set.

## Later (engine phases)
- **J3**: add `"closure": true` to `eels_run` and `eels_share` (read as `"line"`; the two exclude each other, so the line
  counts once).
- **L2/L3**: name the camps (`"camp": "lq_eel_weir"`, `"lq_eel_weir_b"`), and make "break the camp" an objective on
  `camp cleared` by exact target; drop the `spawned_` switches' cell matching.
- **C10**: a named weir-boss (`foe.add`) leading the first band.
- **SQ2**: the Hall's share from `till:job:rm_corra_coalcot` (day cap 14; counters `coin_till_corra_coalcot_*`).
- **J1/C6**: the witnesses' choices and Karin's bargain shown only to the wolf on that step or road.
- **C8**: `talk.remember` the keys at each phase entry (and `lq_eels_thin` for the day when a wolf is paid in kind).
- **N2**: Ivo's thin-purse thanks by the wolf's own `lq_thin`.

## Talk lines for wave G (exact; `resident.tree`, priority 2 unless said; recaps in `inn.tree` `greet`)
```
say[world.mem.lq_eels=weir here.region=fenshore self.dialect=mirebridge priority=2] Take friends and a strong back to the weir below Rivermouth. The guards come out after dark, and they don't like being outnumbered. The stakes pull easy in the mud.
say[world.mem.lq_eels=run here.region=fenshore self.dialect=mirebridge priority=2] The eels are up Withy Creek again, thick as reeds. Mud and eels, there's supper in every pot.
say[world.mem.lq_eels=run self.quarter=wharf priority=2] Eels are dear at the Fish Hall. Somebody pulled their weir out of the fens.
say[world.mem.lq_eels=shared here.region=larkfield self.dialect=larkfield priority=2] Fenshore has a share of the Fish Hall's eels now. Fair price, if you ask me.
# hub, Ivo's thin purse (the day's key, C8 later; the DM may seed it):
say[self.id=cty_ivo_sootridge world.mem.lq_eels_thin=yes priority=4] #thanks#, fen-friend. The stores are sunk this week, so it's bread and my word. The eels'll pay you back, mark me.
# inn.tree greet, Fenshore:
say[world.mem.lq_eels=weir here.region=fenshore] The eels never came up Withy Creek. The skipper at Fen Shore landing thinks somebody's stopped them on the river.
```
- Place names for `tools/dialect_allow.json` if `dialect_check` objects: Withy Creek, Rivermouth, Fen Shore landing,
  the Fish Hall (most are in Wave A's list already).

## Givers behind locked doors (2026-11-22, second pass; doc 86 "Givers behind locked doors")
- **Left open by this pass, closed by the third (below).** Ivo Sootridge (giver), Jasper Greymore and Nab Bogditch
  (choices) work in the Fen Shore and Rivermouth boat sheds and the Withy Creek eel house, locked to wolves (doc 82).
  Fenshore's two free residents a wolf can reach now give the small pack's pans and plank. Doc 86 lists what would close
  it.

## Doors a story unlocks (2026-11-22, third pass; doc 86 "Doors a story unlocks")
A door a live story has unlocked (`door.unlock`) is open to wolves while the story holds it (DECISION of 2026-11-22,
engine on branch `r3-nh-doors`; doc 86 "Doors a story unlocks"). A story never unlocks a home, and `tools/nh_check.py`
refuses a door into one, or into a room whose stairs reach one.

- Phase `open`, entry: the Fen Shore Boat Shed (`link_fs_fishing_fencoast_shed_door_a`, Ivo Sootridge), the Rivermouth
  Boat Shed (`link_fs_fishing_delta_shed_door_a`, Jasper Greymore) and the Withy Creek Eel House
  (`link_fs_marsh_eels_eelhouse_door_a`, Nab Bogditch).
- There is no window to close before the run ends. A quest without `phases` is open in every phase of its story
  (`story_quests.open_in`), and each door here serves one, so each stays unlocked until the run ends, when the game puts
  back what each had. (`door.lock` would lock the workers out too. `door.close` closes a door to wolves and leaves its
  lock alone: that is how the stage pack ends its windows.)
