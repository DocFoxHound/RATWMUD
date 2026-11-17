# Talk scorecard

Generated 2026-11-17T06:20:30+00:00 by `tools/talk_score.py` (Docs/Design/84 §15.3).

Runs: `L1`.
Shuffled control (talk_sim --ablate shuffle: lines after the first replaced by lines said at the same side of other conversations on the same topic, by a speaker of the same dialect where possible (§15.5)): L1.

| Id | Metric | Value | Shuffled control | Separation | Target | Gate | Passes |
|---|---|---|---|---|---|---|---|
| R1 | first identical conversation, per listener (h) | 2.54 | 100 | 97% (the control scores better) | 40 h+; hard 20 h+ | hard | no |
| R2 | first repeated opener, per listener (h) | 2.54 | 2.54 | 0% (the control scores better) | 8 h+ | soft | no |
| R3 | distinct openers per 3,000 exchanges | 2,026 | 2,026 | 0% (the control scores better) | L1 1,500+ | soft | yes |
| R4 | distinctive line heard again within 10 h, per 100 h | 3,534 | 3,432 | 3% (the control scores better) | 5 or fewer | soft | no |
| R5 | collision index, per 100 h | 951 | 973 | 2% (within 30%) | 1 or fewer; hard 3 | hard | no |
| R6 | most-spoken line's share (%) | 0.679 | 0.697 | 3% (within 30%) | 0.4% | soft | no |
| R7 | distinct reply entries per 1,000 reply beats | 13.8 | 13.2 | 4% (within 30%) | 500+ | soft | no |
| R8 | habit saturation (% of a speaker's lines, mean) | 0 | 0 | 0% (the control scores better) | 15% or less | soft | yes |
| C1 | owed answers answered, by moves (%) | n/a | | | 97%; hard 92% | hard |  |
| C1Q | questions answered: the '?' heuristic (%) | 94.6 | 82.1 | 13% (within 30%) | diagnostic (C1 until Phase 3; doesn't separate from the control) | soft | n/a |
| M1 | lines carrying moves (%) | 0 | 0 | 0% (the control scores better) | 100% at Phase 7 (every entry) | soft | n/a |
| C2 | replies at the best fit available (%) | n/a | | | 95% | soft | n/a until Phase 3 |
| C3 | endings by a farewell or a marked break (%) | 1.29 | 0.86 | 33% (coherence) | 80% / 3% / 5% | soft | no |
| C4 | tone clashes per 1,000 beats | 0 | 1.07 | 100% (coherence) | 0 | hard | yes |
| C5 | unmarked topic jumps per 1,000 beats | n/a | | | 5 | soft | n/a until Phase 5 |
| C6 | same entry or question twice in a conversation, per 1,000 | 0 | 43.6 | 100% (coherence) | 0 | hard | yes |
| C7 | lines per conversation (mean) | 3.21 | 3.21 | 0% (the control scores better) (structural) | 5-7; p90 9+ | soft | no |
| C8 | conversations with a third speaker (%) | 0 | 0 | 0% (the control scores better) (structural) | 5-15% | soft | no |
| C9 | callbacks per 10 conversations | n/a | | | 1.5+ | soft | n/a until Phase 5 |
| T1 | raw ids spoken, per 1,000 lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T2 | presupposition failures, per 1,000 lines | 4.15 | 4.24 | 2% (within 30%) | 0 | hard | no |
| T3 | self-reference errors (own name, the listener in the third person, unbacked eyewitness words), per 1,000 lines | 0.536 | 0.625 | 14% (within 30%) | 0 | hard | no |
| T4 | identity drift: a stance changed with no event or concede, or answered against one's own, per 1,000 stance lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T6 | 'they' said of one known wolf named in the line, per 1,000 lines | 9.96 | 10 | 0% (within 30%) | 0 | soft | no |
| D1A | lines said a step broader or plainer than the speaker's own accent (%) | 0 | 0 | 0% (the control scores better) | diagnostic: accommodation (§10.6) | soft | n/a |
| O1 | opinions given per 10 conversations | 0 | 0 | 0% (the control scores better) | diagnostic (§8.8) | soft | n/a |
| T5 | telling what the listener already believes, with no 'known' answer (%) | n/a | | | 2% | soft |  |
| D1 | lines with a dialect's words (%): accent none | 0 | 0.031 | 100% | none 0% | hard | yes |
| D1L | lines with a dialect's words (%): light speakers | 0.139 | 0.0996 | 29% (within 30%) | light 25-50% | soft | no |
| D1B | lines with a dialect's words (%): broad speakers | 0.274 | 0.263 | 4% (within 30%) | broad 60%+ | soft | no |
| D2 | another dialect's words in a speaker's line | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| D3 | dialect guessed from 20 lines (%): light and broad | 38.4 | 54.7 | 30% (the control scores better) | 85%+ | soft | no |
| U1 | a resident's lines from their own words (%) | 0 | 0 | 0% (the control scores better) | 20-35% | soft | no |
| U2 | distinguishability of look-alike residents (%) | 65.6 | 61.3 | 7% (within 30%) | 90%+ | soft | no |
| K1 | conversations on everyday topics (%) | 0 | 0 | 0% (the control scores better) | 35%+ | soft | no |
| K2 | largest topic's share (%) | 75.2 | 75.2 | 0% (the control scores better) | 25% or less | soft | no |
| K3 | same news told twice to the same resident | 131 | 131 | 0% (the control scores better) | 0 | hard | no |
| W1 | window lines from mantles (%) | n/a | | | 40%+ | soft | n/a until Phase 13 |
| W2 | free-text understanding | n/a | | | 85%+; wrong 8% or less | hard | n/a until Phase 13 |
| W3 | a resident answers chat with the option off | n/a | | | 0 | hard | n/a until Phase 12 |
| B1 | one beat: the director's mean (ms) | n/a | | | 0.6 ms or less (gate: talk_bench) | soft |  |
| B1P | one beat: the director's 99th percentile (ms) | n/a | | | 3 ms or less (gate: talk_bench) | soft |  |
| B1L | a director look: 99th percentile (ms) | n/a | | | 5 ms or less (gate: talk_bench) | soft |  |
| B5 | exchanges simulated a second | 2.39 | 2.39 | 0% (the control scores better) | 150+ | hard | no |
| X1 | lexical cohesion: replies sharing a content word with the line before (%) | 28.7 | 1.94 | 93% (coherence) | diagnostic (not in §15.3): shows the control is discriminating | soft | n/a |
| H1 | blind naturalness 1-5 | n/a | | | 3.8+ | soft | n/a until slice ends (talk_rate.py) |
| H2 | 'written whole by a writer?' (%) | n/a | | | 40%+ | soft | n/a until slice ends (talk_rate.py) |

## By profile

| Id | L1 |
|---|---|
| R1 | 2.54 |
| R2 | 2.54 |
| R3 | 2,026 |
| R4 | 3,534 |
| R5 | 951 |
| R6 | 0.679 |
| R7 | 13.8 |
| R8 | 0 |
| C1Q | 94.6 |
| M1 | 0 |
| C3 | 1.29 |
| C4 | 0 |
| C6 | 0 |
| C7 | 3.21 |
| C8 | 0 |
| T1 | 0 |
| T2 | 4.15 |
| T3 | 0.536 |
| T4 | 0 |
| T6 | 9.96 |
| D1A | 0 |
| O1 | 0 |
| D1 | 0 |
| D1L | 0.139 |
| D1B | 0.274 |
| D2 | 0 |
| D3 | 38.4 |
| U1 | 0 |
| U2 | 65.6 |
| K1 | 0 |
| K2 | 75.2 |
| K3 | 131 |
| B5 | 2.39 |
| X1 | 28.7 |

## Details

- **L1**: {"R1_detail": {"listeners": 4, "median": 6.710902776215274, "censored": false, "exchange": 34}, "R2_detail": {"median": 4.334597221212995, "censored": false}, "R6_detail": {"line": "Nobody's blaming you.", "times": 152}, "R7_detail": {"replies": 15414, "closing_beats": 0, "closings_distinct_per_1000": null}, "R8_detail": {"p90": 0.0, "speakers": 104, "lines20": 21980, "any_habit_mean": 0.006262847900193967}, "C1Q_detail": {"questions": 1472, "answered": 1392}, "C1_detail": {"owed": 0, "met": 0}, "C2_detail": {"answers": 0, "fallbacks": 0}, "M1_detail": {"library": 0.0, "scene": 0.0, "written": 0.0}, "C3_detail": {"farewell": 1.2899527017342698, "marked_break": 0.0, "cut_off": 1.1896230471549376, "cap": 0.0, "nothing_fits": 97.5204242511108, "curt": 0.0}, "C4_detail": {"clashes": 0, "by_tone": 0}, "C7_detail": {"p90": 4, "three_lines": 74.93191916296402, "four_lines": 23.333811093593233}, "T2_detail": {"by_kind": {"rain_claim": 92, "rain_now": 1}, "examples": ["rain_claim: Wet night, light fingers. Orla Wickby lost a purse. (rm_ulf_cinderley, indoors/clear 21h)", "rain_claim: Rain covers footsteps. Wenna Tarford heard nothing. (rm_gwen_fogstack, indoors/clear 15h)", "rain_claim: Wet night, light fingers. Wenna Tarford lost a purse. (rmx_knut_sootley, indoors/clear 21h)", "rain_claim: Wet night, light fingers. Ragnar Cinderwell lost a purse. (rm_ivar_nailham, indoors/clear 19h)", "rain_claim: Wet night, light fingers. Ragnar Cinderwell lost a purse. (rm_ulla_greymore, indoors/c
