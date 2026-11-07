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

Phase 1 done on `r2-controller` (below); the user's answers to section 9 first.

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
