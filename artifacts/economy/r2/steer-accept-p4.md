# Acceptance: steer-accept-p4.json

Scorecard `artifacts/economy/r2/steer-scorecard.json`, targets `artifacts/economy/r2/steer-targets-p4.json`, baseline `stable10b`. Year 1 is judged on days 120-365, later years whole. 19 of 31 pass.

| run | year | test | result | detail |
|---|---|---|---|---|
| steer-cal-R33 | 1 | T1 reach S | n/a | no `m_upper_net` |
| steer-cal-R33 | 1 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-cal-R33 | 1 | T8 supervisor | n/a | no supervisor logged |
| steer-cal-R33 | 1 | T4 hunger | PASS | H 0.023 (at most 0.15); worst 28-day H 0.137 (at most 0.3); K 0.61 (at most 5) |
| steer-cal-R33 | 1 | T5 treasury | FAIL | days at the float 6 (stable10b 2), under its floor 15 (stable10b 39), by season against stable10b + 3: summer under 15 > 0+3; winter float 4 > 0+3; mean 804p |
| steer-cal-R33 | 1 | T6 shops | FAIL | tills short 34.3 (at most 1.15 x 34.2 = 39.4); support unpaid 348 (at most 1.5 x stable10b's 219) |
| steer-cal-R33 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p4-S1 | 1 | T1 reach S | PASS | m_upper_net 27.3±1.47: in it by day 81 (by 90) |
| steer-p4-S1 | 1 | T2 hold S | FAIL | every day at a supervisor back-off: nothing held |
| steer-p4-S1 | 1 | T3 no limit cycle | FAIL | most fire-and-clear cycles of a rule 4 (at most 4); at a back-off 100% of days (at most 30%) |
| steer-p4-S1 | 1 | T8 supervisor | FAIL | acts on 38.2% of nights (at most 15%) |
| steer-p4-S1 | 1 | T4 hunger | PASS | H 0.007 (at most 0.15); worst 28-day H 0.045 (at most 0.3); K 0.36 (at most 5) |
| steer-p4-S1 | 1 | T5 treasury | FAIL | days at the float 12 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3: winter float 6 > 0+3; mean 892p |
| steer-p4-S1 | 1 | T6 shops | PASS | tills short 28.9 (at most 1.15 x 34.2 = 39.4); support unpaid 275 (at most 1.5 x stable10b's 219) |
| steer-p4-S1 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p4-S2 | 1 | T1 reach S | PASS | m_upper_net 27.3±1.47: in it by day 81 (by 90) |
| steer-p4-S2 | 1 | T2 hold S | FAIL | every day at a supervisor back-off: nothing held |
| steer-p4-S2 | 1 | T3 no limit cycle | FAIL | most fire-and-clear cycles of a rule 3 (at most 4); at a back-off 100% of days (at most 30%) |
| steer-p4-S2 | 1 | T8 supervisor | FAIL | acts on 29.3% of nights (at most 15%) |
| steer-p4-S2 | 1 | T4 hunger | PASS | H 0.020 (at most 0.15); worst 28-day H 0.141 (at most 0.3); K 0.64 (at most 5) |
| steer-p4-S2 | 1 | T5 treasury | PASS | days at the float 2 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 1179p |
| steer-p4-S2 | 1 | T6 shops | PASS | tills short 29.0 (at most 1.15 x 34.2 = 39.4); support unpaid 272 (at most 1.5 x stable10b's 219) |
| steer-p4-S2 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p4-S3 | 1 | T1 reach S | PASS | m_upper_net 27.3±1.47: in it by day 78 (by 90) |
| steer-p4-S3 | 1 | T2 hold S | FAIL | every day at a supervisor back-off: nothing held |
| steer-p4-S3 | 1 | T3 no limit cycle | FAIL | most fire-and-clear cycles of a rule 3 (at most 4); at a back-off 100% of days (at most 30%) |
| steer-p4-S3 | 1 | T8 supervisor | FAIL | acts on 31.3% of nights (at most 15%) |
| steer-p4-S3 | 1 | T4 hunger | PASS | H 0.026 (at most 0.15); worst 28-day H 0.170 (at most 0.3); K 0.39 (at most 5) |
| steer-p4-S3 | 1 | T5 treasury | PASS | days at the float 4 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 747p |
| steer-p4-S3 | 1 | T6 shops | PASS | tills short 32.2 (at most 1.15 x 34.2 = 39.4); support unpaid 140 (at most 1.5 x stable10b's 219) |
| steer-p4-S3 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
