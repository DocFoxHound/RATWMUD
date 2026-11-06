# 74. Steering the economy (2026-11-06)

The user, 2026-11-06 (verbatim):

> we've spent a long time trying to make the economy stable, I think it's as close as its going to get. We need to use
> existing levers and invent new ones that we can pull on the game to keep it stable over time, and make a small
> automated system that takes its settings from the DM interface (in its economy tab). What I want is for the user
> interface in DM to be a scale, from left to right, that adjusts where the money rests. In the center is close to our
> stable system now. To the left means the money and food is equal across all wolves, and to the right means the money
> is almost entirely in the hands of the rich upper class. What I think you're going to have to do is decide where
> levers can be applied to the economy that will help manage this. Also, those settings will determine how much these
> systems use these levers. I don't want admins to tweak individual parts, but I want them to be able to tell the
> automated system how much it should be leaning on these levers to make the system closer to what they want.

And the follow-up (verbatim):

> also I want the following options that can be toggled on or off: Never Hungry, and Never Broke. Never Hungry will
> automatically put food in the pantry of wolves going hungry. Never Broke will always put just enough money for wolves
> to pay for necessities when their bank account is low. We need a way to take money off the top of the richest places
> (which might shift, the richest spot may not always be the same group) to account for the added money

This doc is the design only: no code, no run, nothing written to DEV. It builds on doc 73 (the Wharf and its stabilizing
rounds; the centre is **stable10b**, `ridgemere2` 85ea134, export70), doc 68 (the city's loop, the self-correcting rules,
the treasury's drift), doc 64 Phase 12 (the orchestrator removed: a monitor and the DM's levers only) and doc 42 (money
in circulation). Doc 64's orchestrator decided for the big holders; this steering decides nothing for anyone. It moves a
handful of rules, and three small new money moves, slowly and within bounds, toward where the DM says the money should
rest.

**How it was measured.** stable10b, year 3 (days 730-1094) unless marked: `artifacts/economy/r2/stable10b.jsonl`,
`stable10b.full.jsonl.gz`, `stable10b-report.md`. The 28-day windows below are from the daily lines. Code at 85ea134.

## Headline

- **One slider, "Where the money rests"**, from -100 to +100, shown on the DM's Economy tab. Stored as the rule
  `steerLean`, 0 to 200, with 100 the centre: a rule can't go below 0. **-100 (Shared):** money and food about equal
  across all wolves. **0:** today's economy, stable10b. **+100 (Held by the rich):** most of the money that can move sits
  with the upper class and the Houses' estates. Beside it there is an on/off, `steer`. Off is the default, so every
  world and run is as today until a DM turns it on.
- **Two toggles, Never Hungry and Never Broke** (`neverHungry`, `neverBroke`), each on or off, and independent of the
  steering. Never Hungry puts food in the pantry of a wolf going hungry. The food is bought at the stable price from a
  shop's real stock, so the shop and its makers are paid. Never Broke tops up a household that is short, with just enough
  money for the next day's necessities.
- **The skim pays for both.** It takes money off the top of whoever is richest at that moment: households, shop tills,
  farms, the Houses' estates or the treasury. It judges each holder by what it holds over its own needs, and ranks them
  afresh every time. It takes exactly what the toggles pay out, so no money is made. It never takes a holder under its
  needs: a till keeps its float and its wages, and an estate keeps its posts' nights and the Crown's grants. Every penny
  is logged: who paid, how much, and for what.
- **The steering has one number, the push** `u`, from -1 to +1. Every lever's value is a fixed function of the push (a
  table, section 4). At `u = 0` every lever is at today's value. The slider puts the push at its own position at once
  (feed-forward). Then, every 28 days, the controller trims it within a narrow band around that position, judged on
  56-day means. The slider sets both where the targets are and how far each lever may go.
- **Admins don't tweak levers.** With the steering on, the tab shows each steered lever read-only: its value now, its
  value at the centre, its bound at this slider position, and the reason for its last move. Only the slider, the on/off
  and the two toggles can be edited. Every change is audited (`economy.steer`) and sent to the running server, as every
  economy change already is.
- **Money is conserved.** Every move is a `shift` between existing accounts, plus one new pass-through account,
  `fund:relief`. Nothing is minted outside the Wharf's valve. Never Hungry's fallback, when no shop has food, is an order
  from the Wharf, inside the valve's own rules.
- **The safety floor holds at every slider position.** The city's alms (`giveAlms`) always run as they do today. A
  hunger guard stops the push going right, and pulls it back toward the centre, while hunger is over its ceiling. With
  Never Hungry on, the floor is that toggle's job. **At the far right the poor are poor, not dead.**
- **No migration.** The four DM settings are rules in `live.economy.rules`, a JSON column. The lever table is data in
  `Data/Items/city_loop.json`. The controller's state is saved with the loop's (`LoopState`), like the toll drift's.

## 1. The centre: what today's economy is

The slider's centre is stable10b as measured, not as its rules intend. Four facts set the design.

1. **The treasury's drift is saturated.** `treasuryTarget` is 5,500p, but the treasury's slow level sits at about
   1,690p. So `followTreasury` has held the toll at its +1 bound (31%) since day 238, and the alms drift stays 0. The
   surplus step needs the treasury over `treasuryCap` + `surplusKeep` (6,000p), and it never fired in three years. The
   centre is therefore pinned to measured levels. The steering adds to the toll drift; it doesn't replace it (section 6.7).
2. **Purses are nearly flat by class.** Year-3 means per head: lower 50p, middle 47p, upper 48p. The comforts rule
   (`comfortsShare` 20% a day of what a household holds over `comfortsAbove`, 40p a head) drains any purse back to the
   shops. The money that isn't in purses is in the tills (29.8%: floats and a week's wages, the rest paid out each night
   as the keeper's profit), the farms' floats (5.5%), the estates (3.3%) and the treasury (2.7%).
3. **The earners' Gini misses the Houses.** `gini_earners` counts households holding a paid post. A House head living on
   leisure holds none, so a world whose money moved to the Houses could show a *lower* earners' Gini. The right half of
   the slider is therefore steered on **the upper share** (section 3), and the earners' Gini steers the left half.
4. **The rules have room at the centre.** stable10b's 28-day windows: Gini 0.470-0.514 (3-year mean 0.487, year 3 0.484);
   households short 0-3.18 (year 3 0-1.64); hungry residents an hour 0-0.128 (year 3 0-0.052); treasury 870-2,150p (year
   3 1,563-2,056p). The centre's dead bands and ceilings below are set just outside these ranges. **The steering at the
   centre should not move at all in a stable10b-like run.**

## 2. The slider

| | stored `steerLean` | `x` | the label on the tab |
|---|---|---|---|
| far left | 0 | -1 | Shared: money and food equal across all wolves |
| | 50 | -0.5 | Leaning shared |
| centre | 100 | 0 | Today's economy |
| | 150 | +0.5 | Leaning to the rich |
| far right | 200 | +1 | Held by the rich: the upper class and the Houses |

`x = (steerLean - 100) / 100`. The tab shows -100 to +100 and stores 0 to 200. Values between the five marks are
interpolated linearly, both in the targets (section 3) and in the lever table (section 4).

With the steering off, the slider is kept but greyed and unused. The push is 0, every lever is at today's value, and the
toggles still work.

## 3. Targets by slider position

The controller judges each measure on its 56-day mean (section 6.1). A target with a band is steered toward. A ceiling
or floor is a guard.

**Since Tune round 1 (Progress) the table reads as below**; the first design's values are in brackets where they moved.
The ends were set to what the economy holds with its safety floor intact: the left's share-out at 30% reaches a Gini of
about 0.32 (open question 1), and the right, with the rich hold at 600p a head, starved the shops and the treasury
(open question 2).

