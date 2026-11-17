# Talk scorecard

Generated 2026-11-17T12:38:13+00:00 by `tools/talk_score.py` (Docs/Design/84 §15.3).

Runs: `L1`.
Shuffled control (talk_sim --ablate shuffle: lines after the first replaced by lines said at the same side of other conversations on the same topic, by a speaker of the same dialect where possible (§15.5)): L1.

| Id | Metric | Value | Shuffled control | Separation | Target | Gate | Passes |
|---|---|---|---|---|---|---|---|
| R1 | first identical conversation, per listener (h) | 4.26 | 24.4 | 83% (the control scores better) | 40 h+; hard 20 h+ | hard | no |
| R2 | first repeated opener, per listener (h) | 3.67 | 3.67 | 0% (the control scores better) | 8 h+ | soft | no |
| R3 | distinct openers per 3,000 exchanges | 2,784 | 2,784 | 0% (the control scores better) | L1 1,500+ | soft | yes |
| R4 | distinctive line heard again within 10 h, per 100 h | 1,331 | 1,458 | 9% (within 30%) | 5 or fewer | soft | no |
| R5 | collision index, per 100 h | 0 | 142 | 100% | 1 or fewer; hard 3 | hard | yes |
| R6 | most-spoken line's share (%) | 0.925 | 0.896 | 3% (the control scores better) | 0.4% | soft | no |
| R7 | distinct reply entries per 1,000 reply beats | 73.9 | 69.7 | 6% (within 30%) | 500+ | soft | no |
| R8 | habit saturation (% of a speaker's lines, mean) | 0.0197 | 0.0474 | 58% | 15% or less | soft | yes |
| C1 | owed answers answered, by moves (%) | 95.3 | 24.3 | 75% (coherence) | 97%; hard 92% | hard | yes |
| C1Q | questions answered: the '?' heuristic (%) | 94.4 | 84.7 | 10% (within 30%) | diagnostic (C1 until Phase 3; doesn't separate from the control) | soft | n/a |
| M1 | lines carrying moves (%) | 100 | 100 | 0% (the control scores better) | 100% at Phase 7 (every entry) | soft | n/a |
| C2 | replies at the best fit available (%) | 100 | 100 | 0% (the control scores better) | 95% | soft | yes |
| C3 | endings by a farewell or a marked break (%) | 98.9 | 83.5 | 16% (within 30%) (coherence) | 80% / 3% / 5% | soft | yes |
| C4 | tone clashes per 1,000 beats | 0 | 0.207 | 100% (coherence) | 0 | hard | yes |
| C5 | unmarked topic jumps per 1,000 beats | 0 | 0.454 | 100% | 5 | soft | yes |
| C6 | same entry or question twice in a conversation, per 1,000 | 0 | 107 | 100% (coherence) | 0 | hard | yes |
| C7 | lines per conversation (mean) | 3.52 | 3.52 | 0% (the control scores better) (structural) | 5-7; p90 9+ | soft | no |
| C8 | conversations with a third speaker (%) | 0.64 | 0.64 | 0% (the control scores better) (structural) | 5-15% | soft | no |
| C9 | callbacks per 10 conversations | 1.68 | 1.68 | 0% (the control scores better) | 1.5+ | soft | yes |
| T1 | raw ids spoken, per 1,000 lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T2 | presupposition failures, per 1,000 lines | 0 | 5.82 | 100% | 0 | hard | yes |
| T3 | self-reference errors (own name, the listener in the third person, unbacked eyewitness words), per 1,000 lines | 0 | 0.124 | 100% | 0 | hard | yes |
| T4 | identity drift: a stance changed with no event or concede, or answered against one's own, per 1,000 stance lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T6 | 'they' said of one known wolf named in the line, per 1,000 lines | 3.26 | 3.18 | 3% (the control scores better) | 0 | soft | no |
| D1A | lines said a step broader or plainer than the speaker's own accent (%) | 3.46 | 3.46 | 0% (the control scores better) | diagnostic: accommodation (§10.6) | soft | n/a |
| O1 | opinions given per 10 conversations | 0.0247 | 0.00145 | 94% | diagnostic (§8.8) | soft | n/a |
| T5 | telling what the listener already believes, with no 'known' answer (%) | 0 | 0 | 0% (the control scores better) | 2% | soft | yes |
| D1 | lines with a dialect's words (%): accent none | 0 | 1.27 | 100% | none 0% | hard | yes |
| D1L | lines with a dialect's words (%): light speakers | 0.694 | 0.363 | 48% | light 25-50% | soft | no |
| D1B | lines with a dialect's words (%): broad speakers | 0.694 | 0.51 | 27% (within 30%) | broad 60%+ | soft | no |
| D2 | another dialect's words in a speaker's line | 0 | 2 | 100% | 0 | hard | yes |
| D3 | dialect guessed from 20 lines (%): light and broad | 85.6 | 91.9 | 7% (the control scores better) | 85%+ | soft | yes |
| U1 | a resident's lines from their own words (%) | 0 | 0 | 0% (the control scores better) | 20-35% | soft | no |
| U2 | distinguishability of look-alike residents (%) | 54.4 | 53.6 | 1% (within 30%) | 90%+ | soft | no |
| K1 | conversations on everyday topics (%) | 45.3 | 45.3 | 0% (the control scores better) | 35%+ | soft | yes |
| K2 | largest topic's share (%) | 24.3 | 24.3 | 0% (the control scores better) | 25% or less | soft | yes |
| K3 | same news told twice to the same resident | 2 | 2 | 0% (the control scores better) | 0 | hard | no |
| W1 | window lines from mantles (%) | n/a | | | 40%+ | soft | n/a until Phase 13 |
| W2 | free-text understanding | n/a | | | 85%+; wrong 8% or less | hard | n/a until Phase 13 |
| W3 | a resident answers chat with the option off | n/a | | | 0 | hard | n/a until Phase 12 |
| B1 | one beat: the director's mean (ms) | 0.177 | 0.177 | 0% (the control scores better) | 0.6 ms or less (gate: talk_bench) | soft | yes |
| B1P | one beat: the director's 99th percentile (ms) | 0.617 | 0.617 | 0% (the control scores better) | 3 ms or less (gate: talk_bench) | soft | yes |
| B1L | a director look: 99th percentile (ms) | 7.49 | 7.49 | 0% (the control scores better) | 5 ms or less (gate: talk_bench) | soft | no |
| B5 | exchanges simulated a second | 2.28 | 2.28 | 0% (the control scores better) | 150+ | hard | no |
| X1 | lexical cohesion: replies sharing a content word with the line before (%) | 23.9 | 2.46 | 90% (coherence) | diagnostic (not in §15.3): shows the control is discriminating | soft | n/a |
| H1 | blind naturalness 1-5 | n/a | | | 3.8+ | soft | n/a until slice ends (talk_rate.py) |
| H2 | 'written whole by a writer?' (%) | n/a | | | 40%+ | soft | n/a until slice ends (talk_rate.py) |

## By profile

| Id | L1 |
|---|---|
| R1 | 4.26 |
| R2 | 3.67 |
| R3 | 2,784 |
| R4 | 1,331 |
| R5 | 0 |
| R6 | 0.925 |
| R7 | 73.9 |
| R8 | 0.02 |
| C1 | 95.3 |
| C1Q | 94.4 |
| M1 | 100 |
| C2 | 100 |
| C3 | 98.9 |
| C4 | 0 |
| C5 | 0 |
| C6 | 0 |
| C7 | 3.52 |
| C8 | 0.64 |
| C9 | 1.68 |
| T1 | 0 |
| T2 | 0 |
| T3 | 0 |
| T4 | 0 |
| T6 | 3.26 |
| D1A | 3.46 |
| O1 | 0.025 |
| T5 | 0 |
| D1 | 0 |
| D1L | 0.694 |
| D1B | 0.694 |
| D2 | 0 |
| D3 | 85.6 |
| U1 | 0 |
| U2 | 54.4 |
| K1 | 45.3 |
| K2 | 24.3 |
| K3 | 2 |
| B1 | 0.177 |
| B1P | 0.617 |
| B1L | 7.49 |
| B5 | 2.28 |
| X1 | 23.9 |

## Details

- **L1**: {"R1_detail": {"listeners": 4, "median": 8.078611109230163, "censored": false, "exchange": 160}, "R2_detail": {"median": 4.512749998949293, "censored": false}, "R6_detail": {"sentence": "Good.", "times": 224, "line": "Off home, then.", "line_pct": 0.578}, "R7_detail": {"replies": 14674, "closing_beats": 2658, "closings_distinct_per_1000": 85.4}, "R8_detail": {"p90": 0.0, "speakers": 134, "lines20": 23955, "any_habit_mean": 0.048110412156412784}, "C1Q_detail": {"questions": 1931, "answered": 1822}, "C1_detail": {"owed": 1956, "met": 1865}, "C2_detail": {"answers": 1813, "fallbacks": 0}, "M1_detail": {"scene": 100.0, "written": 100.0}, "C3_detail": {"farewell": 19.95346131471786, "marked_break": 0.0, "cut_off": 1.0907504363001745, "cap": 0.0, "nothing_fits": 0.04363001745200698, "curt": 78.91215823152996}, "C4_detail": {"clashes": 0, "by_tone": 0, "in_line": 0}, "C7_detail": {"p90": 6, "three_lines": 64.49970913321698, "four_lines": 3.6649214659685865}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"by_kind": {}, "examples": [], "eyewitness_judged": true}, "T4_detail": {"stance_lines": 33, "drifted": 0, "answered_against_own": 0, "examples": []}, "T6_detail": {"lines": 79, "across": 67, "examples": ["rm_gorm_pitchmarsh: More of it. We'd only just done talking of the last. A thief took Gudrun Mossdale's things. Hope they like them.", "rm_finn_saltwell: A thief took from Wenna Tarford. Bet they ate well tonight.", "rm_jorunn_ropewick: Another bad business. It never 
