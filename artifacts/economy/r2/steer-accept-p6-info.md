# Acceptance: steer-accept-p6-info.json

Scorecard `artifacts/economy/r2/steer-scorecard.json`, targets `artifacts/economy/r2/steer-targets-p6.json`, baseline `stable10b`. Year 1 is judged on days 120-365, later years whole. 14 of 23 pass.

| run | year | test | result | detail |
|---|---|---|---|---|
| steer-p6-A5 | 1 | T1 reach S | PASS | m_upper_share 23.2995±2.36: in it by day 66 (by 90) |
| steer-p6-A5 | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 24.972 (23.043..25.650) |
| steer-p6-A5 | 2 | T2 hold S | FAIL | 89% of days in band (at least 90%); 28-day mean 24.876 (22.767..25.714) |
| steer-p6-A5 | 3 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 24.647 (21.942..25.452) |
| steer-p6-A5 | 1 | T3 no limit cycle | FAIL | most fire-and-clear cycles of a rule 2 (at most 4); at a back-off 86% of days (at most 30%) |
| steer-p6-A5 | 1 | T8 supervisor | FAIL | acts on 64.6% of nights (at most 15%) |
| steer-p6-A5 | 1 | T4 hunger | PASS | H 0.030 (at most 0.15); worst 28-day H 0.234 (at most 0.3); K 0.64 (at most 5) |
| steer-p6-A5 | 1 | T5 treasury | PASS | days at the float 4 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 708p |
| steer-p6-A5 | 1 | T6 shops | PASS | tills short 35.2 (at most 1.15 x 34.4 = 39.6); support unpaid 156 (at most 1.5 x stable10b's 219) |
| steer-p6-A5 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p6-A5 | 2 | T3 no limit cycle | FAIL | most fire-and-clear cycles of a rule 4 (at most 4); at a back-off 82% of days (at most 30%) |
| steer-p6-A5 | 2 | T8 supervisor | FAIL | acts on 61.1% of nights (at most 15%) |
| steer-p6-A5 | 2 | T4 hunger | PASS | H 0.009 (at most 0.15); worst 28-day H 0.082 (at most 0.3); K 1.01 (at most 5) |
| steer-p6-A5 | 2 | T5 treasury | FAIL | days at the float 4 (stable10b 0), under its floor 13 (stable10b 0), by season against stable10b + 3: spring under 13 > 0+3; summer float 4 > 0+3; mean 692p |
| steer-p6-A5 | 2 | T6 shops | PASS | tills short 28.1 (at most 1.15 x 26.5 = 30.4); support unpaid 83 (at most 1.5 x stable10b's 122) |
| steer-p6-A5 | 2 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p6-A5 | 3 | T3 no limit cycle | FAIL | most fire-and-clear cycles of a rule 4 (at most 4); at a back-off 60% of days (at most 30%) |
| steer-p6-A5 | 3 | T8 supervisor | FAIL | acts on 50.7% of nights (at most 15%) |
| steer-p6-A5 | 3 | T4 hunger | PASS | H 0.020 (at most 0.15); worst 28-day H 0.162 (at most 0.3); K 0.84 (at most 5) |
| steer-p6-A5 | 3 | T5 treasury | PASS | days at the float 4 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 706p |
| steer-p6-A5 | 3 | T6 shops | PASS | tills short 27.7 (at most 1.15 x 24.2 = 27.8); support unpaid 86 (at most 1.5 x stable10b's 76) |
| steer-p6-A5 | 3 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p6-A5 | - | T13 no drift S | FAIL | year means 21.833, 22.256, 23.462: year 2 to 3 1.206 (under one band, 2.36, and no more than year 1 to 2, 0.424, or under 1.18) |