| measure | -100 | -50 | 0 | +50 | +100 |
|---|---|---|---|---|---|
| **Gini of earning households** (steers the left half and the centre) | 0.33 ± 0.04 (0.15) | 0.41 ± 0.04 (0.32) | **0.487 ± 0.035** | watched | watched |
| **The upper share** (steers the right half and the centre) | at most 14% (7) | at most 14% (7) | **13% ± 2** (7 ± 2.5) | 18.5% ± 2.5 (25 ± 4) | 24% ± 3 (45 ± 5) |
| Top-10% share of household purses (watched, logged) | at most 27% (20) | at most 31% (30) | logged (about 35%) | at least 37% (40) | at least 40% (55) |
| Hunger ceiling: hungry residents an hour, 28-day mean (every position) | 0.02 | 0.05 | 0.15 | 0.15 | 0.15 |
| Households short a day, 28-day mean (ceiling) | 0.1 | 0.5 | 3.5 | 4.0 | 5.0 |
| The treasury, 28-day mean: `treasuryFloor` (no leaning right, no lowering the toll; the ground rent to the treasury) | 800p (1,400) | 800p | 800p | 800p | 800p |
| The treasury, 28-day mean and that night: `treasuryLow` (the hunger guard's weekly pull) | 500p (1,000) | 500p | 500p | 500p | 500p |

- **The upper share** is everything the upper class and the Houses hold, over all the money in the realm: upper-class
  households' purses, the five estates, and the tills of the Houses' businesses (`houseOfPost`: the ironworks, the
  foundry, the country workshops). In stable10b this is the estates' 3.3% plus the upper purses' 3.3%, plus the House
  businesses' small tills: about 7%.
- **The right end is 45%, not 90%.** Some money can't move. The tills' floats and a week of their wages (about 30%) are
  what lets a shop pay its posts. The farms' floats (5.5%) do the same for the farms. Every other household needs a
  working purse to buy its food. That leaves at most about half the money to move to the rich. "Almost entirely in the
  hands of the rich" means almost all the money that can move.
- **The top-10% share** has no measure today (section 9 adds one). Its centre is not known, so it is only logged at the
  centre. The figure of about 35% is a lognormal's at Gini 0.487, a guess. At the other positions it is a check, not a
  steered target. The centre run (steer-centre) writes its measured value into this table.
- **The earners' Gini at the right** is watched, not steered: point 3 in section 1.
- **The left end, 0.15,** is as low as the purses can go when wages are paid at 18:00 and spent the next day. It is
  measured at 18:00, after every PayHour step (section 6.5).

## 4. The push and the lever table

The push `u` is one number from -1 to +1, kept in `LoopState.steer.push`. Each lever's value is read from this table at
`u`, interpolated linearly between the marks. **W** means the world's own value of that rule: whatever `rules_` or
`city_loop.json` gives it today. So at `u = 0` every lever is the world's own, and a DM's earlier setting of
`surplusKeep` (say) is the centre the steering uses.

| lever | what it moves | -1 | -0.5 | 0 | +0.5 | +1 |
|---|---|---|---|---|---|---|
| **share-out** (new) | % of the rich's spare, levelled to the poorest each night | 30 | 12 | 0 | 0 | 0 |
| **hands' share** (new) | % of a till's nightly profit paid to its posts instead of the keeper | 50 | 25 | 0 | 0 | 0 |
| **toll lean** (new, on the toll) | points added to the gate toll | +1 (+3) | +0.5 (+1.5) | 0 | 0 (-1) | 0 (-2) |
| `surplusKeep` (existing) | what the treasury keeps over its cap before the surplus goes out | 0 | 1,000 | W (4,000) | W | W |
| `treasuryCap` (existing) | over this, the wage support, the extra alms and the surplus | 1,500 | W (2,000) | W | W | W |
| `surplusAlmsTo` (existing) | the surplus alms level the poorest up to this, a head | 80 | 60 | W (40) | W | W |
| **ground rent** (new) | % of a keeper's nightly profit to the Crown's estate (to the treasury while it is under `treasuryFloor`) | 0 | 0 | 0 | 15 (20) | 30 (45) |
| **rent scale** (new) | % on every land rent (`landRent*`) | 100 | 100 | 100 | 112 (120) | 125 (140) |
| **the Houses' dividend** (new) | % of the estates' pooled spare paid to upper-class households each night | 0 | 0 | 0 | 15 (20) | 30 (40) |
| **the rich hold** (new) | the comforts threshold for upper-class households, pennies a head | W (40) | W | W | 100 (150) | 150 (600) |
| **the spending keep** (new in Tune round 1) | the comforts threshold for every other household, pennies a head | 25 | 32 | W (40) | W | W |

The table is data, in `Data/Items/city_loop.json` under `"steering": {"levers": {...}, "targets": {...}}`, read with
the rest of the city loop's data. A DM can't edit it. Atlas shows it, and an author can edit the file. The new levers
are not rules: they have no value of their own, only the table's at the push. With the steering off they are 0 (or
100 for the rent scale, or W), so today's code paths are unchanged.

**How far the push may go from the slider.** The push starts at `x`, so the slider acts at once. The controller then
trims it within `[x - w, x + w]`, clipped to [-1, 1]. The width `w = steerWidthMid + (steerWidthEnd - steerWidthMid) ×
|x|` is 0.15 at the centre and 0.30 at the ends. So at the centre the steering may lean at most 15% of the way to
either end: it nudges and doesn't remake the economy. At the ends it has room to reach its target. **This is how the
slider sets how hard each lever may be leaned on.** At slider position `x`, a lever can reach at most its table value at
`x ± w`. The tab shows that bound beside the lever (section 8.2).

One exception: the hunger guard may pull the push past `x - w`, toward the centre and never beyond it (section 7).

## 5. The levers

Each lever moves money with `shift(from, to, …, why)` between accounts that exist, so the ledger and world_check's flow
table record it by group. Each has a protection that means it can't break whoever pays.

### 5.1 The share-out (new; the left half)

- **When:** at PayHour, after `giveAlms` and Never Broke, before `spendSurplus`.
- **Budget:** `r` = the table's share-out percent. The budget is the sum of two parts:
  - **Households over the mean.** Each NPC household whose purse per head is over the mean purse per head of all NPC
    households gives `r`% of its excess over that mean. It never goes under `skimKeepDays` days of necessities a head
    (section 6.3), and it pays from the member with the most first.
  - **The other rich holders.** The richest tenth of the skim's other holders (tills, farms, estates, the treasury;
    section 6) give `r`% of their spare, by the skim's own rules and caps.
- **Out:** into `fund:relief`, then out at once through `topUpPoorest` (`RatwCityLoop.cpp:1979`), generalised to take its
  source account, with no `surplusAlmsTo` cap. It raises the poorest households per head to one level, poorest first,
  paid to the member with the least. Whatever it can't place stays in `fund:relief` for tomorrow, and is logged.
- **Effect:** the purses move `r`% of the way to the mean each night. At 30% the gap halves in two nights, which is why
  the far left can reach a Gini of 0.15 against the daily churn of wages and spending. Food follows the money: with
  `fairFood` on (it is), the least-fed member buys first, and equal purses buy equal food.

### 5.2 The hands' share (new; the left half)

- **Where:** in `payWages`, where a city till's excess over its cap goes to its keeper as profit
  (`RatwCityLoop.cpp:~1722`).
- **What:** first `h`% of that excess goes to the till's other posts that night, split by their wage, and the rest goes
  to the keeper.
- **Bounds:** the till keeps exactly what it keeps today (its float and `floatWagesDays` of wages), so it can't break the
  till. A House business's or workshop's excess still goes to its House: the hands' share applies only to city tills
  with a keeper.
- **Effect:** money moves from keepers to hands at its source. Each night it is booked as `posts' share` beside `profit`.
- **Wage drift:** `judgeWages` is passed the keeper's profit after the hands' share, so the wage drift judges the till on
  what its keeper actually kept.

### 5.3 The toll lean (new; on the existing toll)

- **What:** `gateTollNow()` (`RatwCityLoop.cpp:2645`) adds the lean's points to the world's toll and `loop.toll`. The
  total is clamped to `gateToll`'s range, 0..100.
- **Left:** a higher toll takes more of every draw's cost from the tills into the treasury. That fills the treasury
  toward `treasuryCap` + `surplusKeep`, which the left half also lowers, so the surplus fires and its alms level up the
  poorest (`topUpPoorest`). The left half opens this path, which has been dormant all along.
- **Right:** a lower toll leaves more with the keepers, so more profit, which the ground rent then shares with the
  Crown.
- **Treasury guard:** the lean may not go below 0 while the treasury's 28-day mean is under 1,400p. If the mean is under
  1,000p, the lean goes back up 0.5 points each judging until it is at least 0, whatever the push. The alms (`almsADay`,
  the line under hunger) are never steered down.

### 5.4 The treasury's surplus (existing rules, steered; the left half)

`surplusKeep`, `treasuryCap` and `surplusAlmsTo` are read through `rule()`. With the steering on, `rule()` returns the
table's value at the push for these three (section 6.6). At the left the surplus is reachable: at -1, the cap is 1,500,
the keep is 0, and the surplus levels up to 80p a head. The existing step does the rest. It is 80% alms to the poorest
and 20% works orders to the city's trades, and it is never taken under the cap. A lower cap also lets the wage support
and the no-earner and low-wage alms draw more, since those come only from what is over the cap. That leans the same way.

### 5.5 The ground rent (new; the right half)

- **Where:** in `payWages`, beside the hands' share.
- **What:** `g`% of a city keeper's nightly excess goes to the Crown's estate (`estateFund` of the Crown; the treasury if
  the estates are off), booked `ground rent`.
- **Who:** the Crown is the realm's landlord in the city, and its estate already grants to the other Houses when they are
  short. A House business's excess already goes to its own House and pays none.
- **Bounds:** the till keeps its float and wages as today.

### 5.6 The rent scale (new; the right half)

- **Where:** `landRentPercent` (`RatwFarms.cpp:503`) multiplies the area's land rent by `s`/100.
- **Bounds:** a farm pays rent only on a night it paid its crew in full (`payRents`, `RatwFarms.cpp:419`). So a higher
  rent can't cost a crew its wages: it comes from the farm's takings over that. The farm's float and its basket are kept
  as today.

### 5.7 The Houses' dividend (new; the right half)

- **When:** at PayHour, after the wages, the alms and the toggles.
- **What:** `d`% of the estates' pooled spare (`estateSpare`, which already keeps every estate's posts' nights, the
  Wharf's reserve and the Crown's grants) is paid to the upper-class households (`homeClass` = upper; 13 in stable10b),
  per head, to the member with the most.
- **Effect:** the money stays in the upper share. It moves from the Houses' purses to the Houses' people, where the rich
  hold (5.8) keeps it.
- **The estates' guard:** if any estate's posts went short on any night in the last 28 days, the dividend is 0 until
  the next judging. `estateSpare` already protects them; this backs it up.

### 5.8 The rich hold (new; the right half)

