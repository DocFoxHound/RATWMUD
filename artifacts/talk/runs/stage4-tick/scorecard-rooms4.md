# Talk scorecard

Generated 2026-11-19T04:35:29+00:00 by `tools/talk_score.py` (Docs/Design/84 §15.3).

Runs: `L1`.
Shuffled control (talk_sim --ablate shuffle: lines after the first replaced by lines said at the same side of other conversations on the same topic, by a speaker of the same dialect where possible (§15.5)): L1.

| Id | Metric | Value | Shuffled control | Separation | Target | Gate | Passes |
|---|---|---|---|---|---|---|---|
| R1 | first identical conversation, per listener (h) | 3.69 | 19.1 | 81% (the control scores better) | 40 h+; hard 20 h+ | hard | no |
| R2 | first repeated opener, per listener (h) | 12.9 | 12.9 | 0% (the control scores better) | 8 h+ | soft | yes |
| R3 | distinct openers per 3,000 exchanges | 2,863 | 2,863 | 0% (the control scores better) | L1 1,500+ | soft | yes |
| R4 | distinctive line heard again within 10 h, per 100 h | 1,705 | 1,804 | 5% (within 30%) | 5 or fewer | soft | no |
| R5 | collision index, per 100 h | 0 | 154 | 100% | 1 or fewer; hard 3 | hard | yes |
| R6 | most-spoken line's share (%) | 1.23 | 1.25 | 2% (within 30%) | 0.4% | soft | no |
| R7 | distinct reply entries per 1,000 reply beats | 93.8 | 93.2 | 1% (within 30%) | 500+ | soft | no |
| R8 | habit saturation (% of a speaker's lines, mean) | 0.233 | 0.293 | 20% (within 30%) | 15% or less | soft | yes |
| C1 | owed answers answered, by moves (%) | 95.5 | 23.5 | 75% (coherence) | 97%; hard 92% | hard | yes |
| C1Q | questions answered: the '?' heuristic (%) | 95.4 | 80.6 | 15% (within 30%) | diagnostic (C1 until Phase 3; doesn't separate from the control) | soft | n/a |
| M1 | lines carrying moves (%) | 100 | 100 | 0% (the control scores better) | 100% at Phase 7 (every entry) | soft | n/a |
| C2 | replies at the best fit available (%) | 99.5 | 99.4 | 0% (within 30%) | 95% | soft | yes |
| C3 | endings by a farewell or a marked break (%) | 98.7 | 79.3 | 20% (within 30%) (coherence) | 80% / 3% / 5% | soft | yes |
| C4 | tone clashes per 1,000 beats | 0 | 0.321 | 100% (coherence) | 0 | hard | yes |
| C5 | unmarked topic jumps per 1,000 beats | 0 | 0 | 0% (the control scores better) | 5 | soft | yes |
| C6 | same entry or question twice in a conversation, per 1,000 | 0 | 115 | 100% (coherence) | 0 | hard | yes |
| C7 | lines per conversation (mean) | 3.67 | 3.67 | 0% (the control scores better) (structural) | 5-7; p90 9+ | soft | no |
| C8 | conversations with a third speaker (%) | 0.841 | 0.841 | 0% (the control scores better) (structural) | 5-15% | soft | no |
| C9 | callbacks per 10 conversations | 1.83 | 1.83 | 0% (the control scores better) | 1.5+ | soft | yes |
| T1 | raw ids spoken, per 1,000 lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T2 | presupposition failures, per 1,000 lines | 0 | 9.39 | 100% | 0 | hard | yes |
| T3 | self-reference errors (own name, the listener in the third person, unbacked eyewitness words), per 1,000 lines | 0 | 0.161 | 100% | 0 | hard | yes |
| T4 | identity drift: a stance changed with no event or concede, or answered against one's own, per 1,000 stance lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T6 | 'they' said of one known wolf named in the line, per 1,000 lines | 5.74 | 4.05 | 29% (the control scores better) | 0 | soft | no |
| D1A | lines said a step broader or plainer than the speaker's own accent (%) | 4.21 | 4.21 | 0% (the control scores better) | diagnostic: accommodation (§10.6) | soft | n/a |
| O1 | opinions given per 10 conversations | 0 | 0 | 0% (the control scores better) | diagnostic (§8.8) | soft | n/a |
| T5 | telling what the listener already believes, with no 'known' answer (%) | 0 | 0 | 0% (the control scores better) | 2% | soft | yes |
| D1 | lines with a dialect's words (%): accent none | 0 | 2.66 | 100% | none 0% | hard | yes |
| D1L | lines with a dialect's words (%): light speakers | 2.64 | 2.62 | 1% (within 30%) | light 25-50% | soft | no |
| D1B | lines with a dialect's words (%): broad speakers | 1.9 | 1.8 | 5% (within 30%) | broad 60%+ | soft | no |
| D2 | another dialect's words in a speaker's line | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| D3 | dialect guessed from 20 lines (%): light and broad | 86.7 | 89.8 | 3% (the control scores better) | 85%+ | soft | yes |
| U1 | a resident's lines from their own words (%) | 15 | 6.24 | 58% | 20-35% | soft | no |
| U2 | distinguishability of look-alike residents (%) | 59.1 | 55.8 | 6% (within 30%) | 90%+ | soft | no |
| K1 | conversations on everyday topics (%) | 44.6 | 44.6 | 0% (the control scores better) | 35%+ | soft | yes |
| K2 | largest topic's share (%) | 25.6 | 25.6 | 0% (the control scores better) | 25% or less | soft | no |
| K3 | same news told twice to the same resident | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| W1 | window lines from mantles (%) | n/a | | | 40%+ | soft | n/a until Phase 13 |
| W2 | free-text understanding | n/a | | | 85%+; wrong 8% or less | hard | n/a until Phase 13 |
| W3 | a resident answers chat with the option off | n/a | | | 0 | hard | n/a until Phase 12 |
| B1 | one beat: the director's mean (ms) | 0.203 | 0.203 | 0% (the control scores better) | 0.6 ms or less (gate: talk_bench) | soft | yes |
| B1P | one beat: the director's 99th percentile (ms) | 0.839 | 0.839 | 0% (the control scores better) | 3 ms or less (gate: talk_bench) | soft | yes |
| B1L | a director look: 99th percentile (ms) | 8.72 | 8.72 | 0% (the control scores better) | 5 ms or less (gate: talk_bench) | soft | no |
| B5 | exchanges simulated a second | 2.09 | 2.09 | 0% (the control scores better) | 150+ | hard | no |
| X1 | lexical cohesion: replies sharing a content word with the line before (%) | 25.6 | 2.23 | 91% (coherence) | diagnostic (not in §15.3): shows the control is discriminating | soft | n/a |
| H1 | blind naturalness 1-5 | n/a | | | 3.8+ | soft | n/a until slice ends (talk_rate.py) |
| H2 | 'written whole by a writer?' (%) | n/a | | | 40%+ | soft | n/a until slice ends (talk_rate.py) |

## By profile

| Id | L1 |
|---|---|
| R1 | 3.69 |
| R2 | 12.9 |
| R3 | 2,863 |
| R4 | 1,705 |
| R5 | 0 |
| R6 | 1.23 |
| R7 | 93.8 |
| R8 | 0.233 |
| C1 | 95.5 |
| C1Q | 95.4 |
| M1 | 100 |
| C2 | 99.5 |
| C3 | 98.7 |
| C4 | 0 |
| C5 | 0 |
| C6 | 0 |
| C7 | 3.67 |
| C8 | 0.841 |
| C9 | 1.83 |
| T1 | 0 |
| T2 | 0 |
| T3 | 0 |
| T4 | 0 |
| T6 | 5.74 |
| D1A | 4.21 |
| O1 | 0 |
| T5 | 0 |
| D1 | 0 |
| D1L | 2.64 |
| D1B | 1.9 |
| D2 | 0 |
| D3 | 86.7 |
| U1 | 15 |
| U2 | 59.1 |
| K1 | 44.6 |
| K2 | 25.6 |
| K3 | 0 |
| B1 | 0.203 |
| B1P | 0.839 |
| B1L | 8.72 |
| B5 | 2.09 |
| X1 | 25.6 |

## Details

- **L1**: {"R1_detail": {"listeners": 4, "median": 20.04720835200375, "censored": false, "exchange": 83}, "R2_detail": {"median": 19.69511112945361, "censored": false}, "R6_detail": {"sentence": "Mind how you go.", "times": 306, "line": "Can't say I'm surprised.", "line_pct": 0.538}, "R7_detail": {"replies": 14865, "closing_beats": 3263, "closings_distinct_per_1000": 241.5}, "R8_detail": {"p90": 0.7692307692307693, "speakers": 123, "lines20": 24620, "any_habit_mean": 0.39514175819096364}, "C1Q_detail": {"questions": 2310, "answered": 2204}, "C1_detail": {"owed": 2259, "met": 2158}, "C2_detail": {"answers": 2160, "fallbacks": 0}, "M1_detail": {"scene": 100.0, "written": 100.0}, "C3_detail": {"farewell": 24.43592390502876, "marked_break": 0.0, "cut_off": 1.3124907830703436, "cap": 0.0, "nothing_fits": 0.014747087450228579, "curt": 74.23683822445066}, "C4_detail": {"clashes": 0, "by_tone": 0, "in_line": 0}, "C7_detail": {"p90": 6, "three_lines": 60.13862262203215, "four_lines": 3.642530600206459}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"by_kind": {}, "examples": [], "eyewitness_judged": true}, "T4_detail": {"stance_lines": 0, "drifted": 0, "answered_against_own": 0, "examples": []}, "T6_detail": {"lines": 143, "across": 128, "examples": ["rm_percival_rustford: Morning. A light-pawed sort took from Wenna Tarford. Bet they ate well tonight.", "rmx_fenwick_mossdale: Another theft. Wenna Tarford. They should make an example.", "rm_vigdis_pitchmarsh: More trouble. We were 
