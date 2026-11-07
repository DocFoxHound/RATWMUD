# Acceptance: steer-accept-p5.json

Scorecard `artifacts/economy/r2/steer-scorecard.json`, targets `artifacts/economy/r2/steer-targets-p5.json`, baseline `stable10b`. Year 1 is judged on days 120-365, later years whole. 60 of 64 pass.

| run | year | test | result | detail |
|---|---|---|---|---|
| steer-p5-A1 | 1 | T1 reach G | PASS | m_g_day 0.0657±0.01: in it by day 28 (by 90) |
| steer-p5-A1 | 1 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.066 (0.062..0.072) |
| steer-p5-A1 | 1 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p5-A1 | 1 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p5-A1 | 1 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-p5-A1 | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 1492p |
| steer-p5-A1 | 1 | T6 shops | PASS | tills short 32.2 (at most 1.15 x 32.1 = 37.0); support unpaid 101 (at most 1.5 x stable10b's 219) |
| steer-p5-A1 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -8p a day (stable10b 0p, within 200) |
| steer-p5b-A2 | 1 | T1 reach G | PASS | m_g_day 0.1861±0.0197: in it by day 25 (by 90) |
| steer-p5b-A2 | 1 | T2 hold G | PASS | 93% of days in band (at least 90%); 28-day mean 0.183 (0.174..0.213) |
| steer-p5b-A2 | 1 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 1 (at most 4); at a back-off 17% of days (at most 30%) |
| steer-p5b-A2 | 1 | T8 supervisor | PASS | acts on 13.0% of nights (at most 15%) |
| steer-p5b-A2 | 1 | T4 hunger | PASS | H 0.001 (at most 0.05); worst 28-day H 0.007 (at most 0.1); K 0.07 (at most 0.5) |
| steer-p5b-A2 | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 1909p |
| steer-p5b-A2 | 1 | T6 shops | PASS | tills short 32.8 (at most 1.15 x 30.6 = 35.2); support unpaid 79 (at most 1.5 x stable10b's 219) |
| steer-p5b-A2 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -8p a day (stable10b 0p, within 200) |
| steer-p5b-A3 | 1 | T1 reach G | PASS | m_g_day 0.35±0.0208: in it by day 2 (by 90) |
| steer-p5b-A3 | 1 | T2 hold G | PASS | 95% of days in band (at least 90%); 28-day mean 0.349 (0.337..0.375) |
| steer-p5b-A3 | 1 | T1 reach S | PASS | m_upper_share 13.4346±1: in it by day 12 (by 90) |
| steer-p5b-A3 | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 13.460 (13.147..13.704) |
| steer-p5b-A3 | 1 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p5b-A3 | 1 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p5b-A3 | 1 | T4 hunger | PASS | H 0.022 (at most 0.15); worst 28-day H 0.128 (at most 0.3); K 0.91 (at most 3.5) |
| steer-p5b-A3 | 1 | T5 treasury | PASS | days at the float 2 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 1316p |
| steer-p5b-A3 | 1 | T6 shops | PASS | tills short 29.1 (at most 1.15 x 29.1 = 33.5); support unpaid 219 (at most 1.5 x stable10b's 219) |
| steer-p5b-A3 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p5b-A3 | - | T10 centre | PASS | 366 daily lines the same as stable10b's on every field it has |
| steer-p5c-A4 | 1 | T4 hunger (no worse than ref) | FAIL | H 0.017 (ref 0.024); K 0.76 (ref 0.74) |
| steer-p5c-A4 | 1 | T5 treasury (no worse than ref) | PASS | days at the float 10 (ref 12) |
| steer-p5c-A4 | 1 | T6 shops (no worse than ref) | PASS | tills short 31.8 (ref 34.4); support unpaid 258 (ref 334) |
| steer-p5c-A4 | 1 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-p5c-A5 | 1 | T4 hunger (no worse than ref) | FAIL | H 0.030 (ref 0.024); K 0.64 (ref 0.74) |
| steer-p5c-A5 | 1 | T5 treasury (no worse than ref) | PASS | days at the float 4 (ref 12) |
| steer-p5c-A5 | 1 | T6 shops (no worse than ref) | FAIL | tills short 35.2 (ref 34.4); support unpaid 156 (ref 334) |
| steer-p5c-A5 | 1 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-p5b-A6 | 1 | T1 reach G | PASS | m_g_day 0.35±0.0208 (one-sided): in it by day 0 (by 90) |
| steer-p5b-A6 | 1 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.284 (0.274..0.303) |
| steer-p5b-A6 | 1 | T1 reach S | PASS | m_upper_share 13.4346±1 (one-sided): in it by day 4 (by 90) |
| steer-p5b-A6 | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 11.799 (11.446..12.068) |
| steer-p5b-A6 | 1 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p5b-A6 | 1 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p5b-A6 | 1 | T4 hunger | PASS | H 0.000 (at most 0.15); worst 28-day H 0.000 (at most 0.3); K 0.04 (at most 3.5) |
| steer-p5b-A6 | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 2158p |
| steer-p5b-A6 | 1 | T6 shops | PASS | tills short 34.1 (at most 1.25 x 29.1 = 36.4, the floors-off run); support unpaid 90 (at most 1.5 x stable10b's 219) |
| steer-p5b-A6 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -4p a day (stable10b 0p, within 200) |
| steer-p5b-A6 | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.17 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-p5b-A7 | 1 | T1 reach G | PASS | m_g_day 0.0657±0.01 (one-sided): in it by day 28 (by 90) |
| steer-p5b-A7 | 1 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.066 (0.062..0.072) |
| steer-p5b-A7 | 1 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 1 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p5b-A7 | 1 | T8 supervisor | PASS | acts on 0.4% of nights (at most 15%) |
| steer-p5b-A7 | 1 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-p5b-A7 | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 1491p |
| steer-p5b-A7 | 1 | T6 shops | PASS | tills short 31.8 (at most 1.25 x 32.2 = 40.2, the floors-off run); support unpaid 101 (at most 1.5 x stable10b's 219) |
| steer-p5b-A7 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -8p a day (stable10b 0p, within 200) |
| steer-p5b-A7 | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.17 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-p5c-A8 | 1 | T4 hunger (no worse than ref) | PASS | H 0.000 (ref 0.000); K 0.07 (ref 0.07) |
| steer-p5c-A8 | 1 | T5 treasury (no worse than ref) | PASS | days at the float 0 (ref 0) |
| steer-p5c-A8 | 1 | T6 shops (no worse than ref) | PASS | tills short 33.5 (ref 33.8); support unpaid 81 (ref 84) |
| steer-p5c-A8 | 1 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-p5c-A8 | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.18 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-p5c-A9 | - | T12 sweep leg 1 G | PASS | day 120 to -1: 0.361 -> 0.0657; half in 15 days (at most 30), 90% in 26 (at most 75); overshoot 0.000 (at most one band, 0.01) |
| steer-p5c-A9 | - | T12 sweep leg 2 S | PASS | day 240 to 1: 12.381 -> 23.2995; half in 25 days (at most 30), 90% in 41 (at most 75); overshoot 1.401 (at most one band, 2.36) |
| steer-p5c-A9 | - | T12 sweep leg 3 G | FAIL | day 360 to 0: 0.450 -> 0.35; half in 23 days (at most 30), 90% in 76 (at most 75); overshoot 0.000 (at most one band, 0.0208) |
| steer-p5c-A9 | - | T12 sweep leg 3 S | PASS | day 360 to 0: 24.701 -> 13.4346; half in 19 days (at most 30), 90% in 33 (at most 75); overshoot 0.000 (at most one band, 1) |
