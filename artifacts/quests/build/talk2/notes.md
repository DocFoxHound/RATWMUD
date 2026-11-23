# Talk library, part 2: gossip and helpful knowledge (notes)

What is here: `campaign.dsl` (the campaign's lines, cast-and-talk.md §4), `extract_packs.py` → `packs_raw.json` →
`build_packs.py` → `packs.json` (the packs' lines), and `gen.py`, which writes them into `Data/Talk` (see its head).

## Today-only
- Every line is valid and loads today, and none plays until its key is set. The keys are `world.mem.nh_*`, `sq_*`,
  `mq_*`, `lq_*`, `st_*` and `nh_coin`. The exception is the small stage pack: its helpful lines are gated on
  `nh_stage`, so they play as soon as the stage key is set.
- Before C8, the DM seeds the keys by hand, for example `nh_stage`, `sq_on=yes` per town, a pack's "open" key and
  `nh_coin=thin`. The test town has none of them, so `talk_scorecard_quick` and `talk_golden` don't move.
- **Dialect gate:** speech is gated `self.from=<town>` rather than `self.dialect`, because `dialect_check` reads
  `from`. `self.from` is where the speaker grew up, and Fenshore and Mirebridge share one speech. Part 1's lines do
  the same.
- **Priorities:**
  - window gossip lines carry no priority, so a real claim still competes;
  - helpful pack lines in `news` take priority 2, and the thin purse priority 3;
  - dialect lines in `nh_*` nodes take priority 2;
  - recaps in `inn.greet` take priority 1, and an innkeeper's own pack lines priority 2.
- **Who owns what:** part 1 owns the `nh_story`, `nh_help`, `nh_who` and `nh_ask` nodes, their recaps and plain
  floors, and the `news` choice. This part fills those nodes after their bare `say`.
- **Re-running:** run `gen.py Data/Talk` again after anything rewrites `resident.tree` or `inn.tree`.

## Later (engine phases)
- **C9** (`figure.set`): the `{fig}` lines naming `#world.figure_villain#` or `#world.figure_hero#` are talk rules
  only (`rules/story_ninth_howl.talk`), gated on the figure being set. The window's render check has no figure, so they
  aren't tree lines. Once C9 lands (or the DM uses `save_figure`), they can join `nh_who` and `news` as well.
- **C8** (`talk.remember`): with it, the "someone did it" lines (`sq_last`, `st_last`, the packs' ending keys) begin
  to play, and the per-wolf closes in `pair.mem` play too. Those closes are the pack waves' work, not this part's.
- **T6** (a `story` topic): the eleven NPC-to-NPC scenes ride the everyday topics `news`, `gossip`, `day`, `lore` and
  `bandits` until a campaign topic exists.
- **N2** (`player.quest.*`): targets' own lines while a wolf's quest is under way are the pack waves'. This part only
  carries the `self.id` lines the pack designs put in `gossip` and `inn.greet`.

## Review fixes (2026-11-22)
- Vocatives the grammars keep for pups (`lambkin`, `tadpole`, `whelp`), for intimates (`my apple`) or for strangers
  (`honoured guest`) are `#address#` now, so an adult wolf isn't called a pup and a neighbour isn't a guest; the
  speaker's grammar picks the word (campaign.dsl, build_packs.py, the scenes).
- No greeting asks a question (the `moot_called` recap, Rosamund's and Nell's `inn.greet` lines).
- Ungated thanks no longer claim a deed that may not have happened (Ch 2 mirebridge, saltwell, oakhaven, plain).
- The Upper Mere ice watch is spoken of in the bell stage, not the harvest (`st-ice-watch` waits on `nh_mere_frozen`);
  Coldwater's harvest line points at the camp bounty. No "winter" promised for the ford's loss.
- Packs: the pawnshop pup is the old wolf's littlest; no "Cottage 3" in a recap; the Sump-lamps thief is a he, so he
  isn't taken for the headland's lamplighter (decisions §2.2); no "tomorrow" on a key that lasts days; the 40-year-old
  logger isn't "young"; `st-fog-lanterns` plays in letters and lights (its s56 phase).
- Scene closers answer what was said (no "It is" after a question or a doubt).
- At the commit: the badger lines (Coldwater, Larkfield) say a badger runs and bites only when cornered, and Saltwell's
  caravan line names the farm cart that comes most mornings, as the reviewed `nh-pack.small.south` has it.
