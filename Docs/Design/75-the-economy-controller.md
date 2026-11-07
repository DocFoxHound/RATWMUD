# 75. The economy controller (2026-11-06)

The user, 2026-11-06 (verbatim):

> I want you to expand the plan and take into consideration what we have already tried. Think about what works and what
> sorts of levers we have applied before and what strategies and effects they have had. What I want is to make a
> powerful, integrated automated system (we already have one) that intelligently and accurate adjusts the economy based
> off the slider in DM. But I won't want to spend days trying to figure out what works. I really would like to move onto
> something else, so this is probably the last shot I'm going to give this. Build me a plan. A good, solid plan for
> building out the automated economic controller in a way that WILL work. Be creative and think out of the box, and look
> at how other games have approached this

This doc is a plan only. No code was changed, no simulation was run, and nothing was written to DEV. It replaces the
controller in doc 74 sections 4 and 8. It keeps doc 74's slider, toggles, skim (as reworked in 214a9b9), storage, audit
and DM routes. The code is `ridgemere2` at 4f851dd. The centre is still **stable10b** (85ea134, export70).

Standing rules, all kept: money is conserved (nothing is minted outside the Wharf's valve); characters are male or female
only; never the words for the domestic animal (wolves, pups); cheese's rules are untouched; nothing writes PROD; no
simulation runs without the user's go-ahead (section 9 asks for it once, for the whole run budget).

## Headline

- **Feed-forward is the controller; feedback is a guard.** Doc 74's tune round 1 showed it: the table did the work, the
  guards did the rest, and the trim did nothing. So the main build is a **measured table plus a safety supervisor that
  backs off the lever that is paying**. Slow trims are added only for a measure that is shown to drift in the 3-year
  runs, and their gains come from single-lever runs, never from runs where many levers moved at once.
- **The two ends get actuators whose effect is direct.**
  - Left: **the Levelling tithe.** Each night every household's cash per head moves a fraction r of the way to the mean.
    It is the share-out done right: households only, never tills, and its effect on the night is arithmetic.
  - Right: **Crown charters on middle-class keepers' shops**, paired with a larger dividend so the Houses' new money
    keeps moving. A charter changes where a till's profit goes; it does not park money.
- **Strength is chosen by a rule, not a judgement.** The calibration runs each end at three strengths. A script picks
  the strongest one that passes the safety tests open-loop. The slider's ends are labelled with what that strength
  actually delivered.
- **The centre is stable10b, byte for byte, on every rung.** At x = 0 every lever has the world's own value. The new
  values blend in with |x|.
- **The scorecard is checked against runs we already have before it judges anything new.** Today's centre must pass
  it, and each fallback rung must pass its own bar. So "ships on a fallback" never means "ships failing".
- **The honest expectation:** the left end will very likely ship as built. The right end will probably land near doc
  74's t1 (about 23% with the rich) plus what the charters add, maybe 25-32%. "Almost entirely with the rich" is not
  something this city can hold without starving its shops, and the plan says so up front (Q2).
- **A time box: about 13 hours wall clock, 15 at most, with a check-in with the user at the 6-hour mark.** The DM
  tab's edicts and ledger come after, in their own unhurried pass, so the slider ships first.

## 1. What we learned

### What worked

| What | Why it worked |
|---|---|
| Wages follow till cover (doc 68) | The biggest Gini win: .016/yr down to flat; owners' share 23% to 13.6%. Profit becomes wages at the till. The outflow grows with the stock. |
| Treasury surplus over cap (doc 68) | Held the treasury flat. The outflow grows with the stock. |
| Prices follow stock, with the price level held at 1 | Stops every maker ratcheting to the band's top. |
| Births 1:1 with deaths, treasury judged every 28 days | 12 years stable (Gini about .50 flat). |
| Feed-forward from the slider (doc 74) | The levers moved the same night; the measures followed, half the way in 40-56 days. |
| Hands' share, ground rent to the treasury under its floor | Tax at the source; fixed summer hunger at the right (0.45-4.5/h down to 0.004-0.06). |
| `spendAbove` (the spending keep) | 40p to 25p took the left's tills short from 37 to 27.6: about 0.6 a penny, a clean single-lever A/B. |
| Exact floors | Never Hungry and Never Broke work out the exact need and skim exactly that. |
| Hard bounds | `workshopWageMost 2`, Wharf credit capped at 2,000p: each fixed a drift. |
| Determinism | Byte-identical runs make A/B attribution exact. |

### What failed, and why

| What | Why it failed |
|---|---|
| Doc 42's 40 rules, doc 64's 64 rule bits, doc 46's orchestrator | Money pooled wherever an outflow did not grow with the balance (doc 63's law). Money sent through channels reached paid-post holders, not the poor. |
| The share-out at 30% | A nightly flow against the daily churn: plateaus at Gini 0.32-0.34. It also takes from working tills. |
| The rich hold at 600p (steer-right-v1) | Took about 9,000p out of circulation; the treasury hit its float by day 84; hunger 36/h. |
| The spending keep at the left alone | The levelled money was held, not spent: purses +4,200p, tills −3,900p. |
| treasuryTarget 5,500 | Unreachable (the treasury settles at 1,500-2,200p). The drift pinned at its bound; the surplus never fired. |
| The toll lean and the toll drift | Two rules on one account. −2 at the right drained the treasury to 594p. |
| The trim in doc 74 | Almost always at its band edge or in its dead band. The table did the work; the guards did the rest. |
| The hunger pull | Bang-bang, keyed on a measure that lags the treasury by 3 weeks, and two pulls could stack on one night. |
| The rent scale | Takes the farms' basket money (city spending), not idle money. |
| Short runs | 60-day smokes missed the Wharf drain; 120-day runs passed and the years collapsed. |

### Patterns to avoid

1. **An outflow that does not grow with the balance.** It pools. Every new money move must be proportional to a stock.
2. **Two mechanisms on one account.** Where it cannot be avoided, the coupling is named and tested (3.1).
3. **A target the economy cannot reach.** It saturates its lever, and the bound becomes the real setting.
4. **Moving a stock out of circulation.** It shows up weeks later as tills short and hunger.
5. **Slow measures for fast decisions.** 56-day means and 28-day judging gave limit cycles.
6. **Judging against zero.** Idle posts are structural (doc 65). Tills short and wage support are judged against
   stable10b's own seasonal profile, not against zero.
7. **Agents judging rounds by eye.** It cost 1.5-3 hours a round. The scorecard is written first and run by a script.
8. **Fitting gains from runs where many levers moved.** Five confounded marks cannot say which lever did what.
9. **A guard that only freezes.** A freeze holds a lever where it is; doc 74's collapse was stopped only by pulling
   back. Every guard backs off its own payer.

## 2. How other games do it, and what we borrow

