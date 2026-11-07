# Acceptance: steer-accept-dry.json

Scorecard `artifacts/economy/r2/steer-scorecard.json`, targets `artifacts/economy/r2/steer-targets-t1.json`, baseline `stable10b`. Year 1 is judged on days 120-365, later years whole. 131 of 142 pass.

| run | year | test | result | detail |
|---|---|---|---|---|
| steer-centre-t1 | 1 | T1 reach G | PASS | m_gini 0.487±0.035: in it by day 0 (by 90) |
| steer-centre-t1 | 1 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.488 (0.470..0.514) |
| steer-centre-t1 | 2 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.490 (0.478..0.514) |
| steer-centre-t1 | 3 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.484 (0.470..0.505) |
| steer-centre-t1 | 1 | T1 reach S | PASS | m_upper_share 13±2: in it by day 8 (by 90) |
| steer-centre-t1 | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 13.460 (13.147..13.704) |
| steer-centre-t1 | 2 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 13.176 (12.485..13.833) |
| steer-centre-t1 | 3 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 12.700 (12.164..13.009) |
| steer-centre-t1 | 1 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-centre-t1 | 1 | T8 supervisor | n/a | no supervisor logged |
| steer-centre-t1 | 1 | T4 hunger | PASS | H 0.022 (at most 0.15); worst 28-day H 0.128 (at most 0.3); K 0.91 (at most 3.5) |
| steer-centre-t1 | 1 | T5 treasury | PASS | days at the float 2 (stable10b 2), under its floor 39 (stable10b 39), by season against stable10b + 3; mean 1316p |
| steer-centre-t1 | 1 | T6 shops | PASS | tills short 29.1 (at most 1.15 x 29.1 = 33.5); support unpaid 219 (at most 1.5 x stable10b's 219) |
| steer-centre-t1 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-centre-t1 | 2 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-centre-t1 | 2 | T8 supervisor | n/a | no supervisor logged |
| steer-centre-t1 | 2 | T4 hunger | PASS | H 0.010 (at most 0.15); worst 28-day H 0.061 (at most 0.3); K 0.61 (at most 3.5) |
| steer-centre-t1 | 2 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 1615p |
| steer-centre-t1 | 2 | T6 shops | PASS | tills short 23.2 (at most 1.15 x 23.2 = 26.7); support unpaid 122 (at most 1.5 x stable10b's 122) |
| steer-centre-t1 | 2 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-centre-t1 | 3 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-centre-t1 | 3 | T8 supervisor | n/a | no supervisor logged |
| steer-centre-t1 | 3 | T4 hunger | PASS | H 0.011 (at most 0.15); worst 28-day H 0.052 (at most 0.3); K 0.45 (at most 3.5) |
| steer-centre-t1 | 3 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 1759p |
| steer-centre-t1 | 3 | T6 shops | PASS | tills short 22.2 (at most 1.15 x 22.2 = 25.5); support unpaid 76 (at most 1.5 x stable10b's 76) |
| steer-centre-t1 | 3 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-centre-t1 | - | T10 centre | PASS | 1095 daily lines the same as stable10b's on every field it has |
| steer-centre-t1 | - | T13 no drift G | PASS | year means 0.489, 0.489, 0.484: year 2 to 3 0.005 (under one band, 0.035, and no more than year 1 to 2, 0.000, or under 0.0175) |
| steer-centre-t1 | - | T13 no drift S | PASS | year means 13.435, 13.154, 12.681: year 2 to 3 0.474 (under one band, 2, and no more than year 1 to 2, 0.280, or under 1) |
| steer-centre-floors-t1 | 1 | T1 reach G | PASS | m_gini 0.487±0.035 (one-sided): in it by day 0 (by 90) |
| steer-centre-floors-t1 | 1 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.411 (0.389..0.443) |
| steer-centre-floors-t1 | 2 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.426 (0.415..0.443) |
| steer-centre-floors-t1 | 3 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.422 (0.408..0.438) |
| steer-centre-floors-t1 | 1 | T1 reach S | PASS | m_upper_share 13±2 (one-sided): in it by day 4 (by 90) |
| steer-centre-floors-t1 | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 12.343 (12.006..12.714) |
| steer-centre-floors-t1 | 2 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 12.278 (11.762..13.002) |
| steer-centre-floors-t1 | 3 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 11.913 (11.712..12.257) |
| steer-centre-floors-t1 | 1 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-centre-floors-t1 | 1 | T8 supervisor | n/a | no supervisor logged |
| steer-centre-floors-t1 | 1 | T4 hunger | PASS | H 0.000 (at most 0.15); worst 28-day H 0.000 (at most 0.3); K 0.07 (at most 3.5) |
| steer-centre-floors-t1 | 1 | T5 treasury | PASS | days at the float 0 (stable10b 2), under its floor 0 (stable10b 39), by season against stable10b + 3; mean 2121p |
| steer-centre-floors-t1 | 1 | T6 shops | FAIL | tills short 36.6 (at most 1.25 x 29.1 = 36.4, the floors-off run); support unpaid 106 (at most 1.5 x stable10b's 219) |
| steer-centre-floors-t1 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-centre-floors-t1 | 1 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.18 a day (at most 5); idle share n/a (not logged) |
| steer-centre-floors-t1 | 2 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-centre-floors-t1 | 2 | T8 supervisor | n/a | no supervisor logged |
| steer-centre-floors-t1 | 2 | T4 hunger | PASS | H 0.000 (at most 0.15); worst 28-day H 0.000 (at most 0.3); K 0.00 (at most 3.5) |
| steer-centre-floors-t1 | 2 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 2294p |
| steer-centre-floors-t1 | 2 | T6 shops | FAIL | tills short 30.8 (at most 1.25 x 23.2 = 29.0, the floors-off run); support unpaid 50 (at most 1.5 x stable10b's 122) |
| steer-centre-floors-t1 | 2 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-centre-floors-t1 | 2 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.08 a day (at most 5); idle share n/a (not logged) |
| steer-centre-floors-t1 | 3 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-centre-floors-t1 | 3 | T8 supervisor | n/a | no supervisor logged |
| steer-centre-floors-t1 | 3 | T4 hunger | PASS | H 0.000 (at most 0.15); worst 28-day H 0.000 (at most 0.3); K 0.00 (at most 3.5) |
| steer-centre-floors-t1 | 3 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 2298p |
| steer-centre-floors-t1 | 3 | T6 shops | FAIL | tills short 30.8 (at most 1.25 x 22.2 = 27.7, the floors-off run); support unpaid 52 (at most 1.5 x stable10b's 76) |
| steer-centre-floors-t1 | 3 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-centre-floors-t1 | 3 | T11 floors | PASS | H 0.0000 (at most 0.01); short after relief 0.00 (at most 0.5); Never Hungry's Wharf pennies 0.25 a day (at most 5); idle share n/a (not logged) |
| steer-centre-floors-t1 | - | T13 no drift G | PASS | year means 0.413, 0.426, 0.422: year 2 to 3 0.004 (under one band, 0.035, and no more than year 1 to 2, 0.012, or under 0.0175) |
| steer-centre-floors-t1 | - | T13 no drift S | PASS | year means 12.362, 12.259, 11.905: year 2 to 3 0.353 (under one band, 2, and no more than year 1 to 2, 0.103, or under 1) |
| steer-right-t1 | 1 | T4 hunger (no worse than ref) | PASS | H 0.024 (ref 0.024); K 0.74 (ref 0.74) |
| steer-right-t1 | 1 | T5 treasury (no worse than ref) | PASS | days at the float 12 (ref 12) |
| steer-right-t1 | 1 | T6 shops (no worse than ref) | PASS | tills short 34.4 (ref 34.4); support unpaid 334 (ref 334) |
| steer-right-t1 | 1 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-right-t1 | 2 | T4 hunger (no worse than ref) | PASS | H 0.005 (ref 0.005); K 0.88 (ref 0.88) |
| steer-right-t1 | 2 | T5 treasury (no worse than ref) | PASS | days at the float 10 (ref 10) |
| steer-right-t1 | 2 | T6 shops (no worse than ref) | PASS | tills short 26.5 (ref 26.5); support unpaid 201 (ref 201) |
| steer-right-t1 | 2 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-right-t1 | 3 | T4 hunger (no worse than ref) | PASS | H 0.018 (ref 0.018); K 0.70 (ref 0.70) |
| steer-right-t1 | 3 | T5 treasury (no worse than ref) | PASS | days at the float 12 (ref 12) |
| steer-right-t1 | 3 | T6 shops (no worse than ref) | PASS | tills short 24.2 (ref 24.2); support unpaid 184 (ref 184) |
| steer-right-t1 | 3 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-left-t1 | 1 | T4 hunger (no worse than ref) | PASS | H 0.000 (ref 0.000); K 0.00 (ref 0.00) |
| steer-left-t1 | 1 | T5 treasury (no worse than ref) | PASS | days at the float 0 (ref 0) |
| steer-left-t1 | 1 | T6 shops (no worse than ref) | PASS | tills short 31.2 (ref 31.2); support unpaid 91 (ref 91) |
| steer-left-t1 | 1 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-left-t1 | 2 | T4 hunger (no worse than ref) | PASS | H 0.003 (ref 0.003); K 0.00 (ref 0.00) |
| steer-left-t1 | 2 | T5 treasury (no worse than ref) | PASS | days at the float 0 (ref 0) |
| steer-left-t1 | 2 | T6 shops (no worse than ref) | PASS | tills short 26.8 (ref 26.8); support unpaid 57 (ref 57) |
| steer-left-t1 | 2 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-left-t1 | 3 | T4 hunger (no worse than ref) | PASS | H 0.000 (ref 0.000); K 0.00 (ref 0.00) |
| steer-left-t1 | 3 | T5 treasury (no worse than ref) | PASS | days at the float 0 (ref 0) |
| steer-left-t1 | 3 | T6 shops (no worse than ref) | PASS | tills short 26.5 (ref 26.5); support unpaid 54 (ref 54) |
| steer-left-t1 | 3 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-right-t1 | 1 | T4 hunger (no worse than ref) | PASS | H 0.024 (ref 0.024); K 0.74 (ref 0.74) |
| steer-right-t1 | 1 | T5 treasury (no worse than ref) | PASS | days at the float 12 (ref 12) |
| steer-right-t1 | 1 | T6 shops (no worse than ref) | PASS | tills short 34.4 (ref 34.4); support unpaid 334 (ref 334) |
| steer-right-t1 | 1 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-right-t1 | 2 | T4 hunger (no worse than ref) | PASS | H 0.005 (ref 0.005); K 0.88 (ref 0.88) |
| steer-right-t1 | 2 | T5 treasury (no worse than ref) | PASS | days at the float 10 (ref 10) |
| steer-right-t1 | 2 | T6 shops (no worse than ref) | PASS | tills short 26.5 (ref 26.5); support unpaid 201 (ref 201) |
| steer-right-t1 | 2 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-right-t1 | 3 | T4 hunger (no worse than ref) | PASS | H 0.018 (ref 0.018); K 0.70 (ref 0.70) |
| steer-right-t1 | 3 | T5 treasury (no worse than ref) | PASS | days at the float 12 (ref 12) |
| steer-right-t1 | 3 | T6 shops (no worse than ref) | PASS | tills short 24.2 (ref 24.2); support unpaid 184 (ref 184) |
| steer-right-t1 | 3 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-left-t1 | 1 | T1 reach G | PASS | m_gini 0.33±0.04: in it by day 1 (by 90) |
| steer-left-t1 | 1 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.331 (0.326..0.338) |
| steer-left-t1 | 2 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.339 (0.335..0.341) |
| steer-left-t1 | 3 | T2 hold G | PASS | 100% of days in band (at least 90%); 28-day mean 0.343 (0.338..0.349) |
| steer-left-t1 | 1 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-left-t1 | 1 | T8 supervisor | n/a | no supervisor logged |
| steer-left-t1 | 1 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-left-t1 | 1 | T5 treasury | FAIL | days at the float 0 (stable10b 2), under its floor 64 (stable10b 39), by season against stable10b + 3: summer under 62 > 0+3; mean 1495p |
| steer-left-t1 | 1 | T6 shops | PASS | tills short 31.2 (at most 1.15 x 31.2 = 35.9); support unpaid 91 (at most 1.5 x stable10b's 219) |
| steer-left-t1 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -8p a day (stable10b 0p, within 200) |
| steer-left-t1 | 2 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-left-t1 | 2 | T8 supervisor | n/a | no supervisor logged |
| steer-left-t1 | 2 | T4 hunger | PASS | H 0.003 (at most 0.02); worst 28-day H 0.018 (at most 0.04); K 0.00 (at most 0.1) |
| steer-left-t1 | 2 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 1512p |
| steer-left-t1 | 2 | T6 shops | PASS | tills short 26.8 (at most 1.15 x 26.8 = 30.8); support unpaid 57 (at most 1.5 x stable10b's 122) |
| steer-left-t1 | 2 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-left-t1 | 3 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-left-t1 | 3 | T8 supervisor | n/a | no supervisor logged |
| steer-left-t1 | 3 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-left-t1 | 3 | T5 treasury | PASS | days at the float 0 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3; mean 1514p |
| steer-left-t1 | 3 | T6 shops | PASS | tills short 26.5 (at most 1.15 x 26.5 = 30.5); support unpaid 54 (at most 1.5 x stable10b's 76) |
| steer-left-t1 | 3 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-left-t1 | - | T13 no drift G | PASS | year means 0.331, 0.339, 0.344: year 2 to 3 0.005 (under one band, 0.04, and no more than year 1 to 2, 0.007, or under 0.02) |
| steer-right-t1 | 1 | T1 reach S | PASS | m_upper_share 24±3: in it by day 74 (by 90) |
| steer-right-t1 | 1 | T2 hold S | PASS | 100% of days in band (at least 90%); 28-day mean 23.441 (21.992..25.921) |
| steer-right-t1 | 2 | T2 hold S | PASS | 97% of days in band (at least 90%); 28-day mean 22.315 (20.952..23.363) |
| steer-right-t1 | 3 | T2 hold S | FAIL | 86% of days in band (at least 90%); 28-day mean 21.808 (20.573..23.002) |
| steer-right-t1 | 1 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-right-t1 | 1 | T8 supervisor | n/a | no supervisor logged |
| steer-right-t1 | 1 | T4 hunger | PASS | H 0.024 (at most 0.15); worst 28-day H 0.149 (at most 0.3); K 0.74 (at most 5) |
| steer-right-t1 | 1 | T5 treasury | FAIL | days at the float 12 (stable10b 2), under its floor 90 (stable10b 39), by season against stable10b + 3: summer float 8 > 0+3; summer under 64 > 0+3; winter under 12 > 0+3; mean 815p |
| steer-right-t1 | 1 | T6 shops | FAIL | tills short 34.4 (at most 1.15 x 34.4 = 39.6); support unpaid 334 (at most 1.5 x stable10b's 219) |
| steer-right-t1 | 1 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-right-t1 | 2 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-right-t1 | 2 | T8 supervisor | n/a | no supervisor logged |
| steer-right-t1 | 2 | T4 hunger | PASS | H 0.005 (at most 0.15); worst 28-day H 0.021 (at most 0.3); K 0.88 (at most 5) |
| steer-right-t1 | 2 | T5 treasury | FAIL | days at the float 10 (stable10b 0), under its floor 40 (stable10b 0), by season against stable10b + 3: autumn float 4 > 0+3; autumn under 34 > 0+3; summer float 4 > 0+3; summer under 6 > 0+3; mean 894p |
| steer-right-t1 | 2 | T6 shops | FAIL | tills short 26.5 (at most 1.15 x 26.5 = 30.4); support unpaid 201 (at most 1.5 x stable10b's 122) |
| steer-right-t1 | 2 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-right-t1 | 3 | T3 no limit cycle | n/a | no supervisor logged (`sup_actions`) |
| steer-right-t1 | 3 | T8 supervisor | n/a | no supervisor logged |
| steer-right-t1 | 3 | T4 hunger | PASS | H 0.018 (at most 0.15); worst 28-day H 0.106 (at most 0.3); K 0.70 (at most 5) |
| steer-right-t1 | 3 | T5 treasury | FAIL | days at the float 12 (stable10b 0), under its floor 0 (stable10b 0), by season against stable10b + 3: summer float 8 > 0+3; mean 984p |
| steer-right-t1 | 3 | T6 shops | FAIL | tills short 24.2 (at most 1.15 x 24.2 = 27.8); support unpaid 184 (at most 1.5 x stable10b's 76) |
| steer-right-t1 | 3 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-right-t1 | - | T13 no drift S | PASS | year means 23.300, 22.302, 21.775: year 2 to 3 0.527 (under one band, 3, and no more than year 1 to 2, 0.998, or under 1.5) |
