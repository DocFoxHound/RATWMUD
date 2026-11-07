# Acceptance: steer-accept-p5-r33.json

Scorecard `artifacts/economy/r2/steer-scorecard.json`, targets `artifacts/economy/r2/steer-targets-p4.json`, baseline `stable10b`. Year 1 is judged on days 120-365, later years whole. 26 of 37 pass.

| run | year | test | result | detail |
|---|---|---|---|---|
| steer-p5-A4 | 1 | T1 reach S | PASS | m_upper_net 20.3673±1.47: in it by day 68 (by 90) |
| steer-p5-A4 | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 20.003 (19.514..20.441) |
| steer-p5-A4 | 1 | T3 no limit cycle | FAIL | most fire-and-clear cycles of a rule 5 (at most 4); at a back-off 93% of days (at most 30%) |
| steer-p5-A4 | 1 | T8 supervisor | FAIL | acts on 55.3% of nights (at most 15%) |
| steer-p5-A4 | 1 | T4 hunger | PASS | H 0.013 (at most 0.15); worst 28-day H 0.058 (at most 0.3); K 0.81 (at most 4) |
| steer-p5-A4 | 1 | T5 treasury | FAIL | days at the float 7 (stable10b 2), under its floor 11 (stable10b 39), by season against stable10b + 3: summer float 4 > 0+3; summer under 11 > 0+3; mean 938p |
| steer-p5-A4 | 1 | T6 shops | PASS | tills short 29.2 (at most 1.15 x 31.7 = 36.5); support unpaid 232 (at most 1.5 x stable10b's 219) |
| steer-p5-A4 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p5-A5 | 1 | T1 reach S | PASS | m_upper_net 27.3±1.47: in it by day 78 (by 90) |
| steer-p5-A5 | 1 | T2 hold S | FAIL | every day at a supervisor back-off: nothing held |
| steer-p5-A5 | 1 | T3 no limit cycle | FAIL | most fire-and-clear cycles of a rule 2 (at most 4); at a back-off 100% of days (at most 30%) |
| steer-p5-A5 | 1 | T8 supervisor | FAIL | acts on 66.3% of nights (at most 15%) |
| steer-p5-A5 | 1 | T4 hunger | PASS | H 0.015 (at most 0.15); worst 28-day H 0.104 (at most 0.3); K 0.62 (at most 5) |
| steer-p5-A5 | 1 | T5 treasury | PASS | days at the float 6 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 681p |
| steer-p5-A5 | 1 | T6 shops | PASS | tills short 34.3 (at most 1.15 x 34.2 = 39.4); support unpaid 152 (at most 1.5 x stable10b's 219) |
| steer-p5-A5 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p5b-A5 | 1 | T1 reach S | PASS | m_upper_net 27.3±1.47: in it by day 80 (by 90) |
| steer-p5b-A5 | 1 | T2 hold S | FAIL | 86% of days in band (at least 90%); 28-day mean 26.662 (25.337..28.082) |
| steer-p5b-A5 | 1 | T3 no limit cycle | FAIL | most fire-and-clear cycles of a rule 3 (at most 4); at a back-off 69% of days (at most 30%) |
| steer-p5b-A5 | 1 | T8 supervisor | FAIL | acts on 52.4% of nights (at most 15%) |
| steer-p5b-A5 | 1 | T4 hunger | PASS | H 0.025 (at most 0.15); worst 28-day H 0.125 (at most 0.3); K 1.14 (at most 5) |
| steer-p5b-A5 | 1 | T5 treasury | FAIL | days at the float 20 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3: autumn float 6 > 2+3; summer float 4 > 0+3; winter float 10 > 0+3; mean 781p |
| steer-p5b-A5 | 1 | T6 shops | PASS | tills short 35.7 (at most 1.15 x 34.2 = 39.4); support unpaid 282 (at most 1.5 x stable10b's 219) |
| steer-p5b-A5 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p5-A8 | 1 | T1 reach S | PASS | m_upper_net 27.3±1.47 (one-sided): in it by day 0 (by 90) |
| steer-p5-A8 | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 16.287 (15.144..17.712) |
| steer-p5-A8 | 1 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p5-A8 | 1 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p5-A8 | 1 | T4 hunger | PASS | H 0.000 (at most 0.15); worst 28-day H 0.000 (at most 0.3); K 0.00 (at most 5) |
| steer-p5-A8 | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 1156p |
| steer-p5-A8 | 1 | T6 shops | PASS | tills short 33.6 (at most 1.25 x 34.3 = 42.8, the floors-off run); support unpaid 89 (at most 1.5 x stable10b's 219) |
| steer-p5-A8 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -8p a day (stable10b 0p, within 200) |
| steer-p5-A8 | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.15 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-p5-A9 | - | T12 sweep leg 1 G | PASS | day 120 to -1: 0.361 -> 0.0657; half in 15 days (at most 30), 90% in 27 (at most 75); overshoot 0.000 (at most one band, 0.01) |
| steer-p5-A9 | - | T12 sweep leg 2 S | FAIL | day 240 to 1: 12.333 -> 27.3; half in 27 days (at most 30), 90% in 86 (at most 75); overshoot 0.715 (at most one band, 1.47) |
| steer-p5-A9 | - | T12 sweep leg 3 G | PASS | day 360 to 0: 0.456 -> 0.35; half in 18 days (at most 30), 90% in 30 (at most 75); overshoot 0.000 (at most one band, 0.0208) |
| steer-p5-A9 | - | T12 sweep leg 3 S | PASS | day 360 to 0: 38.621 -> 13.4346; half in 21 days (at most 30), 90% in 34 (at most 75); overshoot 0.000 (at most one band, 1) |