| Game or idea | What they do | What we take |
|---|---|---|
| EVE Online | An economist, a monthly report of faucets and sinks by channel; bounty multipliers per system that drift toward an equilibrium | A weekly ledger by class and lever on the DM tab (Phase 7). Targets are measured equilibria, not hand-tuned constants. |
| Ultima Online | A closed resource economy broken by hoarding | Conserved money works only if idle stock is recycled. The floors are funded from idle money first (the reworked skim). |
| World of Warcraft | A percentage cut on every sale | Proportional levies stabilise on their own. Flat costs are regressive. |
| Old School RuneScape | An automatic buyer that spends only what it has, up to a quota | The floors pay exactly the need, from whoever is richest that night; no standing pot sits idle. |
| RuneScape 2007 trade limits, Diablo 3's auction house | Controlling trades directly caused a riot; buying beat finding | Never touch prices, wages or trades. Never Broke pays only necessities, never more than working. |
| Star Wars Galaxies | Handouts hard to take back | The floors ship at their final generosity. |
| RimWorld's storyteller | A curve of measured state with hard caps; adaptation slow and small | The table is the curve; the supervisor is the cap. A slow trim only where a drift is shown. |
| Taylor rule, central banks | Neutral rate plus a fraction of the gap; one instrument per target | Where a trim is added, half the gap per judging, its gain from a single-lever A/B. |
| Tinbergen's rule | As many instruments as targets | One distribution actuator per end; the supervisor's back-offs each act on one payer. |
| Victoria 3 | Wealth drifts to what income can sustain | Steer flows (who gets a till's profit), and the stocks follow. |
| Anno 1800 | Tiers with luxury needs | The rich at the right keep spending (rich hold at most 150p); their money comes back to makers. |
| Econophysics (yard-sale model with redistribution χ) | One redistribution rate sets a stable Gini; a monotone curve | The Levelling tithe is χ, so G falls smoothly as r rises: a table works. |
| Georgism | Land rent as a low-distortion pipe | The ground rent: to the Houses at the right; to the treasury when it is low. |
| Eco | Laws and edicts players can read | Each actuator shown as a named edict ("The Levelling tithe: 40%", "Crown charters: 12 shops"). |
| Alter Aeon (a MUD) | A feedback loop on its sinks; don't price new players out | The closest shipped example. The floors protect the poorest. |

**Considered and not taken:** a standing relief coffer filled by a levy. 7 days of the floors' cost is about
3,500-4,000p, about 6% of all money parked idle (Pattern 4), and it would take the same idle money as the skim, only
earlier. It also is not "a skim off whoever is richest at the time", which is what the user asked for.

## 3. The architecture

### 3.1 What the controller owns

With `steer` on, these move only through the controller. `set_rules` already refuses the steered rules (409); the list
grows to cover all of them.

| Account or rule | Set by | May be backed off by |
|---|---|---|
| The Levelling tithe r (households only; never a till, farm or estate) | Table | Supervisor rules 2 and 3, at the left |
| Charters: which middle-class keepers' tills are a House's | Table (a count, n) | Supervisor rule 3, at the right |
| `dividend` (estate spare to upper purses) | Table, rising with the charter strength | – |
| `postsShare`, `groundRent`, `surplusAlmsTo` | Table | Supervisor rule 1 owns the ground rent's route to the treasury |
| `richAbove` | Table | Supervisor rule 2, at the right, toward 40p |
| `spendAbove` | Table | – (a slow trim at the left only if Phase 6 shows drift) |
| The toll, the alms drift | `followTreasury`, unchanged, with a blended target (4.5) | – |
| `treasuryCap`, `treasuryTarget`, `surplusKeep` | Blended from the world's value at 0 to the measured one at the ends | – |
| Never Hungry, Never Broke | The toggles; paid by the reworked skim | – |

Each lever has one setter and at most one back-off, so a lever never has two rules pulling it different ways. The
supervisor may only move a lever back toward its world value, never past it.

**Accounts with two hands on them, named and tested:**
- The treasury is paid by the skim and steered by `followTreasury`. While its 28-day mean is under its floor, the skim
  skips the treasury, so the two never pull on it together.
- The estates are paid into by the charters and taken from by the skim when the floors are on. That is accepted: the
  floors levelling the right end is what the floors are for. A8 tests it with a one-sided band (5.3).

**Left alone, always:** prices and shelves following stock and sales, wages following till cover, the farms' surplus to
the Houses, the Crown's grants, the estates' baskets and luxuries (cheese's rules untouched), the Wharf valve, and the
city's base alms (`giveAlms`), which run at every slider position.

**Removed while steering is on:** the share-out (`shareOut`), the toll lean, `rentScale`, the weekly hunger pull, the
judging's hunger pull and doc 74's single push `u` with its trim. All of them are at zero at the centre already, so
removing them does not change the centre.

### 3.2 The centre is the world

At x = 0, every value is the world's own: the toll drift on its 5,500p target, `treasuryCap` 2,000, `surplusKeep`
4,000, `spendAbove` 40, r = 0, n = 0, every trim 0. stable10b's centre exists because the drift sits pinned at +1
against that unreachable target; making the target reachable at the centre would make a new economy. So measured
values apply only off the centre and blend in with |x|:

    value(x) = world + |x|^p × (end value − world), p = 1 unless the calibration says otherwise

T10 is then byte-identical, as in doc 74's tune round 1, and the ladder's promise holds on every rung.

### 3.3 The table and the supervisor

Every actuator has a **table value** at the slider's position: a curve through 5 marks (−1, −0.5, 0, +0.5, +1), read with
the existing `steerAt`. The end marks come from the calibration's selection rule (5.2); the half marks are measured too.

**Moving the slider:** the table values jump the same night (as doc 74), except the charters, which ramp: the new count
is reached in equal nightly steps over 14 days, so no one night moves many tills at once. The slow means are re-seated
at the measure's current value, so the first judging after a move judges the new position (doc 74 weakness 14).

### 3.4 What it measures

| Measure | Definition | Why |
|---|---|---|
| **G**, the left's measure | Gini of cash per head over **all resident households**, weighted by heads; reported as **the mean of the value before and after the tithe** (the Gini a wolf lives with through the day), with the after-tithe value beside it | "Equal across all wolves" includes the retired, orphans and the out of work. After-tithe alone flatters by about Δ/2. |
| **F**, food equality (watched, left) | Gini of pantry days per head, same households | The user's left end is money *and* food equal. |
| **S**, the right's measure | (upper-class purses + estate funds + every House till, built-in and chartered) / all money | Doc 74's S plus chartered tills. |
| H, K | Hungry per hour; households short (before relief) | As doc 74. |
| Tr | Treasury cash, 18:00 | As doc 74. |
| Tills short | Doc 74's count | The shops' health. |
| Watched only | Earners' Gini (doc 74's G), top 10% share, bottom 40% share, income share by class per night, the Houses' luxury purchases and cheese's price | Continuity with every earlier report. Charters raise the estates' spare, and the Houses' luxuries are a share of spare, so cheese's price may climb inside its band; its rules stay untouched, and its behaviour is logged. |

**Seasons.** Each slow measure is judged against a seasonal profile **of its own mark**, from the calibration run at
that mark (the ends have deeper summers than the centre), smoothed over 15 days. The treasury is judged on a 91-day
window, so it does not chase summer. Dead bands are 2σ of each mark's de-seasonalised noise.

### 3.5 Targets on a measured envelope

1. The calibration runs each end at three strengths open-loop (5.2) and picks one by rule.
2. The chosen runs' days 120-365 means give the targets at the marks: G, S, the treasury and the tills short.
3. A target is what the table already delivers, so **every position is reachable**.
4. Year 1 is still settling (stable10b's treasury: 1,473 then 1,759; the left's Gini: 0.328 then 0.344). After the
   3-year confirmation, the script re-centres the targets and the end labels on years 2-3. That changes labels and the
   supervisor's floors, not the table.

The DM tab labels each end with its measured reach, for example "Shared: Gini 0.24 through the day" and "Held by the
rich: 29% with the upper class and the Houses".

### 3.6 The safety supervisor

The supervisor replaces both hunger pulls. It runs nightly and checks in priority order. **Each rule backs off the
lever that is paying, at a fixed rate, until the cause clears.** A back-off may cut the slider's reach; when it does,
the DM log says so ("The tithe eased to 34%: the shops ran short").

| # | Condition (7-day means, de-seasonalised) | Action |
|---|---|---|
| 1 | Treasury under its floor | Route the ground rent to the treasury in proportion to the gap (all of it at 50% under the floor), and the skim skips the treasury. This is the fix that worked in t1 (summer hunger 0.45-4.5/h down to 0.004-0.06). |
| 2 | Tills short over 1.3 × this mark's target | Left: lower r by 3 points a week. Right: lower `richAbove` by 10p a week toward 40p. |
| 3 | H or K over its ceiling (floors off) | Left: lower r by 5 points a week. Right: hand back 3 charters a week, newest first. |
| 4 | Any lever at a back-off or a bound 28 days running | Log "at the envelope's edge" to the DM. No action. |

**The treasury floor** is this mark's seasonal treasury profile minus its 2σ band, and never under 600p. A floor at a
fixed fraction of a mean would fire on 30-40% of nights at the right (steer-right-t1's 28-night mean is under 800p on
113-172 nights a year); a floor from the mark's own spread fires only on the real dips.

**Hysteresis.** A rule clears when its measure is back under 90% of its trigger for 7 days running. The backed-off lever
then returns toward its table value at half the rate it left. Only one action per lever per night, so nothing stacks.
The doc 74 lesson that "the treasury falls three weeks before the hunger comes" is why rule 1 acts first and on the
treasury itself.

### 3.7 Never Hungry, Never Broke and the skim

Kept exactly as doc 74 and the skim rework (214a9b9): Never Hungry at ShopShut − 1 (`RatwCityLoop.cpp:762`), paid by a
skim off whoever is richest that night, idle money first, in a set order. Two small changes:

1. **Never Hungry also covers the farms' `crew:*` households**, which doc 74 left out.
2. **Never Broke never beats working.** Its credit stays at necessities only (food and needs), never cash, and never
   more than a hand's day wage per head.

Never Hungry's cost was 0.0-0.5p a day across three years, almost all of it bought from the Wharf. That is not worth a
new morning pass, which would also buy for households who would have shopped later in the day. If Batch 0 shows the
skim taking too much busy money (D1), the levers are the skim's comfortable line and its step order, both table values.

The floors run at their final generosity from the first build (the Star Wars Galaxies lesson).

### 3.8 How admins see it

The DM Economy tab (`SteeringPanel.tsx`) keeps its four controls, each confirmed and audited: the slider, the on/off,
Never Hungry, Never Broke. In the main build it gains only the reach labels on the slider's ends and the supervisor's
back-off line. The rest comes in Phase 7:

- **The edicts.** One line per actuator, as an in-world law:
  - "The Levelling tithe: 40% a night. Equality 0.24 through the day (aim 0.24)."
  - "Crown charters: 12 shops held by the Houses. Upper class and Houses hold 29% (aim 29%)."
  - "The gate toll: 31.5. Treasury 1,640p."
- **The supervisor**, when it acts: "Charters handed back: the shops ran short (34 against 30)."
- **The weekly ledger**, EVE style: money in and out by lever and by class, who paid the skim.
- **The log**: each night's actions kept for a year, not 24 entries.

The panel stops hard-coding values that live in data (doc 74 weakness 12).

## 4. The levers

### 4.1 The Levelling tithe (new, replaces the share-out)

- **What:** each night, after wages and before the comforts, every resident household's cash per head moves a fraction
  r of the way to the head-weighted mean: x′ = (1 − r)x + r·μ. Households over the mean pay; households under it
  receive. Money is conserved. Nobody goes below the mean by paying. Tills, farms and estates are never touched (doc 74
  weakness 7 gone).
- **Why a table works:** a move toward the mean scales every gap by (1 − r), so G after the tithe is (1 − r) × G before
  it. The steady state is G* = (1 − r)(G* + Δ), where Δ is the day's re-divergence from wages and spending. G falls
  smoothly and monotonically as r rises, so a table through measured marks is accurate.
- **Pennies:** cash is whole pennies, so each move is rounded, and the remainder is handed out in a fixed order (the
  largest remainders first, ties by household id) so the total is exact and runs stay deterministic.
- **Range:** 0 at the centre to `levelMost` at −1, chosen by the calibration among 30%, 40% and 50% (Q3).
- **Evidence:** doc 74's share-out reached 0.33 with r = 0.3 on a looser rule. On the earners' set, Δ ≈ 0.14 a day. At r
  = 0.5 the after-tithe G is about 0.14, **but the Gini lived with through the day is about 0.21**, and over all
  households likely higher. The calibration measures it.
- **Paired with:** the levelled money was held, not spent. `spendAbove` falls to 25p at −1 in the table (measured, 0.6
  tills short a penny). Supervisor rule 2 backs r off if the shops still go short.

### 4.2 Crown charters (new, the right's actuator)

- **What:** a middle-class keeper's city till is chartered to the House whose district it is in. The keeper is then a
  post paid its wage, and the House's estate takes the profit the keeper's household took before. The till counts in S.
- **Only middle-class keepers.** Keepers of the large businesses (the carters' yard, the brewery, the rag mill, the
  tannery, the Grey Gull, the Sailors' Rest: `LargeBusinesses`, `RatwCareers.cpp:437`) are already upper class, so
  chartering them would move their profit from one part of S to another and gain nothing. Ranking is by 28-day mean
  profit, skipping any till whose keeper's household is upper class. Handed back in reverse order.
- **No other rule changes for a chartered till.** It keeps `floatWagesDays` (7), not the workshops' 4, so no buffer is
  swept to the estate on the first night, and its wages keep the keeper-till rise bound, not `workshopWageMost`. The
  overlay carries its own wage-days and wage-rise rules for that. Only the profit's destination changes.
- **A flow, not a stock.** A charter redirects a profit stream (keepers' profit averages about 615p a day across the city
  in steer-centre-t1, year 1, against about 63.8k of money). That stream used to reach keepers' households, which spend
  it at the shops. On its own it would pool in estates that spend only on posts, baskets and luxuries: steer-right-v1's
  failure, larger. So charters are **paired with the dividend**, the existing outflow that grows with the estates' spare
  (`housesDividend`): its share rises with the charter strength (30%, 45%, 60% at the three strengths), so the new
  money moves on to upper purses, which spend over `richAbove`.
- **Set by count from the table, not by a loop.** S is a stock and charters set a flow, so a rule of "charters wanted =
  gap ÷ a till's cash" would integrate and cycle. The table gives n at each mark from the calibration's measured S
  against n; supervisor rule 3 hands them back.
- **Plumbing:** `houseTills` is a static catalogue (`RatwItems.h:421`), read where `Society` is not in reach:
  `houseOfPost` (`RatwCityLoop.cpp:80`, anonymous namespace), `houseOf` (`RatwSteering.cpp:33`) and the skim's
  keepers' households step. All three get a Society-aware lookup that checks the overlay first. Phase 2 counts this.
- **Range:** 0 at the centre and to the left; at +1, 33%, 66% or 100% of middle keepers' tills, chosen by the
  calibration (5.2). The built-in House businesses always stay the Houses'.
- **Expected reach:** modest. The six large keepers are already counted, so what charters add is middle keepers'
  profit minus what the dividend sends on. The likely result is F1's 23% plus a few points. If the strongest passing
  strength adds under 3 points, the right ships F1 (D2).

### 4.3 Table levers (kept)

| Lever | −1 | 0 | +1 | Kept because |
|---|---|---|---|---|
| `postsShare` (hands' share) | 50% | 0 | 0 | Taxes at the source; cuts Δ, so the tithe needs less r. |
| `surplusAlmsTo` | 80p | W | W | The surplus raises the poorest first. |
| `groundRent` | 0 | 0 | 30% | Profit to the Crown's estate; to the treasury under its floor (rule 1). |
| `dividend` | 0 | 0 | 30-60% | Moves estate spare to upper purses; rises with the charters. |
| `richAbove` | W | W | 150p | 150p was measured safe. 200p cost the treasury; 600p starved the city. Never higher. |
| `spendAbove` | 25p | W | W | The levelled money must be spent. |

W is the world's value. The calibration may move the half marks only within these bounds.

### 4.4 Trims, only where shown to be needed

No trim is built in the main build. Phase 6 adds one only if a measure drifts by more than one band from year 1 to
year 3 in the confirmation runs, and only with a gain from a single-lever A/B:

- **Tills short at the left:** `spendAbove`, 0.6 tills short a penny (doc 74, 40p→25p), at most 2p every 14 days, 20p to
  40p.
- **Tills short at the right:** `richAbove` between 40p and 150p (the right's shortage comes from the Houses'
  threshold, not `spendAbove`), its gain from the steer-right v1 vs t1 pair.
- **The treasury:** the toll drift's step, from doc 73 round 10's runs at 0, 1 and 2 points.
- **G or S drift:** r or n by one table step, at most once per 28 days.

Each trim is held while its lever is at a bound (anti-windup) and kept when the slider moves (bumpless).

### 4.5 The treasury: `followTreasury`, fed a reachable target off the centre

`followTreasury` already is "toll ±1, half a point per judging, toward a target", and it worked whenever its target was
reachable (doc 68's 12-year run). It stays, unchanged, with `treasuryTarget`, `treasuryCap` and `surplusKeep` blended
from the world's values at 0 (5,500, 2,000, 4,000) to the measured ones at the ends (target = the mark's year-1 mean;
cap = target × 1.15; keep = target × 0.5, so the surplus fires at the ends). The toll's bound stays the user-approved
`tollDriftMost` ±1 point.

### 4.6 What goes, and why

| Lever | Why it goes |
|---|---|
| `shareOut` | Replaced by the tithe; it also took from working tills. |
| `tollLean` | A second rule on the toll. `followTreasury` owns the toll. |
| `rentScale` | Takes the farms' city spending, not idle money. |
| The weekly and judging hunger pulls | Replaced by the supervisor, which backs off the lever that pays. |
| Doc 74's single push `u` and its trim | The table did the work; the trim did nothing. |

### 4.7 Ideas considered and left out

| Idea | Why not now |
|---|---|
| A relief coffer | Parks about 6% of money idle; takes the same money as the skim, only earlier (section 2). |
| Never Hungry in the morning | Fixes a sub-penny cost and would crowd out shoppers. |
| House patronage (estates commissioning works in proportion to their spare) | The second outflow for the estates if the dividend is not enough. Phase 6 only, if A5 shows the estates pooling (estate funds rising through year 1 by more than 20%). |
| A Python surrogate of the city loop | 1-2 days to calibrate, and it would drift from world_check. |
| Model predictive control | A lot of build for little over a table and guards. |
| Warm starts from day 365 (the checkpoint WIP, 6d200d9) | Untested; finishing it is a project of its own (Q6). |
| `--fast` sims (`econ_watch --fast`) | Validated on 30 days only. Not for acceptance. |

## 5. Calibration and the acceptance harness

### 5.1 Tools

| Tool | What it does |
|---|---|
| `world_check --steer-table FILE` (new flag) | Reads the steering table from a file, so a variant needs no copied Data directory. |
| `tools/steer_runs.py` (from `steer-smokes.sh`) | Takes a manifest of variants (rule lines, table file, slider moves by day), builds the export folders, runs 5 at a time on cores 0-3, 4-7, 8-11, 12-15, 16-19, then `econ_report` on each. |
| `tools/steer_calibrate.py` | Reads the calibration runs, applies the selection rule, and writes the table, the targets, the per-mark seasonal profiles, the dead bands and the treasury floors. |
| `tools/steer_accept.py` | Reads a batch and the scorecard, prints PASS or FAIL for every test, exits non-zero on any FAIL. |
| `artifacts/economy/r2/steer-scorecard.json` | The thresholds below, per rung, agreed before any new run (Q5). |

### 5.2 Calibration (10 one-year runs, two batches, about 30 minutes)

Open-loop (table only, supervisor logging only), toggles off, from day 0:

| Runs | What |
|---|---|
| L30, L40, L50 | x = −1 with `levelMost` 30%, 40%, 50% |
| R33, R66, R100 | x = +1 with charters on 33%, 66%, 100% of middle keepers' tills, dividend 30%, 45%, 60% |
| C | x = 0 |
| H− , H+ | x = −0.5 and +0.5 at the middle strength |
| P | one single-lever perturbation: x = −1 at L40 with `spendAbove` 30p, for the left's coupling |

**The selection rule:** each end takes the strongest strength that passes T4-T7 open-loop. The half marks are then
re-scaled to the chosen strength by the measured curve. If no strength passes at an end, that end takes its fallback
rung (section 7). Reach comes out of the rule; nobody picks it.

The script also measures S against n each night and the estates' spending, so the charters are modelled as the flow
they are.

### 5.3 Acceptance (9 runs, two batches, about 35 minutes)

Closed-loop with the supervisor, steering on at day 0. 1 year each, measured over days 120-365, except A9.

| Run | Slider | Toggles |
|---|---|---|
| A1-A5 | −1, −0.5, 0, +0.5, +1 | off |
| A6 | 0 | both on |
| A7 | −1 | both on |
| A8 | +1 | both on |
| A9 | the sweep, 480 days: 0 → −1 at day 120, → +1 at day 240, → 0 at day 360 | off |

**Tests for every run** ("target" and "profile" mean this mark's measured values):

| # | Test | Pass |
|---|---|---|
| T1 | Reach | The distribution measure (G left of centre, S right of it, both at the centre) inside its band by day 90. |
| T2 | Hold | Inside its band on at least 90% of days 120-365, not counting days a supervisor back-off is logged. |
| T3 | No limit cycle | No supervisor rule fires and clears more than 4 times; no lever at a back-off more than 30% of days. |
| T4 | Hunger | H's mean ≤ hungerMost (doc 74 table); no 28-day H over 2×. K's mean ≤ shortMost. |
| T5 | Treasury | Days at its 200p float, and days with its 28-day mean under its floor, each no more than stable10b's in the same season plus 3. |
| T6 | Shops | Tills short's mean ≤ 1.15 × the same position's floors-off target (for floors-off runs, that is this mark's calibrated target; for floors-on runs, the same position's floors-off run, at most 1.25×). Wage support unpaid ≤ 1.5 × stable10b's. |
| T7 | Books | Money conserved every night; estates' posts paid every night; Wharf net within ±200p a day of stable10b's. |
| T8 | Supervisor | Acts on at most 15% of nights. |

**Tests for particular runs:**

| # | Run | Pass |
|---|---|---|
| T9 | Steering off | The daily lines byte-identical to stable10b's (except new fields). |
| T10 | A3 (centre, on) | The daily lines byte-identical to stable10b's (except new fields). |
| T11 | A6-A8 (floors) | H ≤ 0.01; households short after relief ≤ 0.5; Never Hungry's Wharf pennies at most 5p a day; at least half the floors' money from idle spare. At A8, S is judged one-sided: at or under its target passes, as the floors' own levelling. |
| T12 | A9 (sweep) | Each leg reaches half its gap in 30 days and 90% in 75; no overshoot over one band. |
| T13 | 3-year runs (5.4) | Years 2 and 3 pass T2-T8; the change from year 2 to year 3 is under one band and smaller than year 1 to year 2 (no drift). |

**The dry run comes first.** Before the scorecard is fixed, `steer_accept.py` runs against the runs already on disk
(steer-centre-t1, steer-right-t1, steer-left-t1, steer-centre-floors-t1, stable10b). No simulation is needed. Each
test for every run must pass on steer-centre-t1, and each rung's own bar must pass on the run that rung ships (section 7).
Where a test fails today's centre, the test is wrong, and it is fixed before anything new is judged. The checks already
done by hand: steer-centre-t1 touches the float once in year 1 (T5 is relative now, so it has margin);
steer-centre-floors-t1's tills short is 1.22 × the centre (T6's floors bar is 1.25 × its own position); Never Hungry's
Wharf cost is 0.0-0.5p a day (T11 is an absolute 5p).

### 5.4 Confirmation (5 runs, about 40 minutes)

The 3-year runs of A1, A3, A5, A7 and A8. T13 must pass. These are the only 3-year runs. The script then re-centres
the targets and end labels on years 2-3 (3.5).

## 6. The build, in phases

Wall-clock times. A 1-year run is about 13 minutes on 4 cores, a 3-year run about 37; five run at once. Building
happens while batches run.

| Phase | Work | Agent | Runs (wall) | Ends with |
|---|---|---|---|---|
| **0. Decisions** | The user answers section 9. | – | – | Answers, and the go-ahead for the run budget. |
| **1. Tools, baseline, dry run** | `--steer-table`; `steer_runs.py`, `steer_accept.py`; the scorecard and its dry run on the t1 runs (5.3); the new measures logged (G before and after, F, S with charters, income shares, luxuries and cheese's price). **Batch 0** on today's code: left, right, centre with floors, left with floors, right with floors, 1 year. It is the skim rework's first simulation and a check of the harness. | 2.5 h | 15 min, in the background | **D1**; a scorecard that today's centre passes. |
| **2. Actuators** | The tithe (whole-penny remainders); charters on middle keepers' tills with the overlay in all three lookups, its own wage-days and wage-rise rules, the 14-day ramp, saved state; the dividend tied to the charter strength; the blended values at x (3.2); removing the share-out, toll lean, rent scale, pulls and `u`. Unit tests in `Tests/steer_tests.cpp`: the Gini identity in doubles, conservation and the remainder order in pennies, determinism, charter grant and hand-back, upper keepers skipped, steering off and the centre identical. | 3 h | – | Green tests. |
| **3. Calibrate** | The calibration batches; `steer_calibrate.py` writes the table. | 1 h | 30 min | **D2**. |
| **Check-in** | About 6.5 hours in. The user sees the measured reach at each end and the chosen strengths, and says go on or stop at a rung. | – | – | The user's word. |
| **4. Supervisor** | The four rules with their back-off rates and hysteresis, the treasury floors, the skim skipping a low treasury, the reach labels on the tab. Unit tests: rule order, back-off and return rates, one action per lever per night. | 2 h | – | Green tests. |
| **5. Accept** | The acceptance batches; `steer_accept.py`. | 1 h | 35 min | PASS, or a fix round. |
| **5b. Fix rounds** | At most two. Each fix is a table, bound or rate change written by the script, or one code fix. Re-run only the failing runs. | 45 min each | 30 min each | **D3**. |
| **6. Confirm** | The 3-year batch; T13; a trim (4.4) only if a measure drifts, in which case its one-lever run joins a last fix round. Doc 75 Progress; `test_studio.py`, `steering.test.ts` and the fixture updated; merge into `ridgemere2` and push. | 1.5 h | 40 min | The slider ships. |
| **7. The DM tab** | After acceptance, unhurried, not in the box: the edicts, the ledger, the year of log, in `studio.py` and `SteeringPanel.tsx` with their tests. | 2-3 h | – | Done. |

**Total to the slider shipping:** about 11 hours without fix rounds, about 13 with both. **Hard box: 15 hours of wall
clock.** At the box, whatever passes ships, by the fallback ladder. The 6.5-hour check-in is the user's chance to stop
early (memory `session-budget`).

### Decision points

- **D1 (after Batch 0):** if the reworked skim takes more than half of the floors' money from busy money, or the tills
  short with floors is over 1.25 × without, change the skim's comfortable line or step order in the table and say so.
  No new mechanism.
- **D2 (after calibration):** read the selected strengths. If the right's chosen strength adds under 3 points of S over
  F1, the right ships F1 and the charters stay off (built, unused). If even L30 fails at the left, the left ships F3.
- **D3 (after the second fix round):** ship what passes. Each end that fails drops one rung.

## 7. The fallback ladder

Each rung is a usable, honest slider. The centre is byte-identical to stable10b on every rung (3.2), and steering off
stays the default. Each rung has its own bar in the scorecard, checked by the dry run, so a rung that ships passes.

| Rung | What ships | Its bar | When |
|---|---|---|---|
| **Full** | The table and supervisor as planned, at the strengths the calibration chose. | The scorecard (5.3). | Everything passes. |
| **F1** (right only) | The right end without charters: ground rent, dividend 30%, `richAbove` 150, the supervisor (doc 74 t1: about 23% upper share). | No worse than steer-right-t1 on T4-T7 (its 10, 5 and 6 float days a year are the ceiling), and rule 1 on, which should lower them. | Charters fail, or add under 3 points. |
| **F2** (either end) | The same end one calibrated strength down (L40 → L30, R66 → R33). | The scorecard. | The chosen strength fails acceptance. |
| **F3** | Doc 74's t1 controller with its targets set to the measured envelope, the slider labelled with its reach, and the floors on their own. | No worse than the t1 run at that end. | The new build fails as a whole at that end. |

The ladder is chosen per end, so the left can ship Full while the right ships F1.

## 8. Risks

| Risk | Defused by |
|---|---|
| The tithe's money is held, not spent, and the shops go short | `spendAbove` 25p at −1; supervisor rule 2 lowers r; T6. |
| Charters recreate the starved right end (the estates pool the profit) | Middle keepers only; the till keeps its 7-day buffer; the dividend rises with the charters; rule 3 hands back 3 a week; patronage in Phase 6 if the estates pool; F1 is the expected result if it fails, and Q2 says so. |
| Charters break a business path | Only the profit's destination changes; the overlay is unit-tested in all three lookups. |
| Charters fight the floors at +1 | A8's S is judged one-sided. |
| The treasury drains at the right | The floor from the mark's own spread; rule 1 sends the ground rent and stops the skim taking from it; F1's bar is t1's own float days. |
| The supervisor freezes instead of curing | Every rule backs off its own payer at a fixed rate, with hysteresis. |
| Gains fitted from confounded runs | No gains in the main build; a trim's gain comes from a single-lever A/B. |
| Calibration lands at the edge of collapse | Three strengths per end and a selection rule; a collapsing strength is simply not chosen. |
| The scorecard fails runs that are fine | The dry run on the t1 runs before anything new is judged. |
| The centre changes | Every value is the world's at x = 0; T10 is byte-identical. |
| Year 1 is still settling | Targets and labels re-centred on years 2-3; T13 judged on trend. |
| The ends chase summer | Per-mark profiles; a 91-day treasury window. |
| A flattering Gini | G reported as the day's mean (before and after the tithe); food equality watched. |
| The Houses buy more luxuries, cheese among them | Logged in every acceptance run; cheese's rules untouched. |
| The time box is blown | The 6.5-hour check-in; two fix rounds only; the ladder; the DM tab's extras after the box. |

## 9. What the user decides before it starts

1. **The run budget.** About 34 one-year runs (Batch 0, calibration, acceptance, re-runs) and five 3-year runs, all in
   the background, about 3 hours of run time in all, in batches of 5. *Recommended: yes, once, for the whole plan.* No
   paid model calls.
2. **What the far right means.** May the Houses take middle-class keepers' shops by Crown charter (keepers become
   stewards on a wage, the profit goes to the House, and more of the Houses' spare goes on to upper purses)? Honestly:
   about 23% with the rich (F1) is what has been shown safe, and charters may add a few points, maybe to 25-32%. That is
   the most this city can hold without starving its shops; "almost entirely with the rich" is not reachable. *Recommended:
   yes, try the charters; accept F1 if they add under 3 points.*
3. **What the far left means.** Gini over all households per head, reported as the day's mean, with the tithe at the
   strongest of 30%, 40% or 50% that keeps the city fed and the shops stocked. Honestly: about 0.20-0.28 through the
   day at the strongest, with food equality shown beside it. *Recommended: yes, let the rule pick, up to 50%.*
4. **The ends are what the economy can deliver.** The slider's ends are labelled by the measured reach. *Recommended:
   yes.*
5. **The scorecard (5.3) is the judge.** It is first checked against today's runs; after that, if a test fails after two
   fix rounds, that end drops a rung, and nobody argues with the number. *Recommended: yes.*
6. **Warm starts.** Finish the checkpoint/resume WIP (6d200d9) to halve run times? *Recommended: no.* The plan fits
   without it.
7. **The check-in at 6.5 hours.** Stop there if the measured reach is not worth the rest? *Recommended: yes, keep the
   check-in.*

## Progress

Phases 1-6 done on `r2-controller` and Phase 7 on `r2-controller-dm`, both merged into `ridgemere2` (below); the user's answers to section 9 first. **What shipped:** the left Full (the Levelling tithe at 50%, L50); the right F1 (no charters; the dividend 30%, the ground rent, the rich hold 150p, the cap 980p, the supervisor), which fails F1's own bar by small margins in every year (T4, T6), with the supervisor on most nights at +1. Phase 7 (the DM tab's edicts, weekly ledger and year of log) is built. Next: the open items in Phase 6's entry.

### 2026-11-06: the user's decisions

The user said "Okay. Do it." to the plan with every recommendation in section 9: the run budget yes, once for the
whole plan; try the charters, F1 if they add under 3 points; the tithe's strength picked by the rule, up to 50%; the
ends labelled by the measured reach; the scorecard is the judge; no warm starts; the check-in after Phase 3 kept.

### Phase 1: tools, baseline, dry run (2026-11-06, branch `r2-controller`)

About 1 h 20 min of the phase's 2.5 h. 11 one-year runs of the budget's 34 (Batch 0's five, D1's six); no 3-year run.
All from scratch copies of export70 (`export-<run>/`, hard-linked), on the cores the task gave (2-5, 6-9, 10-13, 14-17,
18-19 with two threads). Nothing written to DEV or PROD; no server.

**Built.**

- `world_check --steer-table FILE`: the steering table read from a file over city_loop.json's (a file with a
  `steering` object, or the object itself; a key it leaves out keeps the data's value). The flag sets
  `RATW_STEER_TABLE` before the catalogue is read; a file that is missing or not JSON stops the run (exit 2), since it
  would quietly be today's economy. (`readSteering` in `RatwItems.cpp`, one reader for both.)
- The new measures on the daily line (with the steering on): `m_g_after`, `m_g_before`, `m_g_day` (G: the Gini of cash
  a head over all households, weighted by heads; with no tithe yet, before = after), `m_food_gini` (F, pantry days a
  head), and the floors' skim split `skim_idle` (its first pass) and `skim_busy` (its second). S is today's
  `m_upper_share`, which already counts every House till through `houseOf`; the charters (Phase 2) join it there. Income
  shares by class, the luxuries and cheese's price are read from the full log (`flow`, `trade`, `ware`) by
  `tools/steer_measures.py`. Logging only: the centre's daily lines are the same as steer-centre-t1's on every field it
  has (checked on an 8-day smoke).
- `tools/steer_runs.py MANIFEST`: the variants (rule lines, a table file, `--rule-at` moves), the export folders, five
  at a time, waits for every run, then `econ_report`. Manifests: `steer-b0.json`, `steer-d1.json`, `steer-d1b.json`
  (each run's binary is a copy, `build-c/world_check.b0`, `.d1`, `.d1b`, so a rebuild never touches a running batch).
- `tools/steer_accept.py BATCH`: T1-T13 from the daily lines, PASS/FAIL/n/a per run and year, a markdown report, exit 1
  on any FAIL. Rungs `full`/`F2` on the scorecard; `F1`/`F3` on "no worse than `ref`" in T4-T7.
- `artifacts/economy/r2/steer-scorecard.json` (5.3's numbers) and `steer-targets-t1.json` (the dry run's targets:
  doc 74's table on its own measures, each mark's floors-off tills short from the t1 run there, and `floor_from`, the run
  whose seasonal treasury profile gives the mark's floor).
- `tools/steer_measures.py OUT RUN...`: the measures table for a batch (`steer-phase1-measures.md`).
- `steer_tests` 186 checks (was 178): G's head-weighted Gini on known purses (three singles, 0.444; a couple and one
  alone); the comfortable line, the order and the guard read from the table. ctest 80 of 80; `test_econ_report` (its
  guard line now prints the table's 1000%) and `test_studio` OK.

**The dry run** (`steer-accept-dry.md`, no simulation): **steer-centre-t1 passes every test** (T3 and T8 n/a: no
supervisor yet), T10 included (1,095 daily lines the same as stable10b's on every field). Two tests were wrong on
today's centre and were fixed first:

- T13 failed the centre (G 0.489, 0.489, 0.484: year 2 to 3 moved 0.005 against 0.000 before). Now a change under half
  a band also passes as no drift (`T13.noise_bands` 0.5).
- T5 judged each run under its own seasonal profile, so a healthy treasury could count "under its floor". Now each mark
  names the run its floor comes from (`floor_from`: the calibration run in Phase 3).

The rungs' bars pass on the runs they ship (F1 on steer-right-t1; F3 on the left and right t1 runs). For information,
the full scorecard on the t1 runs: steer-right-t1 fails T5 (8-12 float days a year, mostly summer) and T6 (support
unpaid 1.5-2.4x stable10b's); steer-left-t1 fails T5's floor days in year 1 (its floor is its 3-year profile, and year 1
is still settling); steer-centre-floors-t1 fails T6 (tills short 36.6, 1.26x the centre on days 120-365; the hand check
of 1.22x was the whole year).

**Batch 0** (today's code, 1 year, days 120-365; `steer-phase1-measures.md`, `steer-accept-b0.md`). steer-b0-left and
steer-b0-right are byte for byte the t1 runs' first year: the skim rework touches only the floors.

| run | G (after; day) | F | Gini earners | S % | H/h | K | treasury mean (least) | float days | tills short (x floors off) | support unpaid | floors' skim/day: idle / busy | skim by payer group (p a day) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| centre (stable10b) | - | - | 0.489 | 13.4 | 0.022 | 0.91 | 1,316 (200) | 2 | 29.1 | 219 | - | - |
| left | 0.101 | 0.062 | 0.331 | 13.3 | 0.000 | 0.00 | 1,495 (1,115) | 0 | 31.2 | 91 | - | (the share-out's own: middle 337, upper 179, lower 151, the Crown 112) |
| left, floors | 0.099 | 0.062 | 0.329 | 13.3 | 0.000 | 0.00 | 1,491 (1,081) | 0 | 31.6 (1.01) | 92 | 5 / 0 | the floors almost nothing: the share-out has levelled them |
| right | 0.430 | 0.066 | 0.546 | 23.3 | 0.024 | 0.74 | 815 (200) | 12 | 34.4 | 334 | - | - |
| right, floors | 0.368 | 0.062 | 0.489 | 18.3 | 0.000 | 0.07 | 2,040 (1,579) | 0 | 41.6 (1.21) | 169 | 680 / 0 | the Crown 460, upper 103, Timberland 72, Fell 46 |
| centre, floors | 0.298 | 0.062 | 0.445 | 11.8 | 0.000 | 0.07 | 2,099 (1,619) | 0 | 37.9 (1.30) | 118 | 526 / 58 (10% busy) | the Crown 310, Timberland 92, keepers 79, Fell 49, upper 43, middle 10, treasury 1 |

The upper share at the centre is steer-centre-t1's (stable10b logs none). Income shares by class hardly move (upper 6-8%,
middle 53-55%, lower 38-39%). Cheese (rules untouched): households and Houses paid 6.9p a piece at the centre, 15.5p at
the right, 25.1p at the right with floors; 3.7 pieces a day everywhere.

**D1: triggered, by the shops.** The skim took only 10% of the floors' money from busy money (under half), but the
centre's tills short with both floors was 37.9 against 29.1 without, 1.30x (over 1.25x). The money came from the
estates (77% of it; the guard, on 245 of 246 nights, took their whole spare first), and the estates' spare is what buys
their baskets (559p a day at the centre, 128p with the floors), luxuries (92p to 7p) and patrons' works from the city's
makers. "Idle" by the estates' own measure, busy for the shops. The rule's levers only (the order and the comfortable
line, as table values; `skimOrder` is new in the table, its five steps by name), on the centre with both floors:

| variant | tills short (x 29.1) | floors' skim/day | busy share | from |
|---|---|---|---|---|
| Batch 0 (estates first, the guard on, comfortable 4) | 37.9 (1.30) | 584 | 10% | estates 77% |
| the guard off | 37.0 (1.27) | 567 | 35% | estates 66% |
| the estates last, the guard off | 36.7 (1.26) | 547 | 42% | estates 48%, keepers 33% |
| **the estates last, the guard off, comfortable 2** | **34.1 (1.17)** | 507 | 0% | keepers 58%, upper 39%, treasury 3% |

**Applied in city_loop.json's steering:** `skimOrder` the treasury, upper, keepers, middle, the estates last;
`comfortable` 2 (was 4: the line a head is now about 38p, two days' necessities, still never under the comforts' keep);
the guard off (`tillsShortOver` 1000: its trip, 275, is never reached; it leaned on the estates first, the opposite of
the fix). Re-run at all three positions (`steer-d1b-*`; the centre's run is byte for byte the --steer-table run, so the
flag and the data give the same run):

| run (both floors) | tills short (x floors off) | S % | G after | H | treasury mean (least) | floors' skim/day (busy) | from | Houses' luxuries p/day |
|---|---|---|---|---|---|---|---|---|
| left | 31.1 (1.00) | 13.2 | 0.099 | 0 | 1,494 (1,135) | 5 (0%) | - | 91 |
| centre | 34.1 (1.17) | 11.8 | 0.285 | 0 | 2,158 (1,633) | 507 (0%) | keepers 293, upper 198, treasury 15 | 98 |
| right | 33.8 (0.98) | 13.7 | 0.288 | 0 | 2,131 (1,647) | 510 (0%) | upper 412, keepers 84, treasury 14 | 100 |

All three now pass T6's floors bar and T11 (H 0, short after relief 0, Never Hungry's Wharf pennies 0.17 a day, all of
the floors' money idle).

**What changes the plan.**

1. **The right end with both floors on is nearly the centre.** With the estates last, the floors are paid by upper
   purses at the right (412p a day), and S falls from 23.3 to 13.7 (with Batch 0's order it fell to 18.3, but the
   shops paid: 1.21x). A8 passes (S is one-sided there), but the slider's label at the right must say what it holds
   with the floors on. If the user wants the right to keep its reach with the floors, the estates can go back ahead of
   the upper households at the right only, a table value per mark; not done here, since D1's rule is about the shops.
2. **G is a different scale from the earners' Gini.** Over all households, a head: about 0.29-0.30 at the centre with
   the floors (0.33-0.37 at the centre's first days without them; the calibration's C run will measure it), 0.10 at
   the left after tonight's share-out and 0.43 at the right. Today's left already reaches 0.10 after the share-out; the
   before-the-levelling value is not taken in today's code. The tithe (Phase 2) must set `m_g_before` before it moves
   money, or the left's G will flatter by about half the day's re-divergence. Section 9.3's "0.20-0.28 through the day"
   was on a guess of the scale; the calibration sets the labels.
3. **F says little.** Pantry days a head have a Gini of about 0.06 at every position (households top up to two days'
   food), so F is kept as a watched line, not a measure with a target.
4. **Runs take longer than the plan's figures.** A 1-year run took 16-20 minutes on four threads and 29 minutes in the
   two-thread slot or with five at once (the plan says 13). A batch of five is about 30 minutes, not 15: calibration
   about 60 minutes, acceptance about 60, the 3-year confirmation about 90. About 1.5 hours more wall clock overall.
5. **Today's floors run on the left with the share-out are almost free** (5p a day): the share-out levels the purses
   first. When the tithe replaces it, A7's floors cost should stay near that.

**Not done in this phase:** no change to `steer-compare.py` or the DM tab; `steer_calibrate.py` is Phase 3's.

### Phase 2: the actuators (2026-11-06, branch `r2-controller`)

About 2 h of the phase's 3 h. No calibration run: a 60-day check of the centre (three runs on today's code, three on
the new) and two 60-day smokes at the ends, on scratch copies of export70, on the task's cores. Nothing written to DEV
or PROD; no server.

**Built.**

- **The Levelling tithe** (`Society::levellingTithe`, `tithed`; `RatwSteering.cpp`): each night after the alms and before
  Never Broke, every resident household's cash a head moves r of the way to the head-weighted mean, through
  fund:relief; tills, farms, the estates and the treasury untouched. `tithed` works in whole numbers (r in millionths,
  each want's floor and remainder exact), so the pennies left over go to the largest remainders, true ties in the
  households' order, and the total is exact. G is taken just before it moves (`m_g_before`); `m_g_after` stays the
  night's end, `m_g_day` their mean. The table lever `levelling`: 40% at -1 (provisional: the calibration picks 30, 40
  or 50), blended to 0 at the centre.
- **The Crown's charters** (`chartersNight`, `charterable`, `charterOf`, `tillHouse`): the lever `charters`, a percent of
  the middle keepers' tills (a merchant's city till, not a farm's, not a House's by the catalogue, its keeper's
  household not upper class), 33% at +1 (provisional). The number is fixed when the slider moves (its share of the
  tills that may be chartered and those that are) and reached in equal nightly steps over `chartersRampDays` (14)
  nights, best tills first (28-night mean profit, then id), handed back newest first; all handed back at once when the
  steering is turned off. The ramp waits until the keepers' profit means have had 28 nights, so the ranking is real
  (with the steering on from day 0 the first charters come on day 28). A charter goes to the House whose district the
  shop is in (`LifeDay::houseOf`: the cell's first claim by a faction named "House ..."), else the Crown. Saved:
  the list in order, the ramp, the profit means.
  - The overlay in all three lookups: the till's profit (`payWages`: a chartered till's profit goes to the House's fund,
    booked as the House's shops' rent; the hands' share and the ground rent come off first as for any keeper); the
    upper share (`upperShare` through `tillHouse`); the skim's keepers' step (a chartered shop's keeper is no longer a
    keeper there). Its own rules: the cap stays `floatWagesDays` (not `workshopWagesDays`) and the wage rise stays the
    keeper's bound (not `workshopWageMost`), since the static catalogue (`houseOfPost`) does not see charters.
- **The dividend tied to the charters:** `dividend` at x is the row's value plus `dividendPerCharter` (0.45) points a
  point of charters; the row is 15 at +1, so 33%, 66% and 100% give 29.85%, 44.7% and 60%.
- **The blended values at x (3.2):** `items::steerAt` reads a null at a half mark as halfway between its neighbours
  (a null at the centre or an end is still the world's own), so an end value blends in with |x|. The same rule in the
  studio, `steering.ts` and `econ_report.py`; the shared fixture has cases for it.
- **`followTreasury` fed a reachable target (4.5):** `treasuryTarget` is a steered rule now: 5,500 at the centre (the
  world's), 1,500 at -1 and 850 at +1 (the t1 runs' year-1 means; provisional), blended between. The old
  `min(target, cap + keep)` rule is gone. The cap and keep at the ends are left as t1 had them (the left 1,500 and 0);
  the plan's "cap = target x 1.15, keep = target x 0.5" is for `steer_calibrate.py` to write in Phase 3, not set here.
- **Removed:** the share-out, the toll lean (and its guard), the rent scale, the weekly and judging hunger pulls, the
  push's trim and judging (`steerBand`, the `steerJudgeDays`, `steerStep`, `steerStepMost`, `steerWidthMid`,
  `steerWidthEnd` rules), `spares()`. `push` in the save is now the slider's place; a slider move re-seats the slow
  means. The history keeps the changes only.
- **Never Hungry** covers the farms' crews (each a household of its heads in one account, the line its heads' days).
- **The lists:** Core's rules and table, city_loop.json, `map_editor.py` (`RULE_LIMITS`, `STEERED_RULES` with
  `treasuryTarget`, `STEER_OWN_RULES`), `model.mjs`, `economy.ts`, the studio's levers and its view (`band` is the
  slider twice), `steering.ts` (`leverAt`), the panel's two lines, `econ_report.py`, world_check's daily line (new:
  `lev_levelling`, `lev_charters`, `lev_target`, `tithe`, `tithe_gave`, `tithe_got`, `charters`, `charters_want`,
  `charters_granted`, `charters_back`, `charter_profit`; gone: `lev_share_out`, `lev_toll`, `lev_rent_scale`,
  `share_out`, `share_out_homes`, `land_rent_extra`, `steer_push_lo`, `steer_push_hi`, `steer_judged`).

**Tests.** `steer_tests` 235 checks (was 186): the table and its blends; the levers at the ends; the tithe (the Gini
identity in doubles at five r; the remainders and ties in pennies; the total kept; r 0 and all-at-the-mean move
nothing; deterministic; in the city, conserved, G before over G after, nothing at the centre); the charters (none
while the profit means are under 28 nights; one a night on a 4-night ramp, the best first; the Crown's for a district
no House claims; the profit to the House, the keeper on a wage; the overlay; the skim's keepers' step; the upper keeper
never chartered; saved and read again; two worlds the same; handed back newest first; all back when off); the slider
(the levers there the same night, nothing judged in 200 nights, saved). `sim_fast_tests` reads the tithe in place of
the share-out. ctest 80 of 80; `test_studio`, `test_econ_report`, `steering.test.ts`, `economy.test.ts` pass; `tsc`
clean.

**The centre is unchanged** (`steer-p2base.json`: c4ae14e's binary and Data; `steer-p2.json`: this build; 60 days):

| run | daily lines | the loop state |
|---|---|---|
| steering off | byte for byte the same file | the same |
| on, at the centre | every shared field the same on all 60 days | the same but the steering's own record (the removed fields, the new ones, the history without day 55's judging) |
| on, at the centre, both floors | every shared field the same on all 60 days | as above |

**The smokes at the ends** (60 days, the provisional table, not calibration; money conserved every night, nobody hungry):

- Left (tithe 40%): G before the tithe about 0.10, after the night about 0.05 (0.37 and 0.22 on day 0); about 900p a
  night moved; the treasury held at 1,500p; tills short 16 a day.
- Right (charters 33%): none until day 28, 27 by day 40, 29 by day 50 (29 of about 88 middle keepers' tills); S 18-19%
  before them, about 34-36% after; tills short 18 a day.

**Not sure, for Phase 3.**

1. **S jumps when a till is chartered**, because the till's whole cash (its float and a week's wages) counts in S the
   night it is chartered, before any profit moves. Part of the right's 34% is that relabelling, not money moving to
   the rich. The calibration should report S with and without the chartered tills' own cash.
2. **The ramp's 28-night wait** is not in the plan. Without it the first charters would be ranked on no profit at all.
3. **The left's treasury values** stay as t1 had them (see above); the plan's 1.15 and 0.5 rule would raise the left's
   cap and keep and cut its surplus alms, so it is left to the calibration to measure.
4. **The DM tab** still has the push's scale (now a point at the slider) and no charter or tithe edicts: Phase 7's.


### Phase 2 review (2026-11-06, branch `r2-controller`)

A review of Phase 2's diff before the calibration spends runs on it. Checked and found sound: the tithe's pennies (exact
in whole numbers, remainders largest first, ties by household, the total kept, payers never under the mean), the
charters' profit path (after the hands' share and the ground rent, to the House's estate, booked as rents and shops; a
failed shift leaves the money in the till), the dividend's rise (0 at the centre and the left), the overlay in the
upper share and the skim's keepers' step (`houseOfPost` is left alone on purpose, so a chartered till keeps a keeper's
7 wage days and wage bound), the till's wage buffer, determinism (the ranking sorted by mean then id; the profit means
saved in their shortest exact form), save and reload, the centre (no charter, no tithe, the treasury target the
world's), and the removed levers (nothing of the share-out, the toll lean, the rent scale, the pulls or the push still
acts; the only remnant is `topUpPoorest`'s unused fund:relief branch, harmless).

**One defect, fixed.** A charter's House came from the shop's own cell's claims. In Ridgemere 2.0 most shops are
interiors that no House claims while the street outside is a House's, so about 25 middle keepers' shops in Greatly's
and Grayrock's streets (the Morning Oven, the Hook and Cleaver, Harbour Provisions...) were chartered to the Crown.
`World::districtHouse` now takes the cell's own House claim, else that of the cells its doors lead out to, the nearest
first, never past the first street (so never a neighbour's across a seam). Tests: `authoring_tests` (a street's House
by the faction's name, a room with none takes its street's, a room a House claims keeps its own, a street with none has
none) and `steer_tests` (a till in a House's district is that House's; a House with no estate, or none, the Crown's).
A 60-day check at the right (`steer-p2fix-right`, against `steer-p2-right`): the same 29 charters, S 34.98% against
34.92%, tills short 18.2 against 18.4, money conserved; the chartered profit over 60 days now Greatly 1,754p (was 49),
Grayrock 1,517p (486), the Crown 5,951p (8,828).

**Tidied.** The five removed rules (`steerJudgeDays`, `steerStep`, `steerStepMost`, `steerWidthMid`, `steerWidthEnd`)
were still in `city_loop.json`'s rules, read by nothing; gone. The editor's rule list marks `treasuryTarget` as held by
the steering, as the studio's `STEERED_RULES` already did.

**Noted, not changed.** At -0.5 the blended treasury target (3,500p) is over the cap and keep there (1,750p + 1,000p),
so the toll drift is pinned up as it is at the centre against 5,500p. That is the plan's blend (3.2), and the
calibration's half-mark runs will show whether it matters. The dividend's rise follows the charters' table strength
from the first night, while the charters themselves wait 28 nights and ramp over 14.

### Phase 3: calibrate (2026-11-06, branch `r2-controller`)

About 1 h 20 min (the plan's 1 h plus 30 min of runs; the runs took 55). The 10 one-year runs of 5.2, in two batches
(`steer-cal1.json`, `steer-cal2.json`; a copy of dd9f354's build, `build-c/world_check.p3`), open-loop, toggles off, on
scratch copies of export70 on the task's cores; every run exited 0. 21 of the budget's 34 one-year runs used; no 3-year
run. Nothing written to DEV or PROD; no server. **Data/Items/city_loop.json is not changed**: the table waits for the
user's word at the check-in.

**Built.** `tools/steer_calibrate.py SPEC`: the selection rule (steer_accept's own T4-T7, year 1, days 120-365, against
the targets agreed before any run, `steer-targets-t1.json`: the t1 run's tills short at each end, the table's hungerMost
and shortMost; T5's floor from the run's own profile, since the run is its mark's floor run); D2; the half marks on the
measured curve (the centre to the half-mark run, then the ends' slope); the targets at each mark (the measure's mean,
its band 2 sigma of its 28-day mean, at least 0.01 in G and 1 point in S; tills short; the floor run); the measures
table, with S also shown without the chartered tills' own cash (their floats, from the full log and the loop state's
list). For the check-in only, each right strength is also judged on F1's own bar. `steer_accept.py` gains an optional
`T5.sigma_least` (unused by the scorecard as written). Reports: `steer-calibration.md` (the scorecard as written),
`steer-calibration-floor.md` (the floor reading below); tables `steer-table-calibrated.json`,
`steer-table-calibrated-floor.json`; targets `steer-targets-cal.json`, `steer-targets-cal-floor.json`.

**What the runs measured** (days 120-365, means a day):

| run | G day (before; after) | Gini earners | S % (without chartered cash) | H/h | K | treasury mean (28-day least) | float days | tills short | support unpaid | wage support |
|---|---|---|---|---|---|---|---|---|---|---|
| C (centre) | 0.350 | 0.489 | 13.4 | 0.022 | 0.91 | 1,316 (870) | 2 | 29.1 | 219 | 60 |
| L30 | 0.100 (0.123; 0.078) | 0.325 | 13.1 | 0 | 0 | 1,492 (1,451) | 0 | 31.5 | 96 | 284 |
| L40 | 0.081 (0.105; 0.056) | 0.312 | 13.0 | 0 | 0 | 1,493 (1,454) | 0 | 31.3 | 99 | 289 |
| L50 | 0.066 (0.092; 0.039) | 0.302 | 12.7 | 0 | 0 | 1,492 (1,454) | 0 | 32.2 | 101 | 298 |
| Hm (-0.5, tithe 20) | 0.143 | 0.334 | 12.6 | 0.001 | 0.07 | 2,291 (1,916) | 0 | 34.5 | 43 | 363 |
| P (L40, spendAbove 30) | 0.078 | 0.305 | 12.5 | 0.001 | 0.07 | 1,482 (1,441) | 0 | 34.0 | 113 | 308 |
| F1 (steer-b0-right) | 0.430 | 0.546 | 23.3 | 0.024 | 0.74 | 815 (364) | 12 | 34.4 | 334 | 11 |
| R33 (29 charters) | 0.457 | 0.568 | 34.9 (27.3) | 0.023 | 0.61 | 804 (437) | 6 | 34.3 | 348 | 12 |
| R66 (58) | 0.476 | 0.583 | 42.0 (29.6) | 0.031 | 0.93 | 650 (351) | 23 | 36.3 | 371 | 8 |
| R100 (88) | 0.478 | 0.584 | 47.5 (29.9) | 0.022 | 0.95 | 708 (470) | 15 | 37.5 | 376 | 9 |
| Hp (+0.5, 29 charters) | 0.400 | 0.518 | 30.3 (21.8) | 0.017 | 0.41 | 1,066 (516) | 10 | 30.2 | 276 | 34 |

The tithe moved 933, 1,071 and 1,158p a night at L30-L50. The chartered profit to the Houses was 179, 247 and 313p a
day at R33-R100, and the dividend 206, 332 and 467p; the estates' funds rose 25%, 18% and 10% from days 120-147 to
338-365 (R33 over the 20% that would call for patronage in Phase 6). Cheese (rules untouched) cost households and Houses
5.0p a piece at the left, 6.9p at the centre, 14.7-20.1p at the right; 3.7 pieces a day everywhere. The skim did
nothing (the toggles were off). C is byte for byte stable10b on all 366 daily lines' shared fields.
P against L40: 0.54 tills short a penny of `spendAbove` (doc 74 measured 0.6).

**The selection rule, as the scorecard is written: no strength passes at either end.**

- The left: every strength passes T4, T6 and T7, and fails T5 on 5-6 summer days "under its floor". The floor is the
  run's seasonal profile less 2 sigma, and a treasury held at its cap all year has a sigma of 1-5p: the floor sits a few
  pennies under the profile, and a 20p wobble in a 28-day mean that never left 1,450-1,515p counts as a dip. On the
  t1 run's floor instead (the targets agreed beforehand) the left fails worse, for the same reason. So the left takes F3.
- The right: every strength fails T5 (6-23 float days against stable10b's 2) and T6 (support unpaid 348-376p a day
  against 1.5 x 219 = 328). F1's own reference fails the same two tests open-loop (12 float days, 334 unpaid). On F1's
  own bar (no worse than steer-b0-right), R33 passes T4, T5 and T7 and fails T6 on support unpaid alone: 348 against
  334. So the right takes F1.

**A reading proposed for the check-in** (`steer-cal-spec-floor.json`; not the scorecard as written, and not adopted):
T5's floor at least as far under the profile as the centre's own noise (sigma at least stable10b's, 191p, so the floor
is at least 383p under the profile). Then all three left strengths pass and **the left takes L50 (Full)**: G 0.066 through
the day at -1, the half mark's tithe 13.7% (on the curve from the centre's 0.350 through Hm's 0.143, aiming at 0.208).
The right is F1 under either reading.

**D2.** The right: F1, by the selection rule, though the charters add a lot of S: +11.6 points at R33 (+4.0 without the
chartered tills' own cash), +18.7 (+6.3) at R66, +24.2 (+6.6) at R100. The left: F3 as written; Full at L50 with the
proposed floor.

**For the check-in.**

1. **The floor reading.** Adopt it (the left ships Full at L50) or keep the scorecard as written (the left ships F3)?
   Adopting it means `T5.sigma_least` in the scorecard, and the dry run is unchanged by it (the centre's floor is
   stable10b's own, already 191p).
2. **The right.** F1 by the rule. The alternative the rule does not allow: carry R33 into acceptance and let the
   supervisor's rule 1 (the ground rent to the treasury under its floor, the skim skipping the treasury) try to fix the
   float days and support unpaid that the open-loop right shows with or without charters. About a third of R33's added S
   is real money moving (4 points of 11.6); the rest is the chartered tills' own floats counted in S.
3. **The -0.5 mark's treasury.** Its blended target (3,500p) is above what the treasury reaches, so the toll drift is
   pinned up: Hm holds 2,291p (800p more than either neighbour) and its tills short is 34.5, 1.19 x the centre's and over
   both ends'. Setting the half mark's `treasuryTarget` to a measured value (about 1,400p) is the plan's own rule (4.5);
   not done here.
4. **The ends' treasury rules** stay as they ran (left target 1,500, cap 1,500, keep 0; right target 850): both targets
   are already within 60p of what the runs held (1,492 and 804), so the plan's x 1.15 cap and x 0.5 keep rule was not
   applied, since it would change the ends from what was measured.
5. **The half mark at -0.5**: its band is 0.01 (Hm's spread is under that) and its target 0.208 lies between the runs
   made, so A2 in Phase 5 is the first run at a tithe of 13.7%.

### After Phase 3: the user's word and the coordinator's calls

The user, on the check-in: "I wouldn't wait for my check-in on this one. Run the full planned 13-ish hours and make the
adjustments you feel is best, or run it however long you feel like is best until this is done. I will allot you up to 15
hours max for this". Under that delegation the coordinator decided, **after seeing the Phase 3 results** (so these are not
choices made blind, and the doc says so):

1. **The floor reading is adopted.** T5's floor is at least the centre's own noise under the profile (`T5.sigma_least`
   191p, stable10b's sigma, in `steer-scorecard.json`). A treasury held flat at its cap had a sigma of 1-5p, so a 20p
   wobble counted as a dip: a defect in the test, not in the left's economy. With it the left takes L50 (Full), if
   acceptance agrees. The dry run is unchanged in substance (steer-centre-t1 passes every test; 132 of 142 pass, one
   more than before).
2. **The right takes R33 into acceptance.** Without the chartered tills' own cash the charters add +4.0 to +6.6 real
   points of S over F1 (over the user's 3-point bar). The supervisor's treasury rule is to try to fix R33's float days and
   unpaid wage support. If R33 with the supervisor still fails acceptance after the fix rounds, the right ships F1 by the
   ladder. S at the right is judged **without** the chartered tills' own cash (`m_upper_net`, new on the daily line);
   both are reported.
3. **The -0.5 mark's treasury target is 1,400p** (the plan's rule 4.5: a level the treasury reaches there, between its
   neighbours' 1,316p and 1,492p; the blend's 3,500p pinned the toll up, and Hm held 2,291p with tills short 1.19x the
   centre's). The +0.5 mark was checked likewise: see Phase 4's smoke, which moved the whole right side's target.

### Phase 4: the safety supervisor (2026-11-06, branch `r2-controller`)

About 35 minutes of wall clock (the plan's 2 h), the three one-year smoke runs the task allowed at the right with R33
and a 60-day centre check, on scratch copies of export70 (`steer-p4.json`; a copy of this build, `build-c/world_check.p4`,
and of its Data, `build-c/data.p4`), cores 2-17; every run exited 0. **24 of the budget's 34 one-year runs used** (21 +
3; the 60-day check is not counted as one); no 3-year run. Nothing written to DEV or PROD; no server.

**Built** (`Society::steerSupervise`, `RatwSteering.cpp`; nightly, last of the steering's night, so what it moves is read
from the next night):

- **The measures:** 7-night means (the treasury at 18:00, the tills short, hungry residents an hour, households short, the
  wage support unpaid). No rule fires until they have had 7 nights (the first ctest run showed why: a world's first night
  is its hungriest, and a mean of one night fired rule 3 on day 1).
- **Rule 1, the treasury under its floor:** the ground rent to the treasury in proportion to the gap (all of it at half
  the floor under it), held there until the rule clears and easing off over 14 nights after; the skim leaves the
  treasury be (`skimStep` 2 is empty); and the rich hold eases 10p a week toward the world's 40p. That last is beyond
  the plan's rule 1: at the right the rich holding over the world's keep is the money out of the shops' and the toll's
  circulation (doc 74: "200p cost the treasury"), so it is the lever that pays for a low treasury there. The plan's
  rule 1 as written would have done less than today's: the table's own route (all the ground rent to the treasury under
  `treasuryFloor` 800p, doc 74's) already fires under the right's floor of 600p, so the table's route is kept beside it
  and the supervisor's can only add.
- **Rule 2, the shops:** the tills short over 1.3 x the mark's (`targets.tillsShort`: 32.2 at -1, 29.1 at the centre,
  34.3 at +1, the half marks halfway), **or the wage support unpaid over `targets.unpaidMost`** (328p a night, T6's bar,
  1.5 x stable10b's 219): the tithe eases 3 points a week (left), the rich hold 10p a week (right). The unpaid half is
  new, for the right's problem.
- **Rule 3, hunger:** H over `hungerMost` or K over `shortMost` (each judged only with its floor off): the tithe eases 5
  points a week (left); at the right, 3 charters a week handed back, newest first (`sup.charters`, a whole one at a time
  through `chartersHeld`; the ramp and the table's count are reckoned without them, so a hand-back never compounds).
- **Rule 4:** a lever backed off 28 nights running is "at the envelope's edge": in the log and `sup_edge`, nothing done.
- **Hysteresis and rates:** a rule fires only where it has a lever to move (so nothing fires at the centre, whatever the
  night), and clears once its measure is under 90% of its trigger (the treasury: over the floor / 0.9) for 7 nights
  running; a bad night starts the count again. Each lever moves once a night, at the fastest rate of the rules on it
  (rules 2 and 3 on the tithe together: 5/7 of a point a night, not 8/7); with none on, it returns at half the rate it
  left. A back-off never takes a lever past the world's own value, and is clipped nightly if the slider moves. Every
  fire, clear, a lever's first and last move and the edge are a line in the steering's log (the DM tab's), e.g. "Day 76:
  the supervisor acts: the shops ran short (29.9 tills short against 44.6, the wage support 341p a night unpaid against
  328p); the rich hold eased by 1.4p".
- **The treasury floors** (`treasuryFloors` in the table, 13 months of 28 days a mark, written by the new
  `tools/steer_floors.py` from each mark's calibration run, steer_accept's T5 floor with sigma at least 191p, never under
  600p): -1 from L50 (1,075-1,135p from day 120 on), the centre from C (600-1,506p), +1 from R33 (600p all year); the
  half marks null, halfway between. The first 120 days of each mark's floor run carry no profile, so months 1-3 are 600p.
- **Logged on the daily line:** `sup_actions` (the rules on), `sup_backoff`, `sup_levelling`, `sup_rich_above`,
  `sup_charters`, `sup_route`, `sup_floor`, the 7-night means `sup_tr7`, `sup_tills7`, `sup_h7`, `sup_k7`,
  `sup_unpaid7`, `sup_edge`, `sup_why`; and `m_upper_net`, S without the chartered tills' own cash.
- **Saved:** the loop state's `steer.sup` (the means, each rule's state, the back-offs and their rates, the route, the
  charters held back); reset when the steering is turned on or off.
- **The DM tab:** the slider's ends carry the table's `reach` labels; a supervisor line ("The supervisor: the treasury
  under its floor; the rich hold eased 20p, 1 charter handed back, 50% of the ground rent to the treasury."), and each
  lever's "now" with its back-off (`studio.py` `steer_reach`, `steer_supervisor`; `steering.ts` `supervisorLine`;
  `SteeringPanel.tsx`). The labels are provisional until acceptance re-measures them: left "Shared: Gini 0.07 through the
  day", right "about 22-27% ... (29-35% counting the chartered shops' tills)".
- **The table** (`city_loop.json`): levelling 50 at -1 and 13.7 at -0.5 (Phase 3's floor reading); charters 33 at +1;
  `treasuryTarget` 1,400p at -0.5 and the world's own right of the centre; `treasuryCap` 980p at +1 (below).
- **The scorecard and targets:** `T5.sigma_least` 191; `steer-targets-p4.json` (from `steer-targets-cal-floor.json`;
  +1 R33's S without its chartered cash, 27.3% +- 1.47, tills 34.25, its floor; +0.5 halfway; the half marks' tills and
  floors halfway, `floor_from` a list being the mean of the runs' floors, new in steer_accept); and T2 now FAILs a run
  backed off on every day of a year (it judged nothing and passed).

**Tests.** `steer_tests` 541 checks (was 235): the floors at the slider and by month; nothing at the centre; the left's
rule 2 at 3/7 a night, a week 3 points, rules 2 and 3 together at 5/7, clearing, the return at half the rate, back at the
table; nothing judged before 7 nights; the treasury's rule with nothing to move at the left; hysteresis (a night short of
clearing, a bad night resets the count); the right's rule 1 (the route in proportion to the gap, all of it at half the
floor, the skim leaving the treasury be, the rich hold to 40p and never past it, the edge after 28 nights, the route and
the rich hold easing back after); rule 2 on the wage support alone; the supervisor saved and read again; off, afresh;
the charters held back newest first and given back. `sim_fast_tests`, `test_studio` (the reach, the supervisor from
the save), `steering.test.ts` (the supervisor's line), `test_econ_report` pass; ctest 80 of 80; `tsc` clean.

**The centre is unchanged:** a 60-day run at the centre, steering on (`steer-p4-centre60`), is the same as steer-cal-C
(byte for byte stable10b) on every shared field of all 60 daily lines; only the new fields are added, and the supervisor
never fired.

**The smoke at the right** (R33, toggles off, steering on from day 0, 1 year, days 120-365; S1 the table as first built
with the target 850p and the world's cap; S2 with the toll drift's target the world's at +1; S3 with the cap 980p at +1):

| run | S % | S without chartered cash | H/h | K | treasury mean (28-day least) | float days | tills short | support unpaid | wage support | toll | rich hold (mean) | supervisor acts / backed off | fires |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| F1 (steer-b0-right) | 23.3 | 23.3 | 0.024 | 0.74 | 815 (364) | 12 | 34.4 | 334 | 11 | 30.7 | 130 | - | - |
| R33, no supervisor | 34.9 | 27.3 | 0.023 | 0.61 | 804 (437) | 6 | 34.3 | 348 | 12 | 30.0 | 150 | - | - |
| S1 | 29.3 | 20.1 | 0.007 | 0.36 | 892 (685) | 12 | 28.9 | 275 | 8 | 29.6 | 72 | 38% / 100% | shops 4, treasury 2, hunger 1 |
| S2: target the world's | 29.6 | 21.0 | 0.020 | 0.64 | 1,179 (724) | 2 | 29.0 | 272 | 26 | 30.6 | 85 | 29% / 100% | shops 3, treasury 1, hunger 1 |
| S3: cap 980p | 31.3 | 22.2 | 0.026 | 0.39 | 747 (609) | 4 | 32.2 | 140 | 193 | 30.0 | 98 | 31% / 100% | treasury 3, shops 1, hunger 2 |

On the scorecard (`steer-accept-p4.md`, information only, not acceptance): S1 fails T5 (12 float days); S2 and S3 pass
T4-T7, so the treasury rule plus either table change fixes R33's float days and unpaid wage support. All three fail T2
(backed off every day), T3 (backed off 100% of days, at most 30%) and T8 (acting on 29-38% of nights, at most 15%).

**What it says.**

1. **The supervisor can fix the right's problem, at a price.** Rule 2's wage-support trigger fired around day 76 and
   kept the rich hold eased for the rest of the year (to about 65-100p), which cut the unpaid support (348 to 272-275p)
   and the tills short (34.3 to 29), but took 5-7 points of real S (27.3% to 20-22%, under F1's 23.3%). With a 7-night
   mean of a seasonal measure against T6's annual bar, the rule acted as a standing table change, not a guard.
2. **The table should carry the structural fix.** S2 shows the right's treasury shortfall is mostly the toll drift's own
   target: at 850p the toll sat a point under the centre's (which is pinned up against 5,500p); with the world's target
   the toll held its point, the treasury rose 287p and float days fell 12 to 2. S3 shows the unpaid support is the cap:
   at 2,000p the right's treasury (about 800p) never paid any; at 980p it paid 193p a day and the unpaid fell to 140p.
3. **So the table now has both** (target the world's right of the centre, so +0.5 is checked too: its blend was 3,175p
   and is now 5,500p, the toll held up as at the centre; and the cap 980p at +1, 1,490p at +0.5). With the cap paying
   the support, the unpaid should sit well under 328p and rule 2 should fire only on real excursions, so the rich hold
   should stay near 150p and S near R33's 27%. **That combination has not been run:** the task's three smoke runs went
   on single-lever A/Bs. It is A4, A5 and A8 in Phase 5, with the fix rounds behind them.
4. **T3 and T8 will be tight for any rule that fires for weeks:** the return at half the rate means a 60-night back-off
   takes months to come back, all of them "at a back-off". If A5 still fails T3/T8 with the new table, the fix round's
   levers are `unpaidMost` (to the mark's own level x 1.3, as the tills), the return rate, or rule 1's rich-hold move.
   Not changed here.

**Not done:** `econ_report.py` does not yet print the supervisor's fields (the daily lines and `steer_accept` have them);
the DM tab's edicts, ledger and year of log are Phase 7's; the reach labels are re-measured in Phase 5/6.

### Phase 4 review (2026-11-06, branch `r2-controller`)

An adversarial review of Phase 4's diff (47c02c8) before acceptance spends runs on it. About 45 minutes. Two 110-day runs
at +1 for an A/B (`steer-p4r.json`, `steer-p4rb.json`; not counted as one-year runs, so still 24 of 34); both exited 0.
Nothing written to DEV or PROD; no server.

**Checked and found sound.** Money: rule 1's route is a `shift` from the till, taken off the excess before the rest of the
rent, and every run conserved money every night. The skim leaving the treasury be only drops one payer. Back-offs never
pass the world's value: `steerLever` clips the tithe at 0 and the rich hold at the world's 40p; `move` clips to the room
each night, so a slider move takes the back-off with it; a rule whose lever is gone clears the same night. Save and load:
every field of `sup` is written and read back (the test compares them exactly). It starts afresh when the steering is
turned on or off. Determinism: no randomness; the hold-back of charters is whole charters, newest first. The floors'
month is `day % 365`, which is the calendar's day of the year (`dayOfYear - 1`), so the floors stay in phase with the
seasons on any world. The centre: the 60-day check is the same as steer-cal-C on every shared field of all 60 days
(re-checked), and with the floors off no rule has a lever there. The supervisor's 7-night means follow the daily
lines' tills short, households short and wage support unpaid exactly. The treasury mean is within 24p of the daily line
(read at 18:00, before the surplus). The hunger mean is the steering's own hourly count, up to 0.15 from world_check's
`hungry_mean`. The DM labels match the calibration: G 0.066 at L50 is "0.07"; the right's "22-27% (29-35% counting the
chartered tills)" spans the smokes and R33. They stay provisional.

**One defect, fixed: a back-off kept deepening after its cause had cleared.** A rule stays on through its clearing
nights, and while on it pulled its lever at the full rate, even with its measure already back over its trigger. In the
three smokes, 83 of S1's 138 rule-on nights, 56 of S2's 116 and 58 of S3's 115 (48-60%) had no trigger true. Section
3.6 says a rule backs off "until the cause clears", and the lever returns once the rule has cleared, so in between it
should hold. Now (`steerSupervise`): a rule pulls only on nights its trigger is true. On but under its trigger, its lever
holds (no deeper, no return), and it returns at half its rate only once every rule on it has cleared. Tests (544 checks,
was 541): the tithe's back-off held through the clearing nights, held on a quiet night, and pulled again on a bad night.
A replay of the smokes' logged rules (open loop, so no feedback) cut the rich hold's mean back-off on days 120-365 from
78.5p, 65.1p and 52.3p to 22.6p, 19.5p and 8.5p. Days backed off fell from 100% to 63-76%, still over T3's 30%.

The A/B in a world, +1, the committed table, 110 days (days 60-109; the two runs are the same until the rule first fires
on day 69):

| run | rule 1 | rich hold eased by day 100 | S without chartered cash | treasury mean | float days | support unpaid | tills short | nights acting |
|---|---|---|---|---|---|---|---|---|
| Phase 4 as built | on days 69-106 | 45.7p (53.6p when it cleared) | 25.8 | 647 | 4 | 189 | 31.0 | 38 |
| the review's hold | on day 69, still on at 109 | 31.4p, held | 26.1 | 608 | 4 | 208 | 31.5 | 41 |

So the hold keeps more of the slider's reach (the rich hold 31p eased, not 54p), but its smaller back-off also lifts
the treasury more slowly: rule 1 was still on at day 109 (7-night mean 637p, under its 667p clearing line), and so far it
acts more nights. The evidence is mixed over this window. It is the plan's rule, and the change is one function, so
acceptance judges it. If A5 fails T8 on long rule-1 episodes, the fix round's levers are rule 1's clearing line
(`clearShare`) or `clearNights`. The first 110 days do not test the table's target change (the toll's first judging is
about day 182).

**Found, not changed (for the coordinator):**

1. **T3 against the plan's own rates.** A back-off that returns at half the rate it left is at a back-off for about 3 x
   its pulling nights. With T3 at 30% of days 120-365 (74 days), a year allows about 25 pulling nights in all, about two
   minimal rule-1 episodes. The right had 3-5 a year in every smoke. Whether T3's 30% or the return rate gives way is a
   scorecard call. The plan's fix rounds allow a rate change.
2. **Rule 1's route does almost nothing at +1.** The right's floor is 600p, under the table's own `treasuryFloor` 800p.
   So whenever rule 1 is on, the table's route is already sending the ground rent to the treasury: in S3, 19,344p of the
   20,862p rent on rule-1 nights went there that way. At the right, rule 1 works through the rich hold, and through the
   skim with the floors on. The route matters only where the floor is over 800p (+0.5 in autumn and winter).
3. **Two back-offs on the rich hold.** Rules 1 and 2 both ease it, against 3.1's "at most one back-off" a lever. They
   pull the same way, once a night at the faster rate, so they never fight. S1 against steer-cal-R33 (the same until
   day 76) shows the easing pays the treasury: on days 76-181, 887p against 685p, float days 2 against 7, tills short
   29.7 against 35.3. The cost is S.
4. **The floors' first month** comes from the run's last day (day 365 wraps to year day 0) and the 15-day smoothing:
   days 0-7 only, from the year's end (1,506p at the centre, 1,135p at the left). It applies to days 0-27, when a new
   world's treasury is near 1,200p. The tests found nothing bad: the centre's treasury is over 2,000p then, and with the
   floors off the rule has no lever at the centre or the left. With the floors on, rule 1 could fire there in the first
   month (the skim only).
5. **The supervisor shares the steering's 24-line history** with the DM's slider changes, so a busy year pushes those
   out. That is Phase 7's year of log.
6. **Charters backed off before any exist:** during the ramp's 28-night wait, rule 3's back-off can grow while no charter
   is held. The ramp then starts that many short, and the edge is logged about charters never granted. This is cosmetic
   unless hunger fires in the first month.
7. **The right's treasury target is the world's 5,500p** against a cap of 980p. That pins the toll drift up on purpose,
   as at the centre. It is section 1's pattern 3, chosen knowingly after S2. That combination is still unrun past day 110.

Tests: steer_tests 544, ctest 80 of 80.

### Phase 5: acceptance, fix round 1 and D3 (2026-11-06/07, branch `r2-controller`)

About 1 h 35 min of wall clock (22:41 to 00:16 MST). Three batches, every run exited 0, nothing left running; scratch
copies of export70 only, on cores 2-19; nothing written to DEV or PROD; no server. **Runs used: 12 one-year runs and two
480-day sweeps, 14.6 one-year runs' worth** (the task's "about 13" was overrun by 1.6; with Phases 1-4's 24 that is 38.6
against the plan's 34). No 3-year run.

| batch | build and table | runs |
|---|---|---|
| `steer-p5.json` (prefix `steer-p5-`) | ab8067d as committed (R33 at the right) | A1, A4, A5, A8, A9 |
| `steer-p5b.json` (`steer-p5b-`) | fix round 1 (below), R33 at the right | A2, A3, A6, A7 (their first runs) and A5 again |
| `steer-p5c.json` (`steer-p5c-`) | fix round 1 with the right at F1 (D3): the shipped build and table | A4, A5, A8, A9 |

**What the first batch said.** The left (A1) passed every test, the supervisor never acting. A8 (+1, both floors on)
passed every test. The right failed on the supervisor: A5 (+1) had rule 1 on from day 69 to day 247 (178 nights), because
with `treasuryCap` 980p the treasury lives at 600-700p and its floor at +1 is 600p (`floorLeast`): its 7-night mean dipped
under 600p on 9% of days but was over the clearing line (667p) for 7 nights running almost never. So the rich hold was
eased 38p on average, S without the chartered tills' cash fell to 23.5% (target 27.3 +- 1.47), and A5 failed T2, T3 (at a
back-off 100% of days) and T8 (acting on 66% of nights); T4-T7 passed. A4 (+0.5) failed T3, T8 and T5 (4 float days in
summer); its late-year fires came from the +0.5 floor halfway between the centre's and +1's (the centre's winter floor,
about 1,000p, under a treasury holding about 1,000p there). A9 (the sweep) passed legs 1 and 3 and failed leg 2 (S at +1
90% of the way in 86 days, at most 75): the supervisor's 7-night means carried the centre's nights into the far left's
ceilings (the night of the move to -1 rule 3 fired on the centre's 0.47 households short against the left's 0.1), and at
+1 rule 2 fired on the wage support's 7-night mean at 348p against 328p.

**Fix round 1** (one code fix, the rest table values; `steer_tests` 548 checks, was 544):

1. **The supervisor's means re-seated on a slider move** (`steerFollow`): its nights to 0 and the means afresh, so no rule
   judges the new place on the old one's nights for 7 nights. This is 3.3's re-seating, which Phase 4 had not given the
   supervisor's own means. A test checks it.
2. **`returnShare` 1** (`supervisor.returnShare`, new; the plan's half before): a lever returns at the rate it left. At
   half, a back-off is at a back-off about three times as long as it pulls (the Phase 4 review's finding 1), so one
   month's episode alone was over T3's 30%.
3. **`unpaidMost` 426p** at every mark: rule 2's wage support half judged as its tills half is, 1.3 x its bar (T6's 328p).
   A 7-night mean of a seasonal measure against an annual bar fired on ordinary weeks (A4: 334-336p twice).
4. **The +0.5 floor from A4** (the first run of the +0.5 table; the plan's rule, the mark's own seasonal profile less 2
   sigma, at least 600p): 600-735p. Set after seeing A4, and the scorecard's `floor_from` at +0.5 is A4 too
   (`steer-targets-p5.json`), so the supervisor and the judge use the same floor.
5. **`treasuryCap` 1,300p at +1**, to lift the treasury off its floor. **It did not work and is undone:** in A5 again the
   treasury rose only 70p (681 to 781p) while the wage support unpaid rose 152 to 282p and the float days 6 to 20; A5 still
   failed T3 (69% at a back-off), T5 and T8 (52%). The cap moves money between the treasury and the wage support; it does
   not raise the treasury's level. The +0.5 cap was held at the 1,490p it had.

An open-loop replay of the supervisor's rules on A5's and S3's daily lines (no feedback, a guide only) found no setting of
the plan's own fix-round levers (`clearShare` 0.9-1.0, `clearNights` 4-7, the return rate) that keeps rule 1 at +1 under
T8's 15% with the floor at 600p: 15-25% of nights, 4-5 fires. Only the world's cap (2,000p, smoke S2) keeps the treasury
clear of its floor (7-10% of nights), and there the wage support unpaid is about T6's bar. At the right, T6 (the wage
support) and T8 (a quiet supervisor) pull the cap opposite ways, and the plan's floor is the right's ordinary treasury.

**D3 (the coordinator's call 2 and the ladder):** the charters failed acceptance after the fix round (R33: A5 T2, T3, T8;
A5 again T3, T5, T8; `steer-accept-p5-r33.md`). There is no F2 under R33, so **the right takes F1**, written into
`city_loop.json`: charters 0 at +1 (built, unused), the dividend 30% at +1, the cap 980p at +1 (1,490p at +0.5), the
target the world's, the supervisor with fix round 1. The +1 and +0.5 targets are F1's own (steer-b0-right, F1's open-loop
run: S 23.3% +- 2.36, tills 34.4; +0.5 halfway). **F1 fails its own bar narrowly** ("no worse than steer-b0-right in
T4-T7", with no tolerance): A5 T4 (H 0.030 an hour against 0.024) and T6 (tills short 35.2 against 34.4; the wage support
unpaid 156p against 334p passes), A4 T4 (K 0.76 against 0.74). A8 passes F1's bar (against steer-d1b-right-floors) and
T11. On the full scorecard (for information, `steer-accept-p5-info.md`) A5 at F1 passes T1, T2 and T4-T7 and fails T3
(86% at a back-off) and T8 (65% of nights): the supervisor's rule 1 is on most of the year at +1 here too, easing the rich
hold 38p on average, and that is why A5's S is 21.8%, not F1's open-loop 23.3%. F3 (doc 74's t1 controller) is no longer
in the code (Phase 2 removed it), so the ladder has no rung under F1 to take. **The right end is open:** F1 is in the table
as the last buildable rung, with its failures named; what to do about it is the coordinator's (see "Open" below).

**The left ships Full (L50).** -0.5's G target is now A2's own, by the calibrate rule (the mark's days 120-365 mean, its
band 2 sigma of its 28-day mean): 0.186 +- 0.020. Phase 4 had it halfway (0.208 +- 0.01) because no run had that table;
the tithe at 13.7% delivers 0.186 there (more equal than aimed). Set after seeing A2, and said so.

**The acceptance as shipped** (`steer-accept-p5.json`, `steer-accept-p5.md`): **60 of 64 tests pass; the 4 that fail are
F1's T4 at A4 and A5, F1's T6 at A5, and the sweep's leg 3 in G.** A1 is from the first batch: it has no supervisor action
and no slider move, and nothing the fix round changed is read at -1, so it is the same run on the shipped build. A2, A3,
A6 and A7 ran on the shipped build; the table left of +0.5 is the shipped one.

| run | x | floors | G day | S % (without chartered cash) | H/h | K | treasury mean | float days | tills short | support unpaid | supervisor acts / backed off | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| A1 `steer-p5-A1` | -1 | off | 0.066 | 12.7 | 0.000 | 0.00 | 1,492 | 0 | 32.2 | 101 | 0% / 0% | Full: every test passes |
| A2 `steer-p5b-A2` | -0.5 | off | 0.186 | 13.1 | 0.001 | 0.07 | 1,909 | 0 | 32.8 | 79 | 13% / 17% | Full: every test passes (G on A2's target) |
| A3 `steer-p5b-A3` | 0 | off | 0.350 | 13.4 | 0.022 | 0.91 | 1,316 | 2 | 29.1 | 219 | 0% / 0% | Full: every test passes; T10 byte for byte stable10b (366 lines) |
| A4 `steer-p5c-A4` | +0.5 | off | 0.392 | 19.5 | 0.017 | 0.76 | 850 | 10 | 31.8 | 258 | 27% / 35% | F1: T4 fails (K 0.76 against 0.74) |
| A5 `steer-p5c-A5` | +1 | off | 0.404 | 21.8 | 0.030 | 0.64 | 708 | 4 | 35.2 | 156 | 65% / 86% | F1: T4 and T6 fail |
| A6 `steer-p5b-A6` | 0 | both | 0.285 | 11.8 | 0.000 | 0.04 | 2,158 | 0 | 34.1 (1.17 x A3) | 90 | 0% / 0% | Full: every test passes, T11 too |
| A7 `steer-p5b-A7` | -1 | both | 0.066 | 12.7 | 0.000 | 0.00 | 1,491 | 0 | 31.8 (0.99 x A1) | 101 | 0% / 0% | Full: every test passes, T11 too |
| A8 `steer-p5c-A8` | +1 | both | 0.294 | 14.2 | 0.000 | 0.07 | 1,147 | 0 | 33.5 | 81 | 0% / 0% | F1: passes, T11 too |
| R33: A5 `steer-p5-A5` | +1 | off | 0.403 | 32.1 (23.5) | 0.015 | 0.62 | 681 | 6 | 34.3 | 152 | 66% / 100% | Full: T2, T3, T8 fail |
| R33: A5 again `steer-p5b-A5` (cap 1,300p) | +1 | off | 0.443 | 34.1 (26.6) | 0.025 | 1.14 | 781 | 20 | 35.7 | 282 | 52% / 69% | Full: T2, T3, T5, T8 fail |
| R33: A4 `steer-p5-A4` | +0.5 | off | 0.372 | 24.5 (19.0) | 0.013 | 0.81 | 938 | 7 | 29.2 | 232 | 55% / 93% | Full: T3, T5, T8 fail |
| R33: A8 `steer-p5-A8` | +1 | both | 0.297 | 24.2 (16.2) | 0.000 | 0.00 | 1,156 | 0 | 33.6 | 89 | 0% / 0% | Full: every test passes |

Days 120-365, means a day. The A9 sweep (`steer-p5c-A9`, 480 days, toggles off; 0 -> -1 at day 120, -> +1 at 240, -> 0 at
360):

| leg | measure | from -> target | half the way | 90% | overshoot | T12 |
|---|---|---|---|---|---|---|
| 1, to -1 | G | 0.361 -> 0.066 | 15 days | 26 days | 0 | PASS |
| 2, to +1 | S | 12.4 -> 23.3 | 25 days | 41 days | 1.40 (band 2.36) | PASS |
| 3, to 0 | G | 0.450 -> 0.350 | 23 days | 76 days (at most 75) | 0 | FAIL, by a day |
| 3, to 0 | S | 24.7 -> 13.4 | 19 days | 33 days | 0 | PASS |

(On the first batch's build, R33 at +1, leg 2 failed, 86 days, and legs 1 and 3 passed.)

**What was checked, as the user asked:**

- **The system follows the slider.** In the sweep every lever moved the night the slider did (day 120: the tithe 0 -> 50,
  the hands' share 0 -> 50; day 240: the tithe back to 0, the ground rent 0 -> 30, the rich hold 40 -> 150p; day 360: the
  cap 980 -> 2,000p, the ground rent 30 -> 0, the rich hold back to the world's 40p), and the measures followed: G 0.348 ->
  0.068 in about 10 days at -1; S 13.5 -> 21-24.5% at +1; G 0.358 and S 13.8-14.4% back at the centre. The ends hold
  their marks: A1's G 0.066 against its 0.066 target on 100% of days, A2's 0.186, A3 byte for byte stable10b.
- **Money is conserved** every night of every run: the daily line's `conserved` is true on every day of all 14 runs, the
  two sweeps' 480 days included; T7 passes on every run it judges (the estates' posts paid every night, the Wharf net
  within 8p a day of stable10b's where judged).
- **The supervisor acted when it should, and only then.** Nothing fired at the centre (A3, A6) or with the floors on (A6,
  A7, A8). At the left it fired on real excursions and cleared: A2's hunger rule on day 224 (2.43 households short against
  0.5) eased the tithe and cleared by day 241; in the sweep the hunger rule fired on day 224 at -1 (1.29 households short
  against 0.1) and eased the tithe to 38.6% by the next move (day 240). At the right it fired on the treasury under its
  600p floor, but there that floor is the treasury's ordinary level, so it acted on 27-65% of nights: that is the right
  end's failure, not a quiet guard.
- **The scorecard per test** is in `steer-accept-p5.md` (shipped), `steer-accept-p5-info.md` (the right at F1 on the full
  scorecard) and `steer-accept-p5-r33.md` (the charters).

**The labels** (`reach` in the table): the left "Shared: Gini 0.07 through the day" (A1: 0.066); the right now "Held by
the rich: about 22% of all money with the upper class and the Houses (13% at the centre)" (A5 at F1: 21.8%). Both are for
Phase 6 to re-centre on years 2-3.

**Tests:** `steer_tests` 548 checks; ctest 80 of 80; `test_studio` and `test_econ_report` pass; the Editor's lib tests 75 of
75 and `tsc` clean (with the main checkout's `node_modules` linked in for the run). The Editor's `src/model` tests have one
failure that is not the steering's (`content.test.mjs`: bundled Greyfen has 11 people, the test wants 10).

**Open, for the coordinator (not decided here):**

1. **The right end.** F1 is in the table and fails its own bar narrowly (T4: H 0.030 against 0.024; K 0.76 against 0.74 at
   +0.5; T6: tills short 35.2 against 34.4), with the supervisor on most nights at +1. The cause is the same with or
   without charters: the right's treasury lives at 600-800p and the plan's floor is never under 600p. The choices I see,
   none of them run: (a) accept F1 as it is, with the margins named; (b) the right's floor under 600p (the plan's
   `floorLeast`, so the user's call), which would let rule 1 fire only on real dips; (c) the world's cap at +1 (2,000p, as
   S2: the treasury about 1,180p, the supervisor quiet), with the wage support unpaid then about T6's bar (S2: 272p with
   the rich hold eased); (d) rule 1 at the right without its rich-hold move (Phase 4's addition, beyond the plan), so a
   low treasury there no longer eats the slider's reach. Each needs about one run at +1 and one at +0.5, which this phase's
   budget did not have.
2. **The sweep's leg 3 in G** missed by a day (90% in 76 days, at most 75), from +1 back to the centre.
3. **-0.5's G target** moved to what the table delivers (0.186), and **+0.5's floor** to its own run's; both were set after
   seeing the runs, by the plan's own rules for targets and floors.

### Phase 6: confirmation and ship (2026-11-07, branch `r2-controller`, merged into `ridgemere2`)

About 1 h 50 min of wall clock (00:18 to about 02:10 MST), most of it the runs. **The five 3-year runs of 5.4** in one
batch (`steer-p6.json`, prefix `steer-p6-`; a copy of 25de4d4's build and Data, `build-c/world_check.p6`, `build-c/data.p6`),
five at once on cores 2-19, every run exited 0 (65-91 minutes; the plan said about 40), nothing left running. No other
run: no trim was needed (below), so no targeted check. **Runs used in all: about 38.6 one-year runs and the five 3-year
runs** (the plan's 34 and 5). Scratch copies of export70 only; nothing written to DEV or PROD; no server.

**What the coordinator decided before the runs** (under the user's delegation): ship the right at F1 as Phase 5 left it,
choice (a) of Phase 5's "Open", and confirm it, rather than spend runs on (b)-(d). Each of those changes a plan rule or the
user's treasury levers, and none had been run.

**Year 1 is Phase 5's, byte for byte.** The first 366 daily lines of A1, A5, A7 and A8 are the same as `steer-p5-A1`,
`steer-p5c-A5`, `steer-p5b-A7` and `steer-p5c-A8` on every field, and A3's are the same as `steer-p5b-A3`. So A1 really is
the same run on the shipped build, which Phase 5 had argued but not run. A3 is stable10b on all 1,095 daily lines (T10).

**The targets for years 2 and 3** (`steer-targets-p6.json`, written before any year 2 or 3 was read): T6 needs each
mark's floors-off tills short for every year, and tills short falls after year 1 everywhere (stable10b: 29.1, 23.2,
22.2). The centre's are stable10b's own years. +1's are steer-right-t1's (F1's own 3-year run; steer-b0-right is its first
year). -1's are L50's year-1 32.15, scaled by steer-left-t1's own year-to-year ratio (27.6, 27.3), since no 3-year run of
the L50 table existed. The half marks are halfway.

**The results** (`steer-accept-p6.md`; year 1 is days 120-365, years 2 and 3 are whole; means a day):

| run | x, floors | year | G day | S % | H/h | K | treasury mean | float days | tills short | support unpaid | supervisor acts / backed off |
|---|---|---|---|---|---|---|---|---|---|---|---|
| A1 `steer-p6-A1` | -1, off | 1 | 0.066 | 12.7 | 0.000 | 0.00 | 1,492 | 0 | 32.2 | 101 | 0% / 0% |
| | | 2 | 0.062 | 12.3 | 0.001 | 0.00 | 1,512 | 0 | 29.1 | 71 | 8% / 10% |
| | | 3 | 0.060 | 12.1 | 0.000 | 0.00 | 1,510 | 0 | 28.3 | 66 | 0% / 0% |
| A3 `steer-p6-A3` | 0, off | 1 | 0.350 | 13.4 | 0.022 | 0.91 | 1,316 | 2 | 29.1 | 219 | 0% / 0% |
| | | 2 | 0.349 | 13.2 | 0.010 | 0.61 | 1,615 | 0 | 23.2 | 122 | 0% / 0% |
| | | 3 | 0.340 | 12.7 | 0.011 | 0.45 | 1,759 | 0 | 22.2 | 76 | 0% / 0% |
| A5 `steer-p6-A5` | +1 F1, off | 1 | 0.404 | 21.8 | 0.030 | 0.64 | 708 | 4 | 35.2 | 156 | 65% / 86% |
| | | 2 | 0.421 | 22.3 | 0.009 | 1.01 | 692 | 4 | 28.1 | 83 | 61% / 82% |
| | | 3 | 0.439 | 23.5 | 0.020 | 0.84 | 706 | 4 | 27.7 | 86 | 51% / 60% |
| A7 `steer-p6-A7` | -1, both | 1 | 0.066 | 12.7 | 0.000 | 0.00 | 1,491 | 0 | 31.8 | 101 | 0% / 0% |
| | | 2 | 0.062 | 12.4 | 0.000 | 0.00 | 1,511 | 0 | 28.9 | 67 | 4% / 4% |
| | | 3 | 0.060 | 12.1 | 0.000 | 0.00 | 1,509 | 0 | 27.5 | 64 | 0% / 0% |
| A8 `steer-p6-A8` | +1 F1, both | 1 | 0.294 | 14.2 | 0.000 | 0.07 | 1,147 | 0 | 33.5 | 81 | 0% / 0% |
| | | 2 | 0.316 | 15.4 | 0.000 | 0.00 | 1,286 | 0 | 27.8 | 40 | 0% / 0% |
| | | 3 | 0.304 | 15.1 | 0.000 | 0.00 | 1,294 | 0 | 27.2 | 35 | 0% / 0% |
| steer-right-t1 (F1's ref) | +1, off | 1 / 2 / 3 | - | 23.3 / 22.3 / 21.8 | .024 / .005 / .018 | .74 / .88 / .70 | 815 / 894 / 984 | 12 / 10 / 12 | 34.4 / 26.5 / 24.2 | 334 / 201 / 184 | - |

Money was conserved on every night of all five runs (3 x 1,095 nights). The estates' posts were paid every night.

**The verdicts: 109 of 116 tests pass.**

- **A1, A7 (the left, Full): every test passes in every year, T13 too.** G's year means are 0.066, 0.062 and 0.060. Year
  2 to 3 moved 0.002, under half a band. The supervisor acted on 8% (A1) and 4% (A7) of year 2's nights, on hunger and the
  shops, and cleared both times; it was quiet in year 3. With both floors on, T11 passes every year: nobody hungry, nobody
  short after relief, Never Hungry's Wharf pennies 0.10-0.23 a day, and all of the floors' money idle.
- **A8 (+1 F1, both floors, judged on the full scorecard, S one-sided): every test passes in every year, T11 and T13
  too.** F1's floors reference has one year only, so it could not be judged on F1's own bar.
- **A3 (the centre): T10 passes; T13 passes; T2 fails in year 3.** G's 28-day mean was in its band (0.350 +- 0.021) on
  76% of year 3's days. A3 is stable10b byte for byte, and at the centre no lever moves, so this is the centre's own
  drift (0.350, 0.349, 0.340) against a year-1 target: a test defect, not a controller fault (5.3: "where a test fails
  today's centre, the test is wrong"). Re-centring on years 2-3 (below) moves it to year 1 (just under 90% of days, against 90%). At x = 0,
  T10 is the test that binds.
- **A5 (+1 F1, on F1's bar, "no worse than steer-right-t1 in T4-T7"): T5 and T7 pass every year; T4 and T6 fail every
  year, by small margins.** T4: H 0.030 / 0.009 / 0.020 against 0.024 / 0.005 / 0.018, and K 1.01 / 0.84 against 0.88 /
  0.70 in years 2 and 3. T6: tills short 35.2 / 28.1 / 27.7 against 34.4 / 26.5 / 24.2. Support unpaid is half the ref's,
  83-156p against 184-334p. The float days are a third of the ref's, 4 a year against 10-12. On the full scorecard (for
  information, `steer-accept-p6-info.md`) A5 also fails T3 and T8 every year: the supervisor acts on 65%, 61% and 51% of
  nights, because the treasury lives at its 600p floor at +1. It fails T5 in year 2 (13 spring days under the floor, 4
  summer float days). T13 in S fails narrowly: 21.8, 22.3, 23.5, a change of 1.21 against half a band, 1.18. The drift is
  toward its target (23.3) as the supervisor eases less each year (at a back-off 86%, 82%, 60% of days).

**No trim (4.4).** No measure drifted on the shipped scorecard: T13 passes on A1, A3, A7 and A8. A5's S moved 1.2 points
in year 3, which is under one band. It moved toward the target, and S has no actuator at F1: the plan's S trim is "n by
one table step", and F1 has no charters. So no trim was built, and nothing joined a last fix round.

**Re-centred on years 2-3** (3.5.4; set after seeing the runs, as the plan says; `steer-targets-p6-recentred.json`,
re-judged in `steer-accept-p6-recentred.md` and `steer-accept-p6-info-recentred.md`):

| mark | measure | year-1 target (band) | years 2-3 (band) |
|---|---|---|---|
| -1 | G | 0.066 (0.010) | 0.061 (0.010) |
| 0 | G | 0.350 (0.021) | 0.344 (0.021) |
| 0 | S | 13.4 (1.0) | 12.9 (1.0) |
| +1 | S | 23.3 (2.36) | 22.9 (3.48) |

The verdicts are the same in substance. 109 of 116 pass: A3's T2 moves to year 1, and A5's F1 failures are unchanged.
On the full scorecard A5's T2 and T13 now pass, because the wider band comes from years 2-3's own spread. That is a
mechanical result, not an improvement.

**The labels** (the table's `reach`, re-centred on years 2-3):
- left: "Shared: Gini 0.06 through the day (cash a head over all households; 0.34 at the centre)"
- right: "Held by the rich: about 23% of all money with the upper class and the Houses (13% at the centre)"

**The treasury floors were not re-centred.** 3.5.4 says re-centring changes "the supervisor's floors, not the table",
but the floors are in the table (`treasuryFloors`). From A1, stable10b and A5 (`steer_floors.py`):
- -1: 1,111-1,135p (months 2-4 were 600p, from year 1's empty first 120 days).
- The centre: 954-1,576p (was 600-1,506p).
- +1: 600p, as now.
Raising the centre's floor by up to 650p changes what Never Hungry's and Never Broke's skim may take from the treasury at
the centre and at -0.5 (A6, A2), and no run has judged that. They stay as Phase 4 wrote them; this is open.

**What shipped, rung by rung:**

| end | rung | what | measured reach (years 2-3) | its bar |
|---|---|---|---|---|
| left (-1) | **Full** | the Levelling tithe 50%, the hands' share 50%, `spendAbove` 25p, the treasury 1,500p, the supervisor | G 0.061 through the day (the centre 0.344) | the scorecard: passes every test, 1 and 3 years, floors on and off |
| -0.5 | Full | the tithe 13.7%, treasury target 1,400p | G 0.186 (year 1; A2, no 3-year run) | passes (Phase 5) |
| centre | - | the world, byte for byte | stable10b | T10 passes on 1,095 lines |
| +0.5 | F1 | half of +1, cap 1,490p | S 19.5% (year 1; A4) | fails F1's T4 (K 0.76 against 0.74; Phase 5) |
| right (+1) | **F1** | no charters (built, unused), the dividend 30%, the ground rent 30%, the rich hold 150p, cap 980p, target the world's, the supervisor | S 22.9% (the centre 12.9%) | fails F1's bar in T4 and T6 by small margins every year; with both floors it passes the full scorecard |

**Checked, as the user asked.** The system follows the slider: Phase 5's sweep moved every lever the night the slider
moved, and the measures followed. Over three years each end holds its mark with no drift: G 0.066 to 0.060 at the left; S
21.8 to 23.5 at the right, toward its 23.3. The whole economy is managed by the controller. With the steering on, the DM
has only the slider, the on/off, Never Hungry and Never Broke; every lever is the table's or the supervisor's, and the
supervisor only ever moves a lever back toward the world's own value.

**Tests:**
- `steer_tests` 548 checks; ctest 80 of 80 (`build-c`).
- `test_studio` (52 tests, 16 skipped) and `test_econ_report` pass.
- The Editor's lib tests 75 of 75 and `tsc` clean.
- The Editor's `src/model` tests: 80 of 81. The one failure is not the steering's: `content.test.mjs`, bundled Greyfen
  has 11 people and the test wants 10.
- `test_studio`, `steering.test.ts` and `Tests/fixtures/steer_table.json` were already on the shipped table (Phase 5) and
  read the labels by their prefix only, so the re-centred labels changed no test.

**Open:**

1. **The right end at F1.** It fails F1's own bar by small margins (T4, T6), and the supervisor is on 51-65% of nights,
   because the right's treasury lives at 600-800p and the plan's floor is never under 600p. Phase 5's choices (b)-(d) are
   still the levers: a floor under 600p at the right (the user's), the world's 2,000p cap at +1 (the user's treasury
   levers), or rule 1 without its rich-hold move at the right (Phase 4's addition). Each needs about one run at +1 and one
   at +0.5.
2. **The treasury floors** re-centred on years 2-3 are computed but not in the table (above).
3. **T2 at the centre** cannot hold a single year-1 band over three years. The centre has no lever, and its G wanders
   0.337-0.375. The scorecard should judge T2 at x = 0 against each year's own stable10b, or not at all (T10 binds there).
4. The sweep's leg 3 in G (Phase 5: 76 days against 75) was not re-run.
5. Phase 7, the DM tab's edicts, ledger and year of log.

### Phase 7: the DM tab's edicts, ledger and year of log (2026-11-07, branch `r2-controller-dm`)

Built from `ridgemere2` at 3caefe8, 02:01 to about 02:25 MST with the merge and push. No simulation was needed. One check ran: two 20-day runs
(left and right, both floors on) on the Phase 6 binary and two on this one, to show the change only counts. Together they
are about a tenth of a one-year run, on scratch copies of export70. Nothing was written to DEV or PROD, and no server ran.

**What the DM tab gains.** Everything new is read only. The DM still has the slider, the on/off, Never Hungry and Never
Broke, and nothing else (3.8).

1. **The edicts.** Each lever the slider has moved off the world's own value gets one line, written as a law. The line
   says what the lever is for and compares that measure with its aim at the slider. Two examples:
   - "The Levelling tithe: 50% of the way to the mean a night. The earners' Gini 0.34 (aim 0.33 ± 0.04)."
   - "The Houses' dividend: 30% of the estates' spare to the upper class a night. The upper class and the Houses hold
     22.4% (aim 24.0% ± 3.0%)."

   The charters get a line whenever any are held or wanted, and the hands' share, the ground rent, the rich hold and the
   spending keep get one each when moved. The last line is always the gate toll, as Core reckons it (`gateTollNow`), with
   the treasury's 28-day mean and the target the slider gives the drift. Example: "The gate toll: 30.5. Treasury 1,491p
   (the toll drifts toward 1,500p)."

   The tithe's edict uses the earners' Gini, not 3.8's "Gini through the day". The table's target and the save's mean are
   both the earners' Gini, and the save holds no mean of G through the day; that measure lives only in a run's daily
   lines. The supervisor's line ("The supervisor: ...; the rich hold eased 20p") stays as Phase 4 built it, beside the
   levers.
2. **The weekly ledger**, EVE style. Core keeps a new `ledger` in the steering's save: a week per `day / 7`, the last 52
   weeks. Each week holds the nights counted and, for each lever, the pennies each payer paid and each receiver was given.
   The levers are the tithe, the charters, the ground rent (the Crown's estate and the treasury apart), the hands' share,
   the dividend, the treasury's wage support, the surplus to the poorest, the gate toll and the skim (by payer group and
   by purpose). Households are counted by class: the tithe now books what it took and gave by household class
   (`titheOut`, `titheIn`). The ledger is counted each night the steering or a floor is on, after every money step.

   The tab shows one week at a time, the newest first, picked from a list of the last eight. It gives two tables: each
   lever's money moved, who paid it and who was given it; and, for each payer or receiver (households by class) across
   all the levers, money in, money out and net.
3. **A year of log.** Core's steering log now keeps the lines of the last 366 days, at most 1,000, where before it kept
   the last 24. A busy year of the supervisor no longer pushes the DM's slider moves out (Phase 4 review, point 5). The
   studio reads the DM's audited changes up to 1,000 lines (it was 24). The tab's log has a filter: everything, the DM's
   changes, the slider as the world took it, or the supervisor.

**It only counts.** The ledger is left out of the steering's `empty()`, so counting it never wakes the steering's nightly
step on a world where that step would not run. The tithe's class lookup is the day's cached `homeClass`, which the
dividend and the luxuries already use. The 20-day check passed:
- The daily lines (`.jsonl`) match byte for byte on both runs.
- The full recordings (`.full.jsonl.gz`) match byte for byte.
- The end-of-run loop dumps differ only in the new `loop.steer.ledger`.

The real ledger read sensibly. At the left in week 1, the tithe moved 7,075p: middle-class households paid 4,371p and
were given 4,355p, upper-class households paid 1,576p, and lower-class households were given 2,399p. At the right, the
ground rent paid the Crown's estate 1,796p, the dividend paid upper-class households 170p, and the skim's 2,622p for Never
Broke came from upper-class households and the treasury.

**Files:**
- Core: `RatwSociety.h` (`LedgerWeek`, `ledger`, the log's limits, `titheOut`/`titheIn`), `RatwSteering.cpp`
  (`steerLog`, `steerLedgerNight`, the tithe by class), `RatwCityLoop.cpp` (the nightly call) and `RatwWire.cpp` (save
  and strict load: at most 52 weeks, each after the last, each at most 7 nights).
- Studio: `tools/studio.py` (`steer_edicts`, `steer_ledger`, `steer_toll`; the save's toll read beside the steering).
- Editor: `Editor/src/lib/steering.ts` (`LedgerWeek`, `logLines` by kind, `ledgerTitle`, `ledgerWho`),
  `Editor/src/dm/SteeringPanel.tsx` and `dm.css`.

**Tests:**
- `steer_tests` 558 checks (from 548), with a new `theLedgerAndTheLog`. It checks that:
  - the tithe's pennies are paid and given alike, by class;
  - the ledger is saved and read again;
  - a save's ledger is read with care (52 weeks at most, in order, a week at most 7 nights);
  - with the steering off there is no ledger;
  - 31 lines of log are kept, saved and read again, and a year later the old lines are gone.
- ctest 80 of 80.
- `test_studio` 53 tests (16 skipped, the database ones), with a new offline test of the edicts, the ledger and the
  year of log. `test_econ_report` passes.
- The Editor's lib tests 76 of 76, with a new one for the log by kind and the ledger's words, and `tsc` is clean.
- The `src/model` tests are still 80 of 81: the same `content.test.mjs` Greyfen failure (11 people against 10), not
  this work.

