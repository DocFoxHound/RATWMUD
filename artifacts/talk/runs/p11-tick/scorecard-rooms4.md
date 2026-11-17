# Talk scorecard

Generated 2026-11-17T17:30:36+00:00 by `tools/talk_score.py` (Docs/Design/84 §15.3).

Runs: `L1`.
Shuffled control (talk_sim --ablate shuffle: lines after the first replaced by lines said at the same side of other conversations on the same topic, by a speaker of the same dialect where possible (§15.5)): L1.

| Id | Metric | Value | Shuffled control | Separation | Target | Gate | Passes |
|---|---|---|---|---|---|---|---|
| R1 | first identical conversation, per listener (h) | 4.09 | 20.1 | 80% (the control scores better) | 40 h+; hard 20 h+ | hard | no |
| R2 | first repeated opener, per listener (h) | 4.09 | 4.09 | 0% (the control scores better) | 8 h+ | soft | no |
| R3 | distinct openers per 3,000 exchanges | 2,818 | 2,818 | 0% (the control scores better) | L1 1,500+ | soft | yes |
| R4 | distinctive line heard again within 10 h, per 100 h | 1,135 | 1,238 | 8% (within 30%) | 5 or fewer | soft | no |
| R5 | collision index, per 100 h | 0 | 108 | 100% | 1 or fewer; hard 3 | hard | yes |
| R6 | most-spoken line's share (%) | 0.923 | 0.923 | 0% (the control scores better) | 0.4% | soft | no |
| R7 | distinct reply entries per 1,000 reply beats | 75.3 | 74.9 | 0% (within 30%) | 500+ | soft | no |
| R8 | habit saturation (% of a speaker's lines, mean) | 0.0574 | 0.0588 | 2% (within 30%) | 15% or less | soft | yes |
| C1 | owed answers answered, by moves (%) | 95.5 | 24.2 | 75% (coherence) | 97%; hard 92% | hard | yes |
| C1Q | questions answered: the '?' heuristic (%) | 94.9 | 83 | 13% (within 30%) | diagnostic (C1 until Phase 3; doesn't separate from the control) | soft | n/a |
| M1 | lines carrying moves (%) | 100 | 100 | 0% (the control scores better) | 100% at Phase 7 (every entry) | soft | n/a |
| C2 | replies at the best fit available (%) | 99.6 | 99.5 | 0% (within 30%) | 95% | soft | yes |
| C3 | endings by a farewell or a marked break (%) | 98.5 | 82.9 | 16% (within 30%) (coherence) | 80% / 3% / 5% | soft | yes |
| C4 | tone clashes per 1,000 beats | 0 | 0.464 | 100% (coherence) | 0 | hard | yes |
| C5 | unmarked topic jumps per 1,000 beats | 0 | 0 | 0% (the control scores better) | 5 | soft | yes |
| C6 | same entry or question twice in a conversation, per 1,000 | 0 | 109 | 100% (coherence) | 0 | hard | yes |
| C7 | lines per conversation (mean) | 3.51 | 3.51 | 0% (the control scores better) (structural) | 5-7; p90 9+ | soft | no |
| C8 | conversations with a third speaker (%) | 0.391 | 0.391 | 0% (the control scores better) (structural) | 5-15% | soft | no |
| C9 | callbacks per 10 conversations | 1.68 | 1.68 | 0% (the control scores better) | 1.5+ | soft | yes |
| T1 | raw ids spoken, per 1,000 lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T2 | presupposition failures, per 1,000 lines | 0 | 3.85 | 100% | 0 | hard | yes |
| T3 | self-reference errors (own name, the listener in the third person, unbacked eyewitness words), per 1,000 lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T4 | identity drift: a stance changed with no event or concede, or answered against one's own, per 1,000 stance lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T6 | 'they' said of one known wolf named in the line, per 1,000 lines | 5.01 | 3.85 | 23% (the control scores better) | 0 | soft | no |
| D1A | lines said a step broader or plainer than the speaker's own accent (%) | 4.18 | 4.18 | 0% (the control scores better) | diagnostic: accommodation (§10.6) | soft | n/a |
| O1 | opinions given per 10 conversations | 0 | 0 | 0% (the control scores better) | diagnostic (§8.8) | soft | n/a |
| T5 | telling what the listener already believes, with no 'known' answer (%) | 0 | 0 | 0% (the control scores better) | 2% | soft | yes |
| D1 | lines with a dialect's words (%): accent none | 0 | 1.74 | 100% | none 0% | hard | yes |
| D1L | lines with a dialect's words (%): light speakers | 0.821 | 0.544 | 34% | light 25-50% | soft | no |
| D1B | lines with a dialect's words (%): broad speakers | 0.795 | 0.736 | 7% (within 30%) | broad 60%+ | soft | no |
| D2 | another dialect's words in a speaker's line | 1 | 3 | 67% | 0 | hard | no |
| D3 | dialect guessed from 20 lines (%): light and broad | 82.7 | 88.5 | 7% (the control scores better) | 85%+ | soft | no |
| U1 | a resident's lines from their own words (%) | 0.13 | 0.0789 | 39% | 20-35% | soft | no |
| U2 | distinguishability of look-alike residents (%) | 53.7 | 54.3 | 1% (the control scores better) | 90%+ | soft | no |
| K1 | conversations on everyday topics (%) | 44.8 | 44.8 | 0% (the control scores better) | 35%+ | soft | yes |
| K2 | largest topic's share (%) | 27.5 | 27.5 | 0% (the control scores better) | 25% or less | soft | no |
| K3 | same news told twice to the same resident | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| W1 | window lines from mantles (%) | n/a | | | 40%+ | soft | n/a until Phase 13 |
| W2 | free-text understanding | n/a | | | 85%+; wrong 8% or less | hard | n/a until Phase 13 |
| W3 | a resident answers chat with the option off | n/a | | | 0 | hard | n/a until Phase 12 |
| B1 | one beat: the director's mean (ms) | 0.219 | 0.219 | 0% (the control scores better) | 0.6 ms or less (gate: talk_bench) | soft | yes |
| B1P | one beat: the director's 99th percentile (ms) | 0.946 | 0.946 | 0% (the control scores better) | 3 ms or less (gate: talk_bench) | soft | yes |
| B1L | a director look: 99th percentile (ms) | 10.7 | 10.7 | 0% (the control scores better) | 5 ms or less (gate: talk_bench) | soft | no |
| B5 | exchanges simulated a second | 1.81 | 1.81 | 0% (the control scores better) | 150+ | hard | no |
| X1 | lexical cohesion: replies sharing a content word with the line before (%) | 24 | 2.31 | 90% (coherence) | diagnostic (not in §15.3): shows the control is discriminating | soft | n/a |
| H1 | blind naturalness 1-5 | n/a | | | 3.8+ | soft | n/a until slice ends (talk_rate.py) |
| H2 | 'written whole by a writer?' (%) | n/a | | | 40%+ | soft | n/a until slice ends (talk_rate.py) |

## By profile

| Id | L1 |
|---|---|
| R1 | 4.09 |
| R2 | 4.09 |
| R3 | 2,818 |
| R4 | 1,135 |
| R5 | 0 |
| R6 | 0.923 |
| R7 | 75.3 |
| R8 | 0.057 |
| C1 | 95.5 |
| C1Q | 94.9 |
| M1 | 100 |
| C2 | 99.6 |
| C3 | 98.5 |
| C4 | 0 |
| C5 | 0 |
| C6 | 0 |
| C7 | 3.51 |
| C8 | 0.391 |
| C9 | 1.68 |
| T1 | 0 |
| T2 | 0 |
| T3 | 0 |
| T4 | 0 |
| T6 | 5.01 |
| D1A | 4.18 |
| O1 | 0 |
| T5 | 0 |
| D1 | 0 |
| D1L | 0.821 |
| D1B | 0.795 |
| D2 | 1 |
| D3 | 82.7 |
| U1 | 0.13 |
| U2 | 53.7 |
| K1 | 44.8 |
| K2 | 27.5 |
| K3 | 0 |
| B1 | 0.219 |
| B1P | 0.946 |
| B1L | 10.7 |
| B5 | 1.81 |
| X1 | 24 |

## Details

- **L1**: {"R1_detail": {"listeners": 4, "median": 23.63254167908161, "censored": false, "exchange": 77}, "R2_detail": {"median": 23.63254167908161, "censored": false}, "R6_detail": {"sentence": "More of it.", "times": 199, "line": "Can't say I'm surprised.", "line_pct": 0.645}, "R7_detail": {"replies": 13002, "closing_beats": 2415, "closings_distinct_per_1000": 89.4}, "R8_detail": {"p90": 0.0, "speakers": 128, "lines20": 21251, "any_habit_mean": 0.07740317483035654}, "C1Q_detail": {"questions": 1696, "answered": 1610}, "C1_detail": {"owed": 1716, "met": 1638}, "C2_detail": {"answers": 1574, "fallbacks": 0}, "M1_detail": {"scene": 100.0, "written": 100.0}, "C3_detail": {"farewell": 20.37821975872188, "marked_break": 0.0, "cut_off": 1.4672318226279752, "cap": 0.0, "nothing_fits": 0.032605151613955004, "curt": 78.12194326703619}, "C4_detail": {"clashes": 0, "by_tone": 0, "in_line": 0}, "C7_detail": {"p90": 6, "three_lines": 63.694163677861106, "four_lines": 3.374633192044343}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"by_kind": {}, "examples": [], "eyewitness_judged": true}, "T4_detail": {"stance_lines": 0, "drifted": 0, "answered_against_own": 0, "examples": []}, "T6_detail": {"lines": 108, "across": 98, "examples": ["rmx_orm_blackwood: A cutpurse took from Lavinia Ashford. Bet they ate well tonight.", "rm_brenna_milldale: Another bad business. It never stops. A light-pawed sort took from Lavinia Ashford. Bet they ate well tonight.", "rm_ivar_nailham: A thief took Fri
