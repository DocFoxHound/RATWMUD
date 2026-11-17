# Talk scorecard

Generated 2026-11-17T06:20:24+00:00 by `tools/talk_score.py` (Docs/Design/84 §15.3).

Runs: `L1`.
Shuffled control (talk_sim --ablate shuffle: lines after the first replaced by lines said at the same side of other conversations on the same topic, by a speaker of the same dialect where possible (§15.5)): L1.

| Id | Metric | Value | Shuffled control | Separation | Target | Gate | Passes |
|---|---|---|---|---|---|---|---|
| R1 | first identical conversation, per listener (h) | 17.6 | 99.5 | 82% (the control scores better) | 40 h+; hard 20 h+ | hard | no |
| R2 | first repeated opener, per listener (h) | 10.1 | 10.1 | 0% (the control scores better) | 8 h+ | soft | yes |
| R3 | distinct openers per 3,000 exchanges | 2,803 | 2,803 | 0% (the control scores better) | L1 1,500+ | soft | yes |
| R4 | distinctive line heard again within 10 h, per 100 h | 1,582 | 1,851 | 15% (within 30%) | 5 or fewer | soft | no |
| R5 | collision index, per 100 h | 0 | 332 | 100% | 1 or fewer; hard 3 | hard | yes |
| R6 | most-spoken line's share (%) | 0.815 | 0.852 | 4% (within 30%) | 0.4% | soft | no |
| R7 | distinct reply entries per 1,000 reply beats | 98.1 | 90 | 8% (within 30%) | 500+ | soft | no |
| R8 | habit saturation (% of a speaker's lines, mean) | 0.113 | 0.0611 | 46% (the control scores better) | 15% or less | soft | yes |
| C1 | owed answers answered, by moves (%) | 98.5 | 19.4 | 80% (coherence) | 97%; hard 92% | hard | yes |
| C1Q | questions answered: the '?' heuristic (%) | 97.7 | 85 | 13% (within 30%) | diagnostic (C1 until Phase 3; doesn't separate from the control) | soft | n/a |
| M1 | lines carrying moves (%) | 100 | 100 | 0% (the control scores better) | 100% at Phase 7 (every entry) | soft | n/a |
| C2 | replies at the best fit available (%) | 100 | 99.9 | 0% (within 30%) | 95% | soft | yes |
| C3 | endings by a farewell or a marked break (%) | 89.2 | 32.2 | 64% (coherence) | 80% / 3% / 5% | soft | yes |
| C4 | tone clashes per 1,000 beats | 0 | 0.0826 | 100% (coherence) | 0 | hard | yes |
| C5 | unmarked topic jumps per 1,000 beats | 0 | 41.6 | 100% | 5 | soft | yes |
| C6 | same entry or question twice in a conversation, per 1,000 | 0 | 179 | 100% (coherence) | 0 | hard | yes |
| C7 | lines per conversation (mean) | 6.5 | 6.5 | 0% (the control scores better) (structural) | 5-7; p90 9+ | soft | yes |
| C8 | conversations with a third speaker (%) | 2.56 | 2.56 | 0% (the control scores better) (structural) | 5-15% | soft | no |
| C9 | callbacks per 10 conversations | 4.07 | 4.07 | 0% (the control scores better) | 1.5+ | soft | yes |
| T1 | raw ids spoken, per 1,000 lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T2 | presupposition failures, per 1,000 lines | 0.0206 | 6.98 | 100% | 0 | hard | no |
| T3 | self-reference errors (own name, the listener in the third person, unbacked eyewitness words), per 1,000 lines | 0.227 | 0.433 | 48% | 0 | hard | no |
| T4 | identity drift: a stance changed with no event or concede, or answered against one's own, per 1,000 stance lines | 0 | 8.89 | 100% | 0 | hard | yes |
| T6 | 'they' said of one known wolf named in the line, per 1,000 lines | 0.248 | 0.206 | 17% (the control scores better) | 0 | soft | no |
| D1A | lines said a step broader or plainer than the speaker's own accent (%) | 4.2 | 4.2 | 0% (the control scores better) | diagnostic: accommodation (§10.6) | soft | n/a |
| O1 | opinions given per 10 conversations | 0.801 | 0.055 | 93% | diagnostic (§8.8) | soft | n/a |
| T5 | telling what the listener already believes, with no 'known' answer (%) | 0.786 | 0.406 | 48% (the control scores better) | 2% | soft | yes |
| D1 | lines with a dialect's words (%): accent none | 0 | 2 | 100% | none 0% | hard | yes |
| D1L | lines with a dialect's words (%): light speakers | 0.999 | 0.76 | 24% (within 30%) | light 25-50% | soft | no |
| D1B | lines with a dialect's words (%): broad speakers | 1.98 | 1.61 | 19% (within 30%) | broad 60%+ | soft | no |
| D2 | another dialect's words in a speaker's line | 0 | 2 | 100% | 0 | hard | yes |
| D3 | dialect guessed from 20 lines (%): light and broad | 78.7 | 91.5 | 14% (the control scores better) | 85%+ | soft | no |
| U1 | a resident's lines from their own words (%) | 0 | 0 | 0% (the control scores better) | 20-35% | soft | no |
| U2 | distinguishability of look-alike residents (%) | 63.9 | 60.1 | 6% (within 30%) | 90%+ | soft | no |
| K1 | conversations on everyday topics (%) | 46.9 | 46.9 | 0% (the control scores better) | 35%+ | soft | yes |
| K2 | largest topic's share (%) | 21.2 | 21.2 | 0% (the control scores better) | 25% or less | soft | yes |
| K3 | same news told twice to the same resident | 1 | 1 | 0% (the control scores better) | 0 | hard | no |
| W1 | window lines from mantles (%) | n/a | | | 40%+ | soft | n/a until Phase 13 |
| W2 | free-text understanding | n/a | | | 85%+; wrong 8% or less | hard | n/a until Phase 13 |
| W3 | a resident answers chat with the option off | n/a | | | 0 | hard | n/a until Phase 12 |
| B1 | one beat: the director's mean (ms) | 0.295 | 0.295 | 0% (the control scores better) | 0.6 ms or less (gate: talk_bench) | soft | yes |
| B1P | one beat: the director's 99th percentile (ms) | 1.21 | 1.21 | 0% (the control scores better) | 3 ms or less (gate: talk_bench) | soft | yes |
| B1L | a director look: 99th percentile (ms) | 8.32 | 8.32 | 0% (the control scores better) | 5 ms or less (gate: talk_bench) | soft | no |
| B5 | exchanges simulated a second | 1.92 | 1.92 | 0% (the control scores better) | 150+ | hard | no |
| X1 | lexical cohesion: replies sharing a content word with the line before (%) | 17.9 | 2.01 | 89% (coherence) | diagnostic (not in §15.3): shows the control is discriminating | soft | n/a |
| H1 | blind naturalness 1-5 | n/a | | | 3.8+ | soft | n/a until slice ends (talk_rate.py) |
| H2 | 'written whole by a writer?' (%) | n/a | | | 40%+ | soft | n/a until slice ends (talk_rate.py) |

## By profile

| Id | L1 |
|---|---|
| R1 | 17.6 |
| R2 | 10.1 |
| R3 | 2,803 |
| R4 | 1,582 |
| R5 | 0 |
| R6 | 0.815 |
| R7 | 98.1 |
| R8 | 0.113 |
| C1 | 98.5 |
| C1Q | 97.7 |
| M1 | 100 |
| C2 | 100 |
| C3 | 89.2 |
| C4 | 0 |
| C5 | 0 |
| C6 | 0 |
| C7 | 6.5 |
| C8 | 2.56 |
| C9 | 4.07 |
| T1 | 0 |
| T2 | 0.021 |
| T3 | 0.227 |
| T4 | 0 |
| T6 | 0.248 |
| D1A | 4.2 |
| O1 | 0.801 |
| T5 | 0.786 |
| D1 | 0 |
| D1L | 0.999 |
| D1B | 1.98 |
| D2 | 0 |
| D3 | 78.7 |
| U1 | 0 |
| U2 | 63.9 |
| K1 | 46.9 |
| K2 | 21.2 |
| K3 | 1 |
| B1 | 0.295 |
| B1P | 1.21 |
| B1L | 8.32 |
| B5 | 1.92 |
| X1 | 17.9 |

## Details

- **L1**: {"R1_detail": {"listeners": 4, "median": 93.8064722869614, "censored": true, "exchange": 411}, "R2_detail": {"median": 10.263166664277087, "censored": false}, "R6_detail": {"line": "Sleep sound.", "times": 395}, "R7_detail": {"replies": 27117, "closing_beats": 13880, "closings_distinct_per_1000": 31.0}, "R8_detail": {"p90": 0.22172949002217296, "speakers": 115, "lines20": 48115, "any_habit_mean": 0.1657240386601171}, "C1Q_detail": {"questions": 2822, "answered": 2757}, "C1_detail": {"owed": 18009, "met": 17740}, "C2_detail": {"answers": 13402, "fallbacks": 0}, "M1_detail": {"scene": 100.0, "written": 100.0}, "C3_detail": {"farewell": 89.16633105114781, "marked_break": 0.0, "cut_off": 1.154517384883877, "cap": 0.0, "nothing_fits": 9.679151563968318, "curt": 9.665726943213855}, "C4_detail": {"clashes": 0, "by_tone": 0}, "C7_detail": {"p90": 10, "three_lines": 4.618069539535508, "four_lines": 10.108739428111155}, "T2_detail": {"by_kind": {"children": 1}, "examples": ["children: My son rides with the Frosthollow caravan. (rmx_edda_muddale, indoors/clear 20h)"]}, "T3_detail": {"by_kind": {"unbacked": 9, "own_name": 1, "listener_third": 1}, "examples": ["unbacked: rm_liv_cinderby to rm_ivar_nailham: Good day, friend. I saw someone near Corwin Sootstack's door that night.", "unbacked: rm_wynn_rustworth to rmx_fenwick_mossdale: Anyway. I saw you at the well this morning.", "unbacked: rm_greta_gullton to rm_aud_dripgate: It doesn't stop there, either. Aud Kemp's been arrested. I saw h
