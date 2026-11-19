# Mantle batch spares-1: the first spare pool (2026-11-18)

Writers: six Claude (Opus) writer agents in parallel (4-6 spares each), plus the lead's editor pass and gates.
Branch: r3-mantles-spares (from origin/ridgemere2 092e418c). World: ridgemere3 at game day 43.4 (773 living).
Doc 84 goal 2 and §12; handbook §3.6. The user cut the budget, so this is a small pool: about one year of births a pool,
at least 2 a pool (doc 84 §12.1's 88 stays the long-run target).

## What was written

32 spare mantles: `tier spare`, `state ready`, no `written_for`, `fits stage=young|adolescent|adult|old`, `young`,
`adolescent` and `grown` sections, name-free, sex-neutral (no `^m`/`^f`, the spouse only through `#self.spouse_word#`
gated on `self.married=yes`), personas with no pronoun. 2,774 items in all (symbols included; 69-73 items a mantle
that aren't symbols, against G2's spare floor of 64), about 110,000 words.

| Pool | Spares | ids | sly + guarded |
|---|---|---|---|
| plain | 12 | rm901-rm912 | 3 (rm901 guarded, rm904 sly, rm910 guarded) |
| coldwater | 5 | cw901-cw905 | 1 (cw903 guarded) |
| larkfield | 5 | lf901-lf905 | 1 (lf902 sly) |
| frosthollow | 2 | fh901, fh902 | 1 (fh902 guarded) |
| oakhaven | 2 | oh901, oh902 | 1 (oh901 sly) |
| southmeadow | 2 | sm901, sm902 | 0 |
| saltwell | 2 | sw901, sw902 | 0 |
| mirebridge | 2 | mb901, mb902 | 1 (mb902 guarded) |

Voices (title, honesty): rm901 the smoke-reader (guarded), rm902 the rooftop kite-flyer, rm903 the drain-poker,
rm904 the waterline guesser (sly), rm905 the first-light blesser, rm906 the arch-clapper, rm907 the hen-namer,
rm908 the thread-winder, rm909 the list that goes missing, rm910 the button tin (guarded), rm911 the pot-taster,
rm912 the walking knitter; cw901 the leaf-boat folder, cw902 the window-mist drawer, cw903 the question for a question
(guarded), cw904 the beast-talker, cw905 the crack-stepper; lf901 the knotted string, lf902 the trader-up (sly),
lf903 the candle-ends, lf904 the latch-checker, lf905 the price-rhymer; fh901 the dark-whistler, fh902 the
candle-ender (guarded); oh901 the pocket-swapper (sly), oh902 the bee-teller; sm901 the rain-sniffer, sm902 the
hen-fretter; sw901 the knot-cord keeper, sw902 the sand-map drawer; mb901 the unasked mender, mb902 the thatch-hider
(guarded).

## Gates

- `mantle_batch gates` over the 32: G1 "32 mantles, 2774 items, 2316 rules and scenes; 0 refused"; G2 `--strict`
  over all eight dialect folders "410 mantles: 0 refusals, 2 warnings" (rm117 and cw197 `resident.job` lines saying
  `self.trade_words`: on origin/ridgemere2 before this batch, not touched); lint "32 mantles: 0 refusals, 0 warnings";
  G3 passes trivially (spares have no wearer).
- `dialect_check --world ridgemere3`: 0 failures, 0 warnings. `talk_lint`: 0 new, 0 gone since the baseline.
- No graft node uses a window-tree node name, including the 22 that doc 84 §45 (on r3-talk-p13) adds.
- `mantle_load ridgemere3 --batch spares-1 --written-by claude` (no `--bind`): 32 written; backups
  `Saved/Backups/mantles-ridgemere3-backup-20261118-213853.jsonl` (whole) and `...-spares-1-20261118-214106.jsonl`.
  `mantle_load check`: "395 placed residents wear no mantle (of 773); 410 mantles, 378 worn".
- `mantle_export --check`: the rows and the export agree. G1 over the whole export: 410 mantles, 0 refused.

## `mantle_check --pools --world ridgemere3`: 8 pools, 8 not green (expected at this size)

| Pool | Spares | Years green (of 5) | Sly + guarded, spares / living |
|---|---|---|---|
| plain | 12 | year 1 | 25.0 / 21.1 |
| coldwater | 5 | none (year 1 needs 5.4) | 20.0 / 22.4 |
| larkfield | 5 | year 1 | 20.0 / 19.5 |
| frosthollow | 2 | none (needs 2.3) | 50.0 / 33.3 (off) |
| oakhaven | 2 | none (needs 2.3) | 50.0 / 50.0 |
| southmeadow | 2 | none (needs 2.3) | 0.0 / 15.8 (off) |
| saltwell | 2 | none (needs 2.6) | 0.0 / 23.1 (off) |
| mirebridge | 2 | year 5 | 50.0 / 26.1 (off) |

The gate's green line is 1.5 x the half-year's births + 2, which is more than one year of births for every pool.
Two spares can only be 0% or 50% sly or guarded, so four of the small pools can't fall within 5 points; each was set
to the nearer side. The next batch should add a third spare to each small pool (which brings year 1 to green) and
choose its honesty to bring the mix within 5 points.

## Decisions and problems
- rm905's quirk was shortened to `blesses_the_first_sun_to_strike_the_roofs`: the first load was refused whole by
  `mantles_quirk_check` (at most 60 characters, a database rule the gates don't check), so nothing was written that
  time.
- Likes and dislikes gated in lines must be ids from `Data/Talk/opinions.json` (G1 refuses others); the writers kept
  to them.
- Not done (outside this budget): stage kits, mark banks, the rest of §12.1's 88.
