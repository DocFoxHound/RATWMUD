# Talk scorecard

Generated 2026-11-19T04:35:30+00:00 by `tools/talk_score.py` (Docs/Design/84 §15.3).

Runs: `L1`.
Shuffled control (talk_sim --ablate shuffle: lines after the first replaced by lines said at the same side of other conversations on the same topic, by a speaker of the same dialect where possible (§15.5)): L1.

| Id | Metric | Value | Shuffled control | Separation | Target | Gate | Passes |
|---|---|---|---|---|---|---|---|
| R1 | first identical conversation, per listener (h) | 23.8 | 12 | 49% | 40 h+; hard 20 h+ | hard | yes |
| R2 | first repeated opener, per listener (h) | 12.3 | 12.3 | 0% (the control scores better) | 8 h+ | soft | yes |
| R3 | distinct openers per 3,000 exchanges | 2,958 | 2,958 | 0% (the control scores better) | L1 1,500+ | soft | yes |
| R4 | distinctive line heard again within 10 h, per 100 h | 210 | 230 | 9% (within 30%) | 5 or fewer | soft | no |
| R5 | collision index, per 100 h | 0 | 2 | 100% | 1 or fewer; hard 3 | hard | yes |
| R6 | most-spoken line's share (%) | 2.39 | 1.98 | 17% (the control scores better) | 0.4% | soft | no |
| R7 | distinct reply entries per 1,000 reply beats | 205 | 270 | 24% (the control scores better) | 500+ | soft | no |
| R8 | habit saturation (% of a speaker's lines, mean) | 1.23 | 0.986 | 20% (the control scores better) | 15% or less | soft | yes |
| C1 | owed answers answered, by moves (%) | 99.6 | 41.7 | 58% (coherence) | 97%; hard 92% | hard | yes |
| C1Q | questions answered: the '?' heuristic (%) | 91.2 | 82.8 | 9% (within 30%) | diagnostic (C1 until Phase 3; doesn't separate from the control) | soft | n/a |
| M1 | lines carrying moves (%) | 100 | 100 | 0% (the control scores better) | 100% at Phase 7 (every entry) | soft | n/a |
| C2 | replies at the best fit available (%) | 100 | 99.6 | 0% (within 30%) | 95% | soft | yes |
| C3 | endings by a farewell or a marked break (%) | 96.1 | 86.7 | 10% (within 30%) (coherence) | 80% / 3% / 5% | soft | yes |
| C4 | tone clashes per 1,000 beats | 0 | 0 | 0% (the control scores better) (coherence) | 0 | hard | yes |
| C5 | unmarked topic jumps per 1,000 beats | 0 | 0 | 0% (the control scores better) | 5 | soft | yes |
| C6 | same entry or question twice in a conversation, per 1,000 | 0 | 94.2 | 100% (coherence) | 0 | hard | yes |
| C7 | lines per conversation (mean) | 3.17 | 3.17 | 0% (the control scores better) (structural) | 5-7; p90 9+ | soft | no |
| C8 | conversations with a third speaker (%) | 0 | 0 | 0% (the control scores better) (structural) | 5-15% | soft | no |
| C9 | callbacks per 10 conversations | 1.47 | 1.47 | 0% (the control scores better) | 1.5+ | soft | no |
| T1 | raw ids spoken, per 1,000 lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T2 | presupposition failures, per 1,000 lines | 0 | 3.15 | 100% | 0 | hard | yes |
| T3 | self-reference errors (own name, the listener in the third person, unbacked eyewitness words), per 1,000 lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T4 | identity drift: a stance changed with no event or concede, or answered against one's own, per 1,000 stance lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T6 | 'they' said of one known wolf named in the line, per 1,000 lines | 0.451 | 0 | 100% (the control scores better) | 0 | soft | no |
| D1A | lines said a step broader or plainer than the speaker's own accent (%) | 97.6 | 97.6 | 0% (the control scores better) | diagnostic: accommodation (§10.6) | soft | n/a |
| O1 | opinions given per 10 conversations | 0 | 0 | 0% (the control scores better) | diagnostic (§8.8) | soft | n/a |
| T5 | telling what the listener already believes, with no 'known' answer (%) | 0 | 0 | 0% (the control scores better) | 2% | soft | yes |
| D1 | lines with a dialect's words (%): accent none | 0 | 0 | 0% (the control scores better) | none 0% | hard | yes |
| D1L | lines with a dialect's words (%): light speakers | 35.3 | 41.2 | 14% (the control scores better) | light 25-50% | soft | yes |
| D1B | lines with a dialect's words (%): broad speakers | 22.4 | 20.4 | 9% (within 30%) | broad 60%+ | soft | no |
| D2 | another dialect's words in a speaker's line | 0 | 1 | 100% | 0 | hard | yes |
| D3 | dialect guessed from 20 lines (%): light and broad | 50 | 50 | 0% (the control scores better) | 85%+ | soft | no |
| U1 | a resident's lines from their own words (%) | 20 | 15.3 | 23% (within 30%) | 20-35% | soft | yes |
| U2 | distinguishability of look-alike residents (%) | 0 | 0 | 0% (the control scores better) | 90%+ | soft | no |
| K1 | conversations on everyday topics (%) | 50.1 | 50.1 | 0% (the control scores better) | 35%+ | soft | yes |
| K2 | largest topic's share (%) | 25.4 | 25.4 | 0% (the control scores better) | 25% or less | soft | no |
| K3 | same news told twice to the same resident | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| W1 | window lines from mantles (%) | n/a | | | 40%+ | soft | n/a until Phase 13 |
| W2 | free-text understanding | n/a | | | 85%+; wrong 8% or less | hard | n/a until Phase 13 |
| W3 | a resident answers chat with the option off | n/a | | | 0 | hard | n/a until Phase 12 |
| B1 | one beat: the director's mean (ms) | 0.203 | 0.203 | 0% (the control scores better) | 0.6 ms or less (gate: talk_bench) | soft | yes |
| B1P | one beat: the director's 99th percentile (ms) | 0.839 | 0.839 | 0% (the control scores better) | 3 ms or less (gate: talk_bench) | soft | yes |
| B1L | a director look: 99th percentile (ms) | 8.72 | 8.72 | 0% (the control scores better) | 5 ms or less (gate: talk_bench) | soft | no |
| B5 | exchanges simulated a second | 2.09 | 2.09 | 0% (the control scores better) | 150+ | hard | no |
| X1 | lexical cohesion: replies sharing a content word with the line before (%) | 19.8 | 2.38 | 88% (coherence) | diagnostic (not in §15.3): shows the control is discriminating | soft | n/a |
| H1 | blind naturalness 1-5 | n/a | | | 3.8+ | soft | n/a until slice ends (talk_rate.py) |
| H2 | 'written whole by a writer?' (%) | n/a | | | 40%+ | soft | n/a until slice ends (talk_rate.py) |

## By profile

| Id | L1 |
|---|---|
| R1 | 23.8 |
| R2 | 12.2 |
| R3 | 2,958 |
| R4 | 210 |
| R5 | 0 |
| R6 | 2.39 |
| R7 | 205 |
| R8 | 1.23 |
| C1 | 99.6 |
| C1Q | 91.2 |
| M1 | 100 |
| C2 | 100 |
| C3 | 96.1 |
| C4 | 0 |
| C5 | 0 |
| C6 | 0 |
| C7 | 3.17 |
| C8 | 0 |
| C9 | 1.47 |
| T1 | 0 |
| T2 | 0 |
| T3 | 0 |
| T4 | 0 |
| T6 | 0.451 |
| D1A | 97.6 |
| O1 | 0 |
| T5 | 0 |
| D1 | 0 |
| D1L | 35.3 |
| D1B | 22.4 |
| D2 | 0 |
| D3 | 50 |
| U1 | 20 |
| U2 | 0 |
| K1 | 50.1 |
| K2 | 25.4 |
| K3 | 0 |
| B1 | 0.203 |
| B1P | 0.839 |
| B1L | 8.72 |
| B5 | 2.09 |
| X1 | 19.8 |

## Details

- **L1**: {"R1_detail": {"listeners": 4, "median": 75.57411118149498, "censored": true, "exchange": 106}, "R2_detail": {"median": 75.57411118149498, "censored": true}, "R6_detail": {"sentence": "We'll all go short together.", "times": 53, "line": "Don't I know it.", "line_pct": 2.118}, "R7_detail": {"replies": 1334, "closing_beats": 184, "closings_distinct_per_1000": 494.6}, "R8_detail": {"p90": 1.8111254851228977, "speakers": 4, "lines20": 2180, "any_habit_mean": 1.9580507799887183}, "C1Q_detail": {"questions": 215, "answered": 196}, "C1_detail": {"owed": 263, "met": 262}, "C2_detail": {"answers": 262, "fallbacks": 0}, "M1_detail": {"scene": 100.0}, "C3_detail": {"farewell": 13.266761768901569, "marked_break": 0.0, "cut_off": 3.708987161198288, "cap": 0.0, "nothing_fits": 0.14265335235378032, "curt": 82.88159771754636}, "C4_detail": {"clashes": 0, "by_tone": 0, "in_line": 0}, "C7_detail": {"p90": 6, "three_lines": 58.63052781740371, "four_lines": 0.5706134094151213}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"by_kind": {}, "examples": [], "eyewitness_judged": true}, "T4_detail": {"stance_lines": 0, "drifted": 0, "answered_against_own": 0, "examples": []}, "T6_detail": {"lines": 1, "across": 1, "examples": ["Did you hear? Maren Coalwick and Rowan Cragpeak are wed. / tn_oh_ludo_ashwood: I never thought they'd do it."]}, "T1_detail": {"examples": []}, "K2_detail": {"topics": {"prices": 25.39, "work": 11.98, "caravan": 8.84, "animals": 6.85, "player": 6.7, "food": 6.42, 