- **Where:** in `householdsShop`, the comforts budget (`RatwCityLoop.cpp:1279`). For an upper-class household,
  `comfortsAbove()` is replaced by the table's value: at +1, 600p a head.
- **Effect:** the rich keep their money instead of spending it down to 40p a head.
- **Not touched:** the luxuries' threshold (line 1236, which also reads `comfortsAbove()` for upper households), and
  cheese with it. Cheese stays as it is.

### Levers considered and left out

- **The wage drift's rules** (`wageDriftSpare`, the floors). They already settle the hands' and keepers' split each
  month, and the hands' share does the same job without fighting them.
- **`floatWagesDays`.** Moving it locks money in tills, which belong to no class.
- **The luxuries' rules.** Cheese stays as it is.
- **`comfortsShare` for everyone.** The comforts' valve feeds the shops, and the shops pay the posts.
- **The Wharf's standing levers.** The valve stays the DM's, on its own action.

## 6. The skim

The skim is one function that takes a sum of money from whoever is richest at that moment and puts it in `fund:relief`.
It has three callers: Never Hungry, Never Broke and the share-out. (The share-out's households-over-the-mean part is its
own; section 5.1.)

### 6.1 Who can be skimmed, and their spare

The spare is what a holder has over its own needs, worked out fresh each time.

| holder | spare |
|---|---|
| An NPC household (never a player's purse) | its purses together − `max(comfortsAbove, skimKeepDays × necessities a head)` × heads |
| A city shop's till | cash − its float − (`floatWagesDays` + 1) × tonight's wages owed |
| A House business's or workshop's till | cash − its float − (`workshopWagesDays` + 1) × tonight's wages owed |
| A farm's till | cash − its float − its basket's most − its crew's night |
| An estate (each of the five) | `estateSpare(fund, owed)`: its posts' nights, the Wharf's reserve and the Crown's grants kept |
| The treasury | cash − max(`treasuryCap`, `TreasuryFloat`) − tonight's wage support wanted |

- **Never skimmed:** `fund:relief` itself, the accounts of the beyond (`outside:*` other than an estate's), the Wharf's
  credit (a limit, not coins) and players.
- **Necessities a head** are a day's: `almsNow()` (the alms line, a day's food), plus `households.reserve` (6p), plus the
  day's share of the household needs (fuel and the rest in `crafts.json` `needs`: each one's stable price divided by its
  `everyDays`, worked out once a day).
- **The tills at the two times.** At the shops' last hour a till holds the day's takings, so its spare is real. At
  PayHour, after `payWages`, it holds exactly its cap, so its spare is about 0. By then the night's profit is in the
  keeper's purse, which is skimmed as a household.

### 6.2 How it takes: water-filling from the top

- **Rank** every holder with spare > 0 by its spare in pennies, most first. Ties are broken by account id, so the order is
  deterministic.
- **Cap:** each holder may give at most `skimMost`% of its spare in one skim (50%).
- **Level:** find the highest whole-penny level `L` such that the holders over `L`, each taken down to `max(L, its spare ×
  (1 - skimMost/100))`, give at least the sum. Take that, from the most spare down, and the last pennies in rank order.
  The richest give first and most. A holder near the top gives a little. No one is taken under its needs.
- **Short?** If the first pass can't raise the sum (every cap reached), a second pass lifts the cap to 100% of the spare.
  If that still falls short, the shortfall is logged (`skim_short`), and the caller pays what was raised. A household's
  spare line drops to one day's necessities in the second pass; nothing else's does.
- **It is fresh each time.** The ranking is made at the moment of the skim from cash now. A House rich this week and
  drawn down pays less next week, and a till full on market day pays that day. **No group is fixed.**

### 6.3 Exactly what the toggles add

- Each caller first works out what it will pay out (the batch). It asks the skim for that sum less what `fund:relief`
  already holds, then pays out of the fund.
- So the fund only passes money through. Its balance at the end of each night is 0 or a few pennies left over from a
  share-out that couldn't place them. That leftover is used first next time and logged daily (`relief_fund`).
- Over any span: **skimmed = paid out by the toggles and the share-out + the change in `fund:relief`**. The report checks
  this to the penny (section 9).

### 6.4 The ledger

Every skim `shift` has the reason `skim: never hungry`, `skim: never broke` or `skim: share-out`. Every payment out of
the fund has `relief: food`, `relief: necessities` or `relief: share-out`. The day's line counts the skim by payer group
(households by class, shops, House businesses, farms, estates by House, the treasury), and by purpose. It also names the
day's five largest payers. The full log adds a `skim` table: payer, group, pennies, purpose.

## 7. Never Hungry and Never Broke

Both are rules, `neverHungry` and `neverBroke`, each 0 or 1. Each works whether the steering is on or off, at every
slider position. They cover NPC households only, the farms' crews among them (they are households). They never cover
players.

### 7.1 Never Hungry

- **When:** at the shops' last hour (`ShopShut - 1`, 17:00), after `householdsShop`, the patrons, the estates, the farms'
  baskets and the Wharf's hour. Every household has done its own buying by then.
- **Who:** every member still holding less than `shortDays × NourishADay` (a day's food) on hand. This is the same line
  that marks a household short today.
- **What:** food up to that line and no more, put in the member's own stock, its pantry. Residents eat from what they
  carry (`eatFirst`, `RatwResidents.cpp:813`). The homes' `larder` accounts hold nothing in the loop and stay unused.
- **Bought where:** from an open shop in the household's community that has food on its shelf. The cheapest food first,
  a piece at a time, never a luxury. It pays the till **the stable price** (`stablePrice`, `RatwWharf.cpp:18`), not the
  shop's drifted price. The shop is paid, the piece leaves its stock, and its makers are paid by its next draw as for any
  sale.
- **Wharf fallback:** if no shop in reach has food, the food is ordered from the Wharf (`orderFromWharf`,
  `RatwWharf.cpp:141`) at its stable price and the toll. That is inside the valve's own rules (an import, money to the
  beyond), and it is logged apart.
- **How it is paid for:** the batch works out the whole hour's food bill (pieces × stable price) first. The skim then
  raises exactly that from whoever is richest at 17:00 (the full tills of market day, most often), and the fund pays the
  tills.
- **Logs:** `households_short` stays the count before relief, as today, so the run compares with stable10b. A new
  `households_short_after` counts after it, and should be 0 every day. `never_hungry` gives pieces, pennies, homes, and
  the Wharf's share.
- **Expected cost:** stable10b had 0.45 households short a day in year 3, each lacking about 1.5 meals. That is a few
  pennies a day. Year 1 (0.91) costs about twice that.

### 7.2 Never Broke

- **When:** at PayHour, right after `giveAlms`. The day's wages and the city's alms come first, and Never Broke covers
  what is left.
- **Who:** every NPC household whose purses together are under its necessities: necessities a head (6.1) × heads.
- **What:** it is topped up to exactly that line, paid to the member with the least. A household over the line gets
  nothing, and no one is given more than the next day's food, reserve and needs. This is "just enough".
- **How it is paid for:** the night's top-ups are added up first, the skim raises that sum from whoever is richest at
  18:00, and the fund pays out.
- **Expected cost:** a household given the alms holds 10p a head (the alms line), so Never Broke adds the reserve and the
  needs, about 6-9p a head. In stable10b, roughly 100 residents end a night on the alms, so it may be **500-900p a
  day**. That is much more than Never Hungry: a real flow from the rich to the poorest. The steer-centre-floors run
  measures it.
- **The steering compensates.** With the steering on, it sees the flattening in the Gini and the upper share and leans
  the push right within its band. With the steering off, nothing compensates, and the toggle simply costs the rich that
  much.

### 7.3 The floor, the toggles and the slider together

- **Both toggles off:** the gentler built-in floor holds at every position.
  - `giveAlms` runs exactly as today, with the alms line never steered down.
  - **The hunger guard** applies. If the 28-day mean of hungry residents an hour is over its ceiling (section 3), or
    households short are over theirs, the push may not move right. If either is over twice its ceiling, the push moves
    left by one step each judging. It can go below the band's left edge, but never past the centre.
  - At +100 the poor hold little: the rich hold their money, and the Crown takes the ground rent. They still get the
    day's alms and the first claim on food (`fairFood`). **The poor are poor, not dead.**
- **Never Hungry on:** the hunger floor is that toggle's job, and it holds at every position. The guard stops acting on
  hunger (it still logs it). So the slider can lean right fully, and the rich pay for the poor's food through the skim.
- **Never Broke on:** the money floor is that toggle's job, and it holds at every position. The alms still run first. The
  share-out still runs at the left, after Never Broke.
- **At the left,** both toggles are mostly idle. The share-out levels the purses, so few households fall under the line.

## 8. The controller

`steerEconomy(day)`: a new step at PayHour, after `followTreasury`. It is deterministic: no random numbers, accounts in
sorted order, and whole pennies for money. Its doubles live only in the saved state, as the toll drift's do.

### 8.1 Measures, each night at PayHour's end

All are computed in Core (none exist there today):

- **G:** the Gini of earning households' purses. This is world_check's definition (`world_check.cpp:1120-1130`), moved
  into Core as `Society::giniEarners()` and called by world_check, so the two can't differ.
- **S:** the upper share (section 3).
- **T10:** the richest tenth of NPC households' share of all NPC household purses.
- **H:** hungry residents an hour today, a resident counted when its hunger is 80 or more, sampled each hour in
  `loopHourly`. This is world_check's `hungry_mean`.
