# Acceptance: steer-accept-p6-recentred.json

Scorecard `artifacts/economy/r2/steer-scorecard.json`, targets `artifacts/economy/r2/steer-targets-p6-recentred.json`, baseline `stable10b`. Year 1 is judged on days 120-365, later years whole. 109 of 116 pass.

| run | year | test | result | detail |
|---|---|---|---|---|
| steer-p6-A1 | 1 | T1 reach G | PASS | m_g_day 0.061±0.01: in it by day 29 (by 90) |
| steer-p6-A1 | 1 | T2 hold G | PASS | 93% of days in band (at least 90%); 28-day mean 0.066 (0.062..0.072) |
| steer-p6-A1 | 2 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.062 (0.058..0.068) |
| steer-p6-A1 | 3 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.060 (0.058..0.063) |
| steer-p6-A1 | 1 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p6-A1 | 1 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p6-A1 | 1 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-p6-A1 | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 1492p |
| steer-p6-A1 | 1 | T6 shops | PASS | tills short 32.2 (at most 1.15 x 32.1 = 37.0); support unpaid 101 (at most 1.5 x stable10b's 219) |
| steer-p6-A1 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -8p a day (stable10b 0p, within 200) |
| steer-p6-A1 | 2 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 1 (at most 4); at a back-off 10% of days (at most 30%) |
| steer-p6-A1 | 2 | T8 supervisor | PASS | acts on 7.7% of nights (at most 15%) |
| steer-p6-A1 | 2 | T4 hunger | PASS | H 0.001 (at most 0.02); worst 28-day H 0.018 (at most 0.04); K 0.00 (at most 0.1) |
| steer-p6-A1 | 2 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 1512p |
| steer-p6-A1 | 2 | T6 shops | PASS | tills short 29.1 (at most 1.15 x 27.6 = 31.8); support unpaid 71 (at most 1.5 x stable10b's 122) |
| steer-p6-A1 | 2 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p6-A1 | 3 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p6-A1 | 3 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p6-A1 | 3 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-p6-A1 | 3 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 1510p |
| steer-p6-A1 | 3 | T6 shops | PASS | tills short 28.3 (at most 1.15 x 27.3 = 31.4); support unpaid 66 (at most 1.5 x stable10b's 76) |
| steer-p6-A1 | 3 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p6-A1 | - | T13 no drift G | PASS | year means 0.066, 0.062, 0.060: year 2 to 3 0.002 (under one band, 0.01, and no more than year 1 to 2, 0.004, or under 0.005) |
| steer-p6-A3 | 1 | T1 reach G | PASS | m_g_day 0.3443±0.0212: in it by day 2 (by 90) |
| steer-p6-A3 | 1 | T2 hold G | FAIL | 90% of days in band (at least 90%); 28-day mean 0.349 (0.337..0.375) |
| steer-p6-A3 | 2 | T2 hold G | PASS | 92% of days in band (at least 90%); 28-day mean 0.349 (0.335..0.376) |
| steer-p6-A3 | 3 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.340 (0.326..0.356) |
| steer-p6-A3 | 1 | T1 reach S | PASS | m_upper_share 12.9176±1: in it by day 26 (by 90) |
| steer-p6-A3 | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 13.460 (13.147..13.704) |
| steer-p6-A3 | 2 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 13.176 (12.485..13.833) |
| steer-p6-A3 | 3 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 12.700 (12.164..13.009) |
| steer-p6-A3 | 1 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p6-A3 | 1 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p6-A3 | 1 | T4 hunger | PASS | H 0.022 (at most 0.15); worst 28-day H 0.128 (at most 0.3); K 0.91 (at most 3.5) |
| steer-p6-A3 | 1 | T5 treasury | PASS | days at the float 2 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 1316p |
| steer-p6-A3 | 1 | T6 shops | PASS | tills short 29.1 (at most 1.15 x 29.1 = 33.5); support unpaid 219 (at most 1.5 x stable10b's 219) |
| steer-p6-A3 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p6-A3 | 2 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p6-A3 | 2 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p6-A3 | 2 | T4 hunger | PASS | H 0.010 (at most 0.15); worst 28-day H 0.061 (at most 0.3); K 0.61 (at most 3.5) |
| steer-p6-A3 | 2 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 1615p |
| steer-p6-A3 | 2 | T6 shops | PASS | tills short 23.2 (at most 1.15 x 23.2 = 26.7); support unpaid 122 (at most 1.5 x stable10b's 122) |
| steer-p6-A3 | 2 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p6-A3 | 3 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p6-A3 | 3 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p6-A3 | 3 | T4 hunger | PASS | H 0.011 (at most 0.15); worst 28-day H 0.052 (at most 0.3); K 0.45 (at most 3.5) |
| steer-p6-A3 | 3 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 1759p |
| steer-p6-A3 | 3 | T6 shops | PASS | tills short 22.2 (at most 1.15 x 22.2 = 25.5); support unpaid 76 (at most 1.5 x stable10b's 76) |
| steer-p6-A3 | 3 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p6-A3 | - | T10 centre | PASS | 1095 daily lines the same as stable10b's on every field it has |
| steer-p6-A3 | - | T13 no drift G | PASS | year means 0.350, 0.349, 0.340: year 2 to 3 0.009 (under one band, 0.0212, and no more than year 1 to 2, 0.001, or under 0.0106) |
| steer-p6-A3 | - | T13 no drift S | PASS | year means 13.435, 13.154, 12.681: year 2 to 3 0.474 (under one band, 1, and no more than year 1 to 2, 0.280, or under 0.5) |
| steer-p6-A5 | 1 | T4 hunger (no worse than ref) | FAIL | H 0.030 (ref 0.024); K 0.64 (ref 0.74) |
| steer-p6-A5 | 1 | T5 treasury (no worse than ref) | PASS | days at the float 4 (ref 12) |
| steer-p6-A5 | 1 | T6 shops (no worse than ref) | FAIL | tills short 35.2 (ref 34.4); support unpaid 156 (ref 334) |
| steer-p6-A5 | 1 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-p6-A5 | 2 | T4 hunger (no worse than ref) | FAIL | H 0.009 (ref 0.005); K 1.01 (ref 0.88) |
| steer-p6-A5 | 2 | T5 treasury (no worse than ref) | PASS | days at the float 4 (ref 10) |
| steer-p6-A5 | 2 | T6 shops (no worse than ref) | FAIL | tills short 28.1 (ref 26.5); support unpaid 83 (ref 201) |
| steer-p6-A5 | 2 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-p6-A5 | 3 | T4 hunger (no worse than ref) | FAIL | H 0.020 (ref 0.018); K 0.84 (ref 0.70) |
| steer-p6-A5 | 3 | T5 treasury (no worse than ref) | PASS | days at the float 4 (ref 12) |
| steer-p6-A5 | 3 | T6 shops (no worse than ref) | FAIL | tills short 27.7 (ref 24.2); support unpaid 86 (ref 184) |
| steer-p6-A5 | 3 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-p6-A7 | 1 | T1 reach G | PASS | m_g_day 0.061±0.01 (one-sided): in it by day 29 (by 90) |
| steer-p6-A7 | 1 | T2 hold G | PASS | 93% of days in band (at least 90%); 28-day mean 0.066 (0.062..0.072) |
| steer-p6-A7 | 2 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.062 (0.058..0.065) |
| steer-p6-A7 | 3 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.060 (0.058..0.063) |
| steer-p6-A7 | 1 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 1 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p6-A7 | 1 | T8 supervisor | PASS | acts on 0.4% of nights (at most 15%) |
| steer-p6-A7 | 1 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-p6-A7 | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 1491p |
| steer-p6-A7 | 1 | T6 shops | PASS | tills short 31.8 (at most 1.25 x 32.2 = 40.2, the floors-off run); support unpaid 101 (at most 1.5 x stable10b's 219) |
| steer-p6-A7 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -8p a day (stable10b 0p, within 200) |
| steer-p6-A7 | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.17 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-p6-A7 | 2 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 1 (at most 4); at a back-off 4% of days (at most 30%) |
| steer-p6-A7 | 2 | T8 supervisor | PASS | acts on 3.6% of nights (at most 15%) |
| steer-p6-A7 | 2 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-p6-A7 | 2 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 1511p |
| steer-p6-A7 | 2 | T6 shops | PASS | tills short 28.9 (at most 1.25 x 29.1 = 36.3, the floors-off run); support unpaid 67 (at most 1.5 x stable10b's 122) |
| steer-p6-A7 | 2 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p6-A7 | 2 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.10 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-p6-A7 | 3 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p6-A7 | 3 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p6-A7 | 3 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-p6-A7 | 3 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 1509p |
| steer-p6-A7 | 3 | T6 shops | PASS | tills short 27.5 (at most 1.25 x 28.3 = 35.3, the floors-off run); support unpaid 64 (at most 1.5 x stable10b's 76) |
| steer-p6-A7 | 3 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-p6-A7 | 3 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.23 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-p6-A7 | - | T13 no drift G | PASS | year means 0.066, 0.062, 0.060: year 2 to 3 0.002 (under one band, 0.01, and no more than year 1 to 2, 0.004, or under 0.005) |
| steer-p6-A8 | 1 | T1 reach S | PASS | m_upper_share 22.8593±3.475 (one-sided): in it by day 0 (by 90) |
| steer-p6-A8 | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 14.241 (13.235..15.452) |
| steer-p6-A8 | 2 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 15.454 (14.180..16.843) |
| steer-p6-A8 | 3 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 15.115 (14.122..16.013) |
| steer-p6-A8 | 1 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p6-A8 | 1 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p6-A8 | 1 | T4 hunger | PASS | H 0.000 (at most 0.15); worst 28-day H 0.000 (at most 0.3); K 0.07 (at most 5) |
| steer-p6-A8 | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 1147p |
| steer-p6-A8 | 1 | T6 shops | PASS | tills short 33.5 (at most 1.25 x 35.2 = 43.9, the floors-off run); support unpaid 81 (at most 1.5 x stable10b's 219) |
| steer-p6-A8 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -5p a day (stable10b 0p, within 200) |
| steer-p6-A8 | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.18 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-p6-A8 | 2 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p6-A8 | 2 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p6-A8 | 2 | T4 hunger | PASS | H 0.000 (at most 0.15); worst 28-day H 0.000 (at most 0.3); K 0.00 (at most 5) |
| steer-p6-A8 | 2 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 1286p |
| steer-p6-A8 | 2 | T6 shops | PASS | tills short 27.8 (at most 1.25 x 28.1 = 35.1, the floors-off run); support unpaid 40 (at most 1.5 x stable10b's 122) |
| steer-p6-A8 | 2 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -2p a day (stable10b 0p, within 200) |
| steer-p6-A8 | 2 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.10 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-p6-A8 | 3 | T3 no limit cycle | PASS | most fire-and-clear cycles of a rule 0 (at most 4); at a back-off 0% of days (at most 30%) |
| steer-p6-A8 | 3 | T8 supervisor | PASS | acts on 0.0% of nights (at most 15%) |
| steer-p6-A8 | 3 | T4 hunger | PASS | H 0.000 (at most 0.15); worst 28-day H 0.000 (at most 0.3); K 0.00 (at most 5) |
| steer-p6-A8 | 3 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 1294p |
| steer-p6-A8 | 3 | T6 shops | PASS | tills short 27.2 (at most 1.25 x 27.7 = 34.7, the floors-off run); support unpaid 35 (at most 1.5 x stable10b's 76) |
| steer-p6-A8 | 3 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 5p a day (stable10b 0p, within 200) |
| steer-p6-A8 | 3 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.18 a day (at most 5); idle 100% of the skim (at least 50%) |
| steer-p6-A8 | - | T13 no drift S | PASS | year means 14.186, 15.448, 15.146: year 2 to 3 0.302 (under one band, 3.475, and no more than year 1 to 2, 1.262, or under 1.7375) |
