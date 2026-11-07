# Acceptance: steer-accept-b0.json

Scorecard `artifacts/economy/r2/steer-scorecard.json`, targets `artifacts/economy/r2/steer-targets-t1.json`, baseline `stable10b`. Year 1 is judged on days 120-365, later years whole. 68 of 74 pass.

| run | year | test | result | detail |
|---|---|---|---|---|
| steer-b0-left | 1 | T1 reach G | PASS | m_gini 0.33±0.04: in it by day 1 (by 90) |
| steer-b0-left | 1 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.331 (0.326..0.338) |
| steer-b0-left | 1 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-b0-left | 1 | T8 supervisor | n/a | no supervisor logged |
| steer-b0-left | 1 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-b0-left | 1 | T5 treasury | FAIL | days at the float 0 (stable10b 2), under its floor 64 (stable10b 39), by season against stable10b + 3: summer under 62 > 0+3; mean 1495p |
| steer-b0-left | 1 | T6 shops | PASS | tills short 31.2 (at most 1.15 x 31.2 = 35.9); support unpaid 91 (at most 1.5 x stable10b's 219) |
| steer-b0-left | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -8p a day (stable10b 0p, within 200) |
| steer-b0-right | 1 | T1 reach S | PASS | m_upper_share 24±3: in it by day 74 (by 90) |
| steer-b0-right | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 23.441 (21.992..25.921) |
| steer-b0-right | 1 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-b0-right | 1 | T8 supervisor | n/a | no supervisor logged |
| steer-b0-right | 1 | T4 hunger | PASS | H 0.024 (at most 0.15); worst 28-day H 0.149 (at most 0.3); K 0.74 (at most 5) |
| steer-b0-right | 1 | T5 treasury | FAIL | days at the float 12 (stable10b 2), under its floor 90 (stable10b 39), by season against stable10b + 3: summer float 8 > 0+3; summer under 64 > 0+3; winter under 12 > 0+3; mean 815p |
| steer-b0-right | 1 | T6 shops | FAIL | tills short 34.4 (at most 1.15 x 34.4 = 39.6); support unpaid 334 (at most 1.5 x stable10b's 219) |
| steer-b0-right | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-b0-centre-floors | 1 | T1 reach G | PASS | m_gini 0.487±0.035 (one-sided): in it by day 0 (by 90) |
| steer-b0-centre-floors | 1 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.444 (0.429..0.464) |
| steer-b0-centre-floors | 1 | T1 reach S | PASS | m_upper_share 13±2 (one-sided): in it by day 3 (by 90) |
| steer-b0-centre-floors | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 11.805 (11.330..12.470) |
| steer-b0-centre-floors | 1 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-b0-centre-floors | 1 | T8 supervisor | n/a | no supervisor logged |
| steer-b0-centre-floors | 1 | T4 hunger | PASS | H 0.000 (at most 0.15); worst 28-day H 0.000 (at most 0.3); K 0.07 (at most 3.5) |
| steer-b0-centre-floors | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 2099p |
| steer-b0-centre-floors | 1 | T6 shops | FAIL | tills short 37.9 (at most 1.25 x 29.1 = 36.4, the floors-off run); support unpaid 118 (at most 1.5 x stable10b's 219) |
| steer-b0-centre-floors | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-b0-centre-floors | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.14 a day (at most 5); idle 90% of the skim (at least 50%) |
| steer-b0-left-floors | 1 | T1 reach G | PASS | m_gini 0.33±0.04 (one-sided): in it by day 1 (by 90) |
| steer-b0-left-floors | 1 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.329 (0.325..0.335) |
| steer-b0-left-floors | 1 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-b0-left-floors | 1 | T8 supervisor | n/a | no supervisor logged |
| steer-b0-left-floors | 1 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-b0-left-floors | 1 | T5 treasury | FAIL | days at the float 0 (stable10b 2), under its floor 89 (stable10b 39), by season against stable10b + 3: summer under 64 > 0+3; mean 1491p |
| steer-b0-left-floors | 1 | T6 shops | PASS | tills short 31.6 (at most 1.25 x 31.2 = 39.0, the floors-off run); support unpaid 92 (at most 1.5 x stable10b's 219) |
| steer-b0-left-floors | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -8p a day (stable10b 0p, within 200) |
| steer-b0-left-floors | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.20 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-b0-right-floors | 1 | T1 reach S | PASS | m_upper_share 24±3 (one-sided): in it by day 0 (by 90) |
| steer-b0-right-floors | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 18.214 (16.852..19.802) |
| steer-b0-right-floors | 1 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-b0-right-floors | 1 | T8 supervisor | n/a | no supervisor logged |
| steer-b0-right-floors | 1 | T4 hunger | PASS | H 0.000 (at most 0.15); worst 28-day H 0.000 (at most 0.3); K 0.07 (at most 5) |
| steer-b0-right-floors | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 2040p |
| steer-b0-right-floors | 1 | T6 shops | PASS | tills short 41.6 (at most 1.25 x 34.4 = 43.0, the floors-off run); support unpaid 169 (at most 1.5 x stable10b's 219) |
| steer-b0-right-floors | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-b0-right-floors | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.24 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-d1b-centre-floors | 1 | T1 reach G | PASS | m_gini 0.487±0.035 (one-sided): in it by day 0 (by 90) |
| steer-d1b-centre-floors | 1 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.423 (0.408..0.447) |
| steer-d1b-centre-floors | 1 | T1 reach S | PASS | m_upper_share 13±2 (one-sided): in it by day 3 (by 90) |
| steer-d1b-centre-floors | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 11.799 (11.446..12.068) |
| steer-d1b-centre-floors | 1 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-d1b-centre-floors | 1 | T8 supervisor | n/a | no supervisor logged |
| steer-d1b-centre-floors | 1 | T4 hunger | PASS | H 0.000 (at most 0.15); worst 28-day H 0.000 (at most 0.3); K 0.04 (at most 3.5) |
| steer-d1b-centre-floors | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 2158p |
| steer-d1b-centre-floors | 1 | T6 shops | PASS | tills short 34.1 (at most 1.25 x 29.1 = 36.4, the floors-off run); support unpaid 90 (at most 1.5 x stable10b's 219) |
| steer-d1b-centre-floors | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -4p a day (stable10b 0p, within 200) |
| steer-d1b-centre-floors | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.17 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-d1b-left-floors | 1 | T1 reach G | PASS | m_gini 0.33±0.04 (one-sided): in it by day 1 (by 90) |
| steer-d1b-left-floors | 1 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.329 (0.324..0.338) |
| steer-d1b-left-floors | 1 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-d1b-left-floors | 1 | T8 supervisor | n/a | no supervisor logged |
| steer-d1b-left-floors | 1 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-d1b-left-floors | 1 | T5 treasury | FAIL | days at the float 0 (stable10b 2), under its floor 64 (stable10b 39), by season against stable10b + 3: summer under 61 > 0+3; mean 1494p |
| steer-d1b-left-floors | 1 | T6 shops | PASS | tills short 31.1 (at most 1.25 x 31.2 = 39.0, the floors-off run); support unpaid 91 (at most 1.5 x stable10b's 219) |
| steer-d1b-left-floors | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -8p a day (stable10b 0p, within 200) |
| steer-d1b-left-floors | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.17 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-d1b-right-floors | 1 | T1 reach S | PASS | m_upper_share 24±3 (one-sided): in it by day 0 (by 90) |
| steer-d1b-right-floors | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 13.717 (13.061..14.280) |
| steer-d1b-right-floors | 1 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-d1b-right-floors | 1 | T8 supervisor | n/a | no supervisor logged |
| steer-d1b-right-floors | 1 | T4 hunger | PASS | H 0.000 (at most 0.15); worst 28-day H 0.000 (at most 0.3); K 0.07 (at most 5) |
| steer-d1b-right-floors | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 2131p |
| steer-d1b-right-floors | 1 | T6 shops | PASS | tills short 33.8 (at most 1.25 x 34.4 = 43.0, the floors-off run); support unpaid 84 (at most 1.5 x stable10b's 219) |
| steer-d1b-right-floors | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -2p a day (stable10b 0p, within 200) |
| steer-d1b-right-floors | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.17 a day (at most 5); idle 100% of the skim (at least 50%) |