- **K:** households short today (`loopToday_.householdsShort`).
- **Tr:** the treasury at 18:00 (`treasuryNight`).

### 8.2 Means and judging

- **Slow means:** G, S and T10 have a 56-day exponential mean (`steerMeanDays`). H, K and Tr have a 28-day mean. Each
  mean is a plain mean until it has its days, as `followTreasury`'s level is.
- **Judging:** the first judging comes once the 56 days are in. After that, every `steerJudgeDays` days (28).
- **The error** is signed: + means too equal (push right), - too unequal (push left).
  - For `x < 0`, it is on G: `(G - target) / band`, so a Gini over its target pushes left.
  - For `x > 0`, it is on S: `(target - S) / band`, so a share under its target pushes right.
  - At `x = 0`, it is on both. If either measure is outside its band the error takes its sign. If they disagree, the
    error is 0. Within both bands it is 0 (the dead band).
- **The step:** `steerStep` (0.05) in the error's sign, or `steerStepMost` (0.10) when the error is over 2 bands.
  Nothing moves if the error is inside ±1 band.
- **Hold if moving back:** if the measure has already moved toward its target since the last judging by at least a
  quarter of the error, the step waits until the next judging, as `followTreasury` waits, so a change can tell first.
- **Clamps:** the push stays within `[x - w, x + w]` ∩ [-1, 1] (section 4), except for the hunger guard (7.3).
- **Rate limit:** at most 0.10 a judging. The full band at the ends (0.6 wide) takes at least 168 days to cross, and the
  centre's (0.3) at least 84.
- **A slider change** puts the push at the new `x` at once (feed-forward). The means stay and judging restarts its 28
  days, so the move tells before it is trimmed.
- **Turning the steering on** starts the push at `x` and begins the means. **Turning it off** sets the push to 0 and
  keeps the means.

### 8.3 Order in the city loop

At the shops' hours, as today. At 17:00, after the Wharf's hour, Never Hungry runs: its batch, the skim, then the food.

At PayHour (18:00):

1. `settleWharf`, `settleEstates`.
2. `payWages`: each city till's excess goes to the hands' share, then the ground rent, then the keeper.
3. `payFarms`, with the rent scale applied in `landRentPercent`.
4. `supportWages`, `giveAlms`.
5. **Never Broke.**
6. **The share-out** (u < 0).
7. **The Houses' dividend** (u > 0).
8. `spendSurplus`, with its steered keep, cap and level.
9. `treasuryNight`, `followTreasury`.
10. **`steerEconomy`:** measure, update the means, judge, and move the push. The new levers take effect from the next
    hour's steps.
11. `rotStock` and the estates' keep, as today.

The measures are read after every money step, so the Gini is the evening's, after the share-out.

### 8.4 With the toll drift and the treasury

- `followTreasury` stays exactly as it is: it keeps the toll within ±1 point of the world's, and the alms within +0..+2.
  The toll lean is added on top. The two can't fight over the toll. The drift answers the treasury's level against
  5,500p (today, saturated at +1), and the lean answers the slider.
- At the left, the lowered keep and cap make the surplus spend everything over 1,500-3,000p. The treasury's level then
  never nears 5,500 + 825p, so the drift never eases the toll against the lean.
- At the right, the treasury guard (5.3) stops the toll falling while the treasury is low.

### 8.5 How the steered values are read

- `Society::rule(name)` (`RatwCityLoop.cpp:570`) is the single read point. With `steer` on, it looks in `steered_`
  first: a small map refreshed whenever the push or the slider changes, holding `surplusKeep`, `treasuryCap` and
  `surplusAlmsTo` at the push. The DM's own `rules_` stay untouched, so turning the steering off restores them at once.
- The new levers are read through `steerLever(name)`, which returns the table's value at the push (0, or 100 for the rent
  scale, with the steering off).

### 8.6 Saved state

`LoopState.steer` is saved in the society's `loop` (`RatwWire.cpp:992-1038`) only when it has anything in it, and is
`empty()` otherwise. It holds:

- `push`, `judged` (the day), and `lean` (the slider it was judged at);
- the six means and their day counts;
- the measures at the last judging;
- the toll lean (its guard can hold it apart from the table);
- a short history: the last 24 judgings, each with day, measure, target, band, error, step, push before and after, and
  the reason text.

The history gives the DM tab its "why", read from the latest `game.checkpoints` as the toll's drift is today
(`studio.py drift()`).

## 9. Money conservation

- The new money moves are: share-out, hands' share, ground rent, dividend, skim, and relief. Each is a `shift` from one
  existing account to another, or through `fund:relief` (the `fund:` prefix is already allowed, `RatwCareers.cpp:153`).
- The Wharf's import in Never Hungry's fallback is the existing valve's import, counted in `SocietyState::sunk` as
  today.
- **Nothing is minted.** world_check's `conserved` check holds unchanged, and the report adds two checks:
  - **The skim's books balance:** skimmed = relief paid (food + necessities + share-out) + the change in
    `fund:relief`, to the penny, over every day and the run.
  - **No holder is taken under its line:** the skim logs each payer's spare before and after. The report counts any after
    below 0 (it must be none).

## 10. The DM tab

### 10.1 A Steering panel at the top of the Economy tab

New `Editor/src/dm/SteeringPanel.tsx`. In order:

