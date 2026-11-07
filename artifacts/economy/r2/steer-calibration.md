# The calibration (doc 75, 5.2 and D2)

Written by `tools/steer_calibrate.py artifacts/economy/r2/steer-cal-spec.json`. One-year runs, open-loop (the table only, no supervisor), the toggles off, measured on days 120-365. The selection is judged by steer_accept's T4-T7 against `artifacts/economy/r2/steer-targets-t1.json` (the t1 run at each end), baseline stable10b. T5's floor: the run's own seasonal profile less 2 sigma (the scorecard as written).

## The selection rule

| end | run | strength | T4 | T5 | T6 | T7 | passes |
|---|---|---|---|---|---|---|---|
| -1 | steer-cal-L30 | 30 | PASS | FAIL | PASS | PASS | no |
| -1 | steer-cal-L40 | 40 | PASS | FAIL | PASS | PASS | no |
| -1 | steer-cal-L50 | 50 | PASS | FAIL | PASS | PASS | no |
| 1 | steer-cal-R33 | 33 | PASS | FAIL | FAIL | PASS | no |
| 1 | steer-cal-R66 | 66 | PASS | FAIL | FAIL | PASS | no |
| 1 | steer-cal-R100 | 100 | PASS | FAIL | FAIL | PASS | no |

- **-1:** no strength passes; rung **F3**.
- **1:** no strength passes; rung **F1**.

Every test's detail:

