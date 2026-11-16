# Talk scorecard

Generated 2026-11-16T14:33:33+00:00 by `tools/talk_score.py` (Docs/Design/84 §15.3).

Runs: `L1`, `L2`, `L3`, `L4`, `L5`, `L6`, `L7`, `L8`, `L9`, `L10`.
Shuffled control (talk_sim --ablate shuffle: lines after the first replaced by lines said at the same side of other conversations on the same topic, by a speaker of the same dialect where possible (§15.5)): L1, L10, L2, L3, L4, L5, L6, L7, L8, L9.

| Id | Metric | Value | Shuffled control | Separation | Target | Gate | Passes |
|---|---|---|---|---|---|---|---|
| R1 | first identical conversation, per listener (h) | 0.418 | 0.14 | 66% | 40 h+; hard 20 h+ | hard | no |
| R2 | first repeated opener, per listener (h) | 0.14 | 0.14 | 0% (within 30%) | 8 h+ | soft | no |
| R3 | distinct openers per 3,000 exchanges | 469 | 469 | 0% (within 30%) | L1 1,500+ | soft | no |
| R4 | distinctive line heard again within 10 h, per 100 h | 3,901 | 3,892 | 0% (within 30%) | 5 or fewer | soft | no |
| R5 | collision index, per 100 h | 926 | 1,003 | 8% (within 30%) | 1 or fewer; hard 3 | hard | no |
| R6 | most-spoken line's share (%) | 13.2 | 13.1 | 0% (within 30%) | 0.4% | soft | no |
| R7 | distinct reply entries per 1,000 reply beats | 34.8 | 32.4 | 7% (within 30%) | 500+ | soft | no |
| R8 | habit saturation (% of a speaker's lines, mean) | 0.0624 | 0.0583 | 7% (within 30%) | 15% or less | soft | yes |
| C1 | questions answered (%) | 98.9 | 86.7 | 12% (within 30%) (coherence) | 97%; hard 92% | hard | yes |
| C2 | replies at the best fit available (%) | n/a | | | 95% | soft | n/a until Phase 3 |
| C3 | endings by a farewell or a break (%) | 0.0684 | 0.177 | 61% (coherence) | 80% / 3% / 5% | soft | no |
| C4 | tone clashes per 1,000 beats | 0 | 0.135 | 100% (coherence) | 0 | hard | yes |
| C5 | unmarked topic jumps per 1,000 beats | n/a | | | 5 | soft | n/a until Phase 5 |
| C6 | same entry or question twice in a conversation, per 1,000 | 0.119 | 34.3 | 100% (coherence) | 0 | hard | no |
| C7 | lines per conversation (mean) | 2.55 | 2.55 | 0% (within 30%) (structural) | 5-7; p90 9+ | soft | no |
| C8 | conversations with a third speaker (%) | 0 | 0 | 0% (within 30%) (structural) | 5-15% | soft | no |
| C9 | callbacks per 10 conversations | n/a | | | 1.5+ | soft | n/a until Phase 5 |
| T1 | raw ids spoken, per 1,000 lines | 0 | 0 | 0% (within 30%) | 0 | hard | yes |
| T2 | presupposition failures, per 1,000 lines | 0 | 0.045 | 100% | 0 | hard | yes |
| T3 | self-reference errors, per 1,000 lines | 0 | 0 | 0% (within 30%) | 0 | hard | yes |
| T4 | identity drift | n/a | | | 0 | hard | n/a until Phase 6 |
| T5 | telling what the listener already believes (%) | n/a | | | 2% | soft | n/a until Phase 4 |
| D1 | lines with a dialect's words (%): light speakers | 0.241 | 0.243 | 1% (within 30%) | none 0%; light 25-50%; broad 60%+ | hard | no |
| D2 | another dialect's words in a speaker's line | 0 | 0 | 0% (within 30%) | 0 | hard | yes |
| D3 | dialect guessed from 20 lines (%): light and broad | 12.8 | 16.5 | 23% (within 30%) | 85%+ | soft | no |
| U1 | a resident's lines from their own words (%) | 0 | 0 | 0% (within 30%) | 20-35% | soft | no |
| U2 | distinguishability of look-alike residents (%) | 39.5 | 39 | 1% (within 30%) | 90%+ | soft | no |
| K1 | conversations on everyday topics (%) | 24.4 | 24.4 | 0% (within 30%) | 35%+ | soft | no |
| K2 | largest topic's share (%) | 41.8 | 41.8 | 0% (within 30%) | 25% or less | soft | no |
| K3 | same news told twice to the same resident | 109 | 109 | 0% (within 30%) | 0 | hard | no |
| W1 | window lines from mantles (%) | n/a | | | 40%+ | soft | n/a until Phase 13 |
| W2 | free-text understanding | n/a | | | 85%+; wrong 8% or less | hard | n/a until Phase 13 |
| W3 | a resident answers chat with the option off | n/a | | | 0 | hard | n/a until Phase 12 |
| B5 | exchanges simulated a second | 59.2 | 59.2 | 0% (within 30%) | 150+ | hard | no |
| X1 | lexical cohesion: replies sharing a content word with the line before (%) | 15.7 | 1.31 | 92% (coherence) | diagnostic (not in §15.3): shows the control is discriminating | soft | n/a |
| H1 | blind naturalness 1-5 | n/a | | | 3.8+ | soft | n/a until slice ends (talk_rate.py) |
| H2 | 'written whole by a writer?' (%) | n/a | | | 40%+ | soft | n/a until slice ends (talk_rate.py) |

## By profile

| Id | L1 | L2 | L3 | L4 | L5 | L6 | L7 | L8 | L9 | L10 |
|---|---|---|---|---|---|---|---|---|---|---|
| R1 | 0.435 | 3.33 | 1.08 | 0.501 | 0.585 | 0.501 | 0.418 | 0 | 0 | 1.58 |
| R2 | 0.435 | 3.33 | 0.14 | 0.279 | 0.585 | 0.501 | 0.418 | 0 | 0 | 1.58 |
| R3 | 459 | 782 | 401 | 375 | 375 | 532 | 360 | 0 | 0 | 453 |
| R4 | 7,246 | 970 | 1,370 | 897 | 242 | 351 | 313 | 0 | 0 | 640 |
| R5 | 1,878 | 32 | 128 | 30 | 0 | 0 | 0 | 0 | 0 | 81.7 |
| R6 | 9.69 | 8.95 | 17.1 | 15 | 23.1 | 16.7 | 21.4 | 0 | 0 | 18.7 |
| R7 | 13.4 | 102 | 42.9 | 38 | 42.5 | 52.4 | 39.9 | 0 | 0 | 52.7 |
| R8 | 0 | 0 | 0.293 | 0.258 | 0 | 0 | 0 | 0 | 0 | 0.017 |
| C1 | 99.1 | 98 | 100 | 96 | 100 | 99.2 | 100 | 0 | 0 | 99.3 |
| C3 | 0.075 | 0 | 0.111 | 0.042 | 0.167 | 0 | 0 | 0 | 0 | 0.093 |
| C4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| C6 | 0.25 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| C7 | 2.65 | 2.58 | 2.42 | 2.45 | 2.37 | 2.57 | 2.4 | 0 | 0 | 2.42 |
| C8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| T1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| T2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| T3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| D1 | 0 | 0.078 | 0.758 | 1.16 | 0.125 | 0.108 | 0.521 | 0 | 0 | 0 |
| D2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| D3 | 0 | 78.1 | 42.9 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| U1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| U2 | 71.1 | 60.2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| K1 | 28.7 | 24.6 | 27.7 | 24.3 | 10.5 | 23 | 9.67 | 0 | 0 | 8.33 |
| K2 | 38.1 | 38.5 | 42.6 | 43.7 | 55.2 | 46.9 | 53 | 0 | 0 | 48.4 |
| K3 | 105 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| B5 | 81.6 | 59.2 | 154 | 133 | 126 | 124 | 143 | 0 | 0 | 99 |
| X1 | 16 | 15.8 | 14.3 | 16.1 | 15 | 15.9 | 14.5 | 0 | 0 | 16 |

## Details

- **L1**: {"R1_detail": {"listeners": 2, "median": 1.0195138888888888, "censored": false, "exchange": 27}, "R2_detail": {"median": 1.0195138888888888, "censored": false}, "R6_detail": {"line": "A day like any other.", "times": 3085}, "R8_detail": {"p90": 0.0, "speakers": 39, "any_habit_mean": 0.023424288130170483}, "C1_detail": {"questions": 2134, "answered": 2115}, "C3_detail": {"farewell": 0.075, "break": 0.0, "cap": 0.0, "nothing_fits": 99.925}, "C7_detail": {"p90": 4, "three_lines": 39.891666666666666, "four_lines": 9.183333333333334}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"examples": []}, "T1_detail": {"examples": []}, "K2_detail": {"topics": {"event": 38.06, "player": 27.74, "prices": 22.34, "crime": 4.1, "festival": 2.95, "gossip": 1.78, "caravan": 1.52, "family": 0.98, "ships": 0.51, "work": 0.02}}, "D1_detail": {"broad": 0.0, "light": 0.0, "none": 0.0}, "D2_detail": {"by_layers": 0, "by_lexicon": 0, "examples": [], "lexicon": "tools/dialect_check.py"}, "D3_detail": {"speakers": 39, "note": "fewer than two dialects heard"}, "U2_detail": {"pairs": 4, "lines": 3652}, "_counts": {"lines": 31840, "conversations": 12000, "hours": 100.0, "listeners": 2, "speakers": 40}}
- **L2**: {"R1_detail": {"listeners": 1, "median": 3.334791666666667, "censored": false, "exchange": 93}, "R2_detail": {"median": 3.334791666666667, "censored": false}, "R6_detail": {"line": "A day like any other.", "times": 550}, "R8_detail": {"p90": 0.0, "speakers": 79, "any_habit_mean": 0.024342745861733205}, "C1_detail": {"questions": 343, "answered": 336}, "C3_detail": {"farewell": 0.0, "break": 0.0, "cap": 0.0, "nothing_fits": 100.0}, "C7_detail": {"p90": 3, "three_lines": 38.87489504617968, "four_lines": 6.717044500419815}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"examples": []}, "T1_detail": {"examples": []}, "K2_detail": {"topics": {"event": 38.5, "prices": 24.22, "player": 20.91, "gossip": 5.21, "festival": 2.6, "crime": 1.89, "caravan": 1.47, "family": 1.43, "home": 1.43, "ships": 1.34, "times": 0.46, "work": 0.25, "accent": 0.17, "bandits": 0.13}}, "D1_detail": {"light": 0.08, "broad": 0.28, "none": 0.0}, "D2_detail": {"by_layers": 0, "by_lexicon": 0, "examples": [], "lexicon": "tools/dialect_check.py"}, "D3_detail": {"light": 79.2, "none": 66.7, "broad": 75.0, "speakers": 79}, "U2_detail": {"pairs": 3, "lines": 261}, "_counts": {"lines": 6148, "conversations": 2382, "hours": 100.0, "listeners": 1, "speakers": 302}}
- **L3**: {"R1_detail": {"listeners": 1, "median": 1.0847916666666666, "censored": false, "exchange": 40}, "R2_detail": {"median": 0.14034722222222223, "censored": false}, "R6_detail": {"line": "A day like any other.", "times": 1486}, "R8_detail": {"p90": 0.6666666666666666, "speakers": 7, "any_habit_mean": 0.29318518745802863}, "C1_detail": {"questions": 301, "answered": 301}, "C3_detail": {"farewell": 0.1111111111111111, "break": 0.0, "cap": 0.0, "nothing_fits": 99.88888888888889}, "C7_detail": {"p90": 3, "three_lines": 30.194444444444443, "four_lines": 3.2222222222222223}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"examples": []}, "T1_detail": {"examples": []}, "K2_detail": {"topics": {"event": 42.64, "prices": 23.94, "times": 10.83, "player": 7.92, "home": 6.0, "caravan": 3.08, "family": 2.97, "festival": 2.56, "bandits": 0.06}}, "D1_detail": {"light": 0.76}, "D2_detail": {"by_layers": 0, "by_lexicon": 0, "examples": [], "lexicon": "tools/dialect_check.py"}, "D3_detail": {"light": 42.9, "speakers": 7}, "U2_detail": {"pairs": 0, "lines": 0}, "_counts": {"lines": 8708, "conversations": 3600, "hours": 100.0, "listeners": 1, "speakers": 7}}
- **L4**: {"R1_detail": {"listeners": 1, "median": 0.5014583333333333, "censored": false, "exchange": 13}, "R2_detail": {"median": 0.27923611111111113, "censored": false}, "R6_detail": {"line": "A day like any other.", "times": 880}, "R8_detail": {"p90": 0.5703422053231939, "speakers": 5, "any_habit_mean": 0.27233997408039173}, "C1_detail": {"questions": 174, "answered": 167}, "C3_detail": {"farewell": 0.041666666666666664, "break": 0.0, "cap": 0.0, "nothing_fits": 99.95833333333333}, "C7_detail": {"p90": 3, "three_lines": 35.916666666666664, "four_lines": 3.5}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"examples": []}, "T1_detail": {"examples": []}, "K2_detail": {"topics": {"event": 43.67, "prices": 26.54, "home": 13.0, "player": 10.96, "caravan": 3.5, "festival": 2.0, "work": 0.33}}, "D1_detail": {"light": 1.16}, "D2_detail": {"by_layers": 0, "by_lexicon": 0, "examples": [], "lexicon": "tools/dialect_check.py"}, "D3_detail": {"speakers": 5, "note": "fewer than two dialects heard"}, "U2_detail": {"pairs": 0, "lines": 0}, "_counts": {"lines": 5878, "conversations": 2400, "hours": 100.0, "listeners": 1, "speakers": 5}}
- **L5**: {"R1_detail": {"listeners": 1, "median": 0.5847916666666667, "censored": false, "exchange": 8}, "R2_detail": {"median": 0.5847916666666667, "censored": false}, "R6_detail": {"line": "It is at that.", "times": 656}, "R8_detail": {"p90": 0.0, "speakers": 2, "any_habit_mean": 0.0}, "C1_detail": {"questions": 73, "answered": 73}, "C3_detail": {"farewell": 0.16666666666666666, "break": 0.0, "cap": 0.0, "nothing_fits": 99.83333333333333}, "C7_detail": {"p90": 3, "three_lines": 28.333333333333332, "four_lines": 3.1666666666666665}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"examples": []}, "T1_detail": {"examples": []}, "K2_detail": {"topics": {"event": 55.17, "prices": 27.0, "player": 10.0, "caravan": 4.5, "festival": 2.83, "work": 0.5}}, "D1_detail": {"light": 0.13, "none": 0.0}, "D2_detail": {"by_layers": 0, "by_lexicon": 0, "examples": [], "lexicon": "tools/dialect_check.py"}, "D3_detail": {"speakers": 2, "note": "fewer than two dialects heard"}, "U2_detail": {"pairs": 0, "lines": 0}, "_counts": {"lines": 2846, "conversations": 1200, "hours": 100.0, "listeners": 1, "speakers": 2}}
- **L6**: {"R1_detail": {"listeners": 1, "median": 0.5014583333333333, "censored": false, "exchange": 7}, "R2_detail": {"median": 0.5014583333333333, "censored": false}, "R6_detail": {"line": "So it is.", "times": 515}, "R8_detail": {"p90": 0.0, "speakers": 3, "any_habit_mean": 0.0}, "C1_detail": {"questions": 132, "answered": 131}, "C3_detail": {"farewell": 0.0, "break": 0.0, "cap": 0.0, "nothing_fits": 100.0}, "C7_detail": {"p90": 3, "three_lines": 39.583333333333336, "four_lines": 7.416666666666667}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"examples": []}, "T1_detail": {"examples": []}, "K2_detail": {"topics": {"event": 46.92, "prices": 27.17, "player": 22.83, "festival": 2.33, "caravan": 0.33, "gossip": 0.25, "work": 0.17}}, "D1_detail": {"light": 0.11, "broad": 2.19}, "D2_detail": {"by_layers": 0, "by_lexicon": 0, "examples": [], "lexicon": "tools/dialect_check.py"}, "D3_detail": {"speakers": 3, "note": "fewer than two dialects heard"}, "U2_detail": {"pairs": 0, "lines": 0}, "_counts": {"lines": 3089, "conversations": 1200, "hours": 100.0, "listeners": 1, "speakers": 3}}
- **L7**: {"R1_detail": {"listeners": 1, "median": 0.418125, "censored": false, "exchange": 6}, "R2_detail": {"median": 0.418125, "censored": false}, "R6_detail": {"line": "It is at that.", "times": 616}, "R8_detail": {"p90": 0.0, "speakers": 2, "any_habit_mean": 0.0}, "C1_detail": {"questions": 112, "answered": 112}, "C3_detail": {"farewell": 0.0, "break": 0.0, "cap": 0.0, "nothing_fits": 100.0}, "C7_detail": {"p90": 3, "three_lines": 33.916666666666664, "four_lines": 2.0}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"examples": []}, "T1_detail": {"examples": []}, "K2_detail": {"topics": {"event": 53.0, "prices": 31.17, "player": 9.5, "caravan": 3.17, "festival": 3.0, "work": 0.17}}, "D1_detail": {"light": 0.52}, "D2_detail": {"by_layers": 0, "by_lexicon": 0, "examples": [], "lexicon": "tools/dialect_check.py"}, "D3_detail": {"speakers": 2, "note": "fewer than two dialects heard"}, "U2_detail": {"pairs": 0, "lines": 0}, "_counts": {"lines": 2879, "conversations": 1200, "hours": 100.0, "listeners": 1, "speakers": 2}}
- **L8**: {"R1_detail": {"listeners": 0, "median": 0.0, "censored": false, "exchange": null}, "R2_detail": {"median": 0.0, "censored": false}, "R6_detail": {"line": "", "times": 0}, "R8_detail": {"p90": 0.0, "speakers": 0, "any_habit_mean": 0}, "C1_detail": {"questions": 0, "answered": 0}, "C3_detail": {"farewell": 0.0, "break": 0.0, "cap": 0.0, "nothing_fits": 0.0}, "C7_detail": {"p90": 0.0, "three_lines": 0.0, "four_lines": 0.0}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"examples": []}, "T1_detail": {"examples": []}, "K2_detail": {"topics": {}}, "D1_detail": {"light": 0.0}, "D2_detail": {"by_layers": 0, "by_lexicon": 0, "examples": [], "lexicon": "tools/dialect_check.py"}, "D3_detail": {"speakers": 0, "note": "fewer than two dialects heard"}, "U2_detail": {"pairs": 0, "lines": 0}, "_counts": {"lines": 0, "conversations": 0, "hours": 100.0, "listeners": 0, "speakers": 0}}
- **L9**: {"R1_detail": {"listeners": 0, "median": 0.0, "censored": false, "exchange": null}, "R2_detail": {"median": 0.0, "censored": false}, "R6_detail": {"line": "", "times": 0}, "R8_detail": {"p90": 0.0, "speakers": 0, "any_habit_mean": 0}, "C1_detail": {"questions": 0, "answered": 0}, "C3_detail": {"farewell": 0.0, "break": 0.0, "cap": 0.0, "nothing_fits": 0.0}, "C7_detail": {"p90": 0.0, "three_lines": 0.0, "four_lines": 0.0}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"examples": []}, "T1_detail": {"examples": []}, "K2_detail": {"topics": {}}, "D1_detail": {"light": 0.0}, "D2_detail": {"by_layers": 0, "by_lexicon": 0, "examples": [], "lexicon": "tools/dialect_check.py"}, "D3_detail": {"speakers": 0, "note": "fewer than two dialects heard"}, "U2_detail": {"pairs": 0, "lines": 0}, "_counts": {"lines": 0, "conversations": 0, "hours": 100.0, "listeners": 0, "speakers": 0}}
- **L10**: {"R1_detail": {"listeners": 1, "median": 1.5847916666666666, "censored": false, "exchange": 58}, "R2_detail": {"median": 1.5847916666666666, "censored": false}, "R6_detail": {"line": "Aye.", "times": 980}, "R8_detail": {"p90": 0.10090817356205853, "speakers": 6, "any_habit_mean": 0.016818028927009756}, "C1_detail": {"questions": 152, "answered": 151}, "C3_detail": {"farewell": 0.09259259259259259, "break": 0.0, "cap": 0.0, "nothing_fits": 99.9074074074074}, "C7_detail": {"p90": 3, "three_lines": 34.25925925925926, "four_lines": 3.1481481481481484}, "T2_detail": {"by_kind": {}, "examples": []}, "T3_detail": {"examples": []}, "T1_detail": {"examples": []}, "K2_detail": {"topics": {"event": 48.43, "prices": 29.77, "player": 8.33, "crime": 5.46, "festival": 3.06, "caravan": 2.78, "ships": 1.94, "gossip": 0.19, "bandits": 0.05}}, "D1_detail": {"light": 0.0, "broad": 0.0}, "D2_detail": {"by_layers": 0, "by_lexicon": 0, "examples": [], "lexicon": "tools/dialect_check.py"}, "D3_detail": {"speakers": 6, "note": "fewer than two dialects heard"}, "U2_detail": {"pairs": 0, "lines": 0}, "_counts": {"lines": 5232, "conversations": 2160, "hours": 120.0, "listeners": 1, "speakers": 6}}