1. **"Where the money rests"**: a slider from -100 to +100, step 5, with marks at -100, -50, 0, +50 and +100. Labels:
   "Shared: money and food equal" at the left, "Today's economy" at the centre, "Held by the rich" at the right.
   - Dragging shows what the new position means, without saving: its targets, and each lever's bound there.
   - Releasing asks for confirmation ("Move where the money rests from 0 to -40? The steering moves its levers toward
     it over the coming weeks.") and saves.
   - With the steering off the slider is greyed, with the note "The steering is off: the economy runs on its own rules."
2. **The steering**: on/off, with a confirmation each way.
3. **Never Hungry** and **Never Broke**: one switch each, with a sentence each. "A wolf going hungry is given a day's
   food, bought at the stable price; the richest pay for it." "A household short of the next day's necessities is given
   just enough; the richest pay for it."
4. **Where it is against where it is going** (read-only): each measure's 56-day or 28-day mean, its target and band at
   this position, and a small bar for each. Also "the push": a marker on the same scale as the slider, with its allowed
   band shaded.
5. **The levers** (read-only): one row per lever, giving its value now, its value at the centre, its bound at this
   position (`x ± w`), and the last judging's reason. For example: "Day 812: the upper share 18.2% is under its target
   25% (band ±4); the push went +0.05, to 0.62; the ground rent went 23 -> 25%."
6. **The skim** (read-only), over the last 28 days: the total; by payer group; the top five payers; by purpose (Never
   Hungry, Never Broke, share-out); any shortfall; and the relief fund's balance.
7. **The log**: the DM's own changes (from `dm.audit`, action `economy.steer`) and the steering's judgings (from the
   saved history), newest first, in one list.

### 10.2 The rest of the tab while the steering is on

`RulesPanel` shows the three steered rules read-only: "steered: 1,000; yours: 4,000, used again when the steering is
off". The four new rules are shown only in the Steering panel. Every other rule stays editable as today, since the
steering doesn't move it. But the DM can't edit a steered rule while the steering is on. The backend refuses it too
(10.3), so a direct API call can't either.

### 10.3 The backend (`tools/studio.py`)

- **A new route,** `POST /api/economy/steer`, added to `DM_ECONOMY_ROUTES` and to `economy_change`.
  - Body: `{steer?: bool, lean?: -100..100 (whole, in 5s), neverHungry?: bool, neverBroke?: bool}`.
  - It checks the body. It then writes `steer`, `steerLean = lean + 100`, `neverHungry` and `neverBroke` into the world's
    `live.economy.rules`, through `_edit` (`rules=True`), as `set_rules` does.
  - It audits with action `economy.steer` and target `steering`. The detail is in words, e.g. "Where the money rests: 0
    -> -40 (the steering on)" or "Never Broke on". Then `synced` sends `economy.sync`, and the game re-reads
    `live.economy` (`adoptEconomy`) within seconds.
- `set_rules` refuses the four steering rules (they belong to `/steer`). With `steer` on it also refuses `surplusKeep`,
  `treasuryCap` and `surplusAlmsTo`, with the error "the steering holds this rule; turn the steering off to set it".
- `Economy.read` adds a `steering` block:
  - the four settings;
  - the targets and bounds at this position, from `city_loop.json`'s table, computed in Python by the same interpolation
    as Core: one formula, tested on both sides against the same fixture;
  - from the latest checkpoint's `loop.steer`: the push, the means, the history, and the skim's last 28 days.
- The dm and admin roles can change it. Anyone signed in can read it.

### 10.4 Atlas and the world file

- The four new rules are added in the five places a rule lives:
  - `Core/RatwItems.h` `Rules[]`: `steer` 0..1 (fallback 0), `steerLean` 0..200 (100), `neverHungry` 0..1 (0) and
    `neverBroke` 0..1 (0);
  - `Data/Items/city_loop.json` `rules`;
  - `tools/map_editor.py` `RULE_LIMITS`, plus `RULE_SWITCHES` for the three on/offs;
  - `Editor/src/model/model.mjs` `RULE_LIMITS`;
  - `Editor/src/lib/economy.ts` `RULES`.
- The controller's own rules go in the same places: `steerJudgeDays` 7..182 (28), `steerMeanDays` 14..365 (56),
  `steerStep` 1..25 (5, hundredths of the push), `steerStepMost` 1..50 (10), `steerWidthMid` 0..50 (15), `steerWidthEnd`
  0..100 (30), `skimMost` 1..100 (50), `skimKeepDays` 1..28 (3).
  - Atlas can edit these as an author.
  - The DM tab shows them read-only, under "The steering's own rules".
- `map_editor.rule_lines` writes them into the world manifest as any rule (`rule "steerLean" 100`).

## 11. Storage

- The four settings are rules in `live.economy.rules`, the JSON `{name: N}` column (`world_store.py:238`). **No
  migration.** They survive restarts, travel with nothing else (per world, as every economy lever), and are read by
  `adoptEconomy`, both at start and on `economy.sync`.
- A world file can carry them as `rule` lines. That is how the test runs set them (section 13).
- The lever table and targets are data in `city_loop.json`.
- The controller's state is in the society's save (`loop.steer`).

## 12. Logging

### 12.1 The daily line (`world_check.cpp` ~1563)

- **The steering:** `steer`, `steer_lean`, `steer_push`, `steer_push_lo` and `steer_push_hi`, `steer_judged` (true on a
  judging day), and `steer_why` (the reason text, on judging days).
- **The measures:** `m_gini` (G; `gini_earners` stays), `m_upper_share`, `m_top10`, `m_hungry`, `m_short`, `m_treasury`,
  and their means `g_mean`, `s_mean`, `t10_mean`, `h_mean28`, `k_mean28`, `tr_mean28`.
- **The levers at the push:** `lev_share_out`, `lev_posts_share`, `lev_toll` (and `toll_now` as today), `lev_surplus_keep`,
  `lev_cap`, `lev_alms_to`, `lev_ground_rent`, `lev_rent_scale`, `lev_dividend` and `lev_rich_above`.
- **The flows:** `share_out`, `share_out_homes`, `posts_share`, `ground_rent`, `dividend`, `dividend_homes`,
  `land_rent_extra` (what the rent scale added).
- **The skim:**
  - `skim` (the total), `skim_for` ({never_hungry, never_broke, share_out}), `skim_by` (group -> pennies), `skim_top`
    (the five largest payers), `skim_short`, `relief_fund`;
  - `never_hungry` {pieces, pennies, homes, wharf_pieces, wharf_pennies}, `households_short_after`;
  - `never_broke` {pennies, homes}.

### 12.2 The full log

Two new tables:

- `skim`: payer, group, spare_before, pennies, purpose;
- `relief`: member, class, kind (food, necessities, share-out, dividend), pennies, pieces.

The `class` table gains the upper share's parts. Groups in `flow` gain `fund:relief`.

### 12.3 `tools/econ_report.py`

A new section, "The steering", with tests in `test_econ_report.py`:

- by year, each measure's mean against its target and band, with pass/fail;
- the push: path, least and most, and the days at its band's edges;
- each lever's year mean;
- the skim by purpose and by payer group, and its five steadiest payers;
- the two checks of section 9;
- the toggles' cost a day;
- `households_short_after`.

## 13. Tests

### 13.1 Core: new `Tests/steer_tests.cpp`, in the test build

**The skim**

- It takes from the richest first and water-fills.
- It respects each holder's spare line and the 50% cap, then lifts the cap in the second pass.
- It logs a shortfall when nothing has spare.
- Ties break by id, and two identical worlds give identical moves.
- It is fresh: the richest changes between two calls, and so does the payer.
- It never takes from a player, `fund:relief` or the beyond.
- Money is conserved.

**Never Hungry**

- A short member gets a day's food at the stable price from a shop's stock, and the till is paid the stable price.
- When no shop has food, the food comes from the Wharf.
- It never feeds anyone over the line.
- With the toggle off, nothing happens.

**Never Broke**

- A household is topped up to exactly the line.
- A household over the line gets nothing.
- It runs after the alms.

**The levers**

- At push 0 every lever is the world's own, and `rule()` returns what it does today.
- The steering off is identical to today.
- The table interpolates.
- The hands' share and the ground rent split a till's excess, and the till keeps its cap.
- The rent scale is never paid on a night the crew went short.
- The dividend keeps `estateSpare`'s keep.
- The rich hold leaves the luxuries' threshold alone.

**The controller**

- The dead band: no move inside the bands.
- The step and its size, the hold-if-moving-back rule, and the band clamp.
- The rate limit.
- The hunger guard blocks a move right, and pulls the push to the centre at most.
- The treasury guard.
- A slider change gives feed-forward and restarts the judging.
- `LoopState.steer` save and load round-trip.

**Bigger checks**

- A 60-day small world: money conserved, and the skim's books balanced.

### 13.2 Existing tests to update

- **Core:** `Tests/selfcorrect_tests.cpp` (its rule-value asserts, 166, 618 and 661; `followTreasury` with a toll lean),
  `Tests/wharf_tests.cpp` (the fallback order), `Tests/farms_tests.cpp` (the rent scale).
- **Python:** `tools/test_studio.py:995`, the rule parity: the new rules in `RULE_LIMITS` and `ruleDefaults`. A new
  `test_steer` checks that `/api/economy/steer` writes the four rules, audits `economy.steer` in words, and queues
  `economy.sync`. It also checks the refusals: a bad lean, a non-bool, and a steered rule set through `/rules` while the
  steering is on. Python's targets and bounds are checked against Core's on the shared fixture.
- **Other Python:** `tools/test_dm_service.py` (the route list), `tools/test_dungeon_master.py` (if the audit list
  changes).
- **Editor:** `Editor/src/lib/economy.test.ts` (the `RULES` entries; the slider's -100..+100 ↔ 0..200 mapping). A new
  `SteeringPanel.test.tsx` checks that the levers are read-only while the steering is on, and that only the four
  controls send.

## 14. The test plan

Build `build-core` (gcc Release) and run the unit tests and the Python and Editor suites. Run `tools/test_studio.py`
and the selfcorrect, wharf and farms tests.

**The sims need the user's go-ahead** (standing rule: no economy sims without it). With it, they are run as follows.

**Run variants.** world_check has no `--rule` flag. Each variant is a copy of export70 with rule lines added to its
`world.ratw`, in `artifacts/economy/r2/export70-steer-<name>/`. These copies are not committed.

| run | rules added | 
|---|---|
| steer-centre | `rule "steer" 1`, `rule "steerLean" 100` |
| steer-left | `rule "steer" 1`, `rule "steerLean" 0` |
| steer-right | `rule "steer" 1`, `rule "steerLean" 200` |
| steer-centre-floors | `rule "steer" 1`, `rule "steerLean" 100`, `rule "neverHungry" 1`, `rule "neverBroke" 1` |

**First, two 60-day smokes:**

- steer-centre, against stable10b's first 60 days. Every field stable10b's line has must be equal on every day: the
  first judging is day 56, inside the dead band, and nothing new moves money at the centre.
- steer-centre-floors: the skim balances, `households_short_after` is 0, and money is conserved.

**Then the four 3-year runs in parallel,** with stable10b's exact flags (`--simulate 6 26298 --no-check --deterministic
--threads 4 --no-profile --farms 0 --economy-level full`, `--economy`, `--loop-state` and `--economy-full` per run).
Each runs on its own cores: `taskset -c 2-5`, `6-9`, `10-13` and `14-17`. Each takes about 37 minutes. Each gets an
`econ_report` report, `<run>-report.md`. A run started is waited on until it exits.

**Pass, for every run:**

- money conserved every day;
- the estates' posts paid every night;
- the skim's books balanced to the penny;
- no holder taken under its line;
- `tills_short` no worse than stable10b's by more than 10%;
- the treasury's least over its float;
- hungry residents an hour, 28-day mean, never over 0.15 (the floor);
- deterministic: a rerun of the 60 days gives the same digests.

**Pass, by run (year 3):**

| run | must show |
|---|---|
| steer-centre | 56-day Gini within 0.452-0.522; upper share 4.5-9.5%; the push within ±0.15 the whole run (ideally 0 throughout); hunger and households short no worse than stable10b's (0.45); the toll as stable10b's. If the push never moved, every existing field equals stable10b's, and the report says so. |
| steer-left | 56-day Gini at or under 0.19 (target 0.15 ± 0.04); top-10% at or under 20%; households short at or under 0.1, hungry at or under 0.02 an hour; the surplus firing; the share-out's and the hands' share's flows logged. |
| steer-right | upper share at or over 40% (target 45 ± 5); top-10% at or over 55%; hunger under its ceiling (0.15 an hour, households short at or under 5); the estates' posts paid; the alms paid every night. |
| steer-centre-floors | `households_short_after` 0 every day; hungry about 0; skim = relief paid every day; the skim's payers changing across the run (no single group over 80% of a quarter's skim); Gini and upper share within the centre's bands (the steering compensating); the toggles' cost a day reported. |

Later runs at -50 and +50, and a steering-off run with both toggles on (what the toggles cost without compensation), are
cheap follow-ups, if the user wants them.

## 15. Building it in one pass

1. **Data and rules.** Add the 12 rules to the five places (10.4). Add `city_loop.json` `"steering"` (the levers and
   targets tables of sections 3-4), parsed in `RatwItems` into `items::cityLoop().steering`.
2. **Core state.** `LoopState.steer` and its wire (de)serialisation; `steered_`; `rule()`'s lookup; `steerLever()`.
3. **Core measures.** `giniEarners()` (moved from world_check), the upper share, the top tenth, and the hourly hungry
   count.
4. **The skim and `fund:relief`.** `spareOf()` per holder kind, `skim(sum, purpose)`, and its log.
5. **The levers' hooks.** `payWages` (the hands' share, the ground rent), `landRentPercent` (the rent scale), the
   comforts budget (the rich hold), `gateTollNow` (the toll lean), `topUpPoorest(source, cap)`, `shareOut()`,
   `housesDividend()`.
6. **The toggles.** `neverHungry(hod)` at 17:00 and `neverBroke()` after `giveAlms`.
7. **The controller.** `steerEconomy(day)` and its guards.
8. **world_check's fields** and the full log's tables; `econ_report`'s section.
9. **studio.py**: `/api/economy/steer`, `set_rules`' refusals, and `Economy.read`'s `steering` block.
10. **The Editor**: `SteeringPanel`, `RulesPanel`'s steered read-only rows, and `api.ts` `economy.setSteer`.
11. **Tests**, as in section 13.
12. **The two smokes,** then the four 3-year runs (with the go-ahead), then a progress entry here.

## 16. Open questions and risks

1. **The far left's 0.15 may be out of reach.** Wages are paid at 18:00 and food is bought from 8:00, so the purses churn
   every day. If steer-left settles higher, the table's -100 Gini target is set to what the share-out at 30% reaches,
   and the doc says so. The share-out's percent is not raised past 30, so a household's own wage stays mostly its own.
2. **The far right's 45% depends on the rich hold** (600p a head) and on the Crown's grants. Money in the Crown's estate
   is upper share, but the Crown grants it on to the short Houses, so it doesn't stay. If steer-right falls short, the
   rich hold's +1 value is the first thing to raise. It is data, so this is a change to `city_loop.json`.
3. **Never Broke may cost more than estimated** (500-900p a day). The skim still finds it (the realm holds about 64,500p),
   but at the centre it pushes the Gini and the upper share down, and the steering then leans right within ±0.15. If
   that band isn't enough, steer-centre-floors will show the push held at its edge. The fix is then a wider centre band,
   not a cheaper floor.
4. **Never Hungry through the Wharf** imports food, which sinks money to the beyond, and the valve then mints it back in
   on its own rules. If the fallback is used often, the shops' food shelves are the real problem. The report counts it
   apart.
5. **Players** are never skimmed and never relieved, here or in the live game. The live game's DM tab says so beside the
   toggles.
6. **The ground rent goes to the Crown's estate only.** Shops are not mapped to the Houses whose land they stand on. A
   later design could pay the House of the shop's district.

## Progress

### The build (branch `r2-steering`, 2026-11-06)

Built in one pass on `r2-steering` (from `ridgemere2` 85ea134), build dir `build-steer` (gcc Release). Nothing written to
DEV; no economy sim run (they need the user's go-ahead, section 14).

**What was built.**

- **The rules** (all twelve) in the five places a rule lives: `Core/RatwItems.h` `Rules[]`, `Data/Items/city_loop.json`
  `rules`, `tools/map_editor.py` `RULE_LIMITS` (the on/offs in `RULE_SWITCHES`; `STEER_SETTINGS`, `STEERED_RULES` and
  `STEER_OWN_RULES` name the three kinds), `Editor/src/model/model.mjs` `RULE_LIMITS` (which also gains `almsNoEarner`,
  missing there since round 4) and `Editor/src/lib/economy.ts` `RULES` (each tagged `steer: 'setting' | 'own' | 'held'`).
- **The table** (sections 3 and 4) in `city_loop.json` `"steering"`, with `treasuryFloor` and `treasuryLow`, read into
  `items::cityLoop().steering`; the same table is the code's default. `items::steerAt` reads a row; the studio
  (`studio.steer_at`), the report (`econ_report.steer_at`) and the Editor (`lib/steering.ts` `steerAt`) reckon it the
  same way, each tested on the shared cases in `Tests/fixtures/steer_table.json`.
- **Core** (`Core/RatwSteering.cpp`, new, and hooks in `RatwCityLoop.cpp`, `RatwFarms.cpp`, `RatwWire.cpp`):
  - `rule()` returns the table's `surplusKeep`, `treasuryCap` and `surplusAlmsTo` at the push while the steering is on
    (the world's own stay in `rules_`, read by `ownRule()`); `steerLever()` gives every lever at the push, or its neutral
    value with the steering off.
  - The levers: the hands' share and the ground rent in `payWages` (both taken off the same excess, the till keeping its
    cap); the rent scale in `landRentPercent` (`land_rent_extra` logged in `payRents`); the rich hold in the comforts'
    budget (the luxuries' keep untouched, so cheese is as it was); the toll lean in `gateTollNow`, with its treasury
    guard; `topUpPoorest(budget, from, to)` generalised; `shareOut()`; `housesDividend()` (nothing for 28 nights after any
    estate's posts went short).
  - The skim: `spares(deep)` (every holder's spare over its own needs now, the six kinds of section 6.1; the estates
    together never under the one purse's keep), `skim(sum, purpose)` (water-filling from the top, at most `skimMost`% of
    a spare, then the second pass; ties by id), through `fund:relief`.
  - The floors: `neverHungry()` at 17:00 and `neverBroke()` at 18:00 after the alms.
  - The controller: `steerFollow()` (on, off and the slider, followed at once: at 18:00 and whenever `adoptEconomy`
    reads a DM's change) and `steerEconomy()` (the measures, the means, the judging, the hunger guard).
  - The state in `LoopState::Steer`, saved as the loop's `"steer"` only once the steering or a floor has run, with the
    last 24 judgings and the skim's last 28 days (by purpose, payer group and five largest payers) for the DM tab.
- **Logging.** world_check's daily line has the fields of 12.1 (flat, as listed) whenever the steering or a floor is
  on or has run; a run without them writes the line it always did. The full log's `flow` table has a `relief` group
  for `fund:relief`. `tools/econ_report.py` has the section "The steering" (12.3).
- **The DM tab.** `Editor/src/dm/SteeringPanel.tsx` at the top of the Economy tab: the slider (-100 to +100 in fives,
  marks at -100, -50, 0, +50 and +100, the words at both ends and the centre; dragging previews the targets and the
  levers' bounds, letting go confirms and saves), the on/off and the two floors (each confirmed), the measures against
  their targets, the push on its scale with its band, the levers read only (now, at the centre, the bounds there), the
  skim's last 28 days, and the log (the DM's `economy.steer` audit and the judgings). `RulesPanel` shows the three
  steered rules read only while the steering is on ("steered: 1,000; yours: 4,000, used again when the steering is
  off"), never the four settings, and on the DM's tab the steering's own rules read only. The pure parts are in
  `Editor/src/lib/steering.ts`.
- **The studio.** `POST /api/economy/steer` (both suites' route sets; audited `economy.steer`, target `steering`,
  detail in words; `economy.sync` queued); `set_rules` refuses the four settings (400) and, with the steering on, the
  three steered rules (409, "the steering holds this rule; turn the steering off to set it"); `Economy.read` has a
  `steering` block (the settings, the targets and band at the slider, the levers' bounds, the table, the steering's
  own rules, from the save the push, the means, the judgings and the skim, and the DM's last 24 changes).

**Tests.** ctest 80 of 80 (the new `steer_behaviors`, 84 checks: the rules and the table; the centre leaving every
account as the steering off does over two ticked days; the skim's order, caps, second pass, shortfall, freshness and
ties; Never Broke to exactly the line after the alms; Never Hungry to a day's food and no more, from the shop at the
stable price; the levers at both ends, the toll lean's guard, off restoring the world's rules; the hands' share and the
ground rent with the till keeping its cap; the share-out levelling the poorest; the controller's feed-forward, its
first judging on day 56, the larger step, the band's edge, determinism, a slider change, the save read again, off; the
dead band; money conserved in each). `tools/test_studio.py` 51 (new: the routes with a stand-in, the steering offline
with the economy in memory, the table against the shared cases, every rule of Core's `Rules[]` with the studio's, and on
a scratch database (Database/.env linked in for the run, then removed) the steering through the real API: written to
`live.economy.rules`, audited in words, `economy.sync` queued, read back, the refusals). `tools/test_econ_report.py` 14
(new: the steering's section). The Editor's `lib` tests (new `steering.test.ts`, 6) and `tsc --noEmit` clean.
Failing before this branch and still: `tools/test_map_editor.py`'s and `Editor/src/model/content.test.mjs`'s Greyfen
resident count (11 against 10), `EconomyFlowTests.test_the_farms` (8 areas against 10), `Editor/src/lib/glyphs.test.ts`.

**A scratch server** of DEV (`build-steer/ratw_server --database dev --scratch`) loaded the world with the new Core
(build 70, 559 residents) and served the client; stopped after a few minutes. The DM tab is the studio's, not the game
server's, so its slider was checked through the studio's API on a scratch database (above), not in a browser.

**Not run: the smokes and the 3-year runs.** `artifacts/economy/r2/steer-smokes.sh` makes the four variant folders
(`export70-steer-centre`, `-left`, `-right`, `-centre-floors`: export70's `world.ratw` with the rule lines after its
store line, its cells and seams hard-linked) and, with `smokes` or `years`, runs the four in parallel on cores 2-5,
6-9, 10-13 and 14-17 with stable10b's flags, waits for each and writes each report. The folders are made (a two-hour
load of `export70-steer-centre-floors` read its rule lines); no run has been started.

**Where the build differs from the design, or is unsure.**

1. **Never Hungry covers the residents' households, not the farms' virtual crews** (`crew:*` accounts, one purse for
   many heads, which buy at every open shop). Their short days stay as `farms.*.crewShort` in the log.
2. **The earners' Gini in Core** is world_check's definition, taken at 18:00 after the money's steps; world_check's
   own `gini_earners` (at 23:00) is unchanged, so stable10b's field still compares.
3. **The skim's "no holder under its line" check** (section 9) is kept by construction and by the unit tests; the run's
   log has no per-payer spare before and after.
4. **The full log has no new `skim` and `relief` tables** (12.2): the daily line's `skim_by`, `skim_top` and `relief`
   carry them, and the flow table's `relief` group.
5. **The hunger guard** (7.3) is built but not unit-tested (it needs residents going hungry over ticked days); the
   first smoke will show it. Never Hungry's Wharf fallback is likewise untested (the test city's valve is shut).
6. **The DM's log** shows the DM's changes (by the audit's clock) and the judgings (by game day) one after the other in
   one list, not interleaved: the two have no common clock.
7. **Never Broke's cost** (500-900p a day estimated) is unmeasured; at 18:00 the tills are at their caps, so the skim
   then falls on the keepers' purses, the estates and the treasury's spare. The floors smoke will show who pays.

### The review (2026-11-06)

Four fixes before the merge; nothing else in the money's path changed.

1. **Never Hungry's skim leaves the treasury out.** It runs at 17:00, before the night's wages, so tonight's wage support
   is not known yet, and what the treasury holds over its cap is exactly what pays it. Taking that at 17:00 could leave
   a short till's posts unpaid. The 18:00 skims (Never Broke, the share-out) still take the treasury's spare, over its cap
   and the wage support it has just paid.
2. **A day's necessities a head is worked out each time it is asked**, not kept for the day: a DM's change to the alms in
   the afternoon, or a save read back mid-day, now gives the same line as an unbroken run.
3. **The DM's rules route refuses the steering's own rules** (`steerJudgeDays` to `skimKeepDays`), 400: they are an
   author's, in Atlas, and the DM tab only shows them. Atlas's route still sets them.
4. **The slider asks and saves once.** Letting go of it fired pointer-up and then blur (the confirmation takes the focus),
   and each asked and sent again; an arrow key asked at every press. Now it asks once per position, on letting go, on
   Enter or on leaving it. It can also be set while the steering is off (the confirmation says it takes effect when the
   steering is turned on), as the confirmation already expected.

Tests: ctest 80 of 80 (`steer_tests` adds the treasury check); `test_studio.py` 51, all but the old `test_the_farms`;
`test_econ_report.py` 14; the Editor's steering and economy tests and `tsc` clean.

### The slider's runs (2026-11-06)

The user's go-ahead (verbatim): "Yeah, run the tests. Just make sure that the automated system is following the slider
and that the system, as a whole, is being managed by the automated system". Runs on export70 copies with stable10b's
flags (`--simulate 6 26298 --no-check --deterministic --threads 4 --no-profile --farms 0 --economy-level full`); nothing
written to DEV. The table of every run by year against stable10b: `artifacts/economy/r2/steer-runs-report.md` (made by
`steer-compare.py`); each run's own report `steer-<run>-report.md`; the runner `steer-smokes.sh`.

**The smokes (60 days, the merge's code).** Money conserved in all four. `steer-centre` was stable10b to the field for
days 0-54, then its first judging leaned *left* (the push to -0.10): the upper share measured 14.2% against the design's
7 ± 2.5. `steer-right` set its toll lean to -2 at 18:00 on day 0 from the treasury before the night's wages (over its
1,400p floor) and, judged only when the push moved, kept it there while the treasury fell to 594p (stable10b 1,773p).
`steer-left` and `steer-centre-floors` showed no defect (the skim balanced to the penny, Never Broke 413p a day).

**Three fixes, each tested, merged and pushed** (72a8dbb, cdbaa2e):

1. **The toll lean's guard each night** (5.3): on the treasury's 28-night mean after the money's steps, the lean at
   least 0 while the mean is under `treasuryFloor`, moving at most half a point a night. (`treasuryLow`'s half-point a
   judging is gone; `treasuryLow` now bars leaning right, below.)
2. **The hunger guard's weekly pull** (7.3). The first far-right 3-year run (`steer-right-v1`, stopped at day 268): the
   rich hold (600p a head) and the ground rent took about 9,000p out of circulation, the toll and the alms followed, the
   treasury sat at its float from day 84, and hunger reached 36 residents an hour and 189 households short (28-night
   means) while the guard, a 0.05 step every 28 nights, had the push at +0.65 on day 252. Now, with Never Hungry off and
   the means a month old, every seventh night the push goes back toward the centre by `steerStepMost` (0.10, never past
   0) while the hungry or the short are over twice their ceilings, or the treasury under `treasuryFloor`, on their means
   and that night; the judging also won't lean right with the treasury's mean under `treasuryLow`. (A version on hunger
   and `treasuryLow` alone still let hunger reach 5.4 an hour: the treasury falls three weeks before the hunger comes.)
3. **The centre's upper share where it is: 13 ± 2** (was 7 ± 2.5; the left's watched most 13). `steer-observe` (the
   steering on, its centre band 0 wide, so the push held at 0) is stable10b to the field for all 1,095 days and logs the
   measures: the upper share's 56-day mean 12.6-14.5% (year means 13.7, 13.2, 12.7), the top tenth 34.3-35.5%. The
   design's 7% missed the Houses' businesses' tills (about 7% of the money).

Also `world_check --rule-at DAY NAME VALUE`: a rule set at a day's start as `economy.sync` sets it (`adoptEconomy`),
for the sweep; tested in `sim_fast_tests`.

**The 3-year runs** (the fixed code; `steer-left` and `steer-observe` ran on the code before fixes 2-3, which don't
touch them: the left's push is never over 0 and its targets didn't change). Year means; stable10b's year 3 in brackets.

| run | push | Gini earners | upper share | top 10% | hungry/h | short | treasury (least) | tills short | wage support | alms |
|---|---|---|---|---|---|---|---|---|---|---|
| stable10b y3 | - | 0.484 | (12.7) | (34.3) | 0.011 | 0.45 | 1,759 (1,027) | 22.2 | 106 | 411 |
| centre y3 | 0 | 0.484 | 12.7 | 34.3 | 0.011 | 0.45 | 1,759 (1,027) | 22.2 | 106 | 411 |
| left y3 | -1 | 0.324 | 11.2 | 25.5 | 0.000 | 0.00 | 1,499 (1,175) | 36.7 | 337 | 0 |
| right y1/y2/y3 | 0.27/0.08/0.21 | 0.507/0.489/0.500 | 17.4/14.2/15.9 | 37.1/34.6/35.2 | 0.021/0.007/0.014 | 1.14/0.76/0.65 | 1,489 (654) y3 | 23.1 | 57 | 428 |
| centre-floors y3 | +0.15 | 0.437 | 14.0 | 30.8 | 0.000 | 0.00 | 2,251 (1,712) | 30.2 | 235 | 225 |

- **Money conserved every day in every run; the estates' posts paid every night** (236,416 of 236,416 in year 3).
- **steer-centre is stable10b to the field, all 1,095 days**: every judging in both bands, the push 0 throughout.
- **steer-left**: the push at -1 (its band's edge) all run; the share-out 787-823p a night, the hands' share 222-251p,
  the surplus firing (205-251p a day) with the cap at 1,500p and the keep 0; nobody hungry or short; alms almost never
  needed. But the Gini settles at 0.31-0.32, not 0.15 (open question 1: the share-out at 30% is not enough against the
  daily churn), and drifts up a little each year; the top tenth 25%, not 20%. Tills short 37-38 a day against 22-26,
  and the wage support 3-4 times stable10b's: the left's toll (+3) and its low cap cost the shops.
- **steer-right**: the guard holds the floor (hunger's 28-night mean at most 0.18 an hour, households short at most
  2.7), but it holds the push near the centre: it was pulled from 1 to 0 by day 112 and since then climbs a step a
  judging and is pulled back each time the treasury's mean goes under 1,400p, which the centre's own treasury is under
  for much of year 1. The upper share 14-17%, far from 40-45%; the top tenth 35-37%, not 55%. The far right, as the
  lever table has it, can't be held by this economy without the alms' purse running dry.
- **steer-centre-floors**: Never Broke 470-640p a day (61-76 homes), Never Hungry under a penny a day (mostly from the
  Wharf: the shops' food shelves are bare at 17:00 when it is wanted), households short after it 9 household-days in
  three years. The skim: 596,332p taken, 596,332p paid out, fund:relief 0 at the end, balanced every day to the penny;
  its payers shift (upper households 22-39% of a quarter, the Crown's estate 14-31%, middle households 17-30%, the
  treasury up to 21%, Timberland's estate up to 17%; no group over 39% of any quarter). The steering leaned right to its
  band's edge (+0.15) by day 111 and stayed there: the Gini 0.42-0.44 stays under the centre's band (0.452) (open
  question 3: the band is too narrow to make up for Never Broke). Tills short 30-33, wage support 235-273p.
- **steer-sweep** (centre, then far left at day 365, far right at day 730): the push followed each move the same night
  (0 -> -1 -> +1), every lever with it. After the move left, the 56-day Gini went half the way (0.504 -> 0.324) in 41
  days, 90% in 129; the top tenth 36.6 -> 25.7% half in 42 days. After the move right, the upper share rose 11.5 ->
  22% in 56 days, then the guard pulled the push to +0.4 by day 786 (hunger's mean 0.34 an hour at its worst, 11.5
  households short) and +0.1 by day 982, and it climbed again to +0.5 by the end.

**Judged.** (a) The automation follows the slider in direction everywhere and exactly at the centre, but not in
reach: the far left gets the Gini to 0.32 (target 0.15) and the far right is held near the centre by its own safety
guard. (b) With the steering on, the steered levers move only with the push, and every lever moved when the push did;
the skim and the floors balance to the penny; the money is conserved and the estates' posts paid at every position.
Outside the automation still: the toll drift's `treasuryTarget` (5,500p, never reached at any position, so the drift sits
at +1 against the right's lean), and the treasury floor the right's guard leans on is a fixed 1,400p.

**Next, for the user to choose** (no run started): a gentler far right (the rich hold at +1 nearer 200p than 600p, so
the poor's money keeps circulating), or the push leaning right from the centre a step at a time instead of at once; the
right's treasury guard taken from the centre's own treasury (its 28-night mean in `steer-observe` is under 1,400p on
166 nights of year 1, 50 of year 2 and none of year 3); a wider centre band with Never Broke on; the left's Gini target set to what 30% reaches (about 0.32), or
a stronger share-out; and `treasuryTarget` set from the steered cap and keep while the steering is on.

### Tune round 1 (2026-11-06)

The same go-ahead as "The slider's runs". Branch `r2-steering-t1` (merged as 9daf44a, pushed). Tuned on 1-year runs of
export70 copies with stable10b's flags, five at a time on their own cores, then the 3-year runs from the main checkout
(`steer-smokes.sh t1`: `steer-<run>-t1`). The table of every run by year: `artifacts/economy/r2/steer-runs-t1-report.md`;
each run's own `steer-<run>-t1-report.md`. Nothing written to DEV.

**What the 1-year runs found.**

- **The far right starves the shops, not only the treasury.** The rich hold keeps upper-class money out of the shops; in
  `rc` (the rich hold 150p) the purses held 2,500p more than the centre and the tills 1,900p less, the shoppers spent 400p
  a day less, and the toll fell with the draws. Every summer (the treasury's lean season even at the centre: its least
  is its 200p float in year 1) the treasury reached its float and the hunger followed: 1.1 an hour at the rich hold 200p,
  4.5 at 300p, 0.45 even at 150p. A gentler table alone didn't hold it.
- **Fix: the ground rent goes to the treasury while the treasury is under its floor** (on its 28-night mean or that
  night). With it the same table's summer hunger was 0.004-0.06 an hour. The weekly pull then reacted only to the treasury,
  which the ground rent was already refilling, and took the push from 1 to 0.15 with nobody hungry, so it now pulls on
  `treasuryLow`; `treasuryFloor` bars leaning right.
- **The floors from the centre's own treasury:** `treasuryFloor` 800p, `treasuryLow` 500p. In `steer-observe` the
  treasury's 28-night mean was never under 909p (under 1,400p on 164 nights of year 1).
- **The far left's shops** were short because the levelled money was held, not spent. The purses held 4,200p more than
  the centre and the tills 3,900p less, comforts fell by a third, and the toll lean (+3, then +1) made no difference
  (tills short 37.8 against 37.7). New lever **the spending keep** (`spendAbove`): the comforts threshold of every
  household not of the Houses, 25p a head at -1 (32p at -0.5; the world's 40p from the centre right). In year 1 the
  left's tills short went from 37 to 27.6 a day (stable10b 26.5). 15p was a little better for the shops (25.6) and a
  little worse for the Gini (0.34).
- The far right table chosen: the rich hold 150p, ground rent 30%, the dividend 30%, the rents 125%, no toll lean (it
  only drained the treasury and fought the drift). A stronger one (200p / 45% / 40% / 140%) reached 27% but brought the
  treasury's mean to 509p and hunger's 28-night mean to 0.23 an hour.

**The changes** (each tested: steer_tests 107 checks; ctest 80 of 80; test_studio 51 OK; test_econ_report; the Editor's
steering tests): the lever and target tables in sections 3 and 4 (the first design's values in brackets); the ground
rent to the treasury under its floor (booked `ground rent: the treasury`, logged `ground_rent_treasury`); the guard on
`treasuryLow`; `spendAbove`; the toll drift's target, with the steering on, never over the steered cap and keep (the far
left's 1,500p, where its surplus holds the treasury; the centre's stays the world's 5,500p); with Never Broke on, a Gini
under its band is that floor's own levelling and isn't steered against (open question 3, by a one-sided band rather than
a wider one); Never Hungry tries the Wharf's next food when the cheapest is over the Wharf's day's room.

**The 3-year runs** (year means; stable10b in brackets):

| run | push | Gini earners | upper share | top 10% | hungry/h | short | treasury (least) | tills short | wage support | support unpaid |
|---|---|---|---|---|---|---|---|---|---|---|
| centre y1-y3 | 0 | 0.489/0.489/0.484 | 13.7/13.2/12.7 | 35.5/35.5/34.3 | stable10b to the field, every day | | | | | |
| left y1/y2/y3 | -1 | 0.328/0.339/0.344 (target 0.33 ± 0.04) | 13.1/13.1/12.8 | 26.2/26.8/27.2 (at most 27) | 0.000/0.003/0.000 | 0 | 1,514 (1,402) y3 | 27.6/26.8/26.5 (26.5/23.2/22.2) | 251/224/215 (82/70/106) | 86/57/54 (190/122/76) |
| right y1/y2/y3 | 0.87/0.75/0.75 | 0.539/0.548/0.547 | 22.8/22.3/21.8 (target 24 ± 3) | 40.9/40.7/40.3 (at least 40) | 0.017/0.005/0.018 | 1.24/0.88/0.70 | 1,007/894/984 (200) | 31.8/26.5/24.2 | 41/15/17 | 301/201/184 |
| centre-floors y1/y2/y3 | 0 | 0.415/0.426/0.422 | 12.5/12.3/11.9 | 31.2/31.7/30.8 | 0 | 0.05/0/0 | 2,298 (1,788) y3 | 32.3/30.8/30.8 | 280/241/242 | 94/50/52 |

- **Every run: money conserved every day; the estates' posts paid every night** (236,416 of 236,416 in year 3); every
  lever on every day exactly the table's at the push (checked against `city_loop.json`), the toll lean never outside 0
  and the table's.
- **centre**: stable10b to the field all 1,095 days, the push 0.
- **left**: the push -1 all run, every judging in its band; the Gini 0.32-0.35 against 0.33 ± 0.04, up a little each
  year (0.347 in its worst quarter); nobody hungry or short; the skim 777,767p in, 777,767p out. The toll drift stays
  at 0 (its target 1,500p, the treasury in its band), so it doesn't fight the lean. The tills short are 27.6 in year 1
  (stable10b 26.5), but 15-19% over stable10b in years 2-3; the treasury's wage support is 2-3 times stable10b's, paid
  from what the left's lower cap lets it spend.
- **right**: the push from 1 to 0.75 on days 167-175 (the treasury's 28-night mean 445-458p, under its low), then in
  band at 0.75 for the rest of the run; the upper share 21-23.5% a quarter, in its band; the top tenth 39-43%; hunger's
  28-night mean at most 0.18 an hour (once over its 0.15 ceiling), households short at most 3.7; the ground rent 125p a
  day, 47p of it to the treasury. But the treasury sits low (year means 894-1,007p) and is at its 200p float on 21 days
  (it never goes under), and the shops' support unpaid is 1.6-2.4 times stable10b's.
- **centre-floors**: the push 0 throughout (it was +0.15 from day 111): the Gini 0.42-0.43 accepted as Never Broke's
  levelling, the upper share 11.9-12.5% in its band. The skim 556,534p taken and 556,534p paid out, fund:relief 0, no
  day off; no group over 35% of any quarter (middle households 22-35%, the Crown's estate 14-31%, upper households
  6-29%, the treasury up to 32% of the last three days). Never Broke 444-574p a day; no household short after Never
  Hungry on any day (9 household-days before). Tills short 31-32 (stable10b 22-27).
- **sweep** (centre, far left at day 365, far right at day 730): the push followed each move the same night (0 -> -1
  -> +1), every lever with it. After the move left the Gini went 0.504 -> 0.338, half the way in 40 days and 90% in 137.
  After the move right the upper share went 12.8 -> 24.3%, half in 56 days and 90% in 143; the push stayed at +1
  through year 3 with the treasury's 28-night mean at least 773p and hunger's at most 0.17.

**Judged.** (a) The automation follows the slider: the push tracks the slider and its band, every lever follows the
push, and at every position the measures reach their bands: the left's Gini, the right's upper share and the centre's
both. But the ends are where this economy can hold them with its safety floor intact, not the first design's: 0.33,
not 0.15, at the far left; 24%, not 45%, at the far right. (b) The steered levers move only with the push. The toll
drift no longer works against the lean: the right has no lean, and the left's drift is judged against its own cap. The
right's treasury guard is set from the centre's own treasury. The skim and the floors balance to the penny, and money and
the estates' posts hold at every position. Left open: the right's treasury at its float on 21 days, with the shops'
unpaid wages up; the left's shops 15-19% over stable10b in years 2-3; Never Broke's cost to the shops (tills short 31
against 22-27).
