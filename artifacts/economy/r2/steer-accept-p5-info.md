# Acceptance: steer-accept-p5-info.json

Scorecard `artifacts/economy/r2/steer-scorecard.json`, targets `artifacts/economy/r2/steer-targets-p5.json`, baseline `stable10b`. Year 1 is judged on days 120-365, later years whole. 20 of 25 pass.

| run | year | test | result | detail |
|---|---|---|---|---|
| steer-p5c-A4 | 1 | T1 reach S | PASS | m_upper_share 18.3671±2.36: in it by day 0 (by 90) |
| steer-p5c-A4 | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 19.618 (18.603..20.317) |
| steer-p5c-A4 | 1 | T3 no limit cycle | FAIL | most fire-and-clear cycles of a rule 3 (at most 4); at a back-off 35% of days (at most 30%) |
| steer-p5c-A4 | 1 | T8 supervisor | FAIL | acts on 26.8% of nights (at most 15%) |
| steer-p5c-A4 | 1 | T4 hunger | PASS | H 0.017 (at most 0.15); worst 28-day H 0.109 (at most 0.3); K 0.76 (at most 4) |
| steer-p5c-A4 | 1 | T5 treasury | FAIL | days at the float 10 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3: summer float 4 > 0+3; mean 850p |
| steer-p5c-A4 | 1 | T6 shops | PASS | tills short 31.8 (at most 1.15 x 31.8 = 36.6); support unpaid 258 (at most 1.5 x stable10b's 219) |
| steer-p5c-A4 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p5c-A5 | 1 | T1 reach S | PASS | m_upper_share 23.2995±2.36: in it by day 66 (by 90) |
| steer-p5c-A5 | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 24.972 (23.043..25.650) |
| steer-p5c-A5 | 1 | T3 no limit cycle | FAIL | most fire-and-clear cycles of a rule 2 (at most 4); at a back-off 86% of days (at most 30%) |
| steer-p5c-A5 | 1 | T8 supervisor | FAIL | acts on 64.6% of nights (at most 15%) |
| steer-p5c-A5 | 1 | T4 hunger | PASS | H 0.030 (at most 0.15); worst 28-day H 0.234 (at most 0.3); K 0.64 (at most 5) |
| steer-p5c-A5 | 1 | T5 treasury | PASS | days at the float 4 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 708p |
| steer-p5c-A5 | 1 | T6 shops | PASS | tills short 35.2 (at most 1.15 x 34.4 = 39.6); support unpaid 156 (at most 1.5 x stable10b's 219) |
| steer-p5c-A5 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p5c-A8 | 1 | T1 reach S | PASS | m_upper_share 23.2995±2.36 (one-sided): in it by day 0 (by 90) |
| steer-p5c-A8 | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 14.241 (13.235..15.452) |
| steer-p5c-A8 | 1 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p5c-A8 | 1 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p5c-A8 | 1 | T4 hunger | PASS | H 0.000 (at most 0.15); worst 28-day H 0.000 (at most 0.3); K 0.07 (at most 5) |
| steer-p5c-A8 | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 1147p |
| steer-p5c-A8 | 1 | T6 shops | PASS | tills short 33.5 (at most 1.25 x 35.2 = 43.9, the floors-off run); support unpaid 81 (at most 1.5 x stable10b's 219) |
| steer-p5c-A8 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -5p a day (stable10b 0p, within 200) |
| steer-p5c-A8 | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.18 a day (at most 5); idle 100% of the skim (at least 50%) |
