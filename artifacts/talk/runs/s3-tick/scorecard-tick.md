# Talk scorecard

Generated 2026-11-17T10:20:05+00:00 by `tools/talk_score.py` (Docs/Design/84 §15.3).

Runs: `L1`.
Shuffled control (talk_sim --ablate shuffle: lines after the first replaced by lines said at the same side of other conversations on the same topic, by a speaker of the same dialect where possible (§15.5)): L1.

| Id | Metric | Value | Shuffled control | Separation | Target | Gate | Passes |
|---|---|---|---|---|---|---|---|
| R1 | first identical conversation, per listener (h) | 6.07 | 6.07 | 0% (the control scores better) | 40 h+; hard 20 h+ | hard | no |
| R2 | first repeated opener, per listener (h) | 6.02 | 6.02 | 0% (the control scores better) | 8 h+ | soft | no |
| R3 | distinct openers per 3,000 exchanges | 2,728 | 2,728 | 0% (the control scores better) | L1 1,500+ | soft | yes |
| R4 | distinctive line heard again within 10 h, per 100 h | 2,417 | 2,571 | 6% (within 30%) | 5 or fewer | soft | no |
| R5 | collision index, per 100 h | 0 | 249 | 100% | 1 or fewer; hard 3 | hard | yes |
| R6 | most-spoken line's share (%) | 1.15 | 1.16 | 1% (within 30%) | 0.4% | soft | no |
| R7 | distinct reply entries per 1,000 reply beats | 118 | 105 | 11% (within 30%) | 500+ | soft | no |
| R8 | habit saturation (% of a speaker's lines, mean) | 0.104 | 0.0886 | 15% (the control scores better) | 15% or less | soft | yes |
| C1 | owed answers answered, by moves (%) | 97.6 | 24.7 | 75% (coherence) | 97%; hard 92% | hard | yes |
| C1Q | questions answered: the '?' heuristic (%) | 95.2 | 84.5 | 11% (within 30%) | diagnostic (C1 until Phase 3; doesn't separate from the control) | soft | n/a |
| M1 | lines carrying moves (%) | 100 | 100 | 0% (the control scores better) | 100% at Phase 7 (every entry) | soft | n/a |
| C2 | replies at the best fit available (%) | 100 | 100 | 0% (the control scores better) | 95% | soft | yes |
| C3 | endings by a farewell or a marked break (%) | 98.7 | 70.8 | 28% (within 30%) (coherence) | 80% / 3% / 5% | soft | yes |
| C4 | tone clashes per 1,000 beats | 0.158 | 0.316 | 50% (coherence) | 0 | hard | no |
| C5 | unmarked topic jumps per 1,000 beats | 0 | 28.9 | 100% | 5 | soft | yes |
| C6 | same entry or question twice in a conversation, per 1,000 | 0 | 111 | 100% (coherence) | 0 | hard | yes |
| C7 | lines per conversation (mean) | 4.65 | 4.65 | 0% (the control scores better) (structural) | 5-7; p90 9+ | soft | no |
| C8 | conversations with a third speaker (%) | 1.94 | 1.94 | 0% (the control scores better) (structural) | 5-15% | soft | no |
| C9 | callbacks per 10 conversations | 1.65 | 1.65 | 0% (within 30%) | 1.5+ | soft | yes |
| T1 | raw ids spoken, per 1,000 lines | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| T2 | presupposition failures, per 1,000 lines | 0.0316 | 4.65 | 99% | 0 | hard | no |
| T3 | self-reference errors (own name, the listener in the third person, unbacked eyewitness words), per 1,000 lines | 0.0948 | 0.284 | 67% | 0 | hard | no |
| T4 | identity drift: a stance changed with no event or concede, or answered against one's own, per 1,000 stance lines | 0 | 7.87 | 100% | 0 | hard | yes |
| T6 | 'they' said of one known wolf named in the line, per 1,000 lines | 3.16 | 3.6 | 12% (within 30%) | 0 | soft | no |
| D1A | lines said a step broader or plainer than the speaker's own accent (%) | 2.86 | 2.86 | 0% (the control scores better) | diagnostic: accommodation (§10.6) | soft | n/a |
| O1 | opinions given per 10 conversations | 1.25 | 0.138 | 89% | diagnostic (§8.8) | soft | n/a |
| T5 | telling what the listener already believes, with no 'known' answer (%) | 0.947 | 1.15 | 18% (within 30%) | 2% | soft | yes |
| D1 | lines with a dialect's words (%): accent none | 0 | 0.966 | 100% | none 0% | hard | yes |
| D1L | lines with a dialect's words (%): light speakers | 0.489 | 0.326 | 33% | light 25-50% | soft | no |
| D1B | lines with a dialect's words (%): broad speakers | 0.412 | 0.187 | 55% | broad 60%+ | soft | no |
| D2 | another dialect's words in a speaker's line | 0 | 0 | 0% (the control scores better) | 0 | hard | yes |
| D3 | dialect guessed from 20 lines (%): light and broad | 84.2 | 89.2 | 6% (the control scores better) | 85%+ | soft | no |
| U1 | a resident's lines from their own words (%) | 0 | 0 | 0% (the control scores better) | 20-35% | soft | no |
| U2 | distinguishability of look-alike residents (%) | 61.5 | 58.8 | 4% (within 30%) | 90%+ | soft | no |
| K1 | conversations on everyday topics (%) | 45.5 | 45.5 | 0% (the control scores better) | 35%+ | soft | yes |
| K2 | largest topic's share (%) | 24.2 | 24.2 | 0% (the control scores better) | 25% or less | soft | yes |
| K3 | same news told twice to the same resident | 2 | 2 | 0% (the control scores better) | 0 | hard | no |
| W1 | window lines from mantles (%) | n/a | | | 40%+ | soft | n/a until Phase 13 |
| W2 | free-text understanding | n/a | | | 85%+; wrong 8% or less | hard | n/a until Phase 13 |
| W3 | a resident answers chat with the option off | n/a | | | 0 | hard | n/a until Phase 12 |
| B1 | one beat: the director's mean (ms) | 0.292 | 0.292 | 0% (the control scores better) | 0.6 ms or less (gate: talk_bench) | soft | yes |
| B1P | one beat: the director's 99th percentile (ms) | 1.39 | 1.39 | 0% (the control scores better) | 3 ms or less (gate: talk_bench) | soft | yes |
| B1L | a director look: 99th percentile (ms) | 7.52 | 7.52 | 0% (the control scores better) | 5 ms or less (gate: talk_bench) | soft | no |
| B5 | exchanges simulated a second | 1.99 | 1.99 | 0% (the control scores better) | 150+ | hard | no |
| X1 | lexical cohesion: replies sharing a content word with the line before (%) | 18.7 | 2.74 | 85% (coherence) | diagnostic (not in §15.3): shows the control is discriminating | soft | n/a |
| H1 | blind naturalness 1-5 | n/a | | | 3.8+ | soft | n/a until slice ends (talk_rate.py) |
| H2 | 'written whole by a writer?' (%) | n/a | | | 40%+ | soft | n/a until slice ends (talk_rate.py) |

## By profile

| Id | L1 |
|---|---|
| R1 | 6.07 |
| R2 | 6.02 |
| R3 | 2,728 |
| R4 | 2,417 |
| R5 | 0 |
| R6 | 1.15 |
| R7 | 118 |
| R8 | 0.104 |
| C1 | 97.6 |
| C1Q | 95.2 |
| M1 | 100 |
| C2 | 100 |
| C3 | 98.7 |
| C4 | 0.158 |
| C5 | 0 |
| C6 | 0 |
| C7 | 4.65 |
| C8 | 1.94 |
| C9 | 1.65 |
| T1 | 0 |
| T2 | 0.032 |
| T3 | 0.095 |
| T4 | 0 |
| T6 | 3.16 |
| D1A | 2.86 |
| O1 | 1.25 |
| T5 | 0.947 |
| D1 | 0 |
| D1L | 0.489 |
| D1B | 0.412 |
| D2 | 0 |
| D3 | 84.2 |
| U1 | 0 |
| U2 | 61.5 |
| K1 | 45.5 |
| K2 | 24.2 |
| K3 | 2 |
| B1 | 0.292 |
| B1P | 1.39 |
| B1L | 7.52 |
| B5 | 1.99 |
| X1 | 18.7 |

## Details

- **L1**: {"R1_detail": {"listeners": 4, "median": 6.410236109618611, "censored": false, "exchange": 133}, "R2_detail": {"median": 6.0433472208151455, "censored": false}, "R6_detail": {"sentence": "Off I go, then.", "times": 365, "line": "Off I go, then.", "line_pct": 1.154}, "R7_detail": {"replies": 19755, "closing_beats": 5085, "closings_distinct_per_1000": 60.0}, "R8_detail": {"p90": 0.3875968992248062, "speakers": 146, "lines20": 31493, "any_habit_mean": 0.14448609818434338}, "C1Q_detail": {"questions": 2101, "answered": 2000}, "C1_detail": {"owed": 11477, "met": 11200}, "C2_detail": {"answers": 7913, "fallbacks": 0}, "M1_detail": {"scene": 100.0, "written": 100.0}, "C3_detail": {"farewell": 35.87819947043248, "marked_break": 0.0, "cut_off": 1.2503677552221242, "cap": 0.0, "nothing_fits": 0.014710208884966167, "curt": 62.85672256546043}, "C4_detail": {"clashes": 5, "by_tone": 0, "in_line": 5}, "C7_detail": {"p90": 9, "three_lines": 47.46984407178582, "four_lines": 1.3827596351868197}, "T2_detail": {"by_kind": {"children": 1}, "examples": ["children: Every time we meet, it's the carts. My son rides with the Oakhaven caravan. (rm_soren_coaldale, indoors/clear 18h)"]}, "T3_detail": {"by_kind": {"own_name": 1, "listener_third": 1, "unbacked": 1}, "examples": ["own_name: rm_frida_rainhand to rm_gorm_slagworth: Frida Rainhand. Folk get it wrong all the time.", "listener_third: rm_tobias_saltton to rmx_aud_kemp: They say Aud Kemp prefers the other baker's bread.", "unbacked: rmx_ivar_neth