| run | test | result | detail |
|---|---|---|---|
| steer-cal-L30 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-cal-L30 | T5 treasury | FAIL | days at the float 0 (stable10b 2), under its floor 8 (stable10b 39), by season against stable10b + 3: summer under 6 > 0+3; mean 1492p |
| steer-cal-L30 | T6 shops | PASS | tills short 31.5 (at most 1.15 x 31.2 = 35.9); support unpaid 96 (at most 1.5 x stable10b's 219) |
| steer-cal-L30 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -8p a day (stable10b 0p, within 200) |
| steer-cal-L40 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-cal-L40 | T5 treasury | FAIL | days at the float 0 (stable10b 2), under its floor 10 (stable10b 39), by season against stable10b + 3: summer under 6 > 0+3; mean 1493p |
| steer-cal-L40 | T6 shops | PASS | tills short 31.3 (at most 1.15 x 31.2 = 35.9); support unpaid 99 (at most 1.5 x stable10b's 219) |
| steer-cal-L40 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -8p a day (stable10b 0p, within 200) |
| steer-cal-L50 | T4 hunger | PASS | H 0.000 (at most 0.02); worst 28-day H 0.000 (at most 0.04); K 0.00 (at most 0.1) |
| steer-cal-L50 | T5 treasury | FAIL | days at the float 0 (stable10b 2), under its floor 9 (stable10b 39), by season against stable10b + 3: summer under 5 > 0+3; mean 1492p |
| steer-cal-L50 | T6 shops | PASS | tills short 32.2 (at most 1.15 x 31.2 = 35.9); support unpaid 101 (at most 1.5 x stable10b's 219) |
| steer-cal-L50 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net -8p a day (stable10b 0p, within 200) |
| steer-cal-R33 | T4 hunger | PASS | H 0.023 (at most 0.15); worst 28-day H 0.137 (at most 0.3); K 0.61 (at most 5) |
| steer-cal-R33 | T5 treasury | FAIL | days at the float 6 (stable10b 2), under its floor 17 (stable10b 39), by season against stable10b + 3: summer under 15 > 0+3; winter float 4 > 0+3; mean 804p |
| steer-cal-R33 | T6 shops | FAIL | tills short 34.3 (at most 1.15 x 34.4 = 39.6); support unpaid 348 (at most 1.5 x stable10b's 219) |
| steer-cal-R33 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-cal-R66 | T4 hunger | PASS | H 0.031 (at most 0.15); worst 28-day H 0.210 (at most 0.3); K 0.93 (at most 5) |
| steer-cal-R66 | T5 treasury | FAIL | days at the float 23 (stable10b 2), under its floor 125 (stable10b 39), by season against stable10b + 3: autumn float 13 > 2+3; autumn under 91 > 39+3; summer float 6 > 0+3; summer under 25 > 0+3; winter float 4 > 0+3; winter under 9 > 0+3; mean 650p |
| steer-cal-R66 | T6 shops | FAIL | tills short 36.3 (at most 1.15 x 34.4 = 39.6); support unpaid 371 (at most 1.5 x stable10b's 219) |
| steer-cal-R66 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |
| steer-cal-R100 | T4 hunger | PASS | H 0.022 (at most 0.15); worst 28-day H 0.125 (at most 0.3); K 0.95 (at most 5) |
| steer-cal-R100 | T5 treasury | FAIL | days at the float 15 (stable10b 2), under its floor 89 (stable10b 39), by season against stable10b + 3: autumn float 9 > 2+3; autumn under 75 > 39+3; summer under 7 > 0+3; winter float 6 > 0+3; winter under 7 > 0+3; mean 708p |
| steer-cal-R100 | T6 shops | FAIL | tills short 37.5 (at most 1.15 x 34.4 = 39.6); support unpaid 376 (at most 1.5 x stable10b's 219) |
| steer-cal-R100 | T7 books | PASS | conserved True, estates' posts paid True, Wharf net 0p a day (stable10b 0p, within 200) |

For the check-in, not the rule: the right's strengths on F1's own bar (no worse than steer-b0-right, the right without charters, in T4-T7):

| run | test | result | detail |
|---|---|---|---|
| steer-cal-R33 | T4 hunger (no worse than ref) | PASS | H 0.023 (ref 0.024); K 0.61 (ref 0.74) |
| steer-cal-R33 | T5 treasury (no worse than ref) | PASS | days at the float 6 (ref 12) |
| steer-cal-R33 | T6 shops (no worse than ref) | FAIL | tills short 34.3 (ref 34.4); support unpaid 348 (ref 334) |
| steer-cal-R33 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-cal-R66 | T4 hunger (no worse than ref) | FAIL | H 0.031 (ref 0.024); K 0.93 (ref 0.74) |
| steer-cal-R66 | T5 treasury (no worse than ref) | FAIL | days at the float 23 (ref 12) |
| steer-cal-R66 | T6 shops (no worse than ref) | FAIL | tills short 36.3 (ref 34.4); support unpaid 371 (ref 334) |
| steer-cal-R66 | T7 books | PASS | conserved True, estates' posts paid True |
| steer-cal-R100 | T4 hunger (no worse than ref) | FAIL | H 0.022 (ref 0.024); K 0.95 (ref 0.74) |
| steer-cal-R100 | T5 treasury (no worse than ref) | FAIL | days at the float 15 (ref 12) |
| steer-cal-R100 | T6 shops (no worse than ref) | FAIL | tills short 37.5 (ref 34.4); support unpaid 376 (ref 334) |
| steer-cal-R100 | T7 books | PASS | conserved True, estates' posts paid True |

## D2

- The right: no strength passes T4-T7, so the right takes its fallback rung (5.2) whatever the charters add; what they add: steer-cal-R33 34.9% (+11.6 points over F1's 23.3%, +4.0 without the chartered tills' own cash); steer-cal-R66 42.0% (+18.7 points over F1's 23.3%, +6.3 without the chartered tills' own cash); steer-cal-R100 47.5% (+24.2 points over F1's 23.3%, +6.6 without the chartered tills' own cash). The bar is 3 points: rung **F1**.
- The left: not even the weakest strength passes: rung **F3**.

## The measures (days 120-365, means a day)

G: the Gini of cash a head over all households, weighted by heads, the day's mean of before and after the tithe. S: the upper class, the estates and the Houses' tills (chartered ones among them), percent of all money; S without the chartered tills' own cash takes their floats out (the relabelling, not money moved). Estates: the five estates' funds, the mean of days 120-147 and of days 338-365.

| run | G (day) | G before | G after | Gini earners | S % | S without chartered cash | charters | H/h | K | treasury mean (least; 28-day least) | float days | toll | tills short | wage support | support unpaid | tithe moved | charter profit | dividend | estates' funds | conserved |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| steer-cal-L30 | 0.100 | 0.123 | 0.078 | 0.325 | 13.1 | 13.1 | 0.0 | 0.000 | 0.00 | 1,492 (1,090; 1,451) | 0 | 30.0 | 31.5 | 284 | 96 | 933 | 0 | 0 | 1,860 -> 2,275 | True |
| steer-cal-L40 | 0.081 | 0.105 | 0.056 | 0.312 | 13.0 | 13.0 | 0.0 | 0.000 | 0.00 | 1,493 (1,110; 1,454) | 0 | 30.0 | 31.3 | 289 | 99 | 1,071 | 0 | 0 | 1,860 -> 2,275 | True |
| steer-cal-L50 | 0.066 | 0.092 | 0.039 | 0.302 | 12.7 | 12.7 | 0.0 | 0.000 | 0.00 | 1,492 (1,107; 1,454) | 0 | 30.0 | 32.2 | 298 | 101 | 1,158 | 0 | 0 | 1,877 -> 2,306 | True |
| steer-cal-R33 | 0.457 | 0.457 | 0.457 | 0.568 | 34.9 | 27.3 | 29.0 | 0.023 | 0.61 | 804 (200; 437) | 6 | 30.0 | 34.3 | 12 | 348 | 0 | 179 | 206 | 1,840 -> 2,307 | True |
| steer-cal-R66 | 0.476 | 0.476 | 0.476 | 0.583 | 42.0 | 29.6 | 58.0 | 0.031 | 0.93 | 650 (200; 351) | 23 | 30.0 | 36.3 | 8 | 371 | 0 | 247 | 332 | 1,787 -> 2,107 | True |
| steer-cal-R100 | 0.478 | 0.478 | 0.478 | 0.584 | 47.5 | 29.9 | 88.0 | 0.022 | 0.95 | 708 (200; 470) | 15 | 30.0 | 37.5 | 9 | 376 | 0 | 313 | 467 | 1,740 -> 1,911 | True |
| steer-cal-C | 0.350 | 0.350 | 0.350 | 0.489 | 13.4 | 13.4 | 0.0 | 0.022 | 0.91 | 1,316 (200; 870) | 2 | 30.6 | 29.1 | 60 | 219 | 0 | 0 | 0 | 1,881 -> 2,264 | True |
| steer-cal-Hm | 0.143 | 0.159 | 0.126 | 0.334 | 12.6 | 12.6 | 0.0 | 0.001 | 0.07 | 2,291 (1,420; 1,916) | 0 | 30.6 | 34.5 | 363 | 43 | 818 | 0 | 0 | 1,822 -> 2,094 | True |
| steer-cal-Hp | 0.400 | 0.400 | 0.400 | 0.518 | 30.3 | 21.8 | 29.0 | 0.017 | 0.41 | 1,066 (200; 516) | 10 | 30.6 | 30.2 | 34 | 276 | 0 | 241 | 156 | 2,035 -> 2,412 | True |
| steer-cal-P | 0.078 | 0.101 | 0.056 | 0.305 | 12.5 | 12.5 | 0.0 | 0.001 | 0.07 | 1,482 (1,037; 1,441) | 0 | 30.0 | 34.0 | 308 | 113 | 1,067 | 0 | 0 | 1,820 -> 2,236 | True |
| steer-b0-right | 0.430 | 0.430 | 0.430 | 0.546 | 23.3 | 23.3 | - | 0.024 | 0.74 | 815 (200; 364) | 12 | 30.7 | 34.4 | 11 | 334 | - | - | 98 | 1,729 -> 2,214 | True |

Income by class (the full log), the luxuries and cheese (its rules untouched):

| run | upper % | middle % | lower % | Houses' luxuries p/day | cheese p a piece | cheese pieces/day |
|---|---|---|---|---|---|---|
| steer-cal-L30 | 6.2 | 54.7 | 39.2 | 105 | 5.02 | 3.7 |
| steer-cal-L40 | 6.1 | 54.9 | 39.0 | 105 | 5.00 | 3.7 |
| steer-cal-L50 | 6.1 | 54.7 | 39.2 | 107 | 5.00 | 3.7 |
| steer-cal-R33 | 9.4 | 50.8 | 39.9 | 66 | 14.71 | 3.7 |
| steer-cal-R66 | 10.9 | 49.5 | 39.6 | 61 | 15.96 | 3.7 |
| steer-cal-R100 | 12.2 | 48.7 | 39.2 | 42 | 20.11 | 3.7 |
| steer-cal-C | 7.3 | 54.0 | 38.7 | 92 | 6.92 | 3.7 |
| steer-cal-Hm | 6.8 | 55.0 | 38.2 | 93 | 5.23 | 3.7 |
| steer-cal-Hp | 8.9 | 51.5 | 39.6 | 79 | 11.88 | 3.7 |
| steer-cal-P | 6.4 | 54.5 | 39.1 | 97 | 5.01 | 3.7 |
| steer-b0-right | 8.2 | 52.6 | 39.2 | 63 | 15.46 | 3.7 |

## The curves and the half marks

- levelling: -1 ships F3; the lever is 0 at both its marks
- charters: 1 ships F1; the lever is 0 at both its marks
- The left's coupling (steer-cal-P against steer-cal-L40: spendAbove 25p -> 30p at -1): tills short 31.3 -> 34.0 (+0.54 a penny); G 0.081 -> 0.078; H 0.000 -> 0.001; treasury 1493 -> 1482p.

## The table written

`/home/martinb/Documents/GitHub/MUDProject/.claude/worktrees/r2-controller/artifacts/economy/r2/steer-table-calibrated.json`:

```json
{
 "levelling": [
  0,
  0,
  0,
  0,
  0
 ],
 "charters": [
  0,
  0,
  0,
  0,
  0
 ]
}
```

## The targets at each mark

`/home/martinb/Documents/GitHub/MUDProject/.claude/worktrees/r2-controller/artifacts/economy/r2/steer-targets-cal.json`:

| mark | measure | target | band | tills short | treasury | floor from |
|---|---|---|---|---|---|---|
| -1 | - | - | - | - | - | this side ships its fallback rung; its targets are the rung's reference run's |
| -0.5 | - | - | - | - | - | this side ships its fallback rung; its targets are the rung's reference run's |
| 0 | G (m_g_day) | 0.35 | 0.0208 | 29.14 | 1316 | steer-cal-C |
| 0 | S (m_upper_share) | 13.4346 | 1.0 | 29.14 | 1316 | steer-cal-C |
| 0.5 | - | - | - | - | - | this side ships its fallback rung; its targets are the rung's reference run's |
| 1 | - | - | - | - | - | this side ships its fallback rung; its targets are the rung's reference run's |

The treasury floors (T5; the floor run's 28-day mean, its seasonal profile less 2 sigma, never under 600p):

| mark | floor run | sigma | floor least | floor most |
|---|---|---|---|---|
| 0 | steer-cal-C | 10 | 870 | 1873 |
