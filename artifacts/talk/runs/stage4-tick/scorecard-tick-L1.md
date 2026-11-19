# Talk scorecard

Generated 2026-11-19T04:35:21+00:00 by `tools/talk_score.py` (Docs/Design/84 §15.3).

Runs: `L1`.
Shuffled control (talk_sim --ablate shuffle: lines after the first replaced by lines said at the same side of other conversations on the same topic, by a speaker of the same dialect where possible (§15.5)): L1.

| Id | Metric | Value | Shuffled control | Separation | Target | Gate | Passes |
|---|---|---|---|---|---|---|---|
| R1 | first identical conversation, per listener (h) | 3.69 | 12 | 69% (the control scores better) | 40 h+; hard 20 h+ | hard | no |
| R2 | first repeated opener, per listener (h) | 12.3 | 12.3 | 0% (the control scores better) | 8 h+ | soft | yes |
| R3 | distinct openers per 3,000 exchanges | 2,910 | 2,910 | 0% (the control scores better) | L1 1,500+ | soft | yes |
| R4 | distinctive line heard again within 10 h, per 100 h | 1,915 | 2,034 | 6% (within 30%) | 5 or fewer | soft | no |
| R5 | collision index, per 100 h | 0 | 156 | 100% | 1 or fewer; hard 3 | hard | yes |
| R6 | most-spoken line's share (%) | 1.14 | 1.18 | 3% (within 30%) | 0.4% | soft | no |
| R7 | distinct reply entries per 1,000 reply beats | 95 | 90.5 | 5% (within 30%) | 500+ | soft | no |
| R8 | habit saturation (% of a speaker's lines, mean) | 0.265 | 0.315 | 16% (within 30%) | 15% or less | soft | yes |
| C1 | owed answers answered, by moves (%) | 96 | 25.4 | 74% (coherence) | 97%; hard 92% | hard | yes |
| C1Q | questions answered: the '?' heuristic (%) | 95 | 80.8 | 15% (within 30%) | diagnostic (C1 until Phase 3; doesn't separate from the control) | soft | n/a |
| M1 | lines carrying moves (%) | 100 | 100 | 0% (the control scores better) | 100% at Phase 7 (every entry) | soft | n/a |
| C2 | replies at the best fit available (%) | 99.6 | 99.5 | 0% (within 30%) | 95% | soft | yes |
| C3 | endings by a farewell or a marked break (%) | 98.4 | 80 | 19% (within 30%) (coherence) | 80% / 3% / 5% | soft | yes |
| C4 | tone clashes per 1,000 beats | 0 | 0.295 | 100% (coherence) | 0 | hard | yes |
| C5 | unmarked topic jumps per 1,000 beats | 0 | 0 | 0% (the control scores better) | 5 | soft | yes |
| C6 | same entry or question twice in a conversation, per 1,000 | 0 | 113 | 100% (coherence) | 0 | hard | yes |
| C7 | lines per conversation (mean) | 3.63 | 3.63 | 0% (the control scores better) (structural) | 5-7; p90 9+ | soft | no |
| C8 | conversations with a third speaker (%) | 0.762 | 0.762 | 0% (the control scores better) (structural) | 5-15% | soft | no |
| C9 | callbacks per 10 conversations | 1.8 | 1.8 | 0% (the control scores better) | 1.5+ | soft | yes |
| T1 | raw ids spoken, per 1,000 lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T2 | presupposition failures, per 1,000 lines | 0 | 8.88 | 100% | 0 | hard | yes |
| T3 | self-reference errors (own name, the listener in the third person, unbacked eyewitness words), per 1,000 lines | 0 | 0.147 | 100% | 0 | hard | yes |
| T4 | identity drift: a stance changed with no event or concede, or answered against one's own, per 1,000 stance lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T6 | 'they' said of one known wolf named in the line, per 1,000 lines | 5.31 | 3.72 | 30% (the control scores better) | 0 | soft | no |
| D1A | lines said a step broader or plainer than the speaker's own accent (%) | 11.9 | 11.9 | 0% (the control scores better) | diagnostic: accommodation (§10.6) | soft | n/a |
| O1 | opinions given per 10 conversations | 0 | 0 | 0% (the control scores better) | diagnostic (§8.8) | soft | n/a |
| T5 | telling what the listener already believes, with no 'known' answer (%) | 0 | 0 | 0% (the control scores better) | 2% | soft | yes |
| D1 | lines with a dialect's words (%): accent none | 0 | 2.66 | 100% | none 0% | hard | yes |
| D1L | lines with a dialect's words (%): light speakers | 2.76 | 2.76 | 0% (within 30%) | light 25-50% | soft | no |
| D1B | lines with a dialect's words (%): broad speakers | 6.18 | 5.66 | 8% (within 30%) | broad 60%+ | soft | no |
| D2 | another dialect's words in a speaker's line | 0 | 1 | 100% | 0 | hard | yes |
| D3 | dialect guessed from 20 lines (%): light and broad | 83.3 | 87.3 | 4% (the control scores better) | 85%+ | soft | no |
| U1 | a resident's lines from their own words (%) | 15.4 | 6.99 | 55% | 20-35% | soft | no |
| U2 | distinguishability of look-alike residents (%) | 59.1 | 55.8 | 6% (within 30%) | 90%+ | soft | no |
| K1 | conversations on everyday topics (%) | 45.1 | 45.1 | 0% (the control scores better) | 35%+ | soft | yes |
| K2 | largest topic's share (%) | 23.2 | 23.2 | 0% (the control scores better) | 25% or less | soft | yes |
| K3 | same news told twice to the same resident | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| W1 | window lines from mantles (%) | n/a | | | 40%+ | soft | n/a until Phase 13 |
| W2 | free-text understanding | n/a | | | 85%+; wrong 8% or less | hard | n/a until Phase 13 |
| W3 | a resident answers chat with the option off | n/a | | | 0 | hard | n/a until Phase 12 |
| B1 | one beat: the director's mean (ms) | 0.203 | 0.203 | 0% (the control scores better) | 0.6 ms or less (gate: talk_bench) | soft | yes |
| B1P | one beat: the director's 99th percentile (ms) | 0.839 | 0.839 | 0% (the control scores better) | 3 ms or less (gate: talk_bench) | soft | yes |
| B1L | a director look: 99th percentile (ms) | 8.72 | 8.72 | 0% (the control scores better) | 5 ms or less (gate: talk_bench) | soft | no |
| B5 | exchanges simulated a second | 2.09 | 2.09 | 0% (the control scores better) | 150+ | hard | no |
| X1 | lexical cohesion: replies sharing a content word with the line before (%) | 25.1 | 2.24 | 91% (coherence) | diagnostic (not in §15.3): shows the control is discriminating | soft | n/a |
| H1 | blind naturalness 1-5 | n/a | | | 3.8+ | soft | n/a until slice ends (talk_rate.py) |
| H2 | 'written whole by a writer?' (%) | n/a | | | 40%+ | soft | n/a until slice ends (talk_rate.py) |

## By profile

| Id | L1 |
|---|---|
| R1 | 3.69 |
| R2 | 12.2 |
| R3 | 2,910 |
| R4 | 1,915 |
| R5 | 0 |
| R6 | 1.14 |
| R7 | 95 |
| R8 | 0.265 |
| C1 | 96 |
| C1Q | 95 |
| M1 | 100 |
| C2 | 99.6 |
| C3 | 98.4 |
| C4 | 0 |
| C5 | 0 |
| C6 | 0 |
| C7 | 3.63 |
| C8 | 0.762 |
| C9 | 1.8 |
| T1 | 0 |
| T2 | 0 |
| T3 | 0 |
| T4 | 0 |
| T6 | 5.31 |
| D1A | 11.9 |
| O1 | 0 |
| T5 | 0 |
| D1 | 0 |
| D1L | 2.76 |
| D1B | 6.18 |
| D2 | 0 |
| D3 | 83.3 |
| U1 | 15.4 |
| U2 | 59.1 |
| K1 | 45.1 |
| K2 | 23.2 |
| K3 | 0 |
| B1 | 0.203 |
| B1P | 0.839 |
| B1L | 8.72 |
| B5 | 2.09 |
| X1 | 25.1 |

## Details

- **L1**: {"R1_detail": {"listeners": 8, "median": 23.75188891100956, "censored": true, "exchange": 83}, "R2_detail": {"median": 19.69511112945361, "censored": true}, "R6_detail": {"sentence": "Mind how you go.", "times": 308, "line": "Can't say I'm surprised.", "line_pct": 0.494}, "R7_detail": {"replies": 16199, "closing_beats": 3447, "closings_distinct_per_1000": 247.5}, "R8_detail": {"p90": 0.8695652173913043, "speakers": 127, "lines20": 26800, "any_habit_mean": 0.44436723919246773}, "C1Q_detail": {"questions": 2525, "answered": 2400}, "C1_detail": {"owed": 2522, "met": 2420}, "C2_detail": {"answers": 2422, "fallbacks": 0}, "M1_detail": {"scene": 100.0, "written": 100.0}, "C3_detail": {"farewell": 23.38946805666934, "marked_break": 0.0, "cut_off": 1.537022186581128, "cap": 0.0, "nothing_fits": 0.02673082063619353, "curt": 75.04677893611334}, "C4_detail": {"clashes": 0, "by_tone": 0, "in_line": 0}, "C7_detail": {"p90": 6, "three_lines": 59.997326917936384, "four_lines": 3.354717989842288}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"by_kind": {}, "examples": [], "eyewitness_judged": true}, "T4_detail": {"stance_lines": 0, "drifted": 0, "answered_against_own": 0, "examples": []}, "T6_detail": {"lines": 144, "across": 129, "examples": ["rm_percival_rustford: Morning. A light-pawed sort took from Wenna Tarford. Bet they ate well tonight.", "rmx_fenwick_mossdale: Another theft. Wenna Tarford. They should make an example.", "rm_vigdis_pitchmarsh: More trouble. We were on 
